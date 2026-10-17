#include "ui/statsview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL statistics view:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    const float scales[]={.8f,1,1.5f,2.5f}; const Vector2 screens[]={{640,640},{1020,640},{1280,820},{1440,988}};
    for (size_t s=0;s<4;s++) for (size_t v=0;v<4;v++) for (int edge=0;edge<3;edge++) {
        Vector2 anchor={edge==0 ? 0 : edge==1 ? screens[v].x*.5f : screens[v].x, screens[v].y-22*scales[s]};
        UIStatsLayout layout=ui_stats_layout(anchor,screens[v],scales[s]); CHECK(layout.valid);
        CHECK(layout.panel.x>=0 && layout.panel.y>=0 && layout.panel.x+layout.panel.width<=screens[v].x && layout.panel.y+layout.panel.height<=screens[v].y);
        CHECK(layout.description.y+layout.description.height<=layout.rows[0].y+.01f);
        for (int i=0;i<8;i++) { CHECK(layout.rows[i].height>=13*scales[s]); CHECK(layout.rows[i].x>=layout.panel.x && layout.rows[i].x+layout.rows[i].width<=layout.panel.x+layout.panel.width); }
        CHECK(layout.rows[7].y+layout.rows[7].height<=layout.separator.y && layout.done.y+layout.done.height<=layout.panel.y+layout.panel.height);
    }
    UIStatsLayout reference=ui_stats_layout((Vector2){720,977},(Vector2){1440,988},1);
    CHECK(reference.panel.x==720 && reference.panel.y==648 && reference.panel.width==244 && reference.panel.height==312);
    CHECK(reference.title.y==653 && reference.description.y==683 && reference.rows[0].y==704 && reference.rows[7].y==893 && reference.separator.y==924 && reference.done.y==929);
    CHECK(!ui_stats_layout((Vector2){0,0},(Vector2){100,100},1).valid); CHECK(!ui_stats_layout((Vector2){NAN,0},(Vector2){1440,988},1).valid);
    printf("PASS: %u statistics popover checks (exact study measurements, corner anchors, bounded checklist and 80/100/150/250 percent)\n",checks); return 0;
}
