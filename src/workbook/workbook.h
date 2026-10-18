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
#define DX_ANALYSIS_HISTORY_BYTES (8u*1024u*1024u)
typedef enum { EDIT_ANALYSIS,EDIT_CELLS,EDIT_OBJECTS,EDIT_RANGE,EDIT_FORMAT,EDIT_CELLS_FORMAT,EDIT_OBJECTS_FORMAT,EDIT_RANGE_FORMAT,EDIT_ANALYSIS_FORMAT } EditKind;
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
/* Pure owned primary-Analysis journal primitives. live may be sheet.analysis or
   the App's independently owned authoritative value; the save cache is otherwise
   untouched. next is an independent owned prepared value, never a sheet/object
   history location or borrowed text. Preflight preserves all inputs and leaves
   changed untouched on failure. Each Analysis and each existing history stack
   has an 8 MiB owned bound (including its Analysis structures).
   Adoption is nonfallible and requires successful preflight, changed=true and
   unchanged Analysis/history inputs (a separate ledger adoption is allowed).
   It moves pointers, consumes next and clears Analysis redo.
   Commit the other constituent first/afterward, then call worksheet_record ONCE
   with EDIT_ANALYSIS_FORMAT (or EDIT_ANALYSIS for a lone Analysis change).
   History follows the same preflight/move/format/transfer sequence. These helpers
   never touch the source-version ledger, formats, journal, dirty bit or save cache
   (except when live explicitly is sheet.analysis). They do not validate source
   ledger/schema association or infer heap ownership outside these trusted inputs. */
bool worksheet_analysis_preflight(const Worksheet *sheet,Analysis *live,Analysis *next,bool *changed,char **error);
void worksheet_analysis_adopt(Worksheet *sheet,Analysis *live,Analysis *next);
bool worksheet_analysis_history_preflight(const Worksheet *sheet,Analysis *live,bool redo,char **error);
void worksheet_analysis_history_adopt(Worksheet *sheet,Analysis *live,bool redo);
#define DX_WORKBOOK_FORMAT_CURRENT_BYTES (8u*1024u*1024u)
#define DX_WORKBOOK_FORMAT_RETAINED_BYTES (32u*1024u*1024u)
#define DX_APP_FORMAT_ADDITIONAL_BYTES (64u*1024u*1024u)
typedef enum { FORMAT_BUDGET_OK,FORMAT_BUDGET_INPUT,FORMAT_BUDGET_MODEL,FORMAT_BUDGET_OVERFLOW,
    FORMAT_BUDGET_SHEET,FORMAT_BUDGET_CURRENT,FORMAT_BUDGET_RETAINED,FORMAT_BUDGET_APP } FormatBudgetStatus;
typedef struct { size_t sheet_count,current,retained,max_sheet_current,max_sheet; } WorkbookFormatUsage;
typedef struct { size_t sheet; const FormatStore *formats; const SourceVersions *versions; } WorkbookFormatReplacement;
typedef struct {
    size_t adopted_staging; /* Owned next buffers that become part of the next model. */
    size_t io_snapshots,other_staging,collector_owned,page_metadata_owned,pending_reserved;
} FormatTransientUsage;
typedef struct { size_t before,after,peak; } FormatBudget;
/* Pure logical model accounting. Format arrays, scope terminators, memberships
   and all retained format history count; source_versions.bytes counts once,
   including dormant proofs, paths and capacity. Its actual object high-water is
   validated. Inline Workbook/Worksheet metadata and ordinary data values are not
   extra owned payload. Replacements are unique existing sheets; NULL components
   keep live state. Complete borrowed prospective history views are allowed and
   must never be freed as independent owners. No aggregate cap is imposed by
   measurement, so otherwise-valid legacy state can be inspected without change.
   Outputs are unchanged on failure; helpers allocate nothing or do FS/SQL work. */
FormatBudgetStatus workbook_format_usage(const Workbook *book,const WorkbookFormatReplacement *replacements,size_t count,WorkbookFormatUsage *out);
/* Explicit opt-in policy, not an installed reservation manager or a Save hook.
   Before=live retained+adopted staging+all other categories; after=next retained
   +all other categories. Each concrete owned allocation/reservation belongs in
   exactly one category. Independently owned save/load snapshots can be measured
   with usage().retained; save snapshots naturally omit history. Count active,
   pending and published canceled work until its ownership is actually released.
   Borrowed Page views count once at their owner. pending_reserved is unallocated
   capacity not already included elsewhere; caller explicitly converts/releases
   it. This helper never assumes cancellation freed a buffer, prunes history, or
   allocates/releases reservations. Producer checks upper bounds before allocation
   and actual replacements again before publication. Other staging surviving
   adoption stays in other_staging, not adopted_staging. No allocator/process-peak
   or per-job definition-cap guarantee is implied by these aggregate counts. */
FormatBudgetStatus workbook_format_preflight(const WorkbookFormatUsage *live,const WorkbookFormatUsage *next,const FormatTransientUsage *transient,FormatBudget *out);
const char *workbook_format_budget_message(FormatBudgetStatus status); /* Borrowed static diagnostic. */
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
