/* BP1 test boundary only: official DuckDB-Wasm -> existing DXP1 decoder ->
   exact C atoms -> the shared metadata header reader. This does not publish a
   Workbook, load sources, decode owner families or enable browser Save/Open. */
#include "workbook/workbook_codec_read.h"
#include "data/browser_query.h"
#include "data/page_wire.h"
#include <emscripten/emscripten.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_LIMIT (2u*1024u*1024u)
#define INPUT_LIMIT (4u*1024u*1024u)
static const char *files[]={"v12.dxls","v12-reordered.dxls","v12-legacy24.dxls","v13.dxls","v13-reordered.dxls","v14.dxls","v14-reordered.dxls"};
static const unsigned versions[]={12,12,12,13,13,14,14};
static const unsigned layouts[]={25,25,24,25,25,25,25};
static const char *sorts_files[]={"sorts-v12.dxls","sorts-v13.dxls","sorts-v14.dxls","sorts-v14-reordered.dxls"};
static const unsigned sorts_versions[]={12,13,14,14};
static const char *preferences_files[]={"preferences-v12.dxls","preferences-v14-unknown-null.dxls","preferences-v14-unknown-nul.dxls","preferences-v13.dxls","preferences-v14.dxls","preferences-v14-reordered.dxls"};
static const unsigned preferences_versions[]={12,14,14,13,14,14};
static const char *objects_files[]={"objects-v12.dxls","objects-v13.dxls","objects-v14.dxls","objects-v14-reordered.dxls"};
static const unsigned objects_versions[]={12,13,14,14};
static const char *queries_files[]={"queries-v12.dxls","queries-v14-blank.dxls","queries-v14-nul.dxls","queries-v13.dxls","queries-v14.dxls","queries-v14-reordered.dxls"};
static const unsigned queries_versions[]={12,14,14,13,14,14};
static bool sorts_mode,sorts_wire,preferences_mode,objects_mode,objects_wire,queries_mode,queries_wire;
static const char *options_files[]={"options-v12.dxls","options-v13.dxls","options-v14.dxls","options-v14-reordered.dxls"};
static const unsigned options_versions[]={12,13,14,14};
static bool options_mode,options_wire,tables_mode,pm_mode,filters_mode,filters_wire,grid_mode,counters_mode;
static const char *grid_files[]={"grid-v12.dxls","grid-v13.dxls","grid-v14.dxls","grid-v14-reordered.dxls"};
static const unsigned grid_versions[]={12,13,14,14};
static const char *counters_files[]={"counters-v12.dxls","counters-v13.dxls","counters-v14.dxls","counters-v14-reordered.dxls"};
static const unsigned counters_versions[]={12,13,14,14};
static void counters_report(bool ok,const char *error);
static void counters_tick(void);
static size_t counters_retained(void);
static bool counters_observe(size_t extra);
static bool counters_stale(size_t size);
static void grid_report(bool ok,const char *error);
static void grid_tick(void);
static size_t grid_retained(void);
static bool grid_observe(size_t extra);
static const char *filters_files[]={"filters-v12.dxls","filters-v13.dxls","filters-v14.dxls","filters-v14-reordered.dxls"};
static const unsigned filters_versions[]={12,13,14,14};
static void filters_report(bool ok,const char *error);
static void filters_tick(void);
static size_t filters_retained(void);
static bool filters_observe(size_t extra);
static unsigned pm_wire;
static const char *pm_files[]={"pivot-marks-v12.dxls","pivot-marks-v13.dxls","pivot-marks-v14.dxls","pivot-marks-v14-reordered.dxls"};
static const unsigned pm_versions[]={12,13,14,14};
static void pm_report(bool ok,const char *error);
static void pm_tick(void);
static size_t pm_retained(void);
static bool pm_observe(size_t extra);
static unsigned tables_wire;
static const char *tables_files[]={"tables-v12.dxls","tables-v13.dxls","tables-v14.dxls","tables-v14-reordered.dxls"};
static const unsigned tables_versions[]={12,13,14,14};
static void tables_report(bool ok,const char *error);
static void tables_tick(void);
static size_t tables_retained(void);
static bool tables_observe(size_t extra);
static bool table_wire(void) {return (tables_mode||pm_mode)&&tables_wire;}
static bool owner_wire(void) {return ((queries_mode||options_mode||tables_mode||pm_mode||filters_mode)&&queries_wire)||((options_mode||tables_mode||pm_mode||filters_mode)&&options_wire);}
static bool object_wire(void) {return (objects_mode||queries_mode||options_mode||tables_mode||pm_mode||filters_mode||counters_mode)&&objects_wire;}
static void options_report(bool ok,const char *error);
static void options_tick(void);
static size_t options_retained(void);
static bool options_observe(size_t extra);
static void queries_report(bool ok,const char *error);
static void queries_tick(void);
static size_t queries_retained(void);
static bool queries_observe(size_t extra);
static void objects_report(bool ok,const char *error);
static void objects_tick(void);
static size_t objects_retained(void);
static bool objects_observe(size_t extra);
static void preferences_report(bool ok,const char *error);
static void preferences_tick(void);
static size_t preferences_retained(void);
static bool preferences_observe(size_t extra);
static void sorts_report(bool ok,const char *error);
static void sorts_tick(void);
static size_t sorts_retained(void);
static bool sorts_observe(size_t extra);
typedef struct {
    WorkbookCodecRead *read;
    WorkbookCodecHeader *header,*prior;
    WorkbookCodecKey prior_key;
    size_t prior_header_bytes;
    Page page,meta_schema,sheets_schema,sorts_schema,preferences_schema,objects_schema,queries_schema,options_schema,filters_schema,grid_columns_schema,grid_cells_schema,counters_schema,table_sources_schema,table_filters_schema,table_sorts_schema,pivot_schema,marks_schema;
    WorkbookCodecColumn columns[25];
    WorkbookCodecAtom *atoms;
    size_t atom_bytes,wire_bytes,peak_input,pressure_bytes;
    unsigned char *pressure;
    char *sql,*descriptors,*bindings,*error;
    char epoch[21],registration[21],sha[65];
    uint64_t total;
    WorkbookCodecKey key;
    unsigned cookie,ticket,fixture,phase,fault,checks,queries,headers,rejections,stale,callbacks,resolves;
    unsigned meta_queries,sheets_queries,describe_queries,normal_queries[7],u64_flags,fixtures;
    bool budget_rejected,sentinel_rejected,tiny_negative_proven,tiny_negative_rejected;
    bool waiting,ready,done,failed,fixture_ready,held_ready,stale_released;
} Driver;
static Driver d;

