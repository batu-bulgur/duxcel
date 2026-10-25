#ifndef DUXCEL_GRID_H
#define DUXCEL_GRID_H
#include "../data/database.h"
#include "../ui/ui.h"
typedef enum { SELECT_CELLS, SELECT_ROWS, SELECT_COLUMNS, SELECT_ALL } SelectionMode;
/* Scalar-only navigation bookmark: never copies a Grid's owned column arrays,
   borrowed formats, or in-progress pointer state. Zoom remains sheet-owned. */
typedef struct {
    uint64_t row,anchor_row,end_row,top;
    size_t col,anchor_col,end_col;
    float horizontal;
    double fraction;
    SelectionMode selection;
    bool valid;
} GridLocation;
/* Transient shortcut bounds supplied for the object at the active cell. */
typedef struct { uint64_t r0,r1; size_t c0,c1; bool active; } GridRegion;
/* Native trackpads expose gesture phases. Unphased browser/legacy packets use
   the bounded per-axis fallback, rather than restarting return every frame. */
typedef enum { GRID_PAN_NONE, GRID_PAN_BEGIN, GRID_PAN_ACTIVE, GRID_PAN_RELEASE, GRID_PAN_MOMENTUM } GridPanPhase;
typedef struct {
    float quiet,elapsed,window_time,window_distance,peak_rate;
    float reversal; /* bounded CSS-point accumulation at a returning boundary */
    int direction,return_edge; /* +1 near edge, -1 far edge, zero without a boundary */
    bool active,returning;
} GridPanAxis;
/* Snapshot input separately so regression tests exercise the actual interaction code. */
typedef struct {
    Vector2 mouse, wheel;
    Vector2 pixel_pan; /* precise layout pixels, positive right/down */
    bool pixel_scroll; /* bypass legacy wheel detents/Shift remapping */
    GridPanPhase pan_phase;
    bool pressed, released, down, right, shift, shortcut, alt;
    bool separate_keyboard,key_shift,key_shortcut,key_alt;
    int key;
    float dt;
} GridInput;
typedef struct {
    uint64_t anchor_row, end_row, row, top; /* row/col are the active cell; end is the independently moving range extent. */
    uint64_t used_rows,scroll_rows; /* sparse content extent; frozen during thumb drag */
    size_t anchor_col, end_col, col;
    size_t used_columns;
    SelectionMode selection;
    GridRegion navigation;
    const FormatStore *formats; /* Borrowed for this frame; no ownership. */
    uint64_t format_row;
    size_t format_col;
    /* Borrowed store-local tokens: 0 retains legacy/manual lookup; SIZE_MAX
       explicitly suspends an imported definition. A publisher must verify the
       Page/schema/owner before supplying matched tokens. Expire on mutation. */
    size_t format_result_token,format_record_token;
    float horizontal; /* unscaled UI units, independent of monitor DPI/zoom */
    float gutter; /* Interface units, independent of zoom; reserved for visible row numbers. */
    float digit_width;
    double fraction; /* fractional row for trackpads */
    float zoom; /* canvas only; independent of application UI scale */
    float row_height; /* Unscaled row density; independent of canvas zoom. */
    Vector2 elastic,spring_velocity; /* transient canvas units; never saved or used as query offsets */
    GridPanAxis pan_x,pan_y; /* transient gesture/tail ownership, independent between axes */
    bool elastic_enabled;
    float *widths, *offsets;
    char **column_names; /* Owned source names; unused canvas columns are NULL. */
    size_t width_count, data_columns, first_col;
    uint64_t width_revision;
    int resizing, scrollbar, cursor, last_edge;
    float drag_start, width_start, grab_offset;
    double clock, last_click;
    float auto_scroll;
    bool dragging, zebra,source_header,hide_gridlines,bounded,hide_selection;
    bool freeze,freeze_blocked,controls_enabled; /* Retain the choice when its viewport is too narrow. */
    size_t frozen_columns;
    float frozen_width; /* Unscaled width of the first two source columns. */
    float chrome_scale; /* Interface scale; canvas zoom does not resize controls. */
    bool lock_structure; /* keep scrolling/selection live during workbook I/O */
    bool block_pointer; /* an overlay owns new pointer gestures; keyboard stays live */
    Rectangle rect, body, vtrack, vthumb, htrack, hthumb;
    unsigned visible_rows, render_rows;
    bool filter_requested,fit_requested;
    size_t fit_column;
    bool allow_move,moving,move_requested,context_requested;
    Vector2 move_start;
    bool move_threshold_met;
    uint64_t move_row;
    size_t move_col;
    int64_t move_dr;
    int move_dc;
} Grid;
void grid_init(Grid *g);
void grid_destroy(Grid *g);
GridLocation grid_location(const Grid *g);
bool grid_restore_location(Grid *g,const GridLocation *location);
void grid_schema(Grid *g, const Page *p, bool reset);
void grid_ensure_columns(Grid *g, size_t count);
void grid_set_width(Grid *g, size_t col, float width);
/* Project committed physical widths, independent of source names/schema. The
   omitted virtual tail is96. Validate before changing any cache or geometry;
   this does not author a worksheet action. Input must not alias Grid arrays. */
