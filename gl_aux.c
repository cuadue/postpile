#include "gl_aux.h"

#include <stdio.h>
#include <GL/glew.h>

void __check_gl_error(const char *file, int line)
{
    const char *err = "Unknown GL error";
    switch (glGetError()) {
    case GL_NO_ERROR: return;
    #define c(x) case (x): err = #x; break
    c(GL_INVALID_ENUM);
    c(GL_INVALID_VALUE);
    c(GL_INVALID_OPERATION);
    c(GL_INVALID_FRAMEBUFFER_OPERATION);
    c(GL_OUT_OF_MEMORY);
    c(GL_STACK_UNDERFLOW);
    c(GL_STACK_OVERFLOW);
    #undef c
    }
    fprintf(stderr, "%s:%d: %s\n", file, line, err);
}

