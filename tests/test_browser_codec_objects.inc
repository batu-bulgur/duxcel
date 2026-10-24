/* Object-descriptor registry only: one actual result, eight bounded feeds.
   Source/Pivot definitions, placement against manual cells, owner counters and
   complete Workbook publication remain separate metadata families. */
typedef struct {
    unsigned queries,accepted256,rejected_feeds,groups,proofs,prior_version;
    unsigned wire_rejections,schema_rejections,rejected_first[7];
    size_t pending_owned;
    bool held_released;
} ObjectsState;
static ObjectsState objects;
static size_t objects_retained(void) {return sizeof(objects);}
static bool objects_observe(size_t extra) {
    size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
    CHECK(current<=INPUT_LIMIT&&extra<=INPUT_LIMIT-current,"Concrete object-registry C buffers exceed admission");
    if(current+extra>d.peak_input)d.peak_input=current+extra;return true;
}
EM_JS(void, report_objects,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned queries,unsigned rejections,unsigned stale,size_t peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned registry,unsigned describe,unsigned accepted,unsigned rejected,unsigned groups,unsigned proofs,unsigned wire_rejections,unsigned schema_rejections,const unsigned *first_rows,const char *error),{
    const at=normal>>>2,rows=first_rows>>>2;
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'objects',checks,fixtures,headers,queries,rejections,stale,
        peakOwnedInput:peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:Array.from(HEAPU32.subarray(at,at+2)),familyExecutions:{meta,sheets,objects:registry,describe},
        acceptedFeeds:{rows256:accepted},rejectedFeeds:rejected,validSplitGroups:groups,
        rejectedFirstRows:Array.from(HEAPU32.subarray(rows,rows+7)),wireRejections:wire_rejections,schemaRejections:schema_rejections,
        requiredNullRejected:Boolean(proofs&1),duplicateIdRejected:Boolean(proofs&2),geometryRejected:Boolean(proofs&4),
        forwardBindingRejected:Boolean(proofs&8),overlapRejected:Boolean(proofs&16),sentinelRejected:Boolean(proofs&32),
        budgetRejected:Boolean(proofs&64),wireNulRejected:Boolean(proofs&128),originalTypeRejected:Boolean(proofs&256),
        error:error?UTF8ToString(error):null,boundary:'official-worker/shared-C-meta-sheets-object-registry Header; no full objects, source execution, Workbook publication or browser Save/Open'});
})
static void objects_report(bool ok,const char *error) {
    report_objects(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.peak_input,d.normal_queries,
        d.meta_queries,d.sheets_queries,objects.queries,d.describe_queries,objects.accepted256,objects.rejected_feeds,
        objects.groups,objects.proofs,objects.wire_rejections,objects.schema_rejections,objects.rejected_first,error);
}
static uint64_t objects_expected_id(size_t sheet,size_t index) {
    return sheet==63&&index==31?UINT64_MAX:UINT64_C(9007199254740993)+index;
}
static bool objects_header_contents(const WorkbookCodecHeader *h,unsigned version,const WorkbookCodecKey *expected) {
    const WorkbookCodecKey *key=workbook_codec_header_key(h);
    CHECK(h&&workbook_codec_header_objects_ready(h)&&!workbook_codec_header_sorts_ready(h)&&!workbook_codec_header_preferences_ready(h)&&
        workbook_codec_header_sheet_count(h)==64&&workbook_codec_header_active_sheet(h)==63,"Object registry Header readiness/count/active changed");
    CHECK(workbook_codec_header_version(h)==version&&workbook_codec_header_user_scale(h)==1.25f,"Object registry package version/scale changed");
    CHECK(key&&key->document_epoch==expected->document_epoch&&key->operation_ticket==expected->operation_ticket&&key->snapshot_generation==expected->snapshot_generation,"Object registry Header key changed");
    for(size_t sheet=0;sheet<64;sheet++){
        char name[96];snprintf(name,sizeof(name),"Sorted 'İ' %zu",sheet);
        CHECK(same(workbook_codec_header_name(h,sheet),name),"Object registry worksheet UTF-8/name changed");
        const Analysis *a=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);
        CHECK(a&&a->kind==SOURCE_CSV&&a->path&&!*a->path&&same(workbook_codec_header_reference(h,sheet),"missing-source.csv")&&same(a->schema,"")&&same(a->table,"NULL"),"Object registry raw reference/unavailable browser locator changed");
        CHECK(!a->query&&!a->source_version&&!a->unique_rows&&!a->filter_column&&!a->filter_value&&!a->filter_count&&!a->group_column&&!a->measure_column&&!a->sort_column&&!a->descending&&!a->sort_count,"Object registry loaded/executed unrelated source definitions");
        CHECK(v&&v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==1.5f&&!v->columns&&!v->column_count,"Object registry changed worksheet viewport");
        CHECK(workbook_codec_header_object_count(h,sheet)==32,"Object descriptor count changed");
        for(size_t index=0;index<32;index++){
            WorkbookCodecObjectView object={0};CHECK(workbook_codec_header_object_at(h,sheet,index,&object),"Owned object descriptor missing");
            if(index)snprintf(name,sizeof(name),"Object 'İ' %zu:%zu",sheet,index);else snprintf(name,sizeof(name),"NULL");
            ObjectKind kind=index==31?OBJECT_TABLE:index%2?OBJECT_CHART:OBJECT_PIVOT;
            ChartMark mark=index%2&&index!=31?(ChartMark)(index%4):MARK_BAR;
            CHECK(object.id==objects_expected_id(sheet,index)&&same(object.name,name)&&object.kind==kind&&object.mark==mark&&!object.chart_measure,"Exact object ID/name/kind/mark/measure lost");
            CHECK(object.row==10+index*12&&object.column==8&&object.height==8&&object.width==4&&object.source_id==(index==31?0:objects_expected_id(sheet,31)),"Exact object geometry/forward Table link changed");
        }
        WorkbookCodecObjectView invalid={.id=UINT64_MAX};CHECK(!workbook_codec_header_object_at(h,sheet,32,&invalid)&&invalid.id==UINT64_MAX,"Invalid object getter changed caller output");
    }
    CHECK(!workbook_codec_header_object_count(h,64),"Missing worksheet has object descriptors");return true;
}
static bool objects_prior_unchanged(void) {
    CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes,"Old independent object Header ownership changed");
    return !d.header||objects_header_contents(d.header,objects.prior_version,&d.prior_key);
}
static bool objects_begin(void) {objects.prior_version=workbook_codec_header_version(d.header);return begin_read();}
static bool objects_send(char *sql,const Page *schema,unsigned limit,bool hold,bool policy) {
    sorts_wire=false;objects_wire=policy;return send_sql(sql,schema,limit,hold);
}
static bool objects_send_family(WorkbookCodecFamily family,bool hold) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
    CHECK(request&&request->family==family&&!request->first_row,"Unexpected/reexecuted object-registry family cursor");
    if(family!=WORKBOOK_CODEC_OBJECTS){
        const Page *schema=family==WORKBOOK_CODEC_META?&d.meta_schema:&d.sheets_schema;
        if(family==WORKBOOK_CODEC_META)d.meta_queries++;else d.sheets_queries++;
        return objects_send(browser_page_sql(request->sql,schema,0,&d.error),schema,(unsigned)request->row_limit,false,false);
    }
    CHECK(request->row_limit==2049&&request->column_count==11&&request->source_column_count==11,"Object registry transport envelope changed");
    char *relation=dx_strdup(request->sql),*next=NULL;
    if(d.fault==1)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND id=9007199254740993::UBIGINT THEN NULL::VARCHAR ELSE name END AS name) FROM (%s) AS _objects_null",relation);
    else if(d.fault==2)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND id=9007199254740994::UBIGINT THEN 9007199254740993::UBIGINT ELSE id END AS id) FROM (%s) AS _objects_duplicate ORDER BY sheet_id,id",relation);
    else if(d.fault==3)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=63 AND id=18446744073709551615::UBIGINT THEN %" PRIu64 "::UBIGINT ELSE row_id END AS row_id) FROM (%s) AS _objects_geometry",(uint64_t)DX_SHEET_ROWS-1,relation);
    else if(d.fault==4)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=63 AND id=18446744073709551615::UBIGINT THEN %d::INTEGER ELSE kind END AS kind) FROM (%s) AS _objects_wrong_table",OBJECT_PIVOT,relation);
    else if(d.fault==5)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=63 AND id=18446744073709551615::UBIGINT THEN 370::UBIGINT ELSE row_id END AS row_id) FROM (%s) AS _objects_overlap",relation);
    else if(d.fault==6)next=dx_printf("SELECT * FROM ((%s) UNION ALL SELECT 64::INTEGER AS sheet_id,1::UBIGINT AS id,'sentinel'::VARCHAR AS name,10::UBIGINT AS row_id,8::INTEGER AS column_id,8::UBIGINT AS height,4::INTEGER AS width,0::INTEGER AS kind,0::INTEGER AS mark,0::UBIGINT AS source_id,0::INTEGER AS chart_measure) AS _objects_sentinel ORDER BY sheet_id,id LIMIT 2049",relation);
    else if(d.fault==8)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=63 AND id=18446744073709551615::UBIGINT THEN concat('a',chr(0),'b') ELSE name END AS name) FROM (%s) AS _objects_nul",relation);
    if(next){CHECK(objects_observe(text_bytes(relation)+text_bytes(next)),"Object fault SQL coexistence exceeds input budget");free(relation);relation=next;}
    /* browser_page_sql intentionally retains LIMIT256. This family already has
       its own LIMIT2049 and canonicalizes all eleven fixed named columns once. */
    char *sql=dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(id AS VARCHAR) AS id,CAST(name AS VARCHAR) AS name,CAST(row_id AS VARCHAR) AS row_id,CAST(column_id AS VARCHAR) AS column_id,CAST(height AS VARCHAR) AS height,CAST(width AS VARCHAR) AS width,CAST(kind AS VARCHAR) AS kind,CAST(mark AS VARCHAR) AS mark,CAST(source_id AS VARCHAR) AS source_id,CAST(chart_measure AS VARCHAR) AS chart_measure FROM (%s) AS _objects_projection",relation);
    CHECK(sql&&strlen(sql)<=DX_BROWSER_SQL_BYTES&&objects_observe(text_bytes(relation)+text_bytes(sql)),"Canonical object projection exceeds owned SQL budget");free(relation);objects.queries++;
    return objects_send(sql,&d.objects_schema,2049,hold,true);
}
static bool objects_atoms(const WorkbookCodecTable *table,const Page *schema,size_t rows,bool pending) {
    CHECK(d.page.column_count==schema->column_count&&d.page.row_count==rows&&d.page.offset==0&&d.page.total==rows&&d.page.count_pending==pending,"Object workflow dimensions/completeness changed");
    CHECK(!d.atoms,"Prior object atoms not retired");size_t bytes=rows*schema->column_count*sizeof(*d.atoms);
    CHECK(objects_observe(bytes),"Object atom staging exceeds input admission");d.atoms=dx_alloc(bytes);d.atom_bytes=bytes;const char *types=table->types;
    for(size_t column=0;column<schema->column_count;column++){
        while(*types==' ')types++;WorkbookCodecAtomKind kind=atom_kind(*types++);const Column *actual=d.page.columns+column,*expected=schema->columns+column;
        CHECK(same(actual->name,expected->name)&&same(actual->type_name,expected->type_name)&&actual->type==expected->type&&actual->number_type.kind==expected->number_type.kind&&actual->number_type.precision==expected->number_type.precision&&actual->number_type.scale==expected->number_type.scale,"Object canonical original-type sidecar changed");
        d.columns[column]=(WorkbookCodecColumn){actual->name,kind};
        for(size_t row=0;row<rows;row++){
            const Cell *cell=d.page.cells+row*schema->column_count+column;WorkbookCodecAtom *atom=d.atoms+row*schema->column_count+column;
            atom->kind=cell->is_null?WORKBOOK_CODEC_NULL:kind;if(cell->is_null)continue;CHECK(cell->text,"Object atom has no exact text");
            if(kind==WORKBOOK_CODEC_I32)CHECK(i32(cell->text,&atom->value.i32),"Object INTEGER text was not canonical/exact");
            else if(kind==WORKBOOK_CODEC_U64)CHECK(u64(cell->text,&atom->value.u64),"Object UBIGINT text was not canonical/exact");
            else if(kind==WORKBOOK_CODEC_DOUBLE){char *end=NULL;atom->value.real=strtod(cell->text,&end);CHECK(end&&end!=cell->text&&!*end,"Object/base DOUBLE text invalid");}
            else if(kind==WORKBOOK_CODEC_BOOLEAN){CHECK(!strcmp(cell->text,"true")||!strcmp(cell->text,"false"),"Object/base BOOLEAN text invalid");atom->value.boolean=!strcmp(cell->text,"true");}
            else atom->value.text=(WorkbookCodecText){cell->text,strlen(cell->text)};
        }
    }
    return true;
}
static bool objects_feed_base(WorkbookCodecFamily family) {
    bool meta=family==WORKBOOK_CODEC_META;
    CHECK(objects_atoms(workbook_codec_table_find(meta?"meta":"sheets"),meta?&d.meta_schema:&d.sheets_schema,meta?1:64,false),"Object workflow base atoms failed");
    WorkbookCodecBatch batch={d.columns,d.page.column_count,d.atoms,d.page.row_count,0,true};CHECK(objects_observe(0),"Object base retained state exceeds admission");
    CHECK(workbook_codec_read_feed(d.read,d.key,family,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Object base metadata rejected");return true;
}
static bool objects_feed_result(void) {
    bool reject=d.fault>=1&&d.fault<=7,sentinel=d.fault==6;size_t rows=sentinel?2049:2048;
    CHECK(objects_atoms(workbook_codec_table_find("objects"),&d.objects_schema,rows,sentinel),"Object exact atoms failed");
    if(d.fault==7){
        size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
        CHECK(current<INPUT_LIMIT-65536u,"Object pressure fixture has no concrete headroom");
        d.pressure_bytes=INPUT_LIMIT-current-65536u;d.pressure=dx_alloc(d.pressure_bytes);memset(d.pressure,0,d.pressure_bytes);
        CHECK(objects_observe(0),"Concrete object pressure exceeds total input admission");
    }
    unsigned accepted=0;
    for(size_t row=0;row<rows;row+=256){
        size_t count=rows-row<256?rows-row:256;const WorkbookCodecRequest *before=workbook_codec_read_request(d.read);
        CHECK(before&&before->family==WORKBOOK_CODEC_OBJECTS&&before->first_row==row,"Object chunks do not follow one original result");
        size_t bytes=workbook_codec_read_owned_bytes(d.read);const char *sql=before->sql;
        WorkbookCodecBatch batch={d.columns,11,d.atoms+row*11,count,row,row+count==rows&&!d.page.count_pending};CHECK(objects_observe(0),"Object feed coexistence exceeds admission");
        WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_OBJECTS,&batch,retained(),&d.error);
        size_t failed_row=d.fault>=3&&d.fault<=5?1792:sentinel?2048:0;bool fails=reject&&row==failed_row;
        if(fails){
            static const char *reasons[]={"SQL NULL","strictly ordered","geometry","reference a Table descriptor","overlap","Too many object descriptors","budget"};
            CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error&&strstr(d.error,reasons[d.fault-1]),d.error?d.error:"Expected object semantic/budget failure missing");
            CHECK(workbook_codec_read_owned_bytes(d.read)==bytes&&workbook_codec_read_request(d.read)->first_row==row&&workbook_codec_read_request(d.read)->sql==sql,"Failed object batch changed accepted candidate ownership/cursor");
            CHECK(objects_prior_unchanged(),"Rejected object batch changed old Header");
            CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Failed object candidate replaced old Header");
            CHECK(objects_observe(0),"Object rejection diagnostic exceeds admission");objects.rejected_first[d.fault-1]=(unsigned)row;
            d.rejections++;objects.rejected_feeds++;objects.proofs|=1u<<(d.fault-1);return true;
        }
        CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid object prefix rejected");CHECK(count==256,"Valid object feed must contain256 original rows");objects.accepted256++;accepted++;
    }
    CHECK(!reject&&accepted==8,"Valid object registry did not use exactly eight256-row feeds");objects.groups++;return true;
}
static bool objects_restart(void) {clear_input();d.fault++;objects_wire=false;return objects_begin();}
static bool objects_tick_body(void) {
    if(d.phase==0){
        CHECK(d.fixture<2,"Object fixture index exceeds inventory");d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;
        char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",objects_files[d.fixture]);
        fixture_start(d.cookie,d.ticket,objects_files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=301;return true;
    }
    if(d.phase==301){if(!d.fixture_ready)return true;d.phase=302;return objects_send(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false,false);}
    if(d.waiting)return true;
    if(d.phase>=302&&d.phase<=304){
        CHECK(d.ready&&!d.error,d.error?d.error:"Original object workflow DESCRIBE missing");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Original object schema invalid");
        CHECK(objects_observe(schema.bytes),"Original object schema coexistence exceeds admission");const WorkbookCodecTable *table=workbook_codec_table_find(d.phase==302?"meta":d.phase==303?"sheets":"objects");
        if(!source_schema(&schema,table,d.phase==302?3:d.phase==303?25:11)){page_free(&schema);return false;}
        Page *destination=d.phase==302?&d.meta_schema:d.phase==303?&d.sheets_schema:&d.objects_schema;page_free(destination);*destination=schema;clear_input();
        if(d.phase<304){d.phase++;return objects_send(dx_strdup(d.phase==303?"DESCRIBE SELECT * FROM _duxcel_workspace.sheets":"DESCRIBE SELECT * FROM _duxcel_workspace.objects"),NULL,256,false,false);}
        CHECK(canonical_schema(&d.meta_schema,workbook_codec_table_find("meta"))&&canonical_schema(&d.sheets_schema,workbook_codec_table_find("sheets"))&&canonical_schema(&d.objects_schema,workbook_codec_table_find("objects")),"Object canonical schemas failed");
        CHECK(objects_begin(),"Object workflow header begin failed");d.phase=305;
    }
    if(d.phase==305){d.phase=306;return objects_send_family(WORKBOOK_CODEC_META,false);}
    if(d.phase==306){CHECK(d.ready&&!d.error,d.error?d.error:"Object meta result absent");CHECK(objects_feed_base(WORKBOOK_CODEC_META),"Object meta feed failed");clear_input();d.phase=307;return objects_send_family(WORKBOOK_CODEC_SHEETS,false);}
    if(d.phase==307){
        CHECK(d.ready&&!d.error,d.error?d.error:"Object sheets result absent");CHECK(objects_feed_base(WORKBOOK_CODEC_SHEETS),"Object sheets feed failed");clear_input();
        CHECK(workbook_codec_read_add_objects(d.read,true,&d.error),d.error?d.error:"Object continuation admission failed");objects.pending_owned=workbook_codec_read_owned_bytes(d.read);
        if(d.fault==9){d.phase=309;return objects_send(dx_strdup("DESCRIBE SELECT sheet_id,id::VARCHAR AS id,* EXCLUDE(sheet_id,id) FROM _duxcel_workspace.objects"),NULL,256,false,false);}
        d.phase=308;return objects_send_family(WORKBOOK_CODEC_OBJECTS,d.fault==10&&!objects.held_released);
    }
    if(d.phase==309){
        CHECK(d.ready&&!d.error,d.error?d.error:"Wrong original object type DESCRIBE absent");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),"Malformed-type DESCRIBE did not decode as a schema");
        CHECK(objects_observe(schema.bytes),"Rejected original schema exceeds input admission");bool rejected=!schema_matches(&schema,workbook_codec_table_find("objects"),11);page_free(&schema);
        CHECK(rejected,"Original VARCHAR object ID was accepted as UBIGINT");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
        CHECK(request&&request->family==WORKBOOK_CODEC_OBJECTS&&!request->first_row&&workbook_codec_read_owned_bytes(d.read)==objects.pending_owned,"Wrong original schema altered object cursor");
        CHECK(objects_prior_unchanged(),"Wrong original schema changed old Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Wrong-schema candidate replaced old Header");
        d.rejections++;objects.schema_rejections++;objects.proofs|=256;CHECK(objects_restart(),"Wrong schema recovery begin failed");d.phase=305;return true;
    }
    if(d.phase==308){
        if(d.fault==10&&!objects.held_released){
            if(!d.held_ready)return true;CHECK(!d.ready,"Held object result entered C before release");d.cookie++;cancel_candidate();d.waiting=false;objects.held_released=true;
            unsigned stale=d.stale;release_held();CHECK(d.stale==stale+1&&!d.ready&&objects_prior_unchanged(),"Superseded object callback accessed/published borrowed wire");return objects_send_family(WORKBOOK_CODEC_OBJECTS,false);
        }
        CHECK(d.ready,"Object registry result missing");
        if(d.fault==8){
            CHECK(d.error&&strstr(d.error,"NUL"),"Actual embedded-NUL object name did not fail transport");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
            CHECK(request&&request->family==WORKBOOK_CODEC_OBJECTS&&!request->first_row&&workbook_codec_read_owned_bytes(d.read)==objects.pending_owned&&!d.page.columns&&!d.page.cells,"NUL failure published a Page or changed object cursor");
            CHECK(objects_prior_unchanged(),"NUL wire failure changed old Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"NUL candidate replaced old Header");
            d.rejections++;objects.wire_rejections++;objects.proofs|=128;CHECK(objects_restart(),"NUL recovery begin failed");d.phase=305;return true;
        }
        CHECK(!d.error,d.error?d.error:"Object transport error");CHECK(objects_feed_result(),"Actual object feed failed");
        if(d.fault>=1&&d.fault<=7){CHECK(objects_restart(),"Object rejection recovery begin failed");d.phase=305;return true;}
        clear_input();CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete object Header could not move");
        CHECK(objects_header_contents(d.header,objects_versions[d.fixture],&d.key),"Completed owned object Header differs from native fixture");d.headers++;
        if(!d.fault){
            d.fixture++;if(d.fixture<2){page_free(&d.meta_schema);page_free(&d.sheets_schema);page_free(&d.objects_schema);d.phase=0;return true;}
            d.fixture=1;d.fault=1;objects_wire=false;CHECK(objects_begin(),"Object negative candidate begin failed");d.phase=305;return true;
        }
        if(d.fault==10){CHECK(objects_restart(),"Final object recovery begin failed");d.phase=305;return true;}
        CHECK(d.fault==11&&d.fixtures==2&&d.headers==4&&d.queries==46&&d.rejections==9&&d.stale==1&&d.meta_queries==13&&d.sheets_queries==13&&objects.queries==13&&d.describe_queries==7,"Object terminal metadata workload changed");
        CHECK(objects.accepted256==61&&objects.rejected_feeds==7&&objects.groups==4&&objects.proofs==511&&objects.wire_rejections==1&&objects.schema_rejections==1,"Object accepted-prefix/rejection/proof inventory changed");
        const unsigned first_rows[]={0,0,1792,1792,1792,2048,0};for(size_t i=0;i<7;i++)CHECK(objects.rejected_first[i]==first_rows[i],"Failed object batch lost its accepted-prefix boundary");
        for(size_t i=0;i<2;i++)CHECK(d.normal_queries[i]==6,"Normal object fixture query workload changed");
        d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
    }
    if(d.phase==9&&d.fixture_ready){
        clear_input();page_free(&d.meta_schema);page_free(&d.sheets_schema);page_free(&d.objects_schema);workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;
        d.done=true;printf("PASS: browser-codec-objects %u checks,2 current native fixtures,4 Headers,46 queries,7 codec/1 wire/1 schema rejections,1 stale callback; no Workbook publication\n",d.checks);objects_report(true,NULL);emscripten_cancel_main_loop();return true;
    }
    return true;
}
static void objects_tick(void) {
    if(d.phase==308&&d.fault==10&&d.held_ready&&!objects.held_released)d.waiting=false;objects_tick_body();
}
