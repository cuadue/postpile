#pragma once

#include "gl3.hpp"
#include "atlas.hpp"

struct RenderPost {
    struct Setup {
        const gl3_mesh *mesh;
        const gl3_material *material;
        std::string group;
        float uv_scale;
    };

    struct Drawlist { 
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 shadow_view_projection;

        GLuint depth_map = UINT_MAX;

        struct Item {
            glm::vec2 uv_offset;
            glm::vec3 position;
            float visibility;
        };

        std::vector<Item> items;
        Lights lights;
    };

    void init(const struct Setup *setup);
    static void prep();
    void draw(const RenderPost::Drawlist &drawlist);

    VertexAttribArray vertex;
    VertexAttribArray normal;
    VertexAttribArray uv;
    VertexAttribArray position;
    VertexAttribArray visibility;
    VertexAttribArray uv_offset;

    UniformMat4 view_matrix;
    UniformMat4 projection_matrix;
    UniformMat4 shadow_view_projection_matrix;

    UniformInt diffuse_map;
    UniformInt shadow_map;
    UniformFloat shader_uv_scale;

    UniformInt num_lights;
    UniformVec3Vec light_vec;
    UniformVec3Vec light_color;

    ArrayBuffer<glm::vec3> position_buffer;
    ArrayBuffer<float> visibility_buffer;
    ArrayBuffer<glm::vec2> uv_offset_buffer;

    VertexArrayObject vao;
    size_t group_count;
    std::string group;
    float uv_scale;
};
