/*
 * ncr_simulation - building blocks to create custom simulations
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 */
#pragma once

#include <atomic>
#include <iostream>
#include <chrono>
#include <cassert>
#include <thread>
#include <cmath>

#include <ncr/ncr_log.hpp>
#include <ncr/ncr_units.hpp>
#include <ncr/ncr_common.hpp>

namespace ncr {


/*
 * struct iteration_state - Iteration of a simulation
 *
 * This struct contains information about an iteration of a simulation. For
 * instance, simulation passes along an iteration_state during callback
 * invocations.
 *
 * It is also required in transport_t<T>, in case the transport operates in any
 * timing mode which is not MTM_NONE. In such a case, message transport
 * crucially depends on delivery times of the messages stored in the transport,
 * which in turn depends on timing information of the simulation.s
 *
 * TODO: maybe merge with simulation_config. Essentially, an iteration state
 * is simply an updated simulation_config relative to the current
 * state/progress of a simulation.
 */
struct iteration_state {
	// dt is the delta-t that is to be integrated forward. in timeless mode,
	// this value will be undefined
	double        dt;

	// t0 is the initial time that the simulation started. usually, in most
	// simulations, this will be 0
	double        t_0;

	// t_max indicates the maximal running time that the simulation was
	// configured to run. this value will be undefined in timeless mode
	double        t_max;

	// the current simulation time step. For instance, a simulation running in
	// timed mode and starting from time 0 will have set this at 0 in the first
	// callback call, and incremented by dt during every time ste.
	double        t;

	// number of ticks elapsed in the simulation
	std::uint64_t ticks;

	// indicates if this simulation is running in timeless mode.
	bool          timeless;
};


/*
 * simulation_config - user-configurable settings of a simulator
 */
struct simulation_config {
	// number of threads to use during simulation.
	// By default, use all available threads on the local machine.
	const unsigned int  nthreads = std::thread::hardware_concurrency();

	// indicate if the simulation is running in a timeless fashion or not. If
	// yes, then time will not be forward integrated by the simulator, and the
	// user *must* implement the stop-condition callback.
	// By default, the simulation will run in timed mode.
	bool                timeless = false;

	// value to be used during numerical evaluation. Specifically, this is a
	// small epsilon that is added to delta-t to estimate if the next time step
	// would reach or exceed the maximum time limit. The aim is to capture
	// numerical instabilities during integration of delta-ts which cannot be
	// properly represented in IEEE 754 floating point variables.
	// Some experiments revealed that an epsilon of about 1e-10 is a good guess
	// for most cases.
	double              t_eps    = 1e-10;

	// initial / starting time of the simulation.
	// This value will be ignored if running in timeless mode.
	double              t_0      = 0.0_ms;

	// maximum simulated running time.
	// This value will be ignored if running in timeless mode.
	double              t_max    = 0.0_ms;

	// default time integration step-width.
	// Note that the simulator has a final adaptive step-width. That is, due to
	// numerical limitations, the final step with might be smaller or larger
	// than this delta-t
	// This value will be ignored if running in timeless mode.
	double              dt       = 0.1_ms;
};


// typedefs for the callbacks for better readability
typedef bool (*cb_stop_condition_fn_t)(const iteration_state*, void *data);
typedef void (*cb_tick_fn_t)(const iteration_state*, void *data);


/*
 * struct callbacks_t - struct with all callbacks invoked by simulation
 */
struct simulation_callbacks {
	// stop_condition is called to determine if the simulation should finish or
	// not. If this callback is not set, then the simulation will evluate if the
	// current time-step exceeds the maximal time that is supposed to be
	// simulated. If this is the case, the simulation will stop.
	// The stop-condition callback takes a pointer to the current iteration state as
	// argument. An implementation of this callback can thus decide on the stop
	// condition depending on the current iteration.
	//
	// NOTE: the stop-condition callback will be invoked _before_ the
	//       iteration-callback. Hence, it does not make sense to implement a
	//       callback for a timed simulation that tests for t > tmax, in
	//       particular due to an adaptive alteration of dt (see comment for
	//       the iteration callback below). For timed simulations, the simulator
	//       will _always_ evaluate if tmax is reached and stop after the final
	//       invokation of the iteration callback. For timeless simulations,
	//       this is, of course, not the case.
	cb_stop_condition_fn_t stop_condition = nullptr;

