#include "workbook/sheet_presence_asset.h"
#include "workbook/history.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet presence:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook fixture(void) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Snapshot İ",&(Analysis){.kind=SOURCE_DATABASE,.path="/external.duckdb"},NULL));
    Worksheet *s=b.sheets; char *error=NULL;
    CHECK(cells_set(&s->cells,10,1,"NULL",&error)); CHECK(cells_set(&s->cells,11,1,"9007199254740993.12345678901234567890",&error));
    CHECK(cells_set(&s->cells,12,1,"İstanbul\t東京\ntext",&error)); CHECK(cells_set(&s->cells,13,1,"",&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(objects_add_table(&s->objects,20,4,"Input",&(Analysis){.kind=SOURCE_CSV,.path="/missing.csv"},&error)==0); worksheet_record(s,EDIT_OBJECTS);
    SourceVersion row={.version=1,.owner=s->objects.items[0].id,.kind=SOURCE_CSV,.path="/missing.csv"};
    SourceVersions ledger={0}; CHECK(source_versions_plan(&s->source_versions,&row,true,&ledger,&error)); s->source_versions=ledger; s->objects.items[0].source.source_version=1;
    CHECK(objects_add(&s->objects,30,4,"Summary",&error)==1); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_bind_source(&s->objects,1,s->objects.items[0].id,&error)); worksheet_record(s,EDIT_OBJECTS);
    Column columns[2]={{.name="Id",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}},
        {.name="Amount",.type=DUCKDB_TYPE_DECIMAL,.type_name="DECIMAL(38,4)",.numeric=true,.number_type={NUM_DECIMAL,38,4}}};
    Page schema={.columns=columns,.column_count=2}; SourceDigest digest={.bytes=100,.observed_stamp=11}; digest.identity.size=100; digest.sha256[0]=17;
    SourcePacket raw={0}; CHECK(source_scope_raw_build(1,1,&s->objects.items[0].source,&schema,&digest,"engine",&raw,&error));
    row.raw=raw; ledger=(SourceVersions){0}; CHECK(source_versions_plan(&s->source_versions,&row,false,&ledger,&error)); source_versions_free(&s->source_versions); s->source_versions=ledger;
    SourcePacket result={0}; CHECK(source_scope_result_build(&raw,&s->objects.items[0].source,&result,&error));
    FormatScopeView raw_scope={1,raw.data,raw.bytes},result_scope={1,result.data,result.bytes};
    uint64_t records[]={1,UINT64_C(9007199254740993)}; size_t fields[]={0,1}; CellFormat style=format_default(); style.color=2;
    CHECK(format_apply_records(&s->formats,&raw_scope,records,2,fields,2,FORMAT_COLOR,style,&error)); worksheet_record(s,EDIT_FORMAT);
    style.flags=FORMAT_ITALIC; CHECK(format_apply_scoped(&s->formats,(FormatRange){40,40,7,7},FORMAT_ITALIC,style,&result_scope,1,true,&error)); worksheet_record(s,EDIT_FORMAT);
    source_packet_free(&raw); source_packet_free(&result);
    row=(SourceVersion){.version=2,.owner=88,.kind=SOURCE_PARQUET,.path="/retired.parquet"}; ledger=(SourceVersions){0};
    CHECK(source_versions_plan(&s->source_versions,&row,true,&ledger,&error)); source_versions_free(&s->source_versions); s->source_versions=ledger;
    s->objects.next_id=99; s->source_versions.high_water=77; b.sheet_high_water=UINT64_MAX;
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&s->formats,(FormatRange){10,12,1,1},FORMAT_BOLD,bold,&error)); worksheet_record(s,EDIT_FORMAT);
    s->view=(SheetView){.row=10,.col=1,.anchor_row=12,.anchor_col=3,.end_row=10,.end_col=1,.top=7,.horizontal=91.125f,.fraction=.25,.selection=2,.canvas_zoom=1.75f,.column_count=16384};
    s->view.columns=dx_alloc(16384*sizeof(char *)); s->view.widths=dx_alloc(16384*sizeof(float));
    for (size_t i=0;i<16384;i++) { s->view.columns[i]=dx_strdup(i ? "" : "Exact"); s->view.widths[i]=96; }
    s->view.widths[0]=52; s->view.widths[1]=nextafterf(96,100); s->view.widths[16383]=1200; s->hidden=true;
    CHECK(!error); return b;
}
static void capture(void) {
    Workbook b=fixture(); Worksheet *s=b.sheets; Analysis actual=analysis_copy(&s->analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    char *error=NULL; CHECK(analysis_filter_set(&actual,"City",FILTER_EQUAL,"",&error) && !error);
    SheetPresenceView view={b.id,b.sheet_high_water,s,&actual}; SheetPresenceMeasure m={0}; SheetPresenceAsset *asset=NULL;
    CHECK(sheet_presence_measure(view,&m)==SHEET_PRESENCE_OK && m.owned && m.staging==m.owned+m.validation_scratch);
    CHECK(sheet_presence_capture(view,m.staging-1,&asset,NULL)==SHEET_PRESENCE_LIMIT && !asset && !s->analysis.schema);
    CHECK(sheet_presence_capture(view,m.staging,&asset,NULL)==SHEET_PRESENCE_OK && asset);
    SheetPresenceView copy=sheet_presence_view(asset); const Worksheet *owned=copy.sheet;
    CHECK(copy.document.high==b.id.high && copy.document.low==b.id.low && copy.sheet_high_water==UINT64_MAX);
    CHECK(owned->id==s->id && owned->hidden && !strcmp(owned->name,s->name) && owned->name!=s->name);
    CHECK(!strcmp(copy.primary->schema,"main") && !strcmp(copy.primary->table,"alpha") && copy.primary->schema!=actual.schema && copy.primary->path!=actual.path);
    CHECK(owned->cells.count==3 && owned->cells.cells!=s->cells.cells && owned->cells.cells[0].text!=s->cells.cells[0].text);
    CHECK(!strcmp(cells_get(&owned->cells,10,1),"NULL") && !*cells_get(&owned->cells,13,1) && !strcmp(cells_get(&owned->cells,11,1),"9007199254740993.12345678901234567890"));
    CHECK(!owned->edit_session_id && !owned->cells.revision && !owned->undo_actions_count && !owned->cells.undo_count && !owned->objects.undo_count && !owned->formats.undo_count && !owned->width_history && !owned->width_revision);
    CHECK(owned->objects.next_id==99 && owned->objects.items[1].source_id==s->objects.items[0].id && owned->objects.items[0].name!=s->objects.items[0].name);
    CHECK(owned->source_versions.high_water==77 && owned->source_versions.items!=s->source_versions.items && owned->source_versions.items[0].path!=s->source_versions.items[0].path && owned->source_versions.items[0].raw.bytes && owned->source_versions.items[0].raw.data!=s->source_versions.items[0].raw.data);
    CHECK(owned->source_versions.count==2 && owned->source_versions.items[1].owner==88 && !owned->source_versions.items[1].raw.bytes);
    CHECK(copy.primary->filter_value && !*copy.primary->filter_value && copy.primary->filter_value!=actual.filter_value);
    CHECK(owned->formats.scope_count==2 && owned->formats.scopes[0].definition!=s->formats.scopes[0].definition && owned->formats.items[0].record_ids!=s->formats.items[0].record_ids && owned->formats.items[0].record_ids[1]==UINT64_C(9007199254740993));
    CHECK(owned->view.widths!=s->view.widths && owned->view.columns[0]!=s->view.columns[0] && owned->view.widths[0]==52 && owned->view.widths[16383]==1200 && owned->view.column_count==16384);
    CHECK(!memcmp(owned->view.widths,s->view.widths,16384*sizeof(float)));
    CHECK(owned->view.row==10 && owned->view.top==7 && owned->view.horizontal==91.125f && owned->view.fraction==.25 && owned->view.canvas_zoom==1.75f && owned->view.selection==2);
    CHECK(owned->formats.items!=s->formats.items && (format_at(&owned->formats,10,1).flags&FORMAT_BOLD));
    SheetPresenceAsset *before=asset; s->view.widths[0]=NAN;
    CHECK(sheet_presence_capture(view,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_INVALID && asset==before && owned->view.widths[0]==52); s->view.widths[0]=52;
    view.primary=NULL; CHECK(sheet_presence_measure(view,&m)==SHEET_PRESENCE_INVALID); view.primary=&actual;
    actual.source_version=1; CHECK(sheet_presence_capture(view,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_INVALID && asset==before); actual.source_version=0;
    uint64_t id=s->cells.cells[1].row; s->cells.cells[1].row=s->cells.cells[0].row;
    CHECK(sheet_presence_capture(view,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_INVALID && asset==before); s->cells.cells[1].row=id;
    size_t bytes=s->cells.bytes; s->cells.bytes++; CHECK(sheet_presence_measure(view,&m)==SHEET_PRESENCE_INVALID); s->cells.bytes=bytes;
    s->objects.items[1].source_id=88; CHECK(sheet_presence_capture(view,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_INVALID && asset==before); s->objects.items[1].source_id=1;
    SourcePacket saved=s->source_versions.items[0].raw; s->source_versions.items[0].raw=(SourcePacket){0}; s->source_versions.bytes-=saved.bytes;
    CHECK(sheet_presence_capture(view,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_INVALID && asset==before); s->source_versions.items[0].raw=saved; s->source_versions.bytes+=saved.bytes;
    size_t field=s->formats.items[0].fields[1]; s->formats.items[0].fields[1]=2;
    CHECK(sheet_presence_capture(view,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_INVALID && asset==before); s->formats.items[0].fields[1]=field;
    uint64_t owner=s->formats.scopes[0].owner; s->formats.scopes[0].owner=99;
    CHECK(sheet_presence_capture(view,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_INVALID && asset==before); s->formats.scopes[0].owner=owner;
    SheetPresenceMeasure sentinel={1,2,3}; m=sentinel; view.sheet_high_water=0;
    CHECK(sheet_presence_measure(view,&m)==SHEET_PRESENCE_INVALID && !memcmp(&m,&sentinel,sizeof(m))); view.sheet_high_water=b.sheet_high_water;
    /* Borrowed repetitive metadata proves the cap without allocating 32 MiB. */
    char long_name[2049]; memset(long_name,'A',sizeof(long_name)-1); long_name[sizeof(long_name)-1]=0;
    char **names=dx_alloc(s->view.column_count*sizeof(char *)); for (size_t i=0;i<s->view.column_count;i++) names[i]=long_name;
    char **old_names=s->view.columns; s->view.columns=names;
    CHECK(sheet_presence_measure(view,&m)==SHEET_PRESENCE_LIMIT);
    CHECK(sheet_presence_capture(view,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_LIMIT && asset==before);
    s->view.columns=old_names; free(names);
    /* Existing immutable owner can be the source of its atomic replacement. */
    CHECK(sheet_presence_capture(copy,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_OK);
    analysis_free(&actual); workbook_free(&b); copy=sheet_presence_view(asset);
    CHECK(!strcmp(copy.primary->table,"alpha") && copy.sheet->view.widths[16383]==1200 && copy.sheet->source_versions.high_water==77);
    sheet_presence_free(&asset); CHECK(!asset); sheet_presence_free(&asset); sheet_presence_free(NULL);
}
int main(void) { capture(); CHECK(!sheet_presence_view(NULL).sheet); printf("PASS: %u independent sheet presence ownership checks\n",checks); return 0; }
