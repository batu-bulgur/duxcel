#ifndef DUXCEL_PAINTLAYER_H
#define DUXCEL_PAINTLAYER_H
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
#define UI_PAINT_DEPTH 3
#define UI_PAINT_BUFFERS 6
#define UI_PAINT_SLOT_BYTES ((size_t)16*1024*1024)
#define UI_PAINT_TOTAL_BYTES ((size_t)32*1024*1024)
typedef struct {
    Rectangle bounds;
    int x,y,width,height,capacity_width,capacity_height;
    size_t bytes;
    bool valid;
} UIPaintLayout;
/* Physical pixel alignment and retained 64-pixel capacity buckets. Reserve is
   the full logical group size, so a changing clip does not reallocate frames. */
UIPaintLayout ui_paint_layout(Rectangle bounds,Vector2 density,Vector2 reserve,Vector2 capacity);
typedef struct { RenderTexture2D target; size_t bytes; } UIPaintBuffer;
typedef struct { UIPaintLayout layout; unsigned buffer_index,texture; } UIPaintImage;
typedef struct {
    UIPaintLayout layout;
    Matrix projection,modelview;
    Rectangle saved_clip;
    unsigned framebuffer,scissor_depth,buffer_index;
    int framebuffer_width,framebuffer_height;
    int viewport_width,viewport_height;
    bool saved_clipping;
    float opacity;
} UIPaintFrame;
#endif
