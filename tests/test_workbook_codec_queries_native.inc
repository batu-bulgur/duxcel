/* Native comparison of the complete owner-keyed query family. Only metadata is
   evaluated: source files stay missing and stored query text is never executed. */
#define QUERY_HEADER_LIMIT (8u*1024u*1024u)
static char *query_fixture_text(size_t sheet,size_t slot) {
    return !sheet&&!slot?dx_strdup("NULL"):dx_printf("SELECT 1 /* 'İ' %zu:%zu */;  \n",sheet,slot);
}
static void queries_fixture(const char *path,const char *missing,unsigned version) {CHECK(version==15);
    Workbook book;workbook_init(&book);char *error=NULL;
    for(size_t sheet=0;sheet<64;sheet++){
        Analysis source={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=query_fixture_text(sheet,0)};
        char title[64];snprintf(title,sizeof(title),"Sorted 'İ' %zu",sheet);CHECK(workbook_add(&book,title,&source,&error));analysis_free(&source);
        Worksheet *s=book.sheets+sheet;s->view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
        s->objects.items=dx_alloc(32*sizeof(*s->objects.items));s->objects.count=32;
        for(size_t j=0;j<32;j++){
            SheetObject *o=s->objects.items+j;*o=(SheetObject){.id=object_fixture_id(sheet,j),.row=10+j*12,.rows=8,.col=8,.columns=4,.revision=1,
                .kind=OBJECT_TABLE,.mark=MARK_BAR,.name=j?dx_printf("Object 'İ' %zu:%zu",sheet,j):dx_strdup("NULL"),
                .source={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=query_fixture_text(sheet,j+1)}};
        }
        s->objects.next_id=object_fixture_id(sheet,31);
    }
    book.active=63;book.user_scale=1.25f;

    book.sheets[0].formats.source_aware=true;
    CHECK(workbook_save(&book,path,&error)&&!error);workbook_free(&book);
}
static const EmitFixture emit_queries_fixtures[]={

    {"queries-v15.dxls",15,25,false},{"queries-v15-reordered.dxls",15,25,true},
    {"queries-v15-blank.dxls",15,25,false},{"queries-v15-nul.dxls",15,25,false}
};
static const char *const queries_post_sql[]={NULL,NULL,
    "UPDATE _duxcel_workspace.queries SET query='   ' WHERE sheet_id=63 AND object_id=18446744073709551615::UBIGINT",
    "UPDATE _duxcel_workspace.queries SET query=concat('a',chr(0),'b') WHERE sheet_id=63 AND object_id=18446744073709551615::UBIGINT"};
