#pragma once
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include "fir_filter.hpp"

class Time {
    int time_of_day = 0;
    int day_of_year = 0;
    int year = 0;
    fir_filter<glm::vec2> filter;
    glm::vec2 vec() const;

public:
    explicit Time(const std::vector<float> coeffs)
        : filter(coeffs, vec())
        {}
    void advance_hour();
    void step();

    float fractional_day() const;
    float fractional_night() const;
    int whole_day() const;
    float fractional_year() const;
};
