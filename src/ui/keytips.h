#ifndef DX_KEYTIPS_H
#define DX_KEYTIPS_H
#include "ribbon_spec.h"

enum { UI_KEYTIPS_ROOT=-1, UI_KEYTIPS_FILE=-2, UI_KEYTIPS_PREFIX_CAP=4 };
typedef enum {
    UI_KEYTIP_FILE_NEW, UI_KEYTIP_FILE_OPEN, UI_KEYTIP_FILE_SAVE,
    UI_KEYTIP_FILE_SAVE_AS, UI_KEYTIP_FILE_CLOSE, UI_KEYTIP_FILE_IMPORT, UI_KEYTIP_FILE_QUIT,
    UI_KEYTIP_FILE_COUNT
} UIKeytipFileAction;
typedef enum {
    UI_KEYTIP_NONE, UI_KEYTIP_PREFIX, UI_KEYTIP_TAB,
    UI_KEYTIP_COMMAND, UI_KEYTIP_FILE
} UIKeytipResultKind;
typedef struct { UIKeytipResultKind kind; int value; } UIKeytipResult;
typedef struct { bool active; int tab; char prefix[UI_KEYTIPS_PREFIX_CAP]; } UIKeytips;

/* No allocation/input/render calls. Caller owns focus, visible tab changes,
   action availability and dispatch. Cancel on pointer/modal/focus takeover.
   A zero-initialized state is inactive; begin always opens root tab tips. */
void ui_keytips_begin(UIKeytips *state);
/* Retain the last context/prefix for the caller's fade-out paint. */
void ui_keytips_cancel(UIKeytips *state);
/* Escape removes one prefix character, then returns to tabs, then dismisses. */
bool ui_keytips_escape(UIKeytips *state);
/* Input is ASCII letter/digit from a physical press, never a repeat. Unknown
   prefixes preserve state and return NONE; an active caller consumes them.
   Complete commands deactivate the state; TAB/PREFIX retain hint mode. */
UIKeytipResult ui_keytips_feed(UIKeytips *state,int ascii,bool repeat);

/* Metadata is static borrowed text, valid for the lifetime of the process.
   Command/file labels return only the unmatched suffix, NULL when this prefix
   excludes the action. They deliberately permit inactive state for fade-out. */
const char *ui_keytips_tab_label(int tab);
const char *ui_keytips_command_label(const UIKeytips *state,int tab,UIAction action);
const char *ui_keytips_file_label(const UIKeytips *state,UIKeytipFileAction action);
const char *ui_keytips_file_name(UIKeytipFileAction action);
/* Distinguish Duxcel extensions in a guide; these are not Excel mappings. */
bool ui_keytips_command_custom(int tab,UIAction action);
#endif
