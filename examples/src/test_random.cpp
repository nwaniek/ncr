#include <iostream>

#include <ncr/ncr_random.hpp>

/*
auto
take(size_t n, callable, args) -> std::vector<type deduced from callable>
{
	std::vector<callable::return_type> result;
	result.resize(n);
	for (size_t i = 0; i < n; i++) {
		result[i] = callable(args);
	}
	return result;
}
*/


void
test_rng_state_serialization()
{
	std::mt19937_64 *rng;

	if (true)
		rng = ncr::mkrng("0x55b39939aeb0");
	else {
		rng = new std::mt19937_64;
		std::string state = "0x55b39939aeb0";
		std::istringstream is(state);
		is >> *rng;
	}
	// the rng should *always* generate the same numbers:
	// 0.933962
	// 0.897977
	// 0.710671
	// 0.946668
	// 0.0192711
	// 0.404902
	// 0.251318
	// 0.0227124
	// 0.520643
	// 0.34467
	for (int i = 0; i < 10; i++)
		std::cout << ncr::unif_random(rng) << "\n";

	// emit the current rng state
	std::cout << rng << "\n";
	delete rng;
}



int
main(int, char *[])
{
	test_rng_state_serialization();
	return 0;
}
