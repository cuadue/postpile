/* (C) 2016 Wes Waugh
 * GPLv3 License: respect Stallman because he is right.
 */
#include <map>
#include <tuple>
#include <vector>
#include <GL/glew.h>

extern "C" {
#include <SDL.h>
}

#include "gl2.hpp"

void __check_gl_error(const char *file, int line) {
    const char *err;
    switch (glGetError()) {
    case GL_NO_ERROR: return;
    #define c(x) case (x): err = #x; break
    c(GL_INVALID_ENUM);
    c(GL_INVALID_VALUE);
    c(GL_INVALID_OPERATION);
    c(GL_INVALID_FRAMEBUFFER_OPERATION);
    c(GL_OUT_OF_MEMORY);
    c(GL_STACK_UNDERFLOW);
    c(GL_STACK_OVERFLOW);
    default:
        puts("Unknown GL error\n");
        break;
    }
    fprintf(stderr, "%s:%d: %s\n", file, line, err);
}

using std::map;
using std::string;
using std::pair;
using std::vector;

static int has_texture_coords(const wf_mesh &mesh)
{
    return mesh.texture2.size() / 2 >= mesh.vertex4.size() / 4;
}

static int has_normals(const wf_mesh &mesh)
{
    return mesh.normal3.size() / 3 >= mesh.vertex4.size() / 4;
}

gl2_material::gl2_material()
{
    for (int i = 0; i < 4; i++) {
        diffuse[i] = 1;
    }
    for (int i = 0; i < 4; i++) {
        specular[i] = 0;
    }
    shininess = 0;
    texture = 0;
}

void gl2_material::set_diffuse(const std::vector<GLfloat> &x)
{
    for (int i = 0; i < 4; i++) diffuse[i] = x[i];
}

void gl2_setup_mesh_data(const wf_mesh &mesh)
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(4, GL_FLOAT, 0, &mesh.vertex4[0]);

    if (has_normals(mesh)) {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, 0, &mesh.normal3[0]);
        check_gl_error();
    }

    if (has_texture_coords(mesh)) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, &mesh.texture2[0]);
    }
    check_gl_error();
}

void gl2_teardown_mesh_data()
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    check_gl_error();
}

void gl2_setup_material(const gl2_material &mat)
{
    // stage 0: light texture
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, mat.texture);
    glClientActiveTexture(GL_TEXTURE0);
    glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, mat.diffuse);
    glColor4fv(mat.diffuse);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    // arg0 * arg2 + arg1 * (1-arg2)
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);

    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);

    glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_CONSTANT);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

    glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);

    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_ADD);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

    // stage 1: modulate
    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, mat.texture);
    glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, mat.diffuse);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);

    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PRIMARY_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);

    glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
}

void gl2_teardown_material()
{
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    check_gl_error();
}

void gl2_draw_mesh_group(const wf_mesh &mesh, const string &group_name)
{
    for (const wf_group &group : mesh.groups.at(group_name)) {
        int n = group.triangle_indices.size();
        const unsigned *p = &group.triangle_indices[0];
        glDrawElements(GL_TRIANGLES, n, GL_UNSIGNED_INT, p);
    }
}

GLuint load_texture_2d(SDL_Surface *surf)
{
    GLuint ret;
    glGenTextures(1, &ret);
    glBindTexture(GL_TEXTURE_2D, ret);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    check_gl_error();
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);

    if (GLEW_EXT_texture_filter_anisotropic) {
        float aniso = 0;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
    }
    else {
        fprintf(stderr, "No anisotropic filtering support\n");
    }

    SDL_Surface *converted = SDL_ConvertSurfaceFormat(
            surf, SDL_PIXELFORMAT_RGBA8888, 0);
    surf = NULL;

    if (converted) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h,
                     0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, converted->pixels);
        SDL_FreeSurface(converted);
        converted = NULL;
    }
    else {
        fprintf(stderr, "Failed to convert texture: %s\n", SDL_GetError());
        ret = 0;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return ret;
}

gl2_material::gl2_material(
    const wf_material &wf,
    SDL_Surface *(*load_texture)(const char *path))
{
    assert(load_texture);

    for (int i = 0; i < 4; i++) {
        diffuse[i] = wf.diffuse.color[i];
        specular[i] = wf.specular.color[i];
    }

    shininess = wf.specular_exponent;

    string texture_file = wf.diffuse.texture_file;

    if (texture_file.size()) {
        SDL_Surface *surf;
        if ((surf = load_texture(texture_file.c_str()))) {
            texture = load_texture_2d(surf);
            SDL_FreeSurface(surf);
        }
        else {
            perror(texture_file.c_str());
            texture = 0;
        }
    }
}
