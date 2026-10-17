#ifndef DX_INSPECTOR_UI_H
#define DX_INSPECTOR_UI_H
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    Rectangle title,collapse,dismiss,source,list,details,detail_title,description,edit,preview,footer,inspect,note;
    Rectangle sql_description,sql_code,copy,steps;
    float row_height;
    bool compact,valid;
} UIInspectorLayout;
/* Physical panel, logical study measurements. Short panes retain complete
   action targets; only the step viewport and explanatory text shrink. */
UIInspectorLayout ui_inspector_layout(Rectangle panel,float scale,size_t count,bool sql);
#endif
