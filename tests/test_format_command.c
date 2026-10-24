#include "workbook/format_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL format command:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static ManualFormatTarget target(Workbook *b,size_t index) {
    ManualFormatTarget t={0}; CHECK(workbook_prepare_manual_format(b,index,&t,NULL)); return t;
}
static FormatSnapshot plan(Workbook *b,size_t index,unsigned flags) {
    FormatSnapshot p={0}; CellFormat v=format_default(); v.flags=(unsigned char)flags;
    CHECK(format_plan_spatial(&b->sheets[index].formats,(FormatRange){1,1,1,1},FORMAT_BOLD|FORMAT_ITALIC,v,&p,NULL)); return p;
}
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis a={0}; CHECK(workbook_add(&b,"A",&a,NULL)); CHECK(workbook_add(&b,"B",&a,NULL)); b.dirty=false; return b;
}
static void reject(Workbook *b,ManualFormatTarget t,FormatSnapshot *p,const WorkbookHistoryFormatRecorder *recorder,ManualFormatStatus expected) {
    Workbook before=*b; Worksheet sheets[2]={b->sheets[0],b->sheets[1]}; FormatSnapshot candidate=*p; char *error=NULL;
    ManualFormatResult r=workbook_apply_manual_format_recorded(b,&t,p,recorder,&error);
    CHECK(r.status==expected && error && !memcmp(&before,b,sizeof(before)) && !memcmp(sheets,b->sheets,sizeof(sheets)) && !memcmp(&candidate,p,sizeof(candidate))); free(error);
}
static void basic_and_fences(void) {
    Workbook b=book(); ManualFormatTarget good=target(&b,0),bad; FormatSnapshot p=plan(&b,0,FORMAT_BOLD);
    bad=good; bad.owner.document++; reject(&b,bad,&p,NULL,MANUAL_FORMAT_STALE);
    bad=good; bad.owner.sheet++; reject(&b,bad,&p,NULL,MANUAL_FORMAT_STALE);
    bad=good; bad.owner.workbook.low++; reject(&b,bad,&p,NULL,MANUAL_FORMAT_STALE);
    bad=good; bad.owner.sheet_id=UINT64_MAX; reject(&b,bad,&p,NULL,MANUAL_FORMAT_STALE);
    bad=good; bad.owner.expected_revision++; reject(&b,bad,&p,NULL,MANUAL_FORMAT_STALE);
    bad=good; bad.owner.expected_document_revision++; reject(&b,bad,&p,NULL,MANUAL_FORMAT_STALE);
    bad=good; bad.expected_format_revision++; reject(&b,bad,&p,NULL,MANUAL_FORMAT_STALE);
    bad=good; bad.owner.row=1; reject(&b,bad,&p,NULL,MANUAL_FORMAT_INVALID);
    unsigned saved=p.items[0].mask; p.items[0].mask=0; reject(&b,good,&p,NULL,MANUAL_FORMAT_INVALID); p.items[0].mask=saved;
    b.sheets[0].analysis.kind=SOURCE_PARQUET; reject(&b,good,&p,NULL,MANUAL_FORMAT_PROTECTED); b.sheets[0].analysis.kind=SOURCE_NONE;
    b.sheets[0].formats.source_aware=true; reject(&b,good,&p,NULL,MANUAL_FORMAT_PROTECTED); b.sheets[0].formats.source_aware=false;
    p.source_aware=true; reject(&b,good,&p,NULL,MANUAL_FORMAT_PROTECTED); p.source_aware=false;
    ManualFormatTarget unchanged=good; CHECK(!workbook_prepare_manual_format(&b,2,&unchanged,NULL) && !memcmp(&good,&unchanged,sizeof(good)));
    CHECK(!workbook_prepare_manual_format(NULL,0,&unchanged,NULL));
    CHECK(workbook_reorder(&b,0,1,NULL));
    ManualFormatResult r=workbook_apply_manual_format_recorded(&b,&good,&p,NULL,NULL);
    CHECK(r.status==MANUAL_FORMAT_APPLIED && !p.items && b.command_revision==1 && b.dirty && b.sheets[1].undo_actions_count==1 &&
        r.before_revision==0 && r.after_revision==1 && r.before_document_revision==0 && r.after_document_revision==1);
    CHECK(format_at(&b.sheets[1].formats,1,1).flags==FORMAT_BOLD && !b.sheets[0].undo_actions_count);
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && b.command_revision==2);
    b.dirty=false; ManualFormatTarget now=target(&b,1); FormatSnapshot noop={0}; uint64_t revision=b.sheets[1].formats.revision;
    CHECK(workbook_apply_manual_format_recorded(&b,&now,&noop,NULL,NULL).status==MANUAL_FORMAT_NOOP && !noop.items && !b.dirty && b.command_revision==2 &&
        b.sheets[1].redo_actions_count==1 && b.sheets[1].formats.revision==revision);
    FormatSnapshot branch=plan(&b,0,FORMAT_ITALIC); now=target(&b,0);
    CHECK(workbook_apply_manual_format_recorded(&b,&now,&branch,NULL,NULL).status==MANUAL_FORMAT_APPLIED && !b.sheets[1].redo_actions_count && !workbook_history_available(&b,true));
    workbook_free(&b);
}
typedef struct { Workbook *book; unsigned mode,reserves,publishes,cancels; uint64_t expected; } Probe;
static bool reserve(void *context,const WorkbookHistoryFormatRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; p->reserves++; CHECK(r->effects==8 && r->expected_revision==p->book->command_revision && r->revision==p->expected+1 && !*ticket);
    CHECK(!p->book->sheets[0].formats.count && !p->book->sheets[0].undo_actions_count && !p->book->dirty);
    if (p->mode==1) return false;
    if (p->mode==2) return true;
    *ticket=dx_alloc(1);
    if (p->mode==3) { p->book->sheets[0].formats.revision++; return true; }
    if (p->mode==4) { p->book->command_revision++; return true; }
    if (p->mode==5) return false;
    return true;
}
static void publish(void *context,void *ticket) {
    Probe *p=context; p->publishes++; CHECK(ticket && p->book->dirty && p->book->command_revision==p->expected+1 &&
        p->book->sheets[0].undo_actions_count==1 && format_at(&p->book->sheets[0].formats,1,1).flags==FORMAT_BOLD); free(ticket);
}
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callback_paths(void) {
    for (unsigned mode=0;mode<=5;mode++) {
        Workbook b=book(); ManualFormatTarget t=target(&b,0); FormatSnapshot p=plan(&b,0,FORMAT_BOLD); Probe probe={.book=&b,.mode=mode,.expected=b.command_revision};
        WorkbookHistoryFormatRecorder recorder={reserve,publish,cancel,&probe}; char *error=NULL;
        ManualFormatResult r=workbook_apply_manual_format_recorded(&b,&t,&p,&recorder,&error);
        CHECK(probe.reserves==1);
        if (!mode) CHECK(r.status==MANUAL_FORMAT_APPLIED && probe.publishes==1 && !probe.cancels && !p.items && !error);
        else {
            CHECK(r.status==(mode==1 ? MANUAL_FORMAT_LIMIT : mode==2 || mode==5 ? MANUAL_FORMAT_INVALID : MANUAL_FORMAT_STALE));
            CHECK(!probe.publishes && probe.cancels==(mode>=3 ? 1u : 0u) && !b.dirty && !b.sheets[0].formats.count && !b.sheets[0].undo_actions_count && p.items && error);
        }
        free(error); format_snapshot_free(&p); workbook_free(&b);
    }
    Workbook b=book(); ManualFormatTarget t=target(&b,0); FormatSnapshot p=plan(&b,0,FORMAT_BOLD); WorkbookHistoryFormatRecorder broken={0};
    reject(&b,t,&p,&broken,MANUAL_FORMAT_INVALID); format_snapshot_free(&p);
    p=(FormatSnapshot){0}; Probe probe={.book=&b}; WorkbookHistoryFormatRecorder recorder={reserve,publish,cancel,&probe};
    CHECK(workbook_apply_manual_format_recorded(&b,&t,&p,&recorder,NULL).status==MANUAL_FORMAT_NOOP && !probe.reserves);
    b.command_revision=UINT64_MAX; t=target(&b,0); p=plan(&b,0,FORMAT_BOLD); reject(&b,t,&p,NULL,MANUAL_FORMAT_LIMIT); format_snapshot_free(&p);
    p=(FormatSnapshot){0}; CHECK(workbook_apply_manual_format_recorded(&b,&t,&p,NULL,NULL).status==MANUAL_FORMAT_NOOP);
    b.command_revision=0; b.sheets[0].formats.revision=UINT64_MAX; t=target(&b,0); p=plan(&b,0,FORMAT_BOLD);
    reject(&b,t,&p,NULL,MANUAL_FORMAT_LIMIT); format_snapshot_free(&p); workbook_free(&b);
}
static void real_stream(void) {
    Workbook b=book(); ManualFormatTarget t=target(&b,0);
    WorkbookHistoryStreamOrigin o={.owner={b.id.high,b.id.low,99,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
    WorkbookHistoryStream *s=NULL; CHECK(workbook_history_stream_new(o,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_BYTES},&s)==HISTORY_STREAM_OK);
    WorkbookHistoryFormatRecorder recorder=workbook_history_stream_format_recorder(s); WorkbookHistoryEvent *held=NULL;
    for (unsigned i=0;i<40;i++) {
        t=target(&b,0); FormatSnapshot p=plan(&b,0,i%2 ? FORMAT_ITALIC : FORMAT_BOLD);
        CHECK(workbook_apply_manual_format_recorded(&b,&t,&p,&recorder,NULL).status==MANUAL_FORMAT_APPLIED && !p.items && b.command_revision==i+1);
        if (!i) CHECK(workbook_history_stream_dispatch(s,&held)==HISTORY_STREAM_OK);
    }
    CHECK(b.sheets[0].undo_actions_count==32 && b.sheets[0].formats.undo_count==32 && workbook_history_stream_state(s).pending==39);
    FormatDelta first={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(format_delta_decode(workbook_history_event_payload(held),info->bytes,2*DX_FORMAT_CURRENT_BYTES+DX_FORMAT_VALIDATION_BYTES,&first)==FORMAT_DELTA_OK &&
        !first.before.count && first.after.items[0].value.flags==FORMAT_BOLD); format_delta_free(&first);
    CHECK(workbook_history_stream_complete(s,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_OK);
    /* Reopen/snapshot gets independent runtime identities even for the same IDs. */
    Workbook *copy=workbook_snapshot(&b); CHECK(copy); FormatSnapshot p=plan(copy,0,FORMAT_BOLD); reject(copy,t,&p,NULL,MANUAL_FORMAT_STALE);
    format_snapshot_free(&p); workbook_free(copy); free(copy); workbook_free(&b);
    b=book(); t=target(&b,0); o=(WorkbookHistoryStreamOrigin){.owner={b.id.high,b.id.low,99,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
    CHECK(workbook_history_stream_new(o,(WorkbookHistoryStreamOptions){1,1},&s)==HISTORY_STREAM_OK); recorder=workbook_history_stream_format_recorder(s);
    p=plan(&b,0,FORMAT_BOLD); reject(&b,t,&p,&recorder,MANUAL_FORMAT_LIMIT); format_snapshot_free(&p);
    CHECK(!workbook_history_stream_state(s).owned_bytes && !workbook_history_stream_state(s).current.action_high_water);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) { basic_and_fences(); callback_paths(); real_stream(); printf("PASS: %u fenced manual format command checks\n",checks); return 0; }
