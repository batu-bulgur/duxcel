/* Primary filter/sort/query composition over immutable native archives through
   the official Worker. This owns metadata only; decoy source SQL never runs. */
typedef struct {
    unsigned executions[5],groups[5],poisoned[5],feeds[8],first_rows[5];
    unsigned output_fixture,prior_fixture,rejected_feeds,primary_rejections,schema_rejections,proofs;
    size_t maximum_header,pending_owned;
    uint64_t prior_identity,detached_identity;
    const char *pending_sql;
    bool held_released,large_primary,large_secondary;
} FiltersState;
static FiltersState filters;
static size_t filters_retained(void) {return sizeof(filters);}
static bool filters_observe(size_t extra) {
    size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
    CHECK(current<=INPUT_LIMIT&&extra<=INPUT_LIMIT-current,"Concrete primary-filter C buffers exceed input admission");
    if(current+extra>d.peak_input)d.peak_input=current+extra;return true;
}
EM_JS(void, report_filters,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned requests,unsigned rejections,unsigned stale,size_t peak,size_t header_peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned describe,const unsigned *executions,const unsigned *feeds,const unsigned *groups,const unsigned *poisoned,unsigned rejected,unsigned primary_rejections,unsigned schema_rejections,unsigned proofs,const unsigned *first_rows,int large_primary,int large_secondary,const char *error),{
    const read=(p,n)=>Array.from(HEAPU32.subarray(p>>>2,(p>>>2)+n));
    const e=read(executions,5),f=read(feeds,8),g=read(groups,5),p=read(poisoned,5);
    const families=a=>({objects:a[0],queries:a[1],sourceOptions:a[2],sorts:a[3],filters:a[4]});
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'primary-filters',checks,fixtures,headers,queries:requests,rejections,stale,
        peakOwnedInput:peak,maximumCompletedHeaderOwned:header_peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:read(normal,4),familyExecutions:{meta,sheets,...families(e),describe},
        acceptedFeeds:{objectsEmpty:f[0],queries:f[1],sourceOptions:f[2],sorts:f[3],filters256:f[4],filters29:f[5],filters3:f[6],filters4:f[7]},
        completedResults:families(g),poisonedResults:families(p),rejectedFeeds:rejected,primaryRejections:primary_rejections,schemaRejections:schema_rejections,
        rejectedFirstRows:read(first_rows,5),largePrimaryExact:Boolean(large_primary),largeSecondaryExact:Boolean(large_secondary),
        requiredNullRejected:Boolean(proofs&1),caseDuplicateRejected:Boolean(proofs&2),lateJsonRejected:Boolean(proofs&4),positionRejected:Boolean(proofs&8),
        budgetRejected:Boolean(proofs&16),primaryJsonRejected:Boolean(proofs&32),originalTypeRejected:Boolean(proofs&64),
        error:error?UTF8ToString(error):null,boundary:'official-worker/shared-C-primary filters/sorts/query/options Header; owned metadata only, no source SQL, complete Workbook publication or browser Save/Open'});
})
static void filters_report(bool ok,const char *error) {
    report_filters(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.peak_input,filters.maximum_header,d.normal_queries,d.meta_queries,d.sheets_queries,d.describe_queries,
        filters.executions,filters.feeds,filters.groups,filters.poisoned,filters.rejected_feeds,filters.primary_rejections,filters.schema_rejections,filters.proofs,filters.first_rows,filters.large_primary,filters.large_secondary,error);
}
static Page *filters_schema(unsigned index) {
    return index==0?&d.meta_schema:index==1?&d.sheets_schema:index==2?&d.objects_schema:index==3?&d.queries_schema:index==4?&d.options_schema:index==5?&d.sorts_schema:&d.filters_schema;
}
static const char *filters_name(unsigned index) {static const char *const names[]={"meta","sheets","objects","queries","source_options","sorts","filters"};return names[index];}
static unsigned filters_columns(unsigned index) {static const unsigned columns[]={3,25,11,3,3,4,5};return columns[index];}
static WorkbookCodecFamily filters_family(unsigned index) {
    static const WorkbookCodecFamily families[]={WORKBOOK_CODEC_META,WORKBOOK_CODEC_SHEETS,WORKBOOK_CODEC_OBJECTS,WORKBOOK_CODEC_QUERIES,WORKBOOK_CODEC_SOURCE_OPTIONS,WORKBOOK_CODEC_SORTS,WORKBOOK_CODEC_FILTERS};return families[index];
}
static bool filters_repeated(const char *text,char byte) {
    if(!text||strlen(text)!=65537)return false;for(size_t i=0;i<65537;i++)if(text[i]!=byte)return false;return true;
}
static bool filters_header_contents(const WorkbookCodecHeader *h,unsigned fixture,const WorkbookCodecKey *expected) {
    size_t count=fixture==3?19:1;const WorkbookCodecKey *key=workbook_codec_header_key(h);static const char *const values[]={NULL,"","NULL"};
    CHECK(h&&workbook_codec_header_filters_ready(h)&&workbook_codec_header_sorts_ready(h)&&workbook_codec_header_objects_ready(h)&&workbook_codec_header_queries_ready(h)&&workbook_codec_header_source_options_ready(h),"Primary-filter composed Header readiness changed");
    CHECK(!workbook_codec_header_preferences_ready(h)&&!workbook_codec_header_table_sources_ready(h)&&!workbook_codec_header_pivot_marks_ready(h),"Unrequested metadata families became ready");
    CHECK(workbook_codec_header_version(h)==filters_versions[fixture]&&workbook_codec_header_sheet_count(h)==count&&workbook_codec_header_active_sheet(h)==count-1&&workbook_codec_header_user_scale(h)==1.25f,"Primary-filter package/count/scale changed");
    CHECK(key&&key->document_epoch==expected->document_epoch&&key->operation_ticket==expected->operation_ticket&&key->snapshot_generation==expected->snapshot_generation,"Primary-filter Header key changed");
    for(size_t sheet=0;sheet<count;sheet++) {
        char name[128],query[160];snprintf(name,sizeof(name),"Filtered 'İ' %zu",sheet);snprintf(query,sizeof(query),"SELECT * FROM __BP1_FILTERS_MUST_NOT_EXECUTE /* 'İ' %zu */;  \n",sheet);
        const Analysis *a=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);const char *stored=NULL;bool unique=false;
        CHECK(same(workbook_codec_header_name(h,sheet),name)&&a&&a->kind==SOURCE_CSV&&a->path&&!*a->path&&same(workbook_codec_header_reference(h,sheet),"missing-source.csv")&&same(a->schema,"")&&same(a->table,"NULL")&&!a->source_version,"Primary-filter owned source/reference/name changed");
        CHECK(!workbook_codec_header_object_count(h,sheet)&&a->unique_rows==(sheet%2!=0)&&workbook_codec_header_unique_rows(h,sheet,0,&unique)&&unique==a->unique_rows,"Empty object registry or primary source option changed");
        CHECK(workbook_codec_header_query(h,sheet,0,&stored)&&stored==a->query&&same(stored,query),"Stored source query bytes/UTF-8/trailing spaces changed");
        CHECK(same(a->group_column,"Group 'İ'")&&same(a->measure_column,"Measure 'İ'")&&a->aggregate==AGG_AVG&&same(a->filter_column,"Primary 'İ'")&&a->filter_op==FILTER_CONTAINS,"Primary group/aggregate/filter changed");
        CHECK(!a->filter_number.kind&&!a->filter_number.precision&&!a->filter_number.scale,"Primary filter fabricated numeric source metadata");
        if(fixture==2&&!sheet){CHECK(filters_repeated(a->filter_value,'p'),"65537-byte primary value was truncated or borrowed");filters.large_primary=true;}
        else CHECK(same(a->filter_value,values[sheet%3]),"Primary SQL NULL/empty/literal NULL changed");
        size_t secondary=fixture==3?15:fixture==2?4:3;CHECK(a->filter_count==secondary,"Secondary filter count changed");
        for(size_t f=0;f<secondary;f++) {
            const AnalysisFilter *field=a->filters+f;snprintf(name,sizeof(name),"Filter 'İ' %zu",f);
            CHECK(same(field->column,name)&&field->op==(fixture==3&&f==14?FILTER_BETWEEN:FILTER_CONTAINS)&&!field->number.kind&&!field->number.precision&&!field->number.scale,"Secondary name/operator/number metadata changed");
            if(fixture==2&&f==3){CHECK(filters_repeated(field->value,'x'),"65537-byte secondary value was truncated or borrowed");filters.large_secondary=true;}
            else CHECK(same(field->value,fixture==3&&f==14?"[\"10\",\"20\"]":values[f%3]),"Secondary SQL NULL/empty/literal/JSON value changed");
        }
        CHECK(same(a->sort_column,"Sort 'İ' 0")&&a->descending==(sheet%2!=0)&&a->sort_count==7,"Primary and secondary sort composition changed");
        for(size_t n=0;n<7;n++){snprintf(name,sizeof(name),"Sort 'İ' %zu",n+1);CHECK(same(a->sorts[n].column,name)&&a->sorts[n].descending==((n+1)%2!=0),"Secondary sort field/order/direction changed");}
        CHECK(v&&v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==1.5f&&!v->columns&&!v->column_count,"Primary-filter persisted view changed");
    }
    const char *untouched="untouched",*query=untouched;bool unique=true;WorkbookCodecObjectView object,saved;memset(&object,0x5a,sizeof(object));memcpy(&saved,&object,sizeof(saved));
    CHECK(!workbook_codec_header_query(h,count,0,&query)&&query==untouched&&!workbook_codec_header_query(h,0,UINT64_MAX,&query)&&query==untouched,"Invalid raw-query getter changed output or resolved foreign owner");
    CHECK(!workbook_codec_header_unique_rows(h,0,1,&unique)&&unique&&!workbook_codec_header_object_at(h,0,0,&object)&&!memcmp(&object,&saved,sizeof(object)),"Empty-registry getter fabricated an owner or modified output");return true;
}
static uint64_t filters_header_identity(const WorkbookCodecHeader *h) {
    uint64_t hash=UINT64_C(1469598103934665603);if(!h)return hash;
    for(size_t sheet=0;sheet<workbook_codec_header_sheet_count(h);sheet++) {
        const Analysis *a=workbook_codec_header_analysis(h,sheet);const void *pointers[]={a,workbook_codec_header_name(h,sheet),workbook_codec_header_reference(h,sheet),a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
        for(size_t n=0;n<sizeof(pointers)/sizeof(*pointers);n++)hash=(hash^(uintptr_t)pointers[n])*UINT64_C(1099511628211);
        for(size_t n=0;n<a->filter_count;n++){hash=(hash^(uintptr_t)a->filters[n].column)*UINT64_C(1099511628211);hash=(hash^(uintptr_t)a->filters[n].value)*UINT64_C(1099511628211);}
        for(size_t n=0;n<a->sort_count;n++)hash=(hash^(uintptr_t)a->sorts[n].column)*UINT64_C(1099511628211);
    }
    return hash;
}
static bool filters_prior_unchanged(void) {
    CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes&&filters_header_identity(d.header)==filters.prior_identity,"Retained primary-filter Header graph/allocations changed");
    return !d.header||filters_header_contents(d.header,filters.prior_fixture,&d.prior_key);
}
static bool filters_begin(void) {filters.prior_fixture=filters.output_fixture;filters.prior_identity=filters_header_identity(d.header);return begin_read();}
static bool filters_send(char *sql,const Page *schema,unsigned limit,bool hold,unsigned policy) {
    filters_wire=policy==9;sorts_wire=policy==1;objects_wire=policy==2;queries_wire=policy==3&&schema==&d.queries_schema;options_wire=policy==3&&schema==&d.options_schema;return send_sql(sql,schema,limit,hold);
}
static bool filters_send_family(unsigned index,bool hold) {
    static const unsigned limits[]={2,65,2049,2113,2113,449,961};const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);const Page *schema=filters_schema(index);
    CHECK(index<7&&request&&request->family==filters_family(index)&&!request->first_row&&request->column_count==filters_columns(index)&&request->row_limit==limits[index],"Primary-filter canonical family/envelope changed");
    char *relation=dx_strdup(request->sql),*changed=NULL;
    if(index==1&&d.fault==6)changed=dx_printf("SELECT * REPLACE (CASE WHEN id=0 THEN %d::INTEGER ELSE filter_op END AS filter_op,CASE WHEN id=0 THEN '[\"only\"]' ELSE filter_value END AS filter_value) FROM (%s) AS _filters_primary_json",FILTER_BETWEEN,relation);
    if(index==6) {
        CHECK(!strcmp(relation,"SELECT sheet_id,position,name,op,value FROM _duxcel_workspace.filters ORDER BY sheet_id,position LIMIT 961"),"Primary-filter C ordered projection changed");
        if(d.fault==1)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=18 AND position=14 THEN NULL::VARCHAR ELSE name END AS name) FROM (%s) AS _filters_required_null",relation);
        else if(d.fault==2)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=18 AND position=14 THEN 'pRIMARY ''İ''' ELSE name END AS name) FROM (%s) AS _filters_case_duplicate",relation);
        else if(d.fault==3)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=18 AND position=14 THEN '[\"only\"]' ELSE value END AS value) FROM (%s) AS _filters_late_json",relation);
        else if(d.fault==4)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=18 AND position=14 THEN 15::INTEGER ELSE position END AS position) FROM (%s) AS _filters_position",relation);
    }
    if(changed){CHECK(filters_observe(text_bytes(relation)+text_bytes(changed)),"Primary-filter fault SQL coexistence exceeds admission");free(relation);relation=changed;}
    char *sql=dx_strdup("SELECT ");for(size_t c=0;c<schema->column_count;c++)dx_append(&sql,"%sCAST(%s AS VARCHAR) AS %s",c?",":"",schema->columns[c].name,schema->columns[c].name);dx_append(&sql," FROM (%s) AS _filters_projection",relation);
    CHECK(strlen(sql)<=DX_BROWSER_SQL_BYTES&&filters_observe(text_bytes(relation)+2u*text_bytes(sql)),"Primary-filter canonical SQL staging exceeds admission");free(relation);
    if(index==0)d.meta_queries++;else if(index==1)d.sheets_queries++;else filters.executions[index-2]++;
    return filters_send(sql,schema,limits[index],hold,index<2?0:index==2?2:index<5?3:index==5?1:9);
}
static bool filters_atoms(unsigned index,size_t rows) {
    const Page *schema=filters_schema(index);const WorkbookCodecTable *table=workbook_codec_table_find(filters_name(index));
    CHECK(d.page.column_count==schema->column_count&&d.page.row_count==rows&&!d.page.offset&&d.page.total==rows&&!d.page.count_pending,"Primary-filter complete result dimensions changed");
    CHECK(!d.atoms,"Prior primary-filter atoms not retired");size_t bytes=rows*schema->column_count*sizeof(*d.atoms);CHECK(filters_observe(bytes),"Primary-filter atom staging exceeds admission");d.atoms=dx_alloc(bytes);d.atom_bytes=bytes;const char *types=table->types;
    for(size_t c=0;c<schema->column_count;c++) {
        while(*types==' ')types++;WorkbookCodecAtomKind kind=atom_kind(*types++);const Column *actual=d.page.columns+c,*expected=schema->columns+c;
        CHECK(same(actual->name,expected->name)&&same(actual->type_name,expected->type_name)&&actual->type==expected->type&&actual->number_type.kind==expected->number_type.kind&&actual->number_type.precision==expected->number_type.precision&&actual->number_type.scale==expected->number_type.scale,"Primary-filter original typed sidecar changed");d.columns[c]=(WorkbookCodecColumn){actual->name,kind};
        for(size_t row=0;row<rows;row++) {
            const Cell *cell=d.page.cells+row*schema->column_count+c;WorkbookCodecAtom *a=d.atoms+row*schema->column_count+c;a->kind=cell->is_null?WORKBOOK_CODEC_NULL:kind;if(cell->is_null)continue;CHECK(cell->text,"Primary-filter exact atom has no owned text");
            if(kind==WORKBOOK_CODEC_I32)CHECK(i32(cell->text,&a->value.i32),"Primary-filter INTEGER is not exact");else if(kind==WORKBOOK_CODEC_U64)CHECK(u64(cell->text,&a->value.u64),"Primary-filter UBIGINT is not exact");
            else if(kind==WORKBOOK_CODEC_DOUBLE){char *end=NULL;a->value.real=strtod(cell->text,&end);CHECK(end&&end!=cell->text&&!*end,"Primary-filter DOUBLE is not exact text");}
            else if(kind==WORKBOOK_CODEC_BOOLEAN){CHECK(!strcmp(cell->text,"true")||!strcmp(cell->text,"false"),"Primary-filter BOOLEAN is not exact");a->value.boolean=!strcmp(cell->text,"true");}else a->value.text=(WorkbookCodecText){cell->text,strlen(cell->text)};
        }
    }
    return true;
}
static bool filters_poison(unsigned index) {
    for(size_t row=0;row<d.page.row_count;row++)for(size_t c=0;c<d.page.column_count;c++){Cell *cell=d.page.cells+row*d.page.column_count+c;if(!cell->is_null){CHECK(cell->text,"Primary-filter input text disappeared before poison");memset(cell->text,'~',strlen(cell->text));}}
    /* Even an empty source_options result owns typed sidecars. Poison those
       too, so its recorded retirement proof is actual rather than a row count. */
    if(index>=3)for(size_t c=0;c<d.page.column_count;c++){Column *column=d.page.columns+c;memset(column->name,'~',strlen(column->name));memset(column->type_name,'~',strlen(column->type_name));}
    if(d.atoms)memset(d.atoms,0,d.atom_bytes);if(index>=3||(index==2&&d.page.row_count))filters.poisoned[index-2]++;return true;
}
static bool filters_feed(unsigned index) {
    size_t sheets=d.fixture==3?19:1,rows=index==0?1:index==1?sheets:index==2?0:index==3?sheets:index==4?sheets/2:index==5?sheets*7:d.fixture==3?285:d.fixture==2?4:3;
    CHECK(filters_atoms(index,rows),"Primary-filter atom adaptation failed");bool reject=index==6&&d.fault>=1&&d.fault<=5;size_t failed=d.fault==5?0:256;
    if(reject&&d.fault==5){size_t current=retained()+workbook_codec_read_owned_bytes(d.read);CHECK(current<HEADER_LIMIT-1u,"Primary-filter concrete budget fixture has no headroom");d.pressure_bytes=HEADER_LIMIT-current-1u;d.pressure=dx_alloc(d.pressure_bytes);memset(d.pressure,0,d.pressure_bytes);CHECK(filters_observe(0),"Primary-filter pressure exceeds input budget");}
    for(size_t first=0;first<rows||(!rows&&!first);) {
        size_t count=rows-first>256?256:rows-first;const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==filters_family(index)&&request->first_row==first,"Primary-filter feeds reran SQL or lost the immutable result cursor");size_t owned=workbook_codec_read_owned_bytes(d.read);const char *sql=request->sql;
        WorkbookCodecBatch batch={d.columns,d.page.column_count,d.atoms+first*d.page.column_count,count,first,first+count==rows};CHECK(filters_observe(0),"Primary-filter feed coexistence exceeds admission");WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,filters_family(index),&batch,retained(),&d.error);
        if(reject&&first==failed) {
            static const char *const reasons[]={"SQL NULL","Duplicate column filter","Invalid column filter","contiguous positions","budget"};CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error&&strstr(d.error,reasons[d.fault-1]),d.error?d.error:"Expected primary-filter rejection reason missing");
            CHECK(workbook_codec_read_owned_bytes(d.read)==owned&&workbook_codec_read_request(d.read)->first_row==first&&workbook_codec_read_request(d.read)->sql==sql,"Rejected primary-filter batch changed ownership/cursor/SQL pointer");CHECK(filters_prior_unchanged(),"Rejected filters changed independent Header");
            CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Rejected primary filters published Header");CHECK(filters_observe(0),"Filter rejection diagnostic exceeds admission");filters.first_rows[d.fault-1]=(unsigned)first;filters.proofs|=1u<<(d.fault-1);filters.rejected_feeds++;d.rejections++;return true;
        }
        CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid primary-filter prefix rejected");
        if(index>=2&&index<6)filters.feeds[index-2]++;else if(index==6){CHECK(count==256||count==29||count==3||count==4,"Unexpected primary-filter tail");filters.feeds[count==256?4:count==29?5:count==3?6:7]++;}
        first+=count;if(!count)break;
    }
    CHECK(!reject,"Expected primary-filter rejection was not reached");if(index>=2)filters.groups[index-2]++;return filters_poison(index);
}
static void filters_free_schemas(void) {for(unsigned i=0;i<7;i++)page_free(filters_schema(i));}
static bool filters_restart(void) {clear_input();d.fault++;filters_wire=sorts_wire=objects_wire=queries_wire=options_wire=false;return filters_begin();}
static bool filters_tick_body(void) {
    if(!d.phase){CHECK(d.fixture<4,"Primary-filter fixture index exceeds inventory");d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",filters_files[d.fixture]);fixture_start(d.cookie,d.ticket,filters_files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=801;return true;}
    if(d.phase==801){if(!d.fixture_ready)return true;d.phase=802;return filters_send(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false,0);}if(d.waiting)return true;
    if(d.phase>=802&&d.phase<=808) {
        unsigned index=d.phase-802;CHECK(d.ready&&!d.error,d.error?d.error:"Original primary-filter DESCRIBE absent");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Original primary-filter schema invalid");CHECK(filters_observe(schema.bytes),"Primary-filter schema exceeds admission");
        if(!source_schema(&schema,workbook_codec_table_find(filters_name(index)),filters_columns(index))){page_free(&schema);return false;}Page *destination=filters_schema(index);page_free(destination);*destination=schema;clear_input();
        if(index<6){d.phase++;return filters_send(dx_printf("DESCRIBE SELECT * FROM _duxcel_workspace.%s",filters_name(index+1)),NULL,256,false,0);}for(unsigned i=0;i<7;i++)CHECK(canonical_schema(filters_schema(i),workbook_codec_table_find(filters_name(i))),"Primary-filter canonical typed sidecar construction failed");CHECK(filters_begin(),"Primary-filter candidate begin failed");d.phase=809;
    }
    if(d.phase==809){d.phase=810;return filters_send_family(0,false);}
    if(d.phase>=810&&d.phase<=815) {
        unsigned index=d.phase-810;CHECK(d.ready&&!d.error,d.error?d.error:"Primary-filter prerequisite result absent");CHECK(filters_feed(index),"Primary-filter prerequisite feed failed");clear_input();
        if(index==1)CHECK(workbook_codec_read_add_objects(d.read,true,&d.error),d.error?d.error:"Primary-filter empty descriptors arm failed");
        else if(index==2)CHECK(workbook_codec_read_add_queries(d.read,true,&d.error),d.error?d.error:"Primary-filter raw-query arm failed");
        else if(index==3)CHECK(workbook_codec_read_add_source_options(d.read,true,&d.error),d.error?d.error:"Primary-filter options arm failed");
        else if(index==4)CHECK(workbook_codec_read_add_sorts(d.read,true,&d.error),d.error?d.error:"Primary-filter sorts arm failed");
        else if(index==5) {
            size_t owned=workbook_codec_read_owned_bytes(d.read);CHECK(!workbook_codec_read_request(d.read),"Sorts did not leave a complete unclaimed Header");
            bool armed=workbook_codec_read_add_filters(d.read,true,retained(),&d.error);
            if(d.fault==6){CHECK(!armed&&d.error&&strstr(d.error,"Invalid column filter"),d.error?d.error:"Malformed primary BETWEEN was accepted");CHECK(workbook_codec_read_owned_bytes(d.read)==owned&&!workbook_codec_read_request(d.read)&&filters_prior_unchanged(),"Primary validation failure changed accepted candidate/old Header");filters.primary_rejections++;filters.proofs|=32;d.rejections++;CHECK(filters_restart(),"Primary JSON rejection recovery failed");d.phase=817;filters.pending_owned=workbook_codec_read_owned_bytes(d.read);filters.pending_sql=workbook_codec_read_request(d.read)->sql;return filters_send(dx_strdup("DESCRIBE SELECT * REPLACE (op::VARCHAR AS op) FROM _duxcel_workspace.filters"),NULL,256,false,0);}
            CHECK(armed,d.error?d.error:"Primary-filter continuation arm failed");filters.pending_owned=workbook_codec_read_owned_bytes(d.read);filters.pending_sql=workbook_codec_read_request(d.read)->sql;
        }
        d.phase++;return filters_send_family(index+1,index==5&&d.fault==8&&!filters.held_released);
    }
    if(d.phase==817) {
        CHECK(d.fault==7&&d.ready&&!d.error,d.error?d.error:"Wrong original filters INTEGER DESCRIBE absent");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),"Wrong filters original-type schema invalid");CHECK(filters_observe(schema.bytes),"Wrong original filter schema exceeds admission");bool rejected=!schema_matches(&schema,workbook_codec_table_find("filters"),5);page_free(&schema);CHECK(rejected,"Original VARCHAR op accepted as INTEGER");
        const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_META&&!request->first_row&&request->sql==filters.pending_sql&&workbook_codec_read_owned_bytes(d.read)==filters.pending_owned&&filters_prior_unchanged(),"Original filter type rejection changed candidate/old Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Wrong original filter type published a Header");d.rejections++;filters.schema_rejections++;filters.proofs|=64;CHECK(filters_restart(),"Original filter schema recovery failed");d.phase=809;return true;
    }
    if(d.phase==816) {
        if(d.fault==8&&!filters.held_released){if(!d.held_ready)return true;CHECK(!d.ready,"Held actual filter result entered reader early");d.cookie++;cancel_candidate();d.waiting=false;filters.held_released=true;unsigned stale=d.stale;release_held();CHECK(d.stale==stale+1&&!d.ready&&filters_prior_unchanged(),"Stale filter callback decoded input or changed independent Header");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==WORKBOOK_CODEC_FILTERS&&!request->first_row&&request->sql==filters.pending_sql&&workbook_codec_read_owned_bytes(d.read)==filters.pending_owned,"Stale filters changed accepted prerequisite ownership/cursor");return filters_send_family(6,false);}
        CHECK(d.ready&&!d.error,d.error?d.error:"Primary filters result absent");CHECK(filters_feed(6),"Primary filters feed failed");if(d.fault>=1&&d.fault<=5){CHECK(filters_restart(),"Secondary primary-filter rejection recovery failed");d.phase=809;return true;}clear_input();CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete primary-filter Header could not move");filters.output_fixture=d.fixture;CHECK(filters_header_contents(d.header,d.fixture,&d.key),"Owned primary definitions differ from native fixture");d.headers++;size_t bytes=workbook_codec_header_owned_bytes(d.header);CHECK(bytes<=HEADER_LIMIT,"Completed primary-filter Header exceeds its limit");if(bytes>filters.maximum_header)filters.maximum_header=bytes;
        if(!d.fault){d.fixture++;if(d.fixture<4){filters_free_schemas();d.phase=0;return true;}d.fixture=3;d.fault=1;CHECK(filters_begin(),"Primary-filter negative candidate begin failed");d.phase=809;return true;}if(d.fault==8){CHECK(filters_restart(),"Final primary-filter recovery begin failed");d.phase=809;return true;}
        CHECK(d.fault==9&&d.fixtures==4&&d.headers==6&&d.queries==113&&d.rejections==7&&d.stale==1&&d.meta_queries==12&&d.sheets_queries==12&&d.describe_queries==29,"Primary-filter terminal request inventory changed");
        const unsigned expected_feeds[]={12,12,12,12,7,3,2,1},first[]={256,256,256,256,0};for(size_t i=0;i<5;i++){CHECK(filters.executions[i]==12&&filters.groups[i]==(i==4?6u:12u)&&filters.poisoned[i]==(i==0?0u:i==4?6u:12u),"Primary-filter completed/poisoned inventory changed");CHECK(filters.first_rows[i]==first[i],"Primary-filter rejection accepted-prefix changed");}for(size_t i=0;i<8;i++)CHECK(filters.feeds[i]==expected_feeds[i],"Primary-filter accepted-feed inventory changed");for(size_t i=0;i<4;i++)CHECK(d.normal_queries[i]==14,"Normal primary-filter fixture query count changed");
        CHECK(filters.rejected_feeds==5&&filters.primary_rejections==1&&filters.schema_rejections==1&&filters.proofs==127&&filters.large_primary&&filters.large_secondary,"Primary-filter reasoned proof inventory incomplete");filters.detached_identity=filters_header_identity(d.header);d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
    }
    if(d.phase==9&&d.fixture_ready){clear_input();filters_free_schemas();CHECK(filters_header_contents(d.header,3,&d.key)&&filters_header_identity(d.header)==filters.detached_identity,"Owned filters/sorts/query bytes changed after all Page/catalog handles closed");workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;d.done=true;printf("PASS: browser-codec-primary-filters %u checks,4 native files,6 Headers,113 queries,5 feed/1 primary/1 schema rejections,1 stale callback; owned definitions only, no source execution or Workbook publication\n",d.checks);filters_report(true,NULL);emscripten_cancel_main_loop();return true;}return true;
}
static void filters_tick(void) {if(d.phase==816&&d.fault==8&&d.held_ready&&!filters.held_released)d.waiting=false;filters_tick_body();}
