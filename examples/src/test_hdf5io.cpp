#include <string>
#include <vector>
#include <iostream>
#include <cstdarg>

#define NCR_ENABLE_LOG_LEVEL_VERBOSE
#ifndef NCR_CVAR_ENABLE_HDF5
#define NCR_CVAR_ENABLE_HDF5
#endif

#include <ncr/ncr_log.hpp>
#include <ncr/ncr_string.hpp>
#include <ncr/ncr_cvar.hpp>

#include <highfive/H5File.hpp>
#include <highfive/H5Attribute.hpp>
#include <highfive/H5DataSpace.hpp>
#include <highfive/H5DataSet.hpp>
#include <highfive/H5Group.hpp>

// easy interface
#include <highfive/H5Easy.hpp>

namespace ncr {
	NCR_LOG_DECLARATION(new logger_policy_stdcout());
}

// some struct to store tuple like information. However, this is typed, so it is
// perfectly reflected in a struct.
typedef struct {
	int index;
	float value;
} tuple_type;

// custom data types can be built using the CompoundType, and then registered
// via a certain macro as shown below. This is very cumbersome, and it's very
// likely possible to map this via an X Macro definition
HighFive::CompoundType create_compound_tuple_type() {
	return {{"index", HighFive::create_datatype<int>()},
		    {"value", HighFive::create_datatype<float>()}};
}
HIGHFIVE_REGISTER_TYPE(tuple_type, create_compound_tuple_type)


void basic_example_write_data()
{
	using namespace HighFive;

	// create a new file
	File file("new_file.h5", File::ReadWrite | File::Create | File::Truncate);

	// let's create a new group
	Group group = file.createGroup("/some/nested/group");

	// generate some data
	std::vector<int> data(50);
	int i = 0;
	for (auto &x : data)
		x = i++;

	// we can manually specify the dimensions of the data that is to be written,
	// or use DataSpace::From(data).
	// Here we specify this manually, namely a vector with 50 entries
	std::vector<size_t> dims = {50};

	// create a DataSet within the H5 file
	DataSet dataset = group.createDataSet<int>("dataset_one", DataSpace(dims));

	// write the vector of int to the dataset. This directly writes to the file.
	dataset.write(data);

	// but we can also write a specific entry in the entire dataset. Here, we
	// select the 13th entry and write the number 71.
	dataset.select(ElementSet{13}).write(71);

	// Note that the selection can also be multiple elements, but then the data
	// must match the number of elements selected
	std::vector<size_t> foo(5, 1);
	dataset.select(ElementSet{17,18,19,20,21}).write(foo);

	// XXX: we can not easily specify a "range" (or at least, I don't know
	// currently)

	// create an integer attribute on top of this dataset
	Attribute attr = dataset.createAttribute<int>("name_of_attribute", DataSpace(1,1));
	attr.write(71);

	// it is also possible to use H5Easy to "dump" attributes to a dataset, but
	// then care must be taken regarding groups!
	H5Easy::dumpAttribute(file, "/some/nested/group/dataset_one", "another_attribute", 17);

	// it is possible to directly overwrite an existing attribute. The DumpMode
	// can also be given in the first call to dump the attribute.
	H5Easy::dumpAttribute(file, "/some/nested/group/dataset_one", "another_attribute", 23, H5Easy::DumpMode::Overwrite);


	// we can also have datasets of strings
	std::vector<std::string> str_data{"hello", "world", "something", "else", "something", "new"};
	DataSet strs = group.createDataSet<std::string>("string_dataset", DataSpace::From(str_data));
	strs.write(str_data);


	// we can also create new data types that need to be "committed" to the file
	auto t1 = create_compound_tuple_type();
	t1.commit(file, "tuple_type");

	std::vector<tuple_type> tuple_data{{0, 1.7}, {1, 7.3}};
	DataSet tuple_dset = group.createDataSet("tuple_dataset", DataSpace::From(tuple_data), t1);
	tuple_dset.write(tuple_data);

	// it's possible to force write content of a file to disk.
	file.flush();
}


void basic_example_read_data()
{
	using namespace HighFive;

	// open the file
	File file("new_file.h5", File::ReadOnly);

	// open the group
	Group group = file.getGroup("/some/nested/group");

	// open the dataset
	DataSet dataset = group.getDataSet("dataset_one");

	// reading the data from a dataset is straightforward
	std::vector<int> result;
	dataset.read(result);
	{
		int i = 0;
		for (auto &x: result) {
			if (i++) std::cout << " ";
			std::cout << x;
		}
		std::cout << "\n";
	}

	// reading the string dataset
	auto strs = group.getDataSet("string_dataset");
	std::vector<std::string> str_result;
	strs.read(str_result);
	{
		int i = 0;
		for (auto &x: str_result) {
			if (i++) std::cout << " ";
			std::cout << x;
		}
		std::cout << "\n";
	}
}


