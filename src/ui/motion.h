#ifndef DUXCEL_MOTION_H
#define DUXCEL_MOTION_H
#include <stdbool.h>
typedef struct {
    float value,from,target,elapsed,duration;
    double sampled_at;
    bool initialized,clock_initialized,standard_ease,ease_out;
} UIMotion;
/* The definition's cubic-bezier(.22,.8,.25,1), evaluated by elapsed time. */
float ui_motion_ease(float progress);
float ui_motion_ease_out(float progress);
float ui_motion_standard_ease(float progress);
/* seconds belongs to the preceding sample; a new target starts at time zero. */
float ui_motion_update(UIMotion *motion,float target,float seconds,float duration,bool immediate);
float ui_motion_update_at(UIMotion *motion,float target,double now,float duration,bool immediate);
#endif
