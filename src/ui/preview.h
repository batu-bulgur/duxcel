#ifndef DX_DATA_PREVIEW_H
#define DX_DATA_PREVIEW_H
#include "../grid/grid.h"
typedef struct { Rectangle banner,context,grid,return_button; bool valid; } UIPreviewLayout;
UIPreviewLayout ui_preview_layout(Rectangle canvas,float scale);
/* A transient read-only canvas uses an independent Grid. Data stays owned by
   the caller; this view never changes workbook cells, objects or history. */
bool ui_preview_draw(UIContext *ui,Grid *grid,const Page *page,Rectangle canvas,
    size_t step,const char *title,bool enabled,bool grid_enabled,bool loading,const char *error);
#ifdef DUXCEL_DEV_REVIEW
#define UI_PREVIEW_REVIEW_TARGETS 6
typedef struct {
    char key[24],role[16],text[1024],source_file[160]; uint32_t source_line; Rectangle bounds,clip;
    bool enabled,container,truncated;
} UIPreviewReviewTarget;
typedef struct {
    bool armed,return_enabled,overflow; size_t count;
    UIPreviewReviewTarget targets[UI_PREVIEW_REVIEW_TARGETS];
} UIPreviewReviewFrame;
/* Optional passive paint records; the ordinary production/test API above is
   unchanged. Arm and supply logical Return availability before drawing. */
bool ui_preview_draw_review(UIContext *ui,Grid *grid,const Page *page,Rectangle canvas,
    size_t step,const char *title,bool enabled,bool grid_enabled,bool loading,const char *error,
    UIPreviewReviewFrame *review);
#endif
#endif
