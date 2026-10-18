#include "inspector.h"
#include <math.h>
static Rectangle rect(Rectangle panel,float scale,float x,float y,float width,float height) {
    return (Rectangle){panel.x+x*scale,panel.y+y*scale,fmaxf(0,width)*scale,fmaxf(0,height)*scale};
}
static Rectangle intersection(Rectangle a,Rectangle b) {
    float x=fmaxf(a.x,b.x),y=fmaxf(a.y,b.y);
    return (Rectangle){x,y,fmaxf(0,fminf(a.x+a.width,b.x+b.width)-x),fmaxf(0,fminf(a.y+a.height,b.y+b.height)-y)};
}
UIInspectorLayout ui_inspector_layout_scrolled(Rectangle panel,float scale,size_t count,bool sql,float outer,float inner) {
    UIInspectorLayout out={0};
    if (!isfinite(scale) || scale<=0 || !isfinite(panel.x) || !isfinite(panel.y) || !isfinite(panel.width) || !isfinite(panel.height) || !isfinite(outer) || !isfinite(inner)) return out;
    float width=panel.width/scale,height=panel.height/scale;
    if (!isfinite(width) || !isfinite(height) || width<160 || height<220) return out;
    out.compact=height<360; out.row_height=55;
    if (sql) {
        /* Preserve the existing SQL layout; its scrolling remains independent. */
        out.row_height=height<300 ? 40 : 55;
        out.header=rect(panel,scale,0,0,width,35);
        out.title=rect(panel,scale,13,0,width-75,35);
        out.collapse=rect(panel,scale,width-56,6,23,23); out.dismiss=rect(panel,scale,width-31,6,23,23);
        out.collapse.y-=.5f; out.dismiss.y-=.5f;
        out.source=rect(panel,scale,13,35,width-26,34);
        out.sql_description=rect(panel,scale,13,69,width-26,32);
        out.sql_code=rect(panel,scale,14,101,width-28,height-152);
        out.footer=rect(panel,scale,0,height-51,width,51);
        out.copy=rect(panel,scale,12,height-39,(width-32)/2,27);
        out.steps=rect(panel,scale,20+(width-32)/2,height-39,(width-32)/2,27);
        out.valid=true; return out;
    }
    /* CSS keeps the list minimum and unshrunk details. Overflow belongs to
       the entire inspector; rows retain their own independent list scroll. */
    float natural=(7+55*(float)count)*scale,rows=55*(float)count*scale;
    float detail=97*scale+1+ceilf(15*scale),foot=56*scale+1+floorf(13*scale);
    float fixed=78*scale+detail+foot;
    float list=fmaxf(155*scale,fminf(natural,panel.height-fixed)),full=fixed+list;
    if (!isfinite(natural) || !isfinite(rows) || !isfinite(full)) return (UIInspectorLayout){0};
    out.outer_max=fmaxf(0,(full-panel.height)/scale); out.outer_scrollable=out.outer_max>0;
    out.inner_max=fmaxf(0,(natural-list)/scale); out.inner_scrollable=out.inner_max>0;
    out.outer_offset=fmaxf(0,fminf(outer,out.outer_max)); out.inner_offset=fmaxf(0,fminf(inner,out.inner_max));
    float outer_lane=out.outer_scrollable ? floorf(9*scale) : 0,inner_lane=out.inner_scrollable ? floorf(9*scale) : 0;
    float content_x=panel.x+1,content_width=panel.width-1-outer_lane,shift=out.outer_offset*scale;
    if (content_width<=34*scale || !isfinite(panel.x+panel.width) || !isfinite(panel.y+panel.height)) return (UIInspectorLayout){0};
    out.header=(Rectangle){content_x,panel.y,content_width,35*scale};
    out.body_clip=(Rectangle){content_x,panel.y+35*scale,content_width,panel.height-35*scale};
    out.title=(Rectangle){content_x+13*scale,panel.y,content_width-75*scale,35*scale};
    float right=content_x+content_width;
    out.collapse=(Rectangle){right-56*scale,panel.y+6*scale-.5f,23*scale,23*scale};
    out.dismiss=(Rectangle){right-31*scale,panel.y+6*scale-.5f,23*scale,23*scale};
    out.source=(Rectangle){content_x,panel.y+35*scale-shift,content_width,34*scale};
    out.list_frame=(Rectangle){content_x,panel.y+69*scale-shift,content_width,list};
    out.list=intersection((Rectangle){out.list_frame.x,out.list_frame.y,content_width-inner_lane,list},out.body_clip);
    out.list_origin=(Rectangle){content_x+7*scale,out.list_frame.y+7*scale-out.inner_offset*scale,content_width-14*scale-inner_lane,rows};
    /* Preserve full translated tracks. The owner clips paint/hit to body_clip
       without changing thumb geometry when the sticky title hides the list. */
    if (out.inner_scrollable) out.list_scrollbar=(Rectangle){right-inner_lane,out.list_frame.y,inner_lane,list};
    if (out.outer_scrollable) out.outer_scrollbar=(Rectangle){right,panel.y,outer_lane,panel.height};
    float y=out.list_frame.y+list+9*scale;
    out.details=(Rectangle){content_x,y,content_width,detail};
    out.detail_title=(Rectangle){content_x+13*scale,y+1+16*scale,content_width-26*scale,ceilf(15*scale)};
    out.description=(Rectangle){out.detail_title.x,out.detail_title.y+out.detail_title.height+9*scale,out.detail_title.width,16*scale};
    float actions=out.description.y+out.description.height+13*scale,action_width=(content_width-34*scale)/2;
    out.edit=(Rectangle){out.detail_title.x,actions,action_width,27*scale};
    out.preview=(Rectangle){out.edit.x+action_width+8*scale,actions,action_width,27*scale};
    float footer_y=fmaxf(panel.y+panel.height-foot-shift,y+detail);
    out.footer=(Rectangle){content_x,footer_y,content_width,foot};
    out.inspect=(Rectangle){content_x+13*scale,footer_y+1+12*scale,content_width-26*scale,20*scale};
    out.note=(Rectangle){out.inspect.x,out.inspect.y+out.inspect.height+9*scale,out.inspect.width,floorf(13*scale)};
    out.valid=true; return out;
}
UIInspectorLayout ui_inspector_layout(Rectangle panel,float scale,size_t count,bool sql) {
    return ui_inspector_layout_scrolled(panel,scale,count,sql,0,0);
}
