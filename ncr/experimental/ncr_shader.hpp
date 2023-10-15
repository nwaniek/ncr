/*
 * ncr_shader - An OpenGL shader interface
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <ncr/ncr_log.hpp>
#include <ncr/ncr_filesystem.hpp>

#include <ncr/experimental/ncr_glutils.hpp>

namespace ncr {


/*
 * struct to help work with shaders
 */
struct shader
{
	std::map<std::string, GLint> uniforms;

	std::vector<std::string> vert_src;
	std::vector<std::string> frag_src;

	std::vector<GLuint> vert_shdr;
	std::vector<GLuint> frag_shdr;

	GLuint program;

	std::string name;
	bool is_baked;


	shader(std::string _name)
	: name(_name), is_baked(false)
	{}


	~shader()
	{
		glDeleteProgram(this->program);
		for (auto vert: this->vert_shdr)
			glDeleteShader(vert);
		for (auto frag: this->frag_shdr)
			glDeleteShader(frag);
	}

	void
	add_source(std::string src, GLuint shader_type)
	{
		switch (shader_type) {
		case GL_VERTEX_SHADER:
			this->vert_src.push_back(src);
			break;
		case GL_FRAGMENT_SHADER:
			this->frag_src.push_back(src);
			break;
		default:
			log_error("Cannot add shader source of unknown type ", shader_type, "\"");
		}
	}

	void
	load_source(std::string filename, GLuint shader_type)
	{
		std::string src;
		auto err = read_file(filename, src);
		if (err == filesystem_status::ErrorFileNotFound)
			log_error("Shader source file \"", filename, "\" not found.\n");
		else
			this->add_source(src, shader_type);
	}

	void
	compile()
	{
		// compile vertex shaders
		for (auto &src: this->vert_src) {
			GLuint shdr;
			compile_vertex_shader(shdr, src);
			this->vert_shdr.push_back(shdr);
		}

		for (auto &src: this->frag_src) {
			GLuint shdr;
			compile_fragment_shader(shdr, src);
			this->frag_shdr.push_back(shdr);
		}
	}

	bool
	link()
	{
		this->program = glCreateProgram();

		for (auto shdr: this->vert_shdr)
			glAttachShader(this->program, shdr);

		for (auto shdr: this->frag_shdr)
			glAttachShader(this->program, shdr);

		glLinkProgram(this->program);

		if (!gl_shader_is_linked(this->program)) {
			log_error("Could not link shader program \"", this->name, "\".\n");
			gl_program_print_log(this->program);
			return false;
		}

		return true;
	}


	inline bool
	bake()
	{
		this->is_baked = false;
		this->compile();
		this->is_baked = this->link();
		return this->is_baked;
	}


	void
	add_uniform(std::string unif)
	{
		this->uniforms[unif] = -1;
		if (this->is_baked)
			load_uniform_location(unif);
	}

	void
	load_uniform_location(std::string key)
	{
		GLint loc = glGetUniformLocation(this->program, key.c_str());
		if (loc < 0)
			log_warning("Could not retrieve location of uniform \"", key, "\" in shader \"", this->name, "\".\n");
		else
			this->uniforms[key] = loc;
	}


	void
	load_uniform_locations()
	{
		for (auto &[key, value] : this->uniforms)
			load_uniform_location(key);
	}


	void
	use()
	{
		glUseProgram(this->program);
	}


	template <typename T> void set(std::string, T);
	template <typename T> void set2(std::string, T, T);
	template <typename T> void set3(std::string, T, T, T);
	template <typename T> void set4(std::string, T, T, T, T);

#define UNIF_LIST(_)     \
	_(set, f,  float)    \
	_(set, i,  int)      \
	_(set, ui, unsigned)

#define X1(FN_NAME, POSTFIX, T) \
	template <> \
	void set<T>(std::string unif, T v) \
	{ \
		glUniform1 ## POSTFIX(this->uniforms[unif], v); \
	}

#define X2(FN_NAME, POSTFIX, T) \
	template <> \
	void set2<T>(std::string unif, T v1, T v2) \
	{ \
		glUniform2 ## POSTFIX(this->uniforms[unif], v1, v2); \
	}

#define X3(FN_NAME, POSTFIX, T) \
	template <> \
	void set3<T>(std::string unif, T v1, T v2, T v3) \
	{ \
		glUniform3 ## POSTFIX(this->uniforms[unif], v1, v2, v3); \
	}

#define X4(FN_NAME, POSTFIX, T) \
	template <> \
	void set4<T>(std::string unif, T v1, T v2, T v3, T v4) \
	{ \
		glUniform4 ## POSTFIX(this->uniforms[unif], v1, v2, v3, v4); \
	}

UNIF_LIST(X1)
UNIF_LIST(X2)
UNIF_LIST(X3)
UNIF_LIST(X4)

#undef X4
#undef X3
#undef X2
#undef X1
#undef UNIF_LIST


};

} // ncr::
