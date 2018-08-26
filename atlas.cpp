#include "atlas.hpp"

void TexAtlas::init(const char *img, const char *almanac)
{
    material.init(img);
    FILE *fp = fopen(almanac, "r");
    if (!fp) {
        perror(almanac);
        abort();
    }

    char buf[2048];
    while (fgets(buf, sizeof(buf), fp)) {
        float x, y;
        char filename[2048];
        if (sscanf(buf, "scale %d", &scale)) {
            continue;
        }
        else if (sscanf(buf, "%f %f %s", &x, &y, filename)) {
            offset[filename] = glm::vec2(x, y);
        }
    }
}
