#pragma once

#include <cassert>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.1415926535897931159979634685441851615906
#endif
#define SQRT_3 1.7320508075688772

template <typename T>
struct Point {
    T x, y;
};

template <typename T>
struct CubeCoord {
    T x, y, z;

    template <typename U>
    static CubeCoord<T> from(const CubeCoord<U>& other)
    {
        CubeCoord<T> ret = {
            .x = static_cast<T>(other.x),
            .y = static_cast<T>(other.y),
            .z = static_cast<T>(other.z)
        };
        return ret;
    }
};

template <typename T>
struct HexCoord {
    T q, r;

    template <typename U>
    static HexCoord<T> from(const HexCoord<U>& other)
    {
        HexCoord<T> ret = {
            .q = static_cast<T>(other.q),
            .r = static_cast<T>(other.r)
        };
        return ret;
    }
};

template <typename T>
HexCoord<T> to_hex_coord_f(const HexCoord<T>& h)
{
    HexCoord<T> ret = {
        .q = h.q,
        .r = h.r
    };
    return ret;
}

HexCoord<int> adjacent_hex(int yaw);

template <typename T>
HexCoord<T> hex_add(const HexCoord<T>& a, const HexCoord<T>& b)
{
    HexCoord<T> ret = {
        .q = a.q + b.q,
        .r = a.r + b.r
    };
    return ret;
}

template <typename T>
CubeCoord<T> hex_to_cube(const HexCoord<T>& h)
{
    CubeCoord<T> c = {
        .x = h.q,
        .y = h.r,
        .z = -h.q - h.r
    };
    assert(c.x + c.y + c.z == 0);
    return c;
}

template <typename T>
CubeCoord<int> cube_round(T x, T y, T z)
{
    int rx = lrint(x);
    int ry = lrint(y);
    int rz = lrint(z);

    T dx = fabs((double)rx - x);
    double dy = fabs((double)ry - y);
    double dz = fabs((double)rz - z);

    if (dx > dy && dx > dz) rx = -ry - rz;
    else if (dy > dz)       ry = -rx - rz;
    else                    rz = -rx - ry;

    CubeCoord<int> ret = { .x = rx, .y = ry, .z = rz };
    assert(ret.x + ret.y + ret.z == 0);
    return ret;
}

template <typename T>
HexCoord<T> cube_to_hex(const CubeCoord<T>& c)
{
    assert(abs(c.x + c.y + c.z) < 0.1);
    HexCoord<T> h = {
        .q = c.x,
        .r = c.y
    };
    return h;
}

template <typename T>
Point<double> hex_to_pixel(const HexCoord<T>& h)
{
    Point<double> p = {
        .x = 3.0/2.0 * h.q,
        .y = SQRT_3 * ((double)h.r + h.q / 2.0)
    };
    return p;
}

HexCoord<double> pixel_to_hex_double(double x, double y);

HexCoord<int> pixel_to_hex_int(double x, double y);

template <typename T>
T hex_distance(const HexCoord<T>& a, const HexCoord<T>& b)
{
    CubeCoord<T> ac = hex_to_cube(a);
    CubeCoord<T> bc = hex_to_cube(b);
    T dx = std::abs(ac.x - bc.x);
    T dy = std::abs(ac.y - bc.y);
    T dz = std::abs(ac.z - bc.z);
    T ret = std::max(dx, dy);
    return std::max(ret, dz);
}

