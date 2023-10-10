/*
 * ncr_chrono - utility functions for tracking simulation time.
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 *
 * This file provides structures for tracking simulated real time in simulated
 * wall clock time as well as simply in ticks since the simulation started. The
 * main goal is to have type safe versions of time instead of merely tracking
 * these values in floats and integers.
 *
 * Note that this file and the interfaces is loosely inspired by
 * std::chrono but does *NOT* provide the same functionality. If you need to
 * track real wall clock time, refer to the functions in std::chrono. If you
 * wish for an easy way to track simulated time, e.g. time advanced using
 * ncr_simulation, then the functions and interfaces defined below might come in
 * handy.
 */

#include <cstdint>
#include <cstddef>


namespace ncr {
	enum struct ClockType : uint8_t { Ticks, Time, None};

	template <ClockType ct>
	struct time_storage_type {};

	template <>
	struct time_storage_type<ClockType::Ticks> {
		using type = size_t;
	};

	template <>
	struct time_storage_type<ClockType::Time> {
		using type = double;
	};

	template <>
	struct time_storage_type<ClockType::None> {
		using type = void;
	};


	// need some time provider, i.e. a clock interface
	// The simulation essentially implements such an interface and stores the
	// values in iteration_state

	// some type traits depending on
	//
	template <ClockType clock_type> struct duration;


	template <ClockType clock_type = ClockType::Ticks>
	struct time_point {
		using time_type = typename time_storage_type<clock_type>::type;

		time_type value;

		time_point() : value(0) {}
		time_point(const time_type time) : value(time) {}
		void operator=(const time_type time) { this->value = time; }

		time_point
		operator+(const time_point<clock_type> &t) {
			return time_point(this->value + t.value);
		}

		time_point
		operator+(const duration<clock_type> &d) {
			return time_point(this->value + d.value);
		}

		time_point&
		operator+=(const time_point<clock_type> &t) {
			this->value += t.value;
			return *this;
		}

		time_point&
		operator+=(const duration<clock_type> &d) {
			this->value += d.value;
			return *this;
		}

		bool
		operator==(const time_point<clock_type> &t) const {
			return this->value == t.value;
		}

		bool
		operator==(const time_type time) const {
			return this->value == time;
		}

		bool
		operator<=(const time_point<clock_type> &t) const {
			return this->value <= t.value;
		}

		bool
		operator<=(const time_type time) const {
			return this->value <= time;
		}
	};



	template <ClockType clock_type = ClockType::Ticks>
	struct duration {
		using time_type = typename time_storage_type<clock_type>::type;

		time_type value;

		duration() : value(0) {}
		duration(const time_type duration) : value(duration) {}
		void operator=(const time_type duration) { this->value = duration; }

		duration
		operator+(const duration<clock_type> &d) {
			return duration(this->value + d.value);
		}

		duration&
		operator+=(const duration<clock_type> &d) {
			this->value += d.value;
			return *this;
		}

		bool
		operator==(const duration<clock_type> &d) const {
			return this->value == d.value;
		}

		bool
		operator==(const time_type duration) const {
			return this->value == duration;
		}

		bool
		operator<=(const duration<clock_type> &d) const {
			return this->value <= d.value;
		}

		bool
		operator<=(const time_type duration) const {
			return this->value <= duration;
		}

	};
};
