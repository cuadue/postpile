#pragma once

#include "gl3.hpp"

struct Framebuffer {
    GLuint framebuffer;
    GLuint texture_target;
    int texture_size;
    void clear();
    void bind();
    void resize_texture(int size);
};

struct Depthmap {
    struct Drawlist { 
        struct Item {
            glm::mat4 model_matrix;
        };

        glm::mat4 offset;
        Framebuffer fb;
        std::map<std::string, std::vector<Item>> grouped_items;
    };

    void init(const gl3_mesh *mesh);
    void clear();
    void render(const Drawlist &drawlist, Framebuffer fb);
    void resize_texture(int size);
    void shrink_texture();
    void grow_texture();

    GLuint program;

    glm::mat4 view_projection;
    UniformMat4 shader_view_projection;
    VertexAttribArray vertex;
    VertexAttribArrayMat4 model_matrix;

    VertexArrayObject vao;
    ArrayBuffer<glm::mat4> model_matrix_buffer;
};

