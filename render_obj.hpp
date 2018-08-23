#pragma once

#include "gl3.hpp"

extern "C" {
#include "gl_aux.h"
}

struct RenderObj {
    RenderObj();
    void init(const char *vert_file, const char *frag_file);
    void draw(const Drawlist &drawlist);

    GLuint program;

    VertexAttribArray vertex;
    VertexAttribArray normal;
    VertexAttribArray uv;

    UniformMat4 MVP;  // model view projection matrix
    UniformMat3 N;  // Normal matrix
    UniformMat4 shadow_MVP;

    UniformInt diffuse_map;
    UniformInt shadow_map;
    UniformFloat visibility;

    UniformInt num_lights;
    UniformVec3Vec light_vec;
    UniformVec3Vec light_color;
};
