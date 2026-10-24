#include "workbook/source_format_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL source format command:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(bool table) {
    Workbook b; workbook_init(&b); Analysis a={.kind=SOURCE_CSV,.path="/source"},empty={0};
    CHECK(workbook_add(&b,"A",table ? &empty : &a,NULL));
    if (table) {
        CHECK(objects_add_table(&b.sheets[0].objects,10,2,"Raw",&a,NULL)==0); worksheet_record(b.sheets,EDIT_OBJECTS);
        CHECK(objects_add_table(&b.sheets[0].objects,40,2,"Other",&a,NULL)==1); worksheet_record(b.sheets,EDIT_OBJECTS);
    }
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static uint64_t owner(const SourceFormatCommandContext *c,bool table) { return table ? c->book->sheets[0].objects.items[0].id : 0; }
static SourceFormatCommandTarget target(SourceFormatCommandContext *c,bool table) {
    SourceFormatCommandTarget t; CHECK(workbook_prepare_source_format(c,0,owner(c,table),&t,NULL)); return t;
}
static SourceFormatCommandPlan plan(SourceFormatCommandContext *c,bool table) {
    Worksheet *s=c->book->sheets; uint64_t id=owner(c,table),version=s->source_versions.high_water+1;
    const Analysis *a=table ? &s->objects.items[0].source : c->primary;
    Column col={.name="Amount",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}};
    Page schema={.columns=&col,.column_count=1}; SourceDigest digest={.bytes=42}; digest.identity.size=42; digest.sha256[0]=1;
    SourcePacket raw={0}; CHECK(source_scope_raw_build(id,version,a,&schema,&digest,"engine",&raw,NULL));
    SourceVersion entry={.owner=id,.version=version,.kind=a->kind,.path=a->path,.raw=raw}; SourceFormatCommandPlan p={0};
    CHECK(source_versions_plan(&s->source_versions,&entry,true,&p.sources,NULL));
    uint64_t records[]={1,9007199254740993ULL}; size_t field=0; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_plan_records(&s->formats,&(FormatScopeView){id,raw.data,raw.bytes},records,2,&field,1,FORMAT_BOLD,bold,&p.formats,NULL));
    if (table) { bool changed=false; CHECK(objects_plan_source_version(&s->objects,id,0,version,&p.objects,&changed,NULL) && changed); }
    else { p.primary=analysis_copy(c->primary); p.primary.source_version=version; }
    source_packet_free(&raw); return p;
}
static void reject(SourceFormatCommandContext *c,const SourceFormatCommandTarget *t,SourceFormatCommandPlan *p,SourceFormatCommandStatus expected) {
    Workbook before=*c->book; Worksheet sheet=c->book->sheets[0]; SourceFormatCommandPlan candidate=*p; Analysis primary=*c->primary; char *error=NULL;
    SourceFormatCommandResult r=workbook_apply_source_format_recorded(c,t,p,NULL,&error);
    CHECK(r.status==expected && error && !memcmp(&before,c->book,sizeof(before)) && !memcmp(&sheet,c->book->sheets,sizeof(sheet)) &&
        !memcmp(&candidate,p,sizeof(candidate)) && !memcmp(&primary,c->primary,sizeof(primary))); free(error);
}
static void fences(bool table) {
    Workbook b=book(table); uint64_t epoch=10; SourceFormatCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false};
    SourceFormatCommandTarget good=target(&c,table),bad; SourceFormatCommandPlan p=plan(&c,table);
#define STALE(field) do { bad=good; bad.field++; reject(&c,&bad,&p,SOURCE_FORMAT_COMMAND_STALE); } while (0)
    STALE(owner.document); STALE(owner.sheet); STALE(owner.sheet_id); STALE(owner.workbook.high); STALE(owner.expected_revision); STALE(owner.expected_document_revision);
    STALE(request_epoch); STALE(format_revision); STALE(history_last); STALE(objects.next_id); STALE(sources.high_water);
    if (table) { STALE(items[1].rows); STALE(items[0].revision); }
