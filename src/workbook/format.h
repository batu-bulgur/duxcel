#ifndef DUXCEL_FORMAT_H
#define DUXCEL_FORMAT_H
#include "../common.h"
#define DX_FORMAT_PATCHES 512
#define DX_FORMAT_HISTORY 32
#define DX_FORMAT_GUARDS 33
#define DX_FORMAT_RECORDS 20000
#define DX_FORMAT_FIELDS 16384
#define DX_FORMAT_SCOPE_BYTES (256u*1024u)
#define DX_FORMAT_CURRENT_BYTES (1024u*1024u)
#define DX_FORMAT_HISTORY_BYTES (32u*1024u*1024u)
typedef enum { FORMAT_AUTO,FORMAT_LEFT,FORMAT_CENTER,FORMAT_RIGHT } FormatAlignment;
typedef enum { FORMAT_GENERAL,FORMAT_ACCOUNTING,FORMAT_NUMBER } FormatNumber;
enum { FORMAT_BOLD=1,FORMAT_ITALIC=2,FORMAT_UNDERLINE=4,FORMAT_ALIGN=8,
    FORMAT_COLOR=16,FORMAT_SIZE=32,FORMAT_NUMERIC=64,FORMAT_DECIMALS=128,FORMAT_ALL=255 };
typedef struct {
    unsigned char flags,align,color,size,number,decimals;
    unsigned char specified; /* Derived by format_at; not stored as a property. */
} CellFormat;
typedef struct { uint64_t r0,r1; size_t c0,c1; } FormatRange;
/* The producer owns the exact canonical result/schema/source-version definition.
   These bytes are copied on edits, never borrowed by a stored patch. */
typedef struct { uint64_t owner; const void *definition; size_t bytes; } FormatScopeView;
typedef struct { uint64_t owner; void *definition; size_t bytes; } FormatScope;
typedef enum { FORMAT_SPATIAL,FORMAT_RESULT,FORMAT_GUARDED,FORMAT_RECORDS } FormatSelector;
typedef struct {
    FormatRange range; unsigned mask; CellFormat value;
    FormatSelector selector;
    size_t *guard_ids; size_t guard_count; /* Owned indices into the snapshot's scope table. */
    /* FORMAT_RECORDS only: one exact scope, and the Cartesian membership of
       these owned, strictly increasing vectors. range is unused and zero. */
    uint64_t *record_ids; size_t record_count;
    size_t *fields; size_t field_count; /* Original source ordinals, not display columns. */
} FormatPatch;
/* Scope-aware interpretation is explicit owned snapshot state. Compaction can
   remove every scope without reverting finite spatial patches to legacy data
   formatting. Undo/Redo restores this mode together with the exact patches.
   It does not itself activate a renderer or a source-format producer. */
typedef struct { FormatPatch *items; size_t count; FormatScope *scopes; size_t scope_count; bool source_aware; } FormatSnapshot;
typedef struct {
    FormatPatch *items; size_t count; uint64_t revision;
    FormatSnapshot undo[DX_FORMAT_HISTORY],redo[DX_FORMAT_HISTORY]; size_t undo_count,redo_count;
    FormatScope *scopes; size_t scope_count;
    bool source_aware;
} FormatStore;
CellFormat format_default(void);
/* Legacy/manual lookup: finite spatial and guarded manual parts apply. Record
   selectors never apply through this or either legacy scoped resolver. */
CellFormat format_at(const FormatStore *store,uint64_t row,size_t column);
/* NULL is a manual/blank cell. Imported targets never inherit an unguarded finite
   rectangle; only a whole-column spatial default or an exact guard may apply. */
CellFormat format_resolve(const FormatStore *store,uint64_t row,size_t column,const FormatScopeView *scope);
/* Resolve the definition once per owner/frame, then reuse its token for cells.
   0 means manual/blank; SIZE_MAX means unmatched imported data. Tokens expire at
   any store mutation, including Undo/Redo. Neither function allocates. */
size_t format_scope_token(const FormatStore *store,const FormatScopeView *scope);
CellFormat format_resolve_token(const FormatStore *store,uint64_t row,size_t column,size_t token);
/* Includes record membership in the same newest-mask precedence as positional
   guards/defaults. The producer validates the original ordinal against its exact
   typed schema in scope; a missing/unknown record context never matches records.
   Exact result definitions include sort/filter while raw-record definitions do
   not, so each has its own token. An imported record context never falls back
   to manual spatial formatting when its result token is missing/unmatched.
   Both matched tokens must name the same owner; an inconsistent pair permits
   only broad spatial defaults, never cross-owner or aggregate propagation.
   Like the legacy resolvers, this lookup does not allocate. */
CellFormat format_resolve_record_token(const FormatStore *store,uint64_t row,size_t column,
    size_t result_token,size_t record_token,uint64_t record_id,size_t original_field);
bool format_valid(const FormatStore *store,char **error);
/* Valid scoped state/mode is deliberately not serializable through v12, even
   after its last scope was compacted away. */
bool format_persistable(const FormatStore *store,char **error);
size_t format_owned_bytes(const FormatStore *store,bool include_history);
bool format_apply(FormatStore *store,FormatRange range,unsigned mask,CellFormat value,char **error);
bool format_apply_scoped(FormatStore *store,FormatRange range,unsigned mask,CellFormat value,
    const FormatScopeView *scopes,size_t count,bool include_manual,char **error);
/* Native v14 can archive these dormant selectors; live producers must still
   verify their source/fields and captured request before activation. IDs must be
   nonzero and strictly increasing (at most 20,000); original field ordinals must
   be strictly increasing in [0,16383]. Inputs and the exact scope are copied. */
bool format_apply_records(FormatStore *store,const FormatScopeView *scope,
    const uint64_t *record_ids,size_t record_count,const size_t *fields,size_t field_count,
    unsigned mask,CellFormat value,char **error);
/* Preflight the same edit without touching the live store/history. out must be
   zero-initialized or own a valid independent snapshot; success replaces it,
   failure preserves it. The plan owns all inputs and has passed current/history
   budgets. Commit only while the captured store is unchanged, or free to cancel.
   A no-op plan commits without creating an independent history entry. */
bool format_plan_records(const FormatStore *store,const FormatScopeView *scope,
    const uint64_t *record_ids,size_t record_count,const size_t *fields,size_t field_count,
    unsigned mask,CellFormat value,FormatSnapshot *out,char **error);
/* Explicit legacy migration only. Finite spatial patches gain guards and retain
   their manual component; full-column defaults and existing scopes are unchanged.
   The planned mode becomes source-aware even when no finite patches remain. */
bool format_plan_guard_legacy(const FormatStore *store,const FormatScopeView *scopes,size_t count,FormatSnapshot *out,char **error);
void format_free(FormatStore *store);
void format_copy(FormatStore *to,const FormatStore *from);
bool format_history(FormatStore *store,bool redo);
void format_trim_history(FormatStore *store,bool redo,size_t keep);
/* Plan before editing cells/objects, then atomically commit the owned snapshot.
   Unchanged plans are freed without adding an independent history entry.
   Moves mixing record membership and affected same-owner positional guards are
   rejected until a planner can preserve their precedence without scoped resets. */
bool format_plan_move(const FormatStore *store,FormatRange range,int64_t dr,int dc,FormatSnapshot *out,char **error);
bool format_plan_axis(const FormatStore *store,bool rows,uint64_t at,uint64_t count,bool insert,FormatSnapshot *out,char **error);
bool format_commit(FormatStore *store,FormatSnapshot *owned);
void format_snapshot_free(FormatSnapshot *snapshot);
#endif
