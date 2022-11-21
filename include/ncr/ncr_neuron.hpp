/*
 * ncr_neuron - Spiking Neuron and synaptic plasticity models
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 */

/*
 * TODO: maybe introduce sub-namespace ncr::neurons ?
 */
#pragma once

#include <map>
#include <string>
#include <cmath>
#include <functional>

#include <ncr/ncr_log.hpp>
#include <ncr/ncr_units.hpp>
#include <ncr/ncr_utils.hpp>
#include <ncr/ncr_vector.hpp>
#include <ncr/ncr_numeric.hpp>

namespace ncr {

namespace Izhikevich { // {{{

// The Izhikevich neuron has a dimensionality of 2 (as it has to keep track of
// two variables over time)
constexpr static size_t Dimensionality = 2;


template <typename T = double>
struct Params {
	// The Izhikevich model is described by the coupled dynamical system
	// (omitting * to indicate multiplication)
	//
	//    v' = 0.05v^2 + 5v + 140 - u + I
	//    u' = a(bv - u)
	//
	// In these equations, v is the membrane potential and u is a recovery
	// variable.
	//
	// The parameters a, b, c, and d are described in detail below. Note that
	// for some specific neuron types, such as class 1 or class 2 excitable (as
	// described in Izhikevich 2004), the constants 5 and 140 need to be changed
	// as well. This is currently omitted in the implementation.

	// Time scale of the recovery variable u
	T a;

	// Sensitivity of the cerivery variable u to the subthreshold fluctuations
	// of v
	T b;

	// After-spike reset value of the membrane potential
	T c;

	// after-spike reset of the recovery variable u
	T d;

	// Typical initial membrane potential, only used for initialization
	T V0;

	// Spike threshold, indicative if the neuron spikes and used during
	// postprocessing of the dynamical system
	T thresh;
};


template <typename T = double>
struct State
{
	/*
	 * The main state of the neuron is given by two coupled dynamical variables
	 * u and v. They are stored within one vector here.
	 */
	vector_t<Dimensionality, T> v;

	/*
	 * The "reported" voltage v of the neuron. This is sligthly different than
	 * the dynamical system, as detailed in the postprocessing after a spike
	 * (see Izhikevich 2003 for more details).
	 */
	T v_reported;

	/*
	 * Flag indicating if the neuron is spiking after a forward integration
	 * step.
	 */
	bool spiking;
};


template <typename T = double>
struct Neuron
{
	// Each Izhikevich neuron has a certain base-type from which its parameters
	// are synthesized
	std::string type;

	// The parameters of this specific neuron, usually pre-configured during a
	// call to `make`.
	Params<T>   params;

