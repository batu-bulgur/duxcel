#include "workbook/browse_preview.h"
#include "workbook/analysis_asset.h"
#include "workbook/browse_preview_admission.h"
#include "workbook/browse_preview_codec.h"
#include "workbook/browse_preview_workbook.h"
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
    SheetObject table;
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
    f->page.cells[0].is_null=true; f->page.cells[3].number=-0.0; { uint64_t bits=UINT64_C(0x7ff8000000001234); memcpy(&f->page.cells[4].number,&bits,8); } f->page.cells[5].number=INFINITY;
    f->page.records=(PageRecords){.ids=dx_alloc(2*sizeof(uint64_t)),.source_key=dx_strdup("actual reader key"),.count=1,.stamp=77,.file_identity_valid=true,.file_identity={.device=31,.inode=41,.size=500,.links=1}};
    f->page.records.ids[0]=1901;
    f->proof=(RangeResultProof){.object=0,.request=f->request,.query_epoch=f->query,.source_epoch=f->source,.owner=&f->owner,.definition=&f->definition,.page=&f->page,.ready=true};
    f->resolver=(RangeResultContext){resolve,f};
    f->context=(BrowsePreviewContext){.document=&f->document,.document_session=&f->session,.sheet=&f->sheet,.sheet_session=&f->sheet_session,
        .object_high_water=&f->high_water,.metadata_clock=&f->metadata,.request_epoch=&f->request,.query_epoch=&f->query,.source_epoch=&f->source,
        .definition=&f->definition,.result_owner=&f->owner,.sources=&f->sources,.results=&f->resolver,.other_owned=sizeof(*f)};
}
static void finish(Fixture *f) { page_free(&f->page); analysis_free(&f->definition); analysis_free(&f->table.source); source_versions_free(&f->sources); }
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
    f.page.count_pending=true; info=measured(&f,&target,NULL); CHECK(info.count_pending && info.total==102);
    CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK && browse_preview_page(preview)->count_pending);
    CHECK(browse_preview_info(preview).owner==0 && browse_preview_info(preview).object_high_water==0);
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
static BrowsePreview *wire_copy(const BrowsePreview *preview) {
    BrowsePreviewPacket packet={0}; BrowsePreviewWireInfo measured={0},inspected={0}; BrowsePreview *decoded=NULL;
    CHECK(browse_preview_encode_measure(preview,NULL,&measured)==BROWSE_PREVIEW_OK);
    CHECK(browse_preview_encode(preview,measured.coexistence-1,&packet)==BROWSE_PREVIEW_LIMIT && !packet.data);
    CHECK(browse_preview_encode(preview,measured.coexistence,&packet)==BROWSE_PREVIEW_OK);
    CHECK(browse_preview_preflight(packet.data,packet.bytes,&inspected)==BROWSE_PREVIEW_OK && inspected.owned==browse_preview_info(preview).owned);
    CHECK(browse_preview_decode(packet.data,packet.bytes,inspected.coexistence-1,&decoded)==BROWSE_PREVIEW_LIMIT && !decoded);
    CHECK(browse_preview_decode(packet.data,packet.bytes,inspected.coexistence,&decoded)==BROWSE_PREVIEW_OK);
    BrowsePreviewPacket again={0}; CHECK(browse_preview_encode(decoded,SIZE_MAX,&again)==BROWSE_PREVIEW_OK);
    CHECK(packet.bytes==again.bytes && !memcmp(packet.data,again.data,packet.bytes));
    const Page *before=browse_preview_page(preview),*after=browse_preview_page(decoded);
    CHECK(after->cells!=before->cells || !before->row_count);
    for (size_t i=0;i<before->row_count*before->column_count;i++) {
        CHECK(after->cells[i].is_null==before->cells[i].is_null && !memcmp(&after->cells[i].number,&before->cells[i].number,8));
        CHECK(before->cells[i].text ? after->cells[i].text && !strcmp(before->cells[i].text,after->cells[i].text) && before->cells[i].text!=after->cells[i].text : !after->cells[i].text);
    }
    browse_preview_packet_free(&packet); browse_preview_packet_free(&again); return decoded;
}
static void put_wire(unsigned char *data,size_t at,uint64_t value,unsigned bytes) { for (unsigned i=0;i<bytes;i++) { data[at+i]=(unsigned char)value; value>>=8; } }
static uint64_t get_wire(const unsigned char *data,size_t at,unsigned bytes) { uint64_t value=0; for (unsigned i=0;i<bytes;i++) value|=(uint64_t)data[at+i]<<(8*i); return value; }
static void bad_wire(const unsigned char *data,size_t bytes,BrowsePreview **prior,BrowsePreviewStatus expected) {
    BrowsePreview *old=*prior; const Page *page=browse_preview_page(old);
    static unsigned case_id; case_id++;
    BrowsePreviewStatus got=browse_preview_decode(data,bytes,SIZE_MAX,prior);
    if (got!=expected) fprintf(stderr,"Wire corruption case %u: expected %d, got %d\n",case_id,expected,got);
    CHECK(got==expected && *prior==old && browse_preview_page(*prior)==page);
}
static void wire_corruption(void) {
    Fixture f; start(&f); BrowsePreviewTarget target; BrowsePreviewInfo info=measured(&f,&target,NULL); BrowsePreview *preview=NULL;
    CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK);
    BrowsePreview *decoded=wire_copy(preview); BrowsePreviewPacket packet={0}; CHECK(browse_preview_encode(preview,SIZE_MAX,&packet)==BROWSE_PREVIEW_OK);
    unsigned char *bad=dx_alloc(packet.bytes+1); memcpy(bad,packet.data,packet.bytes);
    for (size_t n=0;n<packet.bytes;n++) { BrowsePreview *old=decoded; CHECK(browse_preview_decode(packet.data,n,SIZE_MAX,&decoded)!=BROWSE_PREVIEW_OK && decoded==old); }
    bad_wire(bad,packet.bytes+1,&decoded,BROWSE_PREVIEW_INVALID);
    const size_t invalid_bytes[]={0,6,24,32,56,124,128,132,136,140,144,148,212,252};
    for (size_t i=0;i<sizeof(invalid_bytes)/sizeof(*invalid_bytes);i++) {
        memcpy(bad,packet.data,packet.bytes); size_t at=invalid_bytes[i];
        if (at==124) bad[at]|=8; else if (at==24 || at==128 || at==132) put_wire(bad,at,0,8*(at==24)+4*(at!=24)); else bad[at]^=1;
        bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID);
    }
    memcpy(bad,packet.data,packet.bytes); put_wire(bad,4,0,2); bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_UNSUPPORTED);
    memcpy(bad,packet.data,packet.bytes); put_wire(bad,72,357,8); put_wire(bad,112,257,4); bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_LIMIT);
    memcpy(bad,packet.data,packet.bytes); put_wire(bad,116,257,4); bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_LIMIT);
    memcpy(bad,packet.data,packet.bytes); put_wire(bad,120,3,4); bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID);
    memcpy(bad,packet.data,packet.bytes); put_wire(bad,64,UINT64_MAX,8); bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID);
    memcpy(bad,packet.data,packet.bytes); put_wire(bad,72,101,8); bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID);
    size_t definition_bytes=(size_t)get_wire(packet.data,128,4),source_bytes=(size_t)get_wire(packet.data,132,4);
    size_t schema=256+definition_bytes+source_bytes,cells=schema+(size_t)get_wire(packet.data,136,4),records=cells+(size_t)get_wire(packet.data,140,4);
    const size_t schema_bad[]={schema+4,schema+7,schema+10,schema+12,schema+20+4};
    for (size_t i=0;i<sizeof(schema_bad)/sizeof(*schema_bad);i++) { memcpy(bad,packet.data,packet.bytes); bad[schema_bad[i]]=i==4 ? 0xc0 : 255; bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID); }
    memcpy(bad,packet.data,packet.bytes); put_wire(bad,schema,0,4); bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID);
    memcpy(bad,packet.data,packet.bytes); put_wire(bad,schema,255,4); bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_UNSUPPORTED);
    memcpy(bad,packet.data,packet.bytes); bad[cells]=2; bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID);
    memcpy(bad,packet.data,packet.bytes); bad[cells+13]=0x80; bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID);
    memcpy(bad,packet.data,packet.bytes); put_wire(bad,records,0,8); bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID);
    memcpy(bad,packet.data,packet.bytes); bad[256+definition_bytes+8]^=1; bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID);
    memcpy(bad,packet.data,packet.bytes); put_wire(bad,40,10,8); put_wire(bad,256+16,1,8); bad_wire(bad,packet.bytes,&decoded,BROWSE_PREVIEW_INVALID);
    BrowsePreviewWireInfo inspected={0}; CHECK(browse_preview_preflight(packet.data,packet.bytes,&inspected)==BROWSE_PREVIEW_OK);
    BrowsePreview *old=decoded; CHECK(browse_preview_decode(packet.data,packet.bytes,inspected.coexistence+browse_preview_info(decoded).owned-1,&decoded)==BROWSE_PREVIEW_LIMIT && decoded==old);
    CHECK(browse_preview_decode((void *)(uintptr_t)1,SIZE_MAX,SIZE_MAX,&decoded)==BROWSE_PREVIEW_LIMIT && decoded==old);
    free(bad); browse_preview_packet_free(&packet); browse_preview_free(decoded); browse_preview_free(preview); finish(&f);
}
static void table_owner(void) {
    Fixture f; start(&f); f.table=(SheetObject){.id=7,.kind=OBJECT_TABLE,.revision=9,.rows=5,.columns=3,.source=analysis_copy(&f.definition)};
    f.high_water=10; f.context.owner=7; f.context.table=&f.table; f.context.definition=&f.table.source;
    f.context.query_epoch=f.context.source_epoch=&f.table.revision; f.proof.object=7; f.proof.definition=&f.table.source;
    BrowsePreviewTarget target; BrowsePreview *preview=NULL; BrowsePreviewInfo info=measured(&f,&target,NULL);
    CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&preview)==BROWSE_PREVIEW_OK);
    BrowsePreview *copy=wire_copy(preview); BrowsePreviewLive live={.document=f.document,.sheet=f.sheet,.owner=7,.object_high_water=f.high_water,.definition=&f.table.source,.sources=&f.sources,.other_owned=sizeof(f),.table=&f.table}; SourceDependencyAdmission report={0};
    CHECK(browse_preview_admit(copy,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_EXACT && report.unbound_sources==1);
    f.table.kind=OBJECT_PIVOT; preserved(&f,&target,&preview,BROWSE_PREVIEW_INVALID);
    CHECK(browse_preview_admit(copy,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_CONFLICT); f.table.kind=OBJECT_TABLE;
    measured(&f,&target,preview); f.table.revision++; preserved(&f,&target,&preview,BROWSE_PREVIEW_STALE); f.table.revision--;
    live.object_high_water=7; CHECK(browse_preview_admit(copy,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_UNAVAILABLE);
    browse_preview_free(copy); browse_preview_free(preview); finish(&f);
}
static void association(void) {
    Fixture f; start(&f); BrowsePreview *unbound=NULL,*pending=NULL,*complete=NULL; BrowsePreviewTarget target;
    BrowsePreviewInfo info=measured(&f,&target,NULL); CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&unbound)==BROWSE_PREVIEW_OK);
    BrowsePreviewLive live={.document=f.document,.sheet=f.sheet,.owner=0,.object_high_water=f.high_water,.definition=&f.definition,.sources=&f.sources,.other_owned=sizeof(f)};
    SourceDependencyAdmission report={0}; size_t reserved=0;
    CHECK(browse_preview_admission_measure(unbound,live,&reserved)==BROWSE_PREVIEW_OK);
    CHECK(browse_preview_admit(unbound,live,reserved,&report)==SOURCE_DEPENDENCY_EXACT && report.unbound_sources==1);
    SourceDependencyAdmission saved=report;
    CHECK(browse_preview_admit(unbound,live,reserved-1,&report)==SOURCE_DEPENDENCY_LIMIT && !memcmp(&report,&saved,sizeof(report)));
    f.sources.items=dx_alloc(sizeof(SourceVersion)); f.sources.items[0]=(SourceVersion){.owner=0,.version=1,.kind=SOURCE_CSV,.path=dx_strdup(f.definition.path)};
    f.sources.count=f.sources.capacity=f.sources.high_water=1; f.sources.bytes=sizeof(SourceVersion)+strlen(f.definition.path)+1; f.definition.source_version=1;
    CHECK(browse_preview_admit(unbound,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_CONFLICT && report.unbound_sources==1);
    info=measured(&f,&target,NULL); CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&pending)==BROWSE_PREVIEW_OK);
    CHECK(browse_preview_admit(pending,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_EXACT && report.sources.captured_pending==1 && !report.unbound_sources);
    size_t bytes=0; const void *data=browse_preview_sources(pending,&bytes); SourceVersionsAssetInfo framed={0}; SourceVersionsAsset decoded={0};
    CHECK(source_versions_asset_preflight(data,bytes,0,&framed)==SOURCE_VERSIONS_CODEC_OK && framed.count==1 && framed.high_water==1);
    CHECK(source_versions_asset_decode(data,bytes,0,framed.owned,&decoded)==SOURCE_VERSIONS_CODEC_OK);
    CHECK(decoded.versions.items[0].path!=f.sources.items[0].path && !decoded.versions.items[0].raw.bytes); source_versions_asset_free(&decoded);
    SourceDigest digest={.identity={.size=500},.bytes=500,.sha256={1}}; SourcePacket raw={0};
    CHECK(source_scope_raw_build(0,1,&f.definition,&f.page,&digest,"test-engine",&raw,NULL)); f.sources.items[0].raw=raw; f.sources.bytes+=raw.bytes;
    CHECK(browse_preview_admit(pending,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_REFINED && report.sources.captured_pending==1 && report.sources.refined==1);
    CHECK(browse_preview_info(pending).binding_pending);
    info=measured(&f,&target,NULL); CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&complete)==BROWSE_PREVIEW_OK);
    CHECK(browse_preview_admit(complete,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_EXACT && report.sources.exact==1 && !report.sources.captured_pending);
    digest.sha256[0]=2; SourcePacket replacement={0}; CHECK(source_scope_raw_build(0,1,&f.definition,&f.page,&digest,"test-engine",&replacement,NULL));
    CHECK(replacement.bytes==raw.bytes); source_packet_free(&f.sources.items[0].raw); f.sources.items[0].raw=replacement;
    CHECK(browse_preview_admit(complete,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_CONFLICT && report.sources.conflicting==1);
    f.sources.bytes-=replacement.bytes; source_packet_free(&f.sources.items[0].raw);
    CHECK(browse_preview_admit(complete,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_UNAVAILABLE && report.sources.unavailable==1);
    live.sources=NULL; CHECK(browse_preview_admit(pending,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_UNAVAILABLE); live.sources=&f.sources;
    live.document.low++; CHECK(browse_preview_admit(pending,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_CONFLICT && report.sources.captured_pending==1); live.document=f.document;
    live.sheet++; CHECK(browse_preview_admit(pending,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_CONFLICT); live.sheet=f.sheet;
    live.owner=9; CHECK(browse_preview_admit(pending,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_CONFLICT); live.owner=0;
    f.definition.source_version=0; CHECK(browse_preview_admit(pending,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_CONFLICT); f.definition.source_version=1;
    const BrowsePreview *assets[]={unbound,pending,complete};
    for (size_t i=0;i<3;i++) {
        SourceDependencyAdmission original={0},roundtrip={0}; BrowsePreview *copy=wire_copy(assets[i]);
        SourceDependencyStatus expected=browse_preview_admit(assets[i],live,SIZE_MAX,&original);
        CHECK(browse_preview_admit(copy,live,SIZE_MAX,&roundtrip)==expected && !memcmp(&original,&roundtrip,sizeof(original)));
        browse_preview_free(copy);
    }
    saved=report; unsigned char *bad=(unsigned char *)browse_preview_sources(pending,&bytes); unsigned char old=bad[24]; bad[24]=2;
    CHECK(browse_preview_admit(pending,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_INVALID && !memcmp(&report,&saved,sizeof(report))); bad[24]=old;
    browse_preview_free(unbound); browse_preview_free(pending); browse_preview_free(complete); finish(&f);
}
static void workbook_association(void) {
    Fixture f; start(&f); BrowsePreviewTarget target; BrowsePreview *primary=NULL,*table_preview=NULL;
    BrowsePreviewInfo info=measured(&f,&target,NULL); CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&primary)==BROWSE_PREVIEW_OK);
    f.table=(SheetObject){.id=7,.kind=OBJECT_TABLE,.revision=9,.rows=5,.columns=3,.source=analysis_copy(&f.definition)};
    f.high_water=10; f.context.owner=7; f.context.table=&f.table; f.context.definition=&f.table.source;
    f.context.query_epoch=f.context.source_epoch=&f.table.revision; f.proof.object=7; f.proof.definition=&f.table.source;
    info=measured(&f,&target,NULL); CHECK(browse_preview_capture(&f.context,&target,info.coexistence,&table_preview)==BROWSE_PREVIEW_OK);
    Workbook book={.id=f.document,.sheet_high_water=9,.command_revision=500,.count=2,.active=1,.user_scale=1,.edit_session_id=999};
    book.sheets=dx_alloc(2*sizeof(Worksheet));
    book.sheets[0]=(Worksheet){.id=7,.name=dx_strdup("Connected"),.analysis=analysis_copy(&f.definition),.edit_session_id=888,.view={.canvas_zoom=1}};
    book.sheets[1]=(Worksheet){.id=8,.name=dx_strdup("Other"),.edit_session_id=777,.view={.canvas_zoom=1}};
    Worksheet *sheet=book.sheets; sheet->objects.items=dx_alloc(2*sizeof(SheetObject)); sheet->objects.count=2; sheet->objects.next_id=10;
    sheet->objects.items[0]=(SheetObject){.id=7,.kind=OBJECT_TABLE,.name=dx_strdup("Connected Table"),.row=330,.col=16,.rows=5,.columns=3,.revision=12345,.source=analysis_copy(&f.table.source)};
    sheet->objects.items[1]=(SheetObject){.id=9,.kind=OBJECT_PIVOT,.name=dx_strdup("Derived"),.row=350,.col=16,.rows=14,.columns=6,.source_id=7,.revision=54321};
    CHECK(workbook_identity_valid(&book));
    const BrowsePreview *assets[]={primary,table_preview};
    BrowsePreviewWorkbookAdmission report={0};
    for (size_t i=0;i<2;i++) {
        size_t reserved=0; CHECK(browse_preview_workbook_measure(assets[i],&book,sizeof(book),&reserved)==BROWSE_PREVIEW_OK);
        CHECK(browse_preview_workbook_admit(assets[i],&book,sizeof(book),reserved,&report)==SOURCE_DEPENDENCY_EXACT && report.classification==BROWSE_PREVIEW_CAPTURED_ONLY);
        BrowsePreviewWorkbookAdmission saved=report;
        CHECK(browse_preview_workbook_admit(assets[i],&book,sizeof(book),reserved-1,&report)==SOURCE_DEPENDENCY_LIMIT && !memcmp(&report,&saved,sizeof(report)));
    }
    Worksheet swap=book.sheets[0]; book.sheets[0]=book.sheets[1]; book.sheets[1]=swap; sheet=book.sheets+1;
    CHECK(browse_preview_workbook_admit(table_preview,&book,0,SIZE_MAX,&report)==SOURCE_DEPENDENCY_EXACT && report.classification==BROWSE_PREVIEW_CAPTURED_ONLY);
    /* Fresh load/runtime clocks do not become durable source freshness proof. */
    book.edit_session_id=1; sheet->edit_session_id=2; sheet->objects.items[0].revision=1; book.command_revision=UINT64_MAX;
    CHECK(browse_preview_workbook_admit(table_preview,&book,0,SIZE_MAX,&report)==SOURCE_DEPENDENCY_EXACT && report.classification==BROWSE_PREVIEW_CAPTURED_ONLY);
    char *path=sheet->objects.items[0].source.path; sheet->objects.items[0].source.path=dx_strdup("/changed.csv");
    CHECK(browse_preview_workbook_admit(table_preview,&book,0,SIZE_MAX,&report)==SOURCE_DEPENDENCY_CONFLICT && report.classification==BROWSE_PREVIEW_NOT_ASSOCIATED);
    free(sheet->objects.items[0].source.path); sheet->objects.items[0].source.path=path;
    sheet->objects.items[0].kind=OBJECT_PIVOT;
    CHECK(browse_preview_workbook_admit(table_preview,&book,0,SIZE_MAX,&report)==SOURCE_DEPENDENCY_CONFLICT && report.classification==BROWSE_PREVIEW_NOT_ASSOCIATED); sheet->objects.items[0].kind=OBJECT_TABLE;
    uint64_t id=sheet->objects.items[0].id; sheet->objects.items[0].id=6;
    CHECK(browse_preview_workbook_admit(table_preview,&book,0,SIZE_MAX,&report)==SOURCE_DEPENDENCY_UNAVAILABLE && report.classification==BROWSE_PREVIEW_NOT_ASSOCIATED); sheet->objects.items[0].id=id;
    sheet->id=9;
    CHECK(browse_preview_workbook_admit(primary,&book,0,SIZE_MAX,&report)==SOURCE_DEPENDENCY_UNAVAILABLE); sheet->id=7;
    book.id.low++;
    CHECK(browse_preview_workbook_admit(primary,&book,0,SIZE_MAX,&report)==SOURCE_DEPENDENCY_CONFLICT); book.id=f.document;
    BrowsePreviewWorkbookAdmission saved=report; book.sheets[0].id=7;
    CHECK(browse_preview_workbook_admit(primary,&book,0,SIZE_MAX,&report)==SOURCE_DEPENDENCY_INVALID && !memcmp(&report,&saved,sizeof(report))); book.sheets[0].id=8;
    sheet->objects.items[1].id=7;
    CHECK(browse_preview_workbook_admit(primary,&book,0,SIZE_MAX,&report)==SOURCE_DEPENDENCY_INVALID && !memcmp(&report,&saved,sizeof(report))); sheet->objects.items[1].id=9;
    CHECK(browse_preview_workbook_admit(primary,&book,SIZE_MAX,SIZE_MAX,&report)==SOURCE_DEPENDENCY_LIMIT && !memcmp(&report,&saved,sizeof(report)));
    for (size_t i=0;i<2;i++) {
        Worksheet *current=book.sheets+i; free(current->name); analysis_free(&current->analysis);
        for (size_t j=0;j<current->objects.count;j++) { free(current->objects.items[j].name); analysis_free(&current->objects.items[j].source); }
        free(current->objects.items);
    }
    free(book.sheets); browse_preview_free(primary); browse_preview_free(table_preview); finish(&f);
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
    BrowsePreview *copy=wire_copy(preview); browse_preview_free(copy);
    browse_preview_free(preview); finish(&f);
}
int main(void) { ownership(); stale(); profiles(); association(); table_owner(); workbook_association(); maximum(); wire_corruption(); printf("PASS: %u bounded browse preview ownership and association checks\n",checks); return 0; }
