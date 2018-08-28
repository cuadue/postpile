#pragma once

#include "gl3.hpp"
#include "atlas.hpp"

struct RenderPost {
    struct Setup {
        const gl3_mesh *mesh;
        const gl3_material *material;
        float uv_scale;
    };

    struct Drawlist { 
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 shadow_view_projection;

        GLuint depth_map = UINT_MAX;
        bool use_alpha = false;

        struct Item {
            glm::vec2 uv_offset;
            glm::mat4 model_matrix;
            float visibility;
        };

        std::map<std::string, std::vector<Item>> grouped_items;

        Lights lights;
    };

    void init(const Setup setup);
    void add_group(const char *name);
    static void prep();
    void draw(const RenderPost::Drawlist &drawlist);

private:
    struct PerGroupData {
        VertexArrayObject vao;
        const gl3_group *indices;
        size_t count;
    };

    GLuint program;

    VertexAttribArray vertex;
    VertexAttribArray normal;
    VertexAttribArray uv;
    VertexAttribArrayMat4 model_matrix;
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

    ArrayBuffer<glm::mat4> model_matrix_buffer;
    ArrayBuffer<float> visibility_buffer;
    ArrayBuffer<glm::vec2> uv_offset_buffer;

    size_t group_count;
    float uv_scale;
    std::map<std::string, PerGroupData> groups;
    const gl3_material *material;
};