	// The state of the neuron, which tracks if it is spiking, and the membrane
	// voltage. See `State` for details.
	State<T>    state;
};


template <typename T = double>
inline Params<T>
get_default_params(std::string type)
{
	if (type == "tonic_spiking")    return {.a =  0.02,  .b =  0.20, .c = -65.0, .d =   6.00, .V0 = -70.0_mV, .thresh = 30.0_mV};
	if (type == "phasic_spiking")   return {.a =  0.02,  .b =  0.25, .c = -65.0, .d =   6.00, .V0 = -64.0_mV, .thresh = 30.0_mV};
	if (type == "tonic_bursting")   return {.a =  0.02,  .b =  0.20, .c = -50.0, .d =   2.00, .V0 = -70.0_mV, .thresh = 30.0_mV};
	if (type == "phasic_bursting")  return {.a =  0.02,  .b =  0.25, .c = -55.0, .d =   0.05, .V0 = -64.0_mV, .thresh = 30.0_mV};
	if (type == "mixed_mode")       return {.a =  0.02,  .b =  0.20, .c = -55.0, .d =   4.00, .V0 = -70.0_mV, .thresh = 30.0_mV};
	if (type == "spike_freq_adapt") return {.a =  0.01,  .b =  0.20, .c = -65.0, .d =   8.00, .V0 = -70.0_mV, .thresh = 30.0_mV};

	// TODO: by default, use some fallback?
	log_error("Unknown Izhikevich neuron type \"", type, " in call to get_default_params.\n");
	return Params<T>{};
}


template <typename T = double>
inline auto
get_demo_input(std::string type) -> T(*)(T)
{
	if (type == "tonic_spiking")    return [](T t) -> T { if (t > 10.0_ms) return 14.0_mV; return 0.0_mV; };
	if (type == "phasic_spiking")   return [](T t) -> T { if (t > 20.0_ms) return  0.5_mV; return 0.0_mV; };
	if (type == "tonic_bursting")   return [](T t) -> T { if (t > 22.0_ms) return 15.0_mV; return 0.0_mV; };
	if (type == "phasic_bursting")  return [](T t) -> T { if (t > 20.0_ms) return  0.6_mV; return 0.0_mV; };
	if (type == "mixed_mode")       return [](T t) -> T { if (t > 10.0_ms) return 10.0_mV; return 0.0_mV; };
	if (type == "spike_freq_adapt") return [](T t) -> T { if (t > 10.0_ms) return 30.0_mV; return 0.0_mV; };

	log_error("Unknown Izhikevich neuron type \"", type, " in call to get_demo_input.\n");
	return [](T) -> T { return 0.0; };
}


template <typename T = double>
inline Neuron<T>
make(std::string type)
{
	Neuron<T> n;
	n.type = type;
	n.params = get_default_params<T>(type);

	n.state.v = {n.params.V0, n.params.b * n.params.V0};
	n.state.v_reported = n.params.V0;
	n.state.spiking = false;

	return n;
}


// type of a solver required for the differential equation of an Izhikevich
// Neuron
template <typename T, typename InputFunction>
using SolverType = odesolver_step_fn<Dimensionality, T, InputFunction, Neuron<T>&>;


/*
 * The default dynamical system of an Izhikevich neuron given some input
 * function
 */
template <typename T, typename InputFunction>
inline void
diffeq(
		const T t,
		const vector_t<Dimensionality, T> &y,
		vector_t<Dimensionality> &dydt,
		InputFunction input,
		Neuron<T> &neuron)
{
	// extract shorthands for neuron params required in the differential eq
	const T
		a = neuron.params.a,
		b = neuron.params.b;

	// retrieve input from the input function
	T Iext = input(t);

	// shorthand to values passed in
	const T v = y[0];
	const T u = y[1];

	// actual state equations according to Izhikevich 2003
	dydt[0] = 0.04*v*v + 5.0 * v + 140.0 - u + Iext;
	dydt[1] = a * (b*v - u);
}


template <typename T, typename InputFunction>
inline void
step(
		Neuron<T> &n,
		double &t,
		double &dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	// package the input and the neuron into a dptr. Going via a void pointer is
	// because of ncr_numeric's dptr implementation. TODO: reconsider if it
	// might be better to have an additional template parameter there

	vector_t<Dimensionality, T> y_out;
	solver(diffeq, t, dt, n.state.v, y_out, input, n);

	// copy integrator result to local state and postprocess
	n.state.v = y_out;

	// auxiliary test for spiking behavior as described in Izhikevich 2003
	if (n.state.v[0] > n.params.thresh) {
		n.state.v[0] = n.params.c;
		n.state.v[1] = n.state.v[1] + n.params.d;

		// set "reported" voltage to the threshold
		n.state.v_reported = n.params.thresh;
		n.state.spiking = true;
	}
	else {
		n.state.v_reported = n.state.v[0];
		n.state.spiking = false;
	}
}


template <typename T, typename InputFunction>
inline T
integrate(
		Neuron<T> &n,
		const T t,
		const T dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	// compute the target time to reach during integration
	T t_target = t+dt;

	// needs local dt to pass as reference to step(), in order to avoid
	// updating the callees dt
	T dt_tmp = dt;
	T t_tmp = t;

	// iterate until target time reached using local variables only
	while (t_tmp < t_target) {
		dt_tmp = std::min(dt_tmp, t_target - t_tmp);
		step(n, t_tmp, dt_tmp, input, solver);
	}

	return t_tmp;
}


} // Izhikevich::
  // }}}


/*
 * Implementation of the FitzhughNagumo neuron model
 */

namespace FitzhughNagumo { // {{{

constexpr static size_t Dimensionality = 2;

template <typename T = double>
struct Params
{
	T a = 1.0;
	T b = 1.0;
	T R = 1.0;
	T tau = 10.0_ms;
	T thresh = 1.8_mV;
};


template <typename T = double>
struct State
{
	/*
	 * The FH Neuron has two dynamic variables (V, w), merged into one single
	 * vector. There is no explicit spike tracking in the FH model as of yet.
	 */
	vector_t<Dimensionality, T> v{0.0, 0.0};
};


template <typename T = double>
struct Neuron
{
	// params and state of the neuron
	Params<T> params;
	State<T>  state;
};


template <typename T = double>
inline Neuron<T>
make()
{
	Neuron<T> n;
	n.state.v = {0.0, 0.0};
	return n;
}


template <typename T = double>
inline auto
get_demo_input() -> T(*)(T)
{
	return [](T) -> T { return 1.0; };
}


template <typename T, typename InputFunction>
using SolverType = odesolver_step_fn<Dimensionality, T, InputFunction, Neuron<T>&>;


template <typename T, typename InputFunction>
void
diffeq(
		const T t,
		const vector_t<Dimensionality, T> &y,
		vector_t<Dimensionality, T> &dydt,
		InputFunction input,
		Neuron<T> &neuron)
{
	// extract shorthands
	const T
		a   = neuron.params.a,
		b   = neuron.params.b,
		tau = neuron.params.tau,
		R   = neuron.params.R;

	// shorthands for v and w
	T v = y[0], w = y[1];

	// input = R * I. TODO: fixme, i.e. multiply by R
	T I = R * input(t);

	// actual state equations
	dydt[0] = v - (v*v*v)/3.0 - w + I;
	dydt[1] = 1 / tau * (v + a - b * w);
}


template <typename T, typename InputFunction>
void
step(
		Neuron<T> &neuron,
		T &t,
		T &dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	vector_t<Dimensionality, T> y_out;
	solver(diffeq, t, dt, neuron.state.v, y_out, input, neuron);
	neuron.state.v = y_out;
}


template <typename T, typename InputFunction>
inline T
integrate(
		Neuron<T> &n,
		const T t,
		const T dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	// compute the target time to reach during integration
	T t_target = t+dt;

	// needs local dt to pass as reference to step(), in order to avoid
	// updating the callees dt
	T dt_tmp = dt;
	T t_tmp = t;

	// iterate until target time reached using local variables only
	while (t_tmp < t_target) {
		dt_tmp = std::min(dt_tmp, t_target - t_tmp);
		step(n, t_tmp, dt_tmp, input, solver);
	}

	return t_tmp;
}


} // FitzhughNagumo::
  // }}}


/*
 * Adaptive Exponential Integrate and Fire Neuron Model
 *
 * see Fourcaud-Trocme et al 2003
 */
namespace AdEx { // {{{

constexpr static size_t Dimensionality = 2;

template <typename T = double>
struct Params
{
	// Resting Potential
	T V_rest = -65.0_mV;

	// After-spike reset potential
	T V_reset = -68.0_mV;

	// Spike and reset threshold
	T V_thresh = -30.0_mV;

	// Threshold for action potentials
	T V_ap_thresh = -59.9_mV;

	// Spike slope factor
	T delta_t = 3.48;

	// Sensitivity of the recovery variable (u, see below) on the membrane
	// potential v.
	T a = 1.0;