	// iteration is called during each step of the simulation. This callback is
	// intended to be used to advance the state of whatever is simulated.
	// The iteration callback takes a pointer to the current iteration state as
	// argument. An implementation can then forward entities that are simulated
	// depending on the current state & iteration information.
	//
	// NOTE: in a timed simulation, the iteration callback will be called for
	//       all times in the range [t0, tmax], including t0 and tmax. Moreover,
	//       it can happen that dt in the last iteration is slightly increased
	//       or decreased to adapt due to numerical considerations. For
	//       instance, a dt of 0.1 cannot be properly represented in IEEE 754
	//       floating point numbers without rounding errors. To account for
	//       this, every iteration is evaluated to overshoot tmax, including a
	//       numerical epsilon.
	cb_tick_fn_t tick = nullptr;

	// often, it is necessary to access some user defined variables during a
	// simulation, e.g. to track global state without having static global
	// variables. this is what the `data` field is made for. `data` will be
	// passed to all callbacks as last argument.
	// Note that this is a void pointer to avoid template creep.
	void *data = nullptr;
};


/*
 * simulation_run_statistics - statistics about a simuluation run
 *
 * This is a convenience data structure with statistics about the runtime of a
 * simulation run. These values could be obtained otherwise, but it's so much
 * easier to directly access them after a simulation.
 */
struct simulation_run_statistics {
	// total runtimes
	std::chrono::nanoseconds  runtime_total_ns;
	std::chrono::milliseconds runtime_total_ms;
	std::chrono::seconds      runtime_total_s;
	std::chrono::minutes      runtime_total_min;
	std::chrono::hours        runtime_total_h;
	std::chrono::days         runtime_total_d;

	// relative runtimes for pretty prenting
	std::chrono::nanoseconds  runtime_ns;
	std::chrono::milliseconds runtime_ms;
	std::chrono::minutes      runtime_min;
	std::chrono::seconds      runtime_s;
	std::chrono::hours        runtime_h;
	std::chrono::days         runtime_d;
};


/*
 * Simulator State
 */
struct simulation {
	std::atomic<bool>              running;
	std::atomic<bool>	           paused;

	// configuration of a simulator
	const simulation_config&     config;

	// callback structure for user defined callback implementations
	const simulation_callbacks&  callbacks;

