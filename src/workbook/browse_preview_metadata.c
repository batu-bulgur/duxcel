#include "browse_preview_internal.h"
#include <string.h>
BrowsePreviewStatus browse_preview_metadata_preflight(const BrowsePreview *preview,BrowsePreviewMetadataInfo *out) {
    if (!preview || !out) return BROWSE_PREVIEW_INVALID;
    const BrowsePreviewInfo *info=&preview->info; BrowsePreviewMetadataInfo next={0};
    if (analysis_asset_preflight(preview->definition,info->definition_bytes,info->object_high_water,&next.definition)!=ANALYSIS_ASSET_OK ||
        next.definition.sheet!=info->sheet || next.definition.owner!=info->owner ||
        source_versions_asset_preflight(preview->sources,info->source_bytes,info->object_high_water,&next.sources)!=SOURCE_VERSIONS_CODEC_OK ||
        next.sources.sheet!=info->sheet || next.sources.high_water!=info->source_high_water) return BROWSE_PREVIEW_INVALID;
    if (next.definition.measure.owned>SIZE_MAX-next.sources.owned) return BROWSE_PREVIEW_LIMIT;
    next.owned=next.definition.measure.owned+next.sources.owned;
    size_t stack=DX_SOURCE_SCOPE_FIELDS*sizeof(const char *)+DX_SOURCE_SCOPE_TEXT_BYTES;
    if (next.definition.measure.validation_scratch>SIZE_MAX-stack) return BROWSE_PREVIEW_LIMIT;
    next.validation_scratch=next.definition.measure.validation_scratch+stack;
    *out=next; return BROWSE_PREVIEW_OK;
}
BrowsePreviewStatus browse_preview_metadata_decode(const BrowsePreview *preview,const BrowsePreviewMetadataInfo *info,size_t reserved,AnalysisAsset *definition,SourceVersionsAsset *sources) {
    if (!preview || !info || !definition || !sources) return BROWSE_PREVIEW_INVALID;
    if (info->owned>reserved || info->validation_scratch>reserved-info->owned) return BROWSE_PREVIEW_LIMIT;
    const BrowsePreviewInfo *p=&preview->info; AnalysisAsset next_definition={0}; SourceVersionsAsset next_sources={0};
    if (analysis_asset_decode(preview->definition,p->definition_bytes,p->object_high_water,info->definition.measure.owned+info->definition.measure.validation_scratch,&next_definition)!=ANALYSIS_ASSET_OK) return BROWSE_PREVIEW_INVALID;
    if (source_versions_asset_decode(preview->sources,p->source_bytes,p->object_high_water,info->sources.owned,&next_sources)!=SOURCE_VERSIONS_CODEC_OK) { analysis_asset_free(&next_definition); return BROWSE_PREVIEW_INVALID; }
    const Analysis *d=&next_definition.definition;
    BrowsePreviewStatus status=BROWSE_PREVIEW_INVALID;
    const SourceVersion *binding=d->source_version ? source_versions_find(&next_sources.versions,d->source_version) : NULL;
    if (d->source_version!=p->source_version || next_sources.versions.count!=(binding ? 1u : 0u) ||
        (d->source_version && !binding) || (binding && (binding->owner!=p->owner || binding->kind!=d->kind || !d->path || strcmp(binding->path,d->path) || (!binding->raw.bytes)!=p->binding_pending)) ||
        (!binding && p->binding_pending)) goto done;
    if ((d->kind!=SOURCE_CSV && d->kind!=SOURCE_PARQUET && d->kind!=SOURCE_DATABASE) ||
        (d->kind==SOURCE_DATABASE && (!d->table || !*d->table))) { status=BROWSE_PREVIEW_UNSUPPORTED; goto done; }
    *definition=next_definition; *sources=next_sources; return BROWSE_PREVIEW_OK;
done:
    analysis_asset_free(&next_definition); source_versions_asset_free(&next_sources); return status;
}
