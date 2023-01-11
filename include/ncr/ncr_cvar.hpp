/*
 * ncr_cvar - a Quake 3 inspired Configuration Variable system
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 *
 * This file implements a configurable variable (cvar) system similar to the one
 * that existed in Quake 2 and 3 (called q2 and q3, respectively). That is, you
 * can register cvars somewhere in your code, and these variables can be changed
 * (possibly) at other locations, for instance via a command line interface or
 * from configuration files.
 * Note that while the idea is taken from q2 and q3, the implementation below is
 * not based on the oriqinal q2 or q3 source code. Hence, it is not an exact
 * replacement that you could drop in a code based based on those two.
 */


/*
 * TODO: allow user to set NCR_TYPE_MISMATCH_IS_ERROR, by which a conversion
 *       issue below will be treated as a fatal error
 */

#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <optional>

#include <ncr/ncr_log.hpp>
#include <ncr/ncr_utils.hpp>

#ifdef NCR_CVAR_ENABLE_HDF5
#include <highfive/bits/H5Annotate_traits.hpp>
#include <highfive/H5DataType.hpp>
#include <highfive/H5DataSpace.hpp>
#include <highfive/H5Group.hpp>
#endif

namespace ncr {


// TODO: use type traits for compile time errors


//   CVT_TYPE       type              name        field  conversion function
#define NCR_CVAR_TYPE_LIST(_)                    \
	_(CVT_BOOL,     bool,             boolean,  b) \
    _(CVT_CHAR,     char,             char,     c) \
	_(CVT_INT,      int,              integer,  i) \
	_(CVT_UNSIGNED, unsigned,         unsigned, u) \
	_(CVT_FLOAT,    float,            float,    f) \
	_(CVT_DOUBLE,   double,           double,   d) \
	_(CVT_STRING,   std::string,      string,   s)


// additional types that don't explicitly exist in CVT, but will get mapped to
// other fields
#define NCR_CVAR_TYPE_ALIASES(_) \
	_(CVT_STRING,   const char *, "string",   s)


// list of vector types that the cvar system can handle. Note that this requires
// to specify not only the fields to which the data is mapped, but also the
// value_type.
// TODO: this doesn't really scale, and is generally quite ugly. Maybe re-think
//       the cvar system's implementation here. Or directly us ethe type list
//       from above? Also not everything is supported right now
#define NCR_CVAR_VECTOR_TYPE_LIST(_) \
	_(CVT_INTV,      std::vector<int>,         "intv"      , i, int)      \
	_(CVT_UNSIGNEDV, std::vector<unsigned>,    "unsignedv" , u, unsigned) \
	_(CVT_FLOATV,    std::vector<float>,       "floatv"    , f, float)    \
    _(CVT_DOUBLEV,   std::vector<double>,      "doublev"   , d, double)   \
    _(CVT_STRINGV,   std::vector<std::string>, "stringv"   , s, std::string)


/*
 * cvar_type - enumeration of all supported cvar types
 */
#define CVT_ITEM_NAME(NAME, ...) \
	NAME,
enum class cvar_type : unsigned {
	NCR_CVAR_TYPE_LIST(CVT_ITEM_NAME)
	NCR_CVAR_VECTOR_TYPE_LIST(CVT_ITEM_NAME)
};
#undef CVT_ITEM_NAME


/*
 */
#define CVT_TYPE_NAME(_0, _1, TYPE_NAME, ...) \
	#TYPE_NAME,
static inline constexpr const char* CVT_TYPE_NAMES[] = {
	NCR_CVAR_TYPE_LIST(CVT_TYPE_NAME)
	NCR_CVAR_VECTOR_TYPE_LIST(CVT_TYPE_NAME)
};
#undef CVT_TYPE_NAME


/*
 * cvar_type_is_vector_type - determine if a type is a vector type
 */
inline bool
cvar_type_is_vector(cvar_type type)
{
	// TODO: auto-generate this code
	return (type == cvar_type::CVT_INTV)
		|| (type == cvar_type::CVT_UNSIGNEDV)
		|| (type == cvar_type::CVT_FLOATV)
		;
}


/*
 * cpp_to_cvt - compile time conversions from C/C++ types to cvar_type
 */
template<typename T>
struct cpp_to_cvt {};

// the proper C++20 way below would be
//		template<> struct cpp_to_cvt<CPP_TYPE> {
// however, G++ Bug 85282 prevents this and requires the workaround that is
// implemented belo. See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85282
// for more information.
#define NCR_CPP_TO_CVT_SPECIALIZATION(CVT_TYPE, CPP_TYPE, TYPE_NAME, ...)\
	template<std::same_as<CPP_TYPE> T> struct cpp_to_cvt<T> { \
		static constexpr cvar_type value = cvar_type::CVT_TYPE; \
	};

NCR_CVAR_TYPE_LIST(NCR_CPP_TO_CVT_SPECIALIZATION)
NCR_CVAR_TYPE_ALIASES(NCR_CPP_TO_CVT_SPECIALIZATION)
NCR_CVAR_VECTOR_TYPE_LIST(NCR_CPP_TO_CVT_SPECIALIZATION)
#undef NCR_CPP_TO_CVT_SPECIALIZATION


template <cvar_type CVT, typename = void>
struct cvt_to_cpp {};

#define NCR_CVT_TO_CPP_SPECIALIZATION(CVT_TYPE, CPP_TYPE, ...) \
	template <cvar_type C> \
	struct cvt_to_cpp<C, typename std::enable_if<C == cvar_type::CVT_TYPE>::type> { \
		using type = CPP_TYPE; \
	}; \

NCR_CVAR_TYPE_LIST(NCR_CVT_TO_CPP_SPECIALIZATION)
// NCR_CVAR_TYPE_ALIASES(NCR_CVT_TO_CPP_SPECIALIZATION) -> this would only be const char *, keep it mapped to std::string
NCR_CVAR_VECTOR_TYPE_LIST(NCR_CVT_TO_CPP_SPECIALIZATION)
#undef NCR_CVT_TO_CPP_SPECIALIZATION


/*
 * status codes returned by cvar_* functions
 */
enum struct cvar_status : unsigned {
	success            = 0,
	is_nullptr         = 1,
	conversion_failure = 2,
	unknown_cvar_type  = 3,
	type_mismatch      = 4,
};


/*
 * struct cvar_value - configurable variable value container
 */
struct cvar_value
{
	std::string      s         = "";

