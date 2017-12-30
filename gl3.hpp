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

struct ArrayBuffer {
    void init(const std::vector<float> &data, bool _present);
    bool present = false;
    GLuint buffer;
};

struct VertexAttribArray {
    void init(GLuint program, const char *name, int size);
    void activate(const ArrayBuffer& ab) const;
    void disable(const ArrayBuffer& ab) const;
    GLuint location;
    int size;
};

struct gl3_group {
    GLuint index_buffer;
    size_t count;
    void init(const wf_group &wf);
    void draw() const;
};

struct gl3_material {
    gl3_material() {}
    explicit gl3_material(GLuint tex) : texture(tex) {}
    void init(const std::string &path);
    int activate(int index) const;
    GLuint texture = UINT_MAX;
};

struct VertexArrayObject {
    void init();
    GLuint location;
};

struct gl3_mesh {
    VertexArrayObject vao;
    ArrayBuffer vertex_buffer;
    ArrayBuffer normal_buffer;
    ArrayBuffer uv_buffer;

    std::map<std::string, gl3_group> groups;
    void draw_group(const std::string &group) const;

    void init(const wf_mesh &wf);
    void activate() const;
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

struct Drawlist {
    const gl3_mesh *mesh;
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 shadow_view_projection;

    GLuint depth_map;

    struct Item {
        const gl3_material *material;
        glm::mat4 model_matrix;
        float visibility;
        std::string group;
    };

    std::vector<Item> items;
    Lights lights;
};

void _check_drawlist(const Drawlist &, const char *, int);
#define CHECK_DRAWLIST(dl) _check_drawlist(dl, __FILE__, __LINE__)
