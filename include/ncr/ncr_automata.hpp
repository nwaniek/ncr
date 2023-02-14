/*
 * ncr_automata - Algorithms and Data Structures for Finite State Machines
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 *
 * This file includes algorithms and data structures for Finite State Machines
 * (FMSs), such as definitions for alphabets, genomes that encode transitions
 * and states, as well as functions for DFAs and NFAs. On top of this, it
 * includes structs for finite state machines, which have a DFA and a Genome in
 * their backend. That is, a DFA is encoded within a Genome and synthesized into
 * a realization DFA within an FSM.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <ostream>
#include <tuple>
#include <optional>
#include <fstream>
#include <ostream>
#include <map>
#include <algorithm>
#include <iomanip>

#if DEBUG_VERBOSE
#include <sstream>
#endif

#include <ncr/ncr_algorithm.hpp>
#include <ncr/ncr_utils.hpp>
#include <ncr/ncr_log.hpp>
#include <ncr/ncr_random.hpp>

namespace ncr {


/*
 * In the definition of a DFA, the set of final states can be empty. This is
 * important during validation of a DFA. For instance, an acceptor should have a
 * final state, but for a transducer, it is not required. Hence, it might make
 * sense to ignore if the set of final states is empty or not.
 *
 * TODO: move this flag somewhere else, or even make it a constexpr bool
 */
#ifndef DFA_ALLOW_EMPTY_FINAL_STATE_SET
#define DFA_ALLOW_EMPTY_FINAL_STATE_SET true
#endif

#define DFA_TRANSITION_UNDEFINED   -1
#define DFA_STATE_NOT_PROCESSED    -1


/*
 * state flags for automata
 */
#define NCR_AUTOMATON_STATE_FLAGS(_) \
	_(automaton_state_flags, DEFAULT , (0 << 0)) \
	_(automaton_state_flags, IS_START, (1 << 0)) \
	_(automaton_state_flags, IS_FINAL, (1 << 1))

/*
 * flags used / returned by FSM's validate() function.
 *
 */
#define NCR_FSM_VALIDATION_FLAGS(_) \
	_(fsm_validation_flags, IS_DFA                      , (0 << 0)) \
	_(fsm_validation_flags, IS_NFA                      , (1 << 0)) \
	_(fsm_validation_flags, MISSING_STATES              , (1 << 1)) \
	_(fsm_validation_flags, MISSING_TRANSITIONS         , (1 << 2)) \
	_(fsm_validation_flags, MULTIPLE_STARTING_STATES    , (1 << 3)) \
	_(fsm_validation_flags, MISSING_STARTING_STATE      , (1 << 4)) \
	_(fsm_validation_flags, NO_FINAL_STATES             , (1 << 5)) \
	_(fsm_validation_flags, TRANSITION_SOURCE_UNKNOWN   , (1 << 6)) \
	_(fsm_validation_flags, TRANSITION_TARGET_UNKNOWN   , (1 << 7)) \
	_(fsm_validation_flags, DUPLICATE_TRANSITION        , (1 << 8)) \
	_(fsm_validation_flags, TRANSITION_SYMBOL_IS_UNKNOWN, (1 << 9)) \

/*
 * flags returned by run() function
 */
#define NCR_FSM_RUN_FLAGS(_) \
	_(fsm_run_flags, OK                         , (0 << 0)) \
	_(fsm_run_flags, ERROR_NOT_INITIALIZED      , (1 << 1)) \
	_(fsm_run_flags, ERROR_CURRENT_STATE_NOT_SET, (1 << 2)) \
	_(fsm_run_flags, ERROR_NOT_IN_STARTING_STATE, (1 << 3)) \
	_(fsm_run_flags, ERROR_NO_VIABLE_TRANSITION , (1 << 4)) \
	_(fsm_run_flags, ERROR_NOT_IN_FINAL_STATE   , (1 << 5)) \
	_(fsm_run_flags, ERROR_INVALID_WORD         , (1 << 6))


/*
 * flags which indicate certain DFA types.
 * types of finite state automata: epsilon-NFA, NFA, DFA
 *
 * TODO: these are not really flags, but whatever
 */
#define NCR_FSM_TYPE_FLAGS(_) \
	_(fsm_type, UNKNOWN, (0 << 0)) \
	_(fsm_type, DFA    , (1 << 0)) \
	_(fsm_type, NFA    , (1 << 1)) \
	_(fsm_type, eNFA   , (1 << 2)) \


/*
 * This define/table collexts all flags defined above.
 * The entries in each row of the table are:
 *	1. flag-list name
 *	2. enum-name for the flags
 *	3. function name for conversion of flag to string
 *	4. default element of the flags
 */
//    list name					enum name             conversion function         default value
#define NCR_AUTOMATON_FLAG_LIST(_) \
	_(NCR_AUTOMATON_STATE_FLAGS,    automaton_state_flags,  state_flag_to_str,          DEFAULT) \
	_(NCR_FSM_VALIDATION_FLAGS,		fsm_validation_flags,   fsm_validation_flag_to_str, IS_DFA) \
	_(NCR_FSM_RUN_FLAGS,			fsm_run_flags,          fsm_run_flag_to_str,        OK) \
	_(NCR_FSM_TYPE_FLAGS,           fsm_type,               fsm_type_flag_to_str,       UNKNOWN)


/*
 * Code generation of all enums defined in the FLAG_LIST.
 * More precisely, each row of FLAG_LIST is transformed into an enum of a
 * certain name, by expanding the list_name according to the x-macro rule.
 */
#define NCR_FLAG_TO_ENUM(SCOPE, FLAG_NAME, VALUE) \
		FLAG_NAME = VALUE,
#define XMACRO(LIST_NAME, ENUM_NAME, ...) \
	enum struct ENUM_NAME : unsigned { \
		LIST_NAME(NCR_FLAG_TO_ENUM) \
	}; \
	NCR_DEFINE_ENUM_FLAG_OPERATORS(ENUM_NAME) \
	\
	inline \
	std::ostream& \
	operator<< (std::ostream &out, const ENUM_NAME e) \
	{ \
		out << static_cast<unsigned>(e); \
		return out; \
	}\
	\
	inline \
	bool \
	test(ENUM_NAME a)\
	{ \
		return std::underlying_type<ENUM_NAME>::type(a); \
	}

NCR_AUTOMATON_FLAG_LIST(XMACRO)
#undef XMACRO
#undef NCR_FLAG_TO_ENUM


/*
 * Code generation for all flag-lists defined in FLAG_LIST.
 * More precisely, each row in FLAG_LIST will be turned into a function that
 * accepts an unsigned integer and turns this unsigned into a human-readable
 * string. This string, in turn, is made up of the real names of flags specified
 * in each concrete flag list.
 *
 * The x-macro definition is required to turn each individual flag into an
 * if-branch of the function that will be generated.
 *
 * Note: this function uses std::string, and appends to it. It is therefore not
 * the fastest solution and should be used only in code that is not on any
 * critical path or in high performance execution. In ncr, is mostly used in
 * log_verbose environments.
 */
#define NCR_FLAG_TO_STR(SCOPE, FLAG_NAME, ...) \
			if ((flag & SCOPE::FLAG_NAME) == SCOPE::FLAG_NAME) { \
				if (n) result += " | "; \
				result += #FLAG_NAME; \
				n = 1; \
			}
#define X_TO_FN(LIST_NAME, ENUM_NAME, FN_NAME, DEFAULT_FLAG) \
	inline std::string \
	FN_NAME(ENUM_NAME flag) { \
		unsigned n = 0; \
		std::string result = ""; \
		result += std::to_string(static_cast<unsigned>(flag)); \
		result += ": "; \
		if (flag == ENUM_NAME::DEFAULT_FLAG) { \
			result += #DEFAULT_FLAG; \
		} \
		else { \
			LIST_NAME(NCR_FLAG_TO_STR) \
		} \
		return result; \
	}
NCR_AUTOMATON_FLAG_LIST(X_TO_FN)
#undef X_TO_FN
#undef NCR_FLAG_TO_STR






/*
 * struct symbol - A symbol of an alphabet
 *
 * Each symbol has an ID (within the alphabet) as well as a human-readable
 * glyph. The two might coincide. It also has an indicator of it is a blank
 * symbol or not.
 *
 * All members of the symbol are const, as a symbol is considered immutable.
 */
struct symbol
{
	// each symbol in an alphabet has an ID. Essentially, this is the palce in
	// the array of symbols within the alphabet to avoid searching or other
	// lookup functions
	const size_t
		id;

	// Every symbol in an alphabet is represented by a single (huma readable)
	// character
	const char
		glyph;

	// indicator if this symbol is a (the) blank symbol or not
	const bool
		is_blank = false;
};


/*
 * word_t - a word is a concatenation of symbols
 *
 * word_t is simply a typedef to a vector of pointer to symbols. Symbols are
 * assumed to be immuatable, while words themselves are mutable. Passing
 * pointers is more efficient than having copies of symbols.
 */
typedef std::vector<const symbol *> basic_string_t;


inline void
copy_str(const basic_string_t &src, basic_string_t &dest)
{
	dest.clear();
	for (size_t i = 0; i < src.size(); i++)  {
		auto *sym = src[i];
		if (sym == nullptr)
			break;
		dest.push_back(sym);
	}
}


/*
 * struct basic_alphabet - A basic alphabet.
 *
 * An alphabet consists of a certain number `n_symbols` of `symbols`.
 */
struct basic_alphabet {
	// an alphabet contains a certain number of symbols
	const size_t
		n_symbols;

	// number of input symbols, i.e. number of symbols without number of blank
	// symbols
	const size_t
		n_input_symbols;

	// number of blank symbols
	const size_t
		n_blank_symbols;

	// the symbols themselves. Note that it is expected that the symbols are
	// arranged such that the input symbols come first, followed by the blank
	// symbols. That is, symbols = [i0, i1, ..., iN, b0, b1, ... bM], where ii
	// is an input symbol, and bj is a blank symbol
	const symbol *
		symbols;
};


