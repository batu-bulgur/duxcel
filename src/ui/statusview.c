#include "statusview.h"
#include <math.h>
#include <string.h>
UIStatusLayout ui_status_layout(Vector2 viewport,float scale,const float widths[3],float scale_width) {
    UIStatusLayout out={0};
    if (!widths||!isfinite(scale)||scale<=0||!isfinite(viewport.x)||!isfinite(viewport.y)||viewport.x/scale<240||viewport.y/scale<100||!isfinite(scale_width)||scale_width<0) return out;
    for (int i=0;i<3;i++) if (!isfinite(widths[i])||widths[i]<0) return out;
    float width=viewport.x/scale,y=viewport.y/scale-22,zoom_x=width-97;
    out.panel=(Rectangle){0,y*scale,viewport.x,22*scale};
    out.zoom=(Rectangle){zoom_x*scale,(y+2)*scale,92*scale,19*scale};
    out.minus=(Rectangle){(zoom_x+1)*scale,(y+3)*scale,23*scale,17*scale};
    out.level=(Rectangle){(zoom_x+24)*scale,(y+3)*scale,44*scale,17*scale};
    out.plus=(Rectangle){(zoom_x+68)*scale,(y+3)*scale,23*scale,17*scale};
    out.scale_text=width>=620;
    float control=out.scale_text ? scale_width+(width>=900 ? 31 : 27) : 23;
    out.scale=(Rectangle){(zoom_x-2-control)*scale,(y+1.5f)*scale,control*scale,20*scale};
    out.customize=(Rectangle){out.scale.x-23*scale,(y+1.5f)*scale,23*scale,20*scale};
    float x=12,limit=out.customize.x/scale-24;
    for (int i=0;i<3;i++) {
        bool show=i==0 || (width>= (i==1 ? 620 : 900) && widths[i]>0);
        float occupied=widths[i]+(i ? 23 : 12);
        if (!show || (i && x+occupied>limit)) continue;
        occupied=fmaxf(0,fminf(occupied,limit-x));
        out.context[i]=(Rectangle){x*scale,(y+1)*scale,occupied*scale,21*scale}; x+=occupied;
    }
    out.metrics=(Rectangle){x*scale,(y+1)*scale,fmaxf(0,out.customize.x-x*scale),21*scale};
    out.valid=true; return out;
}
void ui_status_metrics_layout(UIContext *ui,const char *chosen,UIStatusMetrics *out) {
    memset(out,0,sizeof(*out)); if (!chosen) return; size_t used=0;
    while (*chosen && out->count<8) {
        const char *next=strstr(chosen," · "); size_t length=next ? (size_t)(next-chosen) : strlen(chosen);
        if (length && length<sizeof(out->labels)-used) {
            unsigned index=out->count++; out->offsets[index]=used;
            memcpy(out->labels+used,chosen,length); out->labels[used+length]=0;
            out->widths[index]=ui_measure(ui,out->labels+used,12); out->width+=out->widths[index]+(index ? 15*ui->scale : 0); used+=length+1;
        }
        if (!next) break; chosen=next+strlen(" · ");
    }
}
void ui_status_metrics_draw(UIContext *ui,const UIStatusMetrics *layout,Rectangle area,Color color) {
    float x=area.x+area.width-8*ui->scale-layout->width;
    for (unsigned i=0;i<layout->count;i++) {
        ui_label(ui,layout->labels+layout->offsets[i],(Rectangle){x,area.y,layout->widths[i]+.001f*ui->scale,area.height},12,color,false,UI_REGULAR);
        x+=layout->widths[i]+15*ui->scale;
    }
}
