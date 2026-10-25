#ifndef DUXCEL_HISTORY_PROFILE_H
#define DUXCEL_HISTORY_PROFILE_H
#include "cell_history_payload.h"
#include "source_history_slot.h"
#include "range_history_slot.h"
#include "sheet_history_slot.h"
/* Finite admission limits, not permission to truncate a protected interval. */
#define DX_HISTORY_PROFILE_RECORDS 4096u
#define DX_HISTORY_PROFILE_OWNED_BYTES (64u*1024u*1024u)
#define DX_HISTORY_PROFILE_STAGING_BYTES (128u*1024u*1024u)
typedef enum { HISTORY_PROFILE_CHECKPOINT_ONLY,HISTORY_PROFILE_RETAINED } WorkbookHistoryProfileKind;
typedef struct {
    WorkbookVersionOwner owner; /* Expected transient stream owner, never a durable key. */
    uint64_t edit_session;
    const WorkbookHistoryEventInfo *event;
    const void *data,*dependencies;
    size_t bytes,dependency_bytes;
    uint64_t sheet;
    union { unsigned effects; WorkbookSheetOperation operation; } intent;
    const SourceDependencyLive *live; /* Optional metadata comparison only; never retained. */
} WorkbookHistoryProfileRecord;
typedef struct {
    const WorkbookHistoryPath *path;
    const size_t *records; /* One ordinal per path event into the unique record table. */
    size_t count;
} WorkbookHistoryProfilePath;
typedef struct {
    WorkbookId document;
    unsigned container_version,core_version; /* Exact current nested16/core15. */
    WorkbookHistoryProfileKind kind;
    const WorkbookVersionIndex *versions;
    const WorkbookHistoryAnchors *anchors;
    WorkbookHistoryProfilePath current,saved;
    const WorkbookHistoryProfileRecord *records; /* Strict increasing transition identity. */
    size_t count;
} WorkbookHistoryProfileView;
typedef struct {
    size_t records,metadata_bytes,payload_bytes,staging_bytes;
} WorkbookHistoryProfileCaps;
typedef struct {
    size_t records,metadata_bytes,payload_bytes,owned,prior_owned,validation_peak,staging;
    uint64_t root_bytes; /* Described physical bytes, not a RAM reservation or hash proof. */
} WorkbookHistoryProfileMeasure;
typedef struct {
    unsigned family;
    uint64_t sheet;
    bool source_compared;
    SourceDependencyStatus compatibility;
    SourceDependencyAdmission observations;
} WorkbookHistoryProfileReport;
typedef enum { HISTORY_PROFILE_OK,HISTORY_PROFILE_INVALID,HISTORY_PROFILE_UNSUPPORTED,
    HISTORY_PROFILE_LIMIT,HISTORY_PROFILE_ASSOCIATION,HISTORY_PROFILE_COVERAGE,HISTORY_PROFILE_METADATA } WorkbookHistoryProfileStatus;
typedef struct WorkbookHistoryProfile WorkbookHistoryProfile;
/* Combined CURRENT metadata/typed-closure candidate, not a new file encoding.
   Explicit checkpoint-only current artifacts advertise no event history. Retained
   mode exactly covers the union of Saved/current paths, including Redo; shared
   prefix slots reference the SAME unique transition record, decoded once.
   Existing anchor/window/protection and typed family1..5 validators remain the
   authority. No omitted family/default, snapshot-invented Undo or silent pruning.
   The candidate independently owns copied metadata/frames/canonical bytes and
   scalar reports; temporary existing family decoder graphs are released serially.
   No process pointer/session clock is serialized or used as durable identity:
   document/anchor/lineage/transition/position remain the declared durable keys.
   Preflight allocates nothing, including the allocation-free cell inspection.
   Complete new ownership + prior-candidate coexistence + maximum serial decoder
   peak is admitted before allocation. Caps are explicit finite caller policy;
   exceedance refuses the whole candidate and preserves every old/input owner.
   Inputs stay immutable/readable through all passes, with no callbacks/IO.
   REQUIRED OUTER CONTRACT: existing native admitted core owners separately prove
   exact physical root bytes/chunks/digests/document/model closure and stay owned
   through publication. This controller joins descriptors, never certifies their
   physical bodies or accepts an unchecked caller coherence boolean. Source
   compatibility/pending/unbound/opaque observations never imply execution
   readiness. No live document adoption, replay, writer/schema activation or GC. */
WorkbookHistoryProfileStatus workbook_history_profile_measure(WorkbookHistoryProfileView view,WorkbookHistoryProfileCaps caps,
    const WorkbookHistoryProfile *prior,WorkbookHistoryProfileMeasure *out);
WorkbookHistoryProfileStatus workbook_history_profile_prepare(WorkbookHistoryProfileView view,WorkbookHistoryProfileCaps caps,
    WorkbookHistoryProfile **out);
void workbook_history_profile_free(WorkbookHistoryProfile **profile);
/* Immutable borrows last until profile replacement/free. Runtime/live ledger
   pointers never escape the admission call; returned records have live=NULL. */
const WorkbookHistoryProfileView *workbook_history_profile_view(const WorkbookHistoryProfile *profile);
const WorkbookHistoryProtection *workbook_history_profile_protection(const WorkbookHistoryProfile *profile);
const WorkbookHistoryProfileReport *workbook_history_profile_report(const WorkbookHistoryProfile *profile,size_t index);
size_t workbook_history_profile_owned(const WorkbookHistoryProfile *profile);
#endif
