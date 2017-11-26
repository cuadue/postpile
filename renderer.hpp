#pragma once
#include <string>
#include <map>

#include "wavefront.hpp"

template <typename Renderer>
struct Drawlist {
    const typename Renderer::Mesh *mesh;
    glm::mat4 view_projection_matrix;

    struct Model {
        const typename Renderer::Material *material;
        glm::mat4 model_matrix;
    };
    struct Group {
        const char *group_name;
        std::vector<Model> models;
    };
    // Map group name to a bunch of models drawing that group
    std::map<std::string, std::vector<Model>> groups;
};

struct gl2_material;

struct GL2 {
    typedef wf_mesh Mesh;
    typedef gl2_material Material;
};

struct gl3_mesh;

struct GL3 {
    typedef gl3_mesh Mesh;
    typedef gl2_material Material;
};

typedef Drawlist<GL2> DrawlistGl2;
typedef Drawlist<GL3> DrawlistGL3;

