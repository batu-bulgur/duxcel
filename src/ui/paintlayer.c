#include "paintlayer.h"
#include <math.h>
UIPaintLayout ui_paint_layout(Rectangle bounds,Vector2 density,Vector2 reserve,Vector2 capacity) {
    UIPaintLayout out={0};
    if (!isfinite(bounds.x)||!isfinite(bounds.y)||!isfinite(bounds.width)||!isfinite(bounds.height)||bounds.width<=0||bounds.height<=0||fabsf(bounds.x)>32768||fabsf(bounds.y)>32768||bounds.width>32768||bounds.height>32768||
        !isfinite(density.x)||!isfinite(density.y)||density.x<=0||density.y<=0||density.x>8||density.y>8||
        !isfinite(reserve.x)||!isfinite(reserve.y)||reserve.x<0||reserve.y<0||reserve.x>32768||reserve.y>32768||
        !isfinite(capacity.x)||!isfinite(capacity.y)||capacity.x<0||capacity.y<0||capacity.x>8192||capacity.y>8192) return out;
    out.x=(int)floorf(bounds.x*density.x); out.y=(int)floorf(bounds.y*density.y);
    out.width=(int)ceilf((bounds.x+bounds.width)*density.x)-out.x;
    out.height=(int)ceilf((bounds.y+bounds.height)*density.y)-out.y;
    float width=fmaxf(out.width,fmaxf(reserve.x*density.x,capacity.x)),height=fmaxf(out.height,fmaxf(reserve.y*density.y,capacity.y));
    if (width>8192||height>8192) return (UIPaintLayout){0};
    out.capacity_width=(int)ceilf(width/64)*64; out.capacity_height=(int)ceilf(height/64)*64;
    out.bytes=(size_t)out.capacity_width*(size_t)out.capacity_height*4;
    if (!out.width||!out.height||out.bytes>UI_PAINT_SLOT_BYTES) return (UIPaintLayout){0};
    out.bounds=(Rectangle){out.x/density.x,out.y/density.y,out.width/density.x,out.height/density.y};
    out.valid=true; return out;
}
