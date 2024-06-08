#include <optional>
#include <string>

#include <fstream>
#include <sstream>

#include "shared.hpp"

#define NCR_ENABLE_SHORT_NAMES

// ncr includes
#include <ncr/ncr_common.hpp>
#include <ncr/ncr_utils.hpp>
#include <ncr/ncr_log.hpp>
#include <ncr/ncr_cvar.hpp>
#include <ncr/ncr_cmd.hpp>
#include <ncr/ncr_automata.hpp>
#include <ncr/ncr_simulation.hpp>


// TODO: don't do this...
using namespace ncr;

// declare logging struct
namespace ncr {
	NCR_LOG_DECLARATION;
}


// default configuration to read on startup
#define DEFAULT_CONFIG_FILENAME "etc/commander.cfg"


void
setup_logging()
{
	NCR_LOG_INSTANCE.set_policy(new logger_policy_stdcout());
}


// read a string which contains a list of values into vectors


static ncr::cvar_map cvars;



struct mutation_modifier_linear_args {
	unsigned t_min;
	double ds;
};


struct mutation_modifier_random_args {
	unsigned t_min;
	double mean;
	double variance;
};


struct mutation_rate_dynamics_arguments
{
	union {
		mutation_modifier_linear_args linear;
		mutation_modifier_random_args random;
	};
};


struct mutation_rate_dynamics
{
	// baseline mutation rates, which might be modified due to some additional
	// function (the modifiers)
	ncr::mutation_rates baseline;

	// data for the mutation rate dynamics
	mutation_rate_dynamics_arguments args;

	// modifier function that takes baseline mutation rates and returns a
	// (temporary) object
	ncr::mutation_rates (*modifier_fn)(
			const ncr::iteration_state *,
			const ncr::mutation_rates&,
			const mutation_rate_dynamics_arguments&) = nullptr;

	// call operator to make things easier to use
	ncr::mutation_rates operator()(const ncr::iteration_state *iter_state)
	{
		if (!iter_state) return baseline;
		if (!modifier_fn) return baseline;
		return modifier_fn(iter_state, this->baseline, this->args);
	}
};




void
setup_cvars()
{
	using namespace ncr;

	// register all variables that are going to be used here
	cvars.register_cvar("e_reproduction_method", "top_50");
	cvars.register_cvar("w_readonly",            true);

	// additional cvars for testing purposes
	cvars.register_cvar("t_unsigned", 10u);
	cvars.register_cvar("t_string",   "hello");
	cvars.register_cvar("t_float",    10.f);
	cvars.register_cvar("t_double",   10.0);
	cvars.register_cvar("t_int",      1234);
	cvars.register_cvar("t_bool",     true);

	cvars.register_cvar("t_char",     'A');

	// vector variables
	cvars.register_cvar("s_vector",   std::vector<int>{71, 72, 73});


	// mutation rate and modifier function
	cvars.register_cvar("g_mutation_rate", 0.10);
	cvars.register_cvar("g_mutation_rate_modifier_fn", std::vector<std::string>{"null"});
}



void
_set_mutation_rate_modifier_fn(std::vector<std::string> args)
{
	std::cout << args[0] << "\n";
}



void
exec_set(std::vector<std::string> argv)
{
	// first argument is the cvar name, second is its value. First, try to find
	// the cvar in the list of cvars.
	auto *cvar = cvars.get(argv[0]);
	if (!cvar) {
		log_warning("Cannot set unknown cvar \"", argv[0], "\"\n");
		return;
	}

	if (ncr::cvar_is_vector_type(cvar)) {
		ncr::cvar_parsev(++argv.begin(), argv.end(), cvar);
	}
	else {
		// check number of arguments for scalar cvar
		if (argv.size() != 2) {
			log_error("Invalid number of arguments to \"set\" for cvar \"", cvar->name, "\".\n");
			return;
		}
		ncr::cvar_parse(argv[1], cvar);
	}
}



// set a function plus arguments. This will call into specific code for a
// variable so that the arguments can be parsed there
/*
void
exec_fset(std::vector<std::string> argv)
{
	auto *cvar = cvars.get(argv[0]);
	if (!cvar) {
		log_warning("Cannot set unknown cvar \"", argv[0], "\"\n");
		return;
	}

	std::map<std::string, void(*)(std::vector<std::string> args)>
	_fn_setters{
		{"g_mutation_rate_modifier_fn", _set_mutation_rate_modifier_fn},
	};

	if (!_fn_setters.contains(argv[0])) {
		log_warning("No set function defined for \"", argv[0], "\"\n");
		return;
	}
	_fn_setters[argv[0]](argv);
}
*/


