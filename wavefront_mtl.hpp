#pragma once
#include <vector>
#include <map>
#include <string>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

struct wf_mtl_component {
    glm::vec4 color;
    std::string texture_file;
    void dump() const;
};

struct wf_material
{
    std::string name;
    wf_mtl_component ambient, diffuse, specular;
    float specular_exponent;
    void dump() const;
};

std::map<std::string, wf_material> wf_mtllib_from_file(const char *path);

