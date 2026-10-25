#include "sheet_history_payload.h"
#include "history_event_frame.h"
#include <stdlib.h>
#include <string.h>
struct WorkbookSheetPayload {
    WorkbookSheetPayloadInfo info;
    WorkbookHistoryEventInfo event;
    union { SheetLifecycle metadata; SheetTopologyEvent topology; } payload;
};
typedef struct { uint64_t id; const unsigned char *name; size_t bytes; bool hidden; } Row;
static uint64_t number(const unsigned char *p,unsigned n) { uint64_t v=0; for (unsigned i=0;i<n;i++) v|=(uint64_t)p[i]<<(8*i); return v; }
static bool same(Row a,Row b) { return a.id==b.id && a.hidden==b.hidden && a.bytes==b.bytes && !memcmp(a.name,b.name,a.bytes); }
/* Called only after the existing strict DXSL preflight proves every bound. */
static bool metadata_effect(const void *data,uint64_t sheet,WorkbookSheetOperation operation,size_t count) {
    Row sides[2][DX_MAX_SHEETS]; const unsigned char *p=(const unsigned char *)data+DX_SHEET_LIFECYCLE_HEADER_BYTES;
    for (unsigned side=0;side<2;side++) for (size_t i=0;i<count;i++) {
        sides[side][i]=(Row){number(p,8),p+16,(size_t)number(p+12,4),number(p+8,4)!=0}; p+=16+sides[side][i].bytes;
    }
    size_t before=count,after=count;
    for (size_t i=0;i<count;i++) { if (sides[0][i].id==sheet) before=i; if (sides[1][i].id==sheet) after=i; }
    if (before==count || after==count) return false;
    if (operation==SHEET_REORDER) {
        if (before==after || !same(sides[0][before],sides[1][after])) return false;
        for (size_t i=0,j=0;i<count;i++) if (i!=before) { if (j==after) j++; if (!same(sides[0][i],sides[1][j++])) return false; }
        return true;
    }
    if (before!=after) return false;
    for (size_t i=0;i<count;i++) if (i!=before && !same(sides[0][i],sides[1][i])) return false;
    Row a=sides[0][before],b=sides[1][after];
    if (operation==SHEET_RENAME) return a.hidden==b.hidden && (a.bytes!=b.bytes || memcmp(a.name,b.name,a.bytes));
    if (operation!=SHEET_HIDE && operation!=SHEET_SHOW) return false;
    return a.bytes==b.bytes && !memcmp(a.name,b.name,a.bytes) && a.hidden==(operation==SHEET_SHOW) && b.hidden==(operation==SHEET_HIDE);
}
WorkbookSheetPayloadStatus workbook_sheet_metadata_preflight(const SheetLifecyclePacket *packet,uint64_t sheet,WorkbookSheetOperation operation,SheetLifecycleInfo *out) {
    if (!packet || !out || !sheet || operation<SHEET_RENAME || operation>SHEET_SHOW) return SHEET_PAYLOAD_INVALID;
    SheetLifecycleInfo info; SheetLifecycleStatus status=sheet_lifecycle_preflight(packet->data,packet->bytes,&info);
    if (status!=SHEET_LIFECYCLE_OK) return status==SHEET_LIFECYCLE_UNSUPPORTED ? SHEET_PAYLOAD_UNSUPPORTED : SHEET_PAYLOAD_INVALID;
    if (!metadata_effect(packet->data,sheet,operation,info.count)) return SHEET_PAYLOAD_INVALID;
    *out=info; return SHEET_PAYLOAD_OK;
}
static bool frame(WorkbookSheetPayloadView v) {
    const WorkbookHistoryEventInfo *e=v.event;
    return e && v.data && v.sheet && v.bytes==e->bytes && (unsigned)v.operation<=SHEET_SHOW &&
        workbook_history_event_frame_valid(e) &&
        !e->dependency_encoding && !e->dependency_bytes && !e->inactive_scopes;
}
WorkbookSheetPayloadStatus workbook_sheet_payload_preflight(WorkbookSheetPayloadView v,WorkbookSheetPayloadInfo *out) {
    if (!out || !v.event) return SHEET_PAYLOAD_INVALID;
    if (v.event->family!=DX_HISTORY_SHEET_LIFECYCLE_FAMILY ||
        (v.event->encoding!=DX_HISTORY_SHEET_LIFECYCLE_PROFILE && v.event->encoding!=DX_HISTORY_SHEET_TOPOLOGY_PROFILE)) return SHEET_PAYLOAD_UNSUPPORTED;
    if (!frame(v)) return SHEET_PAYLOAD_INVALID;
    WorkbookSheetPayloadInfo info={.document={v.event->owner.document_high,v.event->owner.document_low},.sheet=v.sheet,.operation=v.operation,.bytes=v.bytes}; size_t owned;
    if (v.event->encoding==DX_HISTORY_SHEET_LIFECYCLE_PROFILE) {
        SheetLifecycleInfo metadata; SheetLifecyclePacket packet={(void *)v.data,v.bytes};
        WorkbookSheetPayloadStatus status=workbook_sheet_metadata_preflight(&packet,v.sheet,v.operation,&metadata); if (status) return status;
        const unsigned char *p=v.data;
        if (number(p+8,8)!=info.document.high || number(p+16,8)!=info.document.low) return SHEET_PAYLOAD_INVALID;
        info.kind=SHEET_PAYLOAD_METADATA; owned=metadata.owned;
    } else {
        SheetTopologyInfo topology; SheetTopologyStatus status=sheet_topology_preflight(v.data,v.bytes,&topology);
        if (status!=SHEET_TOPOLOGY_OK) return status==SHEET_TOPOLOGY_UNSUPPORTED ? SHEET_PAYLOAD_UNSUPPORTED : status==SHEET_TOPOLOGY_LIMIT ? SHEET_PAYLOAD_LIMIT : SHEET_PAYLOAD_INVALID;
        if (v.operation>SHEET_DELETE || topology.sheet!=v.sheet || topology.document.high!=info.document.high || topology.document.low!=info.document.low ||
            topology.operation!=(v.operation==SHEET_DELETE ? SHEET_TOPOLOGY_REMOVE : SHEET_TOPOLOGY_ADD)) return SHEET_PAYLOAD_INVALID;
        info.kind=SHEET_PAYLOAD_TOPOLOGY; owned=topology.owned; info.validation_scratch=topology.validation_scratch;
    }
    if (owned>SIZE_MAX-sizeof(WorkbookSheetPayload) || owned+sizeof(WorkbookSheetPayload)>SIZE_MAX-info.validation_scratch) return SHEET_PAYLOAD_LIMIT;
    info.owned=owned+sizeof(WorkbookSheetPayload); info.staging=info.owned+info.validation_scratch; *out=info; return SHEET_PAYLOAD_OK;
}
WorkbookSheetPayloadStatus workbook_sheet_payload_validate(WorkbookSheetPayloadView v,size_t reserved,WorkbookSheetPayloadInfo *out) {
    if (!out) return SHEET_PAYLOAD_INVALID; WorkbookSheetPayloadInfo info;
    WorkbookSheetPayloadStatus status=workbook_sheet_payload_preflight(v,&info); if (status) return status;
    if (reserved<info.validation_scratch) return SHEET_PAYLOAD_LIMIT;
    if (info.kind==SHEET_PAYLOAD_TOPOLOGY) { SheetTopologyInfo topology; if (sheet_topology_validate(v.data,v.bytes,reserved,&topology)!=SHEET_TOPOLOGY_OK) return SHEET_PAYLOAD_INVALID; }
    *out=info; return SHEET_PAYLOAD_OK;
}
WorkbookSheetPayloadStatus workbook_sheet_payload_decode(WorkbookSheetPayloadView v,size_t reserved,WorkbookSheetPayload **out) {
    if (!out) return SHEET_PAYLOAD_INVALID; WorkbookSheetPayloadInfo info;
    WorkbookSheetPayloadStatus status=workbook_sheet_payload_preflight(v,&info); if (status) return status;
    if (reserved<info.staging) return SHEET_PAYLOAD_LIMIT;
    WorkbookSheetPayload *next=dx_alloc(sizeof(*next)); next->info=info; next->event=*v.event; bool ok;
    if (info.kind==SHEET_PAYLOAD_METADATA) ok=sheet_lifecycle_decode(v.data,v.bytes,info.owned-sizeof(*next),&next->payload.metadata)==SHEET_LIFECYCLE_OK;
    else ok=sheet_topology_decode(v.data,v.bytes,info.staging-sizeof(*next),&next->payload.topology)==SHEET_TOPOLOGY_OK;
    if (!ok) { workbook_sheet_payload_free(&next); return SHEET_PAYLOAD_INVALID; }
    workbook_sheet_payload_free(out); *out=next; return SHEET_PAYLOAD_OK;
}
void workbook_sheet_payload_free(WorkbookSheetPayload **owned) {
    if (!owned || !*owned) return; WorkbookSheetPayload *p=*owned;
    if (p->info.kind==SHEET_PAYLOAD_METADATA) sheet_lifecycle_free(&p->payload.metadata);
    else sheet_topology_free(&p->payload.topology);
    free(p); *owned=NULL;
}
const WorkbookSheetPayloadInfo *workbook_sheet_payload_info(const WorkbookSheetPayload *p) { return p ? &p->info : NULL; }
const WorkbookHistoryEventInfo *workbook_sheet_payload_event(const WorkbookSheetPayload *p) { return p ? &p->event : NULL; }
const SheetLifecycle *workbook_sheet_payload_metadata(const WorkbookSheetPayload *p) { return p && p->info.kind==SHEET_PAYLOAD_METADATA ? &p->payload.metadata : NULL; }
const SheetTopologyEvent *workbook_sheet_payload_topology(const WorkbookSheetPayload *p) { return p && p->info.kind==SHEET_PAYLOAD_TOPOLOGY ? &p->payload.topology : NULL; }
