#ifndef DX_BROWSE_PREVIEW_ADMISSION_H
#define DX_BROWSE_PREVIEW_ADMISSION_H
#include "browse_preview.h"
#include "source_dependency_admission.h"
typedef struct {
    WorkbookId document;
    uint64_t sheet,owner,object_high_water;
    const Analysis *definition;
    const SourceVersions *sources;
    size_t other_owned;
    const SheetObject *table; /* Actual Table source, NULL for primary owner0. */
} BrowsePreviewLive;
/* Full temporary DXAN/DXSV ownership plus grammar/RAW stack, existing preview
   and separately charged live inputs. No source IO or current Page is needed.
   Only metadata compatibility is reported: never current data, an exact core
   root/attachment, file verification, source execution or format permission.
   A changed source_version (including0->bound) conflicts even at the same path.
   Captured pending completion may refine, retaining captured_pending/inactivity. */
BrowsePreviewStatus browse_preview_admission_measure(const BrowsePreview *preview,BrowsePreviewLive live,size_t *coexistence);
SourceDependencyStatus browse_preview_admit(const BrowsePreview *preview,BrowsePreviewLive live,size_t reserved,SourceDependencyAdmission *out);
#endif
