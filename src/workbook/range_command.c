#include "range_command.h"
#include "history.h"
#include "mixed_range_plan.h"
#include <stdlib.h>
#include <string.h>
typedef char RangeTargetStackBound[(sizeof(RangeCommandTarget)<1536u*1024u) ? 1 : -1];
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static bool context_valid(const RangeCommandContext *c) { return c && c->primary && c->primary_epoch && (!c->source || (c->source->rows && c->source->columns && c->source->count_pending && c->source->fetch_failed && c->source->ready)); }
static SourceRangeContext source_context(const RangeCommandContext *c,const Worksheet *s) {
    const RangeSourcePolicy *p=c->source;
    return (SourceRangeContext){s,c->primary,*p->rows,*p->columns,*p->count_pending,*p->fetch_failed,*p->ready};
}
static bool source_fence(const RangeCommandContext *c,const RangeCommandTarget *t) {
    if ((c->source!=NULL)!=t->source_present || c->source!=t->source_authority) return false;
    if (!c->source) return true; const RangeSourcePolicy *p=c->source;
    return p->rows==t->source.rows && p->columns==t->source.columns && p->count_pending==t->source.count_pending && p->fetch_failed==t->source.fetch_failed && p->ready==t->source.ready &&
        *p->rows==t->source_rows && *p->columns==t->source_columns && *p->count_pending==t->count_pending && *p->fetch_failed==t->fetch_failed && *p->ready==t->result_ready;
}
static RangeCommandStatus source_status(SourceRangeStatus s) { return s==SOURCE_RANGE_READY ? RANGE_COMMAND_READY : s==SOURCE_RANGE_NOOP ? RANGE_COMMAND_NOOP : s==SOURCE_RANGE_PROTECTED ? RANGE_COMMAND_PROTECTED : s==SOURCE_RANGE_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID; }
static bool eligible(const RangeCommandContext *c,const Worksheet *s) {
    if (c->source) return true;
    if (c->primary->kind!=SOURCE_NONE || s->analysis.kind!=SOURCE_NONE || s->formats.source_aware || s->formats.scope_count || s->objects.count>DX_MAX_OBJECTS || (s->objects.count && !s->objects.items)) return false;
    for (size_t i=0;i<s->objects.count;i++) if (s->objects.items[i].kind==OBJECT_TABLE || s->objects.items[i].source_id || s->objects.items[i].source.kind!=SOURCE_NONE) return false;
    return true;
}
static bool format_shape(const FormatStore *f) { return f->count<=DX_FORMAT_PATCHES && (!f->count || f->items) && f->scope_count<=DX_FORMAT_PATCHES*DX_FORMAT_GUARDS && (!f->scope_count || f->scopes) && f->undo_count<=32 && f->redo_count<=32 && format_owned_bytes(f,false)<=DX_FORMAT_CURRENT_BYTES; }
static bool columns(RangeCommand command) { return command.operation==RANGE_OPERATION_INSERT_COLUMNS || command.operation==RANGE_OPERATION_DELETE_COLUMNS; }
static bool insert(RangeCommand command) { return command.operation==RANGE_OPERATION_INSERT_ROWS || command.operation==RANGE_OPERATION_INSERT_COLUMNS; }
static ColumnWidthCommandContext width_context(Worksheet *s,size_t reserved) { return (ColumnWidthCommandContext){&s->view,&s->width_history,&s->width_revision,reserved}; }
static WorkbookWidthContext width_fence_context(RangeCommandContext *c) { return (WorkbookWidthContext){.book=c->command.book,.request_epoch=c->command.request_epoch,.ready=c->command.ready,.read_only=c->command.read_only}; }
static bool width_fence(const Worksheet *s,const RangeCommandTarget *t,const RangeCommandContext *c) {
    const WorkbookWidthTarget *w=&t->widths;
    return c->command.book->history_branch==w->branch && worksheet_history_high_water()==w->order &&
        s->width_revision==w->width_revision && s->width_history==w->history && column_width_history_info(s->width_history).generation==w->generation &&
        s->view.widths==w->widths && s->view.columns==w->columns && s->view.column_count==w->count &&
        (!w->count || !memcmp(s->view.widths,w->values,w->count*sizeof(float)));
}
/* Build the canonical fixed-domain shifted view without allocation. The dense
   bridge then measures all independently owned journal/array coexistence. */
