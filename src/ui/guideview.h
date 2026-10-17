#ifndef DX_GUIDE_VIEW_H
#define DX_GUIDE_VIEW_H
#include "textview.h"
typedef struct {
    Rectangle panel,clip,header,close,intro,names[5],descriptions[5],shortcuts,labels[6],keys[6],note,start;
    float content_height;
    bool valid;
} UIGuideLayout;
typedef struct {
    TextLines names[5],descriptions[5],labels[6],intro,note;
    UIGuideLayout layout;
    UIScrollbar scrollbar;
    float scroll;
    int focus;
} UIGuide;
UIGuideLayout ui_guide_layout(Vector2 viewport,float scale,const float heights[5],const float shortcut_heights[6],float intro_height,float note_height,float key_width);
bool ui_guide_draw(UIContext *ui,UIGuide *guide,bool pointer);
void ui_guide_free(UIGuide *guide);
#endif
