#ifndef HEX_H
#define HEX_H

struct point { double x, y; };
struct hex_coord { int q, r; };

struct hex_coord hex_add(struct hex_coord a, struct hex_coord b);
struct point hex_to_pixel(struct hex_coord);
struct hex_coord pixel_to_hex(double x, double y);
int hex_distance(struct hex_coord a, struct hex_coord b);
struct hex_coord adjacent_hex(int yaw);

#endif
