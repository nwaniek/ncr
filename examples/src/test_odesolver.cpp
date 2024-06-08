#include <iostream>
#include <vector>
#include <cmath>
#include <cstddef>
#include <cassert>
#include <iomanip> // setprecision
#include <algorithm>
#include <fstream>

#include "shared.hpp"
#include <ncr/ncr_log.hpp>
#include <ncr/ncr_math.hpp>
#include <ncr/ncr_numeric.hpp>
#include <ncr/ncr_utils.hpp>
#include <ncr/ncr_units.hpp>
#include <ncr/ncr_random.hpp>
#include <ncr/ncr_algorithm.hpp>

#include <cblas.h>

#define COUT_PRECISION 4

// TODO: don't do this
using namespace ncr;

namespace ncr {
	NCR_LOG_DECLARATION;
}

void
test_odesolver_1D()
{
	using namespace std;

	// initial value
	double from = 0;
	double to = 1;
	double dt = 0.1;

	// define some differential function
	differential_1D_fn fn = [](double t, double y) -> double{
		NCR_UNUSED(y);
		return 0.5 * (sin(4.*t) + 4. * t * cos(4.*t));
	};
	auto gt_fn = [](double t, double y) -> double{
		NCR_UNUSED(y);
		return 1./8. * (4. * t * sin(4.*t));
	};
	double yInit = gt_fn(from, 0.);

	auto result_eul  = odesolve_1D(odesolve_step_euler_1D, fn, from, to, dt, yInit);
	auto result_rk2  = odesolve_1D(odesolve_step_rk2_1D, fn, from, to, dt, yInit);
	auto result_rk4  = odesolve_1D(odesolve_step_rk4_1D, fn, from, to, dt, yInit);
	auto result_rkck = odesolve_1D(odesolve_step_rkck_adaptive_1D, fn, from, to, dt, yInit);
	auto result_rkdp = odesolve_1D(odesolve_step_rkdp_adaptive_1D, fn, from, to, dt, yInit);

	// compute ground truth
	size_t Nsteps = (size_t)ceil((to - from) / dt);
	std::vector<double> gt(Nsteps);
	for (size_t i = 0; i < Nsteps; ++i) {

		double t = from + (double)i * dt;
		gt[i] = gt_fn(t, 0.0);
	}

	cout << scientific << boolalpha;
	for (size_t i = 0; i < Nsteps; ++i) {

		double eps_eul = abs(gt[i] - result_eul.ys[i]);
		double eps_rk2 = abs(gt[i] - result_rk2.ys[i]);
		double eps_rk4 = abs(gt[i] - result_rk4.ys[i]);

		cout << "gt = "   << gt[i] <<
			", eul = "   << result_eul.ys[i] <<
			", rk2 = "   << result_rk2.ys[i] <<
			", rk4 = "   << result_rk4.ys[i] <<
			", rkck = "  << result_rkck.ys[i] <<
			", rkdp = "  << result_rkdp.ys[i] <<
			", eps_eul = " << eps_eul <<
			", eps_rk2 = " << eps_rk2 <<
			", eps_rk4 = " << eps_rk4 <<
			", eul > rk2 = " << (eps_eul > eps_rk2) <<
			", rk2 > rk4 = " << (eps_rk2 > eps_rk4) <<
			"\n";
	}
}



using _fn = void(*)(double x, const size_t N, double *y_in, double *y_out);




// a sample differential equation (think of xs as t)
void
diffgl(double x, const size_t N, double *y_in, double *y_out)
{
	NCR_UNUSED(x);
	for (size_t i = 0; i < N; i++)
		y_out[i] = y_in[i];
}



template<size_t N, typename T = double>
void
tmpl_diffgl(
		double x,
		const vector_t<N, T> &y_in,
		vector_t<N, T> &y_out,
		std::nullptr_t)
{
	NCR_UNUSED(x);

	for (size_t i = 0; i < N; i++)
		y_out[i] = y_in[i];
}


struct solver_step_nd_t {
	double t;
	size_t N;
	double *y;
	double dt;
};



template <size_t N, typename T = double>
void
lorenz(const T t,
	const vector_t<N, T> &y,
	vector_t<N, T> &dydt,
	std::nullptr_t)
{
	static_assert(N == 3);

	NCR_UNUSED(t);

	constexpr T sigma = 10.0;
	constexpr T R     = 28.0;
	constexpr T b     = 8.0 / 3.0;

	dydt[0] = sigma * (y[1] - y[0]);
	dydt[1] = R * y[0] - y[1] - y[0] * y[2];
	dydt[2] = y[0] * y[1] - b * y[2];
}


