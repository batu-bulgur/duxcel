#include "ui/popup.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"popup check %d failed: %s\n",checks,#x); exit(1); } } while(0)
int main(void) {
    UIPopupPresence p={0};
    ui_popup_presence(&p,false,10,false); CHECK(!p.active && !p.visible && p.alpha==0);
    ui_popup_presence(&p,true,11,false); CHECK(p.active && p.visible && p.alpha==0 && p.offset==-4);
    ui_popup_presence(&p,true,11.055,false);
    CHECK(fabsf(p.alpha-ui_motion_ease_out(.5f))<.00001f);
    CHECK(fabsf(p.offset-(-4+4*ui_motion_ease(.055f/.120f)))<.00001f);
    ui_popup_presence(&p,true,11.121,false); CHECK(p.alpha==1 && p.offset==0);
    ui_popup_presence(&p,false,12,false); CHECK(!p.active && p.visible && p.alpha==1 && p.offset==0);
    ui_popup_presence(&p,false,12.055,false); CHECK(p.alpha>0 && p.alpha<1 && p.offset<0);
    float alpha=p.alpha,offset=p.offset;
    ui_popup_presence(&p,true,12.055,false); CHECK(p.active && p.alpha==alpha && p.offset==offset);
    ui_popup_presence(&p,true,12.18,false); CHECK(p.alpha==1 && p.offset==0);
    ui_popup_presence(&p,false,13,false); ui_popup_presence(&p,false,13.121,false);
    CHECK(!p.active && !p.visible && p.alpha==0 && p.offset==-3);
    ui_popup_presence(&p,true,14,false); CHECK(p.alpha==0 && p.offset==-4);
    ui_popup_presence(&p,true,14,true); CHECK(p.alpha==1 && p.offset==0);
    ui_popup_presence(&p,false,14,true); CHECK(!p.visible && p.alpha==0);
    for (int hz=30;hz<=240;hz+=30) {
        UIPopupPresence q={0}; ui_popup_presence(&q,false,0,false); ui_popup_presence(&q,true,1,false);
        for (int i=1;i<=hz;i++) { ui_popup_presence(&q,true,1+(double)i/hz,false); CHECK(q.alpha>=0 && q.alpha<=1 && q.offset>=-4 && q.offset<=0); }
        CHECK(q.alpha==1 && q.offset==0);
    }
    printf("PASS: %d popup fade/translation timing, interruption, input lifecycle, reduced motion and refresh-rate checks\n",checks);
    return 0;
}
