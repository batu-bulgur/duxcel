#include "source_history_payload.h"
static size_t maximum(size_t a,size_t b) { return a>b ? a : b; }
static bool add(size_t *n,size_t bytes) { if (bytes==SIZE_MAX || *n>SIZE_MAX-bytes) return false; *n+=bytes; return true; }
static WorkbookSourcePayloadStatus format_status(FormatDeltaStatus s) {
    return s==FORMAT_DELTA_OK ? SOURCE_PAYLOAD_OK : s==FORMAT_DELTA_UNSUPPORTED ? SOURCE_PAYLOAD_UNSUPPORTED : s==FORMAT_DELTA_LIMIT ? SOURCE_PAYLOAD_LIMIT : SOURCE_PAYLOAD_INVALID;
}
static WorkbookSourcePayloadStatus definition_status(DefinitionEventStatus s) {
    return s==DEFINITION_EVENT_OK ? SOURCE_PAYLOAD_OK : s==DEFINITION_EVENT_UNSUPPORTED ? SOURCE_PAYLOAD_UNSUPPORTED : s==DEFINITION_EVENT_LIMIT ? SOURCE_PAYLOAD_LIMIT : SOURCE_PAYLOAD_INVALID;
}
static WorkbookSourcePayloadStatus sources_status(SourceVersionsCodecStatus s) {
    return s==SOURCE_VERSIONS_CODEC_OK ? SOURCE_PAYLOAD_OK : s==SOURCE_VERSIONS_CODEC_UNSUPPORTED ? SOURCE_PAYLOAD_UNSUPPORTED : s==SOURCE_VERSIONS_CODEC_LIMIT ? SOURCE_PAYLOAD_LIMIT : SOURCE_PAYLOAD_INVALID;
}
typedef struct { WorkbookSourcePayloadInfo info; FormatDeltaInfo formats; SourceVersionsAssetInfo sources; DefinitionEventInfo definition; } Plan;
static WorkbookSourcePayloadStatus preflight(WorkbookSourcePayloadView v,Plan *out) {
    if (!out || !v.event) return SOURCE_PAYLOAD_INVALID;
    const WorkbookHistoryEventInfo *e=v.event;
    if ((e->family!=DX_HISTORY_FORMAT_FAMILY && e->family!=DX_HISTORY_DEFINITION_FAMILY) ||
        (e->family==DX_HISTORY_FORMAT_FAMILY ? e->encoding!=DX_FORMAT_DELTA_ENCODING : e->encoding!=DX_DEFINITION_EVENT_ENCODING)) return SOURCE_PAYLOAD_UNSUPPORTED;
    if (!workbook_history_event_frame_valid(e) || !v.sheet || !v.data || v.bytes!=e->bytes || v.dependency_bytes!=e->dependency_bytes) return SOURCE_PAYLOAD_INVALID;
    Plan p={.info={.sheet=v.sheet,.effects=v.effects}}; WorkbookSourcePayloadStatus status;
    if (e->family==DX_HISTORY_FORMAT_FAMILY) {
        if (v.effects!=8 || !e->dependency_encoding || !v.dependencies || !v.dependency_bytes) return SOURCE_PAYLOAD_INVALID;
        if (e->dependency_encoding!=DX_SOURCE_VERSIONS_ENCODING) return SOURCE_PAYLOAD_UNSUPPORTED;
        status=format_status(format_delta_preflight(v.data,v.bytes,&p.formats)); if (status) return status;
        status=sources_status(source_versions_asset_preflight(v.dependencies,v.dependency_bytes,UINT64_MAX,&p.sources)); if (status) return status;
        if (p.formats.sheet!=v.sheet || p.sources.sheet!=v.sheet) return SOURCE_PAYLOAD_INVALID;
        p.info.owned=sizeof(FormatDelta)+sizeof(SourceVersionsAsset);
        if (!add(&p.info.owned,p.formats.owned) || !add(&p.info.owned,p.sources.owned)) return SOURCE_PAYLOAD_LIMIT;
        size_t scopes=maximum(p.formats.before_scopes,p.formats.after_scopes);
        p.info.validation_scratch=p.formats.validation_scratch;
        if (scopes) {
            size_t proof=analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true);
            if (!add(&proof,scopes)) return SOURCE_PAYLOAD_LIMIT;
            p.info.validation_scratch=maximum(p.info.validation_scratch,proof);
        }
    } else {
        if (e->dependency_encoding || e->dependency_bytes || v.dependencies) return SOURCE_PAYLOAD_INVALID;
        status=definition_status(definition_event_preflight(v.data,v.bytes,&p.definition)); if (status) return status;
        if (p.definition.sheet!=v.sheet || p.definition.effects!=v.effects || p.definition.inactive_scopes!=e->inactive_scopes) return SOURCE_PAYLOAD_INVALID;
        p.info.owned=p.definition.owned; p.info.validation_scratch=p.definition.validation_scratch;
    }
    p.info.staging=p.info.owned;
    if (!add(&p.info.staging,p.info.validation_scratch)) return SOURCE_PAYLOAD_LIMIT;
    *out=p; return SOURCE_PAYLOAD_OK;
}
WorkbookSourcePayloadStatus workbook_source_payload_preflight(WorkbookSourcePayloadView v,WorkbookSourcePayloadInfo *out) {
    if (!out) return SOURCE_PAYLOAD_INVALID;
    Plan p; WorkbookSourcePayloadStatus status=preflight(v,&p); if (!status) *out=p.info; return status;
}
static SourceDependencyStatus dependency_status(SourceSubsetStatus s) {
    return s==SOURCE_SUBSET_EXACT ? SOURCE_DEPENDENCY_EXACT : s==SOURCE_SUBSET_REFINED ? SOURCE_DEPENDENCY_REFINED :
        s==SOURCE_SUBSET_UNAVAILABLE ? SOURCE_DEPENDENCY_UNAVAILABLE : s==SOURCE_SUBSET_CONFLICT ? SOURCE_DEPENDENCY_CONFLICT : SOURCE_DEPENDENCY_INVALID;
}
SourceDependencyStatus workbook_source_payload_admit(WorkbookSourcePayloadView v,SourceDependencyLive live,size_t reserved,SourceDependencyAdmission *out) {
    if (!out || !live.sheet) return SOURCE_DEPENDENCY_INVALID;
    Plan p; WorkbookSourcePayloadStatus status=preflight(v,&p);
    if (status) return status==SOURCE_PAYLOAD_UNSUPPORTED ? SOURCE_DEPENDENCY_UNSUPPORTED : status==SOURCE_PAYLOAD_LIMIT ? SOURCE_DEPENDENCY_LIMIT : SOURCE_DEPENDENCY_INVALID;
    if (reserved<p.info.staging) return SOURCE_DEPENDENCY_LIMIT;
    SourceDependencyAdmission next={0}; SourceDependencyStatus result=SOURCE_DEPENDENCY_INVALID;
    if (v.event->family==DX_HISTORY_FORMAT_FAMILY) {
        FormatDelta delta={0}; SourceVersionsAsset sources={0};
        if (format_delta_decode(v.data,v.bytes,p.formats.owned+p.formats.validation_scratch,&delta)==FORMAT_DELTA_OK &&
            source_versions_asset_decode(v.dependencies,v.dependency_bytes,UINT64_MAX,p.sources.owned,&sources)==SOURCE_VERSIONS_CODEC_OK) {
            /* Borrow the existing temporary source owner; never free this
               shallow view as another independently stored dependency owner. */
            FormatDependencies captured={sources,v.event->inactive_scopes};
            result=source_format_dependencies_admit(format_delta_snapshot_view(&delta.before),format_delta_snapshot_view(&delta.after),
                &captured,live,p.info.validation_scratch,&next);
        }
        format_delta_free(&delta); source_versions_asset_free(&sources);
    } else {
        DefinitionEvent event={0};
        if (definition_event_decode(v.data,v.bytes,p.info.staging,&event)==DEFINITION_EVENT_OK) {
            /* DXDE decode already proves its EXACT definition+format union,
               including both observation counts. Re-selecting only one family
               would incorrectly reject rows referenced solely by its partner. */
            next.unbound_sources=event.unbound_sources; next.opaque_scopes=event.inactive_scopes;
            result=dependency_status(source_versions_subset_admit(&event.sources,live.sheet,live.versions,live.object_high_water,&next.sources));
        }
        definition_event_free(&event);
    }
    if (result!=SOURCE_DEPENDENCY_INVALID && result!=SOURCE_DEPENDENCY_LIMIT && result!=SOURCE_DEPENDENCY_UNSUPPORTED) *out=next;
    return result;
}