void
setup_cmds(ncr::cmds &cmds)
{
	cmds.register_cmd("set",  exec_set);
	// cmds.register_cmd("fset", exec_fset);
}


/*
 * parse the mutation rate configuration
 */


void
test_cvar()
{
	// get a cvar by name
	auto local_var = cvars.get("w_readonly");

	// direct access
	std::cout << std::boolalpha << local_var->value.b << std::endl;
	// type safe variant
	std::cout << std::boolalpha << ncr::value<bool>(local_var) << std::endl;

	// try to get the value of local_var as integer
	std::cout << "As Int: "    << ncr::value<int>(local_var)    << std::endl;
	std::cout << "As Float: "  << ncr::value<float>(local_var)  << std::endl;
	std::cout << "As Double: " << value<double>(local_var) << std::endl;

	auto another_var = cvars.get("t_char");
	std::cout << "Char value: " << ncr::cvar_to_str(another_var) << std::endl;


	// std::cout << "get_integer
	std::cout << "as_integer(): " << local_var->as_integer() << std::endl;
	std::cout << "as_float():   " << local_var->as_float()   << std::endl;

	std::cout << "get_value():  " << local_var->get_value<int>() << std::endl;


	// some other modifier functions
	float val;
	*local_var >> val;
	std::cout << "operator>> " << val << "\n";
}


void
test_vector()
{
	auto cvar = cvars.get("s_vector");
	std::cout << "Vector value from cfg: " << ncr::cvar_to_str(cvar)  << std::endl;

	ncr::cvar_reset(cvar);
	std::cout << "Vector value after reset: " << ncr::cvar_to_str(cvar)  << std::endl;

	ncr::cvar_clear(cvar);
	std::cout << "Vector value after clear: " << ncr::cvar_to_str(cvar)  << std::endl;

	// reset again
	ncr::cvar_reset(cvar);
	std::cout << "Vector value after reset: " << ncr::cvar_to_str(cvar)  << std::endl;

	// directly access the vector value and do other stuff with it

	for (auto const &v : cvar | ncr::cvar_integer_view()) {
		std::cout << v << "\n";
	}


	/*
	auto &ref = ncr_cvar_vref<int>(cvar);
	for (size_t i = 0; i < ref.size(); i++) {
		std::cout << ref[i] << std::endl;
	}
	*/
}


ncr::mutation_rates
mutation_rate_modifier_linear(
		const ncr::iteration_state *iter_state,
		const ncr::mutation_rates &baseline,
		const mutation_rate_dynamics_arguments &args)
{
	// copy the baseline
	ncr::mutation_rates result = baseline;
	if (!iter_state || (args.linear.t_min > iter_state->ticks))
		return result;

	// then modify if the ticks exceed something use multiplication to avoid
	// accumulation of errors in floating points using incremental accumulator
	unsigned long dticks = (iter_state->ticks - args.linear.t_min);

	// compute the modification relative to the baseline
	double ds = static_cast<double>(dticks + 1) * args.linear.ds;

	// add the modifier to all values in the mutation_rates struct
	result.delete_state               += ds;
	result.create_state               += ds;
	result.modify_state_start         += ds;
	result.modify_state_accepting     += ds;
	result.drop_transition            += ds;
	result.spawn_transition           += ds;
	result.modify_transition_source   += ds;
	result.modify_transition_target   += ds;
	result.modify_transition_symbol   += ds;

	return result;
}


// TODO: make accessing vector elements simpler
const cvar_value&
cvar_vec_get_elem(const ncr::cvar *v, unsigned i)
{
	return v->vec[i];
}


ncr::cvar_status
parse_mutation_rate_modifier_linear(const ncr::cvar *cvar_fn, mutation_rate_dynamics &dynamics)
{
	int t;
	double ds;
	{
		auto tmp = ncr::str_to_type<int>(cvar_vec_get_elem(cvar_fn, 1).s);
		if (!tmp.has_value()) {
			log_error("Type conversion failed for an argument of g_mutationrate_modifier_fn\n");
			return ncr::cvar_status::type_mismatch;
		}
		t = tmp.value();
	}
	{
		auto tmp = ncr::str_to_type<double>(cvar_vec_get_elem(cvar_fn, 2).s);
		if (!tmp.has_value()) {
			log_error("Type conversion failed for an argument of g_mutationrate_modifier_fn\n");
			return ncr::cvar_status::type_mismatch;
		}
		ds = tmp.value();
	}
	std::cout << "Mutation rates: using linear increase starting at time " << t << " and with step size " << ds << ".\n";

	// set the modifier function and its arguments
	dynamics.modifier_fn = mutation_rate_modifier_linear;
	dynamics.args.linear.t_min = t;
	dynamics.args.linear.ds = ds;

	return ncr::cvar_status::success;
}


