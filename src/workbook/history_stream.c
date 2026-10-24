#include "history_stream.h"
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
WorkbookHistoryStreamStatus workbook_history_stream_new(WorkbookHistoryStreamOrigin o,WorkbookHistoryStreamOptions options,WorkbookHistoryStream **out) {
    if (!out || *out || !(o.owner.document_high|o.owner.document_low) || !o.owner.session || !o.edit_session ||
        o.origin>o.available || o.available>o.cursor || o.cursor>o.tail || o.lineage>o.lineage_high_water) return HISTORY_STREAM_INVALID;
    if (o.owner.mode!=VERSION_WORKING_HEAD) return HISTORY_STREAM_READ_ONLY;
    if (!options.entries || options.entries>DX_HISTORY_STREAM_ENTRIES || !options.bytes || options.bytes>DX_HISTORY_STREAM_BYTES) return HISTORY_STREAM_LIMIT;
    WorkbookHistoryStream *s=dx_alloc(sizeof(*s)); *s=(WorkbookHistoryStream){.state={.current=o},.options=options};
    *out=s; return HISTORY_STREAM_OK;
}
WorkbookHistoryStreamState workbook_history_stream_state(const WorkbookHistoryStream *s) { return s ? s->state : (WorkbookHistoryStreamState){0}; }
static bool reserve_cell(void *context,const CellEditRecord *r,void **ticket,char **error) {
    WorkbookHistoryStream *s=context;
    if (!s || !r || !ticket || *ticket) return fail(error,"Choose a valid empty history reservation.");
    const WorkbookHistoryStreamOrigin *o=&s->state.current; const CellEditTarget *t=&r->target;
    if (t->workbook.high!=o->owner.document_high || t->workbook.low!=o->owner.document_low || t->document!=o->edit_session ||
        t->expected_document_revision!=o->command_revision) return fail(error,"The retained history owner changed; your draft is kept.");
    if (o->command_revision==UINT64_MAX || r->revision!=o->command_revision+1 || !t->sheet ||
        r->delta.sheet!=t->sheet_id || r->delta.row!=t->row || r->delta.column!=t->column)
        return fail(error,"The cell history envelope is invalid; your draft is kept.");
    bool forked=o->cursor<o->tail;
    if (o->cursor==UINT64_MAX || o->action_high_water==UINT64_MAX || o->transition_high_water==UINT64_MAX ||
        (forked && o->lineage_high_water==UINT64_MAX)) return fail(error,"Retained history identity space is exhausted; your draft is kept.");
    size_t bytes=0;
    if (cell_delta_encoded_size(r->delta,&bytes)!=CELL_DELTA_CODEC_OK) return fail(error,"Choose a changed bounded cell payload before recording history.");
    size_t charge=sizeof(WorkbookHistoryEvent)+bytes;
    if (s->reservation || s->state.pending+(s->flight!=NULL)>=s->options.entries || charge>s->options.bytes-s->state.owned_bytes)
        return fail(error,"Retained history is busy; your draft is kept.");
    WorkbookHistoryEvent *e=dx_alloc(charge); *e=(WorkbookHistoryEvent){.owner=s,.charge=charge,
        .info={.owner=o->owner,.edit_session=o->edit_session,.revision=r->revision,
            .action=o->action_high_water+1,.transition=o->transition_high_water+1,
            .lineage=forked ? o->lineage_high_water+1 : o->lineage,.parent_lineage=o->lineage,
            .previous_position=o->cursor,.position=o->cursor+1,.forked=forked,
            .family=DX_HISTORY_CELL_FAMILY,.encoding=DX_CELL_DELTA_ENCODING,.bytes=bytes}};
    size_t written=0;
    if (cell_delta_encode(r->delta,e->data,bytes,&written)!=CELL_DELTA_CODEC_OK || written!=bytes) {
        free(e); return fail(error,"The cell history payload changed before admission.");
    }
    s->reservation=e; s->state.reserved=true; s->state.owned_bytes+=charge; *ticket=e; return true;
}
static void cancel_cell(void *context,void *ticket) {
    WorkbookHistoryStream *s=context; WorkbookHistoryEvent *e=ticket;
    assert(s && e && s->reservation==e && e->owner==s);
    s->reservation=NULL; s->state.reserved=false; s->state.owned_bytes-=e->charge; free(e);
}
static void publish_cell(void *context,void *ticket) {
    WorkbookHistoryStream *s=context; WorkbookHistoryEvent *e=ticket;
    assert(s && e && s->reservation==e && e->owner==s);
    if (s->last) s->last->next=e; else s->head=e; s->last=e;
    WorkbookHistoryStreamOrigin *o=&s->state.current;
    o->command_revision=e->info.revision; o->action_high_water=e->info.action; o->transition_high_water=e->info.transition;
    if (e->info.forked) o->lineage_high_water=e->info.lineage;
    o->lineage=e->info.lineage; o->cursor=o->tail=e->info.position;
    s->reservation=NULL; s->state.reserved=false; s->state.pending++;
}
CellEditRecorder workbook_history_stream_cell_recorder(WorkbookHistoryStream *s) { return (CellEditRecorder){reserve_cell,publish_cell,cancel_cell,s}; }
WorkbookHistoryStreamStatus workbook_history_stream_dispatch(WorkbookHistoryStream *s,WorkbookHistoryEvent **out) {
    if (!s || !out || *out) return HISTORY_STREAM_INVALID;
    if (s->flight) return HISTORY_STREAM_BUSY;
    if (!s->head) return HISTORY_STREAM_EMPTY;
    WorkbookHistoryEvent *e=s->head; s->head=e->next; e->next=NULL; if (!s->head) s->last=NULL;
    s->flight=e; s->state.pending--; s->state.in_flight=true; *out=e; return HISTORY_STREAM_OK;
}
const WorkbookHistoryEventInfo *workbook_history_event_info(const WorkbookHistoryEvent *e) { return e ? &e->info : NULL; }
const void *workbook_history_event_payload(const WorkbookHistoryEvent *e) { return e ? e->data : NULL; }
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
