#ifndef DX_RIBBON_SPEC_H
#define DX_RIBBON_SPEC_H
#include "ribbon.h"
#include "carbon_icons.h"
enum { RIBBON_HOME,RIBBON_INSERT,RIBBON_DATA,RIBBON_ANALYZE,RIBBON_VIEW,RIBBON_VISUALIZATION,RIBBON_TABS };
typedef enum {
    ACTION_COPY,ACTION_PASTE,ACTION_CUT,ACTION_FONT,ACTION_FONT_SIZE,ACTION_BOLD,ACTION_ITALIC,ACTION_UNDERLINE,ACTION_TEXT_COLOR,
    ACTION_ALIGN_LEFT,ACTION_ALIGN_CENTER,ACTION_ALIGN_RIGHT,ACTION_WRAP,ACTION_NUMBER_FORMAT,ACTION_ACCOUNTING,ACTION_DECIMAL_LESS,ACTION_DECIMAL_MORE,
    ACTION_SORT_ASC,ACTION_SORT_DESC,ACTION_FILTER,ACTION_CLEAR_FILTERS,ACTION_CREATE_VIEW,ACTION_TRANSFORM,ACTION_REFRESH,
    ACTION_MATRIX,ACTION_BAR,ACTION_LINE,ACTION_NEW_SHEET,ACTION_SOURCE_ORDER,ACTION_SQL,
    ACTION_ADD_ROWS,ACTION_ADD_COLUMNS,ACTION_SWAP_SHELVES,ACTION_ADD_MEASURE,ACTION_BROWSE,
    ACTION_WORKBOOK,ACTION_FIELDS,ACTION_FOCUS,ACTION_DENSITY,ACTION_SCALE,ACTION_GRIDLINES,ACTION_FREEZE,ACTION_RESET_ZOOM,ACTION_RESET_LAYOUT,ACTION_GUIDE,ACTION_FIND,ACTION_GO_TO
} UIAction;
typedef struct { const char *label; UICarbonIcon icon; UIAction action; } UIRibbonCommand;
extern const char *const ui_ribbon_tabs[RIBBON_TABS];
extern const int ribbon_counts[RIBBON_TABS],ribbon_group_counts[RIBBON_TABS];
extern const UIRibbonCommand ribbon_commands[RIBBON_TABS][UI_RIBBON_COMMANDS];
extern const UIRibbonGroup ribbon_groups[RIBBON_TABS][UI_RIBBON_GROUPS];
UIRibbonLayout ui_ribbon_spec_layout(float width,int mode,int tab,const float labels[UI_RIBBON_COMMANDS],const float names[UI_RIBBON_GROUPS]);
/* Explicit group artwork from the executable reference, not its first action.
   Invalid tab/group returns CARBON_COUNT, the existing no-icon sentinel. */
UICarbonIcon ui_ribbon_group_icon(int tab,int group);
int ui_ribbon_action_index(int tab,UIAction action);
bool ui_ribbon_command_visible(int tab,int mode,int command);
#endif
