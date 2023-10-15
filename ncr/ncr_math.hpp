/*
 * ncr_math - mathematical functions and definitions
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 *
 * NOTE: currently considered obsolete and not widely used in ncr
 */
#pragma once

// #include <cmath>
#include <cstddef>

namespace ncr {

#define M_PI 3.14159265358979323846


/*
 * very basic vector types that are commonly used and don't need a template
 *
 * TODO: swap with stuff from Eigen, or Christoph's swizzling magic and an ugly
 *       template (not the one below)
 */
struct Vec2i { int    x, y; };
struct Vec2f { float  x, y; };
struct Vec2d { double x, y; };
struct Vec3f { float  x, y, z; };
struct Vec3d { double x, y, z; };


/*
 * sign - copmute the sign of a value
 */
template <typename T = double>
T
sign(T val) {
	return T((T(0) < val) - (val < T(0)));
}


/**
 * clamp - clamp a value to [-pi,pi]
 */
template <typename RealType = double>
RealType
clamp(RealType x)
{
	while (x < -M_PI) x += static_cast<RealType>(2.0) * M_PI;
	while (x > +M_PI) x -= static_cast<RealType>(2.0) * M_PI;
	return x;
}



constexpr inline
int
periodic(int a, int b)
{
	// the modulo operator % will yield a negative result in case the first
	// operand is negative. The following code achieves python-like modulo
	// results.
	return (b + (a % b)) % b;
}

}
