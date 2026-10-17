#include "inspector.h"
#include <math.h>
static Rectangle rect(Rectangle panel,float scale,float x,float y,float width,float height) {
    return (Rectangle){panel.x+x*scale,panel.y+y*scale,fmaxf(0,width)*scale,fmaxf(0,height)*scale};
}
UIInspectorLayout ui_inspector_layout(Rectangle panel,float scale,size_t count,bool sql) {
    UIInspectorLayout out={0};
    if (!isfinite(scale) || scale<=0 || !isfinite(panel.x) || !isfinite(panel.y) || !isfinite(panel.width) || !isfinite(panel.height)) return out;
    float width=panel.width/scale,height=panel.height/scale;
    if (width<160 || height<220) return out;
    out.compact=height<360; out.row_height=height<300 ? 40 : 55;
    out.title=rect(panel,scale,13,0,width-75,35);
    out.collapse=rect(panel,scale,width-52,6,23,23); out.dismiss=rect(panel,scale,width-27,6,23,23);
    out.source=rect(panel,scale,13,35,width-26,34);
    if (sql) {
        out.sql_description=rect(panel,scale,13,69,width-26,32);
        out.sql_code=rect(panel,scale,14,101,width-28,height-152);
        out.footer=rect(panel,scale,0,height-51,width,51);
        out.copy=rect(panel,scale,12,height-39,(width-32)/2,27);
        out.steps=rect(panel,scale,20+(width-32)/2,height-39,(width-32)/2,27);
    } else {
        float foot=height<300 ? 40 : out.compact ? 52 : 70,detail=height<300 ? 64 : out.compact ? 86 : 112;
        float available=height-foot-detail-85,list=fminf((float)fmin((double)count,1000000)*out.row_height,fmaxf(0,available));
        out.list=rect(panel,scale,7,76,width-14,list);
        float y=76+list+9;
        out.details=rect(panel,scale,0,y,width,detail);
        float padding=out.compact ? 8 : 16;
        out.detail_title=rect(panel,scale,13,y+padding,width-26,15);
        float description_height=height<300 ? 0 : 16;
        out.description=rect(panel,scale,13,y+padding+24,width-26,description_height);
        float actions=y+detail-padding-27,action_width=(width-34)/2;
        out.edit=rect(panel,scale,13,actions,action_width,27); out.preview=rect(panel,scale,21+action_width,actions,action_width,27);
        out.footer=rect(panel,scale,0,height-foot,width,foot);
        out.inspect=rect(panel,scale,13,height-foot+10,width-26,20);
        out.note=rect(panel,scale,13,height-foot+39,width-26,foot>=70 ? 15 : 0);
    }
    out.valid=true; return out;
}
