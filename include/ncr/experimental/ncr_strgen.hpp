#pragma once

// TODO: this file should probably be called ncr_alphabet, and incorporate all
// basic_string_t stuff from ncr_automata.hpp
//
// TODO: when doing so, basic_string_t should be renamed to
// ncr::alphabet::basic_string (without the _t)


#include <vector>
#include <algorithm>

// meh, includes the entire automata file. maybe there's a better way to solve
// this (only needs basic_string_t at the moment
#include <ncr/ncr_log.hpp>
#include <ncr/ncr_automata.hpp>
#include <ncr/ncr_random.hpp>


namespace ncr {
namespace alphabet {
namespace generators {


/*
 * The interface of a string generator must have the following functions:
 *
 *	.operator()
 *  .reset()
 *
 * To make it as fast as possible and without using virtual functions, the
 * following uses a CRTP (Curously-Recurring Template Pattern) for look-alike
 * polymorphism.
 *
 * NOTE: CRTP is not the solution here. Virtual function calls do not add an
 * excessive amount of time to the execution. That is, they are not the
 * bottleneck. Moreover, with CRTP, we cannot keep a pointer to the base class
 * without knowing the implementation details
 */

/*
template <typename Detail>
struct Generator {
	ncr::basic_string_t
	operator() () {
		return static_cast<Detail*>(this)->operator()();
	}

	void
	reset() {
		static_cast<Detail*>(this)->reset();
	}
};
*/


struct IGenerator
{
	virtual ~IGenerator() {}
	virtual ncr::basic_string_t operator()() = 0;
	virtual void reset() = 0;
};


/*
 * struct Random - generator for random strings
 *
 * This generator invokes the random number generator and creates a random
 * string. The generator's constructor arguments specify the alphabet, as well
 * as the length of the string.
 */
struct Random : IGenerator {

	Random(
		std::mt19937_64 *rng,
		const ncr::basic_alphabet &alph,
		const size_t length)
	:
		_rng(rng),
		_alphabet(alph),
		_length(length)
	{ }

	virtual ~Random() {}

	virtual ncr::basic_string_t
	operator() () {
		return ncr::random_string(this->_rng, &this->_alphabet, this->_length);
	}

	virtual void reset() {}

private:
	std::mt19937_64 *_rng;
	const ncr::basic_alphabet &_alphabet;
	const size_t _length;
};



/*
 * struct UniqueRandom - Generate random (but unique) strings
 *
 * This (stateful) struct generates random strings without the chance of
 * introducing duplicates.
 */
struct UniqueRandom : IGenerator
{
	UniqueRandom(
		std::mt19937_64 *rng,
		const ncr::basic_alphabet &alph,
		const size_t length)
	:
		_rng(rng),
		_alphabet(alph),
		_length(length),
		_n_max(std::pow(alph.n_symbols, length))
	{
		reset();
	}

	virtual ~UniqueRandom() {}

	virtual ncr::basic_string_t
	operator() ()
	{
		// TODO: make this behavior optional (via flag for constructor)
		if (_indexes.size() == 0) {
			log_warning("ncr::alphabet::generators::UniqueRandom exhausted, replenishing via automatic reset.\n");
			reset();
		}

		// select a random entry of the list of indexes
		size_t indx = choice<size_t>(0, this->_indexes.size() - 1, this->_rng);

		// swap the chosen with the last, pop the end (this is a fast method to
		// pop a random element from a vector)
		std::swap(_indexes[indx], _indexes.back());
		indx = _indexes.back();
		_indexes.pop_back();

		// select the n-th string selected above
		return ncr::nth_string(&this->_alphabet, this->_length, indx);
	}

	virtual void reset() {
		// resize and fill the vector
		_indexes.resize(_n_max);
		std::iota(std::begin(_indexes), std::end(_indexes), 0);
	}

	private:
		std::mt19937_64 *_rng;
		const ncr::basic_alphabet &_alphabet;
		const size_t _length;
		const size_t _n_max;
		std::vector<size_t> _indexes;
};


/*
 * struct Successive - generate successive strings of an alphabet
 *
 * With each call of operator(), this yields the next (lexicographic) string of
 * the alphabet, initially starting at the 0-th strings. Note that, once the
 * number of possible strings is exhausted, the next successor will be the 0th
 * string. A call to .reset() will reset the (internal) string counter.
 */
struct Successive : IGenerator {
	Successive(
		const ncr::basic_alphabet &alph,
		const size_t length)
	:
		_n(0),
		_n_max(std::pow(alph.n_symbols, length)),
		_alphabet(alph),
		_length(length)
	{
	}

	virtual ~Successive() { }

	virtual ncr::basic_string_t
	operator() () {
		// get the index for the next item, then increment the counter (modulo
		// the max size)
		size_t n = this->_n;
		this->_n = (this->_n + 1) % this->_n_max;

		// upon detection of an overflow, emit a warning to the log
		// XXX: make this more informative, maybe? or use a callback flag? or
		//      throw an exception? make configurable?
		if (this->_n == 0)
			ncr::log_warning("ncr::alphabet::generators::Successive wrap around detected\n");

		return ncr::nth_string(&this->_alphabet, this->_length, n);
	}

	virtual void reset() {
		this->_n = 0;
	}

private:
	size_t _n;
	const size_t _n_max;
	const ncr::basic_alphabet &_alphabet;
	const size_t _length;
};



}}} // ncr::alphabet::generators
