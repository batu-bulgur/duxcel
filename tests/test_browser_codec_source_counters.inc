/* Source-version high-waters only; deliberately independent of object IDs.
   Reuses the earlier counter fixture/atom helpers, counting their concrete
   static state in retained ownership. No ledger or source is activated. */
typedef struct {
    unsigned source_queries,source32,source_groups,source_defaults,source_poisoned,rejected_feeds,proofs,rejected_first[4],stale_callbacks;
    size_t maximum_header,pending_owned,stale_wire,stale_total;
    uint64_t prior_identity,detached_identity;
    const char *pending_sql;
    bool held_released,maximum_exact,source_above_objects,source_below_objects;
} SourceCountersState;
static SourceCountersState source_counters;
static size_t source_counters_retained(void) {return sizeof(source_counters)+sizeof(counters);}
static bool source_counters_observe(size_t extra) {
    size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
    CHECK(current<=INPUT_LIMIT&&extra<=INPUT_LIMIT-current,"Concrete source-counter C buffers exceed input admission");
    if(current+extra>d.peak_input)d.peak_input=current+extra;return true;
}
static bool source_counters_stale(size_t size) {
    CHECK(d.phase==1312&&d.fault==5&&source_counters.held_released&&!d.ready,"Unexpected stale source-counter reservation path");
    CHECK(size<=DX_PAGE_WIRE_BYTES&&size<=SIZE_MAX-21u,"Stale source-counter wire reservation is oversized");
    CHECK(source_counters_observe(size+21u),"Stale source-counter wire/scalar reservation exceeds admission");
    source_counters.stale_callbacks++;source_counters.stale_wire=size;source_counters.stale_total=21;return true;
}
EM_JS(void, report_source_counters,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned queries,unsigned rejections,unsigned stale,unsigned callbacks,size_t peak,size_t header_peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned objects,unsigned object_queries,unsigned source_queries,unsigned describe,unsigned object_feeds,unsigned counter_feeds,unsigned source_feeds,unsigned object_groups,unsigned source_groups,unsigned object_defaults,unsigned source_defaults,unsigned object_poisoned,unsigned source_poisoned,unsigned rejected,unsigned proofs,const unsigned *first,unsigned stale_callbacks,size_t stale_wire,size_t stale_total,int maximum,int independent,const char *error),{
    const read=(p,n)=>Array.from(HEAPU32.subarray(p>>>2,(p>>>2)+n));
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'source-counters',checks,fixtures,headers,queries,rejections,stale,callbacks,
        peakOwnedInput:peak,maximumCompletedHeaderOwned:header_peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:read(normal,4),familyExecutions:{meta,sheets,objects,objectCounters:object_queries,sourceCounters:source_queries,describe},
        acceptedFeeds:{objects56:object_feeds,objectCounters32:counter_feeds,sourceCounters32:source_feeds},validObjectCounterGroups:object_groups,validSourceCounterGroups:source_groups,
        objectCounterDefaults:object_defaults,sourceCounterDefaults:source_defaults,poisonedObjectCounterResults:object_poisoned,poisonedSourceCounterResults:source_poisoned,
        rejectedFeeds:rejected,rejectedFirstRows:read(first,4),requiredNullRejected:Boolean(proofs&1),identityRejected:Boolean(proofs&2),missingRowRejected:Boolean(proofs&4),budgetRejected:Boolean(proofs&8),stalePreserved:Boolean(proofs&16),freshRecovery:Boolean(proofs&32),olderNoSourceCounterQuery:Boolean(proofs&64),
        exactSourceMaximum:Boolean(maximum),independentDomains:Boolean(independent),staleCallbacks:stale_callbacks,staleWireReservationBytes:stale_wire,staleTotalReservationBytes:stale_total,
        error:error?UTF8ToString(error):null,boundary:'official-worker/DXP1/shared-C-meta-sheets-descriptors-object-and-source-counter Header; no source SQL, source-version ledger/bindings, issuance, source activation, Workbook publication or browser Save/Open'});
})
static void source_counters_report(bool ok,const char *error) {
    report_source_counters(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.callbacks,d.peak_input,source_counters.maximum_header,d.normal_queries,d.meta_queries,d.sheets_queries,counters.object_queries,counters.counter_queries,source_counters.source_queries,d.describe_queries,counters.objects56,counters.counters32,source_counters.source32,counters.groups,source_counters.source_groups,counters.defaults,source_counters.source_defaults,counters.poisoned,source_counters.source_poisoned,source_counters.rejected_feeds,source_counters.proofs,source_counters.rejected_first,source_counters.stale_callbacks,source_counters.stale_wire,source_counters.stale_total,source_counters.maximum_exact,source_counters.source_above_objects&&source_counters.source_below_objects,error);
}
static uint64_t source_counters_expected(unsigned version,size_t sheet) {
    return version<14?0:sheet%4==0?0:sheet%4==1?1:sheet%4==2?UINT64_C(9007199254741043):UINT64_MAX;
}
static bool source_counters_header_contents(const WorkbookCodecHeader *h,unsigned version,const WorkbookCodecKey *key) {
    CHECK(counters_header_contents(h,version,key),"Source-counter base Header differs from frozen parent fixture");
    CHECK(workbook_codec_header_source_version_counters_ready(h),"Source-version counter family did not complete");
    for(size_t sheet=0;sheet<64;sheet++) {
        uint64_t value=0,object_value=0;CHECK(workbook_codec_header_source_version_high_water(h,sheet,&value)&&value==source_counters_expected(version,sheet),"Exact independent source-version high-water changed");
        CHECK(workbook_codec_header_object_high_water(h,sheet,&object_value)&&object_value==counters_expected_high_water(version,sheet),"Source counters changed object allocation state");
        if(version==14&&value==UINT64_MAX)source_counters.maximum_exact=true;
        if(version==14&&sheet==3&&value==UINT64_MAX&&object_value==1)source_counters.source_above_objects=true;
        if(version==14&&sheet==4&&value==0&&object_value>0)source_counters.source_below_objects=true;
    }
    uint64_t saved=UINT64_C(9007199254740999);
    CHECK(!workbook_codec_header_source_version_high_water(h,64,&saved)&&saved==UINT64_C(9007199254740999)&&!workbook_codec_header_source_version_high_water(NULL,0,&saved)&&saved==UINT64_C(9007199254740999)&&!workbook_codec_header_source_version_high_water(h,0,NULL),"Invalid source-counter getter changed output");return true;
}
static uint64_t source_counters_header_identity(const WorkbookCodecHeader *h) {
    uint64_t hash=counters_header_identity(h);bool ready=workbook_codec_header_source_version_counters_ready(h);
    hash=counters_hash_word(hash,ready);
    if(ready)for(size_t sheet=0;sheet<workbook_codec_header_sheet_count(h);sheet++){uint64_t value=0;if(!workbook_codec_header_source_version_high_water(h,sheet,&value))return 0;hash=counters_hash_word(hash,value);}
    return hash;
}
static bool source_counters_prior_unchanged(void) {
    CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes&&source_counters_header_identity(d.header)==source_counters.prior_identity,"Independent prior source-counter Header/pointers/scalars changed");return true;
}
static bool source_counters_begin(void) {source_counters.prior_identity=source_counters_header_identity(d.header);return begin_read();}
static bool source_counters_send(char *sql,const Page *schema,unsigned limit,bool hold,bool object_policy) {
    sorts_wire=false;queries_wire=false;options_wire=false;tables_wire=0;pm_wire=0;filters_wire=false;objects_wire=object_policy;return send_sql(sql,schema,limit,hold);
}
static bool source_counters_send_family(WorkbookCodecFamily family,bool hold) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==family&&!request->first_row,"Unexpected/reexecuted source-counter family");
    if(family==WORKBOOK_CODEC_META||family==WORKBOOK_CODEC_SHEETS){bool meta=family==WORKBOOK_CODEC_META;if(meta)d.meta_queries++;else d.sheets_queries++;const Page *schema=meta?&d.meta_schema:&d.sheets_schema;return source_counters_send(browser_page_sql(request->sql,schema,0,&d.error),schema,(unsigned)request->row_limit,false,false);}
    bool objects=family==WORKBOOK_CODEC_OBJECTS,source=family==WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS;
    CHECK(objects||source||family==WORKBOOK_CODEC_OBJECT_COUNTERS,"Unexpected source-counter continuation");
    CHECK(request->column_count==(objects?11u:2u)&&request->source_column_count==request->column_count&&request->row_limit==(objects?2049u:65u),"Source-counter transport envelope changed");
    char *relation=dx_strdup(request->sql),*changed=NULL;
    if(!objects)CHECK(!strcmp(relation,source?"SELECT sheet_id,high_water FROM _duxcel_workspace.source_version_counters ORDER BY sheet_id LIMIT 65":"SELECT sheet_id,high_water FROM _duxcel_workspace.object_counters ORDER BY sheet_id LIMIT 65"),"Counter canonical named query changed");
    if(source){
        if(d.fault==1)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=63 THEN NULL::UBIGINT ELSE high_water END AS high_water) FROM (%s) AS _source_null ORDER BY sheet_id",relation);
        else if(d.fault==2)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=63 THEN 64::INTEGER ELSE sheet_id END AS sheet_id) FROM (%s) AS _source_foreign ORDER BY sheet_id",relation);
        else if(d.fault==3)changed=dx_printf("SELECT * FROM (%s) AS _source_missing WHERE sheet_id<>63 ORDER BY sheet_id",relation);
    }
    if(changed){CHECK(source_counters_observe(text_bytes(relation)+text_bytes(changed)),"Source-counter fault SQL coexistence exceeds admission");free(relation);relation=changed;}
    char *sql=objects?dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(id AS VARCHAR) AS id,CAST(name AS VARCHAR) AS name,CAST(row_id AS VARCHAR) AS row_id,CAST(column_id AS VARCHAR) AS column_id,CAST(height AS VARCHAR) AS height,CAST(width AS VARCHAR) AS width,CAST(kind AS VARCHAR) AS kind,CAST(mark AS VARCHAR) AS mark,CAST(source_id AS VARCHAR) AS source_id,CAST(chart_measure AS VARCHAR) AS chart_measure FROM (%s) AS _source_objects",relation):dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(high_water AS VARCHAR) AS high_water FROM (%s) AS _source_high_water",relation);
    CHECK(sql&&strlen(sql)<=DX_BROWSER_SQL_BYTES&&source_counters_observe(text_bytes(relation)+text_bytes(sql)),d.error?d.error:"Source-counter projection exceeds admission");free(relation);
    if(objects)counters.object_queries++;else if(source)source_counters.source_queries++;else counters.counter_queries++;
    return source_counters_send(sql,objects?&d.objects_schema:source?&d.source_counters_schema:&d.counters_schema,objects?2049:65,hold,objects);
}
static bool source_counters_feed_object_counters(void) {
    CHECK(counters_atoms(workbook_codec_table_find("object_counters"),&d.counters_schema,64),"Object-counter typed adaptation failed");
    for(size_t first=0;first<64;first+=32){const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_OBJECT_COUNTERS&&request->first_row==first,"Object-counter split reexecuted/misordered its result");size_t bytes=workbook_codec_read_owned_bytes(d.read);WorkbookCodecBatch batch={d.columns,2,d.atoms+first*2,32,first,first==32};CHECK(source_counters_observe(0),"Object-counter feed exceeds admission");CHECK(workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_OBJECT_COUNTERS,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid object counter rejected");CHECK(workbook_codec_read_owned_bytes(d.read)==bytes,"Object-counter POD grew owned storage");counters.counters32++;}
    for(size_t row=0;row<64;row++)d.atoms[row*2+1].value.u64=0;
    CHECK(counters_poison(0)&&counters_poison(1),"Object-counter input poison failed");counters.poisoned++;counters.groups++;return true;
}
static bool source_counters_feed_result(void) {
    size_t rows=d.fault==3?63:64;
    CHECK(counters_atoms(workbook_codec_table_find("source_version_counters"),&d.source_counters_schema,rows),"Source-counter typed result adaptation failed");
    if(d.fault==1)CHECK(d.atoms[126].value.i32==63&&d.atoms[127].kind==WORKBOOK_CODEC_NULL,"Actual required source-counter NULL was lost");
    if(d.fault==2)CHECK(d.atoms[126].value.i32==64&&d.atoms[127].value.u64==UINT64_MAX,"Actual foreign source-counter worksheet was lost");
    if(d.fault==3)CHECK(d.atoms[124].value.i32==62&&d.atoms[125].value.u64==UINT64_C(9007199254741043),"Actual missing-final-row source result changed");
    if(d.fault==4){size_t current=retained()+workbook_codec_read_owned_bytes(d.read);CHECK(current<HEADER_LIMIT&&HEADER_LIMIT-current<SIZE_MAX,"Source-counter pressure lacks admission headroom");d.pressure_bytes=HEADER_LIMIT-current+1u;d.pressure=dx_alloc(d.pressure_bytes);memset(d.pressure,0,d.pressure_bytes);CHECK(source_counters_observe(0),"Concrete source-counter pressure exceeds4MiB");}
    for(size_t first=0;first<rows;first+=32){
        size_t count=rows-first<32?rows-first:32;const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS&&request->first_row==first,"Source-counter split lost single-result position");size_t owned=workbook_codec_read_owned_bytes(d.read);const char *sql=request->sql;
        WorkbookCodecBatch batch={d.columns,2,d.atoms+first*2,count,first,first+count==rows};CHECK(source_counters_observe(0),"Source-counter feed coexistence exceeds admission");WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,&batch,retained(),&d.error);
        if(d.fault>=1&&d.fault<=4&&first==(d.fault==4?0u:32u)){
            const char *reason=d.fault==1?"SQL NULL":d.fault==2?"identity":d.fault==3?"exactly one row":"budget";CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error&&strstr(d.error,reason),d.error?d.error:"Expected source-counter failure absent");
            CHECK(source_counters_observe(0),"Source-counter rejection diagnostic exceeds admission");CHECK(workbook_codec_read_owned_bytes(d.read)==owned&&workbook_codec_read_request(d.read)->first_row==first&&workbook_codec_read_request(d.read)->sql==sql,"Rejected source-counter batch changed prefix/cursor/owned bytes");
            CHECK(source_counters_prior_unchanged(),"Rejected source counters changed old Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Rejected source-counter candidate replaced old Header");CHECK(source_counters_observe(0),"Source-counter finish diagnostic exceeds admission");source_counters.rejected_first[d.fault-1]=(unsigned)first;source_counters.rejected_feeds++;source_counters.proofs|=1u<<(d.fault-1);d.rejections++;return true;
        }
        CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid source-counter prefix rejected");CHECK(workbook_codec_read_owned_bytes(d.read)==owned,"Source-counter POD feed grew retained storage");source_counters.source32++;
    }
    CHECK(!d.fault||d.fault==6,"Rejected source-counter candidate completed unexpectedly");
    for(size_t row=0;row<64;row++){CHECK(d.atoms[row*2+1].kind==WORKBOOK_CODEC_U64,"Source counter missing before POD poison");d.atoms[row*2+1].value.u64=0;}
    CHECK(counters_poison(0)&&counters_poison(1),"Source-counter input poison failed");source_counters.source_poisoned++;source_counters.source_groups++;return true;
}
static void source_counters_free_schemas(void) {counters_free_schemas();page_free(&d.source_counters_schema);}
static bool source_counters_restart(void) {clear_input();d.fault++;objects_wire=false;return source_counters_begin();}
static bool source_counters_complete(void) {
    CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete source-counter Header could not move");CHECK(source_counters_header_contents(d.header,source_counters_versions[d.fixture],&d.key),"Completed source-counter Header differs from native fixture");d.headers++;size_t bytes=workbook_codec_header_owned_bytes(d.header);CHECK(bytes<=HEADER_LIMIT,"Completed source-counter Header exceeds admission");if(bytes>source_counters.maximum_header)source_counters.maximum_header=bytes;
    if(!d.fault){d.fixture++;if(d.fixture<4){source_counters_free_schemas();d.phase=0;return true;}d.fixture=3;d.fault=1;CHECK(source_counters_begin(),"Source-counter negative candidate begin failed");d.phase=1307;return true;}
    CHECK(d.fault==6&&d.fixtures==4&&d.headers==5&&d.queries==64&&d.rejections==4&&d.stale==1&&d.callbacks==63&&d.meta_queries==10&&d.sheets_queries==10&&counters.object_queries==10&&counters.counter_queries==9&&source_counters.source_queries==8&&d.describe_queries==17,"Source-counter terminal execution/Header inventory changed");
    CHECK(counters.objects56==10&&counters.counters32==18&&counters.groups==9&&counters.defaults==1&&counters.poisoned==9&&source_counters.source32==9&&source_counters.source_groups==3&&source_counters.source_defaults==2&&source_counters.source_poisoned==3&&source_counters.rejected_feeds==4&&source_counters.proofs==127,"Source-counter feed/default/POD proof inventory changed");
    CHECK(source_counters.stale_callbacks==1&&source_counters.stale_total==21&&source_counters.stale_wire&&source_counters.maximum_exact&&source_counters.source_above_objects&&source_counters.source_below_objects&&counters.retired_exact&&counters.maximum_exact,"Source-counter stale/exact independent-domain proofs incomplete");
    const unsigned first[]={32,32,32,0},normal[]={6,8,10,10};for(size_t n=0;n<4;n++)CHECK(source_counters.rejected_first[n]==first[n]&&d.normal_queries[n]==normal[n],"Source-counter failure prefix/normal requests changed");
    source_counters.detached_identity=source_counters_header_identity(d.header);d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
}
static bool source_counters_arm(void) {
    bool present=source_counters_versions[d.fixture]==14;unsigned queries=d.queries;
    CHECK(workbook_codec_read_add_source_version_counters(d.read,present,&d.error),d.error?d.error:"Source-counter continuation failed");
    if(!present){CHECK(!workbook_codec_read_request(d.read)&&queries==d.queries,"Older source-counter default submitted SQL");source_counters.source_defaults++;source_counters.proofs|=64;return source_counters_complete();}
    source_counters.pending_owned=workbook_codec_read_owned_bytes(d.read);source_counters.pending_sql=workbook_codec_read_request(d.read)->sql;d.phase=1312;return source_counters_send_family(WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,d.fault==5&&!source_counters.held_released);
}
static bool source_counters_tick_body(void) {
    if(d.phase==0){CHECK(d.fixture<4,"Source-counter fixture index exceeds inventory");d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",source_counters_files[d.fixture]);fixture_start(d.cookie,d.ticket,source_counters_files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=1301;return true;}
    if(d.phase==1301){if(!d.fixture_ready)return true;d.phase=1302;return source_counters_send(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false,false);}
    if(d.waiting)return true;
    if(d.phase>=1302&&d.phase<=1306){
        unsigned index=d.phase-1302;const char *names[]={"meta","sheets","objects","object_counters","source_version_counters"};Page *destinations[]={&d.meta_schema,&d.sheets_schema,&d.objects_schema,&d.counters_schema,&d.source_counters_schema};size_t counts[]={3,25,11,2,2};
        CHECK(d.ready&&!d.error,d.error?d.error:"Source-counter original DESCRIBE absent");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Source-counter original schema invalid");CHECK(source_counters_observe(schema.bytes),"Source-counter original schema coexistence exceeds admission");if(!source_schema(&schema,workbook_codec_table_find(names[index]),counts[index])){page_free(&schema);return false;}page_free(destinations[index]);*destinations[index]=schema;clear_input();
        unsigned version=source_counters_versions[d.fixture],last=version==12?2:version==13?3:4;
        if(index<last){d.phase++;return source_counters_send(dx_printf("DESCRIBE SELECT * FROM _duxcel_workspace.%s",names[index+1]),NULL,256,false,false);}
        for(unsigned n=0;n<=last;n++)CHECK(canonical_schema(destinations[n],workbook_codec_table_find(names[n]))&&destinations[n]->column_count==counts[n],"Source-counter canonical schema construction failed");CHECK(source_counters_begin(),"Source-counter Header begin failed");d.phase=1307;
    }
    if(d.phase==1307){d.phase=1308;return source_counters_send_family(WORKBOOK_CODEC_META,false);}
    if(d.phase==1308){CHECK(d.ready&&!d.error,d.error?d.error:"Source-counter Meta result absent");CHECK(counters_feed_base(WORKBOOK_CODEC_META),"Source-counter Meta feed failed");clear_input();d.phase=1309;return source_counters_send_family(WORKBOOK_CODEC_SHEETS,false);}
    if(d.phase==1309){CHECK(d.ready&&!d.error,d.error?d.error:"Source-counter Sheets result absent");CHECK(counters_feed_base(WORKBOOK_CODEC_SHEETS),"Source-counter Sheets feed failed");clear_input();CHECK(workbook_codec_read_add_objects(d.read,true,&d.error),d.error?d.error:"Source-counter objects continuation failed");d.phase=1310;return source_counters_send_family(WORKBOOK_CODEC_OBJECTS,false);}
    if(d.phase==1310){CHECK(d.ready&&!d.error,d.error?d.error:"Source-counter objects result absent");CHECK(counters_feed_objects(),"Source-counter objects feed failed");clear_input();bool present=source_counters_versions[d.fixture]>=13;unsigned queries=d.queries;CHECK(workbook_codec_read_add_object_counters(d.read,present,&d.error),d.error?d.error:"Object-counter dependency failed");if(!present){CHECK(!workbook_codec_read_request(d.read)&&queries==d.queries,"v12 object counter default submitted SQL");counters.defaults++;return source_counters_arm();}d.phase=1311;return source_counters_send_family(WORKBOOK_CODEC_OBJECT_COUNTERS,false);}
    if(d.phase==1311){CHECK(d.ready&&!d.error,d.error?d.error:"Object-counter dependency result absent");CHECK(source_counters_feed_object_counters(),"Object-counter dependency feed failed");clear_input();return source_counters_arm();}
    if(d.phase==1312){
        if(d.fault==5&&!source_counters.held_released){if(!d.held_ready)return true;CHECK(!d.ready,"Held source-counter result entered C before release");d.cookie++;cancel_candidate();d.waiting=false;source_counters.held_released=true;unsigned stale=d.stale;release_held();CHECK(d.stale==stale+1&&!d.ready&&source_counters_prior_unchanged(),"Stale source-counter callback borrowed/published input");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS&&!request->first_row&&request->sql==source_counters.pending_sql&&workbook_codec_read_owned_bytes(d.read)==source_counters.pending_owned,"Stale source-counter callback changed pending state");source_counters.proofs|=16;CHECK(source_counters_restart(),"Whole source-counter candidate restart failed");d.phase=1307;return true;}
        CHECK(d.ready&&!d.error,d.error?d.error:"Source-counter result absent");CHECK(source_counters_feed_result(),"Source-counter feed failed");if(d.fault>=1&&d.fault<=4){CHECK(source_counters_restart(),"Rejected source-counter restart failed");d.phase=1307;return true;}clear_input();if(d.fault==6)source_counters.proofs|=32;return source_counters_complete();
    }
    if(d.phase==9&&d.fixture_ready){clear_input();CHECK(source_counters_header_identity(d.header)==source_counters.detached_identity&&source_counters_header_contents(d.header,source_counters_versions[3],&d.key),"Source-counter Header changed after catalog close");source_counters_free_schemas();workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;d.done=true;printf("PASS: browser-codec-source-counters %u checks,4 native archives,5 Headers,64 queries,4 codec rejections,1 stale callback; no ledger or Workbook publication\n",d.checks);source_counters_report(true,NULL);emscripten_cancel_main_loop();return true;}
    return true;
}
static void source_counters_tick(void) {if(d.phase==1312&&d.fault==5&&d.held_ready&&!source_counters.held_released)d.waiting=false;source_counters_tick_body();}
