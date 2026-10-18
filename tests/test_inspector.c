#include "ui/inspector.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { fprintf(stderr,"FAIL inspector:%d: %s\n",__LINE__,#condition); exit(1); } } while (0)
static bool inside(Rectangle inner,Rectangle outer) {
    return inner.width>=0 && inner.height>=0 && inner.x>=outer.x-.01f && inner.y>=outer.y-.01f && inner.x+inner.width<=outer.x+outer.width+.01f && inner.y+inner.height<=outer.y+outer.height+.01f;
}
static bool same(Rectangle a,Rectangle b) {
    return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height;
}
static bool contains(Rectangle r,float x,float y) {
    return x>=r.x && y>=r.y && x<r.x+r.width && y<r.y+r.height;
}
static bool clipped_contains(Rectangle target,Rectangle clip,float x,float y) {
    return contains(target,x,y) && contains(clip,x,y);
}
static void reference_header_checks(void) {
    /* Retained DOM: 20261018T030846-reference-rail-1269/<case>-expanded.state.json.
       Panel/Collapse are measured window points. Dismiss/header clip follow
       the frozen styles.css 23-unit buttons, two-unit gap and 35-unit title.
       The short narrow-150 state has an outer scrollbar lane: not covered. */
    static const struct {
        float scale;
        Rectangle panel,collapse,dismiss,header_clip;
    } reference[]={
        {1,{1160,93,280,909},{1384,98.5f,23,23},{1409,98.5f,23,23},{1160,93,280,35}},
        {1.5f,{1500,139,420,948},{1836,147.5f,34.5f,34.5f},{1873.5f,147.5f,34.5f,34.5f},{1500,139,420,52.5f}},
        {1,{360,93,280,605},{584,98.5f,23,23},{609,98.5f,23,23},{360,93,280,35}}
    };
    for (size_t i=0;i<sizeof reference/sizeof *reference;i++) for (int sql=0;sql<2;sql++) {
        UIInspectorLayout layout=ui_inspector_layout(reference[i].panel,reference[i].scale,6,sql!=0);
        CHECK(layout.valid);
        CHECK(same(layout.collapse,reference[i].collapse));
        CHECK(same(layout.dismiss,reference[i].dismiss));
        const Rectangle actual[]={layout.collapse,layout.dismiss};
        const Rectangle measured[]={reference[i].collapse,reference[i].dismiss};
        for (size_t b=0;b<2;b++) {
            Rectangle r=measured[b],clip=reference[i].header_clip;
            CHECK(inside(actual[b],clip));
            /* Input geometry at captured edges, including the formerly
               unreachable left/top strips; these are not dispatch tests. */
            CHECK(clipped_contains(actual[b],clip,r.x+.25f,r.y+.25f));
            CHECK(clipped_contains(actual[b],clip,r.x+r.width-.25f,r.y+r.height-.25f));
            CHECK(!clipped_contains(actual[b],clip,r.x-.25f,r.y+r.height/2));
            CHECK(!clipped_contains(actual[b],clip,r.x+r.width+.25f,r.y+r.height/2));
            CHECK(!clipped_contains(actual[b],clip,r.x+r.width/2,r.y-.25f));
            CHECK(!clipped_contains(actual[b],clip,r.x+r.width/2,r.y+r.height+.25f));
            /* A clipped header must not retain its hidden button strip. */
            clip.y=r.y+1; clip.height=reference[i].header_clip.y+reference[i].header_clip.height-clip.y;
            CHECK(!clipped_contains(actual[b],clip,r.x+.25f,r.y+.25f));
            CHECK(clipped_contains(actual[b],clip,r.x+.25f,r.y+1.25f));
        }
        float gap=(reference[i].collapse.x+reference[i].collapse.width+reference[i].dismiss.x)/2;
        CHECK(!contains(layout.collapse,gap,reference[i].collapse.y+1));
        CHECK(!contains(layout.dismiss,gap,reference[i].dismiss.y+1));
    }
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
    reference_header_checks();
    printf("PASS: %u native inspector geometry checks (study rows/details, bounded actions, short panes, SQL, 80/100/150 percent)\n",checks); return 0;
}
