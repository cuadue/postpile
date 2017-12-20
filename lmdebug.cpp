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
    program = load_program(vert_file, frag_file);
    vertex.init(program, "vertex", 4);
    uv.init(program, "vertex_uv", 2);
    texture_uniform.init(program, "samp");
}

void LmDebug::draw(const Drawlist &drawlist, GLuint texture)
{
    check_gl_error();
    glUseProgram(program);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawBuffer(GL_BACK);

    drawlist.mesh->activate();
    vertex.activate(drawlist.mesh->vertex_buffer);
    uv.activate(drawlist.mesh->uv_buffer);
    texture_uniform.set(0);

    for (const auto &pair : drawlist.groups) {
        for (const Drawlist::Model &model : pair.second) {
            (void)model;
            drawlist.mesh->draw_group(pair.first);
        }
    }

    vertex.disable(drawlist.mesh->vertex_buffer);
    uv.disable(drawlist.mesh->uv_buffer);

    glDisable(GL_BLEND);
    check_gl_error();
}
