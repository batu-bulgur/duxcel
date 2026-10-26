#ifndef DX_BROWSE_PREVIEW_WORKBOOK_H
#define DX_BROWSE_PREVIEW_WORKBOOK_H
#include "browse_preview_admission.h"
typedef enum { BROWSE_PREVIEW_NOT_ASSOCIATED,BROWSE_PREVIEW_CAPTURED_ONLY } BrowsePreviewClassification;
typedef struct {
    SourceDependencyAdmission dependencies;
    BrowsePreviewClassification classification;
} BrowsePreviewWorkbookAdmission;
/* actual is the already-admitted immutable Workbook/core model for this call,
   not an independently fabricated expected Analysis/ledger or a live App Page.
   Resolve stable sheet ID and primary0/real Table owner from that model once.
   Runtime sessions/query/request/source epochs and captured metadata clock are
   observations only after load; they are never durable freshness identities.
   EXACT/REFINED gives CAPTURED_ONLY, not current data/readiness, record-format
   permission, current statistics/query input or native root/attachment proof.
   Conflict/unavailability may omit this optional preview, never silently remap
   its owner/source. An archive reader passes its already-loaded unpublished
   Workbook; no second DB decoder or model mutation is introduced.
   other_owned charges actual model/other retained inputs. Failure on malformed
   input/limit/unsupported preserves reports and all owners. */
BrowsePreviewStatus browse_preview_workbook_measure(const BrowsePreview *preview,const Workbook *actual,size_t other_owned,size_t *coexistence);
SourceDependencyStatus browse_preview_workbook_admit(const BrowsePreview *preview,const Workbook *actual,size_t other_owned,size_t reserved,BrowsePreviewWorkbookAdmission *out);
#endif
