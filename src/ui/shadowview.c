#include "shadowview.h"
#include <math.h>
#include <stdint.h>

/* Chrome150 5b586c06e0d27582900f17e2d59c5370d8d6e0bb:
   blink/core/style/shadow_data.h (sigma = radius/2), with Skia DEPS revision
   14d05ec761901b6e9e9193af8b347ab3a7f6fed0 src/gpu/BlurUtils.cpp
   CreateIntegralTable/ComputeIntegralTableWidth and ganesh/GrBlurUtils.cpp
   MakeRectBlur. This is a small scalar implementation, not a Skia dependency.
   The A8 LUT and linear lookup precede RGBA8 premultiplication; blending the
   authored color directly over the final background loses that layer step.
   GrDefaultGeoProcFactory.cpp declares premultiplied color as half4. We use an
   explicit binary16 RGB coefficient encoding, retaining byte opacity separately.
   This is our portable finite-precision encoding choice, not a simulation of
   Skia's complete shader/interpolation/blending pipeline. */
#define TABLE_LIMIT 4096
static int table_width(double sigma) {
    double minimum=2*ceil(6*sigma);
    if (!isfinite(minimum) || minimum>TABLE_LIMIT) return 0;
    int width=32;
    while (width<minimum) width*=2;
    return width;
}
static bool same_rect(Rectangle a,Rectangle b) {
    return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height;
}
UIShadowLayout ui_shadow_layout(float scale,Vector2 density) {
    UIShadowLayout out={0};
    if (!isfinite(scale) || scale<=0 || scale>8 || !isfinite(density.x) || !isfinite(density.y) ||
        density.x<=0 || density.y<=0 || density.x>8 || density.y>8) return out;
    out.table_width_x=table_width(7*(double)scale*density.x);
    out.table_width_y=table_width(7*(double)scale*density.y);
    if (!out.table_width_x || !out.table_width_y) return (UIShadowLayout){0};
    out.width=(int)ceil(26*(double)scale*density.x);
    out.corner_rows=(int)ceil(21*(double)scale*density.y);
    out.atlas_width=out.width; out.atlas_height=2*out.corner_rows+1;
    if (out.width<=0 || out.corner_rows<=0 || out.width>2048 || out.atlas_height>2048 ||
        (size_t)out.width>UI_SHADOW_MASK_LIMIT/4/(size_t)out.atlas_height)
        return (UIShadowLayout){0};
    if (out.width/(double)density.x>32768 || out.corner_rows/(double)density.y>16384)
        return (UIShadowLayout){0};
    out.bytes=(size_t)out.width*(size_t)out.atlas_height*4;
    out.scale=scale; out.density=density;
    out.top=(Rectangle){0,0,out.width,out.corner_rows};
    out.body=(Rectangle){0,out.corner_rows,out.width,1};
    out.bottom=(Rectangle){0,out.corner_rows+1,out.width,out.corner_rows};
    out.valid=true; return out;
}
static bool valid_layout(const UIShadowLayout *layout,UIShadowLayout *canonical) {
    if (!layout) return false;
    UIShadowLayout q=ui_shadow_layout(layout->scale,layout->density);
    if (!q.valid || !layout->valid || layout->width!=q.width || layout->corner_rows!=q.corner_rows ||
        layout->atlas_width!=q.atlas_width || layout->atlas_height!=q.atlas_height || layout->bytes!=q.bytes ||
        layout->table_width_x!=q.table_width_x || layout->table_width_y!=q.table_width_y ||
        !same_rect(layout->top,q.top) || !same_rect(layout->body,q.body) || !same_rect(layout->bottom,q.bottom))
        return false;
    if (canonical) *canonical=q;
    return true;
}
UIShadowPlacement ui_shadow_placement(const UIShadowLayout *layout,Rectangle wrapper) {
    UIShadowPlacement out={0}; UIShadowLayout q;
    if (!valid_layout(layout,&q) || !isfinite(wrapper.x) || !isfinite(wrapper.y) ||
        !isfinite(wrapper.width) || !isfinite(wrapper.height) || fabsf(wrapper.x)>32768 || fabsf(wrapper.y)>32768 ||
        wrapper.width<42*q.scale || wrapper.width>32768 || wrapper.height>32768) return out;
    float width=q.width/q.density.x,corner=q.corner_rows/q.density.y;
    if (wrapper.height<2*corner) return out;
    float x=wrapper.x-width,body_start=wrapper.y+corner,bottom_start=wrapper.y+wrapper.height-corner;
    if (bottom_start<body_start) return out; /* Unrepresentable short span. */
    out.top=(UIShadowSlice){q.top,(Rectangle){x,wrapper.y,width,corner}};
    out.body=(UIShadowSlice){q.body,(Rectangle){x,body_start,width,bottom_start-body_start}};
    out.bottom=(UIShadowSlice){q.bottom,(Rectangle){x,bottom_start,width,corner}};
    out.valid=true; return out;
}
static void integral_table(uint8_t *table,int width) {
    table[0]=255;
    float inverse=1.0f/width;
    for (int i=1;i<width-1;i++) {
        float x=(i+.5f)*inverse;
        x=(-6*x+3)*0.7071067811865475244f;
        float integral=.5f*(erff(x)+1);
        table[i]=(uint8_t)floorf(255*integral+.5f);
    }
    table[width-1]=0;
}
static double lookup(const uint8_t *table,int width,double sigma,double value) {
    double position=value*width/(6*sigma)-.5;
    if (position<=0) return table[0]/255.0;
    if (position>=width-1) return table[width-1]/255.0;
    int at=(int)floor(position);
    double fraction=position-at;
    return (table[at]*(1-fraction)+table[at+1]*fraction)/255;
}
static uint8_t byte_round(double value) {
    return (uint8_t)floor(fmin(255,fmax(0,value))+.5);
}
/* Round a normalized nonnegative coefficient to IEEE binary16, nearest with
   ties to even. C99 math avoids compiler-specific half types or float layouts.
   Normal values have 11 significant bits; subnormals retain a 2^-24 step. */
