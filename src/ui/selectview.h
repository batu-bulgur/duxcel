#ifndef DUXCEL_SELECTVIEW_H
#define DUXCEL_SELECTVIEW_H
#include "ui.h"

typedef enum {
    UI_SELECT_REPRESENTATION,
    UI_SELECT_PLOT,
    UI_SELECT_FONT_SIZE,
    UI_SELECT_NUMBER_FORMAT
} UISelectStyle;

/* Closed reference <select> field. Rect is in display coordinates; type and
   padding use UI scale. The caller owns its value, focus and dropdown state.
   Select focus is explicit even after pointer activation, unlike action-menu
   keyboard rings. This helper only paints and reports activation. */
bool ui_select_field(UIContext *ui,Rectangle rect,const char *label,UISelectStyle style,bool focused,bool enabled);

typedef struct {
    Rectangle panel,items;
    float row_height;
    int count,visible;
    bool valid;
} UISelectMenuLayout;
/* Compact native option-list presentation, not measured OS-popup paint parity.
   anchor is the desired top-left below the field; window/anchor and all output
   rectangles/row_height are display coordinates. width is logical units; rows are 26 units
   with a 1-unit border/inset and 5-unit window margins. count is 1..64.
   Caller owns the first visible option, keyboard navigation and scroll state. */
UISelectMenuLayout ui_select_menu_layout(Vector2 anchor,Vector2 window,float scale,float width,int count);
Rectangle ui_select_menu_row(const UISelectMenuLayout *layout,int visible_row);
void ui_select_menu_frame(UIContext *ui,const UISelectMenuLayout *layout);
/* Only pointer activation is returned. allow=false blocks the opening click
   without painting an otherwise available option as disabled. No labels/state
   are retained; selected is the only option which receives a check icon. */
bool ui_select_menu_option(UIContext *ui,const UISelectMenuLayout *layout,int visible_row,
    const char *label,bool selected,bool focused,bool enabled,bool allow);
#endif
