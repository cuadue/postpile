#pragma once
#include <string>
#include <map>
#include <SDL.h>

#include "wavefront.hpp"

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

void gl2_render(
    const wf_mesh &obj,
    const std::map<std::string, gl2_material> &materials);

void gl2_render_group(
    const wf_mesh &obj,
    const wf_group &group,
    const std::map<std::string, gl2_material> &materials);

void gl2_setup_mesh_data(const wf_mesh &mesh);
void gl2_setup_material(const gl2_material &mat);
void gl2_draw_mesh_group(const wf_mesh &, const std::string &);
void gl2_teardown_material();
void gl2_teardown_mesh_data();
