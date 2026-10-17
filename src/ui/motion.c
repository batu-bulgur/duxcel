#include "motion.h"
#include <math.h>
#include <float.h>
static float cubic(float t,float a,float b) {
    float one=1-t; return 3*one*one*t*a+3*one*t*t*b+t*t*t;
}
static float curve(float progress,float x1,float y1,float x2,float y2) {
    if (!isfinite(progress) || progress<=0) return 0;
    if (progress>=1) return 1;
    float low=0,high=1;
    for (int i=0;i<18;i++) { float t=(low+high)*.5f; if (cubic(t,x1,x2)<progress) low=t; else high=t; }
    return cubic((low+high)*.5f,y1,y2);
}
float ui_motion_ease(float progress) { return curve(progress,.22f,.8f,.25f,1); }
float ui_motion_ease_out(float progress) { return curve(progress,0,0,.58f,1); }
float ui_motion_standard_ease(float progress) { return curve(progress,.25f,.1f,.25f,1); }
static void advance_motion(UIMotion *motion,float seconds) {
    if (!isfinite(motion->duration) || motion->duration<=0) { motion->value=motion->target; return; }
    if (isfinite(seconds) && seconds>0) motion->elapsed=fminf(motion->duration,motion->elapsed+seconds);
    float progress=motion->elapsed/motion->duration;
    motion->value=motion->elapsed>=motion->duration ? motion->target : motion->from+(motion->target-motion->from)*(motion->ease_out ? ui_motion_ease_out(progress) : motion->standard_ease ? ui_motion_standard_ease(progress) : ui_motion_ease(progress));
}
float ui_motion_update(UIMotion *motion,float target,float seconds,float duration,bool immediate) {
    if (!motion) return isfinite(target) ? target : 0;
    if (!isfinite(target)) return motion->initialized ? motion->value : 0;
    if (!motion->initialized || immediate || !isfinite(duration) || duration<=0) {
        bool standard=motion->standard_ease,ease_out=motion->ease_out;
        *motion=(UIMotion){.value=target,.from=target,.target=target,.duration=duration,.initialized=true,.standard_ease=standard,.ease_out=ease_out}; return target;
    }
    /* The preceding frame elapsed before this new target was sampled. Advance
       the old transition first, then start the new one from its current paint.
       This also keeps continuously retargeted ribbon indicators moving. */
    advance_motion(motion,seconds);
    if (target!=motion->target) { motion->from=motion->value; motion->target=target; motion->elapsed=0; motion->duration=duration; }
    return motion->value;
}
float ui_motion_update_at(UIMotion *motion,float target,double now,float duration,bool immediate) {
    if (!motion) return isfinite(target) ? target : 0;
    bool clock_valid=isfinite(now) && (!motion->clock_initialized || now>=motion->sampled_at);
    double delta=clock_valid && motion->clock_initialized ? now-motion->sampled_at : 0;
    float value=ui_motion_update(motion,target,delta>FLT_MAX ? FLT_MAX : (float)delta,duration,immediate);
    if (clock_valid) { motion->sampled_at=now; motion->clock_initialized=true; }
    return value;
}