	// increment added to w (see State below) after spike
	T b = 1.0;

	// Membrane resistance
	T R = 1.0;

	// Membrane time constant
	T tau = 10.0_ms;

	// Adaptation current time constant
	T tau_w = 30.0_ms;
};


/*
 * State of an Adaptive Exponential IF Neuron
 */
template <typename T = double>
struct State
{
	/*
	 * The neuron's state consists of two coupled variable V and w, merged into
	 * one vector for purposes of numerical integration
	 */
	vector_t<Dimensionality, T> v;

	/*
	 * Flag indicating if the neuron is spiking or not
	 */
	bool spiking = false;
};


template <typename T = double>
struct Neuron
{
	Params<T> params;
	State<T>  state;
};


template <typename T = double>
auto get_demo_input() -> T (*)(T)
{
	return [](T t) -> T { if (t > 20.0_ms) return 10.0; return 0.0; };
}


template <typename T = double>
inline Neuron<T>
make()
{
	Neuron<T> n;

	n.state.v = {n.params.V_rest, 0.0};
	n.state.spiking = false;

	return n;
}


// type of a solver required for the differential equation of an Izhikevich
// Neuron
template <typename T, typename InputFunction>
using SolverType = odesolver_step_fn<Dimensionality, T, InputFunction, Neuron<T>&>;


template <typename T, typename InputFunction>
void
diffeq(
		const T t,
		const vector_t<Dimensionality, T> &y,
		vector_t<Dimensionality, T> &dydt,
		InputFunction input,
		Neuron<T> &neuron)
{
	// extract shorthands of the neuron's params to reduce length of code
	const T
		tau         = neuron.params.tau,
		V_rest      = neuron.params.V_rest,
		delta_t     = neuron.params.delta_t,
		V_ap_thresh = neuron.params.V_ap_thresh,
		R           = neuron.params.R,
		a           = neuron.params.a;

	// get the input at time t
	T Iext = input(t);

	dydt[0] = 1.0/tau * (
				-(y[0] - V_rest) + delta_t * std::exp((y[0] - V_ap_thresh) / delta_t) - R * y[1] + R * Iext
			);
	dydt[1] = 1.0/tau * (
				a * (y[0] - V_rest) - y[1]
			);
}


template <typename T, typename InputFunction>
inline void
step(
		Neuron<T> &n,
		double &t,
		double &dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	// buffer for ODE integrator step
	vector_t<Dimensionality, T> y_out;
	solver(diffeq, t, dt, n.state.v, y_out, input, n);

	// copy over result of the forward integration step
	n.state.v = y_out;

	// extract shorthand to improve legibility
	const T
		V_reset  = n.params.V_reset,
		V_thresh = n.params.V_thresh,
		b        = n.params.a;

	// determine if spike or not
	n.state.spiking = n.state.v[0] >= V_thresh;
	if (n.state.spiking) {
		n.state.v[0]  = V_reset;
		n.state.v[1] += b;
	}
}


template <typename T, typename InputFunction>
inline T
integrate(
		Neuron<T> &n,
		const T t,
		const T dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	// compute the target time to reach during integration
	T t_target = t+dt;

	// needs local dt to pass as reference to step(), in order to avoid
	// updating the callees dt
	T dt_tmp = dt;
	T t_tmp = t;

	// iterate until target time reached using local variables only
	while (t_tmp < t_target) {
		dt_tmp = std::min(dt_tmp, t_target - t_tmp);
		step(n, t_tmp, dt_tmp, input, solver);
	}

	return t_tmp;
}


} // AdEx::
  // }}}


/*
 * Adaptive Exponential Quadratic Integrate and Fire Neuron Model
 *
 * see Izhikevich 2004
 */
namespace AdExQuadratic { // {{{

constexpr static size_t Dimensionality = 2;

template <typename T = double>
struct Params
{
	// Resting Potential
	T V_rest = -65.0_mV;

	// After-spike reset potential
	T V_reset = -68.0_mV;

	// Spike and reset threshold
	T V_thresh = -30.0_mV;

	// Critical voltage for spike initiation
	T V_c = -50.0_mV;

	// Sensitivity of the recovery variable (u, see below) on the membrane
	// potential v.
	T a = 1.0;

	// increment added to w (see State below) after spike
	T b = 0.1;

	// membrane potential update coefficient
	T c = 0.07;

	// Membrane time constant
	T tau = 10.0_ms;

	// Adaptation current time constant
	T tau_w = 30.0_ms;
};


template <typename T = double>
struct State
{
	/*
	 * The neuron's state consists of two coupled variable V and w, merged into
	 * one vector for purposes of numerical integration
	 */
	vector_t<Dimensionality, T> v;

