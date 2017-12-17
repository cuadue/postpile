#pragma once

#include <GL/glew.h>

#define check_gl_error() __check_gl_error(__FILE__, __LINE__)
void __check_gl_error(const char *, int);

#define check_gl_framebuffer(fb) __check_gl_framebuffer(fb, __FILE__, __LINE__)
void __check_gl_framebuffer(int fb, const char *, int);
