#ifndef DUXCEL_WORKBOOK_H
#define DUXCEL_WORKBOOK_H
#include "../transforms/analysis.h"
#include "cells.h"
#include "objects.h"
#include "format.h"
#include "source_scope.h"
#define DX_WORKSPACE_VERSION 14
#define DX_MAX_SHEETS 64
#define DX_HISTORY 32
typedef enum { EDIT_ANALYSIS,EDIT_CELLS,EDIT_OBJECTS,EDIT_RANGE,EDIT_FORMAT,EDIT_CELLS_FORMAT,EDIT_OBJECTS_FORMAT,EDIT_RANGE_FORMAT } EditKind;
typedef struct { bool active,insert; size_t column; float width; } ColumnEdit;
/* View units are logical/unscaled, never framebuffer pixels. No raylib types here. */
typedef struct {
    uint64_t row,anchor_row,end_row,top;
    size_t col,anchor_col,end_col;
    unsigned selection;
    float horizontal;
    double fraction;
    float canvas_zoom;
    char **columns;
    float *widths;
    size_t column_count;
} SheetView;
typedef struct {
    char *name;
    bool hidden;
    Analysis analysis;
    SheetView view;
    CellStore cells;
    ObjectStore objects;
    FormatStore formats;
    SourceVersions source_versions; /* Owned immutable bindings/high-water, outside Undo. */
    Analysis undo[DX_HISTORY],redo[DX_HISTORY];
    size_t undo_count,redo_count;
    EditKind undo_actions[DX_HISTORY],redo_actions[DX_HISTORY];
    ColumnEdit undo_columns[DX_HISTORY],redo_columns[DX_HISTORY];
    size_t undo_actions_count,redo_actions_count;
} Worksheet;
typedef struct {
    Worksheet *sheets;
    size_t count,active;
    float user_scale;
    char *path;
    bool dirty,reduced_motion;
} Workbook;
void view_free(SheetView *view);
void workbook_init(Workbook *book);
void workbook_free(Workbook *book);
/* Copies definitions, source-version ledger/high-water and presentation only;
   undo stacks/pages are session state. NULL denotes an invalid source ledger. */
Workbook *workbook_snapshot(const Workbook *book);
bool workbook_add(Workbook *book,const char *name,const Analysis *analysis,char **error);
bool workbook_rename(Workbook *book,size_t index,const char *name,char **error);
bool workbook_reorder(Workbook *book,size_t from,size_t to,char **error);
bool workbook_duplicate(Workbook *book,size_t index,char **error);
bool workbook_hide(Workbook *book,size_t index,bool hidden,char **error);
size_t workbook_visible_count(const Workbook *book);
/* Display position to physical sheet index; count means no such visible sheet. */
size_t workbook_visible_sheet(const Workbook *book,size_t position);
void worksheet_record(Worksheet *sheet,EditKind kind);
unsigned worksheet_edit_effects(EditKind kind); /* analysis=1, cells=2, objects=4, formats=8 */
void worksheet_transfer_history(Worksheet *sheet,bool redo);
bool worksheet_range_fits(const Worksheet *sheet,SheetRange range,int64_t dr,int dc,uint64_t source_rows,size_t source_columns,char **error);
/* Moves contained objects and manual values atomically, with one journal action. */
bool worksheet_move_range(Worksheet *sheet,SheetRange range,int64_t dr,int dc,uint64_t source_rows,size_t source_columns,char **error);
/* Insert/delete outside the imported columns. Shift sparse cells and complete
   objects together, rejecting a split object or a last-column overflow. */
bool worksheet_column(Worksheet *sheet,size_t column,bool insert,size_t source_columns,char **error);
/* Shift complete rows below the read-only source; reject splitting an object. */
bool worksheet_row(Worksheet *sheet,uint64_t row,bool insert,uint64_t source_rows,char **error);
bool worksheet_rows(Worksheet *sheet,uint64_t row,uint64_t count,bool insert,uint64_t source_rows,char **error);
/* Both functions preserve caller-owned data on failure. Save only accepts new paths
   or an existing versioned Duxcel workspace, and updates metadata transactionally. */
bool workbook_save(const Workbook *book,const char *path,char **error);
bool workbook_load(Workbook *book,const char *path,char **error);
bool workbook_is_path(const char *path); /* .dxls, or the legacy .duxcel suffix */
char *workbook_target_path(const char *path); /* absolute, even for a new file */
char *workbook_relative_path(const char *workspace,const char *source);
char *workbook_source_path(const char *workspace,const char *reference);
#endif
