#include "ui/paintlayer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"paint layout at %d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    const float densities[]={1,1.25f,1.5f,2,3}; const float scales[]={.8f,1,1.5f};
    for (int d=0;d<5;d++) for (int s=0;s<3;s++) for (int i=1;i<=16;i++) {
        float dpi=densities[d],scale=scales[s],extent=228*i/16.0f;
        Rectangle bounds={.25f,137*scale,extent*scale,600*scale};
        UIPaintLayout layout=ui_paint_layout(bounds,(Vector2){dpi,dpi},(Vector2){228*scale,600*scale},(Vector2){0});
        CHECK(layout.valid); CHECK(layout.capacity_width%64==0 && layout.capacity_height%64==0);
        CHECK(layout.width<=layout.capacity_width && layout.height<=layout.capacity_height);
        CHECK(layout.bounds.x<=bounds.x && layout.bounds.y<=bounds.y);
        CHECK(layout.bounds.x+layout.bounds.width>=bounds.x+bounds.width-.001f && layout.bounds.y+layout.bounds.height>=bounds.y+bounds.height-.001f);
        CHECK(layout.bytes==(size_t)layout.capacity_width*layout.capacity_height*4 && layout.bytes<=UI_PAINT_SLOT_BYTES);
        UIPaintLayout stable=ui_paint_layout(bounds,(Vector2){dpi,dpi},(Vector2){228*scale,600*scale},(Vector2){layout.capacity_width,layout.capacity_height});
        CHECK(stable.valid && stable.capacity_width==layout.capacity_width && stable.capacity_height==layout.capacity_height);
    }
    UIPaintLayout clipped=ui_paint_layout((Rectangle){-10.25f,20.5f,228,120},(Vector2){2,2},(Vector2){228,120},(Vector2){0});
    CHECK(clipped.valid && clipped.x==-21 && clipped.y==41 && clipped.width==457 && clipped.height==240);
    CHECK(!ui_paint_layout((Rectangle){0,0,8192,8192},(Vector2){1,1},(Vector2){0},(Vector2){0}).valid);
    CHECK(!ui_paint_layout((Rectangle){NAN,0,10,10},(Vector2){1,1},(Vector2){0},(Vector2){0}).valid);
    CHECK(!ui_paint_layout((Rectangle){0,0,10,10},(Vector2){0,1},(Vector2){0},(Vector2){0}).valid);
    CHECK(!ui_paint_layout((Rectangle){0,0,10,10},(Vector2){1,1},(Vector2){INFINITY,0},(Vector2){0}).valid);
    CHECK(!ui_paint_layout((Rectangle){0,0,10,10},(Vector2){1,1},(Vector2){0},(Vector2){1e30f,0}).valid);
    printf("PASS: %d paint-layer geometry checks (pixel alignment, DPI, retained capacity and resource bounds)\n",checks); return 0;
}
