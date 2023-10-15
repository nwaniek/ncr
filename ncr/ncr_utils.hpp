/*
 * ncr_utils - Utility functions, macros, structs specifications
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 * This file contains utility functions and macro definitions that are used
 * throghout NCR, and which are handy for other projects.
 *
 * Due to the nature of this file, this file does *not* include any other NCR
 * headers and can be used standalone.
 */
#pragma once

#ifndef NCR_UTILS
#define NCR_UTILS
#endif

#include <string>
#include <cstddef>
#include <optional>
#include <sstream>
#include <iomanip>
#include <utility>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace ncr {


/*
 * NCR_DEFINE_FUNCTION_ALIAS - define a function alias for another function
 *
 * Using perfect forwarding, this creates a function with a novel name that
 * forwards all the arguments to the original function
 *
 * Note: In case there are multiple overloaded functions, this macro can be used
 *       _after_ the last overloaded function itself.
 */
#define NCR_DEFINE_FUNCTION_ALIAS(ALIAS_NAME, ORIGINAL_NAME)           \
	template <typename... Args>                                        \
	inline auto ALIAS_NAME(Args &&... args)                            \
		noexcept(noexcept(ORIGINAL_NAME(std::forward<Args>(args)...))) \
		-> decltype(ORIGINAL_NAME(std::forward<Args>(args)...))        \
	{                                                                  \
		return ORIGINAL_NAME(std::forward<Args>(args)...);             \
	}


/*
 * NCR_DEFINE_FUNCTION_ALIAS_EXT - similar as above, but with additional
 * template arguments that are not captured in the case above.
 *
 * For instance, if one implements a function that gets specialized on its
 * return type, then the this could be used.
 *
 * Example:
 *
 *     // some template which has a template arguemnt for the return type
 *     template <typename T, typename U> T ncr_some_fun(int x);
 *
 *     // specialization
 *     template <typename U>
 *     float ncr_some_fun(int x)
 *     {
 *     	return (float)x;
 *     }
 *
 *     NCR_DEFINE_SHORT_NAME_EXT(some_fun, ncr_some_fun)
 */
#define NCR_DEFINE_FUNCTION_ALIAS_EXT(ALIAS_NAME, ORIGINAL_NAME)                 \
	template <typename... Args2, typename... Args>                               \
	inline auto ALIAS_NAME(Args &&... args)                                      \
		noexcept(noexcept(ORIGINAL_NAME<Args2...>(std::forward<Args>(args)...))) \
		-> decltype(ORIGINAL_NAME<Args2...>(std::forward<Args>(args)...))        \
	{                                                                            \
		return ORIGINAL_NAME<Args2...>(std::forward<Args>(args)...);             \
	}

#define NCR_DEFINE_TYPE_ALIAS(ALIAS_NAME, ORIGINAL_NAME) \
	using ALIAS_NAME = ORIGINAL_NAME


/*
 * NCR_DEFINE_SHORT_NAME - define a short name for a longer one
 *
 * This allows to easily define short function names, e.g. without the ncr_
 * prefix, for a given function. Not the the alias definition will only take
 * place if NCR_ENABLE_SHORT_NAMES is defined.
 */
#ifdef NCR_ENABLE_SHORT_NAMES
	#define NCR_DEFINE_SHORT_FN_NAME(SHORT_NAME, LONG_NAME) \
		NCR_DEFINE_FUNCTION_ALIAS(SHORT_NAME, LONG_NAME)

	#define NCR_DEFINE_SHORT_FN_NAME_EXT(SHORT_FN_NAME, LONG_FN_NAME) \
		NCR_DEFINE_FUNCTION_ALIAS_EXT(SHORT_FN_NAME, LONG_FN_NAME)

	#define NCR_DEFINE_SHORT_TYPE_ALIAS(SHORT_NAME, LONG_NAME) \
		NCR_DEFINE_TYPE_ALIAS(SHORT_NAME, LONG_NAME)
#else
	#define NCR_DEFINE_SHORT_FN_NAME(_0, _1)
	#define NCR_DEFINE_SHORT_FN_NAME_EXT(_0, _1)
	#define NCR_DEFINE_SHORT_TYPE_ALIAS(_0, _1)
#endif


/*
 * compile time count of elements in an array. If standard library is used,
 * could also use std::size instead.
 */
