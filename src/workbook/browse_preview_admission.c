#include "browse_preview_admission.h"
#include "browse_preview_internal.h"
#include "definition_state.h"
#include <string.h>
static BrowsePreviewStatus preflight(const BrowsePreview *preview,BrowsePreviewLive live,BrowsePreviewMetadataInfo *metadata,size_t *coexistence) {
    if (!preview || !live.sheet || !live.definition || (!live.document.high && !live.document.low)) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewStatus status=browse_preview_metadata_preflight(preview,metadata); if (status!=BROWSE_PREVIEW_OK) return status;
    size_t total=browse_preview_info(preview).owned;
    const size_t parts[]={metadata->owned,metadata->validation_scratch,live.other_owned};
    for (size_t i=0;i<sizeof(parts)/sizeof(*parts);i++) { if (total>SIZE_MAX-parts[i]) return BROWSE_PREVIEW_LIMIT; total+=parts[i]; }
    *coexistence=total; return BROWSE_PREVIEW_OK;
}
BrowsePreviewStatus browse_preview_admission_measure(const BrowsePreview *preview,BrowsePreviewLive live,size_t *coexistence) {
    if (!coexistence) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewMetadataInfo metadata={0}; size_t measured=0;
    BrowsePreviewStatus status=preflight(preview,live,&metadata,&measured); if (status==BROWSE_PREVIEW_OK) *coexistence=measured; return status;
}
SourceDependencyStatus browse_preview_admit(const BrowsePreview *preview,BrowsePreviewLive live,size_t reserved,SourceDependencyAdmission *out) {
    if (!out) return SOURCE_DEPENDENCY_INVALID;
    BrowsePreviewMetadataInfo metadata={0}; size_t coexistence=0;
    BrowsePreviewStatus status=preflight(preview,live,&metadata,&coexistence);
    if (status!=BROWSE_PREVIEW_OK) return status==BROWSE_PREVIEW_LIMIT ? SOURCE_DEPENDENCY_LIMIT : SOURCE_DEPENDENCY_INVALID;
    if (reserved<coexistence) return SOURCE_DEPENDENCY_LIMIT;
    BrowsePreviewInfo info=browse_preview_info(preview); AnalysisAsset definition={0}; SourceVersionsAsset sources={0};
    status=browse_preview_metadata_decode(preview,&metadata,metadata.owned+metadata.validation_scratch,&definition,&sources);
    if (status!=BROWSE_PREVIEW_OK) return status==BROWSE_PREVIEW_UNSUPPORTED ? SOURCE_DEPENDENCY_UNSUPPORTED : SOURCE_DEPENDENCY_INVALID;
    SourceDependencyStatus result=SOURCE_DEPENDENCY_INVALID; SourceDependencyAdmission report={0};
    const SourceVersion *binding=definition.definition.source_version ? source_versions_find(&sources.versions,definition.definition.source_version) : NULL;
    report.unbound_sources=!definition.definition.source_version;
    report.sources.captured_pending=binding && !binding->raw.bytes;
    bool owner_matches=!live.owner ? !live.table : live.table && live.table->kind==OBJECT_TABLE && live.table->id==live.owner && live.definition==&live.table->source;
    if (!owner_matches || info.document.high!=live.document.high || info.document.low!=live.document.low || info.sheet!=live.sheet || info.owner!=live.owner ||
        !definition_analysis_equal(&definition.definition,live.definition)) { result=SOURCE_DEPENDENCY_CONFLICT; goto done; }
    SourceSubsetStatus subset=source_versions_subset_admit(&sources,live.sheet,live.sources,live.object_high_water,&report.sources);
    if (subset==SOURCE_SUBSET_INVALID) goto done;
    result=subset==SOURCE_SUBSET_EXACT ? SOURCE_DEPENDENCY_EXACT : subset==SOURCE_SUBSET_REFINED ? SOURCE_DEPENDENCY_REFINED :
        subset==SOURCE_SUBSET_UNAVAILABLE ? SOURCE_DEPENDENCY_UNAVAILABLE : SOURCE_DEPENDENCY_CONFLICT;
    if (result!=SOURCE_DEPENDENCY_CONFLICT && live.object_high_water<info.object_high_water) result=SOURCE_DEPENDENCY_UNAVAILABLE;
done:
    analysis_asset_free(&definition); source_versions_asset_free(&sources);
    if (result!=SOURCE_DEPENDENCY_INVALID) *out=report;
    return result;
}
