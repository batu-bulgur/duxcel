#include "object_layout_command.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static size_t object_index(const ObjectStore *s,uint64_t id) { size_t i=0; while (i<s->count && s->items[i].id!=id) i++; return i; }
static bool valid_context(const ObjectLayoutContext *c) { return c && c->source_rows && c->source_columns && c->source_count_pending; }
static uint64_t effective_rows(const ObjectLayoutContext *c) { return *c->source_count_pending ? UINT64_MAX : *c->source_rows; }
bool workbook_prepare_object_layout(ObjectLayoutContext *c,size_t sheet,uint64_t object,ObjectLayoutTarget *out,char **error) {
    if (!valid_context(c) || !out) return fail(error,"Choose a valid layout owner.");
    ObjectLayoutTarget next;
    if (!workbook_prepare_object_fence(&c->command,sheet,object,&next.object,error)) return false;
    next.source_rows=*c->source_rows; next.source_columns=*c->source_columns; next.source_count_pending=*c->source_count_pending;
    *out=next; return true;
}
static ObjectLayoutStatus project(const ObjectStore *s,uint64_t id,ObjectLayoutCommand command,const CellStore *cells,uint64_t source_rows,size_t source_columns,
    SheetObject *items,ObjectLayoutPlanInfo *out,char **error) {
    if (!s || !id || !out || !cells || s->count>DX_MAX_OBJECTS || !s->count || !s->items) { fail(error,"Invalid object layout plan."); return OBJECT_LAYOUT_INVALID; }
    ObjectSnapshot before={s->items,s->count}; ObjectsAssetInfo b={0},a={0};
    if (objects_asset_measure(1,&before,s->next_id,&b)!=OBJECTS_ASSET_OK) { fail(error,"Invalid bounded object definitions."); return OBJECT_LAYOUT_INVALID; }
    size_t at=object_index(s,id);
    if (at==s->count) { fail(error,"The layout object is missing."); return OBJECT_LAYOUT_INVALID; }
    const SheetObject *o=s->items+at;
    if (o->kind==OBJECT_TABLE || (command.kind!=OBJECT_PIVOT && command.kind!=OBJECT_CHART) || (unsigned)command.mark>MARK_SCATTER ||
        (command.kind==OBJECT_PIVOT && command.mark!=MARK_BAR) || !command.rows || !command.columns ||
        (command.kind==OBJECT_CHART && (command.rows<8 || command.rows>4096 || command.columns<3 || command.columns>256))) {
        fail(error,"Choose a valid matrix/chart representation and extent."); return OBJECT_LAYOUT_INVALID;
    }
    if (command.operation==OBJECT_LAYOUT_REPRESENTATION) {
        if (o->kind==command.kind && o->mark==command.mark) return OBJECT_LAYOUT_NOOP;
    } else if (command.operation==OBJECT_LAYOUT_SIZE) {
        if (o->kind!=OBJECT_CHART || command.kind!=OBJECT_CHART || command.mark!=o->mark) { fail(error,"Choose a chart to resize."); return OBJECT_LAYOUT_INVALID; }
    } else { fail(error,"Unknown object layout command."); return OBJECT_LAYOUT_INVALID; }
    if (!objects_fits(s,(int)at,o->row,o->col,command.rows,command.columns,cells,source_rows,source_columns,error)) return OBJECT_LAYOUT_INVALID;
    if (o->kind==command.kind && o->mark==command.mark && o->rows==command.rows && o->columns==command.columns) return OBJECT_LAYOUT_NOOP;
    memcpy(items,s->items,s->count*sizeof(*items)); items[at].kind=command.kind; items[at].mark=command.mark;
    items[at].rows=command.rows; items[at].columns=command.columns; ObjectSnapshot after={items,s->count};
    if (objects_asset_measure(1,&after,s->next_id,&a)!=OBJECTS_ASSET_OK) { fail(error,"Invalid complete object layout definitions."); return OBJECT_LAYOUT_INVALID; }
    *out=(ObjectLayoutPlanInfo){a.measure.owned,a.measure.validation_scratch>b.measure.validation_scratch ? a.measure.validation_scratch : b.measure.validation_scratch}; return OBJECT_LAYOUT_READY;
}
ObjectLayoutStatus objects_layout_measure(const ObjectStore *s,uint64_t id,ObjectLayoutCommand command,const CellStore *cells,uint64_t source_rows,size_t source_columns,ObjectLayoutPlanInfo *out,char **error) {
    if (!out) return OBJECT_LAYOUT_INVALID;
    SheetObject items[DX_MAX_OBJECTS]; ObjectLayoutPlanInfo info={0};
    ObjectLayoutStatus status=project(s,id,command,cells,source_rows,source_columns,items,&info,error); if (status==OBJECT_LAYOUT_READY) *out=info; return status;
}
static ObjectLayoutResult check(ObjectLayoutContext *c,const ObjectLayoutTarget *t,char **error) {
    if (!valid_context(c) || !t) { fail(error,"Invalid layout context."); return (ObjectLayoutResult){.status=OBJECT_LAYOUT_INVALID}; }
    ObjectFenceResult f=workbook_check_object_fence(&c->command,&t->object,error);
    ObjectLayoutResult r={.status=f.status==OBJECT_FENCE_READY ? OBJECT_LAYOUT_READY : f.status==OBJECT_FENCE_STALE ? OBJECT_LAYOUT_STALE :
        f.status==OBJECT_FENCE_PROTECTED ? OBJECT_LAYOUT_PROTECTED : OBJECT_LAYOUT_INVALID,.object=f.owner,.before_revision=f.revision,.after_revision=f.revision};
    if (r.status==OBJECT_LAYOUT_READY && (*c->source_rows!=t->source_rows || *c->source_columns!=t->source_columns || *c->source_count_pending!=t->source_count_pending)) {
        r.status=OBJECT_LAYOUT_STALE; fail(error,"Source extent changed; prepare the layout again.");
    }
    return r;
}
ObjectLayoutResult workbook_apply_object_layout_recorded(ObjectLayoutContext *c,const ObjectLayoutTarget *t,ObjectLayoutCommand command,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    ObjectLayoutResult r=check(c,t,error); if (r.status!=OBJECT_LAYOUT_READY) return r;
    Workbook *b=c->command.book; Worksheet *s=resolve(b,t->object.owner.sheet_id);
    SheetObject items[DX_MAX_OBJECTS]; ObjectLayoutPlanInfo info={0};
    r.status=project(&s->objects,t->object.object,command,&s->cells,effective_rows(c),*c->source_columns,items,&info,error); if (r.status!=OBJECT_LAYOUT_READY) return r;
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available() || info.owned>c->command.plan_reserved || info.validation_scratch>c->command.plan_reserved-info.owned) {
        r.status=OBJECT_LAYOUT_LIMIT; fail(error,"Presentation revision space or admitted planning budget is exhausted."); return r;
    }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) {
        r.status=OBJECT_LAYOUT_INVALID; if (!error || !*error) fail(error,"Object history or source ownership is incomplete."); return r;
    }
    ObjectStore candidate={.items=items,.count=s->objects.count,.next_id=s->objects.next_id},copy={0};
    if (!objects_valid(&s->objects,error) || !objects_valid(&candidate,error)) { r.status=OBJECT_LAYOUT_INVALID; return r; }
    objects_copy(&copy,&candidate); ObjectSnapshot after={copy.items,copy.count}; copy.items=NULL; copy.count=0; objects_free(&copy);
    ObjectSnapshot before={s->objects.items,s->objects.count}; DefinitionDependencySelection dependencies={0}; void *ticket=NULL;
    if (definition_dependencies_select(s->id,(DefinitionDependencyView){NULL,&before},(DefinitionDependencyView){NULL,&after},&s->source_versions,s->objects.next_id,&dependencies)!=DEFINITION_DEPENDENCIES_OK) {
        r.status=OBJECT_LAYOUT_INVALID; fail(error,"The complete object source dependencies are inconsistent."); goto cancelled;
    }
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=OBJECT_LAYOUT_INVALID; fail(error,"Object event recording is incomplete."); goto cancelled; }
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,
            .expected_revision=b->command_revision,.revision=b->command_revision+1,.validation_reserved=c->command.validation_reserved,
            .view={.sheet=s->id,.object_high_water=s->objects.next_id,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&s->source_versions}};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? OBJECT_LAYOUT_INVALID : OBJECT_LAYOUT_LIMIT; if (!error || !*error) fail(error,"Object event recording is busy; no change was applied."); goto cancelled; }
        r=check(c,t,error); if (r.status!=OBJECT_LAYOUT_READY) goto cancelled;
        s=resolve(b,t->object.owner.sheet_id);
    }
    objects_adopt_definition(&s->objects,&after);
    worksheet_record(s,EDIT_OBJECTS);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b); r.status=OBJECT_LAYOUT_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket);
    objects_snapshot_free(&after); return r;
}
