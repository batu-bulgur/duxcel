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
#endif
