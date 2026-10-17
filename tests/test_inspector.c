#include "ui/inspector.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { fprintf(stderr,"FAIL inspector:%d: %s\n",__LINE__,#condition); exit(1); } } while (0)
static bool inside(Rectangle inner,Rectangle outer) {
    return inner.width>=0 && inner.height>=0 && inner.x>=outer.x-.01f && inner.y>=outer.y-.01f && inner.x+inner.width<=outer.x+outer.width+.01f && inner.y+inner.height<=outer.y+outer.height+.01f;
}
int main(void) {
    const float scales[]={.8f,1,1.5f},widths[]={188,244,280,400},heights[]={240,300,360,829};
    for (size_t s=0;s<3;s++) for (size_t w=0;w<4;w++) for (size_t h=0;h<4;h++) for (int sql=0;sql<2;sql++) {
        Rectangle panel={50,70,widths[w]*scales[s],heights[h]*scales[s]};
        UIInspectorLayout layout=ui_inspector_layout(panel,scales[s],27,sql!=0); CHECK(layout.valid);
        CHECK(inside(layout.title,panel)); CHECK(inside(layout.collapse,panel)); CHECK(inside(layout.dismiss,panel)); CHECK(inside(layout.source,panel)); CHECK(inside(layout.footer,panel));
        if (sql) { CHECK(inside(layout.sql_code,panel)); CHECK(inside(layout.copy,panel)); CHECK(inside(layout.steps,panel)); CHECK(layout.sql_code.y+layout.sql_code.height<=layout.footer.y+.01f); }
        else {
            CHECK(inside(layout.list,panel)); CHECK(inside(layout.details,panel)); CHECK(inside(layout.edit,panel)); CHECK(inside(layout.preview,panel)); CHECK(inside(layout.inspect,panel)); CHECK(inside(layout.note,panel));
            CHECK(layout.edit.width>=77*scales[s]); CHECK(layout.edit.height==27*scales[s]);
            CHECK(layout.list.y+layout.list.height<=layout.details.y+.01f); CHECK(layout.details.y+layout.details.height<=layout.footer.y+.01f);
            CHECK(layout.edit.y>=layout.detail_title.y+layout.detail_title.height); CHECK(layout.preview.x>=layout.edit.x+layout.edit.width);
        }
    }
    UIInspectorLayout normal=ui_inspector_layout((Rectangle){0,0,280,829},1,6,false);
    CHECK(normal.list.height==330 && normal.row_height==55 && normal.details.y==415 && normal.footer.y==759);
    UIInspectorLayout empty=ui_inspector_layout((Rectangle){0,0,244,300},1,0,false); CHECK(empty.valid && !empty.list.height);
    CHECK(!ui_inspector_layout((Rectangle){0,0,280,829},NAN,6,false).valid);
    CHECK(!ui_inspector_layout((Rectangle){0,0,150,829},1,6,false).valid);
    CHECK(!ui_inspector_layout((Rectangle){0,0,280,-1},1,6,false).valid);
    printf("PASS: %u native inspector geometry checks (study rows/details, bounded actions, short panes, SQL, 80/100/150 percent)\n",checks); return 0;
}