/*
 * concrete definition of a binary alphabet. First, the definition of the
 * symbols themselves, then the definition of the alphabet. Both are declared
 * static constexpr const to let the compiler optimize as much as possible, and
 * to also identify these as immutable.
 *
 * Note that these variables *must* be declared inline so that there will ever
 * be only one memory location generated for them. This requires C++17.
 */
inline constexpr const symbol
binary_symbols[] = {
	// input symbols
	{.id = 0, .glyph = '0', .is_blank = false},
	{.id = 1, .glyph = '1', .is_blank = false},
};


inline constexpr const basic_alphabet
binary_alphabet = {
	.n_symbols       = 2,
	.n_input_symbols = 2,
	.n_blank_symbols = 0,
	.symbols         = binary_symbols,
};


inline constexpr const symbol
binary_symbols_incl_blank[] = {
	// input symbols
	{.id = 0, .glyph = '0', .is_blank = false},
	{.id = 1, .glyph = '1', .is_blank = false},
	// blank symbols
	{.id = 2, .glyph = '.', .is_blank = true},
};


inline constexpr const basic_alphabet
binary_alphabet_incl_blank = {
	.n_symbols       = 3,
	.n_input_symbols = 2,
	.n_blank_symbols = 1,
	.symbols         = binary_symbols_incl_blank,
};


// TODO: define set operators (union, intersection, etc.) to operate on
// alphabets


inline
std::ostream&
operator<< (std::ostream &out, const symbol *sym)
{
	out << sym->glyph;
	return out;
}


inline
std::ostream&
operator<< (std::ostream &out, std::vector<const symbol *> word)
{
	// TODO: std-ify
	for (size_t i = 0; i < word.size(); ++i)
		out << word[i];
	return out;
}



/*
 * str_to_symbols - Turn a string into a vector of symbols of an alphabet.
 */
inline std::vector<const symbol *>
str_to_symbols(std::string str, const basic_alphabet &alphabet)
{
	using namespace std;

	std::vector<const symbol *> result;
	for (char &c: str) {
		for (size_t i = 0; i < alphabet.n_symbols; ++i) {
			if (alphabet.symbols[i].glyph == c) {
				result.push_back(&alphabet.symbols[i]);
			}
		}
	}

	return result;
}


/*
 * symbols_to_str - Turn a vector of symbols into its string representation.
 */
inline std::string
symbols_to_str(std::vector<symbol> symbols)
{
	std::string result = "";
	for (size_t i = 0; i < symbols.size(); ++i) {
		result += symbols[i].glyph;
	}
	return result;
}


/*
 * symbols_to_str - Turn a vector of pointer to symbols to a string.
 */
inline std::string
symbols_to_str(std::vector<const symbol *> word)
{
	std::string result = "";
	for (auto sym: word) {
		result += sym->glyph;
	}
	return result;
}


/*
 * random_string - Get a random string of input symbols with certain length.
 */
template <typename RngT = std::mt19937_64>
basic_string_t
random_string(RngT* rng, const basic_alphabet *alphabet, const size_t length)
{
	basic_string_t result(length);
	for (size_t i = 0; i < length; ++i)
		result[i] = &alphabet->symbols[choice(0ul, alphabet->n_input_symbols - 1, rng)];
	return result;
}


/*
 * nth_string - generate the n-th string of given length from an alphabet
 */
inline basic_string_t
nth_string(const basic_alphabet *alphabet, const size_t length, size_t n)
{
	basic_string_t result(length);
	const auto alpha_len = alphabet->n_symbols;
	assert((n < std::pow(alpha_len, length)) && "String index out of bounds");

	size_t l = length;
	while (l > 0) {
		size_t k = n / std::pow(alpha_len, l - 1);
		result[length - l] = &alphabet->symbols[k];
		n = max(0, n - k * std::pow(alpha_len, l-1));
		l -= 1;
	}

	return result;
}


/*
 * state_gene - Encoding of a state for a genome
 */
struct state_gene
{
	// this is the index of the gene in the vector of genes in the genome it
	// belongs to
	size_t
		id;

	// each state can have a certain label. This will usually correspond to a
	// representation of the ID.
	// NOTE: at the moment, the label follows precisely the ID, and is ignored
	//       in the function to_tuple below
	char
		label;

	// flags of the state, for instance if the state is starting or accepting
	automaton_state_flags
		flag = {};
};


inline std::tuple<const size_t&, const automaton_state_flags&>
to_tuple(const state_gene& s)
{
	return std::tie(s.id, s.flag);
}


/*
 * is_start - helper function to determine if a state is a start
 */
inline bool
is_start(const state_gene *s)
{
	return static_cast<unsigned>(s->flag & automaton_state_flags::IS_START) > 0;
}

inline bool
is_start(const state_gene &s)
{
	return is_start(&s);
}


/*
 * is_final - helper function to determine if a state is accepting
 */
inline bool
is_final(const state_gene *state)
{
	return static_cast<unsigned>(state->flag & automaton_state_flags::IS_FINAL) > 0;
}

inline bool
is_final(const state_gene &state)
{
	return is_final(&state);
}


/*
 * operator== - Comparison operator for automaton states.
 *
 * Note that this only compares the data stored in the gene. It does not compare
 * if the states are equivalent in the sense of the FSM that they belong to.
 */
inline bool
operator==(
		const state_gene &left,
		const state_gene &right)
{
	// TODO: a state is defined not only by its id, label, and flag, but also by
	//       the incident and excident transitions

	// pre 2022-01-11
	// return (left.label == right.label)
	// 	&& (left.flag == right.flag);

	// post 2022-01-11
	//return (left.id   == right.id)
	//	&& (left.flag == right.flag);
	return to_tuple(left) == to_tuple(right);
}


/*
 * transition_gene - Encoding of a (transducer) transition for a genome
 */
struct transition_gene {
	// Q x Σ -> Q x Σ
	size_t state_from;
	size_t symbol_read;
	size_t state_to;
	size_t symbol_write;
};


inline std::tuple<const size_t&, const size_t&, const size_t&, const size_t&>
to_tuple(const transition_gene& t)
{
	return std::tie(t.state_from, t.symbol_read, t.state_to, t.symbol_write);
}

/*
 * operator== - Compare two automaton transitions for equality.
 *
 * Note that this only compares the data stored in the gene. Specifically,
 * this operator compares if the values assign to the transition, such as symbol
 * and states, are identical. This does not test equivalence in the sense of the
 * FSM that the transitions are defined for.
 */
inline bool
operator==(
		const transition_gene &left,
		const transition_gene &right)
{
	return to_tuple(left) == to_tuple(right);
}


/*
 * in_domain - determine if a state is in the domain of a transition.
 */
inline bool
in_domain(const state_gene *s, const transition_gene *t)
{
	return t->state_from == s->id;
}

inline bool
in_domain(const state_gene &s, const transition_gene &t)
{
	return in_domain(&s, &t);
}

inline bool
is_defined_for(const transition_gene &t, const state_gene &s)
{
	return in_domain(s, t);
}


/*
 * in_image - determine if a state is in the image of a transition
 */
inline bool
in_image(const state_gene *s, const transition_gene *t)
{
	return t->state_to == s->id;
}

inline bool
in_image(const state_gene &s, const transition_gene &t)
{
	return in_image(&s, &t);
}

inline bool
leads_to(const transition_gene t, const state_gene s)
{
	return in_image(s, t);
}


/*
 * struct fsm_genome - Genome of an automaton
 *
 * An automaton genome consists of information about the states as well as the
 * transitions that make up an automaton. Note that the genes are expected to
 * operate based on indexes and not on pointers. That is, a transition goes from
 * a certain state to another state, where the state is specified by the
 * state's ID within the vector of states.
 */
struct fsm_genome
{
	std::vector<state_gene>      states;
	std::vector<transition_gene> transitions;
};


/*
 * Configuration of various mutation strategies
 */
struct mutation_rates
{
	// state mutation rates
	double delete_state               = 0.04;
	double create_state               = 0.04;
	double modify_state_start         = 0.01;
	double modify_state_accepting     = 0.04;

	// transition mutation rates
	double drop_transition            = 0.04;
	double spawn_transition           = 0.04;
	double modify_transition_source   = 0.04;
	double modify_transition_target   = 0.04;
	double modify_transition_symbol   = 0.04;
	double modify_transition_emission = 0.04;
};


/*
 * operator== - Compare two genomes.
 *
 * Two genomes are identical if all their contained elements (states,
 * transitions) are identical.
 */
inline bool
operator==(
		const fsm_genome &left,
		const fsm_genome &right)
{
	return (left.states      == right.states)
		&& (left.transitions == right.transitions);
}


#define GENOME_CONTAINS_LIST \
	X(const transition_gene, transitions) \
	X(const state_gene     , states)


// TODO: the contains() set of functions could be generalized. It could also
//       just rely on std::find instead of manually iterating
//       Then, move to utils

/*
 * contains - Determine if a needle exists in a given genome's container.
 *
 * This is simply a shorthand that uses function overloading to reduce code
 */
#define X(type_t, container) \
	inline bool \
	contains(const fsm_genome &target, type_t &needle) { \
		for (size_t i = 0; i < target.container.size(); ++i) { \
			if (target.container[i] == needle) \
				return true; \
		} \
		return false; \
	}
	GENOME_CONTAINS_LIST
#undef X


/*
 * contains - Determine if a pointer to state-gene is in a vector
 */
inline bool
contains(const std::vector<const state_gene *> &v, const state_gene *s)
{
	// TODO: merge with GENOME_CONTAINS_LIST
	for (size_t i = 0; i < v.size(); ++i) {
		if (s == v[i])
			return true;
	}
	return false;
}


/*
 * get_state_by_index - Get a genome's state by index
 *
 * This function returns the state given a certain index into the array of
 * states within a genome. Note that this is different than getting a state by
 * its label!
 */
