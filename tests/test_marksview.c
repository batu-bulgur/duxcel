#include "ui/marksview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"marks geometry line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static bool near(float a,float b) { return fabsf(a-b)<.002f; }
int main(void) {
    UIMarksContent content={.measures=2,.assigned={false,false,true,true,false,false}};
    UIMarksLayout exact=ui_marks_layout((Rectangle){0,553,227,245},1,&content,0);
    CHECK(exact.valid && exact.scrollable && near(exact.content_height,306.5f) && near(exact.maximum,61.5f));
    CHECK(exact.representation.x==10 && exact.representation.y==583 && exact.representation.width==198 && exact.representation.height==26);
    CHECK(exact.measure_heading.y==609 && exact.measure_heading.height==24 && exact.measures[0].y==633 && exact.measures[1].y==659);
    CHECK(exact.targets[0].x==10 && exact.targets[0].y==688 && exact.targets[0].width==64 && exact.targets[0].height==40 && exact.targets[2].height==47 && exact.targets[3].y==738);
    CHECK(exact.filter_heading.y==787 && exact.empty_filter.y==811 && exact.inherited.y==839 && exact.scrollbar.x==218);
    CHECK(near(ui_marks_content_height(227,&content),306.5f));
    for (int scale=0;scale<3;scale++) for (unsigned measures=0;measures<=8;measures++) for (unsigned filters=0;filters<=8;filters++) {
        float s=scale==0 ? .8f : scale==1 ? 1 : 1.5f; content.measures=measures; content.filters=filters; content.plot=measures>1;
        for (unsigned i=0;i<filters;i++) content.filter_widths[i]=i%2 ? 88 : 160;
        UIMarksLayout view=ui_marks_layout((Rectangle){12*s,-30*s,227*s,245*s},s,&content,1e4f);
        CHECK(view.valid && view.offset==view.maximum && view.content_height>=245 && view.content_width>0);
        CHECK(view.representation.x==22*s && near(view.representation.width,198*s));
        for (unsigned i=0;i<6;i++) CHECK(view.targets[i].x>=12*s && view.targets[i].x+view.targets[i].width<=230*s+.002f && view.targets[i].height>0);
        for (unsigned i=0;i<filters;i++) {
            CHECK(view.filters[i].x>=22*s && view.filters[i].x+view.filters[i].width<=220*s+.002f && view.filters[i].height>0);
            if (i) CHECK(view.filters[i].y>=view.filters[i-1].y && (view.filters[i].y>view.filters[i-1].y || view.filters[i].x>=view.filters[i-1].x+view.filters[i-1].width));
        }
    }
    CHECK(!ui_marks_layout((Rectangle){0,0,10,10},1,&content,0).valid);
    content.filters=9; CHECK(!ui_marks_layout((Rectangle){0,0,227,245},1,&content,0).valid);
    content.filters=1; content.filter_widths[0]=NAN; CHECK(!ui_marks_layout((Rectangle){0,0,227,245},1,&content,0).valid);
    printf("PASS: %u exact Marks geometry checks (measures, six roles, full-section scrolling, scrollbar reservation, filter wrap and scale)\n",checks); return 0;
}
