/* One official Arrow execution per column/cell family, ACKed <=256-row DXP1
   chunks into the shared C reader. This never publishes a Workbook or source. */
typedef struct {
    unsigned streams[2],data[2],eofs[2],feed256[2],feed29[2],feed7[2],accepted_eof[2],poisoned[2],completed[2],errors[2];
    unsigned chunk_calls,acks,done_calls,stale_chunks,stale_done,held_calls,cancelled_calls,rejected_feeds,proofs,first_rows[5];
    unsigned family,sequence,stream_acks,output_fixture,prior_fixture;
    uint64_t rows,prior_identity,detached_identity;
    size_t incoming_wire,incoming_strings,maximum_header,pending_owned;
    const char *pending_sql;
    bool active,finished,rejected,held,cancel_pending,cancelled,end_acked,long_column,long_cell,empty_present,missing_absent;
} GridState;
static GridState grid;
static size_t grid_retained(void) {return sizeof(grid)+grid.incoming_wire+grid.incoming_strings;}
static bool grid_observe(size_t extra) {
    size_t current=retained()+workbook_codec_read_owned_bytes(d.read);
    CHECK(current<=INPUT_LIMIT&&extra<=INPUT_LIMIT-current,"Concrete streamed grid C buffers exceed input admission");
    if(current+extra>d.peak_input)d.peak_input=current+extra;return true;
}
EM_JS(void, report_grid,(int ok,unsigned checks,unsigned fixtures,unsigned headers,unsigned queries,unsigned rejects,unsigned stale,size_t peak,size_t header_peak,const unsigned *normal,unsigned meta,unsigned sheets,unsigned describe,const unsigned *streams,const unsigned *data,const unsigned *eofs,const unsigned *f256,const unsigned *f29,const unsigned *f7,const unsigned *feof,const unsigned *poisoned,const unsigned *completed,const unsigned *errors,unsigned chunks,unsigned acks,unsigned dones,unsigned stale_chunks,unsigned stale_done,unsigned held,unsigned cancelled,unsigned rejected,unsigned proofs,const unsigned *first,int long_column,int long_cell,int empty_present,int missing_absent,const char *error),{
    const read=(p,n)=>Array.from(HEAPU32.subarray(p>>>2,(p>>>2)+n)),pair=p=>{const a=read(p,2);return {columns:a[0],cells:a[1]};};
    const a=read(f256,2),b=read(f29,2),c=read(f7,2),e=read(feof,2),families=read(streams,2);
    Module['dxCodecReadTestDone']({ok:Boolean(ok),mode:'grid',checks,fixtures,headers,queries,rejections:rejects,stale,
        peakOwnedInput:peak,maximumCompletedHeaderOwned:header_peak,headerOwnedLimit:2097152,inputOwnedLimit:4194304,
        normalFixtureQueries:read(normal,4),familyExecutions:{meta,sheets,columns:families[0],cells:families[1],describe},
        matchedDataCallbacks:pair(data),matchedEofCallbacks:pair(eofs),completedStreams:pair(completed),errorStreams:pair(errors),poisonedDataChunks:pair(poisoned),
        acceptedFeeds:{columns256:a[0],columns29:b[0],columns7:c[0],columnsEof:e[0],cells256:a[1],cells29:b[1],cells7:c[1],cellsEof:e[1]},
        chunkCallbacks:chunks,acceptedAcks:acks,doneCallbacks:dones,staleChunkCallbacks:stale_chunks,staleDoneCallbacks:stale_done,staleScalarReservationBytes:stale_chunks?42:0,staleTerminalReservationBytes:stale_done?2070:0,heldCallbacks:held,cancelledCallbacks:cancelled,
        rejectedFeeds:rejected,rejectedFirstRows:read(first,5),lateWidthRejected:Boolean(proofs&1),requiredColumnNameRejected:Boolean(proofs&2),requiredCellTextRejected:Boolean(proofs&4),duplicateCellRejected:Boolean(proofs&8),budgetRejected:Boolean(proofs&16),
        heldSecondChunkCancelled:Boolean(proofs&32),wholeCandidateRetry:Boolean(proofs&64),freshRecovery:Boolean(proofs&128),longColumnNameExact:Boolean(long_column),maximumCellTextExact:Boolean(long_cell),actualEmptyCellPresent:Boolean(empty_present),missingCellAbsent:Boolean(missing_absent),
        error:error?UTF8ToString(error):null,boundary:'official-worker/ACKed DXP1 chunks/shared-C-column-and-cell Header; owned sparse metadata only, no source SQL, full Workbook/model placement, browser App Open or Save'});
})
static void grid_report(bool ok,const char *error) {
    report_grid(ok,d.checks,d.fixtures,d.headers,d.queries,d.rejections,d.stale,d.peak_input,grid.maximum_header,d.normal_queries,d.meta_queries,d.sheets_queries,d.describe_queries,
        grid.streams,grid.data,grid.eofs,grid.feed256,grid.feed29,grid.feed7,grid.accepted_eof,grid.poisoned,grid.completed,grid.errors,grid.chunk_calls,grid.acks,grid.done_calls,grid.stale_chunks,grid.stale_done,grid.held_calls,grid.cancelled_calls,grid.rejected_feeds,grid.proofs,grid.first_rows,grid.long_column,grid.long_cell,grid.empty_present,grid.missing_absent,error);
}
EM_JS(void, grid_stream_submit,(unsigned cookie,unsigned ticket,const char *sql,const char *columns,const char *bindings,int cells,unsigned limit,unsigned hold),{
    const request={requestId:String(ticket),generation:'9007199254740993',documentId:String(cookie),sourceId:'0',objectId:'0',revision:'18446744073709551615',offset:'0',knownTotal:null,rowLimit:limit,sql:UTF8ToString(sql),columns:JSON.parse(UTF8ToString(columns)),sourceBindings:JSON.parse(UTF8ToString(bindings)),purpose:'workbook-metadata-stream',family:cells?'cells':'columns'};
    Module['dxCodecStreamSubmit']({heap:()=>HEAPU8,allocate:_malloc,release:_free,chunk:_dx_browser_codec_stream_chunk,done:_dx_browser_codec_stream_done,held:_dx_browser_codec_stream_held},cookie,ticket,request,hold);
})
EM_JS(void, grid_stream_cancel,(unsigned cookie,unsigned ticket),{
    Module['dxCodecStreamCancel']({heap:()=>HEAPU8,allocate:_malloc,release:_free,cancelled:_dx_browser_codec_stream_cancelled},cookie,ticket);
})
static Page *grid_schema(unsigned index) {return index==0?&d.meta_schema:index==1?&d.sheets_schema:index==2?&d.grid_columns_schema:&d.grid_cells_schema;}
static const char *grid_name(unsigned index) {static const char *const names[]={"meta","sheets","columns","cells"};return names[index];}
static bool grid_u64(const char *text,uint64_t *value,size_t *bytes) {
    if(!text)return false;size_t n=0;while(n<=20&&text[n])n++;if(!n||n>20||!u64(text,value))return false;*bytes=n+1;return true;
}
static bool grid_repeat(const char *text,char byte,size_t count) {if(!text||strlen(text)!=count)return false;for(size_t n=0;n<count;n++)if(text[n]!=byte)return false;return true;}
static size_t grid_count(unsigned fixture,size_t sheet) {return fixture==3?(sheet?28u:257u):sheet==0?0u:sheet==1?3u:4u;}
static const char *grid_find(WorkbookCodecCellsView view,uint64_t row,size_t col) {for(size_t n=0;n<view.count;n++)if(view.cells[n].row==row&&view.cells[n].col==col)return view.cells[n].text;return NULL;}
static bool grid_header_contents(const WorkbookCodecHeader *h,unsigned fixture,const WorkbookCodecKey *expected) {
    size_t sheets=fixture==3?2:3;const WorkbookCodecKey *key=workbook_codec_header_key(h);
    CHECK(h&&workbook_codec_header_columns_ready(h)&&workbook_codec_header_cells_ready(h)&&workbook_codec_header_sheet_count(h)==sheets&&workbook_codec_header_active_sheet(h)==sheets-1,"Grid Header readiness/count/active changed");
    CHECK(workbook_codec_header_version(h)==grid_versions[fixture]&&workbook_codec_header_user_scale(h)==1.25f&&key&&key->document_epoch==expected->document_epoch&&key->operation_ticket==expected->operation_ticket&&key->snapshot_generation==expected->snapshot_generation,"Grid Header package/key changed");
    CHECK(!workbook_codec_header_objects_ready(h)&&!workbook_codec_header_queries_ready(h)&&!workbook_codec_header_source_options_ready(h)&&!workbook_codec_header_table_sources_ready(h)&&!workbook_codec_header_pivot_marks_ready(h)&&!workbook_codec_header_filters_ready(h)&&!workbook_codec_header_sorts_ready(h)&&!workbook_codec_header_preferences_ready(h),"Grid Header fabricated unrequested family readiness");
    static const float widths[]={52,1200,88.125f,73.5f};static const char *const values[]={"","NULL","İstanbul 🦆","value 'quoted'\t\n"};
    for(size_t sheet=0;sheet<sheets;sheet++) {
        char name[128];snprintf(name,sizeof(name),"Grid 'İ' %zu",sheet);const Analysis *a=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);size_t count=grid_count(fixture,sheet);WorkbookCodecCellsView cells={0};
        CHECK(same(workbook_codec_header_name(h,sheet),name)&&a&&a->kind==SOURCE_NONE&&a->path&&!*a->path&&same(workbook_codec_header_reference(h,sheet),"")&&!a->schema&&!a->table&&!a->query&&!a->filter_column&&!a->filter_value&&!a->filter_count&&!a->group_column&&!a->measure_column&&!a->sort_column&&!a->sort_count&&!a->unique_rows&&!a->source_version,"Grid sheet/source metadata changed or executed");
        CHECK(v&&v->row==DX_SHEET_ROWS-1&&v->col==16383&&v->anchor_row==1&&v->anchor_col==2&&v->end_row==3&&v->end_col==4&&v->top==5&&v->horizontal==120.25f&&v->fraction==.25&&v->selection==2&&v->canvas_zoom==1.5f&&v->column_count==count,"Grid view/column presentation changed");
        CHECK((count&&v->columns&&v->widths)||(!count&&!v->columns&&!v->widths),"Empty column presentation fabricated arrays");
        CHECK(workbook_codec_header_cells(h,sheet,&cells)&&cells.count==count&&((count&&cells.cells)||(!count&&!cells.cells)),"Sparse cell count/ownership changed");size_t bytes=0;
        for(size_t n=0;n<count;n++) {
            if(fixture==2&&sheet==1&&n==2){CHECK(grid_repeat(v->columns[n],'c',65537),"Exact65537-byte column name was truncated/borrowed");grid.long_column=true;}
            else{if(n%4==3)snprintf(name,sizeof(name),"Column 'İ' %zu:%zu",sheet,n);CHECK(same(v->columns[n],n%4==0?"":n%4==1||n%4==2?"NULL":name),"Column empty/duplicate/literal/UTF-8 name changed");}
            CHECK(v->widths[n]==widths[(sheet+n)%4],"Original DOUBLE width did not preserve validated float presentation");
            bool last=sheet+1==sheets&&n+1==count;uint64_t row=last?DX_SHEET_ROWS-1:n/4;size_t col=last?16383:n%4;const SheetCell *cell=cells.cells+n;
            CHECK(cell->row==row&&cell->col==col&&cell->text,"Sparse exact coordinate/order or text ownership changed");
            if(fixture==2&&last){CHECK(grid_repeat(cell->text,'x',65536),"Exact65536-byte cell text was truncated/borrowed");grid.long_cell=true;}else CHECK(same(cell->text,values[n%4]),"Cell empty/literal/UTF-8/quoted bytes changed");
            bytes+=strlen(cell->text)+1;
        }
        CHECK(cells.text_bytes==bytes,"Cell owned text accounting dropped terminators");
        CHECK(!grid_find(cells,DX_SHEET_ROWS-2,16383),"Missing sparse coordinate was fabricated");grid.missing_absent=true;
        if(count){const char *empty=grid_find(cells,0,0);CHECK(empty&&!*empty,"Actual empty TEXT cell became missing/SQL NULL");grid.empty_present=true;}
    }
    WorkbookCodecCellsView out,saved;memset(&out,0x5a,sizeof(out));memcpy(&saved,&out,sizeof(saved));CHECK(!workbook_codec_header_cells(h,sheets,&out)&&!memcmp(&out,&saved,sizeof(out))&&!workbook_codec_header_cells(NULL,0,&out)&&!memcmp(&out,&saved,sizeof(out))&&!workbook_codec_header_cells(h,0,NULL),"Invalid sparse getter changed output");return true;
}
static uint64_t grid_header_identity(const WorkbookCodecHeader *h) {
    uint64_t hash=UINT64_C(1469598103934665603);if(!h)return hash;
    for(size_t sheet=0;sheet<workbook_codec_header_sheet_count(h);sheet++) {
        const Analysis *a=workbook_codec_header_analysis(h,sheet);const SheetView *v=workbook_codec_header_view(h,sheet);WorkbookCodecCellsView cells={0};if(!a||!v||!workbook_codec_header_cells(h,sheet,&cells))return 0;
        const void *pointers[]={a,a->path,workbook_codec_header_name(h,sheet),workbook_codec_header_reference(h,sheet),v,v->columns,v->widths,cells.cells};for(size_t n=0;n<sizeof(pointers)/sizeof(*pointers);n++)hash=(hash^(uintptr_t)pointers[n])*UINT64_C(1099511628211);
        for(size_t n=0;n<v->column_count;n++)hash=(hash^(uintptr_t)v->columns[n])*UINT64_C(1099511628211);for(size_t n=0;n<cells.count;n++)hash=(hash^(uintptr_t)cells.cells[n].text)*UINT64_C(1099511628211);
    }
    return hash;
}
static bool grid_prior_unchanged(void) {CHECK(d.header==d.prior&&workbook_codec_header_owned_bytes(d.header)==d.prior_header_bytes&&grid_header_identity(d.header)==grid.prior_identity,"Independent grid Header/string/array allocation identities changed");return !d.header||grid_header_contents(d.header,grid.prior_fixture,&d.prior_key);}
static bool grid_begin(void) {grid.prior_fixture=grid.output_fixture;grid.prior_identity=grid_header_identity(d.header);return begin_read();}
/* A stream owns SQL/descriptors/binding until done/cancelled; only its current
   copied Page/atoms/pressure are retired between synchronous ACK callbacks. */
