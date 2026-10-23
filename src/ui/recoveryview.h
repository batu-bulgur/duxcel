#ifndef DUXCEL_RECOVERY_VIEW_H
#define DUXCEL_RECOVERY_VIEW_H
#include "ui.h"
enum { UI_RECOVERY_ROWS=32,UI_RECOVERY_VISIBLE=4 };
typedef struct { const char *title,*detail,*identity; bool replay,checkpoint; } UIRecoveryRow;
typedef struct {
    const UIRecoveryRow *rows; size_t count;
    bool busy,more; const char *message;
} UIRecoveryContext;
typedef struct {
    Rectangle panel,heading,message,rows[UI_RECOVERY_VISIBLE],detail,buttons[5];
    size_t visible; bool valid;
} UIRecoveryLayout;
typedef struct { size_t selected,scroll; int focus; UIRecoveryLayout layout; } UIRecoveryView;
typedef enum { UI_RECOVERY_NONE,UI_RECOVERY_REPLAY,UI_RECOVERY_CHECKPOINT,UI_RECOVERY_REFRESH,UI_RECOVERY_NEXT,UI_RECOVERY_LATER } UIRecoveryAction;
/* Borrowed presentation only. The App owns pages, requests, adoption and leases.
   Escape/Later dismisses the presentation; no action implies artifact deletion. */
UIRecoveryLayout ui_recovery_layout(Vector2 viewport,float scale,size_t count);
UIRecoveryAction ui_recovery_draw(UIContext *ui,UIRecoveryView *view,UIRecoveryContext context,bool pointer);
#endif
