#pragma once

#include <map>

#include "gl3.hpp"

struct TexAtlas {
    void init(const char *img, const char *almanac);
    int scale;
    gl3_material material;
    std::map<std::string, glm::vec2> offset;
};
