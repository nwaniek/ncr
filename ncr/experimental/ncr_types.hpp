#pragma once

#include <cstdint>

namespace ncr {

// Following the recommendations in cppfront, we specify some
// fixed precision aliases. It's encouraged to always use these instead of
// variable precision names, as the latter depend on the machine the code is
// running on.
using i8         = std::int8_t;
using i16        = std::int16_t;
using i32        = std::int32_t;
using i64        = std::int64_t;
using u8         = std::uint8_t;
using u16        = std::uint16_t;
using u32        = std::uint32_t;
using u64        = std::uint64_t;

// variable precision names, ideally don't use them
using ushort     = unsigned short;
using ulong      = unsigned long;
using longlong   = long long;
using ulonglong  = unsigned long long;
using longdouble = long double;

// even worse than variable precision names... use them only when required, i.e.
// for backwards compatibility
using __schar    = signed char;    // use i8 instead
using __uchar    = unsigned char;  // use u8 instead

} // ncr::
