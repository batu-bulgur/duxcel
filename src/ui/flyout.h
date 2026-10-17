#ifndef DUXCEL_FLYOUT_H
#define DUXCEL_FLYOUT_H
#include "ribbon_spec.h"
typedef struct {
    Rectangle panel,title,close,body,commands[UI_RIBBON_COMMANDS];
    float content_height;
    bool valid;
} UIFlyoutLayout;
UIFlyoutLayout ui_flyout_layout(Vector2 origin,Vector2 viewport,float scale,int tab,int group,const float labels[UI_RIBBON_COMMANDS]);
#endif
