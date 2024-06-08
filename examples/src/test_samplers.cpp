#include <iostream>
#include <numeric>
#include <vector>
#include <cmath>
#include <cstddef>
#include <cassert>
#include <iomanip> // setprecision
#include <algorithm>
#include <fstream>
#include <random>
#include <algorithm>
#include <functional>

#include "shared.hpp"
#include <ncr/ncr_log.hpp>
#include <ncr/ncr_math.hpp>
#include <ncr/ncr_bayes.hpp>
#include <ncr/ncr_utils.hpp>
#include <ncr/ncr_random.hpp>
#include <ncr/ncr_algorithm.hpp>


namespace ncr {
	NCR_LOG_DECLARATION;
}


static std::mt19937_64 *_rng;


// additional operators
template <typename T = double>
std::ostream&
operator<<(std::ostream &os, const std::vector<T> &v)
{
	os << "[";
	if (v.size() > 0)
		os << v[0];
	for (size_t i = 1; i < v.size(); i++) {
		os << ", " << v[i];
	}
	os << "]";
	return os;
}



struct custom_struct {
	float x = 0.0;
};


template <typename P = double, typename RngT = std::mt19937_64>
struct SamplerTraits {
	using precision_type = P;
	using rng_type       = RngT;
};


template <typename ContainerT, typename Traits = SamplerTraits<double, std::mt19937_64>>
std::vector<size_t>
custom_sampler(
		const ContainerT &cont,
		size_t n_items,
		typename Traits::rng_type *rng,
		const std::function<typename Traits::precision_type (const typename ContainerT::value_type &)> &value = [](const typename ContainerT::value_type &v){ return v; })
{
	using P = typename Traits::precision_type;

	// allocate space for the flat values
	std::vector<P> p(cont.size(), 0.0);

	// copy over the values that indicate weights/fitness/etc. using the member access function
	std::transform(cont.begin(), cont.end(), p.begin(), value);

	// compute probabilities, cumulative probability
	P sum = std::accumulate(std::begin(p), std::end(p), 0.0);

	// normalization
	for (size_t i = 0; i < p.size(); i++) {
		p[i] /= sum;
	}

	// compute cumulative sum
	std::vector<P> t(p.size() - 1);
	std::partial_sum(p.begin(), p.end() - 1, t.begin());

	// draw samples
	std::uniform_real_distribution<P> dist;
	std::vector<size_t> indices(n_items);
	for (size_t i = 0; i < n_items; i++) {
		indices[i] = std::upper_bound(t.begin(), t.end(), dist(*rng)) - t.begin();
	}
	return indices;
}


int
main(int argc, char *argv[])
{
	NCR_UNUSED(argc, argv);
	ncr::NCR_LOG_INSTANCE.set_policy(new ncr::logger_policy_stdcout());

	// get a random number generator
	_rng = ncr::mkrng();

	// do some sampling
	std::vector<float> ws = {0.1, 0.65, 0.0, 0.1, 0.0, 0.15, 0.0};
	std::cout << ws << std::endl;

	{
		std::vector<ptrdiff_t> counts(ws.size(), 0);
		std::vector<size_t> indices = ncr::weighted_sampler(ws, 1000000, _rng);
		for (size_t i = 0; i < indices.size(); i++) {
			counts[indices[i]]++;
		}
		ptrdiff_t sum = std::accumulate(counts.begin(), counts.end(), 0);

		std::vector<float> target(ws.size(), 0.0);
		for (size_t i = 0; i < ws.size(); i++) {
			target[i] = (float)counts[i] / (float)sum;
		}
		std::cout << target << std::endl;
	}

	{
		std::vector<ptrdiff_t> counts(ws.size(), 0);
		std::vector<size_t> indices = ncr::weighted_sampler_std(ws, 1000000, _rng);
		for (size_t i = 0; i < indices.size(); i++) {
			counts[indices[i]]++;
		}
		ptrdiff_t sum = std::accumulate(counts.begin(), counts.end(), 0);

		std::vector<float> target(ws.size(), 0.0);
		for (size_t i = 0; i < ws.size(); i++) {
			target[i] = (float)counts[i] / (float)sum;
		}
		std::cout << target << std::endl;
	}

	{
		std::vector<ptrdiff_t> counts(ws.size(), 0);

		// using Traits = SamplerTraits<std::vector<float>>;
		std::vector<size_t> indices = custom_sampler(ws, 1000000, _rng);

		for (size_t i = 0; i < indices.size(); i++) {
			counts[indices[i]]++;
		}
		ptrdiff_t sum = std::accumulate(counts.begin(), counts.end(), 0);

		std::vector<float> target(ws.size(), 0.0);
		for (size_t i = 0; i < ws.size(); i++) {
			target[i] = (float)counts[i] / (float)sum;
		}
		std::cout << target << std::endl;
	}


	{

		// same as first time, but with a vector of structs
		std::vector<custom_struct> vec{{.x = 0.1}, {.x = 0.65}, {.x = 0.0}, {.x = 0.1}, {.x = 0.0}, {.x = 0.15}, {.x = 0.0}};

		std::vector<ptrdiff_t> counts(ws.size(), 0);

		std::vector<size_t> indices = custom_sampler(vec, 1000000, _rng, [](const custom_struct &v){ return v.x;} );
		for (size_t i = 0; i < indices.size(); i++) {
			counts[indices[i]]++;
		}
		ptrdiff_t sum = std::accumulate(counts.begin(), counts.end(), 0);

		std::vector<float> target(ws.size(), 0.0);
		for (size_t i = 0; i < ws.size(); i++) {
			target[i] = (float)counts[i] / (float)sum;
		}
		std::cout << target << std::endl;
	}


	return 0;
}
