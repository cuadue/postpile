#pragma once
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include "wavefront.hpp"

struct Ray {
    glm::vec3 origin, direction;
};

int ray_intersects_triangle(const Ray &ray, const Triangle &triangle);