#undef STALE
    bad=good; bad.owner.row=1; reject(&c,&bad,&p,SOURCE_FORMAT_COMMAND_INVALID);
    c.ready=false; reject(&c,&good,&p,SOURCE_FORMAT_COMMAND_PROTECTED); c.ready=true;
    c.read_only=true; reject(&c,&good,&p,SOURCE_FORMAT_COMMAND_PROTECTED); c.read_only=false;
    c.validation_reserved=0; reject(&c,&good,&p,SOURCE_FORMAT_COMMAND_LIMIT); c.validation_reserved=SIZE_MAX;
    p.sources.high_water++; reject(&c,&good,&p,SOURCE_FORMAT_COMMAND_INVALID); p.sources.high_water--;
    Analysis *next=table ? &p.objects.items[0].source : &p.primary;
    next->unique_rows=true; reject(&c,&good,&p,SOURCE_FORMAT_COMMAND_INVALID); next->unique_rows=false;
    next->source_version=0; reject(&c,&good,&p,SOURCE_FORMAT_COMMAND_INVALID); next->source_version=p.sources.high_water;
    SourcePacket raw=p.sources.items[0].raw; p.sources.items[0].raw=(SourcePacket){0}; p.sources.bytes-=raw.bytes;
    reject(&c,&good,&p,SOURCE_FORMAT_COMMAND_INVALID); p.sources.items[0].raw=raw; p.sources.bytes+=raw.bytes;
    if (table) { p.objects.items[1].row++; reject(&c,&good,&p,SOURCE_FORMAT_COMMAND_INVALID); p.objects.items[1].row--; }
    source_format_command_plan_free(&p); workbook_free(&b);
}
typedef struct { SourceFormatCommandContext *c; unsigned mode,reserves,publishes,cancels; uint64_t object; DefinitionEventPacket packet; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    Probe *p=context; Workbook *b=p->c->book; Worksheet *s=b->sheets; p->reserves++;
    CHECK(!*ticket && !b->dirty && !s->formats.count && !s->source_versions.count && r->expected_revision==0 && r->revision==1 && r->view.effects==(p->object ? 12u : 9u));
    CHECK(definition_event_capture(r->view,SIZE_MAX,&p->packet)==DEFINITION_EVENT_OK);
    (void)error;
    if (p->mode==1) return false;
    if (p->mode==2) return true;
    *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->c->request_epoch)++;
    if (p->mode==4) s->formats.revision++;
    if (p->mode==5) return false;
    if (p->mode==6) p->c->ready=false;
    if (p->mode==7) s->objects.next_id++;
    return true;
}
static void publish(void *context,void *ticket) {
    Probe *p=context; Worksheet *s=p->c->book->sheets; p->publishes++;
    const Analysis *a=p->object ? &s->objects.items[0].source : p->c->primary;
    CHECK(ticket && p->c->book->dirty && p->c->book->command_revision==1 && s->source_versions.count==1 && a->source_version==1 && s->formats.count==1 &&
        s->undo_actions[s->undo_actions_count-1]==(p->object ? EDIT_OBJECTS_FORMAT : EDIT_ANALYSIS_FORMAT)); free(ticket);
}
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(bool table) {
    for (unsigned mode=0;mode<=7;mode++) {
        Workbook b=book(table); uint64_t epoch=7; SourceFormatCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false};
        SourceFormatCommandTarget t=target(&c,table); SourceFormatCommandPlan p=plan(&c,table); size_t actions=b.sheets[0].undo_actions_count;
        Probe probe={.c=&c,.mode=mode,.object=t.object}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&probe}; char *error=NULL;
        SourceFormatCommandResult r=workbook_apply_source_format_recorded(&c,&t,&p,&recorder,&error); CHECK(probe.reserves==1);
        if (!mode) {
            CHECK(r.status==SOURCE_FORMAT_COMMAND_APPLIED && probe.publishes==1 && !probe.cancels && !p.sources.items && !p.formats.items && !error);
            DefinitionEvent event={0}; CHECK(definition_event_decode(probe.packet.data,probe.packet.bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && event.effects==(table ? 12u : 9u)); definition_event_free(&event);
        } else {
            CHECK(r.status==(mode==1 ? SOURCE_FORMAT_COMMAND_LIMIT : mode==2 || mode==5 ? SOURCE_FORMAT_COMMAND_INVALID : mode==6 ? SOURCE_FORMAT_COMMAND_PROTECTED : SOURCE_FORMAT_COMMAND_STALE));
            CHECK(!probe.publishes && probe.cancels==(mode>=3 ? 1u : 0u) && p.sources.items && p.formats.items && !b.dirty && !b.sheets[0].source_versions.count && !b.sheets[0].formats.count && b.sheets[0].undo_actions_count==actions && error);
            /* A rejected command retains its complete candidate for explicit retry. */
            if (mode==1) CHECK(workbook_apply_source_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED);
        }
        free(error); definition_event_packet_free(&probe.packet); source_format_command_plan_free(&p); workbook_free(&b);
    }
}
static void history(bool table) {
    Workbook b=book(table); uint64_t epoch=1; Analysis live=analysis_copy(&b.sheets[0].analysis);
    SourceFormatCommandContext c={&b,&live,&epoch,SIZE_MAX,true,false}; SourceFormatCommandTarget t=target(&c,table); SourceFormatCommandPlan p=plan(&c,table);
    CHECK(workbook_apply_source_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED);
    Worksheet *s=b.sheets;
    for (unsigned redo=0;redo<2;redo++) {
        CHECK(workbook_history_peek(&b,redo,NULL).status==WORKBOOK_HISTORY_READY);
        if (table) CHECK(objects_history(&s->objects,redo));
        else { CHECK(worksheet_analysis_history_preflight(s,&live,redo,NULL)); worksheet_analysis_history_adopt(s,&live,redo); }
        CHECK(format_history(&s->formats,redo)); worksheet_transfer_history(s,redo); b.command_revision++;
        const Analysis *a=table ? &s->objects.items[0].source : &live;
        CHECK(a->source_version==redo && s->formats.count==redo && s->source_versions.high_water==1 && s->source_versions.count==1 && s->source_versions.items[0].raw.bytes);
    }
    source_format_command_plan_free(&p); analysis_free(&live); workbook_free(&b);
}
static void retained_sources(bool table) {
    Workbook b=book(table); Worksheet *s=b.sheets;
    SourceVersion retired={.version=1,.kind=SOURCE_CSV,.path="/retired"}; SourceVersions initial={0};
    CHECK(source_versions_plan(&s->source_versions,&retired,true,&initial,NULL)); s->source_versions=initial; s->source_versions.high_water=5;
    uint64_t epoch=1; SourceFormatCommandContext c={&b,&s->analysis,&epoch,SIZE_MAX,true,false};
    SourceFormatCommandTarget t=target(&c,table); SourceFormatCommandPlan p=plan(&c,table);
    p.sources.items[0].path[1]='X'; reject(&c,&t,&p,SOURCE_FORMAT_COMMAND_INVALID); p.sources.items[0].path[1]='r';
    CHECK(workbook_apply_source_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED && s->source_versions.high_water==6 && s->source_versions.count==2 && !strcmp(s->source_versions.items[0].path,"/retired"));
    if (table) CHECK(objects_history(&s->objects,false));
    else { CHECK(worksheet_analysis_history_preflight(s,c.primary,false,NULL)); worksheet_analysis_history_adopt(s,c.primary,false); }
    CHECK(format_history(&s->formats,false)); worksheet_transfer_history(s,false); b.command_revision++; b.dirty=false; epoch++;
    CHECK(s->source_versions.high_water==6 && s->redo_actions_count==1 && s->formats.redo_count==1);
    t=target(&c,table); p=plan(&c,table);
    CHECK(p.sources.high_water==7 && p.sources.count==3);
    CHECK(workbook_apply_source_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED && s->source_versions.high_water==7 && s->source_versions.count==3 &&
        !s->redo_actions_count && !s->formats.redo_count && !(table ? s->objects.redo_count : s->redo_count));
    source_format_command_plan_free(&p); workbook_free(&b);
}
static void manual_policy(void) {
    for (unsigned aware=0;aware<2;aware++) {
        Workbook b=book(false); Worksheet *s=b.sheets; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
        CHECK(format_apply(&s->formats,(FormatRange){3,3,0,0},FORMAT_BOLD,bold,NULL)); worksheet_record(s,EDIT_FORMAT); workbook_history_note_edit(&b);
        s->formats.source_aware=aware; uint64_t epoch=1; SourceFormatCommandContext c={&b,&s->analysis,&epoch,SIZE_MAX,true,false};
        SourceFormatCommandTarget t=target(&c,false); SourceFormatCommandPlan p=plan(&c,false); b.dirty=false;
        if (!aware) reject(&c,&t,&p,SOURCE_FORMAT_COMMAND_PROTECTED);
        else {
            CHECK(workbook_apply_source_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED);
            CHECK((format_at(&s->formats,3,0).flags&FORMAT_BOLD) && s->formats.source_aware);
            CHECK(!(format_resolve_token(&s->formats,3,0,SIZE_MAX).flags&FORMAT_BOLD));
        }
        source_format_command_plan_free(&p); workbook_free(&b);
    }
}
static void stream_handoff(bool table) {
    for (unsigned pressure=0;pressure<2;pressure++) {
        Workbook b=book(table); uint64_t epoch=1; SourceFormatCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false};
        SourceFormatCommandTarget t=target(&c,table); SourceFormatCommandPlan p=plan(&c,table);
        WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,99,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
        WorkbookHistoryStream *stream=NULL;
        CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,pressure ? 1 : DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
        WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(stream); char *error=NULL;
        SourceFormatCommandResult r=workbook_apply_source_format_recorded(&c,&t,&p,&recorder,&error);
        if (pressure) {
            CHECK(r.status==SOURCE_FORMAT_COMMAND_LIMIT && error && !b.dirty && !b.command_revision && !b.sheets[0].source_versions.count && p.sources.items);
            WorkbookHistoryStreamState state=workbook_history_stream_state(stream); CHECK(!state.pending && !state.owned_bytes && !state.reserved && !state.current.command_revision && !state.current.action_high_water);
        } else {
            CHECK(r.status==SOURCE_FORMAT_COMMAND_APPLIED && !error && workbook_history_stream_state(stream).pending==1);
            WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK);
            const WorkbookHistoryEventInfo *info=workbook_history_event_info(event); DefinitionEvent decoded={0};
            CHECK(info->family==DX_HISTORY_DEFINITION_FAMILY && info->revision==b.command_revision &&
                definition_event_decode(workbook_history_event_payload(event),info->bytes,SIZE_MAX,&decoded)==DEFINITION_EVENT_OK && decoded.effects==(table ? 12u : 9u) && decoded.sources.versions.count==1 && decoded.formats.after.count==1);
            definition_event_free(&decoded); const WorkbookHistoryEvent *identity=event;
            CHECK(workbook_history_stream_retry(stream,&event)==HISTORY_STREAM_OK && !event);
            CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK && event==identity);
            CHECK(workbook_history_stream_complete(stream,&event)==HISTORY_STREAM_OK && !event);
        }
        free(error); CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); source_format_command_plan_free(&p); workbook_free(&b);
    }
}
static void limits(void) {
    Workbook b=book(false); uint64_t epoch=1; SourceFormatCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false};
    SourceFormatCommandTarget t=target(&c,false); SourceFormatCommandPlan p=plan(&c,false);
    b.command_revision=UINT64_MAX; t=target(&c,false); reject(&c,&t,&p,SOURCE_FORMAT_COMMAND_LIMIT); b.command_revision=0;
    b.sheets[0].formats.revision=UINT64_MAX; t=target(&c,false); reject(&c,&t,&p,SOURCE_FORMAT_COMMAND_LIMIT); b.sheets[0].formats.revision=0;
    t=target(&c,false); FormatSnapshot records=p.formats; p.formats=(FormatSnapshot){0}; reject(&c,&t,&p,SOURCE_FORMAT_COMMAND_INVALID); p.formats=records;
    SourceVersions saved=p.sources; p.sources=(SourceVersions){0}; reject(&c,&t,&p,SOURCE_FORMAT_COMMAND_INVALID); p.sources=saved;
    WorkbookHistoryDefinitionRecorder broken={0}; char *error=NULL;
    CHECK(workbook_apply_source_format_recorded(&c,&t,&p,&broken,&error).status==SOURCE_FORMAT_COMMAND_INVALID && error && p.sources.items && !b.dirty); free(error);
    source_format_command_plan_free(&p); workbook_free(&b);
}
int main(void) {
    fences(false); fences(true); callbacks(false); callbacks(true); history(false); history(true); retained_sources(false); retained_sources(true); manual_policy(); stream_handoff(false); stream_handoff(true); limits();
    source_format_command_plan_free(NULL); printf("PASS: %u atomic source format command checks\n",checks); return 0;
}