void basic_example()
{
	try {
		basic_example_write_data();
		basic_example_read_data();
	}
	catch (HighFive::Exception &err) {
		std::cerr << err.what() << std::endl;
	}
}


/*
 * in bitsoup, we'll need the following layout and hierarchy:
 *
 * TODO: think about not writing the full strings, but only their number in the
 *       set of all possible strings. That is, strings of any finite language
 *       can be enumerated. Because we operate on finite strings, maybe do this.
 *
 *       For this to work nicely, we might want to compute the mapping for every
 *       string in advance (and store in a hash map? or directly store the
 *       string number [ID] in the string itself)
 *
 * /configuration
 *   store all cvars as attributes of this group
 *   This also allows to store lists (sometimes we have that in cvars)
 *
 * /bitsoup N
 *   /[iteration] I
 *      /statistics
 *         /population        -> all statistics of a population
 *         /strings           -> all statistics of the strings
 *      /data
 *         /strings           -> all strings (if written here, or maybe just the IDs)
 *         /tags              -> the binary tag for each string
 *
 *
 * /cross-bitsoup-statistics
 *   all data that we collect from "across" bitsoups
 */

struct iter_state_t{
	unsigned ticks;
};

struct bitsoup_t {
	size_t id;
};

struct bitsoup_memory_t{
	bitsoup_t *bitsoup;
};

// This is a writer, as it truncates any existing file with the same name.
//
// TODO: once there's support for <format> also in GCC's libstd++, use that
//       instead of the custom ncr::strformat implementation used below.
struct BitsoupH5Writer
{
	BitsoupH5Writer(std::string filename)
	: h5file(HighFive::File(filename, HighFive::File::ReadWrite |
				                      HighFive::File::Create |
									  HighFive::File::Truncate))
	, _filename{filename}
	{
		// using namespace HighFive;
		// h5file = File(filename, File::ReadWrite | File::Create | File::Truncate);
	};

	// TODO: don't give the function a group
	//
	// write all configuration variables
	void write_cvars(const ncr::cvar_map &cvars)
	{
		using namespace HighFive;
		const std::string group_name = "/configuration";

		// create group if it does not yet exist
		if (!h5file.exist(group_name))
			h5file.createGroup(group_name);

		Group group = h5file.getGroup(group_name);
		cvars.write_to_hdf5_attributes(group);
	};


	HighFive::Group get_group(
			const char *grpfmt,
			const iter_state_t &iter_state,
			const bitsoup_memory_t &mem)
	{
		using namespace HighFive;
		const std::string group_name = ncr::strformat(grpfmt, mem.bitsoup->id, iter_state.ticks);
		if (!h5file.exist(group_name))
			h5file.createGroup(group_name);
		return h5file.getGroup(group_name);
	}

	// write out all strings for an iteration
	void write_string_data(
			const iter_state_t &iter_state,
			const bitsoup_memory_t &mem)
	{
		using namespace HighFive;

		auto group = get_group(_grpfmtStringData, iter_state, mem);
	};


	// write population statistics for an iteration
	void write_population_statistics(
			const iter_state_t iter_state,
			const bitsoup_memory_t mem)
	{
		using namespace HighFive;

		auto group = get_group(_grpfmtPopulationStatistics, iter_state, mem);
	};


	// write string statistics for an iteration
	void write_string_statistics(
			const iter_state_t &iter_state,
			const bitsoup_memory_t &mem)
	{
		using namespace HighFive;

		auto group = get_group(_grpfmtStringStatistics, iter_state, mem);
	};

	// write the congruency maps
	void write_congruency_data(
			const iter_state_t &iter_state,
			const bitsoup_memory_t &mem)
	{
		using namespace HighFive;

		auto group = get_group(_grpfmtCongruencyData, iter_state, mem);
	};

	// write the acceptance maps
	void write_acceptance_data(
			const iter_state_t &iter_state,
			const bitsoup_memory_t &mem)
	{
		using namespace HighFive;

		auto group = get_group(_grpfmtAcceptanceData, iter_state, mem);
	};

