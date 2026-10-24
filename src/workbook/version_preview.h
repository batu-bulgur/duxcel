#ifndef DUXCEL_VERSION_PREVIEW_H
#define DUXCEL_VERSION_PREVIEW_H
#include "version_restore.h"
typedef struct WorkbookVersionPreview WorkbookVersionPreview;
typedef enum { VERSION_PREVIEW_VERIFY,VERSION_PREVIEW_LOAD,VERSION_PREVIEW_RECHECK,VERSION_PREVIEW_READY } WorkbookVersionPreviewPhase;
typedef struct {
    /* Worker only; true cancels cooperatively. No model/store/file mutation,
       persistence reentry or borrowed pointer retention from the callback. */
    bool (*cancelled)(void *context,WorkbookVersionPreviewPhase phase,uint64_t bytes);
    void *context;
} WorkbookVersionPreviewControl;
typedef enum { VERSION_PREVIEW_PREPARED,VERSION_PREVIEW_FAILED,VERSION_PREVIEW_CANCELLED } WorkbookVersionPreviewResult;
/* Isolated supported-core projection from an already admitted immutable store.
   Requires a nonzero historical reader owner matching the durable document.
   The opaque root key selects owned bytes, never a user label/path or SQL query.
   Pre/post verification and strict core decoding precede READY. External sources
   are not executed; no working book, draft/history issuer, Saved pointer or named
   artifact is supplied/mutated. Result has no Save path and is not dirty.
   Load is cooperative before/after its strict decoder, not interruptible inside
   each decoder query. out must be NULL and is unchanged on failure/cancel. */
WorkbookVersionPreviewResult workbook_version_preview_prepare(const WorkbookVersionCoreSet *store,
    WorkbookVersionKey root,WorkbookVersionOwner reader,WorkbookVersionPreviewControl control,
    WorkbookVersionPreview **out,char **error);
const Workbook *workbook_version_preview_book(const WorkbookVersionPreview *preview);
const WorkbookVersionCoreSet *workbook_version_preview_versions(const WorkbookVersionPreview *preview);
const WorkbookVersionOwner *workbook_version_preview_owner(const WorkbookVersionPreview *preview);
const WorkbookVersionKey *workbook_version_preview_root(const WorkbookVersionPreview *preview);
/* The model and retained root-set lifetimes are independent. Taking the model
   keeps the preview's protected store/reader identity until explicit disposal. */
Workbook *workbook_version_preview_take_book(WorkbookVersionPreview *preview);
/* Last store release can retire private bytes; App adapters queue off frames. */
void workbook_version_preview_free(WorkbookVersionPreview *preview);
#endif