	// iteration state of the simulation
	iteration_state                iteration;
};


// TODO: mayb introduce a "proper" C++ class for the simulation and pack the
//       methods below into this. This class then also carries its state,
//       options, callbacks, etc. instead of having free functions. Not sure if
//       this is really required, but would at least nicely encapsulate all
//       functions.


/*
 * simulation_setup - set up a new simulation object given some callbacks
 */
inline
simulation*
simulation_setup(
		const simulation_config &config,
		const simulation_callbacks &callbacks)
{
	log_debug("Simulation Setup\n");
#if NCR_LOG_LEVEL >= NCR_LOG_LEVEL_DEBUG
	auto start = std::chrono::steady_clock::now();
#endif

	// instantiate a new simulation object, and pass along the configuration as
	// well as the callback information.
	auto sim = new simulation{
		.running   = false,
		.paused    = false,
		.config    = config,
		.callbacks = callbacks,
		// initial iteration is set to zero
		// TODO: move this into a constructor, maybe?
		.iteration{
			.dt        = config.dt,
			.t_0       = config.t_0,
			.t_max     = config.t_max,
			.t         = config.t_0,
			.ticks     = 0,
			.timeless  = config.timeless,
		},
	};

	// check if the user wanted a timeless simulation but didn't specify a
	// cb_stop_condition
	if (sim->config.timeless && (sim->callbacks.stop_condition == nullptr)) {
		log_error("cannot setup timeless simulation without a stop-condition callback\n");
		delete sim;
		return nullptr;
	}

	// TODO: callback for simulation setup, e.g. steps that are needed during
	//       the setup could be called from within here.

#if NCR_LOG_LEVEL >= NCR_LOG_LEVEL_DEBUG
	auto end = std::chrono::steady_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	log_debug("Simulation Setup done after ", std::scientific, duration, " ms\n");
#endif

	return sim;
}


inline
simulation_run_statistics
simulation_get_statistics(
	std::chrono::steady_clock::time_point loop_start,
	std::chrono::steady_clock::time_point loop_end
	)
{
	using namespace std::chrono;

	simulation_run_statistics result;

	// total runtimes
	result.runtime_total_ns  = duration_cast<nanoseconds>(loop_end - loop_start);
	result.runtime_total_ms  = duration_cast<milliseconds>(loop_end - loop_start);
	result.runtime_total_s   = duration_cast<seconds>(loop_end - loop_start);
	result.runtime_total_min = duration_cast<minutes>(loop_end - loop_start);
	result.runtime_total_h   = duration_cast<hours>(loop_end - loop_start);
	result.runtime_total_d   = duration_cast<days>(loop_end - loop_start);

	// relative runtimes
	result.runtime_ns        = result.runtime_total_ns;
	result.runtime_ms        = result.runtime_total_ms;
	result.runtime_ns       -= result.runtime_ms;
	result.runtime_s         = result.runtime_total_s;
	result.runtime_ms       -= result.runtime_s;
	result.runtime_min       = result.runtime_total_min;
	result.runtime_s        -= result.runtime_min;
	result.runtime_h         = result.runtime_total_h;
	result.runtime_min      -= result.runtime_h;
	result.runtime_d         = result.runtime_total_d;
	result.runtime_h        -= result.runtime_d;

	return result;
}



/*
 * simulation_run - run a simulation
 */
inline
void
simulation_run(simulation *sim)
{
	using namespace std::chrono;

	assert(sim != nullptr);
	sim->running = true;

	// measure code execution time, cumulative moving average
	steady_clock::time_point loop_start, loop_end, iter_start, iter_end;
	long double iter_cma = 0.0L;
	loop_start = steady_clock::now();

	bool is_last_iteration = false;
	sim->iteration.timeless = sim->config.timeless;
	while (sim->running) {
		// iteration time measurement
		iter_start = steady_clock::now();

		// TODO: handle any user command requests

		// TODO: if the simulation is paused, move it to the background
		if (sim->paused)
			continue;

		// in a timeless simulation, need to evaluate the stop-condition
		// callback. Due to the setup function, it is guaranteed in this case
		// that the callback is available
		if (sim->config.timeless && sim->callbacks.stop_condition(&sim->iteration, sim->callbacks.data)) {
			sim->running = false;
			break;
		}
		else {
			// in a timed simulation, it's unclear if the user specified a
			// stop-condition. If yes, then evaluate the stop condition and do
			// not evaluate the regular time comparison
			if (sim->callbacks.stop_condition && sim->callbacks.stop_condition(&sim->iteration, sim->callbacks.data)) {
				sim->running = false;
				break;
			}
		}

		// TODO: figure out if more information is needed in the callback
		// TODO: determine if there are callbacks that need to be called
		//       earlier, for instance to pause a simulation
		if (sim->callbacks.tick)
			sim->callbacks.tick(&sim->iteration, sim->callbacks.data);

		// this flag might have been set in the code block below during the
		// previous iteration. Read the long comment below to understand why.
		if (is_last_iteration) {
			sim->running = false;
			break;
		}

		// advance in time. For this, we need to compute the next delta dt. More
		// precisely, the next dt should be either the old dt, or a smaller
		// value in case we would overshoot the maximum target. Then, we need to
		// also check if we're within a certain epsilon due to numerical issues.
		if (!sim->config.timeless) {
			// compute the next delta-t, which is either the previous one, or in
			// case we're hitting the end of the simulation, the final "bit" of
			// time that needs' to be advanced.
			auto tdelta = sim->iteration.t_max - sim->iteration.t;
			sim->iteration.dt = std::fmin(sim->iteration.dt, tdelta);

			// evaluate if we're at the end of the simulation with the next
			// iteration or not
			if ((sim->iteration.dt + sim->iteration.t >= sim->iteration.t_max)
				// TODO: determine if the next check is correct or not
			|| (sim->iteration.dt + sim->config.t_eps >= tdelta)) {
				// If this branch is entered, then the next iteration will
				// either (almost numerically) precisely hit tmax or overshoot
				// it by a numerically irrelevant margin. In the first case, we
				// don't need to do anything, but in the latter we will slightly
				// adapt dt such that we will actually hit tmax precisely.
				//
				// Also, we need to carry over the information that the next is
				// the last iteration. The reason is that a simple comparison if
				// the current simulation time exceeds tmax of the simulation
				// will not work at the beginning of the loop due to the adaptive delta-t computation in the
				// lines above. For instance, the if-clause in the code
				//     if (t > tmax)
				//         break;
				// will always evaluate to false, and hence introduce an
				// infinite loop. And the reason why we cannot change this
				// comparison to (t >= tmax) is that the iteration callback is
				// called only afterwards. Moreover, the simulation actually
				// starts with t0, so advancing the time must happen _after_ the
				// iteration callback was invoked. Overall, this carry-over flag
				// is the cleanest way to check for the final iteration.
				is_last_iteration = true;
				sim->iteration.dt = tdelta;
			}
			sim->iteration.t += sim->iteration.dt;
		}

		// count the number of ticks, both in a timed as well as a timeless
		// simulation
		sim->iteration.ticks += 1;

		// measure simulation code execution time in a cumulative moving average
		// Note that the computation will break as soon as sim->iteration.ticks
		// overflows. This will, however, only rarely be the case for
		// excessively long simulations.
		iter_end = steady_clock::now();
		auto iter_time = duration_cast<nanoseconds>(iter_end - iter_start).count();
		iter_cma = iter_cma + (iter_time - iter_cma) / static_cast<double>(sim->iteration.ticks);
	}
	loop_end = std::chrono::steady_clock::now();

#if NCR_LOG_LEVEL >= NCR_LOG_LEVEL_DEBUG
	// runtime statistics
	auto stats = simulation_get_statistics(loop_start, loop_end);

	// emit some statistics
	// TODO: add iter_cma, expected_simtime_ms, and effective_simtime_ms to
	//       simulation_run_statistics.

	// this is the expected simulation time. It is 0 for a timeless simulation
	double expected_simtime_ms = sim->iteration.t_max - sim->iteration.t_0;
	// effective simulation time, which is the integrated time (which might
	// deviate from expected_simtime in case of numerical issues, but this
	// should be accounted for above
	double effective_simtime_ms = sim->iteration.t;

	log_debug(
		std::scientific,
		"Finished simulation\n",
		"    Simulation Mode:          ", sim->config.timeless ? "timeless" : "timed", "\n",
		"    Expected Simulated Time:  ", expected_simtime_ms, " ms (0 for timeless mode)\n"
		"    Effective Simulated Time: ", effective_simtime_ms, " ms (0 for timeless mode)\n",
		"    Simulated Ticks:          ", sim->iteration.ticks, "\n",
		"    Absolute Running Time:    ", stats.runtime_d.count(), "d ", stats.runtime_h.count(), "h ", stats.runtime_min.count(), "min ", stats.runtime_s.count(), "s ", stats.runtime_ms.count(), "ms\n",
		"    1 Iteration Runtime CMA:  ", iter_cma, " ns\n",
		"    Realtime Factor:          ", sim->config.timeless ? 0 : (effective_simtime_ms / stats.runtime_total_ms.count()), "\n");
#endif
}



/*
 * simulation_finish - tidy up a simulation.
 *
 * Note that this function will release all memory associated to / allocated by
 * its argument and set the argument to nullptr.
 */
inline
void
simulation_finish(simulation **sim)
{
	// TODO: rethink if this function is really necessary, or if the user is
	//       responsible for managing memory and ownership
	if (!sim || !(*sim)) return;
	delete *sim;
	*sim = nullptr;
}


/*
 * simulation_pause - pause a simulation
 */
inline
void
simulation_pause(simulation *sim)
{
	assert(sim != nullptr);
	sim->paused = true;
}


/*
 * simulation_resume - resume a simulation
 */
inline
void
simulation_resume(simulation *sim)
{
	assert(sim != nullptr);
	sim->paused = false;
}


/*
 * simulation_stop - force-stop a simulation
 */
inline
void
simulation_stop(simulation *sim)
{
	assert(sim != nullptr);
	sim->running = false;
}


} // ncr::
