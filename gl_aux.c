#include "gl_aux.h"

#include <stdio.h>

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

void __check_gl_framebuffer(GLint framebuffer, const char *file, int line)
{
    const char *err = "Unknown";
    switch (glCheckFramebufferStatus(framebuffer)) {
    case GL_FRAMEBUFFER_COMPLETE: return;
    #define c(e) case e: err = #e;
    c(GL_FRAMEBUFFER_UNDEFINED);
    c(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
    c(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
    c(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
    c(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
    c(GL_FRAMEBUFFER_UNSUPPORTED);
    c(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
    c(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS);
    #undef c
    }

        fprintf(stderr, "%s:%d: Framebuffer: %s\n", file, line, err);
}
