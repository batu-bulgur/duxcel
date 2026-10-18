#include "ui/inspector.h"
#include <math.h>
#include <float.h>
#include <stdint.h>
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
static void reference_scroll_checks(void) {
    /* Actual DOM/CDP captures: 20261018T041544-reference-steps-scroll-24743.
       Text label widths remain native layout capacity, not DOM font advances. */
    static const struct {
        float scale;
        Rectangle panel,header,source,list,origin,details,edit,preview,footer,note;
    } reference[]={
        {1,{1160,93,280,909},{1161,93,279,35},{1161,128,279,34},{1161,162,279,337},{1168,169,265,330},{1161,508,279,113},{1174,578,122.5f,27},{1304.5f,578,122.5f,27},{1161,932,279,70},{1174,974,253,13}},
        {1.5f,{1500,139,420,948},{1501,139,419,52.5f},{1501,191.5f,419,51},{1501,242.5f,419,505.5f},{1511.5f,253,398,495},{1501,761.5f,419,169.5f},{1520.5f,866.5f,184,40.5f},{1716.5f,866.5f,184,40.5f},{1501,983,419,104},{1520.5f,1045.5f,380,19}},
        {1,{360,93,280,605},{361,93,279,35},{361,128,279,34},{361,162,279,337},{368,169,265,330},{361,508,279,113},{374,578,122.5f,27},{504.5f,578,122.5f,27},{361,628,279,70},{374,670,253,13}},
        {1.5f,{220,139,420,468},{221,139,406,52.5f},{221,191.5f,406,51},{221,242.5f,406,232.5f},{231.5f,253,372,495},{221,488.5f,406,169.5f},{240.5f,593.5f,177.5f,40.5f},{430,593.5f,177.5f,40.5f},{221,658,406,104},{240.5f,720.5f,367,19}}
    };
    for (size_t i=0;i<sizeof reference/sizeof *reference;i++) {
        UIInspectorLayout layout=ui_inspector_layout(reference[i].panel,reference[i].scale,6,false);
        CHECK(layout.valid); CHECK(layout.row_height==55);
        CHECK(same(layout.header,reference[i].header)); CHECK(same(layout.source,reference[i].source));
        CHECK(same(layout.list_frame,reference[i].list)); CHECK(same(layout.list_origin,reference[i].origin));
        CHECK(same(layout.details,reference[i].details)); CHECK(same(layout.edit,reference[i].edit)); CHECK(same(layout.preview,reference[i].preview));
        CHECK(same(layout.footer,reference[i].footer)); CHECK(same(layout.note,reference[i].note));
        CHECK(layout.detail_title.y==layout.details.y+1+16*reference[i].scale);
        CHECK(layout.detail_title.height==ceilf(15*reference[i].scale));
        CHECK(layout.inspect.y==layout.footer.y+1+12*reference[i].scale);
        CHECK(layout.outer_scrollable==(i==3)); CHECK(layout.inner_scrollable==(i==3));
        if (i!=3) { CHECK(!layout.outer_max && !layout.inner_max); CHECK(same(layout.list,layout.list_frame)); }
    }
    Rectangle short_panel={220,139,420,468};
    UIInspectorLayout start=ui_inspector_layout(short_panel,1.5f,6,false);
    CHECK(fabsf(start.outer_max-103.33333587646484f)<.00001f); CHECK(start.inner_max==182);
    CHECK(same(start.outer_scrollbar,(Rectangle){627,139,13,468}));
    CHECK(same(start.list_scrollbar,(Rectangle){614,242.5f,13,232.5f}));
    CHECK(same(start.body_clip,(Rectangle){221,191.5f,406,415.5f}));
    CHECK(same(start.collapse,(Rectangle){543,147.5f,34.5f,34.5f}));
    CHECK(same(start.dismiss,(Rectangle){580.5f,147.5f,34.5f,34.5f}));
    UIInspectorLayout outer=ui_inspector_layout_scrolled(short_panel,1.5f,6,false,FLT_MAX,0);
    CHECK(outer.outer_offset==outer.outer_max && !outer.inner_offset);
    CHECK(outer.details.y==333.5f && outer.footer.y==503 && outer.note.y==565.5f);
    CHECK(same(outer.header,start.header)); CHECK(same(outer.collapse,start.collapse)); CHECK(same(outer.dismiss,start.dismiss));
    CHECK(same(outer.body_clip,start.body_clip)); CHECK(same(outer.outer_scrollbar,start.outer_scrollbar));
    CHECK(same(outer.list_scrollbar,(Rectangle){614,87.5f,13,232.5f}));
    CHECK(same(outer.list,(Rectangle){221,191.5f,393,128.5f}));
    CHECK(outer.list_scrollbar.y<outer.body_clip.y); /* Full track, not shortened by sticky header. */
    CHECK(inside(outer.edit,outer.body_clip)); CHECK(inside(outer.preview,outer.body_clip)); CHECK(inside(outer.footer,outer.body_clip));
    UIInspectorLayout inner=ui_inspector_layout_scrolled(short_panel,1.5f,6,false,0,FLT_MAX);
    CHECK(!inner.outer_offset && inner.inner_offset==182);
    CHECK(inner.list_origin.y==-20 && inner.list_origin.height==495);
    CHECK(same(inner.list_frame,start.list_frame)); CHECK(same(inner.list_scrollbar,start.list_scrollbar));
    CHECK(same(inner.details,start.details)); CHECK(same(inner.footer,start.footer)); CHECK(same(inner.source,start.source));
    UIInspectorLayout both=ui_inspector_layout_scrolled(short_panel,1.5f,6,false,FLT_MAX,FLT_MAX);
    CHECK(both.list_origin.y==-175 && both.list_origin.width==372);
    CHECK(same(both.list_frame,outer.list_frame)); CHECK(same(both.details,outer.details)); CHECK(same(both.header,start.header));
    UIInspectorLayout negative=ui_inspector_layout_scrolled(short_panel,1.5f,6,false,-100,-100);
    CHECK(!negative.outer_offset && !negative.inner_offset); CHECK(same(negative.list_origin,start.list_origin));
    CHECK(!ui_inspector_layout_scrolled(short_panel,1.5f,6,false,NAN,0).valid);
    CHECK(!ui_inspector_layout_scrolled(short_panel,1.5f,6,false,0,INFINITY).valid);
    /* This312-unit pane has no SQL aside overflow. Wrapped code remains an
       independent App concern, not a function of the source-step count. */
    UIInspectorLayout sql=ui_inspector_layout(short_panel,1.5f,6,true),sql_scrolled=ui_inspector_layout_scrolled(short_panel,1.5f,6,true,200,300);
    CHECK(same(sql.sql_code,sql_scrolled.sql_code)); CHECK(same(sql.source,sql_scrolled.source)); CHECK(same(sql.copy,sql_scrolled.copy)); CHECK(same(sql.steps,sql_scrolled.steps));
    CHECK(!sql_scrolled.outer_scrollable && !sql_scrolled.inner_scrollable && !sql_scrolled.outer_offset && !sql_scrolled.inner_offset);
}
static void reference_sql_checks(void) {
    /* Frozen actual SQL captures:20261018T051457-reference-sql-44683.
       All4 required defaults plus exploratory640x400/150. SQL source,
       description and footer use their real flex boxes; code_frame is the
       full pre. code is its padded baseline before App-owned text overflow.
       Copy/Steps include the observed unscaled borders and1/64-point label
       ceil, so the150% Steps width is99.484375 rather than99.4765625. */
    static const struct {
        float scale;
        Rectangle panel,source,description,frame,code,footer,copy,steps;
    } reference[]={
        {1,{1100,93,340,909},{1101,128,339,34},{1114,173,313,14},{1101,198,339,752},{1115,198,311,752},{1101,950,339,52},{1113,963,86.09375f,27},{1361.015625f,963,66.984375f,27}},
        {1.5f,{1410,139,510,948},{1411,191.5f,509,51},{1430.5f,259,470,22},{1411,297.5f,509,712},{1432,297.5f,467,712},{1411,1009.5f,509,77.5f},{1429,1028.5f,128.140625f,40.5f},{1802.515625f,1028.5f,99.484375f,40.5f}},
        {1,{300,93,340,605},{301,128,339,34},{314,173,313,14},{301,198,339,448},{315,198,311,448},{301,646,339,52},{313,659,86.09375f,27},{561.015625f,659,66.984375f,27}},
        {1.5f,{130,139,510,468},{131,191.5f,509,51},{150.5f,259,470,22},{131,297.5f,509,232},{152,297.5f,467,232},{131,529.5f,509,77.5f},{149,548.5f,128.140625f,40.5f},{522.515625f,548.5f,99.484375f,40.5f}},
        {1.5f,{130,139,510,228},{131,191.5f,496,51},{150.5f,259,457,22},{131,297.5f,496,0},{152,297.5f,454,0},{131,297.5f,496,77.5f},{149,316.5f,128.140625f,40.5f},{509.515625f,316.5f,99.484375f,40.5f}}
    };
    for (size_t i=0;i<sizeof reference/sizeof *reference;i++) {
        UIInspectorLayout a=ui_inspector_layout(reference[i].panel,reference[i].scale,6,true);
        CHECK(a.valid); CHECK(same(a.source,reference[i].source)); CHECK(same(a.sql_description,reference[i].description));
        CHECK(same(a.sql_frame,reference[i].frame)); CHECK(same(a.sql_code,reference[i].code)); CHECK(same(a.footer,reference[i].footer));
        CHECK(same(a.copy,reference[i].copy)); CHECK(same(a.steps,reference[i].steps));
        CHECK(a.copy.width>a.steps.width && a.copy.x+a.copy.width<a.steps.x);
        CHECK(a.sql_code.x==a.sql_frame.x+14*reference[i].scale && a.sql_code.y==a.sql_frame.y);
        CHECK(a.sql_code.width==a.sql_frame.width-28*reference[i].scale && a.sql_code.height==a.sql_frame.height);
        CHECK(!a.inner_scrollable && !a.inner_max && !a.inner_offset && !a.list_scrollbar.width);
        CHECK(inside(a.header,reference[i].panel)); CHECK(inside(a.body_clip,reference[i].panel));
        CHECK(a.header.x==reference[i].panel.x+1 && a.header.height==35*reference[i].scale);
        UIInspectorLayout other=ui_inspector_layout_scrolled(reference[i].panel,reference[i].scale,SIZE_MAX,true,0,FLT_MAX);
        CHECK(other.valid && same(a.sql_frame,other.sql_frame) && same(a.copy,other.copy) && same(a.steps,other.steps));
        CHECK(!other.inner_offset && !other.inner_max);
        if (i<4) {
            CHECK(!a.outer_scrollable && !a.outer_max && !a.outer_scrollbar.width);
            CHECK(inside(a.footer,reference[i].panel) && inside(a.sql_frame,reference[i].panel));
        } else {
            CHECK(a.outer_scrollable && fabsf(a.outer_max-5.3333335f)<.000001f && !a.sql_frame.height);
            CHECK(same(a.outer_scrollbar,(Rectangle){627,139,13,228}));
            CHECK(same(a.collapse,(Rectangle){543,147.5f,34.5f,34.5f}));
            CHECK(same(a.dismiss,(Rectangle){580.5f,147.5f,34.5f,34.5f}));
            UIInspectorLayout end=ui_inspector_layout_scrolled(reference[i].panel,reference[i].scale,0,true,FLT_MAX,0);
            CHECK(end.outer_offset==end.outer_max && same(end.header,a.header) && same(end.collapse,a.collapse) && same(end.dismiss,a.dismiss));
            CHECK(same(end.source,(Rectangle){131,183.5f,496,51}));
            CHECK(same(end.sql_description,(Rectangle){150.5f,251,457,22}));
            CHECK(same(end.sql_frame,(Rectangle){131,289.5f,496,0}));
            CHECK(same(end.footer,(Rectangle){131,289.5f,496,77.5f}));
            CHECK(same(end.copy,(Rectangle){149,308.5f,128.140625f,40.5f}));
            CHECK(same(end.steps,(Rectangle){509.515625f,308.5f,99.484375f,40.5f}));
            CHECK(inside(end.footer,end.body_clip));
            UIInspectorLayout start=ui_inspector_layout_scrolled(reference[i].panel,reference[i].scale,0,true,-100,0);
            CHECK(!start.outer_offset && same(start.source,a.source));
        }
    }
    UIInspectorLayout minimum=ui_inspector_layout((Rectangle){0,0,340,35},1,0,true);
    CHECK(minimum.valid && !minimum.body_clip.height && !minimum.sql_frame.height);
    CHECK(!ui_inspector_layout((Rectangle){0,0,340,34},1,0,true).valid);
    CHECK(!ui_inspector_layout((Rectangle){0,0,340,219},1,0,false).valid);
    CHECK(!ui_inspector_layout_scrolled((Rectangle){0,0,340,500},1,0,true,NAN,0).valid);
    CHECK(!ui_inspector_layout_scrolled((Rectangle){0,0,340,500},1,0,true,0,INFINITY).valid);
    CHECK(!ui_inspector_layout((Rectangle){FLT_MAX,0,FLT_MAX,500},1,0,true).valid);
}
int main(void) {
    const float scales[]={.8f,1,1.5f},widths[]={188,244,280,400},heights[]={240,300,360,829};
    for (size_t s=0;s<3;s++) for (size_t w=0;w<4;w++) for (size_t h=0;h<4;h++) for (int sql=0;sql<2;sql++) {
        Rectangle panel={50,70,widths[w]*scales[s],heights[h]*scales[s]};
        UIInspectorLayout layout=ui_inspector_layout(panel,scales[s],27,sql!=0); CHECK(layout.valid);
        CHECK(inside(layout.title,panel)); CHECK(inside(layout.collapse,panel)); CHECK(inside(layout.dismiss,panel)); CHECK(inside(layout.source,panel));
        if (sql) { CHECK(inside(layout.footer,panel)); CHECK(inside(layout.sql_code,panel)); CHECK(inside(layout.copy,panel)); CHECK(inside(layout.steps,panel)); CHECK(layout.sql_code.y+layout.sql_code.height<=layout.footer.y+.01f); }
        else {
            CHECK(inside(layout.header,panel)); CHECK(inside(layout.body_clip,panel)); CHECK(inside(layout.list,layout.body_clip));
            CHECK(inside(layout.edit,layout.details)); CHECK(inside(layout.preview,layout.details)); CHECK(inside(layout.inspect,layout.footer)); CHECK(inside(layout.note,layout.footer));
            CHECK(layout.edit.width>0 && layout.edit.width==layout.preview.width); CHECK(layout.edit.height==27*scales[s]); CHECK(layout.row_height==55);
            CHECK(layout.list.y+layout.list.height<=layout.details.y+.01f); CHECK(layout.details.y+layout.details.height<=layout.footer.y+.01f);
            CHECK(layout.edit.y>=layout.detail_title.y+layout.detail_title.height); CHECK(layout.preview.x>=layout.edit.x+layout.edit.width);
            UIInspectorLayout end=ui_inspector_layout_scrolled(panel,scales[s],27,false,FLT_MAX,FLT_MAX);
            CHECK(inside(end.edit,end.body_clip)); CHECK(inside(end.preview,end.body_clip)); CHECK(inside(end.footer,end.body_clip));
            CHECK(end.outer_offset==end.outer_max && end.inner_offset==end.inner_max);
            CHECK(same(end.header,layout.header)); CHECK(same(end.collapse,layout.collapse)); CHECK(same(end.dismiss,layout.dismiss));
            if (!layout.outer_scrollable) { CHECK(inside(layout.details,panel)); CHECK(inside(layout.footer,panel)); }
        }
    }
    UIInspectorLayout normal=ui_inspector_layout((Rectangle){0,0,280,829},1,6,false);
    CHECK(normal.list_frame.height==337 && normal.list_origin.height==330 && normal.row_height==55 && normal.details.y==415 && normal.footer.y==759);
    UIInspectorLayout empty=ui_inspector_layout((Rectangle){0,0,244,300},1,0,false); CHECK(empty.valid && empty.list_frame.height==155 && !empty.list_origin.height && !empty.inner_scrollable);
    CHECK(!ui_inspector_layout((Rectangle){0,0,280,829},NAN,6,false).valid);
    CHECK(!ui_inspector_layout((Rectangle){0,0,150,829},1,6,false).valid);
    CHECK(!ui_inspector_layout((Rectangle){0,0,280,-1},1,6,false).valid);
    reference_header_checks();
    reference_scroll_checks();
    reference_sql_checks();
    printf("PASS: %u native inspector geometry checks (fixed study rows, sticky header, nested scroll geometry, measured short panes, SQL, 80/100/150 percent)\n",checks); return 0;
}
