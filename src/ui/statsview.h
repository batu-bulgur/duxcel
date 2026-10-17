#ifndef DX_STATS_VIEW_H
#define DX_STATS_VIEW_H
#include "ui.h"
typedef struct { Rectangle panel,title,description,rows[8],separator,done; bool valid; } UIStatsLayout;
UIStatsLayout ui_stats_layout(Vector2 anchor,Vector2 viewport,float scale);
void ui_stats_surface(UIContext *ui,const UIStatsLayout *layout,const char *scope);
/* The whole checklist row toggles; opening-press suppression is independent
   of the setting's availability and visual state. */
bool ui_stats_row(UIContext *ui,Rectangle row,const char *name,const char *value,bool checked,bool focused,bool pointer);
#endif
