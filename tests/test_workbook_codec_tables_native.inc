/* Focused Table source/filter/sort comparison. Definitions are metadata only;
   neither the missing source files nor any stored SQL are executed. */
#define TABLE_BASE UINT64_C(9007199254740993)
#define TABLE_HEADER_LIMIT (4u*1024u*1024u)
static uint64_t table_fixture_id(unsigned version,bool large,size_t sheet,size_t table) {
    return (large&&sheet==1&&table==31)||(!large&&version==14&&table==3) ? UINT64_MAX : TABLE_BASE+table;
}
static char *table_fixture_query(size_t sheet,size_t table,bool primary) {
    return primary ? dx_printf("SELECT * FROM __BP1_PRIMARY_MUST_NOT_EXECUTE /* 'İ' %zu */;  \n",sheet) :
        dx_printf("SELECT * FROM __BP1_TABLE_MUST_NOT_EXECUTE /* 'İ' %zu:%zu */;  \n",sheet,table);
}
static Analysis table_fixture_source(const char *workspace,unsigned version,bool large,size_t sheet,size_t table) {
    static const char *const references[]={"missing-source.csv","missing-source.parquet","missing-source.duckdb",""};
    static const char *const small_schemas[]={NULL,"","NULL",NULL},*const small_tables[]={NULL,"NULL","","NULL"};
    static const char *const schemas[]={NULL,"","NULL"},*const tables[]={"","NULL",NULL},*const values[]={NULL,"","NULL"};
    SourceKind kind=large?SOURCE_CSV:(SourceKind)(SOURCE_CSV+table);
    Analysis a={.kind=kind,.path=kind==SOURCE_DEMO?dx_strdup(""):workbook_source_path(workspace,references[large?0:table]),
        .schema=dx_strdup(large?schemas[table%3]:small_schemas[table]),.table=dx_strdup(large?tables[table%3]:small_tables[table]),
        .query=table_fixture_query(sheet,table,false),.unique_rows=(sheet+table+1)%2!=0};
    CHECK(a.path&&(kind==SOURCE_DEMO||access(a.path,F_OK)!=0));
    if(large){
        a.filter_column=dx_strdup("P 'İ'");a.filter_value=dx_strdup(values[(sheet+table)%3]);a.filter_op=FILTER_CONTAINS;
        a.filter_count=8;
        for(size_t f=0;f<8;f++)a.filters[f]=(AnalysisFilter){dx_printf("F 'İ' %zu",f),dx_strdup(f==7?"[\"10\",\"20\"]":values[f%3]),f==7?FILTER_BETWEEN:FILTER_CONTAINS,{0}};
        a.sort_column=dx_strdup("S 'İ' 0");a.descending=(sheet+table)%2!=0;a.sort_count=7;
        for(size_t n=0;n<7;n++)a.sorts[n]=(AnalysisSort){dx_printf("S 'İ' %zu",n+1),(n+1)%2!=0};
    }else{
        if(table!=1){a.filter_column=dx_printf("P 'İ' %zu",table);a.filter_op=table==0?FILTER_NULL:table==2?FILTER_EQUAL:FILTER_CONTAINS;a.filter_value=dx_strdup(table==0?NULL:table==2?"":"NULL");}
        if(table==1||table==3){a.sort_column=dx_printf("S 'İ' %zu",table==1?0:3);a.descending=true;}
        if(table==2)a.sort_column=dx_strdup("NULL");
        if(version==14&&table==0){char *long_value=dx_alloc(65538);memset(long_value,'x',65537);long_value[65537]=0;a.filters[0]=(AnalysisFilter){dx_strdup("Large 'İ'"),long_value,FILTER_CONTAINS,{0}};a.filter_count=1;}
    }
    if((large&&table%2)||(!large&&table==1)){a.group_column=dx_strdup("Group 'İ'");a.measure_column=dx_strdup("Measure 'İ'");a.aggregate=AGG_AVG;}
    return a;
}
static void tables_make_fixture(const char *path,const char *missing,unsigned version,bool large,bool empty) {
    Workbook book;workbook_init(&book);char *error=NULL;size_t sheets=large?2:1,count=empty?0:large?32:4;
    for(size_t sheet=0;sheet<sheets;sheet++){
        Analysis primary={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=table_fixture_query(sheet,0,true),.unique_rows=sheet%2!=0};
        char *name=dx_printf("Table 'İ' %zu",sheet);CHECK(workbook_add(&book,name,&primary,&error));free(name);analysis_free(&primary);Worksheet *s=book.sheets+sheet;
        s->view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
        s->objects.items=count?dx_alloc(count*sizeof(*s->objects.items)):NULL;s->objects.count=count;
        for(size_t j=0;j<count;j++)s->objects.items[j]=(SheetObject){.id=table_fixture_id(version,large,sheet,j),.row=10+j*12,.rows=8,.col=8,.columns=4,.revision=1,.kind=OBJECT_TABLE,.mark=MARK_BAR,
            .name=j?dx_printf("Table 'İ' %zu:%zu",sheet,j):dx_strdup("NULL"),.source=table_fixture_source(path,version,large,sheet,j)};
        s->objects.next_id=count?table_fixture_id(version,large,sheet,count-1):0;
    }
    book.active=sheets-1;book.user_scale=1.25f;if(version==13)book.sheets[0].objects.next_id=TABLE_BASE+100;if(version==14)book.sheets[0].formats.source_aware=true;
    CHECK(workbook_save(&book,path,&error)&&!error);workbook_free(&book);
}
static void tables_fixture(const char *path,const char *missing,unsigned version) {
    /* The generic emitter calls the factory before its reorder stage. Its
       private stage basename does not identify the output, so use a dedicated
       large factory only for the reordered entry below. */
    tables_make_fixture(path,missing,version,false,false);
}
static void tables_large_fixture(const char *path,const char *missing,unsigned version) {tables_make_fixture(path,missing,version,true,false);}
static const EmitFixture emit_tables_fixtures[]={
    {"tables-v12.dxls",12,25,false},{"tables-v13.dxls",13,25,false},
    {"tables-v14.dxls",14,25,false},{"tables-v14-reordered.dxls",14,25,true}
};
static const char tables_reorder_sql[]=
    "CREATE TABLE _duxcel_workspace.reordered_table_sources AS SELECT descending,* EXCLUDE(descending) FROM _duxcel_workspace.table_sources; DROP TABLE _duxcel_workspace.table_sources; ALTER TABLE _duxcel_workspace.reordered_table_sources RENAME TO table_sources;"
    "CREATE TABLE _duxcel_workspace.reordered_table_filters AS SELECT value,op,name,position,object_id,sheet_id FROM _duxcel_workspace.table_filters; DROP TABLE _duxcel_workspace.table_filters; ALTER TABLE _duxcel_workspace.reordered_table_filters RENAME TO table_filters;"
    "CREATE TABLE _duxcel_workspace.reordered_table_sorts AS SELECT descending,name,position,object_id,sheet_id FROM _duxcel_workspace.table_sorts; DROP TABLE _duxcel_workspace.table_sorts; ALTER TABLE _duxcel_workspace.reordered_table_sorts RENAME TO table_sorts";
