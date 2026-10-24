#include "object_delete_command.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static ObjectDeleteStatus project(const ObjectStore *s,uint64_t id,SheetObject *items,ObjectDeletePlanInfo *out,char **error) {
    if (!s || !id || !out || s->count>DX_MAX_OBJECTS || !s->count || !s->items) { fail(error,"Invalid object deletion plan."); return OBJECT_DELETE_INVALID; }
    ObjectSnapshot before={s->items,s->count}; ObjectsAssetInfo b={0},a={0};
    if (objects_asset_measure(1,&before,s->next_id,&b)!=OBJECTS_ASSET_OK) { fail(error,"Invalid bounded object definitions."); return OBJECT_DELETE_INVALID; }
    size_t at=0; while (at<s->count && s->items[at].id!=id) at++;
    if (at==s->count) { fail(error,"The object is missing."); return OBJECT_DELETE_INVALID; }
    for (size_t i=0;i<s->count;i++) if (s->items[i].source_id==id) {
        fail(error,"This table supplies a pivot or chart. Remove those objects before deleting their source table."); return OBJECT_DELETE_PROTECTED;
    }
    size_t n=0; for (size_t i=0;i<s->count;i++) if (i!=at) items[n++]=s->items[i];
    ObjectSnapshot after={items,n};
    if (objects_asset_measure(1,&after,s->next_id,&a)!=OBJECTS_ASSET_OK) { fail(error,"Invalid complete object deletion definitions."); return OBJECT_DELETE_INVALID; }
    *out=(ObjectDeletePlanInfo){a.measure.owned,a.measure.validation_scratch>b.measure.validation_scratch ? a.measure.validation_scratch : b.measure.validation_scratch}; return OBJECT_DELETE_READY;
}
ObjectDeleteStatus objects_delete_measure(const ObjectStore *s,uint64_t id,ObjectDeletePlanInfo *out,char **error) {
    if (!out) return OBJECT_DELETE_INVALID;
    SheetObject items[DX_MAX_OBJECTS]; ObjectDeletePlanInfo info={0};
    ObjectDeleteStatus status=project(s,id,items,&info,error); if (status==OBJECT_DELETE_READY) *out=info; return status;
}
static ObjectDeleteResult check(ObjectCommandContext *c,const ObjectCommandTarget *t,char **error) {
    ObjectFenceResult f=workbook_check_object_fence(c,t,error);
    return (ObjectDeleteResult){.status=f.status==OBJECT_FENCE_READY ? OBJECT_DELETE_READY : f.status==OBJECT_FENCE_STALE ? OBJECT_DELETE_STALE :
        f.status==OBJECT_FENCE_PROTECTED ? OBJECT_DELETE_PROTECTED : OBJECT_DELETE_INVALID,.object=f.owner,.before_revision=f.revision,.after_revision=f.revision};
}
ObjectDeleteResult workbook_apply_object_delete_recorded(ObjectCommandContext *c,const ObjectCommandTarget *t,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    ObjectDeleteResult r=check(c,t,error); if (r.status!=OBJECT_DELETE_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id);
    SheetObject items[DX_MAX_OBJECTS]; ObjectDeletePlanInfo info={0};
    r.status=project(&s->objects,t->object,items,&info,error); if (r.status!=OBJECT_DELETE_READY) return r;
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available() || info.owned>c->plan_reserved || info.validation_scratch>c->plan_reserved-info.owned) {
        r.status=OBJECT_DELETE_LIMIT; fail(error,"Deletion revision space or admitted planning budget is exhausted."); return r;
    }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) {
        r.status=OBJECT_DELETE_INVALID; if (!error || !*error) fail(error,"Object history or source ownership is incomplete."); return r;
    }
    ObjectStore candidate={.items=items,.count=s->objects.count-1,.next_id=s->objects.next_id},copy={0};
    if (!objects_valid(&s->objects,error) || !objects_valid(&candidate,error)) { r.status=OBJECT_DELETE_INVALID; return r; }
    if (candidate.count) objects_copy(&copy,&candidate); ObjectSnapshot after={copy.items,copy.count}; copy.items=NULL; copy.count=0; objects_free(&copy);
    ObjectSnapshot before={s->objects.items,s->objects.count}; DefinitionDependencySelection dependencies={0}; void *ticket=NULL;
    if (definition_dependencies_select(s->id,(DefinitionDependencyView){NULL,&before},(DefinitionDependencyView){NULL,&after},&s->source_versions,s->objects.next_id,&dependencies)!=DEFINITION_DEPENDENCIES_OK) {
        r.status=OBJECT_DELETE_INVALID; fail(error,"The complete object source dependencies are inconsistent."); goto cancelled;
    }
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=OBJECT_DELETE_INVALID; fail(error,"Object event recording is incomplete."); goto cancelled; }
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,
            .expected_revision=b->command_revision,.revision=b->command_revision+1,.validation_reserved=c->validation_reserved,
            .view={.sheet=s->id,.object_high_water=s->objects.next_id,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&s->source_versions}};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? OBJECT_DELETE_INVALID : OBJECT_DELETE_LIMIT; if (!error || !*error) fail(error,"Object event recording is busy; no change was applied."); goto cancelled; }
        r=check(c,t,error); if (r.status!=OBJECT_DELETE_READY) goto cancelled;
        s=resolve(b,t->owner.sheet_id);
    }
    objects_adopt_removal(&s->objects,&after);
    worksheet_record(s,EDIT_OBJECTS);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b); r.status=OBJECT_DELETE_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket);
    objects_snapshot_free(&after); return r;
}
