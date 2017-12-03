#include "hex.hpp"

#include <cassert>

/* Hex neighbors are sorted by edge. On a clock face, heading index 0 is noon, 
 * 1 is 2 o'clock, 2 is 4 o'clock, etc
 *    4 ___ 5
 *   3 /   \ 0
 *     \___/
 *    2     1
 */
static const HexCoord<int> hex_neighbors[] = {
    {0, 1},
    {1, 0},
    {1, -1},
    {0, -1},
    {-1, 0},
    {-1, 1},
};

static
int mod(int a, int b)
{
    assert(b > 0);
    int ret = a % b;
    if (ret < 0) ret += b;
    assert(ret >= 0 && ret < b);
    return ret;
}

HexCoord<int> adjacent_hex(int yaw)
{
    return hex_neighbors[mod(yaw, 6)];
}

HexCoord<int> pixel_to_hex_int(double x, double y)
{
    double q = x * 2.0 / 3.0;
    double r = -x / 3.0 + y * SQRT_3 / 3.0;

    return cube_to_hex<int>(
        cube_round<double>(q, r, -q-r));
}

HexCoord<double> pixel_to_hex_double(double x, double y)
{
    HexCoord<double> ret;
    ret.q = x * 2.0 / 3.0;
    ret.r = -x / 3.0 + y * SQRT_3 / 3.0;
    return ret;
}
