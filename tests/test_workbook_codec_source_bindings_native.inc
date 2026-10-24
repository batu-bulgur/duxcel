/* Complete persisted source definitions + independent binding POD observations.
   No file proof, RAW-readiness requirement, activation or App publication. */
#define BINDING_BASE UINT64_C(9007199254740993)
#define BINDING_LIMIT (2u*1024u*1024u)
static unsigned binding_rows,binding_32,binding_18;
static size_t binding_last_first,binding_last_owned,binding_sdk_peak,binding_emit_index;
static uint64_t binding_expected(unsigned version,bool large,size_t sheet,uint64_t owner) {CHECK(version==15);

    if(!owner)return sheet==0||sheet==2?1:sheet==1?BINDING_BASE+(large?27:2):0;
    return sheet==5&&owner==BINDING_BASE?UINT64_MAX:sheet==6&&owner==BINDING_BASE+2?7:0;
}
static void binding_append(Worksheet *sheet,uint64_t owner,uint64_t version,SourceKind kind,const char *path,bool complete) {
    SourceVersions *v=&sheet->source_versions;SourceVersion *items=dx_alloc((v->count+1)*sizeof(*items));size_t at=0;
    while(at<v->count&&v->items[at].version<version)at++;
    if(at)memcpy(items,v->items,at*sizeof(*items));if(at<v->count)memcpy(items+at+1,v->items+at,(v->count-at)*sizeof(*items));free(v->items);
    items[at]=(SourceVersion){.owner=owner,.version=version,.kind=kind,.path=dx_strdup(path)};if(complete)items[at].raw=ledger_packet(owner,version,kind,path);
    v->items=items;v->count++;v->capacity=v->count;v->bytes=0;
    for(size_t i=0;i<v->count;i++)v->bytes+=sizeof(*items)+strlen(items[i].path)+1+items[i].raw.bytes;
    if(version>v->high_water)v->high_water=version;
}
static Analysis binding_source(SourceKind kind,const char *path) {
    return (Analysis){.kind=kind,.path=dx_strdup(path),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=dx_strdup("SELECT * FROM __BP1_SOURCE_BINDINGS_MUST_NOT_EXECUTE")};
}
static void bindings_fixture_make(const char *path,const char *missing,unsigned version,bool large) {CHECK(version==15);
    /* Preserve the ledger oracle's original entries byte-for-byte; this second
       real native save adds current definitions and three independent entries. */
    ledger_fixture_make(path,missing,version,large);Workbook b;workbook_init(&b);char *error=NULL;CHECK(workbook_load(&b,path,&error)&&!error);
    char *parquet=dx_printf("%.*s/missing-source.parquet",(int)(strrchr(missing,'/')-missing),missing);CHECK(access(parquet,F_OK)!=0);
    for(size_t s=0;s<64;s++) {
        Worksheet *w=b.sheets+s;free(w->name);w->name=dx_printf("Source bindings 'İ' %zu",s);
        if(s<3){analysis_free(&w->analysis);w->analysis=binding_source(s?SOURCE_PARQUET:SOURCE_CSV,s?parquet:missing);}
        for(size_t j=0;j<w->objects.count;j++)if(w->objects.items[j].kind==OBJECT_TABLE){SheetObject *o=w->objects.items+j;analysis_free(&o->source);o->source=binding_source(s==6?SOURCE_PARQUET:SOURCE_CSV,s==6?parquet:missing);}
    }
    {binding_append(b.sheets+1,0,BINDING_BASE+(large?27:2),SOURCE_PARQUET,parquet,true);binding_append(b.sheets+2,0,1,SOURCE_PARQUET,parquet,false);binding_append(b.sheets+6,BINDING_BASE+2,7,SOURCE_PARQUET,parquet,false);}
    for(size_t s=0;s<64;s++) {
        Worksheet *w=b.sheets+s;w->analysis.source_version=binding_expected(version,large,s,0);
        for(size_t j=0;j<w->objects.count;j++)if(w->objects.items[j].kind==OBJECT_TABLE)w->objects.items[j].source.source_version=binding_expected(version,large,s,w->objects.items[j].id);
        CHECK(!w->formats.source_aware&&source_versions_valid(&w->source_versions,w->objects.next_id,&error)&&!error);
    }
    CHECK(workbook_codec_min_write_version(&b)==version&&workbook_save(&b,path,&error)&&!error);workbook_free(&b);free(parquet);
}
static const EmitFixture emit_source_bindings_fixtures[]={

    {"source-bindings-v15.dxls",15,25,false},{"source-bindings-v15-reordered.dxls",15,25,true}
};
static void bindings_emit_fixture(const char *path,const char *missing,unsigned version) {CHECK(version==15);CHECK(binding_emit_index<2&&emit_source_bindings_fixtures[binding_emit_index].version==version);bindings_fixture_make(path,missing,version,binding_emit_index++==1);}
static const char binding_reorder_sql[]="CREATE TABLE _duxcel_workspace.binding_reordered AS SELECT version,owner,sheet_id FROM _duxcel_workspace.source_bindings; DROP TABLE _duxcel_workspace.source_bindings; ALTER TABLE _duxcel_workspace.binding_reordered RENAME TO source_bindings";
static int emit_source_bindings(const char *directory) {
    char sql[16384];int n=snprintf(sql,sizeof(sql),"%s;%s;%s;%s;%s;%s;%s;%s",objects_reorder_sql,queries_reorder_sql,options_reorder_sql,tables_reorder_sql,counters_reorder_sql,source_counters_reorder_sql,ledger_reorder_sql,binding_reorder_sql);CHECK(n>0&&(size_t)n<sizeof(sql));
    binding_emit_index=0;return emit_fixture_files(directory,emit_source_bindings_fixtures,2,bindings_emit_fixture,sql,NULL);
}
/* Each scalar POD result is materialized/executed once, then copied in <=256
   owned rows. Original INTEGER/UBIGINT/UBIGINT schema is supplied unchanged;
   wrong-original DOUBLE remains DOUBLE and fails before any narrowing. */
