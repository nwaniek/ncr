/*
 * ncr_random - distributions and other random algorithms
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 */

#pragma once

#include <random>
#include <chrono>
#include <cmath>
#include <tuple>
#include <bitset>
#include <cassert>

// TODO: check if the following dependencies are requried, or if the types could
//       be inferred somehow during compilation. These types are currenlty used
//       (only) in the choice methods
#include <set>
#include <vector>
#include <list>

namespace ncr {


// TODO: the functions have non-uniform signatures. Specifically, there are some
//       functions (most), which take the random number generator as last
//       argument, while there are a few which take it as first argument. All of
//       the functions should take it either as first or last, but no mixture.
//       This might depend on recursive templates where we can use it (see
//       random_coord, for example).
//       Due to the deduction of types during template expansion with parameter
//       packs, it might be that the best variant is to take the Rng as first
//       argument, followed by all other arguments
//

/*
 * sign - compute the sign of a value.
 *
 * This is a copy from ncr_math, but repeated here to avoid having to include
 * ncr_math and make this file standalone
 */
template <typename T = double>
T _random_sign(T val) {
	return static_cast<T>((static_cast<T>(0) < val) - (val < static_cast<T>(0)));
}


/*
 * reseed_rng(rng, seed) - Reseed a given random number generator.
 *
 * If the argument `seed` is 0, a seed based on the current computer time will
 * be used.
 */
template <typename RngT = std::mt19937_64>
auto reseed_rng(RngT *rng, uint64_t seed = 0) -> RngT*
{
	assert(rng != nullptr);

	if (!seed)
		seed = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	std::seed_seq seq{uint32_t(seed & 0xFFFFFFFF), uint32_t(seed >> 32)};
	rng->seed(seq);

	return rng;
}


/*
 * choice(a, b, rng) - Draw a random number from range [a, b]
 */
template <typename IntType = int, typename RngT = std::mt19937_64>
auto choice(IntType a, IntType b, RngT *rng) -> IntType
{
	std::uniform_int_distribution<IntType> d{a, b};
	return d(*rng);
}


/*
 * choice - randomly choose an element from a set
 */
template <typename T, typename RngT = std::mt19937_64>
auto choices(std::set<T> set, RngT *rng) -> T
{
	size_t lo = 0;
	size_t hi = set.size() - 1;
	size_t elem_idx = choice<size_t>(lo, hi, rng);

	typename std::set<T>::iterator it = set.begin();
	std::advance(it, elem_idx);
	return *it;
}

/*
 * choice - randomly choose an element from a list
 */
template <typename T, typename RngT = std::mt19937_64>
auto choicel(std::list<T> list, RngT *rng) -> T
{
	size_t lo = 0;
	size_t hi = list.size() - 1;
	size_t elem_idx = choice<size_t>(lo, hi, rng);
	return list[elem_idx];
}

/*
 * choice - randomly choose an element from a vector
 */
template <typename T, typename RngT = std::mt19937_64>
auto choicev(std::vector<T> vec, RngT *rng) -> T
{
	size_t lo = 0;
	size_t hi = vec.size() - 1;
	size_t elem_idx = choice<size_t>(lo, hi, rng);
	return vec[elem_idx];
}


/*
 * Get a uniform randomly distributed number in [0, 1]
 */
template <typename T = double, typename RngT = std::mt19937_64>
T unif_random(RngT *rng)
{
	assert(rng);
	std::uniform_real_distribution<T> unif(static_cast<T>(0), static_cast<T>(1));
	return unif(*rng);
}


/*
 * coinflip - returns true 50% of the time
 */
template <typename RngT = std::mt19937_64>
bool
coinflip(RngT *rng)
{
	return unif_random(rng) > 0.5;
}




/*
 * laplace(mu, b) - Generate sample from a Laplace distribution.
 *
 * mu is the location parameter, b the diversity of the distribution
 */
template <typename RealType, typename RngT = std::mt19937_64>
auto laplace(RealType mu, RealType b, RngT *rng) -> RealType
{
	assert(rng != nullptr);

	std::uniform_real_distribution<RealType> unif{0.0, 1.0};
	auto u = unif(*rng) - .5;
	return mu - b*_random_sign(u) * std::log(1 - 2 * std::abs(u));
}


/**
 * vonmises(mu, kappa) - geturn a sample from a von Mises distribution.
 *
 * Implementation adapted from numpy source.
 */
template <typename RealType, typename RngT = std::mt19937_64>
auto vonmises(RealType mu, RealType kappa, RngT *rng) -> RealType
{
	assert(rng != nullptr);

	RealType s;
	RealType U, V, W, Y, Z;
	RealType result, mod;
	int neg;
	std::uniform_real_distribution<RealType> unif{0.0, 1.0};

	if (kappa < 1e-8)
		return M_PI * (2*unif(*rng)-1);
	else {
		/* with double precision rho is zero until 1.4e-8 */
		if (kappa < 1e-5) {
			/*
			 * second order taylor expansion around kappa = 0
			 * precise until relatively large kappas as second order is 0
			 */
			s = (1./kappa + kappa);
		}
		else {
			RealType r = 1. + sqrt(1 + 4*kappa*kappa);
			RealType rho = (r - sqrt(2*r)) / (2*kappa);
			s = (1. + rho*rho)/(2*rho);
		}

		while (true) {
			U = unif(*rng);
			Z = cos(M_PI*U);
			W = (1. + s*Z)/(s + Z);
			Y = kappa * (s - W);
			V = unif(*rng);
			if ((Y*(2.-Y) - V >= 0) || (log(Y/V)+1. - Y >= 0)) {
				break;
			}
		}

		U = unif(*rng);

		result = acos(W);
		if (U < 0.5)
			result = -result;
		result += mu;
		neg = (result < 0);
		mod = std::abs(result);
		mod = (std::fmod(mod+M_PI, 2*M_PI)-M_PI);
		if (neg)
			mod *= -1;

		return mod;
	}
}


/**
 * vonmises(N, mu, kappa) - Retrieve N samples from a von Mises distribution.
 */
template <typename RealType, typename RngT = std::mt19937_64>
std::vector<RealType>
vonmises(size_t N, RealType mu, RealType kappa, RngT *rng)
{
	std::vector<RealType> result(N);
	for (size_t i = 0; i < N; i++)
		result[i] = vonmises<RealType, RngT>(mu, kappa, rng);
	return result;
}


/*
 * laplace(N, mu, b) - Generate N samples from a laplace distribution
 *
 * mu is the location parameter, b the diversity of the distribution. The
 * results are stored in a temporary. You may specify a random number generator
 * to use for generating required numbers.
 */
template <typename RealType, typename RngT = std::mt19937_64>
auto laplace(size_t N, RealType mu, RealType b, RngT *rng) -> std::vector<RealType>
{
	assert(rng != nullptr);

	std::uniform_real_distribution<RealType> unif{0.0, 1.0};
	std::vector<RealType> result(N);

	for (size_t i = 0; i < N; i++) {
		auto u = unif(*rng) - .5;
		result[i] = mu - b*_random_sign(u) * std::log(1 - 2 * std::abs(u));
	}

	return result;
}


/*
 * poisson(N, h) - Generate N samples from a poisson distribution
 *
 * h is the lambda argument as usually described in poisson distributions. You
 * may specify a random number generator to use for generating required numbers.
 */
template <typename RealType, typename RngT = std::mt19937_64>
auto poisson(size_t N, int h, RngT *rng) -> std::vector<RealType>
{
	assert(rng != nullptr);

	std::poisson_distribution<> d(h);
	std::vector<RealType> result(N);

	for (size_t i = 0; i < N; i++)
		result[i] = d(*rng);

	return result;
}


/*
 * gamma(a, b, rng) - Generate a sample from a gamma distribution.
 *
 * a and b describe the gamma distribution, where a is the shape and b is the
 * distribution's scale. You may specify a random number generator to use for
 * generating required numbers.
 *
 */
template <typename RealType, typename RngT = std::mt19937_64>
auto gamma(RealType a, RealType b, RngT *rng) -> RealType
{
	assert(rng != nullptr);
	std::gamma_distribution<RealType> g(a, b);
	return g(*rng);
}


/*
 * gamma(N, a, b) - Generate N samples from a gamma distribution
 *
 * a and b describe the gamma distribution, where a is the shape and b is the
 * distribution's scale. You may specify a random number generator to use for
 * generating required numbers.
 */
template <typename RealType, typename RngT = std::mt19937_64>
auto gamma(size_t N, RealType a, RealType b, RngT *rng) ->  std::vector<RealType>
{
	assert(rng != nullptr);

	std::gamma_distribution<RealType> g(a, b);
	std::vector<RealType> result(N);

	for (size_t i = 0; i < N; i++)
		result[i] = g(*rng);

	return result;
}



template <typename RngT, typename T>
T random_grid_coord(RngT *rng, const std::tuple<T, T> &limits)
{
	return choice(std::get<0>(limits), std::get<1>(limits), rng);
}


/*
 * random_grid_coord - get a random coordinate on an N-D grid
 *
 * Usage Example to get a coordinate in a 10x50 2 dimensional grid world
 *
 *    auto rng  = // some random number generator
 *	  auto xlim = std::tuple{0, 10};
 *	  auto ylim = std::tuple{0, 50};
 *    random_grid_coord(rng, xlim, ylim);
 *
 *
 * Usage Example to get a coordinate in a 10x50x100 3 dimensional grid world
 *
 *    auto rng  = // some random number generator
 *	  auto xlim = std::tuple{0, 10};
 *	  auto ylim = std::tuple{0, 50};
 *	  auto zlim = std::tuple{0, 100};
 *    random_grid_coord(rng, xlim, ylim, zlim);
 *
 */
template <typename RngT, typename T, typename... Ts>
auto
random_grid_coord(RngT *rng, const std::tuple<T, T> &x, Ts&&... xs)
{
	return std::tuple{
		random_grid_coord(rng, x),
		random_grid_coord(rng, std::forward<Ts>(xs))...
	};
}

template <typename RngT, typename T>
T random_coord(RngT *rng, const std::tuple<T, T> &limits)
{
	T span = std::get<1>(limits) - std::get<0>(limits);
	return unif_random(rng) * span + std::get<0>(limits);
}


/*
 * random_coord - get a random coordinate in N-D world
 *
 * Usage Example to get a coordinate in a 2D space of with some given
 * dimensions:
 *
 *    auto rng  = // some random number generator
 *	  auto xlim = std::tuple{0., 10.};
 *	  auto ylim = std::tuple{0., 50.};
 *    random_coord(rng, xlim, ylim);
 *
 *
 * Usage Example to get a coordinate in a 5D unit space:
 *
 *    auto rng  = // some random number generator
 *	  auto limits = std::tuple{0., 1.};
 *    random_coord(rng, limits, limits, limits, limits, limits);
 *
 */
template <typename RngT, typename T, typename... Ts>
auto
random_coord(RngT *rng, const std::tuple<T, T> &x, Ts&&... xs)
{
	return std::tuple{
		random_coord(rng, x),
		random_coord(rng, std::forward<Ts>(xs))...
	};
}


/*
 * mkrng(seed) - instantiate a new random number generator
 *
 * This instantiates and initializes a new random number generator. If the
 * argument `seed` is 0, then a seed based on the current computer time will be
 * used.
 */
template <typename RngT = std::mt19937_64>
auto mkrng(uint64_t seed = 0) -> RngT*
{
	RngT* rng= new RngT();
	reseed_rng(rng, seed);
	return rng;
}

/*
 * mkrgn(state) - instantiate a random number generator from a state
 *
 * This creates a new rng and uses the state passed in to initialize the rng.
 * This can be helpful, for instance, when pausing a simulation that needs to be
 * resumed at a later time. Then, it's possible to write the rng state simply
 * using operator<< as a string, and re-use this state using the function
 * provided here.
 */
template <typename RngT = std::mt19937_64>
auto mkrng(std::string state) -> RngT*
{
	RngT* rng= new RngT();
	std::istringstream is(state);
	is >> *rng;
	return rng;
}



/*
 * random_bits - Generate a number N of random bits.
 */
template <int N, typename RngT = std::mt19937_64>
std::bitset<N>
random_bits(RngT *rng)
{
	std::bitset<N> result;
	std::bernoulli_distribution dist(0.5);
	for(size_t i = 0; i < N; i++)
		result.set(i, dist(rng));
	return result;
}


} // ncr::
