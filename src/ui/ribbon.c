#include "ribbon.h"
#include <math.h>
#include <string.h>

float ui_ribbon_height(int mode) { return mode==0 ? 80 : mode==1 ? 36 : mode==2 ? 32 : 0; }
int ui_ribbon_group_for(const UIRibbonGroup *groups,int count,int command) {
    if (!groups || count<0 || count>UI_RIBBON_GROUPS || command<0 || command>=UI_RIBBON_COMMANDS) return -1;
    for (int g=0;g<count;g++) if (command>=groups[g].first && command<groups[g].first+groups[g].count) return g;
    return -1;
}
static float occupied(const UIRibbonLayout *layout,const float full[],const float compact[]) {
    float width=layout->overflow_count ? 40 : 0;
    for (int g=0;g<layout->group_count;g++) if (layout->states[g]!=UI_RIBBON_OVERFLOW)
        width+=(layout->states[g]==UI_RIBBON_EXPANDED ? full[g] : compact[g])+12;
    return width;
}
UIRibbonLayout ui_ribbon_layout_custom(float width,int mode,const UIRibbonGroup *groups,int count,const float labels[UI_RIBBON_COMMANDS],const float names[UI_RIBBON_GROUPS],const float expanded[UI_RIBBON_GROUPS],const Rectangle cells[UI_RIBBON_COMMANDS]) {
    UIRibbonLayout out={0};
    if (!isfinite(width) || width<64 || mode<0 || mode>3 || !groups || count<1 || count>UI_RIBBON_GROUPS || !labels || !names) return out;
    unsigned used=0; float full[UI_RIBBON_GROUPS]={0},compact[UI_RIBBON_GROUPS]={0},command[UI_RIBBON_COMMANDS]={0};
    out.group_count=count; out.height=ui_ribbon_height(mode);
    for (int g=0;g<count;g++) {
        int first=groups[g].first,n=groups[g].count;
        if (first<0 || n<1 || first>=UI_RIBBON_COMMANDS || n>UI_RIBBON_COMMANDS-first || !isfinite(names[g]) || names[g]<0) return (UIRibbonLayout){0};
        for (int c=first;c<first+n;c++) {
            if ((used&(1u<<c)) || !isfinite(labels[c]) || labels[c]<0) return (UIRibbonLayout){0};
            used|=1u<<c;
            command[c]=mode==2 ? 28 : mode==0 && groups[g].prominent ? fmaxf(64,labels[c]+18) : fmaxf(56,labels[c]+39);
            full[g]+=command[c]+4;
        }
        full[g]+=mode==0 ? 0 : 17; /* Full-mode launcher lives below controls. */
        if (mode!=2) full[g]=fmaxf(full[g],names[g]+30);
        if (expanded) {
            if (!cells || !isfinite(expanded[g]) || expanded[g]<32) return (UIRibbonLayout){0};
            /* occupied() includes a 12-unit inter-group gutter. */
            full[g]=expanded[g]-12;
            for (int c=first;c<first+n;c++) {
                Rectangle r=cells[c];
                if (!isfinite(r.x) || !isfinite(r.y) || !isfinite(r.width) || !isfinite(r.height) || r.x<0 || r.y<0 || r.width<=0 || r.height<=0 || r.x+r.width>full[g] || r.y+r.height>(mode==0 ? 58 : 28)) return (UIRibbonLayout){0};
            }
        }
        compact[g]=mode==2 ? 40 : mode==0 ? fmaxf(53,names[g]+12) : names[g]+44;
    }
    if (mode==3) { out.valid=true; return out; }
    float available=width-12;
    /* Monotonic two-step degradation keeps explicit display mode unchanged. */
    for (int pass=0;pass<2;pass++) while (occupied(&out,full,compact)>available) {
        int choice=-1;
        for (int g=0;g<count;g++) {
            bool eligible=pass==0 ? out.states[g]==UI_RIBBON_EXPANDED && compact[g]<full[g] : out.states[g]!=UI_RIBBON_OVERFLOW;
            if (eligible && (choice<0 || groups[g].priority<groups[choice].priority)) choice=g;
        }
        if (choice<0) break;
        if (!pass) out.states[choice]=UI_RIBBON_COLLAPSED;
        else { out.states[choice]=UI_RIBBON_OVERFLOW; out.overflow_count++; }
    }
    float x=6,y=mode==0 ? 5 : mode==1 ? 4 : 2;
    for (int g=0;g<count;g++) {
        if (out.states[g]==UI_RIBBON_OVERFLOW) continue;
        float w=out.states[g]==UI_RIBBON_EXPANDED ? full[g] : compact[g];
        float control_height=mode==0 && groups[g].prominent ? 52 : 28,control_y=mode==0 && !groups[g].prominent ? 18 : y;
        out.groups[g]=(Rectangle){x,y,w,mode==0 ? 66 : control_height};
        if (out.states[g]==UI_RIBBON_EXPANDED) {
            float cx=x;
            for (int c=groups[g].first;c<groups[g].first+groups[g].count;c++) {
                out.commands[c]=(Rectangle){cx,control_y,command[c],control_height}; cx+=command[c]+4;
                if (expanded) { out.commands[c]=cells[c]; out.commands[c].x+=x; out.commands[c].y+=y; }
            }
            out.launchers[g]=mode==0 ? (Rectangle){x+w-10,58,20,20} : (Rectangle){x+w-17,y,17,control_height};
        }
        x+=w+12;
    }
    if (out.overflow_count) out.overflow=(Rectangle){width-38,mode==0 ? 18 : y,32,28};
    out.valid=true; return out;
}
UIRibbonLayout ui_ribbon_layout(float width,int mode,const UIRibbonGroup *groups,int count,const float labels[UI_RIBBON_COMMANDS],const float names[UI_RIBBON_GROUPS]) {
    return ui_ribbon_layout_custom(width,mode,groups,count,labels,names,NULL,NULL);
}
