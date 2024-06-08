#include <iostream>
#include <cassert>


#include <cmath>

#define NCR_ENABLE_LOG_LEVEL_VERBOSE

#include <ncr/ncr_algorithm.hpp> // ncr::max
#include <ncr/ncr_automata.hpp>

#include <ncr/experimental/ncr_strgen.hpp>

namespace ncr {
	NCR_LOG_DECLARATION(new logger_policy_stdcout());
}

using namespace ncr;

int main()
{
	// direct API call for ncr

	size_t word_length = 3;
	size_t max_n = std::pow(binary_alphabet.n_symbols, word_length);

	{
		for (size_t i = 0; i < std::pow(binary_alphabet.n_symbols, word_length); i++) {
			auto result = nth_string(&binary_alphabet, word_length, i);
			std::cout << i << ": " << result << "\n";
		}
	}


	// (state-full) object to do the same as above
	std::cout << "---\n";
	{
		auto gen = ncr::alphabet::generators::Successive(binary_alphabet, word_length);
		for (size_t i = 0; i < max_n + 5; i++) {
			std::cout << i << ": " << gen() << "\n";
		}
	}

	// unique random (i.e. no duplicates)
	std::cout << "---\n";
	{
		size_t seed = 0;
		static std::mt19937_64 *rng = ncr::mkrng(seed);
		auto gen = ncr::alphabet::generators::UniqueRandom(rng, binary_alphabet, word_length);
		for (size_t i = 0; i < 2 * max_n; i++) {
			std::cout << i << ": " << gen() << "\n";
		}
	}

	return 0;
}