static ColumnWidthStatus width_axis(Worksheet *s,RangeCommand command,ColumnWidthEntry entries[DX_COLUMN_WIDTH_COLUMNS],ColumnWidthView *out,size_t *owned) {
    size_t ignored=0; ColumnWidthStatus valid=column_width_command_measure(width_context(s,0),(ColumnWidthView){0},&ignored);
    if (valid!=COLUMN_WIDTH_OK && valid!=COLUMN_WIDTH_NOOP && valid!=COLUMN_WIDTH_LIMIT) return valid;
    size_t n=0;
    for (size_t i=0;i<s->view.column_count;i++) {
        uint32_t bits=column_width_bits(s->view.widths[i]); if (bits==DX_COLUMN_WIDTH_DEFAULT_BITS) continue;
        if ((insert(command) && i==DX_COLUMN_WIDTH_COLUMNS-1 && i>=command.column) || (!insert(command) && i==command.column)) continue;
        size_t column=i>=command.column ? insert(command) ? i+1 : i-1 : i;
        entries[n++]=(ColumnWidthEntry){(uint16_t)column,bits};
    }
    *out=(ColumnWidthView){entries,n}; return column_width_command_measure(width_context(s,0),*out,owned);
}
bool workbook_prepare_range_command(RangeCommandContext *c,size_t sheet,RangeCommandTarget *out,char **error) {
    if (!context_valid(c) || !out || !c->command.book || sheet>=c->command.book->count) return fail(error,"Choose a valid manual range owner.");
    Worksheet *s=c->command.book->sheets+sheet;
    if (!eligible(c,s)) return fail(error,"Source and analytical ranges require their complete composite owner.");
    if (!format_shape(&s->formats) || s->cells.count>DX_MAX_EDIT_CELLS || (s->cells.count && !s->cells.cells)) return fail(error,"Invalid bounded manual range ownership.");
    ObjectCommandTarget owner;
    if (!workbook_prepare_object_creation_fence(&c->command,sheet,&owner,error)) return false;
    memset(out,0,sizeof(*out)); out->owner=owner; out->authority=c->primary==&s->analysis ? NULL : c->primary;
    memcpy(&out->primary,c->primary,sizeof(out->primary)); out->cached=s->analysis; out->primary_epoch=*c->primary_epoch;
    if (c->source) { out->source_authority=c->source; out->source=*c->source; out->source_present=true; out->source_rows=*c->source->rows; out->source_columns=*c->source->columns; out->count_pending=*c->source->count_pending; out->fetch_failed=*c->source->fetch_failed; out->result_ready=*c->source->ready; }
    memcpy(&out->cells,&s->cells,sizeof(out->cells)); if (s->cells.count) memcpy(out->entries,s->cells.cells,s->cells.count*sizeof(*out->entries));
    memcpy(&out->formats,&s->formats,sizeof(out->formats)); if (s->formats.count) memcpy(out->patches,s->formats.items,s->formats.count*sizeof(*out->patches));
    if (s->formats.scope_count) memcpy(out->scopes,s->formats.scopes,s->formats.scope_count*sizeof(*out->scopes));
    WorkbookWidthContext wc=width_fence_context(c);
    if (!workbook_prepare_width_command(&wc,sheet,&out->widths,error)) return false;
    return true;
}
RangeCommandStatus workbook_range_command_measure(const RangeCommandContext *c,size_t sheet,RangeCommand command,RangeCommandPlanInfo *out,char **error) {
    if (!context_valid(c) || !c->command.book || sheet>=c->command.book->count || !out) { fail(error,"Invalid manual range measurement owner."); return RANGE_COMMAND_INVALID; }
    const Worksheet *s=c->command.book->sheets+sheet;
    if (!eligible(c,s)) { fail(error,"Source and analytical ranges require their complete composite owner."); return RANGE_COMMAND_PROTECTED; }
    if (!format_shape(&s->formats)) { fail(error,"Invalid bounded format state."); return RANGE_COMMAND_INVALID; }
    if (c->source) {
        SourceRangePlanInfo source={0}; SourceRangeStatus status;
        if (command.operation==RANGE_OPERATION_PASTE) {
            if (!command.text || command.row_count || command.column_count || command.dr || command.dc || command.replace || command.range.r0 || command.range.r1 || command.range.c0 || command.range.c1) return RANGE_COMMAND_INVALID;
            status=source_range_paste_measure(source_context(c,s),command.row,command.column,command.text,&source,error);
        } else if (command.operation==RANGE_OPERATION_CLEAR) {
            if (command.text || command.row || command.row_count || command.column || command.column_count || command.dr || command.dc || command.replace) return RANGE_COMMAND_INVALID;
            status=source_range_clear_measure(source_context(c,s),command.range,&source,error);
        } else { fail(error,"Source-bearing movement and axes require their complete composite owner."); return RANGE_COMMAND_PROTECTED; }
        if (status!=SOURCE_RANGE_READY) return source_status(status);
        *out=(RangeCommandPlanInfo){source.owned+(s->cells.count+source.count)*sizeof(CellDelta),source.validation_scratch,source.rows,source.columns}; return RANGE_COMMAND_READY;
    }
    if (s->objects.count && command.operation!=RANGE_OPERATION_PASTE && command.operation!=RANGE_OPERATION_CLEAR) {
        bool move=command.operation==RANGE_OPERATION_MOVE,rows=command.operation==RANGE_OPERATION_INSERT_ROWS || command.operation==RANGE_OPERATION_DELETE_ROWS,cols=columns(command);
        if (!move && !rows && !cols) { fail(error,"Object-bearing paste/clear operations require a separate complete owner."); return RANGE_COMMAND_PROTECTED; }
        if (command.text || (move && (command.row || command.row_count || command.column || command.column_count)) || (rows && (command.column || command.column_count || command.dr || command.dc || command.replace || command.range.r0 || command.range.r1 || command.range.c0 || command.range.c1)) || (cols && (command.column>=16384 || command.column_count!=1 || command.row || command.row_count || command.dr || command.dc || command.replace || command.range.r0 || command.range.r1 || command.range.c0 || command.range.c1))) return RANGE_COMMAND_INVALID;
        MixedRangePlanInfo mixed={0}; MixedRangeStatus status=move ? mixed_range_measure(s,c->primary,command.range,command.dr,command.dc,command.replace,&mixed,error) : rows ? mixed_rows_measure(s,c->primary,command.row,command.row_count,insert(command),&mixed,error) : mixed_column_measure(s,c->primary,command.column,insert(command),&mixed,error);
        if (status!=MIXED_RANGE_READY) return status==MIXED_RANGE_NOOP ? RANGE_COMMAND_NOOP : status==MIXED_RANGE_PROTECTED ? RANGE_COMMAND_PROTECTED : status==MIXED_RANGE_OCCUPIED ? RANGE_COMMAND_OCCUPIED : status==MIXED_RANGE_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID;
        ObjectSnapshot before={s->objects.items,s->objects.count}; size_t retained=objects_snapshot_owned_bytes(&before);
        if (retained==SIZE_MAX) return RANGE_COMMAND_INVALID;
        size_t width_owned=0;
        if (cols) {
            ColumnWidthEntry entries[DX_COLUMN_WIDTH_COLUMNS]; ColumnWidthView shifted={0}; ColumnWidthStatus widths=width_axis((Worksheet *)s,command,entries,&shifted,&width_owned);
            if (widths!=COLUMN_WIDTH_OK && widths!=COLUMN_WIDTH_NOOP) return widths==COLUMN_WIDTH_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID;
        }
        *out=(RangeCommandPlanInfo){.owned=mixed.owned+retained+2*s->cells.count*sizeof(CellDelta)+width_owned,.validation_scratch=mixed.validation_scratch}; return RANGE_COMMAND_READY;
    }
    if (command.operation==RANGE_OPERATION_CLEAR) {
        if (command.text || command.row || command.row_count || command.column || command.column_count || command.dr || command.dc || command.replace) { fail(error,"Choose one finite clear range."); return RANGE_COMMAND_INVALID; }
        CellRangePlanInfo clear={0}; size_t scratch=0; SheetRange r=command.range;
        if (s->objects.count) {
            MixedClearPlanInfo mixed={0}; MixedRangeStatus status=mixed_clear_measure(s,c->primary,r,&mixed,error);
            if (status!=MIXED_RANGE_READY) return status==MIXED_RANGE_NOOP ? RANGE_COMMAND_NOOP : status==MIXED_RANGE_PROTECTED ? RANGE_COMMAND_PROTECTED : status==MIXED_RANGE_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID;
            clear=mixed.cells; scratch=mixed.validation_scratch;
        } else {
            CellRangeStatus status=cells_clear_measure(&s->cells,r.r0,r.r1,r.c0,r.c1,&clear,error);
            if (status!=CELL_RANGE_READY) return status==CELL_RANGE_NOOP ? RANGE_COMMAND_NOOP : status==CELL_RANGE_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID;
        }
        *out=(RangeCommandPlanInfo){.owned=clear.owned+(s->cells.count+clear.count)*sizeof(CellDelta),.validation_scratch=scratch}; return RANGE_COMMAND_READY;
    }
    if (command.operation==RANGE_OPERATION_PASTE) {
        if (!command.text || command.row_count || command.column_count || command.dr || command.dc || command.replace || command.range.r0 || command.range.r1 || command.range.c0 || command.range.c1) { fail(error,"Choose a complete text-only paste destination."); return RANGE_COMMAND_INVALID; }
        CellPastePlanInfo paste={0}; size_t scratch=0;
        if (s->objects.count) {
            MixedPastePlanInfo mixed={0}; MixedRangeStatus status=mixed_paste_measure(s,c->primary,command.row,command.column,command.text,&mixed,error);
            if (status!=MIXED_RANGE_READY) return status==MIXED_RANGE_NOOP ? RANGE_COMMAND_NOOP : status==MIXED_RANGE_PROTECTED ? RANGE_COMMAND_PROTECTED : status==MIXED_RANGE_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID;
            paste=mixed.cells; scratch=mixed.validation_scratch;
        } else {
            CellRangeStatus status=cells_paste_measure(&s->cells,command.row,command.column,command.text,&paste,error);
            if (status!=CELL_RANGE_READY) return status==CELL_RANGE_NOOP ? RANGE_COMMAND_NOOP : status==CELL_RANGE_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID;
        }
        *out=(RangeCommandPlanInfo){.owned=paste.owned+(s->cells.count+paste.count)*sizeof(CellDelta),.validation_scratch=scratch,.rows=paste.rows,.columns=paste.columns}; return RANGE_COMMAND_READY;
    }
    if (command.text) { fail(error,"Clipboard text belongs only to a paste operation."); return RANGE_COMMAND_INVALID; }
    CellRangePlanInfo cells={0}; SheetRange r=command.range;
    if ((unsigned)command.operation>RANGE_OPERATION_DELETE_COLUMNS || (command.operation==RANGE_OPERATION_MOVE && (command.row || command.row_count || command.column || command.column_count)) || (columns(command) && (command.column_count!=1 || command.column>=16384 || command.row || command.row_count)) || (!columns(command) && command.operation!=RANGE_OPERATION_MOVE && (command.column || command.column_count))) { fail(error,"Choose a valid manual range or row operation."); return RANGE_COMMAND_INVALID; }
    CellRangeStatus status=command.operation==RANGE_OPERATION_MOVE ? cells_range_measure(&s->cells,r.r0,r.r1,r.c0,r.c1,command.dr,command.dc,command.replace,&cells,error) :
        columns(command) ? cells_columns_measure(&s->cells,command.column,command.column_count,insert(command),&cells,error) : cells_rows_measure(&s->cells,command.row,command.row_count,insert(command),&cells,error);
    if (status!=CELL_RANGE_READY && status!=CELL_RANGE_NOOP) return status==CELL_RANGE_OCCUPIED ? RANGE_COMMAND_OCCUPIED : RANGE_COMMAND_INVALID;
    if (command.operation==RANGE_OPERATION_MOVE && !command.dr && !command.dc) return RANGE_COMMAND_NOOP;
    size_t width_owned=0; ColumnWidthStatus width_status=COLUMN_WIDTH_NOOP;
    if (columns(command)) {
        ColumnWidthEntry entries[DX_COLUMN_WIDTH_COLUMNS]; ColumnWidthView shifted={0};
        width_status=width_axis((Worksheet *)s,command,entries,&shifted,&width_owned);
        if (width_status!=COLUMN_WIDTH_OK && width_status!=COLUMN_WIDTH_NOOP) return width_status==COLUMN_WIDTH_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID;
    }
    if (status==CELL_RANGE_NOOP) {
        FormatRange from={r.r0,r.r1,r.c0,r.c1},to={(uint64_t)((int64_t)r.r0+command.dr),(uint64_t)((int64_t)r.r1+command.dr),(size_t)((int64_t)r.c0+command.dc),(size_t)((int64_t)r.c1+command.dc)};
        bool relevant=false;
        for (size_t i=0;i<s->formats.count;i++) {
            FormatRange p=s->formats.items[i].range;
            if (command.operation==RANGE_OPERATION_MOVE) relevant|=(p.r0<=from.r1 && from.r0<=p.r1 && p.c0<=from.c1 && from.c0<=p.c1) || (p.r0<=to.r1 && to.r0<=p.r1 && p.c0<=to.c1 && to.c0<=p.c1);
            else if (columns(command)) relevant|=p.c1>=command.column && (p.c0 || p.c1!=16383 || p.r0 || p.r1!=DX_SHEET_ROWS-1);
            else relevant|=p.r1>=command.row && (p.r0 || p.r1!=DX_SHEET_ROWS-1);
        }
        if (!relevant && width_status==COLUMN_WIDTH_NOOP) return RANGE_COMMAND_NOOP;
    }
    /* Same two-MiB bounded format-planner coexistence as single-object moves.
       Union of before/after sparse coordinates contains at most2*current count. */
    *out=(RangeCommandPlanInfo){.owned=cells.owned+2u*DX_FORMAT_CURRENT_BYTES+2u*s->cells.count*sizeof(CellDelta)+width_owned,.validation_scratch=DX_FORMAT_VALIDATION_BYTES}; return RANGE_COMMAND_READY;
}
static RangeCommandResult check(RangeCommandContext *c,const RangeCommandTarget *t,char **error) {
    if (!context_valid(c) || !t) { fail(error,"Invalid range command context."); return (RangeCommandResult){.status=RANGE_COMMAND_INVALID}; }
    ObjectFenceResult fence=workbook_check_object_creation_fence(&c->command,&t->owner,error);
    RangeCommandResult r={.status=fence.status==OBJECT_FENCE_READY ? RANGE_COMMAND_READY : fence.status==OBJECT_FENCE_STALE ? RANGE_COMMAND_STALE : fence.status==OBJECT_FENCE_PROTECTED ? RANGE_COMMAND_PROTECTED : RANGE_COMMAND_INVALID,.before_revision=fence.revision,.after_revision=fence.revision};
    if (r.status!=RANGE_COMMAND_READY) return r; Worksheet *s=resolve(c->command.book,t->owner.owner.sheet_id);
    if (!source_fence(c,t) || (c->source && memcmp(&s->analysis,&t->cached,sizeof(t->cached))) || !width_fence(s,t,c) || c->primary!=(t->authority ? t->authority : &s->analysis) || *c->primary_epoch!=t->primary_epoch || memcmp(c->primary,&t->primary,sizeof(t->primary)) ||
        memcmp(&s->cells,&t->cells,sizeof(t->cells)) || memcmp(&s->formats,&t->formats,sizeof(t->formats)) || s->cells.count>DX_MAX_EDIT_CELLS ||
        (s->cells.count && memcmp(s->cells.cells,t->entries,s->cells.count*sizeof(*t->entries))) || s->formats.count>DX_FORMAT_PATCHES ||
        (s->formats.count && memcmp(s->formats.items,t->patches,s->formats.count*sizeof(*t->patches))) || s->formats.scope_count>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS || (s->formats.scope_count && memcmp(s->formats.scopes,t->scopes,s->formats.scope_count*sizeof(*t->scopes)))) { r.status=RANGE_COMMAND_STALE; fail(error,"The manual range or its source/format owner changed; prepare again."); return r; }
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
static bool mixed_kind(unsigned effects,EditKind *kind) {
    if (effects&16) return worksheet_width_edit_kind(effects,kind);
    switch (effects) {
    case 2:*kind=EDIT_CELLS;break; case 4:*kind=EDIT_OBJECTS;break; case 6:*kind=EDIT_RANGE;break;
    case 8:*kind=EDIT_FORMAT;break; case 10:*kind=EDIT_CELLS_FORMAT;break; case 12:*kind=EDIT_OBJECTS_FORMAT;break; case 14:*kind=EDIT_RANGE_FORMAT;break;
    default:return false;
    }
    return true;
}
static RangeCommandResult apply_mixed(RangeCommandContext *c,const RangeCommandTarget *t,RangeCommand command,RangeCommandPlanInfo info,RangeCommandResult r,const WorkbookHistoryRangeRecorder *recorder,char **error) {
    Workbook *b=c->command.book; Worksheet *s=resolve(b,t->owner.owner.sheet_id); MixedRangePlan plan={0}; CellDelta *changes=NULL; void *ticket=NULL; ColumnWidthCommand *widths=NULL;
    MixedRangeStatus status=command.operation==RANGE_OPERATION_MOVE ? mixed_range_plan(s,c->primary,command.range,command.dr,command.dc,command.replace,info.owned,c->command.validation_reserved,&plan,error) : columns(command) ? mixed_column_plan(s,c->primary,command.column,insert(command),info.owned,c->command.validation_reserved,&plan,error) : mixed_rows_plan(s,c->primary,command.row,command.row_count,insert(command),info.owned,c->command.validation_reserved,&plan,error);
    if (status!=MIXED_RANGE_READY) { r.status=status==MIXED_RANGE_NOOP ? RANGE_COMMAND_NOOP : status==MIXED_RANGE_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID; return r; }
    r.effects=plan.effects; EditKind kind;
    if (r.effects&16) {
        ColumnWidthStatus prepared=column_width_command_prepare(width_context(s,info.owned),column_width_view(&plan.widths),&widths);
        if (prepared!=COLUMN_WIDTH_OK) { r.status=prepared==COLUMN_WIDTH_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID; goto cancel; }
    }
    if (!mixed_kind(r.effects,&kind)) { r.status=RANGE_COMMAND_INVALID; goto cancel; }
    if (b->command_revision==UINT64_MAX || ((r.effects&2) && s->cells.revision==UINT64_MAX) || ((r.effects&8) && s->formats.revision==UINT64_MAX) || !worksheet_history_order_available()) { r.status=RANGE_COMMAND_LIMIT; goto cancel; }
    FormatStore prospective=s->formats;
    if (r.effects&8) {
        prospective.items=plan.formats.items; prospective.count=plan.formats.count; prospective.scopes=plan.formats.scopes; prospective.scope_count=plan.formats.scope_count; prospective.source_aware=plan.formats.source_aware; prospective.redo_count=0;
        if (prospective.undo_count==DX_FORMAT_HISTORY) { memmove(prospective.undo,prospective.undo+1,(DX_FORMAT_HISTORY-1)*sizeof(*prospective.undo)); prospective.undo_count--; }
        prospective.undo[prospective.undo_count++]=(FormatSnapshot){s->formats.items,s->formats.count,s->formats.scopes,s->formats.scope_count,s->formats.source_aware}; prospective.revision++;
        if (!format_valid(&prospective,error)) { r.status=RANGE_COMMAND_LIMIT; goto cancel; }
    }
    size_t count=0;
    if (r.effects&2) { changes=dx_alloc((s->cells.count+plan.cells.count)*sizeof(*changes)); count=deltas(s->id,&s->cells,&plan.cells,changes); }
    ObjectSnapshot before={s->objects.items,s->objects.count};
    RangeEventView view={.sheet=s->id,.effects=r.effects,.cells=changes,.count=count};
    if (r.effects&4) {
        view.definitions=(DefinitionEventView){.sheet=s->id,.object_high_water=s->objects.next_id,.effects=r.effects&12u,.before={NULL,&before},.after={NULL,&plan.objects},.sources=&s->source_versions};
        if (r.effects&8) { view.definitions.before_formats=format_delta_store_view(&s->formats); view.definitions.after_formats=format_delta_snapshot_view(&plan.formats); }
    } else if (r.effects&8) { view.before_formats=format_delta_store_view(&s->formats); view.after_formats=format_delta_snapshot_view(&plan.formats); }
    if (widths) view.widths=column_width_command_pair(widths);
    RangeEventInfo event={0}; RangeEventStatus encoded=range_event_measure(view,c->command.validation_reserved,&event);
    if (encoded!=RANGE_EVENT_OK) { r.status=encoded==RANGE_EVENT_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID; goto cancel; }
    if (c->admit && !c->admit(c->admission_context,&prospective,info.owned,info.validation_scratch,error)) { r.status=RANGE_COMMAND_LIMIT; goto cancel; }
    unsigned effects=r.effects; r=check(c,t,error); r.effects=effects; if (r.status!=RANGE_COMMAND_READY) goto cancel; s=resolve(b,t->owner.owner.sheet_id); view.definitions.sources=(r.effects&4) ? &s->source_versions : NULL;
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=RANGE_COMMAND_INVALID; goto cancel; }
        WorkbookHistoryRangeRecord record={b->id,b->edit_session_id,s->edit_session_id,b->command_revision,b->command_revision+1,c->command.validation_reserved,view};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? RANGE_COMMAND_INVALID : RANGE_COMMAND_LIMIT; goto cancel; }
        effects=r.effects; r=check(c,t,error); r.effects=effects; if (r.status!=RANGE_COMMAND_READY) goto cancel; s=resolve(b,t->owner.owner.sheet_id); view.definitions.sources=(r.effects&4) ? &s->source_versions : NULL;
    }
    if (widths) {
        if (column_width_command_check(width_context(s,info.owned),widths)!=COLUMN_WIDTH_OK || column_width_command_adopt(width_context(s,info.owned),&widths)!=COLUMN_WIDTH_OK) { r.status=RANGE_COMMAND_STALE; goto cancel; }
    }
    if (r.effects&2) cells_adopt_range(&s->cells,&plan.cells);
    if (r.effects&4) objects_adopt_definition(&s->objects,&plan.objects);
    if (r.effects&8) format_commit(&s->formats,&plan.formats);
    worksheet_record(s,kind); b->command_revision++; b->dirty=true; workbook_history_note_edit(b);
    r.status=RANGE_COMMAND_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    column_width_command_cancel(&widths); free(changes); mixed_range_plan_free(&plan); return r;
