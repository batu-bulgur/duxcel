#include "workbook/workbook_codec_read.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL workbook header reader:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
#define LIMIT (8u*1024u*1024u)
static const WorkbookCodecKey key={UINT64_MAX,UINT64_C(9007199254740993),UINT64_C(9007199254740994)};
static WorkbookCodecAtom i(int32_t v) { return (WorkbookCodecAtom){.kind=WORKBOOK_CODEC_I32,.value.i32=v}; }
static WorkbookCodecAtom u(uint64_t v) { return (WorkbookCodecAtom){.kind=WORKBOOK_CODEC_U64,.value.u64=v}; }
static WorkbookCodecAtom d(double v) { return (WorkbookCodecAtom){.kind=WORKBOOK_CODEC_DOUBLE,.value.real=v}; }
static WorkbookCodecAtom b(bool v) { return (WorkbookCodecAtom){.kind=WORKBOOK_CODEC_BOOLEAN,.value.boolean=v}; }
static WorkbookCodecAtom t(const char *v) { return (WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={v,strlen(v)}}; }
static void error_clear(char **error) { CHECK(*error && **error); free(*error); *error=NULL; }
typedef struct { WorkbookCodecColumn columns[25]; char names[25][32]; WorkbookCodecAtom cells[65*25]; size_t width; } Fixture;
static void fixture_columns(Fixture *f,const char *name) {
    const WorkbookCodecTable *table=workbook_codec_table_find(name); const char *types=table->types; size_t column=0;
    for (const char *p=table->columns;*p;column++) {
        size_t n=strcspn(p,","); CHECK(n<sizeof(f->names[column])); memcpy(f->names[column],p,n); f->names[column][n]=0;
        while (*types==' ') types++;
        WorkbookCodecAtomKind kind=*types=='I' ? WORKBOOK_CODEC_I32 : *types=='U' ? WORKBOOK_CODEC_U64 : *types=='D' ? WORKBOOK_CODEC_DOUBLE : *types=='B' ? WORKBOOK_CODEC_BOOLEAN : WORKBOOK_CODEC_TEXT;
        f->columns[column]=(WorkbookCodecColumn){f->names[column],kind}; types++; p+=n; if (*p) p++;
    }
    f->width=column;
}
static void sheet(Fixture *f,size_t row,SourceKind kind) {
    WorkbookCodecAtom *c=f->cells+row*25;
    for (size_t j=0;j<25;j++) c[j]=(WorkbookCodecAtom){0};
    c[0]=i((int32_t)row); c[1]=t("O'İ 空 sheet"); c[2]=i(kind); c[3]=t("original/source.csv");
    c[6]=t("city"); c[7]=t(""); c[8]=i(FILTER_EQUAL); c[11]=i(AGG_AVG); c[12]=t("city"); c[13]=b(true);
    c[14]=u(DX_SHEET_ROWS-1); c[15]=u(16383); c[16]=u(1); c[17]=u(2); c[18]=u(3); c[19]=u(4); c[20]=u(5);
    c[21]=d(120.25); c[22]=d(.25); c[23]=i(2); c[24]=d(1.5);
}
typedef struct { unsigned calls; size_t maximum; int mode; } Resolver;
static bool resolve(void *pointer,SourceKind kind,const char *reference,size_t maximum,WorkbookCodecOwnedText *out) {
    Resolver *r=pointer; r->calls++; r->maximum=maximum;
    CHECK(kind>=SOURCE_CSV && kind<=SOURCE_DATABASE && reference && maximum);
    size_t bytes=strlen(reference)+8;
    if (r->mode==1) { out->data=dx_strdup("owned failure"); out->bytes=13; return false; }
    if (r->mode==2) { out->data=dx_strdup(""); out->bytes=SIZE_MAX; return true; }
    if (r->mode==3) { out->data=dx_strdup("xx"); out->bytes=1; return true; }
    if (r->mode==4) { out->data=dx_alloc(4); memcpy(out->data,"a\0b",4); out->bytes=3; return true; }
    if (bytes+1>maximum) return false;
    out->data=dx_printf("logical:%s",reference); out->bytes=bytes; return true;
}
static WorkbookCodecRead *begin(unsigned version,WorkbookCodecSheetsLayout layout,size_t limit,Resolver *r) {
    WorkbookCodecRead *read=NULL; char *error=NULL;
    CHECK(workbook_codec_read_begin(&read,key,version,layout,limit,r ? resolve : NULL,r,&error) && !error && read);
    return read;
}
static void meta(WorkbookCodecRead *read,unsigned version,uint64_t active,size_t retained) {
    Fixture f={0}; fixture_columns(&f,"meta"); f.cells[0]=i((int32_t)version); f.cells[1]=u(active); f.cells[2]=d(1.25);
    WorkbookCodecBatch batch={f.columns,3,f.cells,1,0,true}; char *error=NULL;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_META,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
}
static bool same_request(WorkbookCodecRequest a,const WorkbookCodecRequest *b) {
    return b && a.family==b->family && a.sql==b->sql && a.first_row==b->first_row && a.column_count==b->column_count && a.source_column_count==b->source_column_count && a.row_limit==b->row_limit;
}
static void reject(WorkbookCodecRead *read,WorkbookCodecFamily family,const WorkbookCodecBatch *batch,size_t retained) {
    WorkbookCodecRequest before=*workbook_codec_read_request(read); size_t bytes=workbook_codec_read_owned_bytes(read); char *error=dx_strdup("old error");
    CHECK(workbook_codec_read_feed(read,key,family,batch,retained,&error)==WORKBOOK_CODEC_READ_ERROR);
    CHECK(error && strcmp(error,"old error") && same_request(before,workbook_codec_read_request(read)) && bytes==workbook_codec_read_owned_bytes(read)); error_clear(&error);
}
static void test_happy(void) {
    for (unsigned version=12;version<=14;version++) for (unsigned layout=24;layout<=25;layout++) {
        if (version==14 && layout==24) continue;
        Resolver resolver={0}; WorkbookCodecRead *read=begin(version,(WorkbookCodecSheetsLayout)layout,LIMIT,&resolver); char *error=NULL;
        const WorkbookCodecRequest *request=workbook_codec_read_request(read);
        CHECK(request && request->family==WORKBOOK_CODEC_META && request->column_count==3 && request->row_limit==2 && request->first_row==0);
        CHECK(!strcmp(request->sql,"SELECT version,active_sheet,user_scale FROM _duxcel_workspace.meta LIMIT 2"));
        WorkbookCodecHeader *header=NULL; CHECK(!workbook_codec_read_finish(read,&header,&error) && !header); error_clear(&error);
        meta(read,version,1,sizeof(Fixture)); request=workbook_codec_read_request(read);
        CHECK(request && request->family==WORKBOOK_CODEC_SHEETS && request->column_count==25 && request->source_column_count==layout && request->row_limit==65);
        CHECK(strstr(request->sql,"ORDER BY id LIMIT 65") && !strstr(request->sql,"OFFSET") && strlen(request->sql)<1024);
        CHECK((strstr(request->sql,"1::DOUBLE AS canvas_zoom")!=NULL)==(layout==24));
        Fixture f={0}; fixture_columns(&f,"sheets"); sheet(&f,0,SOURCE_CSV); sheet(&f,1,SOURCE_DEMO);
        if (layout==24) { f.cells[24]=d(1); f.cells[49]=d(1); }
        f.cells[4]=t(""); f.cells[5]=t("NULL"); f.cells[7]=(WorkbookCodecAtom){0}; f.cells[25+7]=t("");
        char name[]="Original 'İ' 🦆"; f.cells[1]=t(name);
        WorkbookCodecBatch first={f.columns,25,f.cells,1,0,false};
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&first,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
        CHECK(workbook_codec_read_request(read)->first_row==1 && resolver.calls==1);
        memset(name,'x',strlen(name));
        WorkbookCodecBatch second={f.columns,25,f.cells+25,1,1,true};
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&second,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
        CHECK(!workbook_codec_read_request(read)); size_t before=workbook_codec_read_owned_bytes(read);
        CHECK(workbook_codec_read_finish(read,&header,&error) && !error && header);
        CHECK(workbook_codec_header_owned_bytes(header)<before && workbook_codec_read_owned_bytes(read)+workbook_codec_header_owned_bytes(header)==before);
        CHECK(workbook_codec_header_version(header)==version && workbook_codec_header_sheet_count(header)==2 && workbook_codec_header_active_sheet(header)==1 && workbook_codec_header_user_scale(header)==1.25f);
        const WorkbookCodecKey *stored=workbook_codec_header_key(header); CHECK(stored && stored->document_epoch==UINT64_MAX && stored->operation_ticket==key.operation_ticket && stored->snapshot_generation==key.snapshot_generation);
        CHECK(!strcmp(workbook_codec_header_name(header,0),"Original 'İ' 🦆")); CHECK(!strcmp(workbook_codec_header_reference(header,0),"original/source.csv"));
        const Analysis *a=workbook_codec_header_analysis(header,0),*demo=workbook_codec_header_analysis(header,1); const SheetView *v=workbook_codec_header_view(header,0);
        CHECK(a && a->kind==SOURCE_CSV && !strcmp(a->path,"logical:original/source.csv") && !strcmp(a->schema,"") && !strcmp(a->table,"NULL") && a->filter_value==NULL);
        CHECK(demo && demo->kind==SOURCE_DEMO && !strcmp(demo->path,"") && demo->filter_value && !*demo->filter_value && !demo->query && !demo->source_version);
        CHECK(v && v->row==DX_SHEET_ROWS-1 && v->col==16383 && v->anchor_row==1 && v->anchor_col==2 && v->end_row==3 && v->end_col==4 && v->top==5 && v->horizontal==120.25f && v->fraction==.25 && v->selection==2 && v->canvas_zoom==(layout==24 ? 1 : 1.5f));
        CHECK(!v->column_count && !v->columns && !v->widths && resolver.calls==1);
        CHECK(!workbook_codec_header_name(header,2) && !workbook_codec_header_reference(header,SIZE_MAX) && !workbook_codec_header_analysis(header,2) && !workbook_codec_header_view(header,2));
        WorkbookCodecHeader *identity=header; CHECK(!workbook_codec_read_finish(read,&header,&error) && header==identity); error_clear(&error);
        workbook_codec_read_free(read); CHECK(!strcmp(a->path,"logical:original/source.csv")); workbook_codec_header_free(header);
    }
    CHECK(!workbook_codec_header_key(NULL) && !workbook_codec_header_version(NULL) && !workbook_codec_header_sheet_count(NULL) && workbook_codec_header_active_sheet(NULL)==SIZE_MAX && !workbook_codec_header_user_scale(NULL));
    CHECK(!workbook_codec_read_owned_bytes(NULL) && !workbook_codec_header_owned_bytes(NULL) && !workbook_codec_read_request(NULL)); workbook_codec_read_free(NULL); workbook_codec_header_free(NULL);
}
static void test_meta_and_order(void) {
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL),*identity=read; char *error=NULL;
    for (size_t field=0;field<3;field++) {
        WorkbookCodecKey bad=key; if (!field) bad.document_epoch=0; else if (field==1) bad.operation_ticket=0; else bad.snapshot_generation=0;
        CHECK(!workbook_codec_read_begin(&read,bad,14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL,NULL,&error) && read==identity); error_clear(&error);
    }
    CHECK(!workbook_codec_read_begin(&read,key,11,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL,NULL,&error) && read==identity); error_clear(&error);
    CHECK(!workbook_codec_read_begin(&read,key,15,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL,NULL,&error) && read==identity); error_clear(&error);
    CHECK(!workbook_codec_read_begin(&read,key,14,WORKBOOK_CODEC_SHEETS_24,LIMIT,NULL,NULL,&error) && read==identity); error_clear(&error);
    CHECK(!workbook_codec_read_begin(&read,key,14,(WorkbookCodecSheetsLayout)26,LIMIT,NULL,NULL,&error) && read==identity); error_clear(&error);
    CHECK(!workbook_codec_read_begin(&read,key,14,WORKBOOK_CODEC_SHEETS_25,0,NULL,NULL,&error) && read==identity); error_clear(&error);
    CHECK(!workbook_codec_read_begin(NULL,key,14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL,NULL,&error)); error_clear(&error);
    size_t bytes=workbook_codec_read_owned_bytes(read); error=dx_strdup("keep"); char *old=error;
    for (size_t field=0;field<3;field++) {
        WorkbookCodecKey bad=key; if (!field) bad.document_epoch--; else if (field==1) bad.operation_ticket--; else bad.snapshot_generation--;
        CHECK(workbook_codec_read_feed(read,bad,WORKBOOK_CODEC_SHEETS,NULL,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE && error==old && workbook_codec_read_owned_bytes(read)==bytes);
    }
    free(error); error=NULL;
    Fixture f={0}; fixture_columns(&f,"meta"); f.cells[0]=i(14); f.cells[1]=u(0); f.cells[2]=d(1);
    WorkbookCodecBatch batch={f.columns,3,f.cells,1,0,true};
    reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); batch.first_row=1; reject(read,WORKBOOK_CODEC_META,&batch,0); batch.first_row=0;
    for (size_t col=0;col<3;col++) { WorkbookCodecAtom saved=f.cells[col]; f.cells[col]=(WorkbookCodecAtom){0}; reject(read,WORKBOOK_CODEC_META,&batch,0); f.cells[col]=saved; }
    f.cells[0]=i(13); reject(read,WORKBOOK_CODEC_META,&batch,0); f.cells[0]=i(14);
    f.cells[1]=u(UINT64_MAX); reject(read,WORKBOOK_CODEC_META,&batch,0); f.cells[1]=u(0);
    const double bad_scale[]={NAN,INFINITY,-INFINITY,.79,1.51};
    for (size_t x=0;x<sizeof(bad_scale)/sizeof(*bad_scale);x++) { f.cells[2]=d(bad_scale[x]); reject(read,WORKBOOK_CODEC_META,&batch,0); } f.cells[2]=d(1);
    f.cells[1]=d(0); reject(read,WORKBOOK_CODEC_META,&batch,0); f.cells[1]=u(0);
    batch.row_count=0; reject(read,WORKBOOK_CODEC_META,&batch,0); batch.row_count=2; memcpy(f.cells+3,f.cells,3*sizeof(*f.cells)); reject(read,WORKBOOK_CODEC_META,&batch,0); batch.row_count=1;
    batch.end=false; reject(read,WORKBOOK_CODEC_META,&batch,0); batch.end=true; reject(read,WORKBOOK_CODEC_META,&batch,SIZE_MAX);
    f.columns[0].name="Version"; reject(read,WORKBOOK_CODEC_META,&batch,0); f.columns[0].name=f.names[0];
    f.columns[1].kind=WORKBOOK_CODEC_DOUBLE; reject(read,WORKBOOK_CODEC_META,&batch,0); f.columns[1].kind=WORKBOOK_CODEC_U64;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_META,&batch,0,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    reject(read,WORKBOOK_CODEC_META,&batch,0); workbook_codec_read_free(read);
}
static void test_sheets_invalid(void) {
    Resolver r={0}; WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,&r); meta(read,14,0,0);
    Fixture f={0}; fixture_columns(&f,"sheets"); sheet(&f,0,SOURCE_CSV); WorkbookCodecBatch batch={f.columns,25,f.cells,1,0,true};
    for (size_t col=0;col<25;col++) if (!workbook_codec_nullable(workbook_codec_table_find("sheets"),f.names[col],strlen(f.names[col]))) {
        WorkbookCodecAtom saved=f.cells[col]; f.cells[col]=(WorkbookCodecAtom){0}; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[col]=saved;
    }
    const size_t bad_enum[]={0,2,8,11,23};
    for (size_t x=0;x<sizeof(bad_enum)/sizeof(*bad_enum);x++) { size_t col=bad_enum[x]; WorkbookCodecAtom saved=f.cells[col]; f.cells[col]=i(-1); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[col]=i(100); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[col]=saved; }
    const size_t row_columns[]={14,15,16,17,18,19,20};
    for (size_t x=0;x<sizeof(row_columns)/sizeof(*row_columns);x++) { size_t col=row_columns[x]; WorkbookCodecAtom saved=f.cells[col]; f.cells[col]=u(UINT64_MAX); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[col]=saved; }
    const size_t floats[]={21,22,24};
    for (size_t x=0;x<sizeof(floats)/sizeof(*floats);x++) { size_t col=floats[x]; WorkbookCodecAtom saved=f.cells[col]; f.cells[col]=d(NAN); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[col]=d(INFINITY); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[col]=d(-1); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[col]=saved; }
    WorkbookCodecAtom saved=f.cells[1]; const char nul[]={'a',0,'b'}; f.cells[1]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={nul,sizeof(nul)}}; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0);
    const char *bad_utf8[]={"\xed\xa0\x80","\xc0\x80","\xf4\x90\x80\x80","\x80","\xe2\x82"};
    for (size_t x=0;x<sizeof(bad_utf8)/sizeof(*bad_utf8);x++) { f.cells[1]=t(bad_utf8[x]); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); }
    f.cells[1]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={"x",4097}}; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[1]=saved;
    saved=f.cells[3]; f.cells[3]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={"x",SIZE_MAX}}; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[3]=saved;
    batch.column_count=24; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); batch.column_count=26; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); batch.column_count=25;
    batch.cells=NULL; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); batch.cells=f.cells; batch.columns=NULL; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); batch.columns=f.columns;
    batch.row_count=257; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); batch.row_count=1;
    f.columns[1].name="wrong"; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.columns[1].name=f.names[1];
    for (int mode=1;mode<=4;mode++) { r.mode=mode; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); } r.mode=0;
    char *error=NULL; CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,0,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_header_free(header); workbook_codec_read_free(read);
    read=begin(12,WORKBOOK_CODEC_SHEETS_24,LIMIT,NULL); meta(read,12,0,0); sheet(&f,0,SOURCE_NONE); batch=(WorkbookCodecBatch){f.columns,25,f.cells,1,0,true};
    reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[24]=d(1); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,0,&error)==WORKBOOK_CODEC_READ_ACCEPTED); workbook_codec_read_free(read);
}
static void test_many_and_atomic(void) {
    Resolver r={0}; WorkbookCodecRead *read=begin(13,WORKBOOK_CODEC_SHEETS_25,LIMIT,&r); meta(read,13,0,0);
    Fixture *f=calloc(1,sizeof(*f)); CHECK(f); fixture_columns(f,"sheets"); for (size_t row=0;row<65;row++) sheet(f,row,SOURCE_NONE);
    WorkbookCodecBatch batch={f->columns,25,f->cells,65,0,true}; reject(read,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f));
    batch.row_count=0; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); batch.end=false; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0);
    batch.row_count=2; batch.end=true; f->cells[25]=i(0); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f->cells[25]=i(2); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f->cells[25]=i(1);
    /* Failure after an earlier row owns all its strings must retire only that
       temporary batch, then allow a complete unchanged-state retry. */
    f->cells[27]=i(SOURCE_CSV); r.mode=1; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); CHECK(r.calls==1); r.mode=0; f->cells[27]=i(SOURCE_NONE);
    batch.row_count=32; batch.end=false; char *error=NULL;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    batch.first_row=0; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0);
    batch.first_row=32; batch.cells=f->cells+32*25; batch.row_count=32; batch.end=true;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_sheet_count(header)==64 && r.calls==1);
    CHECK(workbook_codec_header_view(header,63)->row==DX_SHEET_ROWS-1);
    workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); meta(read,14,2,0); Fixture one={0}; fixture_columns(&one,"sheets"); sheet(&one,0,SOURCE_NONE);
    batch=(WorkbookCodecBatch){one.columns,25,one.cells,1,0,true}; reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); workbook_codec_read_free(read);
}
static bool fits(size_t limit,size_t retained) {
    WorkbookCodecRead *read=NULL; char *error=NULL; bool ok=workbook_codec_read_begin(&read,key,14,WORKBOOK_CODEC_SHEETS_25,limit,NULL,NULL,&error);
    if (ok) {
        meta(read,14,0,0); Fixture f={0}; fixture_columns(&f,"sheets"); sheet(&f,0,SOURCE_NONE);
        WorkbookCodecBatch batch={f.columns,25,f.cells,1,0,true}; ok=workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED;
    }
    free(error); workbook_codec_read_free(read); return ok;
}
static void test_budget(void) {
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); size_t base=workbook_codec_read_owned_bytes(read); workbook_codec_read_free(read);
    size_t low=base,high=base+8192; CHECK(!fits(low,0) && fits(high,0));
    while (low+1<high) { size_t mid=low+(high-low)/2; if (fits(mid,0)) high=mid; else low=mid; }
    CHECK(fits(high,0) && !fits(high-1,0)); CHECK(!fits(high,1) && fits(high+1,1)); CHECK(!fits(high+4095,4096) && fits(high+4096,4096));
    read=begin(14,WORKBOOK_CODEC_SHEETS_25,high,NULL); meta(read,14,0,0);
    Fixture f={0}; fixture_columns(&f,"sheets"); sheet(&f,0,SOURCE_NONE); WorkbookCodecBatch batch={f.columns,25,f.cells,1,0,true};
    reject(read,WORKBOOK_CODEC_SHEETS,&batch,1); char *error=NULL;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,0,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
    CHECK(workbook_codec_header_owned_bytes(header)+workbook_codec_read_owned_bytes(read)<high); /* Temporary row array was retired. */
    WorkbookCodecRead *pending=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); WorkbookCodecHeader *identity=header;
    CHECK(!workbook_codec_read_finish(pending,&header,&error) && header==identity); error_clear(&error);
    workbook_codec_read_free(pending); workbook_codec_header_free(header); workbook_codec_read_free(read);
}
static void test_text_boundaries(void) {
    char *name=dx_alloc(4097),*reference=dx_alloc(DX_WORKBOOK_CODEC_READ_TEXT+1u),*value=dx_alloc(DX_WORKBOOK_CODEC_READ_TEXT+1u);
    memset(name,'n',4096); name[4096]=0;
    memset(reference,'r',DX_WORKBOOK_CODEC_READ_TEXT); reference[DX_WORKBOOK_CODEC_READ_TEXT]=0;
    memset(value,'v',DX_WORKBOOK_CODEC_READ_TEXT); value[DX_WORKBOOK_CODEC_READ_TEXT]=0;
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); meta(read,14,0,0);
    Fixture f={0}; fixture_columns(&f,"sheets"); sheet(&f,0,SOURCE_NONE);
    f.cells[1]=t(name); f.cells[3]=t(reference); f.cells[7]=t(value);
    WorkbookCodecBatch batch={f.columns,25,f.cells,1,0,true};
    size_t retained=sizeof(f)+4097u+2u*(DX_WORKBOOK_CODEC_READ_TEXT+1u); char *error=NULL;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    memset(name,'x',4096); memset(reference,'x',DX_WORKBOOK_CODEC_READ_TEXT); memset(value,'x',DX_WORKBOOK_CODEC_READ_TEXT);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
    const Analysis *analysis=workbook_codec_header_analysis(header,0);
    CHECK(strlen(workbook_codec_header_name(header,0))==4096 && workbook_codec_header_name(header,0)[4095]=='n');
    CHECK(strlen(workbook_codec_header_reference(header,0))==DX_WORKBOOK_CODEC_READ_TEXT && workbook_codec_header_reference(header,0)[DX_WORKBOOK_CODEC_READ_TEXT-1]=='r');
    CHECK(analysis && analysis->filter_value && strlen(analysis->filter_value)==DX_WORKBOOK_CODEC_READ_TEXT && analysis->filter_value[DX_WORKBOOK_CODEC_READ_TEXT-1]=='v' && analysis->path && !*analysis->path);
    workbook_codec_read_free(read); workbook_codec_header_free(header); free(name); free(reference); free(value);
}
int main(void) {
    test_happy(); test_meta_and_order(); test_sheets_invalid(); test_many_and_atomic(); test_budget(); test_text_boundaries();
    printf("PASS: workbook header reader %u checks (bounded typed families, exact keys, schema/null/text/viewport validation, deep ownership and atomic budget failures)\n",checks); return 0;
}
