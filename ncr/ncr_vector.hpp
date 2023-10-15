/*
 * ncr_vector - A vector class on top of BLAS (if enabled)
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 * This file contains a transparent vector implementation that falls back onto
 * BLAS as much as possible
 */

#pragma once

#include <cstddef>
#include <cassert>
#include <algorithm>

#include <initializer_list>
#include <iostream>

#ifndef NCR_USE_BLAS
#	define NCR_USE_BLAS false
#endif
#ifndef NCR_VECTOR_MOVE_SEMANTICS
#	define NCR_VECTOR_MOVE_SEMANTICS true
#endif

// additional includes might be required when compiling with BLAS support
#if NCR_USE_BLAS
#include <cblas.h>
#endif

namespace ncr {


// custom vector class which can attach to a memory location
// NOTE: keep the _t suffix to make it more easily distinguishable from
// std::vector or other vector classes, i.e. from Eigen.
template <size_t _N, typename T = double>
struct vector_t
{
	T*
		base_ptr;

	const size_t
		stride;

	bool
		__allocating;

	constexpr static size_t N = _N;

	typedef
		vector_t<_N, T> vector_type;


	// constructor that only hooks into existing memory and doesn't allocate
	// itself. Note: This constructor doesn't default initialize.
	vector_t(T *_ptr, size_t _stride = 1)
	: base_ptr(_ptr)
	, stride(_stride)
	, __allocating(false)
	{ }

	// constructor that allocates and initializes to 0
	vector_t()
	: stride(1)
	, __allocating(true)
	{
		this->base_ptr = new T[_N];
		(*this) = (T)0;
	}

	// constructor that eats an initializer_list of values to pass along and
	// allocates memory to store the data
	vector_t(const std::initializer_list<T> list)
	: stride(1)
	, __allocating(true)
	{
		assert(list.size() == _N);
		this->base_ptr = new T[_N];
		(*this) = list;
	}

	// constructor that eats an initializer_list of values and fills the
	// referenced base_ptr with it
	vector_t(const std::initializer_list<T> list, T *_ptr, size_t _stride = 1)
	: base_ptr(_ptr)
	, stride(_stride)
	, __allocating(false)
	{
		(*this) = list;
	}

	// destructor
	~vector_t() {
		// FIXME: can get rid of the != nullptr test
		if (this->__allocating) {
			delete[] this->base_ptr;
		}
	}

	// remove copy contructor
	// FIXME: why did I remove this again?
	vector_t(const vector_t &vec)
	: base_ptr(nullptr)
	, stride(1)
	, __allocating(true)
	{
		if (&vec == this)
			return;

		// allocate memory
		this->base_ptr = new T[_N];

		// copy over stuff
	#if NCR_USE_BLAS
		cblas_dcopy(N, vec.base_ptr, vec.stride, this->base_ptr, this->stride);
	#else
		for (size_t i = 0; i < N; i++)
			(*this)[i] = vec[i];
	#endif
	}

	// custom move constructor
#if NCR_VECTOR_MOVE_SEMANTICS
	vector_t(vector_t &&v) noexcept
	: base_ptr(v.base_ptr)
	, stride(v.stride)
	, __allocating(v.__allocating)
	{
		// prevent double-free corruption by setting the base_ptr of the
		// "incomming" vector to nullptr
		v.base_ptr = nullptr;
	}
#endif


	// some math operators (TODO: fill in with what is needed)
	T
	operator[](const size_t idx) const {
		assert(idx < _N);
		return this->base_ptr[idx * this->stride];
	}

	T&
	operator[](const size_t idx) {
		assert(idx < _N);
		return this->base_ptr[idx * this->stride];
	}


	// assignment operator for a full vector
	vector_type&
	operator=(const vector_type &right)
	{
		if (&right == this)
			return *this;

	#if NCR_USE_BLAS
		cblas_dcopy(N, right.base_ptr, right.stride, this->base_ptr, this->stride);
	#else
		for (size_t i = 0; i < N; i++)
			(*this)[i] = right[i];
	#endif
		return *this;
	}

	// assignment operator from an initializer list
	// Note: there is no straightforward way to do this via blas, if a user
	// intends to use the blas backend
	vector_type&
	operator=(const std::initializer_list<T> list)
	{
		size_t i = 0;
		for (auto elem : list) {
			(*this)[i] = elem;
			++i;
		}
		return *this;
	}

#if NCR_VECTOR_MOVE_SEMANTICS
	// move assignment
	vector_type&
	operator=(vector_type &&right) noexcept
	{
		if (&right == this)
			return *this;

		// in case we work with allocating vectors, then we can simply grab the
		// pointer as memory management is handled within. If not, we need to
		// copy
		if (this->__allocating && right.__allocating) {
			// grab pointer and set incoming one to null to prevent double free
			// corruptions
			this->base_ptr = right.base_ptr;
			right.base_ptr = nullptr;
		}
		else {
		#if NCR_USE_BLAS
			cblas_dcopy(N, right.base_ptr, right.stride, this->base_ptr, this->stride);
		#else
			for (size_t i = 0; i < N; i++)
				(*this)[i] = right[i];
		#endif
		}

		return *this;
	}
#endif

