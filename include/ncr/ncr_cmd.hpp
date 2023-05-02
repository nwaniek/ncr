/*
 * ncr_cmd - Utility structs and algorithms to build a command interface
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details.
 */
#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include <ncr/ncr_const.hpp>
// TODO: check if this include is really required (it only uses one function)
#include <ncr/ncr_filesystem.hpp>
#include <ncr/ncr_log.hpp>
#include <ncr/ncr_utils.hpp>

namespace ncr {


typedef void(*cmd_function_t)(std::vector<std::string> argv);

/*
 * cmd_item - a specific command
 */
struct cmd_item
{
	std::string        name;
	cmd_function_t function;
};


/*
 * cmd_token - Tokenized command returned by `cmd_tokenize` function.
 */
struct cmd_token {
	std::string name;
	std::vector<std::string> argv;
};

enum struct cmd_status : unsigned {
	Success,
	ErrorCommandNotFound,
	ErrorFileNotFound,
	ErrorTokenizerIncompleteString,
};
NCR_DEFINE_ENUM_FLAG_OPERATORS(cmd_status)

inline
bool
test(cmd_status s)
{
	return std::underlying_type<cmd_status>::type(s);
}




// forward declarations
inline void cmd_compress(std::string &str);
inline cmd_status cmd_tokenize(const std::string &str, std::vector<cmd_token> &cmd_toks);

/*
 * A list of commands
 */
struct cmds
{
	std::vector<cmd_item> commands;

	/*
	 * cmd_find - find a command in the list of commands
	 */
	inline cmd_item*
	find(const std::string name)
	{
		for (size_t i = 0; i < this->commands.size(); ++i)
			if (this->commands[i].name == name)
				return &this->commands[i];
		return nullptr;
	}

	/*
	 * register - register a new command by name and function
	 *
	 * Note that this does not check for duplicate names (yet).
	 *
	 * TODO: check for duplicate names, return error on duplicate
	 */
	inline cmd_status
	register_cmd(std::string cmd, cmd_function_t function)
	{
		this->commands.push_back({cmd, function});
		return cmd_status::Success;
	}

	/*
	 * cmd_execute_token - execute a command token
	 *
	 * A command token is a command that was parsed out of a string, e.g. using
	 * method `cmd_tokenize`.
	 */
	inline cmd_status
	execute_token(const cmd_token &cmd_tok)
	{
		// find the command in the command map
		auto cmd = this->find(cmd_tok.name);
		if (!cmd) {
			log_error("Unknown command \"", cmd_tok.name, "\".\n");
			return cmd_status::ErrorCommandNotFound;
		}
		cmd->function(cmd_tok.argv);
		return cmd_status::Success;
	}

	/*
	 * cmd_execute_string - execute a string
	 *
	 * This function compresses and then parses a string into command
	 * tokens, and finally executes the command tokens.
	 */
	inline cmd_status
	execute_string(std::string &str)
	{
		cmd_status result = cmd_status::Success;

		// remove all things we need to ignore
		cmd_compress(str);

		// extract command tokens from config_str
		std::vector<cmd_token> cmd_tokens;
		auto tmp = cmd_tokenize(str, cmd_tokens);
		if (tmp != cmd_status::Success)
			return tmp;

		// execute commands
		for (auto cmd_tok: cmd_tokens) {
			result = this->execute_token(cmd_tok);

			if (test(result & cmd_status::ErrorCommandNotFound))
				return result;
		}

		return result;
	}

