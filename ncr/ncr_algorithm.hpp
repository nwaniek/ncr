/*
 * ncr_algorithm - Implementations of various more or less useful algorithms
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 */

/*
 * TODO: change from iterator template arguments below to std::forward_iterator
 * and std::sentinel_for
 */
#pragma once

#include <cmath>
#include <vector>
#include <functional>
#include <random>
#include <iterator>
#include <string>
#include <map>

#include <ncr/ncr_utils.hpp>

namespace ncr {


/*
 * SamplerTraits - template to capture the traits of a sampler
 *
 * Usually, the sampler below works with double precision numbers and a Mersenne
 * Twister with 64bits. In case this behaviour needs to change, specific
 * tempalte parameters can be used for the SamplerTraits.
 */
template <typename P = double, typename RngT = std::mt19937_64>
struct SamplerTraits {
	using precision_type = P;
	using rng_type       = RngT;
};



/*
 * weighted_sampler - sample indexes of a weighted vector
 *
 * args:
 *     weights    - vector of weights from which to draw indexes
 *     n_items    - number of indexes to draw
 *     rng        - pointer to a random number generator
 *     value      - function to get the value of an element. In case the
 *                  container consists of plain elements, such as double or
 *                  float, not function needs to be passed in and the elements
 *                  are used as values. In case the container consists of
 *                  elements that are some structs or other objects, this
 *                  function must produce the elements' "value", such as a
 *                  weight or fitness.
 *
 * template type arguments:
 *     ContainerT - type of the container that holds the weights. must be some
 *                  iterable that has a ::value_type type definition.
 *     Traits     - Some type that provides precision_type and rng_type. See
 *                  SamplerTraits for an example
 */

template <typename ContainerT, typename Traits = SamplerTraits<double, std::mt19937_64>>
std::vector<size_t>
weighted_sampler(
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



/*
 * weighted_sampler_std - sample indexes of a weighted vector using std::discrete_distribution
 *
 * args:
 *     weights    - vector of weights from which to draw indexes
 *     n_items    - number of indexes to draw
 *     rng        - pointer to a random number generator
 *
 * template type arguments:
 *     ContainerT - type of the container that holds the weights. must be some iterable
 *     RngT       - random number generator type
 *
 * TODO: if possible, adapt the lambda strategy from above
 */
template <typename ContainerT, typename RngT = std::mt19937_64>
std::vector<size_t>
weighted_sampler_std(const ContainerT &weights, size_t n_items, RngT *rng)
{
	std::vector<size_t> indices(n_items);
	std::discrete_distribution<ptrdiff_t> dist(std::begin(weights), std::end(weights));

	for (size_t i = 0; i < n_items; i++)
		indices[i] = dist(*rng);

	return indices;
}


/*
 * low_variance_sampler - sample items from a container with low variance
 *
 * template type arguments:
 *	T    - type of the container
 *	U    - type of the element inside a container
 *  P    - precision type, e.g. float, double, etc.
 *  RngT - random number generator type
 *
 * TODO: extract U from the container (e.g. via ''contained_type'' or similar)
 */
template <typename T, typename U, typename P, typename RngT = std::mt19937_64>
requires std::floating_point<P>
std::vector<size_t>
low_variance_sampler(
		T &container,
		size_t n_items,
		RngT *rng,
		const std::function<P (const U *elem)> &weight_fn)
{
	// uniform random distribution on [0, 1]
	std::uniform_real_distribution<P> unif(static_cast<P>(0.0), static_cast<P>(1.0));

	// number of elements in the container
	std::vector<size_t> indices(n_items);

	// select the agent with maximal fitness
	auto max_item = std::max_element(container.begin(), container.end(),
			[weight_fn](const U *left, const U *right) {
				return weight_fn(left) < weight_fn(right);
			});
	auto max_weight = weight_fn(*max_item);

	// weighting
	auto beta = 0.0;
	// random start index for sampling
	size_t index = unif_random(rng);
	for (size_t i = 0; i < n_items; i++) {
		beta += unif(*rng) * 2.0 * max_weight;
		while (beta > weight_fn(container[index])) {
			beta -= weight_fn(container[index]);
			index = (index + 1) % n_items;
		}
		indices[i] = index;
	}

	return indices;
}


/*
 * min - compute the minimum value from several arguments
 *
 * This implementation uses C++17 features to be as short as possible. Note that
 * this function returns the leftmost argument, i.e. it follows the C++ standard
 * for std::min (see 25.4.7 in C++11).
 */
template <typename T0, typename T1, typename... Ts>
constexpr auto min(T0 &&v0, T1 &&v1, Ts &&... ts)
{
	if constexpr (sizeof...(ts) == 0) {
		return v1 < v0 ? v1 : v0;
	}
	else {
		return min(min(v0, v1), ts...);
	}
}


/*
 * min - compute the minimum value from several arguments
 *
 * This implementation uses C++17 features to be as short as possible. Note that
 * this function returns the leftmost argument, i.e. it follows the C++ standard
 * for std::min (see 25.4.7 in C++11).
 */
template <typename T0, typename T1, typename... Ts>
constexpr auto max(T0 &&v0, T1 &&v1, Ts &&... ts)
{
	if constexpr (sizeof...(ts) == 0) {
		return v0 > v1 ? v0 : v1;
	}
	else {
		return max(max(v0, v1), ts...);
	}
}


/*
 * levensthein_naive - Compute Levensthein distance between two strings.
 *
 * The strings can be anything as long as proper iterators and a comparison
 * function `cmp_equal` between elements are given. `cmp_equal` must return true
 * if the two elements it gets passed are identical, and false if not.
 *
 * Note: this is a slow, naive, and recursive implementation
 */
template <typename InputIt, class Compare>
size_t
levensthein_naive(
		InputIt a_first, InputIt a_last,
		InputIt b_first, InputIt b_last,
		Compare cmp_equal)
{
	// result = |a| if |b| == 0
	if (b_first == b_last)
		return std::distance(a_first, a_last);

	// result = |b| if |a| == 0
	if (a_first == a_last)
		return std::distance(b_first, b_last);

	// result = lev(tail(a), tail(b)) if a[0] == b[0]
	if (cmp_equal(*a_first, *b_first))
		return levensthein(
				std::next(a_first), a_last,
				std::next(b_first), b_last,
				cmp_equal);

	// else, minimum of the following three
	// lev(tail(a), b)
	const size_t _m0 = levensthein(
			std::next(a_first), a_last,
			b_first,            b_last,
			cmp_equal);

	const size_t _m1 = levensthein(
			a_first,            a_last,
			std::next(b_first), b_last,
			cmp_equal);

	const size_t _m2 = levensthein(
			std::next(a_first), a_last,
			std::next(b_first), b_last,
			cmp_equal);

	return 1 + min(_m0, _m1, _m2);
}


/*
 * levensthein_dynamic - Compute Levensthein distance between two iterators.
 *
 * Note: this is a Dynamic Programming implementation. It follows the pseudocode
 * presented on the Wikipedia page for the Levensthein distance for the
 * single-row variant, i.e. the version where not the full matrix is stored, but
 * only the previous row
 *
 * Note: this depends on the iterator to be copyable, i.e. using a local copy of
 * an iterator should *not* invalidate the original iterator. This is not always
 * guaranteed for all InputIterators.
 */
template <typename InputIt, class Compare>
size_t
levensthein_dynamic(
		InputIt a_first, InputIt a_last,
		InputIt b_first, InputIt b_last,
		Compare cmp_equal)
{
	std::vector<size_t> v0, v1;

	size_t M = std::distance(a_first, a_last);
	size_t N = std::distance(b_first, b_last);

	v0.resize(N + 1);
	v1.resize(N + 1);

	// initialize v0
	for (size_t i = 0; i <= N; i++) {
		v0[i] = i;
	}

	for (size_t i = 0; i <= M - 1; i++) {
		v1[0] = i + 1;

		auto b_local = b_first;

		for (size_t j = 0; j <= N - 1; j++) {
			size_t deletion  = v0[j+1] + 1;
			size_t insertion = v1[j] + 1;

			size_t substitution = 0;
			if (cmp_equal(*a_first, *b_local))
				substitution = v0[j];
			else
				substitution = v0[j] + 1;

			v1[j + 1] = min(deletion, insertion, substitution);

			// move the local iterator forward
			b_local = std::next(b_local);
		}

		// swap data
		std::swap(v0, v1);

		// move forward
		a_first = std::next(a_first);
	}

	return v0[N];
}


/*
 * levensthein - Compute the Levensthein distance between iterators
 *
 * This simply invokes the currently fastest implementation of the Levensthein
 * distance. At the time of this documentation, this is levensthein_dynamic.
 */
template <typename InputIt, class Compare>
size_t
levensthein(
		InputIt a_first, InputIt a_last,
		InputIt b_first, InputIt b_last,
		Compare cmp_equal)
{
	return levensthein_dynamic(a_first, a_last, b_first, b_last, cmp_equal);
}


/*
 * levensthein - Compute Levensthein distance between strings.
 *
 * The strings can be anything as long as operator== is defined for their
 * iterators' value_type.
 */
template <typename InputIt>
size_t
levensthein(
		InputIt a_first, InputIt a_last,
		InputIt b_first, InputIt b_last)
{
	typedef typename std::iterator_traits<InputIt>::value_type elem_type;
	auto cmp_eq = [](elem_type left, elem_type right) {
		return left == right;
	};
	return levensthein(a_first, a_last, b_first, b_last, cmp_eq);
}


/*
 * levensthein - Compute Levensthein distance between strings
 *
 * This is the specialization for C++ strings based on basic_string.
 */
template <typename T,
		 class Traits = std::char_traits<T>,
		 class Allocator = std::allocator<T>>
size_t
levensthein(
		const std::basic_string<T, Traits, Allocator> &a,
		const std::basic_string<T, Traits, Allocator> &b)
{
	return levensthein(a.begin(), a.end(), b.begin(), b.end());
}


/*
 * levensthein - Compute Levensthein distance between strings
 *
 * This is the specialization for maps
 */
template <typename Key, typename T>
size_t
levensthein(
		const std::map<Key, T> &m0,
		const std::map<Key, T> &m1)
{
	return levensthein(m0.begin(), m0.end(), m1.begin(), m1.end());
}


/*
 * levensthein - Compute Levensthein distance between strings
 *
 * This is the specialization for C++ vector types
 */
template <typename T>
size_t
levensthein(const std::vector<T> &a, const std::vector<T> &b)
{
	return levensthein(a.begin(), a.end(), b.begin(), b.end());
}


/*
 * levensthein - Compute Levensthein distance between strings
 *
 * This is the specialization for constant strings
 */
inline size_t
levensthein(const char *a, const char *b)
{
	std::string str_a(a), str_b(b);
	return levensthein(str_a.begin(), str_a.end(), str_b.begin(), str_b.end());
}


/*
 * hamming - Hamming distance between iterators, using a comparison function
 *
 * If the two strings are of equal length, then this is the standard Hamming
 * distance. If, however, the two strings are of different length, then the
 * number of excess items in the longer string is counted as additional length.
 */
template <typename InputIt, class Compare>
size_t
hamming(
		InputIt a_first, InputIt a_last,
		InputIt b_first, InputIt b_last,
		Compare cmp_equal)
{
	// get the minimum length of both to avoid walking beyond iterator bounds
	typedef typename std::iterator_traits<InputIt>::difference_type diff_t;

	diff_t len_a = std::distance(a_first, a_last);
	diff_t len_b = std::distance(b_first, b_last);
	diff_t len   = min(len_a, len_b);

	// default result is the difference of lengths
	size_t dist = std::abs(len_a - len_b);
	diff_t i = 0;
	while (i < len) {
		// compare elements, and add to length if different
		if (!cmp_equal(*a_first, *b_first))
			dist += 1;

		// increment iterators and loop variable
		++a_first, ++b_first, ++i;
	}
	return dist;
}


/*
 * hamming - Hamming distance between iterators, using a comparison function
 *
 * The iterators can be anything and the implementation will use operator== on
 * the value_type of the iterators, as long as it is defined.
 */
template <typename InputIt>
size_t
hamming(
		InputIt a_first, InputIt a_last,
		InputIt b_first, InputIt b_last)
{
	typedef typename std::iterator_traits<InputIt>::value_type elem_type;
	auto cmp_eq = [](elem_type left, elem_type right) {
		return left == right;
	};
	return hamming(a_first, a_last, b_first, b_last, cmp_eq);
}


/*
 * hamming - Hamming distance between two strings
 *
 * Specialization for strings derived from std::basic_string
 */
template <typename T,
		 class Traits = std::char_traits<T>,
		 class Allocator = std::allocator<T>>
size_t
hamming(
		const std::basic_string<T, Traits, Allocator> &a,
		const std::basic_string<T, Traits, Allocator> &b)
{
	return hamming(a.begin(), a.end(), b.begin(), b.end());
}


/*
 * hamming - Compute Hamming distance between strings
 *
 * This is the specialization for maps
 */
template <typename Key, typename T>
size_t
hamming(
		const std::map<Key, T> &m0,
		const std::map<Key, T> &m1)
{
	return hamming(m0.begin(), m0.end(), m1.begin(), m1.end());
}


/*
 * hamming - Compute Levensthein distance between strings
 *
 * This is the specialization for C++ vector types
 */
template <typename T>
size_t
hamming(const std::vector<T> &a, const std::vector<T> &b)
{
	return hamming(a.begin(), a.end(), b.begin(), b.end());
}


/*
 * hamming - Compute Levensthein distance between strings
 *
 * This is the specialization for constant strings
 */
inline size_t
hamming(const char *a, const char *b)
{
	std::string str_a(a), str_b(b);
	return hamming(str_a.begin(), str_a.end(), str_b.begin(), str_b.end());
}


} // ncr::
