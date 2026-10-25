#ifndef DUXCEL_DEFINITION_ORIGIN_H
#define DUXCEL_DEFINITION_ORIGIN_H
#include "source_format_command.h"
#include "definition_baseline.h"
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
   cached sheet, object/format/ledger descriptors and document/sheet action state.
   Authored equality follows definition_state: non-Chart result extents, valid
   numeric inference and runtime query epochs may vary; borrowed pointers,
   placement, Chart size, source/filter/IDs/format/ledger/history stay exact.
   Current materialized shape remains bounded and valid. Actual completion
   matches the current request and current route/primary query epoch.
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
/* Pre-free Analysis lifecycle handoff. READY proves the current OPEN owner;
   STALE is explicit non-admission, not permission to reset its preimage. An App
   without retained activation may continue its ordinary Save/navigation policy
   for an already stale origin, but cannot reattach it after freeing owners.
   ADMITTED needs no handoff: its independent arena must never be rebound. */
typedef enum { DEFINITION_HANDOFF_CACHE_SYNC,DEFINITION_HANDOFF_SUSPEND_TO_SHEET,DEFINITION_HANDOFF_RESUME_TO_APP } DefinitionOriginHandoffOperation;
typedef enum { DEFINITION_HANDOFF_READY,DEFINITION_HANDOFF_APPLIED,DEFINITION_HANDOFF_STALE,DEFINITION_HANDOFF_ADMITTED,
    DEFINITION_HANDOFF_PROTECTED,DEFINITION_HANDOFF_INVALID,DEFINITION_HANDOFF_LIMIT,DEFINITION_HANDOFF_BUSY } DefinitionOriginHandoffStatus;
typedef struct DefinitionOriginHandoff DefinitionOriginHandoff;
typedef struct {
    DefinitionOriginHandoffOperation operation;
    Analysis *destination; /* RESUME only: independent external App location; occupied during prepare is allowed. */
} DefinitionOriginHandoffRequest;
typedef struct {
    size_t owned,clone_owned,coexisting;
    uint64_t origin_epoch,request,primary_epoch;
} DefinitionOriginHandoffInfo;
/* Allocation-free shape/ownership measurement. owned is NEW plan+clone text;
   clone_owned (Analysis+text) is a subset, not an additional reservation.
   coexisting includes old actual/cache Analysis ownership plus owned. Existing
   workbook/owner bytes remain separately charged. No grammar/assets admission.
   CACHE_SYNC keeps equal authored cache text pointers and copies only valid
   numeric inference; a lagging cache clones ONLY the proven actual primary.
   SUSPEND moves actual App ownership into the sheet cache and clears App, with
   NULL authority encoding so moving the Worksheet allocation is harmless.
   RESUME clones sheet authority independently; an occupied destination is only
   observed/charged during prepare. The coordinator clears it after checking
   both outgoing/incoming tickets, before RESUME adoption. */
DefinitionOriginHandoffStatus definition_origin_handoff_measure(DefinitionOriginContext *context,const DefinitionOrigin *origin,
    DefinitionOriginHandoffRequest request,DefinitionOriginHandoffInfo *out,char **error);
/* Reserve owned BEFORE clone; old/new owners coexist and remain alive. Prepared
   ticket pins exact current request/primary epoch, actual/cache values, origin
   epoch and destination. Failure leaves out and every owner unchanged. */
DefinitionOriginHandoffStatus definition_origin_handoff_prepare(DefinitionOriginContext *context,DefinitionOrigin *origin,
    DefinitionOriginHandoffRequest request,size_t reserved,DefinitionOriginHandoff **out,char **error);
/* Read-only paired preflight before either half of a switch transfers ownership.
   Destination must still equal its prepared snapshot or be empty after the
   coordinator's outgoing transfer. READY does not assert destination is empty.
   Same-thread coordinator changes no document/request/primary clock between
   the checks and adopts; each origin's local epoch advances independently. */
DefinitionOriginHandoffStatus definition_origin_handoff_check(DefinitionOriginContext *context,DefinitionOrigin *origin,
    const DefinitionOriginHandoff *ticket,char **error);
/* Recheck pointer-first while old owners live, then transfer without allocation
   or fallible work. Update context.primary on SUSPEND/RESUME and refresh ONLY
   borrowed role/cache descriptors; authored primary/history/ledger/format/IDs
   and canonical bytes never change. Success consumes ticket; failure preserves
   ticket and owners, allowing cancel. RESUME returns BUSY until destination is
   empty; it never disposes its outgoing occupant. Do not free/rebind an origin
   owner before its own prepared handoff. */
DefinitionOriginHandoffStatus definition_origin_handoff_adopt(DefinitionOriginContext *context,DefinitionOrigin *origin,
    DefinitionOriginHandoff **ticket,char **error);
void definition_origin_handoff_cancel(DefinitionOriginHandoff **ticket);
/* Before parsing/encoding, admit complete current primary/object grammar plus
   format-scope validation. Seal only resolved primary/Tables and closed source/
   format dependencies. Imported finite formats require source-aware ownership.
   The existing codecs encode one independent canonical arena:
   DXAN primary, DXOB objects, DXSV immutable ledger and empty->current FormatDelta
   (absent exactly for empty manual format state). This is not a DXDE user action.
   reserved covers the NEW final arena only; owner/live/parser budgets coexist.
   Failure preserves OPEN/no assets; success ADMITTED owns immutable bytes even
   after the live model is edited or freed. Non-Chart dimensions are the exact
   frozen materialized restoration footprint, NOT final source-count proof.
   Later result/cache growth never patches this arena, Saved roots or events.
   A later DXDE preimage can have different layout bytes but equal authored
   definitions. No origin-to-event byte-continuity/replay contract is supplied
   here; retained replay must admit that semantic/render-layout boundary.
   No IO or retained writer activation. */
/* Shape-only current grammar peak, with no parsing or allocation; then measure
   semantic closure under that admitted scratch before reserving exact arena bytes.
   Outputs are unchanged on failure. No assets/model/state are published. */
DefinitionOriginStatus definition_origin_scratch(DefinitionOriginContext *context,const DefinitionOrigin *origin,size_t *out,char **error);
DefinitionOriginStatus definition_origin_seal_measure(DefinitionOriginContext *context,const DefinitionOrigin *origin,DefinitionOriginSealInfo *out,char **error);
DefinitionOriginStatus definition_origin_seal(DefinitionOriginContext *context,DefinitionOrigin *origin,size_t reserved,char **error);
/* Borrow only after ADMITTED. Pointers live until owner free; immutable assets
   are never patched when schema/table, formatting or source metadata later change. */
DefinitionOriginAssets definition_origin_assets(const DefinitionOrigin *origin);
void definition_origin_free(DefinitionOrigin **origin);
#endif