static const char queries_reorder_sql[]="CREATE TABLE _duxcel_workspace.reordered_queries AS SELECT query,object_id,sheet_id FROM _duxcel_workspace.queries; DROP TABLE _duxcel_workspace.queries; ALTER TABLE _duxcel_workspace.reordered_queries RENAME TO queries";
static int emit_queries(const char *directory) {
    char reorder[2048];int n=snprintf(reorder,sizeof(reorder),"%s;%s",objects_reorder_sql,queries_reorder_sql);CHECK(n>0&&(size_t)n<sizeof(reorder));
    return emit_fixture_files(directory,emit_queries_fixtures,4,queries_fixture,reorder,queries_post_sql);
}
typedef struct {ObjectHeaderStamp objects;uint64_t digest;} QueryHeaderStamp;
static QueryHeaderStamp queries_stamp(WorkbookCodecHeader *header) {
    QueryHeaderStamp stamp={objects_stamp(header),UINT64_C(1469598103934665603)};
    bool ready=workbook_codec_header_queries_ready(header);stamp.digest=object_hash_word(stamp.digest,ready);
    if(ready)for(size_t sheet=0;sheet<workbook_codec_header_sheet_count(header);sheet++){
        size_t count=workbook_codec_header_object_count(header,sheet);
        for(size_t slot=0;slot<=count;slot++){
            uint64_t owner=0;if(slot){WorkbookCodecObjectView object={0};CHECK(workbook_codec_header_object_at(header,sheet,slot-1,&object));owner=object.id;if(object.kind!=OBJECT_TABLE)continue;}
            const char *text=(const char *)header;CHECK(workbook_codec_header_query(header,sheet,owner,&text));
            stamp.digest=object_hash_word(stamp.digest,owner);stamp.digest=object_hash_word(stamp.digest,text!=NULL);
            if(text)for(const unsigned char *p=(const unsigned char *)text;;p++){stamp.digest^=*p;stamp.digest*=UINT64_C(1099511628211);if(!*p)break;}
        }
    }
    return stamp;
}
static void kept_queries_header(WorkbookCodecHeader *header,QueryHeaderStamp before) {
    QueryHeaderStamp after=queries_stamp(header);CHECK(after.objects.identity==before.objects.identity&&after.objects.bytes==before.objects.bytes&&after.objects.digest==before.objects.digest&&after.digest==before.digest);
}
static void pending_queries_header(WorkbookCodecRead *read,WorkbookCodecHeader **out) {
    QueryHeaderStamp before=queries_stamp(*out);char *error=NULL;CHECK(!workbook_codec_read_finish(read,out,&error));clear_error(&error);kept_queries_header(*out,before);
}
static WorkbookCodecRead *queries_begin(duckdb_connection connection,unsigned version,const char *target,size_t retained) {CHECK(version==15);
    WorkbookCodecRead *read=objects_begin(connection,version,target,retained);char *error=NULL;
    CHECK(workbook_codec_read_add_objects(read,true,&error)&&!error);unsigned prior_queries=executions,prior_feeds=feeds;
    CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(executions==prior_queries+1&&feeds==prior_feeds+8);return read;
}
static void queries_compare(const char *path,unsigned version,bool canonical,bool faults,WorkbookCodecHeader **out) {CHECK(version==15);
    FileImage before=file_image(path);Workbook native;workbook_init(&native);char *error=NULL;
    bool loaded=workbook_load(&native,path,&error);if(!loaded)fprintf(stderr,"queries native load: %s\n",error?error:"no diagnostic");
    CHECK(loaded&&!error&&native.count==64&&native.active==63);

    CHECK(native.sheets[0].formats.source_aware);
    duckdb_database database=NULL;duckdb_connection connection=NULL;open_readonly(path,&database,&connection);
    size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *read=queries_begin(connection,version,native.path,retained);
    CHECK(workbook_codec_read_add_queries(read,true,&error)&&!error);const WorkbookCodecRequest *request=workbook_codec_read_request(read);
    CHECK(request&&request->family==WORKBOOK_CODEC_QUERIES&&request->column_count==3&&request->source_column_count==3&&request->row_limit==2113);
    CHECK(strstr(request->sql,"CASE WHEN query IS NULL OR octet_length(encode(query))>65536 OR length(trim(query))=0 OR contains(query,chr(0)) THEN NULL::VARCHAR ELSE query END AS query")&&strstr(request->sql,"ORDER BY sheet_id,object_id LIMIT 2113"));
    pending_queries_header(read,out);
    if(faults){
        const char *changes[]={"NULL::INTEGER AS sheet_id","object_id::VARCHAR AS object_id","1::INTEGER AS query"};
        for(size_t i=0;i<3;i++){
            size_t owned=workbook_codec_read_owned_bytes(read);QueryHeaderStamp stamp=queries_stamp(*out);char *bad=dx_printf("SELECT * REPLACE (%s) FROM (%s)",changes[i],request->sql);
            CHECK(result_feed(connection,read,bad,256,false,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&query_last_first==0&&workbook_codec_read_owned_bytes(read)==owned);
            clear_error(&error);free(bad);kept_queries_header(*out,stamp);pending_queries_header(read,out);
        }
        size_t owned=workbook_codec_read_owned_bytes(read);CHECK(owned+retained<QUERY_HEADER_LIMIT);size_t bytes=QUERY_HEADER_LIMIT-owned-retained-1;unsigned char *pressure=dx_alloc(bytes);memset(pressure,0,bytes);
        CHECK(result_feed(connection,read,NULL,256,false,retained+bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"budget")&&query_last_first==0&&workbook_codec_read_owned_bytes(read)==owned);
        clear_error(&error);free(pressure);pending_queries_header(read,out);
        char *raw=dx_strdup("SELECT sheet_id,object_id,CASE WHEN sheet_id=0 AND object_id=0 THEN 'a'||chr(0)||'b' ELSE query END AS query FROM _duxcel_workspace.queries ORDER BY sheet_id,object_id LIMIT 2113");
        CHECK(result_feed(connection,read,raw,256,false,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"NUL")&&nul_cells==1&&query_last_first==0&&workbook_codec_read_owned_bytes(read)==owned);
        clear_error(&error);free(raw);pending_queries_header(read,out);
    }
    size_t expected_rows=0;
    for(size_t sheet=0;sheet<native.count;sheet++){
        expected_rows+=native.sheets[sheet].analysis.query!=NULL;
        for(size_t j=0;j<native.sheets[sheet].objects.count;j++)expected_rows+=native.sheets[sheet].objects.items[j].source.query!=NULL;
    }
    unsigned prior_queries=executions,prior_feeds=feeds;
    CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(executions==prior_queries+1&&feeds==prior_feeds+9&&query_result_rows==expected_rows&&query_batches256==8);
    if(canonical)CHECK(query_result_rows==2112&&query_batches64==1&&!query_null_rows); /* One2112-row result:8x256+64, never requery. */
    CHECK(workbook_codec_read_finish(read,out,&error)&&!error&&workbook_codec_header_queries_ready(*out));workbook_codec_read_free(read);duckdb_disconnect(&connection);duckdb_close(&database);
    CHECK(workbook_codec_header_objects_ready(*out)&&!workbook_codec_header_sorts_ready(*out)&&!workbook_codec_header_preferences_ready(*out)&&workbook_codec_header_version(*out)==version);
    CHECK(workbook_codec_header_sheet_count(*out)==native.count&&workbook_codec_header_active_sheet(*out)==native.active&&workbook_codec_header_user_scale(*out)==native.user_scale);
    for(size_t sheet=0;sheet<native.count;sheet++){
        const Worksheet *s=native.sheets+sheet;const Analysis *a=workbook_codec_header_analysis(*out,sheet);const SheetView *v=workbook_codec_header_view(*out,sheet),*w=&s->view;
        CHECK(a&&text_same(a->path,s->analysis.path)&&a->kind==SOURCE_CSV&&!strcmp(a->schema,"")&&!strcmp(a->table,"NULL")&&!a->sort_column&&!a->filter_count&&!a->sort_count&&!a->unique_rows&&!a->source_version);
        CHECK(!strcmp(workbook_codec_header_name(*out,sheet),s->name)&&!strcmp(workbook_codec_header_reference(*out,sheet),"missing-source.csv")&&!s->hidden&&!native.reduced_motion);
        CHECK(v&&v->row==w->row&&v->col==w->col&&v->anchor_row==w->anchor_row&&v->anchor_col==w->anchor_col&&v->end_row==w->end_row&&v->end_col==w->end_col&&v->top==w->top&&v->horizontal==w->horizontal&&v->fraction==w->fraction&&v->selection==w->selection&&v->canvas_zoom==w->canvas_zoom);
        CHECK(workbook_codec_header_object_count(*out,sheet)==32&&s->objects.count==32);
        for(size_t slot=0;slot<=32;slot++){
            uint64_t owner=slot?object_fixture_id(sheet,slot-1):0;const char *text=(const char *)*out;CHECK(workbook_codec_header_query(*out,sheet,owner,&text));
            const Analysis *expected=slot?&s->objects.items[slot-1].source:&s->analysis;CHECK(text_same(text,expected->query));
            if(!slot)CHECK(a->query==text);else{
                const SheetObject *o=s->objects.items+slot-1;WorkbookCodecObjectView view={0};CHECK(workbook_codec_header_object_at(*out,sheet,slot-1,&view));
                CHECK(o->kind==OBJECT_TABLE&&view.kind==OBJECT_TABLE&&view.mark==MARK_BAR&&!view.source_id&&!view.chart_measure&&view.id==o->id&&view.id==owner&&text_same(view.name,o->name));
                CHECK(view.row==10+(slot-1)*12&&view.column==8&&view.height==8&&view.width==4&&expected->kind==SOURCE_CSV&&text_same(expected->path,a->path));
            }
            if(canonical){char *exact=query_fixture_text(sheet,slot);CHECK(text&&!strcmp(text,exact));free(exact);}
        }
    }
    const char *saved=(const char *)*out,*invalid=saved;CHECK(!workbook_codec_header_query(*out,64,0,&invalid)&&invalid==saved);
    CHECK(!workbook_codec_header_query(*out,0,UINT64_MAX,&invalid)&&invalid==saved);CHECK(!workbook_codec_header_query(*out,0,0,NULL));
    QueryHeaderStamp sealed=queries_stamp(*out);workbook_free(&native);kept_queries_header(*out,sealed);
    FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);free(error);
}
static void queries_rejected(const char *path,unsigned version,bool missing_table,size_t first,const char *reason,bool mapped_null,WorkbookCodecHeader **out) {CHECK(version==15);
    FileImage before=file_image(path);Workbook native;workbook_init(&native);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&native,"Retained native",&blank,&error));Workbook old=native;
    CHECK(!workbook_load(&native,path,&error)&&!memcmp(&native,&old,sizeof(native))&&!strcmp(native.sheets[0].name,"Retained native"));clear_error(&error);
    char *target=dx_absolute_path(path);CHECK(target);duckdb_database database=NULL;duckdb_connection connection=NULL;open_readonly(path,&database,&connection);
    QueryHeaderStamp stamp=queries_stamp(*out);WorkbookCodecRead *read=queries_begin(connection,version,target,stamp.objects.bytes);size_t owned=workbook_codec_read_owned_bytes(read);
    if(missing_table){CHECK(!workbook_codec_read_add_queries(read,false,&error)&&error&&strstr(error,"requires")&&!workbook_codec_read_request(read)&&workbook_codec_read_owned_bytes(read)==owned);clear_error(&error);}
    else{
        CHECK(workbook_codec_read_add_queries(read,true,&error)&&!error);unsigned prior_queries=executions;
        CHECK(result_feed(connection,read,NULL,256,true,stamp.objects.bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,reason));
        CHECK(executions==prior_queries+1&&query_last_first==first&&workbook_codec_read_request(read)->first_row==first&&workbook_codec_read_owned_bytes(read)==query_last_owned);
        if(mapped_null)CHECK(query_null_rows>=1&&strstr(error,"SQL NULL"));clear_error(&error);pending_queries_header(read,out);
    }
    kept_queries_header(*out,stamp);workbook_codec_read_free(read);duckdb_disconnect(&connection);duckdb_close(&database);free(target);workbook_free(&native);kept_queries_header(*out,stamp);
    FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);free(error);
}
static void queries_empty(const char *path,WorkbookCodecHeader **out) {
    Workbook native;workbook_init(&native);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&native,"Blank primary",&blank,&error)&&workbook_save(&native,path,&error));workbook_free(&native);
    FileImage before=file_image(path);char *target=dx_absolute_path(path);CHECK(target);duckdb_database database=NULL;duckdb_connection connection=NULL;open_readonly(path,&database,&connection);
    WorkbookCodecRead *read=NULL;size_t retained=workbook_codec_header_owned_bytes(*out);CHECK(workbook_codec_read_begin(&read,key,15,WORKBOOK_CODEC_SHEETS_25,QUERY_HEADER_LIMIT,resolve,target,&error));
    CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    CHECK(workbook_codec_read_add_objects(read,true,&error)&&result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);
    CHECK(workbook_codec_read_add_queries(read,true,&error));unsigned prior_queries=executions,prior_feeds=feeds;
    CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!query_result_rows&&executions==prior_queries+1&&feeds==prior_feeds+1);
    CHECK(workbook_codec_read_finish(read,out,&error)&&!error);workbook_codec_read_free(read);duckdb_disconnect(&connection);duckdb_close(&database);free(target);
    const char *text=(const char *)*out;CHECK(workbook_codec_header_queries_ready(*out)&&workbook_codec_header_query(*out,0,0,&text)&&!text&&!workbook_codec_header_analysis(*out,0)->query);
    FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);
}
static int queries_only(void) {
    char temporary[]="/tmp/duxcel-codec-queries-XXXXXX";CHECK(mkdtemp(temporary));char *directory=dx_absolute_path(temporary);CHECK(directory);char *missing=dx_printf("%s/missing-source.csv",directory);CHECK(access(missing,F_OK)!=0);WorkbookCodecHeader *header=NULL;
    for(size_t i=0;i<4;i++){
        const EmitFixture *entry=emit_queries_fixtures+i;char *path=dx_printf("%s/%s",directory,entry->name);queries_fixture(path,missing,entry->version);
        if(entry->reordered){mutate(path,reorder_sql);mutate(path,objects_reorder_sql);mutate(path,queries_reorder_sql);}
        if(queries_post_sql[i]){mutate(path,queries_post_sql[i]);queries_rejected(path,15,false,2048,"SQL NULL",true,&header);}
        else queries_compare(path,entry->version,true,!entry->reordered,&header);
        CHECK(!unlink(path));free(path);
    }
    for(unsigned version=15;version<=15;version++){
        char *path=dx_printf("%s/absent%u.dxls",directory,version);queries_fixture(path,missing,version);mutate(path,"DROP TABLE _duxcel_workspace.queries");queries_rejected(path,version,true,0,"",false,&header);CHECK(!unlink(path));free(path);
    }
    const char *invalid[]={
        "UPDATE _duxcel_workspace.queries SET query=NULL WHERE sheet_id=0 AND object_id=0",
        "UPDATE _duxcel_workspace.queries SET query=repeat('x',65537) WHERE sheet_id=0 AND object_id=0",
        "CREATE TABLE _duxcel_workspace.bad_queries AS SELECT * FROM _duxcel_workspace.queries WHERE NOT(sheet_id=63 AND object_id=18446744073709551615::UBIGINT) UNION ALL SELECT * FROM _duxcel_workspace.queries WHERE sheet_id=0 AND object_id=0; DROP TABLE _duxcel_workspace.queries; ALTER TABLE _duxcel_workspace.bad_queries RENAME TO queries",
        "UPDATE _duxcel_workspace.objects SET kind=0 WHERE sheet_id=63 AND id=18446744073709551615::UBIGINT",
        "UPDATE _duxcel_workspace.queries SET sheet_id=64 WHERE sheet_id=63 AND object_id=18446744073709551615::UBIGINT",
        "INSERT INTO _duxcel_workspace.queries(sheet_id,object_id,query) VALUES (64,0,'sentinel')",
        "CREATE TABLE _duxcel_workspace.bad_queries AS SELECT sheet_id,object_id::VARCHAR AS object_id,query FROM _duxcel_workspace.queries; DROP TABLE _duxcel_workspace.queries; ALTER TABLE _duxcel_workspace.bad_queries RENAME TO queries"
    };
    const char *reasons[]={"SQL NULL","SQL NULL","ordered","source owner","ordered","Too many query definitions","SQL type"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++){
        char *path=dx_printf("%s/rejected%zu.dxls",directory,i);queries_fixture(path,missing,15);mutate(path,invalid[i]);queries_rejected(path,15,false,i>=3&&i<=5?2048:0,reasons[i],i<2,&header);CHECK(!unlink(path));free(path);
    }
    char *path=dx_printf("%s/limit.dxls",directory);queries_fixture(path,missing,15);mutate(path,"UPDATE _duxcel_workspace.queries SET query=repeat('x',65536) WHERE sheet_id=0 AND object_id=0");queries_compare(path,15,false,false,&header);
    const char *text=NULL;CHECK(workbook_codec_header_query(header,0,0,&text)&&text&&strlen(text)==65536);for(size_t i=0;i<65536;i++)CHECK(text[i]=='x');
    mutate(path,"DELETE FROM _duxcel_workspace.queries WHERE sheet_id=0 AND object_id=0");queries_compare(path,15,false,false,&header);CHECK(workbook_codec_header_query(header,0,0,&text)&&!text);
    mutate(path,"INSERT INTO _duxcel_workspace.queries(sheet_id,object_id,query) VALUES (0,0,'NULL')");queries_compare(path,15,true,false,&header);CHECK(!unlink(path));free(path);
    path=dx_printf("%s/blank.dxls",directory);queries_empty(path,&header);CHECK(!unlink(path));free(path);workbook_codec_header_free(header);
    CHECK(nul_cells==1&&access(missing,F_OK)!=0);free(missing);CHECK(!rmdir(directory));free(directory);
    printf("PASS: native source queries %u checks; %u metadata executions/%u typed feeds;2112 primary/Table definitions, exact bytes/reorder/64KiB, all-version required tables, realNULL/NUL/blank/duplicates/owner/sentinel rejection, owned state/budget/stale/recovery; no source execution or Worker/App Open claim; engine %s\n",checks,executions,feeds,duckdb_library_version());return 0;
}
#undef QUERY_HEADER_LIMIT
