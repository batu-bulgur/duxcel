#include "table_command.h"
#include "definition_baseline.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) {
    for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i;
    return NULL;
}
static size_t object_index(const ObjectStore *s,uint64_t id) {
    size_t i=0; while (i<s->count && s->items[i].id!=id) i++; return i;
}
bool workbook_prepare_table_transform(TableCommandContext *c,size_t index,uint64_t object,TableTransformTarget *out,char **error) {
    if (!c || !c->request_epoch || !out || !object || !workbook_identity_valid(c->book) || index>=c->book->count)
        return fail(error,"Choose a valid Table before transforming it.");
    Worksheet *s=c->book->sheets+index;
    if (s->objects.count>DX_MAX_OBJECTS || (s->objects.count && !s->objects.items)) return fail(error,"Invalid bounded Table ownership.");
    size_t at=object_index(&s->objects,object);
    if (at==s->objects.count || s->objects.items[at].kind!=OBJECT_TABLE) return fail(error,"The source Table is missing.");
    if (definition_baseline_status((DefinitionDependencyView){&s->objects.items[at].source,NULL})!=DEFINITION_BASELINE_READY)
        return fail(error,"Wait for a resolved Table definition.");
    return workbook_prepare_object_fence(c,index,object,out,error);
}

static TableTransformResult check(TableCommandContext *c,const TableTransformTarget *t,char **error) {
    ObjectFenceResult fence=workbook_check_object_fence(c,t,error);
    TableTransformResult r={.status=fence.status==OBJECT_FENCE_STALE ? TABLE_TRANSFORM_STALE : fence.status==OBJECT_FENCE_PROTECTED ? TABLE_TRANSFORM_PROTECTED : TABLE_TRANSFORM_INVALID,
        .owner=fence.owner,.before_revision=fence.revision,.after_revision=fence.revision};
    if (fence.status!=OBJECT_FENCE_READY) return r;
    Worksheet *s=resolve(c->book,t->owner.sheet_id); size_t at=object_index(&s->objects,t->object);
    if (s->objects.items[at].kind!=OBJECT_TABLE) { r.status=TABLE_TRANSFORM_STALE; fail(error,"The source Table changed."); return r; }
    r.status=TABLE_TRANSFORM_READY; return r;
}
static TableTransformStatus plan_status(ObjectSourcePlanStatus status) {
    return status==OBJECT_SOURCE_PLAN_NOOP ? TABLE_TRANSFORM_NOOP : status==OBJECT_SOURCE_PLAN_LIMIT ? TABLE_TRANSFORM_LIMIT :
        status==OBJECT_SOURCE_PLAN_PROTECTED ? TABLE_TRANSFORM_PROTECTED : TABLE_TRANSFORM_INVALID;
}
static TableTransformResult apply(TableCommandContext *c,const TableTransformTarget *t,const Analysis *proposal,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error,bool replacement) {
    TableTransformResult r=check(c,t,error); if (r.status!=TABLE_TRANSFORM_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); ObjectSourcePlanInfo info={0};
    ObjectSourcePlanStatus status=replacement ? objects_source_replacement_measure(&s->objects,t->object,proposal,&info) : objects_source_transform_measure(&s->objects,t->object,proposal,&info);
    if (status!=OBJECT_SOURCE_PLAN_OK) {
        r.status=plan_status(status); if (r.status!=TABLE_TRANSFORM_NOOP) fail(error,"Invalid, exhausted or changed Table connection; no transformation was applied."); return r;
    }
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available()) { r.status=TABLE_TRANSFORM_LIMIT; fail(error,"Table command revision space is exhausted."); return r; }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) {
        r.status=TABLE_TRANSFORM_INVALID; if (!error || !*error) fail(error,"Worksheet history or source ownership is incomplete."); return r;
    }
    ObjectSnapshot after={0}; status=replacement ? objects_plan_source_replacement(&s->objects,t->object,proposal,c->plan_reserved,&after,&info) : objects_plan_source_transform(&s->objects,t->object,proposal,c->plan_reserved,&after,&info);
    if (status!=OBJECT_SOURCE_PLAN_OK) { r.status=plan_status(status); fail(error,"Table planning exceeds its admitted budget or definition constraints."); return r; }
    ObjectSnapshot before={s->objects.items,s->objects.count}; DefinitionDependencySelection dependencies={0};
    if (definition_dependencies_select(s->id,(DefinitionDependencyView){NULL,&before},(DefinitionDependencyView){NULL,&after},&s->source_versions,s->objects.next_id,&dependencies)!=DEFINITION_DEPENDENCIES_OK) {
        objects_snapshot_free(&after); r.status=TABLE_TRANSFORM_INVALID; fail(error,"The complete Table source dependency set is inconsistent."); return r;
    }
    void *ticket=NULL;
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=TABLE_TRANSFORM_INVALID; fail(error,"Table event recording is incomplete."); goto cancelled; }
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,
            .expected_revision=b->command_revision,.revision=b->command_revision+1,.validation_reserved=c->validation_reserved,
            .view={.sheet=s->id,.object_high_water=s->objects.next_id,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&s->source_versions}};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) {
            r.status=reserved || ticket ? TABLE_TRANSFORM_INVALID : TABLE_TRANSFORM_LIMIT;
            if (!error || !*error) fail(error,"Table event recording is busy; no transformation was applied."); goto cancelled;
        }
        r=check(c,t,error); if (r.status!=TABLE_TRANSFORM_READY) goto cancelled;
        s=resolve(b,t->owner.sheet_id);
    }
    objects_adopt_source_transform(&s->objects,&after); worksheet_record(s,EDIT_OBJECTS);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b);
    r.status=TABLE_TRANSFORM_APPLIED; r.after_revision=b->command_revision; r.result_changed=info.result_changed;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket);
    objects_snapshot_free(&after); return r;
}

TableTransformResult workbook_apply_table_transform_recorded(TableCommandContext *c,const TableTransformTarget *t,const Analysis *proposal,const WorkbookHistoryDefinitionRecorder *recorder,char **error) { return apply(c,t,proposal,recorder,error,false); }
TableTransformResult workbook_apply_table_replacement_recorded(TableCommandContext *c,const TableTransformTarget *t,const Analysis *proposal,const WorkbookHistoryDefinitionRecorder *recorder,char **error) { return apply(c,t,proposal,recorder,error,true); }
