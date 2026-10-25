#ifndef DUXCEL_PRIMARY_SOURCE_DRAFT_H
#define DUXCEL_PRIMARY_SOURCE_DRAFT_H
#include "cell_command.h"
#include "definition_baseline.h"
/* Unpublished CSV/Parquet/demo or pending DATABASE primary probe; separate from existing-owner origin enrichment
   and from the future detached Sheet creation/presence-asset coordinator. */
typedef struct PrimarySourceDraft PrimarySourceDraft;
typedef struct {
    const Workbook *book;
    const Analysis *primary; /* Actual current App authority, borrowed. */
    const uint64_t *request,*primary_epoch;
    bool ready,read_only;
} PrimarySourceDraftContext;
typedef struct { size_t owned,validation_scratch,naming_scratch; } PrimarySourceDraftInfo;
typedef struct { uint64_t request; const Analysis *analysis; const char *error; } PrimarySourceDraftReply;
typedef struct { Analysis source; char *name; } PrimarySourceCandidate;
typedef enum { PRIMARY_SOURCE_READY,PRIMARY_SOURCE_RESOLVED,PRIMARY_SOURCE_TAKEN,PRIMARY_SOURCE_STALE,
    PRIMARY_SOURCE_PROTECTED,PRIMARY_SOURCE_INVALID,PRIMARY_SOURCE_LIMIT,PRIMARY_SOURCE_FAILED } PrimarySourceDraftStatus;
/* Caller first establishes genuine EXISTING document/sheet transient sessions
   through workbook_prepare_command_session and assigns the actual probe request.
   No new sheet ID, high-water, action, revision, Redo, dirty flag or IO changes.
   Measure is allocation-free; begin reserves owned+naming+grammar scratch BEFORE
   calling the actual naming helper/parser or cloning a source proposal. DATABASE
   requires NULL table; an explicit non-NULL schema stays exact during resolution.
   CSV/Parquet require a nonempty path and no schema/table. Demo preserves the
   valid NULL/empty locator contract and rejects a nonempty locator.
   out stays unchanged on failure. Inputs are trusted readable model ownership. */
PrimarySourceDraftStatus primary_source_draft_measure(PrimarySourceDraftContext context,const Analysis *proposal,
    const char *name,PrimarySourceDraftInfo *out,char **error);
PrimarySourceDraftStatus primary_source_draft_begin(PrimarySourceDraftContext context,const Analysis *proposal,
    const char *name,size_t reserved,PrimarySourceDraft **out,char **error);
/* Exact short-lived lease: actual document address/identity/session, active
   sheet/topology/names, command/history/high-water, request/query epoch and raw
   current primary/cache ownership. Pointer/scalar checks precede semantic work.
   Unlike a long-lived origin, this unpublished probe may be cancelled and
   prepared anew after navigation/Save invalidates its request/borrowed lease. */
PrimarySourceDraftStatus primary_source_draft_check(PrimarySourceDraftContext context,const PrimarySourceDraft *draft,char **error);
/* Reply comes only from the associated real probe, not a current Page or cache.
   Only DATABASE schema/table default enrichment may differ; CSV/Parquet/demo
   preserve ALL authored fields. Valid numeric inference remains runtime metadata.
   Error, stale, unresolved or
   grammar/budget failure preserves the proposal, candidate and working book.
   Existing draft bytes/results remain separately charged; reply owned+validation scratch
   is the additional clone/validation peak. A resolved draft rejects overwrite. */
PrimarySourceDraftStatus primary_source_draft_reply_measure(PrimarySourceDraftContext context,const PrimarySourceDraft *draft,
    PrimarySourceDraftReply reply,PrimarySourceDraftInfo *out,char **error);
PrimarySourceDraftStatus primary_source_draft_resolve(PrimarySourceDraftContext context,PrimarySourceDraft *draft,
    PrimarySourceDraftReply reply,size_t reserved,char **error);
const Analysis *primary_source_draft_source(const PrimarySourceDraft *draft);
const char *primary_source_draft_name(const PrimarySourceDraft *draft);
size_t primary_source_draft_owned(const PrimarySourceDraft *draft);
/* Candidate view stays borrowed until immediate take or draft disposal. Take
   rechecks the current lease and moves independent source/name ownership only.
   It DOES NOT authorize Sheet creation: the future coordinator still reserves
   a complete after-sheet/presence asset and rechecks its own creation fence.
   No speculative nonzero Sheet ID or archive payload exists in this module. */
PrimarySourceDraftStatus primary_source_draft_take(PrimarySourceDraftContext context,PrimarySourceDraft **draft,
    PrimarySourceCandidate *out,char **error);
void primary_source_candidate_free(PrimarySourceCandidate *candidate);
void primary_source_draft_free(PrimarySourceDraft **draft);
#endif
