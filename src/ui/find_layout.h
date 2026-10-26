#ifndef DUXCEL_UI_FIND_LAYOUT_H
#define DUXCEL_UI_FIND_LAYOUT_H
#include <raylib.h>
#include <stdbool.h>
/* Geometry only. The caller owns the query, match mode and navigation result. */
typedef struct { Rectangle panel,title,label,input,whole_cell,message,close,previous,next; bool valid; } UIFindLayout;
UIFindLayout ui_find_layout(Rectangle viewport,float scale);
#endif
