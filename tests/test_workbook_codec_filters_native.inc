/* Native primary/secondary FILTERS comparison. These are stored definitions;
   neither the missing CSV files nor the stored source queries are executed. */
#define FILTER_HEADER_LIMIT (2u*1024u*1024u)
static const char *const filter_values[]={NULL,"","NULL"};
static char *filter_query(size_t sheet) {return dx_printf("SELECT * FROM __BP1_FILTERS_MUST_NOT_EXECUTE /* 'İ' %zu */;  \n",sheet);}
static char *filter_long_value(char byte) {char *value=dx_alloc(65538);memset(value,byte,65537);value[65537]=0;return value;}
static void filters_fixture(const char *path,const char *missing,unsigned version,bool large) {
    Workbook book;workbook_init(&book);char *error=NULL;size_t sheets=large?19u:1u;
    for(size_t s=0;s<sheets;s++){
        Analysis a={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=filter_query(s),.unique_rows=s%2!=0,
            .filter_column=dx_strdup("Primary 'İ'"),.filter_value=table_fixture_nullable(filter_values[s%3]),.filter_op=FILTER_CONTAINS,
            .group_column=dx_strdup("Group 'İ'"),.measure_column=dx_strdup("Measure 'İ'"),.aggregate=AGG_AVG,.sort_column=dx_strdup("Sort 'İ' 0"),.descending=s%2!=0,.sort_count=7};
        if(!large&&version==14){free(a.filter_value);a.filter_value=filter_long_value('p');}
        a.filter_count=large?15u:version==14?4u:3u;
        for(size_t f=0;f<a.filter_count;f++)a.filters[f]=(AnalysisFilter){dx_printf("Filter 'İ' %zu",f),table_fixture_nullable(large&&f==14?"[\"10\",\"20\"]":filter_values[f%3]),large&&f==14?FILTER_BETWEEN:FILTER_CONTAINS,{0}};
        if(!large&&version==14){free(a.filters[3].value);a.filters[3].value=filter_long_value('x');}
        for(size_t n=0;n<7;n++)a.sorts[n]=(AnalysisSort){dx_printf("Sort 'İ' %zu",n+1),(n+1)%2!=0};
        CHECK(!a.filters[0].value&&a.filters[1].value&&!*a.filters[1].value&&!strcmp(a.filters[2].value,"NULL"));
        CHECK(!large&&version==14 ? strlen(a.filter_value)==65537&&strlen(a.filters[3].value)==65537 : text_same(a.filter_value,filter_values[s%3]));
        CHECK(analysis_query_valid(&a,&error)&&analysis_filters_valid(&a,&error)&&analysis_sorts_valid(&a,&error)&&!error);
        char *name=dx_printf("Filtered 'İ' %zu",s);CHECK(workbook_add(&book,name,&a,&error));free(name);analysis_free(&a);
        book.sheets[s].view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
    }
    book.active=sheets-1;book.user_scale=1.25f;
    if(version==13)book.sheets[0].objects.next_id=UINT64_C(9007199254740994);
    if(version==14)book.sheets[0].formats.source_aware=true;
    CHECK(workbook_save(&book,path,&error)&&!error);workbook_free(&book);
    /* Assert the actual archive triad, not only pre-Save C values. This catches
       accidental dx_strdup(NULL) normalization in a future fixture edit. */
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);
    char *sql=dx_printf("SELECT CASE WHEN count(*)=%zu AND count(*) FILTER (WHERE position=0 AND value IS NULL)=%zu AND count(*) FILTER (WHERE position=1 AND value='')=%zu AND count(*) FILTER (WHERE position=2 AND value='NULL')=%zu THEN true ELSE error('FILTERS fixture NULL/empty/literal drift') END FROM _duxcel_workspace.filters",sheets*(large?15u:version==14?4u:3u),sheets,sheets,sheets);execute(c,sql);free(sql);
    sql=dx_printf("SELECT CASE WHEN count(*)=%zu AND count(*) FILTER (WHERE kind=1 AND path='missing-source.csv' AND source_schema='' AND source_table='NULL')=%zu THEN true ELSE error('FILTERS fixture raw source drift') END FROM _duxcel_workspace.sheets",sheets,sheets);execute(c,sql);free(sql);
    if(!large&&version==14)execute(c,"SELECT CASE WHEN (SELECT octet_length(encode(filter_value)) FROM _duxcel_workspace.sheets)=65537 AND (SELECT octet_length(encode(value)) FROM _duxcel_workspace.filters WHERE position=3)=65537 THEN true ELSE error('FILTERS fixture long values drift') END");
    duckdb_disconnect(&c);duckdb_close(&db);
}
static const EmitFixture emit_filters_fixtures[]={
    {"filters-v12.dxls",12,25,false},{"filters-v13.dxls",13,25,false},
    {"filters-v14.dxls",14,25,false},{"filters-v14-reordered.dxls",14,25,true}
};
static const char filters_reorder_sql[]="CREATE TABLE _duxcel_workspace.reordered_filters AS SELECT value,op,name,position,sheet_id FROM _duxcel_workspace.filters; DROP TABLE _duxcel_workspace.filters; ALTER TABLE _duxcel_workspace.reordered_filters RENAME TO filters";
static size_t filters_emit_index;
static void filters_emit_fixture(const char *path,const char *missing,unsigned version) {CHECK(filters_emit_index<4&&emit_filters_fixtures[filters_emit_index].version==version);filters_fixture(path,missing,version,filters_emit_index++==3);}
static int emit_filters(const char *directory) {
    char reorder[4096];int n=snprintf(reorder,sizeof(reorder),"%s;%s;%s;%s",queries_reorder_sql,options_reorder_sql,sorts_reorder_sql,filters_reorder_sql);CHECK(n>0&&(size_t)n<sizeof(reorder));
    filters_emit_index=0;return emit_fixture_files(directory,emit_filters_fixtures,4,filters_emit_fixture,reorder,NULL);
}
typedef struct {WorkbookCodecHeader *identity;size_t bytes;uint64_t digest;} FilterHeaderStamp;
static FilterHeaderStamp filters_stamp(WorkbookCodecHeader *header) {
    FilterHeaderStamp stamp={header,workbook_codec_header_owned_bytes(header),UINT64_C(1469598103934665603)};
    stamp.digest=object_hash_word(stamp.digest,workbook_codec_header_version(header));stamp.digest=object_hash_word(stamp.digest,workbook_codec_header_sheet_count(header));
    stamp.digest=object_hash_word(stamp.digest,workbook_codec_header_active_sheet(header));stamp.digest=object_hash_word(stamp.digest,workbook_codec_header_filters_ready(header));
    for(size_t s=0;s<workbook_codec_header_sheet_count(header);s++){
        const Analysis *a=workbook_codec_header_analysis(header,s);const SheetView *v=workbook_codec_header_view(header,s);CHECK(a&&v);
        stamp.digest=table_hash_analysis(stamp.digest,a);stamp.digest=table_hash_text(stamp.digest,workbook_codec_header_name(header,s));stamp.digest=table_hash_text(stamp.digest,workbook_codec_header_reference(header,s));
        stamp.digest=object_hash_word(stamp.digest,(uintptr_t)a);stamp.digest=object_hash_word(stamp.digest,(uintptr_t)a->filter_value);stamp.digest=object_hash_word(stamp.digest,(uintptr_t)a->query);
        const uint64_t geometry[]={v->row,v->col,v->anchor_row,v->anchor_col,v->end_row,v->end_col,v->top,v->selection};for(size_t i=0;i<8;i++)stamp.digest=object_hash_word(stamp.digest,geometry[i]);
        for(size_t i=0;i<a->filter_count;i++){stamp.digest=object_hash_word(stamp.digest,(uintptr_t)a->filters[i].column);stamp.digest=object_hash_word(stamp.digest,(uintptr_t)a->filters[i].value);}
    }
    return stamp;
}
static void filters_kept(WorkbookCodecHeader *header,FilterHeaderStamp before) {FilterHeaderStamp now=filters_stamp(header);CHECK(now.identity==before.identity&&now.bytes==before.bytes&&now.digest==before.digest);}
static void filters_pending(WorkbookCodecRead *read,WorkbookCodecHeader **out) {FilterHeaderStamp old=filters_stamp(*out);char *error=NULL;CHECK(!workbook_codec_read_finish(read,out,&error));clear_error(&error);filters_kept(*out,old);}
static WorkbookCodecRead *filters_begin(duckdb_connection c,unsigned version,const char *path,size_t retained,bool composed) {
    WorkbookCodecRead *read=NULL;char *error=NULL;CHECK(workbook_codec_read_begin_with_options(&read,key,version,WORKBOOK_CODEC_SHEETS_25,(WorkbookCodecReadOptions){true},FILTER_HEADER_LIMIT,resolve,(void *)path,&error));
    for(size_t i=0;i<2;i++)CHECK(result_feed(c,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    if(composed){
        CHECK(workbook_codec_read_add_objects(read,true,&error)&&result_feed(c,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
        CHECK(workbook_codec_read_add_queries(read,true,&error)&&result_feed(c,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
        CHECK(workbook_codec_read_add_source_options(read,true,&error)&&result_feed(c,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
        CHECK(workbook_codec_read_add_sorts(read,true,&error)&&result_feed(c,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    }
    return read;
}
static void filters_view_equal(const SheetView *v,const SheetView *w) {
    CHECK(v&&w&&v->row==w->row&&v->col==w->col&&v->anchor_row==w->anchor_row&&v->anchor_col==w->anchor_col&&v->end_row==w->end_row&&v->end_col==w->end_col&&v->top==w->top&&v->horizontal==w->horizontal&&v->fraction==w->fraction&&v->selection==w->selection&&v->canvas_zoom==w->canvas_zoom&&!v->column_count&&!v->columns&&!v->widths);
}
static void filters_compare(const char *path,unsigned version,bool large,WorkbookCodecHeader **out) {
    FileImage image=file_image(path);Workbook book;workbook_init(&book);char *error=NULL;bool loaded=workbook_load(&book,path,&error);if(!loaded)fprintf(stderr,"FILTERS native load: %s\n",error?error:"no diagnostic");CHECK(loaded&&!error);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *read=filters_begin(c,version,book.path,retained,true);
    CHECK(workbook_codec_read_add_filters(read,true,retained,&error));const WorkbookCodecRequest *request=workbook_codec_read_request(read);
    CHECK(request&&request->family==WORKBOOK_CODEC_FILTERS&&request->column_count==5&&request->source_column_count==5&&request->row_limit==961&&!request->first_row);
    CHECK(!strcmp(request->sql,"SELECT sheet_id,position,name,op,value FROM _duxcel_workspace.filters ORDER BY sheet_id,position LIMIT 961"));filters_pending(read,out);
    unsigned before=executions;CHECK(result_feed(c,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&executions==before+1&&filter_result_rows==(large?285u:version==14?4u:3u));
    CHECK(filter_batches256==(large?1u:0u)&&filter_batches29==(large?1u:0u));CHECK(!workbook_codec_read_add_filters(read,true,retained,&error));clear_error(&error);
    CHECK(workbook_codec_read_finish(read,out,&error)&&!error);workbook_codec_read_free(read);duckdb_disconnect(&c);duckdb_close(&db);
    CHECK(workbook_codec_header_filters_ready(*out)&&workbook_codec_header_sorts_ready(*out)&&workbook_codec_header_queries_ready(*out)&&workbook_codec_header_source_options_ready(*out)&&workbook_codec_header_objects_ready(*out));
    CHECK(book.count==(large?19u:1u)&&book.active==book.count-1&&workbook_codec_header_sheet_count(*out)==book.count&&workbook_codec_header_version(*out)==version&&workbook_codec_header_active_sheet(*out)==book.active&&workbook_codec_header_user_scale(*out)==book.user_scale);
    for(size_t s=0;s<book.count;s++){
        const Worksheet *w=book.sheets+s;const Analysis *a=workbook_codec_header_analysis(*out,s);table_analysis_compare(a,&w->analysis);filters_view_equal(workbook_codec_header_view(*out,s),&w->view);
        CHECK(!strcmp(workbook_codec_header_name(*out,s),w->name)&&!strcmp(workbook_codec_header_reference(*out,s),"missing-source.csv")&&!workbook_codec_header_object_count(*out,s));
        CHECK(!a->filters[0].value&&a->filters[1].value&&!*a->filters[1].value&&!strcmp(a->filters[2].value,"NULL"));
        if(!large&&version==14){char *p=filter_long_value('p'),*x=filter_long_value('x');CHECK(!strcmp(a->filter_value,p)&&!strcmp(a->filters[3].value,x));free(p);free(x);}else CHECK(text_same(a->filter_value,filter_values[s%3]));
        const char *query=NULL;bool unique=false;CHECK(workbook_codec_header_query(*out,s,0,&query)&&query==a->query&&workbook_codec_header_unique_rows(*out,s,0,&unique)&&unique==a->unique_rows&&unique==(s%2!=0));char *expected=filter_query(s);CHECK(!strcmp(query,expected));free(expected);
    }
    FilterHeaderStamp final=filters_stamp(*out);workbook_free(&book);filters_kept(*out,final);FileImage after=file_image(path);CHECK(image.bytes==after.bytes&&!memcmp(image.data,after.data,image.bytes));free(image.data);free(after.data);
}
static void filters_rejected(const char *path,unsigned version,bool present,bool arm_failure,size_t first,WorkbookCodecHeader **out) {
    FileImage image=file_image(path);Workbook book;workbook_init(&book);Analysis none={0};char *error=NULL;CHECK(workbook_add(&book,"Retained native",&none,&error));Workbook before=book;
    CHECK(!workbook_load(&book,path,&error)&&error&&!memcmp(&book,&before,sizeof(book))&&!strcmp(book.sheets[0].name,"Retained native"));clear_error(&error);
    char *target=dx_absolute_path(path);CHECK(target);duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *read=filters_begin(c,version,target,retained,false);FilterHeaderStamp old=filters_stamp(*out);size_t owned=workbook_codec_read_owned_bytes(read);
    if(arm_failure){CHECK(!workbook_codec_read_add_filters(read,present,retained,&error)&&error&&!workbook_codec_read_request(read)&&workbook_codec_read_owned_bytes(read)==owned);clear_error(&error);}
    else{
        CHECK(workbook_codec_read_add_filters(read,present,retained,&error));unsigned count=executions;CHECK(result_feed(c,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error);
        CHECK(executions==count+1&&filter_last_first==first&&workbook_codec_read_request(read)->first_row==first&&workbook_codec_read_owned_bytes(read)==filter_last_owned);clear_error(&error);filters_pending(read,out);
    }
    filters_kept(*out,old);workbook_codec_read_free(read);duckdb_disconnect(&c);duckdb_close(&db);free(target);workbook_free(&book);filters_kept(*out,old);
    FileImage after=file_image(path);CHECK(image.bytes==after.bytes&&!memcmp(image.data,after.data,image.bytes));free(image.data);free(after.data);
}
static void filters_input_faults(const char *path,WorkbookCodecHeader **out) {
    FileImage image=file_image(path);char *target=dx_absolute_path(path);CHECK(target);duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);char *error=NULL;size_t retained=workbook_codec_header_owned_bytes(*out);FilterHeaderStamp old=filters_stamp(*out);WorkbookCodecRead *read=filters_begin(c,14,target,retained,true);
    CHECK(workbook_codec_read_add_filters(read,true,retained,&error));const char *changes[]={"NULL::VARCHAR AS name","NULL::INTEGER AS op","op::VARCHAR AS op","sheet_id::VARCHAR AS sheet_id","'a'||chr(0)||'b' AS value"};
    for(size_t i=0;i<5;i++){
        char *bad=dx_printf("SELECT * REPLACE (%s) FROM (%s)",changes[i],workbook_codec_read_request(read)->sql);size_t owned=workbook_codec_read_owned_bytes(read);
        CHECK(result_feed(c,read,bad,256,false,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&filter_last_first==0&&workbook_codec_read_owned_bytes(read)==owned);clear_error(&error);free(bad);filters_kept(*out,old);filters_pending(read,out);
    }
    size_t owned=workbook_codec_read_owned_bytes(read);CHECK(owned+retained+65536<FILTER_HEADER_LIMIT);size_t bytes=FILTER_HEADER_LIMIT-owned-retained-65536;unsigned char *pressure=dx_alloc(bytes);memset(pressure,0,bytes);
    CHECK(result_feed(c,read,NULL,256,true,retained+bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"budget")&&workbook_codec_read_owned_bytes(read)==owned&&!workbook_codec_read_request(read)->first_row);clear_error(&error);free(pressure);filters_kept(*out,old);
    CHECK(result_feed(c,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_finish(read,out,&error));workbook_codec_read_free(read);duckdb_disconnect(&c);duckdb_close(&db);free(target);
    FileImage after=file_image(path);CHECK(image.bytes==after.bytes&&!memcmp(image.data,after.data,image.bytes));free(image.data);free(after.data);
}
static void filters_absent(const char *path,unsigned version,WorkbookCodecHeader **out) {
    FileImage image=file_image(path);Workbook book;workbook_init(&book);char *error=NULL;CHECK(workbook_load(&book,path,&error)&&!error&&book.count==1&&!book.sheets[0].analysis.filter_count);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *read=filters_begin(c,version,book.path,retained,true);unsigned count=executions;
    CHECK(workbook_codec_read_add_filters(read,false,retained,&error)&&!error&&!workbook_codec_read_request(read)&&executions==count&&workbook_codec_read_finish(read,out,&error));workbook_codec_read_free(read);duckdb_disconnect(&c);duckdb_close(&db);
    table_analysis_compare(workbook_codec_header_analysis(*out,0),&book.sheets[0].analysis);CHECK(workbook_codec_header_filters_ready(*out));FilterHeaderStamp stamp=filters_stamp(*out);workbook_free(&book);filters_kept(*out,stamp);
    FileImage after=file_image(path);CHECK(image.bytes==after.bytes&&!memcmp(image.data,after.data,image.bytes));free(image.data);free(after.data);
}
static void filters_scratch_admission(const char *path,WorkbookCodecHeader **out) {
    char *target=dx_absolute_path(path);CHECK(target);duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);char *error=NULL;size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *read=filters_begin(c,12,target,retained,true);FilterHeaderStamp old=filters_stamp(*out);size_t owned=workbook_codec_read_owned_bytes(read);
    const char payload[]="[\"6\",\"[\\\"a\\\"]\",\"1\",\"x\"]";size_t scratch=3u*strlen(payload)+4102u+6160u*sizeof(char *);CHECK(owned+retained+scratch<FILTER_HEADER_LIMIT);size_t bytes=FILTER_HEADER_LIMIT-owned-retained-scratch;unsigned char *pressure=dx_alloc(bytes+1);memset(pressure,0,bytes+1);
    CHECK(!workbook_codec_read_add_filters(read,false,retained+bytes+1,&error)&&error&&strstr(error,"scratch")&&workbook_codec_read_owned_bytes(read)==owned&&!workbook_codec_read_request(read));clear_error(&error);filters_kept(*out,old);
    CHECK(workbook_codec_read_add_filters(read,false,retained+bytes,&error)&&!error&&workbook_codec_read_finish(read,out,&error));free(pressure);workbook_codec_read_free(read);duckdb_disconnect(&c);duckdb_close(&db);free(target);CHECK(!strcmp(workbook_codec_header_analysis(*out,0)->filter_value,payload));
}
static int filters_only(void) {
    char temporary[]="/tmp/duxcel-codec-filters-XXXXXX";CHECK(mkdtemp(temporary));char *directory=dx_absolute_path(temporary);CHECK(directory);char *missing=dx_printf("%s/missing-source.csv",directory);CHECK(access(missing,F_OK)!=0);WorkbookCodecHeader *header=NULL;char *paths[4];
    for(size_t i=0;i<4;i++){
        const EmitFixture *entry=emit_filters_fixtures+i;paths[i]=dx_printf("%s/%s",directory,entry->name);filters_fixture(paths[i],missing,entry->version,i==3);
        if(entry->reordered){mutate(paths[i],reorder_sql);mutate(paths[i],queries_reorder_sql);mutate(paths[i],options_reorder_sql);mutate(paths[i],sorts_reorder_sql);mutate(paths[i],filters_reorder_sql);}filters_compare(paths[i],entry->version,i==3,&header);
    }
    filters_input_faults(paths[2],&header);FileImage small=file_image(paths[0]),large=file_image(paths[3]);char *path=dx_printf("%s/private.dxls",directory);
    for(unsigned version=12;version<=14;version++){
        FileImage image=file_image(paths[version-12]);options_copy_image(path,image);free(image.data);mutate(path,"DROP TABLE _duxcel_workspace.filters");
        if(version==14)filters_rejected(path,14,false,true,0,&header);else filters_absent(path,version,&header);CHECK(!unlink(path));
    }
    const char *invalid[]={
        "UPDATE _duxcel_workspace.filters SET name=NULL WHERE position=0",
        "UPDATE _duxcel_workspace.filters SET name='primary ''İ''' WHERE position=0",
        "UPDATE _duxcel_workspace.filters SET name='filter ''İ'' 0' WHERE position=1",
        "UPDATE _duxcel_workspace.filters SET position=3 WHERE position=2",
        "UPDATE _duxcel_workspace.filters SET op=12 WHERE position=2",
        "UPDATE _duxcel_workspace.filters SET sheet_id=1 WHERE position=2",
        "UPDATE _duxcel_workspace.filters SET op=10,value='[\"one\"]' WHERE position=2",
        "UPDATE _duxcel_workspace.sheets SET filter_column=NULL",
        "UPDATE _duxcel_workspace.sheets SET filter_op=10,filter_value='[\"one\"]'"
    };
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++){
        options_copy_image(path,small);mutate(path,invalid[i]);filters_rejected(path,12,true,i==8,0,&header);CHECK(!unlink(path));
    }
    const char *late[]={
        "UPDATE _duxcel_workspace.filters SET position=15 WHERE sheet_id=18 AND position=14",
        "UPDATE _duxcel_workspace.filters SET name='filter ''İ'' 0' WHERE sheet_id=18 AND position=14",
        "UPDATE _duxcel_workspace.filters SET value='[\"one\"]' WHERE sheet_id=18 AND position=14"
    };
    for(size_t i=0;i<3;i++){options_copy_image(path,large);mutate(path,late[i]);filters_rejected(path,14,true,false,256,&header);CHECK(!unlink(path));}
    /* A blank source can retain filter definitions. It cannot retain source SQL. */
    options_copy_image(path,small);mutate(path,"UPDATE _duxcel_workspace.sheets SET kind=0,path=''; DELETE FROM _duxcel_workspace.queries; DELETE FROM _duxcel_workspace.source_options; DROP TABLE _duxcel_workspace.filters");filters_absent(path,12,&header);CHECK(!unlink(path));
    for(unsigned version=12;version<=13;version++){
        options_copy_image(path,small);char *sql=dx_printf("UPDATE _duxcel_workspace.meta SET version=%u; DROP TABLE _duxcel_workspace.filters; UPDATE _duxcel_workspace.sheets SET filter_op=10,filter_value='[\"one\"]'",version);mutate(path,sql);free(sql);filters_rejected(path,version,false,true,0,&header);CHECK(!unlink(path));
    }
    options_copy_image(path,small);mutate(path,"DROP TABLE _duxcel_workspace.filters; UPDATE _duxcel_workspace.sheets SET filter_op=11,filter_value='[\"6\",\"[\\\"a\\\"]\",\"1\",\"x\"]'");filters_scratch_admission(path,&header);CHECK(!unlink(path));
    filters_compare(paths[3],14,true,&header);free(small.data);free(large.data);free(path);workbook_codec_header_free(header);
    for(size_t i=0;i<4;i++){CHECK(!unlink(paths[i]));free(paths[i]);}CHECK(nul_cells==4&&access(missing,F_OK)!=0);free(missing);CHECK(!rmdir(directory));free(directory);
    printf("PASS: native primary filters %u checks; %u cursor metadata executions/%u typed feeds; full primary/secondary definitions, 285-row split256+29, NULL-empty-literal and 65537-byte primary/secondary values; exact query/options/sorts/view composition, closed-handle native parity and atomic error/budget/stale/recovery; stored source SQL never executed; native comparison only, no Worker/App Open claim; engine %s\n",checks,executions,feeds,duckdb_library_version());return 0;
}
#undef FILTER_HEADER_LIMIT