	/*
	 * Flag indicating if the neuron is spiking or not
	 */
	bool spiking;
};


template <typename T = double>
struct Neuron
{
	Params<T> params;
	State<T>  state;
};


template <typename T = double>
auto get_demo_input() -> T (*)(T)
{
	return [](T t) -> T { if (t > 20.0_ms) return 30.0; return 0.0; };
}

template <typename T = double>
inline Neuron<T>
make()
{
	Neuron<T> n;
	n.state.v = {n.params.V_rest, 0.0};
	n.state.spiking = false;
	return n;
}


template <typename T, typename InputFunction>
using SolverType = odesolver_step_fn<Dimensionality, T, InputFunction, Neuron<T>&>;


template <typename T, typename InputFunction>
inline void
diffeq(
		const T t,
		const vector_t<Dimensionality, T> &y,
		vector_t<Dimensionality> &dydt,
		InputFunction input,
		Neuron<T> &neuron)
{
	// extract shorthands of the neuron's params to reduce length of code
	const T
		tau    = neuron.params.tau,
		V_rest = neuron.params.V_rest,
		V_c    = neuron.params.V_c,
		a      = neuron.params.a,
		c      = neuron.params.b;

	T Iext = input(t);

	const T
		V = y[0],
		w = y[1];

	dydt[0] = 1.0/tau * (
			c * (V - V_rest) * (V - V_c) - w + Iext
			);
	dydt[1] = 1.0/tau * (
			a * (V - V_rest) - w
			);
}


template <typename T, typename InputFunction>
inline void
step(
		Neuron<T> &n,
		double &t,
		double &dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	// buffer for ODE integrator step
	vector_t<Dimensionality, T> y_out;
	solver(diffeq, t, dt, n.state.v, y_out, input, n);

	// copy over result of the forward integration step
	n.state.v = y_out;

	// extract shorthand to improve legibility
	const T
		V_reset  = n.params.V_reset,
		V_thresh = n.params.V_thresh,
		b        = n.params.a;

	// determine if spike or not
	n.state.spiking = n.state.v[0] >= V_thresh;
	if (n.state.spiking) {
		n.state.v[0]  = V_reset;
		n.state.v[1] += b;
	}
}

} // AdExQuadratic::
  // }}}


/*
 *
 * Leaky Integrate and Fire Neuron
 *
 */
namespace LeakyIF { // {{{

constexpr static size_t Dimensionality = 1;


template <typename T = double>
struct Params
{
	// Resting potential
	T V_rest   = 0.0_mV;

	// After-spike reset potential
	T V_reset  = -5.0_mV;

	// Spike-generation threshold
	T V_thresh = 20.0_mV;

	// Membrane time constant
	T tau      = 10.0_ms;

	// Refractory period length
	T tau_refractory = 5.0_ms;
};


template <typename T = double>
struct State
{
	/*
	 * the Leaky IF Neuron has only one dynamic variable: its membrane potential
	 */
	vector_t<Dimensionality, T> v;

	/*
	 * flag indicative if the neuron is spiking
	 */
	bool spiking;

	/*
	 * stores the last spike time, required for absolute refractory period
	 */
	T t_last_spike;
};


template <typename T = double>
struct Neuron
{
	// params and state of the neuron
	Params<T> params;
	State<T>  state;
};


template <typename T = double>
inline auto
get_demo_input() -> T (*)(T)
{
	return [](T t) -> T { if (t > 10.0_ms) return 25.0; return 0.0; };
}


template <typename T = double>
inline Neuron<T>
make()
{
	Neuron <T> n;
	n.state.v = {0.0};
	n.state.spiking = false;
	n.state.t_last_spike = -1e5;
	return n;
}


// type of a solver required for the differential equation of an Izhikevich
// Neuron
template <typename T, typename InputFunction>
using SolverType = odesolver_step_fn<Dimensionality, T, InputFunction, Neuron<T>&>;


template <typename T, typename InputFunction>
inline void
diffeq(
		const T t,
		const vector_t<Dimensionality, T> &y,
		vector_t<Dimensionality> &dydt,
		InputFunction input,
		Neuron<T> &neuron)
{
	// shorthand extraction
	const T
		tau    = neuron.params.tau,
		V_rest = neuron.params.V_rest;

	// get input for the current time step
	T Iext = input(t);

	// dynamical system specification
	dydt[0] = 1.0/tau * ((-y[0] - V_rest) + Iext);
}


template <typename T, typename InputFunction>
inline void
step(
		Neuron<T> &n,
		double &t,
		double &dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	vector_t<Dimensionality, T> y_out;

	// absolute refractory period?
	if (n.state.t_last_spike > 0) {
		n.state.t_last_spike -= dt;
		n.state.spiking = false;
	}
	else {
		solver(diffeq, t, dt, n.state.v, y_out, input, n);
		n.state.v = y_out;

		// determine if spike, and if yes, set stuff accordingly
		n.state.spiking = y_out[0] > n.params.V_thresh;
		if (n.state.spiking) {
			// Reset to reset potential
			n.state.v[0] = n.params.V_reset;

			// set absolute refractory period
			n.state.t_last_spike = n.params.tau_refractory;
		}
	}
}



} // LeakIF::
  // }}}


namespace QuadraticIF {
/*
 *
 * Quadratic Integrate and Fire Neuron
 *
 */

constexpr static size_t Dimensionality = 1;




template <typename T = double>
struct Params
{
	// Resting potential
	T V_rest = 0.0_mV;

	// After-spike reset potential
	T V_reset = -5.0_mV;

	// Spike-generation threshold
	T V_thresh = 20.0_mV;

	// Critical voltage for spike initiation
	T V_critical = -50.0_mV;

	// Membrane potential update coefficient
	T c = 0.07;

	// Membrane resistance
	T R = 1.0;

	// Membrane time constant
	T tau = 10.0_ms;

	// Refractory period length
	T tau_refractory = 5.0_ms;
};

template <typename T = double>
struct State
{
	/*
	 * the Quadratic IF Neuron has only one dynamic variable: its membrane potential
	 */
	vector_t<Dimensionality, T> v;

	/*
	 * flag indicative if the neuron is spiking
	 */
	bool spiking;