template <std::size_t N, class T>
constexpr std::size_t len(T(&)[N]) { return N; }


/*
 * Count the number of arguments to a variadic macro. Up to 64 arguments are
 * supported
 */
#define NCR_COUNT_ARGS2(X,_64,_63,_62,_61,_60,_59,_58,_57,_56,_55,_54,_53,_52,_51,_50,_49,_48,_47,_46,_45,_44,_43,_42,_41,_40,_39,_38,_37,_36,_35,_34,_33,_32,_31,_30,_29,_28,_27,_26,_25,_24,_23,_22,_21,_20,_19,_18,_17,_16,_15,_14,_13,_12,_11,_10,_9,_8,_7,_6,_5,_4,_3,_2,_1,N,...) N
#define NCR_COUNT_ARGS(...) NCR_COUNT_ARGS2(0, __VA_ARGS__ ,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)

/*
 * Suppress warnings for unused arguments. Up to 10 arguments are supported in
 * the variadic version NCR_UNUSED
 */
#define NCR_UNUSED_1(X)        (void)X;
#define NCR_UNUSED_2(X0, X1)   NCR_UNUSED_1(X0); NCR_UNUSED_1(X1)
#define NCR_UNUSED_3(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_2(__VA_ARGS__)
#define NCR_UNUSED_4(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_3(__VA_ARGS__)
#define NCR_UNUSED_5(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_4(__VA_ARGS__)
#define NCR_UNUSED_6(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_5(__VA_ARGS__)
#define NCR_UNUSED_7(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_6(__VA_ARGS__)
#define NCR_UNUSED_8(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_7(__VA_ARGS__)
#define NCR_UNUSED_9(X0, ...)  NCR_UNUSED_1(X0); NCR_UNUSED_8(__VA_ARGS__)
#define NCR_UNUSED_10(X0, ...) NCR_UNUSED_1(X0); NCR_UNUSED_9(__VA_ARGS__)

#define NCR_UNUSED_INDIRECT3(N, ...)  NCR_UNUSED_ ## N(__VA_ARGS__)
#define NCR_UNUSED_INDIRECT2(N, ...)  NCR_UNUSED_INDIRECT3(N, __VA_ARGS__)
#define NCR_UNUSED(...)               NCR_UNUSED_INDIRECT2(NCR_COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)


/*
 * The following two tables define converters from string to other types. The
 * first table contains the types for which a 'standard implementation' for the
 * template specialization can be generated, whereas the second table contains
 * those for which manual implementations of the conversion function is defined
 * below.
 *
 * The lists are also used to generate functions with C-names, in case a user
 * does or can not use the templated version, or in case a C interface needs to
 * be used. Note that the C-named functions still return an std::optional, so
 * they cannot directly be externed to C.
 */
#define STOX_STANDARD_TYPES(_) \
	_(stoi, int)           \
	_(stou, unsigned)      \
	_(stof, float)         \
	_(stod, double)

#define STOX_SPECIAL_TYPES(_)  \
	_(stob, bool)          \
	_(stoc, char)          \
	_(stos, std::string)


/*
 * str_to_type<T> - signature for the templated conversion function
 */
template <typename T>
std::optional<T>
str_to_type(std::string);


/*
 * str_to_type<bool> - convert from string to bool.
 *
 * This function allows to pass in '0', '1', 'false', 'true' to get
 * corresponding results in boolean type.
 */
template <>
inline std::optional<bool>
str_to_type(std::string s)
{
	bool result;

	std::istringstream is(s);
	is >> result;

	if (is.fail()) {
		is.clear();
		is >> std::boolalpha >> result;
	}
	return is.fail() ? std::nullopt : std::optional<bool>{result};
}


/*
 * str_to_type<char> - convert a string to char
 *
 * More precisely, this function extracts the first character from the string
 * (if available) and \0 otherwise.
 */
template<>
inline std::optional<char>
str_to_type(std::string s)
{
	if (!s.length())
		return '\0';
	return s[0];
}


/*
 * str_to_type<std::string> - return the argument
 *
 * This function simply returns the argument, as a conversion from string to
 * string is its identity.
 */
template <>
inline std::optional<std::string>
str_to_type(std::string s)
{
	return s;
}