static WorkbookCodecReadStatus binding_feed(duckdb_connection c,WorkbookCodecRead *r,const char *override,size_t split,size_t retained,char **error) {
    const WorkbookCodecRequest *q=workbook_codec_read_request(r);CHECK(q&&q->family==WORKBOOK_CODEC_SOURCE_BINDINGS&&split&&split<=256);
    const char *sql=override?override:q->sql;duckdb_prepared_statement stmt=NULL;CHECK(duckdb_prepare(c,sql,&stmt)==DuckDBSuccess);
    CHECK(duckdb_prepared_statement_column_count(stmt)==3);WorkbookCodecColumn schema[3]={0};size_t fixed=sizeof(schema)+strlen(sql)+1;
    for(size_t i=0;i<3;i++){schema[i].name=duckdb_prepared_statement_column_name(stmt,i);CHECK(schema[i].name);schema[i].kind=atom_kind(duckdb_prepared_statement_column_type(stmt,i));fixed+=strlen(schema[i].name)+1;}
    duckdb_result result={0};CHECK(duckdb_execute_prepared(stmt,&result)==DuckDBSuccess);executions++;size_t rows=(size_t)duckdb_row_count(&result),first=q->first_row;CHECK(rows<=q->row_limit);binding_rows=(unsigned)rows;binding_32=binding_18=0;WorkbookCodecReadStatus status=WORKBOOK_CODEC_READ_ACCEPTED;
    for(size_t at=0;at<rows||(!rows&&!at);) {
        size_t take=rows-at<split?rows-at:split,owned=fixed+take*3*sizeof(WorkbookCodecAtom);WorkbookCodecAtom *atoms=take?dx_alloc(take*3*sizeof(*atoms)):NULL;
        for(size_t row=0;row<take;row++)for(size_t col=0;col<3;col++) {
            WorkbookCodecAtom *a=atoms+row*3+col;if(duckdb_value_is_null(&result,col,at+row)){a->kind=WORKBOOK_CODEC_NULL;continue;}a->kind=schema[col].kind;
            if(a->kind==WORKBOOK_CODEC_I32)a->value.i32=duckdb_value_int32(&result,col,at+row);else if(a->kind==WORKBOOK_CODEC_U64)a->value.u64=duckdb_value_uint64(&result,col,at+row);else if(a->kind==WORKBOOK_CODEC_DOUBLE)a->value.real=duckdb_value_double(&result,col,at+row);else CHECK(false);
        }
        if(owned>binding_sdk_peak)binding_sdk_peak=owned;WorkbookCodecBatch batch={schema,3,atoms,take,first,at+take==rows};binding_last_first=first;binding_last_owned=workbook_codec_read_owned_bytes(r);
        CHECK(workbook_codec_read_feed(r,(WorkbookCodecKey){1,2,3},WORKBOOK_CODEC_SOURCE_BINDINGS,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,error)==WORKBOOK_CODEC_READ_STALE&&!*error&&workbook_codec_read_owned_bytes(r)==binding_last_owned);
        status=workbook_codec_read_feed(r,key,WORKBOOK_CODEC_SOURCE_BINDINGS,&batch,retained>SIZE_MAX-owned?SIZE_MAX:retained+owned,error);feeds++;CHECK(workbook_codec_read_owned_bytes(r)==binding_last_owned);
        if(atoms)memset(atoms,0xa5,take*3*sizeof(*atoms));free(atoms);if(status!=WORKBOOK_CODEC_READ_ACCEPTED)break;binding_32+=take==32;binding_18+=take==18;first+=take;at+=take;if(!rows)break;
    }
    duckdb_destroy_result(&result);duckdb_destroy_prepare(&stmt);for(size_t i=0;i<3;i++)duckdb_free((void *)schema[i].name);return status;
}
typedef struct {const char *path;unsigned unavailable;bool ledger_phase;} BindingResolve;
static bool binding_resolve(void *context,SourceKind kind,const char *reference,size_t maximum,WorkbookCodecOwnedText *out) {
    BindingResolve *c=context;if(c->unavailable==1||(c->unavailable==2&&c->ledger_phase)||(c->unavailable==3&&!c->ledger_phase)){out->data=dx_strdup("");out->bytes=0;return true;}return resolve((void *)c->path,kind,reference,maximum,out);
}
typedef struct {LedgerStamp ledger;TableHeaderStamp tables;uint64_t digest;} BindingStamp;
static BindingStamp binding_stamp(WorkbookCodecHeader *h) {
    BindingStamp stamp={ledger_stamp(h),tables_stamp(h),UINT64_C(1469598103934665603)};stamp.digest=object_hash_word(stamp.digest,workbook_codec_header_source_bindings_ready(h));
    if(workbook_codec_header_source_bindings_ready(h))for(size_t s=0;s<workbook_codec_header_sheet_count(h);s++) {
        WorkbookCodecSourceBindingView v;CHECK(workbook_codec_header_source_binding(h,s,0,&v));stamp.digest=object_hash_word(stamp.digest,v.version);stamp.digest=object_hash_word(stamp.digest,v.locator_matched);
        for(size_t i=0;i<workbook_codec_header_object_count(h,s);i++){WorkbookCodecObjectView o;CHECK(workbook_codec_header_object_at(h,s,i,&o));if(o.kind==OBJECT_TABLE){CHECK(workbook_codec_header_source_binding(h,s,o.id,&v));stamp.digest=object_hash_word(stamp.digest,v.version);stamp.digest=object_hash_word(stamp.digest,v.locator_matched);}}
    }
    return stamp;
}
static void binding_kept(WorkbookCodecHeader *h,BindingStamp old) {ledger_kept(h,old.ledger);kept_tables_header(h,old.tables);CHECK(binding_stamp(h).digest==old.digest);}
static void binding_unfinished(WorkbookCodecRead *r,WorkbookCodecHeader **out) {BindingStamp old=binding_stamp(*out);char *error=NULL;CHECK(!workbook_codec_read_finish(r,out,&error));clear_error(&error);binding_kept(*out,old);}
static WorkbookCodecRead *binding_begin(duckdb_connection c,unsigned version,BindingResolve *locator,size_t retained) {CHECK(version==15);
    WorkbookCodecRead *r=NULL;char *error=NULL;CHECK(workbook_codec_read_begin(&r,key,version,WORKBOOK_CODEC_SHEETS_25,BINDING_LIMIT,binding_resolve,locator,&error));
    for(size_t i=0;i<2;i++){WorkbookCodecReadStatus status=result_feed(c,r,NULL,256,true,retained,&error);if(status!=WORKBOOK_CODEC_READ_ACCEPTED||error)fprintf(stderr,"binding bootstrap step%zu unavailable%u retained%zu owned%zu status%d diagnostic=%s\n",i,locator->unavailable,retained,workbook_codec_read_owned_bytes(r),status,error?error:"NULL");CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED&&!error);}
    size_t bytes=workbook_codec_read_owned_bytes(r);CHECK(!workbook_codec_read_add_source_bindings(r,true,retained,&error)&&strstr(error,"complete unclaimed")&&workbook_codec_read_owned_bytes(r)==bytes);clear_error(&error);
    CHECK(workbook_codec_read_add_objects(r,true,&error)&&result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_queries(r,true,&error)&&result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_source_options(r,true,&error)&&result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_table_sources(r,(WorkbookCodecTableSourcePresence){true,true,true},&error));for(size_t i=0;i<3;i++)CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_object_counters(r,true,&error));CHECK(result_feed(c,r,NULL,32,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_source_version_counters(r,true,&error));CHECK(result_feed(c,r,NULL,32,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    locator->ledger_phase=true;CHECK(workbook_codec_read_add_source_versions(r,true,retained,&error));CHECK(ledger_feed(c,r,NULL,256,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    return r;
}
static void binding_request(WorkbookCodecRead *r) {
    const WorkbookCodecRequest *q=workbook_codec_read_request(r);CHECK(q&&q->family==WORKBOOK_CODEC_SOURCE_BINDINGS&&q->column_count==3&&q->source_column_count==3&&q->row_limit==2113&&!q->first_row);
    CHECK(!strcmp(q->sql,"SELECT sheet_id,owner,version FROM _duxcel_workspace.source_bindings ORDER BY sheet_id,owner LIMIT 2113"));
}
static const SourceVersion *binding_native_entry(const Worksheet *s,uint64_t version) {for(size_t i=0;i<s->source_versions.count;i++)if(s->source_versions.items[i].version==version)return s->source_versions.items+i;return NULL;}
static void binding_compare(const char *path,unsigned version,bool large,unsigned unavailable,WorkbookCodecHeader **out) {CHECK(version==15);
    FileImage original=file_image(path);Workbook b;workbook_init(&b);char *error=NULL;CHECK(workbook_load(&b,path,&error)&&!error&&b.count==64);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);BindingResolve locator={b.path,unavailable,false};WorkbookCodecRead *r=binding_begin(c,version,&locator,retained);unsigned before=executions;
    CHECK(workbook_codec_read_add_source_bindings(r,true,retained,&error)&&!error);size_t bytes=workbook_codec_read_owned_bytes(r);
    {binding_request(r);binding_unfinished(r,out);CHECK(binding_feed(c,r,NULL,32,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&binding_rows==82&&binding_32==2&&binding_18==1&&executions==before+1);}
    CHECK(workbook_codec_read_owned_bytes(r)==bytes&&!workbook_codec_read_add_source_bindings(r,true,retained,&error));clear_error(&error);
    CHECK(workbook_codec_read_finish(r,out,&error)&&!error);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);
    CHECK(workbook_codec_header_source_bindings_ready(*out)&&workbook_codec_header_table_sources_ready(*out)&&workbook_codec_header_source_versions_ready(*out)&&!workbook_codec_header_pivot_marks_ready(*out));
    size_t slots=0,bound=0,ledger_counted=0,complete=0,pending=0;
    for(size_t s=0;s<64;s++) {
        Worksheet *w=b.sheets+s;const Analysis *a=workbook_codec_header_analysis(*out,s);Analysis expected=w->analysis;expected.source_version=0;if((unavailable==1||unavailable==3)&&expected.kind!=SOURCE_NONE)expected.path="";table_analysis_compare(a,&expected);CHECK(!strcmp(workbook_codec_header_name(*out,s),w->name));
        for(size_t i=0;i<=w->objects.count;i++) {
            uint64_t owner=i?w->objects.items[i-1].id:0;const Analysis *native=i?&w->objects.items[i-1].source:&w->analysis;if(i&&w->objects.items[i-1].kind!=OBJECT_TABLE)continue;
            WorkbookCodecSourceBindingView v;uint64_t want=binding_expected(version,large,s,owner);CHECK(workbook_codec_header_source_binding(*out,s,owner,&v)&&v.version==want&&v.version==native->source_version&&v.locator_matched==(want&&unavailable==0));slots++;bound+=want!=0;
            if(want){const SourceVersion *entry=binding_native_entry(w,want);CHECK(entry&&entry->owner==owner&&entry->kind==native->kind&&!strcmp(entry->path,native->path));}
            if(i){WorkbookCodecTableSourceView source;CHECK(workbook_codec_header_table_source(*out,s,owner,&source)&&source.analysis&&!source.analysis->source_version);Analysis model=*native;model.source_version=0;if(unavailable==1||unavailable==3)model.path="";table_analysis_compare(source.analysis,&model);CHECK(source.locator_available==(unavailable!=1&&unavailable!=3));}
        }
        CHECK(workbook_codec_header_source_version_count(*out,s)==w->source_versions.count);ledger_counted+=w->source_versions.count;
        for(size_t i=0;i<w->source_versions.count;i++) {WorkbookCodecSourceVersionView v;const SourceVersion *n=w->source_versions.items+i;CHECK(workbook_codec_header_source_version_at(*out,s,i,&v)&&v.owner==n->owner&&v.version==n->version&&v.kind==n->kind&&v.raw.bytes==n->raw.bytes&&(!v.raw.bytes||!memcmp(v.raw.data,n->raw.data,v.raw.bytes))&&!strcmp(v.reference,ledger_basename(v.kind))&&v.locator_resolved==(unavailable!=1&&unavailable!=2));CHECK(unavailable==1||unavailable==2?!*v.locator:!strcmp(v.locator,n->path));complete+=v.raw.bytes!=0;pending+=v.raw.bytes==0;}
    }
    CHECK(slots==82&&bound==(5u)&&ledger_counted==((large?288u:8u))&&complete==((large?191u:4u))&&pending==((large?97u:4u)));
    WorkbookCodecSourceBindingView v;memset(&v,0x5a,sizeof(v));unsigned char before_bytes[sizeof(v)];memcpy(before_bytes,&v,sizeof(v));CHECK(!workbook_codec_header_source_binding(*out,64,0,&v)&&!memcmp(before_bytes,&v,sizeof(v))&&!workbook_codec_header_source_binding(*out,3,1,&v)&&!memcmp(before_bytes,&v,sizeof(v))&&!workbook_codec_header_source_binding(*out,1,BINDING_BASE+30,&v)&&!memcmp(before_bytes,&v,sizeof(v))&&!workbook_codec_header_source_binding(*out,0,0,NULL));
    BindingStamp old=binding_stamp(*out);workbook_free(&b);binding_kept(*out,old);FileImage after=file_image(path);CHECK(original.bytes==after.bytes&&!memcmp(original.data,after.data,original.bytes));free(original.data);free(after.data);
}
static void binding_rejected(const char *path,unsigned version,bool present,size_t first,WorkbookCodecHeader **out) {CHECK(version==15);
    FileImage original=file_image(path);Workbook native;workbook_init(&native);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&native,"Retained source bindings",&blank,&error));unsigned char saved[sizeof(native)];memcpy(saved,&native,sizeof(native));CHECK(!workbook_load(&native,path,&error)&&error&&!memcmp(saved,&native,sizeof(native)));clear_error(&error);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);char *absolute=dx_absolute_path(path);BindingResolve locator={absolute,0,false};BindingStamp old=binding_stamp(*out);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *r=binding_begin(c,version,&locator,retained);
    if(!present){CHECK(!workbook_codec_read_add_source_bindings(r,present,retained,&error)&&error&&!workbook_codec_read_request(r));}
    else{CHECK(workbook_codec_read_add_source_bindings(r,true,retained,&error));binding_request(r);WorkbookCodecReadStatus status=binding_feed(c,r,NULL,32,retained,&error);const WorkbookCodecRequest *q=workbook_codec_read_request(r);if(status!=WORKBOOK_CODEC_READ_ERROR||!q||q->first_row!=first)fprintf(stderr,"binding reject status%d expected_first%zu actual_first%zu diagnostic%s\n",status,first,q?q->first_row:SIZE_MAX,error?error:"NULL");CHECK(status==WORKBOOK_CODEC_READ_ERROR&&error&&q&&q->first_row==first&&workbook_codec_read_owned_bytes(r)==binding_last_owned);binding_unfinished(r,out);}
    clear_error(&error);binding_kept(*out,old);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(absolute);workbook_free(&native);FileImage after=file_image(path);CHECK(original.bytes==after.bytes&&!memcmp(original.data,after.data,original.bytes));free(original.data);free(after.data);
}
static void binding_restore(const char *path,const char *fields,const char *tail) {char *sql=dx_printf("DROP TABLE _duxcel_workspace.source_bindings; CREATE TABLE _duxcel_workspace.source_bindings AS SELECT %s FROM main.binding_backup%s ORDER BY sheet_id,owner",fields?fields:"*",tail?tail:"");mutate(path,sql);free(sql);}
static void binding_input_faults(const char *path,WorkbookCodecHeader **out) {
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);char *absolute=dx_absolute_path(path),*error=NULL;BindingResolve locator={absolute,0,false};size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *r=binding_begin(c,15,&locator,retained);BindingStamp old=binding_stamp(*out);size_t bytes=workbook_codec_read_owned_bytes(r),count=BINDING_LIMIT-bytes-retained;
    unsigned char *pressure=dx_alloc(count+1);memset(pressure,0,count+1);CHECK(!workbook_codec_read_add_source_bindings(r,true,retained+count+1,&error)&&strstr(error,"budget")&&!workbook_codec_read_request(r)&&workbook_codec_read_owned_bytes(r)==bytes);clear_error(&error);free(pressure);pressure=dx_alloc(count);memset(pressure,0,count);
    CHECK(workbook_codec_read_add_source_bindings(r,true,retained,&error));CHECK(binding_feed(c,r,NULL,32,retained+count,&error)==WORKBOOK_CODEC_READ_ERROR&&strstr(error,"budget")&&!workbook_codec_read_request(r)->first_row&&workbook_codec_read_owned_bytes(r)==bytes);clear_error(&error);free(pressure);binding_unfinished(r,out);binding_kept(*out,old);
    CHECK(binding_feed(c,r,NULL,32,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&workbook_codec_read_owned_bytes(r)==bytes&&workbook_codec_read_finish(r,out,&error)&&!error);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(absolute);
}
static void binding_save_as(const char *path,const char *directory) {
    Workbook a,b;workbook_init(&a);workbook_init(&b);char *error=NULL;CHECK(workbook_load(&a,path,&error));char *child=dx_printf("%s/binding-child",directory),*target=dx_printf("%s/book.dxls",child);CHECK(!mkdir(child,0700)&&workbook_save(&a,target,&error)&&workbook_load(&b,target,&error)&&!error);size_t bound=0;
    for(size_t s=0;s<64;s++){CHECK(a.sheets[s].analysis.source_version==b.sheets[s].analysis.source_version&&a.sheets[s].source_versions.count==b.sheets[s].source_versions.count);for(size_t i=0;i<b.sheets[s].source_versions.count;i++){const SourceVersion *x=a.sheets[s].source_versions.items+i,*y=b.sheets[s].source_versions.items+i;CHECK(x->owner==y->owner&&x->version==y->version&&x->raw.bytes==y->raw.bytes&&(!x->raw.bytes||!memcmp(x->raw.data,y->raw.data,x->raw.bytes)));}
        for(size_t i=0;i<=b.sheets[s].objects.count;i++){const Analysis *p=i?&b.sheets[s].objects.items[i-1].source:&b.sheets[s].analysis;if(i&&b.sheets[s].objects.items[i-1].kind!=OBJECT_TABLE)continue;uint64_t owner=i?b.sheets[s].objects.items[i-1].id:0;CHECK(p->source_version==binding_expected(15,false,s,owner));if(p->source_version){const SourceVersion *entry=binding_native_entry(b.sheets+s,p->source_version);CHECK(entry&&!strcmp(entry->path,p->path)&&strstr(p->path,"binding-child/../missing-source."));bound++;}}}
    CHECK(bound==5);workbook_free(&a);workbook_free(&b);CHECK(!unlink(target)&&!rmdir(child));free(target);free(child);
}
static int source_bindings_only(void) {
    char temporary[]="/tmp/duxcel-codec-source-bindings-XXXXXX";CHECK(mkdtemp(temporary));char *directory=dx_absolute_path(temporary),*missing=dx_printf("%s/missing-source.csv",directory);WorkbookCodecHeader *h=NULL;
    for(size_t i=0;i<2;i++){const EmitFixture *entry=emit_source_bindings_fixtures+i;char *path=dx_printf("%s/%s",directory,entry->name);bindings_fixture_make(path,missing,entry->version,i==1);if(i==1){mutate(path,reorder_sql);mutate(path,binding_reorder_sql);mutate(path,ledger_reorder_sql);mutate(path,tables_reorder_sql);}binding_compare(path,entry->version,i==1,0,&h);if(i==0){binding_save_as(path,directory);for(unsigned n=1;n<=3;n++)binding_compare(path,15,false,n,&h);}CHECK(!unlink(path));free(path);}
    char *path=dx_printf("%s/fault.dxls",directory);bindings_fixture_make(path,missing,15,false);binding_input_faults(path,&h);mutate(path,"CREATE TABLE main.binding_backup AS SELECT * FROM _duxcel_workspace.source_bindings");
    const char *faults[]={
        "CASE WHEN sheet_id=63 THEN NULL ELSE sheet_id END AS sheet_id,owner,version",
        "sheet_id,CASE WHEN sheet_id=63 THEN NULL ELSE owner END AS owner,version",
        "sheet_id,owner,CASE WHEN sheet_id=63 THEN NULL ELSE version END AS version",
        "sheet_id,CASE WHEN sheet_id=63 THEN 18446744073709551615::UBIGINT ELSE owner END AS owner,version",
        "sheet_id,owner,CASE WHEN sheet_id=63 THEN 1::UBIGINT ELSE version END AS version",
        "CASE WHEN sheet_id=63 THEN 62 ELSE sheet_id END AS sheet_id,owner,version",
        "sheet_id,owner,CASE WHEN sheet_id=1 AND owner=0 THEN 9007199254740994::UBIGINT ELSE version END AS version",
        "sheet_id,owner,CASE WHEN sheet_id=0 AND owner=0 THEN 9007199254740993::UBIGINT ELSE version END AS version",
        "sheet_id,owner,CASE WHEN sheet_id=6 AND owner=0 THEN 7::UBIGINT ELSE version END AS version",
        "sheet_id,CAST(owner AS DOUBLE) AS owner,version"
    };
    for(size_t i=0;i<sizeof(faults)/sizeof(*faults);i++){binding_restore(path,faults[i],NULL);binding_rejected(path,15,true,i<6?64:0,&h);}
    binding_restore(path,NULL," WHERE sheet_id<>63");binding_rejected(path,15,true,64,&h);
    binding_restore(path,NULL,NULL);mutate(path,"INSERT INTO _duxcel_workspace.source_bindings VALUES(3,1,0)");binding_rejected(path,15,true,0,&h);
    binding_restore(path,NULL,NULL);mutate(path,"UPDATE _duxcel_workspace.source_versions SET source_reference='different-source.csv' WHERE sheet_id=5 AND version=18446744073709551615::UBIGINT");binding_rejected(path,15,true,0,&h);
    mutate(path,"UPDATE _duxcel_workspace.source_versions SET source_reference='missing-source.csv' WHERE sheet_id=5 AND version=18446744073709551615::UBIGINT");binding_compare(path,15,false,0,&h);
    mutate(path,"DROP TABLE _duxcel_workspace.source_bindings");binding_rejected(path,15,false,0,&h);CHECK(!unlink(path));free(path);

    workbook_codec_header_free(h);CHECK(access(missing,F_OK)!=0);char *parquet=dx_printf("%s/missing-source.parquet",directory);CHECK(access(parquet,F_OK)!=0);free(parquet);free(missing);CHECK(!rmdir(directory));free(directory);
    printf("PASS: native source bindings %u checks; %u metadata executions/%u typed feeds; current core complete82-owner coverage split32+32+18, five CSV/Parquet pending/RAW logical matches, independent POD/unmodified Header Analysis, unavailable locators deferred, immutable ledger8/288, SaveAs reference/RAW separation, atomic late/null/type/owner/missing/budget/stale/retry; largest owned scalar SDK batch %zu bytes, engine materialized result excluded; no source SQL/file proof/activation/App Open; engine %s\n",checks,executions,feeds,binding_sdk_peak,duckdb_library_version());return 0;
}
#undef BINDING_BASE
#undef BINDING_LIMIT