	// The union does not require default initial value according to the
	// language standard. ("if it is a union, the first named member is
	// initialized (recursively) according to these rules, and any padding is
	// initialized to zero bits;")
	union {
		bool         b;
		char         c;
		int          i;
		unsigned int u;
		float        f;
		double       d;
	};


	template <typename T> inline T get_value() const;

	// again, due to G++ Bug 85282, the implementatino for get_value must be
	// outside of the scope of cvar_value. Thus, it's moved to below.
	// For mroe information, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85282
};

#define CVAR_VALUE_TEMPLATE_IMPL(CVT_TYPE, CPP_TYPE, NAME, FIELD) \
	\
	template<> \
	inline CPP_TYPE \
	cvar_value::get_value<CPP_TYPE>() const { \
		return this->FIELD; \
	} \
	\
	template<> \
	inline const CPP_TYPE& \
	cvar_value::get_value<const CPP_TYPE&>() const { \
		return this->FIELD; \
	}

NCR_CVAR_TYPE_LIST(CVAR_VALUE_TEMPLATE_IMPL)
#undef CVAR_VALUE_TEMPLATE_IMPL


/*
 * definition of a configuration variable
 */
struct cvar
{
	const std::string
		name;

	const cvar_type
		type;

	const bool
		is_vector;

	cvar_value
		default_value;

	cvar_value
		value;

	std::vector<cvar_value>
		default_vec;

	std::vector<cvar_value>
		vec;

	// TODO: hide constructor, make cvar_map a friend

	cvar(std::string _name, cvar_type _type)
		: name(_name)
		, type(_type)
		, is_vector(cvar_type_is_vector(type))
	{}

	// Note: currently deleted copy constructor (each cvar should be unique!)
	// TODO: deep copy
	cvar(const cvar &) = delete;


	// again, due to G++ Bug 85282, the implementations for set, get_value and
	// set_value must be outside of the scope of cvar. Thus, they're moved to
	// below. For mroe information, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85282

	template <typename T> inline T get_value() const;
	template <typename T> inline cvar_status set_default(T value);
	template <typename T> inline cvar_status set(T value);

#ifdef NCR_CVAR_ENABLE_HDF5
	// the following functions rely partially on the templates. They're also
	// moved outside of the class scope to prevent "specialization of XY after
	// instantiation" errors.
	inline HighFive::DataType create_highfive_datatype() const;
	inline void write_hdf5_attribute(HighFive::Group group) const;
#endif

	// shorthands
	#define BASIC_TYPE_AS_FN(_1, CPP_TYPE, NAME, ...) \
		inline CPP_TYPE as_##NAME();
	NCR_CVAR_TYPE_LIST(BASIC_TYPE_AS_FN)
	#undef BASIC_TYPE_AS_FN

