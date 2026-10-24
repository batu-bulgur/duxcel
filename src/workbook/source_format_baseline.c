#include "source_format_baseline.h"
#include <stdlib.h>
#include <string.h>
struct SourceFormatBaselinePlan {
    SourceFormatCommandTarget before;
    uint64_t origin_epoch;
    SourceVersions sources;
    FormatStore formats;
};
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
size_t source_format_baseline_reservation(void) { return sizeof(SourceFormatBaselinePlan)+DX_FORMAT_HISTORY_BYTES+DX_SOURCE_VERSIONS_BYTES; }
void source_format_baseline_plan_free(SourceFormatBaselinePlan **owned) {
    if (!owned || !*owned) return;
    SourceFormatBaselinePlan *p=*owned; source_versions_free(&p->sources); format_free(&p->formats); free(p); *owned=NULL;
}
static SourceFormatCommandStatus check(SourceFormatBaselineContext *c,const SourceFormatCommandTarget *t,uint64_t origin,char **error) {
    if (!c || !c->origin_epoch || !t) { fail(error,"Choose an explicit source baseline admission owner."); return SOURCE_FORMAT_COMMAND_INVALID; }
    if (!c->origin_open) { fail(error,"An immutable working origin is already admitted; its source baseline cannot be rewritten."); return SOURCE_FORMAT_COMMAND_PROTECTED; }
    if (*c->origin_epoch!=origin) { fail(error,"The source baseline origin changed; prepare it again."); return SOURCE_FORMAT_COMMAND_STALE; }
    SourceFormatCommandResult owner=workbook_check_source_format_owner(&c->command,t,error); if (owner.status!=SOURCE_FORMAT_COMMAND_READY) return owner.status;
    Worksheet *s=resolve(c->command.book,t->owner.sheet_id);
    if (t->object || s->objects.count || c->command.primary->source_version || s->formats.source_aware) {
        fail(error,"Compatibility baseline admission needs one unbound loaded primary with legacy formats."); return SOURCE_FORMAT_COMMAND_PROTECTED;
    }
    SourceFormatCommandPlan projection={.primary=*c->command.primary,.formats={.scope_count=DX_FORMAT_PATCHES*DX_FORMAT_GUARDS}}; size_t validation=0;
    if (!workbook_source_format_scratch(s,c->command.primary,&projection,&validation,error)) return SOURCE_FORMAT_COMMAND_INVALID;
    if (validation>c->command.validation_reserved) { fail(error,"Source baseline validation exceeds its admitted scratch budget."); return SOURCE_FORMAT_COMMAND_LIMIT; }
    if (!worksheet_legacy_format_history_eligible(s,error)) return SOURCE_FORMAT_COMMAND_INVALID;
    if (s->formats.revision==UINT64_MAX || s->source_versions.high_water==UINT64_MAX || s->source_versions.count==DX_SOURCE_VERSIONS) {
        fail(error,"Source baseline version or invalidation space is exhausted."); return SOURCE_FORMAT_COMMAND_LIMIT;
    }
    return SOURCE_FORMAT_COMMAND_READY;
}
SourceFormatCommandStatus workbook_prepare_source_format_baseline(SourceFormatBaselineContext *c,size_t sheet,const SourceVersion *verified,
    const SourcePacket *result,size_t reserved,SourceFormatBaselinePlan **out,SourceFormatBaselineInfo *info,char **error) {
    if (!c || !c->origin_epoch || !out || *out || !info || !verified || !result) { fail(error,"Prepare complete independent source baseline inputs."); return SOURCE_FORMAT_COMMAND_INVALID; }
    uint64_t origin=*c->origin_epoch; SourceFormatCommandTarget target;
    if (!c->origin_open) { fail(error,"The working source baseline origin is already sealed."); return SOURCE_FORMAT_COMMAND_PROTECTED; }
    if (!c->command.ready || c->command.read_only) { fail(error,"Wait for an editable verified loaded source baseline."); return SOURCE_FORMAT_COMMAND_PROTECTED; }
    if (!workbook_prepare_source_format(&c->command,sheet,0,&target,error)) return SOURCE_FORMAT_COMMAND_INVALID;
    SourceFormatCommandStatus status=check(c,&target,origin,error); if (status!=SOURCE_FORMAT_COMMAND_READY) return status;
    Worksheet *s=resolve(c->command.book,target.owner.sheet_id); const Analysis *a=c->command.primary;
    if (verified->owner || verified->version!=s->source_versions.high_water+1 || verified->kind!=a->kind || !verified->path || !a->path || strcmp(verified->path,a->path) || !verified->raw.bytes) {
        fail(error,"The compatibility source version does not match its unchanged primary."); return SOURCE_FORMAT_COMMAND_INVALID;
    }
    if (reserved<source_format_baseline_reservation()) { fail(error,"The whole source baseline plan exceeds its admitted reservation."); return SOURCE_FORMAT_COMMAND_LIMIT; }
    SourceScopeInfo scope={0},raw={0};
    if (!source_scope_validate(verified->raw.data,verified->raw.bytes,&raw,error) || raw.kind!=SOURCE_SCOPE_RAW || raw.owner || raw.version!=verified->version ||
        !source_scope_validate(result->data,result->bytes,&scope,error) || scope.kind!=SOURCE_SCOPE_RESULT || scope.owner || scope.version!=verified->version ||
        !source_scope_result_matches(result->data,result->bytes,&verified->raw,a,NULL)) {
        if (!error || !*error) fail(error,"The compatibility RESULT guard does not match its verified RAW and definition."); return SOURCE_FORMAT_COMMAND_INVALID;
    }
    SourceFormatBaselinePlan *p=dx_alloc(sizeof(*p)); memset(p,0,sizeof(*p)); p->before=target; p->origin_epoch=origin; bool changed=false;
    if (!source_versions_plan(&s->source_versions,verified,true,&p->sources,error) ||
        !source_versions_valid(&p->sources,s->objects.next_id,error) ||
        !format_plan_guard_store(&s->formats,&(FormatScopeView){0,result->data,result->bytes},1,&p->formats,&changed,error) || !changed) {
        source_format_baseline_plan_free(&p); return SOURCE_FORMAT_COMMAND_INVALID;
    }
    for (size_t i=0;i<1+p->formats.undo_count+p->formats.redo_count;i++) {
        FormatSnapshotView state=i==0 ? format_delta_store_view(&p->formats) : i<=p->formats.undo_count ? format_delta_snapshot_view(p->formats.undo+i-1) : format_delta_snapshot_view(p->formats.redo+i-1-p->formats.undo_count);
        FormatDependencySelection deps={0};
        if (format_dependencies_select(s->id,state,state,&p->sources,s->objects.next_id,&deps)!=FORMAT_DEPENDENCIES_OK) {
            fail(error,"The complete normalized source baseline has unresolved format dependencies."); source_format_baseline_plan_free(&p); return SOURCE_FORMAT_COMMAND_INVALID;
        }
    }
    Analysis after=*a; after.source_version=verified->version; DefinitionDependencySelection deps={0};
    if (definition_dependencies_select(s->id,(DefinitionDependencyView){a,NULL},(DefinitionDependencyView){&after,NULL},&p->sources,s->objects.next_id,&deps)!=DEFINITION_DEPENDENCIES_OK) {
        fail(error,"The normalized primary binding has incomplete immutable dependencies."); source_format_baseline_plan_free(&p); return SOURCE_FORMAT_COMMAND_INVALID;
    }
    status=check(c,&target,origin,error); if (status!=SOURCE_FORMAT_COMMAND_READY) { source_format_baseline_plan_free(&p); return status; }
    SourceFormatCommandPlan projection={.primary=*a,.formats={.scope_count=DX_FORMAT_PATCHES*DX_FORMAT_GUARDS}}; size_t validation=0;
    workbook_source_format_scratch(s,a,&projection,&validation,NULL);
    *info=(SourceFormatBaselineInfo){sizeof(*p)+p->sources.bytes+format_owned_bytes(&p->formats,true),validation,verified->version}; *out=p; return SOURCE_FORMAT_COMMAND_READY;
}
SourceFormatCommandStatus workbook_apply_source_format_baseline(SourceFormatBaselineContext *c,SourceFormatBaselinePlan **owned,char **error) {
    if (!owned || !*owned) { fail(error,"Choose a complete prepared source baseline."); return SOURCE_FORMAT_COMMAND_INVALID; }
    SourceFormatBaselinePlan *p=*owned; SourceFormatCommandStatus status=check(c,&p->before,p->origin_epoch,error); if (status!=SOURCE_FORMAT_COMMAND_READY) return status;
    Worksheet *s=resolve(c->command.book,p->before.owner.sheet_id);
    source_versions_free(&s->source_versions); s->source_versions=p->sources; p->sources=(SourceVersions){0};
    format_adopt_guard_store(&s->formats,&p->formats); c->command.primary->source_version=s->source_versions.high_water; c->command.book->dirty=true;
    source_format_baseline_plan_free(owned); return SOURCE_FORMAT_COMMAND_APPLIED;
}
