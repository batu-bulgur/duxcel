/* Native owner-keyed BOOLEAN comparison. The query definitions remain owned
   and byte-exact; only archive metadata, never those definitions, is executed. */
#define OPTION_HEADER_LIMIT (8u*1024u*1024u)
static void options_fixture(const char *path,const char *missing,unsigned version) {
    queries_fixture(path,missing,version);
    Workbook book;workbook_init(&book);char *error=NULL;CHECK(workbook_load(&book,path,&error)&&!error);
    for(size_t sheet=0;sheet<book.count;sheet++){
        Worksheet *s=book.sheets+sheet;s->analysis.unique_rows=true;
        for(size_t j=0;j<s->objects.count;j++)s->objects.items[j].source.unique_rows=true;
    }
    CHECK(workbook_save(&book,path,&error)&&!error);workbook_free(&book);
}
static const EmitFixture emit_options_fixtures[]={
    {"options-v12.dxls",12,25,false},{"options-v13.dxls",13,25,false},
    {"options-v14.dxls",14,25,false},{"options-v14-reordered.dxls",14,25,true}
};
static const char options_reorder_sql[]="CREATE TABLE _duxcel_workspace.reordered_source_options AS SELECT unique_rows,object_id,sheet_id FROM _duxcel_workspace.source_options; DROP TABLE _duxcel_workspace.source_options; ALTER TABLE _duxcel_workspace.reordered_source_options RENAME TO source_options";
static int emit_options(const char *directory) {
    char reorder[3072];int n=snprintf(reorder,sizeof(reorder),"%s;%s;%s",objects_reorder_sql,queries_reorder_sql,options_reorder_sql);CHECK(n>0&&(size_t)n<sizeof(reorder));
    return emit_fixture_files(directory,emit_options_fixtures,4,options_fixture,reorder,NULL);
}
typedef struct {QueryHeaderStamp queries;uint64_t digest;} OptionHeaderStamp;
static OptionHeaderStamp options_stamp(WorkbookCodecHeader *header) {
    OptionHeaderStamp stamp={queries_stamp(header),UINT64_C(1469598103934665603)};
    bool ready=workbook_codec_header_source_options_ready(header);stamp.digest=object_hash_word(stamp.digest,ready);
    if(ready)for(size_t sheet=0;sheet<workbook_codec_header_sheet_count(header);sheet++)for(size_t slot=0;slot<=workbook_codec_header_object_count(header,sheet);slot++){
        uint64_t owner=0;if(slot){WorkbookCodecObjectView object={0};CHECK(workbook_codec_header_object_at(header,sheet,slot-1,&object));owner=object.id;if(object.kind!=OBJECT_TABLE)continue;}
        bool value=false;CHECK(workbook_codec_header_unique_rows(header,sheet,owner,&value));stamp.digest=object_hash_word(stamp.digest,owner);stamp.digest=object_hash_word(stamp.digest,value);
    }
    return stamp;
}
static void kept_options_header(WorkbookCodecHeader *header,OptionHeaderStamp before) {
    OptionHeaderStamp after=options_stamp(header);
    CHECK(after.queries.objects.identity==before.queries.objects.identity&&after.queries.objects.bytes==before.queries.objects.bytes&&after.queries.objects.digest==before.queries.objects.digest&&after.queries.digest==before.queries.digest&&after.digest==before.digest);
}
static void pending_options_header(WorkbookCodecRead *read,WorkbookCodecHeader **out) {
    OptionHeaderStamp before=options_stamp(*out);char *error=NULL;CHECK(!workbook_codec_read_finish(read,out,&error));clear_error(&error);kept_options_header(*out,before);
}
static void options_add_queries(duckdb_connection connection,WorkbookCodecRead *read,size_t retained) {
    char *error=NULL;CHECK(workbook_codec_read_add_queries(read,true,&error)&&!error);unsigned prior=executions;
    CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&executions==prior+1);
    CHECK(query_result_rows==2112&&query_batches256==8&&query_batches64==1);
}
static void options_compare(const char *path,unsigned version,size_t rows,bool queries_first,bool faults,WorkbookCodecHeader **out) {
    FileImage before=file_image(path);Workbook native;workbook_init(&native);char *error=NULL;
    bool loaded=workbook_load(&native,path,&error);if(!loaded)fprintf(stderr,"options native load: %s\n",error?error:"no diagnostic");CHECK(loaded&&!error&&native.count==64&&native.active==63);
    duckdb_database database=NULL;duckdb_connection connection=NULL;open_readonly(path,&database,&connection);
    size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *read=queries_begin(connection,version,native.path,retained);
    if(queries_first)options_add_queries(connection,read,retained);
    CHECK(workbook_codec_read_add_source_options(read,true,&error)&&!error);const WorkbookCodecRequest *request=workbook_codec_read_request(read);
    CHECK(request&&request->family==WORKBOOK_CODEC_SOURCE_OPTIONS&&request->column_count==3&&request->source_column_count==3&&request->row_limit==2113);
    CHECK(!strcmp(request->sql,"SELECT sheet_id,object_id,unique_rows FROM _duxcel_workspace.source_options ORDER BY sheet_id,object_id LIMIT 2113"));pending_options_header(read,out);
    if(faults){
        const char *changes[]={"NULL::INTEGER AS sheet_id","NULL::UBIGINT AS object_id","NULL::BOOLEAN AS unique_rows","sheet_id::VARCHAR AS sheet_id","object_id::VARCHAR AS object_id","unique_rows::INTEGER AS unique_rows"};
        for(size_t i=0;i<sizeof(changes)/sizeof(*changes);i++){
            size_t owned=workbook_codec_read_owned_bytes(read);OptionHeaderStamp stamp=options_stamp(*out);char *bad=dx_printf("SELECT * REPLACE (%s) FROM (%s)",changes[i],request->sql);
            CHECK(result_feed(connection,read,bad,256,false,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&option_last_first==0&&workbook_codec_read_owned_bytes(read)==owned);
            if(i<3)CHECK(error&&strstr(error,"SQL NULL"));else CHECK(error&&strstr(error,"SQL type"));
            if(i==2)CHECK(option_boolean_schema&&option_null_rows);if(i==5)CHECK(!option_boolean_schema);
            clear_error(&error);free(bad);kept_options_header(*out,stamp);pending_options_header(read,out);
        }
        size_t owned=workbook_codec_read_owned_bytes(read);CHECK(owned+retained+65536<OPTION_HEADER_LIMIT);size_t bytes=OPTION_HEADER_LIMIT-owned-retained-65536;
        unsigned char *pressure=dx_alloc(bytes);memset(pressure,0,bytes);
        CHECK(result_feed(connection,read,NULL,256,false,retained+bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"budget")&&option_last_first==0&&workbook_codec_read_owned_bytes(read)==owned);
        clear_error(&error);free(pressure);pending_options_header(read,out);
    }
    unsigned prior_queries=executions,prior_feeds=feeds;size_t owned=workbook_codec_read_owned_bytes(read);
    CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&option_boolean_schema&&!option_null_rows);
    CHECK(executions==prior_queries+1&&option_result_rows==rows&&feeds==prior_feeds+(rows?9u:1u));
    CHECK(option_batches256==(rows?8u:0u)&&option_batches64==(rows==2112?1u:0u)&&workbook_codec_read_owned_bytes(read)==owned); /* POD flags allocate no graph. */
    if(!queries_first)options_add_queries(connection,read,retained);
    CHECK(!workbook_codec_read_add_source_options(read,true,&error)&&error);clear_error(&error);
    CHECK(workbook_codec_read_finish(read,out,&error)&&!error);workbook_codec_read_free(read);duckdb_disconnect(&connection);duckdb_close(&database);
    CHECK(workbook_codec_header_source_options_ready(*out)&&workbook_codec_header_queries_ready(*out)&&workbook_codec_header_objects_ready(*out));
    CHECK(!workbook_codec_header_sorts_ready(*out)&&!workbook_codec_header_preferences_ready(*out)&&workbook_codec_header_version(*out)==version);
    for(size_t sheet=0;sheet<64;sheet++){
        const Worksheet *s=native.sheets+sheet;const Analysis *primary=workbook_codec_header_analysis(*out,sheet);
        if(!primary||primary->kind!=SOURCE_CSV||!text_same(primary->path,s->analysis.path)||strcmp(workbook_codec_header_reference(*out,sheet),"missing-source.csv")||strcmp(workbook_codec_header_name(*out,sheet),s->name))
            fprintf(stderr,"options sheet%zu kind%d path <%s>/<%s> reference <%s> name <%s>/<%s>\n",sheet,primary?(int)primary->kind:-1,primary&&primary->path?primary->path:"SQL NULL",s->analysis.path?s->analysis.path:"SQL NULL",workbook_codec_header_reference(*out,sheet),workbook_codec_header_name(*out,sheet),s->name);
        CHECK(primary&&primary->kind==SOURCE_CSV&&text_same(primary->path,s->analysis.path)&&!strcmp(workbook_codec_header_reference(*out,sheet),"missing-source.csv")&&!strcmp(workbook_codec_header_name(*out,sheet),s->name));
        CHECK(s->objects.count==32&&workbook_codec_header_object_count(*out,sheet)==32);
        for(size_t slot=0;slot<=32;slot++){
            uint64_t owner=slot?object_fixture_id(sheet,slot-1):0;const Analysis *expected=slot?&s->objects.items[slot-1].source:&s->analysis;
            bool value=!expected->unique_rows;CHECK(workbook_codec_header_unique_rows(*out,sheet,owner,&value)&&value==expected->unique_rows);
            if(!slot)CHECK(primary->unique_rows==value);
            const char *text=NULL;CHECK(workbook_codec_header_query(*out,sheet,owner,&text)&&text_same(text,expected->query));char *exact=query_fixture_text(sheet,slot);CHECK(text&&!strcmp(text,exact));free(exact);
        }
    }
    bool untouched=true;CHECK(!workbook_codec_header_unique_rows(*out,64,0,&untouched)&&untouched);CHECK(!workbook_codec_header_unique_rows(*out,0,UINT64_MAX,&untouched)&&untouched);CHECK(!workbook_codec_header_unique_rows(*out,0,0,NULL));
    OptionHeaderStamp sealed=options_stamp(*out);workbook_free(&native);kept_options_header(*out,sealed);
    FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);free(error);
}
static void options_rejected(const char *path,unsigned version,bool missing_table,bool with_queries,size_t first,const char *reason,WorkbookCodecHeader **out) {
    FileImage before=file_image(path);Workbook native;workbook_init(&native);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&native,"Retained native",&blank,&error));Workbook old=native;
    CHECK(!workbook_load(&native,path,&error)&&!memcmp(&native,&old,sizeof(native))&&!strcmp(native.sheets[0].name,"Retained native"));clear_error(&error);
    char *target=dx_absolute_path(path);CHECK(target);duckdb_database database=NULL;duckdb_connection connection=NULL;open_readonly(path,&database,&connection);
    OptionHeaderStamp stamp=options_stamp(*out);WorkbookCodecRead *read=queries_begin(connection,version,target,stamp.queries.objects.bytes);
    if(with_queries)options_add_queries(connection,read,stamp.queries.objects.bytes);
    size_t owned=workbook_codec_read_owned_bytes(read);
    if(missing_table){CHECK(!workbook_codec_read_add_source_options(read,false,&error)&&error&&strstr(error,"requires")&&!workbook_codec_read_request(read)&&workbook_codec_read_owned_bytes(read)==owned);clear_error(&error);}
    else{
        CHECK(workbook_codec_read_add_source_options(read,true,&error)&&!error);unsigned prior=executions;
        CHECK(result_feed(connection,read,NULL,256,true,stamp.queries.objects.bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,reason));
        CHECK(executions==prior+1&&option_last_first==first&&workbook_codec_read_request(read)->first_row==first&&workbook_codec_read_owned_bytes(read)==option_last_owned);
        if(first==2048)CHECK(option_batches256==8&&!option_batches64);clear_error(&error);pending_options_header(read,out);
    }
    kept_options_header(*out,stamp);workbook_codec_read_free(read);duckdb_disconnect(&connection);duckdb_close(&database);free(target);workbook_free(&native);kept_options_header(*out,stamp);
    FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);free(error);
}
static void options_copy_image(const char *path,FileImage image) {
    FILE *f=fopen(path,"wb");CHECK(f&&fwrite(image.data,1,image.bytes,f)==image.bytes&&!fclose(f));
}
static void options_blank(const char *path,WorkbookCodecHeader **out) {
    Workbook book;workbook_init(&book);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&book,"Blank primary",&blank,&error)&&workbook_save(&book,path,&error));workbook_free(&book);
    mutate(path,"INSERT INTO _duxcel_workspace.source_options(sheet_id,object_id,unique_rows) VALUES (0,0,false)");
    for(size_t pass=0;pass<2;pass++){
        FileImage before=file_image(path);bool loaded=workbook_load(&book,path,&error);CHECK(loaded==(pass==0));if(pass)clear_error(&error);
        char *target=dx_absolute_path(path);CHECK(target);duckdb_database database=NULL;duckdb_connection connection=NULL;open_readonly(path,&database,&connection);size_t retained=workbook_codec_header_owned_bytes(*out);
        WorkbookCodecRead *read=objects_begin(connection,12,target,retained);CHECK(workbook_codec_read_add_objects(read,true,&error)&&result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);
        CHECK(workbook_codec_read_add_queries(read,true,&error)&&result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED);
        CHECK(workbook_codec_read_add_source_options(read,true,&error));OptionHeaderStamp stamp=options_stamp(*out);
        if(pass){CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&strstr(error,"blank primary")&&workbook_codec_read_request(read)->first_row==0);clear_error(&error);kept_options_header(*out,stamp);}
        else{
            CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&option_boolean_schema&&option_result_rows==1);CHECK(workbook_codec_read_finish(read,out,&error));
            bool value=true;CHECK(workbook_codec_header_unique_rows(*out,0,0,&value)&&!value);CHECK(workbook_codec_header_analysis(*out,0)->kind==SOURCE_NONE&&!workbook_codec_header_analysis(*out,0)->unique_rows);
            const char *text=(const char *)*out;CHECK(workbook_codec_header_query(*out,0,0,&text)&&!text);
        }
        workbook_codec_read_free(read);duckdb_disconnect(&connection);duckdb_close(&database);free(target);workbook_free(&book);
        FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);
        if(!pass)mutate(path,"UPDATE _duxcel_workspace.source_options SET unique_rows=true");
    }
}
static int options_only(void) {
    char temporary[]="/tmp/duxcel-codec-options-XXXXXX";CHECK(mkdtemp(temporary));char *directory=dx_absolute_path(temporary);CHECK(directory);char *missing=dx_printf("%s/missing-source.csv",directory);CHECK(access(missing,F_OK)!=0);
    WorkbookCodecHeader *header=NULL;char *paths[4]={0};
    for(size_t i=0;i<4;i++){
        const EmitFixture *entry=emit_options_fixtures+i;paths[i]=dx_printf("%s/%s",directory,entry->name);options_fixture(paths[i],missing,entry->version);
        if(entry->reordered){mutate(paths[i],reorder_sql);mutate(paths[i],objects_reorder_sql);mutate(paths[i],queries_reorder_sql);mutate(paths[i],options_reorder_sql);}
        options_compare(paths[i],entry->version,2112,i!=3,entry->version==13,&header);
    }
    char *path=dx_printf("%s/private.dxls",directory);FileImage valid=file_image(paths[2]);
    for(size_t i=0;i<3;i++){
        FileImage original=file_image(paths[i]);options_copy_image(path,original);free(original.data);mutate(path,"DROP TABLE _duxcel_workspace.source_options");options_rejected(path,(unsigned)i+12,true,true,0,"",&header);CHECK(!unlink(path));
    }
    const char *invalid[]={
        "UPDATE _duxcel_workspace.source_options SET unique_rows=NULL WHERE sheet_id=63 AND object_id=18446744073709551615::UBIGINT",
        "CREATE TABLE _duxcel_workspace.bad_options AS SELECT * FROM _duxcel_workspace.source_options WHERE NOT(sheet_id=63 AND object_id=18446744073709551615::UBIGINT); UPDATE _duxcel_workspace.bad_options SET unique_rows=false WHERE sheet_id=63 AND object_id=9007199254741023; INSERT INTO _duxcel_workspace.bad_options SELECT * FROM _duxcel_workspace.bad_options WHERE sheet_id=63 AND object_id=9007199254741023; DROP TABLE _duxcel_workspace.source_options; ALTER TABLE _duxcel_workspace.bad_options RENAME TO source_options",
        "UPDATE _duxcel_workspace.objects SET kind=0 WHERE sheet_id=63 AND id=18446744073709551615::UBIGINT",
        "UPDATE _duxcel_workspace.source_options SET object_id=18446744073709551614::UBIGINT WHERE sheet_id=63 AND object_id=18446744073709551615::UBIGINT",
        "INSERT INTO _duxcel_workspace.source_options(sheet_id,object_id,unique_rows) VALUES (64,0,false)",
        "CREATE TABLE _duxcel_workspace.bad_options AS SELECT sheet_id,object_id,unique_rows::INTEGER AS unique_rows FROM _duxcel_workspace.source_options; DROP TABLE _duxcel_workspace.source_options; ALTER TABLE _duxcel_workspace.bad_options RENAME TO source_options"
    };
    const char *reasons[]={"SQL NULL","ordered","source owner","source owner","Too many source options","SQL type"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++){
        options_copy_image(path,valid);mutate(path,invalid[i]);options_rejected(path,14,false,i!=2,i==5?0:2048,reasons[i],&header);CHECK(!unlink(path));
    }
    options_copy_image(path,valid);mutate(path,"UPDATE _duxcel_workspace.source_options SET unique_rows=false WHERE sheet_id=0 AND object_id=0; UPDATE _duxcel_workspace.source_options SET unique_rows=false WHERE sheet_id=0 AND object_id=9007199254740993; DELETE FROM _duxcel_workspace.source_options WHERE sheet_id=63 AND object_id=18446744073709551615::UBIGINT");options_compare(path,14,2111,true,false,&header);
    bool value=true;CHECK(workbook_codec_header_unique_rows(header,0,0,&value)&&!value);value=true;CHECK(workbook_codec_header_unique_rows(header,0,object_fixture_id(0,0),&value)&&!value);value=true;CHECK(workbook_codec_header_unique_rows(header,63,UINT64_MAX,&value)&&!value);
    mutate(path,"DELETE FROM _duxcel_workspace.source_options");options_compare(path,14,0,false,false,&header);for(size_t sheet=0;sheet<64;sheet++)for(size_t slot=0;slot<=32;slot++){value=true;CHECK(workbook_codec_header_unique_rows(header,sheet,slot?object_fixture_id(sheet,slot-1):0,&value)&&!value);}
    CHECK(!unlink(path));options_copy_image(path,valid);options_compare(path,14,2112,true,false,&header);CHECK(!unlink(path));free(valid.data);
    options_blank(path,&header);CHECK(!unlink(path));free(path);workbook_codec_header_free(header);
    for(size_t i=0;i<4;i++){CHECK(!unlink(paths[i]));free(paths[i]);}CHECK(!nul_cells&&access(missing,F_OK)!=0);free(missing);CHECK(!rmdir(directory));free(directory);
    printf("PASS: native source options %u checks; %u metadata executions/%u typed feeds;2112 BOOLEAN primary/Table owners, retained exact queries/reorder/defaultfalse/duplicatefalse/NULL/type/owner/sentinel rejection, closed-handle ownership/budget/stale/recovery; no source execution or Worker/App Open claim; engine %s\n",checks,executions,feeds,duckdb_library_version());return 0;
}
#undef OPTION_HEADER_LIMIT
