#include "object_pivot_command.h"
#include "history.h"
#include "definition_state.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static size_t object_index(const ObjectStore *s,uint64_t id) { size_t i=0; while (i<s->count && s->items[i].id!=id) i++; return i; }
static ObjectPivotStatus project(const ObjectStore *s,uint64_t id,const PivotDefinition *proposal,SheetObject *items,ObjectPivotPlanInfo *out,char **error) {
    if (!s || !id || !out || !proposal || s->count>DX_MAX_OBJECTS || !s->count || !s->items) { fail(error,"Invalid Pivot definition plan."); return OBJECT_PIVOT_INVALID; }
    ObjectSnapshot before={s->items,s->count}; ObjectsAssetInfo b={0},a={0};
    if (objects_asset_measure(1,&before,s->next_id,&b)!=OBJECTS_ASSET_OK) { fail(error,"Invalid bounded object definitions."); return OBJECT_PIVOT_INVALID; }
    size_t at=object_index(s,id); PivotAssetMeasure p={0};
    if (at==s->count || s->items[at].kind==OBJECT_TABLE || pivot_asset_measure(1,id,proposal,s->next_id,&p)!=PIVOT_ASSET_OK) {
        fail(error,"Choose a valid complete analytical Pivot definition."); return OBJECT_PIVOT_INVALID;
    }
    const SheetObject *o=s->items+at;
    if (definition_pivot_equal(&o->pivot,proposal)) return OBJECT_PIVOT_NOOP;
    if (o->revision==UINT64_MAX) { fail(error,"Pivot query revision space is exhausted."); return OBJECT_PIVOT_LIMIT; }
    memcpy(items,s->items,s->count*sizeof(*items)); items[at].pivot=*proposal;
    items[at].chart_measure=(unsigned char)objects_pivot_measure_selection(&o->pivot,o->chart_measure,proposal); items[at].revision++;
    ObjectSnapshot after={items,s->count};
    if (objects_asset_measure(1,&after,s->next_id,&a)!=OBJECTS_ASSET_OK) { fail(error,"Invalid complete Pivot object definitions."); return OBJECT_PIVOT_INVALID; }
    *out=(ObjectPivotPlanInfo){a.measure.owned,a.measure.validation_scratch>b.measure.validation_scratch ? a.measure.validation_scratch : b.measure.validation_scratch,p.owned};
    return OBJECT_PIVOT_READY;
}
ObjectPivotStatus objects_pivot_measure(const ObjectStore *s,uint64_t id,const PivotDefinition *proposal,ObjectPivotPlanInfo *out,char **error) {
    if (!out) return OBJECT_PIVOT_INVALID;
    SheetObject items[DX_MAX_OBJECTS]; ObjectPivotPlanInfo info={0};
    ObjectPivotStatus status=project(s,id,proposal,items,&info,error); if (status==OBJECT_PIVOT_READY) *out=info; return status;
}
static ObjectPivotResult check(ObjectCommandContext *c,const ObjectCommandTarget *t,char **error) {
    ObjectFenceResult f=workbook_check_object_fence(c,t,error);
    return (ObjectPivotResult){.status=f.status==OBJECT_FENCE_READY ? OBJECT_PIVOT_READY : f.status==OBJECT_FENCE_STALE ? OBJECT_PIVOT_STALE :
        f.status==OBJECT_FENCE_PROTECTED ? OBJECT_PIVOT_PROTECTED : OBJECT_PIVOT_INVALID,.object=f.owner,.before_revision=f.revision,.after_revision=f.revision};
}
ObjectPivotResult workbook_apply_object_pivot_recorded(ObjectCommandContext *c,const ObjectCommandTarget *t,const PivotDefinition *proposal,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    ObjectPivotResult r=check(c,t,error); if (r.status!=OBJECT_PIVOT_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id);
    SheetObject items[DX_MAX_OBJECTS]; ObjectPivotPlanInfo info={0};
    r.status=project(&s->objects,t->object,proposal,items,&info,error); if (r.status!=OBJECT_PIVOT_READY) return r;
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available() || info.owned>c->plan_reserved || info.validation_scratch>c->plan_reserved-info.owned) {
        r.status=OBJECT_PIVOT_LIMIT; fail(error,"Presentation revision space or admitted planning budget is exhausted."); return r;
    }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) {
        r.status=OBJECT_PIVOT_INVALID; if (!error || !*error) fail(error,"Object history or source ownership is incomplete."); return r;
    }
    ObjectStore candidate={.items=items,.count=s->objects.count,.next_id=s->objects.next_id},copy={0};
    if (!objects_valid(&s->objects,error) || !objects_valid(&candidate,error)) { r.status=OBJECT_PIVOT_INVALID; return r; }
    objects_copy(&copy,&candidate); ObjectSnapshot after={copy.items,copy.count}; copy.items=NULL; copy.count=0; objects_free(&copy);
    ObjectSnapshot before={s->objects.items,s->objects.count}; DefinitionDependencySelection dependencies={0}; void *ticket=NULL;
    if (definition_dependencies_select(s->id,(DefinitionDependencyView){NULL,&before},(DefinitionDependencyView){NULL,&after},&s->source_versions,s->objects.next_id,&dependencies)!=DEFINITION_DEPENDENCIES_OK) {
        r.status=OBJECT_PIVOT_INVALID; fail(error,"The complete object source dependencies are inconsistent."); goto cancelled;
    }
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=OBJECT_PIVOT_INVALID; fail(error,"Object event recording is incomplete."); goto cancelled; }
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,
            .expected_revision=b->command_revision,.revision=b->command_revision+1,.validation_reserved=c->validation_reserved,
            .view={.sheet=s->id,.object_high_water=s->objects.next_id,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&s->source_versions}};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? OBJECT_PIVOT_INVALID : OBJECT_PIVOT_LIMIT; if (!error || !*error) fail(error,"Object event recording is busy; no change was applied."); goto cancelled; }
        r=check(c,t,error); if (r.status!=OBJECT_PIVOT_READY) goto cancelled;
        s=resolve(b,t->owner.sheet_id);
    }
    objects_adopt_definition(&s->objects,&after);
    worksheet_record(s,EDIT_OBJECTS);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b); r.status=OBJECT_PIVOT_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket);
    objects_snapshot_free(&after); return r;
}
