#pragma once

#include "gl3.hpp"

struct Depthmap {
    void init(const char *vert_file, const char *frag_file);
    void begin();
    void render(const Drawlist &drawlist, glm::mat4 model);
    void resize_texture(int size);
    void shrink_texture();
    void grow_texture();

    GLuint program;

    glm::mat4 view_projection;
    UniformMat4 MVP;
    VertexAttribArray vertex;

    GLuint framebuffer;
    GLuint texture_target;
    int texture_size;
};

