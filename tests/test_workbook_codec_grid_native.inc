/* Native presentation/manual-cell metadata only. No source SQL is executed;
   neither a CellStore/history nor a complete Workbook is built by the cursor. */
#define GRID_LIMIT (2u*1024u*1024u)
static const char *const grid_cell_values[]={"","NULL","İstanbul 🦆","value 'quoted'\t\n"};
static const float grid_widths[]={52,1200,88.125f,73.5f};
static char *grid_repeat(char value,size_t bytes) {char *text=dx_alloc(bytes+1);memset(text,value,bytes);text[bytes]=0;return text;}
static char *grid_column_name(unsigned version,bool large,size_t sheet,size_t column) {CHECK(version==15);
    if(!large&&sheet==1&&column==2)return grid_repeat('c',65537);
    return column%4==0?dx_strdup(""):column%4==1||column%4==2?dx_strdup("NULL"):dx_printf("Column 'İ' %zu:%zu",sheet,column);
}
static const char *grid_cell_at(WorkbookCodecCellsView cells,uint64_t row,size_t col) {for(size_t n=0;n<cells.count;n++)if(cells.cells[n].row==row&&cells.cells[n].col==col)return cells.cells[n].text;return NULL;}
static void grid_fixture(const char *path,const char *missing,unsigned version,bool large) {CHECK(version==15);
    CHECK(access(missing,F_OK)!=0);Workbook book;workbook_init(&book);char *error=NULL;size_t sheets=large?2u:3u;
    for(size_t s=0;s<sheets;s++){
        Analysis blank={.path=dx_strdup("")};char *name=dx_printf("Grid 'İ' %zu",s);CHECK(workbook_add(&book,name,&blank,&error));free(name);analysis_free(&blank);
        Worksheet *w=book.sheets+s;size_t count=large?(s?28u:257u):s?s+2u:0;
        w->view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f,.column_count=count};
        w->view.columns=count?dx_alloc(count*sizeof(char *)):NULL;w->view.widths=count?dx_alloc(count*sizeof(float)):NULL;
        w->cells.cells=count?dx_alloc(count*sizeof(SheetCell)):NULL;w->cells.count=w->cells.capacity=count;
        for(size_t n=0;n<count;n++){
            w->view.columns[n]=grid_column_name(version,large,s,n);w->view.widths[n]=grid_widths[(s+n)%4];
            bool last=s+1==sheets&&n+1==count;char *text=!large&&last?grid_repeat('x',65536):dx_strdup(grid_cell_values[n%4]);
            w->cells.cells[n]=(SheetCell){last?DX_SHEET_ROWS-1:n/4,last?16383:n%4,text};w->cells.bytes+=strlen(text)+1;
        }
        if(count)CHECK(w->cells.cells[0].text&&!*w->cells.cells[0].text&&!strcmp(w->cells.cells[1].text,"NULL")&&!strcmp(w->cells.cells[2].text,"İstanbul 🦆"));
        if(!large&&s==1)CHECK(strlen(w->view.columns[2])==65537);
        if(!large&&s+1==sheets)CHECK(strlen(w->cells.cells[count-1].text)==65536);
        CHECK(!grid_cell_at((WorkbookCodecCellsView){w->cells.cells,w->cells.count,w->cells.bytes},DX_SHEET_ROWS-2,16383)&&!*cells_get(&w->cells,DX_SHEET_ROWS-2,16383));
    }
    book.active=sheets-1;book.user_scale=1.25f;book.sheets[0].formats.source_aware=true;
    CHECK(workbook_save(&book,path,&error)&&!error);workbook_free(&book);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);
    char *sql=dx_printf("SELECT CASE WHEN count(*)=%zu AND count(*) FILTER (WHERE kind=0 AND path='' AND filter_column IS NULL AND source_schema IS NULL AND source_table IS NULL)=%zu THEN true ELSE error('Grid blank source changed') END FROM _duxcel_workspace.sheets",sheets,sheets);execute(c,sql);free(sql);
    execute(c,"SELECT CASE WHEN (SELECT count(*) FROM _duxcel_workspace.cells WHERE text='')>0 AND (SELECT count(*) FROM _duxcel_workspace.cells WHERE text='NULL')>0 AND (SELECT count(*) FROM _duxcel_workspace.cells WHERE text IS NULL)=0 THEN true ELSE error('Grid present empty/literal/required NULL drift') END");
    if(!large)execute(c,"SELECT CASE WHEN (SELECT octet_length(encode(name)) FROM _duxcel_workspace.columns WHERE sheet_id=1 AND column_id=2)=65537 AND (SELECT octet_length(encode(text)) FROM _duxcel_workspace.cells WHERE sheet_id=2 AND row_id=1099511627775 AND column_id=16383)=65536 THEN true ELSE error('Grid long text drift') END");
    duckdb_disconnect(&c);duckdb_close(&db);
}
static const EmitFixture emit_grid_fixtures[]={{"grid-v15.dxls",15,25,false},{"grid-v15-reordered.dxls",15,25,true}};
static const char grid_reorder_sql[]=
    "CREATE TABLE _duxcel_workspace.grid_columns AS SELECT width,name,column_id,sheet_id FROM _duxcel_workspace.columns; DROP TABLE _duxcel_workspace.columns; ALTER TABLE _duxcel_workspace.grid_columns RENAME TO columns;"
    "CREATE TABLE _duxcel_workspace.grid_cells AS SELECT text,column_id,row_id,sheet_id FROM _duxcel_workspace.cells; DROP TABLE _duxcel_workspace.cells; ALTER TABLE _duxcel_workspace.grid_cells RENAME TO cells";
