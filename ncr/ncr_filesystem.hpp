/*
 * ncr_filesystem - ncr specific functions to interact with the file system
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 *
 */
#pragma once

#include <sys/stat.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

#include <ncr/ncr_const.hpp>
#include <ncr/ncr_utils.hpp>

namespace ncr {

enum struct filesystem_status : unsigned {
	Success           = 0x00,
	ErrorFileNotFound = 0x01
};
NCR_DEFINE_ENUM_FLAG_OPERATORS(filesystem_status)

inline
bool
test(filesystem_status s)
{
	return std::underlying_type<filesystem_status>::type(s);
}

/*
 * read_file - read a whole file into a string.
 *
 * This is not the fastest method, but it's portable. In case this is ever a
 * speed issue, maybe use fread directly.
 */
inline filesystem_status
read_file(std::string filename, std::string &content)
{
	auto os = std::ostringstream{};
	std::ifstream file(filename);
	if (!file) {
		content = "";
		return filesystem_status::ErrorFileNotFound;
	}
	os << file.rdbuf();
	content = os.str();
	return filesystem_status::Success;
}


/*
 * mkfilename - make a temporary filename based on current time
 */
inline std::string
mkfilename(std::string basename, std::string ext = ".cfg")
{
	std::ostringstream os;
	const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	const std::time_t tt = std::chrono::system_clock::to_time_t(now);
	std::tm tm = *std::localtime(&tt);
	os << basename << "-" << std::put_time(&tm, "%Y%m%d%H%M%S") << ext;
	return os.str();
}


// TODO: move the above into the filesystem namespace as well
namespace filesystem {

/*
 * exists - test if a file exists in the filesystem or not
 *
 * This is the fastest way to determine if a file exists or not. See
 * https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exists-using-standard-c-c11-14-17-c
 * for more discussion.
 */
inline bool
exists(const std::string &filename)
{
	struct stat buffer;
	return stat(filename.c_str(), &buffer) == 0;
}


} // ncr::filesystem

} // ncr::
