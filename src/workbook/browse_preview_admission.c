#include "browse_preview_admission.h"
#include "analysis_asset.h"
#include "definition_state.h"
#include <string.h>
static BrowsePreviewStatus preflight(const BrowsePreview *preview,BrowsePreviewLive live,DefinitionAssetInfo *definition,SourceVersionsAssetInfo *sources,size_t *coexistence) {
    if (!preview || !live.sheet || !live.definition || (!live.document.high && !live.document.low)) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewInfo info=browse_preview_info(preview); size_t bytes=0;
    const void *data=browse_preview_definition(preview,&bytes);
    if (analysis_asset_preflight(data,bytes,info.object_high_water,definition)!=ANALYSIS_ASSET_OK || bytes!=info.definition_bytes || definition->sheet!=info.sheet || definition->owner!=info.owner) return BROWSE_PREVIEW_INVALID;
    data=browse_preview_sources(preview,&bytes);
    if (source_versions_asset_preflight(data,bytes,info.object_high_water,sources)!=SOURCE_VERSIONS_CODEC_OK || bytes!=info.source_bytes || sources->sheet!=info.sheet || sources->high_water!=info.source_high_water) return BROWSE_PREVIEW_INVALID;
    size_t total=info.owned;
    const size_t parts[]={definition->measure.owned,sources->owned,definition->measure.validation_scratch,
        DX_SOURCE_SCOPE_FIELDS*sizeof(const char *)+DX_SOURCE_SCOPE_TEXT_BYTES,live.other_owned};
    for (size_t i=0;i<sizeof(parts)/sizeof(*parts);i++) { if (total>SIZE_MAX-parts[i]) return BROWSE_PREVIEW_LIMIT; total+=parts[i]; }
    *coexistence=total; return BROWSE_PREVIEW_OK;
}
BrowsePreviewStatus browse_preview_admission_measure(const BrowsePreview *preview,BrowsePreviewLive live,size_t *coexistence) {
    if (!coexistence) return BROWSE_PREVIEW_INVALID;
    DefinitionAssetInfo definition={0}; SourceVersionsAssetInfo sources={0}; size_t measured=0;
    BrowsePreviewStatus status=preflight(preview,live,&definition,&sources,&measured); if (status==BROWSE_PREVIEW_OK) *coexistence=measured; return status;
}
SourceDependencyStatus browse_preview_admit(const BrowsePreview *preview,BrowsePreviewLive live,size_t reserved,SourceDependencyAdmission *out) {
    if (!out) return SOURCE_DEPENDENCY_INVALID;
    DefinitionAssetInfo di={0}; SourceVersionsAssetInfo si={0}; size_t coexistence=0;
    BrowsePreviewStatus status=preflight(preview,live,&di,&si,&coexistence);
    if (status!=BROWSE_PREVIEW_OK) return status==BROWSE_PREVIEW_LIMIT ? SOURCE_DEPENDENCY_LIMIT : SOURCE_DEPENDENCY_INVALID;
    if (reserved<coexistence) return SOURCE_DEPENDENCY_LIMIT;
    BrowsePreviewInfo info=browse_preview_info(preview); AnalysisAsset definition={0}; SourceVersionsAsset sources={0}; size_t bytes=0;
    const void *data=browse_preview_definition(preview,&bytes);
    if (analysis_asset_decode(data,bytes,info.object_high_water,di.measure.owned+di.measure.validation_scratch,&definition)!=ANALYSIS_ASSET_OK) return SOURCE_DEPENDENCY_INVALID;
    data=browse_preview_sources(preview,&bytes);
    if (source_versions_asset_decode(data,bytes,info.object_high_water,si.owned,&sources)!=SOURCE_VERSIONS_CODEC_OK) { analysis_asset_free(&definition); return SOURCE_DEPENDENCY_INVALID; }
    SourceDependencyStatus result=SOURCE_DEPENDENCY_INVALID; SourceDependencyAdmission report={0};
    const SourceVersion *binding=definition.definition.source_version ? source_versions_find(&sources.versions,definition.definition.source_version) : NULL;
    if (definition.definition.source_version!=info.source_version || sources.versions.count!=(binding ? 1u : 0u) ||
        (definition.definition.source_version && !binding) ||
        (binding && (binding->owner!=info.owner || binding->kind!=definition.definition.kind || !definition.definition.path || strcmp(binding->path,definition.definition.path) || (!binding->raw.bytes)!=info.binding_pending))) goto done;
    report.unbound_sources=!definition.definition.source_version;
    report.sources.captured_pending=binding && !binding->raw.bytes;
    if (info.document.high!=live.document.high || info.document.low!=live.document.low || info.sheet!=live.sheet || info.owner!=live.owner ||
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