	/*
	 * stores the last spike time, required for absolute refractory period
	 */
	T t_last_spike;
};


template <typename T = double>
struct Neuron
{
	// params and state of the neuron
	Params<T> params;
	State<T>  state;
};


template <typename T = double>
inline auto
get_demo_input() -> T (*)(T)
{
	return [](T t) -> T { if (t > 10.0_ms) return 20.0; return 0.0; };
}


template <typename T = double>
inline Neuron<T>
make()
{
	Neuron<T> n;
	n.state.v = {0.0};
	n.state.spiking = false;
	n.state.t_last_spike = -1e5;
	return n;
}


template <typename T, typename InputFunction>
using SolverType = odesolver_step_fn<Dimensionality, T, InputFunction, Neuron<T>&>;


template <typename T, typename InputFunction>
inline void
diffeq(
		const T t,
		const vector_t<Dimensionality, T> &y,
		vector_t<Dimensionality> &dydt,
		InputFunction input,
		Neuron<T> &neuron)
{
	// shorthand extraction
	const T
		tau        = neuron.params.tau,
		V_rest     = neuron.params.V_rest,
		c          = neuron.params.c,
		R          = neuron.params.R,
		V_critical = neuron.params.V_critical;

	// get input for the current time step
	T Iext = input(t);

	// dynamical system specification
	const T V = y[0];
	dydt[0] = 1.0/tau * (c * (V - V_rest) * (V - V_critical) + R * Iext);
}


template <typename T, typename InputFunction>
inline void
step(
		Neuron<T> &n,
		double &t,
		double &dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	vector_t<Dimensionality, T> y_out;

	// absolute refractory period?
	if (n.state.t_last_spike > 0) {
		n.state.t_last_spike -= dt;
		n.state.spiking = false;
	}
	else {
		solver(diffeq, t, dt, n.state.v, y_out, input, n);
		n.state.v = y_out;

		// determine if spike, and if yes, set stuff accordingly
		n.state.spiking = y_out[0] > n.params.V_thresh;
		if (n.state.spiking) {
			// Reset to reset potential
			n.state.v[0] = n.params.V_reset;

			// set absolute refractory period
			n.state.t_last_spike = n.params.tau_refractory;
		}
	}
}


template <typename T, typename InputFunction>
inline T
integrate(
		Neuron<T> &n,
		const T t,
		const T dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	// compute the target time to reach during integration
	T t_target = t+dt;

	// needs local dt to pass as reference to step(), in order to avoid
	// updating the callees dt
	T dt_tmp = dt;
	T t_tmp = t;

	// iterate until target time reached using local variables only
	while (t_tmp < t_target) {
		dt_tmp = std::min(dt_tmp, t_target - t_tmp);
		step(n, t_tmp, dt_tmp, input, solver);
	}

	return t_tmp;
}


} // QuadraticIF ::
  // }}}


namespace GeneralizedIF {

/*
 *
 * Generalized Integrate-and-Fire Neuron
 *
 * Mihalas & Nieburg, "A Generalized Integrate-and-Fire Neural Model Produces
 * Diverse Spiking Behaviors", 2009, Neural Computation, Volume 21, Issue 3.
 *
 */
#if 0
template <size_t NInternalCurrents, typename T>
struct GeneralizedIFNeuron;


template <typename T = double>
using generalizedif_input_fn_t = T(*)(T t, void *dptr);


template <typename T = double>
T
generalizedif_demo_input(T t, void *)
{
	NCR_UNUSED(t);
	if (t > 10.0_ms)
		return 20.0;
	return 0.0;
}


template <size_t NInternalCurrents, typename T = double>
struct GeneralizedIFParams
{
	// Values taken from Mihalis & Niebur, 2009 (see in particular Table 1, and
	// its description)

	// Resting potential
	T V0 = -70.0_mV;

	// Voltage Reset after spike
	T V_reset = -70.0_mV;

	// Target threshold potential
	T Theta_inf = -50.0_mV;

	// Threshold potential reset value
	T Theta_reset = -60.0_mV;

	// ion channel resistance vector
	vector_t<NInternalCurrents, T> R;

	// ion channel model parameters s s s A
	vector_t<NInternalCurrents, T> A;

};

template <size_t NInternalCurrents, typename T = double>
struct GeneralizedIFState
{
	/*
	 * the Generalized IF Neuron has multiple dynamic variables: V, V_threshold,
	 * as well as all internal currents. In the paper by Mihalas & Niebur, 2 Input
	 * Channels are described. They are arrange in the vector as
	 *    v[0] = V
	 *    v[1] = V_thresh
	 *    v[2] = I_0
	 *    .
	 *    .
	 *    v[N+2] = I_N
	 */
	vector_t<GeneralizedIFNeuron<NInternalCurrents, T>::N, T> v;

	/*
	 * flag indicative if the neuron is spiking
	 */
	bool spiking;

	/*
	 * default constructor for the state of a Generalized IF neuron
	 */
	GeneralizedIFState()
	: spiking(false)
	{
		// initialize state vector
		v[0] = -70.0_mV;
		v[1] = -50.0_mV;
		for (size_t i = 0; i < NInternalCurrents; i++)
			v[i+2] = 0.0;
	}
};

template <size_t NInternalCurrents, size_t N, typename T = double>
void
generalizedif_diffeq(
		const T t,
		const vector_t<N, T> &y,
		vector_t<N, T> &dydt,
		void *dptr)
{
	static_assert(N == GeneralizedIFNeuron<NInternalCurrents, T>::N);
	if (!dptr) {
		log_error("Missing argument dptr to generalizedif_diffeq.\n");
		return;
	}
	GeneralizedIFNeuron<NInternalCurrents, T> *neuron = (GeneralizedIFNeuron<NInternalCurrents, T>*)dptr;

	// shorthand extraction
	const T tau        = neuron->params.tau;
	const T V_rest     = neuron->params.V_rest;
	const T c          = neuron->params.c;
	const T R          = neuron->params.R;
	const T V_critical = neuron->params.V_critical;

	// get input for the current time step
	T Iext = 0.0;
	if (neuron->input_fn)
		Iext = neuron->input_fn(t, neuron->input_fn_dptr);

	// dynamical system specification
}

template <size_t NInternalCurrents, typename T = double>
void
generalizedif_step(
		GeneralizedIFNeuron<NInternalCurrents, T> *neuron,
		T &t,
		T &dt,
		odesolver_step_fn<GeneralizedIFNeuron<NInternalCurrents, T>::N, T> step_fn = odesolve_step_rk2<GeneralizedIFNeuron<NInternalCurrents, T>::N, T>)
{
	vector_t<GeneralizedIFNeuron<NInternalCurrents, T>::N, T> y_out;

	// forward integration
	step_fn(neuron->diff_eq, t, dt, neuron->state.v, y_out, neuron);
	neuron->state.v = y_out;

	// determine if spike or not
	neuron->state.spiking = y_out[0] >= y_out[1];
	if (neuron->state.spiking) {
		neuron->state.y[0] = neuron->params.V_reset;

	}
}

template <size_t NInternalCurrents = 2, typename T = double>
struct GeneralizedIFNeuron
{
	// dimensions of the models' dynamical system, which is {V, V_thresh, Input1, ... InputN}
	constexpr static size_t N = 2 + NInternalCurrents;

