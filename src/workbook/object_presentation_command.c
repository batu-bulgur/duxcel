#include "object_presentation_command.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static size_t object_index(const ObjectStore *s,uint64_t id) { size_t i=0; while (i<s->count && s->items[i].id!=id) i++; return i; }
static ObjectPresentationStatus project(const ObjectStore *s,uint64_t id,ObjectPresentationCommand command,SheetObject *items,ObjectPresentationPlanInfo *out,char **error) {
    if (!s || !id || !out || s->count>DX_MAX_OBJECTS || !s->count || !s->items) { fail(error,"Invalid object presentation plan."); return OBJECT_PRESENTATION_INVALID; }
    ObjectSnapshot before={s->items,s->count}; ObjectsAssetInfo b={0},a={0};
    if (objects_asset_measure(1,&before,s->next_id,&b)!=OBJECTS_ASSET_OK) { fail(error,"Invalid bounded object definitions."); return OBJECT_PRESENTATION_INVALID; }
    size_t at=object_index(s,id);
    if (at==s->count) { fail(error,"The presentation object is missing."); return OBJECT_PRESENTATION_INVALID; }
    const SheetObject *o=s->items+at;
    if (command.operation==OBJECT_PRESENTATION_MARK) {
        if (o->kind!=OBJECT_CHART || command.value>MARK_SCATTER) { fail(error,"Choose a valid chart mark."); return OBJECT_PRESENTATION_INVALID; }
        if (o->mark==command.value) return OBJECT_PRESENTATION_NOOP;
    } else if (command.operation==OBJECT_PRESENTATION_MEASURE) {
        if (o->kind==OBJECT_TABLE || command.value>=o->pivot.counts[PIVOT_VALUES]) { fail(error,"Choose an available analytical measure."); return OBJECT_PRESENTATION_INVALID; }
        if (o->chart_measure==command.value) return OBJECT_PRESENTATION_NOOP;
        if (s->title_revision==UINT64_MAX) { fail(error,"Object title revision space is exhausted."); return OBJECT_PRESENTATION_LIMIT; }
    } else { fail(error,"Unknown object presentation command."); return OBJECT_PRESENTATION_INVALID; }
    memcpy(items,s->items,s->count*sizeof(*items));
    if (command.operation==OBJECT_PRESENTATION_MARK) items[at].mark=(ChartMark)command.value;
    else items[at].chart_measure=(unsigned char)command.value;
    ObjectSnapshot after={items,s->count};
    if (objects_asset_measure(1,&after,s->next_id,&a)!=OBJECTS_ASSET_OK) { fail(error,"Invalid complete presentation definitions."); return OBJECT_PRESENTATION_INVALID; }
    *out=(ObjectPresentationPlanInfo){a.measure.owned,a.measure.validation_scratch>b.measure.validation_scratch ? a.measure.validation_scratch : b.measure.validation_scratch};
    return OBJECT_PRESENTATION_READY;
}
ObjectPresentationStatus objects_presentation_measure(const ObjectStore *s,uint64_t id,ObjectPresentationCommand command,ObjectPresentationPlanInfo *out,char **error) {
    if (!out) return OBJECT_PRESENTATION_INVALID;
    SheetObject items[DX_MAX_OBJECTS]; ObjectPresentationPlanInfo info={0};
    ObjectPresentationStatus status=project(s,id,command,items,&info,error); if (status==OBJECT_PRESENTATION_READY) *out=info; return status;
}
static ObjectPresentationResult check(ObjectCommandContext *c,const ObjectCommandTarget *t,char **error) {
    ObjectFenceResult f=workbook_check_object_fence(c,t,error);
    return (ObjectPresentationResult){.status=f.status==OBJECT_FENCE_READY ? OBJECT_PRESENTATION_READY : f.status==OBJECT_FENCE_STALE ? OBJECT_PRESENTATION_STALE :
        f.status==OBJECT_FENCE_PROTECTED ? OBJECT_PRESENTATION_PROTECTED : OBJECT_PRESENTATION_INVALID,.object=f.owner,.before_revision=f.revision,.after_revision=f.revision};
}
ObjectPresentationResult workbook_apply_object_presentation_recorded(ObjectCommandContext *c,const ObjectCommandTarget *t,ObjectPresentationCommand command,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    ObjectPresentationResult r=check(c,t,error); if (r.status!=OBJECT_PRESENTATION_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id);
    SheetObject items[DX_MAX_OBJECTS]; ObjectPresentationPlanInfo info={0};
    r.status=project(&s->objects,t->object,command,items,&info,error); if (r.status!=OBJECT_PRESENTATION_READY) return r;
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available() || info.owned>c->plan_reserved || info.validation_scratch>c->plan_reserved-info.owned) {
        r.status=OBJECT_PRESENTATION_LIMIT; fail(error,"Presentation revision space or admitted planning budget is exhausted."); return r;
    }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) {
        r.status=OBJECT_PRESENTATION_INVALID; if (!error || !*error) fail(error,"Object history or source ownership is incomplete."); return r;
    }
    ObjectStore candidate={.items=items,.count=s->objects.count,.next_id=s->objects.next_id},copy={0};
    if (!objects_valid(&s->objects,error) || !objects_valid(&candidate,error)) { r.status=OBJECT_PRESENTATION_INVALID; return r; }
    objects_copy(&copy,&candidate); ObjectSnapshot after={copy.items,copy.count}; copy.items=NULL; copy.count=0; objects_free(&copy);
    ObjectSnapshot before={s->objects.items,s->objects.count}; DefinitionDependencySelection dependencies={0}; void *ticket=NULL;
    if (definition_dependencies_select(s->id,(DefinitionDependencyView){NULL,&before},(DefinitionDependencyView){NULL,&after},&s->source_versions,s->objects.next_id,&dependencies)!=DEFINITION_DEPENDENCIES_OK) {
        r.status=OBJECT_PRESENTATION_INVALID; fail(error,"The complete object source dependencies are inconsistent."); goto cancelled;
    }
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=OBJECT_PRESENTATION_INVALID; fail(error,"Object event recording is incomplete."); goto cancelled; }
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,
            .expected_revision=b->command_revision,.revision=b->command_revision+1,.validation_reserved=c->validation_reserved,
            .view={.sheet=s->id,.object_high_water=s->objects.next_id,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&s->source_versions}};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? OBJECT_PRESENTATION_INVALID : OBJECT_PRESENTATION_LIMIT; if (!error || !*error) fail(error,"Object event recording is busy; no change was applied."); goto cancelled; }
        r=check(c,t,error); if (r.status!=OBJECT_PRESENTATION_READY) goto cancelled;
        s=resolve(b,t->owner.sheet_id);
    }
    objects_adopt_definition(&s->objects,&after); if (command.operation==OBJECT_PRESENTATION_MEASURE) s->objects.title_revision++; worksheet_record(s,EDIT_OBJECTS);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b); r.status=OBJECT_PRESENTATION_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket);
    objects_snapshot_free(&after); return r;
}
