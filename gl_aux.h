#pragma once

#define check_gl_error() __check_gl_error(__FILE__, __LINE__)
void __check_gl_error(const char *, int);
