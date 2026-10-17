#ifndef DUXCEL_CHOICEVIEW_H
#define DUXCEL_CHOICEVIEW_H
#include "ui.h"
#define UI_CHOICE_MAX 9
typedef struct { Rectangle panel,title,rows[UI_CHOICE_MAX],separator; int count; bool valid; } UIChoiceLayout;
UIChoiceLayout ui_choice_layout(Vector2 origin,Vector2 viewport,float scale,int count,int separator_before);
typedef struct { UIChoiceLayout frame; Rectangle items; int visible; } UIChoiceListLayout;
/* Extra native source/object actions share the definition's headed menu; only
   the bounded row viewport scrolls, so its title remains visible. */
UIChoiceListLayout ui_choice_list_layout(Vector2 origin,Vector2 viewport,float scale,int count);
void ui_choice_frame(UIContext *ui,const UIChoiceLayout *layout,const char *title);
bool ui_choice_row(UIContext *ui,Rectangle row,const char *label,const char *shortcut,UICarbonIcon icon,bool checked,bool focus,bool enabled);
bool ui_choice_row_input(UIContext *ui,Rectangle row,const char *label,const char *shortcut,UICarbonIcon icon,bool checked,bool focus,bool available,bool accepts_input);
#endif