static void grid_clear_chunk(void) {page_free(&d.page);free(d.atoms);d.atoms=NULL;d.atom_bytes=0;free(d.pressure);d.pressure=NULL;d.pressure_bytes=0;grid.incoming_wire=grid.incoming_strings=0;}
static bool grid_atoms(unsigned index) {
    const Page *schema=grid_schema(index);const WorkbookCodecTable *table=workbook_codec_table_find(grid_name(index));CHECK(d.page.column_count==schema->column_count&&!d.atoms,"Grid chunk original shape/prior atoms changed");
    size_t bytes=d.page.row_count*d.page.column_count*sizeof(*d.atoms);CHECK(grid_observe(bytes),"Streamed grid atom allocation exceeds admission");if(bytes)d.atoms=dx_alloc(bytes);d.atom_bytes=bytes;const char *types=table->types;
    for(size_t c=0;c<d.page.column_count;c++) {
        while(*types==' ')types++;WorkbookCodecAtomKind kind=atom_kind(*types++);const Column *actual=d.page.columns+c,*expected=schema->columns+c;
        CHECK(same(actual->name,expected->name)&&same(actual->type_name,expected->type_name)&&actual->type==expected->type&&actual->number_type.kind==expected->number_type.kind&&actual->number_type.precision==expected->number_type.precision&&actual->number_type.scale==expected->number_type.scale,"Original grid metadata type sidecar changed");d.columns[c]=(WorkbookCodecColumn){actual->name,kind};
        for(size_t row=0;row<d.page.row_count;row++){const Cell *cell=d.page.cells+row*d.page.column_count+c;WorkbookCodecAtom *a=d.atoms+row*d.page.column_count+c;a->kind=cell->is_null?WORKBOOK_CODEC_NULL:kind;if(cell->is_null)continue;CHECK(cell->text,"Exact grid atom text missing");
            if(kind==WORKBOOK_CODEC_I32)CHECK(i32(cell->text,&a->value.i32),"Grid INTEGER not exact");else if(kind==WORKBOOK_CODEC_U64)CHECK(u64(cell->text,&a->value.u64),"Grid UBIGINT not exact");else if(kind==WORKBOOK_CODEC_DOUBLE){char *end=NULL;a->value.real=strtod(cell->text,&end);CHECK(end&&end!=cell->text&&!*end,"Grid DOUBLE text not exact");}else if(kind==WORKBOOK_CODEC_BOOLEAN){CHECK(!strcmp(cell->text,"true")||!strcmp(cell->text,"false"),"Base grid BOOLEAN text invalid");a->value.boolean=!strcmp(cell->text,"true");}else a->value.text=(WorkbookCodecText){cell->text,strlen(cell->text)};
        }
    }
    return true;
}
static bool grid_base_send(unsigned index) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(index<2&&request&&request->family==(index?WORKBOOK_CODEC_SHEETS:WORKBOOK_CODEC_META)&&!request->first_row,"Grid base cursor changed");
    if(index)d.sheets_queries++;else d.meta_queries++;return send_sql(browser_page_sql(request->sql,grid_schema(index),0,&d.error),grid_schema(index),(unsigned)request->row_limit,false);
}
static bool grid_base_feed(unsigned index) {
    CHECK(d.page.row_count==(index?(d.fixture==3?2u:3u):1u)&&!d.page.offset&&!d.page.count_pending&&d.page.total==d.page.row_count,"Grid base result completeness changed");CHECK(grid_atoms(index),"Grid base exact atoms failed");WorkbookCodecBatch batch={d.columns,d.page.column_count,d.atoms,d.page.row_count,0,true};CHECK(grid_observe(0),"Base grid feed coexistence exceeds admission");CHECK(workbook_codec_read_feed(d.read,d.key,index?WORKBOOK_CODEC_SHEETS:WORKBOOK_CODEC_META,&batch,retained(),&d.error)==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Grid base feed rejected");return true;
}
static bool grid_start(unsigned family) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);const Page *schema=grid_schema(family+2);CHECK(family<2&&request&&request->family==(family?WORKBOOK_CODEC_CELLS:WORKBOOK_CODEC_COLUMNS)&&!request->first_row&&request->column_count==4&&request->row_limit==(family?1280001u:1048577u),"Streamed grid family cursor/envelope changed");
    const char *expected=family?"SELECT sheet_id,row_id,column_id,text FROM _duxcel_workspace.cells ORDER BY sheet_id,row_id,column_id LIMIT 1280001":"SELECT sheet_id,column_id,name,width FROM _duxcel_workspace.columns ORDER BY sheet_id,column_id LIMIT 1048577";CHECK(!strcmp(request->sql,expected),"Grid family SQL identity changed");
    char *relation=dx_strdup(request->sql),*changed=NULL;
    if(!family&&d.fault==1)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=1 AND column_id=27 THEN 1200.0000001::DOUBLE ELSE width END AS width) FROM (%s) AS _grid_width",relation);
    else if(!family&&d.fault==2)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=1 AND column_id=27 THEN NULL::VARCHAR ELSE name END AS name) FROM (%s) AS _grid_name",relation);
    else if(family&&d.fault==3)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=1 AND row_id=1099511627775::UBIGINT AND column_id=16383 THEN NULL::VARCHAR ELSE text END AS text) FROM (%s) AS _grid_text",relation);
    else if(family&&d.fault==4)changed=dx_printf("SELECT * REPLACE (CASE WHEN sheet_id=1 AND row_id=1099511627775::UBIGINT AND column_id=16383 THEN 6::UBIGINT ELSE row_id END AS row_id,CASE WHEN sheet_id=1 AND row_id=1099511627775::UBIGINT AND column_id=16383 THEN 2::INTEGER ELSE column_id END AS column_id) FROM (%s) AS _grid_duplicate",relation);
    if(changed){CHECK(grid_observe(text_bytes(relation)+text_bytes(changed)),"Grid fault SQL staging exceeds admission");free(relation);relation=changed;}char *sql=dx_strdup("SELECT ");for(size_t c=0;c<4;c++)dx_append(&sql,"%sCAST(%s AS VARCHAR) AS %s",c?",":"",schema->columns[c].name,schema->columns[c].name);dx_append(&sql," FROM (%s) AS _grid_projection",relation);CHECK(strlen(sql)<=DX_BROWSER_SQL_BYTES&&grid_observe(text_bytes(relation)+2u*text_bytes(sql)),"Grid canonical projection staging exceeds admission");free(relation);
    CHECK(!d.waiting&&!grid.active&&d.bindings&&d.ticket<UINT_MAX,"Overlapping/unowned grid stream request");free(d.sql);d.sql=sql;free(d.descriptors);d.descriptors=descriptors(schema);d.ticket++;d.queries++;if(!d.fault)d.normal_queries[d.fixture]++;d.waiting=true;d.ready=false;
    grid.active=true;grid.finished=grid.rejected=grid.end_acked=false;grid.family=family;grid.sequence=grid.stream_acks=0;grid.rows=0;grid.streams[family]++;CHECK(grid_observe(0),"Grid stream request buffers exceed admission");grid_stream_submit(d.cookie,d.ticket,d.sql,d.descriptors,d.bindings,family,(unsigned)request->row_limit,family&&d.fault==6?2:0);return true;
}
static bool grid_chunk_body(unsigned sequence,const char *first,int end,const void *wire,size_t size,const char *total,int pending) {
    CHECK(grid.active&&d.waiting&&!grid.finished&&!grid.rejected&&!grid.end_acked,"Unexpected/duplicate active grid chunk");uint64_t offset=0,count=0;size_t first_bytes=0,total_bytes=0;
    CHECK(grid_u64(first,&offset,&first_bytes)&&grid_u64(total,&count,&total_bytes)&&offset==grid.rows&&sequence==grid.sequence+1&&end>=0&&end<=1&&pending==!end,"Grid stream scalar identity/sequence/completeness changed");CHECK(size<=DX_PAGE_WIRE_BYTES&&wire&&!d.page.bytes&&!d.atoms,"Unretired/oversized incoming grid wire");
    grid.incoming_wire=size;grid.incoming_strings=first_bytes+total_bytes;CHECK(grid_observe(0),"Incoming grid wire/u64 strings exceed admission");size_t current=retained()+workbook_codec_read_owned_bytes(d.read);CHECK(page_from_wire_owned(&d.page,wire,size,offset,count,pending!=0,INPUT_LIMIT-current,&d.error),d.error?d.error:"Existing DXP1 decoder rejected grid chunk");CHECK(grid_observe(0),"Incoming wire/new owned grid Page coexistence exceeds admission");
    CHECK(d.page.column_count==4&&d.page.row_count<=256&&d.page.offset==offset&&d.page.total==count&&d.page.count_pending==!end&&count==offset+d.page.row_count&&((end&&!d.page.row_count)||(!end&&d.page.row_count)),"Grid chunk Page dimensions/end changed");if(end)grid.eofs[grid.family]++;else grid.data[grid.family]++;
    CHECK(grid_atoms(grid.family+2),"Streamed exact grid atoms failed");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);CHECK(request&&request->family==(grid.family?WORKBOOK_CODEC_CELLS:WORKBOOK_CODEC_COLUMNS)&&request->first_row==offset,"Grid stream advanced outside its immutable reader cursor");size_t owned=workbook_codec_read_owned_bytes(d.read);const char *sql=request->sql;
    if(grid.family&&d.fault==5&&!offset&&!end){size_t current=retained()+owned;CHECK(current<HEADER_LIMIT-1u,"Cell budget pressure fixture has no concrete headroom");d.pressure_bytes=HEADER_LIMIT-current-1u;d.pressure=dx_alloc(d.pressure_bytes);memset(d.pressure,0,d.pressure_bytes);CHECK(grid_observe(0),"Concrete cell pressure exceeds input admission");}
    bool reject=!end&&((!grid.family&&(d.fault==1||d.fault==2))||(grid.family&&d.fault>=3&&d.fault<=5));size_t failed=d.fault==5?0:256;
    if(reject&&offset==failed&&d.fault==1){const WorkbookCodecAtom *width=d.atoms+(d.page.row_count-1)*4+3;CHECK(width->kind==WORKBOOK_CODEC_DOUBLE&&width->value.real>1200&&(float)width->value.real==1200,"Late DOUBLE-width witness was narrowed before validation");}
    WorkbookCodecBatch batch={d.columns,4,d.atoms,d.page.row_count,(size_t)offset,end!=0};CHECK(grid_observe(0),"Grid cursor feed coexistence exceeds admission");WorkbookCodecReadStatus status=workbook_codec_read_feed(d.read,d.key,grid.family?WORKBOOK_CODEC_CELLS:WORKBOOK_CODEC_COLUMNS,&batch,retained(),&d.error);
    if(reject&&offset==failed){static const char *const reasons[]={"Invalid ordered column","SQL NULL","SQL NULL","strictly ordered editable","budget"};CHECK(status==WORKBOOK_CODEC_READ_ERROR&&d.error&&strstr(d.error,reasons[d.fault-1]),d.error?d.error:"Expected grid rejection reason missing");CHECK(grid_observe(0),"Grid feed rejection diagnostic exceeds input admission");CHECK(workbook_codec_read_owned_bytes(d.read)==owned&&workbook_codec_read_request(d.read)->first_row==offset&&workbook_codec_read_request(d.read)->sql==sql&&grid_prior_unchanged(),"Rejected streamed grid batch changed prefix/SQL/owned state or prior Header");CHECK(!workbook_codec_read_finish(d.read,&d.header,&d.error)&&d.header==d.prior,"Rejected streamed grid candidate replaced old Header");CHECK(grid_observe(0),"Incomplete grid diagnostic exceeds input admission");grid.rejected=true;grid.rejected_feeds++;grid.first_rows[d.fault-1]=(unsigned)offset;grid.proofs|=1u<<(d.fault-1);d.rejections++;return false;}
    CHECK(status==WORKBOOK_CODEC_READ_ACCEPTED,d.error?d.error:"Valid streamed grid prefix rejected");grid.sequence=sequence;grid.stream_acks++;grid.acks++;grid.rows=count;
    if(end){grid.end_acked=true;grid.accepted_eof[grid.family]++;CHECK(!workbook_codec_read_request(d.read)&&grid_prior_unchanged(),"EOF adoption published Header before stream cleanup/done");}
    else{CHECK(d.page.row_count==256||d.page.row_count==29||d.page.row_count==7,"Unexpected dense grid chunk size");if(d.page.row_count==256)grid.feed256[grid.family]++;else if(d.page.row_count==29)grid.feed29[grid.family]++;else grid.feed7[grid.family]++;
        for(size_t row=0;row<d.page.row_count;row++)for(size_t c=0;c<4;c++){Cell *cell=d.page.cells+row*4+c;if(!cell->is_null){CHECK(cell->text,"Streamed grid input disappeared before poison");memset(cell->text,'~',strlen(cell->text));}}if(d.atoms)memset(d.atoms,0,d.atom_bytes);grid.poisoned[grid.family]++;
    }
    return true;
}
EMSCRIPTEN_KEEPALIVE int dx_browser_codec_stream_chunk(unsigned cookie,unsigned ticket,unsigned sequence,const char *first,int end,const void *wire,size_t size,const char *total,int pending) {
    if(source_versions_mode)return source_versions_stream_chunk(cookie,ticket,sequence,first,end,wire,size,total,pending);
    grid.chunk_calls++;if(d.done||cookie!=d.cookie||ticket!=d.ticket){grid.stale_chunks++;d.stale++;
        /* The held callback is rejected before any pointer borrow. Charge its
           numeric wire size and reserve two maximum canonical-u64 C strings. */
        if(!d.done&&grid.cancel_pending){if(size>DX_PAGE_WIRE_BYTES){stop("Held stale wire exceeded the helper envelope",__LINE__);return 0;}grid.incoming_wire=size;grid.incoming_strings=42;grid_observe(0);grid.incoming_wire=grid.incoming_strings=0;}return 0;
    }bool accepted=grid_chunk_body(sequence,first,end,wire,size,total,pending);grid_clear_chunk();return accepted?1:0;
}
static bool grid_done_body(int status,const char *rows,unsigned chunks,const char *error) {
    CHECK(grid.active&&d.waiting&&!grid.finished&&(status==0||status==2),"Unexpected active stream terminal status");uint64_t count=0;size_t bytes=0;
    if(rows){size_t n=0;while(n<=20&&rows[n])n++;CHECK(n<=20,"Unbounded grid completion row count");grid.incoming_strings=n+1;}
    if(error){size_t n=0;while(n<=2048&&error[n])n++;CHECK(n<=2048,"Unbounded grid completion diagnostic");grid.incoming_strings+=n+1;}CHECK(grid_observe(0),"Final stream scalar/diagnostic ownership exceeds admission");
    if(status==0){CHECK(grid_u64(rows,&count,&bytes)&&count==grid.rows&&chunks==grid.stream_acks&&grid.end_acked&&!grid.rejected,"Grid success lacks actual empty-EOF ACK/retirement");CHECK(!error||!*error,"Successful grid stream carried an error");grid.completed[grid.family]++;}
    else{CHECK(grid.rejected&&!grid.end_acked&&chunks==grid.stream_acks&&grid_prior_unchanged(),"Unexpected grid transport error or changed rejected prefix");grid.errors[grid.family]++;}
    grid.incoming_strings=0;grid.active=false;grid.finished=true;d.waiting=false;return true;
}
EMSCRIPTEN_KEEPALIVE void dx_browser_codec_stream_done(unsigned cookie,unsigned ticket,int status,const char *rows,unsigned chunks,const char *error) {if(source_versions_mode){source_versions_stream_done(cookie,ticket,status,rows,chunks,error);return;}grid.done_calls++;if(d.done||cookie!=d.cookie||ticket!=d.ticket){grid.stale_done++;d.stale++;
    /* No stale-pointer inspection: reserve the helper's maximum row string
       and UTF-8 diagnostic envelope while its TEMP allocations still exist. */
    if(!d.done&&grid.cancel_pending){grid.incoming_strings=21u+2049u;grid_observe(0);grid.incoming_strings=0;}return;}grid_done_body(status,rows,chunks,error);}
