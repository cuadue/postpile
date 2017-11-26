#pragma once
#include <GL/glew.h>

GLuint compile_shader(const char *path);
GLuint load_program(const char *vert_file, const char *frag_file);
GLint get_uniform_location(GLuint program, const char *name);
GLint get_attrib_location(GLuint program, const char *name);
