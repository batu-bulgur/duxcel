/* Source-version high-water only. The ledger is deliberately empty: these
   observations do not validate ledger bounds, issue versions or bind sources. */
#define SOURCE_COUNTER_LIMIT (2u*1024u*1024u)
static uint64_t source_counter_expected(unsigned version,size_t sheet) {CHECK(version==15);
    static const uint64_t values[]={0,1,UINT64_C(9007199254741043),UINT64_MAX};
    return values[sheet%4];
}
static void source_counters_fixture(const char *path,const char *missing,unsigned version) {CHECK(version==15);
    Workbook book;workbook_init(&book);char *error=NULL;CHECK(access(missing,F_OK)!=0);
    for(size_t s=0;s<64;s++) {
        Analysis a={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=dx_strdup("SELECT * FROM __BP1_SOURCE_COUNTERS_MUST_NOT_EXECUTE")};
        char *name=dx_printf("Counters 'İ' %zu",s);CHECK(workbook_add(&book,name,&a,&error));free(name);analysis_free(&a);Worksheet *w=book.sheets+s;
        w->view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
        size_t count=counters_objects(s);w->objects.count=count;w->objects.items=count?dx_alloc(count*sizeof(*w->objects.items)):NULL;
        for(size_t j=0;j<count;j++) {
            size_t role=s%8;ObjectKind kind=role==3?OBJECT_PIVOT:role==4?OBJECT_CHART:role==5?OBJECT_TABLE:(ObjectKind)((s/8+j)%3);
            SheetObject *o=w->objects.items+j;*o=(SheetObject){.id=counters_id(s,j),.name=j?dx_printf("Owner 'İ' %zu:%zu",s,j):dx_strdup("NULL"),.kind=kind,.mark=kind==OBJECT_CHART?MARK_LINE:MARK_BAR,.row=10+j*20,.col=8,.rows=8,.columns=4,.revision=1};
            if(kind==OBJECT_TABLE)o->source=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=dx_strdup("SELECT * FROM __BP1_SOURCE_COUNTERS_MUST_NOT_EXECUTE")};
        }
        w->objects.next_id=counters_expected(version,s);w->source_versions.high_water=source_counter_expected(version,s);
        CHECK(objects_valid(&w->objects,&error)&&source_versions_valid(&w->source_versions,w->objects.next_id,&error)&&!error);
        CHECK(!w->formats.source_aware&&!w->source_versions.items&&!w->source_versions.count&&!w->source_versions.capacity&&!w->source_versions.bytes&&!w->analysis.source_version);
    }
    book.active=63;book.user_scale=1.25f;
    CHECK(workbook_codec_min_write_version(&book)==version); /* An empty ledger still retains its high-water. */
    CHECK(workbook_save(&book,path,&error)&&!error);workbook_free(&book);
}
static const EmitFixture emit_source_counters_fixtures[]={

    {"source-counters-v15.dxls",15,25,false},{"source-counters-v15-reordered.dxls",15,25,true}
};
static const char source_counters_reorder_sql[]="CREATE TABLE _duxcel_workspace.source_counter_reordered AS SELECT high_water,sheet_id FROM _duxcel_workspace.source_version_counters; DROP TABLE _duxcel_workspace.source_version_counters; ALTER TABLE _duxcel_workspace.source_counter_reordered RENAME TO source_version_counters";
static int emit_source_counters(const char *directory) {return emit_fixture_files(directory,emit_source_counters_fixtures,2,source_counters_fixture,source_counters_reorder_sql,NULL);}
typedef struct {CounterStamp prior;uint64_t digest;} SourceCounterStamp;
static SourceCounterStamp source_counters_stamp(WorkbookCodecHeader *h) {
    SourceCounterStamp stamp={counters_stamp(h),UINT64_C(1469598103934665603)};stamp.digest=object_hash_word(stamp.digest,workbook_codec_header_source_version_counters_ready(h));
    if(workbook_codec_header_source_version_counters_ready(h))for(size_t s=0;s<workbook_codec_header_sheet_count(h);s++){uint64_t high=0;CHECK(workbook_codec_header_source_version_high_water(h,s,&high));stamp.digest=object_hash_word(stamp.digest,high);}
    return stamp;
}
static void source_counters_kept(WorkbookCodecHeader *h,SourceCounterStamp before) {
    counters_kept(h,before.prior);SourceCounterStamp after=source_counters_stamp(h);CHECK(after.digest==before.digest);
}
static void source_counters_pending(WorkbookCodecRead *r,WorkbookCodecHeader **out) {
    SourceCounterStamp stamp=source_counters_stamp(*out);char *error=NULL;CHECK(!workbook_codec_read_finish(r,out,&error));clear_error(&error);source_counters_kept(*out,stamp);
}
static WorkbookCodecRead *source_counters_begin(duckdb_connection c,unsigned version,const char *path,size_t retained) {CHECK(version==15);
    WorkbookCodecRead *r=counters_begin(c,version,path,retained);char *error=NULL;size_t bytes=workbook_codec_read_owned_bytes(r);
    CHECK(!workbook_codec_read_add_source_version_counters(r,true,&error)&&strstr(error,"complete unclaimed"));clear_error(&error);
    CHECK(!workbook_codec_read_request(r)&&workbook_codec_read_owned_bytes(r)==bytes);
    CHECK(workbook_codec_read_add_object_counters(r,true,&error)&&!error);
    CHECK(result_feed(c,r,NULL,32,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    return r;
}
static void source_counters_request(const WorkbookCodecRead *r) {
    const WorkbookCodecRequest *q=workbook_codec_read_request(r);CHECK(q&&q->family==WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS&&q->column_count==2&&q->source_column_count==2&&q->row_limit==65&&!q->first_row);
    CHECK(!strcmp(q->sql,"SELECT sheet_id,high_water FROM _duxcel_workspace.source_version_counters ORDER BY sheet_id LIMIT 65"));
}
static void source_counters_compare(const char *path,unsigned version,size_t split,bool zero_under_max,WorkbookCodecHeader **out) {CHECK(version==15);
    FileImage before=file_image(path);Workbook native;workbook_init(&native);char *error=NULL;CHECK(workbook_load(&native,path,&error)&&!error&&native.count==64&&native.active==63);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *r=source_counters_begin(c,version,native.path,retained);size_t owned=workbook_codec_read_owned_bytes(r);
    CHECK(workbook_codec_read_add_source_version_counters(r,true,&error)&&!error);unsigned prior_queries=executions,prior_feeds=feeds;
    {source_counters_request(r);source_counters_pending(r,out);CHECK(result_feed(c,r,NULL,split,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&executions==prior_queries+1&&feeds==prior_feeds+(split==1?2u:(64+split-1)/split));}
    CHECK(workbook_codec_read_owned_bytes(r)==owned);CHECK(!workbook_codec_read_add_source_version_counters(r,true,&error));clear_error(&error);
    CHECK(workbook_codec_read_finish(r,out,&error)&&!error);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);
    CHECK(workbook_codec_header_source_version_counters_ready(*out)&&workbook_codec_header_object_counters_ready(*out)&&workbook_codec_header_objects_ready(*out)&&!workbook_codec_header_queries_ready(*out)&&!workbook_codec_header_cells_ready(*out)&&workbook_codec_header_sheet_count(*out)==64);
    for(size_t s=0;s<64;s++) {
        const Worksheet *w=native.sheets+s;uint64_t source=0,object=0,expected=zero_under_max&&s==63?0:source_counter_expected(version,s);
        CHECK(workbook_codec_header_source_version_high_water(*out,s,&source)&&source==w->source_versions.high_water&&source==expected);
        CHECK(workbook_codec_header_object_high_water(*out,s,&object)&&object==w->objects.next_id&&object==counters_expected(version,s));
        CHECK(!w->source_versions.items&&!w->source_versions.count&&!w->source_versions.capacity&&!w->source_versions.bytes&&!w->formats.source_aware&&!w->analysis.source_version);
        CHECK(workbook_codec_header_object_count(*out,s)==w->objects.count&&!strcmp(workbook_codec_header_name(*out,s),w->name)&&!strcmp(workbook_codec_header_reference(*out,s),"missing-source.csv"));
        const Analysis *a=workbook_codec_header_analysis(*out,s);const SheetView *v=workbook_codec_header_view(*out,s);CHECK(a&&a->kind==SOURCE_CSV&&!a->query&&!a->source_version&&a->schema&&!*a->schema&&!strcmp(a->table,"NULL")&&text_same(a->path,w->analysis.path));CHECK(v&&v->row==w->view.row&&v->col==w->view.col&&v->canvas_zoom==w->view.canvas_zoom&&workbook_codec_header_user_scale(*out)==native.user_scale);
        CHECK(!strcmp(w->analysis.query,"SELECT * FROM __BP1_SOURCE_COUNTERS_MUST_NOT_EXECUTE"));
        for(size_t n=0;n<w->objects.count;n++){WorkbookCodecObjectView o;const SheetObject *actual=w->objects.items+n;CHECK(workbook_codec_header_object_at(*out,s,n,&o)&&o.id==actual->id&&o.kind==actual->kind&&!strcmp(o.name,actual->name));if(actual->kind==OBJECT_TABLE)CHECK(!actual->source.source_version&&!strcmp(actual->source.query,"SELECT * FROM __BP1_SOURCE_COUNTERS_MUST_NOT_EXECUTE"));}
    }
    if(zero_under_max){uint64_t source=7,object=0;CHECK(workbook_codec_header_source_version_high_water(*out,63,&source)&&source==0&&workbook_codec_header_object_high_water(*out,63,&object)&&object==UINT64_MAX);}
    uint64_t unchanged=UINT64_C(9007199254740997);CHECK(!workbook_codec_header_source_version_high_water(*out,64,&unchanged)&&unchanged==UINT64_C(9007199254740997)&&!workbook_codec_header_source_version_high_water(NULL,0,&unchanged)&&unchanged==UINT64_C(9007199254740997)&&!workbook_codec_header_source_version_high_water(*out,0,NULL));
    SourceCounterStamp stamp=source_counters_stamp(*out);workbook_free(&native);source_counters_kept(*out,stamp);FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);
}
static void source_counters_rejected(const char *path,unsigned version,bool present,size_t first,WorkbookCodecHeader **out) {CHECK(version==15);
    FileImage before=file_image(path);Workbook native;workbook_init(&native);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&native,"Retained source counters",&blank,&error));unsigned char saved[sizeof(native)];memcpy(saved,&native,sizeof(native));
    CHECK(!workbook_load(&native,path,&error)&&error&&!memcmp(saved,&native,sizeof(native))&&!strcmp(native.sheets[0].name,"Retained source counters"));clear_error(&error);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);SourceCounterStamp stamp=source_counters_stamp(*out);char *target=dx_absolute_path(path);CHECK(target);WorkbookCodecRead *r=source_counters_begin(c,version,target,stamp.prior.objects.bytes);size_t owned=workbook_codec_read_owned_bytes(r);
    if(!present){CHECK(!workbook_codec_read_add_source_version_counters(r,present,&error)&&strstr(error,"current core")&&!workbook_codec_read_request(r));}
    else{CHECK(workbook_codec_read_add_source_version_counters(r,true,&error));source_counters_request(r);CHECK(result_feed(c,r,NULL,32,true,stamp.prior.objects.bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&workbook_codec_read_request(r)->first_row==first);}
    CHECK(workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);if(present)source_counters_pending(r,out);source_counters_kept(*out,stamp);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(target);workbook_free(&native);
    FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);
}
static void source_counters_input_faults(const char *path,WorkbookCodecHeader **out) {
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);char *target=dx_absolute_path(path),*error=NULL;CHECK(target);SourceCounterStamp stamp=source_counters_stamp(*out);WorkbookCodecRead *r=source_counters_begin(c,15,target,stamp.prior.objects.bytes);CHECK(workbook_codec_read_add_source_version_counters(r,true,&error));source_counters_request(r);
    size_t owned=workbook_codec_read_owned_bytes(r);CHECK(owned+stamp.prior.objects.bytes<SOURCE_COUNTER_LIMIT);size_t bytes=SOURCE_COUNTER_LIMIT-owned-stamp.prior.objects.bytes;unsigned char *pressure=dx_alloc(bytes);memset(pressure,0,bytes);
    /* Actual result/schema/atoms coexist beyond this exact pressure allocation. */
    CHECK(result_feed(c,r,NULL,32,true,stamp.prior.objects.bytes+bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&strstr(error,"budget")&&!workbook_codec_read_request(r)->first_row&&workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);free(pressure);source_counters_pending(r,out);source_counters_kept(*out,stamp);
    CHECK(workbook_codec_read_feed(r,(WorkbookCodecKey){1,2,3},WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE&&!error&&workbook_codec_read_owned_bytes(r)==owned);
    CHECK(result_feed(c,r,NULL,1,true,stamp.prior.objects.bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(r)==owned);CHECK(workbook_codec_read_finish(r,out,&error)&&!error);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(target);uint64_t high=0;CHECK(workbook_codec_header_source_version_high_water(*out,63,&high)&&high==UINT64_MAX);
}
static int source_counters_only(void) {
    char temporary[]="/tmp/duxcel-codec-source-counters-XXXXXX";CHECK(mkdtemp(temporary));char *directory=dx_absolute_path(temporary),*missing=dx_printf("%s/missing-source.csv",directory);CHECK(directory&&access(missing,F_OK)!=0);WorkbookCodecHeader *h=NULL;
    for(size_t n=0;n<2;n++){const EmitFixture *entry=emit_source_counters_fixtures+n;char *path=dx_printf("%s/%s",directory,entry->name);source_counters_fixture(path,missing,entry->version);if(entry->reordered){mutate(path,reorder_sql);mutate(path,objects_reorder_sql);mutate(path,counters_reorder_sql);mutate(path,source_counters_reorder_sql);}source_counters_compare(path,entry->version,n==1?1:32,false,&h);CHECK(!unlink(path));free(path);}
    char *path=dx_printf("%s/fault.dxls",directory);source_counters_fixture(path,missing,15);source_counters_input_faults(path,&h);
    mutate(path,"CREATE TABLE main.source_counter_backup AS SELECT sheet_id,high_water FROM _duxcel_workspace.source_version_counters");
    const char *changes[]={"CASE WHEN sheet_id=63 THEN NULL ELSE sheet_id END AS sheet_id,high_water","sheet_id,CASE WHEN sheet_id=63 THEN NULL ELSE high_water END AS high_water","CASE WHEN sheet_id=63 THEN 62 ELSE sheet_id END AS sheet_id,high_water","sheet_id,high_water","CASE WHEN sheet_id=63 THEN 64 ELSE sheet_id END AS sheet_id,high_water","sheet_id,CAST(high_water AS VARCHAR) AS high_water","sheet_id,high_water"};
    for(size_t n=0;n<sizeof(changes)/sizeof(*changes);n++){char *sql=dx_printf("DROP TABLE _duxcel_workspace.source_version_counters; CREATE TABLE _duxcel_workspace.source_version_counters AS SELECT %s FROM main.source_counter_backup%s",changes[n],n==3?" WHERE sheet_id<>63":"");mutate(path,sql);free(sql);if(n==6)mutate(path,"INSERT INTO _duxcel_workspace.source_version_counters VALUES(64,0)");source_counters_rejected(path,15,true,n==5?0:n==6?64:32,&h);}
    mutate(path,"DROP TABLE _duxcel_workspace.source_version_counters; CREATE TABLE _duxcel_workspace.source_version_counters AS SELECT * FROM main.source_counter_backup");source_counters_compare(path,15,32,false,&h);
    mutate(path,"UPDATE _duxcel_workspace.source_version_counters SET high_water=0 WHERE sheet_id=63");source_counters_compare(path,15,32,true,&h);
    mutate(path,"DROP TABLE _duxcel_workspace.source_version_counters");source_counters_rejected(path,15,false,0,&h);CHECK(!unlink(path));free(path);

    workbook_codec_header_free(h);CHECK(access(missing,F_OK)!=0);free(missing);CHECK(!rmdir(directory));free(directory);
    printf("PASS: native source version counters %u checks; %u metadata executions/%u typed feeds;64-sheet current independent exact high-water with empty ledger, reorder, atomic null/type/identity/coverage/65-sentinel/budget/stale/retry; source SQL never executed, partial Header only; engine %s\n",checks,executions,feeds,duckdb_library_version());return 0;
}
#undef SOURCE_COUNTER_LIMIT
