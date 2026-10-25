#include "workbook/history_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL history profile:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static WorkbookHistoryProfileCaps caps(void) { return (WorkbookHistoryProfileCaps){DX_HISTORY_PROFILE_RECORDS,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES}; }
#include "support/history_profile_fixture.h"
static void rejects(WorkbookHistoryProfileView view,WorkbookHistoryProfileCaps budget,WorkbookHistoryProfile *prior,WorkbookHistoryProfileStatus expected) {
    WorkbookHistoryProfileMeasure out={.records=77},before=out; WorkbookHistoryProfile *next=prior;
    CHECK(workbook_history_profile_measure(view,budget,prior,&out)==expected && !memcmp(&out,&before,sizeof(out)));
    CHECK(workbook_history_profile_prepare(view,budget,&next)==expected && next==prior);
}
static void mixed(void) {
    Fixture f; fixture(&f); WorkbookHistoryProfile *out=NULL; WorkbookHistoryProfileMeasure m;
    CHECK(workbook_history_profile_measure(f.view,caps(),NULL,&m)==HISTORY_PROFILE_OK && m.records==6 && m.owned==m.metadata_bytes+m.payload_bytes && m.validation_peak && m.staging==m.owned+m.validation_peak && m.root_bytes==12345);
    WorkbookHistoryProfileCaps limited=caps(); limited.staging_bytes=m.staging-1; rejects(f.view,limited,NULL,HISTORY_PROFILE_LIMIT);
    CHECK(workbook_history_profile_prepare(f.view,caps(),&out)==HISTORY_PROFILE_OK && workbook_history_profile_owned(out)==m.owned);
    const WorkbookHistoryProfileView *held=workbook_history_profile_view(out); const WorkbookHistoryProtection *protection=workbook_history_profile_protection(out);
    CHECK(held && held->records!=f.records && held->versions!=&f.index && held->versions->tags[0].label.data!=f.tag.label.data && !memcmp(held->versions->tags[0].label.data,"Current",7));
    CHECK(protection && protection->has_saved && protection->current_cursor==4 && protection->current_through==6 && protection->saved.count==1);
    for (size_t i=0;i<6;i++) CHECK(held->records[i].data!=f.records[i].data && held->records[i].event!=f.frames+i && !held->records[i].live && workbook_history_profile_report(out,i)->family==f.frames[i].family);
    CHECK(workbook_history_profile_report(out,2)->observations.unbound_sources==1 && workbook_history_profile_report(out,2)->compatibility==SOURCE_DEPENDENCY_EXACT);
    CHECK(!workbook_history_profile_report(out,5)->source_compared && !workbook_history_profile_report(out,0)->source_compared);
    WorkbookHistoryProfileMeasure replacement; CHECK(workbook_history_profile_measure(f.view,caps(),out,&replacement)==HISTORY_PROFILE_OK && replacement.prior_owned==m.owned && replacement.staging==m.staging+m.owned);
    limited=caps(); limited.staging_bytes=replacement.staging-1; rejects(f.view,limited,out,HISTORY_PROFILE_LIMIT);
    limited=caps(); limited.records=5; rejects(f.view,limited,out,HISTORY_PROFILE_LIMIT);
    limited=caps(); limited.metadata_bytes=m.metadata_bytes-1; rejects(f.view,limited,out,HISTORY_PROFILE_LIMIT);
    limited=caps(); limited.payload_bytes=m.payload_bytes-1; rejects(f.view,limited,out,HISTORY_PROFILE_LIMIT);
    WorkbookHistoryProfileView v=f.view; v.container_version=15; rejects(v,caps(),out,HISTORY_PROFILE_UNSUPPORTED); v=f.view; v.core_version=16; rejects(v,caps(),out,HISTORY_PROFILE_UNSUPPORTED);
    f.root.encoding=14; rejects(f.view,caps(),out,HISTORY_PROFILE_UNSUPPORTED); f.root.encoding=15;
    v=f.view; v.document.low++; rejects(v,caps(),out,HISTORY_PROFILE_METADATA);
    f.frames[5].family=99; rejects(f.view,caps(),out,HISTORY_PROFILE_UNSUPPORTED); f.frames[5].family=5;
    f.frames[4].encoding=99; rejects(f.view,caps(),out,HISTORY_PROFILE_UNSUPPORTED); f.frames[4].encoding=1;
    f.records[3].intent.effects=2; rejects(f.view,caps(),out,HISTORY_PROFILE_INVALID); f.records[3].intent.effects=16;
    f.frames[5].transition=5; rejects(f.view,caps(),out,HISTORY_PROFILE_COVERAGE); f.frames[5].transition=6;
    f.map[5]=4; rejects(f.view,caps(),out,HISTORY_PROFILE_ASSOCIATION); f.map[5]=5;
    v=f.view; v.current.count=5; rejects(v,caps(),out,HISTORY_PROFILE_COVERAGE);
    f.anchor.boundary.root.bytes[0]++; rejects(f.view,caps(),out,HISTORY_PROFILE_METADATA); f.anchor.boundary.root.bytes[0]--;
    f.records[0].owner.session++; rejects(f.view,caps(),out,HISTORY_PROFILE_ASSOCIATION); f.records[0].owner.session--;
    /* Late full DXSP semantics fail after all copied earlier family bytes have
       closed. The previous candidate and every old descriptor stay owned. */
    SheetTopologyInfo info; CHECK(sheet_topology_preflight(f.records[5].data,f.records[5].bytes,&info)==SHEET_TOPOLOGY_OK);
    unsigned char *bad=(unsigned char *)f.records[5].data; size_t at=f.records[5].bytes-info.presence_bytes+DX_SHEET_PRESENCE_HEADER_BYTES; bad[at]^=1;
    CHECK(workbook_history_profile_measure(f.view,caps(),out,&replacement)==HISTORY_PROFILE_OK); WorkbookHistoryProfile *prior=out;
    CHECK(workbook_history_profile_prepare(f.view,caps(),&out)==HISTORY_PROFILE_INVALID && out==prior); bad[at]^=1;
    f.records[2].live=NULL; CHECK(workbook_history_profile_prepare(f.view,caps(),&out)==HISTORY_PROFILE_OK && workbook_history_profile_report(out,2)->compatibility==SOURCE_DEPENDENCY_UNAVAILABLE); f.records[2].live=&f.live;
    fixture_free(&f); memset(&f,0,sizeof(f)); held=workbook_history_profile_view(out);
    CHECK(held->current.path->tail==6 && held->records[5].event->transition==6 && !memcmp(held->records[0].data,"DXCD",4) && !memcmp(held->versions->tags[0].label.data,"Current",7));
    CHECK(!workbook_history_profile_report(out,6)); workbook_history_profile_free(&out); CHECK(!out); workbook_history_profile_free(&out); workbook_history_profile_free(NULL);
}
static void checkpoint(void) {
    Fixture f; fixture(&f); WorkbookHistoryProfile *out=NULL;
    WorkbookHistoryProfileView v=f.view; v.kind=HISTORY_PROFILE_CHECKPOINT_ONLY;
    rejects(v,caps(),NULL,HISTORY_PROFILE_COVERAGE);
    v.anchors=NULL; v.current=(WorkbookHistoryProfilePath){0}; v.saved=(WorkbookHistoryProfilePath){0}; v.records=NULL; v.count=0;
    WorkbookHistoryProfileMeasure m; CHECK(workbook_history_profile_measure(v,caps(),NULL,&m)==HISTORY_PROFILE_OK && !m.records && !m.payload_bytes && !m.validation_peak && m.owned==m.staging);
    CHECK(workbook_history_profile_prepare(v,caps(),&out)==HISTORY_PROFILE_OK && !workbook_history_profile_protection(out) && !workbook_history_profile_report(out,0));
    CHECK(workbook_history_profile_view(out)->versions->saved.present && !workbook_history_profile_view(out)->records);
    WorkbookHistoryProfileView invalid=v; invalid.kind=(WorkbookHistoryProfileKind)2; rejects(invalid,caps(),out,HISTORY_PROFILE_UNSUPPORTED);
    invalid=v; invalid.current.records=f.map; rejects(invalid,caps(),out,HISTORY_PROFILE_COVERAGE);
    invalid=v; invalid.saved.path=&f.path; rejects(invalid,caps(),out,HISTORY_PROFILE_COVERAGE);
    invalid=v; invalid.records=f.records; rejects(invalid,caps(),out,HISTORY_PROFILE_COVERAGE);
    invalid=v; invalid.kind=HISTORY_PROFILE_RETAINED; rejects(invalid,caps(),out,HISTORY_PROFILE_METADATA);
    /* A current unsaved empty document also has an explicit checkpoint mode;
       there is no inferred event history or synthetic Saved anchor. */
    WorkbookVersionIndex empty={.document_high=f.doc.high,.document_low=f.doc.low}; v.versions=&empty;
    CHECK(workbook_history_profile_prepare(v,caps(),&out)==HISTORY_PROFILE_OK && !workbook_history_profile_view(out)->versions->saved.present);
    CHECK(!workbook_history_profile_view(NULL) && !workbook_history_profile_protection(NULL) && !workbook_history_profile_report(NULL,0) && !workbook_history_profile_owned(NULL));
    CHECK(workbook_history_profile_prepare(v,caps(),NULL)==HISTORY_PROFILE_INVALID && workbook_history_profile_measure(v,caps(),NULL,NULL)==HISTORY_PROFILE_INVALID);
    workbook_history_profile_free(&out); fixture_free(&f);
}
static void fork_union(void) {
    Fixture f; fixture(&f);
    WorkbookHistoryEventPosition saved_events[]={{1,0,1},{2,0,2}},current_events[]={{1,0,1},{3,1,2}};
    WorkbookHistorySegment saved_segment={0,0,2},current_segments[]={{0,0,1},{1,1,2}};
    WorkbookHistoryPath saved={f.doc.high,f.doc.low,&saved_segment,1,saved_events,2,0,2,0,0,2,2};
    WorkbookHistoryPath current={f.doc.high,f.doc.low,current_segments,2,current_events,2,1,3,0,0,1,2};
    size_t saved_map[]={0,1},current_map[]={0,2};
    f.anchor.boundary.position=2;
    f.frames[2].lineage=1; f.frames[2].parent_lineage=0; f.frames[2].forked=true; f.frames[2].position=2; f.frames[2].previous_position=1;
    f.view.current=(WorkbookHistoryProfilePath){&current,current_map,2}; f.view.saved=(WorkbookHistoryProfilePath){&saved,saved_map,2}; f.view.count=3;
    WorkbookHistoryProfile *out=NULL; WorkbookHistoryProfileMeasure m;
    CHECK(workbook_history_profile_measure(f.view,caps(),NULL,&m)==HISTORY_PROFILE_OK && m.records==3 && m.payload_bytes==f.records[0].bytes+f.records[1].bytes+f.records[1].dependency_bytes+f.records[2].bytes);
    CHECK(workbook_history_profile_prepare(f.view,caps(),&out)==HISTORY_PROFILE_OK);
    const WorkbookHistoryProtection *p=workbook_history_profile_protection(out); const WorkbookHistoryProfileView *held=workbook_history_profile_view(out);
    CHECK(p && p->has_saved && p->shared_through==1 && p->current_cursor==1 && p->current_through==2 && p->saved.through==2);
    CHECK(held->current.records[0]==held->saved.records[0] && held->current.path!=held->saved.path && held->records[2].event->forked);
    /* Shared transition bytes are owned/decoded once, but divergent suffixes
       remain separately owned even while the current cursor is before Redo. */
    CHECK(held->records[held->current.records[0]].data==held->records[held->saved.records[0]].data);
    WorkbookHistoryProfileView invalid=f.view; invalid.count=4; rejects(invalid,caps(),out,HISTORY_PROFILE_COVERAGE);
    invalid=f.view; invalid.count=2; rejects(invalid,caps(),out,HISTORY_PROFILE_COVERAGE);
    current_map[0]=1; rejects(f.view,caps(),out,HISTORY_PROFILE_ASSOCIATION); current_map[0]=0;
    current_map[1]=0; rejects(f.view,caps(),out,HISTORY_PROFILE_ASSOCIATION); current_map[1]=2;
    f.frames[2].forked=false; f.frames[2].parent_lineage=1; rejects(f.view,caps(),out,HISTORY_PROFILE_ASSOCIATION); f.frames[2].forked=true; f.frames[2].parent_lineage=0;
    f.frames[2].parent_lineage=1; rejects(f.view,caps(),out,HISTORY_PROFILE_INVALID); f.frames[2].parent_lineage=0;
    current.lineage_high_water=0; rejects(f.view,caps(),out,HISTORY_PROFILE_METADATA); current.lineage_high_water=1;
    /* Reusable action identity is not transition identity: replay transitions
       can reuse the original action without duplicating global record IDs. */
    f.frames[2].action=1; CHECK(workbook_history_profile_prepare(f.view,caps(),&out)==HISTORY_PROFILE_OK);
    fixture_free(&f); memset(saved_events,0,sizeof(saved_events)); memset(current_events,0,sizeof(current_events));
    held=workbook_history_profile_view(out); CHECK(held->current.path->events[1].operation==3 && held->saved.path->events[1].operation==2 && held->records[2].event->action==1);
    workbook_history_profile_free(&out);
}
static void adversarial(void) {
    Fixture f; fixture(&f); WorkbookHistoryProfile *out=NULL;
    CHECK(workbook_history_profile_prepare(f.view,caps(),&out)==HISTORY_PROFILE_OK);
    WorkbookHistoryProfileView invalid=f.view; invalid.count=DX_HISTORY_PROFILE_RECORDS+1u; invalid.records=NULL; rejects(invalid,caps(),out,HISTORY_PROFILE_LIMIT);
    size_t count=f.index.root_count; f.index.root_count=DX_VERSION_ROOTS+1u; rejects(f.view,caps(),out,HISTORY_PROFILE_LIMIT); f.index.root_count=count;
    count=f.path.event_count; f.path.event_count=DX_HISTORY_PROFILE_RECORDS+1u; rejects(f.view,caps(),out,HISTORY_PROFILE_LIMIT); f.path.event_count=count;
    count=f.anchors.count; f.anchors.count=DX_HISTORY_PUBLIC_ANCHORS+1u; rejects(f.view,caps(),out,HISTORY_PROFILE_LIMIT); f.anchors.count=count;
    WorkbookVersionRoot roots[]={f.root,f.root}; roots[0].bytes=UINT64_MAX; roots[1].key.bytes[0]++; roots[1].bytes=1; f.index.roots=roots; f.index.root_count=2;
    rejects(f.view,caps(),out,HISTORY_PROFILE_METADATA); f.index.roots=&f.root; f.index.root_count=1;
    size_t bytes=f.records[0].bytes; f.records[0].bytes=SIZE_MAX; rejects(f.view,caps(),out,HISTORY_PROFILE_LIMIT); f.records[0].bytes=bytes;
    f.frames[0].owner.document_low++; f.records[0].owner=f.frames[0].owner; rejects(f.view,caps(),out,HISTORY_PROFILE_ASSOCIATION); f.frames[0].owner.document_low--; f.records[0].owner=f.frames[0].owner;
    f.frames[0].edit_session=0; f.records[0].edit_session=0; rejects(f.view,caps(),out,HISTORY_PROFILE_INVALID); f.frames[0].edit_session=9; f.records[0].edit_session=9;
    f.records[5].dependencies=f.records[1].dependencies; rejects(f.view,caps(),out,HISTORY_PROFILE_INVALID); f.records[5].dependencies=NULL;
    f.records[4].intent.operation=SHEET_HIDE; rejects(f.view,caps(),out,HISTORY_PROFILE_INVALID); f.records[4].intent.operation=SHEET_RENAME;
    f.records[5].sheet=1; rejects(f.view,caps(),out,HISTORY_PROFILE_INVALID); f.records[5].sheet=3;
    f.frames[1].dependency_encoding=99; rejects(f.view,caps(),out,HISTORY_PROFILE_UNSUPPORTED); f.frames[1].dependency_encoding=DX_SOURCE_VERSIONS_ENCODING;
    f.live.sheet=2; WorkbookHistoryProfile *prior=out;
    CHECK(workbook_history_profile_prepare(f.view,caps(),&out)==HISTORY_PROFILE_ASSOCIATION && out==prior); f.live.sheet=1;
    unsigned char *data=(unsigned char *)f.records[1].dependencies; data[0]^=1; rejects(f.view,caps(),out,HISTORY_PROFILE_INVALID); data[0]^=1;
    f.frames[0].bytes++; rejects(f.view,caps(),out,HISTORY_PROFILE_INVALID); f.frames[0].bytes--;
    CHECK(workbook_history_profile_prepare(f.view,caps(),&out)==HISTORY_PROFILE_OK);
    fixture_free(&f); workbook_history_profile_free(&out);
}
static void self_replacement(void) {
    Fixture f; fixture(&f); WorkbookHistoryProfile *out=NULL;
    CHECK(workbook_history_profile_prepare(f.view,caps(),&out)==HISTORY_PROFILE_OK);
    /* Returned immutable views are valid preparation inputs until replacement.
       No copied descriptor may keep pointing at the old owner when it dies. */
    WorkbookHistoryProfileView borrowed=*workbook_history_profile_view(out);
    WorkbookHistoryProfileMeasure m; CHECK(workbook_history_profile_measure(borrowed,caps(),out,&m)==HISTORY_PROFILE_OK && m.prior_owned==workbook_history_profile_owned(out));
    WorkbookHistoryProfileCaps exact=caps(); exact.staging_bytes=m.staging;
    WorkbookHistoryProfileCaps short_by_one=exact; short_by_one.staging_bytes--;
    WorkbookHistoryProfile *prior=out; const void *old_bytes=borrowed.records[5].data; const WorkbookVersionRoot *old_roots=borrowed.versions->roots;
    rejects(borrowed,short_by_one,out,HISTORY_PROFILE_LIMIT);
    CHECK(out==prior && workbook_history_profile_view(out)->records[5].data==old_bytes);
    CHECK(workbook_history_profile_prepare(borrowed,exact,&out)==HISTORY_PROFILE_OK && out!=prior);
    const WorkbookHistoryProfileView *held=workbook_history_profile_view(out);
    CHECK(held->records[5].data!=old_bytes && held->versions->roots!=old_roots);
    /* The old view is expired now. All observations below use the new owner;
       the optional live source authorities were deliberately never retained. */
    CHECK(held->records[2].event->action==3 && held->records[0].sheet==1 && !memcmp(held->records[4].data,"DXSL",4));
    CHECK(workbook_history_profile_report(out,2)->compatibility==SOURCE_DEPENDENCY_UNAVAILABLE);
    WorkbookHistoryProfileView checkpoint=*held; checkpoint.kind=HISTORY_PROFILE_CHECKPOINT_ONLY;
    checkpoint.anchors=NULL; checkpoint.current=(WorkbookHistoryProfilePath){0}; checkpoint.saved=(WorkbookHistoryProfilePath){0}; checkpoint.records=NULL; checkpoint.count=0;
    CHECK(workbook_history_profile_measure(checkpoint,caps(),out,&m)==HISTORY_PROFILE_OK && m.prior_owned && !m.validation_peak);
    exact=caps(); exact.staging_bytes=m.staging;
    CHECK(workbook_history_profile_prepare(checkpoint,exact,&out)==HISTORY_PROFILE_OK && !workbook_history_profile_protection(out) && !workbook_history_profile_view(out)->records);
    CHECK(!memcmp(workbook_history_profile_view(out)->versions->tags[0].label.data,"Current",7));
    fixture_free(&f); workbook_history_profile_free(&out);
}
int main(void) { mixed(); checkpoint(); fork_union(); adversarial(); self_replacement(); printf("PASS: %u combined current history profile checks\n",checks); return 0; }