	/*
	 * cmd_execute_file - execute an entire file
	 *
	 * This function loads and subsequently parses an entire file into command
	 * tokens and executes them.
	 */
	inline cmd_status
	execute_file(std::string filename)
	{
		// read file content into string
		std::string config_str;
		auto tmp = read_file(filename, config_str);
		if (test(tmp & filesystem_status::ErrorFileNotFound))
			return cmd_status::ErrorFileNotFound;

		// execute string from file
		return this->execute_string(config_str);
	}
};


/*
 * cmd_execute_token - execute a command token
 *
 * A command token is a command that was parsed out of a string, e.g. using
 * method `cmd_tokenize`.
 */
inline cmd_status
cmd_execute_token(cmds &commands, const cmd_token &cmd_tok)
{
	return commands.execute_token(cmd_tok);
}


/*
 * cmd_execute_string - execute a string
 *
 * This function compresses and then parses a string into command
 * tokens, and finally executes the command tokens.
 */
inline cmd_status
cmd_execute_string(cmds &commands, std::string &str)
{
	return commands.execute_string(str);
}


/*
 * cmd_execute_file - execute an entire file
 *
 * This function loads and subsequently parses an entire file into command
 * tokens and executes them.
 */
inline cmd_status
cmd_execute_file(cmds &commands, std::string filename)
{
	return commands.execute_file(filename);
}


/*
 * cmd_is_start_of_line_comment - check if at the start of a line comment
 */
inline bool
cmd_is_start_of_line_comment(const std::string str, const size_t offset)
{
	const size_t slen = str.length();
	return (offset < (slen - 1)) && (str[offset] == '/') && (str[offset+1] == '/');
}


/*
 * cmd_is_start_of_multiline_comment - check if at the start of a multi-line comment
 */
inline bool
cmd_is_start_of_multiline_comment(const std::string str, const size_t offset)
{
	const size_t slen = str.length();
	return (offset < (slen - 1)) && (str[offset] == '/') && (str[offset+1] == '*');
}


/*
 * cmd_is_end_of_multiline_comment - check if at the end of a multi-line comment
 */
inline bool
cmd_is_end_of_multiline_comment(const std::string str, const size_t offset)
{
	const size_t slen = str.length();
	return (offset < (slen - 1)) && (str[offset] == '*') && (str[offset+1] == '/');
}


/*
 * cmd_is_start_of_string - check if at the start of a string
 */
inline bool
cmd_is_start_of_string(const std::string str, const size_t offset)
{
	const size_t slen = str.length();
	return (offset < slen) && (str[offset] == '\"');
}


/*
 * cmd_is_end_of_string - check if at the end of a string.
 *
 * Note that this function will test for \" by reading one character *before*
 * offset. Thus, in principle, offset > 0 in general. if offset == 0, this
 * function returns false.
 */
inline bool
cmd_is_end_of_string(const std::string str, const size_t offset)
{
	const size_t slen = str.length();
	return (offset > 0) && (offset < slen) && ((str[offset] == '\"') && (str[offset - 1] != '\\'));
}


/*
 * cmd_is_whitespace - test if at whitespace
 */
inline bool
cmd_is_whitespace(const std::string str, const size_t offset)
{
	const size_t slen = str.length();
	return (offset < slen) && (str[offset] <= ' ');
}


inline bool
cmd_is_start_of_tuple(const std::string str, const size_t offset)
{
	const size_t slen = str.length();
	return (offset < slen) && (str[offset] == '(');
}


inline bool
cmd_is_end_of_tuple(const std::string str, const size_t offset)
{
	const size_t slen = str.length();
	return (offset < slen) && (str[offset] == ')');
}


/*
 * cmd_compress - inplace compress a string by removing comments and whitespace
 */
inline void
cmd_compress(std::string &str)
{
	const char delim = ';';
	const size_t slen = str.length();
	size_t dlen = 0;
	char ws = 0;

	size_t i = 0;
	while (i < slen) {

		// eat all ws
		if (cmd_is_whitespace(str, i)) {
			if (!ws)
				ws = ' ';
			while ((++i < slen) && cmd_is_whitespace(str, i));
		}

		// eat all // comments
		if (cmd_is_start_of_line_comment(str, i)) {
			i += 2;
			ws = ' ';
			while (++i < slen) {
				if (str[i] == '\n')
					break;
			}
			continue;
		}

		// eat all /* */ comments
		if (cmd_is_start_of_multiline_comment(str, i)) {
			// this implementation captures multi-line comments and replaces
			// them only with a regular whitespace, not a linebreak. This allows
			// to capture things like
			//
			//     set my_var /* this is a multi-line
			//     comment with the value on the second line */ 123.0
			//
			ws = ' ';
			i += 2;
			while (++i < slen) {
				if (cmd_is_end_of_multiline_comment(str, i)) {
					i += 2;
					break;
				}
			}
			continue;
		}

		// insert whitespace?
		if (ws) {
			// don't add whitespace at the beginning of the string or if the
			// previous entry was the delimiter
			if ((dlen > 0) && str[dlen-1] != delim)
				str[dlen++] = ws;
			ws = 0;
		}

		// handle "" quoted strings
		if (cmd_is_start_of_string(str, i)) {
			do {
				str[dlen++] = str[i];
			} while ((++i < slen) && !cmd_is_end_of_string(str, i));
		}

		// eating whitespace comments, and strings, above could have left us at
		// an end-of-string position
		if (i >= slen)
			break;

		// everything else
		do {
			str[dlen++] = str[i];
		} while ((++i < slen)
				&& !cmd_is_whitespace(str, i)
				&& !cmd_is_start_of_line_comment(str, i)
				&& !cmd_is_start_of_multiline_comment(str, i));
	}

	str.resize(dlen);
}




/*
 * cmd_tokenize - turn a (compressed) string into command tokens.
 *
 * A command is considered to be everything that is on one line, with the
 * command itself being the first token on such a line. Subsequent elements on
 * the line will be parsed into the argv field of cmd_token.
 *
 * More precisely, asssume the following line in `str`:
 *
 *    set some_variable 123.4 "hello world"
 *
 * This will extract the command name `set` with argv consisting of
 * `some_variable`, `123.4` and `hello world`. Note that for strings, the
 * surrounding quotes "" will be stripped.
 *
 * This function assumes that the string is already free of comments and
 * extraneous newlines, i.e. that compress was called on a raw input.
 *
 * NOTE: Currently, the function assumes that the string is a well formed
 *       command. Lines which are not well defined are parsed as-is.
 */
inline cmd_status
cmd_tokenize(const std::string &str, std::vector<cmd_token> &cmd_toks)
{
	// declare _min to avoid include <algorithm>
	#define _min(X, Y) ((X) < (Y) ? (X) : (Y))

	// length of the string
	const size_t slen = str.length();
	if (!slen) return {};

	// delimiter of a command
	const char delim = ';';

	bool is_cmd = true;
	size_t tok_start = 0;
	size_t tok_end = 0;

	// results
	// std::vector<cmd_token> cmd_toks;
	size_t n_cmd_toks = 0;

	while (tok_start < slen) {

		// skip over delimiters. This will also eat multiple ;;;;;
		if (str[tok_start] == delim) {
			log_verbose("eat delim\n");

			// what follows a delimiter is a command
			is_cmd = true;
			// jump over delimiter
			tok_end = ++tok_start;
			continue;
		}

		// ignore all whitespace which is not the delimiter
		if (cmd_is_whitespace(str, tok_start)) {
			log_verbose("eat whitespace\n");

			do {
				++tok_start;
				++tok_end; // TODO: no need to increment, will be set afterwards anyway, so maybe remove
				if (tok_start >= slen)
					break;
			} while (cmd_is_whitespace(str, tok_start) && (str[tok_start] != delim));
			tok_end = tok_start;
			continue;
		}

		// handle strings
		if (cmd_is_start_of_string(str, tok_start)) {
			log_verbose("handle string\n");

			// seek end of string (will read over \")
			tok_end = tok_start + 1;
			while (tok_end < slen) {
				if (cmd_is_end_of_string(str, tok_end))
					break;
				++tok_end;
			}

			// Detect malformed string (i.e. end of line reached, but not
			// end of string
			if ((tok_end >= slen) && !cmd_is_end_of_string(str, tok_end)) {
				log_verbose("Malformed input found while tokenizing, string did not end.\n");
				return cmd_status::ErrorTokenizerIncompleteString;
			}
			tok_start = _min(tok_start + 1, slen - 1);
			tok_end   = _min(tok_end, slen - 1);

			// Only insert if there's a command token, but add empty strings
			// as arguments as well.
			if (n_cmd_toks) {
				std::string arg = str.substr(tok_start, tok_end - tok_start);

				// parse out \" in-place
				size_t c = 0;
				size_t i = 0;
				size_t alen = arg.length();
				while (i < alen) {
					if ((i < (alen - 1)) && (arg[i] == '\\') && (arg[i+1] == '\"')) {
						arg[c++] = '\"';
						i += 2;
					}
					else
						arg[c++] = arg[i++];
				}
				arg.resize(c);

				// add as argument to argv
				cmd_toks[n_cmd_toks-1].argv.push_back(arg);
			}

			// jump over ", but make sure not to jump over end of line
			tok_end = _min(tok_end + 1, slen - 1);
			tok_start = tok_end;
			continue;
		}

		// handle everything else
		while (++tok_end < slen) {
			// if we're at a delimiter, we can break, but also for whitespace
			// XXX: maybe use a non-checking whitespace function here instead of
			//      manual test?
			if (str[tok_end] == delim || str[tok_end] == ' ')
				break;
		}

		if (tok_end > tok_start) {
			std::string arg = str.substr(tok_start, tok_end - tok_start);

			// if this is a command, create new cmd_token
			if (is_cmd) {
				cmd_toks.push_back({});
				cmd_toks[n_cmd_toks++].name = arg;
				is_cmd = false;
			}
			// if not, just push to argv
			else if (n_cmd_toks) {
				cmd_toks[n_cmd_toks-1].argv.push_back(arg);
			}
		}
		tok_start = tok_end;
	}

	return cmd_status::Success;
	#undef _min
}



} // ncr::
