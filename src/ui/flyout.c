#include "flyout.h"
#include <math.h>
UIFlyoutLayout ui_flyout_layout(Vector2 origin,Vector2 viewport,float scale,int tab,int group,const float labels[UI_RIBBON_COMMANDS]) {
    UIFlyoutLayout out={0};
    if (!labels||tab<0||tab>=RIBBON_TABS||group<0||group>=ribbon_group_counts[tab]||!isfinite(scale)||scale<=0||!isfinite(origin.x)||!isfinite(origin.y)||!isfinite(viewport.x)||!isfinite(viewport.y)) return out;
    float width=fminf(280,viewport.x/scale-12),available=width-26,body=40;
    if (available<160||viewport.y/scale<170) return out;
    const UIRibbonGroup *spec=&ribbon_groups[tab][group];
    for (int c=spec->first;c<spec->first+spec->count;c++) if (!isfinite(labels[c])||labels[c]<0) return out;
    Rectangle *r=out.commands;
    if (tab==RIBBON_HOME && group==0) {
        body=84; for (int c=0;c<3;c++) r[c]=(Rectangle){0,c*28,fminf(available,labels[c]+35),28};
    } else if (tab==RIBBON_HOME && group==1) {
        body=63; r[3]=(Rectangle){0,0,available-58,26}; r[4]=(Rectangle){available-53,0,53,26};
        for (int c=5;c<9;c++) r[c]=(Rectangle){(c-5)*28,33,27,30};
    } else if (tab==RIBBON_HOME && group==2) {
        for (int c=9;c<12;c++) r[c]=(Rectangle){(c-9)*28,0,27,30};
        r[12]=(Rectangle){90,0,fminf(available-90,labels[12]+39),30};
    } else if (tab==RIBBON_HOME && group==3) {
        body=63; r[13]=(Rectangle){0,0,available,26}; r[14]=(Rectangle){0,33,27,30}; r[15]=(Rectangle){28,33,33,30}; r[16]=(Rectangle){62,33,38,30};
    } else if (tab==RIBBON_HOME && group==4) {
        float sort=fmaxf(labels[17],labels[18])+39,filter=labels[19]+39,clear=labels[20]+39;
        r[17]=(Rectangle){0,0,sort,30}; r[18]=(Rectangle){0,30,sort,30};
        r[19]=(Rectangle){sort+7,0,filter,30}; r[20]=(Rectangle){sort+filter+14,0,clear,30}; body=60;
        if (r[20].x+clear>available) { r[20].x=sort+7; r[20].y=37; body=67; }
    } else {
        float x=0,y=0,row_height=0;
        for (int c=spec->first;c<spec->first+spec->count;c++) {
            float height=tab==RIBBON_HOME && group==5 ? 54 : 30;
            float w=fminf(available,labels[c]+(height==54 ? 18 : 39));
            if (x && x+w>available) { x=0; y+=row_height+7; row_height=0; }
            r[c]=(Rectangle){x,y,w,height}; x+=w+7; row_height=fmaxf(row_height,height);
        }
        body=fmaxf(40,y+row_height);
    }
    float height=62+body,shown=fminf(height,viewport.y/scale-16);
    float x=fmaxf(5,fminf(origin.x/scale,viewport.x/scale-width-6)),y=fmaxf(5,fminf(origin.y/scale,viewport.y/scale-fmaxf(160,shown)-10));
    out.panel=(Rectangle){x*scale,y*scale,width*scale,shown*scale}; out.title=(Rectangle){(x+11)*scale,(y+1)*scale,(width-54)*scale,36*scale};
    out.close=(Rectangle){(x+width-34)*scale,(y+3.5f)*scale,23*scale,30*scale}; out.body=(Rectangle){(x+1)*scale,(y+37)*scale,(width-2)*scale,(shown-38)*scale};
    for (int c=spec->first;c<spec->first+spec->count;c++) { r[c].x=(x+13+r[c].x)*scale; r[c].y=(y+49+r[c].y)*scale; r[c].width*=scale; r[c].height*=scale; }
    out.content_height=height; out.valid=true; return out;
}
