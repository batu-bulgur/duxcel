#ifndef DUXCEL_STATUSVIEW_H
#define DUXCEL_STATUSVIEW_H
#include "ui.h"
typedef struct {
    Rectangle panel,context[3],metrics,customize,scale,zoom,minus,level,plus;
    bool scale_text,valid;
} UIStatusLayout;
/* Widths are measured at 12 units for context and 11 for the scale label. */
UIStatusLayout ui_status_layout(Vector2 viewport,float scale,const float context_widths[3],float scale_width);
typedef struct { char labels[2048]; size_t offsets[8]; float widths[8],width; unsigned count; } UIStatusMetrics;
void ui_status_metrics_layout(UIContext *ui,const char *chosen,UIStatusMetrics *layout);
void ui_status_metrics_draw(UIContext *ui,const UIStatusMetrics *layout,Rectangle area,Color color);
#endif
