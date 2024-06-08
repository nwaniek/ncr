/*
 * TODO: order of function arguments is inconsistent with other parts of the
 * repository such as, for instance, dfa.hpp or fsm.hpp.
 */

#include <ios>
#include <iostream>
#include <cstdlib>
#include <map>

// configuration happens in shared.hpp
#include "shared.hpp"

#include <ncr/ncr_common.hpp>
#include <ncr/ncr_log.hpp>
#include <ncr/ncr_utils.hpp>
#include <ncr/ncr_cvar.hpp>
#include <ncr/ncr_cmd.hpp>
#include <ncr/ncr_math.hpp>
#include <ncr/ncr_random.hpp>
#include <ncr/ncr_simulation.hpp>
#include <ncr/ncr_automata.hpp>
#include <ncr/ncr_transport.hpp>

using namespace ncr;

int
main()
{
	std::string genome_str = "2 0 0 3 1 1 2 2 1 0 1 0 0 1 1 0";
	auto genome = genome_from_str(genome_str);
	std::cout << genome << std::endl;

	sort(genome.transitions);
	std::cout << genome << std::endl;

	sort(genome.transitions);
	std::cout << genome << std::endl;

	sort(genome.transitions);
	std::cout << genome << std::endl;

	std::vector<int> vec = {7, 1, 3, 5, 6};
	std::cout << std::boolalpha << contains(vec, 7) << ", " << contains(vec, 2) << "\n";

	// copy a genome


	return 0;
}
