#include "ui/find_layout.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL Find layout:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void inside(Rectangle r,Rectangle p) { CHECK(r.width>0 && r.height>0 && r.x>=p.x-.01f && r.y>=p.y-.01f && r.x+r.width<=p.x+p.width+.01f && r.y+r.height<=p.y+p.height+.01f); }
int main(void) {
    const float scales[]={.8f,1,1.5f,2,2.5f};
    for (unsigned i=0;i<5;i++) for (unsigned narrow=0;narrow<2;narrow++) {
        float s=scales[i]; Rectangle area={13,17,(narrow ? 296 : 720)*s,320*s}; UIFindLayout l=ui_find_layout(area,s); CHECK(l.valid); inside(l.panel,area);
        Rectangle parts[]={l.title,l.label,l.input,l.whole_cell,l.message,l.close,l.previous,l.next};
        for (size_t n=0;n<sizeof(parts)/sizeof(*parts);n++) inside(parts[n],l.panel);
        CHECK(l.title.y+l.title.height<l.label.y && l.label.y+l.label.height<l.input.y && l.input.y+l.input.height<l.whole_cell.y && l.whole_cell.y+l.whole_cell.height<l.message.y && l.message.y+l.message.height<l.close.y);
        CHECK(l.close.x+l.close.width<l.previous.x && l.previous.x+l.previous.width<l.next.x);
    }
    CHECK(ui_find_layout((Rectangle){0,0,640,480},1.5f).valid);
    CHECK(!ui_find_layout((Rectangle){0,0,295,320},1).valid && !ui_find_layout((Rectangle){0,0,500,251},1).valid);
    CHECK(!ui_find_layout((Rectangle){0,0,500,320},NAN).valid && !ui_find_layout((Rectangle){0,0,500,320},0).valid);
    CHECK(!ui_find_layout((Rectangle){INFINITY,0,500,320},1).valid && !ui_find_layout((Rectangle){0,0,INFINITY,320},1).valid);
    printf("PASS: %u bounded Find dialog geometry checks\n",checks); return 0;
}
