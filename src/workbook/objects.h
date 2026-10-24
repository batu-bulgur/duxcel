#ifndef DUXCEL_OBJECTS_H
#define DUXCEL_OBJECTS_H
#include "../transforms/pivot.h"
#include "../transforms/marks.h"
#include "cells.h"
#define DX_OBJECT_CURRENT_BYTES (8*1024*1024)
#define DX_OBJECT_HISTORY_BYTES (8*1024*1024) /* Each undo/redo snapshot stack. */
#define DX_OBJECT_NAME_BYTES 4096u
typedef enum { OBJECT_PIVOT,OBJECT_CHART,OBJECT_TABLE } ObjectKind;
typedef enum { MARK_BAR,MARK_LINE,MARK_DOT,MARK_SCATTER } ChartMark;
typedef struct {
    uint64_t id,row,rows,revision;
    size_t col,columns;
    char *name;
    ObjectKind kind;
    ChartMark mark;
    unsigned char chart_measure; /* Presentation index; never a query revision. */
    PivotDefinition pivot;
    Analysis source; /* owned by a table; analytical objects bind through source_id */
    MarkDefinition marks; /* owned presentation bindings, retained across representation changes */
    uint64_t source_id; /* 0 = worksheet primary source, otherwise a table ID */
} SheetObject;
typedef struct { SheetObject *items; size_t count; } ObjectSnapshot;
typedef struct {
    SheetObject *items; size_t count; uint64_t next_id,title_revision,marks_revision;
    ObjectSnapshot undo[32],redo[32]; size_t undo_count,redo_count;
} ObjectStore;
typedef struct { uint64_t r0,r1; size_t c0,c1; } SheetRange;
SheetRange objects_region(const ObjectStore *objects,const CellStore *cells,uint64_t source_rows,size_t source_columns,uint64_t row,size_t column,bool whole);
void objects_free(ObjectStore *store);
void objects_copy(ObjectStore *to,const ObjectStore *from);
bool objects_valid(const ObjectStore *store,char **error);
int objects_add(ObjectStore *store,uint64_t row,size_t col,const char *name,char **error);
int objects_add_chart(ObjectStore *store,uint64_t row,size_t col,const char *name,const PivotDefinition *definition,char **error);
int objects_add_table(ObjectStore *store,uint64_t row,size_t col,const char *name,const Analysis *source,char **error);
const SheetObject *objects_source(const ObjectStore *store,const SheetObject *object);
bool objects_bind_source(ObjectStore *store,size_t index,uint64_t source_id,char **error);
bool objects_set_source(ObjectStore *store,size_t index,const Analysis *source,char **error);
/* Prepare only a Table's worksheet-local source-version binding. out must be
   zero-initialized or independently owned. Failure preserves out and changed;
   a same-version success sets changed=false without replacing out. No history
   is pruned to admit the changed plan. The caller still validates the version
   ledger and coordinates the composite Objects/Format journal action. */
bool objects_plan_source_version(const ObjectStore *store,uint64_t owner,
    uint64_t expected_version,uint64_t next_version,ObjectSnapshot *out,bool *changed,char **error);
/* Consume a changed plan from objects_plan_source_version without allocation.
   The store must be unchanged since planning; owned must remain independent
   and unmodified. Moves the previous current snapshot into undo, clears redo,
   and preserves allocation/query/title/Marks counters. */
void objects_adopt_source_version(ObjectStore *store,ObjectSnapshot *owned);
/* Nonfallible model transfer after successful objects_plan_source_transform
   and complete command admission. The live store/independent plan are unchanged
   since planning. Move the prior current graph into bounded Undo, clear Redo
   and consume the after-plan; never clone history/current text again. Preserve
   store allocation/title/Marks counters and install the planned object query
   epochs. Caller reserves its event BEFORE any eviction, records the matching
   worksheet action once, publishes dirty/document clocks and invalidates jobs.
   This is not a standalone command or safe unchecked snapshot installer. */
void objects_adopt_source_transform(ObjectStore *store,ObjectSnapshot *owned);
/* Same nonfallible ownership transfer for admitted, identity-preserving object
   definition plans (including presentation). The command installs any changed
   monotonic title/Marks counters AFTER transfer and publishes one action. */
void objects_adopt_definition(ObjectStore *store,ObjectSnapshot *owned);
void objects_snapshot_free(ObjectSnapshot *owned);
/* Conservative source-binding reservation, not exact allocator measurement:
   a non-NULL array reserves DX_MAX_OBJECTS* sizeof(SheetObject), including when
   count is zero, plus every live name/Pivot/Analysis/Marks string. The snapshot
   has no capacity; ordinary legacy edit/history accounting remains separate.
   Allocation-free; SIZE_MAX for invalid shape, overflow, or noncanonical owned
   strings in inactive fixed slots (which cannot be safely cloned). Requires
   readable strings; does not prove allocation independence or parse payloads. */
size_t objects_snapshot_owned_bytes(const ObjectSnapshot *state);
/* Pin a worker-resolved default relation without a new edit or query revision. */
bool objects_resolve_source(ObjectStore *store,size_t index,const Analysis *resolved,char **error);
/* Borrow valid bounded definitions. Preserve field/aggregate identity across
   reorder; aggregate edits retain the slot, removal falls back to measure zero. */
size_t objects_pivot_measure_selection(const PivotDefinition *before,size_t selected,const PivotDefinition *after);
bool objects_update(ObjectStore *store,size_t index,const PivotDefinition *definition,char **error);
/* Presentation-only change: retain query revision and cached result. */
bool objects_mark(ObjectStore *store,size_t index,ChartMark mark,char **error);
bool objects_chart_measure(ObjectStore *store,size_t index,size_t measure,char **error);
bool objects_marks(ObjectStore *store,size_t index,const MarkDefinition *marks,char **error);
/* Switch representation of the same bound result, without a query revision.
   Caller supplies the cached matrix extent or desired chart extent. */
bool objects_representation(ObjectStore *store,size_t index,ObjectKind kind,ChartMark mark,
    uint64_t rows,size_t columns,const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error);
bool objects_rename(ObjectStore *store,size_t index,const char *name,char **error);
/* Allocation-free shared rename rules: trim, control-byte rejection, sheet-local
   ASCII-case-insensitive uniqueness and current graph byte limit. out provides
   DX_OBJECT_NAME_BYTES+1 bytes and receives the normalized name on success.
   Failure preserves out/changed; no model/history/counter mutation or grammar IO.
   Trusted readable model/input; full canonical graph validation is separate. */
bool objects_prepare_name(const ObjectStore *store,size_t index,const char *name,
    char out[DX_OBJECT_NAME_BYTES+1],bool *changed,char **error);
bool objects_remove(ObjectStore *store,size_t index);
bool objects_history(ObjectStore *store,bool redo);
void objects_trim_history(ObjectStore *store,bool redo,size_t keep);
/* Caller validates the complete destination first. Preserves result revisions. */
void objects_translate_mask(ObjectStore *store,uint32_t mask,int64_t dr,int dc);
int objects_hit(const ObjectStore *store,uint64_t row,size_t col);
bool objects_fits(const ObjectStore *store,int ignore,uint64_t row,size_t col,uint64_t rows,size_t columns,
    const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error);
bool objects_move(ObjectStore *store,size_t index,uint64_t row,size_t col,const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error);
/* Chart presentation only: 8–4096 rows, 3–256 columns; preserve query revision. */
bool objects_resize_chart(ObjectStore *store,size_t index,uint64_t rows,size_t columns,const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error);
#endif
