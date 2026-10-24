#ifndef DUXCEL_DEFINITION_ORIGIN_H
#define DUXCEL_DEFINITION_ORIGIN_H
#include "source_format_command.h"
#include "definition_baseline.h"
#include "source_format_baseline.h"
typedef struct DefinitionOrigin DefinitionOrigin;
typedef struct {
    Workbook *book;
    Analysis *primary; /* Real primary authority, independently owned App or sheet. */
    const uint64_t *request_epoch,*primary_epoch;
    size_t validation_reserved;
    bool ready,read_only;
} DefinitionOriginContext;
typedef enum { DEFINITION_ORIGIN_OPEN,DEFINITION_ORIGIN_ADMITTED } DefinitionOriginPhase;
typedef enum { DEFINITION_ORIGIN_READY,DEFINITION_ORIGIN_APPLIED,DEFINITION_ORIGIN_UNRESOLVED,
    DEFINITION_ORIGIN_STALE,DEFINITION_ORIGIN_INVALID,DEFINITION_ORIGIN_PROTECTED,DEFINITION_ORIGIN_LIMIT } DefinitionOriginStatus;
typedef struct {
    uint64_t request,owner,route,revision;
    const Analysis *analysis;
} DefinitionOriginReply;
typedef struct { size_t owned,validation_scratch; uint64_t owner; } DefinitionOriginCompletionInfo;
typedef struct { size_t asset_bytes,validation_scratch; } DefinitionOriginSealInfo;
typedef struct {
    uint64_t sheet,object_high_water,source_high_water;
    const void *primary,*objects,*sources,*formats;
    size_t primary_bytes,object_bytes,source_bytes,format_bytes;
} DefinitionOriginAssets;
typedef struct {
    DefinitionOriginPhase phase;
    uint64_t sheet,epoch;
    size_t owner_bytes,asset_bytes;
} DefinitionOriginState;
/* Runtime-only working definition origin; not Saved, recovery, a user edit,
   retention policy or durable Undo. The owner freezes exact current primary,
   cached sheet, object/format/ledger descriptors and document/sheet/request epochs.
   Text/packets remain trusted immutable model ownership. Begin may assign the
   same transient session IDs as other commands; authored state stays untouched.
   reserved covers the new native owner, not inputs or later assets/scratch. */
size_t definition_origin_owner_bytes(void);
DefinitionOriginStatus definition_origin_begin(DefinitionOriginContext *context,size_t sheet,size_t reserved,
    DefinitionOrigin **out,char **error);
DefinitionOriginState definition_origin_state(const DefinitionOrigin *origin);
/* Allocation-free shape/grammar-peak measurement; reply.owner is source owner0
   or an existing Table. route0 is primary; positive route must be that Table or
   an analytical object bound to it (source_id0 routes the primary). Exact latest
   request/route query revision plus unchanged authored source are mandatory.
   Only NULL DATABASE relation completion is admitted, never rebinding/Refresh.
   out remains unchanged on failure. No parser or source IO in measurement. */
DefinitionOriginStatus definition_origin_completion_measure(DefinitionOriginContext *context,const DefinitionOrigin *origin,
    const DefinitionOriginReply *reply,DefinitionOriginCompletionInfo *out,char **error);
/* Admit measured new schema/table ownership and parser scratch before grammar
   validation/copy. Recheck before a nonfallible metadata-only transfer; preserve
   every user action/Undo/Redo/document/query clock and immutable saved asset.
   Mark working state dirty, refresh THIS open owner's preimage and advance its
   runtime epoch. No event is fabricated. ADMITTED is terminal and rejects even
   an otherwise matching completion; callers cannot reopen it by clearing flags. */
DefinitionOriginStatus definition_origin_complete(DefinitionOriginContext *context,DefinitionOrigin *origin,
    const DefinitionOriginReply *reply,size_t reserved,char **error);
/* Before parsing/encoding, admit complete current primary/object grammar plus
   format-scope validation. Seal only resolved primary/Tables and closed source/
   format dependencies. Legacy finite imported formats require their compatibility
   baseline first. The existing codecs encode one independent canonical arena:
   DXAN primary, DXOB objects, DXSV immutable ledger and empty->current FormatDelta
   (absent exactly for empty legacy format state). This is not a DXDE user action.
   reserved covers the NEW final arena only; owner/live/parser budgets coexist.
   Failure preserves OPEN/no assets; success ADMITTED owns immutable bytes even
   after the live model is edited or freed. No IO or retained writer activation. */
/* Shape-only current grammar peak, with no parsing or allocation; then measure
   semantic closure under that admitted scratch before reserving exact arena bytes.
   Outputs are unchanged on failure. No assets/model/state are published. */
DefinitionOriginStatus definition_origin_scratch(DefinitionOriginContext *context,const DefinitionOrigin *origin,size_t *out,char **error);
DefinitionOriginStatus definition_origin_seal_measure(DefinitionOriginContext *context,const DefinitionOrigin *origin,DefinitionOriginSealInfo *out,char **error);
DefinitionOriginStatus definition_origin_seal(DefinitionOriginContext *context,DefinitionOrigin *origin,size_t reserved,char **error);
/* Actual open-origin authority for verified legacy formatting enrichment.
   The internal baseline context derives its phase/epoch from THIS owned origin,
   never an App flag. The origin must outlive the opaque plan. Apply normalizes
   all current/Undo/Redo formats+ledger+primary atomically, refreshes this SAME
   OPEN preimage and advances its internal epoch. Seal subsequently captures the
   final normalized assets. ADMITTED rejects both preparation and adoption. */
DefinitionOriginStatus definition_origin_legacy_prepare(DefinitionOriginContext *context,DefinitionOrigin *origin,
    const SourceVersion *verified,const SourcePacket *result,size_t reserved,
    SourceFormatBaselinePlan **out,SourceFormatBaselineInfo *info,char **error);
DefinitionOriginStatus definition_origin_legacy_apply(DefinitionOriginContext *context,DefinitionOrigin *origin,
    SourceFormatBaselinePlan **owned,char **error);
/* Borrow only after ADMITTED. Pointers live until owner free; immutable assets
   are never patched when schema/table, formatting or source metadata later change. */
DefinitionOriginAssets definition_origin_assets(const DefinitionOrigin *origin);
void definition_origin_free(DefinitionOrigin **origin);
#endif
