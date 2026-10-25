#include "history_stream_internal.h"
#include "cell_delta_codec.h"
#include <assert.h>
#include <stdlib.h>
struct WorkbookHistoryEvent {
    WorkbookHistoryStream *owner;
    WorkbookHistoryEvent *next;
    WorkbookHistoryEventInfo info;
    size_t charge;
    unsigned char data[];
};
struct WorkbookHistoryStream {
    WorkbookHistoryStreamState state;
    WorkbookHistoryStreamOptions options;
    WorkbookHistoryEvent *head,*last,*reservation,*flight;
};
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
size_t workbook_history_stream_event_allocation(size_t bytes,size_t dependencies) {
    if (!bytes || bytes>DX_HISTORY_STREAM_BYTES || dependencies>DX_HISTORY_STREAM_BYTES-bytes ||
        bytes+dependencies>DX_HISTORY_STREAM_BYTES-sizeof(WorkbookHistoryEvent)) return SIZE_MAX;
    return sizeof(WorkbookHistoryEvent)+bytes+dependencies;
}
WorkbookHistoryStreamStatus workbook_history_stream_new(WorkbookHistoryStreamOrigin o,WorkbookHistoryStreamOptions options,WorkbookHistoryStream **out) {
    if (!out || *out || !(o.owner.document_high|o.owner.document_low) || !o.owner.session || !o.edit_session ||
        o.origin>o.available || o.available>o.cursor || o.cursor>o.tail || o.lineage>o.lineage_high_water) return HISTORY_STREAM_INVALID;
    if (o.owner.mode!=VERSION_WORKING_HEAD) return HISTORY_STREAM_READ_ONLY;
    if (!options.entries || options.entries>DX_HISTORY_STREAM_ENTRIES || !options.bytes || options.bytes>DX_HISTORY_STREAM_BYTES) return HISTORY_STREAM_LIMIT;
    WorkbookHistoryStream *s=dx_alloc(sizeof(*s)); *s=(WorkbookHistoryStream){.state={.current=o},.options=options};
    *out=s; return HISTORY_STREAM_OK;
}
WorkbookHistoryStreamState workbook_history_stream_state(const WorkbookHistoryStream *s) { return s ? s->state : (WorkbookHistoryStreamState){0}; }
bool workbook_history_stream_reserve(WorkbookHistoryStream *s,WorkbookHistoryEditFence f,
    unsigned family,unsigned encoding,size_t bytes,unsigned dependency_encoding,size_t dependency_bytes,
    size_t inactive_scopes,void **ticket,char **error) {
    if (!s || !ticket || *ticket) return fail(error,"Choose a valid empty history reservation.");
    const WorkbookHistoryStreamOrigin *o=&s->state.current;
    if (f.workbook.high!=o->owner.document_high || f.workbook.low!=o->owner.document_low || f.edit_session!=o->edit_session ||
        f.expected_revision!=o->command_revision) return fail(error,"The retained history owner changed; your draft is kept.");
    if (o->command_revision==UINT64_MAX || f.revision!=o->command_revision+1 || !family || !encoding || !bytes ||
        (!!dependency_encoding != !!dependency_bytes)) return fail(error,"The history envelope is invalid; your draft is kept.");
    bool forked=o->cursor<o->tail;
    if (o->cursor==UINT64_MAX || o->action_high_water==UINT64_MAX || o->transition_high_water==UINT64_MAX ||
        (forked && o->lineage_high_water==UINT64_MAX)) return fail(error,"Retained history identity space is exhausted; your draft is kept.");
    size_t charge=workbook_history_stream_event_allocation(bytes,dependency_bytes);
    if (charge==SIZE_MAX) return fail(error,"The history payload exceeds the staging limit.");
    if (s->reservation || s->state.pending+(s->flight!=NULL)>=s->options.entries || charge>s->options.bytes-s->state.owned_bytes)
        return fail(error,"Retained history is busy; your draft is kept.");
    WorkbookHistoryEvent *e=dx_alloc(charge); *e=(WorkbookHistoryEvent){.owner=s,.charge=charge,
        .info={.owner=o->owner,.edit_session=o->edit_session,.revision=f.revision,
            .action=o->action_high_water+1,.transition=o->transition_high_water+1,
            .lineage=forked ? o->lineage_high_water+1 : o->lineage,.parent_lineage=o->lineage,
            .previous_position=o->cursor,.position=o->cursor+1,.forked=forked,
            .family=family,.encoding=encoding,.bytes=bytes,.dependency_encoding=dependency_encoding,
            .dependency_bytes=dependency_bytes,.inactive_scopes=inactive_scopes}};
    s->reservation=e; s->state.reserved=true; s->state.owned_bytes+=charge; *ticket=e; return true;
}
void *workbook_history_reservation_payload(void *ticket) { return ((WorkbookHistoryEvent *)ticket)->data; }
void *workbook_history_reservation_dependencies(void *ticket) {
    WorkbookHistoryEvent *e=ticket; return e->data+e->info.bytes;
}
static bool reserve_cell(void *context,const CellEditRecord *r,void **ticket,char **error) {
    if (!r || !ticket || *ticket) return fail(error,"Choose a valid empty cell history reservation.");
    const CellEditTarget *t=&r->target;
    if (!t->sheet || r->delta.sheet!=t->sheet_id || r->delta.row!=t->row || r->delta.column!=t->column)
        return fail(error,"The cell history envelope is invalid; your draft is kept.");
    size_t bytes=0;
    if (cell_delta_encoded_size(r->delta,&bytes)!=CELL_DELTA_CODEC_OK) return fail(error,"Choose a changed bounded cell payload before recording history.");
    WorkbookHistoryEditFence f={t->workbook,t->document,t->expected_document_revision,r->revision};
    if (!workbook_history_stream_reserve(context,f,DX_HISTORY_CELL_FAMILY,DX_CELL_DELTA_ENCODING,bytes,0,0,0,ticket,error)) return false;
    size_t written=0;
    if (cell_delta_encode(r->delta,workbook_history_reservation_payload(*ticket),bytes,&written)!=CELL_DELTA_CODEC_OK || written!=bytes) {
        workbook_history_stream_cancel(context,*ticket); *ticket=NULL;
        return fail(error,"The cell history payload changed before admission.");
    }
    return true;
}
void workbook_history_stream_cancel(void *context,void *ticket) {
    WorkbookHistoryStream *s=context; WorkbookHistoryEvent *e=ticket;
    assert(s && e && s->reservation==e && e->owner==s);
    s->reservation=NULL; s->state.reserved=false; s->state.owned_bytes-=e->charge; free(e);
}
void workbook_history_stream_publish(void *context,void *ticket) {
    WorkbookHistoryStream *s=context; WorkbookHistoryEvent *e=ticket;
    assert(s && e && s->reservation==e && e->owner==s);
    if (s->last) s->last->next=e; else s->head=e; s->last=e;
    WorkbookHistoryStreamOrigin *o=&s->state.current;
    o->command_revision=e->info.revision; o->action_high_water=e->info.action; o->transition_high_water=e->info.transition;
    if (e->info.forked) o->lineage_high_water=e->info.lineage;
    o->lineage=e->info.lineage; o->cursor=o->tail=e->info.position;
    s->reservation=NULL; s->state.reserved=false; s->state.pending++;
}
CellEditRecorder workbook_history_stream_cell_recorder(WorkbookHistoryStream *s) { return (CellEditRecorder){reserve_cell,workbook_history_stream_publish,workbook_history_stream_cancel,s}; }
WorkbookHistoryStreamStatus workbook_history_stream_dispatch(WorkbookHistoryStream *s,WorkbookHistoryEvent **out) {
    if (!s || !out || *out) return HISTORY_STREAM_INVALID;
    if (s->flight) return HISTORY_STREAM_BUSY;
    if (!s->head) return HISTORY_STREAM_EMPTY;
    WorkbookHistoryEvent *e=s->head; s->head=e->next; e->next=NULL; if (!s->head) s->last=NULL;
    s->flight=e; s->state.pending--; s->state.in_flight=true; *out=e; return HISTORY_STREAM_OK;
}
const WorkbookHistoryEventInfo *workbook_history_event_info(const WorkbookHistoryEvent *e) { return e ? &e->info : NULL; }
const void *workbook_history_event_payload(const WorkbookHistoryEvent *e) { return e ? e->data : NULL; }
const void *workbook_history_event_dependencies(const WorkbookHistoryEvent *e) { return e && e->info.dependency_bytes ? e->data+e->info.bytes : NULL; }
static WorkbookHistoryStreamStatus finish(WorkbookHistoryStream *s,WorkbookHistoryEvent **owned,bool complete) {
    if (!s || !owned || !*owned) return HISTORY_STREAM_INVALID;
    WorkbookHistoryEvent *e=*owned;
    if (s->flight!=e || e->owner!=s) return HISTORY_STREAM_STALE;
    s->flight=NULL; s->state.in_flight=false;
    if (complete) { s->state.owned_bytes-=e->charge; free(e); }
    else { e->next=s->head; s->head=e; if (!s->last) s->last=e; s->state.pending++; }
    *owned=NULL; return HISTORY_STREAM_OK;
}
WorkbookHistoryStreamStatus workbook_history_stream_complete(WorkbookHistoryStream *s,WorkbookHistoryEvent **owned) { return finish(s,owned,true); }
WorkbookHistoryStreamStatus workbook_history_stream_retry(WorkbookHistoryStream *s,WorkbookHistoryEvent **owned) { return finish(s,owned,false); }
WorkbookHistoryStreamStatus workbook_history_stream_free(WorkbookHistoryStream **owned,bool discard) {
    if (!owned) return HISTORY_STREAM_INVALID;
    WorkbookHistoryStream *s=*owned; if (!s) return HISTORY_STREAM_OK;
    if (s->reservation || s->flight || (s->head && !discard)) return HISTORY_STREAM_BUSY;
    while (s->head) { WorkbookHistoryEvent *e=s->head; s->head=e->next; free(e); }
    free(s); *owned=NULL; return HISTORY_STREAM_OK;
}