static bool grid_held_body(unsigned sequence) {CHECK(grid.active&&d.waiting&&grid.family==1&&d.fault==6&&sequence==2&&grid.sequence==1&&grid.rows==256&&!grid.held,"Unexpected held grid chunk boundary");grid.held=true;grid.held_calls++;return true;}
EMSCRIPTEN_KEEPALIVE void dx_browser_codec_stream_held(unsigned cookie,unsigned ticket,unsigned sequence) {if(source_versions_mode){source_versions_stream_held(cookie,ticket,sequence);return;}if(d.done||cookie!=d.cookie||ticket!=d.ticket){stop("Unexpected stale held-notification",__LINE__);return;}grid_held_body(sequence);}
static bool grid_cancelled_body(const char *error) {
    CHECK(grid.cancel_pending&&grid.active&&d.waiting&&(!error||!*error)&&grid.stale_chunks==1&&grid.stale_done==1,"Grid cancellation did not drain old done/release one stale chunk");const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);
    CHECK(request&&request->family==WORKBOOK_CODEC_CELLS&&request->first_row==256&&request->sql==grid.pending_sql&&workbook_codec_read_owned_bytes(d.read)==grid.pending_owned&&grid_prior_unchanged(),"Cancelled/stale callbacks changed accepted grid prefix/independent Header");grid.cancelled=true;grid.cancel_pending=false;grid.active=false;d.waiting=false;grid.cancelled_calls++;grid.proofs|=32;return true;
}
EMSCRIPTEN_KEEPALIVE void dx_browser_codec_stream_cancelled(unsigned cookie,unsigned ticket,const char *error) {if(source_versions_mode){source_versions_stream_cancelled(cookie,ticket,error);return;}if(d.done||cookie!=d.cookie||ticket!=d.ticket){stop("Unexpected stale cancellation completion",__LINE__);return;}grid_cancelled_body(error);}
static void grid_free_schemas(void) {for(unsigned i=0;i<4;i++)page_free(grid_schema(i));}
static bool grid_restart(void) {CHECK(!grid.active&&!d.waiting&&!grid.incoming_wire&&!grid.incoming_strings,"Grid candidate restarted before stream retirement");clear_input();d.fault++;return grid_begin();}
static bool grid_tick_body(void) {
    if(!d.phase){CHECK(d.fixture<4,"Grid fixture index exceeds inventory");d.cookie++;d.ticket++;d.waiting=true;d.fixture_ready=false;char *attach=dx_printf("ATTACH '%s' AS codec_fixture (READ_ONLY); USE codec_fixture",grid_files[d.fixture]);fixture_start(d.cookie,d.ticket,grid_files[d.fixture],attach,"USE memory; DETACH codec_fixture");free(attach);d.phase=901;return true;}
    if(d.phase==901){if(!d.fixture_ready)return true;d.phase=902;return send_sql(dx_strdup("DESCRIBE SELECT * FROM _duxcel_workspace.meta"),NULL,256,false);}if(d.waiting)return true;
    if(d.phase>=902&&d.phase<=905){unsigned index=d.phase-902;CHECK(d.ready&&!d.error,d.error?d.error:"Original grid DESCRIBE absent");Page schema={0};CHECK(browser_schema_from_describe(&schema,&d.page,&d.error),d.error?d.error:"Original grid schema invalid");CHECK(grid_observe(schema.bytes),"Original grid schema exceeds input admission");if(!source_schema(&schema,workbook_codec_table_find(grid_name(index)),index==0?3:index==1?25:4)){page_free(&schema);return false;}Page *destination=grid_schema(index);page_free(destination);*destination=schema;clear_input();if(index<3){d.phase++;return send_sql(dx_printf("DESCRIBE SELECT * FROM _duxcel_workspace.%s",grid_name(index+1)),NULL,256,false);}for(unsigned i=0;i<4;i++)CHECK(canonical_schema(grid_schema(i),workbook_codec_table_find(grid_name(i))),"Grid canonical typed schema construction failed");CHECK(grid_begin(),"Grid reader begin failed");d.phase=906;}
    if(d.phase==906){d.phase=907;return grid_base_send(0);}
    if(d.phase==907){CHECK(d.ready&&!d.error,d.error?d.error:"Grid meta result absent");CHECK(grid_base_feed(0),"Grid meta feed failed");clear_input();d.phase=908;return grid_base_send(1);}
    if(d.phase==908){CHECK(d.ready&&!d.error,d.error?d.error:"Grid sheets result absent");CHECK(grid_base_feed(1),"Grid sheets feed failed");clear_input();CHECK(workbook_codec_read_add_columns(d.read,true,&d.error),d.error?d.error:"Grid columns arm failed");d.phase=909;return grid_start(0);}
    if(d.phase==909){CHECK(grid.finished&&!grid.active,"Column stream terminal completion absent");if(grid.rejected){CHECK(d.fault==1||d.fault==2,"Unexpected column stream rejection phase");CHECK(grid_restart(),"Grid late-column failure recovery failed");d.phase=906;return true;}clear_input();CHECK(workbook_codec_read_add_cells(d.read,true,&d.error),d.error?d.error:"Grid cells arm failed");d.phase=910;return grid_start(1);}
    if(d.phase==910){
        if(d.fault==6){CHECK(grid.cancelled&&!grid.active,"Held cell stream cancellation completion absent");CHECK(grid_restart(),"Cancelled grid candidate could not restart from meta");d.phase=906;return true;}
        CHECK(grid.finished&&!grid.active,"Cell stream terminal completion absent");if(grid.rejected){CHECK(d.fault>=3&&d.fault<=5,"Unexpected cell stream rejection phase");CHECK(grid_restart(),"Grid late-cell/budget failure recovery failed");d.phase=906;return true;}
        clear_input();CHECK(workbook_codec_read_finish(d.read,&d.header,&d.error),d.error?d.error:"Complete streamed grid Header could not move");grid.output_fixture=d.fixture;CHECK(grid_header_contents(d.header,d.fixture,&d.key),"Owned streamed grid differs from native fixture");d.headers++;size_t bytes=workbook_codec_header_owned_bytes(d.header);CHECK(bytes<=HEADER_LIMIT,"Completed grid Header exceeds its budget");if(bytes>grid.maximum_header)grid.maximum_header=bytes;
        if(!d.fault){d.fixture++;if(d.fixture<4){grid_free_schemas();d.phase=0;return true;}d.fixture=3;d.fault=1;CHECK(grid_begin(),"Grid fault candidate begin failed");d.phase=906;return true;}if(d.fault==7){grid.proofs|=64;CHECK(grid_restart(),"Fresh grid recovery begin failed");d.phase=906;return true;}grid.proofs|=128;
        CHECK(d.fault==8&&d.fixtures==4&&d.headers==6&&d.queries==62&&d.rejections==5&&d.stale==2&&d.meta_queries==12&&d.sheets_queries==12&&d.describe_queries==16,"Grid terminal fixture/query inventory changed");
        const unsigned streams[]={12,10},data[]={21,15},eofs[]={10,6},f256[]={9,6},f29[]={7,3},f7[]={3,3},completed[]={10,6},errors[]={2,3},poisoned[]={19,12},first[]={256,256,256,256,0};
        for(size_t n=0;n<2;n++)CHECK(grid.streams[n]==streams[n]&&grid.data[n]==data[n]&&grid.eofs[n]==eofs[n]&&grid.feed256[n]==f256[n]&&grid.feed29[n]==f29[n]&&grid.feed7[n]==f7[n]&&grid.accepted_eof[n]==eofs[n]&&grid.completed[n]==completed[n]&&grid.errors[n]==errors[n]&&grid.poisoned[n]==poisoned[n],"Grid stream/feed/EOF/owned poison inventory changed");for(size_t n=0;n<5;n++)CHECK(grid.first_rows[n]==first[n],"Grid late failure accepted-prefix changed");for(size_t n=0;n<4;n++)CHECK(d.normal_queries[n]==8,"Normal grid fixture query inventory changed");
        CHECK(grid.chunk_calls==53&&grid.acks==47&&grid.done_calls==22&&grid.stale_chunks==1&&grid.stale_done==1&&grid.held_calls==1&&grid.cancelled_calls==1&&grid.rejected_feeds==5&&grid.proofs==255&&grid.long_column&&grid.long_cell&&grid.empty_present&&grid.missing_absent,"Grid ACK/cancel/recovery/typed ownership proofs incomplete");grid.detached_identity=grid_header_identity(d.header);d.phase=9;d.ticket++;d.waiting=true;d.fixture_ready=false;fixture_start(d.cookie,d.ticket,"","","USE memory; DETACH codec_fixture");return true;
    }
    if(d.phase==9&&d.fixture_ready){clear_input();grid_free_schemas();CHECK(grid_header_contents(d.header,3,&d.key)&&grid_header_identity(d.header)==grid.detached_identity,"Owned grid graph changed after all Page/schema/catalog handles retired");workbook_codec_read_free(d.read);d.read=NULL;workbook_codec_header_free(d.header);d.header=NULL;free(d.bindings);d.bindings=NULL;d.done=true;printf("PASS: browser-codec-grid %u checks,4 native files,6 Headers,62 queries,53 chunks/47 ACKs,5 atomic rejects,1 cancelled held chunk+stale done; no Workbook publication\n",d.checks);grid_report(true,NULL);emscripten_cancel_main_loop();return true;}return true;
}
static void grid_tick(void) {
    if(d.phase==910&&d.fault==6&&grid.held&&!grid.cancel_pending&&!grid.cancelled){const WorkbookCodecRequest *request=workbook_codec_read_request(d.read);if(!request||request->family!=WORKBOOK_CODEC_CELLS||request->first_row!=256){stop("Held cell stream lost accepted first256 prefix",__LINE__);return;}grid.pending_owned=workbook_codec_read_owned_bytes(d.read);grid.pending_sql=request->sql;grid.cancel_pending=true;d.cookie++;d.ticket++;grid_stream_cancel(d.cookie,d.ticket);return;}
    grid_tick_body();
}
