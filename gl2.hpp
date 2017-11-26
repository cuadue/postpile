#pragma once
#include <string>
#include <SDL.h>

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

void gl2_draw_drawlist(const DrawlistGl2& drawlist);
void gl2_light();
