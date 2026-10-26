#ifndef DX_BROWSE_PREVIEW_SET_H
#define DX_BROWSE_PREVIEW_SET_H
#include "browse_preview_workbook.h"
#define DX_BROWSE_PREVIEW_SET_ENTRIES (DX_MAX_SHEETS*(DX_MAX_OBJECTS+1u))
typedef struct BrowsePreviewSet BrowsePreviewSet;
typedef enum { BROWSE_PREVIEW_AUXILIARY_CURRENT_ARTIFACT=1 } BrowsePreviewSetScope;
typedef struct {
    uint64_t sheet,owner,source_version;
    size_t encoded_bytes,owned_bytes;
    BrowsePreviewWorkbookAdmission admission;
} BrowsePreviewManifestEntry;
typedef struct {
    WorkbookId document;
    uint64_t snapshot_revision;
    BrowsePreviewSetScope scope;
    size_t count,capacity,assets_owned,owned,validation_scratch,coexistence;
} BrowsePreviewSetInfo;
typedef enum { BROWSE_PREVIEW_SET_OK,BROWSE_PREVIEW_SET_INVALID,BROWSE_PREVIEW_SET_LIMIT,
    BROWSE_PREVIEW_SET_ASSOCIATION,BROWSE_PREVIEW_SET_CANCELLED,BROWSE_PREVIEW_SET_UNSUPPORTED } BrowsePreviewSetStatus;
typedef struct {
    /* Read-only/non-reentrant; inputs/snapshot/references remain immutable and
       alive. Called before each entry and once at entry==count before transfer.
       True cancels preparation; no callback runs after ownership publication. */
    bool (*cancelled)(void *context,size_t entry);
    void *context;
} BrowsePreviewSetControl;
/* One actual immutable snapshot, auxiliary current-publication-head scope only.
   No historical preview/Save As authority or physical root proof. Entries are
   canonically ordered by(sheet,primary0/Table owner), with no payload clone or
   source/model ownership. Snapshot revision is metadata, not data freshness.

   Each input slot owns one explicit asset reference. Retain borrowed entries
   from a prior set first; neither take nor getters manufacture ownership.
   Duplicate keys/pointers reject, including independently captured same-owner
   assets. All inputs/out remain unchanged on cancel/invalid/association/limit.
   Success nulls every transferred input slot and replaces one output ref.

   retained_limit bounds the complete NEW set including exact entry capacity and
   asset ownership. reserved covers new+prior+temporary-admission coexistence
   plus other_owned (actual model/other inputs; exclude supplied assets/prior).
   Shared prior/new payloads are conservatively charged twice. Measure performs
   allocation-free framing/owner bounds; full metadata compatibility is checked
   serially by take. Incompatible optional assets are not silently omitted. */
BrowsePreviewSetStatus browse_preview_set_measure(const Workbook *actual,BrowsePreview *const *assets,size_t count,
    const BrowsePreviewSet *prior,size_t retained_limit,size_t other_owned,BrowsePreviewSetInfo *out);
BrowsePreviewSetStatus browse_preview_set_take(const Workbook *actual,BrowsePreview **assets,size_t count,
    size_t retained_limit,size_t other_owned,size_t reserved,BrowsePreviewSetControl control,BrowsePreviewSet **out);
BrowsePreviewSetInfo browse_preview_set_info(const BrowsePreviewSet *set);
const BrowsePreviewManifestEntry *browse_preview_set_manifest(const BrowsePreviewSet *set,size_t position);
const BrowsePreview *browse_preview_set_asset(const BrowsePreviewSet *set,size_t position);
/* Keep one owning ref alive through retain and borrowed reads. No payload
   clone/IO; final release frees C owners on that caller's thread. */
BrowsePreviewSet *browse_preview_set_retain(const BrowsePreviewSet *set);
void browse_preview_set_free(BrowsePreviewSet *set);
#endif