	// assignment operator for a scalar
	vector_type&
	operator=(const T v)
	{
	#if NCR_USE_BLAS
		cblas_dcopy(N, &v, 0, this->base_ptr, this->stride);
	#else
		for (size_t i = 0; i < N; i++)
			(*this)[i] = v;
	#endif
		return *this;
	}

	// multiply scalar
	vector_type&
	operator*=(const T v)
	{
		// TODO: this depends on the type! FIXME: future version should have
		// template specialization on the contained type
	#if NCR_USE_BLAS
		cblas_dscal(N, v, this->base_ptr, this->stride);
	#else
		for (size_t i = 0; i < N; i++)
			(*this)[i] *= v;
	#endif
		return *this;
	}

	// add scalar
	vector_type&
	operator+=(const T v)
	{
	#if NCR_USE_BLAS
		cblas_daxpy(N, 1.0, &v, 0, this->base_ptr, this->stride);
	#else
		for (size_t i = 0; i < N; i++)
			(*this)[i] += v;
	#endif
		return *this;
	}

	vector_type&
	operator+=(const vector_type &right)
	{
	#if NCR_USE_BLAS
		cblas_daxpy(N, 1.0, right.base_ptr, right.stride, this->base_ptr, this->stride);
	#else
		for (size_t i = 0; i < N; i++)
			(*this)[i] += right[i];
	#endif
		return *this;
	}


	// subtract scalar
	vector_type&
	operator-=(const T v)
	{
	#if NCR_USE_BLAS
		cblas_daxpy(N, -1.0, &v, 0, this->base_ptr, this->stride);
	#else
		for (size_t i = 0; i < N; i++)
			(*this)[i] -= v;
	#endif
		return *this;
	}

	vector_type&
	operator-=(const vector_type &right)
	{
	#if NCR_USE_BLAS
		cblas_daxpy(N, -1.0, right.base_ptr, right.stride, this->base_ptr, this->stride);
	#else
		for (size_t i = 0; i < N; i++)
			(*this)[i] -= right[i];
	#endif
		return *this;
	}

	// divide by scalar
	vector_type&
	operator/=(const T v)
	{
		assert(v != 0.0);
		const T divisor = 1.0 / v;
		this->operator*=(divisor);
		return *this;
	}

	// add to this vector (called y) another vector x multiplied by a, such that the result
	// is y = alpha * x + y.
	vector_type&
	axpy(const T alpha, const vector_type &x)
	{
	#if NCR_USE_BLAS
		cblas_daxpy(N, alpha, x.base_ptr, x.stride, this->base_ptr, this->stride);
	#else
		for (size_t i = 0; i < N; i++)
			(*this)[i] += alpha * x[i];
	#endif
		return *this;
	}