	// params and state of the neuron
	GeneralizedIFParams<NInternalCurrents, T> params;
	GeneralizedIFState<NInternalCurrents, T>  state;

	// differential equation of the neuron (set to the default implementation)
	dynamical_system_fn<N, T> diff_eq = generalizedif_diffeq<NInternalCurrents, N, T>;

	// function wich provides input to the neuron, and additional data passed
	// along
	generalizedif_input_fn_t<T> input_fn = nullptr;
	void *input_fn_dptr = nullptr;

	/*
	 * set_input - set the input function to the neuron
	 */
	void
	set_input(generalizedif_input_fn_t<T> _input_fn, void *_dptr = nullptr)
	{
		this->input_fn = _input_fn;
		this->input_fn_dptr = _dptr;
	}

	/*
	 * step - forward integration for timestep t and a delta-t
	 *
	 * Note: depending on the integrator step function, dt might be changed. If
	 * this is the case and undesired, it might be better to call .integrate
	 * with the same arguments, which will forward integrate up until t+dt is
	 * reached.
	 */
	void
	step(
			T &t,
			T &dt,
			odesolver_step_fn<N, T> step_fn = odesolve_step_rk2<N, T>)
	{
		generalizedif_step(this, t, dt, step_fn);
	}

	/*
	 * integrate - forward integrate until t+dt is reached
	 */
	void
	integrate(
			T &t,
			T &dt,
			odesolver_step_fn<N, T> step_fn = odesolve_step_rk2<N, T>)
	{
		T t_target = t+dt;
		while (t < t_target) {
			dt = std::min(dt, t_target - t);
			step(t, dt, step_fn);
		}
	}
};
#endif


} // GeneralizeIF::
  // }}}



namespace HodgkinHuxley {

/*
 * Hodgkin-Huxley Neuron model implementation
 *
 * For more details on the HH Model, and also the constants and parameters used in
 * here, see the references
 *    * Hodgkin & Huxley, 1952, A Quantitative Description of Membrane Current
 *      and its Application to Conductance And Excitation in Nerve
 *      https://www.ncbi.nlm.nih.gov/pmc/articles/PMC1392413/pdf/jphysiol01442-0106.pdf
 *    * Gerstner, 2014, Neuronal Dynamics: From Single Neurons to Networks and Models of Cognition,
 *      the relevant chapter can be found at https://neuronaldynamics.epfl.ch/online/Ch2.S2.html
 *    * Prof. Jochen Braun, 2021, http://bernstein-network.de/wp-content/uploads/2021/02/04_Lecture-04-Hodgkin-Huxley-model.pdf
 *    * Ryan Siciliano, 2012, https://www.math.mcgill.ca/gantumur/docs/reps/RyanSicilianoHH.pdf
 *    * https://mrgreene09.github.io/computational-neuroscience-textbook/Ch4.html
 *	  * https://scarab.bates.edu/cgi/viewcontent.cgi?filename=5&article=1000&context=oer&type=additional
 *    * https://www.worldscientific.com/doi/abs/10.1142/S0218127405014349
 *    * https://www.math.mcgill.ca/gantumur/docs/reps/RyanSicilianoHH.pdf
 */

constexpr static size_t Dimensionality = 4;


template <typename T = double>
struct Params
{
	// Membrane Capacitance in µF/cm²
	T C_m  = 0.0;

	// ion channel and leak reversal potentials
	T E_Na = 0.0;
	T E_K  = 0.0;
	T E_l  = 0.0;

	// ion channel conductances in mS/cm²
	T g_Na = 0.0;
	T g_K  = 0.0;
	T g_l  = 0.0;

	// initial membrane potential
	T V0   = 0.0;

	// Spiking threshold. Note that the neuron model does not have an explicit
	// spike tracking mechanism, but as soon as the membrane Voltage goes above
	// this level, a Spike is assumed
	T V_thresh = 0.0;

	// pointers to functions that compute alphas and betas
	T (*alpha_n)(const T v) = nullptr;
	T (*alpha_m)(const T v) = nullptr;
	T (*alpha_h)(const T v) = nullptr;

