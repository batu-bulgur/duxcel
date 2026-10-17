#ifndef DUXCEL_BRAND_H
#define DUXCEL_BRAND_H
#include <raylib.h>
#include <stdbool.h>
/* Logical bounds plus framebuffer pixels per logical point. No allocation,
   texture creation, parsing or file I/O; shares generated geometry with assets. */
void brand_draw(Rectangle bounds,float pixel_ratio);
/* Optional antialiased display cache for the Settings identity. Load once with
   a graphics context; mipmaps follow framebuffer DPI without per-frame work. */
bool brand_init(void);
void brand_draw_cached(Rectangle bounds);
void brand_shutdown(void);
/* OS identity, decoded once after InitWindow; independent of the working directory. */
bool brand_install_icon(void);
#endif
