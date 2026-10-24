#include "definition_dependencies.h"
#include <string.h>
static bool shape(uint64_t sheet,DefinitionDependencyView view,uint64_t owners) {
    AnalysisAssetMeasure a={0}; ObjectsAssetInfo o={0};
    return (!view.primary || analysis_asset_measure(sheet,0,view.primary,owners,&a)==ANALYSIS_ASSET_OK) &&
        (!view.objects || objects_asset_measure(sheet,view.objects,owners,&o)==OBJECTS_ASSET_OK);
}
static DefinitionDependenciesStatus binding(const Analysis *a,uint64_t owner,const SourceVersions *ledger,bool needed[DX_SOURCE_VERSIONS],size_t *unbound) {
    if (!a->source_version) { if (a->kind!=SOURCE_NONE) (*unbound)++; return DEFINITION_DEPENDENCIES_OK; }
    if (a->kind==SOURCE_NONE || !a->path || !*a->path) return DEFINITION_DEPENDENCIES_INVALID;
    if (a->kind!=SOURCE_CSV && a->kind!=SOURCE_PARQUET) return DEFINITION_DEPENDENCIES_UNSUPPORTED;
    const SourceVersion *entry=source_versions_find(ledger,a->source_version);
    if (!entry) return DEFINITION_DEPENDENCIES_MISSING;
    if (entry->owner!=owner || entry->kind!=a->kind || strcmp(entry->path,a->path)) return DEFINITION_DEPENDENCIES_CONFLICT;
    needed[(size_t)(entry-ledger->items)]=true; return DEFINITION_DEPENDENCIES_OK;
}
static DefinitionDependenciesStatus collect(DefinitionDependencyView view,const SourceVersions *ledger,bool needed[DX_SOURCE_VERSIONS],size_t *unbound) {
    DefinitionDependenciesStatus status=DEFINITION_DEPENDENCIES_OK;
    if (view.primary) status=binding(view.primary,0,ledger,needed,unbound);
    for (size_t i=0;view.objects && i<view.objects->count && status==DEFINITION_DEPENDENCIES_OK;i++) {
        const SheetObject *o=view.objects->items+i;
        if (o->kind==OBJECT_TABLE) status=binding(&o->source,o->id,ledger,needed,unbound);
    }
    return status;
}
void definition_dependencies_free(DefinitionDependencies *out) { if (out) { source_versions_asset_free(&out->sources); *out=(DefinitionDependencies){0}; } }
DefinitionDependenciesStatus definition_dependencies_build(uint64_t sheet,DefinitionDependencyView before,DefinitionDependencyView after,
    const SourceVersions *ledger,uint64_t owners,size_t reserved,DefinitionDependencies *out) {
    if (!sheet || !out || ledger==&out->sources.versions || (!!before.primary!=!!after.primary) || (!!before.objects!=!!after.objects) ||
        !source_versions_valid(ledger,owners,NULL) || !shape(sheet,before,owners) || !shape(sheet,after,owners)) return DEFINITION_DEPENDENCIES_INVALID;
    bool needed[DX_SOURCE_VERSIONS]={0}; size_t unbound=0;
    DefinitionDependenciesStatus status=collect(before,ledger,needed,&unbound);
    if (status==DEFINITION_DEPENDENCIES_OK) status=collect(after,ledger,needed,&unbound);
    if (status!=DEFINITION_DEPENDENCIES_OK) return status;
    DefinitionDependencies next={.unbound_sources=unbound};
    SourceVersionsCodecStatus copied=source_versions_asset_select(sheet,ledger,owners,needed,ledger->count,reserved,&next.sources);
    if (copied!=SOURCE_VERSIONS_CODEC_OK) return copied==SOURCE_VERSIONS_CODEC_LIMIT ? DEFINITION_DEPENDENCIES_LIMIT : DEFINITION_DEPENDENCIES_INVALID;
    definition_dependencies_free(out); *out=next; return DEFINITION_DEPENDENCIES_OK;
}
