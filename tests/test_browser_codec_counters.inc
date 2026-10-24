/* Allocation-counter metadata only. One original result per armed family;
   normal DXP1 transports two columns, and descriptors retain their explicit
   object envelope. No ObjectStore, issuer or Workbook is published here. */
typedef struct {
    unsigned object_queries,counter_queries,objects56,counters32,groups,poisoned,rejected_feeds,proofs,rejected_first[4];
    unsigned stale_callbacks;
    size_t maximum_header,pending_owned,stale_wire,stale_total;
    uint64_t prior_identity,detached_identity;
    const char *pending_sql;
    bool held_released,retired_exact,maximum_exact;
} CountersState;
static CountersState counters;
static size_t counters_retained(void) {return sizeof(counters);}
static bool counters_observe(size_t extra) {
    size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
    CHECK(current<=INPUT_LIMIT&&extra<=INPUT_LIMIT-current,"Concrete counter C buffers exceed input admission");
    if(current+extra>d.peak_input)d.peak_input=current+extra;return true;
}
static bool counters_stale(size_t size) {
    CHECK(d.phase==1210&&d.fault==5&&counters.held_released&&!d.ready,"Unexpected stale counter reservation path");
    CHECK(size<=DX_PAGE_WIRE_BYTES&&size<=SIZE_MAX-21u,"Stale counter wire reservation is oversized");
    /* Numeric sizes only: never dereference borrowed wire or total after the
       cookie/ticket mismatch. Twenty decimal digits plus NUL bound the total. */
    CHECK(counters_observe(size+21u),"Stale counter borrowed-buffer reservation exceeds input admission");
    counters.stale_callbacks++;counters.stale_wire=size;counters.stale_total=21;return true;
}
EM_JS(void, report_counters,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned queries,unsigned rejections,unsigned stale,size_t peak,size_t header_peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned objects,unsigned counter_queries,unsigned describe,unsigned object_feeds,unsigned counter_feeds,unsigned groups,unsigned poisoned,unsigned rejected,unsigned proofs,const unsigned *first,unsigned stale_callbacks,size_t stale_wire,size_t stale_total,int retired,int maximum,const char *error),{
    const read=(p,n)=>Array.from(HEAPU32.subarray(p>>>2,(p>>>2)+n));
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'counters',checks,fixtures,headers,queries,rejections,stale,
        peakOwnedInput:peak,maximumCompletedHeaderOwned:header_peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:read(normal,2),familyExecutions:{meta,sheets,objects,objectCounters:counter_queries,describe},
        acceptedFeeds:{objects56:object_feeds,counters32:counter_feeds},validCounterGroups:groups,poisonedCounterResults:poisoned,
        rejectedFeeds:rejected,rejectedFirstRows:read(first,4),lowHighWaterRejected:Boolean(proofs&1),requiredNullRejected:Boolean(proofs&2),identityRejected:Boolean(proofs&4),budgetRejected:Boolean(proofs&8),stalePreserved:Boolean(proofs&16),freshRecovery:Boolean(proofs&32),
        exactRetiredCounter:Boolean(retired),exactMaximumCounter:Boolean(maximum),staleCallbacks:stale_callbacks,staleWireReservationBytes:stale_wire,staleTotalReservationBytes:stale_total,
        error:error?UTF8ToString(error):null,boundary:'official-worker/DXP1/shared-C-meta-sheets-descriptors-object-counters Header; no source SQL, ObjectStore adoption, ID issuance, source-version counters/ledger, Workbook publication or browser Save/Open'});
})
static void counters_report(bool ok,const char *error) {
    report_counters(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.peak_input,counters.maximum_header,d.normal_queries,d.meta_queries,d.sheets_queries,counters.object_queries,counters.counter_queries,d.describe_queries,counters.objects56,counters.counters32,counters.groups,counters.poisoned,counters.rejected_feeds,counters.proofs,counters.rejected_first,counters.stale_callbacks,counters.stale_wire,counters.stale_total,counters.retired_exact,counters.maximum_exact,error);
}
static size_t counters_expected_count(size_t sheet) {size_t role=sheet%8;return role<3?0:role==6?3:1;}
static uint64_t counters_expected_id(size_t sheet,size_t item) {size_t role=sheet%8;return role==3?1:role==7?UINT64_MAX:UINT64_C(9007199254740993)+item;}
static uint64_t counters_expected_high_water(unsigned version,size_t sheet) {
    size_t role=sheet%8,count=counters_expected_count(sheet);uint64_t live=count?counters_expected_id(sheet,count-1):0;
    return role==1||role==5?UINT64_C(9007199254741023):role==2?UINT64_MAX:live;
}
static bool counters_header_contents(const WorkbookCodecHeader *h,unsigned version,const WorkbookCodecKey *expected) {
    const WorkbookCodecKey *key=workbook_codec_header_key(h);
    CHECK(h&&workbook_codec_header_objects_ready(h)&&workbook_codec_header_object_counters_ready(h)&&workbook_codec_header_sheet_count(h)==64&&workbook_codec_header_active_sheet(h)==63,"Counter Header readiness/count/active changed");
    CHECK(workbook_codec_header_version(h)==version&&workbook_codec_header_user_scale(h)==1.25f&&key&&key->document_epoch==expected->document_epoch&&key->operation_ticket==expected->operation_ticket&&key->snapshot_generation==expected->snapshot_generation,"Counter Header version/scale/captured key changed");
    CHECK(!workbook_codec_header_queries_ready(h)&&!workbook_codec_header_source_options_ready(h)&&!workbook_codec_header_table_sources_ready(h)&&!workbook_codec_header_pivot_marks_ready(h)&&!workbook_codec_header_filters_ready(h)&&!workbook_codec_header_sorts_ready(h)&&!workbook_codec_header_preferences_ready(h)&&!workbook_codec_header_columns_ready(h)&&!workbook_codec_header_cells_ready(h),"Counter Header fabricated unrequested metadata families");
    size_t total=0;
    for(size_t sheet=0;sheet<64;sheet++) {
        char name[96];snprintf(name,sizeof(name),"Counters 'İ' %zu",sheet);const Analysis *a=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);uint64_t value=0;
        CHECK(same(workbook_codec_header_name(h,sheet),name)&&a&&a->kind==SOURCE_CSV&&a->path&&!*a->path&&same(workbook_codec_header_reference(h,sheet),"missing-source.csv")&&same(a->schema,"")&&same(a->table,"NULL"),"Counter raw reference/UTF-8/empty/literal or unavailable browser locator changed");
        CHECK(!a->query&&!a->source_version&&!a->unique_rows&&!a->filter_column&&!a->filter_value&&!a->filter_count&&!a->group_column&&!a->measure_column&&!a->sort_column&&!a->descending&&!a->sort_count,"Counter workflow loaded/executed unrelated source definitions");
        CHECK(v&&v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==1.5f&&!v->columns&&!v->widths&&!v->column_count,"Counter worksheet viewport changed");
        CHECK(workbook_codec_header_object_high_water(h,sheet,&value)&&value==counters_expected_high_water(version,sheet),"Exact max-live/retired/exhausted high-water changed");
        if(sheet%8==1&&value==UINT64_C(9007199254741023))counters.retired_exact=true;
        if(sheet%8==2&&value==UINT64_MAX)counters.maximum_exact=true;
        size_t count=counters_expected_count(sheet);total+=count;CHECK(workbook_codec_header_object_count(h,sheet)==count,"Counter object descriptor count changed");
        for(size_t item=0;item<count;item++) {
            WorkbookCodecObjectView o={0};CHECK(workbook_codec_header_object_at(h,sheet,item,&o),"Counter descriptor missing");
            size_t role=sheet%8;ObjectKind kind=role==3?OBJECT_PIVOT:role==4?OBJECT_CHART:role==5?OBJECT_TABLE:(ObjectKind)((sheet/8+item)%3);
            if(item)snprintf(name,sizeof(name),"Owner 'İ' %zu:%zu",sheet,item);else snprintf(name,sizeof(name),"NULL");
            CHECK(o.id==counters_expected_id(sheet,item)&&o.kind==kind&&same(o.name,name)&&o.mark==(kind==OBJECT_CHART?MARK_LINE:MARK_BAR)&&!o.source_id&&!o.chart_measure&&o.row==10+20*item&&o.column==8&&o.height==8&&o.width==4,"Exact counter descriptor ID/kind/name/geometry changed");
        }
    }
    CHECK(total==56,"Counter fixture descriptor inventory changed");uint64_t unchanged=UINT64_C(9007199254740997);
    CHECK(!workbook_codec_header_object_high_water(h,64,&unchanged)&&unchanged==UINT64_C(9007199254740997)&&!workbook_codec_header_object_high_water(NULL,0,&unchanged)&&unchanged==UINT64_C(9007199254740997)&&!workbook_codec_header_object_high_water(h,0,NULL),"Invalid counter getter changed output");return true;
}
static uint64_t counters_hash_bytes(uint64_t hash,const void *data,size_t size) {const unsigned char *bytes=data;for(size_t n=0;n<size;n++)hash=(hash^bytes[n])*UINT64_C(1099511628211);return hash;}
static uint64_t counters_hash_word(uint64_t hash,uint64_t value) {return counters_hash_bytes(hash,&value,sizeof(value));}
static uint64_t counters_hash_text(uint64_t hash,const char *text) {hash=counters_hash_word(hash,(uintptr_t)text);return text?counters_hash_bytes(hash,text,strlen(text)+1):hash;}
static uint64_t counters_header_identity(const WorkbookCodecHeader *h) {
    uint64_t hash=UINT64_C(1469598103934665603);if(!h)return hash;
    const WorkbookCodecKey *key=workbook_codec_header_key(h);if(!key)return 0;hash=counters_hash_bytes(hash,key,sizeof(*key));
    hash=counters_hash_word(hash,workbook_codec_header_version(h));hash=counters_hash_word(hash,workbook_codec_header_sheet_count(h));hash=counters_hash_word(hash,workbook_codec_header_active_sheet(h));
    float scale=workbook_codec_header_user_scale(h);hash=counters_hash_bytes(hash,&scale,sizeof(scale));
    const uint64_t ready[]={workbook_codec_header_objects_ready(h),workbook_codec_header_object_counters_ready(h),workbook_codec_header_queries_ready(h),workbook_codec_header_source_options_ready(h),workbook_codec_header_table_sources_ready(h),workbook_codec_header_pivot_marks_ready(h),workbook_codec_header_filters_ready(h),workbook_codec_header_sorts_ready(h),workbook_codec_header_preferences_ready(h),workbook_codec_header_columns_ready(h),workbook_codec_header_cells_ready(h)};hash=counters_hash_bytes(hash,ready,sizeof(ready));
    for(size_t sheet=0;sheet<workbook_codec_header_sheet_count(h);sheet++) {
        const Analysis *a=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);uint64_t high=0;if(!a||!v||!workbook_codec_header_object_high_water(h,sheet,&high))return 0;
        hash=counters_hash_word(hash,(uintptr_t)a);hash=counters_hash_bytes(hash,a,sizeof(*a));hash=counters_hash_word(hash,(uintptr_t)v);hash=counters_hash_bytes(hash,v,sizeof(*v));hash=counters_hash_word(hash,high);
        hash=counters_hash_text(hash,workbook_codec_header_name(h,sheet));hash=counters_hash_text(hash,workbook_codec_header_reference(h,sheet));hash=counters_hash_text(hash,a->path);hash=counters_hash_text(hash,a->schema);hash=counters_hash_text(hash,a->table);
        size_t count=workbook_codec_header_object_count(h,sheet);hash=counters_hash_word(hash,count);
        for(size_t item=0;item<count;item++) {WorkbookCodecObjectView o={0};if(!workbook_codec_header_object_at(h,sheet,item,&o))return 0;
            const uint64_t words[]={o.id,o.row,o.height,o.source_id,o.column,o.width,o.kind,o.mark,o.chart_measure};hash=counters_hash_bytes(hash,words,sizeof(words));hash=counters_hash_text(hash,o.name);}
    }
    return hash;
}
static bool counters_prior_unchanged(void) {
    CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes&&counters_header_identity(d.header)==counters.prior_identity,"Independent prior counter Header/string/descriptor/POD ownership changed");return true;
}
static bool counters_begin(void) {counters.prior_identity=counters_header_identity(d.header);return begin_read();}
static bool counters_send(char *sql,const Page *schema,unsigned limit,bool hold,bool object_policy) {sorts_wire=false;objects_wire=object_policy;return send_sql(sql,schema,limit,hold);}
static bool counters_send_family(WorkbookCodecFamily family,bool hold) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==family&&!request->first_row,"Unexpected/reexecuted counter workflow family");
    if(family==WORKBOOK_CODEC_META||family==WORKBOOK_CODEC_SHEETS) {bool meta=family==WORKBOOK_CODEC_META;if(meta)d.meta_queries++;else d.sheets_queries++;const Page *schema=meta?&d.meta_schema:&d.sheets_schema;return counters_send(browser_page_sql(request->sql,schema,0,&d.error),schema,(unsigned)request->row_limit,false,false);}
    bool objects=family==WORKBOOK_CODEC_OBJECTS;CHECK(objects||family==WORKBOOK_CODEC_OBJECT_COUNTERS,"Unexpected counter workflow continuation");
    CHECK(request->column_count==(objects?11u:2u)&&request->source_column_count==request->column_count&&request->row_limit==(objects?2049u:65u),"Counter transport family envelope changed");
    char *relation=dx_strdup(request->sql),*changed=NULL;
    if(!objects) {
        CHECK(!strcmp(relation,"SELECT sheet_id,high_water FROM _duxcel_workspace.object_counters ORDER BY sheet_id LIMIT 65"),"Counter cursor named query changed");
        if(d.fault==1)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=63 THEN 18446744073709551614::UBIGINT ELSE high_water END AS high_water) FROM (%s) AS _counters_low ORDER BY sheet_id",relation);
        else if(d.fault==2)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=63 THEN NULL::UBIGINT ELSE high_water END AS high_water) FROM (%s) AS _counters_null ORDER BY sheet_id",relation);
        else if(d.fault==3)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=63 THEN 64::INTEGER ELSE sheet_id END AS sheet_id) FROM (%s) AS _counters_foreign ORDER BY sheet_id",relation);
    }
    if(changed){CHECK(counters_observe(text_bytes(relation)+text_bytes(changed)),"Counter fault SQL coexistence exceeds admission");free(relation);relation=changed;}
    char *sql=objects?dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(id AS VARCHAR) AS id,CAST(name AS VARCHAR) AS name,CAST(row_id AS VARCHAR) AS row_id,CAST(column_id AS VARCHAR) AS column_id,CAST(height AS VARCHAR) AS height,CAST(width AS VARCHAR) AS width,CAST(kind AS VARCHAR) AS kind,CAST(mark AS VARCHAR) AS mark,CAST(source_id AS VARCHAR) AS source_id,CAST(chart_measure AS VARCHAR) AS chart_measure FROM (%s) AS _counters_objects",relation):dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(high_water AS VARCHAR) AS high_water FROM (%s) AS _counters_high_water",relation);
    CHECK(sql&&strlen(sql)<=DX_BROWSER_SQL_BYTES&&counters_observe(text_bytes(relation)+text_bytes(sql)),d.error?d.error:"Counter projection SQL exceeds admission");free(relation);
    if(objects)counters.object_queries++;else counters.counter_queries++;return counters_send(sql,objects?&d.objects_schema:&d.counters_schema,objects?2049:65,hold,objects);
}
static bool counters_atoms(const WorkbookCodecTable *table,const Page *schema,size_t rows) {
    CHECK(d.page.column_count==schema->column_count&&d.page.row_count==rows&&!d.page.offset&&d.page.total==rows&&!d.page.count_pending&&!d.atoms,"Counter original result dimensions/completeness changed");
    size_t bytes=rows*schema->column_count*sizeof(*d.atoms);CHECK(counters_observe(bytes),"Counter atom staging exceeds input admission");d.atoms=dx_alloc(bytes);d.atom_bytes=bytes;const char *types=table->types;
    for(size_t col=0;col<schema->column_count;col++) {
        while(*types==' ')types++;WorkbookCodecAtomKind kind=atom_kind(*types++);const Column *actual=d.page.columns+col,*expected=schema->columns+col;
        CHECK(same(actual->name,expected->name)&&same(actual->type_name,expected->type_name)&&actual->type==expected->type&&actual->number_type.kind==expected->number_type.kind&&actual->number_type.precision==expected->number_type.precision&&actual->number_type.scale==expected->number_type.scale,"Counter original typed sidecar changed");d.columns[col]=(WorkbookCodecColumn){actual->name,kind};
        for(size_t row=0;row<rows;row++) {const Cell *cell=d.page.cells+row*schema->column_count+col;WorkbookCodecAtom *a=d.atoms+row*schema->column_count+col;a->kind=cell->is_null?WORKBOOK_CODEC_NULL:kind;if(cell->is_null)continue;CHECK(cell->text,"Counter exact atom text missing");
            if(kind==WORKBOOK_CODEC_I32)CHECK(i32(cell->text,&a->value.i32),"Counter INTEGER text is not exact");else if(kind==WORKBOOK_CODEC_U64)CHECK(u64(cell->text,&a->value.u64),"Counter UBIGINT text is not exact");else if(kind==WORKBOOK_CODEC_DOUBLE){char *end=NULL;a->value.real=strtod(cell->text,&end);CHECK(end&&end!=cell->text&&!*end,"Counter base DOUBLE is not exact");}else if(kind==WORKBOOK_CODEC_BOOLEAN){CHECK(!strcmp(cell->text,"true")||!strcmp(cell->text,"false"),"Counter base BOOLEAN is not exact");a->value.boolean=!strcmp(cell->text,"true");}else a->value.text=(WorkbookCodecText){cell->text,strlen(cell->text)};
        }
    }
    return true;
}
static bool counters_poison(size_t column) {for(size_t row=0;row<d.page.row_count;row++){Cell *cell=d.page.cells+row*d.page.column_count+column;if(!cell->is_null){CHECK(cell->text,"Counter input text missing before poison");memset(cell->text,'~',strlen(cell->text));}}return true;}
static bool counters_feed_base(WorkbookCodecFamily family) {
    bool meta=family==WORKBOOK_CODEC_META;CHECK(counters_atoms(workbook_codec_table_find(meta?"meta":"sheets"),meta?&d.meta_schema:&d.sheets_schema,meta?1:64),"Counter base atom adaptation failed");
    WorkbookCodecBatch batch={d.columns,d.page.column_count,d.atoms,d.page.row_count,0,true};CHECK(counters_observe(0),"Counter base feed exceeds admission");
    CHECK(workbook_codec_read_feed(d.read,d.key,family,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Counter base feed rejected");
    if(!meta)for(size_t col=1;col<=5;col++)if(col!=2)CHECK(counters_poison(col),"Owned counter sheet input poison failed");return true;
}
static bool counters_feed_objects(void) {
    CHECK(counters_atoms(workbook_codec_table_find("objects"),&d.objects_schema,56),"Counter descriptor atom adaptation failed");WorkbookCodecBatch batch={d.columns,11,d.atoms,56,0,true};CHECK(counters_observe(0),"Counter descriptor feed exceeds admission");
    CHECK(workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_OBJECTS,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Counter descriptor feed rejected");counters.objects56++;return counters_poison(2);
}
static bool counters_feed_result(void) {
    CHECK(counters_atoms(workbook_codec_table_find("object_counters"),&d.counters_schema,64),"Counter exact atom adaptation failed");
    if(d.fault==1)CHECK(d.atoms[126].value.i32==63&&d.atoms[127].kind==WORKBOOK_CODEC_U64&&d.atoms[127].value.u64==UINT64_MAX-1u,"Actual low high-water witness was rounded or omitted");
    if(d.fault==2)CHECK(d.atoms[126].value.i32==63&&d.atoms[127].kind==WORKBOOK_CODEC_NULL,"Actual required NULL counter did not survive the typed sidecar");
    if(d.fault==3)CHECK(d.atoms[126].kind==WORKBOOK_CODEC_I32&&d.atoms[126].value.i32==64&&d.atoms[127].value.u64==UINT64_MAX,"Actual foreign counter identity witness changed");
    if(d.fault==4) {size_t current=retained()+workbook_codec_read_owned_bytes(d.read);CHECK(current<HEADER_LIMIT&&HEADER_LIMIT-current<SIZE_MAX,"Counter pressure fixture lacks admission headroom");d.pressure_bytes=HEADER_LIMIT-current+1u;d.pressure=dx_alloc(d.pressure_bytes);memset(d.pressure,0,d.pressure_bytes);CHECK(counters_observe(0),"Concrete counter pressure exceeds4MiB input admission");}
    for(size_t first=0;first<64;first+=32) {
        const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_OBJECT_COUNTERS&&request->first_row==first,"Counter split feeds lost their single-result cursor");size_t owned=workbook_codec_read_owned_bytes(d.read);const char *sql=request->sql;
        WorkbookCodecBatch batch={d.columns,2,d.atoms+first*2,32,first,first+32==64};CHECK(counters_observe(0),"Counter feed coexistence exceeds input admission");WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,retained(),&d.error);
        if(d.fault>=1&&d.fault<=4&&first==(d.fault==4?0u:32u)) {
            const char *reason=d.fault==2?"SQL NULL":d.fault==4?"budget":"high-water";CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error&&strstr(d.error,reason),d.error?d.error:"Expected actual counter failure missing");
            CHECK(counters_observe(0),"Counter feed rejection diagnostic exceeds input admission");
            CHECK(workbook_codec_read_owned_bytes(d.read)==owned&&workbook_codec_read_request(d.read)->first_row==first&&workbook_codec_read_request(d.read)->sql==sql,"Rejected counter batch changed accepted scalar ownership/cursor");
            CHECK(counters_prior_unchanged(),"Rejected counter batch changed prior Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Rejected counter candidate replaced prior Header");CHECK(counters_observe(0),"Counter rejection diagnostic exceeds input admission");counters.rejected_first[d.fault-1]=(unsigned)first;counters.rejected_feeds++;counters.proofs|=1u<<(d.fault-1);d.rejections++;return true;
        }
        CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid counter prefix rejected");CHECK(workbook_codec_read_owned_bytes(d.read)==owned,"Counter scalar feed unexpectedly grew retained state");counters.counters32++;
    }
    CHECK(!d.fault||d.fault==6,"Rejected counter candidate completed unexpectedly");
    for(size_t row=0;row<64;row++){CHECK(d.atoms[row*2+1].kind==WORKBOOK_CODEC_U64,"Counter POD missing before poison");d.atoms[row*2+1].value.u64=0;}
    CHECK(counters_poison(0)&&counters_poison(1),"Owned counter Page poison failed");counters.poisoned++;counters.groups++;return true;
}
static void counters_free_schemas(void) {page_free(&d.meta_schema);page_free(&d.sheets_schema);page_free(&d.objects_schema);page_free(&d.counters_schema);}
static bool counters_restart(void) {clear_input();d.fault++;objects_wire=false;return counters_begin();}
static bool counters_complete(void) {
    CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete counter Header could not move");CHECK(counters_header_contents(d.header,counters_versions[d.fixture],&d.key),"Completed counter Header differs from the native fixture");d.headers++;size_t bytes=workbook_codec_header_owned_bytes(d.header);CHECK(bytes<=HEADER_LIMIT,"Completed counter Header exceeds owned admission");if(bytes>counters.maximum_header)counters.maximum_header=bytes;
    if(!d.fault){d.fixture++;if(d.fixture<2){counters_free_schemas();d.phase=0;return true;}d.fixture=1;d.fault=1;CHECK(counters_begin(),"Counter negative candidate begin failed");d.phase=1206;return true;}
    CHECK(d.fault==6&&d.fixtures==2&&d.headers==3&&d.queries==40&&d.rejections==4&&d.stale==1&&d.callbacks==39&&d.meta_queries==8&&d.sheets_queries==8&&counters.object_queries==8&&counters.counter_queries==8&&d.describe_queries==8,"Counter terminal execution/header/rejection inventory changed");
    CHECK(counters.objects56==8&&counters.counters32==9&&counters.groups==3&&counters.poisoned==3&&counters.rejected_feeds==4&&counters.proofs==63&&counters.stale_callbacks==1&&counters.stale_total==21&&counters.stale_wire&&counters.retired_exact&&counters.maximum_exact,"Counter feed/owned-POD/stale proof inventory changed");
    const unsigned first[]={32,32,32,0},normal[]={8,8};for(size_t n=0;n<4;n++)CHECK(counters.rejected_first[n]==first[n],"Counter failure prefix/normal workload changed");for(size_t n=0;n<2;n++)CHECK(d.normal_queries[n]==normal[n],"Counter failure prefix/normal workload changed");
    counters.detached_identity=counters_header_identity(d.header);d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
}
static bool counters_tick_body(void) {
    if(d.phase==0){CHECK(d.fixture<2,"Counter fixture index exceeds inventory");d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",counters_files[d.fixture]);fixture_start(d.cookie,d.ticket,counters_files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=1201;return true;}
    if(d.phase==1201){if(!d.fixture_ready)return true;d.phase=1202;return counters_send(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false,false);}
    if(d.waiting)return true;
    if(d.phase>=1202&&d.phase<=1205) {
        unsigned index=d.phase-1202;const char *names[]={"meta","sheets","objects","object_counters"};Page *destinations[]={&d.meta_schema,&d.sheets_schema,&d.objects_schema,&d.counters_schema};size_t counts[]={3,25,11,2};
        CHECK(d.ready&&!d.error,d.error?d.error:"Counter original DESCRIBE absent");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Counter original schema invalid");CHECK(counters_observe(schema.bytes),"Counter original schema coexistence exceeds admission");if(!source_schema(&schema,workbook_codec_table_find(names[index]),counts[index])){page_free(&schema);return false;}page_free(destinations[index]);*destinations[index]=schema;clear_input();
        unsigned last=3;if(index<last){d.phase++;return counters_send(dx_printf("DESCRIBE SELECT * FROM _duxcel_workspace.%s",names[index+1]),NULL,256,false,false);}
        for(unsigned n=0;n<=last;n++)CHECK(canonical_schema(destinations[n],workbook_codec_table_find(names[n]))&&destinations[n]->column_count==counts[n],"Counter canonical schema construction failed");CHECK(counters_begin(),"Counter Header begin failed");d.phase=1206;
    }
    if(d.phase==1206){d.phase=1207;return counters_send_family(WORKBOOK_CODEC_META,false);}
    if(d.phase==1207){CHECK(d.ready&&!d.error,d.error?d.error:"Counter meta result absent");CHECK(counters_feed_base(WORKBOOK_CODEC_META),"Counter meta feed failed");clear_input();d.phase=1208;return counters_send_family(WORKBOOK_CODEC_SHEETS,false);}
    if(d.phase==1208){CHECK(d.ready&&!d.error,d.error?d.error:"Counter sheets result absent");CHECK(counters_feed_base(WORKBOOK_CODEC_SHEETS),"Counter sheets feed failed");clear_input();CHECK(workbook_codec_read_add_objects(d.read,true,&d.error),d.error?d.error:"Counter descriptor continuation failed");d.phase=1209;return counters_send_family(WORKBOOK_CODEC_OBJECTS,false);}
    if(d.phase==1209){CHECK(d.ready&&!d.error,d.error?d.error:"Counter descriptor result absent");CHECK(counters_feed_objects(),"Counter descriptor feed failed");clear_input();CHECK(workbook_codec_read_add_object_counters(d.read,true,&d.error),d.error?d.error:"Counter continuation failed");

        counters.pending_owned=workbook_codec_read_owned_bytes(d.read);counters.pending_sql=workbook_codec_read_request(d.read)->sql;d.phase=1210;return counters_send_family(WORKBOOK_CODEC_OBJECT_COUNTERS,d.fault==5&&!counters.held_released);
    }
    if(d.phase==1210){
        if(d.fault==5&&!counters.held_released){if(!d.held_ready)return true;CHECK(!d.ready,"Held actual counter result entered C before release");d.cookie++;cancel_candidate();d.waiting=false;counters.held_released=true;unsigned stale=d.stale;release_held();CHECK(d.stale==stale+1&&!d.ready&&counters_prior_unchanged(),"Stale counter callback decoded/published borrowed buffers");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_OBJECT_COUNTERS&&!request->first_row&&request->sql==counters.pending_sql&&workbook_codec_read_owned_bytes(d.read)==counters.pending_owned,"Stale counter callback changed pending scalar ownership");counters.proofs|=16;CHECK(counters_restart(),"Whole counter candidate restart after stale callback failed");d.phase=1206;return true;}
        CHECK(d.ready&&!d.error,d.error?d.error:"Counter result absent");CHECK(counters_feed_result(),"Counter feed failed");if(d.fault>=1&&d.fault<=4){CHECK(counters_restart(),"Rejected counter candidate restart failed");d.phase=1206;return true;}clear_input();if(d.fault==6)counters.proofs|=32;return counters_complete();
    }
    if(d.phase==9&&d.fixture_ready){clear_input();CHECK(counters_header_identity(d.header)==counters.detached_identity&&counters_header_contents(d.header,counters_versions[1],&d.key),"Counter Header ownership changed after catalog close");counters_free_schemas();workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;d.done=true;printf("PASS: browser-codec-object-counters %u checks,2 current native archives,3 Headers,40 queries,4 codec rejections,1 stale callback; no Workbook publication\n",d.checks);counters_report(true,NULL);emscripten_cancel_main_loop();return true;}
    return true;
}
static void counters_tick(void) {if(d.phase==1210&&d.fault==5&&d.held_ready&&!counters.held_released)d.waiting=false;counters_tick_body();}
