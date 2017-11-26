#pragma once
#include <SDL.h>
#include <GL/glew.h>

#include "renderer.hpp"
extern "C" {
#include "gl_aux.h"
}

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

void gl2_draw_drawlist(const DrawlistGl2& drawlist);
void gl2_light();
