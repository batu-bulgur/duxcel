#include "workbook/history_cells_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL actual cell history:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct {
    Workbook book; WorkbookVersionIndex versions; WorkbookHistoryAnchors anchors;
    WorkbookHistorySegment segment; WorkbookHistoryPath path; WorkbookHistoryEventPosition positions[4];
    WorkbookHistoryEventInfo frames[4]; WorkbookHistoryProfileRecord records[4]; size_t map[4];
    WorkbookHistoryProfileView view; void *packets[4];
} Fixture;
static void packet(Fixture *f,size_t i,uint64_t sheet,uint64_t row,const char *before,const char *after) {
    free(f->packets[i]); CellDelta d={sheet,row,3,before,after,strlen(before),strlen(after)}; size_t n=0,written=0;
    CHECK(cell_delta_encoded_size(d,&n)==CELL_DELTA_CODEC_OK); f->packets[i]=dx_alloc(n);
    CHECK(cell_delta_encode(d,f->packets[i],n,&written)==CELL_DELTA_CODEC_OK);
    f->frames[i]=(WorkbookHistoryEventInfo){.owner={f->book.id.high,f->book.id.low,1,VERSION_WORKING_HEAD},.edit_session=2,.revision=i+1,.action=i+1,.transition=i+1,.position=i+1,.previous_position=i,.family=DX_HISTORY_CELL_FAMILY,.encoding=DX_CELL_DELTA_ENCODING,.bytes=n};
    f->records[i]=(WorkbookHistoryProfileRecord){.owner=f->frames[i].owner,.edit_session=2,.event=f->frames+i,.data=f->packets[i],.bytes=n,.sheet=sheet,.intent.effects=2};
}
static WorkbookHistoryProfile *prepare(Fixture *f) {
    WorkbookHistoryProfile *profile=NULL;
    CHECK(workbook_history_profile_prepare(f->view,(WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES},&profile)==HISTORY_PROFILE_OK);
    return profile;
}
static void fixture(Fixture *f) {
    *f=(Fixture){0}; workbook_init(&f->book);
    CHECK(workbook_add(&f->book,"A",&(Analysis){.kind=SOURCE_PARQUET,.path="/offline/never-execute"},NULL));
    CHECK(workbook_add(&f->book,"B",&(Analysis){0},NULL));
    f->versions=(WorkbookVersionIndex){.document_high=f->book.id.high,.document_low=f->book.id.low};
    f->anchors=(WorkbookHistoryAnchors){.document_high=f->book.id.high,.document_low=f->book.id.low};
    f->segment=(WorkbookHistorySegment){0,0,4};
    for (size_t i=0;i<4;i++) { f->positions[i]=(WorkbookHistoryEventPosition){i+1,0,i+1}; f->map[i]=i; }
    f->path=(WorkbookHistoryPath){f->book.id.high,f->book.id.low,&f->segment,1,f->positions,4,0,4,0,0,2,4};
    uint64_t a=f->book.sheets[0].id,b=f->book.sheets[1].id;
    packet(f,0,a,5,"","NULL"); packet(f,1,a,5,"NULL","42"); packet(f,2,b,9,"","other"); packet(f,3,a,5,"42","");
    CHECK(cells_set(&f->book.sheets[0].cells,5,3,"42",NULL));
    f->view=(WorkbookHistoryProfileView){.document=f->book.id,.container_version=16,.core_version=15,.kind=HISTORY_PROFILE_RETAINED,.versions=&f->versions,.anchors=&f->anchors,.current={&f->path,f->map,4},.records=f->records,.count=4};
}
static void expect(WorkbookHistoryProfile *p,Workbook *b,WorkbookHistoryCellsStateStatus expected) {
    WorkbookHistoryCellStateEntry scratch[4]; memset(scratch,0x6d,sizeof(scratch));
    WorkbookHistoryCellsStateReport out={99,98,97,96},old=out;
    WorkbookHistoryCellsStateStatus status=workbook_history_cells_state_check(p,b,scratch,sizeof(scratch),&out);
    if (status!=expected) fprintf(stderr,"State status %u, expected %u\n",(unsigned)status,(unsigned)expected);
    CHECK(status==expected);
    if (expected!=HISTORY_CELLS_STATE_OK) CHECK(!memcmp(&out,&old,sizeof(out)));
    else CHECK(out.undo==2 && out.redo==2 && out.undo_cells==1 && out.redo_cells==2);
}
int main(void) {
    Fixture f; fixture(&f); WorkbookHistoryProfile *p=prepare(&f); size_t bytes=0;
    CHECK(workbook_history_cells_state_measure(p,&f.book,&bytes)==HISTORY_CELLS_STATE_OK && bytes==4*sizeof(WorkbookHistoryCellStateEntry));
    expect(p,&f.book,HISTORY_CELLS_STATE_OK);
    uint64_t revision=f.book.sheets[0].cells.revision; size_t undo=f.book.sheets[0].cells.undo_count;
    WorkbookHistoryCellsStateReport out={1,2,3,4},old=out; WorkbookHistoryCellStateEntry scratch[4]; memset(scratch,0x51,sizeof(scratch));
    CHECK(workbook_history_cells_state_check(p,&f.book,scratch,bytes-1,&out)==HISTORY_CELLS_STATE_LIMIT && !memcmp(&out,&old,sizeof(out)));
    for (size_t i=0;i<sizeof(scratch);i++) CHECK(((unsigned char *)scratch)[i]==0x51);
    CHECK(workbook_history_cells_state_check(p,&f.book,NULL,bytes,&out)==HISTORY_CELLS_STATE_INVALID);
    CHECK(f.book.sheets[0].cells.revision==revision && f.book.sheets[0].cells.undo_count==undo && !strcmp(cells_get(&f.book.sheets[0].cells,5,3),"42"));
    CHECK(cells_set(&f.book.sheets[0].cells,5,3,"43",NULL)); expect(p,&f.book,HISTORY_CELLS_STATE_MISMATCH); CHECK(cells_set(&f.book.sheets[0].cells,5,3,"42",NULL));
    uint64_t id=f.book.sheets[1].id; f.book.sheets[1].id=99; f.book.sheet_high_water=99; expect(p,&f.book,HISTORY_CELLS_STATE_MISMATCH); f.book.sheets[1].id=id; f.book.sheet_high_water=2;
    f.book.id.low^=1; expect(p,&f.book,HISTORY_CELLS_STATE_INVALID); f.book.id.low^=1;
    /* Latest after matches, but an earlier causal overwrite is inconsistent. */
    packet(&f,0,f.book.sheets[0].id,5,"","bad"); WorkbookHistoryProfile *bad=prepare(&f); expect(bad,&f.book,HISTORY_CELLS_STATE_MISMATCH); workbook_history_profile_free(&bad);
    packet(&f,0,f.book.sheets[0].id,5,"","NULL");
    /* Late Redo corruption must preserve the scalar output after early checks. */
    packet(&f,3,f.book.sheets[0].id,5,"bad",""); bad=prepare(&f); expect(bad,&f.book,HISTORY_CELLS_STATE_MISMATCH); workbook_history_profile_free(&bad);
    CHECK(workbook_history_cells_state_measure(NULL,&f.book,&bytes)==HISTORY_CELLS_STATE_INVALID);
    /* The admitted original owner remains independent of mutated input packets. */
    expect(p,&f.book,HISTORY_CELLS_STATE_OK);
    /* Empty current interval needs no scratch and does not manufacture Undo. */
    WorkbookHistoryProfileView empty=f.view; WorkbookHistoryPath path=f.path; WorkbookHistorySegment segment={0};
    path.segments=&segment; path.events=NULL; path.event_count=0; path.cursor=path.tail=0;
    empty.current=(WorkbookHistoryProfilePath){&path,NULL,0}; empty.records=NULL; empty.count=0;
    CHECK(workbook_history_profile_prepare(empty,(WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES},&bad)==HISTORY_PROFILE_OK);
    CHECK(workbook_history_cells_state_check(bad,&f.book,NULL,0,&out)==HISTORY_CELLS_STATE_OK && !out.undo && !out.redo && !out.undo_cells && !out.redo_cells);
    workbook_history_profile_free(&bad);
    /* A valid other-family slot refuses before touching scratch/output. */
    FormatStore formats={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&formats,(FormatRange){1,1,1,1},FORMAT_BOLD,bold,NULL)); size_t n=0,written=0;
    CHECK(format_delta_encoded_size(f.book.sheets[0].id,(FormatSnapshotView){0},format_delta_store_view(&formats),&n)==FORMAT_DELTA_OK);
    free(f.packets[3]); f.packets[3]=dx_alloc(n);
    CHECK(format_delta_encode(f.book.sheets[0].id,(FormatSnapshotView){0},format_delta_store_view(&formats),f.packets[3],n,&written)==FORMAT_DELTA_OK);
    SourceVersions ledger={0}; size_t depbytes=0; CHECK(source_versions_asset_size(f.book.sheets[0].id,&ledger,0,&depbytes)==SOURCE_VERSIONS_CODEC_OK);
    void *deps=dx_alloc(depbytes); CHECK(source_versions_asset_encode(f.book.sheets[0].id,&ledger,0,deps,depbytes,&written)==SOURCE_VERSIONS_CODEC_OK);
    f.frames[3].family=DX_HISTORY_FORMAT_FAMILY; f.frames[3].encoding=DX_FORMAT_DELTA_ENCODING; f.frames[3].bytes=n;
    f.frames[3].dependency_encoding=DX_SOURCE_VERSIONS_ENCODING; f.frames[3].dependency_bytes=depbytes;
    f.records[3].data=f.packets[3]; f.records[3].bytes=n; f.records[3].dependencies=deps; f.records[3].dependency_bytes=depbytes; f.records[3].intent.effects=8;
    bad=prepare(&f); old=out;
    CHECK(workbook_history_cells_state_check(bad,&f.book,scratch,sizeof(scratch),&out)==HISTORY_CELLS_STATE_UNSUPPORTED && !memcmp(&old,&out,sizeof(out)));
    workbook_history_profile_free(&bad); free(deps); format_free(&formats);
    workbook_history_profile_free(&p); for (size_t i=0;i<4;i++) free(f.packets[i]); workbook_free(&f.book);
    printf("PASS: %u actual current cell-path state checks\n",checks); return 0;
}
