#include "workbook/history_definition.h"
#include "workbook/history_format.h"
#include "workbook/cell_delta_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL history definition:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static WorkbookHistoryStream *create(size_t entries,size_t bytes,bool forked) {
    WorkbookHistoryStreamOrigin o={.owner={1,2,99,VERSION_WORKING_HEAD},.edit_session=44,.command_revision=100,
        .cursor=5,.tail=forked ? 7 : 5,.lineage=2,.action_high_water=40,.transition_high_water=60,.lineage_high_water=7};
    WorkbookHistoryStream *s=NULL; CHECK(workbook_history_stream_new(o,(WorkbookHistoryStreamOptions){entries,bytes},&s)==HISTORY_STREAM_OK); return s;
}
static WorkbookHistoryDefinitionRecord record(WorkbookHistoryStream *s,DefinitionEventView view) {
    WorkbookHistoryStreamOrigin o=workbook_history_stream_state(s).current;
    return (WorkbookHistoryDefinitionRecord){.workbook={1,2},.edit_session=44,.sheet_session=88,.expected_revision=o.command_revision,.revision=o.command_revision+1,.view=view,.validation_reserved=SIZE_MAX};
}
static void reject(WorkbookHistoryStream *s,WorkbookHistoryDefinitionRecord r) {
    WorkbookHistoryStreamState before=workbook_history_stream_state(s); WorkbookHistoryDefinitionRecorder a=workbook_history_stream_definition_recorder(s); void *ticket=NULL; char *error=NULL;
    CHECK(!a.reserve(a.context,&r,&ticket,&error) && !ticket && error); free(error);
    WorkbookHistoryStreamState after=workbook_history_stream_state(s);
    CHECK(!memcmp(&before.current,&after.current,sizeof(before.current)) && before.pending==after.pending && before.owned_bytes==after.owned_bytes && before.reserved==after.reserved && before.in_flight==after.in_flight);
}
static void ownership_fences_and_cancel(void) {
    WorkbookHistoryStream *s=create(2,DX_HISTORY_STREAM_DEFAULT_BYTES,true); SourceVersions sources={.high_water=20};
    Analysis before={.kind=SOURCE_CSV,.path="/source"},after=before; char query[]="SELECT Id FROM input"; after.query=query;
    DefinitionEventView view={.sheet=9,.effects=1,.before={&before,NULL},.after={&after,NULL},.sources=&sources};
    WorkbookHistoryDefinitionRecord good=record(s,view),bad=good;
    bad.workbook.low++; reject(s,bad); bad=good; bad.edit_session++; reject(s,bad); bad=good; bad.expected_revision++; reject(s,bad);
    bad=good; bad.revision++; reject(s,bad); bad=good; bad.sheet_session=0; reject(s,bad); bad=good; bad.view.effects=9; reject(s,bad);
    bad=good; bad.view.after.primary=&before; reject(s,bad); bad=good; bad.view.sources=NULL; reject(s,bad);
    WorkbookHistoryDefinitionRecorder a=workbook_history_stream_definition_recorder(s); void *ticket=NULL; char *error=NULL;
    CHECK(a.reserve(a.context,&good,&ticket,&error) && ticket && !error); size_t charge=workbook_history_stream_state(s).owned_bytes;
    CHECK(workbook_history_stream_state(s).reserved && charge>0 && workbook_history_stream_state(s).current.tail==7); reject(s,good);
    CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_BUSY); a.cancel(a.context,ticket); ticket=NULL;
    CHECK(!workbook_history_stream_state(s).owned_bytes && workbook_history_stream_state(s).current.action_high_water==40 && workbook_history_stream_state(s).current.tail==7);
    CHECK(a.reserve(a.context,&good,&ticket,NULL)); memset(query,'x',sizeof(query)-1); a.publish(a.context,ticket);
    WorkbookHistoryStreamState state=workbook_history_stream_state(s); CHECK(state.pending==1 && state.current.cursor==6 && state.current.tail==6 && state.current.lineage==8 && state.current.action_high_water==41);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *info=workbook_history_event_info(event);
    CHECK(info->family==DX_HISTORY_DEFINITION_FAMILY && info->encoding==DX_DEFINITION_EVENT_ENCODING && !info->dependency_bytes && !workbook_history_event_dependencies(event) && info->revision==101 && info->forked);
    DefinitionEvent decoded={0}; CHECK(definition_event_decode(workbook_history_event_payload(event),info->bytes,SIZE_MAX,&decoded)==DEFINITION_EVENT_OK && !strcmp(decoded.after_primary.query,"SELECT Id FROM input"));
    definition_event_free(&decoded); const WorkbookHistoryEvent *identity=event; CHECK(workbook_history_stream_retry(s,&event)==HISTORY_STREAM_OK);
    CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK && event==identity && workbook_history_stream_state(s).owned_bytes==charge);
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK && !workbook_history_stream_state(s).owned_bytes);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    s=create(1,charge-1,false); good=record(s,view); reject(s,good); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    s=create(1,charge,false); a=workbook_history_stream_definition_recorder(s); good=record(s,view); ticket=NULL;
    CHECK(a.reserve(a.context,&good,&ticket,NULL)); a.publish(a.context,ticket); reject(s,record(s,view));
    CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK); reject(s,record(s,view));
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
}
static void mixed_fifo(void) {
    WorkbookHistoryStream *s=create(64,DX_HISTORY_STREAM_DEFAULT_BYTES,false); SourceVersions sources={0}; Analysis a={.kind=SOURCE_CSV,.path="/source"},b=a;
    DefinitionEventView view={.sheet=9,.effects=1,.before={&a,NULL},.after={&b,NULL},.sources=&sources}; WorkbookHistoryDefinitionRecorder adapter=workbook_history_stream_definition_recorder(s);
    WorkbookHistoryEvent *held=NULL;
    for (unsigned i=0;i<40;i++) {
        a.query=i%2 ? "SELECT Id FROM input" : NULL; b.query=i%2 ? NULL : "SELECT Id FROM input";
        WorkbookHistoryDefinitionRecord r=record(s,view); void *ticket=NULL; CHECK(adapter.reserve(adapter.context,&r,&ticket,NULL)); adapter.publish(adapter.context,ticket);
        if (!i) CHECK(workbook_history_stream_dispatch(s,&held)==HISTORY_STREAM_OK);
    }
    CHECK(workbook_history_stream_state(s).pending==39); const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    DefinitionEvent decoded={0}; CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&decoded)==DEFINITION_EVENT_OK && !decoded.before_primary.query && !strcmp(decoded.after_primary.query,"SELECT Id FROM input")); definition_event_free(&decoded);
    WorkbookHistoryStreamOrigin o=workbook_history_stream_state(s).current;
    CellEditRecord cell={.target={.workbook={1,2},.document=44,.sheet=88,.sheet_id=9,.expected_document_revision=o.command_revision},.revision=o.command_revision+1,.delta={9,0,0,"","cell",0,4}};
    CellEditRecorder c=workbook_history_stream_cell_recorder(s); void *ticket=NULL; CHECK(c.reserve(c.context,&cell,&ticket,NULL)); c.publish(c.context,ticket);
    FormatStore f={0}; FormatSnapshot next={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_plan_spatial(&f,(FormatRange){0},FORMAT_BOLD,bold,&next,NULL)); o=workbook_history_stream_state(s).current;
    WorkbookHistoryFormatRecord format={.workbook={1,2},.edit_session=44,.sheet_session=88,.sheet=9,.expected_revision=o.command_revision,.revision=o.command_revision+1,
        .effects=8,.before=format_delta_store_view(&f),.after=format_delta_snapshot_view(&next),.sources=&sources};
    WorkbookHistoryFormatRecorder fr=workbook_history_stream_format_recorder(s); ticket=NULL; CHECK(fr.reserve(fr.context,&format,&ticket,NULL)); fr.publish(fr.context,ticket);
    format_snapshot_free(&next); format_free(&f); CHECK(workbook_history_stream_complete(s,&held)==HISTORY_STREAM_OK);
    for (unsigned i=1;i<=41;i++) {
        CHECK(workbook_history_stream_dispatch(s,&held)==HISTORY_STREAM_OK); info=workbook_history_event_info(held);
        CHECK(info->action==41+i && info->transition==61+i && info->position==6+i && info->revision==101+i);
        CHECK(info->family==(i==41 ? DX_HISTORY_FORMAT_FAMILY : i==40 ? DX_HISTORY_CELL_FAMILY : DX_HISTORY_DEFINITION_FAMILY));
        CHECK(workbook_history_stream_complete(s,&held)==HISTORY_STREAM_OK);
    }
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
}
static void validation_budget(void) {
    WorkbookHistoryStream *s=create(1,DX_HISTORY_STREAM_DEFAULT_BYTES,false); SourceVersions sources={0};
    Analysis a={.kind=SOURCE_CSV,.filter_column="A",.filter_value="[\"1\",\"2\"]",.filter_op=FILTER_BETWEEN},b=a; b.filter_value="[\"1\",\"3\"]";
    DefinitionEventView v={.sheet=9,.effects=1,.before={&a,NULL},.after={&b,NULL},.sources=&sources}; WorkbookHistoryDefinitionRecord r=record(s,v); r.validation_reserved=0;
    reject(s,r); CHECK(workbook_history_stream_state(s).current.action_high_water==40);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
}
static void explicit_large_capacity(void) {
    SourceVersions sources={0}; SheetObject a={.id=9,.name="Raw",.kind=OBJECT_TABLE,.rows=5,.columns=4,
        .source={.kind=SOURCE_CSV,.path="/one",.filter_column="A",.filter_value="",.filter_op=FILTER_CONTAINS,.filter_count=1}};
    a.source.filters[0]=(AnalysisFilter){.column="B",.value="",.op=FILTER_CONTAINS}; ObjectSnapshot before={&a,1}; ObjectsAssetInfo measured={0};
    CHECK(objects_asset_measure(9,&before,10,&measured)==OBJECTS_ASSET_OK);
    size_t total=DX_OBJECT_CURRENT_BYTES-measured.measure.owned,first=4u*1024u*1024u,second=total-first;
    char *x=dx_alloc(first+1),*y=dx_alloc(second+1); memset(x,'a',first); memset(y,'b',second); a.source.filter_value=x; a.source.filters[0].value=y;
    SheetObject b=a; b.name="New"; ObjectSnapshot after={&b,1};
    DefinitionEventView view={.sheet=9,.object_high_water=10,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&sources};
    WorkbookHistoryStream *s=create(1,DX_HISTORY_STREAM_DEFAULT_BYTES,false); reject(s,record(s,view));
    CHECK(!workbook_history_stream_state(s).owned_bytes && workbook_history_stream_state(s).current.action_high_water==40);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    s=create(1,DX_HISTORY_STREAM_BYTES,false); CHECK(!workbook_history_stream_state(s).owned_bytes);
    WorkbookHistoryDefinitionRecord r=record(s,view); WorkbookHistoryDefinitionRecorder adapter=workbook_history_stream_definition_recorder(s); void *ticket=NULL;
    CHECK(adapter.reserve(adapter.context,&r,&ticket,NULL) && ticket && workbook_history_stream_state(s).owned_bytes>DX_HISTORY_STREAM_DEFAULT_BYTES);
    free(x); free(y); adapter.publish(adapter.context,ticket);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(event); DefinitionEvent decoded={0};
    CHECK(definition_event_decode(workbook_history_event_payload(event),info->bytes,SIZE_MAX,&decoded)==DEFINITION_EVENT_OK && decoded.effects==4 &&
        !strcmp(decoded.before_objects.objects.items[0].name,"Raw") && !strcmp(decoded.after_objects.objects.items[0].name,"New") &&
        strlen(decoded.after_objects.objects.items[0].source.filter_value)==first && strlen(decoded.before_objects.objects.items[0].source.filters[0].value)==second);
    definition_event_free(&decoded); CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK && !workbook_history_stream_state(s).owned_bytes);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
}
int main(void) { ownership_fences_and_cancel(); mixed_fifo(); validation_budget(); explicit_large_capacity(); printf("PASS: %u compound definition stream ownership checks\n",checks); return 0; }