static double unit_binary16(double value) {
    if (!(value>0)) return 0;
    if (value>=1) return 1;
    int exponent=0; frexp(value,&exponent);
    int step_exponent=exponent-11;
    if (step_exponent<-24) step_exponent=-24;
    double units=ldexp(value,-step_exponent),whole=floor(units),fraction=units-whole;
    if (fraction>.5 || (fraction==.5 && ((unsigned)whole&1))) whole++;
    return ldexp(whole,step_exponent);
}
bool ui_shadow_mask_rgba(const UIShadowLayout *layout,unsigned char *rgba,size_t capacity) {
    UIShadowLayout q;
    if (!rgba || !valid_layout(layout,&q) || capacity<q.bytes) return false;
    uint8_t table_x[TABLE_LIMIT],table_y[TABLE_LIMIT];
    integral_table(table_x,q.table_width_x); integral_table(table_y,q.table_width_y);
    double sigma_x=7*(double)q.scale*q.density.x,sigma_y=7*(double)q.scale*q.density.y;
    double left=-5*(double)q.scale*q.density.x;
    const double opacity=33.0/255;
    const double rgb[]={unit_binary16((39.0/255)*opacity),
        unit_binary16((61.0/255)*opacity),unit_binary16((79.0/255)*opacity)};
    for (int y=0;y<q.atlas_height;y++) {
        /* Mirror the same canonical corner about the wrapper's bottom edge.
           Its full-height body is exactly1 beyond the table's three-sigma end. */
        int row=y<q.corner_rows ? y : q.atlas_height-1-y;
        double vertical=y==q.corner_rows ? 1 : lookup(table_y,q.table_width_y,sigma_y,3*sigma_y-row-.5);
        for (int x=0;x<q.width;x++) {
            double horizontal=lookup(table_x,q.table_width_x,sigma_x,left+3*sigma_x+q.width-x-.5);
            /* Authored .13 at an RGBA8 layer boundary has effective byte33.
               RGB coefficients pass through binary16 before coverage; alpha
               keeps the separate byte opacity. Do not reconstruct RGB from
               the rounded A8 alpha or quantize coverage together with color. */
            double coverage=horizontal*vertical,alpha=coverage*opacity;
            size_t at=((size_t)y*(size_t)q.width+(size_t)x)*4;
            rgba[at]=byte_round(255*rgb[0]*coverage); rgba[at+1]=byte_round(255*rgb[1]*coverage);
            rgba[at+2]=byte_round(255*rgb[2]*coverage); rgba[at+3]=byte_round(255*alpha);
        }
    }
    return true;
}
