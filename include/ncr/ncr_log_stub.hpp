/*
 * ncr_log_stub - stub for the minimalistic logging interface
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 *
 * define lightweight logging functions in case log.hpp was not imported or not
 * desired. The same approach allows a user to overwrite the logging functions by
 * providing their own, i.e. simply define the four ncr_log_* functions and
 * don't import log.hpp
 */

#pragma once

#ifndef NCR_LOG_STUB
#define NCR_LOG_STUB
#endif

#include <utility>
#include <iostream>

namespace ncr {

#if !defined(log_error) or \
	!defined(log_warning) or \
	!defined(log_debug) or \
	!defined(log_verbose)
	#pragma GCC warning "log.hpp not included or not all ncr_log_* functions defined. Will use fallback methods."

inline void __log_fallback() {}

template <typename T, typename ...Ts>
inline void __log_fallback(T&& t, Ts&&... ts)
{
	std::cout << std::forward<T>(t);
	__log_fallback(std::forward<Ts>(ts)...);
}

#ifndef log_error
	#define log_error(...)   __log_fallback(__VA_ARGS__)
#endif
#ifndef log_warning
	#define log_warning(...) __log_fallback(__VA_ARGS__)
#endif
#ifndef log_debug
	#define log_debug(...)   __log_fallback(__VA_ARGS__)
#endif
#ifndef log_verbose
	#define log_verbose(...) __log_fallback(__VA_ARGS__)
#endif

#endif

} // ncr::
