#pragma once

#ifndef NCR_TYPES
#define NCR_TYPES
#endif

#include <cstdint>
#include <vector>
#if __cplusplus >= 202302L
	#include <stdfloat>
#endif
#include <complex>

// TODO: check if these are not already defined somehow
using i8           = std::int8_t;
using i16          = std::int16_t;
using i32          = std::int32_t;
using i64          = std::int64_t;
using i128         = long long int;

using u8           = std::uint8_t;
using u16          = std::uint16_t;
using u32          = std::uint32_t;
using u64          = std::uint64_t;
using u128         = unsigned long long int;

#if __cplusplus >= 202302L
	using f16      = std::float16_t;
	using f32      = std::float32_t;
	using f64      = std::float64_t;
	using f128     = std::float128_t;
#else
	using f16      = _Float16;
	using f32      = float;
	using f64      = double;
	using f128     = __float128; // TODO: include test if this is available, and
								 // if not emit at least a warning
#endif

using c64          = std::complex<f32>;
using c128         = std::complex<f64>;
using c256         = std::complex<f128>;

using u8_vector         = std::vector<u8>;
using u8_iterator       = u8_vector::iterator;
using u8_subrange       = std::ranges::subrange<u8_iterator>;
using u8_const_iterator = u8_vector::const_iterator;
using u8_const_subrange = std::ranges::subrange<u8_const_iterator>;

