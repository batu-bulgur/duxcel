/* Known-writer owner allocation metadata. The cursor owns POD high-water
   observations, never an ObjectStore/issuer or complete Workbook. */
#define COUNTERS_LIMIT (2u*1024u*1024u)
#define COUNTERS_BASE UINT64_C(9007199254740993)
static size_t counters_objects(size_t sheet) {
    unsigned role=(unsigned)(sheet%8);return role<3?0:role==6?3:1;
}
static uint64_t counters_id(size_t sheet,size_t item) {
    unsigned role=(unsigned)(sheet%8);return role==3?1:role==7?UINT64_MAX:COUNTERS_BASE+item;
}
static uint64_t counters_expected(unsigned version,size_t sheet) {
    unsigned role=(unsigned)(sheet%8);size_t count=counters_objects(sheet);
    uint64_t live=count?counters_id(sheet,count-1):0;
    if(version==12)return live;
    return role==1?COUNTERS_BASE+30:role==2?UINT64_MAX:role==5?COUNTERS_BASE+30:live;
}
static void counters_fixture(const char *path,const char *missing,unsigned version) {
    Workbook b;workbook_init(&b);char *error=NULL;CHECK(access(missing,F_OK)!=0);
    for(size_t s=0;s<64;s++) {
        Analysis a={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=dx_strdup("SELECT * FROM __BP1_COUNTERS_MUST_NOT_EXECUTE")};
        char *name=dx_printf("Counters 'İ' %zu",s);CHECK(workbook_add(&b,name,&a,&error));free(name);analysis_free(&a);
        Worksheet *w=b.sheets+s;w->view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
        size_t count=counters_objects(s);w->objects.count=count;w->objects.items=count?dx_alloc(count*sizeof(*w->objects.items)):NULL;
        for(size_t j=0;j<count;j++) {
            unsigned role=(unsigned)(s%8);ObjectKind kind=role==3?OBJECT_PIVOT:role==4?OBJECT_CHART:role==5?OBJECT_TABLE:(ObjectKind)((s/8+j)%3);
            SheetObject *o=w->objects.items+j;*o=(SheetObject){.id=counters_id(s,j),.name=j?dx_printf("Owner 'İ' %zu:%zu",s,j):dx_strdup("NULL"),.kind=kind,.mark=kind==OBJECT_CHART?MARK_LINE:MARK_BAR,.row=10+j*20,.col=8,.rows=8,.columns=4,.revision=1};
            if(kind==OBJECT_TABLE)o->source=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=dx_strdup("SELECT * FROM __BP1_COUNTERS_MUST_NOT_EXECUTE")};
        }
        w->objects.next_id=counters_expected(version,s);CHECK(objects_valid(&w->objects,&error)&&!error);
    }
    b.active=63;b.user_scale=1.25f;if(version==14)b.sheets[0].formats.source_aware=true;
    CHECK(workbook_save(&b,path,&error)&&!error);workbook_free(&b);
}
static const EmitFixture emit_counters_fixtures[]={
    {"counters-v12.dxls",12,25,false},{"counters-v13.dxls",13,25,false},
    {"counters-v14.dxls",14,25,false},{"counters-v14-reordered.dxls",14,25,true}
};
static const char counters_reorder_sql[]="CREATE TABLE _duxcel_workspace.counter_reordered AS SELECT high_water,sheet_id FROM _duxcel_workspace.object_counters; DROP TABLE _duxcel_workspace.object_counters; ALTER TABLE _duxcel_workspace.counter_reordered RENAME TO object_counters";
static int emit_counters(const char *directory) {return emit_fixture_files(directory,emit_counters_fixtures,4,counters_fixture,counters_reorder_sql,NULL);}
typedef struct {ObjectHeaderStamp objects;uint64_t digest;} CounterStamp;
static CounterStamp counters_stamp(WorkbookCodecHeader *h) {
    CounterStamp stamp={objects_stamp(h),UINT64_C(1469598103934665603)};
    stamp.digest=object_hash_word(stamp.digest,workbook_codec_header_object_counters_ready(h));
    for(size_t s=0;s<workbook_codec_header_sheet_count(h);s++) {
        const Analysis *a=workbook_codec_header_analysis(h,s);const SheetView *v=workbook_codec_header_view(h,s);CHECK(a&&v);
        stamp.digest=table_hash_analysis(stamp.digest,a);stamp.digest=table_hash_text(stamp.digest,workbook_codec_header_name(h,s));
        stamp.digest=object_hash_word(stamp.digest,(uintptr_t)a);stamp.digest=object_hash_word(stamp.digest,(uintptr_t)v);stamp.digest=object_hash_word(stamp.digest,(uintptr_t)workbook_codec_header_name(h,s));
        for(size_t n=0;n<workbook_codec_header_object_count(h,s);n++){WorkbookCodecObjectView o;CHECK(workbook_codec_header_object_at(h,s,n,&o));stamp.digest=object_hash_word(stamp.digest,(uintptr_t)o.name);}
        if(workbook_codec_header_object_counters_ready(h)){uint64_t value=0;CHECK(workbook_codec_header_object_high_water(h,s,&value));stamp.digest=object_hash_word(stamp.digest,value);}
    }
    return stamp;
}
static void counters_kept(WorkbookCodecHeader *h,CounterStamp before) {
    CounterStamp after=counters_stamp(h);CHECK(after.objects.identity==before.objects.identity&&after.objects.bytes==before.objects.bytes&&after.objects.digest==before.objects.digest&&after.digest==before.digest);
}
static void counters_pending(WorkbookCodecRead *r,WorkbookCodecHeader **out) {
    CounterStamp stamp=counters_stamp(*out);char *error=NULL;CHECK(!workbook_codec_read_finish(r,out,&error));clear_error(&error);counters_kept(*out,stamp);
}
static WorkbookCodecRead *counters_begin(duckdb_connection c,unsigned version,const char *path,size_t retained) {
    WorkbookCodecRead *r=NULL;char *error=NULL;CHECK(workbook_codec_read_begin(&r,key,version,WORKBOOK_CODEC_SHEETS_25,COUNTERS_LIMIT,resolve,(void *)path,&error));
    for(size_t i=0;i<2;i++)CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    size_t before=workbook_codec_read_owned_bytes(r);CHECK(!workbook_codec_read_add_object_counters(r,version>=13,&error)&&strstr(error,"complete unclaimed"));clear_error(&error);CHECK(!workbook_codec_read_request(r)&&workbook_codec_read_owned_bytes(r)==before);
    CHECK(workbook_codec_read_add_objects(r,true,&error)&&!error);CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);return r;
}
static void counters_request(const WorkbookCodecRead *r) {
    const WorkbookCodecRequest *q=workbook_codec_read_request(r);CHECK(q&&q->family==WORKBOOK_CODEC_OBJECT_COUNTERS&&q->column_count==2&&q->source_column_count==2&&q->row_limit==65&&!q->first_row);
    CHECK(!strcmp(q->sql,"SELECT sheet_id,high_water FROM _duxcel_workspace.object_counters ORDER BY sheet_id LIMIT 65"));
}
static void counters_compare(const char *path,unsigned version,size_t split,WorkbookCodecHeader **out) {
    FileImage before=file_image(path);Workbook native;workbook_init(&native);char *error=NULL;CHECK(workbook_load(&native,path,&error)&&!error&&native.count==64&&native.active==63);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *r=counters_begin(c,version,native.path,retained);
    CHECK(workbook_codec_read_add_object_counters(r,version>=13,&error)&&!error);unsigned queries=executions;
    if(version>=13){counters_request(r);counters_pending(r,out);unsigned prior=feeds;CHECK(result_feed(c,r,NULL,split,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);CHECK(executions==queries+1&&feeds==prior+(64+split-1)/split);}
    else CHECK(!workbook_codec_read_request(r)&&executions==queries);
    CHECK(!workbook_codec_read_add_object_counters(r,version>=13,&error));clear_error(&error);
    CHECK(workbook_codec_read_finish(r,out,&error)&&!error);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);
    CHECK(workbook_codec_header_object_counters_ready(*out)&&workbook_codec_header_objects_ready(*out)&&!workbook_codec_header_queries_ready(*out)&&!workbook_codec_header_cells_ready(*out)&&workbook_codec_header_sheet_count(*out)==64);
    for(size_t s=0;s<64;s++) {
        const Worksheet *w=native.sheets+s;uint64_t actual=0;CHECK(workbook_codec_header_object_high_water(*out,s,&actual)&&actual==w->objects.next_id&&actual==counters_expected(version,s));
        CHECK(workbook_codec_header_object_count(*out,s)==w->objects.count&&!strcmp(workbook_codec_header_name(*out,s),w->name)&&!strcmp(workbook_codec_header_reference(*out,s),"missing-source.csv"));
        const Analysis *a=workbook_codec_header_analysis(*out,s);CHECK(a&&a->kind==SOURCE_CSV&&!a->query&&a->schema&&!*a->schema&&!strcmp(a->table,"NULL")&&text_same(a->path,w->analysis.path));
        CHECK(!strcmp(w->analysis.query,"SELECT * FROM __BP1_COUNTERS_MUST_NOT_EXECUTE"));
        for(size_t n=0;n<w->objects.count;n++){WorkbookCodecObjectView o;CHECK(workbook_codec_header_object_at(*out,s,n,&o)&&o.id==w->objects.items[n].id&&o.kind==w->objects.items[n].kind&&!strcmp(o.name,w->objects.items[n].name));}
    }
    uint64_t unchanged=UINT64_C(123456789);CHECK(!workbook_codec_header_object_high_water(*out,64,&unchanged)&&unchanged==UINT64_C(123456789)&&!workbook_codec_header_object_high_water(*out,0,NULL));
    /* The native issuer, independently from this read-only Header, must honor
       retired IDs and retain exhaustion across a real next allocation. */
    for(size_t s=0;s<8;s++) {
        ObjectStore *o=&native.sheets[s].objects;uint64_t h=o->next_id;int added=objects_add(o,1000,8,"Issued after load",&error);
        if(h==UINT64_MAX){CHECK(added<0&&o->next_id==h);clear_error(&error);}else{CHECK(added>=0&&o->items[added].id==h+1&&o->next_id==h+1);CHECK(objects_remove(o,(size_t)added)&&o->next_id==h+1);}
    }
    CounterStamp stamp=counters_stamp(*out);workbook_free(&native);counters_kept(*out,stamp);FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);
}
static void counters_rejected(const char *path,unsigned version,bool present,size_t first,WorkbookCodecHeader **out) {
    FileImage before=file_image(path);Workbook native;workbook_init(&native);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&native,"Retained native",&blank,&error));unsigned char saved[sizeof(native)];memcpy(saved,&native,sizeof(native));
    CHECK(!workbook_load(&native,path,&error)&&error&&!memcmp(saved,&native,sizeof(native))&&!strcmp(native.sheets[0].name,"Retained native"));clear_error(&error);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);CounterStamp stamp=counters_stamp(*out);char *target=dx_absolute_path(path);CHECK(target);WorkbookCodecRead *r=counters_begin(c,version,target,stamp.objects.bytes);size_t owned=workbook_codec_read_owned_bytes(r);
    if(version==12||!present){CHECK(!workbook_codec_read_add_object_counters(r,present,&error)&&strstr(error,version==12?"v12":version==13?"v13":"v14")&&!workbook_codec_read_request(r));}
    else{CHECK(workbook_codec_read_add_object_counters(r,true,&error));counters_request(r);CHECK(result_feed(c,r,NULL,32,true,stamp.objects.bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&workbook_codec_read_request(r)->first_row==first);}
    CHECK(workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);if(version>=13&&present)counters_pending(r,out);counters_kept(*out,stamp);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(target);workbook_free(&native);
    FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);
}
static void counters_input_faults(const char *path,WorkbookCodecHeader **out) {
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);char *target=dx_absolute_path(path),*error=NULL;CHECK(target);CounterStamp stamp=counters_stamp(*out);WorkbookCodecRead *r=counters_begin(c,14,target,stamp.objects.bytes);CHECK(workbook_codec_read_add_object_counters(r,true,&error));counters_request(r);
    size_t owned=workbook_codec_read_owned_bytes(r);CHECK(owned+stamp.objects.bytes<COUNTERS_LIMIT);size_t bytes=COUNTERS_LIMIT-owned-stamp.objects.bytes;unsigned char *pressure=dx_alloc(bytes);memset(pressure,0,bytes);
    CHECK(result_feed(c,r,NULL,32,true,stamp.objects.bytes+bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&strstr(error,"budget")&&!workbook_codec_read_request(r)->first_row&&workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);free(pressure);counters_pending(r,out);counters_kept(*out,stamp);
    CHECK(workbook_codec_read_feed(r,(WorkbookCodecKey){1,2,3},WORKBOOK_CODEC_OBJECT_COUNTERS,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_STALE&&!error&&workbook_codec_read_owned_bytes(r)==owned);
    CHECK(result_feed(c,r,NULL,1,true,stamp.objects.bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(r)==owned);CHECK(workbook_codec_read_finish(r,out,&error)&&!error);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(target);
    uint64_t high=0;CHECK(workbook_codec_header_object_high_water(*out,63,&high)&&high==UINT64_MAX);
}
static int counters_only(void) {
    char temporary[]="/tmp/duxcel-codec-counters-XXXXXX";CHECK(mkdtemp(temporary));char *directory=dx_absolute_path(temporary),*missing=dx_printf("%s/missing-source.csv",directory);CHECK(directory&&access(missing,F_OK)!=0);WorkbookCodecHeader *h=NULL;
    for(size_t n=0;n<4;n++) {
        const EmitFixture *entry=emit_counters_fixtures+n;char *path=dx_printf("%s/%s",directory,entry->name);counters_fixture(path,missing,entry->version);
        if(entry->reordered){mutate(path,reorder_sql);mutate(path,objects_reorder_sql);mutate(path,counters_reorder_sql);}
        counters_compare(path,entry->version,n==3?1:32,&h);CHECK(!unlink(path));free(path);
    }
    char *path=dx_printf("%s/fault.dxls",directory);counters_fixture(path,missing,14);counters_input_faults(path,&h);
    /* A separate backup schema keeps v14's exact workspace inventory intact. */
    mutate(path,"CREATE TABLE main.counter_backup AS SELECT sheet_id,high_water FROM _duxcel_workspace.object_counters");
    const char *changes[]={"CASE WHEN sheet_id=63 THEN NULL ELSE sheet_id END AS sheet_id,high_water","sheet_id,CASE WHEN sheet_id=63 THEN NULL ELSE high_water END AS high_water","CASE WHEN sheet_id=63 THEN 62 ELSE sheet_id END AS sheet_id,high_water","sheet_id,high_water","CASE WHEN sheet_id=63 THEN 64 ELSE sheet_id END AS sheet_id,high_water","sheet_id,CASE WHEN sheet_id=63 THEN high_water-1 ELSE high_water END AS high_water","sheet_id,CAST(high_water AS VARCHAR) AS high_water","sheet_id,high_water"};
    for(size_t n=0;n<sizeof(changes)/sizeof(*changes);n++) {
        char *sql=dx_printf("DROP TABLE _duxcel_workspace.object_counters; CREATE TABLE _duxcel_workspace.object_counters AS SELECT %s FROM main.counter_backup%s",changes[n],n==3?" WHERE sheet_id<>63":"");mutate(path,sql);free(sql);
        if(n==7)mutate(path,"INSERT INTO _duxcel_workspace.object_counters VALUES(64,0)");
        counters_rejected(path,14,true,n==6?0:n==7?64:32,&h);
    }
    mutate(path,"DROP TABLE _duxcel_workspace.object_counters; CREATE TABLE _duxcel_workspace.object_counters AS SELECT * FROM main.counter_backup");counters_compare(path,14,32,&h);
    mutate(path,"DROP TABLE _duxcel_workspace.object_counters");counters_rejected(path,14,false,0,&h);CHECK(!unlink(path));free(path);
    path=dx_printf("%s/absent13.dxls",directory);counters_fixture(path,missing,13);mutate(path,"DROP TABLE _duxcel_workspace.object_counters");counters_rejected(path,13,false,0,&h);CHECK(!unlink(path));free(path);
    path=dx_printf("%s/present12.dxls",directory);counters_fixture(path,missing,12);mutate(path,"CREATE TABLE _duxcel_workspace.object_counters(sheet_id INTEGER,high_water UBIGINT)");counters_rejected(path,12,true,0,&h);CHECK(!unlink(path));free(path);
    workbook_codec_header_free(h);CHECK(access(missing,F_OK)!=0);free(missing);CHECK(!rmdir(directory));free(directory);
    printf("PASS: native object counters %u checks; %u metadata executions/%u typed feeds;64-sheet v12 max-live reconstruction/v13-v14 required retired-exhausted exact U64/native issuer parity, reorder, atomic late/null/type/missing/duplicate/foreign/65-sentinel/budget/stale/recovery; source SQL never executed, partial Header only; engine %s\n",checks,executions,feeds,duckdb_library_version());return 0;
}
#undef COUNTERS_LIMIT
#undef COUNTERS_BASE
