/* Preferences-only continuation in the existing official-Worker driver. The
   two malformed native files exercise the cursor's real current WHERE predicates;
   they are not rows injected after that predicate or complete Workbook loads. */
typedef struct {
    unsigned queries,first32,last33,rejected_feeds,pairs,proofs,predicate_queries;
    bool held_released;
} PreferencesState;
static PreferencesState preferences;
static size_t preferences_retained(void) {return sizeof(preferences);}
static bool preferences_observe(size_t extra) {
    size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
    CHECK(current<=INPUT_LIMIT&&extra<=INPUT_LIMIT-current,"Concrete preferences C buffers exceed admission");
    if(current+extra>d.peak_input)d.peak_input=current+extra;return true;
}
EM_JS(void, report_preferences,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned queries,unsigned rejections,unsigned stale,size_t peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned prefs,unsigned describe,unsigned first,unsigned last,unsigned rejected,unsigned pairs,unsigned proofs,unsigned predicate,const char *error),{
    const at=normal>>>2;
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'preferences',checks,fixtures,headers,queries,rejections,stale,
        peakOwnedInput:peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:Array.from(HEAPU32.subarray(at,at+4)),familyExecutions:{meta,sheets,preferences:prefs,describe},
        acceptedFeeds:{rows32:first,rows33:last},rejectedFeeds:rejected,validSplitPairs:pairs,predicateQueries:predicate,
        unknownNullRejected:Boolean(proofs&1),unknownNulRejected:Boolean(proofs&2),duplicateFalseRejected:Boolean(proofs&4),
        activeHiddenRejected:Boolean(proofs&8),noncanonicalRejected:Boolean(proofs&16),malformedRejected:Boolean(proofs&32),
        sentinelRejected:Boolean(proofs&64),budgetRejected:Boolean(proofs&128),
        error:error?UTF8ToString(error):null,boundary:'official-worker/shared-C-meta-sheets-preferences Header; no Workbook, owner families, browser Save/Open, missing-table or source execution proof'});
})
static void preferences_report(bool ok,const char *error) {
    report_preferences(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.peak_input,d.normal_queries,d.meta_queries,d.sheets_queries,preferences.queries,d.describe_queries,
        preferences.first32,preferences.last33,preferences.rejected_feeds,preferences.pairs,preferences.proofs,preferences.predicate_queries,error);
}
static bool preferences_prior_unchanged(void) {
    CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes,"Old preferences Header ownership changed");
    if(d.header){const WorkbookCodecKey *key=workbook_codec_header_key(d.header);
        CHECK(key&&key->document_epoch==d.prior_key.document_epoch&&key->operation_ticket==d.prior_key.operation_ticket&&key->snapshot_generation==d.prior_key.snapshot_generation,"Old preferences Header key changed");
        CHECK(workbook_codec_header_sheet_count(d.header)==64&&workbook_codec_header_preferences_ready(d.header)&&workbook_codec_header_reduced_motion(d.header)&&workbook_codec_header_sheet_hidden(d.header,0)&&workbook_codec_header_sheet_hidden(d.header,62)&&!workbook_codec_header_sheet_hidden(d.header,63),"Bad/stale preferences candidate changed old Header flags");
        CHECK(same(workbook_codec_header_name(d.header,0),"Sorted 'İ' 0")&&!workbook_codec_header_analysis(d.header,63)->sort_count,"Bad preferences candidate changed old Header definitions");
    }
    return true;
}
static bool preferences_header_expected(void) {
    const WorkbookCodecHeader *h=d.header;const WorkbookCodecKey *key=workbook_codec_header_key(h);
    CHECK(h&&workbook_codec_header_preferences_ready(h)&&!workbook_codec_header_sorts_ready(h)&&workbook_codec_header_sheet_count(h)==64&&workbook_codec_header_active_sheet(h)==63,"Independent preferences Header readiness/count/active changed");
    CHECK(workbook_codec_header_version(h)==preferences_versions[d.fixture]&&workbook_codec_header_user_scale(h)==1.25f&&workbook_codec_header_reduced_motion(h),"Actual preferences package/scale/motion changed");
    CHECK(key&&key->document_epoch==d.key.document_epoch&&key->operation_ticket==d.key.operation_ticket&&key->snapshot_generation==d.key.snapshot_generation,"Preferences Header key changed");
    for(size_t sheet=0;sheet<64;sheet++){
        char *name=dx_printf("Sorted 'İ' %zu",sheet);bool match=same(workbook_codec_header_name(h,sheet),name);CHECK(preferences_observe(text_bytes(name)),"Temporary preference name exceeds input budget");free(name);CHECK(match,"Preferences sheet name lost UTF-8/quotes");
        const Analysis *a=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);
        CHECK(a&&a->kind==SOURCE_CSV&&a->path&&!*a->path&&same(workbook_codec_header_reference(h,sheet),"missing-source.csv")&&same(a->schema,"")&&same(a->table,"NULL"),"Raw source/empty browser locator/NULL literal metadata changed");
        CHECK(!a->query&&!a->source_version&&!a->unique_rows&&!a->filter_column&&!a->filter_value&&!a->filter_count&&!a->group_column&&!a->measure_column&&!a->sort_column&&!a->descending&&!a->sort_count,"Preferences loaded/executed unrelated source/sort state");
        CHECK(workbook_codec_header_sheet_hidden(h,sheet)==(sheet<63),"Preferences visibility lost explicit false or high sheet index");
        CHECK(v&&v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==1.5f&&!v->columns&&!v->column_count,"Preferences changed viewport/canvas");
    }
    CHECK(!workbook_codec_header_sheet_hidden(h,64),"Invalid sheet index aliases a visible/hidden bit");return true;
}
static bool preferences_send(char *sql,const Page *schema,unsigned limit,bool hold) {sorts_wire=false;return send_sql(sql,schema,limit,hold);}
static bool preferences_send_family(WorkbookCodecFamily family,bool hold) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
    CHECK(request&&request->family==family&&!request->first_row,"Unexpected/reexecuted preferences family cursor");
    if(family!=WORKBOOK_CODEC_PREFERENCES){
        const Page *schema=family==WORKBOOK_CODEC_META?&d.meta_schema:&d.sheets_schema;
        if(family==WORKBOOK_CODEC_META)d.meta_queries++;else d.sheets_queries++;
        return preferences_send(browser_page_sql(request->sql,schema,0,&d.error),schema,(unsigned)request->row_limit,false);
    }
    CHECK(request->row_limit==66&&request->column_count==2&&request->source_column_count==2,"Preferences cursor envelope changed");
    {
        CHECK(strstr(request->sql,"OR name IS NULL OR value IS NULL OR contains(name,chr(0))"),"Real current unknown-invalid predicate missing");preferences.predicate_queries++;
    }
    char *relation=dx_strdup(request->sql);
    if(d.fault==1){
        char *next=dx_printf("SELECT * FROM (SELECT * FROM (%s) AS _pref_original WHERE name<>'sheet_hidden_62' UNION ALL SELECT 'sheet_hidden_63'::VARCHAR AS name,false::BOOLEAN AS value) AS _pref_duplicate ORDER BY name",relation);
        CHECK(preferences_observe(text_bytes(relation)+text_bytes(next)),"Duplicate preference SQL exceeds input budget");free(relation);relation=next;
    }else if(d.fault>=2&&d.fault<=4){
        const char *replacement=d.fault==2?"CASE WHEN name='sheet_hidden_63' THEN true ELSE value END AS value":d.fault==3?"CASE WHEN name='sheet_hidden_63' THEN 'sheet_hidden_01' ELSE name END AS name":"CASE WHEN name='sheet_hidden_63' THEN 'sheet_hidden_' ELSE name END AS name";
        char *next=dx_printf("SELECT * REPLACE (%s) FROM (%s) AS _pref_bad ORDER BY name",replacement,relation);
        CHECK(preferences_observe(text_bytes(relation)+text_bytes(next)),"Malformed preference SQL exceeds input budget");free(relation);relation=next;
    }else if(d.fault==5){
        char *next=dx_printf("SELECT * FROM ((%s) UNION ALL SELECT 'sheet_hidden_63'::VARCHAR AS name,false::BOOLEAN AS value) AS _pref_sentinel ORDER BY name LIMIT 66",relation);
        CHECK(preferences_observe(text_bytes(relation)+text_bytes(next)),"Preference sentinel SQL exceeds input budget");free(relation);relation=next;
    }
    char *sql=browser_page_sql(relation,&d.preferences_schema,0,&d.error);
    CHECK(preferences_observe(text_bytes(relation)+text_bytes(sql)),"Canonical preference SQL coexistence exceeds input budget");free(relation);preferences.queries++;
    return preferences_send(sql,&d.preferences_schema,66,hold);
}
static bool preferences_atoms(void) {
    bool sentinel=d.fault==5;size_t rows=sentinel?66:65;
    CHECK(d.page.column_count==2&&d.page.row_count==rows&&d.page.offset==0&&d.page.total==rows&&d.page.count_pending==sentinel,"Preferences rows/completeness changed or invalid unknown row was omitted");
    CHECK(!d.atoms,"Prior preferences atoms not retired");d.atom_bytes=rows*2*sizeof(*d.atoms);d.atoms=dx_alloc(d.atom_bytes);
    for(size_t column=0;column<2;column++){
        CHECK(same(d.page.columns[column].name,d.preferences_schema.columns[column].name)&&same(d.page.columns[column].type_name,d.preferences_schema.columns[column].type_name),"Preferences original-type canonical sidecar changed");
        d.columns[column]=(WorkbookCodecColumn){d.page.columns[column].name,column?WORKBOOK_CODEC_BOOLEAN:WORKBOOK_CODEC_TEXT};
    }
    for(size_t row=0;row<rows;row++)for(size_t column=0;column<2;column++){
        const Cell *cell=d.page.cells+row*2+column;WorkbookCodecAtom *atom=d.atoms+row*2+column;
        atom->kind=cell->is_null?WORKBOOK_CODEC_NULL:column?WORKBOOK_CODEC_BOOLEAN:WORKBOOK_CODEC_TEXT;
        if(cell->is_null)continue;CHECK(cell->text,"Missing exact preference raw text");
        if(!column)atom->value.text=(WorkbookCodecText){cell->text,strlen(cell->text)};
        else{CHECK(!strcmp(cell->text,"true")||!strcmp(cell->text,"false"),"Invalid exact BOOLEAN preference text");atom->value.boolean=!strcmp(cell->text,"true");}
    }
    if(!d.fault&&d.fixture==1)CHECK(same(d.page.cells[0].text,"a")&&d.page.cells[1].is_null,"Unknown NULL native archive row did not survive real current WHERE");
    return true;
}
static bool preferences_feed_result(void) {
    bool bad_file=!d.fault&&d.fixture==1,reject=bad_file||(d.fault>=1&&d.fault<=6),sentinel=d.fault==5;
    CHECK(preferences_atoms(),"Exact preference atom conversion failed");
    if(d.fault==6){d.pressure=dx_alloc(HEADER_LIMIT);d.pressure_bytes=HEADER_LIMIT;memset(d.pressure,0,d.pressure_bytes);}
    unsigned accepted=0;
    for(size_t row=0;row<d.page.row_count;){
        size_t count=row?d.page.row_count-row:32;const WorkbookCodecRequest *before=workbook_codec_read_request(d.read);
        CHECK(before&&before->family==WORKBOOK_CODEC_PREFERENCES&&before->first_row==row,"Preferences feeds did not follow one owned result");
        size_t bytes=workbook_codec_read_owned_bytes(d.read);const char *sql=before->sql;
        WorkbookCodecBatch batch={d.columns,2,d.atoms+row*2,count,row,row+count==d.page.row_count&&!d.page.count_pending};size_t input=retained();
        CHECK(preferences_observe(0),"Preferences batch coexistence exceeds C input budget");
        WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,WORKBOOK_CODEC_PREFERENCES,&batch,input,&d.error);
        bool late=d.fault==1||d.fault==2||sentinel;bool fails=reject&&(!late||row==32);
        if(fails){
            const char *reason=bad_file?"SQL NULL":d.fault==1?"Duplicate worksheet":d.fault==2?"active worksheet":d.fault==3?"canonical decimal":d.fault==4?"supported named projection":sentinel?"Too many supported":"budget";
            CHECK(preferences_observe(0),"Preference error coexistence exceeds input budget");
            CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error&&strstr(d.error,reason),d.error?d.error:"Expected preferences semantic/budget rejection absent");
            CHECK(workbook_codec_read_owned_bytes(d.read)==bytes&&workbook_codec_read_request(d.read)->first_row==row&&workbook_codec_read_request(d.read)->sql==sql,"Rejected preferences batch changed accepted state");
            CHECK(preferences_prior_unchanged(),"Preferences failure changed old independent Header");
            CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Failed preferences candidate replaced old Header");
            d.rejections++;preferences.rejected_feeds++;preferences.proofs|=bad_file?1u:1u<<(d.fault+1);return true;
        }
        CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid preference split rejected");
        if(!row)preferences.first32++;else{CHECK(count==33,"Unexpected valid preference tail");preferences.last33++;}accepted++;row+=count;
    }
    CHECK(!reject&&accepted==2,"Preferences did not use32+33 from one complete owned result");preferences.pairs++;return true;
}
static bool preferences_next_fixture(void) {
    clear_input();page_free(&d.meta_schema);page_free(&d.sheets_schema);page_free(&d.preferences_schema);d.fixture++;d.phase=0;return true;
}
static bool preferences_restart(void) {clear_input();d.fault++;return begin_read();}
static bool preferences_tick_body(void) {
    if(d.phase==0){
        CHECK(d.fixture<4,"Preferences fixture index exceeded inventory");d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;
        char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",preferences_files[d.fixture]);fixture_start(d.cookie,d.ticket,preferences_files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=201;return true;
    }
    if(d.phase==201){if(!d.fixture_ready)return true;d.phase=202;return preferences_send(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false);}
    if(d.waiting)return true;
    if(d.phase>=202&&d.phase<=204){
        CHECK(d.ready&&!d.error,d.error?d.error:"Preferences original DESCRIBE missing");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Invalid original preference schema");
        CHECK(preferences_observe(schema.bytes),"Original preference schema coexistence exceeds input budget");
        const WorkbookCodecTable *table=workbook_codec_table_find(d.phase==202?"meta":d.phase==203?"sheets":"preferences");
        if(!source_schema(&schema,table,d.phase==202?3:d.phase==203?25:2)){page_free(&schema);return false;}
        Page *destination=d.phase==202?&d.meta_schema:d.phase==203?&d.sheets_schema:&d.preferences_schema;page_free(destination);*destination=schema;clear_input();
        if(d.phase<204){d.phase++;return preferences_send(dx_strdup(d.phase==203?"DESCRIBE SELECT * FROM _duxcel_workspace.sheets":"DESCRIBE SELECT * FROM _duxcel_workspace.preferences"),NULL,256,false);}
        CHECK(canonical_schema(&d.meta_schema,workbook_codec_table_find("meta"))&&canonical_schema(&d.sheets_schema,workbook_codec_table_find("sheets"))&&canonical_schema(&d.preferences_schema,workbook_codec_table_find("preferences")),"Preferences canonical schemas failed");
        CHECK(begin_read(),"Initial preferences cursor failed");d.phase=205;
    }
    if(d.phase==205){d.phase=206;return preferences_send_family(WORKBOOK_CODEC_META,false);}
    if(d.phase==206){CHECK(d.ready&&!d.error,d.error?d.error:"Preferences meta result missing");CHECK(sorts_feed_base(WORKBOOK_CODEC_META),"Preferences exact meta feed failed");clear_input();d.phase=207;return preferences_send_family(WORKBOOK_CODEC_SHEETS,false);}
    if(d.phase==207){
        CHECK(d.ready&&!d.error,d.error?d.error:"Preferences sheets result missing");CHECK(sorts_feed_base(WORKBOOK_CODEC_SHEETS),"Preferences exact sheets feed failed");clear_input();
        CHECK(workbook_codec_read_add_preferences(d.read,true,&d.error),d.error?d.error:"Validated preferences continuation failed");d.phase=208;return preferences_send_family(WORKBOOK_CODEC_PREFERENCES,d.fault==7&&!preferences.held_released);
    }
    if(d.phase==208){
        if(d.fault==7&&!preferences.held_released){
            if(!d.held_ready)return true;CHECK(!d.ready,"Held actual preferences entered C early");d.cookie++;cancel_candidate();d.waiting=false;preferences.held_released=true;
            unsigned stale=d.stale;release_held();CHECK(d.stale==stale+1&&!d.ready&&preferences_prior_unchanged(),"Superseded preferences callback accessed/published old wire");return preferences_send_family(WORKBOOK_CODEC_PREFERENCES,false);
        }
        CHECK(d.ready,"Preferences result missing");
        if(!d.fault&&d.fixture==2){
            CHECK(d.error&&strstr(d.error,"NUL"),"Actual unknown-NUL archive row was incorrectly omitted or accepted by wire");
            const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_PREFERENCES&&!request->first_row&&strstr(request->sql,"contains(name,chr(0))"),"Unknown-NUL failure changed cursor or did not use current predicate");
            CHECK(preferences_prior_unchanged(),"Unknown-NUL archive changed old Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Incomplete unknown-NUL candidate replaced old Header");preferences.proofs|=2;d.rejections++;return preferences_next_fixture();
        }
        CHECK(!d.error,d.error?d.error:"Preferences transport failed");CHECK(preferences_feed_result(),"Actual preference atoms failed");
        if(!d.fault&&d.fixture==1)return preferences_next_fixture();
        if(d.fault>=1&&d.fault<=6){CHECK(preferences_restart(),"Preferences negative recovery setup failed");d.phase=205;return true;}
        clear_input();CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete preferences Header move failed");CHECK(preferences_header_expected(),"Exact preferences Header mismatch");d.headers++;
        if(!d.fault){
            if(d.fixture<3)return preferences_next_fixture();d.fault=1;CHECK(begin_read(),"Preferences synthetic negative setup failed");d.phase=205;return true;
        }
        if(d.fault==7){CHECK(preferences_restart(),"Final preferences recovery setup failed");d.phase=205;return true;}
        CHECK(d.fault==8&&d.fixtures==4&&d.headers==4&&d.queries==49&&d.rejections==8&&d.stale==1&&d.meta_queries==12&&d.sheets_queries==12&&preferences.queries==13&&d.describe_queries==12,"Preferences terminal workload changed");
        CHECK(preferences.first32==7&&preferences.last33==4&&preferences.rejected_feeds==7&&preferences.pairs==4&&preferences.proofs==255&&preferences.predicate_queries==13,"Preferences split/predicate/proof inventory changed");
        for(size_t i=0;i<4;i++)CHECK(d.normal_queries[i]==6,"Preferences fixture query workload changed");
        d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
    }
    if(d.phase==9&&d.fixture_ready){
        clear_input();page_free(&d.meta_schema);page_free(&d.sheets_schema);page_free(&d.preferences_schema);workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;
        d.done=true;printf("PASS browser-codec-preferences: %u checks,4 current native archives,4 Headers,49 queries,8 rejections,1 stale callback; no Workbook publication\n",d.checks);preferences_report(true,NULL);emscripten_cancel_main_loop();return true;
    }
    return true;
}
static void preferences_tick(void) {
    if(d.phase==208&&d.fault==7&&d.held_ready&&!preferences.held_released)d.waiting=false;preferences_tick_body();
}
