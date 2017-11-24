#pragma once
#include <string>
#include <map>
#include <SDL.h>

#include "wavefront.hpp"
#include "renderer.hpp"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define check_gl_error() __check_gl_error(__FILE__, __LINE__)
void __check_gl_error(const char *, int);

struct gl2_material {
    gl2_material();
    gl2_material(
        const wf_material &,
        SDL_Surface *(*load_texture)(const char *));

    GLfloat shininess;
    GLfloat specular[4], diffuse[4];
    GLuint texture;
    void set_diffuse(const std::vector<GLfloat> &x);
};

struct Drawlist {
    const wf_mesh *mesh;
    glm::mat4 view_projection_matrix;

    struct Model {
        gl2_material *material;
        glm::mat4 model_matrix;
    };
    struct Group {
        const char *group_name;
        std::vector<Model> models;
    };
    // Map group name to a bunch of models drawing that group
    std::map<std::string, std::vector<Model>> groups;
};

void gl2_setup_mesh_data(const wf_mesh &mesh);
void gl2_setup_material(const gl2_material &mat);
void gl2_draw_mesh_group(const wf_mesh &, const std::string &);
void gl2_teardown_material();
void gl2_teardown_mesh_data();

void gl2_draw_mesh(const glm::mat4 &matrix, std::vector<wf_group> groups);
void gl2_draw_drawlist(const Drawlist& drawlist);

/*
struct gl2_renderer : public renderer {
    void set_projection_matrix(const glm::mat4&) override;
    glm::mat4 projection_matrix() const override;

    void set_modelview_matrix(const glm::mat4&) override;
    glm::mat4 modelview_matrix() const override;
};
*/
