#include "object_binding_command.h"
#include "history.h"
#include "definition_baseline.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static size_t object_index(const ObjectStore *s,uint64_t id) { size_t i=0; while (i<s->count && s->items[i].id!=id) i++; return i; }
static bool context_valid(const ObjectBindingContext *c) { return c && c->primary && c->primary_epoch; }
bool workbook_prepare_object_binding(ObjectBindingContext *c,size_t sheet,uint64_t id,ObjectBindingTarget *out,char **error) {
    if (!context_valid(c) || !out) return fail(error,"Choose a valid source binding owner.");
    ObjectBindingTarget next={0};
    if (!workbook_prepare_object_fence(&c->command,sheet,id,&next.object,error)) return false;
    next.authority=c->primary==&c->command.book->sheets[sheet].analysis ? NULL : c->primary; next.primary_epoch=*c->primary_epoch; memcpy(&next.primary,c->primary,sizeof(*c->primary)); *out=next; return true;
}
static ObjectBindingStatus project(const ObjectStore *s,uint64_t id,uint64_t source,const Analysis *primary,SheetObject *items,ObjectBindingPlanInfo *out,char **error) {
    if (!s || !id || !out || !primary || s->count>DX_MAX_OBJECTS || !s->count || !s->items) { fail(error,"Invalid source binding plan."); return OBJECT_BINDING_INVALID; }
    ObjectSnapshot before={s->items,s->count}; ObjectsAssetInfo b={0},a={0}; AnalysisAssetMeasure p={0};
    if (objects_asset_measure(1,&before,s->next_id,&b)!=OBJECTS_ASSET_OK || analysis_asset_measure(1,0,primary,s->next_id,&p)!=ANALYSIS_ASSET_OK) { fail(error,"Invalid bounded source definitions."); return OBJECT_BINDING_INVALID; }
    size_t at=object_index(s,id);
    if (at==s->count || s->items[at].kind==OBJECT_TABLE) { fail(error,"Choose a Pivot or Chart to change its data source."); return OBJECT_BINDING_INVALID; }
    const SheetObject *o=s->items+at;
    if (o->source_id==source) return OBJECT_BINDING_NOOP;
    const Analysis *chosen=primary;
    if (source) { size_t from=object_index(s,source); if (from==s->count || s->items[from].kind!=OBJECT_TABLE) { fail(error,"The selected Table no longer exists."); return OBJECT_BINDING_INVALID; } chosen=&s->items[from].source; }
    if (chosen->kind==SOURCE_NONE || definition_baseline_status((DefinitionDependencyView){chosen,NULL})!=DEFINITION_BASELINE_READY) { fail(error,"Choose an available source with a resolved relation."); return OBJECT_BINDING_PROTECTED; }
    if (o->revision==UINT64_MAX) { fail(error,"Object query revision space is exhausted."); return OBJECT_BINDING_LIMIT; }
    memcpy(items,s->items,s->count*sizeof(*items)); items[at].source_id=source; items[at].revision++;
    ObjectSnapshot after={items,s->count};
    if (objects_asset_measure(1,&after,s->next_id,&a)!=OBJECTS_ASSET_OK) { fail(error,"Invalid complete binding definitions."); return OBJECT_BINDING_INVALID; }
    size_t scratch=a.measure.validation_scratch>b.measure.validation_scratch ? a.measure.validation_scratch : b.measure.validation_scratch;
    *out=(ObjectBindingPlanInfo){a.measure.owned,scratch>p.validation_scratch ? scratch : p.validation_scratch,p.owned}; return OBJECT_BINDING_READY;
}
ObjectBindingStatus objects_binding_measure(const ObjectStore *s,uint64_t id,uint64_t source,const Analysis *primary,ObjectBindingPlanInfo *out,char **error) {
    if (!out) return OBJECT_BINDING_INVALID;
    SheetObject items[DX_MAX_OBJECTS]; ObjectBindingPlanInfo info={0};
    ObjectBindingStatus status=project(s,id,source,primary,items,&info,error); if (status==OBJECT_BINDING_READY) *out=info; return status;
}
static ObjectBindingResult check(ObjectBindingContext *c,const ObjectBindingTarget *t,char **error) {
    if (!context_valid(c) || !t) { fail(error,"Invalid binding context."); return (ObjectBindingResult){.status=OBJECT_BINDING_INVALID}; }
    ObjectFenceResult f=workbook_check_object_fence(&c->command,&t->object,error);
    ObjectBindingResult r={.status=f.status==OBJECT_FENCE_READY ? OBJECT_BINDING_READY : f.status==OBJECT_FENCE_STALE ? OBJECT_BINDING_STALE :
        f.status==OBJECT_FENCE_PROTECTED ? OBJECT_BINDING_PROTECTED : OBJECT_BINDING_INVALID,.object=f.owner,.before_revision=f.revision,.after_revision=f.revision};
    Worksheet *sheet=r.status==OBJECT_BINDING_READY ? resolve(c->command.book,t->object.owner.sheet_id) : NULL;
    if (r.status==OBJECT_BINDING_READY && (c->primary!=(t->authority ? t->authority : &sheet->analysis) || *c->primary_epoch!=t->primary_epoch || memcmp(c->primary,&t->primary,sizeof(t->primary)))) {
        r.status=OBJECT_BINDING_STALE; fail(error,"The primary source changed; prepare the binding again.");
    }
    return r;
}
ObjectBindingResult workbook_apply_object_binding_recorded(ObjectBindingContext *c,const ObjectBindingTarget *t,uint64_t source,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    ObjectBindingResult r=check(c,t,error); if (r.status!=OBJECT_BINDING_READY) return r;
    Workbook *b=c->command.book; Worksheet *s=resolve(b,t->object.owner.sheet_id);
    SheetObject items[DX_MAX_OBJECTS]; ObjectBindingPlanInfo info={0};
    r.status=project(&s->objects,t->object.object,source,c->primary,items,&info,error); if (r.status!=OBJECT_BINDING_READY) return r;
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available() || info.owned>c->command.plan_reserved || info.validation_scratch>c->command.plan_reserved-info.owned) {
        r.status=OBJECT_BINDING_LIMIT; fail(error,"Presentation revision space or admitted planning budget is exhausted."); return r;
    }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) {
        r.status=OBJECT_BINDING_INVALID; if (!error || !*error) fail(error,"Object history or source ownership is incomplete."); return r;
    }
    DefinitionDependencySelection root={0};
    if (!analysis_query_valid(c->primary,error) || !analysis_filters_valid(c->primary,error) || !analysis_sorts_valid(c->primary,error) ||
        definition_dependencies_select(s->id,(DefinitionDependencyView){c->primary,NULL},(DefinitionDependencyView){c->primary,NULL},&s->source_versions,s->objects.next_id,&root)!=DEFINITION_DEPENDENCIES_OK) {
        r.status=OBJECT_BINDING_INVALID; if (!error || !*error) fail(error,"The unchanged primary source dependencies are inconsistent."); return r;
    }
    ObjectStore candidate={.items=items,.count=s->objects.count,.next_id=s->objects.next_id},copy={0};
    if (!objects_valid(&s->objects,error) || !objects_valid(&candidate,error)) { r.status=OBJECT_BINDING_INVALID; return r; }
    objects_copy(&copy,&candidate); ObjectSnapshot after={copy.items,copy.count}; copy.items=NULL; copy.count=0; objects_free(&copy);
    ObjectSnapshot before={s->objects.items,s->objects.count}; DefinitionDependencySelection dependencies={0}; void *ticket=NULL;
    if (definition_dependencies_select(s->id,(DefinitionDependencyView){NULL,&before},(DefinitionDependencyView){NULL,&after},&s->source_versions,s->objects.next_id,&dependencies)!=DEFINITION_DEPENDENCIES_OK) {
        r.status=OBJECT_BINDING_INVALID; fail(error,"The complete object source dependencies are inconsistent."); goto cancelled;
    }
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=OBJECT_BINDING_INVALID; fail(error,"Object event recording is incomplete."); goto cancelled; }
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,
            .expected_revision=b->command_revision,.revision=b->command_revision+1,.validation_reserved=c->command.validation_reserved,
            .view={.sheet=s->id,.object_high_water=s->objects.next_id,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&s->source_versions}};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? OBJECT_BINDING_INVALID : OBJECT_BINDING_LIMIT; if (!error || !*error) fail(error,"Object event recording is busy; no change was applied."); goto cancelled; }
        r=check(c,t,error); if (r.status!=OBJECT_BINDING_READY) goto cancelled;
        s=resolve(b,t->object.owner.sheet_id);
    }
    objects_adopt_definition(&s->objects,&after);
    worksheet_record(s,EDIT_OBJECTS);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b); r.status=OBJECT_BINDING_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket);
    objects_snapshot_free(&after); return r;
}
