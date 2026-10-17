#ifndef DUXCEL_DOCKVIEW_H
#define DUXCEL_DOCKVIEW_H
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct { Rectangle viewport,content; float maximum,offset; bool compact,valid; } UIDockLayout;
typedef struct { float height,row_height,stride; size_t visible; bool valid; } UIWorkbookList;
UIWorkbookList ui_workbook_list(float maximum,size_t count,bool compact);
UIDockLayout ui_dock_layout(Rectangle viewport,float scale,size_t sheets,bool workbook,bool collapsed,bool fields,bool fields_collapsed,bool authoring,float marks_height,float offset);
UIDockLayout ui_dock_layout_fraction(Rectangle viewport,float scale,size_t sheets,bool workbook,float workbook_open,bool fields,bool fields_collapsed,bool authoring,float marks_height,float offset);
float ui_dock_reveal(UIDockLayout layout,Rectangle target,float scale);
#endif
