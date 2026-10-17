#include "ui/choiceview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"choice geometry at %d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    const float scales[]={.8f,1,1.5f,2.5f}; const Vector2 windows[]={{1440,988},{640,640},{1280,820}};
    for (int s=0;s<4;s++) for (int w=0;w<3;w++) for (int count=3;count<=4;count++) for (int sep=-1;sep<count;sep+=count) for (int corner=0;corner<4;corner++) {
        Vector2 viewport=windows[w],origin={corner&1 ? viewport.x : 0,corner&2 ? viewport.y : 0}; float scale=scales[s];
        UIChoiceLayout l=ui_choice_layout(origin,viewport,scale,count,sep);
        CHECK(l.valid); CHECK(l.count==count); CHECK(l.panel.width==fminf(240,viewport.x/scale-12)*scale);
        CHECK(l.panel.height==(40+28*count+(sep>=0 ? 9 : 0))*scale);
        CHECK(l.panel.x>=0 && l.panel.y>=0); CHECK(l.panel.x+l.panel.width<=viewport.x); CHECK(l.panel.y+l.panel.height<=viewport.y);
        CHECK(l.title.height==30*scale);
        for (int i=0;i<count;i++) { CHECK(l.rows[i].height==28*scale); CHECK(l.rows[i].width==l.panel.width-10*scale); CHECK(l.rows[i].y>=l.title.y+l.title.height); CHECK(l.rows[i].y+l.rows[i].height<=l.panel.y+l.panel.height); }
        CHECK((l.separator.height>0)==(sep>=0));
    }
    UIChoiceLayout scale=ui_choice_layout((Vector2){1210,988},(Vector2){1440,988},1,4,-1);
    CHECK(scale.panel.x==1194); CHECK(scale.panel.y==812); CHECK(scale.panel.height==152);
    UIChoiceLayout ribbon=ui_choice_layout((Vector2){1210,58},(Vector2){1440,988},1,4,3);
    CHECK(ribbon.panel.height==161); CHECK(ribbon.rows[3].y-ribbon.rows[2].y==37);
    CHECK(!ui_choice_layout((Vector2){NAN,0},(Vector2){1440,988},1,4,-1).valid);
    CHECK(!ui_choice_layout((Vector2){0,0},(Vector2){1440,988},0,4,-1).valid);
    CHECK(!ui_choice_layout((Vector2){0,0},(Vector2){1440,988},1,UI_CHOICE_MAX+1,-1).valid);
    for (int s=0;s<3;s++) for (int w=0;w<3;w++) for (int corner=0;corner<4;corner++) {
        Vector2 viewport=windows[w],origin={corner&1 ? viewport.x : 0,corner&2 ? viewport.y : 0}; float factor=scales[s];
        UIChoiceLayout fields=ui_choice_layout(origin,viewport,factor,9,3);
        CHECK(fields.valid && fields.count==9 && fields.panel.height==301*factor);
        CHECK(fields.panel.x>=0 && fields.panel.x+fields.panel.width<=viewport.x);
        CHECK(fields.panel.y>=0 && fields.panel.y+fields.panel.height<=viewport.y);
        for (int i=0;i<9;i++) { CHECK(fields.rows[i].height==28*factor); CHECK(fields.rows[i].y+fields.rows[i].height<=fields.panel.y+fields.panel.height+.001f); }
        CHECK(fabsf(fields.rows[3].y-fields.rows[2].y-37*factor)<.001f);
    }
    CHECK(!ui_choice_layout((Vector2){0,0},(Vector2){1440,988},1,4,4).valid);
    CHECK(!ui_choice_layout((Vector2){0,0},(Vector2){160,100},1,4,-1).valid);
    for (int s=0;s<4;s++) for (int w=0;w<3;w++) for (int count=1;count<=16384;count=count<12 ? count+1 : count*8) {
        UIChoiceListLayout list=ui_choice_list_layout((Vector2){windows[w].x,windows[w].y},windows[w],scales[s],count);
        CHECK(list.frame.valid && list.visible>=1 && list.visible<=count && list.visible<=UI_CHOICE_MAX);
        CHECK(list.frame.panel.width==240*scales[s]);
        CHECK(list.frame.panel.y+list.frame.panel.height<=windows[w].y);
        CHECK(list.items.height==list.visible*28*scales[s]);
        CHECK(list.items.y==list.frame.title.y+list.frame.title.height);
        CHECK(list.items.y+list.items.height<=list.frame.panel.y+list.frame.panel.height);
    }
    UIChoiceListLayout list=ui_choice_list_layout((Vector2){1000,620},(Vector2){1020,640},1.5f,11);
    CHECK(list.frame.valid && list.visible==9 && list.frame.panel.width==360 && list.frame.panel.height==438);
    CHECK(!ui_choice_list_layout((Vector2){0},(Vector2){1280,820},NAN,11).frame.valid);
    CHECK(!ui_choice_list_layout((Vector2){0},(Vector2){1280,820},1,0).frame.valid);
    CHECK(!ui_choice_list_layout((Vector2){0},(Vector2){1280,100},2,11).frame.valid);
    printf("PASS: %d choice-menu checks (exact scale/density/ribbon geometry, separators, anchors, narrow windows and 80/100/150/250 percent)\n",checks); return 0;
}
