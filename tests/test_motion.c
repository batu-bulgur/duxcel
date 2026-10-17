#include "ui/motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"motion check failed at %d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    CHECK(ui_motion_ease(0)==0); CHECK(ui_motion_ease(1)==1); CHECK(ui_motion_ease(NAN)==0);
    CHECK(ui_motion_ease_out(0)==0); CHECK(ui_motion_ease_out(1)==1); CHECK(ui_motion_ease_out(NAN)==0);
    CHECK(fabsf(ui_motion_ease_out(.5f)-.6846431874f)<.00001f);
    CHECK(fabsf(ui_motion_ease(.5f)-.9371895276f)<.00001f);
    CHECK(fabsf(ui_motion_standard_ease(.5f)-.8024033876f)<.00001f);
    float previous=0;
    for (int i=0;i<=1000;i++) { float v=ui_motion_ease(i/1000.0f); CHECK(v>=previous && v<=1); previous=v; }
    const float durations[]={.09f,.11f,.12f,.14f,.16f};
    for (int d=0;d<5;d++) for (int hz=30;hz<=240;hz*=2) {
        UIMotion m={0}; CHECK(ui_motion_update(&m,0,0,durations[d],false)==0);
        CHECK(ui_motion_update(&m,280,10,durations[d],false)==0 && m.elapsed==0);
        float time=0,last=0;
        while (time<durations[d]) { float dt=fminf(1.0f/hz,durations[d]-time); time+=dt; float v=ui_motion_update(&m,280,dt,durations[d],false); CHECK(v>=last && v<=280); CHECK(fabsf(v-280*ui_motion_ease(time/durations[d]))<.002f); last=v; }
        CHECK(m.value==280); CHECK(ui_motion_update(&m,280,100,durations[d],false)==280);
        CHECK(ui_motion_update(&m,0,.04f,durations[d],false)==280 && m.elapsed==0);
        float before=ui_motion_update(&m,0,.04f,durations[d],false); CHECK(before>=0 && before<280);
        CHECK(ui_motion_update(&m,140,0,durations[d],false)==before);
        CHECK(ui_motion_update(&m,140,NAN,durations[d],false)==before);
        CHECK(ui_motion_update(&m,NAN,1,durations[d],false)==before);
        CHECK(ui_motion_update(&m,140,1,durations[d],false)==140);
        CHECK(ui_motion_update(&m,17,0,durations[d],true)==17);
        CHECK(ui_motion_update(&m,23,0,0,false)==23);
    }
    UIMotion a={0},b={0}; ui_motion_update(&a,0,0,.16f,false); ui_motion_update(&b,0,0,.16f,false);
    ui_motion_update(&a,300,0,.16f,false); ui_motion_update(&b,300,0,.16f,false);
    for (int i=0;i<8;i++) { float slow=ui_motion_update(&a,300,.02f,.16f,false); ui_motion_update(&b,300,.01f,.16f,false); float fast=ui_motion_update(&b,300,.01f,.16f,false); CHECK(fabsf(slow-fast)<.001f); }
    UIMotion moving={0}; ui_motion_update(&moving,0,0,.16f,false); ui_motion_update(&moving,100,0,.16f,false);
    float last=0;
    for (int i=1;i<20;i++) { float value=ui_motion_update(&moving,100+i*10,.016f,.16f,false); CHECK(value>last && moving.elapsed==0); last=value; }
    UIMotion clock={0}; CHECK(ui_motion_update_at(&clock,0,10,.16f,false)==0);
    CHECK(ui_motion_update_at(&clock,280,10.5,.16f,false)==0 && clock.elapsed==0);
    CHECK(fabsf(ui_motion_update_at(&clock,280,10.516,.16f,false)-280*ui_motion_ease(.1f))<.001f);
    float painted=clock.value;
    CHECK(ui_motion_update_at(&clock,280,NAN,.16f,false)==painted && clock.sampled_at==10.516);
    CHECK(ui_motion_update_at(&clock,280,9,.16f,false)==painted && clock.sampled_at==10.516);
    CHECK(ui_motion_update_at(&clock,140,10.516,.16f,false)==painted && clock.elapsed==0);
    CHECK(ui_motion_update_at(&clock,140,11,.16f,false)==140);
    CHECK(ui_motion_update_at(&clock,280,12,.16f,true)==280 && clock.sampled_at==12);
    UIMotion standard={.standard_ease=true}; ui_motion_update_at(&standard,0,0,.12f,true);
    CHECK(standard.standard_ease); ui_motion_update_at(&standard,1,1,.12f,false);
    CHECK(fabsf(ui_motion_update_at(&standard,1,1.06,.12f,false)-ui_motion_standard_ease(.5f))<.00001f);
    printf("PASS: %d motion checks (exact easing, 90/110/120/140/160 ms, refresh-rate independence, interruptions, reduced motion and invalid clocks)\n",checks);
    return 0;
}
