#pragma once

#include "gl3.hpp"

struct Depthmap {
    void init(const char *vert_file, const char *frag_file);
    void render(const Drawlist &drawlist, glm::mat4 model);

    GLuint program;

    UniformMat4 MVP;
    VertexAttribArray vertex;

    GLuint framebuffer;
    GLuint texture_target;
};

