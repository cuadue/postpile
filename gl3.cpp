#include <iostream>
#include "gl3.hpp"
extern "C" {
#include "gl3_aux.h"
#include "gl_aux.h"
}
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

using std::string;
using std::map;
using std::vector;
using glm::mat4;

template<>
void Uniform<glm::mat4>::set(const glm::mat4 &value) const
{
    assert(location != UINT_MAX);
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
    check_gl_error();
}

template<>
void Uniform<glm::mat3>::set(const glm::mat3 &value) const
{
    assert(location != UINT_MAX);
    glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
    check_gl_error();
}

template<>
void Uniform<std::vector<glm::vec3>>::set(const std::vector<glm::vec3> &value) const
{
    assert(location != UINT_MAX);
    glUniform3fv(location, value.size(), glm::value_ptr(value[0]));
    check_gl_error();
}

template<>
void Uniform<int>::set(const int &value) const
{
    assert(location != UINT_MAX);
    check_gl_error();
    glUniform1i(location, value);
    check_gl_error();
}

template<>
void Uniform<float>::set(const float &value) const
{
    assert(location != UINT_MAX);
    check_gl_error();
    glUniform1f(location, value);
    check_gl_error();
}

gl3_material::gl3_material() {}

void gl3_material::setup(const UniformInt& diffuse_map) const
{
    diffuse_map.set(index);
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_2D, texture);
    check_gl_error();
}

gl3_group::gl3_group(const wf_group& wf)
{
    assert(sizeof wf.triangle_indices[0] == 4);
    glGenBuffers(1, &index_buffer);
    count = wf.triangle_indices.size();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 count * 4,
                 &wf.triangle_indices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    check_gl_error();
}

void gl3_group::draw() const
{
    check_gl_error();
    assert(index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, NULL);
    check_gl_error();
}

VertexArrayObject::VertexArrayObject()
{
    glGenVertexArrays(1, &location);
    assert(location);
    glBindVertexArray(location);
    check_gl_error();
}

ArrayBuffer::ArrayBuffer(int size, const std::vector<float> &data, bool _present)
: present(_present)
{
    if (present) {
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER,
                     size * data.size(),
                     &data[0], GL_STATIC_DRAW);
    }
}

gl3_mesh::gl3_mesh(const wf_mesh& wf)
: vao()
, vertex_buffer(4, wf.vertex4, true)
, normal_buffer(3, wf.normal3, wf.has_normals())
, uv_buffer(2, wf.texture2, wf.has_texture_coords())
{
    assert(sizeof wf.vertex4[0] == 4);
    assert(sizeof wf.normal3[0] == 4);
    assert(sizeof wf.texture2[0] == 4);

    for (const auto &pair : wf.groups) {
        for (const wf_group &g : pair.second) {
            groups[pair.first].push_back(gl3_group(g));
        }
    }

    glBindVertexArray(0);
    check_gl_error();
}

void VertexAttribArray::init(GLuint program, const char *name, int _size)
{
    location = get_attrib_location(program, name);
    size = _size;
}

void VertexAttribArray::disable(const ArrayBuffer &ab) const
{
    if (!ab.present) return;
    check_gl_error();
    glDisableVertexAttribArray(location);
    check_gl_error();
}

void VertexAttribArray::activate(const ArrayBuffer &ab) const
{
    if (!ab.present) return;
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, ab.buffer);
    glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, 0, NULL);
    check_gl_error();
}

void gl3_mesh::activate() const
{
    glBindVertexArray(vao.location);
}

void gl3_mesh::draw_group(const string &name) const
{
    assert(groups.count(name));
    for (const gl3_group &group : groups.at(name)) {
        group.draw();
    }

    check_gl_error();
}

static GLuint load_texture_2d(SDL_Surface *surf)
{
    GLuint ret;
    glGenTextures(1, &ret);
    glBindTexture(GL_TEXTURE_2D, ret);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    check_gl_error();

    float aniso;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
    if (aniso > 0) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
    }
    else {
        fprintf(stderr, "Anisotropic filtering not supported\n");
    }

    SDL_Surface *converted = SDL_ConvertSurfaceFormat(
            surf, SDL_PIXELFORMAT_RGBA8888, 0);
    surf = NULL;

    if (converted) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h,
                     0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, converted->pixels);
        check_gl_error();
        SDL_FreeSurface(converted);
        converted = NULL;
    }
    else {
        fprintf(stderr, "Failed to convert texture: %s\n", SDL_GetError());
        ret = 0;
    }
    glGenerateMipmap(GL_TEXTURE_2D);

    return ret;
}

static int next_index = 0;
gl3_material::gl3_material(
    const wf_material &wf,
    SDL_Surface *(*load_texture)(const char *path))
{
    assert(load_texture);
    index = next_index++;

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
