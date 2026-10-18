/* Known-writer source ledger observations, not file verification/issuance or an
   activated SourceVersions model. Only this family uses row-oriented SDK APIs;
   the existing vector result_feed adapter and its ownership are unchanged. */
#define LEDGER_BASE UINT64_C(9007199254740993)
#define LEDGER_LIMIT (2u*1024u*1024u)
static unsigned ledger_rows,ledger_256,ledger_29,ledger_pending,ledger_complete;
static size_t ledger_last_first,ledger_last_owned,ledger_sdk_peak;
static size_t ledger_emit_index;
static size_t ledger_count(bool large,size_t sheet) {
    return sheet==0?(large?257u:2u):sheet==1?(large?26u:1u):sheet==2||sheet==5?1u:0u;
}
static size_t ledger_ordinal(bool large,size_t sheet,size_t index) {
    return sheet==0?index:sheet==1?(large?257u:2u)+index:sheet==2?(large?283u:3u):(large?284u:4u);
}
static uint64_t ledger_version(bool large,size_t sheet,size_t index) {
    return sheet==0?(large?1u+index:index?LEDGER_BASE:1):sheet==1?LEDGER_BASE+1+index:UINT64_MAX;
}
static uint64_t ledger_owner(size_t sheet) {return sheet==0?0:sheet==1?LEDGER_BASE+30:sheet==2?UINT64_MAX:LEDGER_BASE;}
static uint64_t ledger_high(unsigned version,bool large,size_t sheet) {
    if(version<14)return 0;
    return sheet==0?(large?257:LEDGER_BASE+17):sheet==1?(large?LEDGER_BASE+26:LEDGER_BASE+1):sheet==2||sheet==5||sheet==63?UINT64_MAX:0;
}
static SourceKind ledger_kind(size_t ordinal) {return ordinal%2?SOURCE_PARQUET:SOURCE_CSV;}
static const char *ledger_basename(SourceKind kind) {return kind==SOURCE_CSV?"missing-source.csv":"missing-source.parquet";}
static SourcePacket ledger_packet(uint64_t owner,uint64_t version,SourceKind kind,const char *path) {
    Column columns[]={
        {.name="Id",.type_name="UBIGINT",.type=DUCKDB_TYPE_UBIGINT,.number_type={NUM_U64,0,0},.numeric=true},
        {.name="City 'İ'",.type_name="VARCHAR",.type=DUCKDB_TYPE_VARCHAR},
        {.name="Amount",.type_name="DECIMAL(38,4)",.type=DUCKDB_TYPE_DECIMAL,.number_type={NUM_DECIMAL,38,4},.numeric=true}
    };
    Page schema={.columns=columns,.column_count=3};SourceDigest digest={.bytes=LEDGER_BASE+50,.identity={.size=LEDGER_BASE+50}};
    for(size_t i=0;i<sizeof(digest.sha256);i++)digest.sha256[i]=(unsigned char)(i*7+3);
    Analysis a={.kind=kind,.path=(char *)path};SourcePacket raw={0};char *error=NULL;
    CHECK(source_scope_raw_build(owner,version,&a,&schema,&digest,duckdb_library_version(),&raw,&error)&&!error);
    CHECK(raw.bytes==141+strlen(duckdb_library_version())&&memchr(raw.data,0,raw.bytes));return raw;
}
static void ledger_fixture_make(const char *path,const char *missing,unsigned version,bool large) {
    Workbook b;workbook_init(&b);char *error=NULL;CHECK(access(missing,F_OK)!=0);
    char *parquet=dx_printf("%.*s/missing-source.parquet",(int)(strrchr(missing,'/')-missing),missing);CHECK(access(parquet,F_OK)!=0);
    for(size_t s=0;s<64;s++) {
        Analysis a={.kind=SOURCE_NONE,.path=dx_strdup("")};char *name=dx_printf("Source versions 'İ' %zu",s);
        CHECK(workbook_add(&b,name,&a,&error));free(name);analysis_free(&a);Worksheet *w=b.sheets+s;
        w->view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
        w->objects.count=counters_objects(s);w->objects.items=w->objects.count?dx_alloc(w->objects.count*sizeof(*w->objects.items)):NULL;
        for(size_t j=0;j<w->objects.count;j++) {
            size_t role=s%8;ObjectKind kind=role==3?OBJECT_PIVOT:role==4?OBJECT_CHART:role==5?OBJECT_TABLE:(ObjectKind)((s/8+j)%3);
            SheetObject *o=w->objects.items+j;*o=(SheetObject){.id=counters_id(s,j),.name=j?dx_printf("Owner 'İ' %zu:%zu",s,j):dx_strdup("NULL"),.kind=kind,.mark=kind==OBJECT_CHART?MARK_LINE:MARK_BAR,.row=10+j*20,.col=8,.rows=8,.columns=4,.revision=1};
            if(kind==OBJECT_TABLE)o->source=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),.query=dx_strdup("SELECT * FROM __BP1_SOURCE_VERSIONS_MUST_NOT_EXECUTE")};
        }
        w->objects.next_id=counters_expected(version,s);SourceVersions *v=&w->source_versions;v->high_water=ledger_high(version,large,s);
        if(version==14&&ledger_count(large,s)) {
            v->count=v->capacity=ledger_count(large,s);v->items=dx_alloc(v->count*sizeof(*v->items));v->bytes=v->count*sizeof(*v->items);
            for(size_t i=0;i<v->count;i++) {
                size_t n=ledger_ordinal(large,s,i);SourceKind kind=ledger_kind(n);SourceVersion *entry=v->items+i;
                *entry=(SourceVersion){.version=ledger_version(large,s,i),.owner=ledger_owner(s),.kind=kind,.path=dx_strdup(kind==SOURCE_CSV?missing:parquet)};
                if(n%3)entry->raw=ledger_packet(entry->owner,entry->version,kind,entry->path);
                v->bytes+=strlen(entry->path)+1+entry->raw.bytes;
            }
        }
        CHECK(objects_valid(&w->objects,&error)&&source_versions_valid(v,w->objects.next_id,&error)&&!error&&!w->formats.source_aware&&!w->analysis.source_version);
    }
    b.active=63;b.user_scale=1.25f;CHECK(workbook_codec_min_write_version(&b)==version);
    CHECK(workbook_save(&b,path,&error)&&!error);workbook_free(&b);free(parquet);
}
static void ledger_emit_fixture(const char *path,const char *missing,unsigned version) {ledger_fixture_make(path,missing,version,ledger_emit_index++==3);}
static const EmitFixture emit_source_versions_fixtures[]={
    {"source-versions-v12.dxls",12,25,false},{"source-versions-v13.dxls",13,25,false},
    {"source-versions-v14.dxls",14,25,false},{"source-versions-v14-reordered.dxls",14,25,true}
};
static const char ledger_reorder_sql[]="CREATE TABLE _duxcel_workspace.ledger_reordered AS SELECT definition,encoding,source_reference,source_kind,owner,version,sheet_id FROM _duxcel_workspace.source_versions; DROP TABLE _duxcel_workspace.source_versions; ALTER TABLE _duxcel_workspace.ledger_reordered RENAME TO source_versions";
static int emit_source_versions(const char *directory) {ledger_emit_index=0;return emit_fixture_files(directory,emit_source_versions_fixtures,4,ledger_emit_fixture,ledger_reorder_sql,NULL);}
/* A single materialized SDK result, <=256 independently owned scalar rows at a
   time. No chunks/vector API is mixed with row APIs. SDK allocations are exact
   size observations and retired with duckdb_free, after poisoning. The engine's
   materialized result/internal allocations are outside this owned C category. */