inline const state_gene *
get_state_by_index(const fsm_genome &genome, size_t index)
{
	if (index >= genome.states.size())
		return nullptr;
	return &genome.states[index];
}


/*
 * get_state_by_label - Get a state by it's label
 *
 * This function returns a state by searching its label. If the label cannot be
 * found, then the returned value is nullptr.
 */
inline const state_gene *
get_state_by_label(const fsm_genome &genome, char label)
{
	state_gene *result = nullptr;
	for (size_t i = 0; i < genome.states.size(); ++i) {
		if (genome.states[i].label == label)
			return &genome.states[i];
	}
	return result;
}


/*
 * get_initial_state - get a pointer to a genome's initial state
 *
 * In case the genome does not contain an initial state, nullptr is returned.
 * This usually indicates a malformed or deficient genome.
 */
inline const state_gene *
get_initial_state(const fsm_genome &genome)
{
	for (size_t i = 0; i < genome.states.size(); ++i) {
		if (is_start(genome.states[i])) {
			return &genome.states[i];
		}
	}
	return nullptr;
}


/*
 * get_accepting_stats - get all accepting states in a genome
 */
inline std::vector<const state_gene *>
get_accepting_states(const fsm_genome &genome)
{
	std::vector<const state_gene *> result;
	for (size_t i = 0; i < genome.states.size(); ++i) {
		if (is_final(genome.states[i])) {
			result.push_back(&genome.states[i]);
		}
	}
	return result;
}


/*
 * get_reachable_state_genes - Get all reachable state-genes of a genome
 */
inline
std::vector<const state_gene *>
get_reachable_state_genes(
		const fsm_genome &genome)
{
	// Note that we expect the FSMs to be rather small and only a handful of
	// states. Hence, using a vector here is perfectly fine, as searching
	// through will effectively be faster, or at least not significantly slower,
	// than other containers with guaranteed constant access times.
	std::vector<const state_gene *> unprocessed;
	std::vector<const state_gene *> reachable;

	auto initial_state = get_initial_state(genome);
	unprocessed.push_back(initial_state);
	reachable.push_back(initial_state);

	while (unprocessed.size() > 0) {
		// get one of the unprocessed states and remove it from the list
		auto s = unprocessed.back();
		unprocessed.pop_back();

		// go over all transitions and check if the transition is defined for
		// the state. If yes, then add all
		for (auto &t : genome.transitions) {
			if (in_domain(s, &t)) {
				const state_gene *target = get_state_by_index(genome, t.state_to);
				if (!contains(reachable, target)) {
					reachable.push_back(target);

					// if this item is reachable, and not yet in the unprocessed
					// stack, move it there to address it in a future loop
					if (!contains(unprocessed, target)) {
						unprocessed.push_back(target);
					}
				}
			}
		}
	}

	return reachable;
}


/*
 * get_unreachable_states - Get all states of a genome that cannot be reached
 */
inline
std::vector<const state_gene *>
get_unreachable_state_genes(
		const fsm_genome &genome)
{
	std::vector<const state_gene *> unreachable;
	std::vector<const state_gene *> reachable;

	// get all reachable states, then store all those states which are not
	// reachable
	reachable = get_reachable_state_genes(genome);
	for (size_t i = 0; i < genome.states.size(); ++i) {
		if (!contains(reachable, &genome.states[i])) {
			unreachable.push_back(&genome.states[i]);
		}
	}

	return unreachable;
}


/*
 * validate_genome - Determine if a genome represents a valid FSM or not
 */
inline
fsm_validation_flags
validate_genome(const basic_alphabet *alphabet, const fsm_genome &genome)
{
	// if non of the other flags will be set, then the genome encodes a DFA.
	// Note that further below we cehck if there are multiple transitions
	// defined for one input symbol and state, which effectively turns the DFA
	// into an NFA.
	auto result = fsm_validation_flags::IS_DFA;

	if (genome.states.size() == 0)
		result |= fsm_validation_flags::MISSING_STATES;

	if (genome.transitions.size() == 0)
		result |= fsm_validation_flags::MISSING_TRANSITIONS;


	// allow only one starting state, there should be at least one accepting state
	unsigned n = 0; // starting states
	unsigned m = 0; // accepting states
	for (auto &s : genome.states) {
		if (is_start(s))
			n += 1;
		if (is_final(s))
			m += 1;
	}
	if (n == 0)
		result |= fsm_validation_flags::MISSING_STARTING_STATE;
	else if (n > 1)
		result |= fsm_validation_flags::MULTIPLE_STARTING_STATES;

	// The following is *not* a requirement for a DFA. The set of final states
	// can be empty.
	if (!DFA_ALLOW_EMPTY_FINAL_STATE_SET) {
		if (m == 0)
			result |= fsm_validation_flags::NO_FINAL_STATES;
	}

	// check that transition starting states are within the number of states of
	// the fsm
	const unsigned nstates = genome.states.size();
	for (auto &t: genome.transitions) {
		if (t.state_from >= nstates)
			result |= fsm_validation_flags::TRANSITION_SOURCE_UNKNOWN;
		if (t.state_to >= nstates)
			result |= fsm_validation_flags::TRANSITION_TARGET_UNKNOWN;
	}

	// only check for duplicate transitions if there are actual transitions and states
	if ((genome.transitions.size() > 0) && (genome.states.size() > 0)) {
		// check to see if there are duplicate transitions
		for (size_t i = 0; i < genome.transitions.size()-1; ++i) {
			for (size_t j = i+1; j < genome.transitions.size(); ++j) {
				if (genome.transitions[i] == genome.transitions[j]) {
					result |= fsm_validation_flags::DUPLICATE_TRANSITION;
				}
			}
		}

		// allocate memory that is initialized to 0 to determine if there are
		// multiple transitions for a (symbol, state) pair (this turns the
		// automaton into an NFA)
		std::vector<int> ts_counter(genome.states.size() * alphabet->n_input_symbols, 0);
		for (auto &t : genome.transitions) {
			// offset into the counter vector
			size_t offset = t.state_from * alphabet->n_input_symbols;
			// increment variable
			ts_counter[offset + t.symbol_read]++;
		}

		// for duplicate transitions, the counter variable increased above 1
		for (auto &c : ts_counter) {
			if (c > 1) {
				result |= fsm_validation_flags::IS_NFA;
				break;
			}
		}
	}

	// validate each transition separately.
	// XXX: transitions are only valid for input symbols, not for blanks
	const size_t n_input_symbols = alphabet->n_input_symbols;
	for (auto &t: genome.transitions) {
		// check if the symbols are within the FSM's alphabet
		// TODO: better return flag (e.g. SYMBOL_IS_BLANK)
		if (t.symbol_read >= n_input_symbols)
			result |= fsm_validation_flags::TRANSITION_SYMBOL_IS_UNKNOWN;
		if (t.symbol_write >= n_input_symbols)
			result |= fsm_validation_flags::TRANSITION_SYMBOL_IS_UNKNOWN;
	}

	return result;
}


/*
 * mutate_states - Mutate the states of an automaton.
 *
 * This mutates the flag of an automaton
 */
inline void
mutate_states(
		const fsm_genome &genome,
		const mutation_rates &mr,
		fsm_genome &target,
		std::vector<transition_gene> &target_transitions,
		std::mt19937_64 *rng,
		// added 2023-02-14
		size_t max_nstates = 3)
{
	std::vector<size_t> start_ids;
	size_t n_accepting_states = 0;

	// we don't know the final number of target states, because we might delete
	// or insert a state. hence, just clear the vector of states in the target.
	target.states.clear();
	target_transitions.clear();

	// OLD CODE (as of before 2023-02-14): we know the number of states, so
	// reserve this amount of states.
	// target.states.reserve(genome.states.size());

	std::vector<size_t> removed_states;
	std::unordered_map<size_t, size_t> state_id_map;

	size_t state_counter = 0;
	for (size_t origin_i = 0; origin_i < genome.states.size(); ++origin_i) {

		// mostly copy, but update the state ID
		state_gene state = {
			.id    = target.states.size(),
			.label = genome.states[origin_i].label,
			.flag  = genome.states[origin_i].flag};

		// drop state?
		if (unif_random(rng) < mr.delete_state) {
			// if this state gets dropped, we need to remove the transitions
			// from/to it. we also need to modify the transition table to
			// reflect that the IDs of all other states changed. We will do this
			// later, but for this we need to track the removed states
			log_verbose("Removing state", origin_i, "\n");
			removed_states.push_back(origin_i);
			continue;
		}

		// mutate state?
		if (unif_random(rng) < mr.modify_state_start)
			state.flag ^= automaton_state_flags::IS_START;

		if (unif_random(rng) < mr.modify_state_accepting)
			state.flag ^= automaton_state_flags::IS_FINAL;

		// record if this is a start state. Needed for later to randomly select
		// a single start state
		if (test(state.flag & automaton_state_flags::IS_START))
			start_ids.push_back(state_counter);

		// simply cound how many accepting states there are
		if (test(state.flag & automaton_state_flags::IS_FINAL))
			n_accepting_states += 1;

		// emplace to avoid copy constructor
		target.states.emplace_back(state);

		// keep track of the state IDs in the new genome
		state_id_map[origin_i] = target.states.size() - 1;

		// increment the new state counter
		++state_counter;
	}

	// maybe add a new state, especially if there's no state to begin with
	if (target.states.size() < max_nstates) {
		if ((target.states.size() == 0) || unif_random(rng) < mr.create_state) {

			// TODO: this is taken from random_genome
			size_t state_id = target.states.size();
			char label = std::to_string(state_id)[0];
			automaton_state_flags flag = automaton_state_flags::DEFAULT;
			state_gene state = {.id = state_id, .label = label, .flag = flag};

			// modify state flag accordingly
			if (unif_random(rng) < mr.modify_state_start)
				state.flag ^= automaton_state_flags::IS_START;

			if (unif_random(rng) < mr.modify_state_accepting)
				state.flag ^= automaton_state_flags::IS_FINAL;

			if (test(state.flag & automaton_state_flags::IS_START))
				start_ids.push_back(state_counter);

			// simply cound how many accepting states there are
			if (test(state.flag & automaton_state_flags::IS_FINAL))
				n_accepting_states += 1;

			// append a new state to the target genome
			target.states.emplace_back(state);
		}
	}

	// go over each transition, if it's start or end is in the list of dropped
	// states, don't further process it. otherwise, check the state ID map to
	// get the proper index. this will fix the transitions in case states got
	// dropped, and rewrite the state IDs
	for (auto &t: genome.transitions) {
		if (contains(removed_states, t.state_from) or contains(removed_states, t.state_to))
			continue;

		// TODO: return a failure state when the ID is missing
		if (state_id_map.count(t.state_from) == 0) {
			log_error("mutate_state: state_id_map missing source key ", t.state_from, "\n");
			continue;
		}
		if (state_id_map.count(t.state_to) == 0) {
			log_error("mutate_state: state_id_map missing target key ", t.state_to, "\n");
			continue;
		}

		// append the packaged transition to the list of transitions
		size_t source_id = state_id_map[t.state_from];
		size_t target_id = state_id_map[t.state_to];
		target_transitions.push_back({.state_from   = source_id,
									  .symbol_read  = t.symbol_read,
									  .state_to     = target_id,
									  .symbol_write = t.symbol_write});
	}


	// determine if there's a start state for the automaton
	if (start_ids.size() == 0) {
		auto id = choice<size_t>(0, target.states.size() - 1, rng);
		target.states[id].flag ^= automaton_state_flags::IS_START;
	}
	// if there are too many start states selected, randomly choose one
	// (indirectly via start_ids)
	else if (start_ids.size() > 1) {
		auto id = choice<size_t>(0, start_ids.size() - 1, rng);
		id = start_ids[id];
		for (auto i: start_ids) {
			if (i == id)
				continue;
			// clear flag on loosers
			target.states[i].flag &= ~automaton_state_flags::IS_START;
		}
	}

	// if there's no accepting state, we simply select one randomly in the case
	// that an empty final state set is disallowed
	if (!DFA_ALLOW_EMPTY_FINAL_STATE_SET) {
		if (n_accepting_states < 1) {
			auto id = choice<size_t>(0, target.states.size() - 1, rng);
			target.states[id].flag ^= automaton_state_flags::IS_FINAL;
		}
	}

	start_ids.clear();
}


