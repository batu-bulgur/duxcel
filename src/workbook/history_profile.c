#include "history_profile.h"
#include <stdlib.h>
#include <string.h>
typedef struct { WorkbookHistoryEventInfo frame; WorkbookHistoryProfileReport report; } Record;
typedef struct { WorkbookHistoryPath path; size_t *records; } Path;
struct WorkbookHistoryProfile {
    WorkbookHistoryProfileView view;
    WorkbookVersionIndex versions;
    WorkbookHistoryAnchors anchors;
    Path current,saved;
    Record *entries;
    WorkbookHistoryProfileRecord *records;
    WorkbookHistoryProtection protection;
    size_t owned;
};
static bool add(size_t *n,size_t extra) { if (*n>SIZE_MAX-extra) return false; *n+=extra; return true; }
static bool array(size_t *n,size_t count,size_t item) { return (!count || item<=SIZE_MAX/count) && add(n,count*item); }
static bool same_owner(WorkbookVersionOwner a,WorkbookVersionOwner b) { return a.document_high==b.document_high && a.document_low==b.document_low && a.session==b.session && a.mode==b.mode; }
static WorkbookHistoryProfileStatus path_measure(WorkbookHistoryProfilePath input,size_t *bytes) {
    if (!input.path) return input.records || input.count ? HISTORY_PROFILE_INVALID : HISTORY_PROFILE_OK;
    const WorkbookHistoryPath *p=input.path;
    if (p->event_count>DX_HISTORY_PROFILE_RECORDS || p->segment_count>DX_HISTORY_PROFILE_RECORDS+1u) return HISTORY_PROFILE_LIMIT;
    if (input.count!=p->event_count || (input.count && !input.records)) return HISTORY_PROFILE_COVERAGE;
    if (!array(bytes,p->event_count,sizeof(WorkbookHistoryEventPosition)+sizeof(size_t)) || !array(bytes,p->segment_count,sizeof(WorkbookHistorySegment))) return HISTORY_PROFILE_LIMIT;
    return HISTORY_PROFILE_OK;
}
static WorkbookHistoryProfileStatus record_measure(const WorkbookHistoryProfileRecord *r,size_t *peak) {
    if (!r->event) return HISTORY_PROFILE_INVALID;
    unsigned family=r->event->family;
    if (family==DX_HISTORY_CELL_FAMILY) {
        WorkbookCellPayloadInfo info;
        WorkbookCellPayloadStatus s=workbook_cell_payload_check((WorkbookCellPayloadView){r->event,r->data,r->dependencies,r->bytes,r->dependency_bytes,r->sheet,r->intent.effects},r->bytes,&info);
        return s==CELL_PAYLOAD_OK ? HISTORY_PROFILE_OK : s==CELL_PAYLOAD_LIMIT ? HISTORY_PROFILE_LIMIT : s==CELL_PAYLOAD_UNSUPPORTED ? HISTORY_PROFILE_UNSUPPORTED : HISTORY_PROFILE_INVALID;
    }
    if (family==DX_HISTORY_FORMAT_FAMILY || family==DX_HISTORY_DEFINITION_FAMILY) {
        WorkbookSourcePayloadInfo info; WorkbookSourcePayloadStatus s=workbook_source_payload_preflight((WorkbookSourcePayloadView){r->event,r->data,r->dependencies,r->bytes,r->dependency_bytes,r->sheet,r->intent.effects},&info);
        if (s!=SOURCE_PAYLOAD_OK) return s==SOURCE_PAYLOAD_LIMIT ? HISTORY_PROFILE_LIMIT : s==SOURCE_PAYLOAD_UNSUPPORTED ? HISTORY_PROFILE_UNSUPPORTED : HISTORY_PROFILE_INVALID;
        *peak=info.staging; return HISTORY_PROFILE_OK;
    }
    if (family==DX_HISTORY_RANGE_FAMILY) {
        WorkbookRangePayloadInfo info; WorkbookRangePayloadStatus s=workbook_range_payload_preflight((WorkbookRangePayloadView){r->event,r->data,r->dependencies,r->bytes,r->dependency_bytes,r->sheet,r->intent.effects},&info);
        if (s!=RANGE_PAYLOAD_OK) return s==RANGE_PAYLOAD_LIMIT ? HISTORY_PROFILE_LIMIT : s==RANGE_PAYLOAD_UNSUPPORTED ? HISTORY_PROFILE_UNSUPPORTED : HISTORY_PROFILE_INVALID;
        *peak=info.staging; return HISTORY_PROFILE_OK;
    }
    if (family==DX_HISTORY_SHEET_LIFECYCLE_FAMILY) {
        WorkbookSheetPayloadInfo info; WorkbookSheetPayloadStatus s=workbook_sheet_payload_preflight((WorkbookSheetPayloadView){r->event,r->data,r->bytes,r->sheet,r->intent.operation},&info);
        if (r->dependencies || r->dependency_bytes) return HISTORY_PROFILE_INVALID;
        if (s!=SHEET_PAYLOAD_OK) return s==SHEET_PAYLOAD_LIMIT ? HISTORY_PROFILE_LIMIT : s==SHEET_PAYLOAD_UNSUPPORTED ? HISTORY_PROFILE_UNSUPPORTED : HISTORY_PROFILE_INVALID;
        *peak=info.validation_scratch; return HISTORY_PROFILE_OK;
    }
    return HISTORY_PROFILE_UNSUPPORTED;
}
static WorkbookHistoryProfileStatus coverage(WorkbookHistoryProfileView v) {
    bool used[DX_HISTORY_PROFILE_RECORDS]={0}; WorkbookHistoryProfilePath paths[2]={v.current,v.saved};
    for (unsigned side=0;side<2;side++) if (paths[side].path) for (size_t i=0;i<paths[side].count;i++) {
        size_t at=paths[side].records[i]; if (at>=v.count) return HISTORY_PROFILE_COVERAGE;
        const WorkbookHistoryProfileRecord *r=v.records+at;
        WorkbookHistoryEventSlotStatus s=workbook_history_event_slot_check((WorkbookHistoryEventSlot){paths[side].path,i,r->owner,r->edit_session,r->event});
        if (s!=HISTORY_EVENT_SLOT_OK) return s==HISTORY_EVENT_SLOT_PATH ? HISTORY_PROFILE_METADATA : HISTORY_PROFILE_ASSOCIATION;
        used[at]=true;
    }
    for (size_t i=0;i<v.count;i++) if (!used[i]) return HISTORY_PROFILE_COVERAGE;
    return HISTORY_PROFILE_OK;
}
WorkbookHistoryProfileStatus workbook_history_profile_measure(WorkbookHistoryProfileView v,WorkbookHistoryProfileCaps caps,const WorkbookHistoryProfile *prior,WorkbookHistoryProfileMeasure *out) {
    if (!out || !v.versions || !(v.document.high|v.document.low)) return HISTORY_PROFILE_INVALID;
    if (v.container_version!=16 || v.core_version!=15 || (unsigned)v.kind>HISTORY_PROFILE_RETAINED) return HISTORY_PROFILE_UNSUPPORTED;
    if (v.count>caps.records || v.count>DX_HISTORY_PROFILE_RECORDS) return HISTORY_PROFILE_LIMIT;
    if (v.count && !v.records) return HISTORY_PROFILE_INVALID;
    const WorkbookVersionIndex *index=v.versions;
    if (index->root_count>DX_VERSION_ROOTS || index->tag_count>DX_VERSION_TAGS) return HISTORY_PROFILE_LIMIT;
    if (workbook_version_index_check(index)!=VERSION_OK || index->document_high!=v.document.high || index->document_low!=v.document.low) return HISTORY_PROFILE_METADATA;
    WorkbookHistoryProfileMeasure m={.records=v.count,.metadata_bytes=sizeof(WorkbookHistoryProfile),.prior_owned=prior ? prior->owned : 0};
    if (!array(&m.metadata_bytes,index->root_count,sizeof(WorkbookVersionRoot)) || !array(&m.metadata_bytes,index->tag_count,sizeof(WorkbookVersionTag)) ||
        !array(&m.metadata_bytes,v.count,sizeof(Record)+sizeof(WorkbookHistoryProfileRecord))) return HISTORY_PROFILE_LIMIT;
    for (size_t i=0;i<index->tag_count;i++) if (!add(&m.metadata_bytes,index->tags[i].label.bytes+1)) return HISTORY_PROFILE_LIMIT;
    for (size_t i=0;i<index->root_count;i++) {
        if (index->roots[i].encoding!=15) return HISTORY_PROFILE_UNSUPPORTED;
        if (index->roots[i].bytes>UINT64_MAX-m.root_bytes) return HISTORY_PROFILE_LIMIT;
        m.root_bytes+=index->roots[i].bytes;
    }
    if (v.kind==HISTORY_PROFILE_CHECKPOINT_ONLY) {
        if (v.anchors || v.current.path || v.current.records || v.current.count || v.saved.path || v.saved.records || v.saved.count || v.count || v.records) return HISTORY_PROFILE_COVERAGE;
    } else {
        if (!v.anchors || !v.current.path || (!!v.saved.path!=index->saved.present)) return HISTORY_PROFILE_METADATA;
        if (v.anchors->count>DX_HISTORY_PUBLIC_ANCHORS || v.anchors->tag_count>DX_VERSION_TAGS) return HISTORY_PROFILE_LIMIT;
        if (!array(&m.metadata_bytes,v.anchors->count,sizeof(WorkbookHistoryAnchor)) || !array(&m.metadata_bytes,v.anchors->tag_count,sizeof(WorkbookHistoryTagAnchor))) return HISTORY_PROFILE_LIMIT;
        WorkbookHistoryProfileStatus s=path_measure(v.current,&m.metadata_bytes); if (s) return s;
        s=path_measure(v.saved,&m.metadata_bytes); if (s) return s;
        WorkbookHistoryProtection protection;
        if (workbook_history_protect(index,v.anchors,v.saved.path,v.current.path,&protection)!=HISTORY_PATH_OK) return HISTORY_PROFILE_METADATA;
    }
    if (m.metadata_bytes>caps.metadata_bytes) return HISTORY_PROFILE_LIMIT;
    for (size_t i=0;i<v.count;i++) {
        const WorkbookHistoryProfileRecord *r=v.records+i;
        if (!r->event || (i && r->event->transition<=v.records[i-1].event->transition)) return HISTORY_PROFILE_COVERAGE;
        if (r->owner.document_high!=v.document.high || r->owner.document_low!=v.document.low || !same_owner(r->owner,r->event->owner) || r->edit_session!=r->event->edit_session) return HISTORY_PROFILE_ASSOCIATION;
        if (!add(&m.payload_bytes,r->bytes) || !add(&m.payload_bytes,r->dependency_bytes) || m.payload_bytes>caps.payload_bytes) return HISTORY_PROFILE_LIMIT;
        size_t peak=0; WorkbookHistoryProfileStatus s=record_measure(r,&peak); if (s) return s;
        if (peak>m.validation_peak) m.validation_peak=peak;
    }
    if (v.kind==HISTORY_PROFILE_RETAINED) { WorkbookHistoryProfileStatus s=coverage(v); if (s) return s; }
    m.owned=m.metadata_bytes;
    if (!add(&m.owned,m.payload_bytes) || m.owned>DX_HISTORY_PROFILE_OWNED_BYTES) return HISTORY_PROFILE_LIMIT;
    m.staging=m.owned;
    if (!add(&m.staging,m.prior_owned) || !add(&m.staging,m.validation_peak) || m.staging>caps.staging_bytes || m.staging>DX_HISTORY_PROFILE_STAGING_BYTES) return HISTORY_PROFILE_LIMIT;
    *out=m; return HISTORY_PROFILE_OK;
}
static void *copy_array(const void *data,size_t count,size_t item) {
    if (!count) return NULL; void *owned=dx_alloc(count*item); memcpy(owned,data,count*item); return owned;
}
static void copy_path(WorkbookHistoryProfilePath input,Path *owned,WorkbookHistoryProfilePath *out) {
    if (!input.path) return; owned->path=*input.path;
    owned->path.segments=copy_array(input.path->segments,input.path->segment_count,sizeof(WorkbookHistorySegment));
    owned->path.events=copy_array(input.path->events,input.path->event_count,sizeof(WorkbookHistoryEventPosition));
    owned->records=copy_array(input.records,input.count,sizeof(size_t));
    *out=(WorkbookHistoryProfilePath){&owned->path,owned->records,input.count};
}
static WorkbookHistoryProfileStatus validate_record(Record *entry,WorkbookHistoryProfileRecord r,const SourceDependencyLive *provided,size_t peak) {
    WorkbookHistoryProfileReport report={.family=r.event->family,.sheet=r.sheet};
    SourceDependencyLive live=provided ? *provided : (SourceDependencyLive){.sheet=r.sheet,.object_high_water=UINT64_MAX};
    if (provided && provided->sheet!=r.sheet) return HISTORY_PROFILE_ASSOCIATION;
    if (r.event->family==DX_HISTORY_FORMAT_FAMILY || r.event->family==DX_HISTORY_DEFINITION_FAMILY) {
        report.source_compared=true;
        report.compatibility=workbook_source_payload_admit((WorkbookSourcePayloadView){r.event,r.data,r.dependencies,r.bytes,r.dependency_bytes,r.sheet,r.intent.effects},live,peak,&report.observations);
    } else if (r.event->family==DX_HISTORY_RANGE_FAMILY) {
        report.source_compared=true;
        report.compatibility=workbook_range_payload_admit((WorkbookRangePayloadView){r.event,r.data,r.dependencies,r.bytes,r.dependency_bytes,r.sheet,r.intent.effects},live,peak,&report.observations);
    } else if (r.event->family==DX_HISTORY_SHEET_LIFECYCLE_FAMILY) {
        WorkbookSheetPayloadInfo info;
        WorkbookSheetPayloadStatus status=workbook_sheet_payload_validate((WorkbookSheetPayloadView){r.event,r.data,r.bytes,r.sheet,r.intent.operation},peak,&info);
        if (status!=SHEET_PAYLOAD_OK) return status==SHEET_PAYLOAD_LIMIT ? HISTORY_PROFILE_LIMIT : status==SHEET_PAYLOAD_UNSUPPORTED ? HISTORY_PROFILE_UNSUPPORTED : HISTORY_PROFILE_INVALID;
    }
    /* Family1's strict complete decoder owns no graph and ran once during the
       allocation-free preflight. Immutable bytes were then copied exactly. */
    if (report.source_compared) {
        if (report.compatibility==SOURCE_DEPENDENCY_INVALID) return HISTORY_PROFILE_INVALID;
        if (report.compatibility==SOURCE_DEPENDENCY_LIMIT) return HISTORY_PROFILE_LIMIT;
        if (report.compatibility==SOURCE_DEPENDENCY_UNSUPPORTED) return HISTORY_PROFILE_UNSUPPORTED;
    }
    entry->report=report; return HISTORY_PROFILE_OK;
}
WorkbookHistoryProfileStatus workbook_history_profile_prepare(WorkbookHistoryProfileView v,WorkbookHistoryProfileCaps caps,WorkbookHistoryProfile **out) {
    if (!out) return HISTORY_PROFILE_INVALID; WorkbookHistoryProfileMeasure m;
    WorkbookHistoryProfileStatus status=workbook_history_profile_measure(v,caps,*out,&m); if (status) return status;
    WorkbookHistoryProfile *next=dx_alloc(sizeof(*next)); next->view=v; next->owned=m.owned; next->versions=*v.versions; next->view.versions=&next->versions;
    next->versions.roots=copy_array(v.versions->roots,v.versions->root_count,sizeof(WorkbookVersionRoot));
    WorkbookVersionTag *tags=copy_array(v.versions->tags,v.versions->tag_count,sizeof(WorkbookVersionTag)); next->versions.tags=tags;
    for (size_t i=0;i<v.versions->tag_count;i++) {
        char *label=dx_alloc(tags[i].label.bytes+1); memcpy(label,tags[i].label.data,tags[i].label.bytes); tags[i].label.data=label;
    }
    if (v.kind==HISTORY_PROFILE_RETAINED) {
        next->anchors=*v.anchors; next->view.anchors=&next->anchors;
        next->anchors.items=copy_array(v.anchors->items,v.anchors->count,sizeof(WorkbookHistoryAnchor));
        next->anchors.tags=copy_array(v.anchors->tags,v.anchors->tag_count,sizeof(WorkbookHistoryTagAnchor));
        copy_path(v.current,&next->current,&next->view.current); copy_path(v.saved,&next->saved,&next->view.saved);
        if (workbook_history_protect(&next->versions,&next->anchors,next->view.saved.path,next->view.current.path,&next->protection)!=HISTORY_PATH_OK) { workbook_history_profile_free(&next); return HISTORY_PROFILE_METADATA; }
    }
    if (v.count) { next->entries=dx_alloc(v.count*sizeof(Record)); next->records=dx_alloc(v.count*sizeof(WorkbookHistoryProfileRecord)); }
    next->view.records=next->records;
    for (size_t i=0;i<v.count;i++) {
        next->entries[i].frame=*v.records[i].event; WorkbookHistoryProfileRecord *r=next->records+i;
        *r=v.records[i]; r->event=&next->entries[i].frame; r->live=NULL;
        r->data=copy_array(v.records[i].data,r->bytes,1); r->dependencies=copy_array(v.records[i].dependencies,r->dependency_bytes,1);
    }
    for (size_t i=0;i<v.count;i++) {
        status=validate_record(next->entries+i,next->records[i],v.records[i].live,m.validation_peak);
        if (status) { workbook_history_profile_free(&next); return status; }
    }
    workbook_history_profile_free(out); *out=next; return HISTORY_PROFILE_OK;
}
static void free_path(Path *p) { free((void *)p->path.segments); free((void *)p->path.events); free(p->records); }
void workbook_history_profile_free(WorkbookHistoryProfile **owned) {
    if (!owned || !*owned) return; WorkbookHistoryProfile *p=*owned;
    for (size_t i=0;i<p->versions.tag_count;i++) free((void *)p->versions.tags[i].label.data);
    free((void *)p->versions.tags); free((void *)p->versions.roots);
    free((void *)p->anchors.items); free((void *)p->anchors.tags); free_path(&p->current); free_path(&p->saved);
    if (p->records) for (size_t i=0;i<p->view.count;i++) { free((void *)p->records[i].data); free((void *)p->records[i].dependencies); }
    free(p->entries); free(p->records); free(p); *owned=NULL;
}
const WorkbookHistoryProfileView *workbook_history_profile_view(const WorkbookHistoryProfile *p) { return p ? &p->view : NULL; }
const WorkbookHistoryProtection *workbook_history_profile_protection(const WorkbookHistoryProfile *p) { return p && p->view.kind==HISTORY_PROFILE_RETAINED ? &p->protection : NULL; }
const WorkbookHistoryProfileReport *workbook_history_profile_report(const WorkbookHistoryProfile *p,size_t i) { return p && i<p->view.count ? &p->entries[i].report : NULL; }
size_t workbook_history_profile_owned(const WorkbookHistoryProfile *p) { return p ? p->owned : 0; }
