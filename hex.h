#ifndef HEX_H
#define HEX_H

struct point { double x, y; };
struct hex_coord { int q, r; };

struct point hex_to_pixel(struct hex_coord);
struct point hex_to_display_pixel(struct hex_coord);
struct hex_coord pixel_to_hex(double x, double y);

#endif
