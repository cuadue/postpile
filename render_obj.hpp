#pragma once

#include "gl3.hpp"

extern "C" {
#include "gl_aux.h"
}

struct RenderObj {
    struct Drawlist {
        struct Setup {
            const gl3_mesh *mesh;
            const gl3_material *material;
        };

        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 shadow_view_projection;

        GLuint depth_map = UINT_MAX;

        struct Item {
            glm::vec2 uv_offset;
            glm::mat4 model_matrix;
            float visibility;
            std::string group;
        };

        std::vector<Item> items;
        Lights lights;
    };

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

void _check_drawlist(const RenderObj::Drawlist &, const char *, int);
