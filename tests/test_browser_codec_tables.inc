/* Actual official-Worker Table definitions. This component never executes
   their decoy source SQL or publishes a Workbook/source runtime. */
typedef struct {
    unsigned executions[6],feeds[9],groups[6],poisoned[6],rejected_first[6];
    unsigned output_fixture,prior_fixture,rejected_feeds,schema_rejections,proofs;
    size_t maximum_header,pending_owned;
    uint64_t prior_identity,detached_identity;
    const char *pending_sql;
    bool held_released;
} TablesState;
static TablesState tables;
static size_t tables_retained(void) {return sizeof(tables);}
static bool tables_observe(size_t extra) {
    size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
    CHECK(current<=INPUT_LIMIT&&extra<=INPUT_LIMIT-current,"Concrete Table-chain C buffers exceed input admission");
    if(current+extra>d.peak_input)d.peak_input=current+extra;return true;
}
EM_JS(void, report_tables,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned requests,unsigned rejections,unsigned stale,size_t peak,size_t header_peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned describe,const unsigned *executions,const unsigned *feeds,const unsigned *groups,const unsigned *poisoned,unsigned rejected,unsigned schema_rejections,unsigned proofs,const unsigned *first_rows,const char *error),{
    const read=(p,n)=>Array.from(HEAPU32.subarray(p>>>2,(p>>>2)+n));
    const e=read(executions,6),f=read(feeds,9),g=read(groups,6),p=read(poisoned,6);
    const families=a=>({objects:a[0],sourceOptions:a[1],queries:a[2],tableSources:a[3],tableFilters:a[4],tableSorts:a[5]});
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'table-sources',checks,fixtures,headers,queries:requests,rejections,stale,
        peakOwnedInput:peak,maximumCompletedHeaderOwned:header_peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:read(normal,4),familyExecutions:{meta,sheets,...families(e),describe},
        acceptedFeeds:{objects:f[0],sourceOptions:f[1],queries:f[2],tableSources:f[3],tableFilters256:f[4],tableFiltersTail:f[5],tableSorts256:f[6],tableSorts192:f[7],tableSortsEmpty:f[8]},
        completedResults:families(g),poisonedResults:families(p),rejectedFeeds:rejected,schemaRejections:schema_rejections,rejectedFirstRows:read(first_rows,6),
        requiredNullRejected:Boolean(proofs&1),wrongOwnerRejected:Boolean(proofs&2),sourceKindRejected:Boolean(proofs&4),lateFilterRejected:Boolean(proofs&8),lateSortRejected:Boolean(proofs&16),budgetRejected:Boolean(proofs&32),originalTypeRejected:Boolean(proofs&64),
        error:error?UTF8ToString(error):null,boundary:'official-worker/shared-C-complete Table definition Header; owned raw refs/unavailable browser locators/query/flags/filters/sorts, no source SQL execution, complete Workbook publication or browser Save/Open'});
})
static void tables_report(bool ok,const char *error) {
    report_tables(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.peak_input,tables.maximum_header,d.normal_queries,d.meta_queries,d.sheets_queries,d.describe_queries,
        tables.executions,tables.feeds,tables.groups,tables.poisoned,tables.rejected_feeds,tables.schema_rejections,tables.proofs,tables.rejected_first,error);
}
static uint64_t tables_id(unsigned fixture,size_t sheet,size_t index) {
    return (fixture==3&&sheet==1&&index==31)||(fixture==2&&index==3)?UINT64_MAX:UINT64_C(9007199254740993)+index;
}
static bool tables_query_text(const char *text,size_t sheet,size_t index,bool primary) {
    char expected[160];
    if(primary)snprintf(expected,sizeof(expected),"SELECT * FROM __BP1_PRIMARY_MUST_NOT_EXECUTE /* 'İ' %zu */;  \n",sheet);
    else snprintf(expected,sizeof(expected),"SELECT * FROM __BP1_TABLE_MUST_NOT_EXECUTE /* 'İ' %zu:%zu */;  \n",sheet,index);
    CHECK(same(text,expected),"Stored decoy query bytes/quotes/semicolon/trailing spaces changed");return true;
}
static bool tables_header_contents(const WorkbookCodecHeader *h,unsigned fixture,const WorkbookCodecKey *expected) {
    bool large=fixture==3;size_t sheets=large?2:1,count=large?32:4;unsigned version=tables_versions[fixture];const WorkbookCodecKey *key=workbook_codec_header_key(h);
    CHECK(h&&workbook_codec_header_table_sources_ready(h)&&workbook_codec_header_objects_ready(h)&&workbook_codec_header_queries_ready(h)&&workbook_codec_header_source_options_ready(h)&&!workbook_codec_header_sorts_ready(h)&&!workbook_codec_header_preferences_ready(h),"Table-chain readiness/default families changed");
    CHECK(workbook_codec_header_version(h)==version&&workbook_codec_header_sheet_count(h)==sheets&&workbook_codec_header_active_sheet(h)==sheets-1&&workbook_codec_header_user_scale(h)==1.25f,"Table-chain package/viewport count changed");
    CHECK(key&&key->document_epoch==expected->document_epoch&&key->operation_ticket==expected->operation_ticket&&key->snapshot_generation==expected->snapshot_generation,"Table-chain Header key changed");
    static const char *const refs[]={"missing-source.csv","missing-source.parquet","missing-source.duckdb",""},*const schemas[]={NULL,"","NULL",NULL},*const named_tables[]={NULL,"NULL","","NULL"},*const values[]={NULL,"","NULL"};
    for(size_t sheet=0;sheet<sheets;sheet++){
        char name[96];snprintf(name,sizeof(name),"Table 'İ' %zu",sheet);CHECK(same(workbook_codec_header_name(h,sheet),name),"Table-chain sheet name/UTF-8 changed");
        const Analysis *primary=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);const char *query=NULL;bool unique=false;
        CHECK(primary&&primary->kind==SOURCE_CSV&&primary->path&&!*primary->path&&same(workbook_codec_header_reference(h,sheet),refs[0])&&same(primary->schema,"")&&same(primary->table,"NULL")&&!primary->source_version,"Primary raw reference/unavailable locator changed");
        CHECK(primary->unique_rows==(sheet%2!=0)&&!primary->filter_column&&!primary->filter_value&&!primary->filter_count&&!primary->group_column&&!primary->measure_column&&!primary->sort_column&&!primary->sort_count&&!primary->descending,"Primary query/options composition fabricated state");
        CHECK(workbook_codec_header_query(h,sheet,0,&query)&&query==primary->query&&tables_query_text(query,sheet,0,true)&&workbook_codec_header_unique_rows(h,sheet,0,&unique)&&unique==primary->unique_rows,"Primary query/option getters disagree with owned Analysis");
        CHECK(v&&v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==1.5f&&!v->columns&&!v->column_count,"Table-chain viewport changed");
        CHECK(workbook_codec_header_object_count(h,sheet)==count,"Table registry count changed");
        for(size_t j=0;j<count;j++){
            WorkbookCodecObjectView object={0};WorkbookCodecTableSourceView source={0};CHECK(workbook_codec_header_object_at(h,sheet,j,&object)&&workbook_codec_header_table_source(h,sheet,object.id,&source)&&source.analysis,"Complete Table descriptor/source missing");
            if(j)snprintf(name,sizeof(name),"Table 'İ' %zu:%zu",sheet,j);else snprintf(name,sizeof(name),"NULL");
            CHECK(object.id==tables_id(fixture,sheet,j)&&same(object.name,name)&&object.kind==OBJECT_TABLE&&object.mark==MARK_BAR&&!object.source_id&&!object.chart_measure&&object.row==10+j*12&&object.column==8&&object.height==8&&object.width==4,"Exact Table ID/name/geometry changed");
            const Analysis *a=source.analysis;SourceKind kind=large?SOURCE_CSV:(SourceKind)(SOURCE_CSV+j);
            CHECK(a->kind==kind&&a->path&&!*a->path&&source.reference&&source.reference!=a->path&&same(source.reference,refs[large?0:j])&&source.locator_available==(kind==SOURCE_DEMO)&&!a->source_version,"Raw Table reference was conflated with its unavailable locator or runtime proof");
            CHECK(same(a->schema,large?values[j%3]:schemas[j])&&same(a->table,large?(j%3==0?"":j%3==1?"NULL":NULL):named_tables[j]),"Table schema/table SQL NULL-empty-literal distinction changed");
            CHECK(a->unique_rows==((sheet+j+1)%2!=0)&&workbook_codec_header_unique_rows(h,sheet,object.id,&unique)&&unique==a->unique_rows,"Table option did not transfer exactly");
            CHECK(workbook_codec_header_query(h,sheet,object.id,&query)&&query==a->query&&tables_query_text(query,sheet,j,false),"Table query getter/allocation transfer changed");
            bool grouped=large?j%2!=0:j==1;CHECK(same(a->group_column,grouped?"Group 'İ'":NULL)&&same(a->measure_column,grouped?"Measure 'İ'":NULL)&&a->aggregate==(grouped?AGG_AVG:AGG_SUM),"Table grouping/aggregate changed");
            if(large){
                CHECK(same(a->filter_column,"P 'İ'")&&same(a->filter_value,values[(sheet+j)%3])&&a->filter_op==FILTER_CONTAINS&&a->filter_count==8&&same(a->sort_column,"S 'İ' 0")&&a->descending==((sheet+j)%2!=0)&&a->sort_count==7,"Large Table primary filter/sort changed");
            }else{
                if(j!=1)snprintf(name,sizeof(name),"P 'İ' %zu",j);
                CHECK(same(a->filter_column,j==1?NULL:name)&&same(a->filter_value,j==0||j==1?NULL:j==2?"":"NULL")&&a->filter_op==(j==0?FILTER_NULL:j==2?FILTER_EQUAL:FILTER_CONTAINS),"Small Table primary filter changed");
                const char *sort=j==1?"S 'İ' 0":j==2?"NULL":j==3?"S 'İ' 3":NULL;
                CHECK(same(a->sort_column,sort)&&a->descending==(j==1||j==3)&&!a->sort_count&&a->filter_count==(version==14&&!j?1u:0u),"Small Table sort/filter count changed");
            }
            CHECK(!a->filter_number.kind&&!a->filter_number.precision&&!a->filter_number.scale,"Persisted filter fabricated numeric source schema");
            for(size_t f=0;f<a->filter_count;f++){
                const AnalysisFilter *filter=a->filters+f;snprintf(name,sizeof(name),"F 'İ' %zu",f);
                CHECK(same(filter->column,large?name:"Large 'İ'")&&filter->op==(large&&f==7?FILTER_BETWEEN:FILTER_CONTAINS)&&!filter->number.kind&&!filter->number.precision&&!filter->number.scale,"Secondary Table filter name/operator changed");
                if(large)CHECK(same(filter->value,f==7?"[\"10\",\"20\"]":values[f%3]),"Secondary filter NULL-empty-literal/JSON bytes changed");
                else{CHECK(filter->value&&strlen(filter->value)==65537,"Valid65537-byte secondary value was capped to64KiB");for(size_t n=0;n<65537;n++)if(filter->value[n]!='x')CHECK(false,"Large secondary value bytes changed");}
            }
            for(size_t n=0;n<a->sort_count;n++){snprintf(name,sizeof(name),"S 'İ' %zu",n+1);CHECK(same(a->sorts[n].column,name)&&a->sorts[n].descending==((n+1)%2!=0),"Secondary Table sort order/descending changed");}
        }
    }
    WorkbookCodecTableSourceView invalid,saved;memset(&invalid,0x5a,sizeof(invalid));memcpy(&saved,&invalid,sizeof(saved));
    CHECK(!workbook_codec_header_table_source(h,sheets,1,&invalid)&&!memcmp(&invalid,&saved,sizeof(invalid))&&!workbook_codec_header_table_source(h,0,0,&invalid)&&!memcmp(&invalid,&saved,sizeof(invalid))&&!workbook_codec_header_table_source(h,0,1,&invalid)&&!memcmp(&invalid,&saved,sizeof(invalid)),"Invalid Table getter changed output or resolved a wrong owner");
    CHECK(!workbook_codec_header_table_source(h,0,tables_id(fixture,0,0),NULL)&&!workbook_codec_header_table_sources_ready(NULL),"Invalid Table output/readiness accepted");return true;
}
static uint64_t tables_header_identity(const WorkbookCodecHeader *h) {
    uint64_t value=UINT64_C(1469598103934665603);if(!h)return value;
    for(size_t s=0;s<workbook_codec_header_sheet_count(h);s++){
        const Analysis *primary=workbook_codec_header_analysis(h,s);value=(value^(uintptr_t)primary)*UINT64_C(1099511628211);value=(value^(uintptr_t)primary->query)*UINT64_C(1099511628211);
        for(size_t j=0;j<workbook_codec_header_object_count(h,s);j++){
            WorkbookCodecObjectView object={0};WorkbookCodecTableSourceView source={0};if(!workbook_codec_header_object_at(h,s,j,&object)||!workbook_codec_header_table_source(h,s,object.id,&source))return 0;
            const Analysis *a=source.analysis;const void *pointers[]={a,source.reference,a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
            for(size_t n=0;n<sizeof(pointers)/sizeof(*pointers);n++)value=(value^(uintptr_t)pointers[n])*UINT64_C(1099511628211);
            for(size_t n=0;n<a->filter_count;n++){value=(value^(uintptr_t)a->filters[n].column)*UINT64_C(1099511628211);value=(value^(uintptr_t)a->filters[n].value)*UINT64_C(1099511628211);}
            for(size_t n=0;n<a->sort_count;n++)value=(value^(uintptr_t)a->sorts[n].column)*UINT64_C(1099511628211);
        }
    }
    return value;
}
static bool tables_prior_unchanged(void) {
    CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes&&tables_header_identity(d.header)==tables.prior_identity,"Old Table Header/allocation identities changed");
    return !d.header||tables_header_contents(d.header,tables.prior_fixture,&d.prior_key);
}
static bool tables_begin(void) {tables.prior_fixture=tables.output_fixture;tables.prior_identity=tables_header_identity(d.header);return begin_read();}
static bool tables_send(char *sql,const Page *schema,unsigned limit,bool hold,unsigned policy) {
    sorts_wire=false;objects_wire=policy==2;queries_wire=policy==3&&schema==&d.queries_schema;options_wire=policy==3&&schema==&d.options_schema;tables_wire=policy>=4?policy:0;
    return send_sql(sql,schema,limit,hold);
}
static Page *tables_schema(unsigned index) {
    return index==0?&d.meta_schema:index==1?&d.sheets_schema:index==2?&d.objects_schema:index==3?&d.options_schema:index==4?&d.queries_schema:index==5?&d.table_sources_schema:index==6?&d.table_filters_schema:&d.table_sorts_schema;
}
static const char *tables_name(unsigned index) {static const char *const names[]={"meta","sheets","objects","source_options","queries","table_sources","table_filters","table_sorts"};return names[index];}
static unsigned tables_columns(unsigned index) {static const unsigned columns[]={3,25,11,3,3,14,6,5};return columns[index];}
static bool tables_send_family(WorkbookCodecFamily family,bool hold) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==family&&!request->first_row,"Unexpected/reexecuted Table-chain family");
    if(family==WORKBOOK_CODEC_META||family==WORKBOOK_CODEC_SHEETS){bool meta=family==WORKBOOK_CODEC_META;if(meta)d.meta_queries++;else d.sheets_queries++;return tables_send(browser_page_sql(request->sql,tables_schema(meta?0:1),0,&d.error),tables_schema(meta?0:1),(unsigned)request->row_limit,false,0);}
    unsigned index=family==WORKBOOK_CODEC_OBJECTS?2:family==WORKBOOK_CODEC_SOURCE_OPTIONS?3:family==WORKBOOK_CODEC_QUERIES?4:family==WORKBOOK_CODEC_TABLE_SOURCES?5:family==WORKBOOK_CODEC_TABLE_FILTERS?6:7;
    CHECK(family==WORKBOOK_CODEC_OBJECTS||family==WORKBOOK_CODEC_SOURCE_OPTIONS||family==WORKBOOK_CODEC_QUERIES||family==WORKBOOK_CODEC_TABLE_SOURCES||family==WORKBOOK_CODEC_TABLE_FILTERS||family==WORKBOOK_CODEC_TABLE_SORTS,"Unsupported Table-chain family");
    const unsigned limits[]={0,0,2049,2113,2113,2049,30721,14337};CHECK(request->row_limit==limits[index]&&request->column_count==tables_columns(index)&&request->source_column_count==tables_columns(index),"Table-chain typed wire envelope changed");
    char *relation=dx_strdup(request->sql),*changed=NULL;
    if(index==5){
        if(d.fault==1)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=1 AND object_id=18446744073709551615::UBIGINT THEN NULL::VARCHAR ELSE path END AS path) FROM (%s) AS _tables_null",relation);
        else if(d.fault==2)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND object_id=9007199254740993::UBIGINT THEN 0::UBIGINT ELSE object_id END AS object_id) FROM (%s) AS _tables_owner ORDER BY sheet_id,object_id",relation);
        else if(d.fault==3)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND object_id=9007199254740993::UBIGINT THEN 0::INTEGER ELSE kind END AS kind) FROM (%s) AS _tables_kind",relation);
    }else if(index==6&&d.fault==4)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=1 AND object_id=18446744073709551615::UBIGINT AND position=7 THEN 8::INTEGER ELSE position END AS position) FROM (%s) AS _tables_filter ORDER BY sheet_id,object_id,position",relation);
    else if(index==7&&d.fault==5)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=1 AND object_id=18446744073709551615::UBIGINT AND position=6 THEN 's ''İ'' 0' ELSE name END AS name) FROM (%s) AS _tables_sort",relation);
    if(changed){CHECK(tables_observe(text_bytes(relation)+text_bytes(changed)),"Table fault SQL coexistence exceeds admission");free(relation);relation=changed;}
    char *sql=dx_strdup("SELECT ");const Page *schema=tables_schema(index);
    for(size_t c=0;c<schema->column_count;c++)dx_append(&sql,"%sCAST(%s AS VARCHAR) AS %s",c?",":"",schema->columns[c].name,schema->columns[c].name);
    dx_append(&sql," FROM (%s) AS _tables_projection",relation);CHECK(strlen(sql)<=DX_BROWSER_SQL_BYTES&&tables_observe(text_bytes(relation)+2u*text_bytes(sql)),"Table canonical projection construction exceeds admission");free(relation);tables.executions[index-2]++;
    return tables_send(sql,schema,(unsigned)request->row_limit,hold,index==2?2:index<5?3:index-1);
}
static bool tables_atoms(unsigned index,size_t rows) {
    const Page *schema=tables_schema(index);const WorkbookCodecTable *table=workbook_codec_table_find(tables_name(index));
    CHECK(d.page.column_count==schema->column_count&&d.page.row_count==rows&&d.page.offset==0&&d.page.total==rows&&!d.page.count_pending,"Complete Table-family dimensions changed");
    CHECK(!d.atoms,"Previous Table atoms remain owned");size_t bytes=rows*schema->column_count*sizeof(*d.atoms);CHECK(tables_observe(bytes),"Table typed-atom staging exceeds input admission");d.atoms=dx_alloc(bytes);d.atom_bytes=bytes;const char *types=table->types;
    for(size_t c=0;c<schema->column_count;c++){
        while(*types==' ')types++;WorkbookCodecAtomKind kind=atom_kind(*types++);const Column *actual=d.page.columns+c,*expected=schema->columns+c;
        CHECK(same(actual->name,expected->name)&&same(actual->type_name,expected->type_name)&&actual->type==expected->type&&actual->number_type.kind==expected->number_type.kind&&actual->number_type.precision==expected->number_type.precision&&actual->number_type.scale==expected->number_type.scale,"Original Table SQL type sidecar changed before exact atom feed");d.columns[c]=(WorkbookCodecColumn){actual->name,kind};
        for(size_t row=0;row<rows;row++){
            const Cell *cell=d.page.cells+row*schema->column_count+c;WorkbookCodecAtom *atom=d.atoms+row*schema->column_count+c;atom->kind=cell->is_null?WORKBOOK_CODEC_NULL:kind;if(cell->is_null)continue;CHECK(cell->text,"Table atom has no exact text");
            if(kind==WORKBOOK_CODEC_I32)CHECK(i32(cell->text,&atom->value.i32),"Table INTEGER text was not exact/canonical");
            else if(kind==WORKBOOK_CODEC_U64)CHECK(u64(cell->text,&atom->value.u64),"Table UBIGINT text was not exact/canonical");
            else if(kind==WORKBOOK_CODEC_DOUBLE){char *end=NULL;atom->value.real=strtod(cell->text,&end);CHECK(end&&end!=cell->text&&!*end,"Table/base DOUBLE invalid");}
            else if(kind==WORKBOOK_CODEC_BOOLEAN){CHECK(!strcmp(cell->text,"true")||!strcmp(cell->text,"false"),"Table BOOLEAN text invalid");atom->value.boolean=!strcmp(cell->text,"true");}
            else atom->value.text=(WorkbookCodecText){cell->text,strlen(cell->text)};
        }
    }
    return true;
}
static bool tables_poison(unsigned group) {
    for(size_t row=0;row<d.page.row_count;row++)for(size_t c=0;c<d.page.column_count;c++){Cell *cell=d.page.cells+row*d.page.column_count+c;if(!cell->is_null){CHECK(cell->text,"Missing owned Table Page text before poison");memset(cell->text,'~',strlen(cell->text));}}
    if(d.atoms)memset(d.atoms,0,d.atom_bytes);tables.poisoned[group]++;return true;
}
static bool tables_feed_base(unsigned index) {
    bool meta=index==0;CHECK(tables_atoms(index,meta?1:d.fixture==3?2:1),"Table base adaptation failed");WorkbookCodecBatch batch={d.columns,d.page.column_count,d.atoms,d.page.row_count,0,true};CHECK(tables_observe(0),"Table base coexistence exceeds admission");
    CHECK(workbook_codec_read_feed(d.read,d.key,meta?WORKBOOK_CODEC_META:WORKBOOK_CODEC_SHEETS,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Table base metadata rejected");return true;
}
static bool tables_feed_family(unsigned index) {
    bool large=d.fixture==3;size_t rows=index==2?(large?64:4):index==3?(large?33:2):index==4?(large?66:5):index==5?(large?64:4):index==6?(large?512:d.fixture==2?1:0):(large?448:0);
    WorkbookCodecFamily family=index==2?WORKBOOK_CODEC_OBJECTS:index==3?WORKBOOK_CODEC_SOURCE_OPTIONS:index==4?WORKBOOK_CODEC_QUERIES:index==5?WORKBOOK_CODEC_TABLE_SOURCES:index==6?WORKBOOK_CODEC_TABLE_FILTERS:WORKBOOK_CODEC_TABLE_SORTS;
    CHECK(tables_atoms(index,rows),"Table family exact atoms failed");bool rejected=(index==5&&(d.fault==1||d.fault==2||d.fault==3||d.fault==6))||(index==6&&d.fault==4)||(index==7&&d.fault==5);size_t failed_first=index==5?0:256;
    if(index==5&&d.fault==6){size_t current=retained()+workbook_codec_read_owned_bytes(d.read);CHECK(current<HEADER_LIMIT-1u,"Table staging budget fixture has no concrete headroom");d.pressure_bytes=HEADER_LIMIT-current-1u;d.pressure=dx_alloc(d.pressure_bytes);memset(d.pressure,0,d.pressure_bytes);CHECK(tables_observe(0),"Table concrete source staging pressure exceeds input admission");}
    for(size_t first=0;first<rows||(!rows&&!first);){
        size_t count=rows-first>256?256:rows-first;const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==family&&request->first_row==first,"Table chunks reran SQL or lost accepted prefix");size_t owned=workbook_codec_read_owned_bytes(d.read);const char *sql=request->sql;
        WorkbookCodecBatch batch={d.columns,d.page.column_count,d.atoms+first*d.page.column_count,count,first,first+count==rows};CHECK(tables_observe(0),"Table result/feed coexistence exceeds admission");WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,family,&batch,retained(),&d.error);
        if(rejected&&first==failed_first){
            static const char *const reasons[]={"SQL NULL","ordered Table descriptor","source kind","contiguous positions","sort column","budget"};
            CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error&&strstr(d.error,reasons[d.fault-1]),d.error?d.error:"Expected reasoned Table rejection missing");
            CHECK(workbook_codec_read_owned_bytes(d.read)==owned&&workbook_codec_read_request(d.read)->first_row==first&&workbook_codec_read_request(d.read)->sql==sql,"Rejected Table batch changed bytes/cursor/SQL pointer");CHECK(tables_prior_unchanged(),"Failed Table candidate changed retained Header graph");
            CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Rejected Table candidate replaced independent Header");CHECK(tables_observe(0),"Table rejection diagnostic exceeds input admission");tables.rejected_first[d.fault-1]=(unsigned)first;tables.proofs|=1u<<(d.fault-1);tables.rejected_feeds++;d.rejections++;return true;
        }
        CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid Table metadata prefix rejected");
        if(index<=5)tables.feeds[index-2]++;else if(index==6){if(count==256)tables.feeds[4]++;else{CHECK(count==0||count==1,"Unexpected small Table filter tail");tables.feeds[5]++;}}
        else if(count==256)tables.feeds[6]++;else if(count==192)tables.feeds[7]++;else{CHECK(!count,"Unexpected Table sort tail");tables.feeds[8]++;}
        first+=count;if(!count)break;
    }
    CHECK(!rejected,"Expected Table rejection was not reached");tables.groups[index-2]++;return tables_poison(index-2);
}
static void tables_free_schemas(void) {for(unsigned i=0;i<8;i++)page_free(tables_schema(i));}
static bool tables_restart(void) {clear_input();d.fault++;tables_wire=0;objects_wire=false;queries_wire=false;options_wire=false;return tables_begin();}
static bool tables_tick_body(void) {
    if(d.phase==0){CHECK(d.fixture<4,"Table fixture index exceeds inventory");d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",tables_files[d.fixture]);fixture_start(d.cookie,d.ticket,tables_files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=601;return true;}
    if(d.phase==601){if(!d.fixture_ready)return true;d.phase=602;return tables_send(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false,0);}
    if(d.waiting)return true;
    if(d.phase>=602&&d.phase<=609){
        unsigned index=d.phase-602;CHECK(d.ready&&!d.error,d.error?d.error:"Original Table-chain DESCRIBE absent");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Original Table schema invalid");CHECK(tables_observe(schema.bytes),"Original Table schema exceeds input admission");
        if(!source_schema(&schema,workbook_codec_table_find(tables_name(index)),tables_columns(index))){page_free(&schema);return false;}Page *destination=tables_schema(index);page_free(destination);*destination=schema;clear_input();
        if(index<7){d.phase++;return tables_send(dx_printf("DESCRIBE SELECT * FROM _duxcel_workspace.%s",tables_name(index+1)),NULL,256,false,0);}
        for(unsigned i=0;i<8;i++)CHECK(canonical_schema(tables_schema(i),workbook_codec_table_find(tables_name(i))),"Table canonical sidecar construction failed");CHECK(tables_begin(),"Table Header staging failed");d.phase=610;
    }
    if(d.phase==610){d.phase=611;return tables_send_family(WORKBOOK_CODEC_META,false);}
    if(d.phase==611){CHECK(d.ready&&!d.error,d.error?d.error:"Table meta result absent");CHECK(tables_feed_base(0),"Table meta feed failed");clear_input();d.phase=612;return tables_send_family(WORKBOOK_CODEC_SHEETS,false);}
    if(d.phase==612){CHECK(d.ready&&!d.error,d.error?d.error:"Table sheets result absent");CHECK(tables_feed_base(1),"Table sheets feed failed");clear_input();CHECK(workbook_codec_read_add_objects(d.read,true,&d.error),d.error?d.error:"Table descriptors arm failed");d.phase=613;return tables_send_family(WORKBOOK_CODEC_OBJECTS,false);}
    if(d.phase==613){CHECK(d.ready&&!d.error,d.error?d.error:"Table descriptors result absent");CHECK(tables_feed_family(2),"Table descriptors feed failed");clear_input();CHECK(workbook_codec_read_add_source_options(d.read,true,&d.error),d.error?d.error:"Table options arm failed");d.phase=614;return tables_send_family(WORKBOOK_CODEC_SOURCE_OPTIONS,false);}
    if(d.phase==614){CHECK(d.ready&&!d.error,d.error?d.error:"Table options result absent");CHECK(tables_feed_family(3),"Table options feed failed");clear_input();CHECK(workbook_codec_read_add_queries(d.read,true,&d.error),d.error?d.error:"Table queries arm failed");d.phase=615;return tables_send_family(WORKBOOK_CODEC_QUERIES,false);}
    if(d.phase==615){
        CHECK(d.ready&&!d.error,d.error?d.error:"Table query result absent");CHECK(tables_feed_family(4),"Table queries feed failed");clear_input();CHECK(workbook_codec_read_add_table_sources(d.read,(WorkbookCodecTableSourcePresence){true,true,true},&d.error),d.error?d.error:"Table source chain arm failed");
        tables.pending_owned=workbook_codec_read_owned_bytes(d.read);tables.pending_sql=workbook_codec_read_request(d.read)->sql;
        if(d.fault==7){d.phase=619;return tables_send(dx_strdup("DESCRIBE SELECT * REPLACE (filter_op::VARCHAR AS filter_op) FROM _duxcel_workspace.table_sources"),NULL,256,false,0);}d.phase=616;return tables_send_family(WORKBOOK_CODEC_TABLE_SOURCES,false);
    }
    if(d.phase==619){
        CHECK(d.ready&&!d.error,d.error?d.error:"Wrong original Table INTEGER DESCRIBE absent");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),"Wrong Table type DESCRIBE invalid");CHECK(tables_observe(schema.bytes),"Wrong Table schema exceeds admission");bool rejected=!schema_matches(&schema,workbook_codec_table_find("table_sources"),14);page_free(&schema);CHECK(rejected,"Original VARCHAR filter_op was accepted as INTEGER");
        const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_TABLE_SOURCES&&!request->first_row&&request->sql==tables.pending_sql&&workbook_codec_read_owned_bytes(d.read)==tables.pending_owned&&tables_prior_unchanged(),"Rejected original Table type changed candidate/old Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Wrong Table type published candidate");d.rejections++;tables.schema_rejections++;tables.proofs|=64;CHECK(tables_restart(),"Table schema rejection recovery failed");d.phase=610;return true;
    }
    if(d.phase==616){CHECK(d.ready&&!d.error,d.error?d.error:"Table sources result absent");CHECK(tables_feed_family(5),"Table sources feed failed");if(d.fault==1||d.fault==2||d.fault==3||d.fault==6){CHECK(tables_restart(),"Table source rejection recovery failed");d.phase=610;return true;}clear_input();d.phase=617;return tables_send_family(WORKBOOK_CODEC_TABLE_FILTERS,false);}
    if(d.phase==617){CHECK(d.ready&&!d.error,d.error?d.error:"Table filters result absent");CHECK(tables_feed_family(6),"Table filters feed failed");if(d.fault==4){CHECK(tables_restart(),"Late Table filter recovery failed");d.phase=610;return true;}clear_input();tables.pending_owned=workbook_codec_read_owned_bytes(d.read);tables.pending_sql=workbook_codec_read_request(d.read)->sql;d.phase=618;return tables_send_family(WORKBOOK_CODEC_TABLE_SORTS,d.fault==8&&!tables.held_released);}
    if(d.phase==618){
        if(d.fault==8&&!tables.held_released){if(!d.held_ready)return true;CHECK(!d.ready,"Held Table sort result entered reader early");d.cookie++;cancel_candidate();d.waiting=false;tables.held_released=true;unsigned stale=d.stale;release_held();CHECK(d.stale==stale+1&&!d.ready&&tables_prior_unchanged(),"Superseded Table callback accessed/published staged wire");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_TABLE_SORTS&&!request->first_row&&request->sql==tables.pending_sql&&workbook_codec_read_owned_bytes(d.read)==tables.pending_owned,"Stale Table callback changed accepted source/filter ownership");return tables_send_family(WORKBOOK_CODEC_TABLE_SORTS,false);}
        CHECK(d.ready&&!d.error,d.error?d.error:"Table sorts result absent");CHECK(tables_feed_family(7),"Table sorts feed failed");if(d.fault==5){CHECK(tables_restart(),"Late Table sort recovery failed");d.phase=610;return true;}clear_input();CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete Table source Header did not move");tables.output_fixture=d.fixture;CHECK(tables_header_contents(d.header,d.fixture,&d.key),"Owned Table definitions differ from native fixture");d.headers++;size_t bytes=workbook_codec_header_owned_bytes(d.header);CHECK(bytes<=HEADER_LIMIT,"Completed Table Header exceeds its limit");if(bytes>tables.maximum_header)tables.maximum_header=bytes;
        if(!d.fault){d.fixture++;if(d.fixture<4){tables_free_schemas();d.phase=0;return true;}d.fixture=3;d.fault=1;CHECK(tables_begin(),"Table negative candidate begin failed");d.phase=610;return true;}
        if(d.fault==8){CHECK(tables_restart(),"Final Table recovery begin failed");d.phase=610;return true;}
        CHECK(d.fault==9&&d.fixtures==4&&d.headers==6&&d.queries==126&&d.rejections==7&&d.stale==1&&d.meta_queries==13&&d.sheets_queries==13&&d.describe_queries==33,"Table-chain terminal request inventory changed");
        const unsigned executions[]={13,13,13,12,8,8},feeds[]={13,13,13,8,9,3,4,3,3},groups[]={13,13,13,8,7,6},first[]={0,0,0,256,256,0};
        for(size_t i=0;i<6;i++)CHECK(tables.executions[i]==executions[i]&&tables.groups[i]==groups[i]&&tables.poisoned[i]==groups[i]&&tables.rejected_first[i]==first[i],"Table family executions/owned poison/failure prefixes changed");for(size_t i=0;i<9;i++)CHECK(tables.feeds[i]==feeds[i],"Table feed inventory changed");for(size_t i=0;i<4;i++)CHECK(d.normal_queries[i]==16,"Normal Table fixture request count changed");
        CHECK(tables.rejected_feeds==6&&tables.schema_rejections==1&&tables.proofs==127,"Table reasoned rejection evidence incomplete");tables.detached_identity=tables_header_identity(d.header);d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
    }
    if(d.phase==9&&d.fixture_ready){clear_input();CHECK(tables_header_contents(d.header,3,&d.key)&&tables_header_identity(d.header)==tables.detached_identity,"Complete Table graph changed after Page/catalog close");tables_free_schemas();workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;d.done=true;printf("PASS: browser-codec-table-sources %u checks,4 native files,6 Headers,126 queries,6 codec/1 schema rejections,1 stale callback; complete owned Table metadata only, no source execution or Workbook publication\n",d.checks);tables_report(true,NULL);emscripten_cancel_main_loop();return true;}
    return true;
}
static void tables_tick(void) {if(d.phase==618&&d.fault==8&&d.held_ready&&!tables.held_released)d.waiting=false;tables_tick_body();}
