#pragma once
#include <string>
#include <map>

#include "wavefront.hpp"

template <typename Material>
struct Drawlist {
    const wf_mesh *mesh;
    glm::mat4 view_projection_matrix;

    struct Model {
        Material *material;
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

typedef Drawlist<gl2_material> DrawlistGl2;

