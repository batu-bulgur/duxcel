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
static FormatSnapshot bound_plan(SourceFormatCommandContext *c,bool table,bool change) {
    Worksheet *s=c->book->sheets; uint64_t object=owner(c,table);
    if (!change) {
        FormatStore copy={0}; format_copy(&copy,&s->formats);
        return (FormatSnapshot){copy.items,copy.count,copy.scopes,copy.scope_count,copy.source_aware};
    }
    const Analysis *a=table ? &s->objects.items[0].source : c->primary;
    const SourceVersion *entry=source_versions_find(&s->source_versions,a->source_version); CHECK(entry);
    CellFormat style=format_default(); style.flags=change ? FORMAT_ITALIC : FORMAT_BOLD;
    uint64_t ids[]={1,9007199254740993ULL}; size_t field=0; FormatSnapshot p={0};
    CHECK(format_plan_records(&s->formats,&(FormatScopeView){object,entry->raw.data,entry->raw.bytes},ids,2,&field,1,change ? FORMAT_ITALIC : FORMAT_BOLD,style,&p,NULL)); return p;
}
static void bound(bool table) {
    Workbook b=book(table); Worksheet *s=b.sheets; uint64_t epoch=1; SourceFormatCommandContext c={&b,&s->analysis,&epoch,SIZE_MAX,true,false};
    SourceFormatCommandTarget t=target(&c,table); SourceFormatCommandPlan first=plan(&c,table);
    CHECK(workbook_apply_source_format_recorded(&c,&t,&first,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED);
    FormatSnapshot alias={s->formats.items,s->formats.count,s->formats.scopes,s->formats.scope_count,s->formats.source_aware};
    t=target(&c,table); CHECK(workbook_apply_bound_source_format_recorded(&c,&t,&alias,NULL,NULL).status==SOURCE_FORMAT_COMMAND_INVALID && alias.items==s->formats.items);
    SourceVersions original=s->source_versions; size_t objects_undo=s->objects.undo_count,analysis_undo=s->undo_count; b.dirty=false;
    FormatSnapshot p=bound_plan(&c,table,true); t=target(&c,table);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,99,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id,.command_revision=b.command_revision};
    WorkbookHistoryStream *stream=NULL; CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    WorkbookHistoryFormatRecorder recorder=workbook_history_stream_format_recorder(stream);
    CHECK(workbook_apply_bound_source_format_recorded(&c,&t,&p,&recorder,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED && !p.items && b.dirty && b.command_revision==2 &&
        !memcmp(&original,&s->source_versions,sizeof(original)) && s->undo_actions[s->undo_actions_count-1]==EDIT_FORMAT && s->objects.undo_count==objects_undo && s->undo_count==analysis_undo);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(event); FormatDelta delta={0};
    CHECK(info->family==DX_HISTORY_FORMAT_FAMILY && info->dependency_bytes && info->revision==2 &&
        format_delta_decode(workbook_history_event_payload(event),info->bytes,SIZE_MAX,&delta)==FORMAT_DELTA_OK);
    format_delta_free(&delta); CHECK(workbook_history_stream_complete(stream,&event)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK);
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && s->formats.redo_count==1);
    p=bound_plan(&c,table,false); t=target(&c,table); b.dirty=false; uint64_t revision=b.command_revision;
    CHECK(workbook_apply_bound_source_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_NOOP && !p.items && !b.dirty && b.command_revision==revision && s->formats.redo_count==1 && !memcmp(&original,&s->source_versions,sizeof(original)));
    CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED);
    /* Exhausted source and document counters still admit a true format no-op. */
    b.command_revision=UINT64_MAX; s->source_versions.high_water=UINT64_MAX; s->formats.revision=UINT64_MAX;
    p=bound_plan(&c,table,false); t=target(&c,table); b.dirty=false;
    CHECK(workbook_apply_bound_source_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_NOOP && !p.items && !b.dirty && b.command_revision==UINT64_MAX && s->source_versions.high_water==UINT64_MAX);
    source_format_command_plan_free(&first); workbook_free(&b);
}
typedef struct { SourceFormatCommandContext *c; unsigned mode,reserves,publishes,cancels; } BoundProbe;
static bool bound_reserve(void *context,const WorkbookHistoryFormatRecord *record,void **ticket,char **error) {
    (void)error; BoundProbe *p=context; p->reserves++; CHECK(record->effects==8 && record->sources==&p->c->book->sheets[0].source_versions && !*ticket);
    if (p->mode==1) return false;
    if (p->mode==2) return true;
    *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->c->request_epoch)++;
    if (p->mode==4) p->c->ready=false;
    if (p->mode==5) p->c->book->sheets[0].source_versions.high_water++;
    if (p->mode==6) return false;
    return true;
}
static void bound_publish(void *context,void *ticket) { BoundProbe *p=context; p->publishes++; CHECK(ticket && p->c->book->command_revision==2 && p->c->book->dirty); free(ticket); }
static void bound_cancel(void *context,void *ticket) { BoundProbe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void bound_callbacks(bool table) {
    for (unsigned mode=0;mode<=6;mode++) {
        Workbook b=book(table); uint64_t epoch=1; SourceFormatCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false};
        SourceFormatCommandTarget t=target(&c,table); SourceFormatCommandPlan first=plan(&c,table);
        CHECK(workbook_apply_source_format_recorded(&c,&t,&first,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED); b.dirty=false;
        FormatSnapshot p=bound_plan(&c,table,true); t=target(&c,table); size_t actions=b.sheets[0].undo_actions_count;
        BoundProbe probe={&c,mode,0,0,0}; WorkbookHistoryFormatRecorder recorder={bound_reserve,bound_publish,bound_cancel,&probe}; char *error=NULL;
        SourceFormatCommandResult r=workbook_apply_bound_source_format_recorded(&c,&t,&p,&recorder,&error);
        CHECK(probe.reserves==1);
        if (!mode) CHECK(r.status==SOURCE_FORMAT_COMMAND_APPLIED && probe.publishes==1 && !probe.cancels && !p.items && !error);
        else {
            CHECK(r.status==(mode==1 ? SOURCE_FORMAT_COMMAND_LIMIT : mode==2 || mode==6 ? SOURCE_FORMAT_COMMAND_INVALID : mode==4 ? SOURCE_FORMAT_COMMAND_PROTECTED : SOURCE_FORMAT_COMMAND_STALE));
            CHECK(!probe.publishes && probe.cancels==(mode>=3 ? 1u : 0u) && p.items && !b.dirty && b.command_revision==1 && b.sheets[0].undo_actions_count==actions && error);
            if (mode==1) CHECK(workbook_apply_bound_source_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED);
        }
        free(error); format_snapshot_free(&p); source_format_command_plan_free(&first); workbook_free(&b);
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

static SourceFormatCommandTarget spatial_target(SourceFormatCommandContext *c) {
    SourceFormatCommandTarget t; CHECK(workbook_prepare_source_aware_spatial_format(c,0,&t,NULL)); return t;
}
static FormatSnapshot spatial_plan(const Worksheet *s,bool broad) {
    FormatSnapshot p={0}; CellFormat style=format_default(); style.color=3; style.flags=FORMAT_ITALIC;
    CHECK(format_plan_spatial(&s->formats,broad ? (FormatRange){0,DX_SHEET_ROWS-1,0,0} : (FormatRange){20,20,20,20},
        broad ? FORMAT_COLOR : FORMAT_ITALIC,style,&p,NULL)); return p;
}
static void spatial(bool table) {
    Workbook b=book(table); Worksheet *s=b.sheets; uint64_t epoch=1;
    Analysis actual=analysis_copy(&s->analysis); SourceFormatCommandContext c={&b,&actual,&epoch,SIZE_MAX,true,false};
    SourceFormatCommandTarget t=target(&c,table); SourceFormatCommandPlan first=plan(&c,table);
    CHECK(workbook_apply_source_format_recorded(&c,&t,&first,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED);
    SourceVersions ledger=s->source_versions; Analysis primary=actual,cached=s->analysis; ObjectStore objects=s->objects;
    FormatSnapshot p=spatial_plan(s,true); t=spatial_target(&c); b.dirty=false;
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,99,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id,.command_revision=b.command_revision};
    WorkbookHistoryStream *stream=NULL; CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    WorkbookHistoryFormatRecorder recorder=workbook_history_stream_format_recorder(stream);
    CHECK(workbook_apply_source_aware_spatial_format_recorded(&c,&t,&p,&recorder,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED && !p.items && b.command_revision==2);
    CHECK(!memcmp(&ledger,&s->source_versions,sizeof(ledger)) && !memcmp(&primary,&actual,sizeof(primary)) && !memcmp(&cached,&s->analysis,sizeof(cached)) && !memcmp(&objects,&s->objects,sizeof(objects)));
    const SourceVersion *entry=s->source_versions.items; size_t raw=format_scope_token(&s->formats,&(FormatScopeView){entry->owner,entry->raw.data,entry->raw.bytes});
    CellFormat record=format_resolve_record_token(&s->formats,0,0,SIZE_MAX,raw,1,0);
    CHECK(raw && raw!=SIZE_MAX && record.flags==FORMAT_BOLD && record.color==3);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK && event);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(event); FormatDelta delta={0};
    CHECK(info->family==DX_HISTORY_FORMAT_FAMILY && info->dependency_bytes && format_delta_decode(workbook_history_event_payload(event),info->bytes,SIZE_MAX,&delta)==FORMAT_DELTA_OK);
    format_delta_free(&delta); CHECK(workbook_history_stream_complete(stream,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK);
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && s->formats.redo_count==1);
    p=bound_plan(&c,table,false); t=spatial_target(&c); b.dirty=false; uint64_t revision=b.command_revision,order=worksheet_history_high_water();
    CHECK(workbook_apply_source_aware_spatial_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_NOOP && !p.items && !b.dirty && b.command_revision==revision && worksheet_history_high_water()==order && s->formats.redo_count==1);
    CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED);
    p=spatial_plan(s,false); t=spatial_target(&c);
    CHECK(workbook_apply_source_aware_spatial_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED && format_at(&s->formats,20,20).flags==FORMAT_ITALIC);
    raw=format_scope_token(&s->formats,&(FormatScopeView){entry->owner,entry->raw.data,entry->raw.bytes});
    record=format_resolve_record_token(&s->formats,20,20,SIZE_MAX,raw,1,0);
    CHECK(!(record.flags&FORMAT_ITALIC)); /* A finite manual patch cannot leak into imported records. */
    b.command_revision=UINT64_MAX; s->formats.revision=UINT64_MAX; s->source_versions.high_water=UINT64_MAX;
    p=bound_plan(&c,table,false); t=spatial_target(&c); b.dirty=false;
    CHECK(workbook_apply_source_aware_spatial_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_NOOP && !p.items && !b.dirty);
    analysis_free(&actual); source_format_command_plan_free(&first); workbook_free(&b);
}
static void spatial_scoped(void) {
    Workbook b=book(false); Worksheet *s=b.sheets; uint64_t epoch=1;
    SourceFormatCommandContext c={&b,&s->analysis,&epoch,SIZE_MAX,true,false};
    SourceFormatCommandTarget t=target(&c,false); SourceFormatCommandPlan first=plan(&c,false);
    CHECK(workbook_apply_source_format_recorded(&c,&t,&first,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED);
    const SourceVersion *entry=s->source_versions.items; SourceVersions ledger=s->source_versions; SourcePacket result={0};
    CHECK(source_scope_result_build(&entry->raw,&s->analysis,&result,NULL));
    FormatScopeView view={0,result.data,result.bytes}; CellFormat style=format_default(); style.align=FORMAT_CENTER; style.color=2; style.flags=FORMAT_ITALIC;
    CHECK(format_apply_scoped(&s->formats,(FormatRange){0,0,0,0},FORMAT_ALIGN,style,&view,1,false,NULL));
    worksheet_record(s,EDIT_FORMAT); b.command_revision++; workbook_history_note_edit(&b);
    CHECK(format_apply_scoped(&s->formats,(FormatRange){20,20,0,0},FORMAT_COLOR,style,&view,1,true,NULL));
    worksheet_record(s,EDIT_FORMAT); b.command_revision++; workbook_history_note_edit(&b);
    FormatSnapshot p={0}; CHECK(format_plan_spatial(&s->formats,(FormatRange){0,DX_SHEET_ROWS-1,0,0},FORMAT_ITALIC,style,&p,NULL));
    t=spatial_target(&c); uint64_t revision=b.command_revision;
    CHECK(workbook_apply_source_aware_spatial_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED && b.command_revision==revision+1 && !memcmp(&ledger,&s->source_versions,sizeof(ledger)));
    size_t raw=format_scope_token(&s->formats,&(FormatScopeView){0,entry->raw.data,entry->raw.bytes}),token=format_scope_token(&s->formats,&view);
    CHECK(raw && raw!=SIZE_MAX && token && token!=SIZE_MAX && raw!=token);
    CellFormat exact=format_resolve_record_token(&s->formats,0,0,token,raw,1,0);
    CHECK(exact.flags==(FORMAT_BOLD|FORMAT_ITALIC) && exact.align==FORMAT_CENTER);
    CellFormat unavailable=format_resolve_record_token(&s->formats,0,0,SIZE_MAX,raw,1,0);
    CHECK(unavailable.flags==(FORMAT_BOLD|FORMAT_ITALIC) && unavailable.align!=FORMAT_CENTER);
    CellFormat manual=format_at(&s->formats,20,0); CHECK(manual.color==2 && manual.flags==FORMAT_ITALIC);
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && format_at(&s->formats,20,0).flags==0 && format_at(&s->formats,20,0).color==2);
    CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED && format_at(&s->formats,20,0).flags==FORMAT_ITALIC);
    source_packet_free(&result); source_format_command_plan_free(&first); workbook_free(&b);
}
static void spatial_rejections(void) {
    for (unsigned mode=0;mode<10;mode++) {
        Workbook b=book(false); Worksheet *s=b.sheets; s->formats.source_aware=true;
        if (mode!=4) s->analysis.kind=SOURCE_DATABASE; /* No RAW eligibility or first binding is invented. */
        uint64_t epoch=1; SourceFormatCommandContext c={&b,&s->analysis,&epoch,SIZE_MAX,true,false};
        if (mode==4) {
            SourceFormatCommandTarget raw=target(&c,false); SourceFormatCommandPlan first=plan(&c,false);
            CHECK(workbook_apply_source_format_recorded(&c,&raw,&first,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED);
            source_format_command_plan_free(&first);
        }
        SourceFormatCommandTarget t=spatial_target(&c),raw_target={0};
        if (mode!=4) CHECK(!workbook_prepare_source_format(&c,0,0,&raw_target,NULL));
        FormatSnapshot p=spatial_plan(s,true);
        if (mode==0) epoch++;
        if (mode==1) t.cached.unique_rows=true;
        if (mode==2) t.objects.next_id++;
        if (mode==3) t.sources.high_water++;
        if (mode==4) c.validation_reserved=0;
        if (mode==5) p.source_aware=false;
        if (mode==6) c.ready=false;
        if (mode==7) c.read_only=true;
        if (mode==8) b.command_revision=UINT64_MAX;
        if (mode==9) s->formats.source_aware=false;
        Workbook before=b; Worksheet sheet=*s; FormatSnapshot owned=p;
        SourceFormatCommandStatus expected=mode<4 || mode==8 ? SOURCE_FORMAT_COMMAND_STALE : mode==4 ? SOURCE_FORMAT_COMMAND_LIMIT : mode==5 ? SOURCE_FORMAT_COMMAND_INVALID : SOURCE_FORMAT_COMMAND_PROTECTED;
        SourceFormatCommandResult result=workbook_apply_source_aware_spatial_format_recorded(&c,&t,&p,NULL,NULL);
        if (result.status!=expected) fprintf(stderr,"Spatial rejection mode %u: expected %u, got %u\n",mode,expected,result.status);
        CHECK(result.status==expected && !memcmp(&before,&b,sizeof(b)) && !memcmp(&sheet,s,sizeof(sheet)) && !memcmp(&owned,&p,sizeof(p)));
        format_snapshot_free(&p); workbook_free(&b);
    }
    Workbook b=book(false); Worksheet *s=b.sheets; uint64_t epoch=1; SourceFormatCommandContext c={&b,&s->analysis,&epoch,SIZE_MAX,true,false};
    SourceFormatCommandTarget t={0},unchanged=t; CHECK(!workbook_prepare_source_aware_spatial_format(&c,0,&t,NULL) && !memcmp(&t,&unchanged,sizeof(t)));
    s->formats.source_aware=true; s->analysis.kind=SOURCE_DATABASE;
    FormatSnapshot p=spatial_plan(s,true); t=spatial_target(&c);
    CHECK(workbook_apply_source_aware_spatial_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED && !s->source_versions.count && !s->analysis.source_version);
    workbook_free(&b);
}
static void spatial_callbacks(void) {
    for (unsigned mode=0;mode<=6;mode++) {
        Workbook b=book(false); uint64_t epoch=1; SourceFormatCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false};
        SourceFormatCommandTarget t=target(&c,false); SourceFormatCommandPlan first=plan(&c,false);
        CHECK(workbook_apply_source_format_recorded(&c,&t,&first,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED); b.dirty=false;
        FormatSnapshot p=spatial_plan(b.sheets,true); t=spatial_target(&c); size_t actions=b.sheets[0].undo_actions_count;
        BoundProbe probe={&c,mode,0,0,0}; WorkbookHistoryFormatRecorder recorder={bound_reserve,bound_publish,bound_cancel,&probe};
        SourceFormatCommandResult r=workbook_apply_source_aware_spatial_format_recorded(&c,&t,&p,&recorder,NULL);
        CHECK(probe.reserves==1);
        if (!mode) CHECK(r.status==SOURCE_FORMAT_COMMAND_APPLIED && probe.publishes==1 && !probe.cancels && !p.items);
        else {
            CHECK(r.status==(mode==1 ? SOURCE_FORMAT_COMMAND_LIMIT : mode==2 || mode==6 ? SOURCE_FORMAT_COMMAND_INVALID : mode==4 ? SOURCE_FORMAT_COMMAND_PROTECTED : SOURCE_FORMAT_COMMAND_STALE));
            CHECK(!probe.publishes && probe.cancels==(mode>=3 ? 1u : 0u) && p.items && !b.dirty && b.command_revision==1 && b.sheets[0].undo_actions_count==actions);
            if (mode==1) CHECK(workbook_apply_source_aware_spatial_format_recorded(&c,&t,&p,NULL,NULL).status==SOURCE_FORMAT_COMMAND_APPLIED);
        }
        format_snapshot_free(&p); source_format_command_plan_free(&first); workbook_free(&b);
    }
}
int main(void) {
    spatial(false); spatial(true); spatial_scoped(); spatial_rejections(); spatial_callbacks();
    fences(false); fences(true); callbacks(false); callbacks(true); history(false); history(true); retained_sources(false); retained_sources(true); manual_policy(); stream_handoff(false); stream_handoff(true); bound(false); bound(true); bound_callbacks(false); bound_callbacks(true); limits();
    source_format_command_plan_free(NULL); printf("PASS: %u atomic source format command checks\n",checks); return 0;
}
