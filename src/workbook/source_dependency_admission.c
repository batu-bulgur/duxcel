#include "source_dependency_admission.h"
static SourceDependencyStatus subset(SourceSubsetStatus s) {
    switch (s) {
    case SOURCE_SUBSET_EXACT: return SOURCE_DEPENDENCY_EXACT;
    case SOURCE_SUBSET_REFINED: return SOURCE_DEPENDENCY_REFINED;
    case SOURCE_SUBSET_UNAVAILABLE: return SOURCE_DEPENDENCY_UNAVAILABLE;
    case SOURCE_SUBSET_CONFLICT: return SOURCE_DEPENDENCY_CONFLICT;
    default: return SOURCE_DEPENDENCY_INVALID;
    }
}
static bool selected_exactly(const bool *rows,size_t count) { for (size_t i=0;i<count;i++) if (!rows[i]) return false; return true; }
SourceDependencyStatus source_definition_dependencies_admit(DefinitionDependencyView before,DefinitionDependencyView after,
    const DefinitionDependencies *captured,SourceDependencyLive live,SourceDependencyAdmission *out) {
    if (!captured || !out || !live.sheet) return SOURCE_DEPENDENCY_INVALID;
    DefinitionDependencySelection selected;
    DefinitionDependenciesStatus s=definition_dependencies_select(captured->sources.sheet,before,after,&captured->sources.versions,UINT64_MAX,&selected);
    if (s!=DEFINITION_DEPENDENCIES_OK) return s==DEFINITION_DEPENDENCIES_UNSUPPORTED ? SOURCE_DEPENDENCY_UNSUPPORTED : SOURCE_DEPENDENCY_INVALID;
    if (selected.unbound_sources!=captured->unbound_sources || !selected_exactly(selected.rows,captured->sources.versions.count)) return SOURCE_DEPENDENCY_INVALID;
    SourceDependencyAdmission next={.unbound_sources=selected.unbound_sources};
    SourceDependencyStatus status=subset(source_versions_subset_admit(&captured->sources,live.sheet,live.versions,live.object_high_water,&next.sources));
    if (status!=SOURCE_DEPENDENCY_INVALID) *out=next; return status;
}
SourceDependencyStatus source_format_dependencies_scratch(FormatSnapshotView before,FormatSnapshotView after,size_t *out) {
    if (!out) return SOURCE_DEPENDENCY_INVALID;
    FormatSnapshotBodyInfo a,b;
    if (format_snapshot_body_measure(before,&a)!=FORMAT_DELTA_OK || format_snapshot_body_measure(after,&b)!=FORMAT_DELTA_OK) return SOURCE_DEPENDENCY_INVALID;
    size_t scopes=before.scope_count>after.scope_count ? before.scope_count : after.scope_count;
    size_t scratch=a.validation_scratch>b.validation_scratch ? a.validation_scratch : b.validation_scratch;
    if (scopes) {
        size_t filter=analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true);
        if (filter==SIZE_MAX || scopes>SIZE_MAX-filter) return SOURCE_DEPENDENCY_LIMIT;
        if (filter+scopes>scratch) scratch=filter+scopes;
    }
    *out=scratch; return SOURCE_DEPENDENCY_EXACT;
}
SourceDependencyStatus source_format_dependencies_admit(FormatSnapshotView before,FormatSnapshotView after,
    const FormatDependencies *captured,SourceDependencyLive live,size_t reserved,SourceDependencyAdmission *out) {
    if (!captured || !out || !live.sheet) return SOURCE_DEPENDENCY_INVALID;
    size_t scratch; SourceDependencyStatus status=source_format_dependencies_scratch(before,after,&scratch);
    if (status!=SOURCE_DEPENDENCY_EXACT) return status;
    if (reserved<scratch) return SOURCE_DEPENDENCY_LIMIT;
    FormatDependencySelection selected;
    FormatDependenciesStatus s=format_dependencies_select(captured->sources.sheet,before,after,&captured->sources.versions,UINT64_MAX,&selected);
    if (s!=FORMAT_DEPENDENCIES_OK) return s==FORMAT_DEPENDENCIES_UNSUPPORTED ? SOURCE_DEPENDENCY_UNSUPPORTED : SOURCE_DEPENDENCY_INVALID;
    if (selected.legacy_scopes!=captured->legacy_scopes || !selected_exactly(selected.rows,captured->sources.versions.count)) return SOURCE_DEPENDENCY_INVALID;
    SourceDependencyAdmission next={.opaque_scopes=selected.legacy_scopes};
    status=subset(source_versions_subset_admit(&captured->sources,live.sheet,live.versions,live.object_high_water,&next.sources));
    if (status!=SOURCE_DEPENDENCY_INVALID) *out=next; return status;
}
