/* Focused actual-SDK descriptor adapter. Native validates its complete model;
   the shared cursor owns only the eleven-column object descriptor family. */
#define OBJECT_BASE UINT64_C(9007199254740993)
#define OBJECT_HEADER_LIMIT (8u*1024u*1024u)
static uint64_t object_fixture_id(size_t sheet,size_t object) {
    return sheet==63 && object==31 ? UINT64_MAX : OBJECT_BASE+object;
}
static void objects_fixture(const char *path,const char *missing,unsigned version) {CHECK(version==15);
    Workbook book; workbook_init(&book); char *error=NULL;
    for (size_t sheet=0;sheet<64;sheet++) {
        Analysis a={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),
            .query=dx_strdup("SELECT * FROM __BP1_SOURCE_MUST_NOT_EXECUTE")};
        char title[64]; snprintf(title,sizeof(title),"Sorted 'İ' %zu",sheet);
        CHECK(workbook_add(&book,title,&a,&error)); analysis_free(&a);
        Worksheet *s=book.sheets+sheet;
        s->view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
        s->objects.items=dx_alloc(32*sizeof(*s->objects.items)); s->objects.count=32;
        for (size_t j=0;j<32;j++) {
            SheetObject *o=s->objects.items+j;
            *o=(SheetObject){.id=object_fixture_id(sheet,j),.row=10+j*12,.rows=8,.col=8,.columns=4,.revision=1,
                .kind=j==31 ? OBJECT_TABLE : j%2 ? OBJECT_CHART : OBJECT_PIVOT,
                .mark=j<31 && j%2 ? (ChartMark)(j%4) : MARK_BAR,
                .source_id=j==31 ? 0 : object_fixture_id(sheet,31),
                .name=j ? dx_printf("Object 'İ' %zu:%zu",sheet,j) : dx_strdup("NULL")};
            if (j==31) o->source=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),
                .query=dx_strdup("SELECT * FROM __BP1_SOURCE_MUST_NOT_EXECUTE")};
        }
        s->objects.next_id=object_fixture_id(sheet,31);
    }
    book.active=63; book.user_scale=1.25f;

     book.sheets[0].formats.source_aware=true;
    CHECK(workbook_save(&book,path,&error) && !error); workbook_free(&book);
}
static const EmitFixture emit_objects_fixtures[]={

    {"objects-v15.dxls",15,25,false},{"objects-v15-reordered.dxls",15,25,true}
};
static const char objects_reorder_sql[]="CREATE TABLE _duxcel_workspace.reordered_objects AS SELECT chart_measure,source_id,mark,kind,width,height,column_id,row_id,name,id,sheet_id FROM _duxcel_workspace.objects; DROP TABLE _duxcel_workspace.objects; ALTER TABLE _duxcel_workspace.reordered_objects RENAME TO objects";
static uint64_t object_hash_word(uint64_t h,uint64_t word) {
    for (size_t i=0;i<8;i++) { h^=(unsigned char)(word>>(i*8)); h*=UINT64_C(1099511628211); }
    return h;
}
typedef struct { WorkbookCodecHeader *identity; size_t bytes; uint64_t digest; } ObjectHeaderStamp;
static ObjectHeaderStamp objects_stamp(WorkbookCodecHeader *header) {
    ObjectHeaderStamp out={header,workbook_codec_header_owned_bytes(header),UINT64_C(1469598103934665603)};
    out.digest=object_hash_word(out.digest,workbook_codec_header_sheet_count(header));
    out.digest=object_hash_word(out.digest,workbook_codec_header_version(header));
    out.digest=object_hash_word(out.digest,workbook_codec_header_objects_ready(header));
    for (size_t sheet=0;sheet<workbook_codec_header_sheet_count(header);sheet++) {
        size_t count=workbook_codec_header_object_count(header,sheet); out.digest=object_hash_word(out.digest,count);
        for (size_t i=0;i<count;i++) {
            WorkbookCodecObjectView o={0}; CHECK(workbook_codec_header_object_at(header,sheet,i,&o));
            const uint64_t words[]={o.id,o.row,o.column,o.height,o.width,o.kind,o.mark,o.source_id,o.chart_measure};
            for (size_t j=0;j<sizeof(words)/sizeof(*words);j++) out.digest=object_hash_word(out.digest,words[j]);
            CHECK(o.name); for (const unsigned char *p=(const unsigned char *)o.name;*p;p++) { out.digest^=*p; out.digest*=UINT64_C(1099511628211); }
        }
    }
    return out;
}
static void kept_objects_header(WorkbookCodecHeader *header,ObjectHeaderStamp before) {
    ObjectHeaderStamp after=objects_stamp(header);
    CHECK(after.identity==before.identity && after.bytes==before.bytes && after.digest==before.digest);
}
static void pending_objects_header(WorkbookCodecRead *read,WorkbookCodecHeader **out) {
    ObjectHeaderStamp before=objects_stamp(*out); char *error=NULL;
    CHECK(!workbook_codec_read_finish(read,out,&error)); clear_error(&error); kept_objects_header(*out,before);
}
static WorkbookCodecRead *objects_begin(duckdb_connection c,unsigned version,const char *target,size_t retained) {CHECK(version==15);
    WorkbookCodecRead *read=NULL; char *error=NULL;
    CHECK(workbook_codec_read_begin(&read,key,version,WORKBOOK_CODEC_SHEETS_25,OBJECT_HEADER_LIMIT,resolve,(void *)target,&error));
    CHECK(result_feed(c,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(result_feed(c,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    return read;
}
static void objects_compare(const char *path,unsigned version,bool present,bool faults,WorkbookCodecHeader **out) {CHECK(present);CHECK(version==15);
    FileImage before=file_image(path); Workbook native; workbook_init(&native); char *error=NULL;
    bool loaded=workbook_load(&native,path,&error);
    if (!loaded) fprintf(stderr,"object native load %s: %s\n",path,error ? error : "no diagnostic");
    CHECK(loaded && !error && native.count==64 && native.active==63);

     CHECK(native.sheets[0].formats.source_aware);
    duckdb_database database=NULL; duckdb_connection connection=NULL; open_readonly(path,&database,&connection);
    size_t retained=workbook_codec_header_owned_bytes(*out);
    WorkbookCodecRead *read=objects_begin(connection,version,native.path,retained);
    CHECK(workbook_codec_read_add_objects(read,true,&error) && !error);
     {
        const WorkbookCodecRequest *request=workbook_codec_read_request(read);
        CHECK(request && request->family==WORKBOOK_CODEC_OBJECTS && request->column_count==11 && request->source_column_count==11 && request->row_limit==2049);
        CHECK(!strcmp(request->sql,"SELECT sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure FROM _duxcel_workspace.objects ORDER BY sheet_id,id LIMIT 2049"));
        pending_objects_header(read,out);
        if (faults) {
            const char *sql=request->sql;
            const char *changes[]={"NULL::UBIGINT AS id","NULL::VARCHAR AS name","'' AS name",
                "CASE WHEN sheet_id=0 AND id=9007199254740993 THEN 'a'||chr(0)||'b' ELSE name END AS name",
                "column_id::VARCHAR AS column_id","0::UBIGINT AS height","0::INTEGER AS width",
                "1099511627776::UBIGINT AS row_id","-1::INTEGER AS column_id","4::INTEGER AS kind",
                "CASE WHEN kind=0 THEN 1 ELSE mark END AS mark","8::INTEGER AS chart_measure"};
            for (size_t i=0;i<sizeof(changes)/sizeof(*changes);i++) {
                size_t owned=workbook_codec_read_owned_bytes(read); ObjectHeaderStamp stamp=objects_stamp(*out);
                char *bad=dx_printf("SELECT * REPLACE (%s) FROM (%s)",changes[i],sql);
                CHECK(result_feed(connection,read,bad,256,false,retained,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)->first_row);
                clear_error(&error); free(bad); kept_objects_header(*out,stamp); pending_objects_header(read,out);
            }
            CHECK(nul_cells==1); /* Actual native VARCHAR vector length includes the interior NUL. */
            char *bad=dx_printf("SELECT * FROM (%s) ORDER BY sheet_id DESC,id",sql); size_t owned=workbook_codec_read_owned_bytes(read);
            CHECK(result_feed(connection,read,bad,256,false,retained,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)->first_row);
            clear_error(&error); free(bad); pending_objects_header(read,out);
            CHECK(owned+retained<OBJECT_HEADER_LIMIT); size_t bytes=OBJECT_HEADER_LIMIT-owned-retained-1;
            unsigned char *pressure=dx_alloc(bytes); memset(pressure,0,bytes);
            CHECK(result_feed(connection,read,NULL,256,false,retained+bytes,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)->first_row);
            clear_error(&error); free(pressure); pending_objects_header(read,out);
        }
        unsigned prior_queries=executions,prior_feeds=feeds;
        CHECK(result_feed(connection,read,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
        CHECK(executions==prior_queries+1 && feeds==prior_feeds+8); /* One2048-row result,8 exact bounded feeds. */
    }
    CHECK(workbook_codec_read_finish(read,out,&error) && !error && workbook_codec_header_objects_ready(*out));
    workbook_codec_read_free(read); duckdb_disconnect(&connection); duckdb_close(&database);
    CHECK(workbook_codec_header_version(*out)==version && workbook_codec_header_sheet_count(*out)==64 && workbook_codec_header_active_sheet(*out)==63 && workbook_codec_header_user_scale(*out)==1.25f);
    CHECK(!workbook_codec_header_sorts_ready(*out) && !workbook_codec_header_preferences_ready(*out));
    for (size_t sheet=0;sheet<64;sheet++) {
        const Worksheet *s=native.sheets+sheet; const Analysis *a=workbook_codec_header_analysis(*out,sheet);
        CHECK(!strcmp(workbook_codec_header_name(*out,sheet),s->name) && !strcmp(workbook_codec_header_reference(*out,sheet),"missing-source.csv"));
        CHECK(a && a->kind==SOURCE_CSV && text_same(a->path,s->analysis.path) && !strcmp(a->schema,"") && !strcmp(a->table,"NULL") && !a->query && !a->sort_column && !a->filter_count && !a->sort_count);
        CHECK(!strcmp(s->analysis.query,"SELECT * FROM __BP1_SOURCE_MUST_NOT_EXECUTE") && !s->hidden);
        const SheetView *v=workbook_codec_header_view(*out,sheet),*w=&s->view;
        CHECK(v && v->row==w->row && v->col==w->col && v->anchor_row==w->anchor_row && v->anchor_col==w->anchor_col && v->end_row==w->end_row && v->end_col==w->end_col && v->top==w->top && v->horizontal==w->horizontal && v->fraction==w->fraction && v->selection==w->selection && v->canvas_zoom==w->canvas_zoom);
        CHECK(workbook_codec_header_object_count(*out,sheet)==s->objects.count && s->objects.count==(32u));
        for (size_t j=0;j<s->objects.count;j++) {
            const SheetObject *o=s->objects.items+j; WorkbookCodecObjectView view={0};
            CHECK(workbook_codec_header_object_at(*out,sheet,j,&view));
            CHECK(view.id==o->id && view.row==o->row && view.column==o->col && view.height==o->rows && view.width==o->columns && view.kind==o->kind && view.mark==o->mark && view.source_id==o->source_id && view.chart_measure==o->chart_measure && !strcmp(view.name,o->name));
            CHECK(view.id==object_fixture_id(sheet,j) && view.height==8 && view.width==4 && view.column==8 && view.row==10+j*12 && !view.chart_measure);
            if (j==31) CHECK(o->kind==OBJECT_TABLE && !o->source_id && o->source.kind==SOURCE_CSV && !strcmp(o->source.query,"SELECT * FROM __BP1_SOURCE_MUST_NOT_EXECUTE"));
            else CHECK(o->source.kind==SOURCE_NONE && o->source_id==object_fixture_id(sheet,31));
        }
    }
    WorkbookCodecObjectView invalid; memset(&invalid,0x5a,sizeof(invalid)); WorkbookCodecObjectView saved=invalid;
    CHECK(!workbook_codec_header_object_at(*out,64,0,&invalid) && !memcmp(&invalid,&saved,sizeof(invalid)));
    CHECK(!workbook_codec_header_object_at(*out,0,32,&invalid) && !memcmp(&invalid,&saved,sizeof(invalid)));
    CHECK(!workbook_codec_header_object_at(*out,0,0,NULL));
    workbook_free(&native); FileImage after=file_image(path); CHECK(before.bytes==after.bytes && !memcmp(before.data,after.data,before.bytes)); free(before.data); free(after.data);
     { WorkbookCodecObjectView view={0}; CHECK(workbook_codec_header_object_at(*out,63,31,&view) && view.id==UINT64_MAX && !strcmp(view.name,"Object 'İ' 63:31")); CHECK(workbook_codec_header_object_at(*out,0,0,&view) && !strcmp(view.name,"NULL")); }
    free(error);
}
static void objects_graph_fault(const char *path,const char *replacement,WorkbookCodecHeader **out) {
    FileImage before=file_image(path); char *target=dx_absolute_path(path); CHECK(target);
    duckdb_database database=NULL; duckdb_connection connection=NULL; open_readonly(path,&database,&connection);
    ObjectHeaderStamp stamp=objects_stamp(*out); char *error=NULL; WorkbookCodecRead *read=objects_begin(connection,15,target,stamp.bytes);
    CHECK(workbook_codec_read_add_objects(read,true,&error)); size_t base=workbook_codec_read_owned_bytes(read);
    char *bad=dx_printf("SELECT * REPLACE (%s) FROM (%s)",replacement,workbook_codec_read_request(read)->sql);
    unsigned prior_queries=executions,prior_feeds=feeds;
    CHECK(result_feed(connection,read,bad,256,true,stamp.bytes,&error)==WORKBOOK_CODEC_READ_ERROR && error && *error);
    CHECK(executions==prior_queries+1 && feeds==prior_feeds+8 && workbook_codec_read_request(read)->first_row==1792 && workbook_codec_read_owned_bytes(read)>base);
    clear_error(&error); kept_objects_header(*out,stamp); pending_objects_header(read,out);
    free(bad); workbook_codec_read_free(read); duckdb_disconnect(&connection); duckdb_close(&database); free(target);
    FileImage after=file_image(path); CHECK(before.bytes==after.bytes && !memcmp(before.data,after.data,before.bytes)); free(before.data); free(after.data);
}
static void objects_archive_rejected(const char *path,bool missing_table,size_t first_row,WorkbookCodecHeader **out) {
    FileImage before=file_image(path); Workbook native; workbook_init(&native); char *error=NULL; Analysis blank={0};
    CHECK(workbook_add(&native,"Retained native",&blank,&error)); Workbook saved=native;
    CHECK(!workbook_load(&native,path,&error) && !memcmp(&native,&saved,sizeof(native)) && !strcmp(native.sheets[0].name,"Retained native")); clear_error(&error);
    char *target=dx_absolute_path(path); CHECK(target); duckdb_database database=NULL; duckdb_connection connection=NULL; open_readonly(path,&database,&connection);
    ObjectHeaderStamp stamp=objects_stamp(*out); WorkbookCodecRead *read=objects_begin(connection,15,target,stamp.bytes);
    if (missing_table) { CHECK(!workbook_codec_read_add_objects(read,false,&error) && !workbook_codec_read_request(read)); clear_error(&error); }
    else {
        CHECK(workbook_codec_read_add_objects(read,true,&error));
        CHECK(result_feed(connection,read,NULL,256,true,stamp.bytes,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_request(read)->first_row==first_row);
        clear_error(&error); pending_objects_header(read,out);
    }
    kept_objects_header(*out,stamp); workbook_codec_read_free(read); duckdb_disconnect(&connection); duckdb_close(&database); free(target); workbook_free(&native);
    FileImage after=file_image(path); CHECK(before.bytes==after.bytes && !memcmp(before.data,after.data,before.bytes)); free(before.data); free(after.data);
}
/* The exact current core requires object metadata even with no live entries.
   The component rejects a missing table, preserving independent prior state. */

static int objects_only(void) {
    char temporary[]="/tmp/duxcel-codec-objects-XXXXXX"; CHECK(mkdtemp(temporary)); char *directory=dx_absolute_path(temporary); CHECK(directory);
    char *missing=dx_printf("%s/missing-source.csv",directory); CHECK(access(missing,F_OK)!=0); WorkbookCodecHeader *header=NULL;
    for(size_t i=0;i<2;i++) {
        const EmitFixture *entry=emit_objects_fixtures+i; char *path=dx_printf("%s/%s",directory,entry->name); objects_fixture(path,missing,entry->version);
        if (entry->reordered) { mutate(path,reorder_sql); mutate(path,objects_reorder_sql); }
        objects_compare(path,entry->version,true,!entry->reordered,&header);
        if ( !entry->reordered) {
            objects_graph_fault(path,"CASE WHEN sheet_id=63 AND kind<>2 THEN 9007199254741993::UBIGINT ELSE source_id END AS source_id",&header);
            objects_graph_fault(path,"CASE WHEN sheet_id=63 AND id=18446744073709551615::UBIGINT THEN 0 ELSE kind END AS kind",&header);
            objects_compare(path,15,true,false,&header); /* Fresh one-result recovery, never rerun a partially accepted cursor. */
        }
        CHECK(!unlink(path)); free(path);
    }

    char *path=dx_printf("%s/rejected14.dxls",directory); objects_fixture(path,missing,15);
    mutate(path,"DELETE FROM _duxcel_workspace.objects WHERE sheet_id=62 AND id=9007199254741023; INSERT INTO _duxcel_workspace.objects(sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure) VALUES (0,9007199254741025,'Extra object',500,8,8,4,0,0,0,0)");
    objects_archive_rejected(path,false,0,&header);
    mutate(path,"DELETE FROM _duxcel_workspace.objects WHERE sheet_id=0 AND id=9007199254741025; INSERT INTO _duxcel_workspace.objects(sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure) VALUES (62,9007199254741023,'Object ''İ'' 62:30',370,8,8,4,0,0,9007199254741024,0); INSERT INTO _duxcel_workspace.objects(sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure) VALUES (64,1,'Sentinel',500,8,8,4,0,0,0,0)");
    objects_archive_rejected(path,false,2048,&header);
    mutate(path,"DELETE FROM _duxcel_workspace.objects WHERE sheet_id=64"); objects_compare(path,15,true,false,&header);
    mutate(path,"DROP TABLE _duxcel_workspace.objects"); objects_archive_rejected(path,true,0,&header);
    CHECK(!unlink(path)); free(path); workbook_codec_header_free(header);
    CHECK(nul_cells==1 && access(missing,F_OK)!=0); free(missing); CHECK(!rmdir(directory)); free(directory);
    printf("PASS: native object descriptors %u checks; %u metadata executions/%u typed feeds;2048-row native parity/forward Table owners/reorder/current required, typed faults/geometry/33-sheet/2049-sentinel/budget/stale/recovery; source definitions never executed; descriptor-only comparison, not full Workbook/Worker/App Open; engine %s\n",checks,executions,feeds,duckdb_library_version()); return 0;
}
#undef OBJECT_BASE
#undef OBJECT_HEADER_LIMIT
