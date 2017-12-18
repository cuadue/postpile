#pragma once

#include "gl3.hpp"

struct LmDebug {
    void init(const char *vert_file, const char *frag_file);
    void draw(const Drawlist &drawlist, GLuint texture);

    GLuint program;
    VertexAttribArray vertex;
    VertexAttribArray uv;
    UniformInt texture_uniform;
};
