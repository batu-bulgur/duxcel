#ifndef DUXCEL_SORTVIEW_H
#define DUXCEL_SORTVIEW_H
#include "ui.h"
#include "../data/database.h"
#ifdef DUXCEL_DEV_REVIEW
#define SORT_REVIEW_TARGETS 96
typedef enum {
    SORT_REVIEW_PANEL,SORT_REVIEW_HEADING,SORT_REVIEW_CLOSE,SORT_REVIEW_FIELDS,SORT_REVIEW_PRIORITIES,SORT_REVIEW_SEARCH,
    SORT_REVIEW_FIELD,SORT_REVIEW_LEVEL,SORT_REVIEW_LEVEL_SELECT,SORT_REVIEW_LEVEL_TITLE,SORT_REVIEW_UP,SORT_REVIEW_DOWN,
    SORT_REVIEW_REMOVE,SORT_REVIEW_ASCENDING,SORT_REVIEW_DESCENDING,SORT_REVIEW_FIELD_SCROLL,SORT_REVIEW_FIELD_THUMB,
    SORT_REVIEW_ORDER_SCROLL,SORT_REVIEW_ORDER_THUMB,SORT_REVIEW_STATUS,SORT_REVIEW_CLEAR,SORT_REVIEW_CANCEL,SORT_REVIEW_APPLY
} SortReviewKind;
typedef struct {
    SortReviewKind kind; Rectangle bounds,clip; uint64_t field_hash; size_t column,priority;
    bool enabled,checked,focused,container,truncated;
    char field[128],label[192]; const char *file; uint32_t line;
} SortReviewTarget;
/* Only the armed draw writes owned, bounded visible records. App adapts them
   immediately after that same draw; shipping has no snapshot or callback. */
typedef struct { bool armed,overflow; size_t count; SortReviewTarget targets[SORT_REVIEW_TARGETS]; } SortReviewFrame;
#endif
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
#ifdef DUXCEL_DEV_REVIEW
    SortReviewFrame review;
#endif
} SortEditor;
typedef enum { SORT_EDITING,SORT_APPLY,SORT_CANCEL } SortResult;
void sort_editor_free(SortEditor *editor);
void sort_editor_open(SortEditor *editor,const Analysis *analysis,const Page *page,size_t column);
SortResult sort_editor_draw(SortEditor *editor,UIContext *ui,const Page *page);
#endif