/*
 * automatically generate code for standard type implementations, because they
 * are all the same. This could also be achieved by having the implementation in
 * the base template case, but then the template would also be defined for other
 * types, which might not be desirable. Hence, rather let the compiler throw an
 * error in case someone tries to call str_to_type on a type which is not
 * known.
 */
#define X_TEMPLATE_IMPL(_1, T)                                      \
	template <>                                                     \
	inline std::optional<T>                                         \
	str_to_type(std::string s)                                  \
	{                                                               \
		T result;                                                   \
		std::istringstream is(s);                                   \
		is >> result;                                               \
		return is.fail() ? std::nullopt : std::optional<T>{result}; \
	}

STOX_STANDARD_TYPES(X_TEMPLATE_IMPL)
#undef X_TEMPLATE_IMPL


// generate alias functions with c-like function names for all types
#define X_ALIAS_IMPL(FN_NAME, T)      \
	inline std::optional<T>           \
	FN_NAME(std::string s) {          \
		return str_to_type<T>(s); \
	}

STOX_STANDARD_TYPES(X_ALIAS_IMPL)
STOX_SPECIAL_TYPES(X_ALIAS_IMPL)
#undef X_ALIAS_IMPL

#undef STOX_SPECIAL_TYPES
#undef STOX_STANDARD_TYPES



#define XTOS_STANDARD_TYPES(_) \
	_(itos, int)           \
	_(utos, unsigned)      \
	_(ftos, float)         \
	_(dtos, double)        \
	_(ctos, char)          \

#define XTOS_SPECIAL_TYPES(_)  \
	_(btos, bool)          \
	_(stos, std::string)


// TODO: r-value?
template <typename T>
inline std::optional<std::string>
type_to_str(T &value);


template <>
inline std::optional<std::string>
type_to_str(bool &value)
{
	std::ostringstream os;
	os << std::boolalpha << value;
	return os.fail() ? std::nullopt : std::optional{os.str()};
}


template <>
inline std::optional<std::string>
type_to_str(std::string &value)
{
	return value;
}



// TODO: find nicer workaround for this and auto-generate it
template <typename T>
inline void
_v_to_os(std::ostringstream &os, std::vector<T> vs)
{
	size_t i = 0;
	for (auto &v : vs) {
		if (i > 0)
			os << " ";
		os << v;
		i++;
	}
}

template <>
inline std::optional<std::string>
type_to_str(std::vector<int> &vs)
{
	std::ostringstream os;
	_v_to_os<int>(os, vs);
	return os.fail() ? std::nullopt : std::optional{os.str()};
}

template <>
inline std::optional<std::string>
type_to_str(std::vector<unsigned> &vs)
{
	std::ostringstream os;
	_v_to_os<unsigned>(os, vs);
	return os.fail() ? std::nullopt : std::optional{os.str()};
}

template <>
inline std::optional<std::string>
type_to_str(std::vector<float> &vs)
{
	std::ostringstream os;
	_v_to_os<float>(os, vs);
	return os.fail() ? std::nullopt : std::optional{os.str()};
}


#define X_TEMPLATE_IMPL(_1, T)                                     \
	template <>                                                    \
	inline std::optional<std::string>                              \
	type_to_str(T &value)                                      \
	{                                                              \
		std::ostringstream os;                                     \
		os << value;                                               \
		return os.fail() ? std::nullopt : std::optional{os.str()}; \
	}

XTOS_STANDARD_TYPES(X_TEMPLATE_IMPL)
#undef X_TEMPLATE_IMPL

// generate alias functions with c-like function names
#define X_ALIAS_IMPL(FN_NAME, T)       \
	inline std::optional<std::string>  \
	FN_NAME(T &value) {                \
		return type_to_str(value); \
	}

XTOS_STANDARD_TYPES(X_ALIAS_IMPL)
XTOS_SPECIAL_TYPES(X_ALIAS_IMPL)
#undef X_ALIAS_IMPL

#undef XTOS_SPECIAL_TYPES
#undef XTOS_STANDARD_TYPES


/*
 * ltrim - trim a string on the left, removing leading whitespace characters
 */
inline std::string&
ltrim(std::string &s, const char *ws = " \n\t\r")
{
	s.erase(0, s.find_first_not_of(ws));
	return s;
}

/*
 * rtrim - trim a string on the right, removing trailing whitespace
 */
