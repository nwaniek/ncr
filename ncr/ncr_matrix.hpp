/*
 * ncr_matrix - ncr specific functions to interact with matrices (Eigen, ...)
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 *
 */
#pragma once

#include <iostream>
#include <fstream>
#include <Eigen/Dense>
#include <ncr/ncr_utils.hpp>
#include <ncr/ncr_filesystem.hpp>

namespace ncr { namespace matrix {


namespace io {

enum struct status: unsigned {
	Unknown           = 0x00,
	Success           = 0x01,
	WriteError        = 0x02,
	ReadError         = 0x04,
	// TODO: more specific errors
	ErrorFileNotFound = 0x08
};
NCR_DEFINE_ENUM_FLAG_OPERATORS(status)


// TODO: this only works for binary matrices, but is the standard way to turn
//       the flat array of an Eigen matrix into a binary file.
// TODO: check during compile time if the matrix is of proper type (dense Eigen
//       matrix)
template <typename Matrix>
inline status
write_dense(const std::string &filename, const Matrix &matrix)
{
	std::ofstream out(filename, std::ios::out | std::ios::binary | std::ios::trunc);
	if (out.is_open()) {
		using index_t = typename Matrix::Index;
		using scalar_t = typename Matrix::Scalar;

		index_t rows = matrix.rows();
		index_t cols = matrix.cols();

		// the format is to simply write the size of the matrix, followed by the
		// raw data in the file. This can be read easily in C++ and also Python
		out.write(reinterpret_cast<const char*>(&rows), sizeof(index_t));
		out.write(reinterpret_cast<const char*>(&cols), sizeof(index_t));

		// write the data itself
		out.write(reinterpret_cast<const char*>(matrix.data()), rows*cols*sizeof(scalar_t));

		// success
		return status::Success;
	}
	else {
		return status::WriteError;
	}
}


template <typename Matrix>
inline status
read_dense(const std::string &filename, Matrix &matrix)
{
	if (!ncr::filesystem::exists(filename))
		return status::ErrorFileNotFound;

	std::ifstream in(filename, std::ios::in | std::ios::binary);
	if (in.is_open()) {
		// read the rows and columns of the matrix
		using index_t = typename Matrix::Index;
		using scalar_t = typename Matrix::Scalar;

		typename Matrix::Index rows = 0, cols = 0;
		in.read(reinterpret_cast<char*>(&rows), sizeof(index_t));
		in.read(reinterpret_cast<char*>(&cols), sizeof(index_t));

		// resize matrix and load from file
		matrix.resize(rows, cols);
		in.read(reinterpret_cast<char*>(matrix.data()), rows*cols*sizeof(scalar_t));

		// success
		return status::Success;
	}
	else {
		return status::ReadError;
	}
}



} // ::io
}} // ncr::matrix
