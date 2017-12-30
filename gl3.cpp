#include <iostream>
#include "gl3.hpp"
extern "C" {
#include "gl3_aux.h"
#include "gl_aux.h"
#include "stb_image.h"
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

int gl3_material::activate(int index) const
{
    if (texture == UINT_MAX) {
        fprintf(stderr, "Uninitialized texture!\n");
        return texture;
    }

    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_2D, texture);
    check_gl_error();
    return index;
}

void gl3_group::init(const wf_group& wf)
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

void VertexArrayObject::init()
{
    glGenVertexArrays(1, &location);
    assert(location);
    glBindVertexArray(location);
    check_gl_error();
}

void ArrayBuffer::init(const std::vector<float> &data, bool _present)
{
    present = _present;
    if (present) {
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(data[0]) * data.size(),
                     &data[0], GL_STATIC_DRAW);
    }
}

void gl3_mesh::init(const wf_mesh& wf)
{
    vao.init();
    vertex_buffer.init(wf.vertex4, true);
    normal_buffer.init(wf.normal3, wf.has_normals());
    uv_buffer.init(wf.texture2, wf.has_texture_coords());

    assert(sizeof wf.vertex4[0] == 4);
    assert(sizeof wf.normal3[0] == 4);
    assert(sizeof wf.texture2[0] == 4);

    for (const auto &pair : wf.groups) {
        if (pair.second.size() != 1) {
            fprintf(stderr, "Not supporting Wavefront groups of size %lu\n",
                    pair.second.size());
            abort();
        }
        groups[pair.first].init(pair.second[0]);
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
    if (!groups.count(name)) {
        fprintf(stderr, "No such group: %s\n", name.c_str());
        abort();
    }
    groups.at(name).draw();
}

// Assumes data is 8 bit per pixel, RGBA
static GLuint load_texture_2d(uint8_t *data, int w, int h)
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

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    check_gl_error();
    glGenerateMipmap(GL_TEXTURE_2D);

    return ret;
}

static void checkerboard(uint8_t *p, int w, int h)
{
    int q = 0;
    int r = 255;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y+=2) {
            for (int k = 0; k < 4; k++) *p++ = q;
            for (int k = 0; k < 4; k++) *p++ = r;
        }
        int t = q; q = r; r = t;
    }
}

void gl3_material::init(const std::string &texture_file) {
    if (!texture_file.size()) return;

    int h = 0, w = 0, n = 0;
    uint8_t *data = stbi_load(texture_file.c_str(), &w, &h, &n, 4);
    if (data) {
        texture = load_texture_2d(data, w, h);
        stbi_image_free(data);
    }
    else {
        fprintf(stderr, "%s: %s\n", texture_file.c_str(),
            stbi_failure_reason());
        #define SIZ 16
        uint8_t buf[SIZ * SIZ * 4];
        checkerboard(buf, SIZ, SIZ);
        texture = load_texture_2d(buf, SIZ, SIZ);
        #undef SIZ
    }

}

void _check_drawlist(const Drawlist &dl, const char *f, int l)
{
    for (const Drawlist::Item &item : dl.items) {
        if (!dl.mesh->groups.count(item.group)) {
            fprintf(stderr, "%s:%d: No such group: %s\n",
                    f, l, item.group.c_str());
            abort();
        }
    }
}
