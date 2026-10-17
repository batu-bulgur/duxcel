#ifndef DX_RIBBON_H
#define DX_RIBBON_H
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

#define UI_RIBBON_COMMANDS 32
#define UI_RIBBON_GROUPS 6
typedef struct {
    const char *name;
    int first,count,priority; /* Lower priority collapses/overflows first. */
    bool prominent; /* Full mode reserves large targets for primary analysis. */
} UIRibbonGroup;
typedef enum { UI_RIBBON_EXPANDED,UI_RIBBON_COLLAPSED,UI_RIBBON_OVERFLOW } UIRibbonState;
typedef struct {
    Rectangle commands[UI_RIBBON_COMMANDS],groups[UI_RIBBON_GROUPS],launchers[UI_RIBBON_GROUPS],overflow;
    UIRibbonState states[UI_RIBBON_GROUPS];
    int group_count,overflow_count;
    float height;
    bool valid;
} UIRibbonLayout;
/* All coordinates and measured text widths use unscaled logical units. The
   deterministic layout owns no memory and performs no rendering or queries. */
UIRibbonLayout ui_ribbon_layout(float width,int mode,const UIRibbonGroup *groups,int count,
    const float command_labels[UI_RIBBON_COMMANDS],const float group_labels[UI_RIBBON_GROUPS]);
/* Optional group-local cells reproduce the canonical stacked/full and dense
   compact controls. Logical geometry only; ownership stays with the caller. */
UIRibbonLayout ui_ribbon_layout_custom(float width,int mode,const UIRibbonGroup *groups,int count,
    const float command_labels[UI_RIBBON_COMMANDS],const float group_labels[UI_RIBBON_GROUPS],
    const float expanded[UI_RIBBON_GROUPS],const Rectangle cells[UI_RIBBON_COMMANDS]);
/* Reference ribbon: reserve the external history/overflow dock exactly once.
   Expanded cells keep the custom-layout geometry; collapsed button rectangles
   use the reference's 4-unit side padding, 1-unit slot border and intrinsic
   width. Fit uses rounded offsetWidth + 8, as in the reference's JS.
   dock_width includes the complete 98/38-unit action dock, not outer padding. */
UIRibbonLayout ui_ribbon_layout_docked(float width,int mode,const UIRibbonGroup *groups,int count,
    const float command_labels[UI_RIBBON_COMMANDS],const float group_labels[UI_RIBBON_GROUPS],
    const float expanded[UI_RIBBON_GROUPS],const Rectangle cells[UI_RIBBON_COMMANDS],float dock_width);
int ui_ribbon_group_for(const UIRibbonGroup *groups,int count,int command);
float ui_ribbon_height(int mode);
#endif
