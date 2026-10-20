#include "workbook/workbook_codec_read.h"
#include <float.h>
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
    WorkbookCodecAtom before_horizontal=f.cells[21],before_canvas=f.cells[24];
    f.cells[21]=d(-DBL_MIN); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0);
    f.cells[21]=d(DBL_MAX); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[21]=before_horizontal;
    f.cells[24]=d(nextafter(.5,0)); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0);
    f.cells[24]=d(nextafter(2.5,INFINITY)); reject(read,WORKBOOK_CODEC_SHEETS,&batch,0); f.cells[24]=before_canvas;
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
/* Optional family gates are separate from the unchanged base Header checks. */
static WorkbookCodecRead *sort_header(unsigned version,size_t count,const char *primary,size_t limit) {
    WorkbookCodecRead *read=begin(version,WORKBOOK_CODEC_SHEETS_25,limit,NULL); meta(read,version,0,0);
    Fixture *f=dx_alloc(sizeof(*f)); fixture_columns(f,"sheets");
    for (size_t row=0;row<count;row++) { sheet(f,row,SOURCE_NONE); f->cells[row*25+12]=primary ? t(primary) : (WorkbookCodecAtom){0}; }
    WorkbookCodecBatch batch={f->columns,25,f->cells,count,0,true}; char *error=NULL;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    free(f); return read;
}
typedef struct {
    WorkbookCodecColumn columns[4];
    WorkbookCodecAtom cells[449*4];
    char names[449][40];
} SortFixture;
static SortFixture *sort_fixture(size_t count) {
    SortFixture *f=dx_alloc(sizeof(*f));
    f->columns[0]=(WorkbookCodecColumn){"sheet_id",WORKBOOK_CODEC_I32};
    f->columns[1]=(WorkbookCodecColumn){"position",WORKBOOK_CODEC_I32};
    f->columns[2]=(WorkbookCodecColumn){"name",WORKBOOK_CODEC_TEXT};
    f->columns[3]=(WorkbookCodecColumn){"descending",WORKBOOK_CODEC_BOOLEAN};
    for (size_t row=0;row<count;row++) {
        snprintf(f->names[row],sizeof(f->names[row]),"Field 'İ' %zu",row);
        f->cells[row*4]=i((int32_t)(row/7)); f->cells[row*4+1]=i((int32_t)(row%7));
        f->cells[row*4+2]=t(f->names[row]); f->cells[row*4+3]=b(row%2!=0);
    }
    return f;
}
static WorkbookCodecBatch sort_batch(SortFixture *f,size_t first,size_t count,bool end) {
    return (WorkbookCodecBatch){f->columns,4,f->cells+first*4,count,first,end};
}
static void reject_arm(WorkbookCodecRead *read,bool present) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(read); WorkbookCodecRequest before=request ? *request : (WorkbookCodecRequest){0};
    size_t bytes=workbook_codec_read_owned_bytes(read); char *error=NULL;
    CHECK(!workbook_codec_read_add_sorts(read,present,&error) && error && *error && workbook_codec_read_owned_bytes(read)==bytes);
    CHECK(request ? same_request(before,workbook_codec_read_request(read)) : !workbook_codec_read_request(read)); error_clear(&error);
}
static void test_sorts_continuation(void) {
    CHECK(!workbook_codec_header_sorts_ready(NULL)); reject_arm(NULL,true);
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); reject_arm(read,true); meta(read,14,0,0); reject_arm(read,true); workbook_codec_read_free(read);
    char *error=NULL;
    for (unsigned version=12;version<=14;version++) {
        read=sort_header(version,2,"city",LIMIT); size_t bytes=workbook_codec_read_owned_bytes(read);
        if (version==14) reject_arm(read,false);
        else {
            CHECK(workbook_codec_read_add_sorts(read,false,&error) && !error && !workbook_codec_read_request(read) && workbook_codec_read_owned_bytes(read)==bytes);
            reject_arm(read,true); WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
            CHECK(workbook_codec_header_sorts_ready(header) && !workbook_codec_header_analysis(header,0)->sort_count && !strcmp(workbook_codec_header_analysis(header,0)->sort_column,"city"));
            reject_arm(read,true); workbook_codec_read_free(read); workbook_codec_header_free(header); read=sort_header(version,2,"city",LIMIT);
        }
        CHECK(workbook_codec_read_add_sorts(read,true,&error) && !error);
        const WorkbookCodecRequest *request=workbook_codec_read_request(read);
        CHECK(request && request->family==WORKBOOK_CODEC_SORTS && request->column_count==4 && request->source_column_count==4 && request->row_limit==449 && !request->first_row);
        CHECK(!strcmp(request->sql,"SELECT sheet_id,position,name,descending FROM _duxcel_workspace.sorts ORDER BY sheet_id,position LIMIT 449") && !strstr(request->sql,"OFFSET"));
        reject_arm(read,true); WorkbookCodecHeader *header=NULL; CHECK(!workbook_codec_read_finish(read,&header,&error) && !header); error_clear(&error);
        SortFixture *f=sort_fixture(0); WorkbookCodecBatch batch=sort_batch(f,0,0,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
        CHECK(!workbook_codec_read_request(read)); reject_arm(read,true);
        CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_sorts_ready(header));
        CHECK(workbook_codec_header_analysis(header,1)->descending && !workbook_codec_header_analysis(header,1)->sort_count);
        workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    }
    char *long_name=dx_alloc(4098); memset(long_name,'s',4097); long_name[4097]=0;
    const char *invalid_primary[]={"",long_name};
    for (size_t n=0;n<2;n++) {
        read=sort_header(14,1,invalid_primary[n],LIMIT); reject_arm(read,true);
        /* Base Header behavior remains unchanged: only arming sort validation
           rejects these definitions; claiming an unextended Header still works. */
        WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
        CHECK(!workbook_codec_header_sorts_ready(header) && !strcmp(workbook_codec_header_analysis(header,0)->sort_column,invalid_primary[n]));
        workbook_codec_read_free(read); workbook_codec_header_free(header);
    }
    free(long_name);
    read=sort_header(13,1,NULL,LIMIT); CHECK(workbook_codec_read_add_sorts(read,false,&error));
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_sorts_ready(header));
    CHECK(!workbook_codec_header_analysis(header,0)->sort_column); workbook_codec_read_free(read); workbook_codec_header_free(header);
}
static void test_sorts_invalid_and_atomic(void) {
    WorkbookCodecRead *read=sort_header(14,2,"city",LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_sorts(read,true,&error));
    SortFixture *f=sort_fixture(2); WorkbookCodecBatch batch=sort_batch(f,0,1,true);
    size_t owned=workbook_codec_read_owned_bytes(read); error=dx_strdup("keep"); char *identity=error;
    for (size_t part=0;part<3;part++) {
        WorkbookCodecKey stale=key;
        if (!part) stale.document_epoch--; else if (part==1) stale.operation_ticket--; else stale.snapshot_generation--;
        CHECK(workbook_codec_read_feed(read,stale,(WorkbookCodecFamily)99,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE && error==identity && workbook_codec_read_owned_bytes(read)==owned);
    }
    free(error); error=NULL;
    for (size_t col=0;col<4;col++) { WorkbookCodecAtom saved=f->cells[col]; f->cells[col]=(WorkbookCodecAtom){0}; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->cells[col]=saved; }
    f->cells[0]=i(-1); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->cells[0]=i(2); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->cells[0]=i(0);
    f->cells[1]=i(-1); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->cells[1]=i(1); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->cells[1]=i(7); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->cells[1]=i(0);
    WorkbookCodecAtom name=f->cells[2];
    const char *invalid_names[]={"","CiTy","\xed\xa0\x80","\xc0\x80","\xf4\x90\x80\x80","\x80","\xe2\x82"};
    for (size_t n=0;n<sizeof(invalid_names)/sizeof(*invalid_names);n++) { f->cells[2]=t(invalid_names[n]); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); }
    const char nul[]={'a',0,'b'}; f->cells[2]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={nul,sizeof(nul)}}; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f));
    f->cells[2]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={NULL,1}}; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f));
    f->cells[2]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={"x",4097}}; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->cells[2]=name;
    f->columns[3].kind=WORKBOOK_CODEC_TEXT; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->columns[3].kind=WORKBOOK_CODEC_BOOLEAN;
    f->columns[0].name="Sheet_id"; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->columns[0].name="sheet_id";
    batch.column_count=3; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); batch.column_count=5; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); batch.column_count=4;
    batch.cells=NULL; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); batch.cells=f->cells;
    batch.columns=NULL; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); batch.columns=f->columns;
    batch.row_count=257; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); batch.row_count=0; batch.end=false; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); batch.row_count=1; batch.end=true;
    batch.first_row=1; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); batch.first_row=0; reject(read,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f)); reject(read,WORKBOOK_CODEC_SORTS,&batch,SIZE_MAX);
    /* A valid first row owns a new name before the second row fails. Retrying
       the unchanged complete batch proves no prefix escaped into the Header. */
    batch.row_count=2; f->cells[6]=t("CITY"); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f));
    f->cells[6]=f->cells[2]; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->cells[6]=t(f->names[1]);
    f->cells[4]=i(0); f->cells[5]=i(0); f->cells[0]=i(1); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f));
    f->cells[0]=i(0); f->cells[4]=i(1); f->cells[5]=i(0);
    strcpy(f->names[1],"NULL"); f->cells[6]=t(f->names[1]);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_sorts_ready(header));
    const Analysis *a=workbook_codec_header_analysis(header,0),*b=workbook_codec_header_analysis(header,1);
    CHECK(a->sort_count==1 && b->sort_count==1 && !strcmp(a->sorts[0].column,f->names[0]) && !strcmp(b->sorts[0].column,f->names[1]));
    CHECK(a->sorts[0].column!=f->names[0] && b->sorts[0].column!=f->names[1]);
    memset(f->names,'x',sizeof(f->names)); free(f); workbook_codec_read_free(read);
    CHECK(!strcmp(a->sorts[0].column,"Field 'İ' 0") && !strcmp(b->sorts[0].column,"NULL") && !a->sorts[0].descending && b->sorts[0].descending); workbook_codec_header_free(header);
    read=sort_header(12,1,NULL,LIMIT); CHECK(workbook_codec_read_add_sorts(read,true,&error)); f=sort_fixture(1); batch=sort_batch(f,0,1,true);
    reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); batch.row_count=0;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); workbook_codec_read_free(read); free(f);
}
static void test_sorts_many_and_order(void) {
    WorkbookCodecRead *read=sort_header(14,64,"city",LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_sorts(read,true,&error));
    SortFixture *f=sort_fixture(449); WorkbookCodecBatch batch=sort_batch(f,0,256,false);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(workbook_codec_read_request(read)->first_row==256); batch=sort_batch(f,256,193,true); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f));
    batch.row_count=192; WorkbookCodecAtom old=f->cells[256*4+2]; f->cells[256*4+2]=f->cells[252*4+2]; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->cells[256*4+2]=old;
    WorkbookCodecAtom sheet_id=f->cells[256*4]; f->cells[256*4]=i(0); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f)); f->cells[256*4]=sheet_id;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_sorts_ready(header));
    for (size_t sheet_id=0;sheet_id<64;sheet_id++) {
        const Analysis *a=workbook_codec_header_analysis(header,sheet_id); CHECK(a->sort_count==7 && analysis_sorts_valid(a,NULL));
        for (size_t n=0;n<7;n++) CHECK(!strcmp(a->sorts[n].column,f->names[sheet_id*7+n]) && a->sorts[n].descending==((sheet_id*7+n)%2!=0));
    }
    free(f); workbook_codec_read_free(read); CHECK(!strcmp(workbook_codec_header_analysis(header,63)->sorts[6].column,"Field 'İ' 447")); workbook_codec_header_free(header); header=NULL;
    read=sort_header(13,2,"city",LIMIT); CHECK(workbook_codec_read_add_sorts(read,true,&error)); f=sort_fixture(2);
    f->cells[0]=i(1); batch=sort_batch(f,0,1,false);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    f->cells[4]=i(0); f->cells[5]=i(0); batch=sort_batch(f,1,1,true); reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f));
    f->cells[4]=i(1); f->cells[5]=i(1);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    CHECK(workbook_codec_read_finish(read,&header,&error)); CHECK(!workbook_codec_header_analysis(header,0)->sort_count && workbook_codec_header_analysis(header,1)->sort_count==2);
    workbook_codec_header_free(header); workbook_codec_read_free(read); free(f);
    /* Partially accepted family cleanup owns all names even without finish. */
    read=sort_header(14,64,"city",LIMIT); CHECK(workbook_codec_read_add_sorts(read,true,&error)); f=sort_fixture(256); batch=sort_batch(f,0,256,false);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(f); workbook_codec_read_free(read);
}
static bool sorts_fit(size_t limit,size_t retained) {
    WorkbookCodecRead *read=sort_header(14,1,"city",limit); char *error=NULL; CHECK(workbook_codec_read_add_sorts(read,true,&error));
    SortFixture *f=sort_fixture(2); char *first=dx_alloc(4097),*second=dx_alloc(4097);
    memset(first,'a',4096); first[4096]=0; memset(second,'b',4096); second[4096]=0; f->cells[2]=t(first); f->cells[6]=t(second);
    WorkbookCodecBatch batch=sort_batch(f,0,2,true);
    bool ok=workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED;
    free(error); free(first); free(second); free(f); workbook_codec_read_free(read); return ok;
}
static void test_sorts_budget_and_limits(void) {
    WorkbookCodecRead *read=sort_header(14,1,"city",LIMIT); size_t base=workbook_codec_read_owned_bytes(read); workbook_codec_read_free(read);
    const size_t retained=sizeof(SortFixture)+2*4097u; size_t low=base+retained+4096,high=base+retained+16384;
    CHECK(!sorts_fit(low,retained) && sorts_fit(high,retained));
    while (low+1<high) { size_t middle=low+(high-low)/2; if (sorts_fit(middle,retained)) high=middle; else low=middle; }
    CHECK(sorts_fit(high,retained) && !sorts_fit(high-1,retained));
    read=sort_header(14,1,"city",high); char *error=NULL; CHECK(workbook_codec_read_add_sorts(read,true,&error));
    SortFixture *f=sort_fixture(2); char *first=dx_alloc(4097),*second=dx_alloc(4097),*pressure=dx_alloc(1);
    memset(first,'a',4096); first[4096]=0; memset(second,'b',4096); second[4096]=0; f->cells[2]=t(first); f->cells[6]=t(second);
    WorkbookCodecBatch batch=sort_batch(f,0,2,true); reject(read,WORKBOOK_CODEC_SORTS,&batch,retained+1); free(pressure);
    WorkbookCodecRead *prior=sort_header(12,1,NULL,LIMIT); WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(prior,&header,&error)); workbook_codec_read_free(prior);
    WorkbookCodecHeader *identity=header; size_t old_bytes=workbook_codec_header_owned_bytes(header);
    CHECK(!workbook_codec_read_finish(read,&header,&error) && header==identity && workbook_codec_header_owned_bytes(header)==old_bytes && !workbook_codec_header_sorts_ready(header)); error_clear(&error);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_sorts_ready(header));
    const Analysis *a=workbook_codec_header_analysis(header,0); CHECK(a->sort_count==2 && strlen(a->sorts[0].column)==4096 && strlen(a->sorts[1].column)==4096);
    memset(first,'x',4096); memset(second,'x',4096); free(first); free(second); free(f); workbook_codec_read_free(read);
    CHECK(a->sorts[0].column[4095]=='a' && a->sorts[1].column[4095]=='b'); workbook_codec_header_free(header);
}
typedef struct { WorkbookCodecColumn columns[2]; WorkbookCodecAtom cells[66*2]; char names[65][32]; } PreferenceFixture;
static int preference_order(const void *a,const void *b) {
    return strcmp(((const WorkbookCodecAtom *)a)->value.text.data,((const WorkbookCodecAtom *)b)->value.text.data);
}
static PreferenceFixture *preference_fixture(size_t sheets) {
    PreferenceFixture *f=dx_alloc(sizeof(*f));
    f->columns[0]=(WorkbookCodecColumn){"name",WORKBOOK_CODEC_TEXT}; f->columns[1]=(WorkbookCodecColumn){"value",WORKBOOK_CODEC_BOOLEAN};
    strcpy(f->names[0],"reduce_motion"); f->cells[0]=t(f->names[0]); f->cells[1]=b(true);
    for (size_t i=0;i<sheets;i++) {
        snprintf(f->names[i+1],sizeof(f->names[i+1]),"sheet_hidden_%zu",i); f->cells[(i+1)*2]=t(f->names[i+1]); f->cells[(i+1)*2+1]=b(i!=0);
    }
    qsort(f->cells,sheets+1,2*sizeof(*f->cells),preference_order); return f;
}
static WorkbookCodecBatch preference_batch(PreferenceFixture *f,size_t first,size_t count,bool end) {
    return (WorkbookCodecBatch){f->columns,2,f->cells+first*2,count,first,end};
}
static void reject_preferences_arm(WorkbookCodecRead *read,bool present) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(read); WorkbookCodecRequest before=request ? *request : (WorkbookCodecRequest){0};
    size_t owned=workbook_codec_read_owned_bytes(read); char *error=NULL;
    CHECK(!workbook_codec_read_add_preferences(read,present,&error) && error && *error && workbook_codec_read_owned_bytes(read)==owned);
    CHECK(request ? same_request(before,workbook_codec_read_request(read)) : !workbook_codec_read_request(read)); error_clear(&error);
}
static void test_preferences_defaults_and_composition(void) {
    CHECK(!workbook_codec_header_preferences_ready(NULL) && !workbook_codec_header_reduced_motion(NULL) && !workbook_codec_header_sheet_hidden(NULL,0)); reject_preferences_arm(NULL,true);
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); reject_preferences_arm(read,true); meta(read,14,0,0); reject_preferences_arm(read,true); workbook_codec_read_free(read);
    char *error=NULL;
    for (unsigned version=12;version<=14;version++) {
        read=sort_header(version,2,"city",LIMIT); size_t owned=workbook_codec_read_owned_bytes(read);
        if (version==14) reject_preferences_arm(read,false);
        else {
            CHECK(workbook_codec_read_add_preferences(read,false,&error) && !error && !workbook_codec_read_request(read) && workbook_codec_read_owned_bytes(read)==owned);
            reject_preferences_arm(read,true); WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
            CHECK(workbook_codec_header_preferences_ready(header) && !workbook_codec_header_reduced_motion(header) && !workbook_codec_header_sheet_hidden(header,1) && !workbook_codec_header_sorts_ready(header));
            reject_preferences_arm(read,true); workbook_codec_read_free(read); workbook_codec_header_free(header); read=sort_header(version,2,"city",LIMIT);
        }
        CHECK(workbook_codec_read_add_preferences(read,true,&error) && !error);
        const WorkbookCodecRequest *request=workbook_codec_read_request(read);
        CHECK(request && request->family==WORKBOOK_CODEC_PREFERENCES && request->row_limit==66 && request->column_count==2 && request->source_column_count==2 && !request->first_row);
        CHECK(strstr(request->sql,"SELECT name,value FROM _duxcel_workspace.preferences WHERE name='reduce_motion' OR starts_with(name,'sheet_hidden_')") && strstr(request->sql,"ORDER BY name LIMIT 66") && !strstr(request->sql,"OFFSET"));
        CHECK((strstr(request->sql,"OR name IS NULL OR value IS NULL OR contains(name,chr(0))")!=NULL)==(version==14));
        reject_preferences_arm(read,true); reject_arm(read,true);
        PreferenceFixture *f=preference_fixture(0); WorkbookCodecBatch empty=preference_batch(f,0,0,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&empty,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
        WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
        CHECK(workbook_codec_header_preferences_ready(header) && !workbook_codec_header_reduced_motion(header) && !workbook_codec_header_sheet_hidden(header,0));
        workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    }
    for (unsigned preferences_first=0;preferences_first<2;preferences_first++) {
        read=sort_header(14,1,"city",LIMIT); PreferenceFixture *f=preference_fixture(1); SortFixture *s=sort_fixture(1);
        if (!preferences_first) {
            CHECK(workbook_codec_read_add_sorts(read,true,&error)); reject_preferences_arm(read,true);
            WorkbookCodecBatch batch=sort_batch(s,0,1,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*s)+sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
        }
        CHECK(workbook_codec_read_add_preferences(read,true,&error)); WorkbookCodecBatch batch=preference_batch(f,0,2,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*s)+sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
        if (preferences_first) {
            CHECK(workbook_codec_read_add_sorts(read,true,&error)); batch=sort_batch(s,0,1,true);
            CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*s)+sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
        }
        reject_preferences_arm(read,true); reject_arm(read,true); WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
        CHECK(workbook_codec_header_preferences_ready(header) && workbook_codec_header_sorts_ready(header) && workbook_codec_header_reduced_motion(header) && !workbook_codec_header_sheet_hidden(header,0));
        CHECK(workbook_codec_header_analysis(header,0)->sort_count==1 && !strcmp(workbook_codec_header_analysis(header,0)->sorts[0].column,"Field 'İ' 0"));
        workbook_codec_read_free(read); workbook_codec_header_free(header); free(s); free(f);
    }
    /* Preferences neither validates nor modifies an unarmed sort definition. */
    read=sort_header(12,1,"",LIMIT); CHECK(workbook_codec_read_add_preferences(read,false,&error)); WorkbookCodecHeader *header=NULL;
    CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_preferences_ready(header) && !workbook_codec_header_sorts_ready(header) && !strcmp(workbook_codec_header_analysis(header,0)->sort_column,""));
    workbook_codec_read_free(read); workbook_codec_header_free(header);
}
static void test_preferences_rejection_and_recovery(void) {
    WorkbookCodecRead *read=sort_header(14,2,"city",LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_preferences(read,true,&error));
    PreferenceFixture *f=preference_fixture(2); WorkbookCodecBatch batch=preference_batch(f,0,3,true);
    size_t owned=workbook_codec_read_owned_bytes(read); error=dx_strdup("keep"); char *identity=error;
    WorkbookCodecKey stale=key; stale.operation_ticket--;
    CHECK(workbook_codec_read_feed(read,stale,(WorkbookCodecFamily)99,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE && error==identity && workbook_codec_read_owned_bytes(read)==owned); free(error); error=NULL;
    for (size_t col=0;col<2;col++) { WorkbookCodecAtom old=f->cells[col]; f->cells[col]=(WorkbookCodecAtom){0}; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); f->cells[col]=old; }
    WorkbookCodecAtom name=f->cells[0]; const char *invalid_names[]={"","future_option","Reduce_motion","sheet_hidden_","sheet_hidden_00","sheet_hidden_01","sheet_hidden_+1","sheet_hidden_-1","sheet_hidden_ 1","sheet_hidden_1x","sheet_hidden_64","sheet_hidden_18446744073709551615","\xed\xa0\x80","\xc0\x80"};
    for (size_t n=0;n<sizeof(invalid_names)/sizeof(*invalid_names);n++) { f->cells[0]=t(invalid_names[n]); reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); }
    const char nul[]={'a',0,'b'}; f->cells[0]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={nul,sizeof(nul)}}; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f));
    f->cells[0]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={NULL,1}}; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f));
    f->cells[0]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={"x",SIZE_MAX}}; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); f->cells[0]=name;
    f->columns[0].name="Name"; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); f->columns[0].name="name";
    f->columns[1].kind=WORKBOOK_CODEC_I32; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); f->columns[1].kind=WORKBOOK_CODEC_BOOLEAN;
    WorkbookCodecAtom value=f->cells[1]; f->cells[1]=i(1); reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); f->cells[1]=value;
    batch.column_count=1; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); batch.column_count=3; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); batch.column_count=2;
    batch.row_count=257; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); batch.row_count=0; batch.end=false; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); batch.row_count=3; batch.end=true;
    batch.first_row=1; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); batch.first_row=0; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f));
    batch.cells=NULL; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); batch.cells=f->cells; batch.columns=NULL; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); batch.columns=f->columns;
    WorkbookCodecAtom last=f->cells[4]; f->cells[4]=f->cells[2]; reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); f->cells[4]=last;
    last=f->cells[2]; f->cells[2]=t("reduce_motion"); reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); f->cells[2]=last;
    f->cells[3]=b(true); reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); f->cells[3]=b(false); /* Late active-hidden failure must retire the earlier motion=true staging. */
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); memset(f->names,'x',sizeof(f->names)); free(f); workbook_codec_read_free(read);
    CHECK(workbook_codec_header_preferences_ready(header) && workbook_codec_header_reduced_motion(header) && !workbook_codec_header_sheet_hidden(header,0) && workbook_codec_header_sheet_hidden(header,1));
    CHECK(!workbook_codec_header_sheet_hidden(header,2) && !workbook_codec_header_sheet_hidden(header,SIZE_MAX) && !strcmp(workbook_codec_header_analysis(header,0)->sort_column,"city")); workbook_codec_header_free(header);
}
static void test_preferences_many_and_budget(void) {
    WorkbookCodecRead *read=sort_header(14,64,"city",LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_preferences(read,true,&error));
    PreferenceFixture *f=preference_fixture(64); WorkbookCodecBatch batch=preference_batch(f,0,1,false);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    f->cells[130]=t("reduce_motion"); f->cells[131]=b(false); batch=preference_batch(f,1,65,true); reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f));
    batch.row_count=64; CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
    for (size_t sheet=0;sheet<64;sheet++) CHECK(workbook_codec_header_sheet_hidden(header,sheet)==(sheet!=0));
    CHECK(workbook_codec_header_preferences_ready(header) && workbook_codec_header_reduced_motion(header) && !workbook_codec_header_sheet_hidden(header,64)); workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    read=sort_header(14,2,"city",LIMIT); CHECK(workbook_codec_read_add_preferences(read,true,&error)); f=preference_fixture(2); f->cells[1]=b(false); batch=preference_batch(f,0,2,false);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    WorkbookCodecAtom name=f->cells[4],value=f->cells[5]; f->cells[4]=t("reduce_motion"); f->cells[5]=b(false); batch=preference_batch(f,2,1,true); reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f));
    f->cells[4]=t("sheet_hidden_0"); reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f)); f->cells[4]=name; f->cells[5]=value;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); header=NULL;
    CHECK(workbook_codec_read_finish(read,&header,&error) && !workbook_codec_header_reduced_motion(header) && workbook_codec_header_sheet_hidden(header,1)); workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    WorkbookCodecRead *prior=sort_header(12,2,"city",LIMIT); CHECK(workbook_codec_read_add_preferences(prior,false,&error)); header=NULL;
    CHECK(workbook_codec_read_finish(prior,&header,&error)); workbook_codec_read_free(prior);
    read=sort_header(14,2,"city",LIMIT); CHECK(workbook_codec_read_add_preferences(read,true,&error)); f=preference_fixture(2); batch=preference_batch(f,0,3,true);
    size_t old_bytes=workbook_codec_header_owned_bytes(header),owned=workbook_codec_read_owned_bytes(read);
    CHECK(owned+old_bytes+sizeof(*f)<LIMIT); size_t pressure_bytes=LIMIT-owned-old_bytes-sizeof(*f); unsigned char *pressure=dx_alloc(pressure_bytes),*extra=dx_alloc(1);
    reject(read,WORKBOOK_CODEC_PREFERENCES,&batch,old_bytes+sizeof(*f)+pressure_bytes+1); free(extra);
    WorkbookCodecHeader *identity=header; CHECK(!workbook_codec_read_finish(read,&header,&error) && header==identity && workbook_codec_header_owned_bytes(header)==old_bytes && !workbook_codec_header_reduced_motion(header) && !workbook_codec_header_sheet_hidden(header,1)); error_clear(&error);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&batch,old_bytes+sizeof(*f)+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && workbook_codec_read_owned_bytes(read)==owned);
    CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_preferences_ready(header)); free(pressure); free(f); workbook_codec_read_free(read);
    CHECK(workbook_codec_header_reduced_motion(header) && workbook_codec_header_sheet_hidden(header,1)); workbook_codec_header_free(header);
}
typedef struct {
    WorkbookCodecColumn columns[11];
    WorkbookCodecAtom cells[2049*11];
    char names[2049][40];
} ObjectFixture;
static ObjectFixture *object_fixture(size_t count) {
    ObjectFixture *f=dx_alloc(sizeof(*f));
    const char *names[]={"sheet_id","id","name","row_id","column_id","height","width","kind","mark","source_id","chart_measure"};
    const WorkbookCodecAtomKind types[]={WORKBOOK_CODEC_I32,WORKBOOK_CODEC_U64,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_U64,WORKBOOK_CODEC_I32,
        WORKBOOK_CODEC_U64,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_U64,WORKBOOK_CODEC_I32};
    for (size_t col=0;col<11;col++) f->columns[col]=(WorkbookCodecColumn){names[col],types[col]};
    for (size_t row=0;row<count;row++) {
        snprintf(f->names[row],sizeof(f->names[row]),"Object 'İ' %zu",row); WorkbookCodecAtom *c=f->cells+row*11;
        c[0]=i((int32_t)(row/32)); c[1]=u(row%32+1); c[2]=t(f->names[row]); c[3]=u((row%32)*2); c[4]=i(0); c[5]=u(1); c[6]=i(1);
        c[7]=i(OBJECT_PIVOT); c[8]=i(MARK_BAR); c[9]=u(0); c[10]=i(0);
    }
    return f;
}
static WorkbookCodecBatch object_batch(ObjectFixture *f,size_t first,size_t count,bool end) {
    return (WorkbookCodecBatch){f->columns,11,f->cells+first*11,count,first,end};
}
static void reject_objects_arm(WorkbookCodecRead *read,bool present) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(read); WorkbookCodecRequest before=request ? *request : (WorkbookCodecRequest){0};
    size_t bytes=workbook_codec_read_owned_bytes(read); char *error=NULL;
    CHECK(!workbook_codec_read_add_objects(read,present,&error) && error && *error && workbook_codec_read_owned_bytes(read)==bytes);
    CHECK(request ? same_request(before,workbook_codec_read_request(read)) : !workbook_codec_read_request(read)); error_clear(&error);
}
static void object_view_unchanged(const WorkbookCodecHeader *header,size_t sheet,size_t index) {
    WorkbookCodecObjectView out={.id=UINT64_MAX,.name="untouched"}; unsigned char before[sizeof(out)]; memcpy(before,&out,sizeof(out));
    CHECK(!workbook_codec_header_object_at(header,sheet,index,&out) && !memcmp(before,&out,sizeof(out)));
}
static void test_objects_defaults_and_composition(void) {
    CHECK(WORKBOOK_CODEC_META==0 && WORKBOOK_CODEC_SHEETS==1 && WORKBOOK_CODEC_HEADER_READY==2 && WORKBOOK_CODEC_HEADER_CLAIMED==3 &&
        WORKBOOK_CODEC_SORTS==4 && WORKBOOK_CODEC_PREFERENCES==5 && WORKBOOK_CODEC_OBJECTS==6);
    CHECK(!workbook_codec_header_objects_ready(NULL) && !workbook_codec_header_object_count(NULL,0)); object_view_unchanged(NULL,0,0); reject_objects_arm(NULL,true);
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); reject_objects_arm(read,true); meta(read,14,0,0); reject_objects_arm(read,true); workbook_codec_read_free(read);
    char *error=NULL;
    for (unsigned version=12;version<=14;version++) {
        read=sort_header(version,2,NULL,LIMIT); size_t owned=workbook_codec_read_owned_bytes(read);
        if (version>=13) reject_objects_arm(read,false);
        else {
            CHECK(workbook_codec_read_add_objects(read,false,&error) && !error && !workbook_codec_read_request(read) && workbook_codec_read_owned_bytes(read)==owned);
            reject_objects_arm(read,true); WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
            CHECK(workbook_codec_header_objects_ready(header) && !workbook_codec_header_object_count(header,0) && !workbook_codec_header_object_count(header,1));
            object_view_unchanged(header,0,0); reject_objects_arm(read,true); workbook_codec_read_free(read); workbook_codec_header_free(header);
            read=sort_header(version,2,NULL,LIMIT);
        }
        CHECK(workbook_codec_read_add_objects(read,true,&error) && !error); const WorkbookCodecRequest *request=workbook_codec_read_request(read);
        CHECK(request && request->family==WORKBOOK_CODEC_OBJECTS && request->row_limit==2049 && request->column_count==11 && request->source_column_count==11 && !request->first_row);
        CHECK(!strcmp(request->sql,"SELECT sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure FROM _duxcel_workspace.objects ORDER BY sheet_id,id LIMIT 2049") && !strstr(request->sql,"OFFSET"));
        reject_objects_arm(read,true); reject_arm(read,true); reject_preferences_arm(read,true);
        ObjectFixture *f=object_fixture(0); WorkbookCodecBatch batch=object_batch(f,0,0,true); WorkbookCodecHeader *header=NULL;
        CHECK(!workbook_codec_read_finish(read,&header,&error) && !header); error_clear(&error);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error && !workbook_codec_read_request(read));
        CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_objects_ready(header));
        CHECK(!workbook_codec_header_sorts_ready(header) && !workbook_codec_header_preferences_ready(header) && !workbook_codec_header_object_count(header,SIZE_MAX));
        object_view_unchanged(header,2,0); workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    }
    for (unsigned objects_first=0;objects_first<2;objects_first++) {
        read=sort_header(14,1,"city",LIMIT); ObjectFixture *f=object_fixture(1); SortFixture *s=sort_fixture(1); PreferenceFixture *p=preference_fixture(1);
        if (objects_first) { CHECK(workbook_codec_read_add_objects(read,true,&error)); WorkbookCodecBatch batch=object_batch(f,0,1,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); }
        CHECK(workbook_codec_read_add_sorts(read,true,&error)); reject_objects_arm(read,true); WorkbookCodecBatch batch=sort_batch(s,0,1,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*s),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
        CHECK(workbook_codec_read_add_preferences(read,true,&error)); reject_objects_arm(read,true); batch=preference_batch(p,0,2,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*p),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
        if (!objects_first) { CHECK(workbook_codec_read_add_objects(read,true,&error)); batch=object_batch(f,0,1,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); }
        WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
        CHECK(workbook_codec_header_objects_ready(header) && workbook_codec_header_sorts_ready(header) && workbook_codec_header_preferences_ready(header) && workbook_codec_header_reduced_motion(header));
        CHECK(workbook_codec_header_object_count(header,0)==1 && workbook_codec_header_analysis(header,0)->sort_count==1); workbook_codec_read_free(read); workbook_codec_header_free(header); free(f); free(s); free(p);
    }
    read=sort_header(14,1,NULL,LIMIT); WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
    CHECK(!workbook_codec_header_objects_ready(header) && !workbook_codec_header_object_count(header,0)); object_view_unchanged(header,0,0); workbook_codec_read_free(read); workbook_codec_header_free(header);
}
static void test_objects_owned_and_references(void) {
    WorkbookCodecRead *read=sort_header(14,2,NULL,LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_objects(read,true,&error)); ObjectFixture *f=object_fixture(4);
    f->cells[9]=u(UINT64_MAX); f->cells[10]=i(7); /* A Pivot's actual Values arity is a later family's obligation. */
    f->cells[11+7]=i(OBJECT_CHART); f->cells[11+8]=i(MARK_SCATTER); f->cells[11+9]=u(UINT64_MAX); f->cells[11+10]=i(3);
    f->cells[22+1]=u(UINT64_MAX); f->cells[22+2]=t("NULL"); f->cells[22+7]=i(OBJECT_TABLE);
    f->cells[33]=i(1); f->cells[33+1]=u(UINT64_MAX); f->cells[33+3]=u(DX_SHEET_ROWS-1); f->cells[33+4]=i(16383); f->cells[33+7]=i(OBJECT_TABLE);
    char name[]="Owned 'İ' 🦆"; f->cells[2]=t(name);
    WorkbookCodecBatch batch=object_batch(f,0,2,false); size_t initial=workbook_codec_read_owned_bytes(read);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)+sizeof(name),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(workbook_codec_read_owned_bytes(read)>initial && workbook_codec_read_request(read)->first_row==2); memset(name,'x',sizeof(name)-1);
    WorkbookCodecHeader *header=NULL; CHECK(!workbook_codec_read_finish(read,&header,&error) && !header); error_clear(&error);
    batch=object_batch(f,2,2,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    size_t all_owned=workbook_codec_read_owned_bytes(read); CHECK(workbook_codec_read_finish(read,&header,&error));
    CHECK(all_owned==workbook_codec_read_owned_bytes(read)+workbook_codec_header_owned_bytes(header)); workbook_codec_read_free(read); memset(f,'x',sizeof(*f)); free(f);
    CHECK(workbook_codec_header_objects_ready(header) && workbook_codec_header_object_count(header,0)==3 && workbook_codec_header_object_count(header,1)==1);
    WorkbookCodecObjectView view={0}; CHECK(workbook_codec_header_object_at(header,0,0,&view));
    CHECK(view.id==1 && view.row==0 && view.column==0 && view.height==1 && view.width==1 && view.source_id==UINT64_MAX && view.kind==OBJECT_PIVOT && view.mark==MARK_BAR && view.chart_measure==7 && !strcmp(view.name,"Owned 'İ' 🦆"));
    CHECK(workbook_codec_header_object_at(header,0,1,&view) && view.kind==OBJECT_CHART && view.mark==MARK_SCATTER && view.chart_measure==3 && view.source_id==UINT64_MAX);
    CHECK(workbook_codec_header_object_at(header,0,2,&view) && view.id==UINT64_MAX && view.kind==OBJECT_TABLE && !view.source_id && !view.chart_measure && !strcmp(view.name,"NULL"));
    CHECK(workbook_codec_header_object_at(header,1,0,&view) && view.id==UINT64_MAX && view.row==DX_SHEET_ROWS-1 && view.column==16383);
    CHECK(!workbook_codec_header_object_at(header,0,0,NULL)); object_view_unchanged(header,0,3); object_view_unchanged(header,1,SIZE_MAX); object_view_unchanged(header,2,0); workbook_codec_header_free(header);
    /* Existing source references may resolve forward, but the last batch is
       not adopted until every reference resolves to a same-sheet Table. */
    read=sort_header(14,2,NULL,LIMIT); CHECK(workbook_codec_read_add_objects(read,true,&error)); f=object_fixture(2); f->cells[9]=u(2); batch=object_batch(f,0,1,false);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    batch=object_batch(f,1,0,true); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f));
    batch=object_batch(f,1,1,true); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); /* Target exists but is a Pivot. */
    f->cells[11+7]=i(OBJECT_TABLE); f->cells[11]=i(1); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); /* Cross-sheet target. */
    f->cells[11]=i(0); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_object_count(header,0)==2); workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
}
static void test_objects_invalid_and_atomic(void) {
    WorkbookCodecRead *read=sort_header(14,2,NULL,LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_objects(read,true,&error)); ObjectFixture *f=object_fixture(2); WorkbookCodecBatch batch=object_batch(f,0,2,true);
    size_t owned=workbook_codec_read_owned_bytes(read); error=dx_strdup("untouched"); char *identity=error; WorkbookCodecKey stale=key; stale.snapshot_generation--;
    CHECK(workbook_codec_read_feed(read,stale,(WorkbookCodecFamily)99,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE && error==identity && workbook_codec_read_owned_bytes(read)==owned); free(error); error=NULL;
    for (size_t col=0;col<11;col++) { WorkbookCodecAtom old=f->cells[11+col]; f->cells[11+col]=(WorkbookCodecAtom){0}; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[11+col]=old; }
    WorkbookCodecAtom name=f->cells[13]; const char *bad_names[]={"","\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80"};
    for (size_t n=0;n<sizeof(bad_names)/sizeof(*bad_names);n++) { f->cells[13]=t(bad_names[n]); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); }
    const char nul[]={'a',0,'b'}; f->cells[13]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={nul,sizeof(nul)}}; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f));
    f->cells[13]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={"x",4097}}; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[13]=name;
    const size_t cols[]={0,1,3,4,5,6,7,8,9,10};
    const WorkbookCodecAtom bad[]={ {.kind=WORKBOOK_CODEC_I32,.value.i32=-1},{.kind=WORKBOOK_CODEC_U64,.value.u64=0},{.kind=WORKBOOK_CODEC_U64,.value.u64=UINT64_MAX},
        {.kind=WORKBOOK_CODEC_I32,.value.i32=-1},{.kind=WORKBOOK_CODEC_U64,.value.u64=UINT64_MAX},{.kind=WORKBOOK_CODEC_I32,.value.i32=16385},
        {.kind=WORKBOOK_CODEC_I32,.value.i32=3},{.kind=WORKBOOK_CODEC_I32,.value.i32=4},{.kind=WORKBOOK_CODEC_U64,.value.u64=1},{.kind=WORKBOOK_CODEC_I32,.value.i32=8} };
    for (size_t n=0;n<sizeof(cols)/sizeof(*cols);n++) { size_t field=11+cols[n]; WorkbookCodecAtom old=f->cells[field]; f->cells[field]=bad[n]; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[field]=old; }
    f->cells[11]=i(2); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[11]=i(0);
    f->cells[12]=u(1); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[12]=u(2);
    f->cells[14]=u(0); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[14]=u(2);
    f->cells[15]=i(16384); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[15]=i(0);
    f->cells[16]=u(0); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[16]=u(DX_SHEET_ROWS); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[16]=u(1);
    f->cells[17]=i(0); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[17]=i(1);
    f->cells[19]=i(MARK_LINE); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[19]=i(MARK_BAR);
    f->cells[18]=i(OBJECT_TABLE); f->cells[20]=u(1); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[20]=u(0);
    f->cells[21]=i(1); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[21]=i(0); f->cells[18]=i(OBJECT_PIVOT);
    f->columns[0].name="Sheet_id"; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->columns[0].name="sheet_id";
    f->columns[1].kind=WORKBOOK_CODEC_DOUBLE; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->columns[1].kind=WORKBOOK_CODEC_U64;
    WorkbookCodecAtom id=f->cells[1]; f->cells[1]=d(1); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[1]=id;
    batch.column_count=10; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); batch.column_count=12; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); batch.column_count=11;
    batch.row_count=257; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); batch.row_count=2;
    batch.cells=NULL; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); batch.cells=f->cells; batch.columns=NULL; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); batch.columns=f->columns;
    batch.first_row=1; reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); batch.first_row=0; reject(read,WORKBOOK_CODEC_SORTS,&batch,sizeof(*f));
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_object_count(header,0)==2); workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    /* Every geometry edge is validated before addition; valid full extents
       remain valid, without overflowing when malicious extents are supplied. */
    read=sort_header(12,1,NULL,LIMIT); CHECK(workbook_codec_read_add_objects(read,true,&error)); f=object_fixture(1); f->cells[5]=u(DX_SHEET_ROWS); f->cells[6]=i(16384); batch=object_batch(f,0,1,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); header=NULL;
    CHECK(workbook_codec_read_finish(read,&header,&error)); WorkbookCodecObjectView view={0}; CHECK(workbook_codec_header_object_at(header,0,0,&view) && view.height==DX_SHEET_ROWS && view.width==16384);
    workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
}
static void test_objects_limits_and_budget(void) {
    WorkbookCodecRead *read=sort_header(14,64,NULL,LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_objects(read,true,&error)); ObjectFixture *f=object_fixture(2049);
    for (size_t first=0;first<2048;first+=256) {
        WorkbookCodecBatch batch=object_batch(f,first,256,false); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
        CHECK(workbook_codec_read_request(read)->first_row==first+256);
    }
    WorkbookCodecBatch sentinel=object_batch(f,2048,1,true); reject(read,WORKBOOK_CODEC_OBJECTS,&sentinel,sizeof(*f));
    sentinel.row_count=0; CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&sentinel,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_objects_ready(header));
    for (size_t sheet=0;sheet<64;sheet++) {
        CHECK(workbook_codec_header_object_count(header,sheet)==32); WorkbookCodecObjectView view={0};
        CHECK(workbook_codec_header_object_at(header,sheet,31,&view) && view.id==32 && view.row==62 && !strcmp(view.name,f->names[sheet*32+31]));
    }
    workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    read=sort_header(14,2,NULL,LIMIT); CHECK(workbook_codec_read_add_objects(read,true,&error)); f=object_fixture(33); f->cells[32*11]=i(0); f->cells[32*11+1]=u(33); f->cells[32*11+3]=u(64);
    WorkbookCodecBatch batch=object_batch(f,0,32,false); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    batch=object_batch(f,32,1,true); reject(read,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)); f->cells[32*11]=i(1); f->cells[32*11+1]=u(1);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); header=NULL;
    CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_object_count(header,0)==32 && workbook_codec_header_object_count(header,1)==1); workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    /* Account precisely for old-array/new-array coexistence, independently
       owned old output, input fixture and an actual retained pressure buffer. */
    WorkbookCodecRead *prior=sort_header(12,1,NULL,LIMIT); CHECK(workbook_codec_read_add_objects(prior,false,&error)); header=NULL;
    CHECK(workbook_codec_read_finish(prior,&header,&error)); workbook_codec_read_free(prior); WorkbookCodecHeader *old_header=header;
    read=sort_header(14,1,NULL,LIMIT); CHECK(workbook_codec_read_add_objects(read,true,&error)); f=object_fixture(2); char *long_name=dx_alloc(4097); memset(long_name,'n',4096); long_name[4096]=0; f->cells[2]=t(long_name); f->cells[13]=t("NULL");
    batch=object_batch(f,0,1,false); size_t before=workbook_codec_read_owned_bytes(read);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f)+4097,&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    size_t with_one=workbook_codec_read_owned_bytes(read),array_bytes=with_one-before-4097;
    CHECK(array_bytes>=sizeof(WorkbookCodecObjectView)); batch=object_batch(f,1,1,true);
    size_t old_bytes=workbook_codec_header_owned_bytes(header),retained=old_bytes+sizeof(*f)+4097,required=2*array_bytes+5;
    CHECK(with_one+retained+required<LIMIT); size_t pressure_bytes=LIMIT-with_one-retained-required; unsigned char *pressure=dx_alloc(pressure_bytes+1);
    reject(read,WORKBOOK_CODEC_OBJECTS,&batch,retained+pressure_bytes+1);
    CHECK(!workbook_codec_read_finish(read,&header,&error) && header==old_header && workbook_codec_header_owned_bytes(header)==old_bytes); error_clear(&error);
    CHECK(workbook_codec_read_owned_bytes(read)==with_one && workbook_codec_read_request(read)->first_row==1);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(workbook_codec_read_owned_bytes(read)==with_one+array_bytes+5); /* New persistent size excludes retired old array. */
    memset(long_name,'x',4096); free(long_name); free(pressure); free(f); CHECK(workbook_codec_read_finish(read,&header,&error));
    WorkbookCodecObjectView view={0}; CHECK(workbook_codec_header_object_at(header,0,0,&view) && strlen(view.name)==4096 && view.name[4095]=='n');
    CHECK(workbook_codec_header_object_at(header,0,1,&view) && !strcmp(view.name,"NULL")); workbook_codec_read_free(read); workbook_codec_header_free(header);
}
typedef struct { WorkbookCodecColumn columns[3]; WorkbookCodecAtom cells[2113*3]; } QueryFixture;
static QueryFixture *query_fixture(size_t count) {
    QueryFixture *f=dx_alloc(sizeof(*f));
    f->columns[0]=(WorkbookCodecColumn){"sheet_id",WORKBOOK_CODEC_I32};
    f->columns[1]=(WorkbookCodecColumn){"object_id",WORKBOOK_CODEC_U64};
    f->columns[2]=(WorkbookCodecColumn){"query",WORKBOOK_CODEC_TEXT};
    for (size_t row=0;row<count;row++) {
        size_t owner=row%33; f->cells[row*3]=i((int32_t)(row/33));
        f->cells[row*3+1]=u(owner==32 ? UINT64_MAX : owner); f->cells[row*3+2]=t("SELECT 'İ', 'NULL';\n");
    }
    return f;
}
static WorkbookCodecBatch query_batch(QueryFixture *f,size_t first,size_t count,bool end) {
    return (WorkbookCodecBatch){f->columns,3,f->cells+first*3,count,first,end};
}
static WorkbookCodecRead *query_header(unsigned version,size_t sheets,size_t per_sheet,SourceKind primary,bool all_tables,size_t limit) {
    WorkbookCodecRead *read=begin(version,WORKBOOK_CODEC_SHEETS_25,limit,NULL); meta(read,version,0,0);
    Fixture *s=dx_alloc(sizeof(*s)); fixture_columns(s,"sheets");
    for (size_t row=0;row<sheets;row++) sheet(s,row,primary);
    WorkbookCodecBatch batch={s->columns,25,s->cells,sheets,0,true}; char *error=NULL;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*s),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error); free(s);
    ObjectFixture *o=object_fixture(0); size_t total=sheets*per_sheet;
    for (size_t row=0;row<total;row++) {
        size_t position=row%per_sheet; WorkbookCodecAtom *c=o->cells+row*11;
        c[0]=i((int32_t)(row/per_sheet)); c[1]=u(position==31 ? UINT64_MAX : position+1); c[2]=t("Table 'İ'");
        c[3]=u(position*2); c[4]=i(0); c[5]=u(1); c[6]=i(1);
        c[7]=i(!all_tables && position<2 ? (int32_t)position : OBJECT_TABLE); c[8]=i(MARK_BAR);
        c[9]=u(!all_tables && position<2 ? 3 : 0); c[10]=i(0);
    }
    CHECK(workbook_codec_read_add_objects(read,true,&error));
    for (size_t first=0;first<total || (!total && !first);) {
        size_t count=total-first>256 ? 256 : total-first; batch=object_batch(o,first,count,first+count==total);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*o),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
        first+=count; if (!count) break;
    }
    free(o); return read;
}
static void reject_queries_arm(WorkbookCodecRead *read,bool present) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(read); WorkbookCodecRequest before=request ? *request : (WorkbookCodecRequest){0};
    size_t bytes=workbook_codec_read_owned_bytes(read); char *error=NULL;
    CHECK(!workbook_codec_read_add_queries(read,present,&error) && error && *error && workbook_codec_read_owned_bytes(read)==bytes);
    CHECK(request ? same_request(before,workbook_codec_read_request(read)) : !workbook_codec_read_request(read)); error_clear(&error);
}
static void query_get_unchanged(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner) {
    const char *out="untouched",*before=out;
    CHECK(!workbook_codec_header_query(header,sheet,owner,&out) && out==before);
}
static void test_queries_contract_and_empty(void) {
    CHECK(WORKBOOK_CODEC_QUERIES==7 && !workbook_codec_header_queries_ready(NULL)); query_get_unchanged(NULL,0,0); reject_queries_arm(NULL,true);
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); reject_queries_arm(read,true); meta(read,14,0,0); reject_queries_arm(read,true); workbook_codec_read_free(read);
    read=sort_header(14,1,NULL,LIMIT); reject_queries_arm(read,true); workbook_codec_read_free(read);
    char *error=NULL;
    for (unsigned version=12;version<=14;version++) {
        read=query_header(version,1,1,SOURCE_NONE,true,LIMIT); reject_queries_arm(read,false);
        CHECK(workbook_codec_read_add_queries(read,true,&error) && !error);
        const WorkbookCodecRequest *r=workbook_codec_read_request(read);
        CHECK(r && r->family==WORKBOOK_CODEC_QUERIES && r->column_count==3 && r->source_column_count==3 && r->row_limit==2113 && !r->first_row);
        CHECK(!strcmp(r->sql,"SELECT sheet_id,object_id,CASE WHEN query IS NULL OR octet_length(encode(query))>65536 OR length(trim(query))=0 OR contains(query,chr(0)) THEN NULL::VARCHAR ELSE query END AS query FROM _duxcel_workspace.queries ORDER BY sheet_id,object_id LIMIT 2113"));
        reject_queries_arm(read,true); reject_objects_arm(read,true); reject_arm(read,true); reject_preferences_arm(read,true);
        QueryFixture *f=query_fixture(1); WorkbookCodecBatch batch=query_batch(f,0,1,true); reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)); /* NONE cannot store a primary query. */
        WorkbookCodecHeader *header=NULL; CHECK(!workbook_codec_read_finish(read,&header,&error) && !header); error_clear(&error);
        batch=query_batch(f,0,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
        CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_queries_ready(header));
        const char *out="old"; CHECK(workbook_codec_header_query(header,0,0,&out) && !out && !workbook_codec_header_analysis(header,0)->query);
        CHECK(workbook_codec_header_query(header,0,1,&out) && !out); query_get_unchanged(header,1,0); query_get_unchanged(header,0,2);
        CHECK(!workbook_codec_header_query(header,0,0,NULL)); reject_queries_arm(read,true);
        workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    }
    read=query_header(14,1,0,SOURCE_DEMO,true,LIMIT); WorkbookCodecHeader *header=NULL;
    CHECK(workbook_codec_read_finish(read,&header,&error) && !workbook_codec_header_queries_ready(header)); query_get_unchanged(header,0,0);
    workbook_codec_read_free(read); workbook_codec_header_free(header);
}
static void test_queries_owned_and_validation(void) {
    WorkbookCodecRead *read=query_header(14,2,3,SOURCE_DEMO,false,LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_queries(read,true,&error));
    QueryFixture *f=query_fixture(3); WorkbookCodecBatch batch=query_batch(f,0,2,true);
    f->cells[4]=u(3); char raw[]="  SELECT 'O''İ', \"NULL\";\n\t"; f->cells[2]=t(raw); f->cells[5]=t("NULL");
    size_t owned=workbook_codec_read_owned_bytes(read); error=dx_strdup("preserve stale"); char *old_error=error;
    for (size_t part=0;part<3;part++) {
        WorkbookCodecKey stale=key; if (!part) stale.document_epoch--; else if (part==1) stale.operation_ticket--; else stale.snapshot_generation--;
        CHECK(workbook_codec_read_feed(read,stale,(WorkbookCodecFamily)99,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE && error==old_error && workbook_codec_read_owned_bytes(read)==owned);
    }
    free(error); error=NULL;
    for (size_t c=0;c<3;c++) { WorkbookCodecAtom saved=f->cells[c]; f->cells[c]=(WorkbookCodecAtom){0}; reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)); f->cells[c]=saved; }
    for (size_t c=0;c<3;c++) { WorkbookCodecAtomKind saved=f->columns[c].kind; f->columns[c].kind=WORKBOOK_CODEC_BOOLEAN; reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)); f->columns[c].kind=saved; }
    const char *name=f->columns[1].name; f->columns[1].name="owner"; reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)); f->columns[1].name=name;
    const uint64_t bad_owners[]={0,1,2,4,UINT64_MAX};
    for (size_t n=0;n<sizeof(bad_owners)/sizeof(*bad_owners);n++) { f->cells[4]=u(bad_owners[n]); reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)); } f->cells[4]=u(3);
    for (int32_t n=-1;n<=2;n+=3) { f->cells[3]=i(n); reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)); } f->cells[3]=i(0);
    const char *bad_text[]={"","\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\x80","\xe2\x82"};
    for (size_t n=0;n<sizeof(bad_text)/sizeof(*bad_text);n++) { f->cells[5]=t(bad_text[n]); reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)); }
    f->cells[5]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={"a\0b",3}}; reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f));
    f->cells[5]=t("NULL"); batch.first_row=1; reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)); batch.first_row=0;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)+sizeof(raw),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    memset(raw,'x',strlen(raw)); free(f); WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    const char *query=NULL; CHECK(workbook_codec_header_query(header,0,0,&query) && !strcmp(query,"  SELECT 'O''İ', \"NULL\";\n\t") && query==workbook_codec_header_analysis(header,0)->query);
    CHECK(workbook_codec_header_query(header,0,3,&query) && !strcmp(query,"NULL"));
    CHECK(workbook_codec_header_query(header,1,0,&query) && !query && workbook_codec_header_query(header,1,3,&query) && !query);
    query_get_unchanged(header,0,1); query_get_unchanged(header,0,2); query_get_unchanged(header,0,4); query_get_unchanged(header,2,3);
    workbook_codec_header_free(header);
}
static void test_queries_limits_atomic_budget(void) {
    char *error=NULL; QueryFixture *f=query_fixture(2113);
    WorkbookCodecRead *read=query_header(14,64,32,SOURCE_DEMO,true,LIMIT); CHECK(workbook_codec_read_add_queries(read,true,&error));
    WorkbookCodecBatch batch=query_batch(f,0,257,false); reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f));
    batch=query_batch(f,0,256,false); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    size_t with_prefix=workbook_codec_read_owned_bytes(read); WorkbookCodecAtom saved=f->cells[511*3+1]; f->cells[511*3+1]=u(UINT64_MAX-1);
    batch=query_batch(f,256,256,false); reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)); CHECK(workbook_codec_read_owned_bytes(read)==with_prefix); f->cells[511*3+1]=saved;
    for (size_t first=256;first<2112;first+=256) {
        size_t count=2112-first>256 ? 256 : 2112-first; batch=query_batch(f,first,count,false);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    }
    batch=query_batch(f,2112,1,true); reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f));
    WorkbookCodecHeader *header=NULL; CHECK(!workbook_codec_read_finish(read,&header,&error) && !header); error_clear(&error);
    batch=query_batch(f,2112,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_queries_ready(header)); workbook_codec_read_free(read);
    for (size_t sheet=0;sheet<64;sheet++) for (size_t owner=0;owner<33;owner++) {
        const char *text=NULL; CHECK(workbook_codec_header_query(header,sheet,owner==32 ? UINT64_MAX : owner,&text) && !strcmp(text,"SELECT 'İ', 'NULL';\n"));
    }
    const char *kept=NULL; CHECK(workbook_codec_header_query(header,63,UINT64_MAX,&kept)); WorkbookCodecHeader *prior=header; size_t prior_bytes=workbook_codec_header_owned_bytes(header);
    read=query_header(12,1,1,SOURCE_DEMO,true,LIMIT); CHECK(workbook_codec_read_add_queries(read,true,&error));
    char *large=dx_alloc(65538); memset(large,'q',65537); large[65537]=0; f->cells[0]=i(0); f->cells[1]=u(0); f->cells[2]=t(large); batch=query_batch(f,0,1,true);
    reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f)+65538+prior_bytes); f->cells[2].value.text.bytes=65536;
    size_t current=workbook_codec_read_owned_bytes(read),retained=sizeof(*f)+65538+prior_bytes;
    /* One staged destination pointer + one owned-text pointer coexist with the
       copied65537-byte string, reader, original input and independent output. */
    size_t staging=2*sizeof(char *)+65537; CHECK(current+retained+staging<LIMIT);
    size_t pressure_bytes=LIMIT-current-retained-staging; unsigned char *pressure=dx_alloc(pressure_bytes+1);
    reject(read,WORKBOOK_CODEC_QUERIES,&batch,retained+pressure_bytes+1);
    CHECK(!workbook_codec_read_finish(read,&header,&error) && header==prior && workbook_codec_header_owned_bytes(header)==prior_bytes); error_clear(&error);
    CHECK(workbook_codec_header_query(header,63,UINT64_MAX,&kept) && !strcmp(kept,"SELECT 'İ', 'NULL';\n"));
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(workbook_codec_read_owned_bytes(read)==current+65537); memset(large,'x',65537); free(large); free(pressure); free(f);
    CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    const char *text=NULL; CHECK(workbook_codec_header_query(header,0,0,&text) && strlen(text)==65536 && text[0]=='q' && text[65535]=='q'); workbook_codec_header_free(header);
}
static void test_queries_composition(void) {
    for (unsigned order=0;order<2;order++) {
        WorkbookCodecRead *read=query_header(14,1,1,SOURCE_DEMO,true,LIMIT); char *error=NULL;
        for (unsigned phase=0;phase<2;phase++) {
            if (phase==order) {
                CHECK(workbook_codec_read_add_queries(read,true,&error)); QueryFixture *f=query_fixture(1); f->cells[1]=u(1);
                /* The pure cursor preserves SQL whitespace bytes; trusted
                   C-built trim validation is the adapter's prerequisite. */
                f->cells[2]=t(" \t\n"); WorkbookCodecBatch batch=query_batch(f,0,1,true);
                CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(f); reject_queries_arm(read,true);
            } else {
                CHECK(workbook_codec_read_add_sorts(read,true,&error)); SortFixture *s=sort_fixture(0); WorkbookCodecBatch batch=sort_batch(s,0,0,true);
                CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*s),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(s);
                CHECK(workbook_codec_read_add_preferences(read,true,&error)); PreferenceFixture *p=preference_fixture(0); batch=preference_batch(p,0,0,true);
                CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*p),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(p);
            }
        }
        WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error));
        CHECK(workbook_codec_header_queries_ready(header) && workbook_codec_header_sorts_ready(header) && workbook_codec_header_preferences_ready(header));
        const char *text=NULL; CHECK(workbook_codec_header_query(header,0,1,&text) && !strcmp(text," \t\n")); workbook_codec_read_free(read); workbook_codec_header_free(header);
    }
}
static void test_queries_foreign_and_partial_budget(void) {
    /* Each Table has a different worksheet-local ID. Existence on another
       worksheet cannot authorize a definition or resolve through a Pivot. */
    WorkbookCodecRead *read=sort_header(14,2,NULL,LIMIT); char *error=NULL;
    ObjectFixture *o=object_fixture(2); o->cells[7]=i(OBJECT_TABLE);
    o->cells[11]=i(1); o->cells[12]=u(UINT64_MAX); o->cells[18]=i(OBJECT_TABLE);
    CHECK(workbook_codec_read_add_objects(read,true,&error)); WorkbookCodecBatch batch=object_batch(o,0,2,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*o),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(o);
    CHECK(workbook_codec_read_add_queries(read,true,&error)); QueryFixture *f=query_fixture(2);
    f->cells[1]=u(UINT64_MAX); batch=query_batch(f,0,1,true); reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f));
    f->cells[0]=i(1); f->cells[1]=u(1); reject(read,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f));
    f->cells[0]=i(0); f->cells[1]=u(1); f->cells[2]=t("first retained;\n"); batch.end=false;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    size_t owned=workbook_codec_read_owned_bytes(read); f->cells[3]=i(1); f->cells[4]=u(UINT64_MAX); f->cells[5]=t("second retained;\n");
    batch=query_batch(f,1,1,true);
    /* Force a text-copy failure after the change array was admitted. A failed
       suffix must preserve both the prior definition and its cursor, then retry. */
    size_t retained=sizeof(*f),available=2*sizeof(char *)+strlen("second retained;\n");
    CHECK(owned+retained+available<LIMIT); size_t pressure_bytes=LIMIT-owned-retained-available;
    unsigned char *pressure=dx_alloc(pressure_bytes);
    reject(read,WORKBOOK_CODEC_QUERIES,&batch,retained+pressure_bytes);
    CHECK(workbook_codec_read_request(read)->first_row==1 && workbook_codec_read_owned_bytes(read)==owned);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    free(pressure); memset(f->cells,0,sizeof(f->cells)); free(f); WorkbookCodecHeader *header=NULL;
    CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    const char *text=NULL; CHECK(workbook_codec_header_query(header,0,1,&text) && !strcmp(text,"first retained;\n"));
    CHECK(workbook_codec_header_query(header,1,UINT64_MAX,&text) && !strcmp(text,"second retained;\n"));
    CHECK(workbook_codec_header_query(header,0,0,&text) && !text); /* Both primaries are blank. */
    query_get_unchanged(header,0,UINT64_MAX); query_get_unchanged(header,1,1); workbook_codec_header_free(header);
}
static QueryFixture *source_options_fixture(size_t count) {
    QueryFixture *f=query_fixture(count); f->columns[2]=(WorkbookCodecColumn){"unique_rows",WORKBOOK_CODEC_BOOLEAN};
    for (size_t row=0;row<count;row++) f->cells[row*3+2]=b(row%2!=0);
    return f;
}
static void reject_source_options_arm(WorkbookCodecRead *read,bool present) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(read); WorkbookCodecRequest before=request ? *request : (WorkbookCodecRequest){0};
    size_t bytes=workbook_codec_read_owned_bytes(read); char *error=NULL;
    CHECK(!workbook_codec_read_add_source_options(read,present,&error) && error && *error && workbook_codec_read_owned_bytes(read)==bytes);
    CHECK(request ? same_request(before,workbook_codec_read_request(read)) : !workbook_codec_read_request(read)); error_clear(&error);
}
static void source_option_get_unchanged(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner) {
    for (unsigned initial=0;initial<2;initial++) { bool out=initial!=0; CHECK(!workbook_codec_header_unique_rows(header,sheet,owner,&out) && out==(initial!=0)); }
}
static void test_source_options_contract(void) {
    CHECK(WORKBOOK_CODEC_SOURCE_OPTIONS==8 && !workbook_codec_header_source_options_ready(NULL)); source_option_get_unchanged(NULL,0,0); reject_source_options_arm(NULL,true);
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); reject_source_options_arm(read,true); meta(read,14,0,0); reject_source_options_arm(read,true); workbook_codec_read_free(read);
    read=sort_header(14,1,NULL,LIMIT); reject_source_options_arm(read,true); workbook_codec_read_free(read); char *error=NULL;
    for (unsigned version=12;version<=14;version++) {
        read=query_header(version,1,1,SOURCE_NONE,true,LIMIT); reject_source_options_arm(read,false);
        CHECK(workbook_codec_read_add_source_options(read,true,&error) && !error); const WorkbookCodecRequest *r=workbook_codec_read_request(read);
        CHECK(r && r->family==WORKBOOK_CODEC_SOURCE_OPTIONS && r->column_count==3 && r->source_column_count==3 && r->row_limit==2113 && !r->first_row);
        CHECK(!strcmp(r->sql,"SELECT sheet_id,object_id,unique_rows FROM _duxcel_workspace.source_options ORDER BY sheet_id,object_id LIMIT 2113"));
        reject_source_options_arm(read,true); reject_queries_arm(read,true); reject_objects_arm(read,true); reject_arm(read,true); reject_preferences_arm(read,true);
        QueryFixture *f=source_options_fixture(1); f->cells[2]=b(true); WorkbookCodecBatch batch=query_batch(f,0,1,true);
        reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); /* True on a blank primary is invalid; false is a real stored option. */
        f->cells[2]=b(false); size_t owned=workbook_codec_read_owned_bytes(read);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && workbook_codec_read_owned_bytes(read)==owned);
        WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_source_options_ready(header));
        bool value=true; CHECK(workbook_codec_header_unique_rows(header,0,0,&value) && !value && !workbook_codec_header_analysis(header,0)->unique_rows);
        CHECK(workbook_codec_header_unique_rows(header,0,1,&value) && !value); source_option_get_unchanged(header,1,0); source_option_get_unchanged(header,0,2);
        CHECK(!workbook_codec_header_unique_rows(header,0,0,NULL)); reject_source_options_arm(read,true);
        workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    }
    read=query_header(14,1,0,SOURCE_NONE,true,LIMIT); CHECK(workbook_codec_read_add_source_options(read,true,&error));
    QueryFixture *f=source_options_fixture(0); WorkbookCodecBatch batch=query_batch(f,0,0,true); size_t owned=workbook_codec_read_owned_bytes(read);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && workbook_codec_read_owned_bytes(read)==owned);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); bool value=true;
    CHECK(workbook_codec_header_unique_rows(header,0,0,&value) && !value); workbook_codec_read_free(read); workbook_codec_header_free(header); free(f);
    read=query_header(12,1,1,SOURCE_DEMO,true,LIMIT); header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && !workbook_codec_header_source_options_ready(header));
    source_option_get_unchanged(header,0,0); source_option_get_unchanged(header,0,1); workbook_codec_read_free(read); workbook_codec_header_free(header);
}
static void test_source_options_atomic_and_types(void) {
    WorkbookCodecRead *read=query_header(14,2,3,SOURCE_DEMO,false,LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_source_options(read,true,&error));
    QueryFixture *f=source_options_fixture(2); f->cells[2]=b(true); f->cells[4]=u(3); WorkbookCodecBatch batch=query_batch(f,0,2,true);
    size_t owned=workbook_codec_read_owned_bytes(read); error=dx_strdup("old stale error"); char *old=error;
    for (size_t part=0;part<3;part++) {
        WorkbookCodecKey stale=key; if (!part) stale.document_epoch--; else if (part==1) stale.operation_ticket--; else stale.snapshot_generation--;
        CHECK(workbook_codec_read_feed(read,stale,(WorkbookCodecFamily)99,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE && error==old && workbook_codec_read_owned_bytes(read)==owned);
    }
    free(error); error=NULL;
    for (size_t col=0;col<3;col++) {
        WorkbookCodecAtom saved=f->cells[col]; f->cells[col]=(WorkbookCodecAtom){0}; reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); f->cells[col]=saved;
        WorkbookCodecAtomKind kind=f->columns[col].kind; f->columns[col].kind=col==2 ? WORKBOOK_CODEC_I32 : WORKBOOK_CODEC_BOOLEAN; reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); f->columns[col].kind=kind;
    }
    const char *name=f->columns[2].name; f->columns[2].name="unique"; reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); f->columns[2].name=name;
    f->cells[5]=i(0); reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); f->cells[5]=b(false);
    const uint64_t wrong[]={0,1,2,4,UINT64_MAX};
    for (size_t n=0;n<sizeof(wrong)/sizeof(*wrong);n++) { f->cells[4]=u(wrong[n]); reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); } f->cells[4]=u(3);
    f->cells[3]=i(-1); reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); f->cells[3]=i(2); reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); f->cells[3]=i(0);
    /* All preceding failures include an otherwise valid primary=true prefix.
       Completing empty proves no flag/seen bit was partially published. */
    WorkbookCodecHeader *header=NULL; CHECK(!workbook_codec_read_finish(read,&header,&error) && !header); error_clear(&error);
    batch=query_batch(f,0,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    CHECK(workbook_codec_read_finish(read,&header,&error)); bool value=true; CHECK(workbook_codec_header_unique_rows(header,0,0,&value) && !value);
    CHECK(workbook_codec_header_unique_rows(header,0,3,&value) && !value); source_option_get_unchanged(header,0,1); source_option_get_unchanged(header,0,2);
    workbook_codec_read_free(read); workbook_codec_header_free(header);
    read=query_header(14,1,1,SOURCE_DEMO,true,LIMIT); CHECK(workbook_codec_read_add_source_options(read,true,&error));
    f->cells[0]=i(0); f->cells[1]=u(0); f->cells[2]=b(false); f->cells[3]=i(0); f->cells[4]=u(0); f->cells[5]=b(false); batch=query_batch(f,0,2,true);
    reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); f->cells[4]=u(1);
    batch=query_batch(f,0,1,false); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    WorkbookCodecAtom saved=f->cells[4]; f->cells[4]=u(0); batch=query_batch(f,1,1,true); reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); f->cells[4]=saved; f->cells[5]=b(true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); memset(f->cells,0,sizeof(f->cells)); free(f);
    header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    CHECK(workbook_codec_header_unique_rows(header,0,0,&value) && !value); CHECK(workbook_codec_header_unique_rows(header,0,1,&value) && value); workbook_codec_header_free(header);
}
static void test_source_options_limits_budget_and_foreign(void) {
    WorkbookCodecRead *read=query_header(14,64,32,SOURCE_DEMO,true,LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_source_options(read,true,&error));
    QueryFixture *f=source_options_fixture(2113); WorkbookCodecBatch batch=query_batch(f,0,257,false); reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f));
    size_t owned=workbook_codec_read_owned_bytes(read);
    for (size_t first=0;first<2112;first+=256) {
        size_t count=2112-first>256 ? 256 : 2112-first; batch=query_batch(f,first,count,false);
        if (first==256) { WorkbookCodecAtom saved=f->cells[511*3+1]; f->cells[511*3+1]=u(UINT64_MAX-1); reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); f->cells[511*3+1]=saved; }
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error && workbook_codec_read_owned_bytes(read)==owned);
    }
    batch=query_batch(f,2112,1,true); reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f));
    batch=query_batch(f,2112,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    for (size_t sheet=0;sheet<64;sheet++) for (size_t owner=0;owner<33;owner++) {
        bool value=false; CHECK(workbook_codec_header_unique_rows(header,sheet,owner==32 ? UINT64_MAX : owner,&value) && value==((sheet*33+owner)%2!=0));
    }
    WorkbookCodecHeader *prior=header; size_t prior_bytes=workbook_codec_header_owned_bytes(header);
    read=query_header(14,1,1,SOURCE_DEMO,true,LIMIT); CHECK(workbook_codec_read_add_source_options(read,true,&error));
    f->cells[0]=i(0); f->cells[1]=u(0); f->cells[2]=b(true); batch=query_batch(f,0,1,true); owned=workbook_codec_read_owned_bytes(read);
    size_t retained=sizeof(*f)+prior_bytes; CHECK(owned+retained<LIMIT); size_t pressure_bytes=LIMIT-owned-retained; unsigned char *pressure=dx_alloc(pressure_bytes+1);
    reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,retained+pressure_bytes+1);
    CHECK(!workbook_codec_read_finish(read,&header,&error) && header==prior && workbook_codec_header_owned_bytes(header)==prior_bytes); error_clear(&error);
    bool value=false; CHECK(workbook_codec_header_unique_rows(header,63,UINT64_MAX,&value) && value);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && workbook_codec_read_owned_bytes(read)==owned);
    free(pressure); CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    CHECK(workbook_codec_header_unique_rows(header,0,0,&value) && value && workbook_codec_header_analysis(header,0)->unique_rows); workbook_codec_header_free(header);
    read=sort_header(14,2,NULL,LIMIT); ObjectFixture *o=object_fixture(2); o->cells[7]=i(OBJECT_TABLE); o->cells[11]=i(1); o->cells[12]=u(UINT64_MAX); o->cells[18]=i(OBJECT_TABLE);
    CHECK(workbook_codec_read_add_objects(read,true,&error)); batch=object_batch(o,0,2,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*o),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(o); CHECK(workbook_codec_read_add_source_options(read,true,&error));
    f->cells[1]=u(UINT64_MAX); f->cells[2]=b(false); batch=query_batch(f,0,1,true); reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f));
    f->cells[0]=i(1); f->cells[1]=u(1); reject(read,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f)); f->cells[1]=u(UINT64_MAX);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(f);
    header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    source_option_get_unchanged(header,0,UINT64_MAX); source_option_get_unchanged(header,1,1); CHECK(workbook_codec_header_unique_rows(header,1,UINT64_MAX,&value) && !value); workbook_codec_header_free(header);
}
static void test_source_options_queries_composition(void) {
    for (unsigned order=0;order<2;order++) {
        WorkbookCodecRead *read=query_header(14,1,1,SOURCE_DEMO,true,LIMIT); char *error=NULL;
        for (unsigned phase=0;phase<2;phase++) {
            QueryFixture *f=NULL;
            if (phase==order) {
                CHECK(workbook_codec_read_add_source_options(read,true,&error)); f=source_options_fixture(2); f->cells[2]=b(true); f->cells[5]=b(false); WorkbookCodecBatch batch=query_batch(f,0,2,true);
                CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); reject_source_options_arm(read,true);
            } else {
                CHECK(workbook_codec_read_add_queries(read,true,&error)); f=query_fixture(2); f->cells[2]=t("SELECT 'retained';  \n"); f->cells[5]=t("NULL"); WorkbookCodecBatch batch=query_batch(f,0,2,true);
                CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); reject_queries_arm(read,true);
            }
            free(f);
        }
        CHECK(workbook_codec_read_add_sorts(read,true,&error)); SortFixture *s=sort_fixture(0); WorkbookCodecBatch batch=sort_batch(s,0,0,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SORTS,&batch,sizeof(*s),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(s);
        CHECK(workbook_codec_read_add_preferences(read,true,&error)); PreferenceFixture *p=preference_fixture(0); batch=preference_batch(p,0,0,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PREFERENCES,&batch,sizeof(*p),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(p);
        WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
        CHECK(workbook_codec_header_source_options_ready(header) && workbook_codec_header_queries_ready(header) && workbook_codec_header_sorts_ready(header) && workbook_codec_header_preferences_ready(header));
        bool value=false; const char *query=NULL; CHECK(workbook_codec_header_unique_rows(header,0,0,&value) && value && workbook_codec_header_analysis(header,0)->unique_rows);
        CHECK(workbook_codec_header_query(header,0,0,&query) && !strcmp(query,"SELECT 'retained';  \n"));
        CHECK(workbook_codec_header_unique_rows(header,0,1,&value) && !value && workbook_codec_header_query(header,0,1,&query) && !strcmp(query,"NULL")); workbook_codec_header_free(header);
    }
}
#define TABLE_TEST_LIMIT (32u*1024u*1024u)
typedef struct { unsigned calls; int mode; } TableResolver;
static bool table_resolve(void *context,SourceKind kind,const char *reference,size_t maximum,WorkbookCodecOwnedText *out) {
    TableResolver *r=context; r->calls++; CHECK(kind>=SOURCE_CSV && kind<=SOURCE_DATABASE && reference && maximum);
    if (r->mode==1) { out->data=dx_strdup(""); out->bytes=0; return true; }
    if (r->mode==2) { out->data=dx_strdup("x"); out->bytes=SIZE_MAX; return true; }
    if (r->mode==3) { out->data=dx_strdup("owned failure"); out->bytes=13; return false; }
    if (r->mode==4) { out->data=dx_alloc(4); memcpy(out->data,"a\0b",4); out->bytes=3; return true; }
    size_t bytes=strlen(reference)+8; if (bytes>=maximum) return false;
    out->data=dx_printf("logical:%s",reference); out->bytes=bytes; return true;
}
static uint64_t table_test_id(size_t position) { return position==31 ? UINT64_MAX : position+1; }
static WorkbookCodecRead *table_header_text(unsigned version,size_t sheets,size_t per_sheet,size_t limit,TableResolver *resolver,const char *query_text) {
    WorkbookCodecRead *read=NULL; char *error=NULL;
    CHECK(workbook_codec_read_begin(&read,key,version,WORKBOOK_CODEC_SHEETS_25,limit,resolver ? table_resolve : NULL,resolver,&error)); meta(read,version,0,0);
    Fixture *s=dx_alloc(sizeof(*s)); fixture_columns(s,"sheets"); for (size_t row=0;row<sheets;row++) sheet(s,row,SOURCE_NONE);
    WorkbookCodecBatch batch={s->columns,25,s->cells,sheets,0,true}; CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*s),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(s);
    size_t total=sheets*per_sheet; ObjectFixture *o=object_fixture(total);
    for (size_t row=0;row<total;row++) { WorkbookCodecAtom *c=o->cells+row*11; c[0]=i((int32_t)(row/per_sheet)); c[1]=u(table_test_id(row%per_sheet)); c[3]=u((row%per_sheet)*2); c[7]=i(OBJECT_TABLE); }
    CHECK(workbook_codec_read_add_objects(read,true,&error));
    for (size_t first=0;first<total || (!total && !first);) { size_t n=total-first>256 ? 256 : total-first; batch=object_batch(o,first,n,first+n==total); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*o),&error)==WORKBOOK_CODEC_READ_ACCEPTED); first+=n; if (!n) break; }
    free(o); QueryFixture *q=query_fixture(total);
    for (size_t row=0;row<total;row++) { q->cells[row*3]=i((int32_t)(row/per_sheet)); q->cells[row*3+1]=u(table_test_id(row%per_sheet)); q->cells[row*3+2]=t(query_text); }
    CHECK(workbook_codec_read_add_queries(read,true,&error));
    for (size_t first=0;first<total || (!total && !first);) { size_t n=total-first>256 ? 256 : total-first; batch=query_batch(q,first,n,first+n==total); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*q)+strlen(query_text)+1u,&error)==WORKBOOK_CODEC_READ_ACCEPTED); first+=n; if (!n) break; }
    q->columns[2]=(WorkbookCodecColumn){"unique_rows",WORKBOOK_CODEC_BOOLEAN}; for (size_t row=0;row<total;row++) q->cells[row*3+2]=b(true);
    CHECK(workbook_codec_read_add_source_options(read,true,&error));
    for (size_t first=0;first<total || (!total && !first);) { size_t n=total-first>256 ? 256 : total-first; batch=query_batch(q,first,n,first+n==total); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*q)+strlen(query_text)+1u,&error)==WORKBOOK_CODEC_READ_ACCEPTED); first+=n; if (!n) break; }
    free(q); return read;
}
static WorkbookCodecRead *table_header(unsigned version,size_t sheets,size_t per_sheet,size_t limit,TableResolver *resolver) {
    return table_header_text(version,sheets,per_sheet,limit,resolver,"SELECT 'retained';  \n");
}
typedef struct { WorkbookCodecColumn columns[14]; WorkbookCodecAtom cells[2049*14]; } TableSourceFixture;
typedef struct { WorkbookCodecColumn columns[6]; WorkbookCodecAtom cells[30721*6]; } TableFilterFixture;
typedef struct { WorkbookCodecColumn columns[5]; WorkbookCodecAtom cells[14337*5]; } TableSortFixture;
static TableSourceFixture *table_source_fixture(size_t count,size_t per_sheet) {
    TableSourceFixture *f=dx_alloc(sizeof(*f)); const char *names[]={"sheet_id","object_id","kind","path","source_schema","source_table","filter_column","filter_value","filter_op","group_column","measure_column","aggregate","sort_column","descending"};
    const WorkbookCodecAtomKind kinds[]={WORKBOOK_CODEC_I32,WORKBOOK_CODEC_U64,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_BOOLEAN};
    for (size_t col=0;col<14;col++) f->columns[col]=(WorkbookCodecColumn){names[col],kinds[col]};
    for (size_t row=0;row<count;row++) { WorkbookCodecAtom *c=f->cells+row*14; c[0]=i((int32_t)(row/per_sheet)); c[1]=u(table_test_id(row%per_sheet)); c[2]=i(SOURCE_DEMO); c[3]=t(""); c[4]=t(""); c[5]=t("NULL"); c[6]=t("Primary"); c[7]=t("NULL"); c[8]=i(FILTER_EQUAL); c[11]=i(AGG_SUM); c[12]=t("Sort"); c[13]=b(false); }
    return f;
}
static TableFilterFixture *table_filter_fixture(size_t count,size_t per_sheet) {
    TableFilterFixture *f=dx_alloc(sizeof(*f)); const char *names[]={"sheet_id","object_id","position","name","op","value"}; const WorkbookCodecAtomKind kinds[]={WORKBOOK_CODEC_I32,WORKBOOK_CODEC_U64,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_TEXT};
    static const char *fields[]={"F0","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","F13","F14"};
    for (size_t col=0;col<6;col++) f->columns[col]=(WorkbookCodecColumn){names[col],kinds[col]};
    for (size_t row=0;row<count;row++) { size_t owner=row/15; WorkbookCodecAtom *c=f->cells+row*6; c[0]=i((int32_t)(owner/per_sheet)); c[1]=u(table_test_id(owner%per_sheet)); c[2]=i((int32_t)(row%15)); c[3]=t(fields[row%15]); c[4]=i(FILTER_EQUAL); c[5]=row%3==0 ? (WorkbookCodecAtom){0} : t(row%3==1 ? "" : "NULL"); }
    return f;
}
static TableSortFixture *table_sort_fixture(size_t count,size_t per_sheet) {
    TableSortFixture *f=dx_alloc(sizeof(*f)); const char *names[]={"sheet_id","object_id","position","name","descending"}; const WorkbookCodecAtomKind kinds[]={WORKBOOK_CODEC_I32,WORKBOOK_CODEC_U64,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_BOOLEAN}; static const char *fields[]={"S0","S1","S2","S3","S4","S5","S6"};
    for (size_t col=0;col<5;col++) f->columns[col]=(WorkbookCodecColumn){names[col],kinds[col]};
    for (size_t row=0;row<count;row++) { size_t owner=row/7; WorkbookCodecAtom *c=f->cells+row*5; c[0]=i((int32_t)(owner/per_sheet)); c[1]=u(table_test_id(owner%per_sheet)); c[2]=i((int32_t)(row%7)); c[3]=t(fields[row%7]); c[4]=b(row%2!=0); } return f;
}
static WorkbookCodecBatch table_base_batch(TableSourceFixture *f,size_t first,size_t count,bool end) { return (WorkbookCodecBatch){f->columns,14,f->cells+first*14,count,first,end}; }
static WorkbookCodecBatch table_filter_batch(TableFilterFixture *f,size_t first,size_t count,bool end) { return (WorkbookCodecBatch){f->columns,6,f->cells+first*6,count,first,end}; }
static WorkbookCodecBatch table_sort_batch(TableSortFixture *f,size_t first,size_t count,bool end) { return (WorkbookCodecBatch){f->columns,5,f->cells+first*5,count,first,end}; }
static void reject_tables_arm(WorkbookCodecRead *read,WorkbookCodecTableSourcePresence present) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(read); WorkbookCodecRequest before=request ? *request : (WorkbookCodecRequest){0}; size_t bytes=workbook_codec_read_owned_bytes(read); char *error=NULL;
    CHECK(!workbook_codec_read_add_table_sources(read,present,&error) && error && *error && workbook_codec_read_owned_bytes(read)==bytes); CHECK(request ? same_request(before,workbook_codec_read_request(read)) : !workbook_codec_read_request(read)); error_clear(&error);
}
static void table_get_unchanged(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner) {
    WorkbookCodecTableSourceView out={(const Analysis *)(uintptr_t)1,"unchanged",true},saved=out;
    CHECK(!workbook_codec_header_table_source(header,sheet,owner,&out) && !memcmp(&out,&saved,sizeof(out)));
}
static void table_empty_children(WorkbookCodecRead *read,size_t retained) {
    char *error=NULL; TableFilterFixture *f=table_filter_fixture(0,1); WorkbookCodecBatch batch=table_filter_batch(f,0,0,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_FILTERS,&batch,sizeof(*f)+retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(f);
    TableSortFixture *s=table_sort_fixture(0,1); batch=table_sort_batch(s,0,0,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SORTS,&batch,sizeof(*s)+retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(s);
}
static void test_tables_contract(void) {
    CHECK(WORKBOOK_CODEC_TABLE_SOURCES==9 && WORKBOOK_CODEC_TABLE_FILTERS==10 && WORKBOOK_CODEC_TABLE_SORTS==11 && !workbook_codec_header_table_sources_ready(NULL)); table_get_unchanged(NULL,0,1); reject_tables_arm(NULL,(WorkbookCodecTableSourcePresence){true,true,true});
    WorkbookCodecRead *read=sort_header(14,1,NULL,LIMIT); reject_tables_arm(read,(WorkbookCodecTableSourcePresence){true,true,true}); workbook_codec_read_free(read); char *error=NULL;
    for (unsigned version=12;version<=14;version++) {
        read=table_header(version,1,1,LIMIT,NULL); reject_tables_arm(read,(WorkbookCodecTableSourcePresence){false,true,true}); reject_tables_arm(read,(WorkbookCodecTableSourcePresence){true,false,true}); reject_tables_arm(read,(WorkbookCodecTableSourcePresence){true,true,false});
        CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error)); reject_tables_arm(read,(WorkbookCodecTableSourcePresence){true,true,true});
        const WorkbookCodecRequest *r=workbook_codec_read_request(read); CHECK(r && r->family==WORKBOOK_CODEC_TABLE_SOURCES && r->row_limit==2049 && r->column_count==14 && r->source_column_count==14);
        CHECK(!strcmp(r->sql,"SELECT sheet_id,object_id,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending FROM _duxcel_workspace.table_sources ORDER BY sheet_id,object_id LIMIT 2049"));
        TableSourceFixture *f=table_source_fixture(1,1); WorkbookCodecBatch batch=table_base_batch(f,0,0,true); reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)); batch=table_base_batch(f,0,1,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(f); r=workbook_codec_read_request(read);
        CHECK(r && r->family==WORKBOOK_CODEC_TABLE_FILTERS && r->row_limit==30721 && r->column_count==6 && !strcmp(r->sql,"SELECT sheet_id,object_id,position,name,op,value FROM _duxcel_workspace.table_filters ORDER BY sheet_id,object_id,position LIMIT 30721"));
        WorkbookCodecHeader *header=NULL; CHECK(!workbook_codec_read_finish(read,&header,&error) && !header); error_clear(&error); table_empty_children(read,0);
        CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_table_sources_ready(header)); reject_tables_arm(read,(WorkbookCodecTableSourcePresence){true,true,true});
        WorkbookCodecTableSourceView view={0}; CHECK(workbook_codec_header_table_source(header,0,1,&view) && view.analysis && view.locator_available && view.analysis->kind==SOURCE_DEMO && !strcmp(view.reference,"") && !strcmp(view.analysis->path,"") && view.analysis->unique_rows && !strcmp(view.analysis->query,"SELECT 'retained';  \n"));
        table_get_unchanged(header,0,0); table_get_unchanged(header,1,1); table_get_unchanged(header,0,UINT64_MAX); CHECK(!workbook_codec_header_table_source(header,0,1,NULL)); workbook_codec_read_free(read); workbook_codec_header_free(header);
        read=table_header(version,1,0,LIMIT,NULL);
        if (version==14) { reject_tables_arm(read,(WorkbookCodecTableSourcePresence){false,true,true}); CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error)); f=table_source_fixture(0,1); batch=table_base_batch(f,0,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(f); table_empty_children(read,0); }
        else CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){false,true,false},&error));
        header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && workbook_codec_header_table_sources_ready(header)); table_get_unchanged(header,0,1); workbook_codec_read_free(read); workbook_codec_header_free(header);
    }
    read=table_header(14,1,1,LIMIT,NULL); WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && !workbook_codec_header_table_sources_ready(header)); table_get_unchanged(header,0,1); workbook_codec_read_free(read); workbook_codec_header_free(header);
}
static void test_tables_owned_and_invalid(void) {
    char *error=NULL; TableResolver resolver={0}; WorkbookCodecRead *read=table_header(14,1,2,TABLE_TEST_LIMIT,&resolver); CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error));
    TableSourceFixture *f=table_source_fixture(2,2); WorkbookCodecBatch batch=table_base_batch(f,0,1,false); size_t owned=workbook_codec_read_owned_bytes(read); error=dx_strdup("stale untouched"); char *old_error=error;
    for (unsigned part=0;part<3;part++) { WorkbookCodecKey stale=key; if (!part) stale.document_epoch--; else if (part==1) stale.operation_ticket--; else stale.snapshot_generation--; CHECK(workbook_codec_read_feed(read,stale,(WorkbookCodecFamily)99,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE && error==old_error && workbook_codec_read_owned_bytes(read)==owned); }
    free(error); error=NULL;
    const size_t required[]={0,1,2,3,8,11,13};
    for (size_t i=0;i<sizeof(required)/sizeof(*required);i++) { size_t col=required[i]; WorkbookCodecAtom saved=f->cells[col]; f->cells[col]=(WorkbookCodecAtom){0}; reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)); f->cells[col]=saved; }
    for (size_t col=0;col<14;col++) { WorkbookCodecAtomKind kind=f->columns[col].kind; f->columns[col].kind=kind==WORKBOOK_CODEC_BOOLEAN ? WORKBOOK_CODEC_I32 : WORKBOOK_CODEC_BOOLEAN; reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)); f->columns[col].kind=kind; }
    const uint64_t owners[]={0,2,UINT64_MAX}; for (size_t i=0;i<3;i++) { f->cells[1]=u(owners[i]); reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)); } f->cells[1]=u(1);
    f->cells[2]=i(SOURCE_NONE); reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)); f->cells[2]=i(SOURCE_CSV); reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f));
    char reference[]="Table 'İ'.csv"; f->cells[3]=t(reference); f->cells[7]=t("");
    for (int mode=2;mode<=4;mode++) { resolver.mode=mode; reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+sizeof(reference)); } resolver.mode=1;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+sizeof(reference),&error)==WORKBOOK_CODEC_READ_ACCEPTED); memset(reference,'x',strlen(reference));
    f->cells[14+2]=i(SOURCE_PARQUET); f->cells[14+3]=t("second.parquet"); f->cells[14+7]=(WorkbookCodecAtom){0}; resolver.mode=0;
    batch=table_base_batch(f,1,1,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(f);
    TableFilterFixture *filters=table_filter_fixture(2,2); filters->cells[6]=i(0); filters->cells[7]=u(2); filters->cells[8]=i(0); filters->cells[9]=t("Other"); filters->cells[11]=t("NULL"); batch=table_filter_batch(filters,0,2,true);
    filters->cells[2]=i(1); reject(read,WORKBOOK_CODEC_TABLE_FILTERS,&batch,sizeof(*filters)); filters->cells[2]=i(0); filters->cells[3]=t("pRIMARY"); reject(read,WORKBOOK_CODEC_TABLE_FILTERS,&batch,sizeof(*filters)); filters->cells[3]=t("F0");
    filters->cells[7]=u(0); reject(read,WORKBOOK_CODEC_TABLE_FILTERS,&batch,sizeof(*filters)); filters->cells[7]=u(2);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_FILTERS,&batch,sizeof(*filters),&error)==WORKBOOK_CODEC_READ_ACCEPTED); memset(filters->cells,0,sizeof(filters->cells)); free(filters);
    const WorkbookCodecRequest *r=workbook_codec_read_request(read); CHECK(r && r->family==WORKBOOK_CODEC_TABLE_SORTS && r->row_limit==14337 && r->column_count==5 && !strcmp(r->sql,"SELECT sheet_id,object_id,position,name,descending FROM _duxcel_workspace.table_sorts ORDER BY sheet_id,object_id,position LIMIT 14337"));
    TableSortFixture *sorts=table_sort_fixture(2,2); sorts->cells[5]=i(0); sorts->cells[6]=u(2); sorts->cells[7]=i(0); sorts->cells[8]=t("Other sort"); batch=table_sort_batch(sorts,0,2,false);
    sorts->cells[3]=t("sOrT"); reject(read,WORKBOOK_CODEC_TABLE_SORTS,&batch,sizeof(*sorts)); sorts->cells[3]=t("S0");
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SORTS,&batch,sizeof(*sorts),&error)==WORKBOOK_CODEC_READ_ACCEPTED); size_t before_move=workbook_codec_read_owned_bytes(read);
    batch=table_sort_batch(sorts,2,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SORTS,&batch,sizeof(*sorts),&error)==WORKBOOK_CODEC_READ_ACCEPTED && workbook_codec_read_owned_bytes(read)==before_move); free(sorts);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    WorkbookCodecTableSourceView first={0},second={0}; CHECK(workbook_codec_header_table_source(header,0,1,&first) && workbook_codec_header_table_source(header,0,2,&second));
    CHECK(!strcmp(first.reference,"Table 'İ'.csv") && first.analysis->path && !*first.analysis->path && !first.locator_available && first.analysis->kind==SOURCE_CSV);
    CHECK(!strcmp(second.reference,"second.parquet") && !strcmp(second.analysis->path,"logical:second.parquet") && second.locator_available && second.analysis->kind==SOURCE_PARQUET);
    CHECK(first.analysis->filter_value && !*first.analysis->filter_value && !second.analysis->filter_value && !first.analysis->filters[0].value && !strcmp(second.analysis->filters[0].value,"NULL"));
    CHECK(first.analysis->filter_count==1 && second.analysis->filter_count==1 && first.analysis->sort_count==1 && second.analysis->sort_count==1 && !strcmp(first.analysis->schema,"") && !strcmp(first.analysis->table,"NULL"));
    const char *query=NULL; bool unique=false; CHECK(workbook_codec_header_query(header,0,1,&query) && query==first.analysis->query && !strcmp(query,"SELECT 'retained';  \n") && workbook_codec_header_unique_rows(header,0,1,&unique) && unique && first.analysis->unique_rows && !first.analysis->source_version); workbook_codec_header_free(header);
}
static void test_tables_full_limits(void) {
    char *error=NULL; WorkbookCodecRead *read=table_header(14,64,32,TABLE_TEST_LIMIT,NULL); CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error));
    TableSourceFixture *base=table_source_fixture(2049,32); WorkbookCodecBatch batch=table_base_batch(base,0,257,false); reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*base));
    for (size_t first=0;first<2048;first+=256) { batch=table_base_batch(base,first,256,false); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*base),&error)==WORKBOOK_CODEC_READ_ACCEPTED); }
    batch=table_base_batch(base,2048,1,true); reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*base)); batch=table_base_batch(base,2048,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*base),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(base);
    TableFilterFixture *filters=table_filter_fixture(30721,32);
    for (size_t first=0;first<30720;first+=256) { batch=table_filter_batch(filters,first,256,false); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_FILTERS,&batch,sizeof(*filters),&error)==WORKBOOK_CODEC_READ_ACCEPTED); }
    batch=table_filter_batch(filters,30720,1,true); reject(read,WORKBOOK_CODEC_TABLE_FILTERS,&batch,sizeof(*filters)); batch=table_filter_batch(filters,30720,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_FILTERS,&batch,sizeof(*filters),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(filters);
    TableSortFixture *sorts=table_sort_fixture(14337,32);
    for (size_t first=0;first<14336;first+=256) { batch=table_sort_batch(sorts,first,256,false); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SORTS,&batch,sizeof(*sorts),&error)==WORKBOOK_CODEC_READ_ACCEPTED); }
    batch=table_sort_batch(sorts,14336,1,true); reject(read,WORKBOOK_CODEC_TABLE_SORTS,&batch,sizeof(*sorts)); batch=table_sort_batch(sorts,14336,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SORTS,&batch,sizeof(*sorts),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(sorts);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    for (size_t sheet=0;sheet<64;sheet++) for (size_t position=0;position<32;position++) { WorkbookCodecTableSourceView view={0}; CHECK(workbook_codec_header_table_source(header,sheet,table_test_id(position),&view) && view.analysis->filter_count==15 && view.analysis->sort_count==7 && view.analysis->unique_rows && !strcmp(view.analysis->query,"SELECT 'retained';  \n")); }
    workbook_codec_header_free(header);
}
static void test_tables_text_and_scratch_budget(void) {
    char *error=NULL; TableSourceFixture *f=table_source_fixture(1,1); char *large=dx_alloc(4u*1024u*1024u+2u); memset(large,'q',4u*1024u*1024u+1u); large[4u*1024u*1024u+1u]=0;
    WorkbookCodecRead *read=table_header(14,1,1,TABLE_TEST_LIMIT,NULL); CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error)); WorkbookCodecBatch batch=table_base_batch(f,0,1,true);
    f->cells[7]=t(large); reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+4u*1024u*1024u+2u); f->cells[7].value.text.bytes=4u*1024u*1024u; f->cells[8]=i(FILTER_CONTAINS);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+4u*1024u*1024u+2u,&error)==WORKBOOK_CODEC_READ_ACCEPTED); table_empty_children(read,sizeof(*f)+4u*1024u*1024u+2u); WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    WorkbookCodecTableSourceView old={0}; CHECK(workbook_codec_header_table_source(header,0,1,&old) && strlen(old.analysis->filter_value)==4u*1024u*1024u); WorkbookCodecHeader *identity=header; size_t old_bytes=workbook_codec_header_owned_bytes(header); const char *old_query=old.analysis->query;
    read=table_header(14,1,1,TABLE_TEST_LIMIT,NULL); CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error));
    char payload[]="[\"6\",\"[\\\"a\\\"]\",\"1\",\"x\"]"; f->cells[7]=t(payload); f->cells[8]=i(FILTER_COMPOSITE); size_t base=workbook_codec_read_owned_bytes(read);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+sizeof(payload)+old_bytes+4u*1024u*1024u+2u,&error)==WORKBOOK_CODEC_READ_ACCEPTED); size_t copied=workbook_codec_read_owned_bytes(read)-base; workbook_codec_read_free(read);
    read=table_header(14,1,1,TABLE_TEST_LIMIT,NULL); CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error)); size_t owned=workbook_codec_read_owned_bytes(read),n=strlen(payload),scratch=3u*n+4102u+6160u*sizeof(char *),retained=sizeof(*f)+sizeof(payload)+old_bytes+4u*1024u*1024u+2u,staged=copied+2u*sizeof(void *);
    CHECK(owned+retained+staged+scratch<TABLE_TEST_LIMIT); size_t pressure_bytes=TABLE_TEST_LIMIT-owned-retained-staged-scratch; unsigned char *pressure=dx_alloc(pressure_bytes+1u);
    payload[0]='x'; CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,retained+pressure_bytes+1u,&error)==WORKBOOK_CODEC_READ_ERROR && error && strstr(error,"scratch")); error_clear(&error);
    CHECK(workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)->first_row);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ERROR && error && strstr(error,"filter")); error_clear(&error);
    CHECK(!workbook_codec_read_finish(read,&header,&error) && header==identity && workbook_codec_header_owned_bytes(header)==old_bytes && old.analysis->query==old_query && !strcmp(old_query,"SELECT 'retained';  \n")); error_clear(&error);
    payload[0]='['; CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(pressure); table_empty_children(read,retained); CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    WorkbookCodecTableSourceView view={0}; CHECK(workbook_codec_header_table_source(header,0,1,&view) && !strcmp(view.analysis->filter_value,payload)); workbook_codec_header_free(header); free(large); free(f);
}
static void test_tables_field_and_native_budgets(void) {
    char *error=NULL; const size_t value_bytes=4u*1024u*1024u; char *text=dx_alloc(value_bytes+1u); memset(text,'x',value_bytes); text[value_bytes]=0;
    TableSourceFixture *f=table_source_fixture(2,2); WorkbookCodecRead *read=table_header(14,1,2,TABLE_TEST_LIMIT,NULL); CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error));
    WorkbookCodecBatch batch=table_base_batch(f,0,2,true); f->cells[7]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={text,value_bytes}}; f->cells[14+7]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={text,value_bytes-29u}};
    /* Raw text counts no terminators:15 other bytes per base row. One byte
       over8MiB rejects the entire batch, then the exact raw-boundary retry is
       accepted privately. It still cannot become a complete native object. */
    size_t owned=workbook_codec_read_owned_bytes(read); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+value_bytes+1u,&error)==WORKBOOK_CODEC_READ_ERROR && error && strstr(error,"raw Table")); error_clear(&error);
    CHECK(workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)->first_row); f->cells[14+7].value.text.bytes=value_bytes-30u;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+value_bytes+1u,&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    TableFilterFixture *filters=table_filter_fixture(1,2); WorkbookCodecBatch child=table_filter_batch(filters,0,1,true); size_t prefix=workbook_codec_read_owned_bytes(read);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_FILTERS,&child,sizeof(*f)+sizeof(*filters)+value_bytes+1u,&error)==WORKBOOK_CODEC_READ_ERROR && error && strstr(error,"raw Table")); error_clear(&error);
    CHECK(workbook_codec_read_owned_bytes(read)==prefix && !workbook_codec_read_request(read)->first_row); child=table_filter_batch(filters,0,0,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_FILTERS,&child,sizeof(*f)+sizeof(*filters)+value_bytes+1u,&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(filters);
    TableSortFixture *sorts=table_sort_fixture(0,2); child=table_sort_batch(sorts,0,0,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SORTS,&child,sizeof(*f)+sizeof(*sorts)+value_bytes+1u,&error)==WORKBOOK_CODEC_READ_ERROR && error && strstr(error,"native 8 MiB current")); error_clear(&error); free(sorts); workbook_codec_read_free(read);
    /* Path64KiB/schema4096 retain exact bytes; one-over, NUL and bad UTF-8
       reject before staging. DEMO has no external locator normalization. */
    read=table_header(14,1,1,TABLE_TEST_LIMIT,NULL); CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error));
    f->cells[7]=t("NULL"); f->cells[3]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={text,65537}}; batch=table_base_batch(f,0,1,true); reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+value_bytes+1u);
    f->cells[3].value.text.bytes=65536; f->cells[4]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={text,4097}}; reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+value_bytes+1u); f->cells[4].value.text.bytes=4096;
    text[0]=0; reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+value_bytes+1u); text[0]=(char)0x80; reject(read,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+value_bytes+1u); text[0]='x';
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f)+value_bytes+1u,&error)==WORKBOOK_CODEC_READ_ACCEPTED); table_empty_children(read,sizeof(*f)+value_bytes+1u);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read); WorkbookCodecTableSourceView prior={0}; CHECK(workbook_codec_header_table_source(header,0,1,&prior) && strlen(prior.reference)==65536 && strlen(prior.analysis->path)==65536 && strlen(prior.analysis->schema)==4096);
    WorkbookCodecHeader *identity=header; size_t old_bytes=workbook_codec_header_owned_bytes(header); const char *old_query=prior.analysis->query;
    /* Queries are absent from the rawTable8MiB sum but are part of native
       ObjectSnapshot bytes. Sufficient Header admission must not hide this. */
    char *query=dx_alloc(65537); memset(query,'q',65536); query[65536]=0;
    read=table_header_text(14,1,2,TABLE_TEST_LIMIT,NULL,query); CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error));
    f->cells[3]=t(""); f->cells[4]=t(""); f->cells[7]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={text,value_bytes}}; f->cells[14+7]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={text,value_bytes-65536u}}; batch=table_base_batch(f,0,2,true);
    size_t retained=sizeof(*f)+value_bytes+1u+65537u+old_bytes; CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    filters=table_filter_fixture(0,2); child=table_filter_batch(filters,0,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_FILTERS,&child,retained+sizeof(*filters),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(filters);
    sorts=table_sort_fixture(1,2); child=table_sort_batch(sorts,0,1,true); owned=workbook_codec_read_owned_bytes(read);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SORTS,&child,retained+sizeof(*sorts),&error)==WORKBOOK_CODEC_READ_ERROR && error && strstr(error,"native 8 MiB current")); error_clear(&error);
    CHECK(workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)->first_row);
    CHECK(!workbook_codec_read_finish(read,&header,&error) && header==identity && prior.analysis->query==old_query && workbook_codec_header_owned_bytes(header)==old_bytes); error_clear(&error);
    CHECK(!strcmp(old_query,"SELECT 'retained';  \n") && query[0]=='q' && query[65535]=='q' && text[0]=='x'); free(sorts); workbook_codec_read_free(read);
    read=table_header(14,1,1,TABLE_TEST_LIMIT,NULL); CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error)); f->cells[7]=t("NULL"); batch=table_base_batch(f,0,1,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED); table_empty_children(read,retained); CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read); CHECK(workbook_codec_header_table_source(header,0,1,&prior) && !strcmp(prior.analysis->filter_value,"NULL"));
    workbook_codec_header_free(header); free(query); free(text); free(f);
}
#undef TABLE_TEST_LIMIT
#define PRESENTATION_LIMIT (128u*1024u*1024u)
typedef struct { WorkbookCodecColumn columns[9]; WorkbookCodecAtom *cells; size_t rows,columns_count,bytes; } DefinitionFixture;
static DefinitionFixture definition_fixture(size_t rows,bool marks) {
    DefinitionFixture f={.rows=rows,.columns_count=marks?8:9}; f.bytes=rows*f.columns_count*sizeof(*f.cells); f.cells=dx_alloc(f.bytes);
    const WorkbookCodecTable *table=workbook_codec_table_find(marks?"marks":"pivot_fields"); const char *p=table->columns,*types=table->types;
    static const char *const pivot_names[]={"sheet_id","object_id","role","position","name","aggregate","filter_op","value","grain"},*const mark_names[]={"sheet_id","object_id","role","position","name","is_measure","aggregate","grain"};
    static const char *const names[4][8]={{"Row0","Row1","Row2","Row3","Row4","Row5","Row6","Row7"},{"Col0","Col1","Col2","Col3","Col4","Col5","Col6","Col7"},{"Value0","Value1","Value2","Value3","Value4","Value5","Value6","Value7"},{"Filter0","Filter1","Filter2","Filter3","Filter4","Filter5","Filter6","Filter7"}};
    for(size_t c=0;c<f.columns_count;c++){while(*types==' ')types++;WorkbookCodecAtomKind type=*types=='I'?WORKBOOK_CODEC_I32:*types=='U'?WORKBOOK_CODEC_U64:*types=='B'?WORKBOOK_CODEC_BOOLEAN:WORKBOOK_CODEC_TEXT;f.columns[c]=(WorkbookCodecColumn){marks?mark_names[c]:pivot_names[c],type};types++;p+=strcspn(p,",");if(*p)p++;}
    for(size_t row=0;row<rows;row++) {
        size_t fields=marks?34:32,owner=row/fields,within=row%fields,role=marks?(within<8?0:within==8?1:within<17?2:within<25?3:within<33?4:5):within/8;
        size_t position=marks?(role==0?within:role==1||role==5?0:within-(role==2?9:role==3?17:25)):within%8;
        WorkbookCodecAtom *c=f.cells+row*f.columns_count;c[0]=i((int32_t)(owner/32));c[1]=u(owner%32+1);c[2]=i((int32_t)role);c[3]=i((int32_t)position);c[4]=t(names[marks?0:role][position]);
        if(marks){c[5]=b(position%2!=0);c[6]=i(AGG_AVG);c[7]=i(position%2?PIVOT_DATE_EXACT:PIVOT_DATE_MONTH);}
        else{c[5]=i(AGG_SUM);c[6]=i(FILTER_CONTAINS);c[7]=row%3==0?(WorkbookCodecAtom){0}:t(row%3==1?"":"NULL");c[8]=i(role<PIVOT_VALUES?PIVOT_DATE_YEAR:PIVOT_DATE_EXACT);}
    }
    return f;
}
static WorkbookCodecBatch definition_batch(DefinitionFixture *f,size_t first,size_t rows,bool end) { return (WorkbookCodecBatch){f->columns,f->columns_count,f->cells+first*f->columns_count,rows,first,end}; }
static size_t definition_input(const DefinitionFixture *f) { return sizeof(*f)+f->bytes; }
static void definition_free(DefinitionFixture *f) { free(f->cells);*f=(DefinitionFixture){0}; }
static WorkbookCodecRead *definition_header(unsigned version,size_t sheets,size_t per_sheet,bool table,unsigned measure,size_t limit,const char *query) {
    WorkbookCodecRead *read=sort_header(version,sheets,NULL,limit);char *error=NULL;size_t total=sheets*per_sheet;ObjectFixture *objects=object_fixture(total);
    for(size_t row=0;row<total;row++){WorkbookCodecAtom *c=objects->cells+row*11;c[0]=i((int32_t)(row/per_sheet));c[1]=u(row%per_sheet+1);c[3]=u((row%per_sheet)*2);c[7]=i(table&&row%per_sheet==per_sheet-1?OBJECT_TABLE:row%2?OBJECT_CHART:OBJECT_PIVOT);c[10]=i(c[7].value.i32==OBJECT_TABLE?0:(int32_t)measure);}
    CHECK(workbook_codec_read_add_objects(read,true,&error));for(size_t first=0;first<total||(!total&&!first);){size_t n=total-first>256?256:total-first;WorkbookCodecBatch batch=object_batch(objects,first,n,first+n==total);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*objects),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=n;if(!n)break;}free(objects);
    QueryFixture *q=query_fixture(table?sheets:0);for(size_t row=0;row<(table?sheets:0);row++){q->cells[row*3]=i((int32_t)row);q->cells[row*3+1]=u(per_sheet);q->cells[row*3+2]=t(query);}
    WorkbookCodecBatch batch=query_batch(q,0,table?sheets:0,true);CHECK(workbook_codec_read_add_queries(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*q)+strlen(query)+1,&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    q->columns[2]=(WorkbookCodecColumn){"unique_rows",WORKBOOK_CODEC_BOOLEAN};for(size_t row=0;row<(table?sheets:0);row++)q->cells[row*3+2]=b(true);CHECK(workbook_codec_read_add_source_options(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*q)+strlen(query)+1,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(q);
    CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error));TableSourceFixture *base=table_source_fixture(table?sheets:0,1);for(size_t row=0;row<(table?sheets:0);row++)base->cells[row*14+1]=u(per_sheet);batch=table_base_batch(base,0,table?sheets:0,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*base)+strlen(query)+1,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(base);table_empty_children(read,strlen(query)+1);return read;
}
static void definition_get_unchanged(const WorkbookCodecHeader *h,size_t sheet,uint64_t owner) {
    const PivotDefinition *p=(const PivotDefinition *)(uintptr_t)1;const MarkDefinition *m=(const MarkDefinition *)(uintptr_t)2;
    CHECK(!workbook_codec_header_pivot_definition(h,sheet,owner,&p)&&p==(const PivotDefinition *)(uintptr_t)1);CHECK(!workbook_codec_header_marks_definition(h,sheet,owner,&m)&&m==(const MarkDefinition *)(uintptr_t)2);
}
static void reject_definitions_arm(WorkbookCodecRead *read,WorkbookCodecPivotMarksPresence present) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(read);WorkbookCodecRequest old=request?*request:(WorkbookCodecRequest){0};size_t bytes=workbook_codec_read_owned_bytes(read);char *error=NULL;
    CHECK(!workbook_codec_read_add_pivot_marks(read,present,&error)&&error&&*error&&workbook_codec_read_owned_bytes(read)==bytes);CHECK(request?same_request(old,workbook_codec_read_request(read)):!workbook_codec_read_request(read));error_clear(&error);
}
static void definition_empty(WorkbookCodecRead *read,bool marks,size_t retained) {
    DefinitionFixture f=definition_fixture(0,marks);WorkbookCodecBatch batch=definition_batch(&f,0,0,true);char *error=NULL;CHECK(workbook_codec_read_feed(read,key,marks?WORKBOOK_CODEC_MARKS:WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f)+retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);definition_free(&f);
}
static void test_definitions_contract(void) {
    CHECK(WORKBOOK_CODEC_PIVOT_FIELDS==12&&WORKBOOK_CODEC_MARKS==13&&!workbook_codec_header_pivot_marks_ready(NULL));definition_get_unchanged(NULL,0,1);reject_definitions_arm(NULL,(WorkbookCodecPivotMarksPresence){true,true});
    for(unsigned version=12;version<=14;version++){
        WorkbookCodecRead *read=sort_header(version,1,NULL,LIMIT);reject_definitions_arm(read,(WorkbookCodecPivotMarksPresence){true,true});workbook_codec_read_free(read);read=definition_header(version,1,1,false,0,LIMIT,"");char *error=NULL;
        reject_definitions_arm(read,(WorkbookCodecPivotMarksPresence){false,true});reject_definitions_arm(read,(WorkbookCodecPivotMarksPresence){true,false});CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error));reject_definitions_arm(read,(WorkbookCodecPivotMarksPresence){true,true});
        const WorkbookCodecRequest *request=workbook_codec_read_request(read);CHECK(request&&request->family==WORKBOOK_CODEC_PIVOT_FIELDS&&request->row_limit==65537&&request->column_count==9&&!strcmp(request->sql,"SELECT sheet_id,object_id,role,position,name,aggregate,filter_op,value,grain FROM _duxcel_workspace.pivot_fields ORDER BY sheet_id,object_id,role,position LIMIT 65537"));
        WorkbookCodecHeader *header=NULL;CHECK(!workbook_codec_read_finish(read,&header,&error)&&!header);error_clear(&error);definition_empty(read,false,0);request=workbook_codec_read_request(read);CHECK(request&&request->family==WORKBOOK_CODEC_MARKS&&request->row_limit==98305&&request->column_count==8&&!strcmp(request->sql,"SELECT sheet_id,object_id,role,position,name,is_measure,aggregate,grain FROM _duxcel_workspace.marks ORDER BY sheet_id,object_id,role,position LIMIT 98305"));definition_empty(read,true,0);
        CHECK(workbook_codec_read_finish(read,&header,&error)&&workbook_codec_header_pivot_marks_ready(header));const PivotDefinition *p=NULL;const MarkDefinition *m=NULL;CHECK(workbook_codec_header_pivot_definition(header,0,1,&p)&&p&&!p->counts[0]&&!p->counts[2]);CHECK(workbook_codec_header_marks_definition(header,0,1,&m)&&m&&!m->counts[0]);definition_get_unchanged(header,1,1);definition_get_unchanged(header,0,0);CHECK(!workbook_codec_header_pivot_definition(header,0,1,NULL)&&!workbook_codec_header_marks_definition(header,0,1,NULL));workbook_codec_read_free(read);workbook_codec_header_free(header);
    }
    WorkbookCodecRead *read=sort_header(12,1,NULL,LIMIT);char *error=NULL;CHECK(workbook_codec_read_add_objects(read,false,&error));QueryFixture *q=query_fixture(0);WorkbookCodecBatch batch=query_batch(q,0,0,true);CHECK(workbook_codec_read_add_queries(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*q),&error)==WORKBOOK_CODEC_READ_ACCEPTED);q->columns[2]=(WorkbookCodecColumn){"unique_rows",WORKBOOK_CODEC_BOOLEAN};CHECK(workbook_codec_read_add_source_options(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*q),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(q);CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){0},&error));CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){0},&error));WorkbookCodecHeader *header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error)&&workbook_codec_header_pivot_marks_ready(header));definition_get_unchanged(header,0,1);workbook_codec_read_free(read);workbook_codec_header_free(header);
}
static void test_definitions_atomic_and_validators(void) {
    WorkbookCodecRead *read=definition_header(14,1,3,true,0,PRESENTATION_LIMIT,"SELECT retained;  \n");char *error=NULL;CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error));DefinitionFixture f=definition_fixture(3,false);WorkbookCodecBatch batch=definition_batch(&f,0,3,false);size_t owned=workbook_codec_read_owned_bytes(read);
    error=dx_strdup("stale unchanged");char *old_error=error;WorkbookCodecKey stale=key;stale.snapshot_generation--;CHECK(workbook_codec_read_feed(read,stale,(WorkbookCodecFamily)99,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE&&error==old_error&&workbook_codec_read_owned_bytes(read)==owned);free(error);error=NULL;
    for(size_t c=0;c<9;c++){WorkbookCodecAtomKind old=f.columns[c].kind;f.columns[c].kind=old==WORKBOOK_CODEC_TEXT?WORKBOOK_CODEC_BOOLEAN:WORKBOOK_CODEC_TEXT;reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));f.columns[c].kind=old;if(c!=7){WorkbookCodecAtom saved=f.cells[c];f.cells[c]=(WorkbookCodecAtom){0};reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));f.cells[c]=saved;}}
    f.cells[1]=u(3);reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));f.cells[1]=u(0);reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));f.cells[1]=u(1);f.cells[4]=t("");reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));f.cells[4]=t("NULL");f.cells[3]=i(1);reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));f.cells[3]=i(0);
    f.cells[9+4]=t("NULL");reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));f.cells[9+4]=t("null"); /* Native strcmp duplicate rule is case-sensitive. */
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);memset(f.cells,0,f.bytes);definition_free(&f);
    f=definition_fixture(1,false);f.cells[2]=i(PIVOT_COLUMNS);f.cells[4]=t("NULL");/* The following row resumes the accepted cursor3. */
    batch=(WorkbookCodecBatch){f.columns,9,f.cells,1,3,false};reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));f.cells[8]=i(PIVOT_DATE_MONTH);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);definition_free(&f);
    f=definition_fixture(1,false);f.cells[2]=i(PIVOT_VALUES);f.cells[4]=t("Amount");batch=(WorkbookCodecBatch){f.columns,9,f.cells,1,4,true};reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));f.cells[8]=i(PIVOT_DATE_EXACT);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);definition_free(&f);
    f=definition_fixture(2,true);batch=definition_batch(&f,0,2,true);
    for(size_t c=0;c<8;c++){WorkbookCodecAtomKind type=f.columns[c].kind;f.columns[c].kind=type==WORKBOOK_CODEC_TEXT?WORKBOOK_CODEC_BOOLEAN:WORKBOOK_CODEC_TEXT;reject(read,WORKBOOK_CODEC_MARKS,&batch,definition_input(&f));f.columns[c].kind=type;WorkbookCodecAtom saved=f.cells[c];f.cells[c]=(WorkbookCodecAtom){0};reject(read,WORKBOOK_CODEC_MARKS,&batch,definition_input(&f));f.cells[c]=saved;}
    f.cells[1]=u(3);reject(read,WORKBOOK_CODEC_MARKS,&batch,definition_input(&f));f.cells[1]=u(1);
    for(unsigned role=MARKS_SIZE;role<=MARKS_SHAPE;role+=MARKS_SHAPE-MARKS_SIZE){f.cells[2]=f.cells[8+2]=i((int32_t)role);reject(read,WORKBOOK_CODEC_MARKS,&batch,definition_input(&f));}f.cells[2]=f.cells[8+2]=i(MARKS_COLOR);
    f.cells[8+4]=t("Row0");f.cells[8+5]=b(false);f.cells[8+6]=i(AGG_DISTINCT);f.cells[8+7]=i(PIVOT_DATE_MONTH);batch=definition_batch(&f,0,2,true);reject(read,WORKBOOK_CODEC_MARKS,&batch,definition_input(&f));f.cells[8+5]=b(true);reject(read,WORKBOOK_CODEC_MARKS,&batch,definition_input(&f));f.cells[8+7]=i(PIVOT_DATE_EXACT);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_MARKS,&batch,definition_input(&f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);memset(f.cells,0,f.bytes);definition_free(&f);
    WorkbookCodecHeader *header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error));workbook_codec_read_free(read);const PivotDefinition *p=NULL;const MarkDefinition *m=NULL;CHECK(workbook_codec_header_pivot_definition(header,0,1,&p)&&p->counts[0]==3&&p->counts[1]==1&&p->counts[2]==1&&!strcmp(p->fields[0][0].name,"NULL")&&!p->fields[0][0].value&&p->fields[0][1].value&&!*p->fields[0][1].value&&!strcmp(p->fields[0][2].value,"NULL"));CHECK(workbook_codec_header_marks_definition(header,0,1,&m)&&m->counts[0]==2&&!strcmp(m->fields[0][0].name,"Row0")&&m->fields[0][1].measure);definition_get_unchanged(header,0,3);WorkbookCodecTableSourceView table={0};const char *query=NULL;CHECK(workbook_codec_header_table_source(header,0,3,&table)&&workbook_codec_header_query(header,0,3,&query)&&query==table.analysis->query&&!strcmp(query,"SELECT retained;  \n"));workbook_codec_header_free(header);
    /* Nonzero presentation index is checked only against complete VALUES;
       zero is valid with no VALUES, even for a Chart. */
    for(unsigned measure=1;measure<=2;measure++){read=definition_header(14,1,1,false,measure,LIMIT,"");CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error));f=definition_fixture(2,false);for(size_t r=0;r<2;r++){f.cells[r*9+2]=i(PIVOT_VALUES);f.cells[r*9+8]=i(0);}batch=definition_batch(&f,0,2,true);if(measure==2)reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));else{CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);definition_empty(read,true,definition_input(&f));}definition_free(&f);workbook_codec_read_free(read);}
    read=definition_header(14,1,1,false,0,LIMIT,"");CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error));f=definition_fixture(1,false);f.cells[4]=t("");batch=definition_batch(&f,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f),NULL)==WORKBOOK_CODEC_READ_ERROR&&!workbook_codec_read_request(read)->first_row);definition_free(&f);workbook_codec_read_free(read);
}
static void test_definitions_full_and_limits(void) {
    WorkbookCodecRead *read=definition_header(14,64,32,false,0,PRESENTATION_LIMIT,"");char *error=NULL;CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error));DefinitionFixture f=definition_fixture(65537,false);WorkbookCodecBatch batch=definition_batch(&f,0,257,false);reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));
    for(size_t first=0;first<65536;first+=256){batch=definition_batch(&f,first,256,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);}batch=definition_batch(&f,65536,1,true);reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f));batch=definition_batch(&f,65536,0,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);definition_free(&f);
    /* Marks' global98304 envelope is deliberately looser than the valid
      69632 rows: Size/Shape each permit only one rather than eight. */
    f=definition_fixture(2048u*34u,true);for(size_t first=0;first<f.rows;first+=256){size_t count=f.rows-first>256?256:f.rows-first;batch=definition_batch(&f,first,count,first+count==f.rows);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_MARKS,&batch,definition_input(&f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);}definition_free(&f);WorkbookCodecHeader *header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error));workbook_codec_read_free(read);
    for(size_t s=0;s<64;s++)for(size_t j=0;j<32;j++){const PivotDefinition *p=NULL;const MarkDefinition *m=NULL;CHECK(workbook_codec_header_pivot_definition(header,s,j+1,&p)&&p->counts[0]==8&&p->counts[1]==8&&p->counts[2]==8&&p->counts[3]==8);CHECK(workbook_codec_header_marks_definition(header,s,j+1,&m)&&m->counts[0]==8&&m->counts[1]==1&&m->counts[2]==8&&m->counts[3]==8&&m->counts[4]==8&&m->counts[5]==1);}workbook_codec_header_free(header);
}
static void test_definitions_budget_and_query_terms(void) {
    char *error=NULL;WorkbookCodecRead *read=definition_header(14,1,1,false,0,PRESENTATION_LIMIT,"");CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error));DefinitionFixture f=definition_fixture(1,false);char payload[]="[\"6\",\"[\\\"a\\\"]\",\"1\",\"x\"]";f.cells[6]=i(FILTER_COMPOSITE);f.cells[7]=t(payload);WorkbookCodecBatch batch=definition_batch(&f,0,1,true);size_t before=workbook_codec_read_owned_bytes(read);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,definition_input(&f)+sizeof(payload),&error)==WORKBOOK_CODEC_READ_ACCEPTED);size_t persistent=workbook_codec_read_owned_bytes(read)-before;workbook_codec_read_free(read);
    read=definition_header(14,1,1,false,0,PRESENTATION_LIMIT,"");CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error));before=workbook_codec_read_owned_bytes(read);size_t scratch=3u*strlen(payload)+4102u+6160u*sizeof(char *),stage=persistent+2u*sizeof(void *)+DX_MARK_ROLES*sizeof(size_t),retained=definition_input(&f)+sizeof(payload);CHECK(before+retained+stage+scratch<PRESENTATION_LIMIT);size_t pressure_bytes=PRESENTATION_LIMIT-before-retained-stage-scratch;unsigned char *pressure=dx_alloc(pressure_bytes+1u);
    payload[0]='x';CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,retained+pressure_bytes+1u,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"scratch"));error_clear(&error);CHECK(workbook_codec_read_owned_bytes(read)==before&&!workbook_codec_read_request(read)->first_row);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"pivot"));error_clear(&error);payload[0]='[';CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(pressure);definition_empty(read,true,retained);WorkbookCodecHeader *old=NULL;CHECK(workbook_codec_read_finish(read,&old,&error));workbook_codec_read_free(read);definition_free(&f);const PivotDefinition *old_pivot=NULL;CHECK(workbook_codec_header_pivot_definition(old,0,1,&old_pivot));const char *old_value=old_pivot->fields[0][0].value;size_t old_bytes=workbook_codec_header_owned_bytes(old);
    char *query=dx_alloc(65537);memset(query,'q',65536);query[65536]=0;const size_t size=8u*1024u*1024u;char *value=dx_alloc(size+1u);memset(value,'v',size);value[size]=0;read=definition_header(14,1,2,true,0,PRESENTATION_LIMIT,query);CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error));f=definition_fixture(1,false);f.cells[7]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={value,size+1u}};batch=definition_batch(&f,0,1,true);retained=definition_input(&f)+size+1u+65537u+old_bytes;reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,retained);f.cells[7].value.text.bytes=size;reject(read,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,retained);f.cells[7].value.text.bytes=size-32768u;before=workbook_codec_read_owned_bytes(read);
    CHECK(2u*sizeof(SheetObject)+1024u<32768u);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"native 8 MiB current"));error_clear(&error);CHECK(workbook_codec_read_owned_bytes(read)==before&&!workbook_codec_read_request(read)->first_row);WorkbookCodecHeader *identity=old;CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity&&workbook_codec_header_owned_bytes(old)==old_bytes&&old_pivot->fields[0][0].value==old_value&&!strcmp(old_value,payload));error_clear(&error);
    f.cells[7].value.text.bytes=size-131072u;CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_PIVOT_FIELDS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);definition_empty(read,true,retained);CHECK(workbook_codec_read_finish(read,&old,&error));workbook_codec_read_free(read);definition_free(&f);WorkbookCodecTableSourceView table={0};CHECK(workbook_codec_header_table_source(old,0,2,&table)&&strlen(table.analysis->query)==65536);const PivotDefinition *p=NULL;CHECK(workbook_codec_header_pivot_definition(old,0,1,&p)&&strlen(p->fields[0][0].value)==size-131072u);workbook_codec_header_free(old);free(query);free(value);
}
#undef PRESENTATION_LIMIT

#define PRIMARY_FILTER_LIMIT (64u*1024u*1024u)
typedef struct { WorkbookCodecColumn columns[5]; WorkbookCodecAtom cells[961*5]; char names[15][32]; } PrimaryFilterFixture;
static PrimaryFilterFixture *primary_filter_fixture(size_t count) {
    PrimaryFilterFixture *f=dx_alloc(sizeof(*f));const char *names[]={"sheet_id","position","name","op","value"};WorkbookCodecAtomKind kinds[]={WORKBOOK_CODEC_I32,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_TEXT};for(size_t c=0;c<5;c++)f->columns[c]=(WorkbookCodecColumn){names[c],kinds[c]};
    for(size_t n=0;n<15;n++)snprintf(f->names[n],sizeof(f->names[n]),"Field 'İ' %zu",n);
    for(size_t row=0;row<count;row++){WorkbookCodecAtom *c=f->cells+row*5;c[0]=i((int32_t)(row/15));c[1]=i((int32_t)(row%15));c[2]=t(f->names[row%15]);c[3]=i(row%15==14?FILTER_BETWEEN:FILTER_CONTAINS);c[4]=row%15==14?t("[\"10\",\"20\"]"):row%3==0?(WorkbookCodecAtom){0}:t(row%3==1?"":"NULL");}return f;
}
static WorkbookCodecBatch primary_filter_batch(PrimaryFilterFixture *f,size_t first,size_t count,bool end) {return (WorkbookCodecBatch){f->columns,5,f->cells+first*5,count,first,end};}
static WorkbookCodecRead *primary_header(unsigned version,size_t sheets,unsigned option,const char *name,const char *value,FilterOp op,size_t limit) {
    WorkbookCodecRead *read=NULL;char *error=NULL;
    CHECK(option?workbook_codec_read_begin_with_options(&read,key,version,WORKBOOK_CODEC_SHEETS_25,(WorkbookCodecReadOptions){option==2},limit,NULL,NULL,&error):workbook_codec_read_begin(&read,key,version,WORKBOOK_CODEC_SHEETS_25,limit,NULL,NULL,&error));meta(read,version,0,0);Fixture *f=dx_alloc(sizeof(*f));fixture_columns(f,"sheets");for(size_t r=0;r<sheets;r++){sheet(f,r,SOURCE_NONE);f->cells[r*25+6]=name?t(name):(WorkbookCodecAtom){0};f->cells[r*25+7]=value?t(value):(WorkbookCodecAtom){0};f->cells[r*25+8]=i(op);}
    WorkbookCodecBatch batch={f->columns,25,f->cells,sheets,0,true};CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f)+(value?strlen(value)+1:0)+(name?strlen(name)+1:0),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);return read;
}
static void reject_primary_arm(WorkbookCodecRead *read,bool present,size_t retained) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(read);WorkbookCodecRequest saved=request?*request:(WorkbookCodecRequest){0};size_t bytes=workbook_codec_read_owned_bytes(read);char *error=NULL;CHECK(!workbook_codec_read_add_filters(read,present,retained,&error)&&error&&*error&&workbook_codec_read_owned_bytes(read)==bytes);CHECK(request?same_request(saved,workbook_codec_read_request(read)):!workbook_codec_read_request(read));error_clear(&error);
}
static void primary_empty(WorkbookCodecRead *read,size_t retained) {PrimaryFilterFixture *f=primary_filter_fixture(0);const WorkbookCodecRequest *request=workbook_codec_read_request(read);WorkbookCodecBatch batch={f->columns,5,f->cells,0,request?request->first_row:0,true};char *error=NULL;CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f)+retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);}
static void test_primary_filters_contract(void) {
    CHECK(WORKBOOK_CODEC_FILTERS==14&&!workbook_codec_header_filters_ready(NULL));reject_primary_arm(NULL,true,0);WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL);reject_primary_arm(read,true,0);workbook_codec_read_free(read);
    for(unsigned version=12;version<=14;version++)for(unsigned present=0;present<2;present++){
        read=primary_header(version,1,0,"Primary",NULL,FILTER_NULL,LIMIT);char *error=NULL;if(version==14&&!present){reject_primary_arm(read,false,0);workbook_codec_read_free(read);continue;}
        CHECK(workbook_codec_read_add_filters(read,present!=0,0,&error));if(present){const WorkbookCodecRequest *request=workbook_codec_read_request(read);CHECK(request&&request->family==WORKBOOK_CODEC_FILTERS&&request->column_count==5&&request->source_column_count==5&&request->row_limit==961&&!strcmp(request->sql,"SELECT sheet_id,position,name,op,value FROM _duxcel_workspace.filters ORDER BY sheet_id,position LIMIT 961"));primary_empty(read,0);}
        reject_primary_arm(read,true,0);WorkbookCodecHeader *header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error)&&workbook_codec_header_filters_ready(header));const Analysis *a=workbook_codec_header_analysis(header,0);CHECK(a&&a->kind==SOURCE_NONE&&!a->filter_value&&!a->filter_count&&!strcmp(a->filter_column,"Primary")&&a->filter_op==FILTER_NULL&&!a->filter_number.kind);workbook_codec_read_free(read);workbook_codec_header_free(header);
    }
    /* Missing legacy tables still validate primary filters, including JSON. */
    const char *names[]={"", "Primary"};for(size_t n=0;n<2;n++){read=primary_header(12,1,0,names[n],"[\"only\"]",FILTER_BETWEEN,LIMIT);reject_primary_arm(read,false,0);CHECK(!workbook_codec_read_add_filters(read,false,0,NULL));workbook_codec_read_free(read);}
    read=primary_header(13,1,0,"Primary","NULL",FILTER_CONTAINS,LIMIT);reject_primary_arm(read,false,SIZE_MAX);char *error=NULL;CHECK(workbook_codec_read_add_filters(read,false,0,&error));WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error)&&!strcmp(workbook_codec_header_analysis(h,0)->filter_value,"NULL"));workbook_codec_read_free(read);workbook_codec_header_free(h);
}
static void test_primary_filter_capture_options(void) {
    const size_t maximum=4u*1024u*1024u;char *large=dx_alloc(maximum+2u);memset(large,'v',maximum+1u);large[maximum+1u]=0;char *name=dx_alloc(65538);memset(name,'n',65537);name[65537]=0;
    for(unsigned option=0;option<3;option++){
        WorkbookCodecRead *read=NULL;char *error=NULL;CHECK(option?workbook_codec_read_begin_with_options(&read,key,14,WORKBOOK_CODEC_SHEETS_25,(WorkbookCodecReadOptions){option==2},PRIMARY_FILTER_LIMIT,NULL,NULL,&error):workbook_codec_read_begin(&read,key,14,WORKBOOK_CODEC_SHEETS_25,PRIMARY_FILTER_LIMIT,NULL,NULL,&error));meta(read,14,0,0);Fixture *f=dx_alloc(sizeof(*f));fixture_columns(f,"sheets");sheet(f,0,SOURCE_NONE);f->cells[6]=t("Primary");f->cells[7]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={large,65537}};WorkbookCodecBatch batch={f->columns,25,f->cells,1,0,true};
        if(option<2){reject(read,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f)+maximum+2u+65538u);f->cells[7].value.text.bytes=65536;f->cells[6]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={name,65536}};CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f)+maximum+2u+65538u,&error)==WORKBOOK_CODEC_READ_ACCEPTED);reject_primary_arm(read,true,maximum+2u+65538u);}
        else{f->cells[7].value.text.bytes=maximum+1u;reject(read,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f)+maximum+2u+65538u);f->cells[7].value.text.bytes=maximum;f->cells[6]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={name,4097}};reject(read,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f)+maximum+2u+65538u);f->cells[6].value.text.bytes=4096;CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f)+maximum+2u+65538u,&error)==WORKBOOK_CODEC_READ_ACCEPTED);CHECK(workbook_codec_read_add_filters(read,true,sizeof(*f)+maximum+2u+65538u,&error));primary_empty(read,sizeof(*f)+maximum+2u+65538u);WorkbookCodecHeader *header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error));const Analysis *a=workbook_codec_header_analysis(header,0);CHECK(strlen(a->filter_value)==maximum&&strlen(a->filter_column)==4096&&a->filter_value[maximum-1]=='v');workbook_codec_header_free(header);}
        free(f);workbook_codec_read_free(read);
    }
    /* Constructor failure must retain the prior reader, including its original
       default capture policy. A failed opt-in cannot relax a live reader. */
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL),*identity=read;size_t bytes=workbook_codec_read_owned_bytes(read);WorkbookCodecRequest request=*workbook_codec_read_request(read);WorkbookCodecKey invalid=key;invalid.document_epoch=0;
    CHECK(!workbook_codec_read_begin_with_options(&read,invalid,14,WORKBOOK_CODEC_SHEETS_25,(WorkbookCodecReadOptions){true},LIMIT,NULL,NULL,NULL)&&read==identity&&workbook_codec_read_owned_bytes(read)==bytes&&same_request(request,workbook_codec_read_request(read)));CHECK(!workbook_codec_read_begin_with_options(&read,key,14,WORKBOOK_CODEC_SHEETS_25,(WorkbookCodecReadOptions){true},bytes-1,NULL,NULL,NULL)&&read==identity&&workbook_codec_read_owned_bytes(read)==bytes);meta(read,14,0,0);Fixture *f=dx_alloc(sizeof(*f));fixture_columns(f,"sheets");sheet(f,0,SOURCE_NONE);f->cells[7]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={large,65537}};WorkbookCodecBatch batch={f->columns,25,f->cells,1,0,true};reject(read,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*f)+maximum+2u+65538u);free(f);workbook_codec_read_free(read);free(name);free(large);
}
static void test_primary_filters_owned_and_invalid(void) {
    WorkbookCodecRead *read=primary_header(14,2,2,"Primary",NULL,FILTER_NULL,LIMIT);char *error=NULL;CHECK(workbook_codec_read_add_filters(read,true,0,&error));PrimaryFilterFixture *f=primary_filter_fixture(3);WorkbookCodecBatch batch=primary_filter_batch(f,0,3,false);size_t owned=workbook_codec_read_owned_bytes(read);error=dx_strdup("stale retained");char *previous=error;WorkbookCodecKey stale=key;stale.operation_ticket--;CHECK(workbook_codec_read_feed(read,stale,(WorkbookCodecFamily)99,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE&&error==previous&&workbook_codec_read_owned_bytes(read)==owned);free(error);error=NULL;
    for(size_t c=0;c<5;c++){WorkbookCodecAtomKind kind=f->columns[c].kind;f->columns[c].kind=kind==WORKBOOK_CODEC_TEXT?WORKBOOK_CODEC_BOOLEAN:WORKBOOK_CODEC_TEXT;reject(read,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f));f->columns[c].kind=kind;if(c!=4){WorkbookCodecAtom saved=f->cells[c];f->cells[c]=(WorkbookCodecAtom){0};reject(read,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f));f->cells[c]=saved;}}
    f->cells[1]=i(1);reject(read,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f));f->cells[1]=i(0);f->cells[2]=t("pRIMARY");reject(read,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f));f->cells[2]=t("NULL");f->cells[5+2]=t("null");reject(read,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f));f->cells[5+2]=t("Empty");f->cells[10+2]=t("Literal");
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);memset(f->cells,0,sizeof(f->cells));free(f);f=primary_filter_fixture(1);f->cells[0]=i(0);f->cells[1]=i(3);f->cells[3]=i(FILTER_BETWEEN);f->cells[4]=t("[\"one\"]");batch=(WorkbookCodecBatch){f->columns,5,f->cells,1,3,false};reject(read,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f));f->cells[0]=i(1);f->cells[1]=i(0);f->cells[4]=t("[\"10\",\"20\"]");CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);f->cells[0]=i(0);f->cells[1]=i(3);batch.first_row=4;reject(read,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f));free(f);primary_empty(read,0);
    WorkbookCodecHeader *header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error)&&workbook_codec_header_filters_ready(header));workbook_codec_read_free(read);const Analysis *a=workbook_codec_header_analysis(header,0),*b=workbook_codec_header_analysis(header,1);CHECK(a->kind==SOURCE_NONE&&a->filter_count==3&&!strcmp(a->filters[0].column,"NULL")&&!a->filters[0].value&&a->filters[1].value&&!*a->filters[1].value&&!strcmp(a->filters[2].value,"NULL")&&!a->filters[0].number.kind&&b->filter_count==1&&b->filters[0].op==FILTER_BETWEEN&&!strcmp(b->filters[0].value,"[\"10\",\"20\"]"));workbook_codec_header_free(header);
    read=primary_header(14,1,2,NULL,NULL,FILTER_NULL,LIMIT);CHECK(workbook_codec_read_add_filters(read,true,0,&error));f=primary_filter_fixture(1);batch=primary_filter_batch(f,0,1,true);reject(read,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f));free(f);primary_empty(read,0);workbook_codec_read_free(read);
}
static void test_primary_filters_full_limits_and_composition(void) {
    WorkbookCodecRead *read=primary_header(14,64,2,"Primary","",FILTER_EQUAL,PRIMARY_FILTER_LIMIT);char *error=NULL;CHECK(workbook_codec_read_add_filters(read,true,0,&error));PrimaryFilterFixture *f=primary_filter_fixture(961);WorkbookCodecBatch batch=primary_filter_batch(f,0,257,false);reject(read,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f));
    for(size_t first=0;first<960;first+=256){size_t count=960-first>256?256:960-first;batch=primary_filter_batch(f,first,count,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);}batch=primary_filter_batch(f,960,1,true);reject(read,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f));batch=primary_filter_batch(f,960,0,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error));workbook_codec_read_free(read);for(size_t sheet=0;sheet<64;sheet++){const Analysis *a=workbook_codec_header_analysis(header,sheet);CHECK(a->filter_count==15&&a->filters[14].op==FILTER_BETWEEN&&!a->filters[0].value&&a->filters[1].value&&!*a->filters[1].value&&!strcmp(a->filters[2].value,"NULL"));}workbook_codec_header_free(header);header=NULL;
    /* Compose independently before/after queries/options and retain their exact
       strings, POD and locator pointers throughout secondary filter adoption. */
    for(unsigned filters_first=0;filters_first<2;filters_first++){
        read=query_header(14,1,0,SOURCE_DEMO,true,LIMIT);QueryFixture *q=query_fixture(1);q->cells[0]=i(0);q->cells[1]=u(0);q->cells[2]=t("SELECT exact;  \n");if(filters_first){CHECK(workbook_codec_read_add_filters(read,true,sizeof(*q),&error));primary_empty(read,sizeof(*q));}
        CHECK(workbook_codec_read_add_queries(read,true,&error));batch=query_batch(q,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*q),&error)==WORKBOOK_CODEC_READ_ACCEPTED);q->columns[2]=(WorkbookCodecColumn){"unique_rows",WORKBOOK_CODEC_BOOLEAN};q->cells[2]=b(true);CHECK(workbook_codec_read_add_source_options(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*q),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(q);if(!filters_first){CHECK(workbook_codec_read_add_filters(read,true,0,&error));primary_empty(read,0);}CHECK(workbook_codec_read_finish(read,&header,&error));const Analysis *a=workbook_codec_header_analysis(header,0);const char *query=NULL;bool flag=false;CHECK(a->path&&!*a->path&&workbook_codec_header_query(header,0,0,&query)&&query==a->query&&!strcmp(query,"SELECT exact;  \n")&&workbook_codec_header_unique_rows(header,0,0,&flag)&&flag&&a->unique_rows&&workbook_codec_header_filters_ready(header)&&!a->filter_count);workbook_codec_read_free(read);workbook_codec_header_free(header);header=NULL;header=NULL;
    }
}
static void test_primary_filters_parser_and_owned_budgets(void) {
    char payload[]="[\"6\",\"[\\\"a\\\"]\",\"1\",\"x\"]";WorkbookCodecRead *read=primary_header(12,1,2,"Primary",payload,FILTER_COMPOSITE,PRIMARY_FILTER_LIMIT);char *error=NULL;size_t owned=workbook_codec_read_owned_bytes(read),scratch=3u*strlen(payload)+4102u+6160u*sizeof(char *);size_t retained=sizeof(payload),pressure_bytes=PRIMARY_FILTER_LIMIT-owned-retained-scratch;unsigned char *pressure=dx_alloc(pressure_bytes+1u);
    CHECK(!workbook_codec_read_add_filters(read,false,retained+pressure_bytes+1u,&error)&&error&&strstr(error,"scratch")&&workbook_codec_read_owned_bytes(read)==owned&&!workbook_codec_read_request(read));error_clear(&error);CHECK(workbook_codec_read_add_filters(read,false,retained+pressure_bytes,&error));free(pressure);WorkbookCodecHeader *old=NULL;CHECK(workbook_codec_read_finish(read,&old,&error));workbook_codec_read_free(read);const Analysis *prior=workbook_codec_header_analysis(old,0);const char *prior_value=prior->filter_value;size_t old_bytes=workbook_codec_header_owned_bytes(old);
    /* The same scratch reservation protects present arming and prospective
       secondary JSON, with old output and owned caller buffers coexisting. */
    read=primary_header(14,1,2,"Primary",NULL,FILTER_NULL,PRIMARY_FILTER_LIMIT);CHECK(workbook_codec_read_add_filters(read,true,old_bytes,&error));PrimaryFilterFixture *f=primary_filter_fixture(1);f->cells[3]=i(FILTER_COMPOSITE);f->cells[4]=t(payload);WorkbookCodecBatch batch=primary_filter_batch(f,0,1,true);owned=workbook_codec_read_owned_bytes(read);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,sizeof(*f)+sizeof(payload)+old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);size_t copy=workbook_codec_read_owned_bytes(read)-owned;workbook_codec_read_free(read);
    read=primary_header(14,1,2,"Primary",NULL,FILTER_NULL,PRIMARY_FILTER_LIMIT);CHECK(workbook_codec_read_add_filters(read,true,old_bytes,&error));owned=workbook_codec_read_owned_bytes(read);size_t stage=copy+sizeof(size_t)+sizeof(AnalysisFilter);retained=sizeof(*f)+sizeof(payload)+old_bytes;pressure_bytes=PRIMARY_FILTER_LIMIT-owned-retained-stage-scratch;pressure=dx_alloc(pressure_bytes+1u);payload[0]='x';CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,retained+pressure_bytes+1u,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"scratch"));error_clear(&error);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"filter"));error_clear(&error);CHECK(workbook_codec_read_owned_bytes(read)==owned&&!workbook_codec_read_request(read)->first_row);WorkbookCodecHeader *identity=old;CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity&&workbook_codec_header_owned_bytes(old)==old_bytes&&prior->filter_value==prior_value&&!strcmp(prior_value,"[\"6\",\"[\\\"a\\\"]\",\"1\",\"x\"]"));error_clear(&error);payload[0]='[';CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(pressure);CHECK(workbook_codec_read_finish(read,&old,&error));workbook_codec_read_free(read);free(f);workbook_codec_header_free(old);old=NULL;
    /* Raw secondary SQL bytes exclude terminators and primary. Native filter
       bytes include both: exact8MiB succeeds; one byte over fails atomically. */
    const size_t maximum=4u*1024u*1024u;char *large=dx_alloc(maximum+1u);memset(large,'x',maximum);large[maximum]=0;read=primary_header(14,1,2,"Primary",NULL,FILTER_NULL,PRIMARY_FILTER_LIMIT);CHECK(workbook_codec_read_add_filters(read,true,maximum+1u,&error));f=primary_filter_fixture(2);f->cells[2]=t("F0");f->cells[7]=t("F1");f->cells[4]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={large,maximum}};f->cells[9]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={large,maximum}};batch=primary_filter_batch(f,0,2,true);retained=sizeof(*f)+maximum+1u;owned=workbook_codec_read_owned_bytes(read);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"Raw secondary"));error_clear(&error);f->cells[9].value.text.bytes=maximum-15u;CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"filters exceed 8 MiB"));error_clear(&error);CHECK(workbook_codec_read_owned_bytes(read)==owned&&!workbook_codec_read_request(read)->first_row);f->cells[9].value.text.bytes=maximum-16u;CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FILTERS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);CHECK(workbook_codec_read_finish(read,&old,&error));workbook_codec_read_free(read);const Analysis *a=workbook_codec_header_analysis(old,0);CHECK(strlen(a->filters[0].value)==maximum&&strlen(a->filters[1].value)==maximum-16u&&!a->filter_value);workbook_codec_header_free(old);free(f);free(large);
}
#undef PRIMARY_FILTER_LIMIT


/* Column presentation is independently armed. The original20,092 assertion
   definitions above remain unchanged, including default/unarmed Header shape. */
typedef struct { WorkbookCodecColumn columns[4]; WorkbookCodecAtom cells[256*4]; char names[256][48]; } ColumnsFixture;
static ColumnsFixture *columns_fixture(size_t sheet_id,size_t index,size_t rows) {
    ColumnsFixture *f=dx_alloc(sizeof(*f));
    f->columns[0]=(WorkbookCodecColumn){"sheet_id",WORKBOOK_CODEC_I32}; f->columns[1]=(WorkbookCodecColumn){"column_id",WORKBOOK_CODEC_I32};
    f->columns[2]=(WorkbookCodecColumn){"name",WORKBOOK_CODEC_TEXT}; f->columns[3]=(WorkbookCodecColumn){"width",WORKBOOK_CODEC_DOUBLE};
    for(size_t row=0;row<rows;row++) {
        snprintf(f->names[row],sizeof(f->names[row]),"Column 'İ' %zu:%zu",sheet_id,index+row);
        f->cells[row*4]=i((int32_t)sheet_id); f->cells[row*4+1]=i((int32_t)(index+row));
        f->cells[row*4+2]=t(f->names[row]); f->cells[row*4+3]=d(52+(double)((index+row)%1000)/8);
    }
    return f;
}
static WorkbookCodecBatch columns_batch(ColumnsFixture *f,size_t first,size_t rows,bool end) {return (WorkbookCodecBatch){f->columns,4,f->cells,rows,first,end};}
static void reject_columns_arm(WorkbookCodecRead *read,bool present) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(read); WorkbookCodecRequest before=request?*request:(WorkbookCodecRequest){0};
    size_t bytes=workbook_codec_read_owned_bytes(read); char *error=NULL;
    CHECK(!workbook_codec_read_add_columns(read,present,&error)&&error&&*error&&workbook_codec_read_owned_bytes(read)==bytes);
    CHECK(request?same_request(before,workbook_codec_read_request(read)):!workbook_codec_read_request(read)); error_clear(&error);
}
static void columns_end(WorkbookCodecRead *read,size_t first,size_t retained) {
    ColumnsFixture *f=columns_fixture(0,0,0); WorkbookCodecBatch batch=columns_batch(f,first,0,true); char *error=NULL;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f)+retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error); free(f);
}
static void test_columns_contract_and_composition(void) {
    CHECK(WORKBOOK_CODEC_FILTERS==14&&WORKBOOK_CODEC_COLUMNS==15&&!workbook_codec_header_columns_ready(NULL)); reject_columns_arm(NULL,true);
    CHECK(!workbook_codec_read_add_columns(NULL,true,NULL));
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); reject_columns_arm(read,true); meta(read,14,0,0); reject_columns_arm(read,true); workbook_codec_read_free(read);
    char *error=NULL;
    for(unsigned version=12;version<=14;version++) {
        read=sort_header(version,2,NULL,LIMIT); reject_columns_arm(read,false); CHECK(!workbook_codec_read_add_columns(read,false,NULL));
        WorkbookCodecHeader *plain=NULL; CHECK(workbook_codec_read_finish(read,&plain,&error)&&!workbook_codec_header_columns_ready(plain));
        CHECK(!workbook_codec_header_view(plain,0)->columns&&!workbook_codec_header_view(plain,0)->widths&&!workbook_codec_header_view(plain,0)->column_count);
        reject_columns_arm(read,true); workbook_codec_read_free(read); workbook_codec_header_free(plain);
        read=sort_header(version,2,NULL,LIMIT); size_t bytes=workbook_codec_read_owned_bytes(read); CHECK(workbook_codec_read_add_columns(read,true,&error)&&!error);
        const WorkbookCodecRequest *request=workbook_codec_read_request(read);
        CHECK(request&&request->family==WORKBOOK_CODEC_COLUMNS&&request->column_count==4&&request->source_column_count==4&&request->row_limit==1048577u&&!request->first_row);
        CHECK(!strcmp(request->sql,"SELECT sheet_id,column_id,name,width FROM _duxcel_workspace.columns ORDER BY sheet_id,column_id LIMIT 1048577")&&!strstr(request->sql,"OFFSET"));
        CHECK(workbook_codec_read_owned_bytes(read)==bytes); reject_columns_arm(read,true); reject_columns_arm(read,false);
        ColumnsFixture *f=columns_fixture(0,0,0); WorkbookCodecBatch batch=columns_batch(f,0,0,false); reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f)); free(f);
        columns_end(read,0,0); CHECK(workbook_codec_read_owned_bytes(read)==bytes); reject_columns_arm(read,true);
        WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)&&workbook_codec_header_columns_ready(header));
        CHECK(!workbook_codec_header_view(header,0)->column_count&&!workbook_codec_header_view(header,1)->columns&&!workbook_codec_header_view(header,1)->widths);
        workbook_codec_read_free(read); workbook_codec_header_free(header);
    }
    /* Table query/options and genuine definitions coexist unchanged; Columns
       can precede/follow the other independent complete Header families. */
    for(unsigned order=0;order<2;order++) {
        const char *query="SELECT 'NULL', 'İ';  \n";read=definition_header(12,1,2,true,0,LIMIT,query);
        if(!order){CHECK(workbook_codec_read_add_columns(read,true,&error));columns_end(read,0,0);}
        CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error)); definition_empty(read,false,0); definition_empty(read,true,0);
        CHECK(workbook_codec_read_add_filters(read,false,0,&error)); CHECK(workbook_codec_read_add_sorts(read,false,&error)); CHECK(workbook_codec_read_add_preferences(read,false,&error));
        if(order){CHECK(workbook_codec_read_add_columns(read,true,&error));columns_end(read,0,0);}
        WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
        WorkbookCodecTableSourceView source={0}; const char *stored=NULL; bool unique=false; const PivotDefinition *pivot=NULL; const MarkDefinition *marks=NULL;
        CHECK(workbook_codec_header_columns_ready(header)&&workbook_codec_header_pivot_marks_ready(header)&&workbook_codec_header_filters_ready(header)&&workbook_codec_header_sorts_ready(header)&&workbook_codec_header_preferences_ready(header));
        CHECK(workbook_codec_header_table_source(header,0,2,&source)&&source.analysis&&source.locator_available&&!strcmp(source.reference,"")&&!strcmp(source.analysis->path,""));
        CHECK(workbook_codec_header_query(header,0,2,&stored)&&stored==source.analysis->query&&!strcmp(stored,query)); CHECK(workbook_codec_header_unique_rows(header,0,2,&unique)&&unique&&source.analysis->unique_rows);
        CHECK(workbook_codec_header_pivot_definition(header,0,1,&pivot)&&pivot&&workbook_codec_header_marks_definition(header,0,1,&marks)&&marks&&!pivot->counts[0]&&!marks->counts[0]);
        workbook_codec_header_free(header);
    }
}
static void test_columns_splits_owned_and_text(void) {
    WorkbookCodecRead *read=sort_header(14,3,NULL,LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_columns(read,true,&error));
    const size_t chunks[]={1,255,256}; size_t first=0;
    for(size_t n=0;n<3;n++) {
        size_t rows=chunks[n]; ColumnsFixture *f=columns_fixture(0,first,rows);
        if(!first){f->cells[2]=t("");f->cells[3]=d(52);}
        if(first==1){f->cells[2]=t("Same");f->cells[6]=t("Same");f->cells[3]=d(1200);f->cells[7]=d(nextafter(52,INFINITY));}
        WorkbookCodecBatch batch=columns_batch(f,first,rows,false); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
        first+=rows; CHECK(workbook_codec_read_request(read)->first_row==first&&workbook_codec_read_request(read)->sql);
        memset(f,0xa5,sizeof(*f)); free(f);
    }
    CHECK(first==512); WorkbookCodecHeader *header=NULL; CHECK(!workbook_codec_read_finish(read,&header,&error)&&!header); error_clear(&error);
    /* Skipping empty sheets is valid; each new sheet still starts at column0. */
    ColumnsFixture *f=columns_fixture(2,0,1); WorkbookCodecBatch batch=columns_batch(f,first,1,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);memset(f,0,sizeof(*f));free(f);
    CHECK(workbook_codec_read_finish(read,&header,&error)); workbook_codec_read_free(read);
    const SheetView *v=workbook_codec_header_view(header,0); CHECK(v->column_count==512&&!v->columns[0][0]&&!strcmp(v->columns[1],"Same")&&!strcmp(v->columns[2],"Same")&&v->columns[1]!=v->columns[2]);
    CHECK(v->widths[0]==52&&v->widths[1]==1200&&v->widths[2]==52&&v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==1.5f);
    CHECK(!strcmp(v->columns[255],"Column 'İ' 0:255")&&!strcmp(v->columns[256],"Column 'İ' 0:256")&&!strcmp(v->columns[511],"Column 'İ' 0:511"));
    CHECK(!workbook_codec_header_view(header,1)->column_count&&!workbook_codec_header_view(header,1)->columns&&workbook_codec_header_view(header,2)->column_count==1&&!strcmp(workbook_codec_header_view(header,2)->columns[0],"Column 'İ' 2:0"));
    workbook_codec_header_free(header);
    /* Native names have no4KiB/64KiB cap. Both long names coexist with the
       owned caller inputs, then survive poisoning/freeing those inputs. */
    const size_t lengths[]={65537u,1024u*1024u+17u}; size_t retained=sizeof(*f);char *texts[2];
    for(size_t n=0;n<2;n++){texts[n]=dx_alloc(lengths[n]+1u);memset(texts[n],n?'b':'a',lengths[n]);texts[n][lengths[n]]=0;retained+=lengths[n]+1u;}
    read=sort_header(14,1,NULL,LIMIT);CHECK(workbook_codec_read_add_columns(read,true,&error));f=columns_fixture(0,0,3);f->cells[2]=t(texts[0]);f->cells[6]=t(texts[1]);f->cells[10]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={NULL,0}};batch=columns_batch(f,0,3,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error));workbook_codec_read_free(read);memset(f,0,sizeof(*f));free(f);
    for(size_t n=0;n<2;n++){memset(texts[n],'x',lengths[n]);free(texts[n]);}
    v=workbook_codec_header_view(header,0);CHECK(v->column_count==3&&!v->columns[2][0]);for(size_t n=0;n<2;n++)CHECK(strlen(v->columns[n])==lengths[n]&&v->columns[n][lengths[n]-1]==(n?'b':'a'));workbook_codec_header_free(header);
}
static void test_columns_invalid_atomic_and_stale(void) {
    WorkbookCodecRead *read=sort_header(14,2,NULL,LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_columns(read,true,&error)); ColumnsFixture *f=columns_fixture(0,0,3);WorkbookCodecBatch batch=columns_batch(f,0,3,false);
    WorkbookCodecKey stale=key;stale.snapshot_generation--;size_t before=workbook_codec_read_owned_bytes(read);error=dx_strdup("owned old diagnostic");char *saved_error=error;
    CHECK(workbook_codec_read_feed(read,stale,WORKBOOK_CODEC_COLUMNS,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE&&error==saved_error&&workbook_codec_read_owned_bytes(read)==before);free(error);error=NULL;
    for(size_t c=0;c<4;c++) {
        WorkbookCodecAtom saved=f->cells[8+c];f->cells[8+c]=(WorkbookCodecAtom){0};reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));f->cells[8+c]=saved;
        f->cells[8+c]=b(true);reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));f->cells[8+c]=saved;
        WorkbookCodecAtomKind type=f->columns[c].kind;f->columns[c].kind=WORKBOOK_CODEC_U64;reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));f->columns[c].kind=type;
    }
    const double invalid[]={NAN,INFINITY,-INFINITY,-1,0,nextafter(52,-INFINITY),nextafter(1200,INFINITY),DBL_MAX};
    for(size_t n=0;n<sizeof(invalid)/sizeof(*invalid);n++){f->cells[11]=d(invalid[n]);reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));}f->cells[11]=d(52);
    const char nul[]={'a',0,'b'};f->cells[10]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={nul,sizeof(nul)}};reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));
    const char *bad[]={"\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82","\x80"};for(size_t n=0;n<sizeof(bad)/sizeof(*bad);n++){f->cells[10]=t(bad[n]);reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));}
    /* Overflow is rejected before UTF-8 reads, even with an unusable pointer. */
    f->cells[10]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={(const char *)(uintptr_t)1,SIZE_MAX}};reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));
    f->cells[10]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={NULL,1}};reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));f->cells[10]=t("Restored");
    const int32_t bad_sheets[]={-1,2,INT32_MAX};for(size_t n=0;n<3;n++){f->cells[8]=i(bad_sheets[n]);reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));}f->cells[8]=i(0);
    const int32_t bad_indices[]={-1,0,1,3,16384,INT32_MAX};for(size_t n=0;n<6;n++){f->cells[9]=i(bad_indices[n]);reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));}f->cells[9]=i(2);
    f->columns[2].name="wrong";reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));f->columns[2].name="name";batch.column_count=3;reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));batch.column_count=5;reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));batch.column_count=4;
    batch.row_count=257;reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));batch.row_count=3;batch.first_row=1;reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));batch.first_row=0;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);memset(f,0,sizeof(*f));free(f);
    f=columns_fixture(1,0,2);f->cells[4]=i(0);f->cells[5]=i(3);batch=columns_batch(f,3,2,true);reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));f->cells[4]=i(1);f->cells[5]=i(1);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error));workbook_codec_read_free(read);
    const SheetView *v=workbook_codec_header_view(header,0);CHECK(v->column_count==3&&!strcmp(v->columns[2],"Restored")&&v->widths[2]==52&&workbook_codec_header_view(header,1)->column_count==2);workbook_codec_header_free(header);
}
static void test_columns_capacity_limits_and_eof(void) {
    WorkbookCodecRead *read=sort_header(14,64,NULL,LIMIT);char *error=NULL;CHECK(workbook_codec_read_add_columns(read,true,&error));size_t first=0;
    for(size_t index=0;index<16384;index+=256) {
        ColumnsFixture *f=columns_fixture(0,index,256);WorkbookCodecBatch batch=columns_batch(f,first,256,false);
        if(index==256){f->cells[255*4+1]=i(511+1);reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));f->cells[255*4+1]=i(511);}
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);first+=256;memset(f,0,sizeof(*f));free(f);
    }
    CHECK(first==16384&&workbook_codec_read_request(read)->first_row==first);ColumnsFixture *f=columns_fixture(0,16384,1);WorkbookCodecBatch batch=columns_batch(f,first,1,true);reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));free(f);
    /* The next sheet starts at0 and can follow the full previous sheet in the
       same single immutable result. A later backtrack remains invalid. */
    f=columns_fixture(1,0,256);batch=columns_batch(f,first,256,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=256;free(f);
    f=columns_fixture(2,0,2);f->cells[4]=i(63);f->cells[5]=i(0);batch=columns_batch(f,first,2,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=2;free(f);
    f=columns_fixture(1,256,1);batch=columns_batch(f,first,1,true);reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f));free(f);
    WorkbookCodecHeader *header=NULL;CHECK(!workbook_codec_read_finish(read,&header,&error)&&!header);error_clear(&error);columns_end(read,first,0);CHECK(workbook_codec_read_finish(read,&header,&error));workbook_codec_read_free(read);
    CHECK(workbook_codec_header_columns_ready(header)&&workbook_codec_header_view(header,0)->column_count==16384&&workbook_codec_header_view(header,1)->column_count==256&&workbook_codec_header_view(header,2)->column_count==1&&workbook_codec_header_view(header,63)->column_count==1);
    CHECK(!strcmp(workbook_codec_header_view(header,0)->columns[16383],"Column 'İ' 0:16383")&&!strcmp(workbook_codec_header_view(header,63)->columns[0],"Column 'İ' 2:1")&&!workbook_codec_header_view(header,62)->column_count);workbook_codec_header_free(header);
}
static void test_columns_exact_peak_budget_and_retained_output(void) {
    char *error=NULL;WorkbookCodecRead *old_read=sort_header(12,1,NULL,LIMIT);CHECK(workbook_codec_read_add_columns(old_read,true,&error));ColumnsFixture *f=columns_fixture(0,0,1);WorkbookCodecBatch batch=columns_batch(f,0,1,true);CHECK(workbook_codec_read_feed(old_read,key,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);
    WorkbookCodecHeader *old=NULL;CHECK(workbook_codec_read_finish(old_read,&old,&error));workbook_codec_read_free(old_read);WorkbookCodecHeader *identity=old;size_t old_bytes=workbook_codec_header_owned_bytes(old);const SheetView *v=workbook_codec_header_view(old,0);SheetView view=*v;const char *name=v->columns[0];
    WorkbookCodecRead *read=sort_header(14,1,NULL,LIMIT);CHECK(workbook_codec_read_add_columns(read,true,&error));size_t first=0;
    const size_t counts[]={1,16,1};size_t capacity=0;
    for(size_t n=0;n<3;n++) {
        size_t rows=counts[n];f=columns_fixture(0,first,rows);for(size_t r=0;r<rows;r++)f->cells[r*4+2]=t("n");batch=columns_batch(f,first,rows,false);
        size_t next=capacity?capacity:16;while(next<first+rows)next*=2;
        size_t arrays=next>capacity?next*(sizeof(char *)+sizeof(float)):0,retired=next>capacity?capacity*(sizeof(char *)+sizeof(float)):0;
        size_t added=rows*2+arrays-retired,peak=rows*sizeof(char *)+rows*2+arrays,owned=workbook_codec_read_owned_bytes(read),retained=sizeof(*f)+old_bytes;
        CHECK(owned+retained+peak<LIMIT);size_t pressure_bytes=LIMIT-owned-retained-peak;unsigned char *pressure=dx_alloc(pressure_bytes+1);
        reject(read,WORKBOOK_CODEC_COLUMNS,&batch,retained+pressure_bytes+1);CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity);error_clear(&error);
        CHECK(workbook_codec_header_owned_bytes(old)==old_bytes&&workbook_codec_header_view(old,0)==v&&!memcmp(v,&view,sizeof(view))&&v->columns[0]==name&&!strcmp(name,"Column 'İ' 0:0"));
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(read)==owned+added);
        free(pressure);memset(f,0,sizeof(*f));free(f);first+=rows;capacity=next;
    }
    columns_end(read,first,old_bytes);CHECK(workbook_codec_read_finish(read,&old,&error)&&old!=identity&&workbook_codec_header_columns_ready(old));workbook_codec_read_free(read);v=workbook_codec_header_view(old,0);
    CHECK(v->column_count==18&&v->columns&&v->widths&&!strcmp(v->columns[0],"n")&&!strcmp(v->columns[17],"n")&&v->columns[0]!=v->columns[17]);workbook_codec_header_free(old);
}
static void test_columns_name_budget_precedes_content(void) {
    WorkbookCodecRead *read=sort_header(14,1,NULL,LIMIT);char *error=NULL;CHECK(workbook_codec_read_add_columns(read,true,&error));
    ColumnsFixture *f=columns_fixture(0,0,1);WorkbookCodecBatch batch=columns_batch(f,0,1,false);size_t bytes=workbook_codec_read_owned_bytes(read);
    /* This is an ordering guard, NOT a valid borrowed input allocation. The
       placeholder content is intentionally unreadable and must never be used:
       its announced name alone cannot fit the explicit8MiB owned limit. It
       proves no giant UTF-8 scan occurs before that independent size denial. */
    f->cells[2]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={(const char *)(uintptr_t)1,1024u*1024u*1024u}};
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"budget"));error_clear(&error);
    CHECK(workbook_codec_read_owned_bytes(read)==bytes&&!workbook_codec_read_request(read)->first_row);
    /* Real finite borrowed storage under exactly the same admission policy. */
    const size_t length=1024u*1024u+17u;char *name=dx_alloc(length+1u);memset(name,'z',length);name[length]=0;f->cells[2]=t(name);
    size_t extra=sizeof(char *)+16u*(sizeof(char *)+sizeof(float))+length+1u,retained=sizeof(*f)+length+1u;
    CHECK(bytes+retained+extra<LIMIT);size_t pressure_bytes=LIMIT-bytes-retained-extra;unsigned char *pressure=dx_alloc(pressure_bytes+1u);
    reject(read,WORKBOOK_CODEC_COLUMNS,&batch,retained+pressure_bytes+1u);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);free(pressure);memset(name,'x',length);free(name);free(f);
    columns_end(read,1,0);WorkbookCodecHeader *header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error));workbook_codec_read_free(read);
    const SheetView *v=workbook_codec_header_view(header,0);CHECK(v->column_count==1&&strlen(v->columns[0])==length&&v->columns[0][length-1]=='z');workbook_codec_header_free(header);
}
/* END independently bounded column presentation tests. */

/* Compact stored-cell metadata; no CellStore setter, histories or missing-cell
   materialization. All20,988 earlier assertion definitions remain above. */
typedef struct { WorkbookCodecColumn columns[4]; WorkbookCodecAtom cells[256*4]; char text[256][48]; } CellsFixture;
static CellsFixture *cells_fixture(size_t sheet_id,size_t ordinal,size_t rows) {
    CellsFixture *f=dx_alloc(sizeof(*f));f->columns[0]=(WorkbookCodecColumn){"sheet_id",WORKBOOK_CODEC_I32};f->columns[1]=(WorkbookCodecColumn){"row_id",WORKBOOK_CODEC_U64};f->columns[2]=(WorkbookCodecColumn){"column_id",WORKBOOK_CODEC_I32};f->columns[3]=(WorkbookCodecColumn){"text",WORKBOOK_CODEC_TEXT};
    static const char *values[]={"","NULL","'İ' 🦆;\n","18446744073709551615","9007199254740993","literal ?"};
    for(size_t n=0;n<rows;n++){size_t index=ordinal+n;strcpy(f->text[n],values[index%6]);f->cells[n*4]=i((int32_t)sheet_id);f->cells[n*4+1]=u(index/16);f->cells[n*4+2]=i((int32_t)((index%16)*2));f->cells[n*4+3]=t(f->text[n]);}return f;
}
static WorkbookCodecBatch cells_batch(CellsFixture *f,size_t first,size_t rows,bool end) {return (WorkbookCodecBatch){f->columns,4,f->cells,rows,first,end};}
static void reject_cells_arm(WorkbookCodecRead *read,bool present) {
    const WorkbookCodecRequest *r=workbook_codec_read_request(read);WorkbookCodecRequest old=r?*r:(WorkbookCodecRequest){0};size_t bytes=workbook_codec_read_owned_bytes(read);char *error=NULL;
    CHECK(!workbook_codec_read_add_cells(read,present,&error)&&error&&*error&&workbook_codec_read_owned_bytes(read)==bytes);CHECK(r?same_request(old,workbook_codec_read_request(read)):!workbook_codec_read_request(read));error_clear(&error);
}
static void cells_get_unchanged(const WorkbookCodecHeader *h,size_t sheet) {
    WorkbookCodecCellsView out={(const SheetCell *)(uintptr_t)1,71,73},before=out;
    CHECK(!workbook_codec_header_cells(h,sheet,&out)&&!memcmp(&out,&before,sizeof(out)));
}
static const char *stored_cell(const WorkbookCodecCellsView *v,uint64_t row,size_t col) {
    for(size_t n=0;n<v->count;n++)if(v->cells[n].row==row&&v->cells[n].col==col)return v->cells[n].text;
    return NULL;
}
static void cells_end(WorkbookCodecRead *read,size_t first,size_t retained) {
    CellsFixture *f=cells_fixture(0,0,0);WorkbookCodecBatch batch=cells_batch(f,first,0,true);char *error=NULL;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f)+retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);free(f);
}
static void test_cells_contract_and_composition(void) {
    CHECK(WORKBOOK_CODEC_COLUMNS==15&&WORKBOOK_CODEC_CELLS==16&&!workbook_codec_header_cells_ready(NULL));cells_get_unchanged(NULL,0);reject_cells_arm(NULL,true);CHECK(!workbook_codec_read_add_cells(NULL,true,NULL));
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL);reject_cells_arm(read,true);meta(read,14,0,0);reject_cells_arm(read,true);workbook_codec_read_free(read);char *error=NULL;
    for(unsigned version=12;version<=14;version++) {
        read=sort_header(version,2,NULL,LIMIT);WorkbookCodecHeader *plain=NULL;CHECK(workbook_codec_read_finish(read,&plain,&error)&&!workbook_codec_header_cells_ready(plain));cells_get_unchanged(plain,0);reject_cells_arm(read,true);workbook_codec_read_free(read);workbook_codec_header_free(plain);
        read=sort_header(version,2,NULL,LIMIT);size_t bytes=workbook_codec_read_owned_bytes(read);
        if(version==14){reject_cells_arm(read,false);CHECK(!workbook_codec_read_add_cells(read,false,NULL));}else{CHECK(workbook_codec_read_add_cells(read,false,&error)&&!workbook_codec_read_request(read)&&workbook_codec_read_owned_bytes(read)==bytes);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error)&&workbook_codec_header_cells_ready(h));WorkbookCodecCellsView v={0};CHECK(workbook_codec_header_cells(h,0,&v)&&!v.cells&&!v.count&&!v.text_bytes);CHECK(workbook_codec_header_cells(h,1,&v)&&!v.cells&&!v.count&&!v.text_bytes);cells_get_unchanged(h,2);CHECK(!workbook_codec_header_cells(h,0,NULL));workbook_codec_read_free(read);workbook_codec_header_free(h);read=sort_header(version,2,NULL,LIMIT);bytes=workbook_codec_read_owned_bytes(read);}
        CHECK(workbook_codec_read_add_cells(read,true,&error)&&!error&&workbook_codec_read_owned_bytes(read)==bytes);const WorkbookCodecRequest *r=workbook_codec_read_request(read);
        CHECK(r&&r->family==WORKBOOK_CODEC_CELLS&&r->column_count==4&&r->source_column_count==4&&r->row_limit==1280001&&!r->first_row&&!strcmp(r->sql,"SELECT sheet_id,row_id,column_id,text FROM _duxcel_workspace.cells ORDER BY sheet_id,row_id,column_id LIMIT 1280001")&&!strstr(r->sql,"OFFSET"));reject_cells_arm(read,true);reject_cells_arm(read,false);
        CellsFixture *f=cells_fixture(0,0,0);WorkbookCodecBatch batch=cells_batch(f,0,0,false);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));free(f);cells_end(read,0,0);reject_cells_arm(read,true);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error)&&workbook_codec_header_cells_ready(h));WorkbookCodecCellsView v={0};CHECK(workbook_codec_header_cells(h,0,&v)&&!v.cells&&!v.count&&!v.text_bytes);workbook_codec_read_free(read);workbook_codec_header_free(h);
    }
    for(unsigned order=0;order<2;order++) {
        const char *query="SELECT 'stored';  \n";read=definition_header(12,1,2,true,0,LIMIT,query);CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error));definition_empty(read,false,0);definition_empty(read,true,0);
        if(!order){CHECK(workbook_codec_read_add_cells(read,true,&error));CellsFixture *f=cells_fixture(0,0,1);WorkbookCodecBatch b=cells_batch(f,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&b,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);}
        CHECK(workbook_codec_read_add_columns(read,true,&error));ColumnsFixture *f=columns_fixture(0,0,1);WorkbookCodecBatch b=columns_batch(f,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&b,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);
        CHECK(workbook_codec_read_add_filters(read,false,0,&error)&&workbook_codec_read_add_sorts(read,false,&error)&&workbook_codec_read_add_preferences(read,false,&error));
        if(order){CHECK(workbook_codec_read_add_cells(read,true,&error));CellsFixture *c=cells_fixture(0,0,1);b=cells_batch(c,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&b,sizeof(*c),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(c);}
        WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);WorkbookCodecCellsView v={0};WorkbookCodecTableSourceView source={0};const char *stored=NULL;bool unique=false;const PivotDefinition *pivot=NULL;const MarkDefinition *marks=NULL;
        CHECK(workbook_codec_header_cells(h,0,&v)&&v.count==1&&v.cells[0].text&&!v.cells[0].text[0]&&v.text_bytes==1&&!stored_cell(&v,0,1));CHECK(workbook_codec_header_columns_ready(h)&&workbook_codec_header_view(h,0)->column_count==1);
        CHECK(workbook_codec_header_table_source(h,0,2,&source)&&workbook_codec_header_query(h,0,2,&stored)&&source.analysis->query==stored&&!strcmp(stored,query)&&workbook_codec_header_unique_rows(h,0,2,&unique)&&unique);
        CHECK(workbook_codec_header_pivot_definition(h,0,1,&pivot)&&pivot&&!pivot->counts[0]&&workbook_codec_header_marks_definition(h,0,1,&marks)&&marks&&!marks->counts[0]);workbook_codec_header_free(h);
    }
}
static void test_cells_splits_ownership_and_edges(void) {
    WorkbookCodecRead *read=sort_header(14,3,NULL,LIMIT);char *error=NULL;CHECK(workbook_codec_read_add_cells(read,true,&error));const size_t sizes[]={1,255,7};size_t first=0,expected_bytes=0;
    for(size_t n=0;n<3;n++){CellsFixture *f=cells_fixture(0,first,sizes[n]);for(size_t r=0;r<sizes[n];r++)expected_bytes+=f->cells[r*4+3].value.text.bytes+1;WorkbookCodecBatch b=cells_batch(f,first,sizes[n],false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&b,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);first+=sizes[n];CHECK(workbook_codec_read_request(read)->first_row==first);memset(f,0xa5,sizeof(*f));free(f);}
    CellsFixture *f=cells_fixture(2,0,1);f->cells[1]=u(DX_SHEET_ROWS-1);f->cells[2]=i(16383);WorkbookCodecBatch batch=cells_batch(f,first,1,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first++;free(f);WorkbookCodecHeader *h=NULL;CHECK(!workbook_codec_read_finish(read,&h,&error)&&!h);error_clear(&error);cells_end(read,first,0);CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);
    WorkbookCodecCellsView v={0};CHECK(workbook_codec_header_cells(h,0,&v)&&v.count==263&&v.text_bytes==expected_bytes&&v.cells[0].text&&!v.cells[0].text[0]);CHECK(stored_cell(&v,0,0)&&!*stored_cell(&v,0,0)&&!stored_cell(&v,0,1)&&!strcmp(stored_cell(&v,0,2),"NULL")&&!strcmp(stored_cell(&v,0,6),"18446744073709551615")&&!strcmp(stored_cell(&v,16,12),"9007199254740993"));
    CHECK(workbook_codec_header_cells(h,1,&v)&&!v.count&&!v.cells&&!v.text_bytes);CHECK(workbook_codec_header_cells(h,2,&v)&&v.count==1&&v.cells[0].row==DX_SHEET_ROWS-1&&v.cells[0].col==16383&&v.text_bytes==1&&v.cells[0].text&&!v.cells[0].text[0]);cells_get_unchanged(h,3);cells_get_unchanged(h,SIZE_MAX);workbook_codec_header_free(h);
}
static void test_cells_invalid_atomic_and_stale(void) {
    WorkbookCodecRead *read=sort_header(14,2,NULL,LIMIT);char *error=NULL;CHECK(workbook_codec_read_add_cells(read,true,&error));CellsFixture *f=cells_fixture(0,0,3);WorkbookCodecBatch batch=cells_batch(f,0,3,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);f=cells_fixture(0,3,3);batch=cells_batch(f,3,3,false);
    size_t bytes=workbook_codec_read_owned_bytes(read);WorkbookCodecKey stale=key;stale.operation_ticket--;error=dx_strdup("retained diagnostic");char *old=error;CHECK(workbook_codec_read_feed(read,stale,WORKBOOK_CODEC_CELLS,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE&&error==old&&workbook_codec_read_owned_bytes(read)==bytes);free(error);error=NULL;
    for(size_t c=0;c<4;c++){WorkbookCodecAtom saved=f->cells[8+c];f->cells[8+c]=(WorkbookCodecAtom){0};reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->cells[8+c]=saved;f->cells[8+c]=d(1);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->cells[8+c]=saved;WorkbookCodecAtomKind kind=f->columns[c].kind;f->columns[c].kind=WORKBOOK_CODEC_BOOLEAN;reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->columns[c].kind=kind;}
    const uint64_t rows[]={DX_SHEET_ROWS,UINT64_MAX};for(size_t n=0;n<2;n++){f->cells[9]=u(rows[n]);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));}f->cells[9]=u(0);
    const int32_t cols[]={-1,16384,INT32_MAX};for(size_t n=0;n<3;n++){f->cells[10]=i(cols[n]);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));}f->cells[10]=i(10);
    const int32_t sheets[]={-1,2,INT32_MAX};for(size_t n=0;n<3;n++){f->cells[8]=i(sheets[n]);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));}f->cells[8]=i(0);
    f->cells[10]=i(6);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->cells[10]=i(10);f->cells[2]=i(4);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->cells[2]=i(6);f->cells[9]=u(1);f->cells[5]=u(2);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->cells[9]=u(0);f->cells[5]=u(0);
    const char nul[]={'a',0,'b'};f->cells[11]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={nul,sizeof(nul)}};reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));const char *bad[]={"\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\x80","\xe2\x82"};for(size_t n=0;n<5;n++){f->cells[11]=t(bad[n]);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));}f->cells[11]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={NULL,1}};reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->cells[11]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={(const char *)(uintptr_t)1,65537}};reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->cells[11]=t("recovered");
    f->columns[3].name="wrong";reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->columns[3].name="text";batch.column_count=3;reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));batch.column_count=5;reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));batch.column_count=4;batch.row_count=257;reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));batch.row_count=3;batch.first_row=0;reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));batch.first_row=1280000;reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));batch.first_row=3;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);memset(f,0,sizeof(*f));free(f);f=cells_fixture(1,0,2);f->cells[4]=i(0);f->cells[5]=u(1);f->cells[6]=i(0);batch=cells_batch(f,6,2,true);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->cells[4]=i(1);f->cells[5]=u(0);f->cells[6]=i(2);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);WorkbookCodecCellsView v={0};CHECK(workbook_codec_header_cells(h,0,&v)&&v.count==6&&!strcmp(v.cells[5].text,"recovered")&&v.cells[0].text&&!v.cells[0].text[0]);workbook_codec_header_free(h);
}
static void test_cells_count_limits_and_eof(void) {
    WorkbookCodecRead *read=sort_header(14,64,NULL,LIMIT);char *error=NULL;CHECK(workbook_codec_read_add_cells(read,true,&error));size_t first=0;
    while(first<20000){size_t count=20000-first>256?256:20000-first;CellsFixture *f=cells_fixture(0,first,count);WorkbookCodecBatch batch=cells_batch(f,first,count,false);if(first==256){f->cells[(count-1)*4+2]=i(0);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->cells[(count-1)*4+2]=i(30);}CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=count;memset(f,0,sizeof(*f));free(f);}
    CHECK(first==20000&&workbook_codec_read_request(read)->first_row==20000);CellsFixture *f=cells_fixture(0,20000,1);WorkbookCodecBatch batch=cells_batch(f,first,1,true);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));free(f);
    f=cells_fixture(1,0,2);f->cells[4]=i(63);f->cells[5]=u(DX_SHEET_ROWS-1);f->cells[6]=i(16383);batch=cells_batch(f,first,2,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=2;free(f);f=cells_fixture(1,1,1);batch=cells_batch(f,first,1,true);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));free(f);WorkbookCodecHeader *h=NULL;CHECK(!workbook_codec_read_finish(read,&h,&error)&&!h);error_clear(&error);cells_end(read,first,0);CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);WorkbookCodecCellsView v={0};CHECK(workbook_codec_header_cells(h,0,&v)&&v.count==20000&&v.cells[19999].row==1249&&v.cells[19999].col==30);CHECK(workbook_codec_header_cells(h,63,&v)&&v.count==1&&v.cells[0].row==DX_SHEET_ROWS-1&&v.cells[0].col==16383);CHECK(workbook_codec_header_cells(h,62,&v)&&!v.cells&&!v.count&&!v.text_bytes);workbook_codec_header_free(h);
}
static void test_cells_text_and_exact_raw_limit(void) {
    char *text=dx_alloc(65538);memset(text,'x',65537);text[65537]=0;CellsFixture *f=cells_fixture(0,0,1);f->cells[3]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={text,65537}};WorkbookCodecRead *read=sort_header(14,2,NULL,LIMIT);char *error=NULL;CHECK(workbook_codec_read_add_cells(read,true,&error));WorkbookCodecBatch batch=cells_batch(f,0,1,false);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f)+65538);f->cells[3].value.text.bytes=65536;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f)+65538,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);cells_end(read,1,65538);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);WorkbookCodecCellsView v={0};CHECK(workbook_codec_header_cells(h,0,&v)&&v.count==1&&v.text_bytes==65537&&strlen(v.cells[0].text)==65536);workbook_codec_header_free(h);
    read=sort_header(14,2,NULL,LIMIT);CHECK(workbook_codec_read_add_cells(read,true,&error));f=cells_fixture(0,0,64);for(size_t n=0;n<64;n++)f->cells[n*4+3]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={text,n==63?65473:65536}};batch=cells_batch(f,0,64,false);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f)+65538);f->cells[63*4+3].value.text.bytes=65472;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f)+65538,&error)==WORKBOOK_CODEC_READ_ACCEPTED);memset(f,0,sizeof(*f));free(f);memset(text,'y',65537);free(text);
    f=cells_fixture(0,64,1);f->cells[3]=t("");batch=cells_batch(f,64,1,true);reject(read,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f));f->cells[0]=i(1);f->cells[1]=u(0);f->cells[2]=i(0);f->cells[3]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={NULL,0}};CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);
    CHECK(workbook_codec_header_cells(h,0,&v)&&v.count==64&&v.text_bytes==4u*1024u*1024u&&strlen(v.cells[0].text)==65536&&strlen(v.cells[63].text)==65472&&v.cells[63].text[65471]=='x');CHECK(workbook_codec_header_cells(h,1,&v)&&v.count==1&&v.text_bytes==1&&v.cells[0].text&&!v.cells[0].text[0]);workbook_codec_header_free(h);
}
static void test_cells_exact_peak_budget_and_retained_output(void) {
    WorkbookCodecRead *old_read=sort_header(12,1,NULL,LIMIT);char *error=NULL;CHECK(workbook_codec_read_add_cells(old_read,true,&error));CellsFixture *f=cells_fixture(0,0,2);WorkbookCodecBatch batch=cells_batch(f,0,2,true);CHECK(workbook_codec_read_feed(old_read,key,WORKBOOK_CODEC_CELLS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *old=NULL;CHECK(workbook_codec_read_finish(old_read,&old,&error));workbook_codec_read_free(old_read);WorkbookCodecHeader *identity=old;size_t old_bytes=workbook_codec_header_owned_bytes(old);WorkbookCodecCellsView view={0};CHECK(workbook_codec_header_cells(old,0,&view));const SheetCell *cells=view.cells;const char *empty=cells[0].text,*literal=cells[1].text;
    WorkbookCodecRead *read=sort_header(14,1,NULL,LIMIT);CHECK(workbook_codec_read_add_cells(read,true,&error));const size_t counts[]={1,16,1};size_t first=0,capacity=0;
    for(size_t n=0;n<3;n++){size_t count=counts[n];f=cells_fixture(0,first,count);for(size_t r=0;r<count;r++)f->cells[r*4+3]=t("n");batch=cells_batch(f,first,count,false);size_t next=capacity?capacity:16;while(next<first+count)next*=2;size_t arrays=next>capacity?next*sizeof(SheetCell):0,retired=next>capacity?capacity*sizeof(SheetCell):0;size_t extra=count*sizeof(char *)+count*2+arrays,owned=workbook_codec_read_owned_bytes(read),retained=sizeof(*f)+old_bytes;CHECK(owned+retained+extra<LIMIT);size_t pressure_bytes=LIMIT-owned-retained-extra;unsigned char *pressure=dx_alloc(pressure_bytes+1);reject(read,WORKBOOK_CODEC_CELLS,&batch,retained+pressure_bytes+1);CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity&&workbook_codec_header_owned_bytes(old)==old_bytes);error_clear(&error);WorkbookCodecCellsView current={0};CHECK(workbook_codec_header_cells(old,0,&current)&&!memcmp(&current,&view,sizeof(view))&&current.cells==cells&&current.cells[0].text==empty&&current.cells[1].text==literal&&!empty[0]&&!strcmp(literal,"NULL"));CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(read)==owned+count*2+arrays-retired);free(pressure);memset(f,0,sizeof(*f));free(f);first+=count;capacity=next;}
    cells_end(read,first,old_bytes);CHECK(workbook_codec_read_finish(read,&old,&error)&&old!=identity);workbook_codec_read_free(read);CHECK(workbook_codec_header_cells(old,0,&view)&&view.count==18&&view.text_bytes==36&&!strcmp(view.cells[17].text,"n")&&view.cells[0].text!=view.cells[17].text);workbook_codec_header_free(old);
    /* Budget admission precedes content reads even for a bounded64KiB cell.
       The unreadable placeholder is only an ordering guard, not valid input. */
    read=sort_header(14,1,NULL,LIMIT);CHECK(workbook_codec_read_add_cells(read,true,&error));f=cells_fixture(0,0,1);f->cells[3]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={(const char *)(uintptr_t)1,65536}};batch=cells_batch(f,0,1,true);size_t owned=workbook_codec_read_owned_bytes(read),extra=sizeof(char *)+16*sizeof(SheetCell)+65537,retained=sizeof(*f);size_t pressure_bytes=LIMIT-owned-retained-extra;unsigned char *pressure=dx_alloc(pressure_bytes+1);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&batch,retained+pressure_bytes+1,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"budget")&&workbook_codec_read_owned_bytes(read)==owned&&!workbook_codec_read_request(read)->first_row);error_clear(&error);free(pressure);free(f);workbook_codec_read_free(read);
}
/* END compact stored-cell continuation tests. */
/* BEGIN optional object-counter continuation tests. */
typedef struct { WorkbookCodecColumn columns[2]; WorkbookCodecAtom cells[65*2]; } CounterFixture;
static CounterFixture counter_fixture(size_t count) {
    CounterFixture f={0}; f.columns[0]=(WorkbookCodecColumn){"sheet_id",WORKBOOK_CODEC_I32};
    f.columns[1]=(WorkbookCodecColumn){"high_water",WORKBOOK_CODEC_U64};
    for (size_t row=0;row<count;row++) { f.cells[row*2]=i((int32_t)row); f.cells[row*2+1]=u(0); }
    return f;
}
static WorkbookCodecBatch counter_batch(CounterFixture *f,size_t first,size_t count,bool end) {
    return (WorkbookCodecBatch){f->columns,2,f->cells+first*2,count,first,end};
}
static WorkbookCodecRead *counter_header(unsigned version,size_t sheets,const uint64_t *ids,size_t limit) {
    WorkbookCodecRead *read=sort_header(version,sheets,NULL,limit); char *error=NULL;
    size_t total=0; for (size_t n=0;n<sheets;n++) if (ids && ids[n]) total++;
    ObjectFixture *f=object_fixture(total);
    for (size_t sheet=0,row=0;sheet<sheets;sheet++) if (ids && ids[sheet]) {
        WorkbookCodecAtom *c=f->cells+row*11; c[0]=i((int32_t)sheet); c[1]=u(ids[sheet]);
        c[7]=i((int32_t)(sheet%3)); row++;
    }
    CHECK(workbook_codec_read_add_objects(read,true,&error)); WorkbookCodecBatch batch=object_batch(f,0,total,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    free(f); return read;
}
static void reject_counters_arm(WorkbookCodecRead *read,bool present) {
    const WorkbookCodecRequest *r=workbook_codec_read_request(read); WorkbookCodecRequest old=r?*r:(WorkbookCodecRequest){0};
    size_t bytes=workbook_codec_read_owned_bytes(read); char *error=NULL;
    CHECK(!workbook_codec_read_add_object_counters(read,present,&error)&&error&&*error&&workbook_codec_read_owned_bytes(read)==bytes);
    CHECK(r?same_request(old,workbook_codec_read_request(read)):!workbook_codec_read_request(read)); error_clear(&error);
}
static void counter_get_unchanged(const WorkbookCodecHeader *h,size_t sheet) {
    uint64_t value=UINT64_C(9007199254740997);
    CHECK(!workbook_codec_header_object_high_water(h,sheet,&value)&&value==UINT64_C(9007199254740997));
}
static void test_object_counters_contract_and_defaults(void) {
    CHECK(WORKBOOK_CODEC_OBJECT_COUNTERS==17&&WORKBOOK_CODEC_CELLS==16&&!workbook_codec_header_object_counters_ready(NULL));
    counter_get_unchanged(NULL,0); reject_counters_arm(NULL,true); CHECK(!workbook_codec_read_add_object_counters(NULL,true,NULL));
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); reject_counters_arm(read,true);
    meta(read,14,0,0); reject_counters_arm(read,true); workbook_codec_read_free(read); char *error=NULL;
    const uint64_t ids[]={0,1,UINT64_C(9007199254740993),UINT64_MAX};
    for (unsigned version=12;version<=14;version++) {
        read=sort_header(version,1,NULL,LIMIT); reject_counters_arm(read,true); reject_counters_arm(read,false); workbook_codec_read_free(read);
        read=counter_header(version,4,ids,LIMIT); WorkbookCodecHeader *plain=NULL;
        CHECK(workbook_codec_read_finish(read,&plain,&error)&&!workbook_codec_header_object_counters_ready(plain));
        counter_get_unchanged(plain,0); reject_counters_arm(read,version!=12); workbook_codec_read_free(read); workbook_codec_header_free(plain);
        read=counter_header(version,4,ids,LIMIT); size_t bytes=workbook_codec_read_owned_bytes(read);
        if (version==12) {
            reject_counters_arm(read,true); CHECK(!workbook_codec_read_add_object_counters(read,true,NULL));
            CHECK(workbook_codec_read_add_object_counters(read,false,&error)&&!error&&!workbook_codec_read_request(read)&&workbook_codec_read_owned_bytes(read)==bytes);
        } else {
            reject_counters_arm(read,false); CHECK(!workbook_codec_read_add_object_counters(read,false,NULL));
            CHECK(workbook_codec_read_add_object_counters(read,true,&error)&&!error&&workbook_codec_read_owned_bytes(read)==bytes);
            const WorkbookCodecRequest *r=workbook_codec_read_request(read);
            CHECK(r&&r->family==WORKBOOK_CODEC_OBJECT_COUNTERS&&r->column_count==2&&r->source_column_count==2&&r->row_limit==65&&!r->first_row&&
                !strcmp(r->sql,"SELECT sheet_id,high_water FROM _duxcel_workspace.object_counters ORDER BY sheet_id LIMIT 65")&&!strstr(r->sql,"OFFSET"));
            reject_counters_arm(read,true); reject_counters_arm(read,false);
            CounterFixture f=counter_fixture(4); for (size_t n=0;n<4;n++) f.cells[n*2+1]=u(ids[n]); WorkbookCodecBatch batch=counter_batch(&f,0,4,true);
            CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(read)==bytes);
            memset(&f,0xa5,sizeof(f));
        }
        reject_counters_arm(read,true); WorkbookCodecHeader *h=NULL;
        CHECK(workbook_codec_read_finish(read,&h,&error)&&workbook_codec_header_object_counters_ready(h)); workbook_codec_read_free(read);
        for (size_t n=0;n<4;n++) { uint64_t value=7; CHECK(workbook_codec_header_object_high_water(h,n,&value)&&value==ids[n]); }
        counter_get_unchanged(h,4); counter_get_unchanged(h,SIZE_MAX); CHECK(!workbook_codec_header_object_high_water(h,0,NULL)); workbook_codec_header_free(h);
    }
    read=sort_header(12,2,NULL,LIMIT); CHECK(workbook_codec_read_add_objects(read,false,&error));
    size_t bytes=workbook_codec_read_owned_bytes(read); CHECK(workbook_codec_read_add_object_counters(read,false,&error)&&!workbook_codec_read_request(read)&&bytes==workbook_codec_read_owned_bytes(read));
    WorkbookCodecHeader *h=NULL; CHECK(workbook_codec_read_finish(read,&h,&error)); workbook_codec_read_free(read);
    for (size_t n=0;n<2;n++) { uint64_t value=UINT64_MAX; CHECK(workbook_codec_header_object_high_water(h,n,&value)&&value==0); } workbook_codec_header_free(h);
}
static void test_object_counters_exact_values_and_eof(void) {
    uint64_t ids[64],expected[64];
    for (size_t n=0;n<64;n++) { ids[n]=n%4==0?0:n%4==1?1:n%4==2?UINT64_C(9007199254740993):UINT64_MAX;
        expected[n]=n==0?0:n%4==0?UINT64_C(9007199254740997):ids[n]; }
    for (unsigned version=13;version<=14;version++) for (unsigned split=0;split<2;split++) {
        WorkbookCodecRead *read=counter_header(version,64,ids,LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_object_counters(read,true,&error));
        CounterFixture f=counter_fixture(65); for (size_t n=0;n<64;n++) f.cells[n*2+1]=u(expected[n]);
        WorkbookCodecBatch batch=counter_batch(&f,0,65,true); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f));
        batch=counter_batch(&f,0,split?1:64,false); size_t first=batch.row_count,bytes=workbook_codec_read_owned_bytes(read);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
        CHECK(workbook_codec_read_owned_bytes(read)==bytes&&workbook_codec_read_request(read)->first_row==first);
        WorkbookCodecHeader *h=NULL; CHECK(!workbook_codec_read_finish(read,&h,&error)&&!h); error_clear(&error);
        if (split) { batch=counter_batch(&f,first,63,false); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error); }
        batch=counter_batch(&f,64,1,true); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f));
        batch=counter_batch(&f,64,0,false); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f));
        batch=counter_batch(&f,64,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(read)==bytes);
        CHECK(workbook_codec_read_finish(read,&h,&error)&&workbook_codec_header_object_counters_ready(h)); workbook_codec_read_free(read); memset(&f,0xa5,sizeof(f));
        for (size_t n=0;n<64;n++) { uint64_t value=0; CHECK(workbook_codec_header_object_high_water(h,n,&value)&&value==expected[n]); }
        workbook_codec_header_free(h);
    }
}
static void test_object_counters_invalid_atomic_and_stale(void) {
    const uint64_t ids[]={1,UINT64_C(9007199254740993),UINT64_MAX}; char *error=NULL;
    WorkbookCodecRead *read=counter_header(14,3,ids,LIMIT); CHECK(workbook_codec_read_add_object_counters(read,true,&error));
    CounterFixture f=counter_fixture(3); for (size_t n=0;n<3;n++) f.cells[n*2+1]=u(ids[n]);
    WorkbookCodecBatch batch=counter_batch(&f,0,3,true); WorkbookCodecKey stale=key; char *prior=dx_strdup("keep diagnostic");
    for (unsigned field=0;field<3;field++) {
        stale=key; if (!field) stale.document_epoch--; else if (field==1) stale.operation_ticket--; else stale.snapshot_generation--;
        WorkbookCodecRequest before=*workbook_codec_read_request(read); size_t bytes=workbook_codec_read_owned_bytes(read);
        CHECK(workbook_codec_read_feed(read,stale,WORKBOOK_CODEC_OBJECT_COUNTERS,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&prior)==WORKBOOK_CODEC_READ_STALE&&
            !strcmp(prior,"keep diagnostic")&&same_request(before,workbook_codec_read_request(read))&&workbook_codec_read_owned_bytes(read)==bytes);
    }
    free(prior); reject(read,WORKBOOK_CODEC_COLUMNS,&batch,sizeof(f)); batch.first_row=1; reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f)); batch.first_row=0;
    for (size_t row=0;row<3;row++) for (size_t col=0;col<2;col++) {
        WorkbookCodecAtom saved=f.cells[row*2+col]; f.cells[row*2+col]=(WorkbookCodecAtom){0}; reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f));
        f.cells[row*2+col]=col?d(1):u(row); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f)); f.cells[row*2+col]=saved;
    }
    for (size_t col=0;col<2;col++) {
        WorkbookCodecColumn saved=f.columns[col]; f.columns[col].name="wrong"; reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f));
        f.columns[col]=saved; f.columns[col].kind=WORKBOOK_CODEC_DOUBLE; reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f)); f.columns[col]=saved;
    }
    batch.column_count=1; reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f)); batch.column_count=3; reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f)); batch.column_count=2;
    for (size_t row=0;row<3;row++) {
        WorkbookCodecAtom saved=f.cells[row*2]; const int32_t bad[]={-1,3,(int32_t)(row?row-1:1)};
        for (size_t k=0;k<3;k++) { f.cells[row*2]=i(bad[k]); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f)); } f.cells[row*2]=saved;
        f.cells[row*2+1]=u(ids[row]-1); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f)); f.cells[row*2+1]=u(ids[row]);
    }
    batch=counter_batch(&f,0,0,true); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f));
    /* A valid prefix inside an invalid/premature-EOF batch must not advance the
       cursor or permit finish. Corrected input is accepted at the same row. */
    batch=counter_batch(&f,0,1,true); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f));
    batch.end=false; CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    batch=counter_batch(&f,1,1,true); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f));
    batch=counter_batch(&f,1,2,true); f.cells[5]=u(UINT64_MAX-1u); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f));
    WorkbookCodecHeader *h=NULL; CHECK(!workbook_codec_read_finish(read,&h,&error)&&!h); error_clear(&error);
    f.cells[3]=u(UINT64_C(9007199254740999)); f.cells[5]=u(UINT64_MAX);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_finish(read,&h,&error)); workbook_codec_read_free(read);
    const uint64_t expected[]={1,UINT64_C(9007199254740999),UINT64_MAX};
    for (size_t n=0;n<3;n++) { uint64_t value=0; CHECK(workbook_codec_header_object_high_water(h,n,&value)&&value==expected[n]); }
    workbook_codec_header_free(h);
}
static void test_object_counters_budget_and_retained_output(void) {
    char *error=NULL; const uint64_t ids[]={0,UINT64_MAX};
    WorkbookCodecRead *read=counter_header(12,2,ids,LIMIT); CHECK(workbook_codec_read_add_object_counters(read,false,&error));
    WorkbookCodecHeader *old=NULL; CHECK(workbook_codec_read_finish(read,&old,&error)); workbook_codec_read_free(read);
    WorkbookCodecHeader *identity=old; WorkbookCodecObjectView object={0}; CHECK(workbook_codec_header_object_at(old,1,0,&object));
    const char *name=object.name; size_t old_bytes=workbook_codec_header_owned_bytes(old);
    read=counter_header(14,2,ids,LIMIT); CHECK(workbook_codec_read_add_object_counters(read,true,&error));
    CounterFixture f=counter_fixture(2); f.cells[1]=u(UINT64_C(9007199254740995)); f.cells[3]=u(UINT64_MAX);
    WorkbookCodecBatch batch=counter_batch(&f,0,1,false); size_t owned=workbook_codec_read_owned_bytes(read),retained=sizeof(f)+old_bytes;
    CHECK(owned<LIMIT&&retained<LIMIT-owned); size_t pressure_bytes=LIMIT-owned-retained; unsigned char *pressure=dx_alloc(pressure_bytes+1u);
    reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,retained+pressure_bytes+1u); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,SIZE_MAX);
    CHECK(old==identity&&workbook_codec_header_owned_bytes(old)==old_bytes&&workbook_codec_header_object_at(old,1,0,&object)&&object.name==name&&!strcmp(name,"Object 'İ' 0"));
    uint64_t value=1; CHECK(workbook_codec_header_object_high_water(old,0,&value)&&value==0); CHECK(workbook_codec_header_object_high_water(old,1,&value)&&value==UINT64_MAX);
    CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity); error_clear(&error);
    free(pressure); pressure=dx_alloc(pressure_bytes);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(read)==owned);
    free(pressure); pressure=dx_alloc(pressure_bytes+1u);
    batch=counter_batch(&f,1,1,true); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,retained+pressure_bytes+1u);
    CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity); error_clear(&error);
    free(pressure); pressure=dx_alloc(pressure_bytes);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(read)==owned);
    memset(&f,0xa5,sizeof(f)); free(pressure); WorkbookCodecHeader *next=NULL;
    CHECK(workbook_codec_read_finish(read,&next,&error)&&next!=old); workbook_codec_read_free(read);
    CHECK(workbook_codec_header_object_high_water(next,0,&value)&&value==UINT64_C(9007199254740995));
    CHECK(workbook_codec_header_object_high_water(next,1,&value)&&value==UINT64_MAX);
    CHECK(workbook_codec_header_object_high_water(old,0,&value)&&value==0); workbook_codec_header_free(next); workbook_codec_header_free(old);
}
static void test_object_counters_composition(void) {
    char *error=NULL;
    for (unsigned order=0;order<2;order++) {
        WorkbookCodecRead *read=query_header(13,2,1,SOURCE_NONE,true,LIMIT);
        CounterFixture f=counter_fixture(2); f.cells[1]=u(UINT64_C(9007199254740993)); f.cells[3]=u(UINT64_MAX);
        WorkbookCodecBatch batch=counter_batch(&f,0,2,true);
        if (!order) { CHECK(workbook_codec_read_add_object_counters(read,true,&error)); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); }
        CHECK(workbook_codec_read_add_queries(read,true,&error)); QueryFixture *q=query_fixture(0); WorkbookCodecBatch empty=query_batch(q,0,0,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&empty,sizeof(*q),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(q);
        CHECK(workbook_codec_read_add_source_options(read,true,&error)); QueryFixture *o=source_options_fixture(0); empty=query_batch(o,0,0,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&empty,sizeof(*o),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(o);
        CHECK(workbook_codec_read_add_columns(read,true,&error)); ColumnsFixture *c=columns_fixture(0,0,1); empty=columns_batch(c,0,1,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&empty,sizeof(*c),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(c);
        CHECK(workbook_codec_read_add_cells(read,false,&error)&&workbook_codec_read_add_preferences(read,false,&error)&&workbook_codec_read_add_sorts(read,false,&error));
        if (order) { CHECK(workbook_codec_read_add_object_counters(read,true,&error)); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED); }
        WorkbookCodecHeader *h=NULL; CHECK(workbook_codec_read_finish(read,&h,&error)&&!error); workbook_codec_read_free(read);
        CHECK(workbook_codec_header_queries_ready(h)&&workbook_codec_header_source_options_ready(h)&&workbook_codec_header_columns_ready(h)&&workbook_codec_header_cells_ready(h)&&workbook_codec_header_object_counters_ready(h));
        uint64_t value=0; CHECK(workbook_codec_header_object_high_water(h,0,&value)&&value==UINT64_C(9007199254740993)); CHECK(workbook_codec_header_object_high_water(h,1,&value)&&value==UINT64_MAX);
        const char *query="preserved"; CHECK(workbook_codec_header_query(h,0,0,&query)&&!query); bool unique=true; CHECK(workbook_codec_header_unique_rows(h,1,1,&unique)&&!unique);
        CHECK(workbook_codec_header_view(h,0)->column_count==1&&!strcmp(workbook_codec_header_view(h,0)->columns[0],"Column 'İ' 0:0")); workbook_codec_header_free(h);
    }
}
/* END optional object-counter continuation tests. */
/* BEGIN source-version counter continuation tests. */
static WorkbookCodecRead *source_counter_header(unsigned version,size_t sheets,const uint64_t *ids,size_t limit) {
    WorkbookCodecRead *read=counter_header(version,sheets,ids,limit); char *error=NULL;
    CHECK(workbook_codec_read_add_object_counters(read,version>=13,&error)&&!error);
    if (version>=13) {
        CounterFixture f=counter_fixture(sheets); for (size_t n=0;n<sheets;n++) f.cells[n*2+1]=u(ids?ids[n]:0);
        WorkbookCodecBatch batch=counter_batch(&f,0,sheets,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    }
    return read;
}
static void reject_source_counters_arm(WorkbookCodecRead *read,bool present) {
    const WorkbookCodecRequest *r=workbook_codec_read_request(read); WorkbookCodecRequest before=r?*r:(WorkbookCodecRequest){0};
    size_t bytes=workbook_codec_read_owned_bytes(read); char *error=NULL;
    CHECK(!workbook_codec_read_add_source_version_counters(read,present,&error)&&error&&*error&&workbook_codec_read_owned_bytes(read)==bytes);
    CHECK(r?same_request(before,workbook_codec_read_request(read)):!workbook_codec_read_request(read)); error_clear(&error);
}
static void source_counter_get_unchanged(const WorkbookCodecHeader *h,size_t sheet) {
    uint64_t value=UINT64_C(9007199254740999);
    CHECK(!workbook_codec_header_source_version_high_water(h,sheet,&value)&&value==UINT64_C(9007199254740999));
}
static void test_source_version_counters_contract_and_defaults(void) {
    CHECK(WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS==18&&WORKBOOK_CODEC_OBJECT_COUNTERS==17&&WORKBOOK_CODEC_CELLS==16);
    CHECK(!workbook_codec_header_source_version_counters_ready(NULL)); source_counter_get_unchanged(NULL,0);
    reject_source_counters_arm(NULL,true); CHECK(!workbook_codec_read_add_source_version_counters(NULL,true,NULL));
    WorkbookCodecRead *read=begin(14,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL); reject_source_counters_arm(read,true);
    meta(read,14,0,0); reject_source_counters_arm(read,true); workbook_codec_read_free(read);
    const uint64_t ids[]={0,1,UINT64_C(9007199254740993),UINT64_MAX}; char *error=NULL;
    for (unsigned version=12;version<=14;version++) {
        read=sort_header(version,1,NULL,LIMIT); reject_source_counters_arm(read,true); reject_source_counters_arm(read,false); workbook_codec_read_free(read);
        read=counter_header(version,4,ids,LIMIT); reject_source_counters_arm(read,true); reject_source_counters_arm(read,false); workbook_codec_read_free(read);
        read=source_counter_header(version,4,ids,LIMIT); WorkbookCodecHeader *plain=NULL;
        CHECK(workbook_codec_read_finish(read,&plain,&error)&&!workbook_codec_header_source_version_counters_ready(plain));
        source_counter_get_unchanged(plain,0); reject_source_counters_arm(read,true); workbook_codec_read_free(read); workbook_codec_header_free(plain);
        read=source_counter_header(version,4,ids,LIMIT); size_t bytes=workbook_codec_read_owned_bytes(read);
        if (version<14) {
            reject_source_counters_arm(read,true); CHECK(!workbook_codec_read_add_source_version_counters(read,true,NULL));
            CHECK(workbook_codec_read_add_source_version_counters(read,false,&error)&&!error&&!workbook_codec_read_request(read));
        } else {
            reject_source_counters_arm(read,false); CHECK(!workbook_codec_read_add_source_version_counters(read,false,NULL));
            CHECK(workbook_codec_read_add_source_version_counters(read,true,&error)&&!error);
            const WorkbookCodecRequest *r=workbook_codec_read_request(read);
            CHECK(r&&r->family==WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS&&r->column_count==2&&r->source_column_count==2&&r->row_limit==65&&!r->first_row&&
                !strcmp(r->sql,"SELECT sheet_id,high_water FROM _duxcel_workspace.source_version_counters ORDER BY sheet_id LIMIT 65")&&!strstr(r->sql,"OFFSET"));
            reject_source_counters_arm(read,true); reject_source_counters_arm(read,false);
            CounterFixture f=counter_fixture(4); WorkbookCodecBatch batch=counter_batch(&f,0,4,true);
            CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f),NULL)==WORKBOOK_CODEC_READ_ACCEPTED);
        }
        CHECK(workbook_codec_read_owned_bytes(read)==bytes); reject_source_counters_arm(read,false); reject_source_counters_arm(read,true);
        WorkbookCodecHeader *h=NULL; CHECK(workbook_codec_read_finish(read,&h,&error)&&!error&&workbook_codec_header_source_version_counters_ready(h)); workbook_codec_read_free(read);
        for (size_t n=0;n<4;n++) { uint64_t value=UINT64_MAX; CHECK(workbook_codec_header_source_version_high_water(h,n,&value)&&value==0);
            CHECK(workbook_codec_header_object_high_water(h,n,&value)&&value==ids[n]); CHECK(workbook_codec_header_analysis(h,n)->source_version==0); }
        source_counter_get_unchanged(h,4); source_counter_get_unchanged(h,SIZE_MAX); CHECK(!workbook_codec_header_source_version_high_water(h,0,NULL)); workbook_codec_header_free(h);
    }
}
static void test_source_version_counters_values_and_eof(void) {
    uint64_t ids[64],expected[64];
    for (size_t n=0;n<64;n++) { ids[n]=n%2?UINT64_MAX:0;
        expected[n]=n%4==0?UINT64_MAX:n%4==1?0:n%4==2?UINT64_C(9007199254740993):1; }
    for (unsigned split=0;split<3;split++) {
        WorkbookCodecRead *read=source_counter_header(14,64,ids,LIMIT); char *error=NULL;
        CHECK(workbook_codec_read_add_source_version_counters(read,true,&error)); CounterFixture f=counter_fixture(65);
        for (size_t n=0;n<64;n++) f.cells[n*2+1]=u(expected[n]); WorkbookCodecBatch batch=counter_batch(&f,0,65,true);
        reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f)); size_t owned=workbook_codec_read_owned_bytes(read);
        size_t first=split==1?1:split==2?31:64; batch=counter_batch(&f,0,first,false);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(read)==owned);
        WorkbookCodecHeader *h=NULL; CHECK(!workbook_codec_read_finish(read,&h,&error)&&!h); error_clear(&error);
        if (first<64) { batch=counter_batch(&f,first,64-first,false); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error); }
        batch=counter_batch(&f,64,1,true); reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f));
        batch=counter_batch(&f,64,0,false); reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f));
        batch=counter_batch(&f,64,0,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
        memset(&f,0xa5,sizeof(f)); CHECK(workbook_codec_read_finish(read,&h,&error)&&workbook_codec_header_source_version_counters_ready(h)); workbook_codec_read_free(read);
        for (size_t n=0;n<64;n++) { uint64_t value=5; CHECK(workbook_codec_header_source_version_high_water(h,n,&value)&&value==expected[n]);
            CHECK(workbook_codec_header_object_high_water(h,n,&value)&&value==ids[n]); }
        workbook_codec_header_free(h);
    }
    WorkbookCodecRead *read=source_counter_header(14,1,NULL,LIMIT); char *error=NULL; CHECK(workbook_codec_read_add_source_version_counters(read,true,&error));
    CounterFixture f=counter_fixture(1); f.cells[1]=u(UINT64_MAX); WorkbookCodecBatch batch=counter_batch(&f,0,1,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    WorkbookCodecHeader *h=NULL; CHECK(workbook_codec_read_finish(read,&h,&error)); workbook_codec_read_free(read); uint64_t value=0;
    CHECK(workbook_codec_header_source_version_high_water(h,0,&value)&&value==UINT64_MAX); workbook_codec_header_free(h);
}
static void test_source_version_counters_invalid_atomic_and_stale(void) {
    const uint64_t ids[]={UINT64_MAX,0,1}; char *error=NULL; WorkbookCodecRead *read=source_counter_header(14,3,ids,LIMIT);
    CHECK(workbook_codec_read_add_source_version_counters(read,true,&error)); CounterFixture f=counter_fixture(3); f.cells[1]=u(1); f.cells[3]=u(UINT64_MAX); f.cells[5]=u(0);
    WorkbookCodecBatch batch=counter_batch(&f,0,3,true); char *diagnostic=dx_strdup("stale diagnostic");
    for (unsigned part=0;part<3;part++) {
        WorkbookCodecKey stale=key; if (!part) stale.document_epoch--; else if (part==1) stale.operation_ticket--; else stale.snapshot_generation--;
        WorkbookCodecRequest before=*workbook_codec_read_request(read); size_t owned=workbook_codec_read_owned_bytes(read);
        CHECK(workbook_codec_read_feed(read,stale,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&diagnostic)==WORKBOOK_CODEC_READ_STALE&&
            !strcmp(diagnostic,"stale diagnostic")&&same_request(before,workbook_codec_read_request(read))&&owned==workbook_codec_read_owned_bytes(read));
    }
    free(diagnostic); reject(read,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f)); batch.first_row=1; reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f)); batch.first_row=0;
    for (size_t row=0;row<3;row++) for (size_t col=0;col<2;col++) {
        WorkbookCodecAtom saved=f.cells[row*2+col]; f.cells[row*2+col]=(WorkbookCodecAtom){0}; reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f));
        f.cells[row*2+col]=col?d(0):u(row); reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f)); f.cells[row*2+col]=saved;
    }
    for (size_t col=0;col<2;col++) {
        WorkbookCodecColumn saved=f.columns[col]; f.columns[col].name="wrong"; reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f));
        f.columns[col]=saved; f.columns[col].kind=WORKBOOK_CODEC_DOUBLE; reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f)); f.columns[col]=saved;
    }
    batch.column_count=1; reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f)); batch.column_count=3; reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f)); batch.column_count=2;
    batch.row_count=257; reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f)); batch.row_count=3;
    batch.cells=NULL; reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f)); batch.cells=f.cells;
    batch.columns=NULL; reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f)); batch.columns=f.columns;
    for (size_t row=0;row<3;row++) {
        WorkbookCodecAtom saved=f.cells[row*2]; const int32_t bad[]={-1,3,(int32_t)(row?row-1:1)};
        for (size_t n=0;n<3;n++) { f.cells[row*2]=i(bad[n]); reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f)); } f.cells[row*2]=saved;
    }
    batch=counter_batch(&f,0,0,true); reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f));
    batch=counter_batch(&f,0,1,true); reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f));
    batch.end=false; CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    batch=counter_batch(&f,1,1,true); reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f));
    batch=counter_batch(&f,1,2,true); f.cells[4]=i(1); reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f));
    f.cells[4]=i(2); f.cells[3]=u(UINT64_C(9007199254740999)); f.cells[5]=u(UINT64_MAX);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    WorkbookCodecHeader *h=NULL; CHECK(workbook_codec_read_finish(read,&h,&error)); workbook_codec_read_free(read); memset(&f,0xa5,sizeof(f));
    const uint64_t expected[]={1,UINT64_C(9007199254740999),UINT64_MAX};
    for (size_t n=0;n<3;n++) { uint64_t value=0; CHECK(workbook_codec_header_source_version_high_water(h,n,&value)&&value==expected[n]); } workbook_codec_header_free(h);
}
static void test_source_version_counters_budget_and_old_output(void) {
    const uint64_t ids[]={UINT64_MAX,0}; char *error=NULL; WorkbookCodecRead *read=source_counter_header(14,2,ids,LIMIT);
    CHECK(workbook_codec_read_add_source_version_counters(read,true,&error)); CounterFixture f=counter_fixture(2); f.cells[1]=u(UINT64_MAX);
    WorkbookCodecBatch batch=counter_batch(&f,0,2,true); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    WorkbookCodecHeader *old=NULL; CHECK(workbook_codec_read_finish(read,&old,&error)); workbook_codec_read_free(read); WorkbookCodecHeader *identity=old;
    const Analysis *analysis=workbook_codec_header_analysis(old,0); const SheetView *view=workbook_codec_header_view(old,0);
    WorkbookCodecObjectView object={0}; CHECK(workbook_codec_header_object_at(old,0,0,&object)); const char *name=object.name,*path=analysis->path;
    size_t old_bytes=workbook_codec_header_owned_bytes(old); read=source_counter_header(14,2,ids,LIMIT); CHECK(workbook_codec_read_add_source_version_counters(read,true,&error));
    f=counter_fixture(2); f.cells[3]=u(UINT64_MAX); batch=counter_batch(&f,0,1,false); size_t owned=workbook_codec_read_owned_bytes(read),retained=old_bytes+sizeof(f);
    CHECK(owned<LIMIT&&retained<LIMIT-owned); size_t pressure_bytes=LIMIT-owned-retained; unsigned char *pressure=dx_alloc(pressure_bytes+1u);
    reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,retained+pressure_bytes+1u); reject(read,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,SIZE_MAX);
    CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity); error_clear(&error);
    CHECK(old_bytes==workbook_codec_header_owned_bytes(old)&&workbook_codec_header_analysis(old,0)==analysis&&workbook_codec_header_view(old,0)==view&&analysis->path==path);
    CHECK(workbook_codec_header_object_at(old,0,0,&object)&&object.name==name&&!strcmp(name,"Object 'İ' 0")); uint64_t value=0;
    CHECK(workbook_codec_header_source_version_high_water(old,0,&value)&&value==UINT64_MAX); CHECK(workbook_codec_header_source_version_high_water(old,1,&value)&&value==0);
    free(pressure); pressure=dx_alloc(pressure_bytes);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(read)==owned);
    free(pressure); pressure=dx_alloc(pressure_bytes+1u); batch=counter_batch(&f,1,1,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,retained+pressure_bytes+1u,NULL)==WORKBOOK_CODEC_READ_ERROR);
    CHECK(workbook_codec_read_request(read)->first_row==1&&workbook_codec_read_owned_bytes(read)==owned);
    CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity); error_clear(&error); free(pressure); pressure=dx_alloc(pressure_bytes);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(read)==owned);
    free(pressure); memset(&f,0xa5,sizeof(f)); WorkbookCodecHeader *next=NULL; CHECK(workbook_codec_read_finish(read,&next,&error)&&next!=old); workbook_codec_read_free(read);
    CHECK(workbook_codec_header_source_version_high_water(next,0,&value)&&value==0); CHECK(workbook_codec_header_source_version_high_water(next,1,&value)&&value==UINT64_MAX);
    CHECK(workbook_codec_header_source_version_high_water(old,0,&value)&&value==UINT64_MAX); CHECK(workbook_codec_header_analysis(old,0)==analysis&&analysis->path==path&&object.name==name);
    printf("INFO: source-counter retained pressure limit=%zu reader=%zu old_header=%zu borrowed=%zu exact_pressure=%zu; exact accepted/+1 denied\n",(size_t)LIMIT,owned,old_bytes,sizeof(f),pressure_bytes);
    workbook_codec_header_free(next); workbook_codec_header_free(old);
}
static void test_source_version_counters_composition(void) {
    for (unsigned order=0;order<2;order++) {
        char query[]="SELECT 'query remains owned';  \n",*error=NULL;
        WorkbookCodecRead *read=definition_header(14,2,2,true,0,LIMIT,query); CounterFixture counters=counter_fixture(2);
        counters.cells[1]=u(2); counters.cells[3]=u(UINT64_MAX); WorkbookCodecBatch batch=counter_batch(&counters,0,2,true);
        CHECK(workbook_codec_read_add_object_counters(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(counters),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
        counters.cells[1]=u(UINT64_MAX); counters.cells[3]=u(0);
        if (!order) { CHECK(workbook_codec_read_add_source_version_counters(read,true,&error)); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(counters),&error)==WORKBOOK_CODEC_READ_ACCEPTED); }
        CHECK(workbook_codec_read_add_pivot_marks(read,(WorkbookCodecPivotMarksPresence){true,true},&error)); definition_empty(read,false,0); definition_empty(read,true,0);
        CHECK(workbook_codec_read_add_columns(read,true,&error)); ColumnsFixture *columns=columns_fixture(0,0,1); WorkbookCodecBatch data=columns_batch(columns,0,1,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_COLUMNS,&data,sizeof(*columns),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(columns);
        CHECK(workbook_codec_read_add_cells(read,true,&error)); CellsFixture *cells=cells_fixture(0,0,1); data=cells_batch(cells,0,1,true);
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_CELLS,&data,sizeof(*cells),&error)==WORKBOOK_CODEC_READ_ACCEPTED); free(cells);
        if (order) { CHECK(workbook_codec_read_add_source_version_counters(read,true,&error)); CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(counters),&error)==WORKBOOK_CODEC_READ_ACCEPTED); }
        WorkbookCodecHeader *h=NULL; CHECK(workbook_codec_read_finish(read,&h,&error)&&!error); workbook_codec_read_free(read); memset(query,'x',strlen(query));
        CHECK(workbook_codec_header_source_version_counters_ready(h)&&workbook_codec_header_object_counters_ready(h)&&workbook_codec_header_queries_ready(h)&&workbook_codec_header_source_options_ready(h)&&workbook_codec_header_table_sources_ready(h)&&workbook_codec_header_pivot_marks_ready(h)&&workbook_codec_header_columns_ready(h)&&workbook_codec_header_cells_ready(h));
        for (size_t sheet=0;sheet<2;sheet++) { uint64_t value=9; CHECK(workbook_codec_header_source_version_high_water(h,sheet,&value)&&value==(sheet?0:UINT64_MAX));
            CHECK(workbook_codec_header_object_high_water(h,sheet,&value)&&value==(sheet?UINT64_MAX:2)); WorkbookCodecTableSourceView table={0};
            CHECK(workbook_codec_header_table_source(h,sheet,2,&table)&&table.analysis->source_version==0&&table.analysis->unique_rows&&!strcmp(table.analysis->query,"SELECT 'query remains owned';  \n"));
            const char *q=NULL; CHECK(workbook_codec_header_query(h,sheet,2,&q)&&q==table.analysis->query); CHECK(workbook_codec_header_analysis(h,sheet)->source_version==0);
            const PivotDefinition *pivot=NULL; const MarkDefinition *marks=NULL; CHECK(workbook_codec_header_pivot_definition(h,sheet,1,&pivot)&&pivot&&workbook_codec_header_marks_definition(h,sheet,1,&marks)&&marks); }
        CHECK(workbook_codec_header_view(h,0)->column_count==1&&!strcmp(workbook_codec_header_view(h,0)->columns[0],"Column 'İ' 0:0"));
        WorkbookCodecCellsView cells_view={0}; CHECK(workbook_codec_header_cells(h,0,&cells_view)&&cells_view.count==1&&!strcmp(cells_view.cells[0].text,"")); workbook_codec_header_free(h);
    }
}
/* END source-version counter continuation tests. */
/* BEGIN owned source-version ledger continuation tests. */
#define LEDGER_TEST_LIMIT (64u*1024u*1024u)
typedef struct { WorkbookCodecColumn columns[7]; WorkbookCodecAtom cells[256*7]; } LedgerFixture;
typedef struct { unsigned calls; size_t maximum; int mode; bool over; } LedgerResolver;
static bool ledger_resolve(void *context,SourceKind kind,const char *reference,size_t maximum,WorkbookCodecOwnedText *out) {
    LedgerResolver *r=context;r->calls++;r->maximum=maximum;CHECK((kind==SOURCE_CSV||kind==SOURCE_PARQUET)&&reference&&*reference&&maximum);
    if(r->mode==1){out->data=dx_strdup("");out->bytes=0;return true;}
    if(r->mode==2){out->data=dx_strdup("owned failure");out->bytes=13;return false;}
    if(r->mode==3){out->data=dx_strdup("bad");out->bytes=SIZE_MAX;return true;}
    if(r->mode==4){out->data=dx_alloc(4);memcpy(out->data,"a\0b",4);out->bytes=3;return true;}
    if(r->mode==5){out->data=dx_alloc(3);memcpy(out->data,"\xc0\xaf",3);out->bytes=2;return true;}
    if(r->mode==6){out->data=dx_strdup("logical/*.csv");out->bytes=strlen(out->data);return true;}
    if(r->mode==7&&!strcmp(reference,"fail")){out->data=dx_strdup("late owned failure");out->bytes=18;return false;}
    size_t bytes=r->mode==8?(!strcmp(reference,"tail")?1998u+(size_t)r->over:974u):r->mode==9?65536u:strlen("logical:")+strlen(reference);
    if(bytes>=maximum){out->data=dx_strdup("bounded failure");out->bytes=strlen(out->data);return false;}
    out->data=dx_alloc(bytes+1u);out->bytes=bytes;
    if(r->mode==8||r->mode==9)memset(out->data,'l',bytes);else snprintf(out->data,bytes+1u,"logical:%s",reference);
    return true;
}
static WorkbookCodecAtom blob(const void *data,size_t bytes) {return (WorkbookCodecAtom){.kind=WORKBOOK_CODEC_BLOB,.value.blob={data,bytes}};}
static LedgerFixture *ledger_fixture(size_t sheet,uint64_t first,size_t rows) {
    LedgerFixture *f=dx_alloc(sizeof(*f));const char *names[]={"sheet_id","version","owner","source_kind","source_reference","encoding","definition"};
    const WorkbookCodecAtomKind kinds[]={WORKBOOK_CODEC_I32,WORKBOOK_CODEC_U64,WORKBOOK_CODEC_U64,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_BLOB};
    for(size_t c=0;c<7;c++)f->columns[c]=(WorkbookCodecColumn){names[c],kinds[c]};
    for(size_t n=0;n<rows;n++){WorkbookCodecAtom *c=f->cells+n*7;c[0]=i((int32_t)sheet);c[1]=u(first+n);c[2]=u(0);c[3]=i(SOURCE_CSV);c[4]=t("archive 'İ'.csv");c[5]=i(0);c[6]=(WorkbookCodecAtom){0};}return f;
}
static WorkbookCodecBatch ledger_batch(LedgerFixture *f,size_t first,size_t rows,bool end) {return (WorkbookCodecBatch){f->columns,7,f->cells,rows,first,end};}
static WorkbookCodecRead *ledger_header(unsigned version,size_t sheets,bool owners,uint64_t high,size_t limit,LedgerResolver *resolver) {
    WorkbookCodecRead *read=NULL;char *error=NULL;CHECK(workbook_codec_read_begin(&read,key,version,WORKBOOK_CODEC_SHEETS_25,limit,resolver?ledger_resolve:NULL,resolver,&error));meta(read,version,0,0);
    Fixture *s=dx_alloc(sizeof(*s));fixture_columns(s,"sheets");for(size_t n=0;n<sheets;n++)sheet(s,n,SOURCE_NONE);WorkbookCodecBatch batch={s->columns,25,s->cells,sheets,0,true};CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*s),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(s);
    size_t total=owners?sheets*3:0;ObjectFixture *o=object_fixture(total);
    for(size_t n=0;n<total;n++){WorkbookCodecAtom *c=o->cells+n*11;size_t j=n%3;c[0]=i((int32_t)(n/3));c[1]=u(j==2?UINT64_MAX:j+1);c[7]=i((int32_t)j);c[3]=u(j*2);}
    CHECK(workbook_codec_read_add_objects(read,true,&error));batch=object_batch(o,0,total,true);CHECK(total<=256&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*o),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(o);
    CHECK(workbook_codec_read_add_object_counters(read,version>=13,&error));CounterFixture counters=counter_fixture(sheets);if(version>=13){for(size_t n=0;n<sheets;n++)counters.cells[n*2+1]=u(owners?UINT64_MAX:42);batch=counter_batch(&counters,0,sheets,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(counters),&error)==WORKBOOK_CODEC_READ_ACCEPTED);}
    CHECK(workbook_codec_read_add_source_version_counters(read,version==14,&error));if(version==14){for(size_t n=0;n<sheets;n++)counters.cells[n*2+1]=u(high);batch=counter_batch(&counters,0,sheets,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(counters),&error)==WORKBOOK_CODEC_READ_ACCEPTED);}
    return read;
}
static SourcePacket ledger_raw(uint64_t owner,uint64_t version,SourceKind kind,bool maximum) {
    size_t count=maximum?64:2;Column *columns=dx_alloc(count*sizeof(*columns));
    for(size_t n=0;n<count;n++){size_t length=maximum?(n==63?3175:4096):n?7:12;columns[n].name=dx_alloc(length+1u);memset(columns[n].name,'x',length);if(maximum){columns[n].name[0]=(char)('A'+n/10);columns[n].name[1]=(char)('0'+n%10);}else memcpy(columns[n].name,n?"Decimal":"Name 'İ'",n?7:9);columns[n].type=n==1&&!maximum?DUCKDB_TYPE_DECIMAL:DUCKDB_TYPE_VARCHAR;columns[n].number_type=n==1&&!maximum?(NumberType){NUM_DECIMAL,38,18}:(NumberType){0};columns[n].numeric=columns[n].number_type.kind!=NUM_NONE;columns[n].type_name=dx_strdup(columns[n].numeric?"DECIMAL(38,18)":"VARCHAR");}
    Page schema={.columns=columns,.column_count=count};Analysis source={.kind=kind,.path="source.csv"};SourceDigest digest={.bytes=42,.identity={.size=42}};for(size_t n=0;n<DX_SOURCE_DIGEST_BYTES;n++)digest.sha256[n]=(unsigned char)n;
    SourcePacket raw={0};char *error=NULL;CHECK(source_scope_raw_build(owner,version,&source,&schema,&digest,"eng",&raw,&error)&&!error);if(maximum)CHECK(raw.bytes==DX_SOURCE_SCOPE_BYTES-1u);
    for(size_t n=0;n<count;n++){free(columns[n].name);free(columns[n].type_name);}free(columns);return raw;
}
static void ledger_get_unchanged(const WorkbookCodecHeader *h,size_t sheet,size_t index) {
    WorkbookCodecSourceVersionView value,before;memset(&value,0xa5,sizeof(value));memcpy(&before,&value,sizeof(value));CHECK(!workbook_codec_header_source_version_at(h,sheet,index,&value)&&!memcmp(&before,&value,sizeof(value)));
}
static void reject_ledger_arm(WorkbookCodecRead *read,bool present,size_t retained) {
    const WorkbookCodecRequest *r=workbook_codec_read_request(read);WorkbookCodecRequest old=r?*r:(WorkbookCodecRequest){0};size_t bytes=workbook_codec_read_owned_bytes(read);char *error=NULL;
    CHECK(!workbook_codec_read_add_source_versions(read,present,retained,&error)&&error&&*error&&workbook_codec_read_owned_bytes(read)==bytes);CHECK(r?same_request(old,workbook_codec_read_request(read)):!workbook_codec_read_request(read));error_clear(&error);
}
static void ledger_empty(WorkbookCodecRead *read,size_t first) {LedgerFixture *f=ledger_fixture(0,1,0);WorkbookCodecBatch batch=ledger_batch(f,first,0,true);char *error=NULL;CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);free(f);}
static void test_ledger_contract_and_defaults(void) {
    CHECK(WORKBOOK_CODEC_SOURCE_VERSIONS==19&&WORKBOOK_CODEC_BLOB==6&&WORKBOOK_CODEC_TEXT==5&&DX_WORKBOOK_CODEC_SOURCE_VERSION_BYTES>=48&&DX_WORKBOOK_CODEC_RAW_SCRATCH_BYTES==131072u);
    CHECK(!workbook_codec_header_source_versions_ready(NULL)&&!workbook_codec_header_source_version_count(NULL,0));ledger_get_unchanged(NULL,0,0);reject_ledger_arm(NULL,true,0);
    WorkbookCodecRead *read=source_counter_header(14,1,NULL,LIMIT);reject_ledger_arm(read,true,0);workbook_codec_read_free(read);char *error=NULL;
    for(unsigned version=12;version<=14;version++) {
        read=ledger_header(version,2,false,UINT64_MAX,LIMIT,NULL);WorkbookCodecHeader *plain=NULL;CHECK(workbook_codec_read_finish(read,&plain,&error)&&!workbook_codec_header_source_versions_ready(plain));ledger_get_unchanged(plain,0,0);reject_ledger_arm(read,true,0);workbook_codec_read_free(read);workbook_codec_header_free(plain);
        read=ledger_header(version,2,false,UINT64_MAX,LIMIT,NULL);reject_ledger_arm(read,version<14,SIZE_MAX);
        if(version<14){reject_ledger_arm(read,true,0);CHECK(workbook_codec_read_add_source_versions(read,false,0,&error)&&!workbook_codec_read_request(read));}
        else{reject_ledger_arm(read,false,0);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));const WorkbookCodecRequest *r=workbook_codec_read_request(read);CHECK(r&&r->family==WORKBOOK_CODEC_SOURCE_VERSIONS&&r->column_count==7&&r->source_column_count==7&&r->row_limit==65537&&!r->first_row&&!strcmp(r->sql,"SELECT sheet_id,version,owner,source_kind,source_reference,encoding,definition FROM _duxcel_workspace.source_versions ORDER BY sheet_id,version LIMIT 65537"));ledger_empty(read,0);}
        reject_ledger_arm(read,true,0);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error)&&workbook_codec_header_source_versions_ready(h));workbook_codec_read_free(read);CHECK(!workbook_codec_header_source_version_count(h,0)&&!workbook_codec_header_source_version_count(h,2));ledger_get_unchanged(h,0,0);ledger_get_unchanged(h,2,0);CHECK(!workbook_codec_header_source_version_at(h,0,0,NULL));workbook_codec_header_free(h);
    }
}
static void test_ledger_owned_packets_and_locators(void) {
    const uint64_t base=UINT64_C(9007199254740993);char *error=NULL;SourcePacket a=ledger_raw(0,1,SOURCE_CSV,false),b=ledger_raw(UINT64_MAX,base+1,SOURCE_PARQUET,false);unsigned char *expected=dx_alloc(a.bytes);memcpy(expected,a.data,a.bytes);
    WorkbookCodecRead *read=ledger_header(14,2,true,UINT64_MAX,LIMIT,NULL);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));LedgerFixture *f=ledger_fixture(0,1,4);f->cells[5]=i(1);f->cells[6]=blob(a.data,a.bytes);f->cells[8]=u(base);f->cells[9]=u(42);f->cells[15]=u(base+1);f->cells[16]=u(UINT64_MAX);f->cells[17]=i(SOURCE_PARQUET);f->cells[19]=i(1);f->cells[20]=blob(b.data,b.bytes);f->cells[22]=u(UINT64_MAX);f->cells[25]=t("archive/*?[] 'İ'.csv");WorkbookCodecBatch batch=ledger_batch(f,0,4,true);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+a.bytes+b.bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);memset(a.data,0xa5,a.bytes);memset(b.data,0xa5,b.bytes);memset(f,0xa5,sizeof(*f));free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);
    CHECK(workbook_codec_header_source_version_count(h,0)==4&&!workbook_codec_header_source_version_count(h,1)&&workbook_codec_header_analysis(h,0)->kind==SOURCE_NONE);WorkbookCodecSourceVersionView v={0};CHECK(workbook_codec_header_source_version_at(h,0,0,&v)&&v.version==1&&!v.owner&&v.kind==SOURCE_CSV&&!v.locator_resolved&&v.locator&&!*v.locator&&v.raw.bytes==a.bytes&&v.raw.data!=a.data&&!memcmp(v.raw.data,expected,a.bytes));
    CHECK(workbook_codec_header_source_version_at(h,0,1,&v)&&v.version==base&&v.owner==42&&!v.raw.data&&!v.raw.bytes);CHECK(workbook_codec_header_source_version_at(h,0,2,&v)&&v.owner==UINT64_MAX&&v.kind==SOURCE_PARQUET&&v.raw.bytes==b.bytes);CHECK(workbook_codec_header_source_version_at(h,0,3,&v)&&v.version==UINT64_MAX&&!v.raw.bytes&&!strcmp(v.reference,"archive/*?[] 'İ'.csv")&&!v.locator_resolved);ledger_get_unchanged(h,0,4);workbook_codec_header_free(h);h=NULL;free(expected);source_packet_free(&a);source_packet_free(&b);
    for(int mode=0;mode<=1;mode++){LedgerResolver r={.mode=mode};read=ledger_header(14,1,false,10,LIMIT,&r);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));f=ledger_fixture(0,1,1);if(mode)f->cells[4]=t("archive/*.csv");batch=ledger_batch(f,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&r.calls==1);free(f);CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);CHECK(workbook_codec_header_source_version_at(h,0,0,&v)&&v.locator_resolved==!mode&&!strcmp(v.locator,mode?"":"logical:archive 'İ'.csv"));workbook_codec_header_free(h);h=NULL;}
}
static void test_ledger_invalid_packets_and_atomic(void) {
    char *error=NULL;SourcePacket raw=ledger_raw(0,1,SOURCE_CSV,false);WorkbookCodecRead *read=ledger_header(14,2,true,UINT64_MAX,LIMIT,NULL);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));LedgerFixture *f=ledger_fixture(0,1,2);WorkbookCodecBatch batch=ledger_batch(f,0,2,true);
    for(size_t c=0;c<7;c++){WorkbookCodecAtom saved=f->cells[c];f->cells[c]=c==6?blob(NULL,0):(WorkbookCodecAtom){0};reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));f->cells[c]=b(false);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));f->cells[c]=saved;WorkbookCodecColumn column=f->columns[c];f->columns[c].name="bad";reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));f->columns[c]=column;}
    const uint64_t bad_owners[]={1,2};for(size_t n=0;n<2;n++){f->cells[2]=u(bad_owners[n]);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));}f->cells[2]=u(0);f->cells[1]=u(0);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));f->cells[1]=u(1);f->cells[8]=u(1);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));f->cells[8]=u(2);
    for(int kind=SOURCE_NONE;kind<=SOURCE_DEMO;kind++)if(kind!=SOURCE_CSV&&kind!=SOURCE_PARQUET){f->cells[3]=i(kind);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));}f->cells[3]=i(SOURCE_CSV);
    f->cells[4]=t("");reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));f->cells[4]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={"a\0b",3}};reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));f->cells[4]=(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={"\xc0\xaf",2}};reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));f->cells[4]=t("reference.csv");
    for(int encoding=-1;encoding<=2;encoding++)if(encoding){f->cells[5]=i(encoding);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));}f->cells[5]=i(0);f->cells[6]=blob(raw.data,raw.bytes);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+raw.bytes);f->cells[5]=i(1);
    const size_t offsets[]={0,4,7,8,12,16,24,32,36};for(size_t n=0;n<sizeof(offsets)/sizeof(*offsets);n++){size_t offset=offsets[n];unsigned char saved=raw.data[offset];raw.data[offset]^=0x7f;reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+raw.bytes);raw.data[offset]=saved;}
    Analysis source={.kind=SOURCE_CSV,.path="source.csv"};SourcePacket result={0};CHECK(source_scope_result_build(&raw,&source,&result,&error));f->cells[6]=blob(result.data,result.bytes);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+raw.bytes+result.bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&strstr(error,"RAW packet"));error_clear(&error);source_packet_free(&result);
    f->cells[6]=blob(raw.data,31);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+raw.bytes);f->cells[6]=blob((const void *)(uintptr_t)1,DX_SOURCE_SCOPE_BYTES);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));f->cells[6]=blob(raw.data,raw.bytes);f->cells[3]=i(SOURCE_PARQUET);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+raw.bytes);f->cells[3]=i(SOURCE_CSV);
    WorkbookCodecKey stale=key;stale.operation_ticket--;char *prior=dx_strdup("untouched");WorkbookCodecRequest before=*workbook_codec_read_request(read);CHECK(workbook_codec_read_feed(read,stale,WORKBOOK_CODEC_SOURCE_VERSIONS,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&prior)==WORKBOOK_CODEC_READ_STALE&&!strcmp(prior,"untouched")&&same_request(before,workbook_codec_read_request(read)));free(prior);
    f->cells[6]=(WorkbookCodecAtom){0};f->cells[5]=i(0);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);workbook_codec_header_free(h);free(f);source_packet_free(&raw);
    read=ledger_header(14,1,false,1,LIMIT,NULL);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));f=ledger_fixture(0,2,1);batch=ledger_batch(f,0,1,true);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));f->cells[1]=u(1);f->cells[2]=u(43);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));free(f);workbook_codec_read_free(read);
}
static void test_ledger_late_prefix_and_resolver_failures(void) {
    char *error=NULL;WorkbookCodecRead *read=ledger_header(14,1,false,UINT64_MAX,LIMIT,NULL);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));LedgerFixture *f=ledger_fixture(0,1,1);WorkbookCodecBatch batch=ledger_batch(f,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);WorkbookCodecHeader *old=NULL;CHECK(workbook_codec_read_finish(read,&old,&error));workbook_codec_read_free(read);WorkbookCodecHeader *identity=old;WorkbookCodecSourceVersionView prior={0};CHECK(workbook_codec_header_source_version_at(old,0,0,&prior));size_t old_bytes=workbook_codec_header_owned_bytes(old);
    read=ledger_header(14,1,false,UINT64_MAX,LIMIT,NULL);CHECK(workbook_codec_read_add_source_versions(read,true,old_bytes,&error));free(f);f=ledger_fixture(0,1,256);batch=ledger_batch(f,0,256,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);f=ledger_fixture(0,257,2);f->cells[8]=u(256);batch=ledger_batch(f,256,2,true);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+old_bytes);CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity);error_clear(&error);WorkbookCodecSourceVersionView now={0};CHECK(workbook_codec_header_source_version_at(old,0,0,&now)&&now.reference==prior.reference&&now.locator==prior.locator&&workbook_codec_header_owned_bytes(old)==old_bytes);
    f->cells[8]=u(258);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);WorkbookCodecHeader *next=NULL;CHECK(workbook_codec_read_finish(read,&next,&error));workbook_codec_read_free(read);CHECK(workbook_codec_header_source_version_count(next,0)==258&&workbook_codec_header_source_version_at(next,0,257,&now)&&now.version==258&&now.reference!=prior.reference);workbook_codec_header_free(next);workbook_codec_header_free(old);free(f);
    for(int mode=2;mode<=7;mode++){LedgerResolver r={.mode=mode};read=ledger_header(14,1,false,UINT64_MAX,LIMIT,&r);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));f=ledger_fixture(0,1,2);if(mode==7)f->cells[11]=t("fail");batch=ledger_batch(f,0,2,true);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));CHECK(r.calls==(mode==7?2u:1u));if(mode==7){r.mode=0;CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);}free(f);workbook_codec_read_free(read);}
}
static void test_ledger_prescan_and_growing_arrays(void) {
    char *error=NULL;LedgerResolver resolver={0};SourcePacket raw=ledger_raw(0,2,SOURCE_CSV,false);
    WorkbookCodecRead *read=ledger_header(14,1,false,UINT64_MAX,LIMIT,&resolver);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));
    LedgerFixture *f=ledger_fixture(0,1,2);f->cells[12]=i(1);f->cells[13]=blob(raw.data,raw.bytes);WorkbookCodecBatch batch=ledger_batch(f,0,2,true);
    unsigned char tag=raw.data[6];raw.data[6]=SOURCE_SCOPE_RESULT;reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+raw.bytes);CHECK(!resolver.calls);raw.data[6]=tag;
    unsigned char last=raw.data[raw.bytes-1];raw.data[raw.bytes-1]=0xff;reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+raw.bytes);CHECK(!resolver.calls);raw.data[raw.bytes-1]=last;
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+raw.bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&resolver.calls==2);workbook_codec_read_free(read);free(f);source_packet_free(&raw);
    read=ledger_header(14,1,false,UINT64_MAX,LIMIT,NULL);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));f=ledger_fixture(0,1,256);batch=ledger_batch(f,0,256,false);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);f=ledger_fixture(0,257,256);batch=ledger_batch(f,256,256,true);
    size_t owned=workbook_codec_read_owned_bytes(read),retained=sizeof(*f);CHECK(owned+retained+32768u<LIMIT);size_t pressure_bytes=LIMIT-owned-retained-32768u;unsigned char *pressure=dx_alloc(pressure_bytes);
    reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,retained+pressure_bytes);CHECK(workbook_codec_read_request(read)->first_row==256&&workbook_codec_read_owned_bytes(read)==owned);free(pressure);
    CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *header=NULL;CHECK(workbook_codec_read_finish(read,&header,&error));workbook_codec_read_free(read);
    WorkbookCodecSourceVersionView view={0};CHECK(workbook_codec_header_source_version_count(header,0)==512&&workbook_codec_header_source_version_at(header,0,511,&view)&&view.version==512);workbook_codec_header_free(header);
}
static void test_ledger_packet_max_and_owned_budget(void) {
    char *error=NULL;SourcePacket raw=ledger_raw(0,1,SOURCE_CSV,true);LedgerFixture *f=ledger_fixture(0,1,1);f->cells[5]=i(1);f->cells[6]=blob(raw.data,raw.bytes);WorkbookCodecBatch batch=ledger_batch(f,0,1,true);WorkbookCodecRead *read=ledger_header(14,1,false,UINT64_MAX,LEDGER_TEST_LIMIT,NULL);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+raw.bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);WorkbookCodecSourceVersionView v={0};CHECK(workbook_codec_header_source_version_at(h,0,0,&v)&&v.raw.bytes==DX_SOURCE_SCOPE_BYTES-1&&!memcmp(v.raw.data,raw.data,raw.bytes));workbook_codec_header_free(h);source_packet_free(&raw);
    raw=ledger_raw(0,1,SOURCE_CSV,false);f->cells[6]=blob((const void *)(uintptr_t)1,raw.bytes);read=ledger_header(14,1,false,UINT64_MAX,LIMIT,NULL);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));size_t owned=workbook_codec_read_owned_bytes(read),retained=sizeof(*f)+raw.bytes;CHECK(owned+retained+32768<LIMIT);size_t pressure_bytes=LIMIT-owned-retained-32768;unsigned char *pressure=dx_alloc(pressure_bytes);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,retained+pressure_bytes);free(pressure);f->cells[6]=blob(raw.data,raw.bytes);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);workbook_codec_read_free(read);
    read=ledger_header(14,1,false,UINT64_MAX,LIMIT,NULL);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));f->cells[5]=i(0);f->cells[6]=(WorkbookCodecAtom){0};owned=workbook_codec_read_owned_bytes(read);retained=sizeof(*f)+raw.bytes;pressure_bytes=LIMIT-owned-retained-32768;pressure=dx_alloc(pressure_bytes);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(pressure);workbook_codec_read_free(read);free(f);source_packet_free(&raw);
    LedgerResolver r={.mode=9};read=ledger_header(14,1,false,UINT64_MAX,LIMIT,&r);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));f=ledger_fixture(0,1,1);batch=ledger_batch(f,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);CHECK(workbook_codec_header_source_version_at(h,0,0,&v)&&v.locator_resolved&&strlen(v.locator)==65536);workbook_codec_header_free(h);free(f);
}
static void test_ledger_full_counts_and_archive_limits(void) {
    char *error=NULL;WorkbookCodecRead *read=ledger_header(14,64,false,UINT64_MAX,LEDGER_TEST_LIMIT,NULL);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));size_t first=0;
    for(size_t sheet=0;sheet<64;sheet++)for(size_t start=0;start<1024;start+=256){LedgerFixture *f=ledger_fixture(sheet,start+1,256);for(size_t n=0;n<256;n++)f->cells[n*7+4]=t("r");WorkbookCodecBatch batch=ledger_batch(f,first,256,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=256;free(f);}
    CHECK(first==65536);LedgerFixture *f=ledger_fixture(64,1,1);WorkbookCodecBatch batch=ledger_batch(f,first,1,true);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));free(f);ledger_empty(read,first);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);for(size_t s=0;s<64;s++){WorkbookCodecSourceVersionView v={0};CHECK(workbook_codec_header_source_version_count(h,s)==1024&&workbook_codec_header_source_version_at(h,s,1023,&v)&&v.version==1024);}workbook_codec_header_free(h);
    char *ordinary=dx_alloc(975),*tail=dx_alloc(2000);memset(ordinary,'r',974);memset(tail,'t',1999);size_t expected_tail=DX_SOURCE_VERSIONS_BYTES-1024*DX_WORKBOOK_CODEC_SOURCE_VERSION_BYTES-1023*975u-1u;CHECK(expected_tail==1998);
    for(unsigned restored=0;restored<2;restored++){LedgerResolver resolver={.mode=8};read=ledger_header(14,9,false,UINT64_MAX,LEDGER_TEST_LIMIT,restored?&resolver:NULL);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));first=0;
        for(size_t sheet=0;sheet<8;sheet++)for(size_t start=0;start<1024;start+=256){f=ledger_fixture(sheet,start+1,256);for(size_t n=0;n<256;n++){bool last=start+n==1023;f->cells[n*7+4]=restored?t(last?"tail":"head"):(WorkbookCodecAtom){.kind=WORKBOOK_CODEC_TEXT,.value.text={last?tail:ordinary,last?expected_tail:974}};}batch=ledger_batch(f,first,256,false);
            if(start==768&&sheet==0){if(restored)resolver.over=true;else f->cells[255*7+4].value.text.bytes++;reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+2975u);if(restored)resolver.over=false;else f->cells[255*7+4].value.text.bytes--;}
            CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+2975u,&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=256;free(f);}
        f=ledger_fixture(8,1,1);batch=ledger_batch(f,first,1,true);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));free(f);
        f=ledger_fixture(7,1025,1);batch=ledger_batch(f,first,1,true);reject(read,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f));free(f);ledger_empty(read,first);h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);for(size_t sheet=0;sheet<8;sheet++)CHECK(workbook_codec_header_source_version_count(h,sheet)==1024);CHECK(!workbook_codec_header_source_version_count(h,8));workbook_codec_header_free(h);
    }
    free(ordinary);free(tail);
}
#undef LEDGER_TEST_LIMIT
/* END owned source-version ledger continuation tests. */
/* BEGIN source-binding continuation tests. */
#define BINDINGS_TEST_LIMIT (64u*1024u*1024u)
typedef struct { WorkbookCodecColumn columns[3]; WorkbookCodecAtom cells[2113*3]; } BindingFixture;
typedef struct { unsigned calls; int mode; } BindingResolver;
static bool binding_resolve(void *context,SourceKind kind,const char *reference,size_t maximum,WorkbookCodecOwnedText *out) {
    BindingResolver *r=context;r->calls++;CHECK((kind==SOURCE_CSV||kind==SOURCE_PARQUET)&&reference&&*reference&&maximum);
    bool ledger=!strncmp(reference,"ledger",6);const char *path=(r->mode==1||(r->mode==2&&ledger)||(r->mode==3&&!ledger))?"":r->mode==4&&ledger?"logical:other.csv":"logical:same.csv";
    if(strlen(path)>=maximum)return false;out->data=dx_strdup(path);out->bytes=strlen(path);return true;
}
static uint64_t binding_owner(size_t slot,size_t tables) {return !slot?0:slot==tables?UINT64_MAX:UINT64_C(9007199254740993)+slot;}
static BindingFixture *binding_fixture(size_t sheets,size_t tables) {
    BindingFixture *f=dx_alloc(sizeof(*f));f->columns[0]=(WorkbookCodecColumn){"sheet_id",WORKBOOK_CODEC_I32};f->columns[1]=(WorkbookCodecColumn){"owner",WORKBOOK_CODEC_U64};f->columns[2]=(WorkbookCodecColumn){"version",WORKBOOK_CODEC_U64};
    size_t count=sheets*(tables+1u);for(size_t n=0;n<count;n++){f->cells[n*3]=i((int32_t)(n/(tables+1u)));f->cells[n*3+1]=u(binding_owner(n%(tables+1u),tables));f->cells[n*3+2]=u(0);}return f;
}
static WorkbookCodecBatch binding_batch(BindingFixture *f,size_t first,size_t rows,bool end) {return (WorkbookCodecBatch){f->columns,3,f->cells+first*3,rows,first,end};}
static void binding_tables(WorkbookCodecRead *read,size_t sheets,size_t tables,SourceKind primary) {
    char *error=NULL;size_t total=sheets*tables;CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error));TableSourceFixture *f=table_source_fixture(total,tables?tables:1);
    for(size_t n=0;n<total;n++){f->cells[n*14+1]=u(binding_owner(n%tables+1,tables));f->cells[n*14+2]=i(primary==SOURCE_PARQUET?SOURCE_PARQUET:SOURCE_CSV);f->cells[n*14+3]=t("table.csv");}
    for(size_t first=0;first<total||(!total&&!first);){size_t rows=total-first>256?256:total-first;WorkbookCodecBatch batch=table_base_batch(f,first,rows,first+rows==total);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_TABLE_SOURCES,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=rows;if(!rows)break;}
    free(f);table_empty_children(read,0);
}
static void binding_counters_and_ledger(WorkbookCodecRead *read,unsigned version,size_t sheets,bool empty) {
    char *error=NULL;CounterFixture f=counter_fixture(sheets);for(size_t n=0;n<sheets;n++)f.cells[n*2+1]=u(UINT64_MAX);WorkbookCodecBatch batch=counter_batch(&f,0,sheets,true);
    CHECK(workbook_codec_read_add_object_counters(read,version>=13,&error));if(version>=13)CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    CHECK(workbook_codec_read_add_source_version_counters(read,version==14,&error));if(version==14)CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    CHECK(workbook_codec_read_add_source_versions(read,version==14,0,&error));if(version==14&&empty)ledger_empty(read,0);
}
static WorkbookCodecRead *binding_header(unsigned version,size_t sheets,size_t tables,SourceKind primary,BindingResolver *resolver,bool ledger_first,bool empty,size_t limit) {
    char *error=NULL;WorkbookCodecRead *read=NULL;CHECK(workbook_codec_read_begin(&read,key,version,WORKBOOK_CODEC_SHEETS_25,limit,binding_resolve,resolver,&error));meta(read,version,0,0);
    Fixture *s=dx_alloc(sizeof(*s));fixture_columns(s,"sheets");for(size_t n=0;n<sheets;n++){sheet(s,n,primary);s->cells[n*25+3]=t("primary.csv");}WorkbookCodecBatch batch={s->columns,25,s->cells,sheets,0,true};CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*s),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(s);
    size_t total=sheets*tables;ObjectFixture *objects=object_fixture(total);for(size_t n=0;n<total;n++){WorkbookCodecAtom *c=objects->cells+n*11;c[0]=i((int32_t)(n/tables));c[1]=u(binding_owner(n%tables+1,tables));c[3]=u((n%tables)*2u);c[7]=i(OBJECT_TABLE);}
    CHECK(workbook_codec_read_add_objects(read,true,&error));for(size_t first=0;first<total||(!total&&!first);){size_t rows=total-first>256?256:total-first;batch=object_batch(objects,first,rows,first+rows==total);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*objects),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=rows;if(!rows)break;}free(objects);
    size_t per=tables+(primary!=SOURCE_NONE),queries=sheets*per;QueryFixture *q=query_fixture(queries);for(size_t n=0;n<queries;n++){size_t slot=n%per+(primary==SOURCE_NONE);q->cells[n*3]=i((int32_t)(n/per));q->cells[n*3+1]=u(binding_owner(slot,tables));q->cells[n*3+2]=t("SELECT 'NULL', 'İ';  \n");}
    CHECK(workbook_codec_read_add_queries(read,true,&error));for(size_t first=0;first<queries||(!queries&&!first);){size_t rows=queries-first>256?256:queries-first;batch=query_batch(q,first,rows,first+rows==queries);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*q),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=rows;if(!rows)break;}
    q->columns[2]=(WorkbookCodecColumn){"unique_rows",WORKBOOK_CODEC_BOOLEAN};for(size_t n=0;n<queries;n++)q->cells[n*3+2]=b(true);CHECK(workbook_codec_read_add_source_options(read,true,&error));for(size_t first=0;first<queries||(!queries&&!first);){size_t rows=queries-first>256?256:queries-first;batch=query_batch(q,first,rows,first+rows==queries);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*q),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=rows;if(!rows)break;}free(q);
    if(ledger_first){CHECK(empty);binding_counters_and_ledger(read,version,sheets,true);binding_tables(read,sheets,tables,primary);}else{binding_tables(read,sheets,tables,primary);binding_counters_and_ledger(read,version,sheets,empty);}return read;
}
static void binding_get_unchanged(const WorkbookCodecHeader *h,size_t sheet,uint64_t owner) {WorkbookCodecSourceBindingView value,before;memset(&value,0xa5,sizeof(value));memcpy(&before,&value,sizeof(value));CHECK(!workbook_codec_header_source_binding(h,sheet,owner,&value)&&!memcmp(&before,&value,sizeof(value)));}
static void reject_binding_arm(WorkbookCodecRead *read,bool present,size_t retained) {
    const WorkbookCodecRequest *r=workbook_codec_read_request(read);WorkbookCodecRequest before=r?*r:(WorkbookCodecRequest){0};size_t bytes=workbook_codec_read_owned_bytes(read);char *error=NULL;
    CHECK(!workbook_codec_read_add_source_bindings(read,present,retained,&error)&&error&&*error&&workbook_codec_read_owned_bytes(read)==bytes);CHECK(r?same_request(before,workbook_codec_read_request(read)):!workbook_codec_read_request(read));error_clear(&error);
}
static void test_bindings_contract_defaults_and_dependencies(void) {
    CHECK(WORKBOOK_CODEC_SOURCE_BINDINGS==20&&!workbook_codec_header_source_bindings_ready(NULL));binding_get_unchanged(NULL,0,0);reject_binding_arm(NULL,true,0);
    WorkbookCodecRead *read=ledger_header(14,1,false,1,LIMIT,NULL);reject_binding_arm(read,true,0);char *error=NULL;CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));ledger_empty(read,0);reject_binding_arm(read,true,0);workbook_codec_read_free(read);
    read=table_header(14,1,1,LIMIT,NULL);reject_binding_arm(read,true,0);workbook_codec_read_free(read);
    for(unsigned version=12;version<=14;version++)for(unsigned order=0;order<2;order++){
        BindingResolver resolver={0};read=binding_header(version,2,1,SOURCE_NONE,&resolver,order,true,LIMIT);size_t bytes=workbook_codec_read_owned_bytes(read);reject_binding_arm(read,version<14,0);CHECK(workbook_codec_read_add_source_bindings(read,version==14,0,&error));
        BindingFixture *f=binding_fixture(2,1);if(version==14){const WorkbookCodecRequest *r=workbook_codec_read_request(read);CHECK(r&&r->family==WORKBOOK_CODEC_SOURCE_BINDINGS&&r->column_count==3&&r->source_column_count==3&&r->row_limit==2113&&!r->first_row&&!strcmp(r->sql,"SELECT sheet_id,owner,version FROM _duxcel_workspace.source_bindings ORDER BY sheet_id,owner LIMIT 2113"));WorkbookCodecBatch batch=binding_batch(f,0,4,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);}else CHECK(!workbook_codec_read_request(read));
        CHECK(workbook_codec_read_owned_bytes(read)==bytes);reject_binding_arm(read,true,0);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error)&&workbook_codec_header_source_bindings_ready(h));workbook_codec_read_free(read);memset(f,0xa5,sizeof(*f));free(f);
        for(size_t sheet=0;sheet<2;sheet++){WorkbookCodecSourceBindingView value={UINT64_MAX,true};CHECK(workbook_codec_header_source_binding(h,sheet,0,&value)&&!value.version&&!value.locator_matched);CHECK(workbook_codec_header_source_binding(h,sheet,UINT64_MAX,&value)&&!value.version&&!value.locator_matched);WorkbookCodecTableSourceView table={0};CHECK(workbook_codec_header_table_source(h,sheet,UINT64_MAX,&table)&&table.analysis->source_version==0&&table.analysis->unique_rows&&!strcmp(table.analysis->query,"SELECT 'NULL', 'İ';  \n"));}
        binding_get_unchanged(h,2,0);binding_get_unchanged(h,0,42);CHECK(!workbook_codec_header_source_binding(h,0,0,NULL));workbook_codec_header_free(h);
    }
}
static void test_bindings_pending_complete_locators_and_ownership(void) {
    for(unsigned parquet=0;parquet<2;parquet++)for(int mode=0;mode<=4;mode++){
        BindingResolver resolver={.mode=mode};SourceKind kind=parquet?SOURCE_PARQUET:SOURCE_CSV;char *error=NULL;WorkbookCodecRead *read=binding_header(14,1,1,kind,&resolver,false,false,LIMIT);LedgerFixture *ledger=ledger_fixture(0,1,2);ledger->cells[3]=i(kind);ledger->cells[4]=t(mode==1?"ledger/* 'İ'.csv":"ledger-primary.csv");ledger->cells[8]=u(UINT64_MAX);ledger->cells[9]=u(UINT64_MAX);ledger->cells[10]=i(kind);ledger->cells[11]=t("ledger-table.csv");SourcePacket raw=ledger_raw(UINT64_MAX,UINT64_MAX,kind,false);ledger->cells[12]=i(1);ledger->cells[13]=blob(raw.data,raw.bytes);WorkbookCodecBatch data=ledger_batch(ledger,0,2,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&data,sizeof(*ledger)+raw.bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(ledger);unsigned calls=resolver.calls;
        CHECK(workbook_codec_read_add_source_bindings(read,true,raw.bytes,&error));BindingFixture *f=binding_fixture(1,1);f->cells[2]=u(1);f->cells[5]=u(UINT64_MAX);data=binding_batch(f,0,2,true);size_t owned=workbook_codec_read_owned_bytes(read);
        if(mode==4){reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f)+raw.bytes);f->cells[2]=u(0);f->cells[5]=u(0);}
        CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f)+raw.bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==owned&&resolver.calls==calls);memset(f,0xa5,sizeof(*f));free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);memset(raw.data,0xa5,raw.bytes);source_packet_free(&raw);
        WorkbookCodecSourceBindingView value={0};CHECK(workbook_codec_header_source_binding(h,0,0,&value)&&value.version==(mode==4?0u:1u)&&value.locator_matched==(mode==0));CHECK(workbook_codec_header_source_binding(h,0,UINT64_MAX,&value)&&value.version==(mode==4?0:UINT64_MAX)&&value.locator_matched==(mode==0));
        const Analysis *primary=workbook_codec_header_analysis(h,0);WorkbookCodecTableSourceView table={0};CHECK(workbook_codec_header_table_source(h,0,UINT64_MAX,&table)&&!primary->source_version&&!table.analysis->source_version&&primary->unique_rows&&table.analysis->unique_rows&&!strcmp(primary->query,"SELECT 'NULL', 'İ';  \n")&&!strcmp(table.analysis->query,primary->query));WorkbookCodecSourceVersionView version={0};CHECK(workbook_codec_header_source_version_at(h,0,0,&version)&&!version.raw.data&&!strcmp(version.reference,mode==1?"ledger/* 'İ'.csv":"ledger-primary.csv"));CHECK(workbook_codec_header_source_version_at(h,0,1,&version)&&version.raw.bytes&&version.raw.data[0]=='D');workbook_codec_header_free(h);
    }
}
static void test_bindings_shape_coverage_semantics_and_stale(void) {
    BindingResolver resolver={0};char *error=NULL;WorkbookCodecRead *read=binding_header(14,1,2,SOURCE_NONE,&resolver,false,false,LIMIT);LedgerFixture *ledger=ledger_fixture(0,1,3);ledger->cells[4]=t("ledger-primary.csv");ledger->cells[9]=u(binding_owner(1,2));ledger->cells[11]=t("ledger-first.csv");ledger->cells[16]=u(UINT64_MAX);ledger->cells[17]=i(SOURCE_PARQUET);ledger->cells[18]=t("ledger-last.csv");WorkbookCodecBatch data=ledger_batch(ledger,0,3,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&data,sizeof(*ledger),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(ledger);CHECK(workbook_codec_read_add_source_bindings(read,true,0,&error));BindingFixture *f=binding_fixture(1,2);data=binding_batch(f,0,3,true);
    for(size_t column=0;column<3;column++){WorkbookCodecAtom atom=f->cells[column];f->cells[column]=(WorkbookCodecAtom){0};reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[column]=t("NULL");reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[column]=atom;WorkbookCodecColumn field=f->columns[column];f->columns[column].kind=WORKBOOK_CODEC_DOUBLE;reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->columns[column]=field;f->columns[column].name="wrong";reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->columns[column]=field;}
    f->cells[0]=i(-1);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[0]=i(1);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[0]=i(0);f->cells[1]=u(42);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[1]=u(0);
    f->cells[2]=u(1);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[2]=u(0);f->cells[5]=u(1);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[5]=u(99);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[5]=u(0);f->cells[8]=u(3);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[8]=u(0);
    data.column_count=2;reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));data.column_count=3;data.first_row=1;reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));data.first_row=0;data.row_count=2;reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));data.row_count=3;
    WorkbookCodecKey stale=key;stale.operation_ticket--;error=dx_strdup("stale unchanged");char *old_error=error;CHECK(workbook_codec_read_feed(read,stale,WORKBOOK_CODEC_SOURCE_BINDINGS,(const void *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE&&error==old_error);error_clear(&error);
    data=binding_batch(f,0,1,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);data=binding_batch(f,1,2,true);f->cells[7]=f->cells[4];reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));CHECK(workbook_codec_read_request(read)->first_row==1);f->cells[7]=u(UINT64_MAX);f->cells[5]=u(2);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);WorkbookCodecSourceBindingView value={0};CHECK(workbook_codec_header_source_binding(h,0,binding_owner(1,2),&value)&&value.version==2&&value.locator_matched);workbook_codec_header_free(h);
    read=definition_header(14,1,3,true,0,LIMIT,"SELECT 'analytical owners stay excluded';");binding_counters_and_ledger(read,14,1,true);CHECK(workbook_codec_read_add_source_bindings(read,true,0,&error));f=binding_fixture(1,1);f->cells[4]=u(1);data=binding_batch(f,0,2,true);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[4]=u(2);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[4]=u(3);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);binding_get_unchanged(h,0,1);binding_get_unchanged(h,0,2);CHECK(workbook_codec_header_source_binding(h,0,3,&value)&&!value.version);workbook_codec_header_free(h);free(f);
}
static void test_bindings_full_domain_eof_and_late_atomicity(void) {
    BindingResolver resolver={0};char *error=NULL;WorkbookCodecRead *read=binding_header(14,64,32,SOURCE_NONE,&resolver,true,true,BINDINGS_TEST_LIMIT);CHECK(workbook_codec_read_add_source_bindings(read,true,0,&error));BindingFixture *f=binding_fixture(64,32);WorkbookCodecBatch data=binding_batch(f,0,257,false);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));size_t bytes=workbook_codec_read_owned_bytes(read);
    for(size_t first=0;first<2048;first+=256){data=binding_batch(f,first,256,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==bytes);}
    data=binding_batch(f,2048,63,true);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));data=binding_batch(f,2048,65,true);f->cells[2112*3]=i(64);f->cells[2112*3+1]=u(0);f->cells[2112*3+2]=u(0);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));data=binding_batch(f,2048,64,true);WorkbookCodecAtom saved=f->cells[2111*3+1];f->cells[2111*3+1]=u(42);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f));f->cells[2111*3+1]=saved;CHECK(workbook_codec_read_request(read)->first_row==2048&&workbook_codec_read_owned_bytes(read)==bytes);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);memset(f,0xa5,sizeof(*f));free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);
    for(size_t sheet=0;sheet<64;sheet++){WorkbookCodecSourceBindingView value={UINT64_MAX,true};CHECK(workbook_codec_header_source_binding(h,sheet,0,&value)&&!value.version&&!value.locator_matched);CHECK(workbook_codec_header_source_binding(h,sheet,UINT64_MAX,&value)&&!value.version&&!value.locator_matched);CHECK(!workbook_codec_header_analysis(h,sheet)->source_version);}workbook_codec_header_free(h);
}
static void test_bindings_exact_pressure_and_retained_header(void) {
    BindingResolver resolver={0};char *error=NULL;WorkbookCodecRead *read=binding_header(14,1,1,SOURCE_CSV,&resolver,false,true,LIMIT);CHECK(workbook_codec_read_add_source_bindings(read,true,0,&error));BindingFixture *f=binding_fixture(1,1);WorkbookCodecBatch data=binding_batch(f,0,2,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);WorkbookCodecHeader *old=NULL;CHECK(workbook_codec_read_finish(read,&old,&error));workbook_codec_read_free(read);free(f);WorkbookCodecHeader *identity=old;const Analysis *old_analysis=workbook_codec_header_analysis(old,0);const char *query=old_analysis->query,*path=old_analysis->path;size_t old_bytes=workbook_codec_header_owned_bytes(old);
    read=binding_header(14,2,1,SOURCE_CSV,&resolver,false,true,LIMIT);f=binding_fixture(2,1);size_t owned=workbook_codec_read_owned_bytes(read),retained=sizeof(*f)+old_bytes;CHECK(owned+retained<LIMIT);size_t pressure_bytes=LIMIT-owned-retained;unsigned char *pressure=dx_alloc(pressure_bytes+1u);reject_binding_arm(read,true,retained+pressure_bytes+1u);free(pressure);pressure=dx_alloc(pressure_bytes);CHECK(workbook_codec_read_add_source_bindings(read,true,retained+pressure_bytes,&error));data=binding_batch(f,0,2,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(pressure);
    pressure=dx_alloc(pressure_bytes+1u);data=binding_batch(f,2,2,true);reject(read,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,retained+pressure_bytes+1u);CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity);error_clear(&error);free(pressure);pressure=dx_alloc(pressure_bytes);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&data,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==owned);free(pressure);memset(f,0xa5,sizeof(*f));free(f);WorkbookCodecHeader *next=NULL;CHECK(workbook_codec_read_finish(read,&next,&error));workbook_codec_read_free(read);
    CHECK(old==identity&&workbook_codec_header_owned_bytes(old)==old_bytes&&workbook_codec_header_analysis(old,0)==old_analysis&&old_analysis->query==query&&old_analysis->path==path&&!strcmp(query,"SELECT 'NULL', 'İ';  \n"));WorkbookCodecSourceBindingView value={UINT64_MAX,true};CHECK(workbook_codec_header_source_binding(old,0,UINT64_MAX,&value)&&!value.version&&!value.locator_matched);CHECK(workbook_codec_header_source_binding(next,1,UINT64_MAX,&value)&&!value.version&&!value.locator_matched);workbook_codec_header_free(next);workbook_codec_header_free(old);
}
#undef BINDINGS_TEST_LIMIT
/* END source-binding continuation tests. */
/* BEGIN base-format row continuation tests. */
#define FORMAT_ROWS_TEST_LIMIT (64u*1024u*1024u)
typedef struct { WorkbookCodecColumn columns[13]; WorkbookCodecAtom cells[256*13]; } FormatRowsFixture;
static FormatRowsFixture *format_rows_fixture(size_t sheet,size_t first,size_t rows) {
    FormatRowsFixture *f=dx_alloc(sizeof(*f));const char *names[]={"sheet_id","position","r0","r1","c0","c1","mask","flags","alignment","color","font_size","numeric_format","decimals"};
    for(size_t c=0;c<13;c++)f->columns[c]=(WorkbookCodecColumn){names[c],c==2||c==3?WORKBOOK_CODEC_U64:WORKBOOK_CODEC_I32};
    for(size_t n=0;n<rows;n++){size_t p=first+n;WorkbookCodecAtom *c=f->cells+n*13;c[0]=i((int32_t)sheet);c[1]=i((int32_t)p);c[2]=u(p*2);c[3]=u(p*2+1);c[4]=i((int32_t)(p%4));c[5]=i((int32_t)(p%4+2));c[6]=i(p%2?FORMAT_BOLD|FORMAT_COLOR:FORMAT_ALL);c[7]=i((int32_t)(p%8));c[8]=i((int32_t)(p%4));c[9]=i((int32_t)(p%4));c[10]=i((int32_t)(11+p%6));c[11]=i((int32_t)(p%3));c[12]=i((int32_t)(p%5));}return f;
}
static WorkbookCodecBatch format_rows_batch(FormatRowsFixture *f,size_t first,size_t rows,bool end) {return (WorkbookCodecBatch){f->columns,13,f->cells,rows,first,end};}
static void format_rows_bindings(WorkbookCodecRead *read,size_t sheets) {
    char *error=NULL;CHECK(workbook_codec_read_add_source_bindings(read,true,0,&error));BindingFixture *f=binding_fixture(sheets,0);WorkbookCodecBatch batch=binding_batch(f,0,sheets,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);
}
static WorkbookCodecRead *format_rows_header(unsigned version,size_t sheets,size_t limit,BindingResolver *resolver) {
    if(version<14)return sort_header(version,sheets,NULL,limit);
    WorkbookCodecRead *read=binding_header(version,sheets,0,SOURCE_NONE,resolver,true,true,limit);format_rows_bindings(read,sheets);return read;
}
static void format_row_get_unchanged(const WorkbookCodecHeader *h,size_t sheet,size_t index) {WorkbookCodecFormatRowView v,before;memset(&v,0xa5,sizeof(v));memcpy(&before,&v,sizeof(v));CHECK(!workbook_codec_header_format_row_at(h,sheet,index,&v)&&!memcmp(&before,&v,sizeof(v)));}
static void reject_format_rows_arm(WorkbookCodecRead *read,bool present,size_t retained) {
    const WorkbookCodecRequest *r=workbook_codec_read_request(read);WorkbookCodecRequest before=r?*r:(WorkbookCodecRequest){0};size_t bytes=workbook_codec_read_owned_bytes(read);char *error=NULL;
    CHECK(!workbook_codec_read_add_format_rows(read,present,retained,&error)&&error&&*error&&workbook_codec_read_owned_bytes(read)==bytes);CHECK(r?same_request(before,workbook_codec_read_request(read)):!workbook_codec_read_request(read));error_clear(&error);
}
static void test_format_rows_contract_empty_and_dependencies(void) {
    CHECK(WORKBOOK_CODEC_FORMAT_ROWS==21&&DX_WORKBOOK_CODEC_FORMAT_PATCH_BYTES>=sizeof(FormatPatch)&&DX_WORKBOOK_CODEC_FORMAT_PATCH_BYTES>=96u);CHECK(!workbook_codec_header_format_rows_ready(NULL)&&!workbook_codec_header_format_row_count(NULL,0));format_row_get_unchanged(NULL,0,0);reject_format_rows_arm(NULL,true,0);
    WorkbookCodecRead *read=begin(12,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL);reject_format_rows_arm(read,true,0);workbook_codec_read_free(read);read=sort_header(14,1,NULL,LIMIT);reject_format_rows_arm(read,true,0);workbook_codec_read_free(read);
    BindingResolver resolver={0};read=binding_header(14,1,0,SOURCE_NONE,&resolver,true,true,LIMIT);reject_format_rows_arm(read,true,0);format_rows_bindings(read,1);char *error=NULL;WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error)&&!workbook_codec_header_format_rows_ready(h));workbook_codec_read_free(read);format_row_get_unchanged(h,0,0);workbook_codec_header_free(h);
    for(unsigned version=12;version<=14;version++){
        read=format_rows_header(version,3,LIMIT,&resolver);reject_format_rows_arm(read,false,0);CHECK(workbook_codec_read_add_format_rows(read,true,0,&error));const WorkbookCodecRequest *r=workbook_codec_read_request(read);CHECK(r&&r->family==WORKBOOK_CODEC_FORMAT_ROWS&&r->column_count==13&&r->source_column_count==13&&r->row_limit==32769&&!r->first_row&&!strcmp(r->sql,"SELECT sheet_id,position,r0,r1,c0,c1,mask,flags,alignment,color,font_size,numeric_format,decimals FROM _duxcel_workspace.formats ORDER BY sheet_id,position LIMIT 32769"));reject_format_rows_arm(read,true,0);
        FormatRowsFixture *f=format_rows_fixture(0,0,0);WorkbookCodecBatch batch=format_rows_batch(f,0,0,true);size_t bytes=workbook_codec_read_owned_bytes(read);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==bytes);free(f);h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error)&&workbook_codec_header_format_rows_ready(h));workbook_codec_read_free(read);CHECK(!workbook_codec_header_format_row_count(h,0)&&!workbook_codec_header_format_row_count(h,2)&&!workbook_codec_header_format_row_count(h,3));format_row_get_unchanged(h,0,0);format_row_get_unchanged(h,3,0);CHECK(!workbook_codec_header_format_row_at(h,0,0,NULL));workbook_codec_header_free(h);
    }
}
static void test_format_rows_properties_splits_and_owned_copies(void) {
    for(unsigned version=12;version<=14;version++){
        BindingResolver resolver={0};char *error=NULL;WorkbookCodecRead *read=format_rows_header(version,3,LIMIT,&resolver);CHECK(workbook_codec_read_add_format_rows(read,true,0,&error));FormatRowsFixture *f=format_rows_fixture(1,0,3);f->cells[2]=u(0);f->cells[3]=u(DX_SHEET_ROWS-1);f->cells[4]=i(0);f->cells[5]=i(16383);f->cells[7]=i(0);f->cells[8]=i(FORMAT_RIGHT);f->cells[9]=i(3);f->cells[10]=i(16);f->cells[11]=i(FORMAT_NUMBER);f->cells[12]=i(4);
        f->cells[13+2]=u(DX_SHEET_ROWS-1);f->cells[13+3]=u(DX_SHEET_ROWS-1);f->cells[13+4]=i(16383);f->cells[13+5]=i(16383);f->cells[13+6]=i(FORMAT_BOLD);f->cells[13+7]=i(0);f->cells[13+9]=i(2);f->cells[13+10]=i(15);f->cells[13+12]=i(3);f->cells[26+2]=u(0);f->cells[26+3]=u(0);f->cells[26+4]=i(0);f->cells[26+5]=i(0);
        WorkbookCodecAtom expected[39];memcpy(expected,f->cells,sizeof(expected));WorkbookCodecBatch batch=format_rows_batch(f,0,2,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f)+sizeof(expected),&error)==WORKBOOK_CODEC_READ_ACCEPTED);memmove(f->cells,f->cells+26,13*sizeof(*f->cells));batch=format_rows_batch(f,2,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f)+sizeof(expected),&error)==WORKBOOK_CODEC_READ_ACCEPTED);memset(f,0xa5,sizeof(*f));free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);
        CHECK(!workbook_codec_header_format_row_count(h,0)&&workbook_codec_header_format_row_count(h,1)==3&&!workbook_codec_header_format_row_count(h,2));WorkbookCodecFormatRowView last={0};
        for(size_t p=0;p<3;p++){WorkbookCodecFormatRowView v={0};const WorkbookCodecAtom *c=expected+p*13;CHECK(workbook_codec_header_format_row_at(h,1,p,&v)&&v.range.r0==c[2].value.u64&&v.range.r1==c[3].value.u64&&v.range.c0==(size_t)c[4].value.i32&&v.range.c1==(size_t)c[5].value.i32&&v.mask==(unsigned)c[6].value.i32&&v.value.flags==(unsigned)c[7].value.i32&&v.value.align==(unsigned)c[8].value.i32&&v.value.color==(unsigned)c[9].value.i32&&v.value.size==(unsigned)c[10].value.i32&&v.value.number==(unsigned)c[11].value.i32&&v.value.decimals==(unsigned)c[12].value.i32&&!v.value.specified);last=v;}
        format_row_get_unchanged(h,1,3);format_row_get_unchanged(h,0,0);CHECK(!workbook_codec_header_analysis(h,1)->source_version);workbook_codec_header_free(h);CHECK(last.range.r0==0&&last.range.r1==0&&last.mask==FORMAT_ALL&&!last.value.specified);
    }
}
static void test_format_rows_invalid_original_scalars_and_atomicity(void) {
    char *error=NULL;WorkbookCodecRead *read=sort_header(12,2,NULL,LIMIT);CHECK(workbook_codec_read_add_format_rows(read,true,0,&error));FormatRowsFixture *f=format_rows_fixture(0,0,2);WorkbookCodecBatch batch=format_rows_batch(f,0,2,true);
    for(size_t c=0;c<13;c++){WorkbookCodecAtom saved=f->cells[c];f->cells[c]=(WorkbookCodecAtom){0};reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->cells[c]=t("NULL");reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->cells[c]=saved;WorkbookCodecColumn col=f->columns[c];f->columns[c].name="wrong";reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->columns[c]=col;f->columns[c].kind=WORKBOOK_CODEC_DOUBLE;reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->columns[c]=col;}
    const struct {size_t c;int32_t values[4];size_t count;} invalid[]={ {0,{-1,2},2},{1,{-1,1},2},{4,{-1,16384},2},{5,{-1,16384},2},{6,{-1,0,256,INT32_MAX},4},{7,{-1,8,256,INT32_MAX},4},{8,{-1,4,256,INT32_MAX},4},{9,{-1,4,256,INT32_MAX},4},{10,{-1,10,17,256},4},{11,{-1,3,256,INT32_MAX},4},{12,{-1,5,256,INT32_MAX},4} };
    for(size_t n=0;n<sizeof(invalid)/sizeof(*invalid);n++){WorkbookCodecAtom saved=f->cells[invalid[n].c];for(size_t j=0;j<invalid[n].count;j++){f->cells[invalid[n].c]=i(invalid[n].values[j]);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));}f->cells[invalid[n].c]=saved;}
    for(size_t c=2;c<=3;c++){WorkbookCodecAtom saved=f->cells[c];f->cells[c]=u(DX_SHEET_ROWS);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->cells[c]=u(UINT64_MAX);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->cells[c]=saved;}f->cells[2]=u(2);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->cells[2]=u(0);f->cells[4]=i(3);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->cells[4]=i(0);
    batch.column_count=12;reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));batch.column_count=13;batch.first_row=1;reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));batch.first_row=0;batch.row_count=257;reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));batch.row_count=0;batch.end=false;reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));batch=format_rows_batch(f,0,2,true);
    WorkbookCodecKey stale=key;stale.snapshot_generation--;error=dx_strdup("stale untouched");char *identity=error;CHECK(workbook_codec_read_feed(read,stale,WORKBOOK_CODEC_FORMAT_ROWS,(const void *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE&&error==identity);error_clear(&error);
    f->cells[13+1]=i(0);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->cells[13+1]=i(1);f->cells[13+10]=i(17);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->cells[13+10]=i(12);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);CHECK(workbook_codec_header_format_row_count(h,0)==2&&!workbook_codec_header_format_row_count(h,1));workbook_codec_header_free(h);
}
static void test_format_rows_full_domain_and_late_recovery(void) {
    BindingResolver resolver={0};char *error=NULL;WorkbookCodecRead *read=format_rows_header(14,64,FORMAT_ROWS_TEST_LIMIT,&resolver);CHECK(workbook_codec_read_add_format_rows(read,true,0,&error));size_t first=0;
    for(size_t sheet=0;sheet<64;sheet++)for(size_t offset=0;offset<512;offset+=256){FormatRowsFixture *f=format_rows_fixture(sheet,offset,256);WorkbookCodecBatch batch=format_rows_batch(f,first,256,false);if(sheet==0&&offset==256){WorkbookCodecAtom saved=f->cells[255*13+1];f->cells[255*13+1]=i(510);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));CHECK(workbook_codec_read_request(read)->first_row==256);f->cells[255*13+1]=saved;}CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=256;free(f);}
    CHECK(first==32768);FormatRowsFixture *f=format_rows_fixture(63,512,1);WorkbookCodecBatch batch=format_rows_batch(f,first,1,true);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));batch=format_rows_batch(f,first,0,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);for(size_t sheet=0;sheet<64;sheet++){WorkbookCodecFormatRowView v={0};CHECK(workbook_codec_header_format_row_count(h,sheet)==512&&workbook_codec_header_format_row_at(h,sheet,511,&v)&&v.range.r0==1022&&v.mask==(FORMAT_BOLD|FORMAT_COLOR));}workbook_codec_header_free(h);
    read=sort_header(13,2,NULL,LIMIT);CHECK(workbook_codec_read_add_format_rows(read,true,0,&error));for(size_t offset=0;offset<512;offset+=256){f=format_rows_fixture(1,offset,256);batch=format_rows_batch(f,offset,256,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);}f=format_rows_fixture(1,512,1);batch=format_rows_batch(f,512,1,true);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->cells[0]=i(0);f->cells[1]=i(0);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));batch=format_rows_batch(f,512,0,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);CHECK(!workbook_codec_header_format_row_count(h,0)&&workbook_codec_header_format_row_count(h,1)==512);workbook_codec_header_free(h);
}
static void test_format_rows_owned_growth_and_old_header(void) {
    char *error=NULL;WorkbookCodecRead *read=sort_header(12,1,NULL,LIMIT);CHECK(workbook_codec_read_add_format_rows(read,true,0,&error));FormatRowsFixture *f=format_rows_fixture(0,0,1);WorkbookCodecBatch batch=format_rows_batch(f,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);WorkbookCodecHeader *old=NULL;CHECK(workbook_codec_read_finish(read,&old,&error));workbook_codec_read_free(read);free(f);WorkbookCodecHeader *identity=old;size_t old_bytes=workbook_codec_header_owned_bytes(old);const Analysis *analysis=workbook_codec_header_analysis(old,0);const char *path=analysis->path;WorkbookCodecFormatRowView prior={0};CHECK(workbook_codec_header_format_row_at(old,0,0,&prior));
    read=sort_header(12,1,NULL,LIMIT);f=format_rows_fixture(0,0,16);size_t owned=workbook_codec_read_owned_bytes(read),retained=sizeof(*f)+old_bytes;CHECK(owned+retained+16*sizeof(FormatPatch)<LIMIT);size_t pressure_bytes=LIMIT-owned-retained;unsigned char *pressure=dx_alloc(pressure_bytes+1);reject_format_rows_arm(read,true,retained+pressure_bytes+1);free(pressure);CHECK(workbook_codec_read_add_format_rows(read,true,retained,&error));pressure_bytes=LIMIT-owned-retained-16*sizeof(FormatPatch);pressure=dx_alloc(pressure_bytes+1);batch=format_rows_batch(f,0,16,false);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,retained+pressure_bytes+1);free(pressure);pressure=dx_alloc(pressure_bytes);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(pressure);free(f);CHECK(workbook_codec_read_owned_bytes(read)==owned+16*sizeof(FormatPatch));
    f=format_rows_fixture(0,16,16);owned=workbook_codec_read_owned_bytes(read);retained=sizeof(*f)+old_bytes;pressure_bytes=LIMIT-owned-retained-32*sizeof(FormatPatch);pressure=dx_alloc(pressure_bytes+1);batch=format_rows_batch(f,16,16,true);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,retained+pressure_bytes+1);CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity);error_clear(&error);free(pressure);pressure=dx_alloc(pressure_bytes);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(pressure);memset(f,0xa5,sizeof(*f));free(f);CHECK(workbook_codec_read_owned_bytes(read)==owned+16*sizeof(FormatPatch));WorkbookCodecHeader *next=NULL;CHECK(workbook_codec_read_finish(read,&next,&error));workbook_codec_read_free(read);CHECK(old==identity&&workbook_codec_header_owned_bytes(old)==old_bytes&&workbook_codec_header_analysis(old,0)==analysis&&analysis->path==path);WorkbookCodecFormatRowView now={0};CHECK(workbook_codec_header_format_row_at(old,0,0,&now)&&!memcmp(&now.range,&prior.range,sizeof(now.range))&&now.mask==prior.mask&&now.value.size==prior.value.size);CHECK(workbook_codec_header_format_row_count(next,0)==32&&workbook_codec_header_format_row_at(next,0,31,&now)&&now.range.r0==62);workbook_codec_header_free(next);workbook_codec_header_free(old);
}
typedef struct { size_t tail; } FormatLedgerResolver;
static bool format_ledger_resolve(void *context,SourceKind kind,const char *reference,size_t maximum,WorkbookCodecOwnedText *out) {
    FormatLedgerResolver *r=context;CHECK(kind==SOURCE_CSV&&reference&&maximum);size_t bytes=!strcmp(reference,"tail")?r->tail:974u;if(bytes>=maximum)return false;out->data=dx_alloc(bytes+1);out->bytes=bytes;memset(out->data,'l',bytes);return true;
}
static WorkbookCodecRead *format_rows_ledger_budget_header(size_t sheets,size_t ledger_sheets,bool restored,FormatLedgerResolver *resolver) {
    char *error=NULL;WorkbookCodecRead *read=NULL;CHECK(workbook_codec_read_begin(&read,key,14,WORKBOOK_CODEC_SHEETS_25,FORMAT_ROWS_TEST_LIMIT,restored?format_ledger_resolve:NULL,resolver,&error));meta(read,14,0,0);Fixture *s=dx_alloc(sizeof(*s));fixture_columns(s,"sheets");for(size_t n=0;n<sheets;n++)sheet(s,n,SOURCE_NONE);WorkbookCodecBatch batch={s->columns,25,s->cells,sheets,0,true};CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SHEETS,&batch,sizeof(*s),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(s);ObjectFixture *o=object_fixture(0);batch=object_batch(o,0,0,true);CHECK(workbook_codec_read_add_objects(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECTS,&batch,sizeof(*o),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(o);CounterFixture counter=counter_fixture(sheets);for(size_t n=0;n<sheets;n++)counter.cells[n*2+1]=u(UINT64_MAX);batch=counter_batch(&counter,0,sheets,true);CHECK(workbook_codec_read_add_object_counters(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,sizeof(counter),&error)==WORKBOOK_CODEC_READ_ACCEPTED);CHECK(workbook_codec_read_add_source_version_counters(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,sizeof(counter),&error)==WORKBOOK_CODEC_READ_ACCEPTED);CHECK(workbook_codec_read_add_source_versions(read,true,0,&error));
    char *ordinary=dx_alloc(975),*tail=dx_alloc(resolver->tail+1);memset(ordinary,'r',974);memset(tail,'t',resolver->tail);size_t first=0;
    for(size_t sheet=0;sheet<ledger_sheets;sheet++)for(size_t offset=0;offset<1024;offset+=256){LedgerFixture *f=ledger_fixture(sheet,offset+1,256);for(size_t n=0;n<256;n++)f->cells[n*7+4]=t(restored?(offset+n==1023?"tail":"head"):(offset+n==1023?tail:ordinary));batch=ledger_batch(f,first,256,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*f)+975+resolver->tail+1,&error)==WORKBOOK_CODEC_READ_ACCEPTED);first+=256;free(f);}ledger_empty(read,first);free(ordinary);free(tail);
    QueryFixture *q=query_fixture(0);batch=query_batch(q,0,0,true);CHECK(workbook_codec_read_add_queries(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_QUERIES,&batch,sizeof(*q),&error)==WORKBOOK_CODEC_READ_ACCEPTED);q->columns[2]=(WorkbookCodecColumn){"unique_rows",WORKBOOK_CODEC_BOOLEAN};CHECK(workbook_codec_read_add_source_options(read,true,&error)&&workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,sizeof(*q),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(q);binding_tables(read,sheets,0,SOURCE_NONE);format_rows_bindings(read,sheets);return read;
}
static void test_format_rows_joint_archive_and_restored_budgets(void) {
    CHECK(DX_WORKBOOK_CODEC_FORMAT_PATCH_BYTES==96u);size_t tail_bytes=DX_SOURCE_VERSIONS_BYTES-1024*DX_WORKBOOK_CODEC_SOURCE_VERSION_BYTES-1023*975u-1u-DX_WORKBOOK_CODEC_FORMAT_PATCH_BYTES;CHECK(tail_bytes==1902u);
    for(unsigned restored=0;restored<2;restored++){
        FormatLedgerResolver resolver={tail_bytes};char *error=NULL;WorkbookCodecRead *read=format_rows_ledger_budget_header(9,8,restored,&resolver);CHECK(workbook_codec_read_add_format_rows(read,true,0,&error));FormatRowsFixture *f=format_rows_fixture(0,0,8);for(size_t n=0;n<8;n++){f->cells[n*13]=i((int32_t)n);f->cells[n*13+1]=i(0);}WorkbookCodecBatch batch=format_rows_batch(f,0,8,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);f=format_rows_fixture(7,1,1);batch=format_rows_batch(f,8,1,true);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));f->cells[0]=i(8);f->cells[1]=i(0);reject(read,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f));CHECK(workbook_codec_read_request(read)->first_row==8);batch=format_rows_batch(f,8,0,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);for(size_t sheet=0;sheet<8;sheet++)CHECK(workbook_codec_header_format_row_count(h,sheet)==1&&workbook_codec_header_source_version_count(h,sheet)==1024);CHECK(!workbook_codec_header_format_row_count(h,8));WorkbookCodecSourceVersionView last={0};CHECK(workbook_codec_header_source_version_at(h,7,1023,&last)&&(restored?strlen(last.locator):strlen(last.reference))==tail_bytes);workbook_codec_header_free(h);
    }
}
#undef FORMAT_ROWS_TEST_LIMIT
/* END base-format row continuation tests. */
/* BEGIN explicit format tag continuation tests. */
#define FORMAT_TAGS_TEST_LIMIT (64u*1024u*1024u)
typedef struct { WorkbookCodecColumn columns[3]; WorkbookCodecAtom cells[256*3]; } FormatTagsFixture;
static FormatTagsFixture *format_tags_fixture(bool state,size_t sheet,size_t position,size_t rows,bool aware) {
    FormatTagsFixture *f=dx_alloc(sizeof(*f));f->columns[0]=(WorkbookCodecColumn){"sheet_id",WORKBOOK_CODEC_I32};f->columns[1]=(WorkbookCodecColumn){state?"source_aware":"position",state?WORKBOOK_CODEC_BOOLEAN:WORKBOOK_CODEC_I32};f->columns[2]=(WorkbookCodecColumn){"kind",WORKBOOK_CODEC_I32};
    for(size_t n=0;n<rows;n++){size_t stride=state?2u:3u;f->cells[n*stride]=i((int32_t)(state?sheet+n:sheet));f->cells[n*stride+1]=state?b(aware):i((int32_t)(position+n));if(!state)f->cells[n*3+2]=i(aware?(int32_t)((position+n)%4):FORMAT_SPATIAL);}return f;
}
static WorkbookCodecBatch format_tags_batch(FormatTagsFixture *f,bool state,size_t first,size_t rows,bool end) {return (WorkbookCodecBatch){f->columns,state?2u:3u,f->cells,rows,first,end};}
static void format_tags_rows(WorkbookCodecRead *read,size_t sheets,const size_t *counts) {
    char *error=NULL;CHECK(workbook_codec_read_add_format_rows(read,true,0,&error));size_t first=0;
    for(size_t s=0;s<sheets;s++)for(size_t p=0;p<counts[s];){size_t rows=counts[s]-p>256?256:counts[s]-p;FormatRowsFixture *f=format_rows_fixture(s,p,rows);WorkbookCodecBatch batch=format_rows_batch(f,first,rows,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);first+=rows;p+=rows;}
    FormatRowsFixture *f=format_rows_fixture(0,0,0);WorkbookCodecBatch batch=format_rows_batch(f,first,0,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_ROWS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);
}
static WorkbookCodecRead *format_tags_header(unsigned version,size_t sheets,const size_t *counts,size_t limit,BindingResolver *resolver) {
    WorkbookCodecRead *read=format_rows_header(version,sheets,limit,resolver);format_tags_rows(read,sheets,counts);return read;
}
static void reject_format_tags_arm(WorkbookCodecRead *read,WorkbookCodecFormatTagsPresence present,size_t retained) {
    const WorkbookCodecRequest *r=workbook_codec_read_request(read);WorkbookCodecRequest prior=r?*r:(WorkbookCodecRequest){0};size_t bytes=workbook_codec_read_owned_bytes(read);char *error=NULL;CHECK(!workbook_codec_read_add_format_tags(read,present,retained,&error)&&error&&*error&&workbook_codec_read_owned_bytes(read)==bytes);CHECK(r?same_request(prior,workbook_codec_read_request(read)):!workbook_codec_read_request(read));error_clear(&error);
}
static void format_tags_get_unchanged(const WorkbookCodecHeader *h,size_t sheet,size_t index) {
    bool aware=true;FormatSelector selector=FORMAT_RECORDS;CHECK(!workbook_codec_header_format_source_aware(h,sheet,&aware)&&aware);CHECK(!workbook_codec_header_format_selector(h,sheet,index,&selector)&&selector==FORMAT_RECORDS);
}
static void format_tags_state(WorkbookCodecRead *read,size_t sheets,bool aware,size_t retained) {
    FormatTagsFixture *f=format_tags_fixture(true,0,0,sheets,aware);WorkbookCodecBatch batch=format_tags_batch(f,true,0,sheets,true);char *error=NULL;size_t bytes=workbook_codec_read_owned_bytes(read);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_STATE,&batch,retained+sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(read)==bytes);memset(f,0xa5,sizeof(*f));free(f);
}
static void format_tags_selectors(WorkbookCodecRead *read,size_t sheets,const size_t *counts,bool aware) {
    size_t first=0;char *error=NULL;for(size_t s=0;s<sheets;s++)for(size_t p=0;p<counts[s];){size_t rows=counts[s]-p>256?256:counts[s]-p;FormatTagsFixture *f=format_tags_fixture(false,s,p,rows,aware);WorkbookCodecBatch batch=format_tags_batch(f,false,first,rows,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);memset(f,0xa5,sizeof(*f));free(f);first+=rows;p+=rows;}
    FormatTagsFixture *f=format_tags_fixture(false,0,0,0,false);WorkbookCodecBatch batch=format_tags_batch(f,false,first,0,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);
}
static void test_format_tags_contract_versions_and_empty(void) {
    CHECK(WORKBOOK_CODEC_FORMAT_STATE==22&&WORKBOOK_CODEC_FORMAT_SELECTORS==23&&FORMAT_SPATIAL==0&&FORMAT_RESULT==1&&FORMAT_GUARDED==2&&FORMAT_RECORDS==3);CHECK(!workbook_codec_header_format_tags_ready(NULL));format_tags_get_unchanged(NULL,0,0);reject_format_tags_arm(NULL,(WorkbookCodecFormatTagsPresence){0},0);
    BindingResolver resolver={0};WorkbookCodecRead *read=begin(12,WORKBOOK_CODEC_SHEETS_25,LIMIT,NULL);reject_format_tags_arm(read,(WorkbookCodecFormatTagsPresence){0},0);workbook_codec_read_free(read);read=sort_header(13,1,NULL,LIMIT);reject_format_tags_arm(read,(WorkbookCodecFormatTagsPresence){false,true},0);workbook_codec_read_free(read);
    const size_t none[3]={0};char *error=NULL;read=format_rows_header(12,1,LIMIT,&resolver);CHECK(workbook_codec_read_add_format_rows(read,true,0,&error));reject_format_tags_arm(read,(WorkbookCodecFormatTagsPresence){0},0);workbook_codec_read_free(read);
    read=format_tags_header(14,3,none,LIMIT,&resolver);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error)&&!workbook_codec_header_format_tags_ready(h));workbook_codec_read_free(read);format_tags_get_unchanged(h,0,0);workbook_codec_header_free(h);
    for(unsigned version=12;version<=14;version++)for(unsigned bits=0;bits<4;bits++){
        WorkbookCodecFormatTagsPresence present={bits&1u,bits&2u};read=format_tags_header(version,3,none,LIMIT,&resolver);bool valid=bits==(version==12?0u:version==13?2u:3u);
        if(!valid){reject_format_tags_arm(read,present,0);workbook_codec_read_free(read);continue;}
        error=dx_strdup("success unchanged");char *identity=error;size_t bytes=workbook_codec_read_owned_bytes(read);CHECK(workbook_codec_read_add_format_tags(read,present,0,&error)&&error==identity&&workbook_codec_read_owned_bytes(read)==bytes);error_clear(&error);
        if(version==14){const WorkbookCodecRequest *r=workbook_codec_read_request(read);CHECK(r&&r->family==WORKBOOK_CODEC_FORMAT_STATE&&!r->first_row&&r->column_count==2&&r->source_column_count==2&&r->row_limit==65&&!strcmp(r->sql,"SELECT sheet_id,source_aware FROM _duxcel_workspace.format_state ORDER BY sheet_id LIMIT 65"));reject_format_tags_arm(read,present,0);format_tags_state(read,3,true,0);}
        if(version>=13){const WorkbookCodecRequest *r=workbook_codec_read_request(read);CHECK(r&&r->family==WORKBOOK_CODEC_FORMAT_SELECTORS&&!r->first_row&&r->column_count==3&&r->source_column_count==3&&r->row_limit==32769&&!strcmp(r->sql,"SELECT sheet_id,position,kind FROM _duxcel_workspace.format_selectors ORDER BY sheet_id,position LIMIT 32769"));reject_format_tags_arm(read,present,0);format_tags_selectors(read,3,none,false);}else CHECK(!workbook_codec_read_request(read));
        reject_format_tags_arm(read,present,0);h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error)&&workbook_codec_header_format_tags_ready(h));reject_format_tags_arm(read,present,0);workbook_codec_read_free(read);for(size_t s=0;s<3;s++){bool aware=false;CHECK(workbook_codec_header_format_source_aware(h,s,&aware)&&aware==(version==14));FormatSelector selector=FORMAT_RESULT;CHECK(!workbook_codec_header_format_selector(h,s,0,&selector)&&selector==FORMAT_RESULT);}format_tags_get_unchanged(h,3,0);CHECK(!workbook_codec_header_format_source_aware(h,0,NULL)&&!workbook_codec_header_format_selector(h,0,0,NULL));workbook_codec_header_free(h);
    }
}
static void test_format_tags_state_atomic_and_original_schema(void) {
    const size_t counts[3]={0,1,0};BindingResolver resolver={0};char *error=NULL;WorkbookCodecRead *read=format_tags_header(14,3,counts,LIMIT,&resolver);CHECK(workbook_codec_read_add_format_tags(read,(WorkbookCodecFormatTagsPresence){true,true},0,&error));FormatTagsFixture *f=format_tags_fixture(true,0,0,3,true);WorkbookCodecBatch batch=format_tags_batch(f,true,0,3,true);
    for(size_t c=0;c<2;c++){WorkbookCodecAtom saved=f->cells[c];f->cells[c]=(WorkbookCodecAtom){0};reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));f->cells[c]=t("NULL");reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));f->cells[c]=saved;WorkbookCodecColumn col=f->columns[c];f->columns[c].kind=WORKBOOK_CODEC_DOUBLE;reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));f->columns[c]=col;f->columns[c].name="wrong";reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));f->columns[c]=col;}
    WorkbookCodecColumn col=f->columns[0];f->columns[0]=f->columns[1];f->columns[1]=col;reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));col=f->columns[0];f->columns[0]=f->columns[1];f->columns[1]=col;
    f->cells[0]=i(-1);reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));f->cells[0]=i(1);reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));f->cells[0]=i(0);f->cells[2]=i(0);reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));f->cells[2]=i(1);f->cells[4]=i(3);reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));f->cells[4]=i(2);
    batch.row_count=2;reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));for(size_t s=3;s<65;s++){f->cells[s*2]=i((int32_t)s);f->cells[s*2+1]=b(false);}batch.row_count=4;reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));batch.row_count=65;reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));batch.row_count=257;reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));batch.row_count=3;batch.first_row=1;reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));batch.first_row=0;batch.column_count=1;reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));batch.column_count=2;
    WorkbookCodecKey stale=key;stale.document_epoch--;error=dx_strdup("stale unchanged");char *old=error;CHECK(workbook_codec_read_feed(read,stale,WORKBOOK_CODEC_FORMAT_STATE,(const void *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE&&error==old);error_clear(&error);
    batch=format_tags_batch(f,true,0,1,false);size_t bytes=workbook_codec_read_owned_bytes(read);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==bytes);memmove(f->cells,f->cells+2,4*sizeof(*f->cells));f->cells[1]=b(false);f->cells[2]=i(1);batch=format_tags_batch(f,true,1,2,true);reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f));f->cells[2]=i(2);batch.end=false;CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_request(read)->first_row==3);batch=format_tags_batch(f,true,3,0,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_STATE,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==bytes);memset(f,0xa5,sizeof(*f));free(f);
    f=format_tags_fixture(false,1,0,1,false);batch=format_tags_batch(f,false,0,1,true);f->cells[2]=i(FORMAT_RESULT);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[2]=i(FORMAT_SPATIAL);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);for(size_t s=0;s<3;s++){bool aware=false;CHECK(workbook_codec_header_format_source_aware(h,s,&aware)&&aware==(s!=1));}FormatSelector kind=FORMAT_RECORDS;CHECK(workbook_codec_header_format_selector(h,1,0,&kind)&&kind==FORMAT_SPATIAL);workbook_codec_header_free(h);
}
static void test_format_tags_selectors_shape_coverage_and_modes(void) {
    const size_t counts[4]={0,3,0,2};BindingResolver resolver={0};char *error=NULL;
    for(unsigned version=12;version<=14;version++){
        WorkbookCodecRead *read=format_tags_header(version,4,counts,LIMIT,&resolver);CHECK(workbook_codec_read_add_format_tags(read,(WorkbookCodecFormatTagsPresence){version==14,version>=13},0,&error));if(version==14)format_tags_state(read,4,true,0);
        if(version>=13){FormatTagsFixture *f=format_tags_fixture(false,1,0,3,version==14);WorkbookCodecBatch batch=format_tags_batch(f,false,0,3,false);
            for(size_t c=0;c<3;c++){WorkbookCodecAtom saved=f->cells[c];f->cells[c]=(WorkbookCodecAtom){0};reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[c]=t("");reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[c]=saved;WorkbookCodecColumn col=f->columns[c];f->columns[c].kind=WORKBOOK_CODEC_DOUBLE;reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->columns[c]=col;f->columns[c].name="wrong";reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->columns[c]=col;}
            const int32_t badkind[]={-1,4,256,INT32_MAX};for(size_t n=0;n<4;n++){f->cells[2]=i(badkind[n]);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));}f->cells[2]=i(version==14?FORMAT_SPATIAL:FORMAT_RESULT);if(version==13)reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[2]=i(FORMAT_SPATIAL);
            f->cells[0]=i(0);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[0]=i(4);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[0]=i(1);f->cells[1]=i(-1);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[1]=i(1);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[1]=i(0);f->cells[7]=i(1);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[7]=i(2);batch.end=true;reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));batch.end=false;batch.row_count=257;reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));batch.row_count=3;
            size_t bytes=workbook_codec_read_owned_bytes(read);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==bytes);free(f);f=format_tags_fixture(false,3,0,2,version==14);batch=format_tags_batch(f,false,3,2,true);f->cells[4]=i(0);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[4]=i(1);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==bytes);memset(f,0xa5,sizeof(*f));free(f);
        }
        WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);for(size_t s=0;s<4;s++){bool aware=false;CHECK(workbook_codec_header_format_source_aware(h,s,&aware)&&aware==(version==14));for(size_t p=0;p<counts[s];p++){FormatSelector kind=FORMAT_RECORDS;CHECK(workbook_codec_header_format_selector(h,s,p,&kind)&&kind==(version==14?(FormatSelector)(p%4):FORMAT_SPATIAL));WorkbookCodecFormatRowView row={0};CHECK(workbook_codec_header_format_row_at(h,s,p,&row)&&row.range.r0==p*2&&!row.value.specified);}}format_tags_get_unchanged(h,4,0);FormatSelector bad=FORMAT_RESULT;CHECK(!workbook_codec_header_format_selector(h,1,3,&bad)&&bad==FORMAT_RESULT);workbook_codec_header_free(h);
    }
}
static void test_format_tags_full_domain_late_atomicity_and_eof(void) {
    size_t counts[64];for(size_t s=0;s<64;s++)counts[s]=512;BindingResolver resolver={0};char *error=NULL;WorkbookCodecRead *read=format_tags_header(14,64,counts,FORMAT_TAGS_TEST_LIMIT,&resolver);CHECK(workbook_codec_read_add_format_tags(read,(WorkbookCodecFormatTagsPresence){true,true},0,&error));format_tags_state(read,64,true,0);size_t bytes=workbook_codec_read_owned_bytes(read),first=0;
    for(size_t s=0;s<64;s++)for(size_t p=0;p<512;p+=256){FormatTagsFixture *f=format_tags_fixture(false,s,p,256,true);WorkbookCodecBatch batch=format_tags_batch(f,false,first,256,false);if(first==256){f->cells[255*3+2]=i(4);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));f->cells[255*3+2]=i(FORMAT_RECORDS);CHECK(workbook_codec_read_request(read)->first_row==256);WorkbookCodecKey stale=key;stale.operation_ticket--;CHECK(workbook_codec_read_feed(read,stale,WORKBOOK_CODEC_FORMAT_SELECTORS,(const void *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE&&!error);}CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==bytes);memset(f,0xa5,sizeof(*f));free(f);first+=256;}
    CHECK(first==32768);FormatTagsFixture *f=format_tags_fixture(false,63,512,1,false);WorkbookCodecBatch batch=format_tags_batch(f,false,first,1,true);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));batch=format_tags_batch(f,false,first,0,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);for(size_t s=0;s<64;s++){bool aware=false;CHECK(workbook_codec_header_format_source_aware(h,s,&aware)&&aware&&workbook_codec_header_format_row_count(h,s)==512);for(size_t p=0;p<512;p++){FormatSelector kind=FORMAT_SPATIAL;CHECK(workbook_codec_header_format_selector(h,s,p,&kind)&&kind==(FormatSelector)(p%4));}}workbook_codec_header_free(h);
}
static void test_format_tags_retained_budget_and_old_header(void) {
    const size_t counts[2]={2,1};BindingResolver resolver={0};char *error=NULL;WorkbookCodecRead *read=format_tags_header(12,2,counts,LIMIT,&resolver);CHECK(workbook_codec_read_add_format_tags(read,(WorkbookCodecFormatTagsPresence){0},0,&error));WorkbookCodecHeader *old=NULL;CHECK(workbook_codec_read_finish(read,&old,&error));workbook_codec_read_free(read);WorkbookCodecHeader *identity=old;size_t old_bytes=workbook_codec_header_owned_bytes(old);const Analysis *analysis=workbook_codec_header_analysis(old,0);const char *path=analysis->path,*name=workbook_codec_header_name(old,0);WorkbookCodecFormatRowView prior={0};CHECK(workbook_codec_header_format_row_at(old,0,1,&prior));
    read=format_tags_header(14,2,counts,LIMIT,&resolver);FormatTagsFixture *f=format_tags_fixture(true,0,0,2,true);size_t owned=workbook_codec_read_owned_bytes(read),retained=sizeof(*f)+old_bytes;CHECK(owned+retained<LIMIT);size_t pressure_bytes=LIMIT-owned-retained;unsigned char *pressure=dx_alloc(pressure_bytes+1);reject_format_tags_arm(read,(WorkbookCodecFormatTagsPresence){true,true},retained+pressure_bytes+1);free(pressure);pressure=dx_alloc(pressure_bytes);CHECK(workbook_codec_read_add_format_tags(read,(WorkbookCodecFormatTagsPresence){true,true},retained+pressure_bytes,&error));WorkbookCodecBatch batch=format_tags_batch(f,true,0,1,false);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_STATE,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==owned);free(pressure);memmove(f->cells,f->cells+2,2*sizeof(*f->cells));batch=format_tags_batch(f,true,1,1,true);pressure=dx_alloc(pressure_bytes+1);reject(read,WORKBOOK_CODEC_FORMAT_STATE,&batch,retained+pressure_bytes+1);CHECK(!workbook_codec_read_finish(read,&old,&error)&&old==identity);error_clear(&error);free(pressure);pressure=dx_alloc(pressure_bytes);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_STATE,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(pressure);free(f);
    f=format_tags_fixture(false,0,0,2,true);retained=sizeof(*f)+old_bytes;batch=format_tags_batch(f,false,0,2,false);pressure=dx_alloc(pressure_bytes+1);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,retained+pressure_bytes+1);free(pressure);pressure=dx_alloc(pressure_bytes);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==owned);free(pressure);free(f);f=format_tags_fixture(false,1,0,1,true);batch=format_tags_batch(f,false,2,1,true);pressure=dx_alloc(pressure_bytes+1);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,retained+pressure_bytes+1);free(pressure);pressure=dx_alloc(pressure_bytes);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,retained+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&workbook_codec_read_owned_bytes(read)==owned);free(pressure);memset(f,0xa5,sizeof(*f));free(f);
    WorkbookCodecHeader *next=NULL;CHECK(workbook_codec_read_finish(read,&next,&error));workbook_codec_read_free(read);CHECK(old==identity&&workbook_codec_header_owned_bytes(old)==old_bytes&&workbook_codec_header_analysis(old,0)==analysis&&analysis->path==path&&workbook_codec_header_name(old,0)==name);WorkbookCodecFormatRowView now={0};CHECK(workbook_codec_header_format_row_at(old,0,1,&now)&&now.range.r0==prior.range.r0&&now.mask==prior.mask&&now.value.size==prior.value.size);bool aware=true;FormatSelector kind=FORMAT_RECORDS;CHECK(workbook_codec_header_format_source_aware(old,0,&aware)&&!aware&&workbook_codec_header_format_selector(old,0,1,&kind)&&kind==FORMAT_SPATIAL);CHECK(workbook_codec_header_format_source_aware(next,1,&aware)&&aware&&workbook_codec_header_format_selector(next,0,1,&kind)&&kind==FORMAT_RESULT);workbook_codec_header_free(next);workbook_codec_header_free(old);
}
static void test_format_tags_explicit_false_with_bound_source(void) {
    BindingResolver resolver={0};char *error=NULL;WorkbookCodecRead *read=binding_header(14,1,0,SOURCE_CSV,&resolver,false,false,LIMIT);LedgerFixture *l=ledger_fixture(0,1,1);l->cells[4]=t("ledger-primary.csv");WorkbookCodecBatch batch=ledger_batch(l,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,sizeof(*l),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(l);CHECK(workbook_codec_read_add_source_bindings(read,true,0,&error));BindingFixture *binding=binding_fixture(1,0);binding->cells[2]=u(1);batch=binding_batch(binding,0,1,true);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&batch,sizeof(*binding),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(binding);const size_t counts[1]={1};format_tags_rows(read,1,counts);CHECK(workbook_codec_read_add_format_tags(read,(WorkbookCodecFormatTagsPresence){true,true},0,&error));format_tags_state(read,1,false,0);FormatTagsFixture *f=format_tags_fixture(false,0,0,1,true);batch=format_tags_batch(f,false,0,1,true);for(int kind=FORMAT_RESULT;kind<=FORMAT_RECORDS;kind++){f->cells[2]=i(kind);reject(read,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f));}f->cells[2]=i(FORMAT_SPATIAL);CHECK(workbook_codec_read_feed(read,key,WORKBOOK_CODEC_FORMAT_SELECTORS,&batch,sizeof(*f),&error)==WORKBOOK_CODEC_READ_ACCEPTED);free(f);WorkbookCodecHeader *h=NULL;CHECK(workbook_codec_read_finish(read,&h,&error));workbook_codec_read_free(read);bool aware=true;WorkbookCodecSourceBindingView value={0};CHECK(workbook_codec_header_format_source_aware(h,0,&aware)&&!aware&&workbook_codec_header_source_binding(h,0,0,&value)&&value.version==1&&value.locator_matched&&workbook_codec_header_source_version_count(h,0)==1&&!workbook_codec_header_analysis(h,0)->source_version);workbook_codec_header_free(h);
}
#undef FORMAT_TAGS_TEST_LIMIT
/* END explicit format tag continuation tests. */
int main(void) {
    test_happy(); test_meta_and_order(); test_sheets_invalid(); test_many_and_atomic(); test_budget(); test_text_boundaries();
    unsigned base_checks=checks;
    test_sorts_continuation(); test_sorts_invalid_and_atomic(); test_sorts_many_and_order(); test_sorts_budget_and_limits();
    unsigned sorts_checks=checks;
    test_preferences_defaults_and_composition(); test_preferences_rejection_and_recovery(); test_preferences_many_and_budget();
    unsigned preferences_checks=checks;
    test_objects_defaults_and_composition(); test_objects_owned_and_references(); test_objects_invalid_and_atomic(); test_objects_limits_and_budget();
    unsigned objects_checks=checks;
    test_queries_contract_and_empty(); test_queries_owned_and_validation(); test_queries_limits_atomic_budget(); test_queries_composition(); test_queries_foreign_and_partial_budget();
    unsigned queries_checks=checks;
    test_source_options_contract(); test_source_options_atomic_and_types(); test_source_options_limits_budget_and_foreign(); test_source_options_queries_composition();
    unsigned options_checks=checks;
    test_tables_contract(); test_tables_owned_and_invalid(); test_tables_full_limits(); test_tables_text_and_scratch_budget(); test_tables_field_and_native_budgets();
    unsigned tables_checks=checks;
    test_definitions_contract();test_definitions_atomic_and_validators();test_definitions_full_and_limits();test_definitions_budget_and_query_terms();
    unsigned definitions_checks=checks;
    test_primary_filters_contract();test_primary_filter_capture_options();test_primary_filters_owned_and_invalid();test_primary_filters_full_limits_and_composition();test_primary_filters_parser_and_owned_budgets();
    printf("PASS: workbook header reader %u checks (%u unchanged base Header; %u original optional sorts, %u combined; %u optional preferences; %u object descriptors, %u prior combined; %u query definitions, %u prior combined; %u source options, %u prior combined; %u Table source-chain checks, %u prior combined; %u Pivot/Marks checks, %u prior combined; %u primary-filter checks; bounded typed families, exact keys, schema/null/text/viewport validation, deep ownership and atomic budget failures; no full objects or workbook Open support)\n",checks,base_checks,sorts_checks-base_checks,sorts_checks,preferences_checks-sorts_checks,objects_checks-preferences_checks,objects_checks,queries_checks-objects_checks,queries_checks,options_checks-queries_checks,options_checks,tables_checks-options_checks,tables_checks,definitions_checks-tables_checks,definitions_checks,checks-definitions_checks);
    unsigned prior_columns=checks;CHECK(prior_columns==20092);
    test_columns_contract_and_composition();test_columns_splits_owned_and_text();test_columns_invalid_atomic_and_stale();test_columns_capacity_limits_and_eof();test_columns_exact_peak_budget_and_retained_output();test_columns_name_budget_precedes_content();
    printf("PASS: workbook columns reader %u checks (20092 original retained; %u Columns checks; atomic owned geometric arrays, explicit EOF, native width/name bounds; no Cells or full workbook support)\n",checks,checks-prior_columns);
    unsigned prior_cells=checks;CHECK(prior_cells==20988);
    test_cells_contract_and_composition();test_cells_splits_ownership_and_edges();test_cells_invalid_atomic_and_stale();test_cells_count_limits_and_eof();test_cells_text_and_exact_raw_limit();test_cells_exact_peak_budget_and_retained_output();
    printf("PASS: workbook cells reader %u checks (20988 original retained; %u Cells checks; compact owned sparse records, explicit EOF, native count/text bounds; no histories or full workbook support)\n",checks,checks-prior_cells);
    unsigned prior_counters=checks;CHECK(prior_counters==21980);
    test_object_counters_contract_and_defaults();test_object_counters_exact_values_and_eof();test_object_counters_invalid_atomic_and_stale();test_object_counters_budget_and_retained_output();test_object_counters_composition();
    printf("PASS: workbook object-counter reader %u checks (21980 original retained; %u counter checks; exact immutable scalar ownership, atomic EOF/order/budget failure and version12 defaults; no issuance, source-ledger or workbook Open support)\n",checks,checks-prior_counters);
    /* BEGIN source-version counter main extension. */
    unsigned prior_source_counters=checks;CHECK(prior_source_counters==23220);
    test_source_version_counters_contract_and_defaults();test_source_version_counters_values_and_eof();test_source_version_counters_invalid_atomic_and_stale();test_source_version_counters_budget_and_old_output();test_source_version_counters_composition();
    printf("PASS: workbook source-version-counter reader %u checks (23220 original retained; %u source counter checks; independent uint64 domain, atomic scalar ownership/EOF and actual retained-pressure admission; no ledger, bindings, issuer, source activation or workbook Open support)\n",checks,checks-prior_source_counters);
    /* END source-version counter main extension. */
    unsigned prior_ledger=checks;CHECK(prior_ledger==24739);
    test_ledger_contract_and_defaults();test_ledger_owned_packets_and_locators();test_ledger_invalid_packets_and_atomic();test_ledger_late_prefix_and_resolver_failures();test_ledger_prescan_and_growing_arrays();test_ledger_packet_max_and_owned_budget();test_ledger_full_counts_and_archive_limits();
    printf("PASS: workbook source-ledger reader %u checks (24739 original retained; %u ledger checks; exact binary RAW/pending ownership, archive and restored reservations, unresolved locators and atomic retained-budget failure; no binding, model adoption or source activation)\n",checks,checks-prior_ledger);
    unsigned prior_bindings=checks;CHECK(prior_bindings==35033);
    test_bindings_contract_defaults_and_dependencies();test_bindings_pending_complete_locators_and_ownership();test_bindings_shape_coverage_semantics_and_stale();test_bindings_full_domain_eof_and_late_atomicity();test_bindings_exact_pressure_and_retained_header();
    printf("PASS: workbook source-binding reader %u checks (35033 original retained; %u binding checks; complete current-owner coverage, independent POD versions, logical locator comparison/deferment and atomic retained-budget failures; no Analysis adoption or source activation)\n",checks,checks-prior_bindings);
    unsigned prior_format_rows=checks;CHECK(prior_format_rows==38778);
    test_format_rows_contract_empty_and_dependencies();test_format_rows_properties_splits_and_owned_copies();test_format_rows_invalid_original_scalars_and_atomicity();test_format_rows_full_domain_and_late_recovery();test_format_rows_owned_growth_and_old_header();test_format_rows_joint_archive_and_restored_budgets();
    printf("PASS: workbook format-row reader %u checks (38778 original retained; %u format-row checks; shared range/style validation, geometric ownership and separate joint archive/restored reservations; no selector/mode/scopes/history or full formats adoption)\n",checks,checks-prior_format_rows);
    unsigned prior_format_tags=checks;CHECK(prior_format_tags==48288);
    test_format_tags_contract_versions_and_empty();test_format_tags_state_atomic_and_original_schema();test_format_tags_selectors_shape_coverage_and_modes();test_format_tags_full_domain_late_atomicity_and_eof();test_format_tags_retained_budget_and_old_header();test_format_tags_explicit_false_with_bound_source();
    printf("PASS: workbook format-tag reader %u checks (48288 original retained; %u mode/selector checks; explicit awareness and complete ordered tag coverage, atomic allocation-free retained-budget admission; no scope/guard/membership validation, full formats or activation)\n",checks,checks-prior_format_tags);
 return 0;
}
