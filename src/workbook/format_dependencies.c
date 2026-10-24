#include "format_dependencies.h"
#include <stdlib.h>
#include <string.h>
static bool valid(FormatSnapshotView v) {
    FormatStore s={.items=(FormatPatch *)v.items,.count=v.count,.scopes=(FormatScope *)v.scopes,.scope_count=v.scope_count,.source_aware=v.source_aware};
    return format_valid(&s,NULL);
}
static FormatDependenciesStatus collect(FormatSnapshotView view,const SourceVersions *ledger,bool needed[DX_SOURCE_VERSIONS],size_t *legacy) {
    if (!view.scope_count) return FORMAT_DEPENDENCIES_OK;
    unsigned char *used=dx_alloc(view.scope_count);
    for (size_t i=0;i<view.count;i++) for (size_t g=0;g<view.items[i].guard_count;g++)
        used[view.items[i].guard_ids[g]]|=view.items[i].selector==FORMAT_RECORDS ? 2 : 1;
    FormatDependenciesStatus status=FORMAT_DEPENDENCIES_OK;
    for (size_t i=0;i<view.scope_count && status==FORMAT_DEPENDENCIES_OK;i++) if (used[i]) {
        const FormatScope *scope=view.scopes+i;
        if (scope->bytes<4 || memcmp(scope->definition,"DXFS",4)) {
            if (used[i]&2) status=FORMAT_DEPENDENCIES_UNSUPPORTED; else (*legacy)++;
            continue;
        }
        SourceScopeInfo info={0};
        if (!source_scope_validate(scope->definition,scope->bytes,&info,NULL) || info.owner!=scope->owner ||
            ((used[i]&2) && info.kind!=SOURCE_SCOPE_RAW)) { status=FORMAT_DEPENDENCIES_INVALID; continue; }
        const SourceVersion *entry=source_versions_find(ledger,info.version);
        if (!entry || !entry->raw.bytes) { status=FORMAT_DEPENDENCIES_MISSING; continue; }
        if (entry->owner!=info.owner || entry->kind!=info.source_kind || entry->raw.bytes!=info.raw_bytes || memcmp(entry->raw.data,info.raw,info.raw_bytes)) {
            status=FORMAT_DEPENDENCIES_CONFLICT; continue;
        }
        if (used[i]&2) for (size_t p=0;p<view.count;p++) {
            const FormatPatch *patch=view.items+p;
            if (patch->selector!=FORMAT_RECORDS || patch->guard_ids[0]!=i) continue;
            for (size_t f=0;f<patch->field_count;f++) if (patch->fields[f]>=info.field_count) { status=FORMAT_DEPENDENCIES_INVALID; break; }
        }
        if (status==FORMAT_DEPENDENCIES_OK) needed[(size_t)(entry-ledger->items)]=true;
    }
    free(used); return status;
}
void format_dependencies_free(FormatDependencies *out) {
    if (!out) return;
    source_versions_asset_free(&out->sources); *out=(FormatDependencies){0};
}
FormatDependenciesStatus format_dependencies_select(uint64_t sheet,FormatSnapshotView before,FormatSnapshotView after,
    const SourceVersions *ledger,uint64_t owners,FormatDependencySelection *out) {
    if (!sheet || !out || !source_versions_valid(ledger,owners,NULL) || !valid(before) || !valid(after)) return FORMAT_DEPENDENCIES_INVALID;
    FormatDependencySelection next={0};
    FormatDependenciesStatus status=collect(before,ledger,next.rows,&next.legacy_scopes);
    if (status==FORMAT_DEPENDENCIES_OK) status=collect(after,ledger,next.rows,&next.legacy_scopes);
    if (status==FORMAT_DEPENDENCIES_OK) *out=next; return status;
}
FormatDependenciesStatus format_dependencies_build(uint64_t sheet,FormatSnapshotView before,FormatSnapshotView after,
    const SourceVersions *ledger,uint64_t owners,size_t reserved,FormatDependencies *out) {
    if (!out || ledger==&out->sources.versions) return FORMAT_DEPENDENCIES_INVALID;
    FormatDependencySelection selected={0};
    FormatDependenciesStatus status=format_dependencies_select(sheet,before,after,ledger,owners,&selected);
    if (status!=FORMAT_DEPENDENCIES_OK) return status;
    FormatDependencies next={.legacy_scopes=selected.legacy_scopes};
    SourceVersionsCodecStatus copied=source_versions_asset_select(sheet,ledger,owners,selected.rows,ledger->count,reserved,&next.sources);
    if (copied!=SOURCE_VERSIONS_CODEC_OK) return copied==SOURCE_VERSIONS_CODEC_LIMIT ? FORMAT_DEPENDENCIES_LIMIT : FORMAT_DEPENDENCIES_INVALID;
    format_dependencies_free(out); *out=next; return FORMAT_DEPENDENCIES_OK;
}
