#include "sheet_lifecycle_command.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
    uint64_t id,session,history,cells,formats,sources,objects,titles,marks;
    size_t undo,redo;
} SheetFence;
struct SheetLifecycleCommandPlan {
    const Workbook *book;
    const Worksheet *sheets;
    const uint64_t *epoch_owner;
    const struct WorkbookLifecycle *lifecycle;
    uint64_t session,revision,epoch,branch,target,target_session;
    WorkbookHistoryResult undo,redo;
    SheetFence fences[DX_MAX_SHEETS];
    SheetLifecycle view;
    SheetLifecyclePacket packet;
    size_t owned;
    void *ticket;
    WorkbookHistorySheetLifecycleRecorder recorder;
};
static SheetMetadataStatus fail(SheetMetadataStatus status,char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return status;
}
static SheetFence fence(const Worksheet *s) {
    return (SheetFence){s->id,s->edit_session_id,s->history_last,s->cells.revision,s->formats.revision,s->source_versions.high_water,
        s->objects.next_id,s->objects.title_revision,s->objects.marks_revision,s->undo_actions_count,s->redo_actions_count};
}
static bool same_fence(SheetFence a,SheetFence b) {
    return a.id==b.id && a.session==b.session && a.history==b.history && a.cells==b.cells && a.formats==b.formats && a.sources==b.sources &&
        a.objects==b.objects && a.titles==b.titles && a.marks==b.marks && a.undo==b.undo && a.redo==b.redo;
}
static bool same_history(WorkbookHistoryResult a,WorkbookHistoryResult b) {
    return a.status==b.status && a.sheet_id==b.sheet_id && a.order==b.order && a.kind==b.kind && a.lifecycle==b.lifecycle &&
        a.column.active==b.column.active && a.column.insert==b.column.insert && a.column.column==b.column.column && a.column.width==b.column.width;
}
static SheetMetadataStatus context_check(SheetLifecycleCommandContext c,char **error) {
    if (!c.book || !c.epoch || !workbook_identity_valid(c.book) || !c.book->count || c.book->active>=c.book->count)
        return fail(SHEET_METADATA_INVALID,error,"Choose a valid workbook metadata owner.");
    if (!c.ready || c.read_only) return fail(SHEET_METADATA_PROTECTED,error,"Worksheet metadata is currently read-only or unavailable.");
    if (!c.book->edit_session_id) return fail(SHEET_METADATA_INVALID,error,"Establish the workbook command session before preparing metadata.");
    return SHEET_METADATA_READY;
}
SheetMetadataStatus sheet_lifecycle_command_check(SheetLifecycleCommandContext c,const SheetLifecycleCommandPlan *p,char **error) {
    SheetMetadataStatus status=context_check(c,error); if (status) return status;
    if (!p) return fail(SHEET_METADATA_INVALID,error,"Choose an owned sheet metadata plan.");
    const Workbook *b=c.book;
    if (b!=p->book || b->sheets!=p->sheets || c.epoch!=p->epoch_owner || *c.epoch!=p->epoch || b->edit_session_id!=p->session ||
        b->command_revision!=p->revision || b->sheet_high_water!=p->view.before.high_water || b->count!=p->view.before.count ||
        b->history_branch!=p->branch || b->lifecycle!=p->lifecycle)
        return fail(SHEET_METADATA_STALE,error,"The workbook metadata owner changed; prepare the command again.");
    SheetLifecycleEntry entries[DX_MAX_SHEETS]; SheetLifecycleState current,target;
    if (sheet_lifecycle_from_workbook(b,entries,&current)!=SHEET_LIFECYCLE_OK || sheet_lifecycle_admit(p->view,current,true,&target)!=SHEET_LIFECYCLE_OK)
        return fail(SHEET_METADATA_STALE,error,"The complete sheet metadata changed; prepare the command again.");
    for (size_t i=0;i<b->count;i++) if (!same_fence(p->fences[i],fence(b->sheets+i)))
        return fail(SHEET_METADATA_STALE,error,"A worksheet command/model fence changed; prepare again.");
    if (!same_history(p->undo,workbook_history_peek(b,false,NULL)) || !same_history(p->redo,workbook_history_peek(b,true,NULL)))
        return fail(SHEET_METADATA_STALE,error,"Workbook chronology changed; prepare the command again.");
    return SHEET_METADATA_READY;
}
SheetMetadataStatus sheet_lifecycle_command_prepare(SheetLifecycleCommandContext c,SheetMetadataCommand command,SheetLifecycleCommandPlan **out,char **error) {
    if (!out || *out || (unsigned)command.operation>SHEET_METADATA_SHOW || !command.sheet ||
        (command.operation!=SHEET_METADATA_REORDER && command.destination) || (command.operation!=SHEET_METADATA_RENAME && command.name))
        return fail(SHEET_METADATA_INVALID,error,"Choose a supported metadata command and empty plan owner.");
    SheetMetadataStatus status=context_check(c,error); if (status) return status;
    const Workbook *b=c.book; size_t at=b->count,to=b->count;
    for (size_t i=0;i<b->count;i++) { if (b->sheets[i].id==command.sheet) at=i; if (b->sheets[i].id==command.destination) to=i; }
    if (at==b->count || !b->sheets[at].edit_session_id || (command.operation==SHEET_METADATA_REORDER && to==b->count))
        return fail(SHEET_METADATA_INVALID,error,"Choose existing stable sheet IDs and an established target session.");
    SheetLifecycleEntry before[DX_MAX_SHEETS],after[DX_MAX_SHEETS]; SheetLifecycle view={0};
    if (sheet_lifecycle_from_workbook(b,before,&view.before)!=SHEET_LIFECYCLE_OK)
        return fail(SHEET_METADATA_INVALID,error,"The complete current sheet metadata is invalid.");
    memcpy(after,before,b->count*sizeof(*after)); view.after=view.before; view.after.sheets=after;
    char *name=NULL;
    if (command.operation==SHEET_METADATA_RENAME) {
        if (c.plan_reserved<sizeof(SheetLifecycleCommandPlan)+DX_SHEET_LIFECYCLE_NAME_BYTES+1u)
            return fail(SHEET_METADATA_LIMIT,error,"The admitted metadata naming budget is exhausted.");
        name=workbook_prepare_sheet_name(b,at,command.name,error); if (!name) return SHEET_METADATA_INVALID; after[at].name=name;
    } else if (command.operation==SHEET_METADATA_REORDER) {
        SheetLifecycleEntry moved=after[at];
        if (at<to) memmove(after+at,after+at+1,(to-at)*sizeof(*after));
        else if (to<at) memmove(after+to+1,after+to,(at-to)*sizeof(*after));
        after[to]=moved;
    } else {
        after[at].hidden=command.operation==SHEET_METADATA_HIDE;
        if (after[at].hidden && after[at].id==view.after.active)
            for (size_t n=1;n<b->count;n++) { size_t next=(at+n)%b->count; if (!after[next].hidden) { view.after.active=after[next].id; break; } }
    }
    SheetLifecycleInfo info; SheetLifecycleStatus measured=sheet_lifecycle_measure(view,&info);
    if (measured!=SHEET_LIFECYCLE_OK) {
        free(name); return measured==SHEET_LIFECYCLE_NOOP ? SHEET_METADATA_NOOP : fail(SHEET_METADATA_INVALID,error,"Keep a complete metadata state and at least one visible worksheet.");
    }
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available()) { free(name); return fail(SHEET_METADATA_LIMIT,error,"Workbook command identity space is exhausted."); }
    WorkbookHistoryResult undo=workbook_history_peek(b,false,error),redo=workbook_history_peek(b,true,error);
    if (undo.status==WORKBOOK_HISTORY_INVALID || redo.status==WORKBOOK_HISTORY_INVALID) { free(name); return fail(SHEET_METADATA_INVALID,error,"Workbook chronology is incomplete."); }
    size_t owned=sizeof(SheetLifecycleCommandPlan)+info.bytes+info.owned-sizeof(SheetLifecycle);
    if (owned>c.plan_reserved || (name && DX_SHEET_LIFECYCLE_NAME_BYTES+1u>c.plan_reserved-owned)) { free(name); return fail(SHEET_METADATA_LIMIT,error,"The admitted complete metadata planning budget is exhausted."); }
    SheetLifecycleCommandPlan *p=dx_alloc(sizeof(*p)); *p=(SheetLifecycleCommandPlan){.book=b,.sheets=b->sheets,.epoch_owner=c.epoch,.lifecycle=b->lifecycle,
        .session=b->edit_session_id,.revision=b->command_revision,.epoch=*c.epoch,.branch=b->history_branch,.target=command.sheet,
        .target_session=b->sheets[at].edit_session_id,.undo=undo,.redo=redo,.owned=owned};
    for (size_t i=0;i<b->count;i++) p->fences[i]=fence(b->sheets+i);
    measured=sheet_lifecycle_capture(view,info.bytes,&p->packet); free(name);
    if (!measured) measured=sheet_lifecycle_decode(p->packet.data,p->packet.bytes,info.owned,&p->view);
    if (measured) { sheet_lifecycle_command_free(&p); return fail(SHEET_METADATA_INVALID,error,"Metadata preparation failed without changing the workbook."); }
    *out=p; return SHEET_METADATA_READY;
}
SheetMetadataStatus sheet_lifecycle_command_reserve(SheetLifecycleCommandContext c,SheetLifecycleCommandPlan *p,const WorkbookHistorySheetLifecycleRecorder *recorder,char **error) {
    if (!p || !recorder || !recorder->reserve || !recorder->cancel) return fail(SHEET_METADATA_INVALID,error,"Choose a complete sheet history reservation owner.");
    if (p->ticket) return fail(SHEET_METADATA_BUSY,error,"This metadata plan already owns a reservation.");
    SheetMetadataStatus status=sheet_lifecycle_command_check(c,p,error); if (status) return status;
    WorkbookHistorySheetLifecycleRecord record={c.book->id,p->session,p->target_session,p->target,p->revision,p->revision+1,&p->packet};
    void *ticket=NULL; bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
    if (!reserved || !ticket) { if (ticket) recorder->cancel(recorder->context,ticket); return fail(SHEET_METADATA_LIMIT,error,"Retained sheet history is busy or invalid; the prepared plan is kept."); }
    status=sheet_lifecycle_command_check(c,p,error);
    if (status) { recorder->cancel(recorder->context,ticket); return status; }
    p->ticket=ticket; p->recorder=*recorder; return SHEET_METADATA_READY;
}
void sheet_lifecycle_command_cancel(SheetLifecycleCommandPlan *p) { if (p && p->ticket) { p->recorder.cancel(p->recorder.context,p->ticket); p->ticket=NULL; p->recorder=(WorkbookHistorySheetLifecycleRecorder){0}; } }
void sheet_lifecycle_command_free(SheetLifecycleCommandPlan **owned) {
    if (!owned || !*owned) return; SheetLifecycleCommandPlan *p=*owned; sheet_lifecycle_command_cancel(p);
    sheet_lifecycle_free(&p->view); sheet_lifecycle_packet_free(&p->packet); free(p); *owned=NULL;
}
const SheetLifecycle *sheet_lifecycle_command_view(const SheetLifecycleCommandPlan *p) { return p ? &p->view : NULL; }
const SheetLifecyclePacket *sheet_lifecycle_command_packet(const SheetLifecycleCommandPlan *p) { return p ? &p->packet : NULL; }
size_t sheet_lifecycle_command_owned(const SheetLifecycleCommandPlan *p) { return p ? p->owned : 0; }
bool sheet_lifecycle_command_reserved(const SheetLifecycleCommandPlan *p) { return p && p->ticket; }
