#ifndef DX_GUIDE_VIEW_H
#define DX_GUIDE_VIEW_H
#include "textview.h"
#ifdef DUXCEL_DEV_REVIEW
#define GUIDE_REVIEW_TARGETS 64
typedef enum {
    GUIDE_REVIEW_PANEL,GUIDE_REVIEW_HEADING,GUIDE_REVIEW_CLOSE,GUIDE_REVIEW_INTRO,GUIDE_REVIEW_NAME,GUIDE_REVIEW_DESCRIPTION,
    GUIDE_REVIEW_SHORTCUTS,GUIDE_REVIEW_LABEL,GUIDE_REVIEW_KEY,GUIDE_REVIEW_NOTE,GUIDE_REVIEW_START,GUIDE_REVIEW_SCROLL,GUIDE_REVIEW_THUMB
} GuideReviewKind;
typedef struct {
    GuideReviewKind kind; Rectangle bounds,clip; size_t text_line;
    bool enabled,focused,container,truncated;
    char key[32],text[256]; const char *file; uint32_t source_line;
} GuideReviewTarget;
/* The armed paint owns bounded visible text and geometry until App adapts this
   same frame. Shipping contains neither these records nor their identifiers. */
typedef struct {
    bool armed,overflow; size_t count; float scroll,maximum; int focus;
    GuideReviewTarget targets[GUIDE_REVIEW_TARGETS];
} GuideReviewFrame;
#endif
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
#ifdef DUXCEL_DEV_REVIEW
    GuideReviewFrame review;
#endif
} UIGuide;
UIGuideLayout ui_guide_layout(Vector2 viewport,float scale,const float heights[5],const float shortcut_heights[6],float intro_height,float note_height,float key_width);
bool ui_guide_draw(UIContext *ui,UIGuide *guide,bool pointer);
void ui_guide_free(UIGuide *guide);
#endif
