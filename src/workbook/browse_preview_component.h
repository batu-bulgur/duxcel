#ifndef DX_BROWSE_PREVIEW_COMPONENT_H
#define DX_BROWSE_PREVIEW_COMPONENT_H
#include "browse_preview_set_codec.h"
#include "version_save.h"
#include "version_chunks.h"
#define DX_BROWSE_PREVIEW_COMPONENT_ENCODING 1u
#define DX_BROWSE_PREVIEW_COMPONENT_IO_BYTES (3u*DX_VERSION_CHUNK_BYTES+4096u)
typedef enum { BROWSE_COMPONENT_VERIFY,BROWSE_COMPONENT_ASSOCIATE,BROWSE_COMPONENT_DESCRIPTOR,
    BROWSE_COMPONENT_CHUNKS,BROWSE_COMPONENT_RECHECK } BrowsePreviewComponentPhase;
typedef struct {
    /* Read-only, non-reentrant; true cancels. Borrowed publication/snapshot and
       assets stay immutable through the enclosing genuine prepare callback. */
    bool (*cancelled)(void *context,BrowsePreviewComponentPhase phase,size_t entry,uint64_t bytes);
    void *context;
} BrowsePreviewComponentControl;
typedef struct {
    WorkbookId document;
    uint64_t snapshot_revision,target_bytes,set_bytes;
    unsigned char target_sha256[DX_SOURCE_DIGEST_BYTES],set_sha256[DX_SOURCE_DIGEST_BYTES];
    size_t entries,coexistence;
} BrowsePreviewComponentInfo;
typedef enum { BROWSE_COMPONENT_READY,BROWSE_COMPONENT_FAILED,BROWSE_COMPONENT_LIMIT,
    BROWSE_COMPONENT_CANCELLED,BROWSE_COMPONENT_UNSUPPORTED } BrowsePreviewComponentStatus;
/* Native unpublished User Save adapter ONLY. Publication is genuine controller
   authority, accepted solely inside its prepare callback with the exact borrowed
   connection/after-CoreSet and actual captured immutable snapshot. Checkpoint,
   Tag and exports reject; no inferred Latest Saved or fabricated expected model.
   Association uses that snapshot without another database/model decoder.

   Stage requires a fresh _duxcel_previews schema and explicit transaction.
   No BEGIN/COMMIT/ROLLBACK/connection ownership escapes; any non-READY requires
   caller rollback. Normal registry/reader/writer advertisement stays external.
   Selected physical root is recorded only after genuine controller selection
   and its complete comparison when reused. Set stays separately preview-only;
   nothing installs a Page, changes readiness or grants historical cache use.

   reserved covers set, new DXPS packet, sequential child encoding/admission
   scratch, bounded hash/SQL chunk scratch and other_owned (actual model/core
   leases/engine ownership remain explicitly caller-accounted). storage_reserved
   bounds new auxiliary payload bytes, not RSS or engine allocation. No payload
   reference or borrowed publication pointer is retained. out is scalar and
   unchanged on failure/cancel; READY remains prepublication, never a receipt. */
BrowsePreviewComponentStatus browse_preview_component_measure(const WorkbookVersionPublication *publication,
    duckdb_connection connection,const WorkbookVersionCoreSet *after,const BrowsePreviewSet *set,
    size_t other_owned,BrowsePreviewComponentInfo *out,char **error);
BrowsePreviewComponentStatus browse_preview_component_stage(const WorkbookVersionPublication *publication,
    duckdb_connection connection,const WorkbookVersionCoreSet *after,const BrowsePreviewSet *set,
    size_t other_owned,size_t reserved,uint64_t storage_reserved,BrowsePreviewComponentControl control,
    BrowsePreviewComponentInfo *out,char **error);
#endif
