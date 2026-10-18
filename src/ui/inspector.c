#include "inspector.h"
#include <math.h>
static Rectangle intersection(Rectangle a,Rectangle b) {
    float x=fmaxf(a.x,b.x),y=fmaxf(a.y,b.y);
    return (Rectangle){x,y,fmaxf(0,fminf(a.x+a.width,b.x+b.width)-x),fmaxf(0,fminf(a.y+a.height,b.y+b.height)-y)};
}
static float caption_width(float advance,float scale) {
    /* Frozen Plex Sans11 labels use1/64-point layout advances. Keep the
       observed ceil after scaling; Steps at150% lands between those units. */
    return (float)(ceil((double)advance*scale*64)/64);
}
UIInspectorLayout ui_inspector_layout_scrolled(Rectangle panel,float scale,size_t count,bool sql,float outer,float inner) {
    UIInspectorLayout out={0};
    if (!isfinite(scale) || scale<=0 || !isfinite(panel.x) || !isfinite(panel.y) || !isfinite(panel.width) || !isfinite(panel.height) || !isfinite(outer) || !isfinite(inner)) return out;
    float width=panel.width/scale,height=panel.height/scale;
    if (!isfinite(width) || !isfinite(height) || width<160 || height<(sql ? 35 : 220)) return out;
    out.compact=height<360; out.row_height=55;
    if (sql) {
        /* Frozen SQL flex minimum: header/source do not shrink, description
           keeps22 units of padding plus its font11 line box, and the footer
           keeps24 units of padding,27-unit buttons and a one-point border.
           Only the pre frame shrinks to zero before the aside overflows.
           Wrapped text and its conditional inner scrollbar remain App-owned. */
        out.row_height=height<300 ? 40 : 55;
        float line=ceilf(15*scale)-1,description=22*scale+line,footer=51*scale+1;
        float fixed=69*scale+description+footer;
        if (!isfinite(fixed) || !isfinite(panel.x+panel.width) || !isfinite(panel.y+panel.height)) return (UIInspectorLayout){0};
        out.outer_max=fmaxf(0,(fixed-panel.height)/scale); out.outer_scrollable=out.outer_max>0;
        out.outer_offset=fmaxf(0,fminf(outer,out.outer_max));
        float lane=out.outer_scrollable ? floorf(9*scale) : 0,content_x=panel.x+1,content_width=panel.width-1-lane;
        float right=content_x+content_width,shift=out.outer_offset*scale;
        out.header=(Rectangle){content_x,panel.y,content_width,35*scale};
        out.body_clip=(Rectangle){content_x,panel.y+35*scale,content_width,fmaxf(0,panel.height-35*scale)};
        /* title is a row paint slot, not the strong element's glyph box. */
        out.title=(Rectangle){content_x+13*scale,panel.y,content_width-71*scale,35*scale};
        out.collapse=(Rectangle){right-56*scale,panel.y+6*scale-.5f,23*scale,23*scale};
        out.dismiss=(Rectangle){right-31*scale,panel.y+6*scale-.5f,23*scale,23*scale};
        out.source=(Rectangle){content_x,panel.y+35*scale-shift,content_width,34*scale};
        float description_y=out.source.y+out.source.height;
        out.sql_description=(Rectangle){content_x+13*scale,description_y+11*scale,content_width-26*scale,line};
        out.sql_frame=(Rectangle){content_x,description_y+description,content_width,fmaxf(0,panel.height-fixed)};
        out.sql_code=(Rectangle){content_x+14*scale,out.sql_frame.y,fmaxf(0,content_width-28*scale),out.sql_frame.height};
        out.footer=(Rectangle){content_x,out.sql_frame.y+out.sql_frame.height,content_width,footer};
        float copy_width=37*scale+2+caption_width(47.09375f,scale),steps_width=37*scale+2+caption_width(27.984375f,scale);
        out.copy=(Rectangle){content_x+12*scale,out.footer.y+1+12*scale,copy_width,27*scale};
        out.steps=(Rectangle){right-12*scale-steps_width,out.copy.y,steps_width,27*scale};
        if (out.outer_scrollable) out.outer_scrollbar=(Rectangle){right,panel.y,lane,panel.height};
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
