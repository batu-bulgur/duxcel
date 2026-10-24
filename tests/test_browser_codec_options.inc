/* Complete owner source_options through real official-Worker results. Queries
   remain owned raw definitions; this never constructs or publishes a Workbook. */
typedef struct {
    unsigned object_queries,option_queries,query_queries,object256,option256,option64,query256,query64;
    unsigned object_groups,option_groups,query_groups,rejected_feeds,schema_rejections,proofs,prior_version,pod_poisoned,query_poisoned;
    unsigned rejected_first[6];
    size_t pending_owned;
    const char *prior_primary,*prior_table,*detached_primary,*detached_table,*pending_sql;
    bool held_released;
} OptionsState;
static OptionsState options;
static size_t options_retained(void) {return sizeof(options);}
static bool options_observe(size_t extra) {
    size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
    CHECK(current<=INPUT_LIMIT&&extra<=INPUT_LIMIT-current,"Concrete source-options C buffers exceed admission");
    if(current+extra>d.peak_input)d.peak_input=current+extra;return true;
}
EM_JS(void, report_options,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned requests,unsigned rejections,unsigned stale,size_t peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned objects,unsigned source_options,unsigned definitions,unsigned describe,unsigned object256,unsigned option256,unsigned option64,unsigned query256,unsigned query64,unsigned object_groups,unsigned option_groups,unsigned query_groups,unsigned rejected,unsigned schema_rejections,unsigned proofs,unsigned pod_poisoned,unsigned query_poisoned,const unsigned *first_rows,const char *error),{
    const at=normal>>>2,rows=first_rows>>>2;
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'source-options',checks,fixtures,headers,queries:requests,rejections,stale,
        peakOwnedInput:peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:Array.from(HEAPU32.subarray(at,at+2)),familyExecutions:{meta,sheets,objects,sourceOptions:source_options,queries:definitions,describe},
        acceptedFeeds:{objects256:object256,sourceOptions256:option256,sourceOptions64:option64,queries256:query256,queries64:query64},
        objectSplitGroups:object_groups,sourceOptionsSplitGroups:option_groups,validSplitGroups:query_groups,
        rejectedFeeds:rejected,schemaRejections:schema_rejections,rejectedFirstRows:Array.from(HEAPU32.subarray(rows,rows+6)),
        podPoisonedResults:pod_poisoned,poisonedQueryResults:query_poisoned,
        requiredNullRejected:Boolean(proofs&1),duplicateFalseRejected:Boolean(proofs&2),wrongKindRejected:Boolean(proofs&4),foreignOwnerRejected:Boolean(proofs&8),
        sentinelRejected:Boolean(proofs&16),budgetRejected:Boolean(proofs&32),originalTypeRejected:Boolean(proofs&64),
        error:error?UTF8ToString(error):null,boundary:'official-worker/shared-C-meta-sheets-object-source-options-query Header; owned POD/raw definitions only, no Table Analysis, source execution, Workbook publication or browser Save/Open'});
})
static void options_report(bool ok,const char *error) {
    report_options(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.peak_input,d.normal_queries,
        d.meta_queries,d.sheets_queries,options.object_queries,options.option_queries,options.query_queries,d.describe_queries,
        options.object256,options.option256,options.option64,options.query256,options.query64,options.object_groups,options.option_groups,options.query_groups,
        options.rejected_feeds,options.schema_rejections,options.proofs,options.pod_poisoned,options.query_poisoned,options.rejected_first,error);
}
static bool options_header_contents(const WorkbookCodecHeader *h,unsigned version,const WorkbookCodecKey *expected) {
    const WorkbookCodecKey *key=workbook_codec_header_key(h);
    CHECK(h&&workbook_codec_header_objects_ready(h)&&workbook_codec_header_queries_ready(h)&&workbook_codec_header_source_options_ready(h)&&
        !workbook_codec_header_sorts_ready(h)&&!workbook_codec_header_preferences_ready(h)&&workbook_codec_header_sheet_count(h)==64&&workbook_codec_header_active_sheet(h)==63,"Source-options Header readiness/count/active changed");
    CHECK(workbook_codec_header_version(h)==version&&workbook_codec_header_user_scale(h)==1.25f,"Source-options package version/scale changed");
    CHECK(key&&key->document_epoch==expected->document_epoch&&key->operation_ticket==expected->operation_ticket&&key->snapshot_generation==expected->snapshot_generation,"Source-options Header key changed");
    for(size_t sheet=0;sheet<64;sheet++){
        char name[96];snprintf(name,sizeof(name),"Sorted 'İ' %zu",sheet);CHECK(same(workbook_codec_header_name(h,sheet),name),"Source-options worksheet UTF-8/name changed");
        const Analysis *a=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);const char *text=NULL;bool value=false;
        CHECK(a&&a->kind==SOURCE_CSV&&a->path&&!*a->path&&same(workbook_codec_header_reference(h,sheet),"missing-source.csv")&&same(a->schema,"")&&same(a->table,"NULL"),"Saved source reference/unavailable locator changed");
        CHECK(!a->source_version&&a->unique_rows&&!a->filter_column&&!a->filter_value&&!a->filter_count&&!a->group_column&&!a->measure_column&&!a->sort_column&&!a->descending&&!a->sort_count,"Source-options Header fabricated or dropped source state");
        CHECK(workbook_codec_header_unique_rows(h,sheet,0,&value)&&value,"Primary owned unique_rows changed after input retirement");
        CHECK(workbook_codec_header_query(h,sheet,0,&text)&&a->query==text&&queries_expected_text(text,sheet,0),"Primary query/getter raw ownership changed");
        CHECK(v&&v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==1.5f&&!v->columns&&!v->column_count,"Source-options viewport changed");
        CHECK(workbook_codec_header_object_count(h,sheet)==32,"Source-options Table registry count changed");
        for(size_t index=0;index<32;index++){
            WorkbookCodecObjectView object={0};CHECK(workbook_codec_header_object_at(h,sheet,index,&object),"Source-options Table descriptor missing");
            if(index)snprintf(name,sizeof(name),"Object 'İ' %zu:%zu",sheet,index);else snprintf(name,sizeof(name),"NULL");
            CHECK(object.id==queries_expected_id(sheet,index)&&same(object.name,name)&&object.kind==OBJECT_TABLE&&object.mark==MARK_BAR&&!object.chart_measure&&!object.source_id,"Exact Table ID/name/representation changed");
            CHECK(object.row==10+index*12&&object.column==8&&object.height==8&&object.width==4,"Exact Table geometry changed");
            value=false;CHECK(workbook_codec_header_unique_rows(h,sheet,object.id,&value)&&value,"Complete Table-owned option is missing or borrowed");
            CHECK(workbook_codec_header_query(h,sheet,object.id,&text)&&queries_expected_text(text,sheet,index+1),"Complete Table-owned query is missing");
        }
    }
    for(unsigned initial=0;initial<2;initial++){
        bool value=initial!=0;
        CHECK(!workbook_codec_header_unique_rows(h,64,0,&value)&&value==(initial!=0),"Missing-sheet option getter modified output");
        CHECK(!workbook_codec_header_unique_rows(h,0,UINT64_MAX,&value)&&value==(initial!=0),"Foreign MAX option resolved across worksheets");
        CHECK(!workbook_codec_header_unique_rows(h,0,1,&value)&&value==(initial!=0),"Dangling option getter modified output");
        CHECK(!workbook_codec_header_unique_rows(NULL,0,0,&value)&&value==(initial!=0),"NULL/incomplete option getter modified output");
    }
    const char *untouched="untouched",*text=untouched;
    CHECK(!workbook_codec_header_source_options_ready(NULL)&&!workbook_codec_header_unique_rows(h,0,0,NULL),"Invalid option readiness/output accepted");
    CHECK(!workbook_codec_header_query(h,64,0,&text)&&text==untouched&&!workbook_codec_header_query(h,0,UINT64_MAX,&text)&&text==untouched,"Invalid query getter modified output");return true;
}
static bool options_prior_unchanged(void) {
    CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes,"Old independent source-options Header ownership changed");
    if(!d.header)return true;
    const char *primary=NULL,*table=NULL;
    CHECK(workbook_codec_header_query(d.header,0,0,&primary)&&primary==options.prior_primary&&workbook_codec_header_query(d.header,63,UINT64_MAX,&table)&&table==options.prior_table,"Prior query allocation identities changed");
    return options_header_contents(d.header,options.prior_version,&d.prior_key);
}
static bool options_begin(void) {
    options.prior_version=workbook_codec_header_version(d.header);options.prior_primary=NULL;options.prior_table=NULL;
    if(d.header)CHECK(workbook_codec_header_query(d.header,0,0,&options.prior_primary)&&workbook_codec_header_query(d.header,63,UINT64_MAX,&options.prior_table),"Prior owned queries unavailable before replacement");
    return begin_read();
}
static bool options_send(char *sql,const Page *schema,unsigned limit,bool hold,unsigned policy) {
    sorts_wire=false;objects_wire=policy==2;queries_wire=policy==3&&schema==&d.queries_schema;options_wire=policy==3&&schema==&d.options_schema;
    return send_sql(sql,schema,limit,hold);
}
static bool options_send_family(WorkbookCodecFamily family,bool hold) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==family&&!request->first_row,"Unexpected/reexecuted source-options family cursor");
    if(family==WORKBOOK_CODEC_META||family==WORKBOOK_CODEC_SHEETS){
        const Page *schema=family==WORKBOOK_CODEC_META?&d.meta_schema:&d.sheets_schema;if(family==WORKBOOK_CODEC_META)d.meta_queries++;else d.sheets_queries++;
        return options_send(browser_page_sql(request->sql,schema,0,&d.error),schema,(unsigned)request->row_limit,false,0);
    }
    bool objects=family==WORKBOOK_CODEC_OBJECTS,flags=family==WORKBOOK_CODEC_SOURCE_OPTIONS;
    CHECK(objects||flags||family==WORKBOOK_CODEC_QUERIES,"Unsupported source-options family");
    CHECK(request->row_limit==(objects?2049u:2113u)&&request->column_count==(objects?11u:3u)&&request->source_column_count==request->column_count,"Source-options envelope changed");
    char *relation=dx_strdup(request->sql),*next=NULL;
    if(objects&&d.fault==3)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND id=9007199254740993::UBIGINT THEN %d::INTEGER ELSE kind END AS kind) FROM (%s) AS _options_wrong_kind",OBJECT_PIVOT,relation);
    if(flags){
        CHECK(!strcmp(relation,"SELECT sheet_id,object_id,unique_rows FROM _duxcel_workspace.source_options ORDER BY sheet_id,object_id LIMIT 2113"),"C source-options projection changed");
        if(d.fault==1)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=63 AND object_id=18446744073709551615::UBIGINT THEN NULL::BOOLEAN ELSE unique_rows END AS unique_rows) FROM (%s) AS _options_null",relation);
        else if(d.fault==2)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND object_id=9007199254740993::UBIGINT THEN 0::UBIGINT ELSE object_id END AS object_id,CASE WHEN sheet_id=0 AND object_id IN (0::UBIGINT,9007199254740993::UBIGINT) THEN false ELSE unique_rows END AS unique_rows) FROM (%s) AS _options_duplicate_false ORDER BY sheet_id,object_id",relation);
        else if(d.fault==4)next=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=0 AND object_id=9007199254740993::UBIGINT THEN 18446744073709551615::UBIGINT ELSE object_id END AS object_id) FROM (%s) AS _options_foreign ORDER BY sheet_id,object_id",relation);
        else if(d.fault==5)next=dx_printf("SELECT * FROM ((%s) UNION ALL SELECT 64::INTEGER AS sheet_id,0::UBIGINT AS object_id,true AS unique_rows) AS _options_sentinel ORDER BY sheet_id,object_id LIMIT 2113",relation);
    }
    if(!objects&&!flags)CHECK(strstr(relation,"CASE WHEN query IS NULL OR octet_length(encode(query))>65536 OR length(trim(query))=0 OR contains(query,chr(0)) THEN NULL::VARCHAR ELSE query END AS query"),"Native query-text guard changed");
    if(next){CHECK(options_observe(text_bytes(relation)+text_bytes(next)),"Source-options fault SQL coexistence exceeds admission");free(relation);relation=next;}
    char *sql=objects?dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(id AS VARCHAR) AS id,CAST(name AS VARCHAR) AS name,CAST(row_id AS VARCHAR) AS row_id,CAST(column_id AS VARCHAR) AS column_id,CAST(height AS VARCHAR) AS height,CAST(width AS VARCHAR) AS width,CAST(kind AS VARCHAR) AS kind,CAST(mark AS VARCHAR) AS mark,CAST(source_id AS VARCHAR) AS source_id,CAST(chart_measure AS VARCHAR) AS chart_measure FROM (%s) AS _options_objects_projection",relation):
        flags?dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(object_id AS VARCHAR) AS object_id,CAST(unique_rows AS VARCHAR) AS unique_rows FROM (%s) AS _options_projection",relation):
        dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(object_id AS VARCHAR) AS object_id,CAST(query AS VARCHAR) AS query FROM (%s) AS _options_query_projection",relation);
    CHECK(sql&&strlen(sql)<=DX_BROWSER_SQL_BYTES&&options_observe(text_bytes(relation)+text_bytes(sql)),"Source-options canonical projection exceeds admission");free(relation);
    if(objects)options.object_queries++;else if(flags)options.option_queries++;else options.query_queries++;
    return options_send(sql,objects?&d.objects_schema:flags?&d.options_schema:&d.queries_schema,objects?2049:2113,hold,objects?2:3);
}
static bool options_atoms(const WorkbookCodecTable *table,const Page *schema,size_t rows,bool pending) {
    CHECK(d.page.column_count==schema->column_count&&d.page.row_count==rows&&d.page.offset==0&&d.page.total==rows&&d.page.count_pending==pending,"Source-options dimensions/completeness changed");
    CHECK(!d.atoms,"Prior source-options atoms not retired");size_t bytes=rows*schema->column_count*sizeof(*d.atoms);CHECK(options_observe(bytes),"Source-options atom staging exceeds admission");
    d.atoms=dx_alloc(bytes);d.atom_bytes=bytes;const char *types=table->types;
    for(size_t column=0;column<schema->column_count;column++){
        while(*types==' ')types++;WorkbookCodecAtomKind kind=atom_kind(*types++);const Column *actual=d.page.columns+column,*expected=schema->columns+column;
        CHECK(same(actual->name,expected->name)&&same(actual->type_name,expected->type_name)&&actual->type==expected->type&&actual->number_type.kind==expected->number_type.kind&&actual->number_type.precision==expected->number_type.precision&&actual->number_type.scale==expected->number_type.scale,"Source-options original-type sidecar changed");
        d.columns[column]=(WorkbookCodecColumn){actual->name,kind};
        for(size_t row=0;row<rows;row++){
            const Cell *cell=d.page.cells+row*schema->column_count+column;WorkbookCodecAtom *atom=d.atoms+row*schema->column_count+column;atom->kind=cell->is_null?WORKBOOK_CODEC_NULL:kind;
            if(cell->is_null)continue;CHECK(cell->text,"Source-options exact atom has no text");
            if(kind==WORKBOOK_CODEC_I32)CHECK(i32(cell->text,&atom->value.i32),"Source-options INTEGER was not canonical/exact");
            else if(kind==WORKBOOK_CODEC_U64)CHECK(u64(cell->text,&atom->value.u64),"Source-options UBIGINT was not canonical/exact");
            else if(kind==WORKBOOK_CODEC_DOUBLE){char *end=NULL;atom->value.real=strtod(cell->text,&end);CHECK(end&&end!=cell->text&&!*end,"Source-options/base DOUBLE invalid");}
            else if(kind==WORKBOOK_CODEC_BOOLEAN){CHECK(!strcmp(cell->text,"true")||!strcmp(cell->text,"false"),"Source-options BOOLEAN text invalid");atom->value.boolean=!strcmp(cell->text,"true");}
            else atom->value.text=(WorkbookCodecText){cell->text,strlen(cell->text)};
        }
    }
    return true;
}
static bool options_feed_base(WorkbookCodecFamily family) {
    bool meta=family==WORKBOOK_CODEC_META;CHECK(options_atoms(workbook_codec_table_find(meta?"meta":"sheets"),meta?&d.meta_schema:&d.sheets_schema,meta?1:64,false),"Source-options base atoms failed");
    WorkbookCodecBatch batch={d.columns,d.page.column_count,d.atoms,d.page.row_count,0,true};CHECK(options_observe(0),"Source-options base retained admission failed");
    CHECK(workbook_codec_read_feed(d.read,d.key,family,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Source-options base feed rejected");return true;
}
static bool options_poison_column(size_t column) {
    for(size_t row=0;row<d.page.row_count;row++){Cell *cell=d.page.cells+row*d.page.column_count+column;CHECK(!cell->is_null&&cell->text,"Owned input missing before poison");memset(cell->text,'~',strlen(cell->text));}return true;
}
static bool options_feed_objects(void) {
    CHECK(options_atoms(workbook_codec_table_find("objects"),&d.objects_schema,2048,false),"Source-options object atoms failed");
    for(size_t first=0;first<2048;first+=256){
        const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_OBJECTS&&request->first_row==first,"Source-options descriptor chunks reran or lost cursor");
        WorkbookCodecBatch batch={d.columns,11,d.atoms+first*11,256,first,first+256==2048};CHECK(options_observe(0),"Source-options object feed exceeds admission");
        CHECK(workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_OBJECTS,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid source-options descriptor prefix rejected");options.object256++;
    }
    options.object_groups++;return options_poison_column(2);
}
static bool options_feed_flags(void) {
    bool reject=d.fault>=1&&d.fault<=6,sentinel=d.fault==5;size_t rows=sentinel?2113:2112;
    CHECK(options_atoms(workbook_codec_table_find("source_options"),&d.options_schema,rows,sentinel),"Source-options exact flag atoms failed");
    if(d.fault==1)CHECK(d.atoms[2111*3].value.i32==63&&d.atoms[2111*3+1].value.u64==UINT64_MAX&&d.atoms[2111*3+2].kind==WORKBOOK_CODEC_NULL,"Real last-owner NULL did not survive BOOLEAN sidecar");
    if(d.fault==2)CHECK(d.atoms[0].value.i32==0&&d.atoms[1].value.u64==0&&!d.atoms[2].value.boolean&&d.atoms[3].value.i32==0&&d.atoms[4].value.u64==0&&!d.atoms[5].value.boolean,"Actual duplicate-false rows were not both false");
    if(d.fault==6){size_t current=retained()+workbook_codec_read_owned_bytes(d.read);CHECK(current<INPUT_LIMIT-65536u,"Source-options pressure fixture has no headroom");d.pressure_bytes=INPUT_LIMIT-current-65536u;d.pressure=dx_alloc(d.pressure_bytes);memset(d.pressure,0,d.pressure_bytes);CHECK(options_observe(0),"Concrete source-options pressure exceeds input admission");}
    unsigned accepted=0;size_t failed_row=d.fault==1||sentinel?2048:0;
    for(size_t first=0;first<rows;first+=256){
        size_t count=rows-first<256?rows-first:256;const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
        CHECK(request&&request->family==WORKBOOK_CODEC_SOURCE_OPTIONS&&request->first_row==first,"Source-options chunks do not follow one immutable result");size_t owned=workbook_codec_read_owned_bytes(d.read);const char *sql=request->sql;
        WorkbookCodecBatch batch={d.columns,3,d.atoms+first*3,count,first,first+count==rows&&!d.page.count_pending};CHECK(options_observe(0),"Source-options feed coexistence exceeds admission");
        WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_SOURCE_OPTIONS,&batch,retained(),&d.error);
        if(reject&&first==failed_row){
            static const char *reasons[]={"SQL NULL","strictly ordered","existing source owner","existing source owner","Too many source options","budget"};
            CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error&&strstr(d.error,reasons[d.fault-1]),d.error?d.error:"Expected source-options rejection missing");
            CHECK(workbook_codec_read_owned_bytes(d.read)==owned&&workbook_codec_read_request(d.read)->first_row==first&&workbook_codec_read_request(d.read)->sql==sql,"Failed source-options batch changed cursor/bytes/SQL ownership");
            CHECK(options_prior_unchanged(),"Failed source-options batch changed old Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Failed source-options candidate replaced old Header");CHECK(options_observe(0),"Source-options diagnostic exceeds admission");
            options.rejected_first[d.fault-1]=(unsigned)first;options.proofs|=1u<<(d.fault-1);d.rejections++;options.rejected_feeds++;return true;
        }
        CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid source-options prefix rejected");CHECK(workbook_codec_read_owned_bytes(d.read)==owned,"Boolean source-options feed allocated persistent state");
        if(count==256){options.option256++;accepted++;}else{CHECK(count==64&&!reject,"Valid source-options tail must contain64 rows");options.option64++;}
    }
    CHECK(!reject&&accepted==8,"Complete source-options result did not use eight256-row prefixes");
    for(size_t row=0;row<rows;row++){CHECK(d.atoms[row*3+2].kind==WORKBOOK_CODEC_BOOLEAN&&d.atoms[row*3+2].value.boolean,"Valid native writer option must be true");d.atoms[row*3+2].value.boolean=false;}
    for(size_t column=0;column<3;column++)CHECK(options_poison_column(column),"Owned source-options Page poison failed");
    options.option_groups++;options.pod_poisoned++;return true;
}
static bool options_feed_queries(void) {
    CHECK(options_atoms(workbook_codec_table_find("queries"),&d.queries_schema,2112,false),"Source-options owned-query atoms failed");unsigned accepted=0;
    for(size_t first=0;first<2112;first+=256){
        size_t count=2112-first<256?2112-first:256;const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
        CHECK(request&&request->family==WORKBOOK_CODEC_QUERIES&&request->first_row==first,"Source-options queries reran or lost cursor");
        WorkbookCodecBatch batch={d.columns,3,d.atoms+first*3,count,first,first+count==2112};CHECK(options_observe(0),"Source-options query retained admission failed");
        CHECK(workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_QUERIES,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid source-options owned-query prefix rejected");
        if(count==256){options.query256++;accepted++;}else{CHECK(count==64,"Valid owned-query tail must contain64 rows");options.query64++;}
    }
    CHECK(accepted==8,"Owned-query result did not use eight256-row prefixes");options.query_groups++;options.query_poisoned++;return options_poison_column(2);
}
static bool options_restart(void) {clear_input();d.fault++;objects_wire=false;queries_wire=false;options_wire=false;return options_begin();}
static void options_free_schemas(void) {page_free(&d.meta_schema);page_free(&d.sheets_schema);page_free(&d.objects_schema);page_free(&d.options_schema);page_free(&d.queries_schema);}
static bool options_tick_body(void) {
    if(d.phase==0){
        CHECK(d.fixture<2,"Source-options fixture index exceeds inventory");d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;
        char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",options_files[d.fixture]);fixture_start(d.cookie,d.ticket,options_files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=501;return true;
    }
    if(d.phase==501){if(!d.fixture_ready)return true;d.phase=502;return options_send(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false,0);}
    if(d.waiting)return true;
    if(d.phase>=502&&d.phase<=506){
        CHECK(d.ready&&!d.error,d.error?d.error:"Original source-options DESCRIBE missing");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Original source-options schema invalid");CHECK(options_observe(schema.bytes),"Original source-options schema exceeds admission");
        const char *name=d.phase==502?"meta":d.phase==503?"sheets":d.phase==504?"objects":d.phase==505?"source_options":"queries";
        if(!source_schema(&schema,workbook_codec_table_find(name),d.phase==503?25:d.phase==504?11:3)){page_free(&schema);return false;}
        Page *destination=d.phase==502?&d.meta_schema:d.phase==503?&d.sheets_schema:d.phase==504?&d.objects_schema:d.phase==505?&d.options_schema:&d.queries_schema;page_free(destination);*destination=schema;clear_input();
        if(d.phase<506){d.phase++;const char *next=d.phase==503?"sheets":d.phase==504?"objects":d.phase==505?"source_options":"queries";return options_send(dx_printf("DESCRIBE SELECT * FROM _duxcel_workspace.%s",next),NULL,256,false,0);}
        CHECK(canonical_schema(&d.meta_schema,workbook_codec_table_find("meta"))&&canonical_schema(&d.sheets_schema,workbook_codec_table_find("sheets"))&&canonical_schema(&d.objects_schema,workbook_codec_table_find("objects"))&&canonical_schema(&d.options_schema,workbook_codec_table_find("source_options"))&&canonical_schema(&d.queries_schema,workbook_codec_table_find("queries")),"Source-options canonical schemas failed");
        CHECK(options_begin(),"Source-options Header begin failed");d.phase=507;
    }
    if(d.phase==507){d.phase=508;return options_send_family(WORKBOOK_CODEC_META,false);}
    if(d.phase==508){CHECK(d.ready&&!d.error,d.error?d.error:"Source-options meta result absent");CHECK(options_feed_base(WORKBOOK_CODEC_META),"Source-options meta feed failed");clear_input();d.phase=509;return options_send_family(WORKBOOK_CODEC_SHEETS,false);}
    if(d.phase==509){CHECK(d.ready&&!d.error,d.error?d.error:"Source-options sheets result absent");CHECK(options_feed_base(WORKBOOK_CODEC_SHEETS),"Source-options sheets feed failed");clear_input();CHECK(workbook_codec_read_add_objects(d.read,true,&d.error),d.error?d.error:"Source-options descriptor continuation failed");d.phase=510;return options_send_family(WORKBOOK_CODEC_OBJECTS,false);}
    if(d.phase==510){
        CHECK(d.ready&&!d.error,d.error?d.error:"Source-options objects absent");CHECK(options_feed_objects(),"Source-options descriptor feed failed");clear_input();CHECK(workbook_codec_read_add_source_options(d.read,true,&d.error),d.error?d.error:"Source-options continuation failed");options.pending_owned=workbook_codec_read_owned_bytes(d.read);options.pending_sql=workbook_codec_read_request(d.read)->sql;
        if(d.fault==7){d.phase=513;return options_send(dx_strdup("DESCRIBE SELECT sheet_id,object_id,unique_rows::VARCHAR AS unique_rows FROM _duxcel_workspace.source_options"),NULL,256,false,0);}
        d.phase=511;return options_send_family(WORKBOOK_CODEC_SOURCE_OPTIONS,d.fault==8&&!options.held_released);
    }
    if(d.phase==513){
        CHECK(d.ready&&!d.error,d.error?d.error:"Wrong original BOOLEAN DESCRIBE absent");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),"Wrong options type DESCRIBE did not decode");CHECK(options_observe(schema.bytes),"Rejected options schema exceeds admission");bool rejected=!schema_matches(&schema,workbook_codec_table_find("source_options"),3);page_free(&schema);
        CHECK(rejected,"Original VARCHAR unique_rows was accepted as BOOLEAN");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
        CHECK(request&&request->family==WORKBOOK_CODEC_SOURCE_OPTIONS&&!request->first_row&&request->sql==options.pending_sql&&workbook_codec_read_owned_bytes(d.read)==options.pending_owned,"Wrong original BOOLEAN schema changed candidate cursor/ownership");
        CHECK(options_prior_unchanged(),"Wrong options schema changed prior Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Wrong options schema replaced Header");d.rejections++;options.schema_rejections++;options.proofs|=64;CHECK(options_restart(),"Options schema rejection recovery failed");d.phase=507;return true;
    }
    if(d.phase==511){
        if(d.fault==8&&!options.held_released){if(!d.held_ready)return true;CHECK(!d.ready,"Held options result entered C before release");d.cookie++;cancel_candidate();d.waiting=false;options.held_released=true;unsigned stale=d.stale;release_held();CHECK(d.stale==stale+1&&!d.ready&&options_prior_unchanged(),"Superseded options callback accessed/published borrowed wire");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_SOURCE_OPTIONS&&!request->first_row&&request->sql==options.pending_sql&&workbook_codec_read_owned_bytes(d.read)==options.pending_owned,"Stale options reply changed live candidate ownership");return options_send_family(WORKBOOK_CODEC_SOURCE_OPTIONS,false);}
        CHECK(d.ready&&!d.error,d.error?d.error:"Source-options result absent");CHECK(options_feed_flags(),"Actual source-options feed failed");
        if(d.fault>=1&&d.fault<=6){CHECK(options_restart(),"Source-options rejection recovery failed");d.phase=507;return true;}
        clear_input();CHECK(workbook_codec_read_add_queries(d.read,true,&d.error),d.error?d.error:"Owned-query continuation after options failed");d.phase=512;return options_send_family(WORKBOOK_CODEC_QUERIES,false);
    }
    if(d.phase==512){
        CHECK(d.ready&&!d.error,d.error?d.error:"Owned-query result after options absent");CHECK(options_feed_queries(),"Actual owned queries after options failed");clear_input();CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete options/query Header could not move");
        CHECK(options_header_contents(d.header,options_versions[d.fixture],&d.key),"Owned source-options Header differs from native fixture");d.headers++;
        if(!d.fault){d.fixture++;if(d.fixture<2){options_free_schemas();d.phase=0;return true;}d.fixture=1;d.fault=1;CHECK(options_begin(),"Options negative candidate begin failed");d.phase=507;return true;}
        if(d.fault==8){CHECK(options_restart(),"Final options recovery begin failed");d.phase=507;return true;}
        CHECK(d.fault==9&&d.fixtures==2&&d.headers==4&&d.queries==59&&d.rejections==7&&d.stale==1&&d.meta_queries==11&&d.sheets_queries==11&&options.object_queries==11&&options.option_queries==11&&options.query_queries==4&&d.describe_queries==11,"Source-options terminal workload changed");
        CHECK(options.object256==88&&options.object_groups==11&&options.option256==48&&options.option64==4&&options.option_groups==4&&options.query256==32&&options.query64==4&&options.query_groups==4&&options.rejected_feeds==6&&options.schema_rejections==1&&options.pod_poisoned==4&&options.query_poisoned==4&&options.proofs==127,"Source-options feed/ownership/rejection proof inventory changed");
        const unsigned expected[]={2048,0,0,0,2048,0};for(size_t index=0;index<6;index++)CHECK(options.rejected_first[index]==expected[index],"Source-options failure prefix changed");for(size_t index=0;index<2;index++)CHECK(d.normal_queries[index]==10,"Normal source-options workload changed");
        CHECK(workbook_codec_header_query(d.header,0,0,&options.detached_primary)&&workbook_codec_header_query(d.header,63,UINT64_MAX,&options.detached_table),"Owned query pointers missing before catalog close");
        d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
    }
    if(d.phase==9&&d.fixture_ready){
        clear_input();CHECK(options_header_contents(d.header,options_versions[1],&d.key),"Owned definitions changed after Page/catalog close");const char *primary=NULL,*table=NULL;
        CHECK(workbook_codec_header_query(d.header,0,0,&primary)&&primary==options.detached_primary&&workbook_codec_header_query(d.header,63,UINT64_MAX,&table)&&table==options.detached_table,"Owned query allocations changed after catalog close");
        options_free_schemas();workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;
        d.done=true;printf("PASS: browser-codec-source-options %u checks,2 current native files,4 Headers,59 queries,6 codec/1 schema rejections,1 stale callback; owned options/raw definitions only, no Workbook publication\n",d.checks);options_report(true,NULL);emscripten_cancel_main_loop();return true;
    }
    return true;
}
static void options_tick(void) {if(d.phase==511&&d.fault==8&&d.held_ready&&!options.held_released)d.waiting=false;options_tick_body();}
