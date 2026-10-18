#ifndef DX_INSPECTOR_UI_H
#define DX_INSPECTOR_UI_H
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    Rectangle title,collapse,dismiss,source,list,details,detail_title,description,edit,preview,footer,inspect,note;
    Rectangle sql_description,sql_code,copy,steps;
    Rectangle header,body_clip,list_frame,list_origin,list_scrollbar,outer_scrollbar;
    float outer_offset,outer_max,inner_offset,inner_max;
    bool outer_scrollable,inner_scrollable;
    float row_height;
    bool compact,valid;
} UIInspectorLayout;
/* Presented rectangles, logical UI-unit offsets/limits. Steps preserves the
   frozen list minimum and details; short panes scroll the outer inspector.
   Header stays sticky; list_origin includes padding and independent inner
   scrolling. Full list_scrollbar is translated, not clipped: callers apply
   body_clip to paint/hit without changing its thumb geometry. SQL is unchanged. */
UIInspectorLayout ui_inspector_layout_scrolled(Rectangle panel,float scale,size_t count,bool sql,float outer,float inner);
/* Compatibility wrapper with both offsets zero. */
UIInspectorLayout ui_inspector_layout(Rectangle panel,float scale,size_t count,bool sql);
#endif
