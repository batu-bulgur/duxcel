#include "range_history_payload.h"
static bool add(size_t *value,size_t bytes) { if (*value>SIZE_MAX-bytes) return false; *value+=bytes; return true; }
WorkbookRangePayloadStatus workbook_range_payload_preflight(WorkbookRangePayloadView v,WorkbookRangePayloadInfo *out) {
    if (!out || !v.event) return RANGE_PAYLOAD_INVALID;
    const WorkbookHistoryEventInfo *frame=v.event;
    if (frame->family!=DX_HISTORY_RANGE_FAMILY || frame->encoding!=DX_RANGE_EVENT_ENCODING) return RANGE_PAYLOAD_UNSUPPORTED;
    if (!workbook_history_event_frame_valid(frame) || !v.data || !v.sheet || v.bytes!=frame->bytes ||
        frame->dependency_encoding || frame->dependency_bytes || frame->inactive_scopes || v.dependencies || v.dependency_bytes) return RANGE_PAYLOAD_INVALID;
    RangeEventInfo decoded={0}; RangeEventStatus status=range_event_preflight(v.data,v.bytes,&decoded);
    if (status!=RANGE_EVENT_OK) return status==RANGE_EVENT_UNSUPPORTED ? RANGE_PAYLOAD_UNSUPPORTED : status==RANGE_EVENT_LIMIT ? RANGE_PAYLOAD_LIMIT : RANGE_PAYLOAD_INVALID;
    if (decoded.sheet!=v.sheet || decoded.effects!=v.effects) return RANGE_PAYLOAD_INVALID;
    WorkbookRangePayloadInfo info={.sheet=decoded.sheet,.object_high_water=decoded.object_high_water,.effects=decoded.effects,
        .unbound_sources=decoded.unbound_sources,.owned=decoded.owned,.validation_scratch=decoded.validation_scratch};
    if (!add(&info.owned,sizeof(RangeEvent))) return RANGE_PAYLOAD_LIMIT;
    info.staging=info.owned; if (!add(&info.staging,info.validation_scratch)) return RANGE_PAYLOAD_LIMIT;
    *out=info; return RANGE_PAYLOAD_OK;
}
static SourceDependencyStatus compatibility(SourceSubsetStatus status) {
    return status==SOURCE_SUBSET_EXACT ? SOURCE_DEPENDENCY_EXACT : status==SOURCE_SUBSET_REFINED ? SOURCE_DEPENDENCY_REFINED :
        status==SOURCE_SUBSET_UNAVAILABLE ? SOURCE_DEPENDENCY_UNAVAILABLE : status==SOURCE_SUBSET_CONFLICT ? SOURCE_DEPENDENCY_CONFLICT : SOURCE_DEPENDENCY_INVALID;
}
SourceDependencyStatus workbook_range_payload_admit(WorkbookRangePayloadView view,SourceDependencyLive live,size_t reserved,SourceDependencyAdmission *out) {
    if (!out || !live.sheet) return SOURCE_DEPENDENCY_INVALID;
    WorkbookRangePayloadInfo info={0}; WorkbookRangePayloadStatus status=workbook_range_payload_preflight(view,&info);
    if (status!=RANGE_PAYLOAD_OK) return status==RANGE_PAYLOAD_UNSUPPORTED ? SOURCE_DEPENDENCY_UNSUPPORTED : status==RANGE_PAYLOAD_LIMIT ? SOURCE_DEPENDENCY_LIMIT : SOURCE_DEPENDENCY_INVALID;
    if (reserved<info.staging) return SOURCE_DEPENDENCY_LIMIT;
    RangeEvent event={0}; RangeEventStatus decoded=range_event_decode(view.data,view.bytes,info.staging,&event);
    if (decoded!=RANGE_EVENT_OK) return decoded==RANGE_EVENT_UNSUPPORTED ? SOURCE_DEPENDENCY_UNSUPPORTED : decoded==RANGE_EVENT_LIMIT ? SOURCE_DEPENDENCY_LIMIT : SOURCE_DEPENDENCY_INVALID;
    SourceDependencyAdmission report={.unbound_sources=event.unbound_sources}; SourceDependencyStatus result;
    if (event.effects&(4u|8u)) {
        const SourceVersionsAsset *sources=event.effects&4u ? &event.definitions.sources : &event.sources;
        result=compatibility(source_versions_subset_admit(sources,live.sheet,live.versions,live.object_high_water,&report.sources));
    } else result=event.sheet==live.sheet ? SOURCE_DEPENDENCY_EXACT : SOURCE_DEPENDENCY_CONFLICT;
    range_event_free(&event);
    if (result!=SOURCE_DEPENDENCY_INVALID && result!=SOURCE_DEPENDENCY_LIMIT && result!=SOURCE_DEPENDENCY_UNSUPPORTED) *out=report;
    return result;
}
