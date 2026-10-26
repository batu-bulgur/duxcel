#ifndef DUXCEL_HISTORY_COMPONENT_H
#define DUXCEL_HISTORY_COMPONENT_H
#include "history_native_capture.h"
#include "history_after_save.h"
#include "history_profile_codec.h"
#define DX_HISTORY_COMPONENT_ENCODING 1u
#define DX_HISTORY_COMPONENT_HEAD 0u
#define DX_HISTORY_COMPONENT_PROFILE 1u
#define DX_HISTORY_COMPONENT_IO_BYTES (DX_SOURCE_DIGEST_BUFFER_BYTES+2048u)
typedef enum { HISTORY_COMPONENT_VERIFY,HISTORY_COMPONENT_DESCRIPTOR,HISTORY_COMPONENT_CHUNKS,HISTORY_COMPONENT_ADMIT,HISTORY_COMPONENT_RECHECK } WorkbookHistoryComponentPhase;
typedef struct {
    bool (*cancelled)(void *context,WorkbookHistoryComponentPhase phase,unsigned asset,uint64_t position,uint64_t bytes);
    void *context;
} WorkbookHistoryComponentControl;
typedef struct {
    WorkbookId document;
    WorkbookHistoryProfileKind kind;
    uint64_t revision,head_bytes,packet_bytes;
    unsigned char head_sha256[DX_SOURCE_DIGEST_BYTES],packet_sha256[DX_SOURCE_DIGEST_BYTES];
} WorkbookHistoryComponentInfo;
typedef enum { HISTORY_COMPONENT_READY,HISTORY_COMPONENT_FAILED,HISTORY_COMPONENT_CANCELLED,HISTORY_COMPONENT_LIMIT,HISTORY_COMPONENT_UNSUPPORTED } WorkbookHistoryComponentStatus;
/* Unpublished current SQL component in _duxcel_history, exact descriptor and
   ordered bounded chunks for actual sealed current15 head + current DXHP bytes.
   Uses only an exclusively borrowed explicit transaction; never opens/closes a
   connection or begins/commits/rolls back. Non-READY stage requires ROLLBACK.
   Stage requires a fresh reserved schema and an actual native capsule; no caller
   staged/coherent boolean. Caller-owned wire arena is independent and bounded;
   stage_reserved covers its full capacity plus DX_HISTORY_COMPONENT_IO_BYTES
   (one 64 KiB driver chunk and conservative bounded metadata/control heap).
   storage_reserved bounds both newly written assets before SQL mutation. Existing
   capture/graph/engine ownership remains separately charged. Head leases and all
   roots are verified before and after consumption; no source query or DB decoder
   is added. Normal archive schemas/readers/writers remain entirely inactive.
   This is PREPUBLICATION: an old Saved/tag index + dirty captured R is not a user
   Save receipt. Future Save must prepare actual after-Saved R/anchor/window state,
   consume the SAME frozen body and prove the outer authored head, not recapture
   after enrichment/R+1 or identify content solely by a revision.
   probe uses max_head_bytes before inventory scanning and admits bounded
   descriptor/chunk SHAPE only, not body/hash/model closure.
   read copies ordered bytes to caller wire arena, verifies exact packet hash,
   reuses existing core_payload_restore + profile codec + native capsule handoff.
   RootSet remains owned by caller; one independent retained reference is taken.
   profile_caps.staging_bytes covers full wire/parse arena capacities, DX_HISTORY_COMPONENT_IO_BYTES, prior profile coexistence and new candidate/semantic peak.
   native_caps covers prior/new physical extents/wrappers. New leaf staging uses
   only parent and its reserved physical grant. Existing output is unchanged on
   every failure/cancel; input arenas are disposable. No missing-schema/old-format
   fallback, live adoption, traversal or completed file publication is implied.
   Physical/typed closure does NOT prove every retained before/after applies to
   the actual captured head/checkpoint model. That bounded offline semantic
   association is still required before advertising usable retained Undo. */
WorkbookHistoryComponentStatus workbook_history_component_probe(duckdb_connection connection,uint64_t max_head_bytes,WorkbookHistoryComponentInfo *out,char **error);
WorkbookHistoryComponentStatus workbook_history_component_stage(duckdb_connection connection,const WorkbookHistoryNativeCapture *capture,
    void *wire,size_t wire_capacity,size_t stage_reserved,uint64_t storage_reserved,WorkbookHistoryComponentControl control,char **error);
/* Prepared after-Save metadata/root closure can be staged before final Commit.
   This still borrows a pinned source through the plan and proves no publication. */
WorkbookHistoryComponentStatus workbook_history_component_stage_after_save(duckdb_connection connection,const WorkbookHistoryAfterSave *plan,
    void *wire,size_t wire_capacity,size_t stage_reserved,uint64_t storage_reserved,WorkbookHistoryComponentControl control,char **error);
WorkbookHistoryComponentStatus workbook_history_component_read(duckdb_connection connection,const WorkbookVersionCoreSet *roots,
    WorkbookHistoryProfileBinding binding,const char *parent,WorkbookHistoryProfileCaps profile_caps,WorkbookHistoryNativeCaps native_caps,
    void *wire,size_t wire_capacity,void *parse,size_t parse_capacity,WorkbookHistoryComponentControl control,
    WorkbookHistoryNativeCapture **out,char **error);
#endif
