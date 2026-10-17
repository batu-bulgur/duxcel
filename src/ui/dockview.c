#include "dockview.h"
#include <math.h>
UIWorkbookList ui_workbook_list(float maximum,size_t count,bool compact) {
    UIWorkbookList out={0};
    if (!isfinite(maximum)||maximum<0||maximum>32768) return out;
    out.stride=compact && count ? fmaxf(21,fminf(29,(maximum+1)/(float)count)) : 29;
    out.row_height=out.stride-1;
    out.visible=(size_t)floorf((maximum+1)/out.stride);
    if (!compact && maximum-out.visible*out.stride>=24) out.visible++;
    if (out.visible>count) out.visible=count;
    out.height=count ? fminf(maximum,(float)count*out.stride-1) : 0;
    out.valid=true; return out;
}
UIDockLayout ui_dock_layout_fraction(Rectangle viewport,float scale,size_t sheets,bool workbook,float workbook_open,bool fields,bool fields_collapsed,bool authoring,float marks_height,float offset) {
    UIDockLayout out={0};
    if (!isfinite(scale)||scale<=0||!isfinite(viewport.x)||!isfinite(viewport.y)||!isfinite(viewport.width)||!isfinite(viewport.height)||viewport.width<100*scale||viewport.height<40*scale||!isfinite(marks_height)||marks_height<0||!isfinite(offset)||!isfinite(workbook_open)||workbook_open<0||workbook_open>1) return out;
    bool compact=viewport.height<=700*scale,shared=fields||authoring;
    float sheet_height=sheets ? (float)fmin((double)sheets*29-1,compact && shared ? 84 : fmaxf(28,(shared ? fminf(190,fmaxf(66,viewport.height/scale*.27f)) : viewport.height/scale)-66)) : 0;
    float book=workbook ? 36+(30+sheet_height)*workbook_open : 0;
    float minimum=book+(fields ? fields_collapsed && !authoring ? 29 : 160 : 0)+(authoring ? fminf(marks_height,compact ? 245 : marks_height) : 0);
    float height=fmaxf(viewport.height,minimum*scale);
    out.maximum=fmaxf(0,(height-viewport.height)/scale); out.offset=fmaxf(0,fminf(out.maximum,offset));
    out.viewport=viewport; out.content=(Rectangle){viewport.x,viewport.y-out.offset*scale,fmaxf(0,viewport.width-(out.maximum>0 ? 9*scale : 0)),height}; out.compact=compact; out.valid=true; return out;
}
UIDockLayout ui_dock_layout(Rectangle viewport,float scale,size_t sheets,bool workbook,bool collapsed,bool fields,bool fields_collapsed,bool authoring,float marks_height,float offset) {
    return ui_dock_layout_fraction(viewport,scale,sheets,workbook,collapsed ? 0 : 1,fields,fields_collapsed,authoring,marks_height,offset);
}
float ui_dock_reveal(UIDockLayout layout,Rectangle target,float scale) {
    if (!layout.valid||!isfinite(scale)||scale<=0||!isfinite(target.y)||!isfinite(target.height)||target.height<=0) return layout.offset;
    float offset=layout.offset;
    if (target.y<layout.viewport.y) offset+=(target.y-layout.viewport.y)/scale;
    else if (target.y+target.height>layout.viewport.y+layout.viewport.height) offset+=(target.y+target.height-layout.viewport.y-layout.viewport.height)/scale;
    return fmaxf(0,fminf(layout.maximum,offset));
}