	T (*beta_n)(const T v)  = nullptr;
	T (*beta_m)(const T v)  = nullptr;
	T (*beta_h)(const T v)  = nullptr;
};


template <typename T = double>
inline auto
get_demo_input(std::string type) -> T(*)(T)
{
	if (type == "classical") return [](T t) -> T {if (t > 10.0_ms /* && t < 15.0_ms */) return  0.1_mV; return 0.0; };
	if (type == "gerstner")  return [](T t) -> T {if (t > 10.0_ms /* && t < 15.0_ms */) return 10.0_mV; return 0.0; };

	log_error("Unknown Hodgkin Huxley type \"", type, ".\n");
	return [](T) -> T { return 0.0; };
}


template <typename T>
T __hh_n_inf(const T v, Params<T> &params)
{
	return params.alpha_n(v) / (params.alpha_n(v) + params.beta_n(v));
}


template <typename T>
T __hh_m_inf(const T v, Params<T> &params)
{
	return params.alpha_m(v) / (params.alpha_m(v) + params.beta_m(v));
}


template <typename T>
T __hh_h_inf(const T v, Params<T> &params)
{
	return params.alpha_h(v) / (params.alpha_h(v) + params.beta_h(v));
}


template <typename T = double>
inline Params<T>
get_default_params(std::string type)
{
	if (type == "classical")
	/*
	 * parameters as well as alpha and beta functions for the HH model. The
	 * values are based on the original paper by Hodgkin & Huxley, and follows
	 * some modifications as presented in, e.g. Prof. Jochen Braun's lectures
	 * notes (see references above) or also in Ryan Siciliano's implementation.
	 * Another source where these parameters were listed as below is in James K
	 * Peterson's book Bioinformation Processing: A Primer on Computational
	 * Cognitive Science, 2016 (see page 145).
	 */
		return {
				.C_m      =   0.01,
				.E_Na     =  55.17_mV,
				.E_K      = -72.14_mV,
				.E_l      = -49.42_mV,
				.g_Na     =   1.2,
				.g_K      =   0.36,
				.g_l      =   0.003,
				.V0       = -60.0_mV,
				.V_thresh =  20.0_mV,
				.alpha_n  = [](const T v){ return 0.01  * (v + 50.0) / (1.0 - std::exp(-(v + 50.0) / 10.0)); },
				.alpha_m  = [](const T v){ return 0.1   * (v + 35.0) / (1.0 - std::exp(-(v + 35.0) / 10.0)); },
				.alpha_h  = [](const T v){ return 0.07  * std::exp(-0.05 * (v + 60.0)); },
				.beta_n   = [](const T v){ return 0.125 * std::exp(-(v + 60.0) / 80.0); },
				.beta_m   = [](const T v){ return 4.0   * std::exp(-0.0556 * (v + 60.0)); },
				.beta_h   = [](const T v){ return 1.0   / (1.0 + std::exp(-0.1 * (v + 30.0))); },
		};

	if (type == "gerstner")
	/*
	 * parameters, alpha, and beta functions as reported in Gerstner, 2014. The
	 * book itself cites Zach Mainen and Huguenard et al. for the precise
	 * parameters listed below.
	 *
	 * Note/XXX: these values don't lead to a working model. There are some
	 * errors in here, but so far it's not clear what precisely is wrong.
	 */
		return {
				.C_m      =   1.0,
				.E_Na     =  55.0_mV,
				.E_K      = -77.0_mV,
				.E_l      = -65.0_mV,
				.g_Na     =  40.0,
				.g_K      =  35.0,
				.g_l      =   0.3,
				.V0       = -65.0_mV,
				.V_thresh =  20.0_mV,
				.alpha_n  = [](const T v){ return  0.02  * (v - 25.0) / (1.0 - std::exp(-(v - 25.0)/9.0)); },
				.alpha_m  = [](const T v){ return  0.182 * (v + 35.0) / (1.0 - std::exp(-(v + 35.0)/9.0)); },
				.alpha_h  = [](const T v){ return  0.25  * std::exp(-(v + 90.0)/12.0); },
				.beta_n   = [](const T v){ return -0.002 * (v - 25.0) / (1.0 - std::exp((v - 25.0)/9.0)); },
				.beta_m   = [](const T v){ return -0.124 * (v + 35.0) / (1.0 - std::exp((v + 35.0)/9.0)); },
				.beta_h   = [](const T v){ return  0.25  * std::exp((v + 62.0)/6.0) / std::exp((v + 90.0)/12.0); },
		};


	log_error("Unknown Hodgkin Huxley type \"", type, ".\n");
	return {};
}


template <typename T = double>
struct State
{
	/*
	 * The Hodgkin-Huxley Neuron model consists of a dynamical system of four
	 * coupled variables V (in the paper called V_m), n, m, h. They are merged
	 * into the single vector v here, with V = v[0], n = v[1], m = v[2], and h =
	 * v[3].
	 */
	vector_t<Dimensionality, T> v;

	/*
	 * Flag to track if the neuron is spiking
	 */
	bool spiking;
};


template <typename T = double>
struct Neuron
{
	// identified of the parameter set used for the HH model
	std::string            type;

