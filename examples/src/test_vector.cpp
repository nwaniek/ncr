#include <iostream>
#include <vector>
#include <cmath>
#include <cstddef>
#include <cassert>
#include <iomanip> // setprecision
#include <algorithm>

#include <cblas.h>

#include "shared.hpp"
#include <ncr/ncr_log.hpp>
#include <ncr/ncr_math.hpp>

#define NCR_USE_BLAS false
#include <ncr/ncr_vector.hpp>

#define COUT_PRECISION 4

/*
 * purpose of this unit: test capabilities of the vector class
 */

using namespace ncr;


void
test_vector()
{
	// version with vector
	vector_t<3, double> v0;
	vector_t<3, double> v1;

	// v0 = 0.;
	v1 = 123.;

	v0 += 2.0;
	v0 *= 10.0;

	v1 = 0;
	v1.axpy(0.1, v0);

	// note: using free operators introduces temporary objects. this can be
	// avoided by using operators bound to the vector class
	v1 = 0.1 * v0;
	v1 = v1 - 0.5;
	v1 = 0.5 + v1;
	v1 = v1 / 2.;

	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << "v0 = " << v0 << "\n";

	// test swapping vectors
	std::cout << "pre swap\n";
	std::cout << "v0 = " << v0 << "\n";
	std::cout << "v1 = " << v1 << "\n";
	std::swap(v0, v1);
	std::cout << "post swap\n";
	std::cout << "v0 = " << v0 << "\n";
	std::cout << "v1 = " << v1 << "\n";
}


void
test_vector_temporaries()
{
	vector_t<3> v0, v1;

	v0 = 1.;
	v1 = 2.;

	v0 = 0.5 * (v1 + .25) + 7.5;
	// v1 = 1.0 - v0;

	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << v0 << std::endl;
	std::cout << v1 << std::endl;
}

void
test_daxpy()
{
	const int N = 3;    // number of elements in x and y
	const int incX = 1; // stride / stepwidth of data in x
	const int incY = 1; // stride / stepwidth of data in y
	double a = 2.0;
	double x[3] = {1., 2., 3.};
	double y[3] = {.0, .0, .0};

	cblas_daxpy(N, a, x, incX, y, incY);

	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << "y = " << y[0] << ", " << y[1] << ", " << y[2] << "\n";
}


void
test_dscal()
{
	const int N = 3;
	const int incX = 1;
	double a    = 0.1;
	double x[3] = {1., 2., 3.};

	cblas_dscal(N, a, x, incX);

	std::cout << std::fixed << std::setprecision(COUT_PRECISION);
	std::cout << "x = " << x[0] << ", " << x[1] << ", " << x[2] << "\n";
}


void
test_misc()
{
	constexpr size_t N = 10;
	double array[N];
	// initialize the memory to some values
	for (size_t i = 0; i < N; i++) {
		array[i] = (double)i;
	}

	ncr::vector_t<3, double> v0{0.5, 0.1, 0.2}, v1, v2(array, 3), v3(&array[1], 3);

	// standard vector usage
	v1 = 0.5 * v0;
	std::cout << std::fixed << std::setprecision(2);
	std::cout << "v0 = " << v0 << "\nv1 = " << v1 << "\n";

	// vector usage of the vector that references another memory slab to store
	// its data
	std::cout << "v2 = " << v2 << "\n";
	std::cout << "v3 = " << v3 << "\n";

	v3 = (0.5 * v1 - v2) * 2.0 - v0;

	std::cout << "a  = [" << array[0];
	for (size_t i = 1; i < N; i++) {
		std::cout << ", " << array[i];
	}
	std::cout << "]\n";

}


int main()
{
	test_daxpy();
	test_dscal();
	test_vector();
	test_vector_temporaries();
	test_misc();

	return 0;
}

