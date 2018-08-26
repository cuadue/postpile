#pragma once

#include <string>
#include <map>
#include <vector>

#include <GL/glew.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
extern "C" {
#include <stdio.h>
#include "gl3_aux.h"
#include "gl_aux.h"
}

#include "wavefront.hpp"

template <typename T>
struct Uniform {
    Uniform() {location = UINT_MAX;}
    void init(GLuint program, const char *_name) {
        location = get_uniform_location(program, _name);
        name = _name;
    }
    const char *name;
    GLuint location;
    void set(const T &value) const;
};

typedef Uniform<glm::mat4> UniformMat4;
typedef Uniform<glm::mat3> UniformMat3;
typedef Uniform<std::vector<glm::vec3>> UniformVec3Vec;
typedef Uniform<int> UniformInt;
typedef Uniform<float> UniformFloat;

struct ArrayBufferBase {
    bool present = false;
    GLuint buffer;
};

template <typename T>
struct ArrayBuffer : ArrayBufferBase {
    void init(const std::vector<T> &data, bool _present)
    {
        present = _present;
        if (present) {
            glGenBuffers(1, &buffer);
            buffer_data_static(data);
        }
    }

    void buffer_data_static(const std::vector<T> &data)
    {
        buffer_data(data, GL_STATIC_DRAW);
    }

    void buffer_data_dynamic(const std::vector<T> &data)
    {
        buffer_data(data, GL_DYNAMIC_DRAW);
    }

    void buffer_data(const std::vector<T> &data, GLenum usage)
    {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(data[0]) * data.size(),
                     &data[0], usage);
    }
};

struct VertexAttribArray {
    void init(GLuint program, const char *name, int size);
    void activate(const ArrayBufferBase& ab) const;
    void disable(const ArrayBufferBase& ab) const;
    GLuint location;
    int size;
    bool instanced = false;
};

struct VertexAttribArrayMat4 {
    void init(GLuint program, const char *name);
    void activate(const ArrayBuffer<glm::mat4>& ab) const;
    void disable(const ArrayBuffer<glm::mat4>& ab) const;
    GLuint location0;
    VertexAttribArray attrib_array;
    bool instanced = false;
};

struct gl3_group {
    GLuint index_buffer;
    size_t count;
    void init(const wf_group &wf);
    void draw() const;
    void draw_instanced(int quantity) const;
};

struct gl3_material {
    gl3_material() {}
    explicit gl3_material(GLuint tex) : texture(tex) {}
    void init(const std::string &path);
    int activate(int index) const;
    GLuint texture = UINT_MAX;
    static gl3_material solid_color(glm::vec3 rgb);
};

struct VertexArrayObject {
    void init();
    void bind();
    void unbind();
    GLuint location;
};

struct gl3_mesh {
    ArrayBuffer<float> vertex_buffer;
    ArrayBuffer<float> normal_buffer;
    ArrayBuffer<float> uv_buffer;

    std::map<std::string, gl3_group> groups;
    void draw_group(const std::string &group) const;
    void draw_group_instanced(const std::string &group, int n) const;

    gl3_group all;
    void draw_all_instanced(int n) const;

    void init(const wf_mesh &wf);
};

struct Light {
    glm::vec3 direction, color;
};

struct Lights {
    std::vector<glm::vec3> direction;
    std::vector<glm::vec3> color;
    void put(const Light &l) {
        direction.push_back(glm::normalize(l.direction));
        color.push_back(l.color);
    }
};

struct Lightmap {
    GLuint framebuffer;
    GLuint texture;
};

#define CHECK_DRAWLIST(dl) _check_drawlist(dl, __FILE__, __LINE__)