EM_JS(void, fixture_start,(unsigned cookie,unsigned ticket,const char *name,const char *attach,const char *detach),{
    Module['dxCodecFixture']({heap:()=>HEAPU8,allocate:_malloc,release:_free,fixture:_dx_browser_codec_fixture_result},cookie,ticket,UTF8ToString(name),UTF8ToString(attach),UTF8ToString(detach));
})
EM_JS(void, query_start,(unsigned cookie,unsigned ticket,const char *sql,const char *columns,const char *bindings,unsigned limit,int hold,int workbook_policy),{
    const request={requestId:String(ticket),generation:'9007199254740993',documentId:String(cookie),sourceId:'0',objectId:'0',revision:'18446744073709551615',offset:'0',knownTotal:null,rowLimit:limit,sql:UTF8ToString(sql),sourceBindings:JSON.parse(UTF8ToString(bindings))};
    if(columns)request.columns=JSON.parse(UTF8ToString(columns));
    if(workbook_policy)request.purpose=workbook_policy===9?'workbook-filters':workbook_policy===8?'workbook-marks':workbook_policy===7?'workbook-pivot-fields':workbook_policy===6?'workbook-table-sorts':workbook_policy===5?'workbook-table-filters':workbook_policy===4?'workbook-table-sources':workbook_policy===3?'workbook-owners':workbook_policy===2?'workbook-objects':'workbook-sorts';
    Module['dxCodecSubmit']({heap:()=>HEAPU8,allocate:_malloc,release:_free,reply:_dx_browser_codec_result,string:UTF8ToString,fixture:_dx_browser_codec_fixture_result},cookie,ticket,request,Boolean(hold));
})
EM_JS(void, release_held,(void),{Module['dxCodecRelease']();})
EM_JS(void, report_done,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned queries,unsigned rejections,unsigned stale,size_t peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned describe,unsigned u64_flags,int budget,int sentinel,int tiny,const char *error),{
    const at=normal>>>2;
    Module['dxCodecReadTestDone']({ok:Boolean(ok),checks,fixtures,headers,queries,rejections,stale,peakOwnedInput:peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,budgetRejected:Boolean(budget),sentinelRejected:Boolean(sentinel),tinyNegativeRejected:Boolean(tiny),u64Sentinels:[...(u64_flags&1?['9007199254740993']:[]),...(u64_flags&2?['18446744073709551615']:[])],normalFixtureQueries:Array.from(HEAPU32.subarray(at,at+7)),familyExecutions:{meta,sheets,describe},error:error?UTF8ToString(error):null,boundary:'official-worker/shared-C-header-reader; no workbook publication or browser Save/Open'});
})
static void report(bool ok,const char *error) {
    report_done(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.peak_input,d.normal_queries,d.meta_queries,d.sheets_queries,d.describe_queries,d.u64_flags,d.budget_rejected,d.sentinel_rejected,d.tiny_negative_rejected,error);
}
static size_t text_bytes(const char *s) { return s ? strlen(s)+1 : 0; }
static size_t retained(void) {
    return sizeof(d)+(sorts_mode?sorts_retained():0)+(preferences_mode?preferences_retained():0)+(objects_mode?objects_retained():0)+(queries_mode?queries_retained():0)+(options_mode?options_retained():0)+(tables_mode?tables_retained():0)+(pm_mode?pm_retained():0)+(filters_mode?filters_retained():0)+(grid_mode?grid_retained():0)+(counters_mode?counters_retained():0)+d.counters_schema.bytes+d.grid_columns_schema.bytes+d.grid_cells_schema.bytes+d.filters_schema.bytes+d.pivot_schema.bytes+d.marks_schema.bytes+d.table_sources_schema.bytes+d.table_filters_schema.bytes+d.table_sorts_schema.bytes+d.page.bytes+d.meta_schema.bytes+d.sheets_schema.bytes+d.sorts_schema.bytes+d.preferences_schema.bytes+d.objects_schema.bytes+d.queries_schema.bytes+d.options_schema.bytes+d.atom_bytes+d.pressure_bytes+
        text_bytes(d.sql)+text_bytes(d.descriptors)+text_bytes(d.bindings)+text_bytes(d.error)+workbook_codec_header_owned_bytes(d.header);
}
static void stop(const char *message,int line) {
    if(d.done)return;
    fprintf(stderr,"FAIL codec-read:%d fixture=%u phase=%u fault=%u: %s\n",line,d.fixture,d.phase,d.fault,message?message:"unknown");
    d.failed=true;d.done=true;if(counters_mode)counters_report(false,message);else if(grid_mode)grid_report(false,message);else if(filters_mode)filters_report(false,message);else if(pm_mode)pm_report(false,message);else if(tables_mode)tables_report(false,message);else if(options_mode)options_report(false,message);else if(queries_mode)queries_report(false,message);else if(objects_mode)objects_report(false,message);else if(preferences_mode)preferences_report(false,message);else if(sorts_mode)sorts_report(false,message);else report(false,message);
    emscripten_cancel_main_loop();
}
#define CHECK(test,message) do {d.checks++;if(!(test)){stop((message),__LINE__);return false;}}while(0)
static bool u64(const char *s,uint64_t *out) {
    if(!s||!*s||(*s=='0'&&s[1]))return false;
    uint64_t value=0;
    for(const unsigned char *p=(const unsigned char*)s;*p;p++){
        if(*p<'0'||*p>'9'||value>(UINT64_MAX-(*p-'0'))/10)return false;
        value=value*10+(*p-'0');
    }
    *out=value;return true;
}
static bool i32(const char *s,int32_t *out) {
    bool negative=s&&*s=='-';uint64_t magnitude;
    if(!u64(s? s+negative:NULL,&magnitude)||magnitude>(negative?UINT64_C(2147483648):INT32_MAX))return false;
    *out=negative?(int32_t)(-(int64_t)magnitude):(int32_t)magnitude;return true;
}
static bool same(const char *a,const char *b) {return a&&b?!strcmp(a,b):a==b;}
static const char *sql_type(char code) {return code=='I'?"INTEGER":code=='U'?"UBIGINT":code=='D'?"DOUBLE":code=='B'?"BOOLEAN":"VARCHAR";}
static WorkbookCodecAtomKind atom_kind(char code) {return code=='I'?WORKBOOK_CODEC_I32:code=='U'?WORKBOOK_CODEC_U64:code=='D'?WORKBOOK_CODEC_DOUBLE:code=='B'?WORKBOOK_CODEC_BOOLEAN:WORKBOOK_CODEC_TEXT;}
static bool source_schema(const Page *schema,const WorkbookCodecTable *table,size_t expected) {
    CHECK(schema->column_count==expected,"Original metadata field count changed");
    const char *name=table->columns,*types=table->types;
    for(size_t i=0;i<expected;i++){
        while(*types==' ')types++;
        size_t n=strcspn(name,",");const Column *found=NULL;
        for(size_t j=0;j<schema->column_count;j++)if(strlen(schema->columns[j].name)==n&&!memcmp(schema->columns[j].name,name,n)){CHECK(!found,"Duplicate original metadata name");found=schema->columns+j;}
        CHECK(found&&same(found->type_name,sql_type(*types)),"Original metadata SQL type/name mismatch");
        name+=n;if(*name)name++;types++;
    }
    return true;
}
static bool schema_matches(const Page *schema,const WorkbookCodecTable *table,size_t expected) {
    // A nonfatal version of the same named-set check for a genuine wrong-type
    // DESCRIBE result. No SQL type fallback can turn VARCHAR into INTEGER.
    if(schema->column_count!=expected)return false;
    const char *name=table->columns,*types=table->types;
    for(size_t i=0;i<expected;i++){
        while(*types==' ')types++;size_t n=strcspn(name,",");size_t found=0;
        for(size_t j=0;j<schema->column_count;j++)if(strlen(schema->columns[j].name)==n&&!memcmp(schema->columns[j].name,name,n)&&same(schema->columns[j].type_name,sql_type(*types)))found++;
        if(found!=1)return false;name+=n;if(*name)name++;types++;
    }
    return true;
}
static bool canonical_schema(Page *out,const WorkbookCodecTable *table) {
    Page next={0};const char *p=table->columns,*types=table->types;
    next.column_count=!strcmp(table->table,"meta")||!strcmp(table->table,"queries")||!strcmp(table->table,"source_options")?3:!strcmp(table->table,"sorts")?4:!strcmp(table->table,"preferences")?2:!strcmp(table->table,"objects")?11:!strcmp(table->table,"table_sources")?14:!strcmp(table->table,"table_filters")?6:!strcmp(table->table,"table_sorts")?5:!strcmp(table->table,"pivot_fields")?9:!strcmp(table->table,"marks")?8:!strcmp(table->table,"filters")?5:(!strcmp(table->table,"columns")||!strcmp(table->table,"cells"))?4:25;
    next.columns=dx_alloc(next.column_count*sizeof(*next.columns));next.bytes=next.column_count*sizeof(*next.columns);
    for(size_t i=0;i<next.column_count;i++){
        while(*types==' ')types++;size_t n=strcspn(p,",");Column *column=next.columns+i;
        column->name=dx_alloc(n+1);memcpy(column->name,p,n);column->name[n]=0;column->type_name=dx_strdup(sql_type(*types));
        CHECK(page_type_from_sql(column->type_name,&column->type,&column->number_type),"Canonical SQL type construction failed");
        column->numeric=column->number_type.kind!=NUM_NONE;next.bytes+=n+1+strlen(column->type_name)+1;
        p+=n;if(*p)p++;types++;
    }
    if((sorts_mode&&!sorts_observe(next.bytes))||(preferences_mode&&!preferences_observe(next.bytes))||(objects_mode&&!objects_observe(next.bytes))||(queries_mode&&!queries_observe(next.bytes))||(options_mode&&!options_observe(next.bytes))||(tables_mode&&!tables_observe(next.bytes))||(pm_mode&&!pm_observe(next.bytes))||(filters_mode&&!filters_observe(next.bytes))||(grid_mode&&!grid_observe(next.bytes))||(counters_mode&&!counters_observe(next.bytes))){page_free(&next);return false;}
    page_free(out);*out=next;return true;
}
static char *descriptors(const Page *schema) {
    static const char *kinds[]={"none","i8","i16","i32","i64","i128","u8","u16","u32","u64","u128","decimal","float"};
    char *json=dx_strdup("[");
    for(size_t i=0;i<schema->column_count;i++){
        const Column *c=schema->columns+i;
        // Names/types come only from the fixed verified codec inventory, not
        // user SQL identifiers. They contain no JSON metacharacters.
        dx_append(&json,"%s{\"name\":\"%s\",\"typeName\":\"%s\",\"numberKind\":\"%s\",\"precision\":%u,\"scale\":%u}",i?",":"",c->name,c->type_name,kinds[c->number_type.kind],c->number_type.precision,c->number_type.scale);
    }
    dx_append(&json,"]");return json;
}
static bool resolver(void *context,SourceKind kind,const char *reference,size_t maximum,WorkbookCodecOwnedText *out) {
    Driver *driver=context;if(((tables_mode||pm_mode)?(kind<SOURCE_CSV||kind>SOURCE_DATABASE):kind!=SOURCE_CSV)||!reference||!*reference||!maximum)return false;
    out->data=malloc(1);if(!out->data)return false;out->data[0]=0;out->bytes=0;driver->resolves++;return true;
}
static bool header_expected(void) {
    const WorkbookCodecHeader *h=d.header;
    CHECK(h&&workbook_codec_header_sheet_count(h)==3&&workbook_codec_header_active_sheet(h)==1,"Header sheet/active metadata changed");
    CHECK(workbook_codec_header_version(h)==versions[d.fixture]&&workbook_codec_header_user_scale(h)==1.25f,"Actual package version/scale mismatch");
    const WorkbookCodecKey *key=workbook_codec_header_key(h);
    CHECK(key&&key->document_epoch==d.key.document_epoch&&key->operation_ticket==d.key.operation_ticket&&key->snapshot_generation==d.key.snapshot_generation,"Exact header routing key lost");
    const char *names[]={"Native 'İ' 🦆","Empty string","Literal NULL"};
    for(size_t i=0;i<3;i++){
        const Analysis *a=workbook_codec_header_analysis(h,i);const SheetView *v=workbook_codec_header_view(h,i);
        CHECK(same(workbook_codec_header_name(h,i),names[i]),"UTF-8 worksheet name changed");
        CHECK(a&&v&&a->path&&!*a->path&&!a->query&&!a->source_version&&!a->filter_count&&!a->sort_count&&!a->unique_rows,"Header executed/loaded later source or query state");
        CHECK(v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==(layouts[d.fixture]==24?1:1.5f)&&!v->columns&&!v->column_count,"Exact viewport/canvas metadata changed");
        if(!i){const char *ref=workbook_codec_header_reference(h,i);
            CHECK(a->kind==SOURCE_CSV&&same(a->schema,"")&&same(a->table,"NULL")&&same(a->filter_column,"city")&&!a->filter_value&&a->filter_op==FILTER_NULL&&same(a->sort_column,"city")&&a->descending&&!a->group_column&&!a->measure_column,"Primary NULL/empty/literal metadata changed");
            CHECK(same(ref,"missing-source.csv"),"Raw source reference lost; empty locator is a separate browser boundary");
        }else CHECK(a->kind==(i==1?SOURCE_NONE:SOURCE_DEMO)&&same(a->filter_value,i==1?"":"NULL")&&same(workbook_codec_header_reference(h,i),""),"Empty versus literal NULL changed");
    }
    return true;
}
static void clear_input(void) {
    page_free(&d.page);free(d.atoms);d.atoms=NULL;d.atom_bytes=0;free(d.pressure);d.pressure=NULL;d.pressure_bytes=0;
    free(d.sql);d.sql=NULL;free(d.descriptors);d.descriptors=NULL;free(d.error);d.error=NULL;d.ready=false;
}
static bool prior_unchanged(void) {
    CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes,"Old independent Header ownership changed");
    if(d.header){const WorkbookCodecKey *key=workbook_codec_header_key(d.header);
        CHECK(key&&key->document_epoch==d.prior_key.document_epoch&&key->operation_ticket==d.prior_key.operation_ticket&&key->snapshot_generation==d.prior_key.snapshot_generation&&workbook_codec_header_sheet_count(d.header)==3&&same(workbook_codec_header_name(d.header,0),"Native 'İ' 🦆")&&workbook_codec_header_view(d.header,0)->row==DX_SHEET_ROWS-1,"Failed/stale candidate mutated retained Header contents");
    }
    return true;
}
static bool begin_read(void) {
    workbook_codec_read_free(d.read);d.read=NULL;
    d.prior=d.header;d.prior_header_bytes=workbook_codec_header_owned_bytes(d.header);
    if(d.header)d.prior_key=*workbook_codec_header_key(d.header);
    d.key=(WorkbookCodecKey){UINT64_C(9007199254740993),UINT64_C(18446744073709551615)-d.fault,UINT64_C(9007199254740995)+d.fixture};
    CHECK(workbook_codec_read_begin_with_options(&d.read,d.key,counters_mode?counters_versions[d.fixture]:grid_mode?grid_versions[d.fixture]:filters_mode?filters_versions[d.fixture]:pm_mode?pm_versions[d.fixture]:tables_mode?tables_versions[d.fixture]:options_mode?options_versions[d.fixture]:queries_mode?queries_versions[d.fixture]:objects_mode?objects_versions[d.fixture]:preferences_mode?preferences_versions[d.fixture]:sorts_mode?sorts_versions[d.fixture]:versions[d.fixture],(WorkbookCodecSheetsLayout)(sorts_mode||preferences_mode||objects_mode||queries_mode||options_mode||tables_mode||pm_mode||filters_mode||grid_mode||counters_mode?25:layouts[d.fixture]),(WorkbookCodecReadOptions){filters_mode},HEADER_LIMIT,resolver,&d,&d.error),d.error?d.error:"Begin shared header reader");
    CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error),"Incomplete metadata unexpectedly replaced old Header");
    free(d.error);d.error=NULL;return true;
}
static bool send_sql(char *owned,const Page *schema,unsigned limit,bool hold) {
    CHECK(!d.waiting&&owned&&d.bindings,"Unowned/overlapping metadata request");
    free(d.sql);d.sql=owned;free(d.descriptors);d.descriptors=schema?descriptors(schema):NULL;
    CHECK(strlen(d.sql)<=DX_BROWSER_SQL_BYTES,"Metadata SQL exceeds bounded bridge policy");
    CHECK(d.ticket<UINT_MAX,"Test ticket exhausted");d.ticket++;d.queries++;if(!schema)d.describe_queries++;if(!d.fault)d.normal_queries[d.fixture]++;d.waiting=true;d.ready=false;
    if(sorts_mode)CHECK(sorts_observe(0),"Sorts request buffers exceed C input budget");
    if(preferences_mode)CHECK(preferences_observe(0),"Preferences request buffers exceed C input budget");
    if(objects_mode)CHECK(objects_observe(0),"Objects request buffers exceed C input budget");
    if(queries_mode)CHECK(queries_observe(0),"Queries request buffers exceed C input budget");
    if(options_mode)CHECK(options_observe(0),"Source options request buffers exceed C input budget");
    if(tables_mode)CHECK(tables_observe(0),"Table request buffers exceed C input budget");
    if(pm_mode)CHECK(pm_observe(0),"Pivot/Marks request buffers exceed C input budget");
    if(filters_mode)CHECK(filters_observe(0),"Filter request buffers exceed C input budget");
    if(grid_mode)CHECK(grid_observe(0),"Grid request buffers exceed C input budget");
    if(counters_mode)CHECK(counters_observe(0),"Counter request buffers exceed C input budget");
    query_start(d.cookie,d.ticket,d.sql,d.descriptors,d.bindings,limit,hold,filters_mode&&filters_wire?9:pm_mode&&pm_wire?pm_wire:table_wire()?tables_wire:owner_wire()?3:object_wire()?2:(sorts_mode||filters_mode)&&sorts_wire);return true;
}
static bool send_family(bool sheets,bool hold) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
    CHECK(request&&request->family==(sheets?WORKBOOK_CODEC_SHEETS:WORKBOOK_CODEC_META)&&!request->first_row,"Unexpected cursor family or repeated execution");
    char *relation=dx_strdup(request->sql);
    if(!sheets&&d.fault==1){free(relation);relation=dx_strdup("SELECT NULL::INTEGER AS version,active_sheet,user_scale FROM _duxcel_workspace.meta LIMIT 2");}
    if(!sheets&&d.fault==2){free(relation);relation=dx_strdup("SELECT version,9007199254740993::UBIGINT AS active_sheet,user_scale FROM _duxcel_workspace.meta LIMIT 2");}
    if(sheets&&d.fault>=3&&d.fault<=7){
        const char *replacement=d.fault==3?"NULL::VARCHAR AS name":d.fault==4?"0::INTEGER AS id":d.fault==5?"18446744073709551615::UBIGINT AS active_row":d.fault==6?"'NaN'::DOUBLE AS horizontal":"concat('A',chr(0),'B') AS name";
        char *changed=dx_printf("SELECT * REPLACE (%s) FROM (%s) AS _codec_bad",replacement,relation);free(relation);relation=changed;
    }
    if(sheets&&d.fault==12){
        char *changed=dx_printf("SELECT seed.* REPLACE (CAST(n.i AS INTEGER) AS id) FROM (SELECT * FROM (%s) AS source WHERE id=0) AS seed CROSS JOIN range(65) AS n(i) ORDER BY id",relation);free(relation);relation=changed;
    }
    if(sheets&&d.fault==13){
        char *changed=dx_printf("SELECT * REPLACE (-1e-50::DOUBLE AS horizontal) FROM (%s) AS _codec_tiny_negative",relation);free(relation);relation=changed;
    }
    if(sheets)d.sheets_queries++;else d.meta_queries++;
    char *sql=browser_page_sql(relation,sheets?&d.sheets_schema:&d.meta_schema,0,&d.error);free(relation);
    return send_sql(sql,sheets?&d.sheets_schema:&d.meta_schema,(unsigned)request->row_limit,hold);
}
static bool atoms_from_page(bool sheets) {
    const WorkbookCodecTable *table=workbook_codec_table_find(sheets?"sheets":"meta");size_t columns=sheets?25:3;
    bool sentinel=sheets&&d.fault==12;
    CHECK(d.page.column_count==columns&&d.page.offset==0&&d.page.count_pending==sentinel&&d.page.total==d.page.row_count,"Expected one bounded canonical metadata result with honest sentinel completeness");
    CHECK(d.page.row_count==(sentinel?65:sheets?3:1),"Metadata fixture row count changed");
    d.atom_bytes=d.page.row_count*columns*sizeof(*d.atoms);d.atoms=dx_alloc(d.atom_bytes);
    const char *types=table->types;
    for(size_t c=0;c<columns;c++){
        while(*types==' ')types++;WorkbookCodecAtomKind kind=atom_kind(*types++);
        d.columns[c]=(WorkbookCodecColumn){d.page.columns[c].name,kind};
        const Page *schema=sheets?&d.sheets_schema:&d.meta_schema;
        CHECK(same(d.page.columns[c].name,schema->columns[c].name)&&same(d.page.columns[c].type_name,schema->columns[c].type_name),"Canonical original-type sidecar changed");
        for(size_t r=0;r<d.page.row_count;r++){
            const Cell *cell=d.page.cells+r*columns+c;WorkbookCodecAtom *atom=d.atoms+r*columns+c;atom->kind=cell->is_null?WORKBOOK_CODEC_NULL:kind;
            if(cell->is_null)continue;CHECK(cell->text,"Missing exact raw cell text");
            if(kind==WORKBOOK_CODEC_U64)CHECK(u64(cell->text,&atom->value.u64),"Noncanonical/out-of-range exact UBIGINT text");
            else if(kind==WORKBOOK_CODEC_I32)CHECK(i32(cell->text,&atom->value.i32),"Out-of-range exact INTEGER text");
            else if(kind==WORKBOOK_CODEC_BOOLEAN){CHECK(!strcmp(cell->text,"true")||!strcmp(cell->text,"false"),"Noncanonical BOOLEAN text");atom->value.boolean=!strcmp(cell->text,"true");}
            else if(kind==WORKBOOK_CODEC_DOUBLE){char *end=NULL;errno=0;atom->value.real=strtod(cell->text,&end);CHECK(end&&end!=cell->text&&!*end,"Invalid complete DOUBLE text");}
            else atom->value.text=(WorkbookCodecText){cell->text,strlen(cell->text)};
        }
    }
    if(!sheets&&d.fault==2){CHECK(d.atoms[1].value.u64==UINT64_C(9007199254740993)&&!strcmp(d.page.cells[1].text,"9007199254740993"),"Wide exact U64 was rounded before header validation");d.u64_flags|=1;}
    if(sheets&&d.fault==5){CHECK(d.atoms[14].value.u64==UINT64_MAX&&!strcmp(d.page.cells[14].text,"18446744073709551615"),"Maximum U64 was rounded before viewport rejection");d.u64_flags|=2;}
    if(sentinel)CHECK(d.atoms[0].value.i32==0&&d.atoms[64*columns].value.i32==64,"Actual 65th worksheet sentinel missing or unordered");
    if(sheets&&d.fault==13){
        double value=d.atoms[21].value.real;
        CHECK(value<0&&isfinite(value)&&(float)value==0,"Original tiny-negative DOUBLE was narrowed or lost before shared-reader validation");d.tiny_negative_proven=true;
    }
    return true;
}
static bool feed_page(bool sheets,bool expected_failure) {
    CHECK(atoms_from_page(sheets),"Exact atom adaptation failed");
    if(d.fault==8&&sheets){d.pressure=dx_alloc(HEADER_LIMIT);d.pressure_bytes=HEADER_LIMIT;memset(d.pressure,0,d.pressure_bytes);}
    const WorkbookCodecHeader *old=d.header;const WorkbookCodecRequest *before=workbook_codec_read_request(d.read);size_t bytes=workbook_codec_read_owned_bytes(d.read),first=before->first_row;
    const char *request_sql=before->sql;
    for(size_t row=0;row<d.page.row_count;row++){
        // The bad duplicate-id case is one atomic batch, otherwise its first
        // valid row would legitimately be adopted before the second failed.
        size_t count=expected_failure?d.page.row_count:1;
        WorkbookCodecBatch batch={d.columns,d.page.column_count,d.atoms+row*d.page.column_count,count,row,row+count==d.page.row_count&&!d.page.count_pending};
        size_t input=retained(),all=input+workbook_codec_read_owned_bytes(d.read);CHECK(all<=INPUT_LIMIT,"Concrete coexisting C buffers exceed test input limit");if(all>d.peak_input)d.peak_input=all;
        WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,sheets?WORKBOOK_CODEC_SHEETS:WORKBOOK_CODEC_META,&batch,input,&d.error);
        if(expected_failure){
            CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error,"Genuine invalid metadata/budget unexpectedly accepted");
            CHECK(d.header==old&&workbook_codec_read_owned_bytes(d.read)==bytes&&workbook_codec_read_request(d.read)->first_row==first&&workbook_codec_read_request(d.read)->sql==request_sql,"Failed feed changed old Header or staged reader");
            CHECK(prior_unchanged(),"Invalid metadata changed retained Header");
            if(d.fault==8){CHECK(strstr(d.error,"budget"),"Pressure case failed for a reason other than budget admission");d.budget_rejected=true;}
            if(d.fault==12){CHECK(strstr(d.error,"Too many worksheets"),"65th sentinel failed for a reason other than the sheet bound");d.sentinel_rejected=true;}
            if(d.fault==13){CHECK(d.tiny_negative_proven&&strstr(d.error,"viewport"),"Tiny-negative rejection lacks original DOUBLE/viewport proof");d.tiny_negative_rejected=true;}
            CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==old,"Failed candidate replaced old Header");
            d.rejections++;return true;
        }
        CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Shared reader rejected valid exact atoms");
    }
    return true;
}
static bool next_fault(void) {
    clear_input();if(d.fault<14){d.fault++;return begin_read();}return true;
}
EM_JS(void, cancel_candidate,(void),{Module['dxEngine'].cancel(false);})
static bool tick_body(void) {
    if(d.phase==0){
        CHECK(d.fixture<7,"Fixture index exceeded inventory");
        d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;
        char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",files[d.fixture]);
        fixture_start(d.cookie,d.ticket,files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=1;return true;
    }
    if(d.phase==1){
        if(!d.fixture_ready)return true;
        CHECK(d.bindings&&*d.epoch&&*d.registration&&strlen(d.sha)==64,"Registered fixture identity missing");
        d.phase=2;return send_sql(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false);
    }
    if(d.waiting)return true;
    if(d.phase==2||d.phase==3){
        CHECK(d.ready&&!d.error,d.error?d.error:"Original DESCRIBE did not arrive");
        Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Invalid original metadata DESCRIBE");
        const WorkbookCodecTable *table=workbook_codec_table_find(d.phase==2?"meta":"sheets");
        if(!source_schema(&schema,table,d.phase==2?3:layouts[d.fixture])){page_free(&schema);return false;}
        if(d.phase==2){page_free(&d.meta_schema);d.meta_schema=schema;clear_input();d.phase=3;return send_sql(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.sheets"),NULL,256,false);}
        page_free(&d.sheets_schema);d.sheets_schema=schema;clear_input();
        CHECK(canonical_schema(&d.meta_schema,workbook_codec_table_find("meta"))&&canonical_schema(&d.sheets_schema,workbook_codec_table_find("sheets")),"Canonical metadata schema failed");
        CHECK(begin_read(),"Could not stage metadata reader");d.phase=4;
    }
    if(d.phase==4){
        if(d.fault==10){d.phase=8;return send_sql(dx_strdup("DESCRIBE SELECT CAST(version AS VARCHAR) AS version,active_sheet,user_scale FROM _duxcel_workspace.meta"),NULL,256,false);}
        d.phase=5;return send_family(false,false);
    }
    if(d.phase==5){
        CHECK(d.ready&&!d.error,d.error?d.error:"Meta result missing");
        bool reject=d.fault==1||d.fault==2;
        CHECK(feed_page(false,reject),"Meta feed failed");clear_input();
        if(reject){CHECK(next_fault(),"Restart after meta rejection failed");d.phase=4;return true;}
        d.phase=6;return send_family(true,d.fault==9&&!d.stale_released);
    }
    if(d.phase==6){
        if(d.fault==9&&!d.stale_released){
            if(!d.held_ready)return true;
            CHECK(!d.ready,"Held result unexpectedly entered reader");
            d.cookie++;cancel_candidate();d.waiting=false;d.stale_released=true;
            unsigned stale=d.stale;release_held();CHECK(d.stale==stale+1&&!d.ready&&prior_unchanged(),"Superseded actual callback decoded/published staged state");
            return send_family(true,false);
        }
        CHECK(d.ready,"Sheets result missing");
        if(d.fault==7){
            CHECK(d.error&&strstr(d.error,"NUL"),"Real embedded-NUL metadata did not fail existing wire boundary");
            const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
            CHECK(request&&request->family==WORKBOOK_CODEC_SHEETS&&!request->first_row,"Wire rejection changed reader progress");
            CHECK(prior_unchanged(),"Wire failure changed retained Header");d.rejections++;CHECK(next_fault(),"Restart after wire rejection failed");d.phase=4;return true;
        }
        CHECK(!d.error,d.error?d.error:"Sheets transport failed");
        bool reject=(d.fault>=3&&d.fault<=8)||d.fault==12||d.fault==13;
        CHECK(feed_page(true,reject),"Sheets exact feed failed");
        if(reject){CHECK(next_fault(),"Restart after sheets rejection failed");d.phase=4;return true;}
        clear_input();CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete Header move failed");
        CHECK(header_expected(),"Header fixture comparison failed");d.headers++;
        WorkbookCodecHeader *saved=d.header;
        CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==saved,"Header was moved twice or old output corrupted");free(d.error);d.error=NULL;
        if(!d.fault){
            d.fixture++;
            if(d.fixture<7){page_free(&d.meta_schema);page_free(&d.sheets_schema);d.phase=0;return true;}
            d.fixture=6;d.fault=1;CHECK(begin_read(),"Fault reader setup failed");d.phase=4;return true;
        }
        if(d.fault<14){CHECK(next_fault(),"Recovery reader setup failed");d.phase=4;return true;}
        CHECK(d.fixtures==7&&d.headers==10&&d.rejections==11&&d.stale==1&&d.queries==54&&d.meta_queries==20&&d.sheets_queries==19&&d.describe_queries==15&&d.u64_flags==3&&d.budget_rejected&&d.sentinel_rejected&&d.tiny_negative_rejected,"Terminal fixture/fault/query inventory changed");
        for(size_t i=0;i<7;i++)CHECK(d.normal_queries[i]==4,"Normal fixture metadata query count changed");
        d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;
        fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
    }
    if(d.phase==8){
        CHECK(d.ready&&!d.error,d.error?d.error:"Wrong-type schema proof missing");
        Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),"Wrong-type real DESCRIBE malformed");
        bool accepted=schema_matches(&schema,workbook_codec_table_find("meta"),3);page_free(&schema);
        CHECK(!accepted,"Original VARCHAR version was accepted as INTEGER");
        CHECK(workbook_codec_read_request(d.read)->family==WORKBOOK_CODEC_META,"Rejected source schema advanced shared Header cursor");
        CHECK(prior_unchanged(),"Original schema rejection changed retained Header");d.rejections++;CHECK(next_fault(),"Final recovery setup failed");d.phase=4;return true;
    }
    if(d.phase==9&&d.fixture_ready){
        clear_input();page_free(&d.meta_schema);page_free(&d.sheets_schema);workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;
        d.done=true;printf("PASS browser-codec-read: %u checks, 7 immutable native fixtures, 10 headers, 11 atomic rejections, 1 real stale callback, 54 queries; no Workbook publication\n",d.checks);
        report(true,NULL);emscripten_cancel_main_loop();return true;
    }
    return true;
}
#include "test_browser_codec_sorts.inc"
#include "test_browser_codec_preferences.inc"
#include "test_browser_codec_objects.inc"
#include "test_browser_codec_queries.inc"
#include "test_browser_codec_options.inc"
#include "test_browser_codec_tables.inc"
#include "test_browser_codec_pivot_marks.inc"
#include "test_browser_codec_filters.inc"
#include "test_browser_codec_grid.inc"
#include "test_browser_codec_counters.inc"
static void tick(void) {
    if(d.done)return;
    if(sorts_mode){sorts_tick();return;}
    if(preferences_mode){preferences_tick();return;}
    if(objects_mode){objects_tick();return;}
    if(queries_mode){queries_tick();return;}
    if(options_mode){options_tick();return;}
    if(tables_mode){tables_tick();return;}
    if(pm_mode){pm_tick();return;}
    if(filters_mode){filters_tick();return;}
    if(grid_mode){grid_tick();return;}
    if(counters_mode){counters_tick();return;}
    // A held response has completed official execution and left EngineHost's
    // active submission before the fixture event allows its stale replay.
    if(d.phase==6&&d.fault==9&&d.held_ready&&!d.stale_released)d.waiting=false;
    tick_body();
}
EMSCRIPTEN_KEEPALIVE void dx_browser_codec_fixture_result(unsigned cookie,unsigned ticket,int event,const char *epoch,const char *registration,const char *sha,size_t bytes,const char *failure) {
    if(d.done||cookie!=d.cookie||ticket!=d.ticket){d.stale++;return;}
    if(failure){stop(failure,__LINE__);return;}
    if(event==1){d.held_ready=true;return;}
    if(event!=0){stop("Unknown fixture transport event",__LINE__);return;}
    d.waiting=false;d.fixture_ready=true;
    if(d.phase==9)return;
    uint64_t epoch_value=0,registration_value=0;
    if(!u64(epoch,&epoch_value)||!epoch_value||!u64(registration,&registration_value)||!registration_value||!sha||strlen(sha)!=64||!bytes){stop("Malformed immutable fixture token",__LINE__);return;}
    d.fixtures++;
    memcpy(d.epoch,epoch,strlen(epoch)+1);memcpy(d.registration,registration,strlen(registration)+1);memcpy(d.sha,sha,65);
    free(d.bindings);d.bindings=dx_printf("[{\"name\":\"%s\",\"engineEpoch\":\"%s\",\"registrationId\":\"%s\"}]",counters_mode?counters_files[d.fixture]:grid_mode?grid_files[d.fixture]:filters_mode?filters_files[d.fixture]:pm_mode?pm_files[d.fixture]:tables_mode?tables_files[d.fixture]:options_mode?options_files[d.fixture]:queries_mode?queries_files[d.fixture]:objects_mode?objects_files[d.fixture]:preferences_mode?preferences_files[d.fixture]:sorts_mode?sorts_files[d.fixture]:files[d.fixture],d.epoch,d.registration);
}
EMSCRIPTEN_KEEPALIVE void dx_browser_codec_result(unsigned cookie,unsigned ticket,const void *wire,size_t size,const char *total,int pending,const char *failure) {
    // Superseded callbacks are ignored before looking at any borrowed buffer,
    // including the error and total pointers. The host retires their ownership.
    if(d.done||cookie!=d.cookie||ticket!=d.ticket){if(counters_mode&&!d.done&&!counters_stale(size))return;d.stale++;return;}
    if(!d.waiting){stop("Duplicate active codec callback",__LINE__);return;}
    d.callbacks++;d.waiting=false;d.ready=true;
    if(failure){free(d.error);d.error=dx_strdup(failure);return;}
    if((filters_mode&&filters_wire)||(pm_mode&&pm_wire)||table_wire()||object_wire()||owner_wire()||((sorts_mode||filters_mode)&&sorts_wire)){
        const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
        WorkbookCodecFamily expected=filters_mode&&filters_wire?WORKBOOK_CODEC_FILTERS:pm_mode&&pm_wire?(pm_wire==7?WORKBOOK_CODEC_PIVOT_FIELDS:WORKBOOK_CODEC_MARKS):table_wire()?(tables_wire==4?WORKBOOK_CODEC_TABLE_SOURCES:tables_wire==5?WORKBOOK_CODEC_TABLE_FILTERS:WORKBOOK_CODEC_TABLE_SORTS):owner_wire()?((options_mode||tables_mode||pm_mode||filters_mode)&&options_wire?WORKBOOK_CODEC_SOURCE_OPTIONS:WORKBOOK_CODEC_QUERIES):object_wire()?WORKBOOK_CODEC_OBJECTS:WORKBOOK_CODEC_SORTS;
        if(!request||request->family!=expected){stop("Trusted metadata wire policy has no matching active cursor",__LINE__);return;}
    }
    uint64_t count=0;if(!u64(total,&count)){stop("Noncanonical callback total",__LINE__);return;}
    size_t owned=retained()+workbook_codec_read_owned_bytes(d.read);
    if(size>DX_PAGE_WIRE_BYTES||owned>INPUT_LIMIT||size>INPUT_LIMIT-owned){stop("Callback wire/retained ownership exceeds test admission",__LINE__);return;}
    size_t input=owned+size+text_bytes(total);if(input>d.peak_input)d.peak_input=input;
    bool decoded=false;
    if(input<=INPUT_LIMIT)decoded=filters_mode&&filters_wire?
        page_from_wire_workbook_filters_owned(&d.page,wire,size,0,count,pending!=0,INPUT_LIMIT-input,&d.error):pm_mode&&pm_wire==7?
        page_from_wire_workbook_pivot_fields_owned(&d.page,wire,size,0,count,pending!=0,INPUT_LIMIT-input,&d.error):pm_mode&&pm_wire==8?
        page_from_wire_workbook_marks_owned(&d.page,wire,size,0,count,pending!=0,INPUT_LIMIT-input,&d.error):table_wire()&&tables_wire==4?
        page_from_wire_workbook_table_sources_owned(&d.page,wire,size,0,count,pending!=0,INPUT_LIMIT-input,&d.error):table_wire()&&tables_wire==5?
        page_from_wire_workbook_table_filters_owned(&d.page,wire,size,0,count,pending!=0,INPUT_LIMIT-input,&d.error):table_wire()&&tables_wire==6?
        page_from_wire_workbook_table_sorts_owned(&d.page,wire,size,0,count,pending!=0,INPUT_LIMIT-input,&d.error):owner_wire()?
        page_from_wire_workbook_owners_owned(&d.page,wire,size,0,count,pending!=0,INPUT_LIMIT-input,&d.error):object_wire()?
        page_from_wire_workbook_objects_owned(&d.page,wire,size,0,count,pending!=0,INPUT_LIMIT-input,&d.error):(sorts_mode||filters_mode)&&sorts_wire?
        page_from_wire_workbook_sorts_owned(&d.page,wire,size,0,count,pending!=0,INPUT_LIMIT-input,&d.error):
        page_from_wire_owned(&d.page,wire,size,0,count,pending!=0,INPUT_LIMIT-input,&d.error);
    if(!decoded){stop(d.error?d.error:"Existing wire decoder rejected metadata",__LINE__);return;}
    // The incoming wire still coexists with the newly owned Page here. The
    // decoder's preflight admitted their sum, including any old destination.
    if(input+d.page.bytes>d.peak_input)d.peak_input=input+d.page.bytes;
}
EM_JS(int, codec_sorts_mode,(void),{return Module['dxCodecSortsMode']?1:0;})
EM_JS(int, codec_preferences_mode,(void),{return Module['dxCodecPreferencesMode']?1:0;})
EM_JS(int, codec_objects_mode,(void),{return Module['dxCodecObjectsMode']?1:0;})
EM_JS(int, codec_queries_mode,(void),{return Module['dxCodecQueriesMode']?1:0;})
EM_JS(int, codec_options_mode,(void),{return Module['dxCodecSourceOptionsMode']?1:0;})
EM_JS(int, codec_tables_mode,(void),{return Module['dxCodecTablesMode']?1:0;})
EM_JS(int, codec_pm_mode,(void),{return Module['dxCodecPivotMarksMode']?1:0;})
EM_JS(int, codec_filters_mode,(void),{return Module['dxCodecFiltersMode']?1:0;})
EM_JS(int, codec_counters_mode,(void),{return Module['dxCodecCountersMode']?1:0;})
EM_JS(int, codec_grid_mode,(void),{return Module['dxCodecGridMode']?1:0;})
int main(void) {
    counters_mode=codec_counters_mode()!=0;grid_mode=codec_grid_mode()!=0;filters_mode=codec_filters_mode()!=0;pm_mode=codec_pm_mode()!=0;tables_mode=codec_tables_mode()!=0;sorts_mode=codec_sorts_mode()!=0;preferences_mode=codec_preferences_mode()!=0;objects_mode=codec_objects_mode()!=0;queries_mode=codec_queries_mode()!=0;options_mode=codec_options_mode()!=0;d.cookie=40;d.ticket=100;
    if((unsigned)sorts_mode+(unsigned)preferences_mode+(unsigned)objects_mode+(unsigned)queries_mode+(unsigned)options_mode+(unsigned)tables_mode+(unsigned)pm_mode+(unsigned)filters_mode+(unsigned)grid_mode+(unsigned)counters_mode>1){stop("Ambiguous codec acceptance modes",__LINE__);return 1;}
    emscripten_set_main_loop(tick,60,0);return 0;
}
