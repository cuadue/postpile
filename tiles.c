/* Usage
float harmonics[] = { 4, 3, 1, 3, 2 };
tile_generator gen = {
    .feature_size = 80,
    .harmonics = harmonics,
    .num_harmonics = ARRAY_COUNT(harmonics),
    .num_tiles = strlen(tiles),
};

open_simplex_noise(time(NULL), &gen.osn);
cal_tiles(&gen, 1000, 1000);
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "osn.h"
#include "tiles.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define CLAMP(x, lil, big) MAX(MIN(big, x), lil)

static
float uncomp_noise(const tile_generator *gen, float x, float y)
{
    x /= gen->feature_size;
    y /= gen->feature_size;

    double divisor = 0;
    double ret = 0;
    for (int i = 0; i < gen->num_harmonics; i++) {
        double h = gen->harmonics[i];
        divisor += h;
        /* TODO harmonic ratios */
        int f = 2 << i;
        ret += open_simplex_noise2(gen->osn, x * f, y * f) * h;
    }
    return ret / divisor;
}

float tile_value(const tile_generator *gen, float x, float y)
{
    float a = uncomp_noise(gen, x, y);
    return (a - gen->_offset) * gen->_gain;
}

void tiles_init(tile_generator *gen, int64_t seed)
{
    open_simplex_noise(seed, &gen->osn);

    float max_value = -INFINITY;
    float min_value = INFINITY;

    gen->_gain = 1;
    gen->_offset = 0;

    for (int i = 0; i < 4 * gen->feature_size; i++) {
        for (int k = 0; k < 4 * gen->feature_size; k++) {
            float x = uncomp_noise(gen, i, k);
            max_value = MAX(x, max_value);
            min_value = MIN(x, min_value);
        }
    }

    gen->_offset = min_value;
    gen->_gain = 1.0 / (max_value - min_value);
}