/*
 * List of possible (simple) mutations with affected member, condition for the
 * mutation, and range specifier.
 *
 * Note that symbol_read and symbol_write are restricted to real input symbols
 */
#define TRANSITION_MUTATION_LIST \
	X(state_from,   unif_random(rng) < mr.modify_transition_source,   0ul, nstates - 1) \
	X(state_to,     unif_random(rng) < mr.modify_transition_target,   0ul, nstates - 1) \
	X(symbol_read , unif_random(rng) < mr.modify_transition_symbol,   0ul, alphabet.n_input_symbols - 1)   \
	X(symbol_write, unif_random(rng) < mr.modify_transition_emission, 0ul, alphabet.n_input_symbols - 1)

/*
 * mutate_transitions - Mutate the transitions of an automaton.
 */
inline void
mutate_transitions(
		const std::vector<transition_gene> &origin,
		const mutation_rates &mr,
		const basic_alphabet &alphabet,
		const size_t nstates,
		std::vector<transition_gene> &target,
		std::mt19937_64 *rng)
{
	for (auto _t: origin) {
		// drop this transition?
		if (unif_random(rng) < mr.drop_transition)
			continue;

		// get a copy first
		transition_gene t = _t;

		#define X(member, condition, range_from, range_to) \
			if (condition) \
				t.member = choice(range_from, range_to, rng);
			TRANSITION_MUTATION_LIST
		#undef X

		target.emplace_back(std::move(t));
	}

	// create a novel transition?
	if (unif_random(rng) < mr.spawn_transition) {
		transition_gene t;

		#define X(member, condition, range_from, range_to) \
			t.member = choice(range_from, range_to, rng);
			TRANSITION_MUTATION_LIST
		#undef X

		if (!contains(target, t))
			target.emplace_back(std::move(t));
	}
}

/*
 * sort - sort a transition vector
 */
inline void
sort(std::vector<transition_gene> &ts)
{
	std::sort(ts.begin(), ts.end(),
			[](const auto &lhs, const auto &rhs){
				return to_tuple(lhs) < to_tuple(rhs);
			});
}

/*
 * sort - sort a genome
 *
 * At the moment, this only sorts the transitions, not states.
 */
inline void
sort(fsm_genome &genome)
{
	// TODO: when sorting states, this will required to re-link the transitions!
	// That is, need to collect the sort order
	sort(genome.transitions);
}


/*
 * genome_rewrite_labels - rewrite the labels such that they match the ID
 */
inline void
genome_rewrite_labels(fsm_genome &genome)
{
	for (size_t i = 0; i < genome.states.size(); i++) {
		genome.states[i].label = std::to_string(i)[0];
	}
}


/*
 * mutate_genome - Mutate a genome based on given mutation rates.
 */
inline fsm_genome
mutate_genome(
		const fsm_genome &genome,
		const mutation_rates &mr,
		const basic_alphabet &alphabet,
		std::mt19937_64 *rng,
		size_t max_nstates = 3)
{
	fsm_genome target;

	// NOTE: transitions are stored in a vector<transition_gene>, where each
	// transition gene is a POD with four size_ts. We need to get a temporary
	// copy of this vector during mutate_state. Reason is that states might get
	// dropped, which leads to a change in the transitions themselves. If not
	// state got dropped, then tmp_transitions = genome.transitions
	std::vector<transition_gene> tmp_transitions;

	// first mutate states, then the transitions. TODO: think if we want to
	// merge this.
	mutate_states(genome, mr, target, tmp_transitions, rng, max_nstates);
	mutate_transitions(tmp_transitions, mr, alphabet, target.states.size(), target.transitions, rng);

	// rewrite the genome labels so that they match the id
	genome_rewrite_labels(target);

	// sort the new genome
	sort(target);

	return target;
}


/*
 * random_genome - Generate a random genome
 *
 * This function returns a new genome for a given alphabet and a fixed number of
 * states.
 */
inline fsm_genome
random_genome(
		const basic_alphabet* alphabet,
		size_t N_states,
		bool random_emission,
		std::mt19937_64 *rng)
{
	fsm_genome genome;

	// setup some states
#if 0
	auto n_states = choice<size_t>(0, N_states - 1, __rng);
#else
	auto n_states = N_states;
#endif

	for (size_t i = 0; i < n_states; ++i) {
		// TODO: generating the label this way is hacky, needs improvement. If
		//       this changes, also adapt mutate_states accordingly
		char label = std::to_string(i)[0];

		automaton_state_flags flag = i == 0 ? automaton_state_flags::IS_START : automaton_state_flags::DEFAULT;

		// TODO: make this configurable
		if (unif_random(rng) < 0.5)
			flag |= automaton_state_flags::IS_FINAL;

		// push new state
		genome.states.push_back({
				.id    = i,
				.label = label,
				.flag  = flag,
				});

	}

	// setup some new transitions
	double prob = 1 / (double)n_states;
	for (size_t i = 0; i < n_states; ++i) {
		// XXX: this will be done only for input symbols.
		// TODO: treat blank symbols separately
		for(size_t j = 0; j < alphabet->n_input_symbols; ++j) {
			size_t k = 0;
			while (k < n_states) {
				if (unif_random(rng) < prob) {
					break;
				}
				k += 1;
			}
			// found a valid transition to another state?
			if (k < n_states) {

				// determine if the emission should be selected randomly
				size_t l = j;
				if (random_emission)
					l = choice<size_t>(0, alphabet->n_input_symbols - 1, rng);

				// finally setup the transition
				genome.transitions.push_back({
					.state_from   = i,
					.symbol_read  = alphabet->symbols[j].id,
					.state_to     = k,
					.symbol_write = alphabet->symbols[l].id,
				});
			}
		}
	}

	// sort the genome
	sort(genome);

	return genome;
}



inline std::ostream&
operator<< (std::ostream &out, const fsm_genome &genome)
{
	for (size_t i = 0; i < genome.states.size(); ++i) {
		out << "s."				<< i
			<< ", .id = "		<< genome.states[i].id
			<< ", .label = "	<< genome.states[i].label
			<< ", .flag = "     << state_flag_to_str(genome.states[i].flag)
			<< "\n";
	}

	for (size_t i = 0; i < genome.transitions.size(); ++i) {
		out << "t."					<< i
			<< ", .state_from = "	<< genome.transitions[i].state_from
			<< ", .symbol_read = "	<< genome.transitions[i].symbol_read
			<< ", .state_to = "		<< genome.transitions[i].state_to
			<< ", .symbol_write = "	<< genome.transitions[i].symbol_write
			<< "\n";
	}
	return out;
}


/*
 * genome_to_str - turn a genome into a string representation
 *
 * The string encoding first tells the number of states, then information about
 * the states, followed by the number of transitions with repeated blocks of
 * transition details:
 *
 *	N_states, s0_flag, ..., sN_flag, M_transitions, t0_from, t0_read, t0_to, t0_write, ..., tM_from, tM_read, tM_to, tM_write
 *
 */
inline std::string
genome_to_str(const fsm_genome &genome)
{
	std::stringstream strs;

	// NOTE: we do not write the label here, as this will be generated from the
	// ID during load. This is (currently) required as the label is not
	// functionally important, and using it within the string has impact on
	// using the string as key for a hash-map
	// NOTE: We also do not write the ID, because the ID is given by the
	// position of the symbol

	strs << genome.states.size() << " ";
	for (const auto &s: genome.states) {
		// strs << s.id    << " ";
		// strs << s.label << " ";
		strs << s.flag << " ";
	}
	strs << genome.transitions.size() << " ";
	for (const auto &t: genome.transitions) {
		strs << t.state_from << " "
			 << t.symbol_read << " "
			 << t.state_to << " "
			 << t.symbol_write << " ";
	}

	// pop potential whitespace at end
	std::string s = strs.str();
	rtrim(s);

	return s;
}


