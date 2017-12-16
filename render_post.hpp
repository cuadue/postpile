#pragma once

#include "gl3.hpp"

extern "C" {
#include "gl_aux.h"
}

struct RenderPost {
    RenderPost();
    void init(const char *vert_file, const char *frag_file);
    void draw(const Drawlist &drawlist);

    GLuint program;

    VertexAttribArray vertex;
    VertexAttribArray normal;
    VertexAttribArray uv;

    UniformMat4 MVP;  // model view projection matrix
    UniformMat3 N;    // normal matrix

    UniformInt diffuse_map;
    UniformFloat visibility;

    UniformInt num_lights;
    UniformVec3Vec light_vec;
    UniformVec3Vec light_color;
};