bool grid_project_widths(Grid *g,const float *widths,size_t count);
void grid_fit_column(Grid *g,UIContext *ui,const Page *page,size_t column);
/* Presentation-only navigation: preserves active cell, selection and row offset. */
void grid_reveal_column(Grid *g,const Page *page,float scale,size_t column,bool center);
void grid_reveal_cell(Grid *g,const Page *page,float scale);
void grid_select_cell(Grid *g, uint64_t row, size_t col, bool extend);
/* scale is interface scale * g->zoom; header/gutter chrome uses interface scale. */
void grid_layout(Grid *g, float scale, const Page *page, Rectangle rect);
void grid_zoom_at(Grid *g, const Page *page, float ui_scale, float zoom, Vector2 pointer);
void grid_handle_input(Grid *g, UIContext *ui, const Page *page, const GridInput *input);
void grid_replay_gesture(Grid *g,UIContext *ui,const Page *page,DxPointerGesture gesture);
void grid_update(Grid *g, UIContext *ui, const Page *page, Rectangle rect, bool enabled);
void grid_draw(Grid *g, UIContext *ui, const Page *page, bool busy);
/* row is in the painted Page's coordinates (including Page.offset). Worksheet
   offsets affect spatial/result patches only; record IDs come from this exact
   Page, including a borrowed preview prefix. Raw-token callers must validate
   that display columns still have their original source-schema order. */
CellFormat grid_cell_format(const Grid *grid,const Page *page,uint64_t row,size_t column);
bool grid_cached(const Grid *g, const Page *p);
void grid_bounds(const Grid *g, uint64_t *r0, uint64_t *r1, size_t *c0, size_t *c1);
/* Displayed source headers occupy row 1; source data/manual/object cells use
   model row +2, blank-sheet cells +1. Keep every displayed address consistent. */
uint64_t grid_row_number(uint64_t model_row,bool source_header);
void grid_format_address(uint64_t model_row,size_t column,bool source_header,char out[48]);
/* Source display row 1 is header chrome, not an addressable model cell.
   Outputs change only on success; final source display row is DX_SHEET_ROWS+1. */
bool grid_parse_display_address(const char *text,bool source_header,uint64_t *row,size_t *column);
/* Worksheet-local A1 address, optionally absolute; outputs change only on success. */
bool grid_parse_address(const char *text,uint64_t *row,size_t *column);
bool grid_cell_at(const Grid *grid,float scale,Vector2 point,uint64_t *row,size_t *column);
Vector2 grid_origin(const Grid *grid,float scale);
float grid_column_x(const Grid *grid,float scale,size_t column);
Rectangle grid_column_clip(const Grid *grid,float scale,size_t column);
Rectangle grid_range_rect(const Grid *grid,float scale,uint64_t row,size_t column,uint64_t rows,size_t columns);
/* Border/hit geometry: independent double-precision endpoints, with offscreen
   edges kept outside clip. Do not use it to position a range's title/content. */
Rectangle grid_range_outline(const Grid *grid,float scale,uint64_t row,size_t column,uint64_t rows,size_t columns,Rectangle clip);
#endif
