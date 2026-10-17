#ifndef DUXCEL_POPUP_H
#define DUXCEL_POPUP_H
#include "motion.h"
typedef struct {
    UIMotion opacity,translation;
    bool initialized,active,visible;
    float alpha,offset;
} UIPopupPresence;
/* Input belongs to active, never to the cached closing picture. */
void ui_popup_presence(UIPopupPresence *presence,bool active,double now,bool immediate);
#endif