	// params and state of the neuron
	Params<T> params;
	State<T>  state;

};


template <typename T = double>
inline Neuron<T>
make(std::string type)
{
	Neuron<T> n;

	n.type = type;
	n.params = get_default_params<T>(type);
	n.state.v = {
		n.params.V0,
		__hh_n_inf(n.params.V0, n.params),
		__hh_m_inf(n.params.V0, n.params),
		__hh_h_inf(n.params.V0, n.params)};
	n.state.spiking = false;

	return n;
}


// type of a solver required for the differential equation of an Izhikevich
// Neuron
template <typename T, typename InputFunction>
using SolverType = odesolver_step_fn<Dimensionality, T, InputFunction, Neuron<T>&>;


template <typename T, typename InputFunction>
inline void
diffeq(
		const T t,
		const vector_t<Dimensionality, T> &y,
		vector_t<Dimensionality> &dydt,
		InputFunction input,
		Neuron<T> &neuron)
{

	// extract shorthands from neuron.params
	const T
		C_m  = neuron.params.C_m; // µF/cm²

	// ion channel and leak reversal potentials
	const T
		E_Na = neuron.params.E_Na,
		E_K  = neuron.params.E_K,
		E_l  = neuron.params.E_l;

	// ion channel conductances
	const T
		g_Na = neuron.params.g_Na,
		g_K  = neuron.params.g_K,
		g_l  = neuron.params.g_l;

	// retrieve external input
	T Iext = input(t);

	// get current state from state vector
	const T
		V = y[0],
		n = y[1],
		m = y[2],
		h = y[3];

	// compute ion channels
	const T I_Na = g_Na * (V - E_Na) * std::pow(m, 3.0) * h;
	const T I_K  = g_K  * (V - E_K)  * std::pow(n, 4.0);
	const T I_l  = g_l  * (V - E_l);

	// compute alphas and betas
	const T
		a_n = neuron.params.alpha_n(V),
		a_m = neuron.params.alpha_m(V),
		a_h = neuron.params.alpha_h(V),

		b_n = neuron.params.beta_n(V),
		b_m = neuron.params.beta_m(V),
		b_h = neuron.params.beta_h(V);

	// specify dynamical system
	dydt[0] = (1.0 / C_m) * (Iext - (I_Na + I_K + I_l));
	dydt[1] = a_n * (1.0 - n) - b_n * n;
	dydt[2] = a_m * (1.0 - m) - b_m * m;
	dydt[3] = a_h * (1.0 - h) - b_h * h;
}


template <typename T, typename InputFunction>
inline void
step(
		Neuron<T> &n,
		double &t,
		double &dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	vector_t<Dimensionality, T> y_out;
	solver(diffeq, t, dt, n.state.v, y_out, input, n);
	n.state.v = y_out;
	n.state.spiking = n.state.v[0] > n.params.V_thresh;
}


template <typename T, typename InputFunction>
inline T
integrate(
		Neuron<T> &n,
		const T t,
		const T dt,
		InputFunction input,
		SolverType<T, InputFunction> solver = odesolve_step_rk2)
{
	// compute the target time to reach during integration
	T t_target = t+dt;

	// needs local dt to pass as reference to step(), in order to avoid
	// updating the callees dt
	T dt_tmp = dt;
	T t_tmp = t;

	// iterate until target time reached using local variables only
	while (t_tmp < t_target) {
		dt_tmp = std::min(dt_tmp, t_target - t_tmp);
		step(n, t_tmp, dt_tmp, input, solver);
	}

	return t_tmp;
}




} // HodgkinHuxley::
  // }}}

/*
 *
 * STDP Synapses
 *
 */


/*
 * Basic STDP Synapse
 */

// forward declarations
template <typename T>
struct STDPSynapseParams;

template <typename T>
struct STDPSynapseState;

template <typename T>
struct STDPSynapse;

/*
 * STDPSynapseParams - Parameters of an STDP Synapse
 */
template <typename T = double>
struct STDPSynapseParams
{
	// Time constants for the STDP kernel, individually specified for the pre-
	// and post-spike time window.
	T tau_pre;
	T tau_post;

	// reset variables for synaptic conductanes whenever a spike appears.
	T c_a_pre;
	T c_a_post;

	// Learning rate of this synapse
	T eta;

	// Weight setup, i.e. initial weights, minimal and maximal weight
	T w0;
	T w_min;
	T w_max;
};


/*
 * stdpsynapse_get_params - get parameters for an STDP Synapse
 *
 * Note: the default parameter set is the one used in the Theta STDP
 * Decorrelation experiment described in Waniek 2018 (PhD Thesis). More
 * precisely, the STDP kernel is asymetric with a longer pre-spike tail for
 * sharp temporal integration.
 */
template <typename T>
STDPSynapseParams<T>
stdpsynapse_get_params(std::string param_type_str = "theta_decorrelation")
{
	static std::map<std::string, STDPSynapseParams<T>> stdp_types;
	static const std::string default_type = "theta_decorrelation";

	/*
	 * Parameters used for the Theta STDP decorrelation experiment in Waniek
	 * 2018.
	 */
	stdp_types["theta_decorrelation"] =
	{
		.tau_pre  =  0.035,
		.tau_post =  0.080,
		.c_a_pre  =  0.010,
		.c_a_post = -0.005,
		.eta      =  1.0,
		.w0       =  0.05,
		.w_min    =  0.0,
		.w_max    =  0.4,
	};

	// TODO: have additional parameters from other publications

	if (!stdp_types.contains(param_type_str)) {
		log_warning("Unknown STDP Synapse parameter set \"", param_type_str, "\". Using fallback \"", default_type, "\" instead.\n");
		return stdp_types[default_type];
	}
	return stdp_types[param_type_str];
}


/*
 * stdpsynapse_diffeq - differential equations for the synape's state
 */
template <size_t N, typename T = double>
void
stdpsynapse_diffeq(
		const T t,
		const vector_t<N, T> &y,
		vector_t<N, T> &dydt,
		void *dptr)
{
	static_assert(N == STDPSynapse<T>::N);
	if (!dptr) {
		log_error("Missing argument dptr to stdpsynapse_diffeq.\n");
		return;
	}
	STDPSynapse<T> *synapse = (STDPSynapse<T>*)dptr;

	// TODO: fill in
	NCR_UNUSED(t, y, dydt);
}




/*
 * stdpsynapse_pre_spike - treatment of the dynamics before a spike occurs
 */


/*
 * stdpsynapse_post_spike - treatment of the dynamics after a spike occured
 */


/*
 * STDPSynapseState - State of an STDP Synapse
 */
template <typename T = double>
struct STDPSynapseState
{
	// The state of a simple STDP Synapse is given by two dynamic variables, one
	// accounting for the pre-spike dynamics, and one for post-spike dynamics
	vector_t<STDPSynapse<T>::N, T> a;

	STDPSynapseState()
	: a{0.0, 0.0}
	{ }
};


template <typename T = double>
struct STDPSynapse
{
	// the synapse has dimensionality 2 (i.e. it uses two dynamic variables)
	constexpr static size_t N = 2;

	STDPSynapseParams<T> params;
	STDPSynapseState<T>  state;

	STDPSynapse(std::string _param_type_str)
	: params(stdpsynapse_get_params<T>(_param_type_str))
	{}
};


/*
 * Triplet Rule STDP Synapse
 */
struct TripletSTDPSynapse
{
};



/*
 *
 * Populations of identical neurons
 *
 */
template <typename NeuronType>
struct Population {};


} // ncr::