static WorkbookCodecReadStatus ledger_feed(duckdb_connection c,WorkbookCodecRead *r,const char *override,size_t split,size_t retained,char **error) {
    const WorkbookCodecRequest *q=workbook_codec_read_request(r);CHECK(q&&q->family==WORKBOOK_CODEC_SOURCE_VERSIONS&&split&&split<=256);
    const char *sql=override?override:q->sql;duckdb_prepared_statement stmt=NULL;CHECK(duckdb_prepare(c,sql,&stmt)==DuckDBSuccess);
    size_t columns=(size_t)duckdb_prepared_statement_column_count(stmt);CHECK(columns==7);WorkbookCodecColumn schema[7]={0};duckdb_type types[7]={0};size_t fixed=sizeof(schema)+sizeof(types)+strlen(sql)+1;
    for(size_t i=0;i<7;i++){schema[i].name=duckdb_prepared_statement_column_name(stmt,i);CHECK(schema[i].name);types[i]=duckdb_prepared_statement_column_type(stmt,i);schema[i].kind=types[i]==DUCKDB_TYPE_BLOB?WORKBOOK_CODEC_BLOB:atom_kind(types[i]);fixed+=strlen(schema[i].name)+1;}
    duckdb_result result={0};CHECK(duckdb_execute_prepared(stmt,&result)==DuckDBSuccess);executions++;size_t rows=(size_t)duckdb_row_count(&result),first=q->first_row;CHECK(rows<=q->row_limit);
    ledger_rows=(unsigned)rows;ledger_256=ledger_29=ledger_pending=ledger_complete=0;WorkbookCodecReadStatus status=WORKBOOK_CODEC_READ_ACCEPTED;
    for(size_t at=0;at<rows||(!rows&&!at);) {
        size_t take=rows-at<split?rows-at:split;WorkbookCodecAtom *atoms=take?dx_alloc(take*7*sizeof(*atoms)):NULL;size_t owned=fixed+take*7*sizeof(*atoms);
        for(size_t row=0;row<take;row++)for(size_t col=0;col<7;col++) {
            WorkbookCodecAtom *a=atoms+row*7+col;if(duckdb_value_is_null(&result,col,at+row)){a->kind=WORKBOOK_CODEC_NULL;if(col==6)ledger_pending++;continue;}
            a->kind=schema[col].kind;
            if(a->kind==WORKBOOK_CODEC_I32)a->value.i32=duckdb_value_int32(&result,col,at+row);
            else if(a->kind==WORKBOOK_CODEC_U64)a->value.u64=duckdb_value_uint64(&result,col,at+row);
            else if(a->kind==WORKBOOK_CODEC_TEXT){duckdb_string text=duckdb_value_string(&result,col,at+row);CHECK(text.data);a->value.text=(WorkbookCodecText){text.data,(size_t)text.size};CHECK(text.size<SIZE_MAX-owned);owned+=(size_t)text.size+1;}
            else if(a->kind==WORKBOOK_CODEC_BLOB){duckdb_blob blob=duckdb_value_blob(&result,col,at+row);CHECK(blob.data||!blob.size);a->value.blob=(WorkbookCodecBlob){blob.data,(size_t)blob.size};CHECK(blob.size<=SIZE_MAX-owned);owned+=(size_t)blob.size;if(col==6)ledger_complete++;}
            else CHECK(false);
        }
        CHECK(owned<32u*1024u*1024u);if(owned>ledger_sdk_peak)ledger_sdk_peak=owned;
        WorkbookCodecBatch batch={schema,7,atoms,take,first,at+take==rows};ledger_last_first=first;ledger_last_owned=workbook_codec_read_owned_bytes(r);
        CHECK(workbook_codec_read_feed(r,(WorkbookCodecKey){1,2,3},WORKBOOK_CODEC_SOURCE_VERSIONS,(const WorkbookCodecBatch *)(uintptr_t)1,SIZE_MAX,error)==WORKBOOK_CODEC_READ_STALE&&!*error&&workbook_codec_read_owned_bytes(r)==ledger_last_owned);
        status=workbook_codec_read_feed(r,key,WORKBOOK_CODEC_SOURCE_VERSIONS,&batch,retained>SIZE_MAX-owned?SIZE_MAX:retained+owned,error);feeds++;
        for(size_t row=0;row<take;row++)for(size_t col=0;col<7;col++) {WorkbookCodecAtom *a=atoms+row*7+col;if(a->kind==WORKBOOK_CODEC_TEXT){memset((void *)a->value.text.data,0xa5,a->value.text.bytes);duckdb_free((void *)a->value.text.data);}else if(a->kind==WORKBOOK_CODEC_BLOB){if(a->value.blob.bytes)memset((void *)a->value.blob.data,0xa5,a->value.blob.bytes);duckdb_free((void *)a->value.blob.data);}}
        if(atoms)memset(atoms,0xa5,take*7*sizeof(*atoms));free(atoms);
        if(status!=WORKBOOK_CODEC_READ_ACCEPTED)break;ledger_256+=take==256;ledger_29+=take==29;first+=take;at+=take;if(!rows)break;
    }
    duckdb_destroy_result(&result);duckdb_destroy_prepare(&stmt);for(size_t i=0;i<7;i++)duckdb_free((void *)schema[i].name);return status;
}
typedef struct {SourceCounterStamp prior;uint64_t digest;} LedgerStamp;
static LedgerStamp ledger_stamp(WorkbookCodecHeader *h) {
    LedgerStamp stamp={source_counters_stamp(h),UINT64_C(1469598103934665603)};stamp.digest=object_hash_word(stamp.digest,workbook_codec_header_source_versions_ready(h));
    if(workbook_codec_header_source_versions_ready(h))for(size_t s=0;s<workbook_codec_header_sheet_count(h);s++) {
        size_t count=workbook_codec_header_source_version_count(h,s);stamp.digest=object_hash_word(stamp.digest,count);
        for(size_t i=0;i<count;i++){WorkbookCodecSourceVersionView v;CHECK(workbook_codec_header_source_version_at(h,s,i,&v));stamp.digest=object_hash_word(stamp.digest,v.version);stamp.digest=object_hash_word(stamp.digest,v.owner);stamp.digest=object_hash_word(stamp.digest,v.kind);stamp.digest=object_hash_word(stamp.digest,(uintptr_t)v.reference);stamp.digest=table_hash_text(stamp.digest,v.reference);stamp.digest=object_hash_word(stamp.digest,(uintptr_t)v.locator);stamp.digest=table_hash_text(stamp.digest,v.locator);stamp.digest=object_hash_word(stamp.digest,v.locator_resolved);stamp.digest=object_hash_word(stamp.digest,(uintptr_t)v.raw.data);stamp.digest=object_hash_word(stamp.digest,v.raw.bytes);for(size_t n=0;n<v.raw.bytes;n++)stamp.digest=object_hash_word(stamp.digest,v.raw.data[n]);}
    }
    return stamp;
}
static void ledger_kept(WorkbookCodecHeader *h,LedgerStamp before) {source_counters_kept(h,before.prior);CHECK(ledger_stamp(h).digest==before.digest);}
static WorkbookCodecRead *ledger_begin(duckdb_connection c,unsigned version,const char *path,size_t retained,size_t limit,WorkbookCodecResolve locator) {
    WorkbookCodecRead *r=NULL;char *error=NULL;CHECK(workbook_codec_read_begin(&r,key,version,WORKBOOK_CODEC_SHEETS_25,limit,locator,(void *)path,&error));
    CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(!workbook_codec_read_add_source_versions(r,version==14,retained,&error)&&strstr(error,"complete unclaimed"));clear_error(&error);
    CHECK(workbook_codec_read_add_objects(r,true,&error)&&result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_object_counters(r,version>=13,&error));if(version>=13)CHECK(result_feed(c,r,NULL,32,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    CHECK(workbook_codec_read_add_source_version_counters(r,version==14,&error));if(version==14)CHECK(result_feed(c,r,NULL,32,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);
    return r;
}
static void ledger_request(WorkbookCodecRead *r) {
    const WorkbookCodecRequest *q=workbook_codec_read_request(r);CHECK(q&&q->family==WORKBOOK_CODEC_SOURCE_VERSIONS&&q->column_count==7&&q->source_column_count==7&&q->row_limit==65537&&!q->first_row);
    CHECK(!strcmp(q->sql,"SELECT sheet_id,version,owner,source_kind,source_reference,encoding,definition FROM _duxcel_workspace.source_versions ORDER BY sheet_id,version LIMIT 65537"));
}
static void ledger_unfinished(WorkbookCodecRead *r,WorkbookCodecHeader **out) {LedgerStamp old=ledger_stamp(*out);char *error=NULL;CHECK(!workbook_codec_read_finish(r,out,&error));clear_error(&error);ledger_kept(*out,old);}
static void ledger_compare(const char *path,unsigned version,bool large,bool unresolved,WorkbookCodecHeader **out) {
    FileImage before=file_image(path);Workbook native;workbook_init(&native);char *error=NULL;CHECK(workbook_load(&native,path,&error)&&!error&&native.count==64);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *r=ledger_begin(c,version,native.path,retained,LEDGER_LIMIT,unresolved?NULL:resolve);unsigned old_exec=executions;
    CHECK(workbook_codec_read_add_source_versions(r,version==14,retained,&error)&&!error);
    if(version==14){ledger_request(r);ledger_unfinished(r,out);CHECK(ledger_feed(c,r,NULL,256,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&executions==old_exec+1&&ledger_rows==(large?285u:5u));CHECK(ledger_256==(large?1u:0u)&&ledger_29==(large?1u:0u)&&ledger_pending==(large?95u:2u)&&ledger_complete==(large?190u:3u));}
    else CHECK(executions==old_exec&&!workbook_codec_read_request(r));
    size_t owned=workbook_codec_read_owned_bytes(r);CHECK(!workbook_codec_read_add_source_versions(r,version==14,retained,&error)&&workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);
    CHECK(workbook_codec_read_finish(r,out,&error)&&!error);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);
    CHECK(workbook_codec_header_source_versions_ready(*out)&&workbook_codec_header_source_version_counters_ready(*out)&&workbook_codec_header_object_counters_ready(*out)&&!workbook_codec_header_queries_ready(*out));
    for(size_t s=0;s<64;s++) {
        Worksheet *w=native.sheets+s;const Analysis *a=workbook_codec_header_analysis(*out,s);CHECK(a&&a->kind==SOURCE_NONE&&a->path&&!*a->path&&!a->query&&!a->source_version&&!strcmp(workbook_codec_header_reference(*out,s),"")&&!strcmp(workbook_codec_header_name(*out,s),w->name));
        uint64_t high=0;CHECK(workbook_codec_header_source_version_high_water(*out,s,&high)&&high==ledger_high(version,large,s)&&high==w->source_versions.high_water);
        size_t count=version==14?ledger_count(large,s):0;CHECK(workbook_codec_header_source_version_count(*out,s)==count&&count==w->source_versions.count);
        for(size_t i=0;i<count;i++) {
            WorkbookCodecSourceVersionView v;SourceVersion *n=w->source_versions.items+i;size_t ordinal=ledger_ordinal(large,s,i);CHECK(workbook_codec_header_source_version_at(*out,s,i,&v)&&v.version==n->version&&v.owner==n->owner&&v.kind==n->kind&&!strcmp(v.reference,ledger_basename(n->kind)));
            CHECK(v.reference!=n->path&&v.locator&&v.locator_resolved==!unresolved&&(unresolved?!*v.locator:!strcmp(v.locator,n->path))&&v.raw.bytes==n->raw.bytes);
            if(n->raw.bytes){SourcePacket expected=ledger_packet(n->owner,n->version,n->kind,n->path);CHECK(v.raw.data&&v.raw.data!=n->raw.data&&!memcmp(v.raw.data,n->raw.data,n->raw.bytes)&&expected.bytes==v.raw.bytes&&!memcmp(expected.data,v.raw.data,v.raw.bytes));SourceScopeInfo info;CHECK(source_scope_validate(v.raw.data,v.raw.bytes,&info,&error)&&info.kind==SOURCE_SCOPE_RAW&&info.owner==v.owner&&info.version==v.version&&info.source_kind==v.kind&&!strcmp(info.engine_version,duckdb_library_version())&&info.file_bytes==LEDGER_BASE+50);source_packet_free(&expected);}else CHECK(!v.raw.data&&ordinal%3==0);
        }
        for(size_t j=0;j<w->objects.count;j++)if(w->objects.items[j].kind==OBJECT_TABLE)CHECK(!strcmp(w->objects.items[j].source.query,"SELECT * FROM __BP1_SOURCE_VERSIONS_MUST_NOT_EXECUTE")&&!w->objects.items[j].source.source_version);
    }
    WorkbookCodecSourceVersionView untouched;memset(&untouched,0x5a,sizeof(untouched));unsigned char bytes[sizeof(untouched)];memcpy(bytes,&untouched,sizeof(bytes));CHECK(!workbook_codec_header_source_version_at(*out,64,0,&untouched)&&!memcmp(bytes,&untouched,sizeof(bytes))&&!workbook_codec_header_source_version_at(*out,0,9999,&untouched)&&!memcmp(bytes,&untouched,sizeof(bytes)));
    LedgerStamp stamp=ledger_stamp(*out);workbook_free(&native);ledger_kept(*out,stamp);FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);
}
static void ledger_rejected_limit(const char *path,unsigned version,bool present,size_t first,size_t limit,WorkbookCodecHeader **out) {
    FileImage before=file_image(path);Workbook native;workbook_init(&native);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&native,"Retained ledger",&blank,&error));unsigned char saved[sizeof(native)];memcpy(saved,&native,sizeof(native));
    CHECK(!workbook_load(&native,path,&error)&&error&&!memcmp(saved,&native,sizeof(native)));clear_error(&error);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);LedgerStamp old=ledger_stamp(*out);char *target=dx_absolute_path(path);WorkbookCodecRead *r=ledger_begin(c,version,target,old.prior.prior.objects.bytes,limit,resolve);
    if(version<14||!present){CHECK(!workbook_codec_read_add_source_versions(r,present,old.prior.prior.objects.bytes,&error)&&error&&!workbook_codec_read_request(r));}
    else{CHECK(workbook_codec_read_add_source_versions(r,true,old.prior.prior.objects.bytes,&error));ledger_request(r);CHECK(ledger_feed(c,r,NULL,256,old.prior.prior.objects.bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&workbook_codec_read_request(r)->first_row==first&&workbook_codec_read_owned_bytes(r)==ledger_last_owned);ledger_unfinished(r,out);}
    clear_error(&error);ledger_kept(*out,old);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(target);workbook_free(&native);
    FileImage after=file_image(path);CHECK(before.bytes==after.bytes&&!memcmp(before.data,after.data,before.bytes));free(before.data);free(after.data);
}
static void ledger_rejected(const char *path,unsigned version,bool present,size_t first,WorkbookCodecHeader **out) {ledger_rejected_limit(path,version,present,first,LEDGER_LIMIT,out);}
static void ledger_restore(const char *path,const char *change) {
    char *sql=dx_printf("DROP TABLE _duxcel_workspace.source_versions; CREATE TABLE _duxcel_workspace.source_versions AS SELECT %s FROM main.ledger_backup ORDER BY sheet_id,version",change?change:"*");mutate(path,sql);free(sql);
}
static void ledger_input_faults(const char *path,WorkbookCodecHeader **out) {
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);char *target=dx_absolute_path(path),*error=NULL;LedgerStamp old=ledger_stamp(*out);size_t retained=old.prior.prior.objects.bytes;
    WorkbookCodecRead *r=ledger_begin(c,14,target,retained,LEDGER_LIMIT,resolve);CHECK(workbook_codec_read_add_source_versions(r,true,retained,&error));size_t bytes=LEDGER_LIMIT-workbook_codec_read_owned_bytes(r)-retained;unsigned char *pressure=dx_alloc(bytes);memset(pressure,0,bytes);
    CHECK(ledger_feed(c,r,NULL,256,retained+bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&strstr(error,"budget")&&!workbook_codec_read_request(r)->first_row&&workbook_codec_read_owned_bytes(r)==ledger_last_owned);clear_error(&error);free(pressure);ledger_unfinished(r,out);ledger_kept(*out,old);
    CHECK(ledger_feed(c,r,NULL,256,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);CHECK(workbook_codec_read_finish(r,out,&error)&&!error);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(target);
}
static void ledger_bounds(const char *path,WorkbookCodecHeader **out) {
    /* Isolate count and archive/restored lower bounds with ample C admission;
       these are not a claim that a4MiB Worker input can carry their maxima. */
    mutate(path,"UPDATE _duxcel_workspace.source_version_counters SET high_water=18446744073709551615::UBIGINT");
    mutate(path,"DROP TABLE _duxcel_workspace.source_versions; CREATE TABLE _duxcel_workspace.source_versions AS SELECT 0::INTEGER AS sheet_id,(i+1)::UBIGINT AS version,0::UBIGINT AS owner,1::INTEGER AS source_kind,'missing-source.csv'::VARCHAR AS source_reference,0::INTEGER AS encoding,NULL::BLOB AS definition FROM range(1025) r(i)");
    ledger_rejected_limit(path,14,true,1024,32u*1024u*1024u,out);
    mutate(path,"DROP TABLE _duxcel_workspace.source_versions; CREATE TABLE _duxcel_workspace.source_versions AS SELECT 0::INTEGER AS sheet_id,(i+1)::UBIGINT AS version,0::UBIGINT AS owner,1::INTEGER AS source_kind,repeat('r',4096)::VARCHAR AS source_reference,0::INTEGER AS encoding,NULL::BLOB AS definition FROM range(257) r(i)");
    ledger_rejected_limit(path,14,true,0,32u*1024u*1024u,out);
    mutate(path,"DROP TABLE _duxcel_workspace.source_versions; CREATE TABLE _duxcel_workspace.source_versions AS SELECT s::INTEGER AS sheet_id,(i+1)::UBIGINT AS version,0::UBIGINT AS owner,1::INTEGER AS source_kind,repeat('r',4096)::VARCHAR AS source_reference,0::INTEGER AS encoding,NULL::BLOB AS definition FROM range(9) a(s),range(240) b(i) ORDER BY s,i");
    ledger_rejected_limit(path,14,true,1792,32u*1024u*1024u,out);
}
static bool ledger_bad_locator(void *context,SourceKind kind,const char *reference,size_t max,WorkbookCodecOwnedText *out) {
    (void)context;(void)kind;(void)reference;(void)max;out->data=dx_strdup("bad*locator");out->bytes=strlen(out->data);return false;
}
static bool ledger_glob_locator(void *context,SourceKind kind,const char *reference,size_t max,WorkbookCodecOwnedText *out) {
    (void)context;(void)kind;(void)reference;(void)max;out->data=dx_strdup("bad*locator");out->bytes=strlen(out->data);return true;
}
static void ledger_locator_faults(const char *path,WorkbookCodecHeader **out) {
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);char *target=dx_absolute_path(path),*error=NULL;LedgerStamp old=ledger_stamp(*out);size_t retained=old.prior.prior.objects.bytes;
    WorkbookCodecResolve callbacks[]={ledger_bad_locator,ledger_glob_locator};
    for(size_t i=0;i<2;i++){WorkbookCodecRead *r=ledger_begin(c,14,target,retained,LEDGER_LIMIT,callbacks[i]);CHECK(workbook_codec_read_add_source_versions(r,true,retained,&error));CHECK(ledger_feed(c,r,NULL,256,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&!workbook_codec_read_request(r)->first_row&&workbook_codec_read_owned_bytes(r)==ledger_last_owned);clear_error(&error);ledger_unfinished(r,out);ledger_kept(*out,old);workbook_codec_read_free(r);}
    duckdb_disconnect(&c);duckdb_close(&db);free(target);
}
static void ledger_save_as(const char *path,const char *directory) {
    Workbook a,b;workbook_init(&a);workbook_init(&b);char *error=NULL;CHECK(workbook_load(&a,path,&error));char *sub=dx_printf("%s/child",directory);CHECK(!mkdir(sub,0700));char *saved=dx_printf("%s/saved.dxls",sub);CHECK(workbook_save(&a,saved,&error)&&workbook_load(&b,saved,&error)&&!error);
    CHECK(a.count==b.count);size_t entries=0;
    for(size_t s=0;s<a.count;s++){CHECK(a.sheets[s].source_versions.count==b.sheets[s].source_versions.count);for(size_t i=0;i<a.sheets[s].source_versions.count;i++){SourceVersion *x=a.sheets[s].source_versions.items+i,*y=b.sheets[s].source_versions.items+i;CHECK(x->version==y->version&&x->owner==y->owner&&x->kind==y->kind&&x->raw.bytes==y->raw.bytes&&(!x->raw.bytes||!memcmp(x->raw.data,y->raw.data,x->raw.bytes)));CHECK(strstr(y->path,"child/../missing-source."));entries++;}}
    CHECK(entries==5);duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(saved,&db,&c);duckdb_result result={0};CHECK(duckdb_query(c,"SELECT source_reference FROM _duxcel_workspace.source_versions ORDER BY sheet_id,version",&result)==DuckDBSuccess&&duckdb_row_count(&result)==5);for(idx_t i=0;i<5;i++){duckdb_string value=duckdb_value_string(&result,0,i);CHECK(value.size==strlen("../missing-source.parquet")||value.size==strlen("../missing-source.csv"));CHECK(!memcmp(value.data,"../missing-source.",18));duckdb_free(value.data);}duckdb_destroy_result(&result);duckdb_disconnect(&c);duckdb_close(&db);workbook_free(&a);workbook_free(&b);CHECK(!unlink(saved)&&!rmdir(sub));free(saved);free(sub);
}
static int source_versions_only(void) {
    char temporary[]="/tmp/duxcel-codec-source-versions-XXXXXX";CHECK(mkdtemp(temporary));char *directory=dx_absolute_path(temporary),*missing=dx_printf("%s/missing-source.csv",directory);WorkbookCodecHeader *h=NULL;
    for(size_t i=0;i<4;i++){const EmitFixture *f=emit_source_versions_fixtures+i;char *path=dx_printf("%s/%s",directory,f->name);ledger_fixture_make(path,missing,f->version,i==3);if(f->reordered){mutate(path,reorder_sql);mutate(path,ledger_reorder_sql);}ledger_compare(path,f->version,i==3,false,&h);if(i==2){ledger_save_as(path,directory);ledger_compare(path,14,false,true,&h);}CHECK(!unlink(path));free(path);}
    char *path=dx_printf("%s/fault.dxls",directory);ledger_fixture_make(path,missing,14,true);ledger_input_faults(path,&h);ledger_locator_faults(path,&h);mutate(path,"CREATE TABLE main.ledger_backup AS SELECT * FROM _duxcel_workspace.source_versions");
    const char *faults[]={
        "sheet_id,version,owner,source_kind,CASE WHEN sheet_id=5 THEN NULL ELSE source_reference END AS source_reference,encoding,definition",
        "sheet_id,version,owner,source_kind,CASE WHEN sheet_id=5 THEN '' ELSE source_reference END AS source_reference,encoding,definition",
        "sheet_id,version,owner,source_kind,CASE WHEN sheet_id=5 THEN 'a'||chr(0)||'b' ELSE source_reference END AS source_reference,encoding,definition",
        "sheet_id,version,owner,source_kind,CASE WHEN sheet_id=5 THEN repeat('r',65537) ELSE source_reference END AS source_reference,encoding,definition",
        "sheet_id,CASE WHEN sheet_id=5 THEN 0::UBIGINT ELSE version END AS version,owner,source_kind,source_reference,encoding,definition",
        "sheet_id,version,CASE WHEN sheet_id=5 THEN 9007199254741024::UBIGINT ELSE owner END AS owner,source_kind,source_reference,encoding,definition",
                "sheet_id,version,owner,CASE WHEN sheet_id=5 THEN 3 ELSE source_kind END AS source_kind,source_reference,encoding,definition",
        "sheet_id,version,owner,source_kind,source_reference,CASE WHEN sheet_id=5 THEN 0 ELSE encoding END AS encoding,definition",
        "sheet_id,version,owner,source_kind,source_reference,encoding,CASE WHEN sheet_id=5 THEN NULL ELSE definition END AS definition",
        "sheet_id,version,owner,source_kind,source_reference,encoding,CASE WHEN sheet_id=5 THEN ''::BLOB ELSE definition END AS definition",
        "sheet_id,version,owner,source_kind,source_reference,encoding,CASE WHEN sheet_id=5 THEN repeat('x',31)::BLOB ELSE definition END AS definition",
        "sheet_id,version,owner,source_kind,source_reference,encoding,CASE WHEN sheet_id=5 THEN repeat('x',262144)::BLOB ELSE definition END AS definition",
        "sheet_id,version,owner,CASE WHEN sheet_id=5 THEN 2 ELSE source_kind END AS source_kind,source_reference,encoding,definition",
        "sheet_id,version,owner,source_kind,source_reference,encoding,CASE WHEN sheet_id=5 THEN unhex(substr(hex(definition),1,12)||'02'||substr(hex(definition),15)) ELSE definition END AS definition",
        "sheet_id,version,owner,source_kind,source_reference,encoding,CASE WHEN sheet_id=5 THEN unhex(substr(hex(definition),1,32)||'0000000000000000'||substr(hex(definition),49)) ELSE definition END AS definition",
        "sheet_id,version,owner,source_kind,source_reference,encoding,CASE WHEN sheet_id=5 THEN unhex(substr(hex(definition),1,48)||'0100000000000000'||substr(hex(definition),65)) ELSE definition END AS definition",
        "sheet_id,version,owner,source_kind,source_reference,encoding,CAST(definition AS VARCHAR) AS definition"
    };
    for(size_t i=0;i<sizeof(faults)/sizeof(*faults);i++){ledger_restore(path,faults[i]);ledger_rejected(path,14,true,i+1==sizeof(faults)/sizeof(*faults)?0:256,&h);}
    ledger_restore(path,NULL);ledger_compare(path,14,true,false,&h);
    mutate(path,"UPDATE _duxcel_workspace.source_version_counters SET high_water=18446744073709551615::UBIGINT WHERE sheet_id=3");
    ledger_restore(path,"CASE WHEN sheet_id=5 THEN 3 ELSE sheet_id END AS sheet_id,version,CASE WHEN sheet_id=5 THEN 1::UBIGINT ELSE owner END AS owner,source_kind,source_reference,encoding,definition");ledger_rejected(path,14,true,256,&h);
    ledger_restore(path,"sheet_id,CASE WHEN sheet_id=0 AND version=257 THEN 256::UBIGINT ELSE version END AS version,owner,source_kind,source_reference,encoding,definition");ledger_rejected(path,14,true,256,&h);
    ledger_restore(path,NULL);ledger_bounds(path,&h);ledger_restore(path,NULL);
    mutate(path,"DROP TABLE _duxcel_workspace.source_versions");ledger_rejected(path,14,false,0,&h);CHECK(!unlink(path));free(path);
    for(unsigned v=12;v<=13;v++){path=dx_printf("%s/present%u.dxls",directory,v);ledger_fixture_make(path,missing,v,false);mutate(path,"CREATE TABLE _duxcel_workspace.source_versions(sheet_id INTEGER,version UBIGINT,owner UBIGINT,source_kind INTEGER,source_reference VARCHAR,encoding INTEGER,definition BLOB)");ledger_rejected(path,v,true,0,&h);CHECK(!unlink(path));free(path);}
    workbook_codec_header_free(h);CHECK(access(missing,F_OK)!=0);char *parquet=dx_printf("%s/missing-source.parquet",directory);CHECK(access(parquet,F_OK)!=0);free(parquet);free(missing);CHECK(!rmdir(directory));free(directory);
    printf("PASS: native source versions %u checks; %u metadata executions/%u typed feeds; known writer12/13 absent and14 exact pending/RAW ledger,64 sheets/56 descriptors/285 rows split256+29, retired/MAX/live Table owners, BLOB SDK byte ownership, independent rebased locator/RAW SaveAs, atomic late/null/type/budget/stale/retry; largest owned SDK copied batch %zu bytes, materialized engine result excluded; source SQL never executed, no file proof/activation/Open; engine %s\n",checks,executions,feeds,ledger_sdk_peak,duckdb_library_version());return 0;
}
#undef LEDGER_BASE
#undef LEDGER_LIMIT
