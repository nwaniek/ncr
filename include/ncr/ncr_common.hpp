/*
 * ncr_common - Things used widely withing ncr
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 */

#pragma once

// required for ::random
#include <cassert>
#include <vector>
#include <random>
#include <chrono>
#include <tuple>
#include <cmath>
#include <cstdint>

// debugging -> cout, endl, cerr
#include <typeinfo>
#include <iostream>

namespace ncr {


/**
 * orient2D - compute the orientation of C wrt line AB.
 *
 * if the result is < 0, then C lies to the right of the directed line AB, if >
 * 0, then C lies to the left. it is zero if C lies on the line
 */
#if 0
template <class V, typename RealType = double>
auto orient2D(V A, V B, V C) -> RealType
{
	Eigen::Matrix<RealType, 2, 2> M;
	M << A[0] - C[0], A[1] - C[1],
	     B[0] - C[0], B[1] - C[1];
	return M.determinant();
}
#endif


/**
 * orient3D - compute the orientation of D wrt counter-clockwise plane ABC
 *
 * if the result is < 0, then D lies above the supporting plane abc (where ABC
 * is looked at in counter-clockwise terms) and below if the result is > 0. It
 * is zero if D lies on the plane
 */
#if 0
template <class V, typename RealType = double>
auto orient3D(V A, V B, V C, V D) -> RealType
{
	Eigen::Matrix<RealType, 3, 3> M;
	M << A[0] - D[0], A[1] - D[1], A[2] - D[2],
	     B[0] - D[0], B[1] - D[1], B[2] - D[2],
	     C[0] - D[0], C[1] - D[1], C[2] - D[2];
	return M.determinant();
}
#endif


} // ncr::
