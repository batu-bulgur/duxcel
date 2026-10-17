#include "ui/guideview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL guide:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    const float heights[]={34,34,51,34,17},shortcuts[]={14.5f,14.5f,14.5f,14.5f,14.5f,14.5f};
    UIGuideLayout reference=ui_guide_layout((Vector2){1440,988},1,heights,shortcuts,18,32,123);
    CHECK(reference.valid && reference.panel.x==460 && reference.panel.y==218 && reference.panel.width==520 && reference.panel.height==552 && reference.content_height==552);
    CHECK(reference.header.y==219 && reference.intro.y==268 && reference.names[0].y==299 && reference.shortcuts.y==525 && reference.shortcuts.height==142 && reference.note.y==680 && reference.start.y==725);
    const float scales[]={.8f,1,1.5f,2.5f}; const Vector2 viewports[]={{640,640},{1020,640},{1280,820},{1440,988}};
    for (size_t s=0;s<4;s++) for (size_t v=0;v<4;v++) {
        UIGuideLayout layout=ui_guide_layout(viewports[v],scales[s],heights,shortcuts,18,32,123); CHECK(layout.valid);
        CHECK(layout.panel.x>=0 && layout.panel.y>=0 && layout.panel.x+layout.panel.width<=viewports[v].x && layout.panel.y+layout.panel.height<=viewports[v].y);
        CHECK(layout.close.x>=layout.clip.x && layout.close.x+layout.close.width<=layout.clip.x+layout.clip.width);
        for (int i=0;i<5;i++) { CHECK(layout.names[i].x+layout.names[i].width<=layout.descriptions[i].x); CHECK(layout.descriptions[i].x+layout.descriptions[i].width<=layout.clip.x+layout.clip.width); }
        for (int i=0;i<6;i++) { CHECK(layout.labels[i].x+layout.labels[i].width<=layout.keys[i].x); CHECK(layout.keys[i].x+layout.keys[i].width<=layout.clip.x+layout.clip.width); }
        CHECK(layout.note.y>=layout.shortcuts.y+layout.shortcuts.height && layout.start.y>=layout.note.y+layout.note.height);
        CHECK(layout.content_height>=layout.panel.height/scales[s]-.01f);
    }
    CHECK(!ui_guide_layout((Vector2){640,640},NAN,heights,shortcuts,18,32,123).valid);
    CHECK(!ui_guide_layout((Vector2){100,100},1,heights,shortcuts,18,32,123).valid);
    CHECK(!ui_guide_layout((Vector2){640,640},1,NULL,shortcuts,18,32,123).valid);
    printf("PASS: %u guide layout checks (exact study geometry, paired descriptions, shortcut columns, narrow scrolling and 80/100/150/250 percent)\n",checks); return 0;
}
