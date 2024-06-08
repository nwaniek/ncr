#include <iostream>
#include <filesystem>
#include <cstdint>
#include <vector>
#include <streambuf>

#include <zip.h>


/*
 * zipfront - frontend interface for a zip backend
 */
struct zipfront
{

};


struct libzip: zipfront
{
	void unzip(std::filesystem::path)
	{ }
	// zip
};


int main()
{
	//std::string filename{"assets/npy/simpletensor2.npz"};
	std::string filename{"assets/npy/multiple_named.npz"};

	int err = 0;
	zip_t* zip;
	if ((zip = zip_open(filename.c_str(), ZIP_RDONLY, &err)) == nullptr) {
		zip_error_t error;
		zip_error_init_with_code(&error, err);
		std::cerr << "cannot open zip archive " << filename << ": " << zip_error_strerror(&error) << "\n";
		return EXIT_FAILURE;
	}

	zip_error_t error;
	zip_int64_t num_entries = zip_get_num_entries(zip, 0);
	for (zip_int64_t i = 0; i < num_entries; i++) {
		zip_stat_t stat;
		if (zip_stat_index(zip, i, 0, &stat) != 0) {
			zip_error_init_with_code(&error, err);
			std::cerr << "cannot read zip_stat_t in " << filename << ": " << zip_error_strerror(&error) << "\n";
			return EXIT_FAILURE;
		}
		// test if name bit is set.
		// TODO: if not, generate a name and issue a warning
		if (stat.valid & ZIP_STAT_NAME)
			std::cout << stat.name << "\n";

		// TODO: test if this is ZIP_CM_DEFLATE (because that means zlib, which
		// is what numpy uses), or ZIP_CM_STORE (uncompressed npz)
		if (stat.valid & ZIP_STAT_COMP_METHOD)
			std::cout << stat.comp_method << "\n"; // 8 == ZIP_CM_DEFLATE -> zlib

		if (stat.valid & ZIP_STAT_SIZE)
			std::cout << stat.size << " bytes\n";
		else {
			// TODO: we have a problem
			std::cerr << "no size information\n";
			return EXIT_FAILURE;
		}

		// TODO: turn stat into information for Npzfile -> ncr::numpy::fileinfo
		// or some new struct specifically for zip files. maybe
		// ncr::numpy::npzfileinfo or similar

		// open the file, directly decompressing it
		zip_file_t *fp = zip_fopen_index(zip, i, 0);
		if (fp == nullptr) {
			std::cerr << "cannot open " << stat.name << " from archive: " << zip_error_strerror(&error) << "\n";
			return EXIT_FAILURE;
		}

		// read the whole thing in one go
		std::vector<std::uint8_t> buffer(stat.size);
		zip_int64_t nread = zip_fread(fp, buffer.data(), stat.size);
		if (nread <= 0) {
			std::cerr << "Could not read from file " << stat.name << ": ";
			if (nread == 0) {
				std::cerr << "end of file reached\n";
			}
			else {
				// TODO: zip_error_init_with_code is wrong here, we do not have
				// an error code
				zip_error_init_with_code(&error, err);
				std::cerr << zip_error_strerror(&error) << "\n";
			}
			return EXIT_FAILURE;
		}
		// read the file

		// close the file
		if ((err = zip_fclose(fp)) != 0) {
			zip_error_init_with_code(&error, err);
			std::cerr << zip_error_strerror(&error) << "\n";
		}
	}

	zip_close(zip);
	return EXIT_SUCCESS;
}