/*
 * genome_from_str - very simple tokenizer to turn a string into a genome
 *
 * TODO: this will break if the string does not match the encoding scheme of
 *       genome_to_str!
 */
inline fsm_genome
genome_from_str(std::string encoded)
{
	// turn into string stream
	std::istringstream iss{encoded};

	fsm_genome genome;

	size_t n_states;
	iss >> n_states;
	for (size_t i = 0; i < n_states; i++) {
		automaton_state_flags flag = {};

		// parse the flag from the string
		unsigned tmp;
		iss >> tmp;
		flag = static_cast<automaton_state_flags>(tmp);

		// append state to genome
		genome.states.push_back({
				.id    = i,
				.label = std::to_string(i)[0],
				.flag  = flag,
				});
	}

	size_t n_transitions;
	iss >> n_transitions;
	for (size_t i = 0; i < n_transitions; i++) {
		size_t from, read, to, write;
		iss >> from
			>> read
			>> to
			>> write;
		genome.transitions.push_back({
				.state_from = from,
				.symbol_read = read,
				.state_to = to,
				.symbol_write = write,
				});
	}

	return genome;
}


inline void
write_genomes_to_file(
		const std::vector<fsm_genome> &genomes,
		const std::string filename = "genomes.dfa")
{
	std::ofstream stream;
	stream.open(filename);

	if (!stream.is_open()) {
		log_error("Could not open file \"", filename, "\" for writing.");
		return;
	}
	for (auto &genome : genomes)
		stream << genome_to_str(genome) << "\n";
	stream.close();
}

inline void
write_genomes_to_file(
		const std::map<std::string, fsm_genome> &genomes,
		const std::string filename = "genomes.dfa")
{
	std::ofstream stream;
	stream.open(filename);

	if (!stream.is_open()) {
		log_error("Could not open file \"", filename, "\" for writing.");
		return;
	}
	// in the case of the map, the key is already the stringified genome
	for (auto &elem: genomes)
		stream << elem.first << "\n";
	stream.close();
}


/*
 * load_genomes_from_file - Load genomes from a file
 *
 * This function takes a filename and a callback. It will then read all lines
 * from the file and decode the genome, passing both the key of the genome, i.e.
 * its encoded string, as well as the instantiated genome.
 */
inline void
load_genomes_from_file(
	const std::string filename,
	void (*cb)(std::string key, fsm_genome, size_t id))
{
	if (!cb) return;

	std::ifstream stream{filename};
	if (!stream.is_open())
		return;

	size_t i = 0; // line = ID
	std::string line;
	while (std::getline(stream, line)) {
		// remove potential whitespace and add to vector
		trim(line);

		// convert line to genome and re-sort
		auto genome = genome_from_str(line);
		sort(genome);

		// callback so that the user can do with the genome whatever is needed,
		// e.g. push to vector or insert into map
		cb(line, genome, i);
		i += 1; // increment ID by 1
	}

	stream.close();
}


// forward declaration
struct transition;

// containers for transitions
using transition_ptr_vector = std::vector<transition*>;
using transition_ptr_table  = std::vector<transition_ptr_vector>;


/*
 * struct state - Concrete / realized state
 */
struct state {
	// each state has an ID within the slab of memory it is currently stored
	size_t   id;

	// a state also tracks the ID of the gene it was translated from. Most
	// often, this will be identical with id, but can differ for instance during
	// minimization of an FSM.
	size_t   gene_id;

	// Each state has a human-readable symbol, which ideally is the id
	// TODO: remove the label, as it is functionally not used anywhere
	char     label;

	// A state can have multiple flags, such as being an accepting or starting
	// state
	automaton_state_flags flag = {};

	// For ease of traversal, each state also contains pointers to all
	// transitions that are in- or outgoing from this specific state. Thereby,
	// the set of states and transitions forms a well defined Graph, on which
	// graph algorithms can be easily run.
	transition_ptr_vector transitions_outgoing;
	transition_ptr_vector transitions_incoming;

};

using state_vector     = std::vector<state>;
using state_ptr_vector = std::vector<state*>;


// TODO: generate the next 20 lines of code
inline bool
is_start(const state *s) {
	return test(s->flag & automaton_state_flags::IS_START);
}

inline bool
is_start(const state &s) {
	return is_start(&s);
}

inline bool
is_final(const state *s) {
	return test(s->flag & automaton_state_flags::IS_FINAL);
}

inline bool
is_final(const state &s) {
	return is_final(&s);
}


/*
 * get_state_by_index - Get a state by index
 *
 * This function returns the state given a certain index into the array of
 * states within a genome. Note that this is different than getting a state by
 * its label!
 */
inline state *
get_state_by_index(const state_ptr_vector &vec, size_t idx)
{
	if (idx > vec.size())
		return nullptr;
	return vec[idx];
}


/*
 * get_state_index - find the index of a state given its ID
 */
inline int
get_state_index_by_id(const state_ptr_vector states, size_t id)
{
	for (size_t i = 0; i < states.size(); i++) {
		if (states[i]->id == id)
			return i;
	}
	return -1;
}


/*
 * get_state_by_label - Get a state by it's label
 *
 * This function returns a state by searching its label. If the label cannot be
 * found, then the returned value is nullptr.
 */
inline state *
get_state_by_label(const state_ptr_vector &vec, char label)
{
	for (size_t i = 0; i < vec.size(); ++i) {
		if (vec[i]->label == label)
			return vec[i];
	}
	return nullptr;
}

/*
 * get_state_by_gene_id - get a state by it's associated Gene-ID
 *
 * This function returns a state by searching its gene_id. If the label cannot
 * be found, then the returned value is nullptr.
 */
inline state *
get_state_by_gene_id(const state_ptr_vector &vec, size_t gene_id)
{
	for (size_t i = 0; i < vec.size(); ++i) {
		if (vec[i]->gene_id == gene_id)
			return vec[i];
	}
	return nullptr;
}

inline bool
contains(const state_ptr_vector &states, state *needle) {
	for (size_t i = 0; i < states.size(); ++i) {
		if (states[i] == needle)
			return true;
	}
	return false;
}


/*
 * get_initial_state - get the initial state of an FSM
 *
 * TODO: what if there are multiple initial states?
 */
inline state *
get_initial_state(const state_ptr_vector states)
{
	for (auto s: states) {
		if (is_start(s)) return s;
	}
	return nullptr;
}


/*
 * struct transition - Concrete / realized transition
 */
struct transition {
	size_t   id;

	// TODO: maybe alias to origin, destination
	state *from;
	state *to;

	// TODO: maybe change to pointers to symbols in the alphabet
	size_t   read;
	size_t   write;
};


// TODO: generate the following code automatically

/*
 * in_domain - determine if a state is in the domain of a transition.
 */
inline bool
in_domain(const state *s, const transition *t)
{
	return t->from == s;
}

inline bool
in_domain(const state &s, const transition &t)
{
	return in_domain(&s, &t);
}

inline bool
is_defined_for(const transition &t, const state &s)
{
	return in_domain(s, t);
}


/*
 * in_image - determine if a state is in the image of a transition
 */
inline bool
in_image(const state *s, const transition *t)
{
	return t->to == s;
}

inline bool
in_image(const state &s, const transition &t)
{
	return in_image(&s, &t);
}

inline bool
leads_to(const transition t, const state s)
{
	return in_image(s, t);
}

inline bool
contains(const transition_ptr_vector &transitions, transition *needle) {
	for (size_t i = 0; i < transitions.size(); ++i) {
		if (transitions[i] == needle)
			return true;
	}
	return false;
}


/*
 * init_transition_table - Initialize a transition table
 *
 * Note: The transition table consideres the full set of symbols in an alphabet,
 * not only input symbols.
 */
inline void
init_transition_table(
		const state_ptr_vector &states,
		const transition_ptr_vector &transitions,
		const size_t n_symbols,
		transition_ptr_table &transition_table
		)
{
	transition_table.resize(states.size());
	for (size_t i = 0; i < states.size(); ++i) {
		transition_table[i].resize(n_symbols, nullptr);
	}

	// walk through all transitions and store pointers to them in the transition
	// table
	for (transition *t : transitions) {
		transition_table[t->from->id][t->read] = t;
	}
}


// TODO: merge with code above, as it is mostly a duplicate. Maybe use a
// template, or XMacro List
inline
state_ptr_vector
get_reachable_states(
		const state_ptr_vector &states,
		const transition_ptr_vector &transitions)
{

	state_ptr_vector reachable;
	state_ptr_vector unprocessed;

	auto initial_state = get_initial_state(states);
	unprocessed.push_back(initial_state);
	reachable.push_back(initial_state);

	while (unprocessed.size() > 0) {
		// get one of the unprocessed states and remove it from the list
		auto s = unprocessed.back();
		unprocessed.pop_back();

		// go over all transitions and check if the transition is defined for
		// the state. If yes, then add all
		for (auto &t: transitions) {
			if (in_domain(s, t)) {
				state *target = t->to;
				if (!contains(reachable, target)) {
					reachable.push_back(target);

					// if this item is reachable, and not yet in the unprocessed
					// stack, move it there to address it in a future loop
					if (!contains(unprocessed, target)) {
						unprocessed.push_back(target);
					}
				}
			}
		}
	}

	return reachable;
}


inline
state_ptr_vector
get_unreachable_states(
		const state_ptr_vector &states,
		const transition_ptr_vector &transitions)
{
	state_ptr_vector unreachable;
	state_ptr_vector reachable;

	// get all reachable states, then store all those states which are not
	// reachable
	reachable = get_reachable_states(states, transitions);
	for (size_t i = 0; i < states.size(); ++i) {
		if (!contains(reachable, states[i])) {
			unreachable.push_back(states[i]);
		}
	}

	return unreachable;
}



