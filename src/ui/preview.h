#ifndef DX_DATA_PREVIEW_H
#define DX_DATA_PREVIEW_H
#include "../grid/grid.h"
typedef struct { Rectangle banner,context,grid,return_button; bool valid; } UIPreviewLayout;
UIPreviewLayout ui_preview_layout(Rectangle canvas,float scale);
/* A transient read-only canvas uses an independent Grid. Data stays owned by
   the caller; this view never changes workbook cells, objects or history. */
bool ui_preview_draw(UIContext *ui,Grid *grid,const Page *page,Rectangle canvas,
    size_t step,const char *title,bool enabled,bool grid_enabled,bool loading,const char *error);
#endif
