#include "workbook/sheet_lifecycle_command.h"
#include "workbook/lifecycle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet metadata command:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void clear(char **error) { free(*error); *error=NULL; }
typedef struct { WorkbookHistorySheetLifecycleRecorder actual; Workbook *book; uint64_t *epoch; unsigned mutation,reserved,cancelled; } Probe;
static bool reserve(void *context,const WorkbookHistorySheetLifecycleRecord *record,void **ticket,char **error) {
    Probe *p=context; p->reserved++;
    bool ok=p->actual.reserve(p->actual.context,record,ticket,error);
    if (!ok) return false;
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(*ticket);
    CHECK(info && info->family==5 && info->encoding==1 && !info->dependency_bytes && info->revision==record->expected_revision+1);
    CHECK(!memcmp(workbook_history_event_payload(*ticket),record->packet->data,record->packet->bytes));
    switch (p->mutation) {
    case 1: ++*p->epoch; break;
    case 2: ++p->book->command_revision; break;
    case 3: ++p->book->sheet_high_water; break;
    case 4: ++p->book->sheets[0].cells.revision; break;
    case 5: ++p->book->sheets[0].edit_session_id; break;
    case 6: p->book->sheets[0].name[0]='X'; break;
    case 7: p->book->active=1; break; /* navigation is allowed */
    }
    return true;
}
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancelled++; p->actual.cancel(p->actual.context,ticket); }
static WorkbookHistoryStream *stream(const Workbook *b,size_t bytes) {
    WorkbookHistoryStream *s=NULL;
    CHECK(workbook_history_stream_new((WorkbookHistoryStreamOrigin){.owner={b->id.high,b->id.low,77,VERSION_WORKING_HEAD},.edit_session=b->edit_session_id,
        .command_revision=b->command_revision,.lineage=1,.lineage_high_water=1},(WorkbookHistoryStreamOptions){4,bytes},&s)==HISTORY_STREAM_OK); return s;
}
int main(void) {
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"First",&(Analysis){0},&error)); CHECK(workbook_add(&book,"Second",&(Analysis){0},&error));
    book.active=0; book.edit_session_id=100; book.sheets[0].edit_session_id=101; book.sheets[1].edit_session_id=102;
    book.dirty=false; uint64_t epoch=7;
    SheetLifecycleCommandContext context={&book,&epoch,2u*1024u*1024u,true,false};
    SheetLifecycleCommandPlan *plan=NULL;
    SheetMetadataCommand rename={SHEET_METADATA_RENAME,book.sheets[0].id,0,"  Renamed  "};
    Workbook original=book; Worksheet originals[2]; memcpy(originals,book.sheets,sizeof(originals));
    CHECK(sheet_lifecycle_command_prepare(context,rename,&plan,&error)==SHEET_METADATA_READY && !error);
    CHECK(!memcmp(&book,&original,sizeof(book)) && !memcmp(book.sheets,originals,sizeof(originals)));
    const SheetLifecycle *view=sheet_lifecycle_command_view(plan);
    CHECK(!strcmp(view->before.sheets[0].name,"First") && !strcmp(view->after.sheets[0].name,"Renamed"));
    CHECK(view->before.sheets[0].name!=book.sheets[0].name && sheet_lifecycle_command_owned(plan)>sheet_lifecycle_command_packet(plan)->bytes);
    CHECK(sheet_lifecycle_command_check(context,plan,&error)==SHEET_METADATA_READY);
    SheetLifecycleCommandContext exact=context; size_t owned_bytes=sheet_lifecycle_command_owned(plan);
    SheetLifecycleCommandPlan *bounded=NULL; exact.plan_reserved=owned_bytes+DX_SHEET_LIFECYCLE_NAME_BYTES;
    CHECK(sheet_lifecycle_command_prepare(exact,rename,&bounded,&error)==SHEET_METADATA_LIMIT && !bounded); clear(&error);
    exact.plan_reserved++; CHECK(sheet_lifecycle_command_prepare(exact,rename,&bounded,&error)==SHEET_METADATA_READY); sheet_lifecycle_command_free(&bounded);
    book.active=1; CHECK(sheet_lifecycle_command_check(context,plan,&error)==SHEET_METADATA_READY); book.active=0;
    WorkbookHistoryStream *s=stream(&book,DX_HISTORY_STREAM_DEFAULT_BYTES); Probe probe={workbook_history_stream_sheet_lifecycle_recorder(s),&book,&epoch,0,0,0};
    WorkbookHistorySheetLifecycleRecorder recorder={reserve,cancel,&probe};
    WorkbookHistoryStreamState before=workbook_history_stream_state(s);
    CHECK(sheet_lifecycle_command_reserve(context,plan,&recorder,&error)==SHEET_METADATA_READY && sheet_lifecycle_command_reserved(plan));
    WorkbookHistoryStreamState pending=workbook_history_stream_state(s);
    CHECK(pending.reserved && !pending.pending && pending.owned_bytes>before.owned_bytes && !memcmp(&pending.current,&before.current,sizeof(before.current)));
    CHECK(!memcmp(&book,&original,sizeof(book)) && !memcmp(book.sheets,originals,sizeof(originals)));
    CHECK(sheet_lifecycle_command_reserve(context,plan,&recorder,&error)==SHEET_METADATA_BUSY && probe.reserved==1); clear(&error);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_BUSY && s);
    sheet_lifecycle_command_cancel(plan); CHECK(probe.cancelled==1 && !sheet_lifecycle_command_reserved(plan));
    pending=workbook_history_stream_state(s); CHECK(!pending.reserved && pending.owned_bytes==before.owned_bytes && !memcmp(&pending.current,&before.current,sizeof(before.current)));
    sheet_lifecycle_command_cancel(plan); CHECK(probe.cancelled==1);
    CHECK(sheet_lifecycle_command_reserve(context,plan,&recorder,&error)==SHEET_METADATA_READY);
    sheet_lifecycle_command_free(&plan); CHECK(!plan && probe.cancelled==2 && !workbook_history_stream_state(s).reserved);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    /* The typed family owner independently validates envelopes and closure,
       rather than trusting preparation merely because its bytes are canonical. */
    CHECK(sheet_lifecycle_command_prepare(context,rename,&plan,&error)==SHEET_METADATA_READY);
    s=stream(&book,DX_HISTORY_STREAM_DEFAULT_BYTES); recorder=workbook_history_stream_sheet_lifecycle_recorder(s);
    WorkbookHistorySheetLifecycleRecord record={book.id,book.edit_session_id,book.sheets[0].edit_session_id,book.sheets[0].id,0,1,sheet_lifecycle_command_packet(plan)};
    WorkbookHistorySheetLifecycleRecord changed=record; void *ticket=NULL;
    changed.workbook.low^=1; CHECK(!recorder.reserve(recorder.context,&changed,&ticket,&error) && !ticket); clear(&error);
    changed=record; changed.sheet=UINT64_MAX; CHECK(!recorder.reserve(recorder.context,&changed,&ticket,&error) && !ticket); clear(&error);
    changed=record; changed.sheet_session=0; CHECK(!recorder.reserve(recorder.context,&changed,&ticket,&error) && !ticket); clear(&error);
    changed=record; changed.edit_session++; CHECK(!recorder.reserve(recorder.context,&changed,&ticket,&error) && !ticket); clear(&error);
    changed=record; changed.revision=2; CHECK(!recorder.reserve(recorder.context,&changed,&ticket,&error) && !ticket); clear(&error);
    changed=record; changed.expected_revision=1; CHECK(!recorder.reserve(recorder.context,&changed,&ticket,&error) && !ticket); clear(&error);
    unsigned char *corrupt=dx_alloc(record.packet->bytes); memcpy(corrupt,record.packet->data,record.packet->bytes); corrupt[4]++;
    SheetLifecyclePacket corrupt_packet={corrupt,record.packet->bytes}; changed=record; changed.packet=&corrupt_packet;
    CHECK(!recorder.reserve(recorder.context,&changed,&ticket,&error) && !ticket); clear(&error); free(corrupt);
    CHECK(!workbook_history_stream_state(s).reserved && !workbook_history_stream_state(s).owned_bytes && workbook_history_stream_state(s).current.action_high_water==0);
    sheet_lifecycle_command_free(&plan); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    /* Mutation during reserve is canceled before any clocks or history change.
       Each iteration restores only its deliberately changed test fixture. */
    for (unsigned mutation=1;mutation<=7;mutation++) {
        CHECK(sheet_lifecycle_command_prepare(context,rename,&plan,&error)==SHEET_METADATA_READY);
        s=stream(&book,DX_HISTORY_STREAM_DEFAULT_BYTES); probe=(Probe){workbook_history_stream_sheet_lifecycle_recorder(s),&book,&epoch,mutation,0,0}; recorder=(WorkbookHistorySheetLifecycleRecorder){reserve,cancel,&probe};
        before=workbook_history_stream_state(s);
        SheetMetadataStatus result=sheet_lifecycle_command_reserve(context,plan,&recorder,&error);
        CHECK(result==(mutation==7 ? SHEET_METADATA_READY : SHEET_METADATA_STALE));
        if (mutation==7) { CHECK(sheet_lifecycle_command_reserved(plan)); sheet_lifecycle_command_cancel(plan); book.active=0; }
        else { CHECK(!sheet_lifecycle_command_reserved(plan) && probe.cancelled==1); clear(&error); }
        pending=workbook_history_stream_state(s); CHECK(!pending.reserved && !pending.pending && pending.owned_bytes==before.owned_bytes && !memcmp(&pending.current,&before.current,sizeof(before.current)));
        sheet_lifecycle_command_free(&plan); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
        if (mutation==1) epoch--; if (mutation==2) book.command_revision--; if (mutation==3) book.sheet_high_water--;
        if (mutation==4) book.sheets[0].cells.revision--; if (mutation==5) book.sheets[0].edit_session_id--; if (mutation==6) book.sheets[0].name[0]='F';
    }
    CHECK(sheet_lifecycle_command_prepare(context,rename,&plan,&error)==SHEET_METADATA_READY);
    s=stream(&book,1); recorder=workbook_history_stream_sheet_lifecycle_recorder(s);
    CHECK(sheet_lifecycle_command_reserve(context,plan,&recorder,&error)==SHEET_METADATA_LIMIT && !sheet_lifecycle_command_reserved(plan)); clear(&error);
    CHECK(!workbook_history_stream_state(s).reserved && workbook_history_stream_state(s).current.action_high_water==0);
    sheet_lifecycle_command_free(&plan); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    SheetMetadataCommand commands[]={{SHEET_METADATA_REORDER,book.sheets[0].id,book.sheets[1].id,NULL},{SHEET_METADATA_HIDE,book.sheets[0].id,0,NULL}};
    for (size_t i=0;i<2;i++) {
        CHECK(sheet_lifecycle_command_prepare(context,commands[i],&plan,&error)==SHEET_METADATA_READY);
        view=sheet_lifecycle_command_view(plan);
        CHECK(view->after.active==(i ? book.sheets[1].id : book.sheets[0].id));
        CHECK(i ? view->after.sheets[0].hidden : view->after.sheets[0].id==book.sheets[1].id);
        sheet_lifecycle_command_free(&plan);
    }
    book.sheets[0].hidden=true; book.active=1;
    CHECK(sheet_lifecycle_command_prepare(context,(SheetMetadataCommand){SHEET_METADATA_SHOW,book.sheets[0].id,0,NULL},&plan,&error)==SHEET_METADATA_READY);
    CHECK(!sheet_lifecycle_command_view(plan)->after.sheets[0].hidden); sheet_lifecycle_command_free(&plan);
    CHECK(sheet_lifecycle_command_prepare(context,(SheetMetadataCommand){SHEET_METADATA_HIDE,book.sheets[1].id,0,NULL},&plan,&error)==SHEET_METADATA_INVALID && !plan); clear(&error);
    book.sheets[0].hidden=false; book.active=0;
    SheetMetadataCommand noops[]={{SHEET_METADATA_RENAME,book.sheets[0].id,0," First "},{SHEET_METADATA_REORDER,book.sheets[0].id,book.sheets[0].id,NULL},
        {SHEET_METADATA_SHOW,book.sheets[0].id,0,NULL}};
    for (size_t i=0;i<3;i++) CHECK(sheet_lifecycle_command_prepare(context,noops[i],&plan,&error)==SHEET_METADATA_NOOP && !plan && !error);
    CHECK(sheet_lifecycle_command_prepare(context,(SheetMetadataCommand){SHEET_METADATA_RENAME,book.sheets[0].id,0,"sEcOnD"},&plan,&error)==SHEET_METADATA_INVALID); clear(&error);
    SheetLifecycleCommandContext blocked=context; blocked.read_only=true;
    CHECK(sheet_lifecycle_command_prepare(blocked,rename,&plan,&error)==SHEET_METADATA_PROTECTED); clear(&error);
    blocked=context; blocked.plan_reserved=1;
    CHECK(sheet_lifecycle_command_prepare(blocked,rename,&plan,&error)==SHEET_METADATA_LIMIT); clear(&error);
    book.edit_session_id=0; CHECK(sheet_lifecycle_command_prepare(context,rename,&plan,&error)==SHEET_METADATA_INVALID); clear(&error); book.edit_session_id=100;
    book.command_revision=UINT64_MAX; CHECK(sheet_lifecycle_command_prepare(context,rename,&plan,&error)==SHEET_METADATA_LIMIT); clear(&error); book.command_revision=0;
    CHECK(!memcmp(&book,&original,sizeof(book)) && !memcmp(book.sheets,originals,sizeof(originals)));
    workbook_free(&book); sheet_lifecycle_command_free(&plan); sheet_lifecycle_command_free(NULL);
    printf("PASS: %u workbook metadata preparation and family5 reservation checks; no adoption or publication\n",checks); return 0;
}