/*
 * dfa_remove_unreachable - remove all states and transitions which are not reachable
 *
 */
inline
std::tuple<state_ptr_vector, transition_ptr_vector>
dfa_remove_unreachable(
		const state_ptr_vector &states,
		const transition_ptr_vector &transitions)
{
	state_ptr_vector ss;
	transition_ptr_vector ts;

	// get all reachable states
	auto reachable = get_reachable_states(states, transitions);

	// copy over all reachable items
	for (size_t i = 0; i < reachable.size(); ++i) {
		ss.push_back(reachable[i]);

		// TODO: can this be sneaked back into get_reachable_states?
		for (transition *t: transitions) {
			if (in_domain(reachable[i], t) || in_image(reachable[i], t)) {
				if (!contains(ts, t))
					ts.push_back(t);
			}
		}
	}

// some debug print
#if DEBUG_VERBOSE
	std::stringstream str;

	str << "new genome has " << ss.size() << " states:\n";
	for (size_t i = 0; i < ss.size(); ++i) {
		str << "index " << i << ", label " << ss[i]->label << "\n";
	}
	log_verbose(str.str());
	str.str(std::string());

	str << "new genome has " << ts.size() << " transitions:\n";
	for (size_t i = 0; i < ts.size(); ++i) {
		str
			<< "index "			<< i
			<< ", .id = "		<< ts[i]->id
			<< ", .from = "		<< ts[i]->from->label
			<< ", .to = "		<< ts[i]->to->label
			<< ", .read = "		<< ts[i]->read
			<< ", .write = "	<< ts[i]->write
			<< "\n";
	}
	log_verbose(str.str());
#endif

	// return tuple of reachable states and transitions
	return {ss, ts};
}


/*
 * remove_dead_states - remove dead stats from a genome
 *
 * Dead states are all those states which are not accepting (final), but lead
 * back to themselves for any symbol in the alphabet.
 */
inline
state_ptr_vector
remove_dead_states(
		const state_ptr_vector &/*states*/,
		const transition_ptr_vector &/*transitions*/)
{
	// TODO: implement. Maybe this is already covered in remove_unreachable
	// states?
	//
	// TODO: be careful, simply walking over all states and testing their
	// transitions will not be sufficient, as removal of one state could end up
	// in a new state being dead! consider the simple automaton
	//    -> q0 -> q1 -> q2 -> q3
	// while in a first run, q3 will be marked as dead, this will make q2 dead,
	// which in return makes q1 dead and finally q0. Hence, we have an entire
	// dead branch.
	// TODO: is it possible to first find all dead-ends, and then propagate this
	//       back?
	//
	// the solution to this function is to reverse all transitions, mark all
	// final states as starting states, and then compute the set of reachable
	// states. all those states which are not reachable are dead/useless.

	state_ptr_vector result;

	return result;
}


/*
 * struct partition - hold a partition
 *
 * This data structures is needed for minimizing a DFA
 */
struct partition {
	std::vector<std::vector<int>> subsets;
	std::vector<int> subset_map;
};


/*
 * declare the type to be used for computations with the successor map. The
 * successor map will contain the ID (size_t) of a state when following a
 * certain transition. The successor map will contain a -1 if a certain
 * transition does not exist.
 *
 * Example:
 *          Alphabet
 * States |  a  |   b
 * -------------------
 *   ->q0 | q1  | -1
 *     q1 | q1  | q2
 *     q2 | q2  | q4
 *     q3 | q3  | q4
 *    *q4 | q2  | q4
 *
 * The above table is the successor map for a DFA with starting symbol q0,
 * accepting state q4, and which accepts some string that only starts with
 * symbols a (there is no transition defined for q0 x b)
 *
 */
using successor_map_t = std::vector<std::vector<ptrdiff_t>>;


/*
 * init_partition - initialize a partition
 *
 * This function resizes .subsets and .subset_map of a partition.
 */
inline void
init_partition(
		partition &p,
		const size_t n_subsets,
		const size_t n_elems)
{
	p.subsets.resize(n_subsets);
	p.subset_map.resize(n_elems);
}


inline void
print_successor_map(const successor_map_t map)
{
	std::cout << "successor map:\n";
	for (size_t i = 0; i < map.size(); i++) {
		for (size_t k = 0; k < map[i].size(); k++) {
			std::cout << map[i][k] << ", ";
		}
		std::cout << "\n";
	}
}


/*
 * are_distinguishable - find out if two states i,j are distinguishable
 *
 * Two states q_i and q_j are distinguishable in a partition, iff for any
 * symbol a of the alphabet, p_i = transition(q_i, a) and p_j =
 * transition(q_j, a), p_i and p_j are in different subsets of the partition
 */
inline bool
are_distinguishable (
		const basic_alphabet *alphabet,
		const successor_map_t successor_map,
		const partition p,
		const int i,
		const int j)
{
	// let's hope the compiler unrolls this for small alphabets...
	for (size_t k = 0; k < alphabet->n_symbols; k++) {

		const int p_i = successor_map[i][k];
		const int p_j = successor_map[j][k];

		// check for DFA_TRANSITION_UNDEFINED in either of the two, which indicates
		// that there is no transition for a certain symbol of the alphabet
		int l = 0;
		if (p_i == DFA_TRANSITION_UNDEFINED) ++l;
		if (p_j == DFA_TRANSITION_UNDEFINED) ++l;

		// if only one of the two was DFA_TRANSITION_UNDEFINED, but the other was not,
		// then l will be 1. If both were DFA_TRANSITION_UNDEFINED, then l will be 2,
		// in which case we are not sure and should continue the loop.
		if (l == 1)
			return true;
		if (l == 2)
			continue;

#if DEBUG_VERBOSE
		if ((size_t)p_i >= p.subset_map.size()) {
			log_error("p_i = ", p_i, " out of bounds (subset_map.size() = ", p.subset_map.size(), ").\n");
			print_successor_map(successor_map);
		}
		if ((size_t)p_j >= p.subset_map.size()) {
			log_error("p_j = ", p_j, " out of bounds (subset_map.size() = ", p.subset_map.size(), ").\n");
			print_successor_map(successor_map);
		}
#endif

		// in case that the transitions lead to elements that are in different
		// subsets, then the two items are distinguishable
		if (p.subset_map[p_i] != p.subset_map[p_j])
			return true;
	}
	return false;
}


/*
 * are_equal_partitions - compare two partitions
 *
 * Two partitions are equal, if their subset_maps are equal. The reason is that
 * the subset_map contains, for each individual state of the DFA, the subset
 * index to which this element belongs. So, for instance, the partition
 *   { {q0, q1, q2}, {q3, q4} }
 * will have the subset_map [0, 0, 0, 1, 1], which is unique as long as the
 * states are properly ordered.
 */
inline bool
are_equal_partitions (
		const partition &left,
		const partition &right)
{
	if (left.subset_map.size() != right.subset_map.size())
		return false;
	for (size_t i = 0; i < left.subset_map.size(); ++i)
		if (left.subset_map[i] != right.subset_map[i])
			return false;
	return true;
}


/*
 * init_successor_map - compute the successor table for Q x Σ -> Q.
 *
 * Specifically, for a state q_j and a symbol q_i, this stores i. Thus, the
 * memory requirements for this table are n * k.  The runtime of this function
 * depends on the runtime of the .resize() operator. under the assumption that
 * it is constant, then the runtime is bounded by O(n * k).
 *
 * Note that this is different from the transition table. While the transition
 * table stores pointers to transitions, the successor map stores indices of
 * target states. While they are certainly related, it is slightly easier to
 * work with the successor map
 *
 */
inline void
init_successor_map(
		const basic_alphabet *alphabet,
		const state_ptr_vector &states,
		const transition_ptr_vector &transitions,
		successor_map_t &successor_map)
{
	// TODO: could infer size of all states from the transition_table
	auto n_states = states.size();

	// initialize the successor map for n_states x n_symbols many entries
	successor_map.resize(n_states);
	for (size_t j = 0; j < n_states; ++j) {
		// resize row j of the successor map to have space for all symbols. The
		// successor map will be initialized to DFA_TRANSITION_UNDEFINED, which
		// indicates that this transition does not exist
		successor_map[j].resize(alphabet->n_symbols, DFA_TRANSITION_UNDEFINED);
	}

	// go through all transitions, find the indexes, and map accordingly
	for (size_t i = 0; i < transitions.size(); ++i) {
		auto *t = transitions[i];

		auto index_from = get_index_of(states, t->from);
		auto index_to   = get_index_of(states, t->to);
		auto index_sym  = t->read;

		if (index_from && index_to)
			successor_map[index_from.value()][index_sym] = index_to.value();
	}
}


/*
 * dfa_compute_equivalence_sets - Compute the set of equivalent states
 *
 * This computes the set of equivalent states given an alphabet and transitions
 * between states. The implementation is based on the Equivalence Theorem, which
 * successively computes subsets of states which are equivalent.
 *
 * The function returns a partition, which contains all subsets as well as
 * subset_map, which indicates for every state in which subset it lives.
 *
 * Note that this is a rather straightforward implementation, without overly
 * strong consideration of runtimes. If this appears to be a bottleneck, then
 * alternative algorithms for computation might be Hopcroft, 1971 or Valmari,
 * 2012.
 *
 */
