#ifndef DUXCEL_SORTVIEW_H
#define DUXCEL_SORTVIEW_H
#include "ui.h"
#include "../data/database.h"
typedef struct {
    Analysis draft;
    TextInput search;
    size_t *matches,count,choice;
    uint64_t search_revision;
    int focus,field_scroll,order_scroll,level;
    bool just_opened;
    int dragging;
    float grab;
    double field_wheel,order_wheel;
    char *error;
} SortEditor;
typedef enum { SORT_EDITING,SORT_APPLY,SORT_CANCEL } SortResult;
void sort_editor_free(SortEditor *editor);
void sort_editor_open(SortEditor *editor,const Analysis *analysis,const Page *page,size_t column);
SortResult sort_editor_draw(SortEditor *editor,UIContext *ui,const Page *page);
#endif
