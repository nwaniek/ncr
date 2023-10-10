/*
 * ncr_log - a minimalistic logging interface
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 *
 * This file declares an extern variable. Hence, it is necessary to have
 *     LOG_DECLARATION;
 * in one of the translation files of your project! It is also required to set a
 * log policy, e.g. one provided within this header or a custom one. To access
 * the logger, there is a convencience macro called LOG_INSTANCE. For instance,
 * a simple main.cpp that uses this logger is
 *
 *     #include <iostream>
 *     #include <ncr/ncr_log.hpp>
 *     #include <some/other/header.hpp>
 *
 *     ncr::NCR_LOG_DECLARATION;
 *
 *     int main() {
 *	       // set the default logger instance
 *         ncr::NCR_LOG_INSTANCE.set_policy(new ncr::logger_policy_stdcout());
 *
 *         // do something interesting
 *         return 0;
 *     }
 *
 * as an alternative, it is possible to provide the logger policy in the
 * LOG_DECLARATION call:
 *
 *     #include <iostream>
 *     #include <ncr/ncr_log.hpp>
 *     #include <some/other/header.hpp>
 *
 *     ncr::NCR_LOG_DECLARATION(new ncr::logger_policy_stdcout());
 *
 *     int main() {
 *         // do something interesting
 *         return 0;
 *     }
 *
 * In the example above, the logger will assume ownership of the policy. This
 * means that the policy will be destroyed in the logger's destructor. Note that
 * the logger will also destroy the policy when a new policy is set via
 * set_policy. To prevent the logger from taking ownership, it is possible to
 * pass an appropriate flag as second argument during LOG_DECLARATION. This is useful
 * for custom logger policies which shouldn't be owned by the logger. To
 * retrieve the pointer from the logger, either keep the logger in a local
 * variable, or call set_policy with nullptr as argument. For more details on
 * the behavior of set_policy, see documentation below.
 *
 *     #include <iostream>
 *     #include <ncr/ncr_log.hpp>
 *     #include <some/other/header.hpp>
 *
 *     struct custom_logger_policy : logger_policy {
 *         // implementation ...
 *     }
 *
 *     ncr::NCR_LOG_DECLARATION(new custom_logger_policy(), false);
 *
 *     int main() {
 *         // do something interesting
 *
 *         // get the pointer to the policy and tidy up afterwards
 *         auto policy = NCR_LOG_INSTANCE.set_policy(nullptr);
 *         delete policy;
 *
 *         return 0;
 *     }
 */
#pragma once

#include <utility>
#ifndef NCR_LOG
#define NCR_LOG
#endif

#include <mutex>
#include <ostream>
#include <iostream>
#include <fstream>
#include <sstream>

namespace ncr {

#define NCR_LOG_LEVEL_NONE    0
#define NCR_LOG_LEVEL_ERROR   1
#define NCR_LOG_LEVEL_WARNING 2
#define NCR_LOG_LEVEL_DEBUG   3
#define NCR_LOG_LEVEL_VERBOSE 4

enum struct log_level : unsigned {
	None    = NCR_LOG_LEVEL_NONE,
	Error   = NCR_LOG_LEVEL_ERROR,
	Warning = NCR_LOG_LEVEL_WARNING,
	Debug   = NCR_LOG_LEVEL_DEBUG,
	Verbose = NCR_LOG_LEVEL_VERBOSE,
};


#ifdef NCR_ENABLE_LOG_LEVEL_VERBOSE
	#define NCR_LOG_LEVEL NCR_LOG_LEVEL_VERBOSE
#else
	#ifdef NCR_ENABLE_LOG_LEVEL_DEBUG
		#define NCR_LOG_LEVEL NCR_LOG_LEVEL_DEBUG
	#else
		#ifdef NCR_ENABLE_LOG_LEVEL_WARNING
			#define NCR_LOG_LEVEL NCR_LOG_LEVEL_WARNING
		#else
			#ifdef NCR_ENABLE_LOG_LEVEL_ERROR
				#define NCR_LOG_LEVEL NCR_LOG_LEVEL_ERROR
			#else
				#define NCR_LOG_LEVEL NCR_LOG_LEVEL_NONE
			#endif
		#endif
	#endif
#endif


// can use these macros instead of explicitly naming the thing
#define NCR_LOG_INSTANCE    ncr_logger_instance
#define NCR_LOG_DECLARATION logger NCR_LOG_INSTANCE


struct logger_policy {
	virtual void log_error(const std::string &msg) = 0;
	virtual void log_warning(const std::string &mgs) = 0;
	virtual void log_debug(const std::string &mgs) = 0;
	virtual void log_verbose(const std::string &msg) = 0;
	virtual void init() = 0;
	virtual void finalize() = 0;
	virtual ~logger_policy() = 0;
};
// function body required for pure virtual destructor
inline logger_policy::~logger_policy() {}


/*
 * logger_policy_stdcout - policy to write everything to std::cout
 */
struct logger_policy_stdcout : logger_policy {

	void log_error(const std::string &msg) override {
		this->stream << "EE: " << msg;
	}

	void log_warning(const std::string &msg) override {
		this->stream << "WW: " << msg;
	}

	void log_debug(const std::string &msg) override {
		this->stream << "II: " << msg;
		// this->stream << msg;
	}

	void log_verbose(const std::string &msg) override {
		this->stream << ">>: " << msg;
		// this->stream << msg;
	}

	void init() override {}
	void finalize() override {}
	~logger_policy_stdcout() override { this->finalize(); }

private:
	std::ostream &stream = std::cout;
};


/*
 * logger_policy_file - policy to write everything to a file
 */
struct logger_policy_file_t : logger_policy {
	void log_error(const std::string &msg) override {
		if (!this->_stream.is_open()) return;
		this->_stream << "EE: " << msg;
	}