/* Only this four-file CLI uses a sequence: the publisher's existing callback
   runs once per entry, before publication. Other modes/factories are unchanged. */
static size_t tables_emit_index;
static void tables_emit_fixture(const char *path,const char *missing,unsigned version) {
    CHECK(tables_emit_index<4&&emit_tables_fixtures[tables_emit_index].version==version);
    bool large=tables_emit_index++==3;tables_make_fixture(path,missing,version,large,false);
}
static int emit_tables(const char *directory) {
    char reorder[6144];int n=snprintf(reorder,sizeof(reorder),"%s;%s;%s;%s",objects_reorder_sql,queries_reorder_sql,options_reorder_sql,tables_reorder_sql);CHECK(n>0&&(size_t)n<sizeof(reorder));
    tables_emit_index=0;return emit_fixture_files(directory,emit_tables_fixtures,4,tables_emit_fixture,reorder,NULL);
}
typedef struct {OptionHeaderStamp options;uint64_t digest;} TableHeaderStamp;
static uint64_t table_hash_text(uint64_t hash,const char *text) {
    hash=object_hash_word(hash,text!=NULL);if(text)for(const unsigned char *p=(const unsigned char *)text;;p++){hash^=*p;hash*=UINT64_C(1099511628211);if(!*p)break;}return hash;
}
static uint64_t table_hash_analysis(uint64_t hash,const Analysis *a) {
    const char *texts[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
    for(size_t i=0;i<sizeof(texts)/sizeof(*texts);i++)hash=table_hash_text(hash,texts[i]);
    const uint64_t words[]={a->kind,a->source_version,a->unique_rows,a->filter_op,a->aggregate,a->descending,a->filter_count,a->sort_count};
    for(size_t i=0;i<sizeof(words)/sizeof(*words);i++)hash=object_hash_word(hash,words[i]);
    for(size_t i=0;i<a->filter_count;i++){hash=table_hash_text(hash,a->filters[i].column);hash=table_hash_text(hash,a->filters[i].value);hash=object_hash_word(hash,a->filters[i].op);}
    for(size_t i=0;i<a->sort_count;i++){hash=table_hash_text(hash,a->sorts[i].column);hash=object_hash_word(hash,a->sorts[i].descending);}return hash;
}
static TableHeaderStamp tables_stamp(WorkbookCodecHeader *header) {
    TableHeaderStamp stamp={options_stamp(header),UINT64_C(1469598103934665603)};bool ready=workbook_codec_header_table_sources_ready(header);stamp.digest=object_hash_word(stamp.digest,ready);
    if(ready)for(size_t s=0;s<workbook_codec_header_sheet_count(header);s++)for(size_t j=0;j<workbook_codec_header_object_count(header,s);j++){
        WorkbookCodecObjectView o={0};CHECK(workbook_codec_header_object_at(header,s,j,&o));if(o.kind!=OBJECT_TABLE)continue;
        WorkbookCodecTableSourceView source={0};CHECK(workbook_codec_header_table_source(header,s,o.id,&source)&&source.analysis);stamp.digest=table_hash_analysis(stamp.digest,source.analysis);stamp.digest=table_hash_text(stamp.digest,source.reference);stamp.digest=object_hash_word(stamp.digest,source.locator_available);
    }
    return stamp;
}
static void kept_tables_header(WorkbookCodecHeader *header,TableHeaderStamp before) {
    TableHeaderStamp after=tables_stamp(header);CHECK(after.options.queries.objects.identity==before.options.queries.objects.identity&&after.options.queries.objects.bytes==before.options.queries.objects.bytes&&after.options.queries.objects.digest==before.options.queries.objects.digest&&after.options.queries.digest==before.options.queries.digest&&after.options.digest==before.options.digest&&after.digest==before.digest);
}
static void pending_tables_header(WorkbookCodecRead *read,WorkbookCodecHeader **out) {
    TableHeaderStamp before=tables_stamp(*out);char *error=NULL;CHECK(!workbook_codec_read_finish(read,out,&error));clear_error(&error);kept_tables_header(*out,before);
}
static WorkbookCodecRead *tables_begin(duckdb_connection connection,unsigned version,const char *target,size_t retained) {
    WorkbookCodecRead *read=NULL;char *error=NULL;CHECK(workbook_codec_read_begin(&read,key,version,WORKBOOK_CODEC_SHEETS_25,TABLE_HEADER_LIMIT,resolve,(void *)target,&error));
    CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_objects(read,true,&error)&&result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_queries(read,true,&error)&&result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_source_options(read,true,&error)&&result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);return read;
}
static void table_analysis_compare(const Analysis *a,const Analysis *b) {
    CHECK(a&&b&&a->kind==b->kind&&a->source_version==b->source_version&&a->unique_rows==b->unique_rows&&a->filter_op==b->filter_op&&a->aggregate==b->aggregate&&a->descending==b->descending&&a->filter_count==b->filter_count&&a->sort_count==b->sort_count);
    const char *left[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
    const char *right[]={b->path,b->schema,b->table,b->query,b->filter_column,b->filter_value,b->group_column,b->measure_column,b->sort_column};
    for(size_t i=0;i<sizeof(left)/sizeof(*left);i++)CHECK(text_same(left[i],right[i]));
    CHECK(analysis_query_valid(a,NULL)&&analysis_filters_valid(a,NULL)&&analysis_sorts_valid(a,NULL)&&!a->filter_number.kind&&!a->filter_number.precision&&!a->filter_number.scale);
    for(size_t i=0;i<a->filter_count;i++)CHECK(text_same(a->filters[i].column,b->filters[i].column)&&text_same(a->filters[i].value,b->filters[i].value)&&a->filters[i].op==b->filters[i].op&&!a->filters[i].number.kind&&!a->filters[i].number.precision&&!a->filters[i].number.scale);
    for(size_t i=0;i<a->sort_count;i++)CHECK(text_same(a->sorts[i].column,b->sorts[i].column)&&a->sorts[i].descending==b->sorts[i].descending);
}
static void tables_compare(const char *path,unsigned version,bool large,WorkbookCodecHeader **out) {
    FileImage original=file_image(path);Workbook native;workbook_init(&native);char *error=NULL;CHECK(workbook_load(&native,path,&error)&&!error);
    size_t sheets=large?2:1,count=large?32:4;CHECK(native.count==sheets&&native.active==sheets-1);
    duckdb_database database=NULL;duckdb_connection connection=NULL;open_readonly(path,&database,&connection);size_t retained=workbook_codec_header_owned_bytes(*out);
    WorkbookCodecRead *read=tables_begin(connection,version,native.path,retained);CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error)&&!error);
    unsigned before=executions;const WorkbookCodecFamily families[]={WORKBOOK_CODEC_TABLE_SOURCES,WORKBOOK_CODEC_TABLE_FILTERS,WORKBOOK_CODEC_TABLE_SORTS};const size_t columns[]={14,6,5},limits[]={2049,30721,14337};
    for(size_t i=0;i<3;i++){
        const WorkbookCodecRequest *request=workbook_codec_read_request(read);CHECK(request&&request->family==families[i]&&request->column_count==columns[i]&&request->source_column_count==columns[i]&&request->row_limit==limits[i]&&!request->first_row);
        pending_tables_header(read,out);CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    }
    CHECK(executions==before+3&&table_result_rows[0]==sheets*count&&table_result_rows[1]==(large?512u:version==14?1u:0u)&&table_result_rows[2]==(large?448u:0u));
    if(large)CHECK(table_batches[1]==2&&table_batches256[1]==2&&table_batches[2]==2&&table_batches256[2]==1);
    CHECK(workbook_codec_read_finish(read,out,&error)&&!error);workbook_codec_read_free(read);duckdb_disconnect(&connection);duckdb_close(&database);
    CHECK(workbook_codec_header_table_sources_ready(*out)&&workbook_codec_header_objects_ready(*out)&&workbook_codec_header_queries_ready(*out)&&workbook_codec_header_source_options_ready(*out));
    CHECK(workbook_codec_header_version(*out)==version&&workbook_codec_header_sheet_count(*out)==sheets&&workbook_codec_header_active_sheet(*out)==sheets-1&&workbook_codec_header_user_scale(*out)==1.25f);
    for(size_t s=0;s<sheets;s++){
        const Worksheet *sheet=native.sheets+s;CHECK(workbook_codec_header_object_count(*out,s)==count&&sheet->objects.count==count);const Analysis *primary=workbook_codec_header_analysis(*out,s);
        table_analysis_compare(primary,&sheet->analysis);CHECK(text_same(workbook_codec_header_name(*out,s),sheet->name)&&!strcmp(workbook_codec_header_reference(*out,s),"missing-source.csv"));
        for(size_t j=0;j<count;j++){
            const SheetObject *object=sheet->objects.items+j;WorkbookCodecTableSourceView source={0};CHECK(workbook_codec_header_table_source(*out,s,object->id,&source)&&source.analysis&&source.reference&&source.locator_available);
            CHECK(object->id==table_fixture_id(version,large,s,j));table_analysis_compare(source.analysis,&object->source);
            char *reference=workbook_relative_path(native.path,object->source.path);CHECK(!strcmp(source.reference,reference));free(reference);
            const char *query=NULL;CHECK(workbook_codec_header_query(*out,s,object->id,&query)&&query==source.analysis->query);char *expected=table_fixture_query(s,j,false);CHECK(!strcmp(query,expected));free(expected);
            bool unique=false;CHECK(workbook_codec_header_unique_rows(*out,s,object->id,&unique)&&unique==source.analysis->unique_rows&&unique==((s+j+1)%2!=0));
            if(!large&&version==14&&!j){const char *value=source.analysis->filters[0].value;CHECK(value&&strlen(value)==65537&&source.analysis->filter_count==1);for(size_t n=0;n<65537;n++)if(value[n]!='x')CHECK(false);}
        }
    }
    WorkbookCodecTableSourceView invalid;memset(&invalid,0x5a,sizeof(invalid));WorkbookCodecTableSourceView saved=invalid;
    CHECK(!workbook_codec_header_table_source(*out,sheets,1,&invalid)&&!memcmp(&invalid,&saved,sizeof(invalid)));CHECK(!workbook_codec_header_table_source(*out,0,0,&invalid)&&!memcmp(&invalid,&saved,sizeof(invalid)));CHECK(!workbook_codec_header_table_source(*out,0,1,NULL));
    TableHeaderStamp sealed=tables_stamp(*out);workbook_free(&native);kept_tables_header(*out,sealed);
    FileImage after=file_image(path);CHECK(original.bytes==after.bytes&&!memcmp(original.data,after.data,original.bytes));free(original.data);free(after.data);
}
static void tables_rejected(const char *path,unsigned version,WorkbookCodecTableSourcePresence presence,WorkbookCodecFamily failed,size_t first,WorkbookCodecHeader **out) {
    FileImage original=file_image(path);Workbook native;workbook_init(&native);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&native,"Retained native",&blank,&error));Workbook before=native;
    CHECK(!workbook_load(&native,path,&error)&&!memcmp(&native,&before,sizeof(native))&&!strcmp(native.sheets[0].name,"Retained native"));clear_error(&error);
    char *target=dx_absolute_path(path);CHECK(target);duckdb_database database=NULL;duckdb_connection connection=NULL;open_readonly(path,&database,&connection);
    TableHeaderStamp stamp=tables_stamp(*out);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *read=tables_begin(connection,version,target,retained);size_t owned=workbook_codec_read_owned_bytes(read);
    if(!presence.sources||!presence.filters||!presence.sorts){CHECK(!workbook_codec_read_add_table_sources(read,presence,&error)&&error&&workbook_codec_read_owned_bytes(read)==owned&&!workbook_codec_read_request(read));clear_error(&error);}
    else{
        CHECK(workbook_codec_read_add_table_sources(read,presence,&error));
        while(workbook_codec_read_request(read)&&workbook_codec_read_request(read)->family!=failed)CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
        CHECK(workbook_codec_read_request(read)&&workbook_codec_read_request(read)->family==failed);unsigned queries=executions;
        CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error);
        CHECK(executions==queries+1&&table_last_family==failed&&table_last_first==first&&workbook_codec_read_request(read)->family==failed&&workbook_codec_read_request(read)->first_row==first&&workbook_codec_read_owned_bytes(read)==table_last_owned);
        clear_error(&error);pending_tables_header(read,out);
    }
    kept_tables_header(*out,stamp);workbook_codec_read_free(read);duckdb_disconnect(&connection);duckdb_close(&database);free(target);workbook_free(&native);kept_tables_header(*out,stamp);
    FileImage after=file_image(path);CHECK(original.bytes==after.bytes&&!memcmp(original.data,after.data,original.bytes));free(original.data);free(after.data);
}
static void tables_input_faults(const char *path,WorkbookCodecHeader **out) {
    FileImage original=file_image(path);char *target=dx_absolute_path(path);CHECK(target);duckdb_database database=NULL;duckdb_connection connection=NULL;open_readonly(path,&database,&connection);char *error=NULL;
    TableHeaderStamp old=tables_stamp(*out);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *read=tables_begin(connection,14,target,retained);
    CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){true,true,true},&error));const char *sql=workbook_codec_read_request(read)->sql;
    const char *bad_columns[]={"NULL::UBIGINT AS object_id","1::INTEGER AS source_schema","descending::INTEGER AS descending"};
    for(size_t i=0;i<sizeof(bad_columns)/sizeof(*bad_columns);i++){
        char *bad=dx_printf("SELECT * REPLACE (%s) FROM (%s)",bad_columns[i],sql);size_t owned=workbook_codec_read_owned_bytes(read);
        CHECK(result_feed(connection,read,bad,256,false,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&table_last_first==0&&workbook_codec_read_owned_bytes(read)==owned);clear_error(&error);free(bad);kept_tables_header(*out,old);
    }
    size_t owned=workbook_codec_read_owned_bytes(read);CHECK(owned+retained+1<TABLE_HEADER_LIMIT);size_t bytes=TABLE_HEADER_LIMIT-owned-retained-1;unsigned char *pressure=dx_alloc(bytes);memset(pressure,0,bytes);
    CHECK(result_feed(connection,read,NULL,256,true,retained+bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"budget")&&table_last_first==0&&workbook_codec_read_owned_bytes(read)==owned);clear_error(&error);free(pressure);kept_tables_header(*out,old);
    for(size_t i=0;i<3;i++)CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_finish(read,out,&error)&&!error);workbook_codec_read_free(read);duckdb_disconnect(&connection);duckdb_close(&database);free(target);
    TableHeaderStamp final=tables_stamp(*out);kept_tables_header(*out,final);FileImage after=file_image(path);CHECK(original.bytes==after.bytes&&!memcmp(original.data,after.data,original.bytes));free(original.data);free(after.data);
}
static void tables_empty_legacy(const char *path,const char *missing,unsigned version,WorkbookCodecHeader **out) {
    tables_make_fixture(path,missing,version,false,true);mutate(path,"DROP TABLE _duxcel_workspace.table_sources; INSERT INTO _duxcel_workspace.table_filters(sheet_id,object_id,position,name,op,value) VALUES (99,1,0,NULL,0,NULL)");
    if(version==14){tables_rejected(path,14,(WorkbookCodecTableSourcePresence){false,true,true},WORKBOOK_CODEC_TABLE_SOURCES,0,out);return;}
    FileImage original=file_image(path);Workbook native;workbook_init(&native);char *error=NULL;CHECK(workbook_load(&native,path,&error)&&!error&&!native.sheets[0].objects.count);
    duckdb_database database=NULL;duckdb_connection connection=NULL;open_readonly(path,&database,&connection);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *read=tables_begin(connection,version,native.path,retained);unsigned queries=executions;
    CHECK(workbook_codec_read_add_table_sources(read,(WorkbookCodecTableSourcePresence){false,true,true},&error)&&!error&&!workbook_codec_read_request(read));CHECK(executions==queries);
    CHECK(workbook_codec_read_finish(read,out,&error)&&!error&&workbook_codec_header_table_sources_ready(*out)&&!workbook_codec_header_object_count(*out,0));workbook_codec_read_free(read);duckdb_disconnect(&connection);duckdb_close(&database);
    table_analysis_compare(workbook_codec_header_analysis(*out,0),&native.sheets[0].analysis);TableHeaderStamp sealed=tables_stamp(*out);workbook_free(&native);kept_tables_header(*out,sealed);
    FileImage after=file_image(path);CHECK(original.bytes==after.bytes&&!memcmp(original.data,after.data,original.bytes));free(original.data);free(after.data);
}
static int tables_only(void) {
    char temporary[]="/tmp/duxcel-codec-tables-XXXXXX";CHECK(mkdtemp(temporary));char *directory=dx_absolute_path(temporary);CHECK(directory);char *missing=dx_printf("%s/missing-source.csv",directory);CHECK(access(missing,F_OK)!=0);
    WorkbookCodecHeader *header=NULL;char *paths[4]={0};
    for(size_t i=0;i<4;i++){
        const EmitFixture *entry=emit_tables_fixtures+i;paths[i]=dx_printf("%s/%s",directory,entry->name);if(i==3)tables_large_fixture(paths[i],missing,14);else tables_fixture(paths[i],missing,entry->version);
        if(entry->reordered){mutate(paths[i],reorder_sql);mutate(paths[i],objects_reorder_sql);mutate(paths[i],queries_reorder_sql);mutate(paths[i],options_reorder_sql);mutate(paths[i],tables_reorder_sql);}
        tables_compare(paths[i],entry->version,i==3,&header);
    }
    tables_input_faults(paths[2],&header);FileImage small=file_image(paths[2]),large=file_image(paths[3]);char *path=dx_printf("%s/private.dxls",directory);
    for(size_t i=0;i<3;i++){
        FileImage version=file_image(paths[i]);options_copy_image(path,version);free(version.data);mutate(path,"DROP TABLE _duxcel_workspace.table_sources");tables_rejected(path,(unsigned)i+12,(WorkbookCodecTableSourcePresence){false,true,true},WORKBOOK_CODEC_TABLE_SOURCES,0,&header);CHECK(!unlink(path));
    }
    const char *missing_children[]={"DROP TABLE _duxcel_workspace.table_filters","DROP TABLE _duxcel_workspace.table_sorts"};
    for(size_t i=0;i<2;i++){options_copy_image(path,small);mutate(path,missing_children[i]);tables_rejected(path,14,(WorkbookCodecTableSourcePresence){true,i!=0,i!=1},WORKBOOK_CODEC_TABLE_SOURCES,0,&header);CHECK(!unlink(path));}
    const char *invalid[]={
        "DELETE FROM _duxcel_workspace.table_sources WHERE object_id=18446744073709551615::UBIGINT",
        "CREATE TABLE _duxcel_workspace.bad_sources AS SELECT * FROM _duxcel_workspace.table_sources WHERE object_id<>18446744073709551615::UBIGINT UNION ALL SELECT * FROM _duxcel_workspace.table_sources WHERE object_id=9007199254740993; DROP TABLE _duxcel_workspace.table_sources; ALTER TABLE _duxcel_workspace.bad_sources RENAME TO table_sources",
        "UPDATE _duxcel_workspace.table_sources SET path=NULL WHERE object_id=9007199254740993",
        "UPDATE _duxcel_workspace.table_sources SET kind=0 WHERE object_id=9007199254740993",
        "UPDATE _duxcel_workspace.objects SET kind=0 WHERE id=18446744073709551615::UBIGINT; DELETE FROM _duxcel_workspace.queries WHERE object_id=18446744073709551615::UBIGINT; DELETE FROM _duxcel_workspace.source_options WHERE object_id=18446744073709551615::UBIGINT",
        "UPDATE _duxcel_workspace.table_sources SET filter_column=NULL WHERE object_id=9007199254740993",
        "UPDATE _duxcel_workspace.table_filters SET name='p ''İ'' 0'",
        "UPDATE _duxcel_workspace.table_filters SET position=1",
        "UPDATE _duxcel_workspace.table_filters SET op=10,value='[\"1\"]'",
        "UPDATE _duxcel_workspace.table_sources SET filter_value='a'||chr(0)||'b' WHERE object_id=9007199254740993"
    };
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++){
        options_copy_image(path,small);mutate(path,invalid[i]);tables_rejected(path,14,(WorkbookCodecTableSourcePresence){true,true,true},i>=5&&i<=8?WORKBOOK_CODEC_TABLE_FILTERS:WORKBOOK_CODEC_TABLE_SOURCES,0,&header);CHECK(!unlink(path));
    }
    const char *late[]={
        "UPDATE _duxcel_workspace.table_filters SET position=8 WHERE sheet_id=1 AND object_id=18446744073709551615::UBIGINT AND position=7",
        "UPDATE _duxcel_workspace.table_sorts SET position=7 WHERE sheet_id=1 AND object_id=18446744073709551615::UBIGINT AND position=6",
        "UPDATE _duxcel_workspace.table_sources SET sort_column=NULL WHERE sheet_id=1 AND object_id=18446744073709551615::UBIGINT",
        "UPDATE _duxcel_workspace.table_sorts SET name='s ''İ'' 0' WHERE sheet_id=1 AND object_id=18446744073709551615::UBIGINT AND position=6",
        "UPDATE _duxcel_workspace.table_sources SET sheet_id=0 WHERE sheet_id=1 AND object_id=18446744073709551615::UBIGINT"
    };
    for(size_t i=0;i<sizeof(late)/sizeof(*late);i++){
        options_copy_image(path,large);mutate(path,late[i]);tables_rejected(path,14,(WorkbookCodecTableSourcePresence){true,true,true},i==0?WORKBOOK_CODEC_TABLE_FILTERS:i==4?WORKBOOK_CODEC_TABLE_SOURCES:WORKBOOK_CODEC_TABLE_SORTS,i==4?0:256,&header);CHECK(!unlink(path));
    }
    free(small.data);free(large.data);tables_compare(paths[0],12,false,&header);
    for(unsigned version=12;version<=14;version++){tables_empty_legacy(path,missing,version,&header);CHECK(!unlink(path));}
    free(path);workbook_codec_header_free(header);for(size_t i=0;i<4;i++){CHECK(!unlink(paths[i]));free(paths[i]);}
    CHECK(nul_cells==1&&access(missing,F_OK)!=0);free(missing);CHECK(!rmdir(directory));free(directory);
    printf("PASS: native Table sources %u checks; %u cursor metadata executions/%u typed feeds; complete sources/filters/sorts, 64 Table/512 filter/448 sort composition, exact queries/flags/raw locators/NULL-empty-literal/65537-byte value, native closed-handle parity and atomic failure/budget/stale/recovery; no source execution or Worker/App Open claim; engine %s\n",checks,executions,feeds,duckdb_library_version());return 0;
}
#undef TABLE_HEADER_LIMIT
#undef TABLE_BASE
