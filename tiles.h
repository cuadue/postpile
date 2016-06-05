typedef struct {
    float *harmonics;
    int num_harmonics;
    float feature_size;
    struct osn_context *osn;
    float _offset;
    float _gain;
    /* TODO
    float *_harmonic_ratios;
    int _num_harmonic_ratios;
    */
} tile_generator;

float tile_value(const tile_generator *gen, float x, float y);
void tiles_init(tile_generator *gen, int64_t seed);
