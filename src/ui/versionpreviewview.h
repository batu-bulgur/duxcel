#ifndef DUXCEL_VERSION_PREVIEW_VIEW_H
#define DUXCEL_VERSION_PREVIEW_VIEW_H
#include "ui.h"
#include "textview.h"
#include "../grid/grid.h"
#include "../workbook/workbook.h"
typedef struct {
    Grid grid;
    Page empty;
    size_t sheet;
    bool ready,definitions;
    char *definition_text;
    TextLines lines;
    int text_scroll;
    double wheel;
    Rectangle back,versions,save_as,previous,next,definition,canvas;
} UIVersionPreviewView;
typedef enum { UI_HISTORY_NONE,UI_HISTORY_RETURN,UI_HISTORY_VERSIONS,UI_HISTORY_SAVE_AS } UIHistoryAction;
/* A read-only surface reusing the actual Grid and formatted-value painters.
   It owns navigation/definition layout only; no source query or authoring callback
   exists. Working App pages/grid/editor state are never supplied or overwritten. */
void ui_history_free(UIVersionPreviewView *view);
void ui_history_begin(UIVersionPreviewView *view,const Workbook *book);
UIHistoryAction ui_history_draw(UIContext *ui,UIVersionPreviewView *view,const Workbook *book,const char *label,const char *message,bool input);
#endif