cancel:
    if (ticket && recorder) recorder->cancel(recorder->context,ticket);
    if (error && !*error) fail(error,"The complete mixed range was not admitted; the original state is kept.");
    column_width_command_cancel(&widths); free(changes); mixed_range_plan_free(&plan); return r;
}
static CellRangeStatus paste_plan(const RangeCommandContext *c,const Worksheet *s,RangeCommand command,RangeCommandPlanInfo info,CellSnapshot *out,char **error) {
    if (c->source) { SourceRangeStatus status=source_range_paste_plan(source_context(c,s),command.row,command.column,command.text,info.owned,c->command.validation_reserved,out,error); return status==SOURCE_RANGE_READY ? CELL_RANGE_READY : status==SOURCE_RANGE_NOOP ? CELL_RANGE_NOOP : status==SOURCE_RANGE_LIMIT ? CELL_RANGE_LIMIT : CELL_RANGE_INVALID; }
    if (!s->objects.count) return cells_paste_plan(&s->cells,command.row,command.column,command.text,info.owned,out,error);
    MixedRangeStatus status=mixed_paste_plan(s,c->primary,command.row,command.column,command.text,info.owned,c->command.validation_reserved,out,error);
    return status==MIXED_RANGE_READY ? CELL_RANGE_READY : status==MIXED_RANGE_NOOP ? CELL_RANGE_NOOP : status==MIXED_RANGE_LIMIT ? CELL_RANGE_LIMIT : CELL_RANGE_INVALID;
}
static CellRangeStatus clear_plan(const RangeCommandContext *c,const Worksheet *s,RangeCommand command,RangeCommandPlanInfo info,CellSnapshot *out,char **error) {
    SheetRange r=command.range;
    if (c->source) { SourceRangeStatus status=source_range_clear_plan(source_context(c,s),r,info.owned,c->command.validation_reserved,out,error); return status==SOURCE_RANGE_READY ? CELL_RANGE_READY : status==SOURCE_RANGE_NOOP ? CELL_RANGE_NOOP : status==SOURCE_RANGE_LIMIT ? CELL_RANGE_LIMIT : CELL_RANGE_INVALID; }
    if (!s->objects.count) return cells_clear_plan(&s->cells,r.r0,r.r1,r.c0,r.c1,info.owned,out,error);
    MixedRangeStatus status=mixed_clear_plan(s,c->primary,r,info.owned,c->command.validation_reserved,out,error);
    return status==MIXED_RANGE_READY ? CELL_RANGE_READY : status==MIXED_RANGE_NOOP ? CELL_RANGE_NOOP : status==MIXED_RANGE_LIMIT ? CELL_RANGE_LIMIT : CELL_RANGE_INVALID;
}
RangeCommandResult workbook_apply_range_command_recorded(RangeCommandContext *c,const RangeCommandTarget *t,RangeCommand command,const WorkbookHistoryRangeRecorder *recorder,char **error) {
    RangeCommandResult r=check(c,t,error); if (r.status!=RANGE_COMMAND_READY) return r;
    Workbook *b=c->command.book; Worksheet *s=resolve(b,t->owner.owner.sheet_id); RangeCommandPlanInfo info={0};
    r.status=workbook_range_command_measure(c,(size_t)(s-b->sheets),command,&info,error); if (r.status!=RANGE_COMMAND_READY) return r;
    r.rows=info.rows; r.columns=info.columns;
    if (info.owned>c->command.plan_reserved || info.validation_scratch>c->command.plan_reserved-info.owned || info.validation_scratch>c->command.validation_reserved) {
        r.status=RANGE_COMMAND_LIMIT; fail(error,"The complete range planning budget is unavailable."); return r;
    }
    if (!format_valid(&s->formats,error) || !source_versions_valid(&s->source_versions,s->objects.next_id,error) || workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID) {
        r.status=RANGE_COMMAND_INVALID; if (!error || !*error) fail(error,"The current manual range/history ownership is incomplete."); return r;
    }
    if (s->objects.count && command.operation!=RANGE_OPERATION_PASTE && command.operation!=RANGE_OPERATION_CLEAR) return apply_mixed(c,t,command,info,r,recorder,error);
    CellSnapshot after={0}; FormatSnapshot formats={0}; CellDelta *changes=NULL; void *ticket=NULL; SheetRange range=command.range;
    ColumnWidthCommand *widths=NULL;
    CellRangeStatus planned=command.operation==RANGE_OPERATION_CLEAR ? clear_plan(c,s,command,info,&after,error) : command.operation==RANGE_OPERATION_PASTE ? paste_plan(c,s,command,info,&after,error) : command.operation==RANGE_OPERATION_MOVE ? cells_range_plan(&s->cells,range.r0,range.r1,range.c0,range.c1,command.dr,command.dc,command.replace,info.owned,&after,error) :
        columns(command) ? cells_columns_plan(&s->cells,command.column,command.column_count,insert(command),info.owned,&after,error) : cells_rows_plan(&s->cells,command.row,command.row_count,insert(command),info.owned,&after,error);
    if (planned!=CELL_RANGE_READY && planned!=CELL_RANGE_NOOP) { r.status=planned==CELL_RANGE_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID; goto cancelled; }
    bool format_planned=(command.operation==RANGE_OPERATION_PASTE || command.operation==RANGE_OPERATION_CLEAR) ? true : command.operation==RANGE_OPERATION_MOVE ? format_plan_move(&s->formats,(FormatRange){range.r0,range.r1,range.c0,range.c1},command.dr,command.dc,&formats,error) :
        format_plan_axis(&s->formats,!columns(command),columns(command) ? command.column : command.row,columns(command) ? command.column_count : command.row_count,insert(command),&formats,error);
    if (!format_planned) { r.status=RANGE_COMMAND_LIMIT; goto cancelled; }
    bool changed=planned==CELL_RANGE_READY,format_changed=command.operation!=RANGE_OPERATION_PASTE && command.operation!=RANGE_OPERATION_CLEAR && !format_snapshot_matches(&s->formats,&formats);
    if (columns(command)) {
        ColumnWidthEntry entries[DX_COLUMN_WIDTH_COLUMNS]; ColumnWidthView shifted={0}; size_t ignored=0;
        ColumnWidthStatus status=width_axis(s,command,entries,&shifted,&ignored);
        if (status==COLUMN_WIDTH_OK) status=column_width_command_prepare(width_context(s,info.owned),shifted,&widths);
        if (status!=COLUMN_WIDTH_OK && status!=COLUMN_WIDTH_NOOP) { r.status=status==COLUMN_WIDTH_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID; goto cancelled; }
    }
    r.effects=(changed ? 2u : 0u)|(format_changed ? 8u : 0u)|(widths ? 16u : 0u);
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
    if (widths) view.widths=column_width_command_pair(widths);
    RangeEventInfo event={0}; RangeEventStatus encoded=range_event_measure(view,c->command.validation_reserved,&event);
    if (encoded!=RANGE_EVENT_OK) { r.status=encoded==RANGE_EVENT_LIMIT ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_INVALID; fail(error,"The complete manual range effect set is invalid."); goto cancelled; }
    if (c->admit) {
        if (!c->admit(c->admission_context,&prospective,info.owned,info.validation_scratch,error)) { r.status=RANGE_COMMAND_LIMIT; if (!error || !*error) fail(error,"Range memory admission was rejected."); goto cancelled; }
        unsigned effects=r.effects; r=check(c,t,error); r.effects=effects; r.rows=info.rows; r.columns=info.columns; if (r.status!=RANGE_COMMAND_READY) goto cancelled; s=resolve(b,t->owner.owner.sheet_id);
    }
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=RANGE_COMMAND_INVALID; fail(error,"Range event recording is incomplete."); goto cancelled; }
        WorkbookHistoryRangeRecord record={b->id,b->edit_session_id,s->edit_session_id,b->command_revision,b->command_revision+1,c->command.validation_reserved,view};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? RANGE_COMMAND_INVALID : RANGE_COMMAND_LIMIT; if (!error || !*error) fail(error,"Range event recording is busy; no change was applied."); goto cancelled; }
        unsigned effects=r.effects; r=check(c,t,error); r.effects=effects; r.rows=info.rows; r.columns=info.columns; if (r.status!=RANGE_COMMAND_READY) goto cancelled; s=resolve(b,t->owner.owner.sheet_id);
    }
    EditKind kind=changed ? format_changed ? EDIT_CELLS_FORMAT : EDIT_CELLS : EDIT_FORMAT;
    if (widths) {
        if (!worksheet_width_edit_kind(r.effects,&kind) || column_width_command_check(width_context(s,info.owned),widths)!=COLUMN_WIDTH_OK) { r.status=RANGE_COMMAND_STALE; goto cancelled; }
        if (column_width_command_adopt(width_context(s,info.owned),&widths)!=COLUMN_WIDTH_OK) { r.status=RANGE_COMMAND_STALE; goto cancelled; }
    }
    if (changed) cells_adopt_range(&s->cells,&after); if (format_changed) format_commit(&s->formats,&formats);
    worksheet_record(s,kind);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b); r.status=RANGE_COMMAND_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket); column_width_command_cancel(&widths); free(changes); cells_snapshot_free(&after); format_snapshot_free(&formats); return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket); column_width_command_cancel(&widths); free(changes); cells_snapshot_free(&after); format_snapshot_free(&formats); return r;
}