inline std::string&
rtrim(std::string &s, const char *ws = " \n\t\r")
{
	s.erase(s.find_last_not_of(ws) + 1);
	return s;
}


/*
 * trim - remove leading and trailing whitespace
 */
inline std::string&
trim(std::string &s, const char *ws = " \n\t\r")
{
	return ltrim(rtrim(s, ws), ws);
}


/*
 * A simple define to reduce the verbosity to declare a tuple. This is
 * particularly useful, for instance, in calls to random.hpp:random_coord.
 * In this example, the template accepts a variadic number of tuples, e.g.
 *
 *     auto xlim = std::tuple{0, 1};
 *     auto ylim = std::tuple{0, 10};
 *     auto coord = random_coord(rng, xlim, ylim);
 *
 * It would be better to avoid the temporary variables. However,
 * brace-initializers wont work, as there is no clear (read: acceptably sane)
 * way to turn an initializer_list into a tuple. With the following macro, it is
 * actually possible to succinctly write
 *
 *     auto coord = random_coord(_T(0, 1), _T(0, 10));
 *
 * without any local declaration of temporaries, or overly long calls that
 * include the specific tuple type.
 */
#ifndef _tup
	#define _tup(...) std::tuple{__VA_ARGS__}
#endif


/*
 * flag_is_set - test if a flag is set in an unsigned value.
 *
 * This function evaluates if a certain flag, i.e. bit pattern, is present in v.
 */
template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline bool
flag_is_set(const T v, const U flag)
{
	return (v & flag) == flag;
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
set_flag(const T v, const U flag)
{
	return v | flag;
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
clear_flag(const T v, const U flag)
{
	return v & ~flag;
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
toggle_flag(const T v, const U flag)
{
	return v ^ flag;
}


/*
 * bitmask - create bitmask of given length at offset
 */
template <typename U>
requires std::unsigned_integral<U> || std::unsigned_integral<typename std::underlying_type<U>::type>
constexpr U
bitmask(U offset, U length)
{
	// casting -1 to unsigned produces a value with 1s everywhere (i.e.
	// 0xFFF...F)
	return ~(U(-1) << length) << offset;
}


template <typename U>
requires std::unsigned_integral<U> || std::unsigned_integral<typename std::underlying_type<U>::type>
inline U
set_bits(U dest, U offset, U length, U bits)
{
	U mask = bitmask<U>(offset, length);
	return (dest & ~mask) | ((bits << offset) & mask);
}


template <typename U>
requires std::unsigned_integral<U> || std::unsigned_integral<typename std::underlying_type<U>::type>
inline U
get_bits(U src, U offset, U length)
{
	return (src & bitmask<U>(offset, length)) >> offset;
}


template <typename U>
requires std::unsigned_integral<U> || std::unsigned_integral<typename std::underlying_type<U>::type>
inline U
toggle_bits(U src, U offset, U length)
{
	U mask = bitmask<U>(offset, length);
	return src ^ mask;
}


/*
 * bit_is_set - test if the Nth bit is set in a variable, where N starts at 0
 *
 * This function evaluates if the Nth bit is present in variable v.
 */
template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline bool
bit_is_set(const T v, const U N)
{
	return (v & 1 << N) > 0;
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
set_bit(const T v, const U N)
{
	return v | (1 << N);
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
clear_bit(const T v, const U N)
{
	return v & ~(1 << N);
}


template <typename T, typename U>
requires (std::unsigned_integral<T> && std::unsigned_integral<U>) ||
		 (std::unsigned_integral<typename std::underlying_type<T>::type> && std::unsigned_integral<typename std::underlying_type<U>::type>)
inline T
toggle_bit(const T v, const U N)
{
	return v ^ (1 << N);
}


/*
 * to_underlying - Get the underlying type of some type
 *
 * This is an implementation of C++23's to_underlying function, which is not yet
 * available in C++20 but handy for casting enum-structs to their underlying
 * type (see NCR_DEFINE_ENUM_FLAG_OPERATORS for an example).
 */
template <typename E>
constexpr typename std::underlying_type<E>::type
to_underlying(E e) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(e);
}


/*
 * NCR_DEFINE_ENUM_FLAG_OPERATORS - define all binary operators used for flags
 *
 * This macro expands into functions for bit-wise and binary operations on
 * enums, e.g. given two enum values a and b, one might want to write `a |= b;`.
 * With the macro below, this will be possible.
 */
#define NCR_DEFINE_ENUM_FLAG_OPERATORS(ENUM_T) \
	inline ENUM_T operator~(ENUM_T a) { return static_cast<ENUM_T>(~ncr::to_underlying(a)); } \
	inline ENUM_T operator|(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) | ncr::to_underlying(b)); } \
	inline ENUM_T operator&(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) & ncr::to_underlying(b)); } \
	inline ENUM_T operator^(ENUM_T a, ENUM_T b)    { return static_cast<ENUM_T>(ncr::to_underlying(a) ^ ncr::to_underlying(b)); } \
	inline ENUM_T& operator|=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) | ncr::to_underlying(b)); } \
	inline ENUM_T& operator&=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) & ncr::to_underlying(b)); } \
	inline ENUM_T& operator^=(ENUM_T &a, ENUM_T b) { return a = static_cast<ENUM_T>(ncr::to_underlying(a) ^ ncr::to_underlying(b)); }


