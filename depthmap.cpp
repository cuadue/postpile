#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern "C" {
#include "gl_aux.h"
}
#include "gl3.hpp"
#include "depthmap.hpp"

void Framebuffer::init()
{


void Framebuffer::clear()
{
    bind();
    glViewport(0, 0, texture_size, texture_size);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    unbind();
}

void Framebuffer::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
}

void Framebuffer::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Depthmap::init()
{
    check_gl_error();
    program = load_program("depthmap.vert", "depthmap.frag");

    vertex.init(program, "vertex", 4);
    shader_view_projection.init(program, "view_projection");
    model_matrix.init(program, "model_matrix");
    model_matrix.instanced = true;

    model_matrix_buffer.init({}, true);

    vao.init();
    vao.bind();
    vertex.point_to(drawlist.mesh->vertex_buffer);
    model_matrix.point_to(model_matrix_buffer);
    vao.unbind();
}

void Framebuffer::init()
{
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &texture_target);

    int size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
    resize_texture(size >> 2);
}

void Framebuffer::resize_texture(int size)
{
    int max_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    if (size > max_size) return;
    if (size <= 0) return;

    texture_size = size;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    check_gl_error();

    glDeleteTextures(1, &texture_target);
    glGenTextures(1, &texture_target);
    glBindTexture(GL_TEXTURE_2D, texture_target);
    check_gl_error();
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                         texture_target, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16,
                 texture_size, texture_size, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);

    check_gl_error();
    check_gl_framebuffer(framebuffer);
    check_gl_error();
    fprintf(stderr, "Shadow map texture size: %d\n", texture_size);
}

void Framebuffer::shrink_texture()
{
    resize_texture(texture_size / 2);
}

void Framebuffer::grow_texture()
{
    resize_texture(texture_size * 2);
}

void Depthmap::render(const Drawlist &drawlist)
{
    if (!drawlist.lights.direction.size()) return;

    glUseProgram(program);
    check_gl_error();
    glDrawBuffer(GL_NONE);
    drawlist.fb.bind();
    vao.bind();

    glm::vec3 light_direction = drawlist.lights.direction[0];
    check_gl_error();

    glm::mat4 projection = glm::ortho<float>(-20, 20, -20, 20, -20, 20);
    glm::mat4 view = glm::lookAt(light_direction, glm::vec3(0, 0, 0),
                                 glm::vec3(0, 1, 0));
    view_projection = projection * view * offset;
    shader_view_projection.set(view_projection);

    check_gl_error();

    std::map<std::string, std::vector<glm::mat4>> instances;

    for (const Drawlist::Item &item : drawlist.items) {
        instances[item.group].push_back(item.model_matrix);
    }

    for (const auto &pair : instances) {
        model_matrix_buffer.buffer_data_dynamic(pair.second);
        model_matrix.activate(model_matrix_buffer);
        drawlist.mesh->draw_group_instanced(pair.first, pair.second.size());

    }

    drawlist.fb.unbind();
    check_gl_error();
}
