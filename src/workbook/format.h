#ifndef DUXCEL_FORMAT_H
#define DUXCEL_FORMAT_H
#include "../common.h"
#define DX_FORMAT_PATCHES 512
#define DX_FORMAT_HISTORY 32
#define DX_FORMAT_GUARDS 33
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
typedef enum { FORMAT_SPATIAL,FORMAT_RESULT,FORMAT_GUARDED } FormatSelector;
typedef struct {
    FormatRange range; unsigned mask; CellFormat value;
    FormatSelector selector;
    size_t *guard_ids; size_t guard_count; /* Owned indices into the snapshot's scope table. */
} FormatPatch;
typedef struct { FormatPatch *items; size_t count; FormatScope *scopes; size_t scope_count; } FormatSnapshot;
typedef struct {
    FormatPatch *items; size_t count; uint64_t revision;
    FormatSnapshot undo[DX_FORMAT_HISTORY],redo[DX_FORMAT_HISTORY]; size_t undo_count,redo_count;
    FormatScope *scopes; size_t scope_count;
} FormatStore;
CellFormat format_default(void);
/* Legacy/manual lookup: finite spatial and guarded manual parts apply. */
CellFormat format_at(const FormatStore *store,uint64_t row,size_t column);
/* NULL is a manual/blank cell. Imported targets never inherit an unguarded finite
   rectangle; only a whole-column spatial default or an exact guard may apply. */
CellFormat format_resolve(const FormatStore *store,uint64_t row,size_t column,const FormatScopeView *scope);
/* Resolve the definition once per owner/frame, then reuse its token for cells.
   0 means manual/blank; SIZE_MAX means unmatched imported data. Tokens expire at
   any store mutation, including Undo/Redo. Neither function allocates. */
size_t format_scope_token(const FormatStore *store,const FormatScopeView *scope);
CellFormat format_resolve_token(const FormatStore *store,uint64_t row,size_t column,size_t token);
bool format_valid(const FormatStore *store,char **error);
/* Valid scoped state is deliberately not serializable through workspace v12. */
bool format_persistable(const FormatStore *store,char **error);
size_t format_owned_bytes(const FormatStore *store,bool include_history);
bool format_apply(FormatStore *store,FormatRange range,unsigned mask,CellFormat value,char **error);
bool format_apply_scoped(FormatStore *store,FormatRange range,unsigned mask,CellFormat value,
    const FormatScopeView *scopes,size_t count,bool include_manual,char **error);
/* Explicit legacy migration only. Finite spatial patches gain guards and retain
   their manual component; full-column defaults and existing scopes are unchanged. */
bool format_plan_guard_legacy(const FormatStore *store,const FormatScopeView *scopes,size_t count,FormatSnapshot *out,char **error);
void format_free(FormatStore *store);
void format_copy(FormatStore *to,const FormatStore *from);
bool format_history(FormatStore *store,bool redo);
void format_trim_history(FormatStore *store,bool redo,size_t keep);
/* Plan before editing cells/objects, then atomically commit the owned snapshot.
   Unchanged plans are freed without adding an independent history entry. */
bool format_plan_move(const FormatStore *store,FormatRange range,int64_t dr,int dc,FormatSnapshot *out,char **error);
bool format_plan_axis(const FormatStore *store,bool rows,uint64_t at,uint64_t count,bool insert,FormatSnapshot *out,char **error);
bool format_commit(FormatStore *store,FormatSnapshot *owned);
void format_snapshot_free(FormatSnapshot *snapshot);
#endif
