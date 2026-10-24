/* Optional official-Worker sorts workflow in the existing no-window driver.
   Transport and exact atom conversion are shared with the header workflow;
   this mode never constructs a Workbook or executes a stored source query. */
typedef struct {
    unsigned queries,accepted_256,accepted_192,rejected_feeds,valid_splits,proofs;
    bool held_released;
} SortsState;
static SortsState sorts;
static size_t sorts_retained(void) {return sizeof(sorts);}
static bool sorts_observe(size_t extra) {
    size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
    CHECK(current<=INPUT_LIMIT&&extra<=INPUT_LIMIT-current,"Concrete coexisting sorts C buffers exceed admission");
    if(current+extra>d.peak_input)d.peak_input=current+extra;return true;
}
EM_JS(void, report_sorts,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned queries,unsigned rejected,unsigned stale,size_t peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned sort_queries,unsigned describes,unsigned first,unsigned last,unsigned rejected_feeds,unsigned pairs,unsigned proofs,const char *error),{
    const at=normal>>>2;
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'sorts',checks,fixtures,headers,queries,rejections:rejected,stale,
        peakOwnedInput:peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:Array.from(HEAPU32.subarray(at,at+2)),familyExecutions:{meta,sheets,sorts:sort_queries,describe:describes},
        acceptedFeeds:{rows256:first,rows192:last},rejectedFeeds:rejected_feeds,validSplitPairs:pairs,
        requiredNullRejected:Boolean(proofs&1),nameNullRejected:Boolean(proofs&2),orderRejected:Boolean(proofs&4),caseDuplicateRejected:Boolean(proofs&8),
        sentinelRejected:Boolean(proofs&16),budgetRejected:Boolean(proofs&32),
        error:error?UTF8ToString(error):null,boundary:'official-worker/shared-C-meta-sheets-sorts Header; no Workbook, owner families, browser Save/Open, missing-table or source execution proof'});
})
static void sorts_report(bool ok,const char *error) {
    report_sorts(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.peak_input,d.normal_queries,d.meta_queries,d.sheets_queries,sorts.queries,d.describe_queries,
        sorts.accepted_256,sorts.accepted_192,sorts.rejected_feeds,sorts.valid_splits,sorts.proofs,error);
}
static bool sorts_prior_unchanged(void) {
    CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes,"Old independent sorts Header ownership changed");
    if(d.header){
        const WorkbookCodecKey *key=workbook_codec_header_key(d.header);const Analysis *last=workbook_codec_header_analysis(d.header,63);
        CHECK(key&&key->document_epoch==d.prior_key.document_epoch&&key->operation_ticket==d.prior_key.operation_ticket&&key->snapshot_generation==d.prior_key.snapshot_generation,"Old sorts Header key changed");
        CHECK(workbook_codec_header_sheet_count(d.header)==64&&workbook_codec_header_sorts_ready(d.header)&&last&&last->sort_count==7&&same(last->sorts[6].column,"Field 'İ' 447"),"Failed/stale sorts candidate mutated old Header");
    }
    return true;
}
static bool sorts_header_expected(void) {
    const WorkbookCodecHeader *h=d.header;const WorkbookCodecKey *key=workbook_codec_header_key(h);
    CHECK(h&&workbook_codec_header_sorts_ready(h)&&workbook_codec_header_sheet_count(h)==64&&workbook_codec_header_active_sheet(h)==63,"Sorts Header count/active/continuation incomplete");
    CHECK(workbook_codec_header_version(h)==sorts_versions[d.fixture]&&workbook_codec_header_user_scale(h)==1.25f,"Actual sorts package version/scale changed");
    CHECK(key&&key->document_epoch==d.key.document_epoch&&key->operation_ticket==d.key.operation_ticket&&key->snapshot_generation==d.key.snapshot_generation,"Sorts Header routing key lost");
    for(size_t sheet=0;sheet<64;sheet++){
        char *name=dx_printf("Sorted 'İ' %zu",sheet);bool match=same(workbook_codec_header_name(h,sheet),name);CHECK(sorts_observe(text_bytes(name)),"Temporary worksheet name exceeds input budget");free(name);CHECK(match,"Exact sorted worksheet name changed");
        const Analysis *a=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);
        CHECK(a&&a->kind==SOURCE_CSV&&a->path&&!*a->path&&same(workbook_codec_header_reference(h,sheet),"missing-source.csv")&&same(a->schema,"")&&same(a->table,"NULL"),"Browser locator/raw source reference identity changed");
        CHECK(!a->query&&!a->source_version&&!a->unique_rows&&!a->filter_column&&!a->filter_value&&!a->filter_count&&!a->group_column&&!a->measure_column,"Sorts Header executed or loaded unrelated source state");
        CHECK(same(a->sort_column,"Primary 'İ'")&&a->descending==(sheet%2!=0)&&a->sort_count==7,"Primary/secondary sort count changed");
        for(size_t position=0;position<7;position++){
            size_t ordinal=sheet*7+position;char *field=dx_printf("Field 'İ' %zu",ordinal);match=same(a->sorts[position].column,field);CHECK(sorts_observe(text_bytes(field)),"Temporary sort name exceeds input budget");free(field);
            CHECK(match&&a->sorts[position].descending==(ordinal%2!=0),"Exact secondary sort order/name/direction lost across256-row split");
        }
        CHECK(v&&v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==1.5f&&!v->columns&&!v->column_count,"Sorts Header viewport/canvas changed");
    }
    return true;
}
static bool sorts_send(char *sql,const Page *schema,unsigned limit,bool hold,bool sorts_policy) {
    sorts_wire=sorts_policy;return send_sql(sql,schema,limit,hold);
}
static bool sorts_send_family(WorkbookCodecFamily family,bool hold) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
    CHECK(request&&request->family==family&&!request->first_row,"Unexpected/repeated sorts workflow family execution");
    if(family!=WORKBOOK_CODEC_SORTS){
        const Page *schema=family==WORKBOOK_CODEC_META?&d.meta_schema:&d.sheets_schema;
        if(family==WORKBOOK_CODEC_META)d.meta_queries++;else d.sheets_queries++;
        return sorts_send(browser_page_sql(request->sql,schema,0,&d.error),schema,(unsigned)request->row_limit,false,false);
    }
    CHECK(request->row_limit==449&&request->column_count==4&&request->source_column_count==4,"Sorts cursor bounded projection changed");
    char *relation=dx_strdup(request->sql);
    if(d.fault==1||d.fault==2||d.fault==4){
        const char *replacement=d.fault==1?"NULL::INTEGER AS sheet_id":d.fault==2?"NULL::VARCHAR AS name":"'pRIMARY ''İ'''::VARCHAR AS name";
        char *next=dx_printf("SELECT * REPLACE (%s) FROM (%s) AS _codec_sorts_bad",replacement,relation);CHECK(sorts_observe(text_bytes(relation)+text_bytes(next)),"Sort replacement SQL exceeds input budget");free(relation);relation=next;
    }else if(d.fault==3){
        char *next=dx_printf("SELECT * FROM (%s) AS _codec_sorts_reverse ORDER BY sheet_id DESC,position",relation);CHECK(sorts_observe(text_bytes(relation)+text_bytes(next)),"Sort order SQL exceeds input budget");free(relation);relation=next;
    }else if(d.fault==5){
        char *next=dx_printf("SELECT * FROM ((%s) UNION ALL SELECT 63::INTEGER AS sheet_id,7::INTEGER AS position,'sentinel'::VARCHAR AS name,false::BOOLEAN AS descending) AS _codec_sorts_sentinel ORDER BY sheet_id,position LIMIT 449",relation);CHECK(sorts_observe(text_bytes(relation)+text_bytes(next)),"Sort sentinel SQL exceeds input budget");free(relation);relation=next;
    }
    // The cursor already supplies LIMIT449. browser_page_sql intentionally
    // remains a normal256-row helper and must not silently truncate this result.
    char *sql=dx_printf("SELECT CAST(sheet_id AS VARCHAR) AS sheet_id,CAST(position AS VARCHAR) AS position,CAST(name AS VARCHAR) AS name,CAST(descending AS VARCHAR) AS descending FROM (%s) AS _codec_sorts_projection",relation);CHECK(sorts_observe(text_bytes(relation)+text_bytes(sql)),"Canonical sort SQL coexistence exceeds input budget");free(relation);
    sorts.queries++;return sorts_send(sql,&d.sorts_schema,449,hold,true);
}
static bool sorts_atoms(const WorkbookCodecTable *table,size_t expected_rows,bool pending) {
    const Page *schema=!strcmp(table->table,"meta")?&d.meta_schema:!strcmp(table->table,"sheets")?&d.sheets_schema:&d.sorts_schema;
    CHECK(d.page.column_count==schema->column_count&&d.page.row_count==expected_rows&&d.page.offset==0&&d.page.total==expected_rows&&d.page.count_pending==pending,"Sorts workflow result dimensions/completeness changed");
    CHECK(!d.atoms,"Prior atom buffer not retired");d.atom_bytes=expected_rows*schema->column_count*sizeof(*d.atoms);d.atoms=dx_alloc(d.atom_bytes);
    const char *types=table->types;
    for(size_t column=0;column<schema->column_count;column++){
        while(*types==' ')types++;WorkbookCodecAtomKind kind=atom_kind(*types++);
        CHECK(same(d.page.columns[column].name,schema->columns[column].name)&&same(d.page.columns[column].type_name,schema->columns[column].type_name),"Canonical sorts original-type sidecar changed");
        d.columns[column]=(WorkbookCodecColumn){d.page.columns[column].name,kind};
        for(size_t row=0;row<expected_rows;row++){
            const Cell *cell=d.page.cells+row*schema->column_count+column;WorkbookCodecAtom *atom=d.atoms+row*schema->column_count+column;
            atom->kind=cell->is_null?WORKBOOK_CODEC_NULL:kind;if(cell->is_null)continue;CHECK(cell->text,"Missing exact sorts raw text");
            if(kind==WORKBOOK_CODEC_I32)CHECK(i32(cell->text,&atom->value.i32),"Invalid exact sort INTEGER text");
            else if(kind==WORKBOOK_CODEC_U64)CHECK(u64(cell->text,&atom->value.u64),"Invalid exact viewport UBIGINT text");
            else if(kind==WORKBOOK_CODEC_DOUBLE){char *end=NULL;atom->value.real=strtod(cell->text,&end);CHECK(end&&end!=cell->text&&!*end,"Invalid exact viewport DOUBLE text");}
            else if(kind==WORKBOOK_CODEC_BOOLEAN){CHECK(!strcmp(cell->text,"true")||!strcmp(cell->text,"false"),"Invalid exact sorts BOOLEAN text");atom->value.boolean=!strcmp(cell->text,"true");}
            else atom->value.text=(WorkbookCodecText){cell->text,strlen(cell->text)};
        }
    }
    return true;
}
static size_t sorts_input(void) {
    size_t input=retained(),all=input+workbook_codec_read_owned_bytes(d.read);
    if(all>d.peak_input)d.peak_input=all;return input;
}
static bool sorts_feed_base(WorkbookCodecFamily family) {
    CHECK(sorts_atoms(workbook_codec_table_find(family==WORKBOOK_CODEC_META?"meta":"sheets"),family==WORKBOOK_CODEC_META?1:64,false),"Base exact atom adaptation failed");
    WorkbookCodecBatch batch={d.columns,d.page.column_count,d.atoms,d.page.row_count,0,true};size_t input=sorts_input();
    CHECK(input+workbook_codec_read_owned_bytes(d.read)<=INPUT_LIMIT,"Base C graph exceeds input bound");
    CHECK(workbook_codec_read_feed(d.read,d.key,family,&batch,input,&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Base metadata feed failed");return true;
}
static bool sorts_feed_result(void) {
    bool reject=d.fault>=1&&d.fault<=6,sentinel=d.fault==5;
    CHECK(sorts_atoms(workbook_codec_table_find("sorts"),sentinel?449:448,sentinel),"Sorts exact atom adaptation failed");
    if(d.fault==6){d.pressure=dx_alloc(HEADER_LIMIT);d.pressure_bytes=HEADER_LIMIT;memset(d.pressure,0,d.pressure_bytes);}
    unsigned accepted=0;
    for(size_t row=0;row<d.page.row_count;row+=256){
        size_t count=d.page.row_count-row<256?d.page.row_count-row:256;
        const WorkbookCodecRequest *before=workbook_codec_read_request(d.read);CHECK(before&&before->family==WORKBOOK_CODEC_SORTS&&before->first_row==row,"Sorts batch continuation changed or SQL rerun");
        size_t bytes=workbook_codec_read_owned_bytes(d.read);const char *sql=before->sql;
        WorkbookCodecBatch batch={d.columns,4,d.atoms+row*4,count,row,row+count==d.page.row_count&&!d.page.count_pending};
        size_t input=sorts_input();CHECK(input+bytes<=INPUT_LIMIT,"Sorts Page/atoms/old Header/pressure coexistence exceeds C input bound");
        WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_SORTS,&batch,input,&d.error);
        bool fails=reject&&(!sentinel||row==256);
        if(fails){
            const char *reason=d.fault==1||d.fault==2?"SQL NULL":d.fault==3?"ordered":d.fault==4?"Duplicate sort":d.fault==5?"Too many secondary":"budget";
            CHECK(sorts_observe(0),"Rejected sorts diagnostic exceeds C input budget");
            CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error&&strstr(d.error,reason),d.error?d.error:"Expected semantic sorts/budget rejection missing");
            CHECK(workbook_codec_read_owned_bytes(d.read)==bytes&&workbook_codec_read_request(d.read)->first_row==row&&workbook_codec_read_request(d.read)->sql==sql,"Rejected sorts batch changed accepted candidate state");
            CHECK(sorts_prior_unchanged(),"Failed sorts changed old Header");
            CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Incomplete sorts candidate replaced old Header");
            d.rejections++;sorts.rejected_feeds++;sorts.proofs|=1u<<(d.fault-1);return true;
        }
        CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid sorts split rejected");
        if(count==256)sorts.accepted_256++;else{CHECK(count==192,"Unexpected valid sorts tail size");sorts.accepted_192++;}accepted++;
    }
    CHECK(!reject&&accepted==2,"Sorts query did not produce exactly two accepted bounded feeds");sorts.valid_splits++;return true;
}
static bool sorts_restart(void) {clear_input();d.fault++;sorts_wire=false;return begin_read();}
static bool sorts_tick_body(void) {
    if(d.phase==0){
        CHECK(d.fixture<2,"Sorts fixture index out of bounds");d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;
        char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",sorts_files[d.fixture]);
        fixture_start(d.cookie,d.ticket,sorts_files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=101;return true;
    }
    if(d.phase==101){if(!d.fixture_ready)return true;d.phase=102;return sorts_send(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false,false);}
    if(d.waiting)return true;
    if(d.phase>=102&&d.phase<=104){
        CHECK(d.ready&&!d.error,d.error?d.error:"Sorts original DESCRIBE missing");Page schema={0};
        CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Sorts original schema invalid");
        CHECK(sorts_observe(schema.bytes),"Original described schema coexistence exceeds C input budget");
        const WorkbookCodecTable *table=workbook_codec_table_find(d.phase==102?"meta":d.phase==103?"sheets":"sorts");
        if(!source_schema(&schema,table,d.phase==102?3:d.phase==103?25:4)){page_free(&schema);return false;}
        Page *destination=d.phase==102?&d.meta_schema:d.phase==103?&d.sheets_schema:&d.sorts_schema;page_free(destination);*destination=schema;clear_input();
        if(d.phase<104){d.phase++;return sorts_send(dx_strdup(d.phase==103?"DESCRIBE SELECT * FROM _duxcel_workspace.sheets":"DESCRIBE SELECT * FROM _duxcel_workspace.sorts"),NULL,256,false,false);}
        CHECK(canonical_schema(&d.meta_schema,workbook_codec_table_find("meta"))&&canonical_schema(&d.sheets_schema,workbook_codec_table_find("sheets"))&&canonical_schema(&d.sorts_schema,workbook_codec_table_find("sorts")),"Sorts canonical schema construction failed");
        CHECK(begin_read(),"Initial sorts cursor setup failed");d.phase=105;
    }
    if(d.phase==105){d.phase=106;return sorts_send_family(WORKBOOK_CODEC_META,false);}
    if(d.phase==106){
        CHECK(d.ready&&!d.error,d.error?d.error:"Sorts meta result missing");CHECK(sorts_feed_base(WORKBOOK_CODEC_META),"Sorts meta feed failed");clear_input();d.phase=107;return sorts_send_family(WORKBOOK_CODEC_SHEETS,false);
    }
    if(d.phase==107){
        CHECK(d.ready&&!d.error,d.error?d.error:"Sorts sheets result missing");CHECK(sorts_feed_base(WORKBOOK_CODEC_SHEETS),"Sorts sheets feed failed");clear_input();
        CHECK(workbook_codec_read_add_sorts(d.read,true,&d.error),d.error?d.error:"Validated existing sorts continuation failed");d.phase=108;return sorts_send_family(WORKBOOK_CODEC_SORTS,d.fault==7&&!sorts.held_released);
    }
    if(d.phase==108){
        if(d.fault==7&&!sorts.held_released){
            if(!d.held_ready)return true;CHECK(!d.ready,"Held actual sorts Page entered C early");d.cookie++;cancel_candidate();d.waiting=false;sorts.held_released=true;
            unsigned before=d.stale;release_held();CHECK(d.stale==before+1&&!d.ready&&sorts_prior_unchanged(),"Stale held sorts callback accessed or published Page");return sorts_send_family(WORKBOOK_CODEC_SORTS,false);
        }
        CHECK(d.ready&&!d.error,d.error?d.error:"Sorts result missing");CHECK(sorts_feed_result(),"Actual sorts Page feed failed");
        if(d.fault>=1&&d.fault<=6){CHECK(sorts_restart(),"Sorts negative recovery setup failed");d.phase=105;return true;}
        clear_input();CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete sorts Header could not move");CHECK(sorts_header_expected(),"Exact sorts Header fixture mismatch");d.headers++;
        if(!d.fault){
            d.fixture++;if(d.fixture<2){page_free(&d.meta_schema);page_free(&d.sheets_schema);page_free(&d.sorts_schema);d.phase=0;return true;}
            d.fixture=1;d.fault=1;sorts_wire=false;CHECK(begin_read(),"Sorts negative cursor setup failed");d.phase=105;return true;
        }
        if(d.fault==7){CHECK(sorts_restart(),"Final sorts recovery setup failed");d.phase=105;return true;}
        CHECK(d.fault==8&&d.fixtures==2&&d.headers==4&&d.queries==37&&d.rejections==6&&d.stale==1&&d.meta_queries==10&&d.sheets_queries==10&&sorts.queries==11&&d.describe_queries==6,"Sorts terminal query/header/rejection inventory changed");
        CHECK(sorts.accepted_256==5&&sorts.accepted_192==4&&sorts.rejected_feeds==6&&sorts.valid_splits==4&&sorts.proofs==63,"Sorts observed split/proof inventory changed");
        for(size_t i=0;i<2;i++)CHECK(d.normal_queries[i]==6,"Sorts normal fixture query workload changed");
        d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
    }
    if(d.phase==9&&d.fixture_ready){
        clear_input();page_free(&d.meta_schema);page_free(&d.sheets_schema);page_free(&d.sorts_schema);workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;
        d.done=true;printf("PASS browser-codec-sorts: %u checks,2 current native fixtures, 4 Headers, 37 queries, 6 semantic/budget rejections, 1 stale callback; no Workbook publication\n",d.checks);sorts_report(true,NULL);emscripten_cancel_main_loop();return true;
    }
    return true;
}
static void sorts_tick(void) {
    if(d.phase==108&&d.fault==7&&d.held_ready&&!sorts.held_released)d.waiting=false;
    sorts_tick_body();
}
