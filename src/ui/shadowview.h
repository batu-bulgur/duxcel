#ifndef DUXCEL_SHADOWVIEW_H
#define DUXCEL_SHADOWVIEW_H
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

#define UI_SHADOW_MASK_LIMIT ((size_t)1024*1024)
typedef struct { Rectangle source,bounds; } UIShadowSlice;
typedef struct {
    Vector2 density;
    float scale;
    int width,corner_rows,atlas_width,atlas_height,table_width_x,table_width_y;
    size_t bytes;
    Rectangle top,body,bottom;
    bool valid;
} UIShadowLayout;
typedef struct { UIShadowSlice top,body,bottom; bool valid; } UIShadowPlacement;

/* Frozen narrow-wrapper shadow: rgba(39,61,79,.13), offset(-5,0), blur14,
   spread0, in UI units. Canonical atlas depends ONLY on UI scale and render
   density; wrapper position/height/scroll must not trigger a rebuild/upload.
   The atlas is RGBA8 PREMULTIPLIED, not straight-alpha. Point-sample at its
   native device resolution, stretching only the single body scanline. */
UIShadowLayout ui_shadow_layout(float scale,Vector2 density);
/* Full child/target rectangle, not its animated visible-width clip. Requires
   width>=42*scale and height>=two corner slices. The caller clips to workspace,
   paints behind the opaque wrapper, and suppresses wide/retained/exiting
   shadows. Positions are preserved; fractional GPU sampling is not simulated. */
UIShadowPlacement ui_shadow_placement(const UIShadowLayout *layout,Rectangle wrapper);
/* Caller owns at least layout.bytes bytes; no allocation or ownership transfer.
   Invalid derived layouts or short buffers fail without changing the buffer.
   This canonical LUT/RGBA8 model is not a claim of bit-identical Skia GPU
   interpolation at every rounding threshold; see the retained reference test. */
bool ui_shadow_mask_rgba(const UIShadowLayout *layout,unsigned char *rgba,size_t capacity);
#endif