void
test_odesolver_lorenz()
{
	// integration setup
	double t  = 0.0;
	double dt = 0.01;
	size_t nsteps = 10;

	// buffers for the stepper
	vector_t<3, double> y_in, y_out;
	y_in[0] = 1.0;
	y_in[1] = 0.0;
	y_in[2] = 0.0;

	// setup integrator for the first step
	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << "t = " << t << ", y = " << y_in << "\n";

	for (size_t i = 0; i < nsteps; i++) {
		// forward integrate and print
		odesolve_step_rk4(lorenz, t, dt, y_in, y_out, nullptr);
		std::cout << "t = " << t << ", y = " << y_out << "\n";

		// swap buffers
		std::swap(y_in, y_out);
	}
}



void
step_euler_nd(_fn fn, const solver_step_nd_t &in, solver_step_nd_t &out)
{
	// x: current x value
	// N: number of items in y
	// y: vector of y values
	// h: step width for x


	out.N = in.N;
	out.dt = in.dt;

	// next x
	out.t = in.t + in.dt;

	// evaluate diff'gl at point x
	fn(in.t, in.N, in.y, out.y);

	// y_next = y + h * f(x, y)
	//        = in.y + h * out.y

	// scale the output vector
	cblas_dscal(out.N, out.dt, out.y, 1);

	// add old y + scaled new y
	cblas_daxpy(out.N, 1.0, in.y, 1, out.y, 1);

	// essentially we have as result:
	//	- next x
	//	- next h
	//	- next ys
	//	- number of elements in the result

	// as arguments, we essentially get
	//  - current ys
	//  - current
}




void
test_euler_step_tmpl()
{
	constexpr size_t N = 1;

	vector_t<N> y_in;
	vector_t<N> y_out;

	// initial values
	y_in = 1.0;

	double t  = 0.0;
	double dt = 0.1;
	odesolve_step_euler(tmpl_diffgl, t, dt, y_in, y_out, nullptr);

	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << "euler:t y[0] = " << y_out[0] << ", dt = " << dt << ", t = " << t << "\n";
}

void
test_step_rk2()
{
	constexpr size_t N = 1;
	vector_t<N> y_in;
	vector_t<N> y_out;

	y_in      = 1.0;
	double t  = 0.0;
	double dt = 0.1;

	odesolve_step_rk2(tmpl_diffgl, t, dt, y_in, y_out, nullptr);

	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << "rk2:t   y[0] = " << y_out[0] << ", dt = " << dt << ", t = " << t << "\n";
}


void
test_step_rk4()
{
	constexpr size_t N = 1;
	vector_t<N> y_in;
	vector_t<N> y_out;

	y_in      = 1.0;
	double t  = 0.0;
	double dt = 0.1;

	odesolve_step_rk4(tmpl_diffgl, t, dt, y_in, y_out, nullptr);

	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << "rk4:t   y[0] = " << y_out[0] << ", dt = " << dt << ", t = " << t << "\n";
}


void
test_step_rkck()
{
	constexpr size_t N = 1;
	vector_t<N> y_in;
	vector_t<N> y_out;

	y_in      = 1.0;
	double t  = 0.0;
	double dt = 0.1;

	odesolve_step_rkck_adaptive(tmpl_diffgl, t, dt, y_in, y_out, nullptr);


	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << "rkck:t  y[0] = " << y_out[0] << ", dt = " << dt << ", t = " << t << "\n";
}


void
test_step_rkdp()
{
	constexpr size_t N = 1;
	vector_t<N> y_in;
	vector_t<N> y_out;

	y_in      = 1.0;
	double t  = 0.0;
	double dt = 0.1;

	odesolve_step_rkdp_adaptive(tmpl_diffgl, t, dt, y_in, y_out, nullptr);

	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << "rkdp:t  y[0] = " << y_out[0] << ", dt = " << dt << ", t = " << t << "\n";
}



void
test_euler_step()
{
	solver_step_nd_t in;
	solver_step_nd_t out;

	// setup input
	in.N = 1;
	in.t = 0.0;
	in.y = new double[1];
	in.y[0] = 1.0;
	in.dt = 0.1;

	// setup output
	out.N = 1;
	out.t = 0.0;
	out.y = new double[1];
	out.dt = 0.0;

	// single step euler
	step_euler_nd(diffgl, in, out);

	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << "euler:n y[0] = " << out.y[0] << ", dt = " << out.dt << ", t = " << out.t << "\n";

	delete[] in.y;
	delete[] out.y;
}


void
test_odesolver_ND()
{
	test_euler_step();
	test_euler_step_tmpl();
	test_step_rk2();
	test_step_rk4();
	test_step_rkck();
	test_step_rkdp();
}


int
main(int argc, char *argv[])
{
	NCR_UNUSED(argc, argv);
	NCR_LOG_INSTANCE.set_policy(new logger_policy_stdcout());

	test_odesolver_1D();
	test_odesolver_ND();
	test_odesolver_lorenz();

	return 0;
}
