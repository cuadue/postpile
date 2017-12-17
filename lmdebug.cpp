#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern "C" {
#include "gl_aux.h"
}
#include "gl3.hpp"
#include "lmdebug.hpp"

void LmDebug::init(const char *vert_file, const char *frag_file)
{
    check_gl_error();
    program = load_program(vert_file, frag_file);
    check_gl_error();

    vertex.init(program, "vertex", 4);
    MVP.init(program, "MVP");

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    check_gl_error();

    glGenTextures(1, &texture_target);
    glBindTexture(GL_TEXTURE_2D, texture_target);
    check_gl_error();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 2048, 2048, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    check_gl_error();
}

void LmDebug::draw(const Drawlist &drawlist)
{
    glUseProgram(program);
    check_gl_error();

    glm::vec3 light_direction = drawlist.lights.direction[0];
    check_gl_error();

    glm::mat4 projection = glm::ortho<float>(-10, 10, -10, 10, -10, 20);
    glm::mat4 view = glm::lookAt(-light_direction, glm::vec3(0, 0, 0),
                                 glm::vec3(0, 1, 0));
    glm::mat4 view_projection = projection * view;

    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                         texture_target, 0);
    check_gl_error();
    glDrawBuffer(GL_NONE);

    drawlist.mesh->activate();
    vertex.activate(drawlist.mesh->vertex_buffer);

    for (const auto &pair : drawlist.groups) {
        for (const Drawlist::Model &model : pair.second) {
            MVP.set(view_projection * model.model_matrix);
            drawlist.mesh->draw_group(pair.first);
        }
    }

    vertex.disable(drawlist.mesh->vertex_buffer);
    check_gl_error();
}