	// write fitness information
	void write_fitness_data(
			const iter_state_t &iter_state,
			const bitsoup_memory_t &mem)
	{
		using namespace HighFive;

		auto group = get_group(_grpfmtFitnessData, iter_state, mem);
	};

public:
	HighFive::File h5file;

private:
	std::string _filename;

	// TODO: at the moment, each iteration is stored within its own group. Maybe
	//       change this to be a huge matrix of data. For this, maybe also
	//       consider using chunking in the HDF5 file. In any case, writing only
	//       parts of the data works using .select:
	//			dataset.select({x0, y0}, {x1, y1}).write(t1);

	const char *_grpfmtPopulationStatistics = "/bitsoup %u/iteration %u/statistics/population";
	const char *_grpfmtStringStatistics     = "/bitsoup %u/iteration %u/statistics/strings";
	const char *_grpfmtStringData           = "/bitsoup %u/iteration %u/data/strings";
	const char *_grpfmtCongruencyData       = "/bitsoup %u/iteration %u/data/congruency";
	const char *_grpfmtAcceptanceData       = "/bitsoup %u/iteration %u/data/acceptance";
	const char *_grpfmtFitnessData          = "/bitsoup %u/iteration %u/data/fitness";
};




// we have a map of cvars (actually something more like a vector)
static ncr::cvar_map cvars;

// entity settings within one biotope
static ncr::cvar *e_entity_count                   = nullptr;
static ncr::cvar *e_state_count                    = nullptr;
static ncr::cvar *e_reproduction_period            = nullptr;
static ncr::cvar *e_clamp_emission                 = nullptr;
static ncr::cvar *e_another_bool_test              = nullptr;
static ncr::cvar *e_random_float_vector            = nullptr;
static ncr::cvar *e_random_string                  = nullptr;
static ncr::cvar *w_bitsoup_string_regen_ticks     = nullptr;


void
bitsoup_example()
{
#define _r(cvar, ...) \
	cvar = cvar_map_register(cvars, #cvar, __VA_ARGS__)

	// entity settings
	_r(e_entity_count,                      100u);
	_r(e_state_count,                         3u);
	_r(e_reproduction_period,                25u);
	_r(e_clamp_emission,                    true);
	_r(e_another_bool_test,                false);
	_r(e_random_float_vector,        std::vector<float>{1.0, 1.1, 1.2, 1.3, 1.4});
	_r(e_random_string,            "hello world, how are you?");
	_r(w_bitsoup_string_regen_ticks,  std::vector<unsigned>{});
#undef _r


	// std::vector<std::string> args{"1", "2", "3", "4"};
	// cvar_parsev(w_bitsoup_string_regen_ticks, args.begin(), args.end());

	std::cout << "cvar type: " << ncr::CVT_TYPE_NAMES[static_cast<unsigned>(w_bitsoup_string_regen_ticks->type)] << "\n";
	std::cout << ncr::cvar_vec_to_str(w_bitsoup_string_regen_ticks).value() << "\n";

	using namespace HighFive;

	/*
	File file("bitsoup.h5", File::ReadWrite | File::Create | File::Truncate);

	// dedicated group for all configurations
	Group grpConfig = file.createGroup("/configuration");

	// each cvar will be stored as an attribute, explicit example: entity count
	Attribute attrEntityCount = grpConfig.createAttribute<int>("e_entity_count", DataSpace::dataspace_scalar);
	attrEntityCount.write(71);
	*/

	// Attribute attrEntityCount2 = grpConfig.createAttribute<int>("e_entity_count", DataSpace::dataspace_scalar);
	// attrEntityCount2.write(71);
	// however, this can be partially automated with cvar attributes.
	// Attribute attr = grpConfig.createAttribute<??>(cvar->name, cvar->is_vector ? something : DataSpace::dataspace_scalar);
	// For this to work we need a new macro (or template specialization) that turns a CVT into a type
	//

	// fake bitsoup, iteration state, and memory
	bitsoup_t bitsoup {.id = 7};
	bitsoup_memory_t mem {.bitsoup = &bitsoup};
	iter_state_t iter_state {.ticks = 123};

	BitsoupH5Writer h5writer{"bitsoup.h5"};
	h5writer.write_cvars(cvars);

	h5writer.write_population_statistics(iter_state, mem);
	h5writer.write_string_statistics(iter_state, mem);
	h5writer.write_string_data(iter_state, mem);
	h5writer.write_congruency_data(iter_state, mem);
	h5writer.write_acceptance_data(iter_state, mem);
	h5writer.write_fitness_data(iter_state, mem);
}


int
main(int, char *[])
{
	basic_example();
	bitsoup_example();
	return 0;
}
