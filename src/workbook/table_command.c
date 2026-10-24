#include "table_command.h"
#include "definition_baseline.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
typedef char TableFenceStackBound[(sizeof(TableTransformTarget)<512u*1024u) ? 1 : -1];
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
    if (s->objects.count>DX_MAX_OBJECTS || (s->objects.count && !s->objects.items) || s->source_versions.count>DX_SOURCE_VERSIONS ||
        (s->source_versions.count && !s->source_versions.items)) return fail(error,"Invalid bounded Table ownership.");
    size_t at=object_index(&s->objects,object);
    if (at==s->objects.count || s->objects.items[at].kind!=OBJECT_TABLE) return fail(error,"The source Table is missing.");
    if (!c->ready || c->read_only || definition_baseline_status((DefinitionDependencyView){&s->objects.items[at].source,NULL})!=DEFINITION_BASELINE_READY)
        return fail(error,"Wait for an editable, resolved Table definition.");
    CellEditTarget owner={0}; if (!workbook_prepare_cell_edit(c->book,index,0,0,&owner,error)) return false;
    memset(out,0,sizeof(*out)); out->owner=owner; out->object=object; out->request_epoch=*c->request_epoch;
    out->format_revision=s->formats.revision; out->history_last=s->history_last;
    /* Exact native runtime fences, including padding, copied byte-for-byte.
       No old pointed-to allocation is read while checking a stale target. */
    memcpy(&out->objects,&s->objects,sizeof(s->objects)); memcpy(&out->sources,&s->source_versions,sizeof(s->source_versions));
    if (s->objects.count) memcpy(out->items,s->objects.items,s->objects.count*sizeof(*out->items));
    if (s->source_versions.count) memcpy(out->versions,s->source_versions.items,s->source_versions.count*sizeof(*out->versions));
    return true;
}
static TableTransformResult check(TableCommandContext *c,const TableTransformTarget *t,char **error) {
    TableTransformResult r={.status=TABLE_TRANSFORM_INVALID};
    if (!c || !c->request_epoch || !t || !t->object || !t->owner.document || !t->owner.sheet || t->owner.row || t->owner.column || !workbook_identity_valid(c->book)) {
        fail(error,"Invalid Table transform command."); return r;
    }
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id);
    r.owner=t->object; r.before_revision=r.after_revision=b->command_revision; r.status=TABLE_TRANSFORM_STALE;
    if (!s || b->id.high!=t->owner.workbook.high || b->id.low!=t->owner.workbook.low || b->edit_session_id!=t->owner.document || s->edit_session_id!=t->owner.sheet ||
        b->command_revision!=t->owner.expected_document_revision || s->cells.revision!=t->owner.expected_revision || s->formats.revision!=t->format_revision ||
        s->history_last!=t->history_last || *c->request_epoch!=t->request_epoch || memcmp(&s->objects,&t->objects,sizeof(s->objects)) ||
        memcmp(&s->source_versions,&t->sources,sizeof(s->source_versions))) {
        fail(error,"The Table owner changed; prepare the transformation again."); return r;
    }
    if (s->objects.count>DX_MAX_OBJECTS || s->source_versions.count>DX_SOURCE_VERSIONS ||
        (s->objects.count && (!s->objects.items || memcmp(s->objects.items,t->items,s->objects.count*sizeof(*t->items)))) ||
        (s->source_versions.count && (!s->source_versions.items || memcmp(s->source_versions.items,t->versions,s->source_versions.count*sizeof(*t->versions))))) {
        fail(error,"Table layout, definitions or source metadata changed; prepare again."); return r;
    }
    size_t at=object_index(&s->objects,t->object);
    if (at==s->objects.count || s->objects.items[at].kind!=OBJECT_TABLE) { fail(error,"The source Table changed."); return r; }
    if (!c->ready || c->read_only) { r.status=TABLE_TRANSFORM_PROTECTED; fail(error,"This Table definition is currently read-only or unavailable."); return r; }
    r.status=TABLE_TRANSFORM_READY; return r;
}
static TableTransformStatus plan_status(ObjectSourcePlanStatus status) {
    return status==OBJECT_SOURCE_PLAN_NOOP ? TABLE_TRANSFORM_NOOP : status==OBJECT_SOURCE_PLAN_LIMIT ? TABLE_TRANSFORM_LIMIT :
        status==OBJECT_SOURCE_PLAN_PROTECTED ? TABLE_TRANSFORM_PROTECTED : TABLE_TRANSFORM_INVALID;
}
TableTransformResult workbook_apply_table_transform_recorded(TableCommandContext *c,const TableTransformTarget *t,const Analysis *proposal,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    TableTransformResult r=check(c,t,error); if (r.status!=TABLE_TRANSFORM_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); ObjectSourcePlanInfo info={0};
    ObjectSourcePlanStatus status=objects_source_transform_measure(&s->objects,t->object,proposal,&info);
    if (status!=OBJECT_SOURCE_PLAN_OK) {
        r.status=plan_status(status); if (r.status!=TABLE_TRANSFORM_NOOP) fail(error,"Invalid, exhausted or changed Table connection; no transformation was applied."); return r;
    }
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available()) { r.status=TABLE_TRANSFORM_LIMIT; fail(error,"Table command revision space is exhausted."); return r; }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) {
        r.status=TABLE_TRANSFORM_INVALID; if (!error || !*error) fail(error,"Worksheet history or source ownership is incomplete."); return r;
    }
    ObjectSnapshot after={0}; status=objects_plan_source_transform(&s->objects,t->object,proposal,c->plan_reserved,&after,&info);
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