ncr::cvar_status
parse_mutation_rate_modifier(mutation_rate_dynamics &mr_dynamics)
{
	auto cvar_fn = cvars.get("g_mutation_rate_modifier_fn");

	if (!cvar_fn->vec.size()) {
		log_warning("Empty argument list for g_mutation_rate_modifier_fn\n");
		return ncr::cvar_status::insufficient_arguments;
	}

	// extract the name of the function we want to use
	std::string _fn_name = cvar_fn->vec[0].s;

	if (_fn_name == "null") {
		return ncr::cvar_status::success;
	}
	if (_fn_name == "linear_modifier") {
		return parse_mutation_rate_modifier_linear(cvar_fn, mr_dynamics);
	}
	log_warning("Unknown mutation rate modifier function \"", cvar_fn->vec[0].s, "\"\n");
	return ncr::cvar_status::conversion_failure;
}


void
test_modifier_function()
{
	// object. XXX: needs to go within bitsoup memory
	mutation_rate_dynamics mr_dynamics;

	// setup default genetic algorithms stuff
	auto g_mutation_rate = cvars.get("g_mutation_rate");
	double mr = value<double>(g_mutation_rate);

	// baseline mutation rates
	mr_dynamics.baseline.delete_state               = mr;
	mr_dynamics.baseline.create_state               = mr;
	mr_dynamics.baseline.modify_state_start         = mr;
	mr_dynamics.baseline.modify_state_accepting     = mr;
	mr_dynamics.baseline.drop_transition            = mr;
	mr_dynamics.baseline.spawn_transition           = mr;
	mr_dynamics.baseline.modify_transition_source   = mr;
	mr_dynamics.baseline.modify_transition_target   = mr;
	mr_dynamics.baseline.modify_transition_symbol   = mr;

	// parse the cvar configuration for the mutation rate modifier

	std::cout << "Baseline MR: " << mr << " (" << mr_dynamics.baseline.create_state << ")\n";
	parse_mutation_rate_modifier(mr_dynamics);

	// fake a simulation to see if the stuff works
	ncr::iteration_state iter_state = {.ticks = 0};
	for (uint64_t t = 0; t < 10; ++t) {
		iter_state.ticks = t;

		if (mr_dynamics.modifier_fn) {
			auto result = mr_dynamics(&iter_state);
			std::cout << "Tick " << t << ", MR = " << result.create_state << "\n";
		}
	}
}


void
test_advanced_commands()
{
	// std::string cmd = "set   /* this is a comment */    some_test (1 \"one;\") (2 \"two\");";
	std::string cmd = "set nested_tuple (1 (2 3) 4);";
	// std::string cmd = "set a 1 \"2\" 3;";

	// first compress the command
	ncr::cmd_compress(cmd);
	std::cout << "Compressed command" << cmd << "\n";

	// now tokenize
	std::vector<cmd_token> toks;
	if (ncr::cmd_tokenize(cmd, toks) != ncr::cmd_status::Success) {
		std::cout << "something went wrong while tokenizing\n";
		return;
	}

	for (auto &tok: toks) {
		std::cout << "  command " << tok.name << "\n";
		std::cout << "    arguments (" << tok.argv.size() << "):\n";
		size_t i = 0;
		for (auto &arg: tok.argv) {
			std::cout << "      arg " << i << ": '" << arg << "'\n";
			++i;
		}

		/*
		result = this->execute_token(cmd_tok);

		if (test(result & cmd_status::ErrorCommandNotFound))
			return result;
		*/
	}


}


int
main()
{
	ncr::cmds cmds;

	// set up all the stuff
	setup_logging();
	setup_cvars();
	setup_cmds(cmds);

	// execute the main file
	if (true) {
		cmd_execute_file(cmds, DEFAULT_CONFIG_FILENAME);

		std::cout << "------------------------------------\n";
		std::cout << "Standard CVAR test\n\n";
		test_cvar();
		std::cout << "------------------------------------\n";
		std::cout << "Vector test\n\n";
		test_vector();
		std::cout << "------------------------------------\n";
		std::cout << "Modifier function test\n\n";
		test_modifier_function();
	}

	if (false) {
		// more interesting stuff to do with the command parser
		//
		std::cout << "------------------------------------\n";
		std::cout << "Advanced Command Arguments\n\n";
		test_advanced_commands();
	}

	return 0;
}
