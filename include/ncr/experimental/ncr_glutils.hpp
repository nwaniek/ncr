/*
 * ncr_glutils - OpenGL utility functions
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 */

#pragma once

#include <string>
#include <ncr/ncr_log.hpp>

#include <GL/glew.h>
#include <GL/glext.h>
#include <GL/gl.h>

#define GL_CHECK_ERROR() gl_check_error(__LINE__, __FILE__, __FUNCTION__)

namespace ncr {


// TODO: prefix with
// TODO: provide short aliases without the  prefix


inline void
gl_check_error(int line, const char *file, const char *func) {
	GLuint error;
	while ((error = glGetError()))
		log_error("E: OpenGL Error %s %s %d, %s\n", file, func, line, gluErrorString(error));
}


/*
 * gl_shader_print_log - print the info log of an operation on a shader
 */
inline void
gl_shader_print_log(int obj) {
	int max_length;
	glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &max_length);
	char* const buf = new char[max_length + 1];
	for (int i = 0; i <= max_length; ++i)
		buf[i] = 0;

	int length;
	glGetShaderInfoLog(obj, max_length, &length, &buf[0]);
	log_error(buf, "\n");

	delete[] buf;
}


/*
 * gl_program_print_log - print the info log of a program
 */
inline void
gl_program_print_log(int obj) {
	int max_length;
	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &max_length);
	char* const buf = new char[max_length + 1];
	for (int i = 0; i <= max_length; ++i)
		buf[i] = 0;

	int length;
	glGetProgramInfoLog(obj, max_length, &length, &buf[0]);
	log_error(buf, "\n");

	delete[] buf;
}


/*
 * gl_shader_is_linked - test if a shader was linked successfully
 */
inline bool
gl_shader_is_linked(int obj) {
	int status;
	glGetProgramiv(obj, GL_LINK_STATUS, &status);
	return status == GL_TRUE;
}


/*
 * gl_shader_is_compiled - test if a shader was compiled successfully
 */
inline bool
gl_shader_is_compiled(int obj) {
	int status;
	glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
	return status == GL_TRUE;
}


/*
 * compile_shader - compile a shader of certain type given its source
 */
inline bool
compile_shader(const GLuint shader_type, GLuint &shader, std::string src)
{
	shader = glCreateShader(shader_type);
	const char *c_src = src.c_str();
	glShaderSource(shader, 1, &c_src, NULL);
	glCompileShader(shader);
	GL_CHECK_ERROR();
	if (!gl_shader_is_compiled(shader)) {
		log_error("Could not compile shader source.\n");
		gl_shader_print_log(shader);
		return false;
	}
	return true;
}


/*
 * compile_vertex_shader - compile a vertex shader given its source
 */
inline bool
compile_vertex_shader(GLuint &shader, std::string src)
{
	return compile_shader(GL_VERTEX_SHADER, shader, src);
}


/*
 * compile_fragment_shader - compile a fragment shader given its source
 */
inline bool
compile_fragment_shader(GLuint &shader, std::string src)
{
	return compile_shader(GL_FRAGMENT_SHADER, shader, src);
}


} // ncr::
