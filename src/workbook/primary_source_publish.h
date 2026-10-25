#ifndef DUXCEL_PRIMARY_SOURCE_PUBLISH_H
#define DUXCEL_PRIMARY_SOURCE_PUBLISH_H
#include "primary_source_draft.h"
#include "definition_origin.h"
#include "sheet_presence_asset.h"
#include "lifecycle.h"
typedef struct PrimarySourcePublishPlan PrimarySourcePublishPlan;
typedef struct {
    size_t coexisting,live,draft,presence,creation,origin,scratch;
    uint64_t prospective_sheet;
} PrimarySourcePublishInfo;
typedef enum { PRIMARY_PUBLISH_READY,PRIMARY_PUBLISH_ADMITTED,PRIMARY_PUBLISH_PENDING,
    PRIMARY_PUBLISH_STALE,PRIMARY_PUBLISH_INVALID,PRIMARY_PUBLISH_LIMIT,PRIMARY_PUBLISH_PROTECTED,
    PRIMARY_PUBLISH_FAILED } PrimarySourcePublishStatus;
typedef struct {
    uint64_t sheet;
    PrimarySourceCandidate candidate; /* Independent App surface source/name. */
    SheetPresenceAsset *presence; /* Independent prospective preimage, not an event. */
    DefinitionOrigin *origin; /* Cached-sheet authority; may be OPEN only when PENDING. */
    DefinitionOriginStatus origin_status;
} PrimarySourcePublication;
/* Pure session publication, not retained topology writing. Measure is allocation
   free. coexisting reserves the complete old Worksheet allocation/current primary
   and cache text, draft, NEW plan/presence/parser, real CREATE staging, new origin
   owner/arena/parser at once. Shared production cost functions are authoritative.
   App registry wrappers, Page/reply/worker/retiring/held-head ownership and other
   governor residents remain OUTER charges; this is a conservative ownership
   reservation, not an RSS bound. No ID/action is issued by measure/prepare.
   An already RESOLVED draft and genuine existing command sessions are required. */
PrimarySourcePublishStatus primary_source_publish_measure(PrimarySourceDraftContext context,
    const PrimarySourceDraft *draft,PrimarySourcePublishInfo *out,char **error);
/* Reserve complete coexisting BEFORE grammar/clone. Only success takes *draft;
   rejection leaves draft, old plan and book untouched. out must be empty. */
PrimarySourcePublishStatus primary_source_publish_prepare(PrimarySourceDraftContext context,
    PrimarySourceDraft **draft,size_t reserved,PrimarySourcePublishPlan **out,char **error);
PrimarySourcePublishStatus primary_source_publish_check(PrimarySourceDraftContext context,
    const PrimarySourcePublishPlan *plan,char **error);
/* Recheck the exact probe lease and reservation, then take the source immediately
   before the REAL SHEET_CREATE producer. Only that producer allocates real ID and
   chronological action. Failure before publication leaves the book unchanged;
   after taking, an unpublished plan is terminal (cancel it, do not retry).
   Success consumes plan and transfers independently owned output. Postpublication
   origin admission uses PRE-reserved bytes only. ADMITTED proves immutable arena;
   PENDING explicitly means the usable created session exists but retained origin
   readiness is absent. Never interpret PENDING as rollback or a sealed baseline.
   The origin borrows the NEW sheet cache, not candidate.source. OPEN authority
   must use the typed lifecycle handoff before App/cache ownership changes.
   Output freeing releases auxiliary ownership and never removes a created sheet. */
PrimarySourcePublishStatus primary_source_publish_apply(PrimarySourceDraftContext context,
    PrimarySourcePublishPlan **plan,size_t reserved,PrimarySourcePublication *out,char **error);
void primary_source_publish_cancel(PrimarySourcePublishPlan **plan);
void primary_source_publication_free(PrimarySourcePublication *publication);
#endif
