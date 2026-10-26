#include "ui/value_type_view.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL type layout:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void inside(Rectangle child,Rectangle parent) { CHECK(child.width>0 && child.height>0 && child.x>=parent.x-.01f && child.y>=parent.y-.01f && child.x+child.width<=parent.x+parent.width+.01f && child.y+child.height<=parent.y+parent.height+.01f); }
int main(void) {
    const float scales[]={.8f,1,1.5f,2,2.5f};
    for (unsigned i=0;i<5;i++) for (unsigned narrow=0;narrow<2;narrow++) {
        float s=scales[i]; Rectangle viewport={17,23,(narrow ? 296 : 720)*s,320*s};
        UIValueTypeLayout l=ui_value_type_layout(viewport,s); CHECK(l.valid); inside(l.panel,viewport); inside(l.title,l.panel);
        for (unsigned j=0;j<5;j++) { inside(l.family[j],l.panel); inside(l.bits[j],l.panel); }
        for (unsigned j=0;j<2;j++) { inside(l.sign[j],l.panel); inside(l.details[j],l.panel); inside(l.minus[j],l.panel); inside(l.value[j],l.panel); inside(l.plus[j],l.panel); }
        inside(l.help,l.panel); inside(l.apply,l.panel); inside(l.cancel,l.panel);
        CHECK(l.cancel.x+l.cancel.width<l.apply.x && l.family[UI_VALUE_TYPE_DECIMAL].x+l.family[UI_VALUE_TYPE_DECIMAL].width<l.family[UI_VALUE_TYPE_CLEAR].x);
    }
    CHECK(ui_value_type_layout((Rectangle){0,0,720,480},1.5f).valid);
    CHECK(!ui_value_type_layout((Rectangle){0,0,720,480},0).valid && !ui_value_type_layout((Rectangle){0,0,720,480},NAN).valid);
    CHECK(!ui_value_type_layout((Rectangle){0,0,295,320},1).valid && !ui_value_type_layout((Rectangle){0,0,400,279},1).valid);
    CHECK(!ui_value_type_layout((Rectangle){NAN,0,720,480},1).valid && !ui_value_type_layout((Rectangle){0,0,INFINITY,480},1).valid);
    printf("PASS: %u bounded value-type chooser layout checks\n",checks); return 0;
}
