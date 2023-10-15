/*
 * ncr_numeric - numeric algorithms, such as ODEsolvers
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 */

/*
 * TODO: documentation has old function names
 */
#pragma once

#include <cmath>
#include <utility>
#include <vector>

// vector is required for N-D ODE solvers
#include <ncr/ncr_vector.hpp>

namespace ncr {


/*
 * Simple ODE solver implementation
 */


// 1D specialization
using differential_1D_fn = double(*)(double t, double y);

using solver_step_1D_fn  =
	double(*)(differential_1D_fn, double &t, double &dt, double y);


/*
 * N-D function (callback) for a dynamical system of dimension N
 */
template<size_t N, typename T = double, typename... Args>
using dynamical_system_fn =
	void(*)(
			// time of evaluation
			const T t,
			// the values of the dynamical system at t
			const vector_t<N, T> &y,
			// the derivative of the dynamical system. The user must provide the
			// computation of this
			vector_t<N, T> &dydt,
			// additional custom data pointer that a user might want to pass
			// along (see also comment on this pointer in the description of
			// odesolver_step_fn below).
			Args... dptr
	);


/*
 * generic definition of a step function to solve ODEs
 */
template <size_t N, typename T = double, typename... Args>
using odesolver_step_fn =
	void (*)(
		// the dynamical system that should be integrated
		const dynamical_system_fn<N, T, Args...> fn,
		// current time for the integration step
		T &t,
		// delta-t (dt) for the integration step
		T &dt,
		// vector of "initial" values of the dynamical system at time t
		const vector_t<N, T> &y_in,
		// vector which will contain the output of the integration
		vector_t<N, T>       &y_out,
		// additional pointer to data that will be forwarded to the dynamical
		// system
		Args... args);


/*
 * ode_solve_step_euler - single Euler step integrator
 */
inline
double
odesolve_step_euler_1D(
		differential_1D_fn f,
		double &t,
		double &dt,
		double y)
{
	// compute single step euler
	double result = y + dt * f(t, y);
	// next position.
	// Note that this is numerically not exactly safe. For instance, integration
	// of multiple steps h of size 0.1, where 0.1 is a double, will lead to
	// rounding errors, because x + h will be x0 + h + h + h ... + h, and 0.1
	// cannot be properly represented in IEEE 754 floats. To some extend, this
	// will be caught in ode_solve, but this is not a guarantee that this is
	// entirely safe to use.
	t += dt;
	return result;
}


template <size_t N, typename T = double, typename... Args>
void
odesolve_step_euler(
		const dynamical_system_fn<N, T, Args...> fn,
		T &t,
		T &dt,
		const vector_t<N, T> &y_in,
		vector_t<N, T>       &y_out,
		Args... args)
{
	// this computes y = y + dt * f(t, y)
	fn(t, y_in, y_out, std::forward<Args>(args)...);
	y_out *= dt;
	y_out += y_in;
	t += dt;
}


/*
 * ode_solve_step_rk2 - 2nd order Runge Kutta integrator
 */
inline
double
odesolve_step_rk2_1D(
		differential_1D_fn f,
		double &t,
		double &dt,
		double y)
{
	// integration
	double k1     = dt * f(t, y);
	double k2     = dt * f(t + dt, y + k1);
	double result = y + 0.5 * (k1 + k2);

	// next position (Note: see comment in _step_euler)
	t += dt;

	return result;
}


/*
 * odesolve_step_rk2 - Runke Kutta 2nd order integrator for an nD system
 */
template <size_t N, typename T, typename... Args>
void
odesolve_step_rk2(
		const dynamical_system_fn<N, T, Args...> fn,
		T &t,
		T &dt,
		const vector_t<N, T> &y_in,
		vector_t<N, T>       &y_out,
		Args... args)
{
	// based on the following python code
	//
	// def ODEsolveStepRK2(f, x, y, h):
	//     xn = x + h
    //     k1 = dt * f(t, y)
    //     k2 = dt * f(t + dt, y + k1)
    //     yn = y + 0.5 * (k1 + k2)
    //     return xn, yn, h

	// temporaries : TODO: can reduce one
	vector_t<N, T> k1, k2;

	// compute intermediate step k1
	fn(t, y_in, k1, std::forward<Args>(args)...);
	k1 *= dt;

	// compute intermediate step k2
	fn(t + dt, y_in + k1, k2, std::forward<Args>(args)...);
	k2 *= dt;

	// compute final value
	y_out = y_in + 0.5 * (k1 + k2);

	// update time
	t += dt;
}


/*
 * ode_solve_step_rk4 - 4th order Runge Kutta integrator
 */
inline
double
odesolve_step_rk4_1D(
		differential_1D_fn f,
		double &t,
		double &dt,
		double y)
{
	double result;

	// RK4 integration
    double k1 = dt * f(t,              y);
    double k2 = dt * f(t + 1./2. * dt, y + 1./2. * k1);
    double k3 = dt * f(t + 1./2. * dt, y + 1./2. * k2);
    double k4 = dt * f(t + dt,         y + k3);

    result = y + 1.0/6.0 * (k1 + 2.0*k2 + 2.0*k3 + k4);

	// next position (see comment in _step_euler)
	t += dt;

	return result;
}


/*
 * odesolve_step_rk4 - Runke Kutta 4th order integrator for an nD system
 */
template <size_t N, typename T = double, typename... Args>
void
odesolve_step_rk4(
		const dynamical_system_fn<N, T, Args...> fn,
		T &t,
		T &dt,
		const vector_t<N, T> &y_in,
		vector_t<N, T>       &y_out,
		Args... args)
{
	// TODO: in this function, several temporaries get created. try to reduce
	//       the number

	vector_t<N, T> k1, k2, k3, k4;

	// compute k1: k1 = dt * f(t, y);
	fn(t, y_in, k1, std::forward<Args>(args)...);
	k1 *= dt;

	// compute k2: k2 = dt * f(t + 1./2. * dt, y + 1./2. * k1);
	fn(t + 0.5 * dt, y_in + 0.5 * k1, k2, std::forward<Args>(args)...);
	k2 *= dt;

	// compute k3: k3 = dt * f(t + 1./2. * dt, y + 1./2. * k2);
	fn(t + 0.5 * dt, y_in + 0.5 * k2, k3, std::forward<Args>(args)...);
	k3 *= dt;

	// compute k4: h * f(t + dt,         y + k3);
	fn(t + dt, y_in + k3, k4, std::forward<Args>(args)...);
	k4 *= dt;

	// assemble: yn = y + 1.0/6.0 * (k1 + 2.0*k2 + 2.0*k3 + k4);
	y_out = y_in + 1.0/6.0 * (k1 + 2.0 * k2 + 2.0 * k3 + k4);

	// next time step (see comment in _step_euler_1D regarding stability)
	t += dt;
}


/*
 * adaptive Runke Kutta according to Cash & Karp, 1990
 */
inline
double
odesolve_step_rkck_adaptive_1D(
		differential_1D_fn f,
		double &t,
		double &dt,
		double y)
{
	// TODO: make the tolerance configurable
	constexpr double tolerance = 1.0e-10;
	double error = 2. * tolerance;

	double result;
	const double orig_t = t;

    while (error > tolerance) {
		// update time
        t = orig_t + dt;

        // for why those numbers, see Cash & Karp, 1990, Table (5) on page 206.
        double k1 = dt * f(t, y);
        double k2 = dt * f(t + (1./5.) * dt, y + (1./5.)        * k1);
        double k3 = dt * f(t + (3./10.)* dt, y + (3./40.)       * k1 + (9./40.)    * k2);
        double k4 = dt * f(t + (3./5.) * dt, y + (3./10.)       * k1 - (9./10.)    * k2 + (6./5.)       * k3);
        double k5 = dt * f(t + (1./1.) * dt, y - (11./54.)      * k1 + (5./2.)     * k2 - (70./27.)     * k3 + (35./27.)        * k4);
        double k6 = dt * f(t + (7./8.) * dt, y + (1631./55296.) * k1 + (175./512.) * k2 + (575./13824.) * k3 + (44275./110592.) * k4 + (253./4096.) * k5);

        // compute 4th and 5th order estimates
        double yn4 = y + (37./378.)     * k1 + (250./621.)     * k3 + (125./594.)     * k4 +                      (512./1771.) * k6;
        double yn5 = y + (2825./27648.) * k1 + (18575./48384.) * k3 + (13525./55296.) * k4 + (277./14336.) * k5 + (1./4.)      * k6;

        error = fabs(yn4 - yn5);
        if (error != 0.) {
            dt = 0.8 * dt * pow((tolerance / error), (1./4.));
		}
        result = yn4;
	}
	return result;
}


/*
 * odesolve_step_rkck_adaptive - adaptive Runke Kutta according to Cash & Karp, 1990
 */
template <size_t N, typename T = double, typename... Args>
void
odesolve_step_rkck_adaptive(
		const dynamical_system_fn<N, T, Args...> fn,
		T &t,
		T &dt,
		const vector_t<N, T> &y_in,
		vector_t<N, T>       &y_out,
		Args... args)
{
	// TODO: this function generates many temporaries, try to reduce this
	vector_t<N, T> k1, k2, k3, k4, k5, k6, yn4, yn5;

	// TODO: make the tolerance configurable
	constexpr T tolerance = 1.0e-10;
	T error = 2. * tolerance;

	const double orig_t = t;

    while (error > tolerance) {
		// update to next step
        t = orig_t + dt;

        // for why those numbers, see Cash & Karp, 1990, Table (5) on page 206.
        fn(t, y_in, k1, std::forward<Args>(args)...);
		k1 *= dt;

        fn(t + (1./5.) * dt, y_in + (1./5.)        * k1, k2, std::forward<Args>(args)...);
		k2 *= dt;

        fn(t + (3./10.)* dt, y_in + (3./40.)       * k1 + (9./40.)    * k2, k3, std::forward<Args>(args)...);
		k3 *= dt;

        fn(t + (3./5.) * dt, y_in + (3./10.)       * k1 - (9./10.)    * k2 + (6./5.)       * k3, k4, std::forward<Args>(args)...);
		k4 *= dt;

        fn(t + (1./1.) * dt, y_in - (11./54.)      * k1 + (5./2.)     * k2 - (70./27.)     * k3 + (35./27.)        * k4, k5, std::forward<Args>(args)...);
		k5 *= dt;

        fn(t + (7./8.) * dt, y_in + (1631./55296.) * k1 + (175./512.) * k2 + (575./13824.) * k3 + (44275./110592.) * k4 + (253./4096.) * k5, k6, std::forward<Args>(args)...);
		k6 *= dt;

        // compute 4th and 5th order estimates
        yn4 = y_in + (37./378.)     * k1 + (250./621.)     * k3 + (125./594.)     * k4 +                      (512./1771.) * k6;
        yn5 = y_in + (2825./27648.) * k1 + (18575./48384.) * k3 + (13525./55296.) * k4 + (277./14336.) * k5 + (1./4.)      * k6;

		// TODO: this generates a temporary, maybe remove
        error = (yn4 - yn5).asum();
        if (error != 0.) {
            dt = 0.8 * dt * pow((tolerance / error), 0.25);
		}
        y_out = yn4;
	}
}


/*
 * adaptive Runge Kutta according to Dormand & Prince, 1980
 * This is the version that's usually used in ode45 in Matlab
 */
inline
double
odesolve_step_rkdp_adaptive_1D(
		differential_1D_fn f,
		double &t,
		double &dt,
		double y)
{
	// TODO: make the tolerance configurable
	constexpr double tolerance = 1.0e-10;
	double error = 2. * tolerance;

	double result;
	const double orig_t = t;

    while (error > tolerance) {
		// update next step
        t = orig_t + dt;

        double k1 = dt * f(t, y);
        double k2 = dt * f(t + (1./5.)  * dt, y + (1./5.)        * k1);
        double k3 = dt * f(t + (3./10.) * dt, y + (3./40.)       * k1 + (9./40.)       * k2);
        double k4 = dt * f(t + (4./5.)  * dt, y + (44./45.)      * k1 - (56./15.)      * k2 + (32./9.)       * k3);
        double k5 = dt * f(t + (8./9.)  * dt, y + (19372./6561.) * k1 - (25360./2187.) * k2 + (64448./6561.) * k3 - (212./729.)  * k4);
        double k6 = dt * f(t + (1./1.)  * dt, y + (9017./3168.)  * k1 - (355./33.)     * k2 + (46732./5247.) * k3 + (49./176.)   * k4 - (5103./18656.) * k5);
        double k7 = dt * f(t + (1./1.)  * dt, y + (35./384.)     * k1                       + (500./1113.)   * k3 + (125./192.)  * k4 - (2187./6784.)  * k5 + (11./84.) * k6);

        // compute estimates
        double yn5 = y + (35./384.)     * k1 + (500./1113.)   * k3 + (125./192.) * k4 - (2187./6784.)    * k5 + (11./84.)    * k6;
        double yn6 = y + (5179./57600.) * k1 + (7571./16695.) * k3 + (393./640.) * k4 - (92097./339200.) * k5 + (187./2100.) * k6 + (1./40.) * k7;

        error = fabs(yn5 - yn6);
        // TODO: check if the adaptation is numerically correct
        if (error != 0.) {
            dt = 0.8 * dt * pow((tolerance / error), (1./4.));
		}
        result = yn5;
	}
	return result;
}


/*
 * adaptive Runge Kutta according to Dormand & Prince, 1980
 * This is the version that's usually used in ode45 in Matlab
 */
template <size_t N, typename T, typename... Args>
void
odesolve_step_rkdp_adaptive(
		const dynamical_system_fn<N, T, Args...> fn,
		T &t,
		T &dt,
		const vector_t<N, T> &y_in,
		vector_t<N, T>       &y_out,
		Args... args)
{
	// TODO: this function generates many temporaries, try to reduce this
	vector_t<N, T> k1, k2, k3, k4, k5, k6, k7, yn5, yn6;

	// TODO: make the tolerance configurable
	constexpr T tolerance = 1.0e-10;
	T error = 2. * tolerance;

	const double orig_t = t;

	while (error > tolerance) {
		// update next step
        t = orig_t + dt;

        fn(t, y_in, k1, std::forward<Args>(args)...);
		k1 *= dt;

        fn(t + (1./5.)  * dt, y_in + (1./5.)        * k1, k2, std::forward<Args>(args)...);
		k2 *= dt;

        fn(t + (3./10.) * dt, y_in + (3./40.)       * k1 + (9./40.)       * k2, k3, std::forward<Args>(args)...);
		k3 *= dt;

        fn(t + (4./5.)  * dt, y_in + (44./45.)      * k1 - (56./15.)      * k2 + (32./9.)       * k3, k4, std::forward<Args>(args)...);
		k4 *= dt;

        fn(t + (8./9.)  * dt, y_in + (19372./6561.) * k1 - (25360./2187.) * k2 + (64448./6561.) * k3 - (212./729.)  * k4, k5, std::forward<Args>(args)...);
		k5 *= dt;

        fn(t + (1./1.)  * dt, y_in + (9017./3168.)  * k1 - (355./33.)     * k2 + (46732./5247.) * k3 + (49./176.)   * k4 - (5103./18656.) * k5, k6, std::forward<Args>(args)...);
		k6 *= dt;

        fn(t + (1./1.)  * dt, y_in + (35./384.)     * k1                       + (500./1113.)   * k3 + (125./192.)  * k4 - (2187./6784.)  * k5 + (11./84.) * k6, k7, std::forward<Args>(args)...);
		k7 *= dt;

        // compute estimates
        yn5 = y_in + (35./384.)     * k1 + (500./1113.)   * k3 + (125./192.) * k4 - (2187./6784.)    * k5 + (11./84.)    * k6;
        yn6 = y_in + (5179./57600.) * k1 + (7571./16695.) * k3 + (393./640.) * k4 - (92097./339200.) * k5 + (187./2100.) * k6 + (1./40.) * k7;

        error = (yn5 - yn6).asum();
        // TODO: check if the adaptation is numerically correct
        if (error != 0.) {
            dt = 0.8 * dt * pow((tolerance / error), (1./4.));
		}
        y_out = yn5;
	}
}


/*
 * result of a solver call consists of the values as well as the steps where the
 * values were computed
 */
struct solver_result_1D_t {
	std::vector<double> ys;
	std::vector<double> ts;
};


/*
 * ode_solve - solve a system with a given integrator
 */
inline
solver_result_1D_t
odesolve_1D(
		solver_step_1D_fn solver,
		differential_1D_fn f,
		double t0,
		double tmax,
		double dt,
		double yInit)
{
	solver_result_1D_t result;
	result.ys.push_back(yInit);
	result.ts.push_back(t0);

	// epsilon to check during computation of dt to catch the most sever
	// rounding errors at the end of an integration
	constexpr double eps = 1e-10;

	double t = t0;
	double yn = yInit;
	double step_result;
	while (t < tmax) {
		// don't exceed tmax during integration
		dt = std::min(dt, tmax - t);
		// try to minimize rounding errors
		if (dt + eps >= tmax - t)
			dt = tmax - t;

		// integration step
		step_result = solver(f, t, dt, yn);

		// move results out to the caller
		result.ys.push_back(step_result);
		result.ts.push_back(t);

		// update yn (note: t and dt were changed by calling solver above)
		yn = step_result;
	}
	return result;
}



/*
 *
 */
template <typename RealType = double>
inline RealType
bessi0(RealType x)
{
	RealType ax, ans;
	RealType y;

	if ((ax=std::abs(x)) < 3.75) {
		y=x/3.75;
		y*=y;
		ans=1.0+y*(3.5156229+y*(3.0899424+y*(1.2067492
			+y*(0.2659732+y*(0.360768e-1+y*0.45813e-2)))));
	} else {
		y=3.75/ax;
		ans=(exp(ax)/sqrt(ax))*(0.39894228+y*(0.1328592e-1
			+y*(0.225319e-2+y*(-0.157565e-2+y*(0.916281e-2
			+y*(-0.2057706e-1+y*(0.2635537e-1+y*(-0.1647633e-1
			+y*0.392377e-2))))))));
	}
	return ans;
}


} // ncr::
