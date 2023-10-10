/*
 * ncr_variant - Utility functions to work with variant types
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 * This file includes helper functions for variant data types in addition to
 * the standard's <variant> header. In particular, it includes a `visit' (and a
 * `visit_unsafe') function which can be slightly faster than std::visit.
 */

// TODO: documentation

#include <cstddef>
#include <variant>
#include <vector>

namespace ncr {

// helper type for visitors
template <typename... Ts>
struct overloaded: Ts... {
	using Ts::operator()...;
};


// explicit deduction guide (not needed in C++20)
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;


template <size_t N, typename Variant, typename Visitor>
void _visit(Variant &&arg, Visitor &&visitor)
{
	if constexpr (N == 0) {
		if (N == arg.index())
			std::forward<Visitor>(visitor)(std::get<0>(std::forward<Variant>(arg)));
	}
	else {
		if (arg.index() == N)
			std::forward<Visitor>(visitor)(std::get<N>(std::forward<Variant>(arg)));
		_visit<N - 1>(arg, std::move(visitor));
	}
}


template <size_t N, typename Variant, typename Visitor>
void _visit_unsafe(Variant &&arg, Visitor &&visitor)
{
	if constexpr (N == 0) {
		if (N == arg.index())
			std::forward<Visitor>(visitor)(*std::get_if<0>(&std::forward<Variant>(arg)));
	}
	else {
		if (arg.index() == N)
			std::forward<Visitor>(visitor)(*std::get_if<N>(&std::forward<Variant>(arg)));
		_visit_unsafe<N - 1>(arg, std::move(visitor));
	}
}


template <typename ...Ts, typename ...Visitors>
void visit(std::variant<Ts...> const &var, Visitors&&... vs)
{
	constexpr auto N = sizeof...(Ts);
	const auto ol = overloaded<Visitors...>{std::forward<Visitors>(vs)...};
	_visit<N - 1>(var, std::move(ol));
}


template <typename ...Ts, typename ...Visitors>
void visit_unsafe(std::variant<Ts...> const &var, Visitors&&... vs)
{
	constexpr auto N = sizeof...(Ts);
	const auto ol = overloaded<Visitors...>{std::forward<Visitors>(vs)...};
	_visit_unsafe<N - 1>(var, std::move(ol));
}


template <std::forward_iterator I, std::sentinel_for<I> S, typename ...Visitors>
void visit(I first, S last, Visitors&&... vs)
{
	while (first != last) {
		visit(std::forward<decltype(*first)>(*first), std::forward<Visitors>(vs)...);
		++first;
	}
}


template <std::forward_iterator I, std::sentinel_for<I> S, typename ...Visitors>
void visit_unsafe(I first, S last, Visitors&&... vs)
{
	while (first != last) {
		visit_unsafe(std::forward<decltype(*first)>(*first), std::forward<Visitors>(vs)...);
		++first;
	}
}


template <typename ...Ts, typename ...Visitors>
void visit(std::vector<std::variant<Ts...>> const &vec, Visitors&&... vs)
{
	visit(vec.begin(), vec.end(), std::forward<Visitors>(vs)...);
}


template <typename ...Ts, typename ...Visitors>
void visit_unsafe(std::vector<std::variant<Ts...>> const &vec, Visitors&&... vs)
{
	visit_unsafe(vec.begin(), vec.end(), std::forward<Visitors>(vs)...);
}


} // ncr::
