/*
 * All hexes have flat-top orientation with unity radius.
 */ 

#include <assert.h>
#include <math.h>

#include "hex.h"

#ifndef M_PI
#define M_PI 3.1415926535897931159979634685441851615906
#endif
#define SQRT_3 1.7320508075688772

struct cube_coord { int x, y, z; };

/* Hex neighbors are sorted by corner number. This is the lazy way to do it.
 * Corners are labeled like this:
 *    4 ___ 5
 *   3 /   \ 0
 *     \___/
 *    2     1
 */
const struct hex_coord hex_neighbors[] = {
    {1, 0},  /* between corners 0 and 1 */
    {0, 1},  /* between corners 1 and 2 */
    {-1, 1}, /* between corners 2 and 3 */
    {-1, 0}, /* between corners 3 and 4 */
    {0, -1}, /* between corners 4 and 5 */
    {1, -1}  /* between corners 5 and 6 */
};

int mod(int a, int b)
{
    assert(b > 0);
    int ret = a % b;
    if (ret < 0) ret += b;
    assert(ret >= 0 && ret < b);
    return ret;
}

#define adjacent_hex(corner) (hex_neighbors[mod((corner), 6)])


struct hex_coord hex_add(struct hex_coord a, struct hex_coord b)
{
    struct hex_coord ret = {
        .q = a.q + b.q,
        .r = a.r + b.r
    };
    return ret;
}

struct cube_coord hex_to_cube(struct hex_coord h)
{
    struct cube_coord c = {
        .x = h.q,
        .y = h.r,
        .z = -h.q - h.r
    };
    assert(c.x + c.y + c.z == 0);
    return c;
}

struct point point_scale(struct point inp, double d)
{
    struct point p = { .x = inp.x * d, .y = inp.y * d };
    return p;
}

struct cube_coord cube_round(double x, double y, double z)
{
    int rx = lrint(x);
    int ry = lrint(y);
    int rz = lrint(z);

    double dx = fabs((double)rx - x);
    double dy = fabs((double)ry - y);
    double dz = fabs((double)rz - z);

    if (dx > dy && dx > dz) rx = -ry - rz;
    else if (dy > dz)       ry = -rx - rz;
    else                    rz = -rx - ry;

    struct cube_coord ret = { .x = rx, .y = ry, .z = rz };
    assert(ret.x + ret.y + ret.z == 0);
    return ret;
}

struct hex_coord cube_to_hex(struct cube_coord c)
{
    assert(c.x + c.y + c.z == 0);
    struct hex_coord h = {
        .q = c.x,
        .r = c.y
    };
    return h;
}

struct point hex_to_pixel(struct hex_coord h)
{
    struct point p = {
        .x = 3.0/2.0 * h.q,
        .y = SQRT_3 * ((double)h.r + h.q / 2.0)
    };
    return p;
}

struct hex_coord pixel_to_hex(double x, double y)
{
    double q = x * 2.0 / 3.0;
    double r = -x / 3.0 + y * SQRT_3 / 3.0;

    return cube_to_hex(cube_round(q, r, -q-r));
}
