#include <iostream>
#include <ncr/ncr_numpy.hpp>


inline std::ostream&
operator<<(std::ostream &os, const u8_const_subrange &range)
{
	for (auto it = range.begin(); it != range.end(); ++it)
		std::cout << *it;
	return os;
}


template <typename T>
void print_array(ncr::ndarray &arr)
{
	std::cout << "array([";
	std::string indent = "";
	for (size_t row = 0; row < arr.shape()[0]; row++) {
		if (row > 0) {
			std::cout << ",\n";
			indent = "       ";
		}
		std::cout << indent << "[";
		for (size_t col = 0; col < arr.shape()[1]; col++) {
			if (col > 0)
				std::cout << ", ";
			std::cout << arr.value<T>(row, col);
		}
		std::cout << "]";
	}
	std::cout << "]";
}


int
main()
{
	std::filesystem::path path("assets/npy/0000000199.npz");

	ncr::numpy::npzfile npz;
	ncr::numpy::from_npz(path, npz);
	auto arr = npz["blob_field"];

	// this array is col-major, but the array loaded here reports row-major.
	// why?

	auto shape = arr.shape();
	std::cout << arr.get_type_description() << "\n";
	print_array<i32>(arr);

	return 0;
}
