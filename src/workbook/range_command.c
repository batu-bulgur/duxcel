#include "range_command.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
typedef char RangeTargetStackBound[(sizeof(RangeCommandTarget)<1536u*1024u) ? 1 : -1];
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static bool context_valid(const RangeCommandContext *c) { return c && c->primary && c->primary_epoch; }
static bool eligible(const RangeCommandContext *c,const Worksheet *s) {
    return c->primary->kind==SOURCE_NONE && s->analysis.kind==SOURCE_NONE && !s->objects.count && !s->formats.source_aware && !s->formats.scope_count;
}
static bool format_shape(const FormatStore *f) { return f->count<=DX_FORMAT_PATCHES && (!f->count || f->items) && f->undo_count<=32 && f->redo_count<=32 && format_owned_bytes(f,false)<=DX_FORMAT_CURRENT_BYTES; }
bool workbook_prepare_range_command(RangeCommandContext *c,size_t sheet,RangeCommandTarget *out,char **error) {
    if (!context_valid(c) || !out || !c->command.book || sheet>=c->command.book->count) return fail(error,"Choose a valid manual range owner.");
    Worksheet *s=c->command.book->sheets+sheet;
    if (!eligible(c,s)) return fail(error,"Source and analytical ranges require their complete composite owner.");
    if (!format_shape(&s->formats) || s->cells.count>DX_MAX_EDIT_CELLS || (s->cells.count && !s->cells.cells)) return fail(error,"Invalid bounded manual range ownership.");
    ObjectCommandTarget owner;
    if (!workbook_prepare_object_creation_fence(&c->command,sheet,&owner,error)) return false;
    memset(out,0,sizeof(*out)); out->owner=owner; out->authority=c->primary==&s->analysis ? NULL : c->primary;
    memcpy(&out->primary,c->primary,sizeof(out->primary)); out->primary_epoch=*c->primary_epoch;
    memcpy(&out->cells,&s->cells,sizeof(out->cells)); if (s->cells.count) memcpy(out->entries,s->cells.cells,s->cells.count*sizeof(*out->entries));
    memcpy(&out->formats,&s->formats,sizeof(out->formats)); if (s->formats.count) memcpy(out->patches,s->formats.items,s->formats.count*sizeof(*out->patches));
    return true;
}
RangeCommandStatus workbook_range_command_measure(const RangeCommandContext *c,size_t sheet,RangeCommand command,RangeCommandPlanInfo *out,char **error) {
    if (!context_valid(c) || !c->command.book || sheet>=c->command.book->count || !out) { fail(error,"Invalid manual range measurement owner."); return RANGE_COMMAND_INVALID; }
    const Worksheet *s=c->command.book->sheets+sheet;
    if (!eligible(c,s)) { fail(error,"Source and analytical ranges require their complete composite owner."); return RANGE_COMMAND_PROTECTED; }
    if (!format_shape(&s->formats)) { fail(error,"Invalid bounded format state."); return RANGE_COMMAND_INVALID; }
    CellRangePlanInfo cells={0}; SheetRange r=command.range;
    if ((unsigned)command.operation>RANGE_OPERATION_DELETE_ROWS || (command.operation==RANGE_OPERATION_MOVE && (command.row || command.row_count))) { fail(error,"Choose a valid manual range or row operation."); return RANGE_COMMAND_INVALID; }
    CellRangeStatus status=command.operation==RANGE_OPERATION_MOVE ? cells_range_measure(&s->cells,r.r0,r.r1,r.c0,r.c1,command.dr,command.dc,command.replace,&cells,error) :
        cells_rows_measure(&s->cells,command.row,command.row_count,command.operation==RANGE_OPERATION_INSERT_ROWS,&cells,error);
    if (status!=CELL_RANGE_READY && status!=CELL_RANGE_NOOP) return status==CELL_RANGE_OCCUPIED ? RANGE_COMMAND_OCCUPIED : RANGE_COMMAND_INVALID;
    if (command.operation==RANGE_OPERATION_MOVE && !command.dr && !command.dc) return RANGE_COMMAND_NOOP;
    if (status==CELL_RANGE_NOOP) {
        FormatRange from={r.r0,r.r1,r.c0,r.c1},to={(uint64_t)((int64_t)r.r0+command.dr),(uint64_t)((int64_t)r.r1+command.dr),(size_t)((int64_t)r.c0+command.dc),(size_t)((int64_t)r.c1+command.dc)};
        bool relevant=false;
        for (size_t i=0;i<s->formats.count;i++) {
            FormatRange p=s->formats.items[i].range;
            if (command.operation==RANGE_OPERATION_MOVE) relevant|=(p.r0<=from.r1 && from.r0<=p.r1 && p.c0<=from.c1 && from.c0<=p.c1) || (p.r0<=to.r1 && to.r0<=p.r1 && p.c0<=to.c1 && to.c0<=p.c1);
            else relevant|=p.r1>=command.row && (p.r0 || p.r1!=DX_SHEET_ROWS-1);
        }
        if (!relevant) return RANGE_COMMAND_NOOP;
    }
    /* Same two-MiB bounded format-planner coexistence as single-object moves.
       Union of before/after sparse coordinates contains at most2*current count. */
    *out=(RangeCommandPlanInfo){cells.owned+2u*DX_FORMAT_CURRENT_BYTES+2u*s->cells.count*sizeof(CellDelta),DX_FORMAT_VALIDATION_BYTES}; return RANGE_COMMAND_READY;
}
static RangeCommandResult check(RangeCommandContext *c,const RangeCommandTarget *t,char **error) {
    if (!context_valid(c) || !t) { fail(error,"Invalid range command context."); return (RangeCommandResult){.status=RANGE_COMMAND_INVALID}; }
    ObjectFenceResult fence=workbook_check_object_creation_fence(&c->command,&t->owner,error);
    RangeCommandResult r={.status=fence.status==OBJECT_FENCE_READY ? RANGE_COMMAND_READY : fence.status==OBJECT_FENCE_STALE ? RANGE_COMMAND_STALE : fence.status==OBJECT_FENCE_PROTECTED ? RANGE_COMMAND_PROTECTED : RANGE_COMMAND_INVALID,.before_revision=fence.revision,.after_revision=fence.revision};
    if (r.status!=RANGE_COMMAND_READY) return r; Worksheet *s=resolve(c->command.book,t->owner.owner.sheet_id);
    if (c->primary!=(t->authority ? t->authority : &s->analysis) || *c->primary_epoch!=t->primary_epoch || memcmp(c->primary,&t->primary,sizeof(t->primary)) ||
        memcmp(&s->cells,&t->cells,sizeof(t->cells)) || memcmp(&s->formats,&t->formats,sizeof(t->formats)) || s->cells.count>DX_MAX_EDIT_CELLS ||
        (s->cells.count && memcmp(s->cells.cells,t->entries,s->cells.count*sizeof(*t->entries))) || s->formats.count>DX_FORMAT_PATCHES ||
        (s->formats.count && memcmp(s->formats.items,t->patches,s->formats.count*sizeof(*t->patches)))) { r.status=RANGE_COMMAND_STALE; fail(error,"The manual range or its source/format owner changed; prepare again."); return r; }
    if (!eligible(c,s)) { r.status=RANGE_COMMAND_PROTECTED; fail(error,"The range now needs a source or analytical composite owner."); }
    return r;
}
static int order(const SheetCell *a,const SheetCell *b) { if (a->row!=b->row) return a->row<b->row ? -1 : 1; return (a->col>b->col)-(a->col<b->col); }
static size_t deltas(uint64_t sheet,const CellStore *before,const CellSnapshot *after,CellDelta *out) {
    size_t a=0,b=0,n=0;
    while (a<before->count || b<after->count) {
        int comparison=a==before->count ? 1 : b==after->count ? -1 : order(before->cells+a,after->cells+b);
        const SheetCell *at=comparison<=0 ? before->cells+a : after->cells+b;
        const char *old=comparison<=0 ? before->cells[a].text : "",*next=comparison>=0 ? after->cells[b].text : "";
        if (strcmp(old,next)) out[n++]=(CellDelta){sheet,at->row,at->col,old,next,strlen(old),strlen(next)};
        if (comparison<=0) a++; if (comparison>=0) b++;
    }
    return n;
}
RangeCommandResult workbook_apply_range_command_recorded(RangeCommandContext *c,const RangeCommandTarget *t,RangeCommand command,const WorkbookHistoryRangeRecorder *recorder,char **error) {
    RangeCommandResult r=check(c,t,error); if (r.status!=RANGE_COMMAND_READY) return r;
    Workbook *b=c->command.book; Worksheet *s=resolve(b,t->owner.owner.sheet_id); RangeCommandPlanInfo info={0};
    r.status=workbook_range_command_measure(c,(size_t)(s-b->sheets),command,&info,error); if (r.status!=RANGE_COMMAND_READY) return r;
    if (info.owned>c->command.plan_reserved || info.validation_scratch>c->command.plan_reserved-info.owned || info.validation_scratch>c->command.validation_reserved) {
        r.status=RANGE_COMMAND_LIMIT; fail(error,"The complete range planning budget is unavailable."); return r;
    }
    if (!format_valid(&s->formats,error) || !source_versions_valid(&s->source_versions,s->objects.next_id,error) || workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID) {
        r.status=RANGE_COMMAND_INVALID; if (!error || !*error) fail(error,"The current manual range/history ownership is incomplete."); return r;
    }
    CellSnapshot after={0}; FormatSnapshot formats={0}; CellDelta *changes=NULL; void *ticket=NULL; SheetRange range=command.range;
    CellRangeStatus planned=command.operation==RANGE_OPERATION_MOVE ? cells_range_plan(&s->cells,range.r0,range.r1,range.c0,range.c1,command.dr,command.dc,command.replace,info.owned,&after,error) :
        cells_rows_plan(&s->cells,command.row,command.row_count,command.operation==RANGE_OPERATION_INSERT_ROWS,info.owned,&after,error);
    if (planned!=CELL_RANGE_READY && planned!=CELL_RANGE_NOOP) { r.status=planned==CELL_RANGE_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID; goto cancelled; }
    bool format_planned=command.operation==RANGE_OPERATION_MOVE ? format_plan_move(&s->formats,(FormatRange){range.r0,range.r1,range.c0,range.c1},command.dr,command.dc,&formats,error) :
        format_plan_axis(&s->formats,true,command.row,command.row_count,command.operation==RANGE_OPERATION_INSERT_ROWS,&formats,error);
    if (!format_planned) { r.status=RANGE_COMMAND_LIMIT; goto cancelled; }
    bool changed=planned==CELL_RANGE_READY,format_changed=!format_snapshot_matches(&s->formats,&formats);
    r.effects=(changed ? 2u : 0u)|(format_changed ? 8u : 0u);
    if (!r.effects) { r.status=RANGE_COMMAND_NOOP; goto cancelled; }
    if (b->command_revision==UINT64_MAX || (changed && s->cells.revision==UINT64_MAX) || (format_changed && s->formats.revision==UINT64_MAX) || !worksheet_history_order_available()) {
        r.status=RANGE_COMMAND_LIMIT; fail(error,"Range revision space is exhausted."); goto cancelled;
    }
    FormatStore prospective=s->formats;
    if (format_changed) {
        prospective.items=formats.items; prospective.count=formats.count; prospective.scopes=formats.scopes; prospective.scope_count=formats.scope_count; prospective.source_aware=formats.source_aware; prospective.redo_count=0;
        if (prospective.undo_count==32) { memmove(prospective.undo,prospective.undo+1,31*sizeof(*prospective.undo)); prospective.undo_count--; }
        prospective.undo[prospective.undo_count++]=(FormatSnapshot){s->formats.items,s->formats.count,s->formats.scopes,s->formats.scope_count,s->formats.source_aware}; prospective.revision++;
        if (!format_valid(&prospective,error)) { r.status=RANGE_COMMAND_LIMIT; goto cancelled; }
    }
    size_t count=0;
    if (changed) { changes=dx_alloc((s->cells.count+after.count)*sizeof(*changes)); count=deltas(s->id,&s->cells,&after,changes); }
    RangeEventView view={.sheet=s->id,.effects=r.effects,.cells=changes,.count=count};
    if (format_changed) { view.before_formats=format_delta_store_view(&s->formats); view.after_formats=format_delta_snapshot_view(&formats); }
    RangeEventInfo event={0}; RangeEventStatus encoded=range_event_measure(view,c->command.validation_reserved,&event);
    if (encoded!=RANGE_EVENT_OK) { r.status=encoded==RANGE_EVENT_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID; fail(error,"The complete manual range effect set is invalid."); goto cancelled; }
    if (c->admit) {
        if (!c->admit(c->admission_context,&prospective,info.owned,info.validation_scratch,error)) { r.status=RANGE_COMMAND_LIMIT; if (!error || !*error) fail(error,"Range memory admission was rejected."); goto cancelled; }
        unsigned effects=r.effects; r=check(c,t,error); r.effects=effects; if (r.status!=RANGE_COMMAND_READY) goto cancelled; s=resolve(b,t->owner.owner.sheet_id);
    }
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=RANGE_COMMAND_INVALID; fail(error,"Range event recording is incomplete."); goto cancelled; }
        WorkbookHistoryRangeRecord record={b->id,b->edit_session_id,s->edit_session_id,b->command_revision,b->command_revision+1,c->command.validation_reserved,view};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? RANGE_COMMAND_INVALID : RANGE_COMMAND_LIMIT; if (!error || !*error) fail(error,"Range event recording is busy; no change was applied."); goto cancelled; }
        unsigned effects=r.effects; r=check(c,t,error); r.effects=effects; if (r.status!=RANGE_COMMAND_READY) goto cancelled; s=resolve(b,t->owner.owner.sheet_id);
    }
    if (changed) cells_adopt_range(&s->cells,&after); if (format_changed) format_commit(&s->formats,&formats);
    worksheet_record(s,changed ? format_changed ? EDIT_CELLS_FORMAT : EDIT_CELLS : EDIT_FORMAT);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b); r.status=RANGE_COMMAND_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket); free(changes); cells_snapshot_free(&after); format_snapshot_free(&formats); return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket); free(changes); cells_snapshot_free(&after); format_snapshot_free(&formats); return r;
}