inline partition
dfa_compute_equivalence_sets(
		const basic_alphabet        *alphabet,
		const state_ptr_vector      &states,
		const transition_ptr_vector &transitions)
{
	// extract some information from the alphabet and the states
	const size_t n_states = states.size();

	// successor map -> this is a transition table of the size n*k
	successor_map_t successor_map;
	init_successor_map(alphabet, states, transitions, successor_map);

	// initialize the partition table. Each element belongs to exactly one set
	// within the partition. In the beginning, there are only two such subsets,
	// and each subset is indicated by an index. The runtime of this part is
	// O(n), as we need to walk over all nodes in the graph (automaton)
	partition current;
	// in the beginning, there will be two subsets (set of final states, set of
	// all states except final states). The second argument to init_partition is
	// the maximal number of elements - here, that's the number of states
	init_partition(current, 2, n_states);

	for (size_t j = 0; j < n_states; ++j) {
		int s = is_final(states[j]) ? 1 : 0;
		current.subset_map[j] = s;
		current.subsets[s].push_back(j);
	}

	// for the next partition, we don't plan with any
	partition next;
	init_partition(next, 0, n_states);

	// continue subset computation until there's no change in the partition
	// anymore
	bool converged = false;
	do {
		// reset data for next iteration. We'll make use of the
		// DFA_STATE_NOT_PROCESSED as an indicator flag if a certain state has
		// been processed already or not
		next.subsets.clear();
		for (size_t i = 0; i < n_states; ++i) {
			next.subset_map[i] = DFA_STATE_NOT_PROCESSED;
		}

		for (size_t s = 0; s < current.subsets.size(); ++s) {
			auto _size = current.subsets[s].size();

			// if this is an empty subset (maybe there was no final state, maybe
			// there are only final state), then skip this subset
			if (_size == 0) {
				continue;
			}

			// if this subset is singleton, we simply move it forward and also
			// store the (potentially new) subset_map entry
			if (_size == 1) {
				int state = current.subsets[s][0];
				next.subsets.push_back(std::vector<int>{state});
				next.subset_map[state] = next.subsets.size() - 1;
				continue;
			}

			// for each pair in each subset of the partition, we need to figure out if
			// they are distinguishable. Note that we walk to i < _size here, to
			// make sure that every item in the subset gets tested if it already
			// is within a set or not in the next few lines below
			for (size_t i = 0; i < _size; ++i) {
				const int q_i = current.subsets[s][i];

				// we walk over every item/state, and if this state has not yet
				// a set assigned to which it belongs, then we need to actually
				// create one. A state might get assigned to a certain state
				// below, when it was compared to another state in a previous
				// iteration of the loop.
				if (next.subset_map[q_i] < 0) {
					next.subsets.push_back(std::vector<int>{q_i});
					next.subset_map[q_i] = next.subsets.size() - 1;
				}

				for (size_t j = i + 1; j < _size; ++j) {
					const int q_j = current.subsets[s][j];

					// We can skip this item, if it was already processed (due to
					// transitivity of state equality)
					if (next.subset_map[q_j] >= 0) {
						continue;
					}

					bool dist = are_distinguishable(alphabet, successor_map, current, q_i, q_j);
					if (!dist) {
						// not distinguishable, store in same subsets
						size_t _subs = next.subset_map[q_i];
						next.subsets[_subs].push_back(q_j);
						next.subset_map[q_j] = _subs;
					}
				}
			}
		}

		// we can compare the two partitions by looking at the subset_map
		converged = are_equal_partitions(current, next);
		if (!converged) {
			// std::swap(current, next);
			current.subsets.swap(next.subsets);
			current.subset_map.swap(next.subset_map);
		}
	}
	while (!converged);

#if DEBUG_VERBOSE
	// print results
	std::stringstream str;
	str << "subset map: ";
	for (size_t i = 0; i < next.subset_map.size(); ++i) {
		str << next.subset_map[i] << " ";
	}
	str << "\n";
	log_verbose(str.str());

	for (size_t i = 0; i < next.subsets.size(); ++i)  {
		str.str(std::string());
		str << "set " << i << ": ";
		for (size_t j = 0; j < next.subsets[i].size(); ++j) {
			str << next.subsets[i][j] << " ";
		}
		str << "\n";
		log_verbose(str.str());
	}
#endif

	// the result is stored in next (actually, it's also stored in current,
	// because the two are equal...)
	return next;
}


/*
 * dfa_merge_equivalent_sets - Merge states that are equivalent
 *
 * Note that the returned vector contains pointers to the *original* states!
 * Thereby, the returned states can and will have an .id which differs from
 * their position in the returned vector.
 */
inline state_ptr_vector
dfa_merge_equivalent_sets(
		const state_ptr_vector &states,
		const partition &partition)
{
	state_ptr_vector result;
	for (auto &subset: partition.subsets) {
		if (subset.size() <= 0)
			continue;
		// simply pick the first in every subset
		result.push_back(states[subset[0]]);
	}

#if DEBUG_VERBOSE
	std::stringstream str;
	str << "merged states: \n";
	for (size_t i = 0; i < result.size(); ++i) {
		str
			<< "index "			<< i
			<< ", .id = "		<< result[i]->id
			<< ", .gene_id = "  << result[i]->gene_id
			<< ", .label = "	<< result[i]->label
			<< "\n";
	}
	log_verbose(str.str());
#endif

	return result;
}


/*
 * dfa_get_merged_transitions - Get all transitions for the set of merged states
 *
 * This function walks all transitions in the argument and returns the subset of
 * those transitions which actually have an end-point in the set of states.
 *
 * Note that the returned vector contains only pointers to the original
 * transitions!
 */
inline transition_ptr_vector
dfa_get_merged_transitions(
		const state_ptr_vector &states,
		const transition_ptr_vector &transitions)
{
	transition_ptr_vector result;
	for (auto &t: transitions) {
		// copy over only those transition pointers with origin and destination
		// in the given set of states.
		if (contains(states, t->from) && contains(states, t->to)) {
			result.push_back(t);
		}
	}

#if DEBUG_VERBOSE
	std::stringstream str;
	str << "merged transitions: \n";
	for (size_t i = 0; i < result.size(); ++i) {
		str << "t" << i << " ("
			<< result[i]->from->label << " -> "
			<< result[i]->to->label << ")\n";
	}
	log_verbose(str.str());
#endif

	return result;
}


/*
 * minimize - Minimize a DFA given by alphabet, states, and transitions
 *
 *
 * TODO: There are several points how to speed up the function. For instance,
 *       computation of merged transitions could be sped up by not calling
 *       contains twice within the function, but by marking them during
 *       compute_equivalence_sets. This is, what Valmari, 2014, actually
 *       implements.
 */
inline
std::tuple<state_ptr_vector, transition_ptr_vector>
dfa_minimize(
		const basic_alphabet *alphabet,
		const state_ptr_vector &states,
		const transition_ptr_vector &transitions)
{
	// TODO: remove dead/and useless states
	// remove_dead_states(fsm.states, fsm.transitions);

	// remove unreachable states from the list of states and transitions
	log_verbose("dfa_remove_unreachable\n");
	auto [ss_reachable, ts_reachable] = dfa_remove_unreachable(states, transitions);

	log_verbose("dfa_compute_equivalence_sets\n");
	partition equiv_sets = dfa_compute_equivalence_sets(alphabet, ss_reachable, ts_reachable);

	// merge states
	log_verbose("dfa_merge_equivalent_sets\n");
	state_ptr_vector ss_merged = dfa_merge_equivalent_sets(ss_reachable, equiv_sets);

	// copy remaining transitions
	log_verbose("dfa_get_merged_transitions\n");
	transition_ptr_vector ts_merged = dfa_get_merged_transitions(ss_merged, ts_reachable);

	// return minimized states and transitions
	return {ss_merged, ts_merged};
}


/*
 * struct finite_state_machine - A finite state machine.
 *
 * This struct holds all the information that is required to step through a
 * finite state machine.
 *
 * Currently, the implementation does not make use of explicit states. Rather,
 * it holds a genome, which entirely describes the automaton, its states, and
 * its transitions, as well as a pointer to the current state.
 */
struct finite_state_machine {
	// indicates if the FSM was initialized, e.g. by call to function init
	bool
		initialized = false;

	// indicates the current state of the FSM (required during a run of the FSM)
	state
		*current_state = nullptr;

	// FSM alphabet that this FSM was designed for
	const basic_alphabet*
		alphabet;

	// Genome that contains all the information about the states and transitions
	// of the FSM
	fsm_genome
		genome;

	// the realized states and transitions are stored in the following two
	// vectors.
	// Note that these vectors also are supposed to have the ownership of the
	// objects! That is, if you remove a state or transition from one of the
	// vectors, you are also responsible to release the memory, if needed.
	state_ptr_vector
		states = {};

	transition_ptr_vector
		transitions = {};

	// pointers to all starting states
	state_ptr_vector
		starting_states = {};

	// pointers to all accepting states
	state_ptr_vector
		accepting_states = {};

	// transition table for faster runs. This stores the transitions such that
	// [j][c] is the j-th row containing all transitions for the j-th state, and
	// the column c represents the symbol read for this transition.
	//
	// NOTE: this table contains the transitions themselves, *not* the
	//       transition target!
	//
	// NOTE: The transition table contains nullptr if a transition is not
	//       defined for a certain Q x Σ pair!
	transition_ptr_table
		transition_table = {};

	// TODO:
	//  * add storage for trace of a run
	//  * add an accumulator for the fitness of this FSM
};



/*
 * translate - Translate a genome into realized states and transitions
 *
 * Note that this function allocates memory for new objects (via new)
 */
inline
std::tuple<state_ptr_vector, state_ptr_vector, state_ptr_vector, transition_ptr_vector>
fsm_translate(const fsm_genome &genome)
{
	state_ptr_vector states;
	state_ptr_vector starts;
	state_ptr_vector accepting;
	transition_ptr_vector transitions;

	// instantiate states from the genome.
	for (size_t i = 0; i < genome.states.size(); ++i) {
		state *s = new state;
		s->id      = i;
		s->gene_id = genome.states[i].id;
		s->label   = genome.states[i].label;
		s->flag    = genome.states[i].flag;
		states.push_back(s);
		if (is_start(s))
			starts.push_back(s);
		if (is_final(s))
			accepting.push_back(s);
	}

	// instantiate all transitions. Makes sure to map the proper indices for the
	// list of states
	for (size_t i = 0; i < genome.transitions.size(); ++i) {
		transition *t = new transition;
		t->id    = i;
		t->from  = get_state_by_gene_id(states, genome.transitions[i].state_from);
		t->to    = get_state_by_gene_id(states, genome.transitions[i].state_to);
		// TODO: maybe pointer to symbol ?
		t->read  = genome.transitions[i].symbol_read;
		t->write = genome.transitions[i].symbol_write;
		transitions.push_back(t);

		// add this transition to the states' transition vectors
		auto s_from = t->from;
		if (s_from) {
			if (!contains(s_from->transitions_outgoing, t))
				s_from->transitions_outgoing.push_back(t);
		}
		auto s_to = t->to;
		if (s_to) {
			if (!contains(s_to->transitions_incoming, t))
				s_to->transitions_incoming.push_back(t);
		}
	}

	// return tuple of states and transitions
	return {states, starts, accepting, transitions};
}


