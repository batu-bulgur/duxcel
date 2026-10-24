#ifndef DUXCEL_VERSION_VIEW_H
#define DUXCEL_VERSION_VIEW_H
#include "ui.h"
#include "../workbook/version_index.h"
enum { UI_VERSION_VISIBLE=4 };
typedef struct {
    Rectangle panel,heading,summary,latest,rows[UI_VERSION_VISIBLE],scrollbar,label,input,message,create,close;
    size_t visible; bool valid;
} UIVersionLayout;
typedef struct { size_t selected,scroll; int focus; UIScrollbar scrollbar; double wheel; UIVersionLayout layout; } UIVersionView;
typedef struct { const WorkbookVersionIndex *index; const char *message; bool writable; } UIVersionContext;
typedef enum { UI_VERSION_NONE,UI_VERSION_CREATE,UI_VERSION_CLOSE } UIVersionAction;
/* Borrowed metadata/presentation only. App owns name/destination validation,
   snapshots, jobs, durability and publication. No query/file IO or model edits. */
UIVersionLayout ui_version_layout(Vector2 viewport,float scale,size_t count);
UIVersionAction ui_version_draw(UIContext *ui,UIVersionView *view,TextInput *name,UIVersionContext context,bool pointer);
#endif
