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
    printf("PASS: workbook header reader %u checks (%u unchanged base Header; %u original optional sorts, %u combined; %u optional preferences; %u object descriptors, %u prior combined; %u query definitions, %u prior combined; %u source options; bounded typed families, exact keys, schema/null/text/viewport validation, deep ownership and atomic budget failures; no full objects or workbook Open support)\n",checks,base_checks,sorts_checks-base_checks,sorts_checks,preferences_checks-sorts_checks,objects_checks-preferences_checks,objects_checks,queries_checks-objects_checks,queries_checks,checks-queries_checks); return 0;
}
