#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include "gl3.hpp"

class LmDebug {
    GLuint program;

    UniformMat4 MVP;
    VertexAttribArray vertex;

    GLuint framebuffer;
    GLuint texture_target;
};

void LmDebug::init(const char *vert_file, const char *frag_file)
{
    program = load_program(vert_file, frag_file);

    vertex.init(program, "vertex", 4);
    MVP.init(program, "MVP");

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glGenTextures(1, &texture_target);
    glBindTexture(GL_TEXTURE_2D, texture_target);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 2048, 2048, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    switch (glCheckFramebufferStatus(framebuffer)) {
    #define c(e) case e:\
        fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, #e);\
        break;
    c(
    #undef c
    }
}

void LmDebug::draw(const Drawlist &drawlist)
{
    glUseProgram(program);

    glm::mat4 projection = glm::ortho<float>(-10, 10, -10, 10, -10, 20);
    glm::mat4 view = glm::lookAt(-light_direction, glm::vec3(0, 0, 0),
                                 glm::vec3(0, 1, 0));
    glm::mat4 mvp = projection * view;

    MVP.set(mvp);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                         texture_target, 0);
    glDrawBuffer(GL_NONE);

    drawlist.mesh->activate();
    vertex.activate(drawlist.mesh->vertex_buffer);

    for (const auto &pair : drawlist.groups) {
        for (const Drawlist::Model &model : pair.second) {
            MVP.set(view_projection * model->model_matrix);
            visibility.set(model->visibility);
            N.set(glm::mat3(1));
            drawlist.mesh->draw_group(group_name);
        }
    }

    vertex.disable(drawlist.mesh->vertex_buffer);
    check_gl_error();
}
