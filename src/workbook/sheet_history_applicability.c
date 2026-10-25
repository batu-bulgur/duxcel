#include "sheet_history_applicability.h"
#include <stdlib.h>
struct WorkbookSheetApplicability { const WorkbookSheetPayload *payload; SheetPresenceAsset *presence; size_t owned; };
WorkbookSheetApplicabilityStatus workbook_sheet_applicability_measure(const WorkbookSheetPayload *payload,const WorkbookSheetApplicability *prior,WorkbookSheetApplicabilityMeasure *out) {
    if (!payload || !out) return SHEET_APPLICABILITY_INVALID;
    WorkbookSheetApplicabilityMeasure m={.owned=sizeof(WorkbookSheetApplicability),.prior_owned=prior ? prior->owned : 0};
    const SheetTopologyEvent *event=workbook_sheet_payload_topology(payload);
    if (event) {
        SheetPresenceWireInfo info;
        if (sheet_presence_preflight(event->presence.data,event->presence.bytes,&info)!=SHEET_PRESENCE_OK) return SHEET_APPLICABILITY_INVALID;
        if (info.owned>SIZE_MAX-m.owned) return SHEET_APPLICABILITY_LIMIT;
        m.owned+=info.owned; m.validation_scratch=info.validation_scratch;
    } else if (!workbook_sheet_payload_metadata(payload)) return SHEET_APPLICABILITY_INVALID;
    if (m.prior_owned>SIZE_MAX-m.owned || m.validation_scratch>SIZE_MAX-m.owned-m.prior_owned) return SHEET_APPLICABILITY_LIMIT;
    m.staging=m.owned+m.prior_owned+m.validation_scratch; *out=m; return SHEET_APPLICABILITY_OK;
}
WorkbookSheetApplicabilityStatus workbook_sheet_applicability_prepare(const WorkbookSheetPayload *payload,size_t reserved,WorkbookSheetApplicability **out) {
    if (!out) return SHEET_APPLICABILITY_INVALID; WorkbookSheetApplicabilityMeasure m;
    WorkbookSheetApplicabilityStatus status=workbook_sheet_applicability_measure(payload,*out,&m); if (status) return status;
    if (reserved<m.staging) return SHEET_APPLICABILITY_LIMIT;
    WorkbookSheetApplicability *next=dx_alloc(sizeof(*next)); next->payload=payload; next->owned=m.owned;
    const SheetTopologyEvent *event=workbook_sheet_payload_topology(payload);
    if (event && sheet_presence_decode(event->presence.data,event->presence.bytes,m.owned-sizeof(*next)+m.validation_scratch,&next->presence,NULL)!=SHEET_PRESENCE_OK) {
        workbook_sheet_applicability_free(&next); return SHEET_APPLICABILITY_INVALID;
    }
    workbook_sheet_applicability_free(out); *out=next; return SHEET_APPLICABILITY_OK;
}
WorkbookSheetApplicabilityStatus workbook_sheet_applicability_check(const WorkbookSheetApplicability *plan,SheetLifecycleState current,const SheetPresenceView *actual,bool redo,WorkbookSheetApplicabilityResult *out) {
    if (!plan || !out) return SHEET_APPLICABILITY_INVALID; WorkbookSheetApplicabilityResult result={0};
    const WorkbookSheetPayloadInfo *info=workbook_sheet_payload_info(plan->payload);
    if (current.document.high!=info->document.high || current.document.low!=info->document.low) return SHEET_APPLICABILITY_MISMATCH;
    const SheetLifecycle *metadata=workbook_sheet_payload_metadata(plan->payload);
    if (metadata) {
        SheetLifecycleStatus status=sheet_lifecycle_admit(*metadata,current,redo,&result.target);
        if (status!=SHEET_LIFECYCLE_OK) return status==SHEET_LIFECYCLE_STALE ? SHEET_APPLICABILITY_MISMATCH : SHEET_APPLICABILITY_INVALID;
    } else {
        const SheetTopologyEvent *event=workbook_sheet_payload_topology(plan->payload);
        if (!actual || !actual->sheet || !actual->primary || actual->document.high!=current.document.high || actual->document.low!=current.document.low || actual->sheet_high_water!=current.high_water) return SHEET_APPLICABILITY_INVALID;
        SheetTopologyStatus status=sheet_topology_admit(sheet_topology_view(event),current,redo,&result.target);
        if (status!=SHEET_TOPOLOGY_OK) return status==SHEET_TOPOLOGY_STALE ? SHEET_APPLICABILITY_MISMATCH : SHEET_APPLICABILITY_INVALID;
        SheetPresenceView normalized=*actual; normalized.sheet_high_water=sheet_presence_view(plan->presence).sheet_high_water;
        if (sheet_presence_matches_refined(plan->presence,normalized,&result.presence)!=SHEET_PRESENCE_OK) return SHEET_APPLICABILITY_INVALID;
        if (!result.presence.matches) return SHEET_APPLICABILITY_MISMATCH;
    }
    *out=result; return SHEET_APPLICABILITY_OK;
}
void workbook_sheet_applicability_free(WorkbookSheetApplicability **owned) {
    if (!owned || !*owned) return; WorkbookSheetApplicability *plan=*owned; sheet_presence_free(&plan->presence); free(plan); *owned=NULL;
}
