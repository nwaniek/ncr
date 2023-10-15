/*
 * ncr_string - utility functions for string manipulation
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 */

#include <cstdarg>
#include <string>
#include <vector>

namespace ncr {

inline
const std::string
strformat(const char *const fmt, ...)
{
	va_list va_args;
	va_start(va_args, fmt);

	// reliably acquire the size using a copy of the variable argument array,
	// and a functionally reliable call to mock the formatting
	va_list va_copy;
	va_copy(va_copy, va_args);
	const int len = std::vsnprintf(NULL, 0, fmt, va_copy);
	va_end(va_copy);

	// return a formatted string without risk of memory mismanagement and
	// without assuming any compiler or platform specific behavior using a
	// vector with zero termination at the end
	std::vector<char> str(len + 1);
	std::vsnprintf(str.data(), str.size(), fmt, va_args);
	va_end(va_args);

	// return the string
	return std::string(str.data(), len);
}

} // ncr::
