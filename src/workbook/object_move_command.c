#include "object_move_command.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
typedef char MoveTargetStackBound[(sizeof(ObjectMoveTarget)<1024u*1024u) ? 1 : -1];
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static size_t index_of(const ObjectStore *s,uint64_t id) { size_t i=0; while (i<s->count && s->items[i].id!=id) i++; return i; }
static bool format_shape(const FormatStore *f) {
    return f && f->count<=DX_FORMAT_PATCHES && (!f->count || f->items) && f->scope_count<=DX_FORMAT_PATCHES*DX_FORMAT_GUARDS &&
        (!f->scope_count || f->scopes) && f->undo_count<=DX_FORMAT_HISTORY && f->redo_count<=DX_FORMAT_HISTORY && format_owned_bytes(f,false)<=DX_FORMAT_CURRENT_BYTES;
}
bool workbook_prepare_object_move(ObjectMoveContext *c,size_t sheet,uint64_t id,ObjectMoveTarget *out,char **error) {
    if (!c || !out || !c->layout.command.book || sheet>=c->layout.command.book->count) return fail(error,"Choose a valid move owner.");
    const FormatStore *f=&c->layout.command.book->sheets[sheet].formats;
    if (!format_shape(f)) return fail(error,"Invalid bounded format ownership.");
    ObjectLayoutTarget layout;
    if (!workbook_prepare_object_layout(&c->layout,sheet,id,&layout,error)) return false;
    memset(out,0,sizeof(*out)); out->layout=layout; memcpy(&out->formats,f,sizeof(*f));
    if (f->count) memcpy(out->patches,f->items,f->count*sizeof(*f->items));
    if (f->scope_count) memcpy(out->scopes,f->scopes,f->scope_count*sizeof(*f->scopes));
    return true;
}
static ObjectMoveStatus project(const ObjectStore *s,uint64_t id,uint64_t row,size_t col,const CellStore *cells,uint64_t rows,size_t columns,
    const FormatStore *formats,SheetObject *items,ObjectMovePlanInfo *out,char **error) {
    if (!s || !id || !out || !cells || !s->count || s->count>DX_MAX_OBJECTS || !s->items || !format_shape(formats)) { fail(error,"Invalid bounded object move."); return OBJECT_MOVE_INVALID; }
    ObjectSnapshot before={s->items,s->count}; ObjectsAssetInfo b={0},a={0};
    if (objects_asset_measure(1,&before,s->next_id,&b)!=OBJECTS_ASSET_OK) { fail(error,"Invalid bounded object definitions."); return OBJECT_MOVE_INVALID; }
    size_t at=index_of(s,id); if (at==s->count) { fail(error,"The move object is missing."); return OBJECT_MOVE_INVALID; }
    const SheetObject *o=s->items+at;
    if (!objects_fits(s,(int)at,o->row,o->col,o->rows,o->columns,cells,rows,columns,error) ||
        !objects_fits(s,(int)at,row,col,o->rows,o->columns,cells,rows,columns,error)) return OBJECT_MOVE_INVALID;
    if (row==o->row && col==o->col) return OBJECT_MOVE_NOOP;
    memcpy(items,s->items,s->count*sizeof(*items)); items[at].row=row; items[at].col=col;
    ObjectSnapshot after={items,s->count};
    if (objects_asset_measure(1,&after,s->next_id,&a)!=OBJECTS_ASSET_OK) { fail(error,"Invalid complete move definitions."); return OBJECT_MOVE_INVALID; }
    /* The move planner copies at most one <=1 MiB current state. Its <=512
       appended non-record patches/guards, compact map and realloc coexistence
       fit in a second MiB. Scope/grammar validation is separately reserved. */
    size_t scratch=a.measure.validation_scratch>b.measure.validation_scratch ? a.measure.validation_scratch : b.measure.validation_scratch;
    size_t proof=formats->scope_count ? analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true)+formats->scope_count : 0;
    if (scratch<proof) scratch=proof; if (scratch<DX_FORMAT_VALIDATION_BYTES) scratch=DX_FORMAT_VALIDATION_BYTES;
    *out=(ObjectMovePlanInfo){a.measure.owned+2u*DX_FORMAT_CURRENT_BYTES,scratch}; return OBJECT_MOVE_READY;
}
ObjectMoveStatus objects_move_measure(const ObjectStore *s,uint64_t id,uint64_t row,size_t col,const CellStore *cells,uint64_t rows,size_t columns,
    const FormatStore *formats,ObjectMovePlanInfo *out,char **error) {
    if (!out) return OBJECT_MOVE_INVALID;
    SheetObject items[DX_MAX_OBJECTS]; ObjectMovePlanInfo info={0}; ObjectMoveStatus status=project(s,id,row,col,cells,rows,columns,formats,items,&info,error);
    if (status==OBJECT_MOVE_READY) *out=info; return status;
}
static ObjectMoveResult check(ObjectMoveContext *c,const ObjectMoveTarget *t,char **error) {
    if (!c || !t || !c->layout.source_rows || !c->layout.source_columns || !c->layout.source_count_pending) { fail(error,"Invalid move context."); return (ObjectMoveResult){.status=OBJECT_MOVE_INVALID}; }
    ObjectFenceResult f=workbook_check_object_fence(&c->layout.command,&t->layout.object,error);
    ObjectMoveResult r={.status=f.status==OBJECT_FENCE_READY ? OBJECT_MOVE_READY : f.status==OBJECT_FENCE_STALE ? OBJECT_MOVE_STALE :
        f.status==OBJECT_FENCE_PROTECTED ? OBJECT_MOVE_PROTECTED : OBJECT_MOVE_INVALID,.object=f.owner,.before_revision=f.revision,.after_revision=f.revision};
    if (r.status!=OBJECT_MOVE_READY) return r;
    const FormatStore *formats=&resolve(c->layout.command.book,t->layout.object.owner.sheet_id)->formats;
    if (*c->layout.source_rows!=t->layout.source_rows || *c->layout.source_columns!=t->layout.source_columns || *c->layout.source_count_pending!=t->layout.source_count_pending ||
        memcmp(formats,&t->formats,sizeof(*formats)) || (formats->count && memcmp(formats->items,t->patches,formats->count*sizeof(*formats->items))) ||
        (formats->scope_count && memcmp(formats->scopes,t->scopes,formats->scope_count*sizeof(*formats->scopes)))) {
        r.status=OBJECT_MOVE_STALE; fail(error,"The source extent or formatting changed; prepare the move again.");
    }
    return r;
}
ObjectMoveResult workbook_apply_object_move_recorded(ObjectMoveContext *c,const ObjectMoveTarget *t,uint64_t row,size_t col,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    ObjectMoveResult r=check(c,t,error); if (r.status!=OBJECT_MOVE_READY) return r;
    ObjectCommandContext *command=&c->layout.command; Workbook *b=command->book; Worksheet *s=resolve(b,t->layout.object.owner.sheet_id);
    SheetObject items[DX_MAX_OBJECTS]; ObjectMovePlanInfo info={0};
    r.status=project(&s->objects,r.object,row,col,&s->cells,*c->layout.source_count_pending ? UINT64_MAX : *c->layout.source_rows,*c->layout.source_columns,&s->formats,items,&info,error);
    if (r.status!=OBJECT_MOVE_READY) return r;
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available() || info.owned>command->plan_reserved || info.validation_scratch>command->plan_reserved-info.owned) {
        r.status=OBJECT_MOVE_LIMIT; fail(error,"Move revision space or admitted planning budget is exhausted."); return r;
    }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID || !source_versions_valid(&s->source_versions,s->objects.next_id,error) || !format_valid(&s->formats,error)) {
        r.status=OBJECT_MOVE_INVALID; if (!error || !*error) fail(error,"Move history or source ownership is incomplete."); return r;
    }
    size_t at=index_of(&s->objects,r.object); const SheetObject *o=s->objects.items+at;
    FormatSnapshot formats={0}; ObjectSnapshot after={0}; void *ticket=NULL;
    if (!format_plan_move(&s->formats,(FormatRange){o->row,o->row+o->rows-1,o->col,o->col+o->columns-1},(int64_t)row-(int64_t)o->row,(int)col-(int)o->col,&formats,error)) { r.status=OBJECT_MOVE_LIMIT; goto cancelled; }
    bool format_changed=!format_snapshot_matches(&s->formats,&formats);
    if (format_changed && s->formats.revision==UINT64_MAX) { r.status=OBJECT_MOVE_LIMIT; fail(error,"Formatting revision space is exhausted."); goto cancelled; }
    ObjectStore candidate={.items=items,.count=s->objects.count,.next_id=s->objects.next_id},copy={0};
    if (!objects_valid(&s->objects,error) || !objects_valid(&candidate,error)) { r.status=OBJECT_MOVE_INVALID; goto cancelled; }
    objects_copy(&copy,&candidate); after=(ObjectSnapshot){copy.items,copy.count}; copy.items=NULL; copy.count=0; objects_free(&copy);
    ObjectSnapshot before={s->objects.items,s->objects.count}; unsigned effects=format_changed ? 12 : 4;
    DefinitionEventView view={.sheet=s->id,.object_high_water=s->objects.next_id,.effects=effects,.before={NULL,&before},.after={NULL,&after},.sources=&s->source_versions};
    if (format_changed) { view.before_formats=format_delta_store_view(&s->formats); view.after_formats=format_delta_snapshot_view(&formats); }
    DefinitionEventInfo event={0}; DefinitionEventStatus event_status=definition_event_measure(view,command->validation_reserved,&event);
    if (event_status!=DEFINITION_EVENT_OK) { r.status=event_status==DEFINITION_EVENT_LIMIT ? OBJECT_MOVE_LIMIT : OBJECT_MOVE_INVALID; fail(error,"The complete move and format dependencies could not be admitted."); goto cancelled; }
    if (c->admit) {
        FormatStore prospective=s->formats;
        if (format_changed) {
            prospective.items=formats.items; prospective.count=formats.count; prospective.scopes=formats.scopes; prospective.scope_count=formats.scope_count; prospective.source_aware=formats.source_aware; prospective.redo_count=0;
            if (prospective.undo_count==DX_FORMAT_HISTORY) { memmove(prospective.undo,prospective.undo+1,(DX_FORMAT_HISTORY-1)*sizeof(*prospective.undo)); prospective.undo_count--; }
            prospective.undo[prospective.undo_count++]=(FormatSnapshot){s->formats.items,s->formats.count,s->formats.scopes,s->formats.scope_count,s->formats.source_aware}; prospective.revision++;
        }
        size_t adopted=objects_snapshot_owned_bytes(&after)+format_owned_bytes(&(FormatStore){.items=formats.items,.count=formats.count,.scopes=formats.scopes,.scope_count=formats.scope_count,.source_aware=formats.source_aware},false);
        if (!c->admit(c->admission_context,&prospective,adopted,info.validation_scratch,format_changed ? objects_snapshot_owned_bytes(&before) : 0,error)) { r.status=OBJECT_MOVE_LIMIT; if (!error || !*error) fail(error,"Move memory admission was rejected."); goto cancelled; }
        r=check(c,t,error); if (r.status!=OBJECT_MOVE_READY) goto cancelled;
        s=resolve(b,t->layout.object.owner.sheet_id);
    }
    view.sources=&s->source_versions;
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=OBJECT_MOVE_INVALID; fail(error,"Move event recording is incomplete."); goto cancelled; }
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,
            .expected_revision=b->command_revision,.revision=b->command_revision+1,.validation_reserved=command->validation_reserved,.view=view};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? OBJECT_MOVE_INVALID : OBJECT_MOVE_LIMIT; if (!error || !*error) fail(error,"Move recording is busy; no change was applied."); goto cancelled; }
        r=check(c,t,error); if (r.status!=OBJECT_MOVE_READY) goto cancelled;
        s=resolve(b,t->layout.object.owner.sheet_id);
    }
    if (format_changed) format_commit(&s->formats,&formats); else format_snapshot_free(&formats);
    objects_adopt_definition(&s->objects,&after); worksheet_record(s,format_changed ? EDIT_OBJECTS_FORMAT : EDIT_OBJECTS);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b); r.status=OBJECT_MOVE_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket);
    objects_snapshot_free(&after); format_snapshot_free(&formats); return r;
}
