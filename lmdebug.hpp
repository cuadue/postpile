#pragma once

#include "gl3.hpp"

struct LmDebug {
    void init(const char *vert_file, const char *frag_file);
    void draw(const Drawlist &drawlist);

    GLuint program;

    UniformMat4 MVP;
    VertexAttribArray vertex;

    GLuint framebuffer;
    GLuint texture_target;
};
