#include "intersect.hpp"

// https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm

#define EPSILON 1e-7

float ray_intersects_triangle(const Ray &ray, Triangle triangle)
{
    glm::vec3 edge1 = triangle.vertices[1] - triangle.vertices[0];
    glm::vec3 edge2 = triangle.vertices[2] - triangle.vertices[0];

    glm::vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);
    if (std::abs(a) < EPSILON) {
        return INFINITY;
    }

    float f = 1.0 / a;
    glm::vec3 s = ray.origin - triangle.vertices[0];
    float u = f * glm::dot(s, h);

    if (u < 0.0 || u > 1.0) {
        return INFINITY;
    }

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray.direction, q);

    if (v < 0.0 || u + v > 1.0) {
        return INFINITY;
    }

    float t = f * glm::dot(edge2, q);
    return std::abs(t) > EPSILON ? t : INFINITY;
}
