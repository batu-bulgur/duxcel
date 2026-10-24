/* Complete owner query registry through one immutable official-Worker result.
   This retains raw definitions only; Table sources/options, full objects and
   Workbook publication remain separate. Stored query strings never execute. */
typedef struct {
    unsigned object_queries,query_queries,object256,query256,query64,object_groups,groups;
    unsigned rejected_feeds,schema_rejections,proofs,prior_version,physical_nulls,poisoned;
    unsigned rejected_first[6],physical_first[2];
    size_t pending_owned;
    bool held_released;
} QueriesState;
static QueriesState queries;
static size_t queries_retained(void) {return sizeof(queries);}
static bool queries_observe(size_t extra) {
    size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
    CHECK(current<=INPUT_LIMIT&&extra<=INPUT_LIMIT-current,"Concrete owner-query C buffers exceed admission");
    if(current+extra>d.peak_input)d.peak_input=current+extra;return true;
}
EM_JS(void, report_queries,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned requests,unsigned rejections,unsigned stale,size_t peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned objects,unsigned definitions,unsigned describe,unsigned object256,unsigned query256,unsigned query64,unsigned object_groups,unsigned groups,unsigned rejected,unsigned schema_rejections,unsigned proofs,unsigned physical_nulls,unsigned poisoned,const unsigned *first_rows,const unsigned *physical_rows,const char *error),{
    const at=normal>>>2,rows=first_rows>>>2,physical=physical_rows>>>2;
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'queries',checks,fixtures,headers,queries:requests,rejections,stale,
        peakOwnedInput:peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:Array.from(HEAPU32.subarray(at,at+4)),familyExecutions:{meta,sheets,objects,queries:definitions,describe},
        acceptedFeeds:{objects256:object256,queries256:query256,queries64:query64},objectSplitGroups:object_groups,validSplitGroups:groups,
        rejectedFeeds:rejected,schemaRejections:schema_rejections,rejectedFirstRows:Array.from(HEAPU32.subarray(rows,rows+6)),
        physicalRejectedFirstRows:Array.from(HEAPU32.subarray(physical,physical+2)),physicalInvalidAsNull:physical_nulls,poisonedQueryResults:poisoned,
        physicalBlankRejected:Boolean(proofs&1),physicalNulRejected:Boolean(proofs&2),requiredNullRejected:Boolean(proofs&4),
        duplicateOwnerRejected:Boolean(proofs&8),wrongKindRejected:Boolean(proofs&16),foreignOwnerRejected:Boolean(proofs&32),
        sentinelRejected:Boolean(proofs&64),budgetRejected:Boolean(proofs&128),originalTypeRejected:Boolean(proofs&256),
        error:error?UTF8ToString(error):null,boundary:'official-worker/shared-C-meta-sheets-object-query Header; raw definitions only, no Table Analysis, source execution, Workbook publication or browser Save/Open'});
})
static void queries_report(bool ok,const char *error) {
    report_queries(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.peak_input,d.normal_queries,
        d.meta_queries,d.sheets_queries,queries.object_queries,queries.query_queries,d.describe_queries,queries.object256,
        queries.query256,queries.query64,queries.object_groups,queries.groups,queries.rejected_feeds,queries.schema_rejections,
        queries.proofs,queries.physical_nulls,queries.poisoned,queries.rejected_first,queries.physical_first,error);
}
static uint64_t queries_expected_id(size_t sheet,size_t index) {
    return sheet==63&&index==31?UINT64_MAX:UINT64_C(9007199254740993)+index;
}
static bool queries_expected_text(const char *actual,size_t sheet,size_t slot) {
    char expected[128];if(!sheet&&!slot)snprintf(expected,sizeof(expected),"NULL");
    else snprintf(expected,sizeof(expected),"SELECT 1 /* 'İ' %zu:%zu */;  \n",sheet,slot);
    CHECK(same(actual,expected),"Exact raw query/literal NULL/trailing UTF-8 bytes changed");return true;
}
static bool queries_header_contents(const WorkbookCodecHeader *h,unsigned version,const WorkbookCodecKey *expected) {
    const WorkbookCodecKey *key=workbook_codec_header_key(h);
    CHECK(h&&workbook_codec_header_objects_ready(h)&&workbook_codec_header_queries_ready(h)&&!workbook_codec_header_sorts_ready(h)&&!workbook_codec_header_preferences_ready(h)&&
        workbook_codec_header_sheet_count(h)==64&&workbook_codec_header_active_sheet(h)==63,"Owner-query Header readiness/count/active changed");
    CHECK(workbook_codec_header_version(h)==version&&workbook_codec_header_user_scale(h)==1.25f,"Owner-query package version/scale changed");
    CHECK(key&&key->document_epoch==expected->document_epoch&&key->operation_ticket==expected->operation_ticket&&key->snapshot_generation==expected->snapshot_generation,"Owner-query Header key changed");
    for(size_t sheet=0;sheet<64;sheet++){
        char name[96];snprintf(name,sizeof(name),"Sorted 'İ' %zu",sheet);
        CHECK(same(workbook_codec_header_name(h,sheet),name),"Owner-query worksheet UTF-8/name changed");
        const Analysis *a=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);const char *text=NULL;
        CHECK(a&&a->kind==SOURCE_CSV&&a->path&&!*a->path&&same(workbook_codec_header_reference(h,sheet),"missing-source.csv")&&same(a->schema,"")&&same(a->table,"NULL"),"Raw source reference/unavailable locator changed");
        CHECK(!a->source_version&&!a->unique_rows&&!a->filter_column&&!a->filter_value&&!a->filter_count&&!a->group_column&&!a->measure_column&&!a->sort_column&&!a->descending&&!a->sort_count,"Owner-query Header fabricated unrelated source state");
        CHECK(workbook_codec_header_query(h,sheet,0,&text)&&a->query==text&&queries_expected_text(text,sheet,0),"Primary Analysis query and borrowed getter disagree");
        CHECK(v&&v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==1.5f&&!v->columns&&!v->column_count,"Owner-query viewport changed");
        CHECK(workbook_codec_header_object_count(h,sheet)==32,"Owner-query Table registry count changed");
        for(size_t index=0;index<32;index++){
            WorkbookCodecObjectView object={0};CHECK(workbook_codec_header_object_at(h,sheet,index,&object),"Owner-query Table descriptor missing");
            if(index)snprintf(name,sizeof(name),"Object 'İ' %zu:%zu",sheet,index);else snprintf(name,sizeof(name),"NULL");
            CHECK(object.id==queries_expected_id(sheet,index)&&same(object.name,name)&&object.kind==OBJECT_TABLE&&object.mark==MARK_BAR&&!object.chart_measure&&!object.source_id,"Exact Table ID/name/representation changed");
            CHECK(object.row==10+index*12&&object.column==8&&object.height==8&&object.width==4,"Exact Table geometry changed");
            CHECK(workbook_codec_header_query(h,sheet,object.id,&text)&&queries_expected_text(text,sheet,index+1),"Complete Table-owned query is missing");
        }
    }
    const char *untouched="untouched",*text=untouched;
    CHECK(!workbook_codec_header_query(h,64,0,&text)&&text==untouched,"Missing-sheet query getter modified output");
    CHECK(!workbook_codec_header_query(h,0,UINT64_MAX,&text)&&text==untouched,"Foreign MAX owner resolved across worksheets");
    CHECK(!workbook_codec_header_query(h,0,1,&text)&&text==untouched&&!workbook_codec_header_query(h,0,0,NULL),"Invalid owner/output query getter changed");return true;
}
static bool queries_prior_unchanged(void) {
    CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes,"Old independent query Header ownership changed");
    return !d.header||queries_header_contents(d.header,queries.prior_version,&d.prior_key);
}
static bool queries_begin(void) {queries.prior_version=workbook_codec_header_version(d.header);return begin_read();}
static bool queries_send(char *sql,const Page *schema,unsigned limit,bool hold,unsigned policy) {
    sorts_wire=false;objects_wire=policy==2;queries_wire=policy==3;return send_sql(sql,schema,limit,hold);
}
static bool queries_send_family(WorkbookCodecFamily family,bool hold) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
    CHECK(request&&request->family==family&&!request->first_row,"Unexpected/reexecuted owner-query family cursor");
    if(family==WORKBOOK_CODEC_META||family==WORKBOOK_CODEC_SHEETS){
        const Page *schema=family==WORKBOOK_CODEC_META?&d.meta_schema:&d.sheets_schema;
        if(family==WORKBOOK_CODEC_META)d.meta_queries++;else d.sheets_queries++;
        return queries_send(browser_page_sql(request->sql,schema,0,&d.error),schema,(unsigned)request->row_limit,false,0);
    }
    bool objects=family==WORKBOOK_CODEC_OBJECTS;
    CHECK(objects||family==WORKBOOK_CODEC_QUERIES,"Unsupported owner-query family");
    CHECK(request->row_limit==(objects?2049u:2113u)&&request->column_count==(objects?11u:3u)&&request->source_column_count==request->column_count,"Owner metadata envelope changed");
    char *relation=dx_strdup(request->sql),*next=NULL;
    if(objects&&d.fault==3)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND id=9007199254740993::UBIGINT THEN %d::INTEGER ELSE kind END AS kind) FROM (%s) AS _query_wrong_kind",OBJECT_PIVOT,relation);
    if(!objects){
        CHECK(strstr(relation,"CASE WHEN query IS NULL OR octet_length(encode(query))>65536 OR length(trim(query))=0 OR contains(query,chr(0)) THEN NULL::VARCHAR ELSE query END AS query"),"Trusted query guard does not preserve native invalid-row policy");
        if(d.fault==1)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND object_id=0 THEN NULL::VARCHAR ELSE query END AS query) FROM (%s) AS _query_null",relation);
        else if(d.fault==2)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND object_id=9007199254740993::UBIGINT THEN 0::UBIGINT ELSE object_id END AS object_id) FROM (%s) AS _query_duplicate ORDER BY sheet_id,object_id",relation);
        else if(d.fault==4)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND object_id=9007199254740993::UBIGINT THEN 18446744073709551615::UBIGINT ELSE object_id END AS object_id) FROM (%s) AS _query_foreign ORDER BY sheet_id,object_id",relation);
        else if(d.fault==5)next=dx_printf("SELECT * FROM ((%s) UNION ALL SELECT 64::INTEGER AS sheet_id,0::UBIGINT AS object_id,'sentinel'::VARCHAR AS query) AS _query_sentinel ORDER BY sheet_id,object_id LIMIT 2113",relation);
    }
    if(next){CHECK(queries_observe(text_bytes(relation)+text_bytes(next)),"Owner fault SQL coexistence exceeds admission");free(relation);relation=next;}
    /* Neither family may use browser_page_sql's ordinary LIMIT256 wrapper. */
    char *sql=objects?dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(id AS VARCHAR) AS id,CAST(name AS VARCHAR) AS name,CAST(row_id AS VARCHAR) AS row_id,CAST(column_id AS VARCHAR) AS column_id,CAST(height AS VARCHAR) AS height,CAST(width AS VARCHAR) AS width,CAST(kind AS VARCHAR) AS kind,CAST(mark AS VARCHAR) AS mark,CAST(source_id AS VARCHAR) AS source_id,CAST(chart_measure AS VARCHAR) AS chart_measure FROM (%s) AS _query_objects_projection",relation):
        dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(object_id AS VARCHAR) AS object_id,CAST(query AS VARCHAR) AS query FROM (%s) AS _query_projection",relation);
    CHECK(sql&&strlen(sql)<=DX_BROWSER_SQL_BYTES&&queries_observe(text_bytes(relation)+text_bytes(sql)),"Owner canonical projection exceeds SQL/owned admission");free(relation);
    if(objects)queries.object_queries++;else queries.query_queries++;
    return queries_send(sql,objects?&d.objects_schema:&d.queries_schema,objects?2049:2113,hold,objects?2:3);
}
static bool queries_atoms(const WorkbookCodecTable *table,const Page *schema,size_t rows,bool pending) {
    CHECK(d.page.column_count==schema->column_count&&d.page.row_count==rows&&d.page.offset==0&&d.page.total==rows&&d.page.count_pending==pending,"Owner workflow dimensions/completeness changed");
    CHECK(!d.atoms,"Prior owner-query atoms not retired");size_t bytes=rows*schema->column_count*sizeof(*d.atoms);
    CHECK(queries_observe(bytes),"Owner exact-atom staging exceeds admission");d.atoms=dx_alloc(bytes);d.atom_bytes=bytes;const char *types=table->types;
    for(size_t column=0;column<schema->column_count;column++){
        while(*types==' ')types++;WorkbookCodecAtomKind kind=atom_kind(*types++);const Column *actual=d.page.columns+column,*expected=schema->columns+column;
        CHECK(same(actual->name,expected->name)&&same(actual->type_name,expected->type_name)&&actual->type==expected->type&&actual->number_type.kind==expected->number_type.kind&&actual->number_type.precision==expected->number_type.precision&&actual->number_type.scale==expected->number_type.scale,"Owner canonical original-type sidecar changed");
        d.columns[column]=(WorkbookCodecColumn){actual->name,kind};
        for(size_t row=0;row<rows;row++){
            const Cell *cell=d.page.cells+row*schema->column_count+column;WorkbookCodecAtom *atom=d.atoms+row*schema->column_count+column;
            atom->kind=cell->is_null?WORKBOOK_CODEC_NULL:kind;if(cell->is_null)continue;CHECK(cell->text,"Owner atom has no exact text");
            if(kind==WORKBOOK_CODEC_I32)CHECK(i32(cell->text,&atom->value.i32),"Owner INTEGER text was not canonical/exact");
            else if(kind==WORKBOOK_CODEC_U64)CHECK(u64(cell->text,&atom->value.u64),"Owner UBIGINT text was not canonical/exact");
            else if(kind==WORKBOOK_CODEC_DOUBLE){char *end=NULL;atom->value.real=strtod(cell->text,&end);CHECK(end&&end!=cell->text&&!*end,"Owner/base DOUBLE text invalid");}
            else if(kind==WORKBOOK_CODEC_BOOLEAN){CHECK(!strcmp(cell->text,"true")||!strcmp(cell->text,"false"),"Owner/base BOOLEAN text invalid");atom->value.boolean=!strcmp(cell->text,"true");}
            else atom->value.text=(WorkbookCodecText){cell->text,strlen(cell->text)};
        }
    }
    return true;
}
static bool queries_feed_base(WorkbookCodecFamily family) {
    bool meta=family==WORKBOOK_CODEC_META;
    CHECK(queries_atoms(workbook_codec_table_find(meta?"meta":"sheets"),meta?&d.meta_schema:&d.sheets_schema,meta?1:64,false),"Owner base atoms failed");
    WorkbookCodecBatch batch={d.columns,d.page.column_count,d.atoms,d.page.row_count,0,true};CHECK(queries_observe(0),"Owner base retained state exceeds admission");
    CHECK(workbook_codec_read_feed(d.read,d.key,family,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Owner base metadata rejected");return true;
}
static bool queries_poison_text(size_t column) {
    for(size_t row=0;row<d.page.row_count;row++){
        Cell *cell=d.page.cells+row*d.page.column_count+column;CHECK(!cell->is_null&&cell->text,"Successful owned metadata text missing before overwrite");
        memset(cell->text,'~',strlen(cell->text));
    }
    return true;
}
static bool queries_feed_objects(void) {
    CHECK(queries_atoms(workbook_codec_table_find("objects"),&d.objects_schema,2048,false),"Owner Table registry atoms failed");
    for(size_t first=0;first<2048;first+=256){
        const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_OBJECTS&&request->first_row==first,"Owner descriptor chunks reran or lost cursor");
        WorkbookCodecBatch batch={d.columns,11,d.atoms+first*11,256,first,first+256==2048};CHECK(queries_observe(0),"Owner descriptor feed exceeds admission");
        CHECK(workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_OBJECTS,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid owner descriptor prefix rejected");queries.object256++;
    }
    queries.object_groups++;return queries_poison_text(2);
}
static bool queries_feed_result(void) {
    bool physical=!d.fault&&(d.fixture==1||d.fixture==2),reject=physical||(d.fault>=1&&d.fault<=6),sentinel=d.fault==5;
    size_t rows=sentinel?2113:2112;CHECK(queries_atoms(workbook_codec_table_find("queries"),&d.queries_schema,rows,sentinel),"Owner query exact atoms failed");
    if(physical){
        const WorkbookCodecAtom *last=d.atoms+2111*3;
        CHECK(last[0].value.i32==63&&last[1].value.u64==UINT64_MAX&&last[2].kind==WORKBOOK_CODEC_NULL&&d.page.cells[2111*3+2].is_null,"Physical blank/NUL query was not mapped to required NULL before wire");queries.physical_nulls++;
    }
    if(d.fault==6){
        size_t current=retained()+workbook_codec_read_owned_bytes(d.read);CHECK(current<INPUT_LIMIT-65536u,"Owner pressure fixture has no concrete headroom");
        d.pressure_bytes=INPUT_LIMIT-current-65536u;d.pressure=dx_alloc(d.pressure_bytes);memset(d.pressure,0,d.pressure_bytes);CHECK(queries_observe(0),"Concrete query pressure exceeds input admission");
    }
    unsigned accepted=0;size_t failed_row=physical||sentinel?2048:0;
    for(size_t row=0;row<rows;row+=256){
        size_t count=rows-row<256?rows-row:256;const WorkbookCodecRequest *before=workbook_codec_read_request(d.read);
        CHECK(before&&before->family==WORKBOOK_CODEC_QUERIES&&before->first_row==row,"Query chunks do not follow one original result");
        size_t owned=workbook_codec_read_owned_bytes(d.read);const char *sql=before->sql;
        WorkbookCodecBatch batch={d.columns,3,d.atoms+row*3,count,row,row+count==rows&&!d.page.count_pending};CHECK(queries_observe(0),"Query feed coexistence exceeds admission");
        WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_QUERIES,&batch,retained(),&d.error);
        if(reject&&row==failed_row){
            static const char *reasons[]={"SQL NULL","strictly ordered","existing source owner","existing source owner","Too many query definitions","budget"};
            const char *reason=physical?"SQL NULL":reasons[d.fault-1];
            CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error&&strstr(d.error,reason),d.error?d.error:"Expected owner query rejection missing");
            CHECK(workbook_codec_read_owned_bytes(d.read)==owned&&workbook_codec_read_request(d.read)->first_row==row&&workbook_codec_read_request(d.read)->sql==sql,"Failed query batch changed accepted ownership/cursor");
            CHECK(queries_prior_unchanged(),"Rejected query batch changed old independent Header");
            CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Rejected query candidate replaced old Header");CHECK(queries_observe(0),"Rejected query diagnostic exceeds admission");
            if(physical){queries.physical_first[d.fixture-1]=(unsigned)row;queries.proofs|=1u<<(d.fixture-1);}
            else{queries.rejected_first[d.fault-1]=(unsigned)row;queries.proofs|=1u<<(d.fault+1);}
            d.rejections++;queries.rejected_feeds++;return true;
        }
        CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid query prefix rejected");
        if(count==256){queries.query256++;accepted++;}else{CHECK(count==64&&!reject,"Valid query tail must contain64 rows");queries.query64++;}
    }
    CHECK(!reject&&accepted==8,"Complete query registry did not use eight256-row prefixes");queries.groups++;queries.poisoned++;return queries_poison_text(2);
}
static bool queries_restart(void) {clear_input();d.fault++;objects_wire=false;queries_wire=false;return queries_begin();}
static bool queries_next_fixture(void) {
    clear_input();page_free(&d.meta_schema);page_free(&d.sheets_schema);page_free(&d.objects_schema);page_free(&d.queries_schema);
    objects_wire=false;queries_wire=false;d.fixture++;CHECK(d.fixture<4,"Owner fixture inventory exhausted");d.phase=0;return true;
}
static bool queries_tick_body(void) {
    if(d.phase==0){
        CHECK(d.fixture<4,"Query fixture index exceeds inventory");d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;
        char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",queries_files[d.fixture]);
        fixture_start(d.cookie,d.ticket,queries_files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=401;return true;
    }
    if(d.phase==401){if(!d.fixture_ready)return true;d.phase=402;return queries_send(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false,0);}
    if(d.waiting)return true;
    if(d.phase>=402&&d.phase<=405){
        CHECK(d.ready&&!d.error,d.error?d.error:"Original query workflow DESCRIBE missing");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Original owner schema invalid");
        CHECK(queries_observe(schema.bytes),"Original owner schema coexistence exceeds admission");const char *table_name=d.phase==402?"meta":d.phase==403?"sheets":d.phase==404?"objects":"queries";
        const WorkbookCodecTable *table=workbook_codec_table_find(table_name);if(!source_schema(&schema,table,d.phase==403?25:d.phase==404?11:3)){page_free(&schema);return false;}
        Page *destination=d.phase==402?&d.meta_schema:d.phase==403?&d.sheets_schema:d.phase==404?&d.objects_schema:&d.queries_schema;page_free(destination);*destination=schema;clear_input();
        if(d.phase<405){d.phase++;const char *next=d.phase==403?"sheets":d.phase==404?"objects":"queries";return queries_send(dx_printf("DESCRIBE SELECT * FROM _duxcel_workspace.%s",next),NULL,256,false,0);}
        CHECK(canonical_schema(&d.meta_schema,workbook_codec_table_find("meta"))&&canonical_schema(&d.sheets_schema,workbook_codec_table_find("sheets"))&&canonical_schema(&d.objects_schema,workbook_codec_table_find("objects"))&&canonical_schema(&d.queries_schema,workbook_codec_table_find("queries")),"Owner canonical schemas failed");
        CHECK(queries_begin(),"Owner query Header begin failed");d.phase=406;
    }
    if(d.phase==406){d.phase=407;return queries_send_family(WORKBOOK_CODEC_META,false);}
    if(d.phase==407){CHECK(d.ready&&!d.error,d.error?d.error:"Owner meta result absent");CHECK(queries_feed_base(WORKBOOK_CODEC_META),"Owner meta feed failed");clear_input();d.phase=408;return queries_send_family(WORKBOOK_CODEC_SHEETS,false);}
    if(d.phase==408){
        CHECK(d.ready&&!d.error,d.error?d.error:"Owner sheets result absent");CHECK(queries_feed_base(WORKBOOK_CODEC_SHEETS),"Owner sheets feed failed");clear_input();
        CHECK(workbook_codec_read_add_objects(d.read,true,&d.error),d.error?d.error:"Owner descriptor continuation failed");d.phase=409;return queries_send_family(WORKBOOK_CODEC_OBJECTS,false);
    }
    if(d.phase==409){
        CHECK(d.ready&&!d.error,d.error?d.error:"Owner descriptor result absent");CHECK(queries_feed_objects(),"Complete owner registry feed failed");clear_input();
        CHECK(workbook_codec_read_add_queries(d.read,true,&d.error),d.error?d.error:"Owner query continuation failed");queries.pending_owned=workbook_codec_read_owned_bytes(d.read);
        if(d.fault==7){d.phase=411;return queries_send(dx_strdup("DESCRIBE SELECT sheet_id,object_id::VARCHAR AS object_id,query FROM _duxcel_workspace.queries"),NULL,256,false,0);}
        d.phase=410;return queries_send_family(WORKBOOK_CODEC_QUERIES,d.fault==8&&!queries.held_released);
    }
    if(d.phase==411){
        CHECK(d.ready&&!d.error,d.error?d.error:"Wrong original query schema DESCRIBE absent");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),"Malformed query-type DESCRIBE did not decode");
        CHECK(queries_observe(schema.bytes),"Rejected query schema exceeds admission");bool rejected=!schema_matches(&schema,workbook_codec_table_find("queries"),3);page_free(&schema);
        CHECK(rejected,"Original VARCHAR query owner was accepted as UBIGINT");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
        CHECK(request&&request->family==WORKBOOK_CODEC_QUERIES&&!request->first_row&&workbook_codec_read_owned_bytes(d.read)==queries.pending_owned,"Wrong query schema advanced/mutated candidate");
        CHECK(queries_prior_unchanged(),"Wrong query source schema changed old Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Wrong-schema query candidate replaced old Header");
        d.rejections++;queries.schema_rejections++;queries.proofs|=256;CHECK(queries_restart(),"Original schema rejection recovery failed");d.phase=406;return true;
    }
    if(d.phase==410){
        if(d.fault==8&&!queries.held_released){
            if(!d.held_ready)return true;CHECK(!d.ready,"Held query result entered C before release");d.cookie++;cancel_candidate();d.waiting=false;queries.held_released=true;
            unsigned stale=d.stale;release_held();CHECK(d.stale==stale+1&&!d.ready&&queries_prior_unchanged(),"Superseded query callback accessed/published borrowed wire");return queries_send_family(WORKBOOK_CODEC_QUERIES,false);
        }
        CHECK(d.ready&&!d.error,d.error?d.error:"Owner query result absent");CHECK(queries_feed_result(),"Actual query feed failed");
        if(!d.fault&&(d.fixture==1||d.fixture==2))return queries_next_fixture();
        if(d.fault>=1&&d.fault<=6){CHECK(queries_restart(),"Query semantic/budget recovery failed");d.phase=406;return true;}
        clear_input();CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete query Header could not move");
        CHECK(queries_header_contents(d.header,queries_versions[d.fixture],&d.key),"Completed owned query Header differs from native fixture");d.headers++;
        if(!d.fault){
            if(d.fixture<3)return queries_next_fixture();d.fault=1;CHECK(queries_begin(),"Query negative candidate begin failed");d.phase=406;return true;
        }
        if(d.fault==8){CHECK(queries_restart(),"Final query recovery begin failed");d.phase=406;return true;}
        CHECK(d.fault==9&&d.fixtures==4&&d.headers==4&&d.queries==69&&d.rejections==9&&d.stale==1&&d.meta_queries==13&&d.sheets_queries==13&&queries.object_queries==13&&queries.query_queries==13&&d.describe_queries==17,"Query terminal metadata workload changed");
        CHECK(queries.object256==104&&queries.object_groups==13&&queries.query256==56&&queries.query64==4&&queries.groups==4&&queries.rejected_feeds==8&&queries.schema_rejections==1&&queries.physical_nulls==2&&queries.poisoned==4&&queries.proofs==511,"Query complete/prefix/rejection/proof inventory changed");
        const unsigned expected[]={0,0,0,0,2048,0};for(size_t i=0;i<6;i++)CHECK(queries.rejected_first[i]==expected[i],"Synthetic query failure prefix changed");for(size_t i=0;i<4;i++)CHECK(d.normal_queries[i]==8,"Normal query fixture workload changed");
        CHECK(queries.physical_first[0]==2048&&queries.physical_first[1]==2048,"Physical invalid-query failure lost its accepted prefix");
        d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
    }
    if(d.phase==9&&d.fixture_ready){
        clear_input();page_free(&d.meta_schema);page_free(&d.sheets_schema);page_free(&d.objects_schema);page_free(&d.queries_schema);workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;
        d.done=true;printf("PASS: browser-codec-queries %u checks,4 current native files,4 Headers,69 queries,8 codec/1 schema rejections,1 stale callback; raw definitions only, no Workbook publication\n",d.checks);queries_report(true,NULL);emscripten_cancel_main_loop();return true;
    }
    return true;
}
static void queries_tick(void) {
    if(d.phase==410&&d.fault==8&&d.held_ready&&!queries.held_released)d.waiting=false;queries_tick_body();
}
