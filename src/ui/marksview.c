#include "marksview.h"
#include <math.h>
static Rectangle mark_rect(Rectangle panel,float scale,float offset,float x,float y,float width,float height) {
    return (Rectangle){panel.x+x*scale,panel.y+(y-offset)*scale,width*scale,height*scale};
}
static bool content_valid(float width,const UIMarksContent *content) {
    if (!content || !isfinite(width) || width<80 || width>32768 || content->measures>UI_MARKS_FIELDS || content->filters>UI_MARKS_FIELDS) return false;
    for (unsigned i=0;i<content->filters;i++) if (!isfinite(content->filter_widths[i]) || content->filter_widths[i]<0 || content->filter_widths[i]>32768) return false;
    return true;
}
static float arrange(UIMarksLayout *out,Rectangle panel,float scale,const UIMarksContent *content,float offset,float width) {
    float y=1,inner=width-20,row_height[2]={40,40};
    out->heading=mark_rect(panel,scale,offset,10,y,inner,29); y+=29;
    out->representation=mark_rect(panel,scale,offset,10,y,inner,26); y+=26;
    out->measure_heading=mark_rect(panel,scale,offset,10,y,inner,24); y+=24;
    float list_height=content->measures ? content->measures*23+(content->measures-1)*3 : 23;
    out->measure_list=mark_rect(panel,scale,offset,10,y,inner,list_height);
    for (unsigned i=0;i<content->measures;i++) out->measures[i]=mark_rect(panel,scale,offset,10,y+i*26,inner,23);
    y+=list_height;
    if (content->plot) { y+=5; out->plot=mark_rect(panel,scale,offset,10,y,inner,22); y+=22; }
    y+=6;
    for (unsigned i=0;i<UI_MARKS_ROLES;i++) if (content->assigned[i]) row_height[i/3]=47;
    float target_width=(inner-6)/3;
    for (unsigned i=0;i<UI_MARKS_ROLES;i++) out->targets[i]=mark_rect(panel,scale,offset,10+(i%3)*(target_width+3),y+(i/3)*(row_height[0]+3),target_width,content->assigned[i] ? 47 : 40);
    y+=row_height[0]+3+row_height[1]+2;
    out->filter_heading=mark_rect(panel,scale,offset,10,y,inner,24); y+=24;
    if (!content->filters) { out->empty_filter=mark_rect(panel,scale,offset,10,y,inner,23); y+=23; }
    else {
        float x=0; unsigned lines=1,rows[UI_MARKS_FIELDS]={0}; float widths[UI_MARKS_FIELDS]={0},left[UI_MARKS_FIELDS]={0};
        for (unsigned i=0;i<content->filters;i++) {
            float w=fminf(inner,fmaxf(40,content->filter_widths[i]));
            if (x && x+w>inner+.001f) { lines++; x=0; }
            widths[i]=w; left[i]=x; rows[i]=lines-1; x+=w+3;
        }
        float height=lines==1 ? 23 : 20.175f;
        for (unsigned i=0;i<content->filters;i++) out->filters[i]=mark_rect(panel,scale,offset,10+left[i],y+rows[i]*(height+3),widths[i],height);
        y+=lines*height+(lines-1)*3;
    }
    y+=5; out->inherited=mark_rect(panel,scale,offset,10,y,inner,13.5f); y+=13.5f+7;
    out->content_width=inner*scale; return y;
}
float ui_marks_content_height(float width,const UIMarksContent *content) {
    if (!content_valid(width,content)) return 0;
    UIMarksLayout out={0}; return arrange(&out,(Rectangle){0},1,content,0,width);
}
UIMarksLayout ui_marks_layout(Rectangle panel,float scale,const UIMarksContent *content,float offset) {
    UIMarksLayout out={0};
    if (!isfinite(scale) || scale<=0 || scale>8 || !isfinite(offset) || !isfinite(panel.x) || !isfinite(panel.y) || !isfinite(panel.width) || !isfinite(panel.height) || fabsf(panel.x)>32768 || fabsf(panel.y)>32768 || panel.width>32768 || panel.height>32768 || panel.height<=0 || !content_valid(panel.width/scale,content)) return out;
    float width=panel.width/scale,full=arrange(&out,panel,scale,content,0,width),height=panel.height/scale;
    out.scrollable=full>height+.001f;
    if (out.scrollable) { width-=9; full=arrange(&out,panel,scale,content,0,width); }
    out.panel=panel; out.viewport=panel; out.content_height=full; out.maximum=fmaxf(0,full-height); out.offset=fmaxf(0,fminf(out.maximum,offset));
    arrange(&out,panel,scale,content,out.offset,width);
    if (out.scrollable) out.scrollbar=(Rectangle){panel.x+panel.width-9*scale,panel.y+scale,9*scale,fmaxf(0,panel.height-scale)};
    out.valid=true; return out;
}
