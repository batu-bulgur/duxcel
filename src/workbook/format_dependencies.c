#include "format_dependencies.h"
#include <stdlib.h>
#include <string.h>
static bool valid(FormatSnapshotView v) {
    FormatStore s={.items=(FormatPatch *)v.items,.count=v.count,.scopes=(FormatScope *)v.scopes,.scope_count=v.scope_count,.source_aware=v.source_aware};
    return format_valid(&s,NULL);
}
static size_t find(const SourceVersions *ledger,uint64_t version) {
    size_t lo=0,hi=ledger->count;
    while (lo<hi) { size_t mid=lo+(hi-lo)/2; if (ledger->items[mid].version<version) lo=mid+1; else hi=mid; }
    return lo<ledger->count && ledger->items[lo].version==version ? lo : ledger->count;
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
        size_t at=find(ledger,info.version);
        if (at==ledger->count || !ledger->items[at].raw.bytes) { status=FORMAT_DEPENDENCIES_MISSING; continue; }
        const SourceVersion *entry=ledger->items+at;
        if (entry->owner!=info.owner || entry->kind!=info.source_kind || entry->raw.bytes!=info.raw_bytes || memcmp(entry->raw.data,info.raw,info.raw_bytes)) {
            status=FORMAT_DEPENDENCIES_CONFLICT; continue;
        }
        if (used[i]&2) for (size_t p=0;p<view.count;p++) {
            const FormatPatch *patch=view.items+p;
            if (patch->selector!=FORMAT_RECORDS || patch->guard_ids[0]!=i) continue;
            for (size_t f=0;f<patch->field_count;f++) if (patch->fields[f]>=info.field_count) { status=FORMAT_DEPENDENCIES_INVALID; break; }
        }
        if (status==FORMAT_DEPENDENCIES_OK) needed[at]=true;
    }
    free(used); return status;
}
void format_dependencies_free(FormatDependencies *out) {
    if (!out) return;
    source_versions_asset_free(&out->sources); *out=(FormatDependencies){0};
}
FormatDependenciesStatus format_dependencies_build(uint64_t sheet,FormatSnapshotView before,FormatSnapshotView after,
    const SourceVersions *ledger,uint64_t owners,size_t reserved,FormatDependencies *out) {
    if (!sheet || !out || ledger==&out->sources.versions || !source_versions_valid(ledger,owners,NULL) || !valid(before) || !valid(after)) return FORMAT_DEPENDENCIES_INVALID;
    bool needed[DX_SOURCE_VERSIONS]={0}; size_t legacy=0;
    FormatDependenciesStatus status=collect(before,ledger,needed,&legacy);
    if (status==FORMAT_DEPENDENCIES_OK) status=collect(after,ledger,needed,&legacy);
    if (status!=FORMAT_DEPENDENCIES_OK) return status;
    size_t count=0,bytes=0;
    for (size_t i=0;i<ledger->count;i++) if (needed[i]) { count++; bytes+=sizeof(SourceVersion)+strlen(ledger->items[i].path)+1+ledger->items[i].raw.bytes; }
    if (bytes>reserved) return FORMAT_DEPENDENCIES_LIMIT;
    FormatDependencies next={.sources={.sheet=sheet,.versions={.high_water=ledger->high_water,.count=count,.capacity=count,.bytes=bytes}},.legacy_scopes=legacy};
    if (count) next.sources.versions.items=dx_alloc(count*sizeof(SourceVersion));
    size_t at=0;
    for (size_t i=0;i<ledger->count;i++) if (needed[i]) {
        const SourceVersion *from=ledger->items+i; SourceVersion *to=next.sources.versions.items+at++; *to=*from;
        to->path=dx_strdup(from->path); to->raw.data=dx_alloc(from->raw.bytes); memcpy(to->raw.data,from->raw.data,from->raw.bytes);
    }
    format_dependencies_free(out); *out=next; return FORMAT_DEPENDENCIES_OK;
}
