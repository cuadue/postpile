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
    VertexAttribArrayMat4 model_matrix;
    VertexAttribArray visibility;

    UniformMat4 view_matrix;
    UniformMat4 projection_matrix;
    UniformMat4 shadow_view_projection_matrix;

    UniformInt diffuse_map;
    UniformInt shadow_map;

    UniformInt num_lights;
    UniformVec3Vec light_vec;
    UniformVec3Vec light_color;

    ArrayBuffer<glm::mat4> model_matrix_buffer;
    ArrayBuffer<float> visibility_buffer;
};
