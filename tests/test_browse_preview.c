#include "workbook/browse_preview.h"
#include "workbook/analysis_asset.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,releases;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr,"FAIL browse preview:%d: %s\n",__LINE__,#c); exit(1); } } while (0)
typedef struct {
    WorkbookId document;
    uint64_t session,sheet,sheet_session,high_water,metadata,request,query,source;
    Analysis definition;
    Page page;
    SourceVersions sources;
    RangeResultProof proof;
    RangeResultContext resolver;
    BrowsePreviewContext context;
    int owner;
} Fixture;
static bool resolve(void *context,uint64_t owner,RangeResultProof *out) { Fixture *f=context; if (owner!=f->context.owner) return false; *out=f->proof; return true; }
static bool other_resolve(void *context,uint64_t owner,RangeResultProof *out) { return resolve(context,owner,out); }
static void release_text(void *text) { releases++; free(text); }
static void start(Fixture *f) {
    memset(f,0,sizeof(*f)); f->document=(WorkbookId){17,29}; f->session=2; f->sheet=7; f->sheet_session=3; f->request=5; f->query=f->source=9;
    f->definition=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("/data/O'Brien-İstanbul-表.csv")};
    f->page=(Page){.columns=dx_alloc(3*sizeof(Column)),.cells=dx_alloc(6*sizeof(Cell)),.column_count=3,.row_count=2,.offset=100,.total=102,.release_cell_text=release_text};
    f->page.columns[0]=(Column){.name=dx_strdup("Nullable"),.type_name=dx_strdup("VARCHAR"),.caption=dx_strdup(""),.detail=dx_strdup("NULL"),.type=DUCKDB_TYPE_VARCHAR};
    f->page.columns[1]=(Column){.name=dx_strdup("Exact"),.type_name=dx_strdup("DECIMAL(38,18)"),.type=DUCKDB_TYPE_DECIMAL,.number_type={NUM_DECIMAL,38,18},.numeric=true};
    f->page.columns[2]=(Column){.name=dx_strdup("Float"),.type_name=dx_strdup("DOUBLE"),.type=DUCKDB_TYPE_DOUBLE,.number_type={NUM_FLOAT,0,0},.numeric=true};
    const char *texts[]={"NULL decoration","","NULL","9007199254740993.012345678901234567","NaN","Infinity"};
    for (size_t i=0;i<6;i++) f->page.cells[i].text=dx_strdup(texts[i]);
    f->page.cells[0].is_null=true; f->page.cells[3].number=-0.0; f->page.cells[4].number=NAN; f->page.cells[5].number=INFINITY;
    f->page.records=(PageRecords){.ids=dx_alloc(2*sizeof(uint64_t)),.source_key=dx_strdup("actual reader key"),.count=1,.stamp=77,.file_identity_valid=true,.file_identity={.device=31,.inode=41,.size=500,.links=1}};
    f->page.records.ids[0]=1901;
    f->proof=(RangeResultProof){.object=0,.request=f->request,.query_epoch=f->query,.source_epoch=f->source,.owner=&f->owner,.definition=&f->definition,.page=&f->page,.ready=true};
    f->resolver=(RangeResultContext){resolve,f};
    f->context=(BrowsePreviewContext){.document=&f->document,.document_session=&f->session,.sheet=&f->sheet,.sheet_session=&f->sheet_session,
        .object_high_water=&f->high_water,.metadata_clock=&f->metadata,.request_epoch=&f->request,.query_epoch=&f->query,.source_epoch=&f->source,
        .definition=&f->definition,.result_owner=&f->owner,.sources=&f->sources,.results=&f->resolver,.other_owned=sizeof(*f)};
}
static void finish(Fixture *f) { page_free(&f->page); analysis_free(&f->definition); source_versions_free(&f->sources); }
static BrowsePreviewInfo measured(Fixture *f,BrowsePreviewTarget *target,BrowsePreview *prior) {
    BrowsePreviewInfo info={0}; CHECK(browse_preview_prepare(&f->context,target)==BROWSE_PREVIEW_OK);
    CHECK(browse_preview_measure(&f->context,target,prior,&info)==BROWSE_PREVIEW_OK); return info;
}
static void preserved(Fixture *f,BrowsePreviewTarget *target,BrowsePreview **preview,BrowsePreviewStatus status) {
    BrowsePreview *before=*preview; const Page *page=browse_preview_page(before); const Cell *cells=page ? page->cells : NULL;
    CHECK(browse_preview_capture(&f->context,target,SIZE_MAX,preview)==status);
    CHECK(*preview==before && (!page || browse_preview_page(*preview)->cells==cells));
}
static void ownership(void) {
    Fixture f; start(&f); BrowsePreview *preview=NULL; BrowsePreviewTarget target; BrowsePreviewInfo info=measured(&f,&target,NULL);
    CHECK(info.rows==2 && info.columns==3 && info.records==1 && info.offset==100 && info.total==102 && !info.count_pending);
    CHECK(info.coexistence==info.owned+info.validation_scratch+sizeof(target)+f.context.other_owned);
    CHECK(browse_preview_capture(&f.context,&target,info.coexistence-1,&preview)==BROWSE_PREVIEW_LIMIT && !preview);
    CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK);
    const Page *copy=browse_preview_page(preview); CHECK(copy && copy!=&f.page && copy->columns!=f.page.columns && copy->cells!=f.page.cells && !copy->release_cell_text);
    CHECK(copy->cells[0].is_null && !strcmp(copy->cells[0].text,"NULL decoration") && !copy->cells[1].is_null && !*copy->cells[1].text && !strcmp(copy->cells[2].text,"NULL"));
    CHECK(!strcmp(copy->cells[3].text,"9007199254740993.012345678901234567") && !memcmp(&copy->cells[3].number,&f.page.cells[3].number,sizeof(double)));
    CHECK(isnan(copy->cells[4].number) && isinf(copy->cells[5].number));
    CHECK(!memcmp(&copy->cells[4].number,&f.page.cells[4].number,sizeof(double)) && !memcmp(&copy->cells[5].number,&f.page.cells[5].number,sizeof(double)));
    CHECK(copy->columns[1].number_type.precision==38 && copy->columns[1].number_type.scale==18 && copy->columns[0].caption && !*copy->columns[0].caption && !strcmp(copy->columns[0].detail,"NULL"));
    CHECK(copy->records.count==1 && copy->records.ids!=f.page.records.ids && copy->records.ids[0]==1901 && copy->records.file_identity_valid && copy->records.file_identity.inode==41);
    uint64_t id=0; CHECK(page_record_id(copy,100,&id) && id==1901 && !page_record_id(copy,101,&id));
    size_t bytes=0; const void *definition=browse_preview_definition(preview,&bytes); AnalysisAsset decoded={0}; DefinitionAssetInfo framed={0};
    CHECK(analysis_asset_preflight(definition,bytes,0,&framed)==ANALYSIS_ASSET_OK && framed.sheet==7 && !framed.owner);
    CHECK(analysis_asset_decode(definition,bytes,0,framed.measure.owned+framed.measure.validation_scratch,&decoded)==ANALYSIS_ASSET_OK);
    CHECK(decoded.definition.path!=f.definition.path && !strcmp(decoded.definition.path,f.definition.path)); analysis_asset_free(&decoded);
    info=measured(&f,&target,preview); CHECK(info.coexistence==2*info.owned+info.validation_scratch+sizeof(target)+f.context.other_owned);
    BrowsePreview *prior=preview; CHECK(browse_preview_capture(&f.context,&target,info.coexistence-1,&preview)==BROWSE_PREVIEW_LIMIT && preview==prior);
    CHECK(!releases); finish(&f); CHECK(releases==6 && !strcmp(copy->cells[3].text,"9007199254740993.012345678901234567"));
    browse_preview_free(preview); CHECK(releases==6);
}
static void stale(void) {
    Fixture f; start(&f); BrowsePreview *preview=NULL; BrowsePreviewTarget target; BrowsePreviewInfo info=measured(&f,&target,NULL);
    CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK);
    uint64_t *clocks[]={&f.document.low,&f.session,&f.sheet,&f.sheet_session,&f.high_water,&f.metadata,&f.request,&f.query,&f.source};
    for (size_t i=0;i<sizeof(clocks)/sizeof(*clocks);i++) { measured(&f,&target,preview); (*clocks[i])++; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE); (*clocks[i])--; }
    measured(&f,&target,preview); f.resolver.resolve=other_resolve; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE); f.resolver.resolve=resolve;
    measured(&f,&target,preview); f.proof.object=99; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE); f.proof.object=0;
    measured(&f,&target,preview); f.proof.owner=&f; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE); f.proof.owner=&f.owner;
    measured(&f,&target,preview); Page same=f.page; f.proof.page=&same; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE); f.proof.page=&f.page;
    measured(&f,&target,preview); f.proof.failed=true; preserved(&f,&target,&preview,BROWSE_PREVIEW_UNAVAILABLE); f.proof.failed=false;
    measured(&f,&target,preview); f.proof.ready=false; preserved(&f,&target,&preview,BROWSE_PREVIEW_UNAVAILABLE); f.proof.ready=true;
    measured(&f,&target,preview); f.page.count_pending=true; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE);
    measured(&f,&target,preview); f.page.total++; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE); f.page.total--;
    measured(&f,&target,preview); Cell *cells=dx_alloc(6*sizeof(Cell)); memcpy(cells,f.page.cells,6*sizeof(Cell)); free(f.page.cells); f.page.cells=cells; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE);
    measured(&f,&target,preview); Column *columns=dx_alloc(3*sizeof(Column)); memcpy(columns,f.page.columns,3*sizeof(Column)); free(f.page.columns); f.page.columns=columns; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE);
    measured(&f,&target,preview); f.sources.items=dx_alloc(sizeof(SourceVersion)); f.sources.items[0]=(SourceVersion){.owner=0,.version=1,.kind=SOURCE_CSV,.path=dx_strdup(f.definition.path)};
    f.sources.count=f.sources.capacity=f.sources.high_water=1; f.sources.bytes=sizeof(SourceVersion)+strlen(f.definition.path)+1; f.definition.source_version=1;
    preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE);
    info=measured(&f,&target,preview); CHECK(info.binding_pending && info.source_version==1);
    SourceDigest digest={.identity={.size=500},.bytes=500,.sha256={1}}; SourcePacket raw={0}; CHECK(source_scope_raw_build(0,1,&f.definition,&f.page,&digest,"test-engine",&raw,NULL));
    f.sources.items[0].raw=raw; f.sources.bytes+=raw.bytes; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE);
    info=measured(&f,&target,preview); CHECK(!info.binding_pending); CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK);
    browse_preview_free(preview); finish(&f);
}
static void profiles(void) {
    Fixture f; start(&f); BrowsePreviewTarget target; BrowsePreview *preview=NULL; BrowsePreviewInfo info;
    f.context.owner=7; f.proof.object=7; f.high_water=10;
    f.page.count_pending=true; info=measured(&f,&target,NULL); CHECK(info.count_pending && info.total==102);
    CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK && browse_preview_page(preview)->count_pending);
    CHECK(browse_preview_info(preview).owner==7 && browse_preview_info(preview).object_high_water==10);
    f.page.records.count=0; info=measured(&f,&target,preview); CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK);
    CHECK(!browse_preview_page(preview)->records.ids && !browse_preview_page(preview)->records.count && browse_preview_page(preview)->records.file_identity_valid && browse_preview_page(preview)->records.stamp==77);
    for (SourceKind kind=SOURCE_PARQUET;kind<=SOURCE_DATABASE;kind++) {
        f.definition.kind=kind;
        if (kind==SOURCE_DATABASE) { CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_UNSUPPORTED); f.definition.table=dx_strdup("sales"); }
        info=measured(&f,&target,preview); CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK);
    }
    free(f.definition.table); f.definition.table=NULL; f.definition.kind=SOURCE_CSV;
    f.page.row_count=0; f.page.total=0; f.page.offset=1000; f.page.count_pending=false;
    info=measured(&f,&target,preview); CHECK(!info.rows && !info.total && !info.count_pending);
    CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK && !browse_preview_page(preview)->cells);
    f.page.count_pending=true; info=measured(&f,&target,preview); CHECK(info.count_pending && !info.rows && !info.total);
    f.page.row_count=2; f.page.total=102; f.page.offset=100; f.definition.kind=SOURCE_CSV;
    f.page.column_count=257; CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_LIMIT); f.page.column_count=3;
    f.page.row_count=257; CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_LIMIT); f.page.row_count=2;
    f.page.offset=UINT64_MAX; CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_INVALID); f.page.offset=100;
    f.page.total=101; CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_INVALID); f.page.total=102;
    f.page.other=(PageOther *)(uintptr_t)1; CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_UNSUPPORTED); f.page.other=NULL;
    f.page.totals=(PivotTotals *)(uintptr_t)1; CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_UNSUPPORTED); f.page.totals=NULL;
    f.page.records.count=3; CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_INVALID); f.page.records.count=1;
    f.page.records.ids[0]=0; CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_INVALID); f.page.records.ids[0]=1901;
    f.page.records.ids[1]=1901; f.page.records.count=2; CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_INVALID); f.page.records.count=1;
    char *old=f.page.cells[1].text; f.page.cells[1].text=dx_alloc(DX_BROWSE_PREVIEW_TEXT_BYTES+2); memset(f.page.cells[1].text,'a',DX_BROWSE_PREVIEW_TEXT_BYTES+1);
    CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_LIMIT); free(f.page.cells[1].text); f.page.cells[1].text=old;
    f.context.other_owned=SIZE_MAX; CHECK(browse_preview_prepare(&f.context,&target)==BROWSE_PREVIEW_OK); preserved(&f,&target,&preview,BROWSE_PREVIEW_LIMIT);
    f.context.other_owned=0; browse_preview_free(preview); finish(&f);
}
static void maximum(void) {
    Fixture f; start(&f); page_free(&f.page);
    f.page=(Page){.column_count=256,.row_count=256,.offset=UINT64_MAX-256,.total=UINT64_MAX,.columns=dx_alloc(256*sizeof(Column)),.cells=dx_alloc(65536*sizeof(Cell))};
    for (size_t i=0;i<256;i++) { f.page.columns[i]=(Column){.name=dx_printf("Field%zu",i),.type_name=dx_strdup("VARCHAR"),.type=DUCKDB_TYPE_VARCHAR}; }
    for (size_t i=0;i<65536;i++) f.page.cells[i].is_null=true;
    BrowsePreviewTarget target; BrowsePreviewInfo info=measured(&f,&target,NULL); BrowsePreview *preview=NULL;
    CHECK(info.rows==256 && info.columns==256 && info.owned<DX_BROWSE_PREVIEW_OWNED_BYTES);
    CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK);
    CHECK(browse_preview_page(preview)->cells[65535].is_null && !browse_preview_page(preview)->cells[65535].text && browse_preview_page(preview)->offset==UINT64_MAX-256);
    browse_preview_free(preview); finish(&f);
}
int main(void) { ownership(); stale(); profiles(); maximum(); printf("PASS: %u bounded browse preview ownership and association checks\n",checks); return 0; }
