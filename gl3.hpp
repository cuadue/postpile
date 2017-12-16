#pragma once

#include <string>
#include <map>
#include <vector>

#include <SDL.h>
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
    ArrayBuffer() : present(false) {}
    ArrayBuffer(int size, const std::vector<float> &data, bool _present);
    bool present;
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
    explicit gl3_group(const wf_group &wf);
    void draw() const;
};

struct gl3_material {
    gl3_material();
    gl3_material(
        const wf_material &,
        SDL_Surface *(*load_texture)(const char *));

    GLfloat shininess;
    GLfloat specular[4], diffuse[4];
    GLuint texture;
    int index;
    void setup(const UniformInt& diffuse_map) const;
    void set_diffuse(const std::vector<GLfloat> &);
};

struct VertexArrayObject {
    VertexArrayObject();
    GLuint location;
};

struct gl3_mesh {
    VertexArrayObject vao;
    ArrayBuffer vertex_buffer;
    ArrayBuffer normal_buffer;
    ArrayBuffer uv_buffer;

    std::map<std::string, std::vector<gl3_group>> groups;

    gl3_mesh() {}
    explicit gl3_mesh(const wf_mesh &wf);
    void draw_group(const std::string &name) const;
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

    struct Model {
        const gl3_material *material;
        glm::mat4 model_matrix;
        float visibility;
    };
    struct Group {
        const char *group_name;
        std::vector<Model> models;
    };
    // Map group name to a bunch of models drawing that group
    std::map<std::string, std::vector<Model>> groups;
    Lights lights;
};
