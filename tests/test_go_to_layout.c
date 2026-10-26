#include "ui/go_to_layout.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL Go To layout:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void inside(Rectangle r,Rectangle p) { CHECK(r.width>0 && r.height>0 && r.x>=p.x-.01f && r.y>=p.y-.01f && r.x+r.width<=p.x+p.width+.01f && r.y+r.height<=p.y+p.height+.01f); }
int main(void) {
    const float scales[]={.8f,1,1.5f,2,2.5f}; for (unsigned i=0;i<5;i++) for (unsigned narrow=0;narrow<2;narrow++) { float s=scales[i]; Rectangle area={13,17,(narrow ? 296 : 720)*s,320*s}; UIGoToLayout l=ui_go_to_layout(area,s); CHECK(l.valid); inside(l.panel,area); inside(l.title,l.panel); inside(l.label,l.panel); inside(l.input,l.panel); inside(l.message,l.panel); inside(l.cancel,l.panel); inside(l.go,l.panel); CHECK(l.cancel.x+l.cancel.width<l.go.x && l.input.y+l.input.height<l.message.y); }
    CHECK(ui_go_to_layout((Rectangle){0,0,720,480},1.5f).valid); CHECK(!ui_go_to_layout((Rectangle){0,0,295,320},1).valid && !ui_go_to_layout((Rectangle){0,0,500,203},1).valid && !ui_go_to_layout((Rectangle){0,0,500,320},NAN).valid);
    printf("PASS: %u bounded Go To dialog geometry checks\n",checks); return 0;
}
