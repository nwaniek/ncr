/*
 * ncr_bayes - Some Bayesian functions and estimators
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>

namespace ncr {


template <typename MeasurementType, typename RealType = double>
struct particle
{
	typedef particle<MeasurementType, RealType> ptype;
	typedef RealType rtype;
	typedef MeasurementType mtype;

	RealType weight;

	virtual RealType compute_weight(const MeasurementType *Z) = 0;
	virtual void move() = 0;
};


/*
 * the low variance resampler selects N indices out of a set of particles
 *
 * TODO: adapt to ncr_algorithm.low_variance_sampler
 */
template <typename ParticleType, typename RealType = double>
struct low_variance_resampler
{
	low_variance_resampler(size_t Nparticles) : N(Nparticles) {
		// seed the rng with a time dependent sequence
		uint64_t seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		std::seed_seq seq{uint32_t(seed & 0xFFFFFFFF), uint32_t(seed >> 32)};
		rng.seed(seq);
	}

	explicit low_variance_resampler(int seed) {
		// seed the rng with a fixed value
		rng.seed(seed);
	}

	/*
	 * resample - resample particles with low variance
	 */
	std::vector<size_t>
	resample(const std::vector<ParticleType> &particles)
	{
		size_t index = size_t(unif(rng) * N);
		std::vector<size_t> indices(N);

		RealType beta = 0.0;
		auto max_particle = std::max_element(particles.begin(), particles.end(),
				[](ParticleType a, ParticleType b) {
					return a.weight < b.weight;
				});
		RealType max_weight = (*max_particle).weight;
		for (size_t i = 0; i < N; i++) {
			beta += unif(rng) * 2.0 * max_weight;
			while (beta > particles[index].weight) {
				beta -= particles[index].weight;
				index = (index + 1) % N;
			}
			indices[i] = index;
		}
		return indices;
	}

	size_t N;
	std::random_device rd;
	std::mt19937_64 rng;
	std::uniform_real_distribution<RealType> unif{0.0, 1.0};
};


template <typename ParticleType,
	  typename RealType = double,
	  typename Resampler = low_variance_resampler<ParticleType, RealType>>
struct particle_filter {
	size_t N;
	std::vector<ParticleType> particles;
	Resampler resampler;

	particle_filter(size_t Nparticles) : N(Nparticles), particles(N), resampler(N) {}

	void move() {
		for(auto &p : particles)
			p.move();
	}

	void compute_weights(const typename ParticleType::mtype *Z) {
		for (auto &p : particles) {
			p.weight = p.compute_weight(Z);
		}
	}

	void resample() {
		std::vector<size_t> &&indices = resampler.resample(this->particles);
		std::vector<ParticleType> new_particles(N);
		for (size_t i = 0; i < N; i++)
			new_particles[i] = particles[indices[i]];
		std::swap(new_particles, particles);
	}
};


} // ncr::