/*
 * fsm_encode_genome - (Re-)compute the genome for given states and transitions
 *
 * This function takes states and transitions and produces the genome that
 * corresponds to these. In a sense, it is the inverse to `translate` (inverse
 * not in a mathematical sense).
 *
 * TODO: move this to genome.hpp
 */
inline fsm_genome
fsm_encode_genome(
		const state_ptr_vector &states,
		const transition_ptr_vector &transitions,
		bool valid_state_ids = false)
{
	fsm_genome result;

	// resize memory
	result.states.resize(states.size());
	result.transitions.resize(transitions.size());

	// build a lookup-table for the IDs if necessary
	// TODO: if valid_state_ids is false, this can most likely be skipped
	size_t *id_lookup_table;
	size_t max_id = 0;
	for (auto &s: states)
		max_id = s->id > max_id ? s->id : max_id;

	id_lookup_table = new size_t[max_id + 1];

	// encode states
	for (size_t i = 0; i < states.size(); ++i) {
		result.states[i].label  = states[i]->label;
		result.states[i].flag   = states[i]->flag;

		size_t state_id = states[i]->id;
		if (valid_state_ids) {
			result.states[i].id = state_id;
			id_lookup_table[state_id] = state_id;
		}
		else {
			// store 'new' ID, because the state's ID does not match its
			result.states[i].id = i;
			// store the 'new' ID in the lookup table at the location
			// of the old ID
			id_lookup_table[state_id] = i;
		}
	}

	// encode transitions
	for (size_t i = 0; i < transitions.size(); ++i) {
		// symbols can be copied directly
		result.transitions[i].symbol_read    = transitions[i]->read;
		result.transitions[i].symbol_write   = transitions[i]->write;

		// use the ID from the lookup table
		size_t origin_id      = id_lookup_table[transitions[i]->from->id];
		size_t destination_id = id_lookup_table[transitions[i]->to->id];

		// store IDs
		result.transitions[i].state_from = origin_id;
		result.transitions[i].state_to   = destination_id;
	}

	// sort the genome
	sort(result);

	// free memory if necessary
	delete[] id_lookup_table;

	log_verbose("encoded states and transitions: \n");
	log_verbose(result, "\n");

	return result;
}


/*
 * init - initialize a finite state machine
 *
 * This function takes an FSM and first transcribes its genome into proper
 * states and transitions. Then, the transition table will be initialized
 */
inline void
fsm_init(finite_state_machine &fsm)
{
	// translate the genome into real states and transitions
	auto [states, starts, accepting, transitions] = fsm_translate(fsm.genome);
	fsm.states           = states;
	fsm.transitions      = transitions;
	fsm.starting_states  = starts;
	fsm.accepting_states = accepting;

	// initialize the transition table
	init_transition_table(
			fsm.states,
			fsm.transitions,
			fsm.alphabet->n_symbols,
			fsm.transition_table);

	// set flag. might be important later on
	fsm.initialized = true;
}


/*
 * fsm_free - release an FSM, and all memory that was acquired for it
 */
inline void
fsm_free(finite_state_machine &fsm)
{
	if (!fsm.initialized) {
		log_error("fsm_free called on an un-initialized FSM!\n");
		return;
	}

	for (size_t i = 0; i < fsm.states.size(); ++i) {
		delete fsm.states[i];
		fsm.states[i] = nullptr;
	}
	for (size_t i = 0; i < fsm.transitions.size(); ++i) {
		delete fsm.transitions[i];
		fsm.transitions[i] = nullptr;
	}
	fsm.transition_table.clear();
	fsm.initialized = false;
}


/*
 * validate - validate a finite state machine.
 *
 * This function takes a finite state machine and examines if the FSM is valid
 * or not. For instance, an FSM with multiple starting states is usually not
 * considered to be a valid FSM. An FSM is also not considered valid if it
 * contains transitions with unknown source or target states.
 *
 * TODO: figure out if there are more things to check, e.g. that symbols that
 *       are read/written during transit are actually valid from a given
 *       alphabet.
 */
inline
fsm_validation_flags
fsm_validate(finite_state_machine &fsm)
{
	// first, validate the genome itself
	auto result = validate_genome(fsm.alphabet, fsm.genome);

	// TODO: check if the FSM is stale, that is if the genome does not match to
	//       the instantiated states. This could be fixed simply by
	//       re-initializing the FSM

	return result;
}


/*
 * reset - Reset a finite state machine.
 *
 * This will remove/deallocate any data stored within the FSM and set the
 * current_state indicator to the starting state of the FSM.
 *
 * This function assumes that the FSM is valid. That is, it contains only a
 * singular starting state
 *
 * TODO: return value
 * TODO: what if there are multiple initial states?
 */
inline void
fsm_reset(finite_state_machine &fsm)
{
	// TODO: reset the trace, recompute the set of all accepting states


	// determine starting state
	// TODO: this could essentially be integrated into validate, i.e. have one
	// specific flag inside the struct that tells the index of the starting
	// state, and then avoid having to walk the entire set of states here
	fsm.current_state = get_initial_state(fsm.states);
}


struct fsm_run_log
{
	// vector containing the FSM's trace, i.e. all transitions that happened
	// during the run. This can be useful to re-trace through the automaton.
	std::vector<transition*>
		trace;

	// information about the input string.
	// This contains the entire string that got passed to the agent
	basic_string_t
		input;

	std::string
		input_string;

	unsigned
		input_length;

	// information about the accepted string. This might not be
	basic_string_t
		accepted;

	std::string
		accepted_string;

	unsigned
		accepted_length;

	// TODO: also store what the FSM produced in case we're operating with
	//       transducers
	basic_string_t
		output;

	std::string
		output_string;

	unsigned
		output_length;

};

inline
void
reset_run_log(fsm_run_log &log)
{
	// TODO: maybe set length to -1 to indicate some erroneous state
	log.trace.resize(0);

	log.input.clear();
	log.input_string = "";
	log.input_length = 0;

	log.accepted.clear();
	log.accepted_string = "";
	log.accepted_length = 0;

	log.output.clear();
	log.output_string = "";
	log.output_length = 0;
}


/*
 * run - run a finite state machine on a given string(vector) of symbols
 *
 * TODO: emission of symbols (for transducers)
 */
inline
fsm_run_flags
fsm_run(
		finite_state_machine       &fsm,
		std::vector<const symbol*> &input,
		fsm_run_log                &log)
{
	// make sure that the run log is never in an invalid state
	reset_run_log(log);

	// sanity check for the current state of the fsm
	if (!fsm.initialized)
		return fsm_run_flags::ERROR_NOT_INITIALIZED;
	if (fsm.current_state == nullptr)
		return fsm_run_flags::ERROR_CURRENT_STATE_NOT_SET;
	if (!is_start(fsm.current_state))
		return fsm_run_flags::ERROR_NOT_IN_STARTING_STATE;

	// walk over all symbols in the message, and walk trough the FSM. this makes
	// use of the transition table that was created by calling init on the FSM.
	fsm_run_flags result = fsm_run_flags::OK;

	// quickly copy over the entire string (the loop below might exit before the
	// entire string was copied)
	copy_str(input, log.input);

	for (size_t i = 0; i < input.size(); ++i) {
		const symbol *sym = input[i];
		if (sym == nullptr) {
			result |= fsm_run_flags::ERROR_INVALID_WORD;
			break;
		}

		// transition might be nullptr, if it does not exist
		auto t = fsm.transition_table[fsm.current_state->id][sym->id];
		if (t) {
			log.accepted.push_back(sym);
			log.trace.push_back(t);
			log_verbose("Read ", fsm.alphabet->symbols[t->read].glyph, " advancing to state ", t->to->id, "\n");

			// also store the output. This is mostly relevant if the FSM is a
			// transducer that produces different things than it reads
			const symbol *output_symbol = &fsm.alphabet->symbols[t->write];
			log.output.push_back(output_symbol);

			// advance the FSM's state
			fsm.current_state = t->to;
		}
		else {
			// There's no transition available for a given state and symbol
			result |= fsm_run_flags::ERROR_NO_VIABLE_TRANSITION;
			break;
		}
	}

	// everything read, but not in final state -> not valid acceptance!
	if (!is_final(fsm.current_state))
		result |= fsm_run_flags::ERROR_NOT_IN_FINAL_STATE;

	// finalize everything in the log
	log.input_string    = symbols_to_str(input);
	log.input_length    = input.size();
	log.accepted_string = symbols_to_str(log.accepted);
	log.accepted_length = log.accepted.size();
	log.output_string   = symbols_to_str(log.output);
	log.output_length   = log.output.size();

	// verbose debug info
	log_verbose("Input string:                     ", symbols_to_str(input), "\n");
	log_verbose("Total number of symbols in input: ", input.size(), "\n");
	log_verbose("Accepted string:                  ", symbols_to_str(log.accepted), "\n");
	log_verbose("Number of symbols accepted:       ", log.accepted.size(), "\n");

	return result;
}


inline
std::tuple<state_ptr_vector, transition_ptr_vector>
fsm_minimize(const finite_state_machine &fsm)
{
	// forward to DFA's minimize
	return dfa_minimize(fsm.alphabet,
			            fsm.states,
						fsm.transitions);
}


} // ncr::