/*
 * get_index_of - get the index of a pointer to T in a vector of T
 *
 * This function returns an optional to indicate if the pointer to T was found
 * or not.
 */
template <typename T>
inline std::optional<size_t>
get_index_of(std::vector<T*> vec, T *needle)
{
	for (size_t i = 0; i < vec.size(); i++)
		if (vec[i] == needle)
			return i;
	return {};
}


/*
 * determine if a container contains a certain element or not
 */
template <typename ContainerT, typename U>
inline bool
contains(const ContainerT &container, const U &needle)
{
	auto it = std::find(container.begin(), container.end(), needle);
	return it != container.end();
}


/*
 * memory_guard - A simple memory scope guard
 *
 * This template allows to pass in several pointers which will be deleted when
 * the guard goes out of scope. This is the same as using a unique_ptr into
 * which a pointer is moved, but with slightly less verbose syntax. This is
 * particularly useful to scope members of structs / classes.
 *
 * Example:
 *
 *     struct MyStruct {
 *         SomeType *var;
 *
 *         void some_function
 *         {
 *             // for some reason, var should live only as long as some_function
 *             // is running. This can be useful in the case of recursive
 *             // functions to which sending all the context or state variables
 *             // is inconvenient, and save them in the surrounding struct. An
 *             // alternative, maybe even a preferred way, is to use PODs that
 *             // contain state and use free functions. Still, the memory guard
 *             // might be handy
 *             var = new SomeType();
 *             memory_guard<SomeType> guard(var);
 *
 *             ...
 *
 *             // the guard will call delete on `var' once it drops out of scope
 *         }
 *     };
 *
 * Example with unique_ptr:
 *
 *            // .. struct is same as above
 *            var = new SomeType();
 *            std::unique_ptr<SomeType>(std::move(*var));
 *
 * Yes, this only saves a few characters to type. However, memory_guard works
 * with an arbitrary number of arguments :)
 */
template <typename... Ts>
struct memory_guard;

template <>
struct memory_guard<> {};

template <typename T, typename... Ts>
struct memory_guard<T, Ts...> : memory_guard<Ts...>
{
	T *ptr = nullptr;
	memory_guard(T *_ptr, Ts *...ptrs) : memory_guard<Ts...>(ptrs...), ptr(_ptr) {}
	~memory_guard() { if (ptr) delete ptr; }
};


/*
 * hexdump - generate a hexdump for a buffer similar to hex editors
 */
void
hexdump(std::ostream& os, const std::vector<uint8_t> &data)
{
	const size_t bytes_per_line = 16;
	for (size_t offset = 0; offset < data.size(); offset += bytes_per_line) {
		os << std::setw(8) << std::setfill('0') << std::hex << offset << ": ";
		for (size_t i = 0; i < bytes_per_line; ++i) {
			// missing bytes will be replaced with whitespace
			if (offset + i < data.size())
				os << std::setw(2) << std::setfill('0') << std::hex << static_cast<i32>(data[offset + i]) << ' ';
			else
				os << "   ";
		}
		os << " | ";
		for (size_t i = 0; i < bytes_per_line; ++i) {
			if (offset + i >= data.size())
				break;

			// non-printable characters will be replaced with '.'
			char c = data[offset + i];
			if (c < 32 || c > 126)
				c = '.';
			os << c;
		}
		os << "\n";
	}
}



} // ncr::