	T
	asum() {
	#if NCR_USE_BLAS
		return cblas_dasum(N, this->base_ptr, this->stride);
	#else
		T result = (T)0;
		for (size_t i = 0; i < N; i++)
			result += std::abs((*this)[i]);
		return result;
	#endif
	}
};


/*
 * Scalar Operators, i.e.
 *		alpha OP vector
 * or
 *		vector OP alpha
 */

// operator+
template <size_t _N, typename T = double>
vector_t<_N, T>
operator+ (T v, const vector_t<_N, T> &vec)
{
	vector_t<_N, T> result;
	result = vec;
	result += v;
	return result;
}

template <size_t _N, typename T = double>
vector_t<_N, T>
operator+ (const vector_t<_N, T> &vec, T v)
{
	vector_t<_N, T> result;
	result = vec;
	result += v;
	return result;
}

#if NCR_VECTOR_MOVE_SEMANTICS
template <size_t _N, typename T = double>
vector_t<_N, T>&&
operator+ (T v, vector_t<_N, T> &&vec)
{
	vec += v;
	return std::move(vec);
}

template <size_t _N, typename T = double>
vector_t<_N, T>&&
operator+ (vector_t<_N, T> &&vec, T v)
{
	vec += v;
	return std::move(vec);
}
#endif


// operator*
template <size_t _N, typename T = double>
vector_t<_N, T>
operator* (T v, const vector_t<_N, T> &vec)
{
	vector_t<_N, T> result;
	result = vec;
	result *= v;
	return result;
}

template <size_t _N, typename T = double>
vector_t<_N, T>
operator* (const vector_t<_N, T> &vec, T v)
{
	vector_t<_N, T> result;
	result = vec;
	result *= v;
	return result;
}

#if NCR_VECTOR_MOVE_SEMANTICS
template <size_t _N, typename T = double>
vector_t<_N, T>&&
operator* (T v, vector_t<_N, T> &&vec)
{
	vec *= v;
	return std::move(vec);
}

template <size_t _N, typename T = double>
vector_t<_N, T>&&
operator* (vector_t<_N, T> &&vec, T v)
{
	vec *= v;
	return std::move(vec);
}
#endif



// operator/
template <size_t _N, typename T = double>
vector_t<_N, T>
operator/ (T v, const vector_t<_N, T> &vec)
{
	vector_t<_N, T> result;
	result = v;
	result /= vec;
	return result;
}

template <size_t _N, typename T = double>
vector_t<_N, T>
operator/ (const vector_t<_N, T> &vec, T v)
{
	vector_t<_N, T> result;
	result = vec;
	result /= v;
	return result;
}

#if NCR_VECTOR_MOVE_SEMANTICS
template <size_t _N, typename T = double>
vector_t<_N, T>&&
operator/ (vector_t<_N, T> &&vec, T v)
{
	vec /= v;
	return std::move(vec);
}

template <size_t _N, typename T = double>
vector_t<_N, T>&&
operator/ (T v, vector_t<_N, T> &&vec)
{
	vec *= 1./v;
	return std::move(vec);
}
#endif


// operator-
template <size_t _N, typename T = double>
vector_t<_N, T>
operator- (T v, const vector_t<_N, T> &vec)
{
	vector_t<_N, T> result;
	result = v;
	result -= vec;
	return result;
}

template <size_t _N, typename T = double>
vector_t<_N, T>
operator- (const vector_t<_N, T> &vec, T v)
{
	vector_t<_N, T> result;
	result = vec;
	result -= v;
	return result;
}

#if NCR_VECTOR_MOVE_SEMANTICS
template <size_t _N, typename T = double>
vector_t<_N, T>&&
operator- (vector_t<_N, T> &&vec, T v)
{
	vec -= v;
	return std::move(vec);
}

template <size_t _N, typename T = double>
vector_t<_N, T>&&
operator- (T v, vector_t<_N, T> &&vec)
{
	// FIXME: maybe not the fastest
	vec *= -1.0;
	vec += v;
	return std::move(vec);
}
#endif


/*
 * Vector Vector ops, i.e. v0 OP v1
 */

// operator+
template <size_t N, typename T = double>
vector_t<N, T>
operator+ (const vector_t<N, T> &v0, const vector_t<N, T> &v1)
{
	vector_t<N, T> result;
	result  = v0;
	result += v1;
	return result;
}

#if NCR_VECTOR_MOVE_SEMANTICS
template <size_t N, typename T = double>
vector_t<N, T>&&
operator+ (vector_t<N, T> &&v0, const vector_t<N, T> &v1)
{
	v0 += v1;
	return std::move(v0);
}

template <size_t N, typename T = double>
vector_t<N, T>&&
operator+ (const vector_t<N, T> &v0, vector_t<N, T> &&v1)
{
	v1 += v0;
	return std::move(v1);
}

template <size_t N, typename T = double>
vector_t<N, T>&&
operator+ (vector_t<N, T> &&v0, vector_t<N, T> &&v1)
{
	v0 += v1;
	return std::move(v0);
}
#endif


// operator-
template <size_t N, typename T = double>
vector_t<N, T>
operator- (const vector_t<N, T> &v0, const vector_t<N, T> &v1)
{
	vector_t<N, T> result;
	result  = v0;
	result -= v1;
	return result;
}

#if NCR_VECTOR_MOVE_SEMANTICS
template <size_t N, typename T = double>
vector_t<N, T>&&
operator- (vector_t<N, T> &&v0, const vector_t<N, T> &v1)
{
	v0 -= v1;
	return std::move(v0);
}

template <size_t N, typename T = double>
vector_t<N, T>&&
operator- (const vector_t<N, T> &v0, vector_t<N, T> &&v1)
{
	// FIXME: maybe not the fastest
	v1 *= -1.0;
	v1 += v0;
	return std::move(v1);
}

template <size_t N, typename T = double>
vector_t<N, T>&&
operator- (vector_t<N, T> &&v0, vector_t<N, T> &&v1)
{
	v0 -= v1;
	return std::move(v0);
}
#endif


// additional operators
template <size_t N, typename T = double>
std::ostream&
operator<<(std::ostream &os, const vector_t<N, T> &v)
{
	os << "[";
	if (v.N > 0)
		os << v[0];
	for (size_t i = 1; i < v.N; i++) {
		os << ", " << v[i];
	}
	os << "]";
	return os;
}


} // ncr::
