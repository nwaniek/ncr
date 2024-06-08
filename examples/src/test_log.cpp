#include <iostream>
#include <type_traits>

// #include "shared.hpp"

#define NCR_ENABLE_LOG_LEVEL_VERBOSE
#include <ncr/ncr_log.hpp>

namespace ncr {
	NCR_LOG_DECLARATION(new logger_policy_stdcout());
}


int main()
{

	ncr::log_error("log_error\n");
	ncr::log_warning("log_warning\n");
	ncr::log_debug("log_debug\n");
	ncr::log_verbose("log_verbose\n");


	return 0;
}