	#define BASIC_TYPE_OP(_1, CPP_TYPE, NAME, ...) \
		inline cvar& operator>>(CPP_TYPE &target);
	NCR_CVAR_TYPE_LIST(BASIC_TYPE_OP)
	#undef BASIC_TYPE_OP
};


#define CVAR_VALUE_TEMPLATE_IMPL(CVT_TYPE, CPP_TYPE, NAME, FIELD) \
	template<> \
	inline CPP_TYPE \
	cvar::get_value<CPP_TYPE>() const { \
		if (this->type != cpp_to_cvt<CPP_TYPE>::value) { \
			log_warning("Accessing cvar \"", this->name, \
					"\", which is of type ", CVT_TYPE_NAMES[static_cast<unsigned>(this->type)] ,", as type ", \
					CVT_TYPE_NAMES[static_cast<unsigned>(cpp_to_cvt<CPP_TYPE>::value)], ".\n"); \
		} \
		return this->value.FIELD; \
	} \
	\
	template<> \
	inline const CPP_TYPE& \
	cvar::get_value<const CPP_TYPE&>() const { \
		if (this->type != cpp_to_cvt<CPP_TYPE>::value) { \
			log_warning("Accessing cvar \"", this->name, \
					"\", which is of type ", CVT_TYPE_NAMES[static_cast<unsigned>(this->type)] ,", as type ", \
					CVT_TYPE_NAMES[static_cast<unsigned>(cpp_to_cvt<CPP_TYPE>::value)], ".\n"); \
		} \
		return this->value.FIELD; \
	}

NCR_CVAR_TYPE_LIST(CVAR_VALUE_TEMPLATE_IMPL)
#undef CVAR_VALUE_TEMPLATE_IMPL


#define BASIC_TYPE_SET_DEFAULT(ITEM, CPP_TYPE, NAME, FIELD) \
	template <> \
	inline cvar_status \
	cvar::set_default<CPP_TYPE>(CPP_TYPE v)  \
	{ \
		if (this->type != cpp_to_cvt<CPP_TYPE>::value) { \
			log_error("Cannot assign ", #NAME, " to cvar \"", this->name, "\".\n"); \
			return cvar_status::type_mismatch; \
		} \
		this->default_value.FIELD = v; \
		return cvar_status::success; \
	}

#define VECTOR_SET_DEFAULT(ITEM, VECTOR_TYPE, NAME, FIELD, ...)\
	template <> \
	inline cvar_status \
	cvar::set_default<VECTOR_TYPE>(VECTOR_TYPE v)  \
	{ \
		if (this->type != cpp_to_cvt<VECTOR_TYPE>::value) { \
			log_error("Cannot assign ", #NAME, " to cvar \"", this->name, "\".\n"); \
			return cvar_status::type_mismatch; \
		} \
		this->default_vec.clear(); \
		for (size_t i = 0; i < v.size(); i++) { \
			cvar_value item = {.s = "", .b = false}; \
			item.FIELD = v[i]; \
			this->default_vec.push_back(item); \
		} \
		return cvar_status::success; \
	}

NCR_CVAR_TYPE_LIST(BASIC_TYPE_SET_DEFAULT)
NCR_CVAR_TYPE_ALIASES(BASIC_TYPE_SET_DEFAULT)
NCR_CVAR_VECTOR_TYPE_LIST(VECTOR_SET_DEFAULT)
#undef VECTOR_SET_DEFAULT
#undef BASIC_TYPE_SET_DEFAULT


#define BASIC_TYPE_SET_VALUE(ITEM, CPP_TYPE, NAME, FIELD)\
	template <> \
	inline cvar_status \
	cvar::set<CPP_TYPE>(CPP_TYPE v)  \
	{ \
		if (this->type != cpp_to_cvt<CPP_TYPE>::value) { \
			log_error("Cannot assign ", #NAME, " to cvar \"", this->name, "\".\n"); \
			return cvar_status::type_mismatch; \
		} \
		this->value.FIELD = v; \
		return cvar_status::success; \
	}

#define VECTOR_SET_VALUE(ITEM, VECTOR_TYPE, NAME, FIELD, ...)\
	template <> \
	inline cvar_status \
	cvar::set<VECTOR_TYPE>(VECTOR_TYPE v)  \
	{ \
		if (this->type != cpp_to_cvt<VECTOR_TYPE>::value) { \
			log_error("Cannot assign ", #NAME, " to cvar \"", this->name, "\".\n"); \
			return cvar_status::type_mismatch; \
		} \
		this->vec.clear(); \
		for (size_t i = 0; i < v.size(); i++) { \
			cvar_value item = {.s = "", .b = false}; \
			item.FIELD = v[i]; \
			this->vec.push_back(item); \
		} \
		return cvar_status::success; \
	}

NCR_CVAR_TYPE_LIST(BASIC_TYPE_SET_VALUE)
NCR_CVAR_TYPE_ALIASES(BASIC_TYPE_SET_VALUE)
NCR_CVAR_VECTOR_TYPE_LIST(VECTOR_SET_VALUE)
#undef VECTOR_SET_VALUE
#undef BASIC_TYPE_SET_VALUE



// shorthand implementations
#define BASIC_TYPE_AS_FN(_1, CPP_TYPE, NAME, ...) \
	inline CPP_TYPE cvar::as_##NAME()  \
	{ \
		return this->get_value<CPP_TYPE>(); \
	} \

NCR_CVAR_TYPE_LIST(BASIC_TYPE_AS_FN)
#undef BASIC_TYPE_AS_FN


#define BASIC_TYPE_OP(_1, CPP_TYPE, NAME, ...) \
	inline cvar& cvar::operator>>(CPP_TYPE &target) \
	{ \
		target = this->get_value<CPP_TYPE>(); \
		return *this; \
	}

NCR_CVAR_TYPE_LIST(BASIC_TYPE_OP)
#undef BASIC_TYPE_OP


#ifdef NCR_CVAR_ENABLE_HDF5
// convert CVAR to H5 Type
	#define CVAR_CPP_TYPE(CVT_TYPE, CPP_TYPE, ...) \
		case cvar_type::CVT_TYPE: { \
			return HighFive::create_datatype<CPP_TYPE>(); \
		}

	#define CVAR_VECTOR_CONTAINED_CPP_TYPE(CVT_TYPE, _1, _2, _3, CPP_TYPE) \
		case cvar_type::CVT_TYPE: { \
			return HighFive::create_datatype<CPP_TYPE>(); \
		}

	inline HighFive::DataType
	cvar::create_highfive_datatype() const
	{
		if (this->is_vector) {
			switch (this->type)
			{
				NCR_CVAR_VECTOR_TYPE_LIST(CVAR_VECTOR_CONTAINED_CPP_TYPE)
			default:
				// this should essentially lead to an exception in hdf5
				return HighFive::DataType();
			}
		}
		else {
			switch (this->type)
			{
				NCR_CVAR_TYPE_LIST(CVAR_CPP_TYPE)
			default:
				// this should essentially lead to an exception in hdf5
				return HighFive::DataType();
			}
		}
	}
	#undef CVAR_CPP_TYPE
	#undef CVAR_VECTOR_CONTAINED_CPP_TYPE


	// due to HighFive's lack of proper bool support, need to check for bool and
	// convert accordingly...
	#define NCR_CVAR_TO_HDF5_ATTRIBUTE(CVT_TYPE, CPP_TYPE, ...) \
		case cvar_type::CVT_TYPE: { \
			auto dtype = this->create_highfive_datatype(); \
			HighFive::Attribute attr = group.createAttribute(this->name, HighFive::DataSpace(HighFive::DataSpace::dataspace_scalar), dtype); \
			\
			if (this->type == CVT_BOOL) { \
				int value = this->get_value<bool>(); \
				attr.write(value); \
			} \
			else { \
				attr.write(this->get_value<CPP_TYPE>()); \
			} \
			break; \
		}

	#define NCR_CVAR_VECTOR_TO_HDF5_ATTRIBUTE(CVT_TYPE, _1, _2, _3, CPP_TYPE) \
		case cvar_type::CVT_TYPE: { \
			std::vector<CPP_TYPE> data; \
			for (auto &v: this->vec) \
				data.push_back(v.get_value<CPP_TYPE>()); \
			\
			HighFive::Attribute attr = group.createAttribute<CPP_TYPE>(this->name, HighFive::DataSpace::From(data)); \
			if (data.size() > 0) \
				attr.write(data); \
			break; \
		}

	inline void
	cvar::write_hdf5_attribute(HighFive::Group group) const
	{
		// delete the attribute if it exists to avoid clashes of the DataSpace
		if (group.hasAttribute(this->name))
			group.deleteAttribute(this->name);

		if (this->is_vector) {
			switch (this->type) {
				NCR_CVAR_VECTOR_TYPE_LIST(NCR_CVAR_VECTOR_TO_HDF5_ATTRIBUTE)

			default:
				log_error("write_hdf5_attribute not implemented for type ", CVT_TYPE_NAMES[static_cast<unsigned>(this->type)], ".\n");
				break;
			}
		}
		else {

			// cannot use X-Macro because HighFive doesn't support bool (which
			// is checked during compile time)... fuckers.

			auto dtype = this->create_highfive_datatype();
			HighFive::Attribute attr = group.createAttribute(this->name, HighFive::DataSpace(HighFive::DataSpace::dataspace_scalar), dtype);

			if (this->type == cvar_type::CVT_BOOL) {
				int val = this->get_value<bool>();
				attr.write(val);
			}
			else {
				switch (this->type) {
				case cvar_type::CVT_CHAR:
					attr.write(this->value.c);
					break;

				case cvar_type::CVT_INT:
					attr.write(this->value.i);
					break;

				case cvar_type::CVT_UNSIGNED:
					attr.write(this->value.u);
					break;

				case cvar_type::CVT_FLOAT:
					attr.write(this->value.f);
					break;

				case cvar_type::CVT_DOUBLE:
					attr.write(this->value.d);
					break;

				case cvar_type::CVT_STRING:
					attr.write(this->value.s);
					break;

				default:
					log_error("write_hdf5_attribute not implemented for type ", CVT_TYPE_NAMES[static_cast<unsigned>(this->type)], ".\n");
					break;
				}
			}
		}
	}
	#undef NCR_CVAR_VECTOR_TO_HDF5_ATTRIBUTE
	#undef NCR_CVAR_TO_HDF5_ATTRIBUTE
#endif



inline bool
is_vector(const cvar *cvar)
{
	return cvar->is_vector;
}

inline bool
cvar_is_vector_type(const cvar *cvar)
{
	return is_vector(cvar);
}

inline bool
cvar_is_vector(const cvar *cvar)
{
	return is_vector(cvar);
}

inline bool
is_vector_type(const cvar *cvar)
{
	return is_vector(cvar);
}


/*
 * _cvar_value - access the value of a cvar given type T
 *
 */
template <typename T> inline T value(const cvar *cvar);
template <typename T> inline T value(const cvar_value *cvar);

#define CVAR_VALUE_TEMPLATE_IMPL(CVT_TYPE, CPP_TYPE, NAME, FIELD) \
	\
	template<> \
	inline CPP_TYPE \
	value<CPP_TYPE>(const cvar_value *val) { \
		return val->get_value<CPP_TYPE>(); \
	} \
	\
	template<> \
	inline const CPP_TYPE& \
	value<const CPP_TYPE&>(const cvar_value *val) { \
		return val->get_value<const CPP_TYPE&>();\
	}\
	\
	template<> \
	inline CPP_TYPE \
	value<CPP_TYPE>(const cvar *cvar) { \
		return cvar->get_value<CPP_TYPE>(); \
	} \
	\
	template<> \
	inline const CPP_TYPE& \
	value<const CPP_TYPE&>(const cvar *cvar) { \
		return cvar->get_value<const CPP_TYPE&>(); \
	}\


NCR_CVAR_TYPE_LIST(CVAR_VALUE_TEMPLATE_IMPL)
#undef CVAR_VALUE_TEMPLATE_IMPL



/*
 * cvar_parse - parse a string into a given cvar
 *
 * In case the conversion from std::string to the cvar's type fails, the value
 * of the cvar will be left as is.
 *
 */
inline cvar_status
parse(const std::string &str, cvar *cvar)
{
	if (!cvar)
		return cvar_status::is_nullptr;

	if (cvar->is_vector) {
		log_error("Attempting to parse string data into vector cvar \"", cvar->name, "\".\n");
		return cvar_status::type_mismatch;
	}

	// for each case in the list of types, try to convert it and check if the
	// returned std::optional actually has a value or not. If not, don't set it
	// and leave the cvar's value as is.
	#define CVAR_PARSE_STR(CVT_TYPE, CPP_TYPE, _2, FIELD) \
		case cvar_type::CVT_TYPE: {  \
			auto tmp = str_to_type<CPP_TYPE>(str); \
			if (!tmp.has_value()) { \
				log_error("Conversion from string \"", str, "\" to ", \
						CVT_TYPE_NAMES[static_cast<unsigned>(cvar->type)], \
						" failed for cvar \"", cvar->name, "\".\n"); \
				return cvar_status::conversion_failure; \
			} \
			else { \
				log_verbose("Setting cvar \"", cvar->name, "\" to ", tmp.value(), ".\n"); \
				cvar->value.FIELD = tmp.value(); \
			}\
		} \
		break;

	switch (cvar->type) {
		NCR_CVAR_TYPE_LIST(CVAR_PARSE_STR)

		default:
			log_error("Unknown cvar type ", static_cast<unsigned>(cvar->type), " for cvar \"", cvar->name, "\".\n");
			return cvar_status::unknown_cvar_type;
	}

	#undef CVAR_PARSE_STR
	return cvar_status::success;
}


inline cvar_status
cvar_parse(const std::string &str, cvar *cvar)
{
	return parse(str, cvar);
}


inline cvar_status
parsev(
		std::vector<std::string>::iterator begin,
		std::vector<std::string>::iterator end,
		cvar *cvar)
{
	if (!cvar)
		return cvar_status::is_nullptr;

	if (!cvar->is_vector) {
		log_error("Attempting to parse vector data into non-vector cvar \"", cvar->name, "\".\n");
		return cvar_status::type_mismatch;
	}

	#define CVAR_PARSE_STR_VECTOR(CVT_TYPE, CPP_TYPE, _2, FIELD, VALUE_TYPE)\
		case cvar_type::CVT_TYPE: { \
			while (begin != end) { \
				auto tmp = str_to_type<VALUE_TYPE>(*begin); \
				if (!tmp.has_value()) { \
					log_error("Conversion from string \"", *begin, "\" to ", \
							CVT_TYPE_NAMES[static_cast<unsigned>(cvar->type)], \
							" failed for cvar \"", cvar->name, "\".\n"); \
					return cvar_status::conversion_failure; \
				} \
				else { \
					log_verbose("Pushing value ", tmp.value(), " to vector cvar \"", cvar->name,".\n"); \
					cvar_value item = {.s = "", .b = false}; \
					item.FIELD = tmp.value(); \
					cvar->vec.push_back(item); \
				} \
				++begin; \
			} \
		} \
		break;

	cvar->vec.clear();
	switch (cvar->type) {
		NCR_CVAR_VECTOR_TYPE_LIST(CVAR_PARSE_STR_VECTOR)

		default:
			log_error("Unknown cvar vector type ", static_cast<unsigned>(cvar->type), " for cvar \"", cvar->name, "\".\n");
			return cvar_status::unknown_cvar_type;
	}

	#undef CVAR_PARSE_STR_VECTOR
	return cvar_status::success;
}

inline cvar_status
cvar_parsev(
		std::vector<std::string>::iterator begin,
		std::vector<std::string>::iterator end,
		cvar *cvar)
{
	return parsev(begin, end, cvar);
}


inline std::optional<std::string>
cvar_vec_to_str(cvar *cvar)
{
	if (!cvar->is_vector) {
		log_warning("Attempted to call cvar_vec_to_str on non-vector cvar \"", cvar->name, "\".\n");
		return {};
	}

	// this uses a manual re-implementation of ncr_utils' __v_to_os -> maybe
	// base this and __v_to_os on a custom ForwardIterator
	#define CVAR_VEC_TO_STR(CVT_TYPE, _1, _2, FIELD, VALUE_TYPE) \
		case cvar_type::CVT_TYPE: { \
			std::ostringstream os; \
			size_t i = 0; \
			for (auto &v: cvar->vec) { \
				if (i > 0) os << " "; \
				os << v.FIELD; \
				i++; \
			} \
			return os.fail() ? std::nullopt : std::optional{os.str()}; \
		} \
		break;

	switch (cvar->type) {
		NCR_CVAR_VECTOR_TYPE_LIST(CVAR_VEC_TO_STR)

		default:
			log_error("Conversion failed from unknown CVAR TYPE ", static_cast<unsigned>(cvar->type), " to string.\n");
			return "";
	}
	#undef CVAR_VEC_TO_STR

	return {};
}


/*
 * cvar_to_str - convert the value of a cvar to a string representation
 */
inline std::string
cvar_to_str(cvar *cvar)
{
	if (cvar->is_vector) {
		auto tmp = cvar_vec_to_str(cvar);
		return tmp.has_value() ? tmp.value() : "";
	}
	else {
		#define CVAR_BASIC_TYPE_TO_STR(CVT_TYPE, CPP_TYPE, _2, FIELD) \
			case cvar_type::CVT_TYPE: { \
				auto tmp = type_to_str<CPP_TYPE>(cvar->value.FIELD); \
				if (!tmp.has_value()) { \
					log_error("Conversion failed from ", CVT_TYPE_NAMES[static_cast<unsigned>(cvar->type)], " to string.\n"); \
					return ""; \
				} \
				else { \
					return tmp.value(); \
				} \
			} \
			break;

		switch(cvar->type) {
			NCR_CVAR_TYPE_LIST(CVAR_BASIC_TYPE_TO_STR)

			default:
				log_error("Conversion failed from unknown CVAR TYPE ", static_cast<unsigned>(cvar->type), " to string.\n");
				return "";
		}

		#undef CVAR_BASIC_TYPE_TO_STR
		return "";
	}
}


/*
 * reset a cvar to its default value
 */
inline cvar_status
reset(cvar *cvar)
{
	#define CVAR_RESET_BASIC_TYPE_TO_DEFAULT(CVT_TYPE, _1, _2, FIELD) \
		case cvar_type::CVT_TYPE: \
			cvar->value.FIELD = cvar->default_value.FIELD; \
			break;

	#define CVAR_RESET_VECTOR_TO_DEFAULT(CVT_TYPE, _1, _2, FIELD, ...) \
		case cvar_type::CVT_TYPE: \
			cvar->vec = cvar->default_vec; \
			break;

	switch (cvar->type) {
		NCR_CVAR_TYPE_LIST(CVAR_RESET_BASIC_TYPE_TO_DEFAULT)
		NCR_CVAR_VECTOR_TYPE_LIST(CVAR_RESET_VECTOR_TO_DEFAULT)

	default:
			log_error("Failed to clear unknown CVAR TYPE ", static_cast<unsigned>(cvar->type), ".\n");
			return cvar_status::unknown_cvar_type;
	}
	#undef CVAR_RESET_VECTOR_TO_DEFAULT
	#undef CVAR_RESET_BASIC_TYPE_TO_DEFAULT

	return cvar_status::success;
}


/*
 * cvar_reset - alias for reset
 */
inline cvar_status
cvar_reset(cvar *cvar)
{
	return reset(cvar);
}


/*
 * clear a vector-valued cvar. In case the cvar is not a vector variable, this
 * will lead to a warning
 */
inline cvar_status
clear(cvar *cvar)
{
	if (!cvar->is_vector) {
		log_warning("Attempted to call ncr::cvar_clear on non-vector cvar \"", cvar->name, "\".\n");
		return cvar_status::type_mismatch;
	}
	cvar->vec.clear();
	return cvar_status::success;
}


/*
 * cvar_clear - alias for clear
 */
inline cvar_status
cvar_clear(cvar *cvar)
{
	return clear(cvar);
}



/*
 * cvar_set_default - set the default variable of a cvar
 */
template <typename T>
inline cvar_status
cvar_set_default(cvar *cvar, T value);

#define BASIC_TYPE_SET_DEFAULT(ITEM, CPP_TYPE, NAME, FIELD) \
	template <> \
	inline cvar_status \
	cvar_set_default<CPP_TYPE>(cvar *cvar, CPP_TYPE value)  \
	{ \
		return cvar->set_default(value); \
	}

#define VECTOR_SET_DEFAULT(ITEM, CPP_TYPE, NAME, FIELD, ...)\
	template <> \
	inline cvar_status \
	cvar_set_default<CPP_TYPE>(cvar *cvar, CPP_TYPE value)  \
	{ \
		return cvar->set_default(value); \
	}

NCR_CVAR_TYPE_LIST(BASIC_TYPE_SET_DEFAULT)
NCR_CVAR_TYPE_ALIASES(BASIC_TYPE_SET_DEFAULT)
NCR_CVAR_VECTOR_TYPE_LIST(VECTOR_SET_DEFAULT)
#undef VECTOR_SET_DEFAULT
#undef BASIC_TYPE_SET_DEFAULT


/*
 * cvar_set - set the value of a cvar
 */
template <typename T>
inline cvar_status
cvar_set(cvar *cvar, T value);

#define BASIC_TYPE_SET_VALUE(ITEM, CPP_TYPE, NAME, FIELD)\
	template <> \
	inline cvar_status \
	cvar_set<CPP_TYPE>(cvar *cvar, CPP_TYPE value)  \
	{ \
		return cvar->set(value); \
	}

#define VECTOR_SET_VALUE(ITEM, CPP_TYPE, NAME, FIELD, ...)\
	template <> \
	inline cvar_status \
	cvar_set<CPP_TYPE>(cvar *cvar, CPP_TYPE value)  \
	{ \
		return cvar->set(value); \
	}

NCR_CVAR_TYPE_LIST(BASIC_TYPE_SET_VALUE)
NCR_CVAR_TYPE_ALIASES(BASIC_TYPE_SET_VALUE)
NCR_CVAR_VECTOR_TYPE_LIST(VECTOR_SET_VALUE)
#undef VECTOR_SET_VALUE
#undef BASIC_TYPE_SET_VALUE



/*
 * TODO: maybe rename to simply cvars_t
 * TODO: documentation
 */
struct cvar_map
{
	size_t size;
	size_t last_idx;
	cvar **map;

	cvar_map(size_t _size = 2048)
	: size(_size)
	, last_idx(0)
	, map(new cvar*[size])
	{
		for (size_t i = 0; i < this->last_idx; i++) {
			this->map[i] = nullptr;
		}
	}

	~cvar_map()
	{
		for (size_t i = 0; i < this->last_idx; i++) {
			delete map[i];
		}
		delete[] map;
	}

	cvar_map(const cvar_map &) = delete;

	inline cvar *
	get(std::string name)
	{
		// find the variable in the map and return a pointer to it
		for (size_t i = 0; i < this->last_idx; i++)
			if (this->map[i]->name == name)
				return this->map[i];
		return nullptr;
	}

	inline std::optional<cvar*>
	get_safe(std::string name)
	{
		cvar *cvar = this->get(name);
		return (cvar != nullptr) ? std::optional(cvar) : std::nullopt;
	}

	/*
	 * register_cvar - register a cvar, given its default value
	 */
	template <typename T>
	inline cvar*
	register_cvar(std::string name, T default_value)
	{
		for (size_t i = 0; i < this->last_idx; i++) {
			if (this->map[i]->name == name) {
				log_error("Duplicate cvar name \"", name, "\".\n");
				return nullptr;
			}
		}
		if (this->last_idx == this->size) {
			log_error("Exceeding cvar_map of size ", this->size, ".\n");
			return nullptr;
		}

		cvar *c = new cvar(name, cpp_to_cvt<T>::value);

		// TODO: move into cvar
		cvar_set_default(c, default_value);
		cvar_set(c, default_value);

		this->map[this->last_idx++] = c;
		return c;
	}

	/*
	 * write_to_file - write the cvar map to file
	 */
	inline void
	write_to_file(std::string filename) const
	{
		log_debug("Writing configuration to \"", filename, "\".\n");

		std::ofstream cfg_file;
		cfg_file.open(filename);
		for (size_t i = 0; i < this->last_idx; i++) {
			auto &cvar = this->map[i];
			auto str = cvar_to_str(cvar);
			if (cvar->type == cvar_type::CVT_STRING)
				str = "\"" + str + "\"";
			cfg_file << "set " << cvar->name << " " << str << ";\n";
		}
		cfg_file.close();
	}


#ifdef NCR_CVAR_ENABLE_HDF5
	inline void
	write_to_hdf5_attributes(HighFive::Group group) const
	{
		// TODO: write an "apply" or "map" method for cvars instead of having
		// this here and alos in write_to_file
		for (size_t i = 0; i < this->last_idx; i++) {
			auto &cvar = this->map[i];
			cvar->write_hdf5_attribute(group);
		}
	}
#endif
};


/*
 * Write a cvar map to a file
 */
inline void
write_cvars_to_file(const ncr::cvar_map &cvars, std::string filename)
{
	cvars.write_to_file(filename);
}

// The following functions are obsolete, use the inlined class methods instead

/*
 * cvar_get - get a pointer to a cvar.
 *
 * This function returns a nullptr in case the cvar is not found by name.
 */
inline cvar*
cvar_map_get(cvar_map &map, std::string name)
{
	return map.get(name);
}


/*
 * cvar_get_safe - get an optional to a pointer to cvar.
 *
 * In case the cvar was not found by name, then the returned value will be a
 * nullopt.
 */
inline std::optional<cvar*>
cvar_map_get_safe(cvar_map &map, std::string name)
{
	return map.get_safe(name);

}


template <typename T>
inline cvar*
cvar_map_register(cvar_map &map, std::string name, T default_value)
{
	return map.register_cvar<T>(name, default_value);
}


/*
 * Test if a cvar-vector contains a certain value
 */
template <typename T>
bool
cvar_vec_contains(cvar *cvar, T value);

#define CVAR_VEC_CONTAINS(ITEM, VECTOR_TYPE, _0, FIELD, VALUE_TYPE) \
	template <> \
	inline bool \
	cvar_vec_contains<VALUE_TYPE>(cvar *cvar, VALUE_TYPE value) \
	{ \
		if (!cvar->is_vector) { \
			log_warning("Attempting to access vector data of non-vector cvar \"", cvar->name, "\".\n"); \
			return false; \
		} \
		for (auto const &elem : cvar->vec) \
			if (elem.FIELD == value) \
				return true; \
		return false; \
	}

NCR_CVAR_VECTOR_TYPE_LIST(CVAR_VEC_CONTAINS)
#undef CVAR_VEC_CONTAINS


// TODO: maybe rename to cvar_typed_view_t to be consistent with most of the
// rest of the code base
template <typename T>
struct cvar_typed_view
{
	using view_type = cvar_typed_view<T>;

	cvar_typed_view() : _cvar(nullptr) {}
	cvar_typed_view(const cvar &cvar) : _cvar(&cvar) {}
	cvar_typed_view(const cvar *cvar) : _cvar(cvar) {}

	void set_cvar(cvar &cvar) { this->_cvar = &cvar; }
	void set_cvar(const cvar *cvar) { this->_cvar = cvar; }

	struct ConstIterator
	{
		using iterator_category = std::forward_iterator_tag;
		using difference_type   = std::ptrdiff_t;

		using value_type        = T;
		using pointer           = value_type const *;
		using reference         = value_type const &;

		ConstIterator(const cvar_value *ptr) : _ptr(ptr) {}

		reference operator*  () { return value<reference>(_ptr); }

		// TODO: the dereference operator is not correct, but it's also not used
		//       anywhere
		// pointer   operator-> () { return &cvar_value<pointer>(_ptr); }

		// prefix increment
		ConstIterator& operator++ ()       { _ptr++; return *this; }

		// postfix increment
		ConstIterator  operator++ (int)    { ConstIterator tmp(*this); ++*this; return tmp; }

		friend bool operator==(const ConstIterator& a, const ConstIterator &b)
		{
			return a._ptr == b._ptr;
		}

		friend bool operator!=(const ConstIterator& a, const ConstIterator &b)
		{
			return !(a == b);
		}

	private:
		const cvar_value *_ptr;
	};


	ConstIterator
	begin()
	{
		if (!_cvar->is_vector)
			return ConstIterator(&_cvar->value);
		return ConstIterator(&_cvar->vec[0]);
	}

	ConstIterator
	end()
	{
		if (!_cvar->is_vector)
			return ConstIterator(&_cvar->value + 1);
		return ConstIterator(&_cvar->vec[_cvar->vec.size()]);
	}

	template <typename U>
	friend constexpr auto operator|(const U &cvar, view_type &&view) {
		view.set_cvar(cvar);
		return view;
	}

	template <typename U>
	friend constexpr auto operator|(const U *cvar, view_type &&view) {
		view.set_cvar(cvar);
		return view;
	}

private:
	const cvar *_cvar;
};


#define VIEW_ALIAS(_0, CPP_TYPE, NAME, ...) \
	using cvar_##NAME##_view = cvar_typed_view<CPP_TYPE>;

// using cvar_int_view   = cvar_typed_view<int>;
// using cvar_float_view = cvar_typed_view<float>;

NCR_CVAR_TYPE_LIST(VIEW_ALIAS)
#undef VIEW_ALIAS


#undef NCR_CVAR_VECTOR_TYPE_LIST
#undef NCR_CVAR_TYPE_ALIASES
#undef NCR_CVAR_TYPE_LIST

} // namespace ncr::cvar