	void log_warning(const std::string &msg) override {
		if (!this->_stream.is_open()) return;
		this->_stream << "WW: " << msg;
	}

	void log_debug(const std::string &msg) override {
		if (!this->_stream.is_open()) return;
		this->_stream << "II: " << msg;
	}

	void log_verbose(const std::string &msg) override {
		if (!this->_stream.is_open()) return;
		this->_stream << ">>: " << msg;
	}

	void init() override {
		if (this->_stream.is_open()) return;
		this->_stream.open(this->_filename);
	}

	void finalize() override {
		if (this->_stream.is_open())
			this->_stream.close();
	}

	logger_policy_file_t(const std::string &filename)
		: _filename(filename)
	{}

	~logger_policy_file_t() override { this->finalize(); }

private:
	std::string   _filename;
	std::ofstream _stream;
};



/*
 * logger - A minimalistic logger implementation
 */
struct logger {

	#define NCR_LOG_LEVEL_IMPL(LL_NAME, LL_REQUIRED)               \
		inline void                                                  \
		log_##LL_NAME ()                                             \
		{                                                            \
			if constexpr (NCR_LOG_LEVEL < LL_REQUIRED)               \
				return;                                              \
			if (this->_policy)                                       \
				this->_policy->log_##LL_NAME(this->_strings.str());  \
			this->_strings.str("");                                  \
		}                                                            \
																	 \
		template <typename Arg, typename... Args>                    \
		inline void log_##LL_NAME(Arg arg, Args &&... args)          \
		{                                                            \
			if constexpr (NCR_LOG_LEVEL < LL_REQUIRED)               \
				return;                                              \
			this->_strings << arg;                                   \
			log_##LL_NAME(std::forward<Args>(args)...);              \
		}

	NCR_LOG_LEVEL_IMPL(error,   NCR_LOG_LEVEL_ERROR)
	NCR_LOG_LEVEL_IMPL(warning, NCR_LOG_LEVEL_WARNING)
	NCR_LOG_LEVEL_IMPL(debug,   NCR_LOG_LEVEL_DEBUG)
	NCR_LOG_LEVEL_IMPL(verbose, NCR_LOG_LEVEL_VERBOSE)

	#undef NCR_LOG_LEVEL_IMPL


	template <log_level LogLevel, typename... Args>
	void
	log(Args... args)
	{
		this->_mtx.lock();
		switch (LogLevel) {
		case log_level::Error:
			this->log_error(args...);
			break;
		case log_level::Warning:
			this->log_warning(args...);
			break;
		case log_level::Debug:
			this->log_debug(args...);
			break;
		case log_level::Verbose:
			this->log_verbose(args...);
			break;
		}
		this->_mtx.unlock();
	}


	/*
	 * logger - Create a new logger
	 *
	 * This allows to create a new logger, and directly setting a certain logger
	 * policy. It is also possible to tell the logger not to have ownership of
	 * the policy. In case ownership is assumed, then the policy will be
	 * destroyed either in the loggger's destructor, or when a new policy is
	 * set via set_policy.
	 */
	logger(logger_policy *policy = nullptr, bool has_policy_ownership = true)
		: _policy(nullptr)
        , _has_policy_ownership(has_policy_ownership)
	{
		this->set_policy(policy);
	}


	/*
	 * ~logger - destroy a logger
	 *
	 * This function calls the policy's .finalize() method. In case the logger
	 * has ownership of the policy, also the policy's destructor will be called
	 * and memory freed.
	 */
	~logger() {
		if (!this->_policy)
			return;

		this->_policy->finalize();
		if (this->_has_policy_ownership)
			delete this->_policy;
	}


	/*
	 * set_policy - set a new logger policy.
	 *
	 * This method returns the previously set policy. Depending on the logger's
	 * has_policy_ownership status, the old policy will be either destroyed and
	 * this function returns a nullptr, or the function returns a pointer to the
	 * old policy.
	 *
	 * Note that this function calls the policy's init().
	 */
	logger_policy*
	set_policy(logger_policy* policy) {
		logger_policy *old_policy = this->_policy;

		// in case the logger has ownership, delete and set to nullptr
		if (_has_policy_ownership) {
			delete old_policy;
			old_policy = nullptr;
		}

		// set and initialize new policy
		this->_policy = policy;
		if (this->_policy)
			this->_policy->init();

		return old_policy;
	}

private:
	logger_policy *
		_policy = nullptr;

	bool
		_has_policy_ownership = true;

	std::mutex
		_mtx;

	std::stringstream
		_strings;
};


extern NCR_LOG_DECLARATION;


template <typename... Args>
inline void
log_error(Args &&... args)
{
	if constexpr (NCR_LOG_LEVEL >= NCR_LOG_LEVEL_ERROR)
		NCR_LOG_INSTANCE.log<log_level::Error>(std::forward<Args>(args)...);
}

template <typename... Args>
inline void
log_warning(Args &&... args)
{
	if constexpr (NCR_LOG_LEVEL >= NCR_LOG_LEVEL_WARNING)
		NCR_LOG_INSTANCE.log<log_level::Warning>(std::forward<Args>(args)...);
}

template <typename... Args>
inline void
log_debug(Args &&... args)
{
	if constexpr (NCR_LOG_LEVEL >= NCR_LOG_LEVEL_DEBUG)
		NCR_LOG_INSTANCE.log<log_level::Debug>(std::forward<Args>(args)...);
}


template <typename... Args>
inline void
log_verbose(Args &&... args)
{
	if constexpr (NCR_LOG_LEVEL >= NCR_LOG_LEVEL_VERBOSE)
		NCR_LOG_INSTANCE.log<log_level::Verbose>(std::forward<Args>(args)...);
}


} // ncr::