static size_t grid_emit_index;
static void grid_emit_fixture(const char *path,const char *missing,unsigned version) {CHECK(version==15);CHECK(grid_emit_index<2&&emit_grid_fixtures[grid_emit_index].version==version);grid_fixture(path,missing,version,grid_emit_index++==1);}
static int emit_grid(const char *directory) {grid_emit_index=0;return emit_fixture_files(directory,emit_grid_fixtures,2,grid_emit_fixture,grid_reorder_sql,NULL);}
typedef struct {WorkbookCodecHeader *identity;size_t bytes;uint64_t digest;} GridStamp;
static GridStamp grid_stamp(WorkbookCodecHeader *h) {
    GridStamp out={h,workbook_codec_header_owned_bytes(h),UINT64_C(1469598103934665603)};out.digest=object_hash_word(out.digest,workbook_codec_header_version(h));out.digest=object_hash_word(out.digest,workbook_codec_header_sheet_count(h));out.digest=object_hash_word(out.digest,workbook_codec_header_active_sheet(h));out.digest=object_hash_word(out.digest,workbook_codec_header_columns_ready(h));out.digest=object_hash_word(out.digest,workbook_codec_header_cells_ready(h));
    for(size_t s=0;s<workbook_codec_header_sheet_count(h);s++){
        const SheetView *v=workbook_codec_header_view(h,s);const Analysis *a=workbook_codec_header_analysis(h,s);CHECK(v&&a);out.digest=table_hash_analysis(out.digest,a);out.digest=table_hash_text(out.digest,workbook_codec_header_name(h,s));out.digest=table_hash_text(out.digest,workbook_codec_header_reference(h,s));
        out.digest=object_hash_word(out.digest,(uintptr_t)a);out.digest=object_hash_word(out.digest,(uintptr_t)v->columns);out.digest=object_hash_word(out.digest,(uintptr_t)v->widths);out.digest=object_hash_word(out.digest,v->column_count);
        for(size_t n=0;n<v->column_count;n++){uint32_t width=0;memcpy(&width,v->widths+n,sizeof(width));out.digest=object_hash_word(out.digest,width);out.digest=table_hash_text(out.digest,v->columns[n]);out.digest=object_hash_word(out.digest,(uintptr_t)v->columns[n]);}
        if(workbook_codec_header_cells_ready(h)){WorkbookCodecCellsView cells;CHECK(workbook_codec_header_cells(h,s,&cells));out.digest=object_hash_word(out.digest,(uintptr_t)cells.cells);out.digest=object_hash_word(out.digest,cells.count);out.digest=object_hash_word(out.digest,cells.text_bytes);for(size_t n=0;n<cells.count;n++){const SheetCell *c=cells.cells+n;out.digest=object_hash_word(out.digest,c->row);out.digest=object_hash_word(out.digest,c->col);out.digest=object_hash_word(out.digest,(uintptr_t)c->text);out.digest=table_hash_text(out.digest,c->text);}}
    }
    return out;
}
static void grid_kept(WorkbookCodecHeader *h,GridStamp before) {GridStamp after=grid_stamp(h);CHECK(after.identity==before.identity&&after.bytes==before.bytes&&after.digest==before.digest);}
static void grid_pending(WorkbookCodecRead *r,WorkbookCodecHeader **out) {GridStamp old=grid_stamp(*out);char *error=NULL;CHECK(!workbook_codec_read_finish(r,out,&error));clear_error(&error);grid_kept(*out,old);}
static WorkbookCodecRead *grid_begin(duckdb_connection c,unsigned version,const char *path,size_t retained) {CHECK(version==15);
    WorkbookCodecRead *r=NULL;char *error=NULL;CHECK(workbook_codec_read_begin(&r,key,version,WORKBOOK_CODEC_SHEETS_25,GRID_LIMIT,resolve,(void *)path,&error));for(size_t i=0;i<2;i++)CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);return r;
}
static bool grid_arm(WorkbookCodecRead *r,WorkbookCodecFamily family,bool present,char **error) {return family==WORKBOOK_CODEC_COLUMNS?workbook_codec_read_add_columns(r,present,error):workbook_codec_read_add_cells(r,present,error);}
static void grid_compare(const char *path,unsigned version,bool cells_first,bool present,WorkbookCodecHeader **out) {CHECK(version==15&&present);
    FileImage image=file_image(path);Workbook b;workbook_init(&b);char *error=NULL;bool loaded=workbook_load(&b,path,&error);if(!loaded)fprintf(stderr,"Grid native load: %s\n",error?error:"no diagnostic");CHECK(loaded&&!error);
    duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *r=grid_begin(c,version,b.path,retained);
    for(size_t step=0;step<2;step++){
        WorkbookCodecFamily family=(step==0)==cells_first?WORKBOOK_CODEC_CELLS:WORKBOOK_CODEC_COLUMNS;CHECK(grid_arm(r,family,true,&error)&&!error);
        const WorkbookCodecRequest *q=workbook_codec_read_request(r);CHECK(q&&q->family==family&&q->column_count==4&&q->source_column_count==4&&q->row_limit==(family==WORKBOOK_CODEC_COLUMNS?1048577u:1280001u));
        CHECK(!strcmp(q->sql,family==WORKBOOK_CODEC_COLUMNS?"SELECT sheet_id,column_id,name,width FROM _duxcel_workspace.columns ORDER BY sheet_id,column_id LIMIT 1048577":"SELECT sheet_id,row_id,column_id,text FROM _duxcel_workspace.cells ORDER BY sheet_id,row_id,column_id LIMIT 1280001"));grid_pending(r,out);
        unsigned before=executions;CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error&&executions==before+1);size_t count=0;for(size_t s=0;s<b.count;s++)count+=family==WORKBOOK_CODEC_COLUMNS?b.sheets[s].view.column_count:b.sheets[s].cells.count;size_t index=family==WORKBOOK_CODEC_CELLS;
        CHECK(grid_result_rows[index]==count&&grid_batches256[index]==(count==285)&&grid_batches29[index]==(count==285));CHECK(!grid_arm(r,family,true,&error));clear_error(&error);
    }
    CHECK(workbook_codec_read_finish(r,out,&error)&&!error);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);
    CHECK(workbook_codec_header_columns_ready(*out)&&workbook_codec_header_cells_ready(*out)&&!workbook_codec_header_objects_ready(*out)&&!workbook_codec_header_filters_ready(*out)&&!workbook_codec_header_queries_ready(*out)&&workbook_codec_header_version(*out)==version&&workbook_codec_header_sheet_count(*out)==b.count&&workbook_codec_header_active_sheet(*out)==b.active&&workbook_codec_header_user_scale(*out)==b.user_scale);
    for(size_t s=0;s<b.count;s++){
        const Worksheet *w=b.sheets+s;const SheetView *v=workbook_codec_header_view(*out,s),*n=&w->view;table_analysis_compare(workbook_codec_header_analysis(*out,s),&w->analysis);CHECK(!strcmp(workbook_codec_header_name(*out,s),w->name)&&!*workbook_codec_header_reference(*out,s));
        CHECK(v&&v->row==n->row&&v->col==n->col&&v->anchor_row==n->anchor_row&&v->anchor_col==n->anchor_col&&v->end_row==n->end_row&&v->end_col==n->end_col&&v->top==n->top&&v->horizontal==n->horizontal&&v->fraction==n->fraction&&v->selection==n->selection&&v->canvas_zoom==n->canvas_zoom&&v->column_count==n->column_count);
        for(size_t j=0;j<v->column_count;j++)CHECK(!strcmp(v->columns[j],n->columns[j])&&v->widths[j]==n->widths[j]);if(!v->column_count)CHECK(!v->columns&&!v->widths);
        WorkbookCodecCellsView cells;CHECK(workbook_codec_header_cells(*out,s,&cells)&&cells.count==w->cells.count&&cells.text_bytes==w->cells.bytes);for(size_t j=0;j<cells.count;j++)CHECK(cells.cells[j].row==w->cells.cells[j].row&&cells.cells[j].col==w->cells.cells[j].col&&!strcmp(cells.cells[j].text,w->cells.cells[j].text));
        if(!cells.count)CHECK(!cells.cells);else CHECK(grid_cell_at(cells,0,0)&&!*grid_cell_at(cells,0,0)&&cells_get(&w->cells,0,0)&&!*cells_get(&w->cells,0,0));CHECK(!grid_cell_at(cells,DX_SHEET_ROWS-2,16383)&&!grid_cell_at((WorkbookCodecCellsView){w->cells.cells,w->cells.count,w->cells.bytes},DX_SHEET_ROWS-2,16383)&&!*cells_get(&w->cells,DX_SHEET_ROWS-2,16383));
    }
    WorkbookCodecCellsView invalid;memset(&invalid,0x5a,sizeof(invalid));WorkbookCodecCellsView unchanged=invalid;CHECK(!workbook_codec_header_cells(*out,b.count,&invalid)&&!memcmp(&invalid,&unchanged,sizeof(invalid))&&!workbook_codec_header_cells(*out,0,NULL));
    GridStamp final=grid_stamp(*out);workbook_free(&b);grid_kept(*out,final);FileImage after=file_image(path);CHECK(image.bytes==after.bytes&&!memcmp(image.data,after.data,image.bytes));free(image.data);free(after.data);
}
static void grid_rejected(const char *path,unsigned version,WorkbookCodecFamily family,bool present,size_t first,bool native_duplicate,WorkbookCodecHeader **out) {CHECK(version==15);
    FileImage image=file_image(path);Workbook b;workbook_init(&b);Analysis blank={0};char *error=NULL;CHECK(workbook_add(&b,"Retained native",&blank,&error));Workbook before=b;
    if(native_duplicate){CHECK(workbook_load(&b,path,&error)&&!error&&b.sheets[1].cells.count==29);/* Historical CTAS duplicates are accepted by native, outside this strict known-writer decoder. */}
    else {CHECK(!workbook_load(&b,path,&error)&&error&&!memcmp(&b,&before,sizeof(b))&&!strcmp(b.sheets[0].name,"Retained native"));clear_error(&error);}
    char *target=dx_absolute_path(path);CHECK(target);duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);WorkbookCodecRead *r=grid_begin(c,version,target,retained);size_t owned=workbook_codec_read_owned_bytes(r);GridStamp old=grid_stamp(*out);
    if(!present){CHECK(!grid_arm(r,family,false,&error)&&error&&!workbook_codec_read_request(r)&&workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);}
    else {CHECK(grid_arm(r,family,true,&error));unsigned before_count=executions;CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error);CHECK(executions==before_count+1&&grid_last_family==family&&grid_last_first==first&&workbook_codec_read_request(r)->first_row==first&&workbook_codec_read_owned_bytes(r)==grid_last_owned);clear_error(&error);grid_pending(r,out);}
    grid_kept(*out,old);workbook_codec_read_free(r);duckdb_disconnect(&c);duckdb_close(&db);free(target);workbook_free(&b);grid_kept(*out,old);FileImage after=file_image(path);CHECK(image.bytes==after.bytes&&!memcmp(image.data,after.data,image.bytes));free(image.data);free(after.data);
}
static void grid_input_faults(const char *path,WorkbookCodecHeader **out) {
    FileImage image=file_image(path);char *target=dx_absolute_path(path);CHECK(target);duckdb_database db=NULL;duckdb_connection c=NULL;open_readonly(path,&db,&c);size_t retained=workbook_codec_header_owned_bytes(*out);char *error=NULL;GridStamp old=grid_stamp(*out);
    for(size_t index=0;index<2;index++){
        WorkbookCodecFamily family=index?WORKBOOK_CODEC_CELLS:WORKBOOK_CODEC_COLUMNS;WorkbookCodecRead *r=grid_begin(c,15,target,retained);CHECK(grid_arm(r,family,true,&error));
        const char *column_changes[]={"NULL::VARCHAR AS name","width::VARCHAR AS width","NULL::INTEGER AS sheet_id","CAST('51.999999999999993' AS DOUBLE) AS width","CAST('1200.0000000000002' AS DOUBLE) AS width","CAST('NaN' AS DOUBLE) AS width"};
        const char *cell_changes[]={"NULL::VARCHAR AS text","row_id::VARCHAR AS row_id","NULL::INTEGER AS column_id","CAST(1099511627776 AS UBIGINT) AS row_id","16384::INTEGER AS column_id"};size_t count=index?5u:6u;
        for(size_t i=0;i<count;i++){
            char *bad=dx_printf("SELECT * REPLACE (%s) FROM (%s)",index?cell_changes[i]:column_changes[i],workbook_codec_read_request(r)->sql);size_t owned=workbook_codec_read_owned_bytes(r);
            CHECK(result_feed(c,r,bad,256,false,retained,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&grid_last_first==0&&workbook_codec_read_owned_bytes(r)==owned);clear_error(&error);free(bad);grid_kept(*out,old);grid_pending(r,out);
        }
        size_t owned=workbook_codec_read_owned_bytes(r);CHECK(owned+retained+65536<GRID_LIMIT);size_t bytes=GRID_LIMIT-owned-retained-65536;unsigned char *pressure=dx_alloc(bytes);memset(pressure,0,bytes);
        CHECK(result_feed(c,r,NULL,256,true,retained+bytes,&error)==WORKBOOK_CODEC_READ_ERROR&&error&&strstr(error,"budget")&&workbook_codec_read_owned_bytes(r)==owned&&!workbook_codec_read_request(r)->first_row);clear_error(&error);free(pressure);grid_kept(*out,old);
        CHECK(result_feed(c,r,NULL,256,true,retained,&error)==WORKBOOK_CODEC_READ_ACCEPTED&&!error);workbook_codec_read_free(r);grid_kept(*out,old);
    }
    duckdb_disconnect(&c);duckdb_close(&db);free(target);FileImage after=file_image(path);CHECK(image.bytes==after.bytes&&!memcmp(image.data,after.data,image.bytes));free(image.data);free(after.data);
}
static int grid_only(void) {
    char temporary[]="/tmp/duxcel-codec-grid-XXXXXX";CHECK(mkdtemp(temporary));char *directory=dx_absolute_path(temporary);CHECK(directory);char *missing=dx_printf("%s/missing-source.csv",directory);CHECK(access(missing,F_OK)!=0);WorkbookCodecHeader *header=NULL;char *paths[2];
    for(size_t i=0;i<2;i++){
        const EmitFixture *entry=emit_grid_fixtures+i;paths[i]=dx_printf("%s/%s",directory,entry->name);grid_fixture(paths[i],missing,entry->version,i==1);if(entry->reordered){mutate(paths[i],reorder_sql);mutate(paths[i],grid_reorder_sql);}grid_compare(paths[i],entry->version,i%2!=0,true,&header);
    }
    grid_input_faults(paths[0],&header);FileImage large=file_image(paths[1]);char *path=dx_printf("%s/private.dxls",directory);
    for(unsigned version=15;version<=15;version++){
        FileImage image=file_image(paths[0]);options_copy_image(path,image);mutate(path,"DROP TABLE _duxcel_workspace.columns");grid_rejected(path,version,WORKBOOK_CODEC_COLUMNS,false,0,false,&header);CHECK(!unlink(path));
        options_copy_image(path,image);mutate(path,"DROP TABLE _duxcel_workspace.cells");grid_rejected(path,version,WORKBOOK_CODEC_CELLS,false,0,false,&header);CHECK(!unlink(path));free(image.data);
    }
    const char *late_columns[]={"UPDATE _duxcel_workspace.columns SET column_id=28 WHERE sheet_id=1 AND column_id=27","UPDATE _duxcel_workspace.columns SET width=CAST('51.999999999999993' AS DOUBLE) WHERE sheet_id=1 AND column_id=27","UPDATE _duxcel_workspace.columns SET name=NULL WHERE sheet_id=1 AND column_id=27","UPDATE _duxcel_workspace.columns SET name='a'||chr(0)||'b' WHERE sheet_id=1 AND column_id=27"};
    for(size_t i=0;i<sizeof(late_columns)/sizeof(*late_columns);i++){options_copy_image(path,large);mutate(path,late_columns[i]);grid_rejected(path,15,WORKBOOK_CODEC_COLUMNS,true,256,false,&header);CHECK(!unlink(path));}
    const char *late_cells[]={"UPDATE _duxcel_workspace.cells SET text=NULL WHERE sheet_id=1 AND row_id=1099511627775","UPDATE _duxcel_workspace.cells SET row_id=1099511627776 WHERE sheet_id=1 AND row_id=1099511627775","UPDATE _duxcel_workspace.cells SET column_id=16384 WHERE sheet_id=1 AND row_id=1099511627775","UPDATE _duxcel_workspace.cells SET text=repeat('x',65537) WHERE sheet_id=1 AND row_id=1099511627775","UPDATE _duxcel_workspace.cells SET text='a'||chr(0)||'b' WHERE sheet_id=1 AND row_id=1099511627775"};
    for(size_t i=0;i<5;i++){options_copy_image(path,large);mutate(path,late_cells[i]);grid_rejected(path,15,WORKBOOK_CODEC_CELLS,true,256,false,&header);CHECK(!unlink(path));}
    /* Native's old CTAS reader permits duplicates; the strict known-writer
       component intentionally rejects them instead of claiming full parity. */
    grid_fixture(path,missing,15,true);mutate(path,"CREATE TABLE _duxcel_workspace.grid_duplicates AS SELECT * FROM _duxcel_workspace.cells UNION ALL SELECT * FROM _duxcel_workspace.cells WHERE sheet_id=1 AND row_id=1099511627775; DROP TABLE _duxcel_workspace.cells; ALTER TABLE _duxcel_workspace.grid_duplicates RENAME TO cells");grid_rejected(path,15,WORKBOOK_CODEC_CELLS,true,256,true,&header);CHECK(!unlink(path));
    options_copy_image(path,large);mutate(path,"DELETE FROM _duxcel_workspace.columns; DELETE FROM _duxcel_workspace.cells");grid_compare(path,15,false,true,&header);CHECK(!unlink(path));grid_compare(paths[1],15,false,true,&header);
    free(large.data);free(path);workbook_codec_header_free(header);for(size_t i=0;i<2;i++){CHECK(!unlink(paths[i]));free(paths[i]);}CHECK(nul_cells==2&&access(missing,F_OK)!=0);free(missing);CHECK(!rmdir(directory));free(directory);
    printf("PASS: native grid metadata %u checks; %u cursor metadata executions/%u typed feeds; exact column widths/names and sparse cells, both285-row split256+29, present empty/missing/literal/UTF8,65537-byte column name/65536-byte cell, closed-handle parity and atomic failure/budget/stale/recovery; historical duplicate-cell exclusion explicit; native comparison only, no Worker/App Open claim; engine %s\n",checks,executions,feeds,duckdb_library_version());return 0;
}
#undef GRID_LIMIT
