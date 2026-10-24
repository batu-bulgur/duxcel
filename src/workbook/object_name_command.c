#include "object_name_command.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static size_t object_index(const ObjectStore *s,uint64_t id) { size_t i=0; while (i<s->count && s->items[i].id!=id) i++; return i; }
bool workbook_prepare_object_name(ObjectCommandContext *c,size_t index,uint64_t id,ObjectNameTarget *out,char **error) {
    if (!c || !c->request_epoch || !out || !id || !workbook_identity_valid(c->book) || index>=c->book->count) return fail(error,"Choose a valid object to rename.");
    Worksheet *s=c->book->sheets+index;
    if (!c->ready || c->read_only) return fail(error,"This object is currently read-only or unavailable.");
    if (s->objects.count>DX_MAX_OBJECTS || !s->objects.items) return fail(error,"Invalid object ownership.");
    size_t at=object_index(&s->objects,id);
    if (at==s->objects.count || !s->objects.items[at].name || strlen(s->objects.items[at].name)>DX_OBJECT_NAME_BYTES) return fail(error,"The named object changed.");
    CellEditTarget owner={0}; if (!workbook_prepare_cell_edit(c->book,index,0,0,&owner,error)) return false;
    memset(out,0,sizeof(*out)); out->owner=owner; out->object=id; memcpy(out->before,s->objects.items[at].name,strlen(s->objects.items[at].name)+1); return true;
}
static ObjectNameStatus project(const ObjectStore *s,uint64_t id,const char *name,SheetObject *items,char *normalized,ObjectNamePlanInfo *out,char **error) {
    if (!s || !id || !out || !name || s->count>DX_MAX_OBJECTS || !s->count || !s->items) { fail(error,"Invalid object rename plan."); return OBJECT_NAME_INVALID; }
    ObjectSnapshot before={s->items,s->count}; ObjectsAssetInfo b={0},a={0};
    if (objects_asset_measure(1,&before,s->next_id,&b)!=OBJECTS_ASSET_OK) { fail(error,"Invalid bounded object definitions."); return OBJECT_NAME_INVALID; }
    size_t at=object_index(s,id); bool changed=false;
    if (at==s->count || !objects_prepare_name(s,at,name,normalized,&changed,error)) return OBJECT_NAME_INVALID;
    if (!changed) return OBJECT_NAME_NOOP;
    if (s->title_revision==UINT64_MAX) { fail(error,"Object title revision space is exhausted."); return OBJECT_NAME_LIMIT; }
    memcpy(items,s->items,s->count*sizeof(*items)); items[at].name=normalized; ObjectSnapshot after={items,s->count};
    if (objects_asset_measure(1,&after,s->next_id,&a)!=OBJECTS_ASSET_OK) { fail(error,"The name is not valid UTF-8 or cannot fit the object definitions."); return OBJECT_NAME_INVALID; }
    *out=(ObjectNamePlanInfo){a.measure.owned,a.measure.validation_scratch>b.measure.validation_scratch ? a.measure.validation_scratch : b.measure.validation_scratch};
    return OBJECT_NAME_READY;
}
ObjectNameStatus objects_name_plan_measure(const ObjectStore *s,uint64_t id,const char *name,ObjectNamePlanInfo *out,char **error) {
    if (!out) return OBJECT_NAME_INVALID;
    SheetObject items[DX_MAX_OBJECTS]; char normalized[DX_OBJECT_NAME_BYTES+1]; ObjectNamePlanInfo info={0};
    ObjectNameStatus status=project(s,id,name,items,normalized,&info,error); if (status==OBJECT_NAME_READY) *out=info; return status;
}
static ObjectNameResult check(ObjectCommandContext *c,const ObjectNameTarget *t,char **error) {
    ObjectNameResult r={.status=OBJECT_NAME_INVALID};
    if (!c || !c->request_epoch || !t || !t->object || !t->owner.document || !t->owner.sheet || t->owner.row || t->owner.column ||
        !memchr(t->before,0,sizeof(t->before)) || !workbook_identity_valid(c->book)) { fail(error,"Invalid rename draft."); return r; }
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); r.object=t->object; r.before_revision=r.after_revision=b->command_revision; r.status=OBJECT_NAME_STALE;
    if (!s || b->id.high!=t->owner.workbook.high || b->id.low!=t->owner.workbook.low || b->edit_session_id!=t->owner.document || s->edit_session_id!=t->owner.sheet ||
        b->command_revision!=t->owner.expected_document_revision || s->cells.revision!=t->owner.expected_revision || s->objects.count>DX_MAX_OBJECTS || !s->objects.items) {
        fail(error,"The rename owner changed; your draft is kept."); return r;
    }
    size_t at=object_index(&s->objects,t->object);
    if (at==s->objects.count || !s->objects.items[at].name || strcmp(s->objects.items[at].name,t->before)) { fail(error,"The object's name changed; your draft is kept."); return r; }
    if (!c->ready || c->read_only) { r.status=OBJECT_NAME_PROTECTED; fail(error,"This object is currently read-only or unavailable."); return r; }
    r.status=OBJECT_NAME_READY; return r;
}
ObjectNameResult workbook_apply_object_name_recorded(ObjectCommandContext *c,const ObjectNameTarget *t,const char *name,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    ObjectNameResult r=check(c,t,error); if (r.status!=OBJECT_NAME_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); ObjectCommandTarget fence;
    if (!workbook_prepare_object_fence(c,(size_t)(s-b->sheets),t->object,&fence,error)) { r.status=OBJECT_NAME_INVALID; return r; }
    SheetObject items[DX_MAX_OBJECTS]; char normalized[DX_OBJECT_NAME_BYTES+1]; ObjectNamePlanInfo info={0};
    r.status=project(&s->objects,t->object,name,items,normalized,&info,error); if (r.status!=OBJECT_NAME_READY) return r;
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available() || info.owned>c->plan_reserved || info.validation_scratch>c->plan_reserved-info.owned) {
        r.status=OBJECT_NAME_LIMIT; fail(error,"Rename revision space or admitted planning budget is exhausted."); return r;
    }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) {
        r.status=OBJECT_NAME_INVALID; if (!error || !*error) fail(error,"Object history or source ownership is incomplete."); return r;
    }
    ObjectStore candidate={.items=items,.count=s->objects.count,.next_id=s->objects.next_id},copy={0};
    if (!objects_valid(&s->objects,error) || !objects_valid(&candidate,error)) { r.status=OBJECT_NAME_INVALID; return r; }
    objects_copy(&copy,&candidate); ObjectSnapshot after={copy.items,copy.count}; copy.items=NULL; copy.count=0; objects_free(&copy);
    ObjectSnapshot before={s->objects.items,s->objects.count}; DefinitionDependencySelection dependencies={0}; void *ticket=NULL;
    if (definition_dependencies_select(s->id,(DefinitionDependencyView){NULL,&before},(DefinitionDependencyView){NULL,&after},&s->source_versions,s->objects.next_id,&dependencies)!=DEFINITION_DEPENDENCIES_OK) {
        r.status=OBJECT_NAME_INVALID; fail(error,"The complete object source dependencies are inconsistent."); goto cancelled;
    }
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=OBJECT_NAME_INVALID; fail(error,"Object event recording is incomplete."); goto cancelled; }
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,
            .expected_revision=b->command_revision,.revision=b->command_revision+1,.validation_reserved=c->validation_reserved,
            .view={.sheet=s->id,.object_high_water=s->objects.next_id,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&s->source_versions}};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? OBJECT_NAME_INVALID : OBJECT_NAME_LIMIT; if (!error || !*error) fail(error,"Object event recording is busy; your draft is kept."); goto cancelled; }
        r=check(c,t,error); if (r.status!=OBJECT_NAME_READY) goto cancelled;
        ObjectFenceResult checked=workbook_check_object_fence(c,&fence,error);
        if (checked.status!=OBJECT_FENCE_READY) { r.status=checked.status==OBJECT_FENCE_PROTECTED ? OBJECT_NAME_PROTECTED : checked.status==OBJECT_FENCE_STALE ? OBJECT_NAME_STALE : OBJECT_NAME_INVALID; goto cancelled; }
        s=resolve(b,t->owner.sheet_id);
    }
    objects_adopt_definition(&s->objects,&after); s->objects.title_revision++; worksheet_record(s,EDIT_OBJECTS);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b); r.status=OBJECT_NAME_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket);
    objects_snapshot_free(&after); return r;
}
