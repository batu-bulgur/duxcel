#include "workbook/workbook_codec_read.h"
#include "platform/platform.h"
#include <duckdb.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned checks,executions,feeds,nul_cells,preference_result_rows;
static unsigned query_result_rows,query_null_rows,query_batches256,query_batches64;
static size_t query_last_owned,query_last_first;
static unsigned option_result_rows,option_null_rows,option_batches256,option_batches64;
static size_t option_last_owned,option_last_first;
static bool option_boolean_schema;
static unsigned table_result_rows[3],table_batches[3],table_batches256[3];
static size_t table_last_owned,table_last_first;
static WorkbookCodecFamily table_last_family;
static unsigned pivot_result_rows[2],pivot_batches[2],pivot_batches256[2];
static size_t pivot_last_owned,pivot_last_first;
static WorkbookCodecFamily pivot_last_family;
static unsigned filter_result_rows,filter_batches256,filter_batches29;
static size_t filter_last_owned,filter_last_first;
static unsigned grid_result_rows[2],grid_batches256[2],grid_batches29[2];
static size_t grid_last_owned,grid_last_first;
static WorkbookCodecFamily grid_last_family;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL native workbook header:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const WorkbookCodecKey key={UINT64_MAX,UINT64_C(9007199254740993),UINT64_C(9007199254740994)};
static void clear_error(char **error) { CHECK(*error && **error); free(*error); *error=NULL; }
static void execute(duckdb_connection connection,const char *sql) {
    duckdb_result result={0};
    if (duckdb_query(connection,sql,&result)!=DuckDBSuccess) { fprintf(stderr,"fixture SQL: %s\n",duckdb_result_error(&result)); exit(1); }
    duckdb_destroy_result(&result);
}
typedef struct { unsigned char *data; size_t bytes; } FileImage;
static FileImage file_image(const char *path) {
    FILE *f=fopen(path,"rb"); CHECK(f); CHECK(!fseek(f,0,SEEK_END)); long bytes=ftell(f); CHECK(bytes>0 && bytes<=64*1024*1024);
    CHECK(!fseek(f,0,SEEK_SET)); FileImage image={dx_alloc((size_t)bytes),(size_t)bytes};
    CHECK(fread(image.data,1,image.bytes,f)==image.bytes && !ferror(f) && !fclose(f)); return image;
}
static bool resolve(void *context,SourceKind kind,const char *reference,size_t maximum,WorkbookCodecOwnedText *out) {
    CHECK(kind>=SOURCE_CSV && kind<=SOURCE_DATABASE);
    char *platform=workbook_source_path(context,reference); CHECK(platform);
    size_t bytes=strlen(platform);
    if (bytes>=maximum) { free(platform); return false; }
    /* Platform paths need not expose their allocation capacity to the codec. */
    out->data=dx_strdup(platform); out->bytes=bytes; free(platform); return true;
}
static WorkbookCodecAtomKind atom_kind(duckdb_type type) {
    switch (type) {
    case DUCKDB_TYPE_INTEGER:return WORKBOOK_CODEC_I32;
    case DUCKDB_TYPE_UBIGINT:return WORKBOOK_CODEC_U64;
    case DUCKDB_TYPE_DOUBLE:return WORKBOOK_CODEC_DOUBLE;
    case DUCKDB_TYPE_BOOLEAN:return WORKBOOK_CODEC_BOOLEAN;
    case DUCKDB_TYPE_VARCHAR:return WORKBOOK_CODEC_TEXT;
    default:CHECK(false); return WORKBOOK_CODEC_NULL;
    }
}
/* Each owned result is read only through chunks/vectors; prepared-statement
   schema APIs are separate. No duckdb_value_*, row_count, column_data/nullmask
   call is mixed into this path. VARCHAR byte lengths come from string_t. */
static WorkbookCodecReadStatus result_feed(duckdb_connection connection,WorkbookCodecRead *read,const char *override,
    size_t split,bool stale_first,size_t extra_retained,char **error) {
    const WorkbookCodecRequest *request=workbook_codec_read_request(read); CHECK(request && split && split<=256);
    WorkbookCodecFamily family=request->family; size_t row_limit=request->row_limit; const char *sql=override ? override : request->sql;
    bool table_family=family>=WORKBOOK_CODEC_TABLE_SOURCES && family<=WORKBOOK_CODEC_TABLE_SORTS;
    size_t table_index=table_family ? (size_t)(family-WORKBOOK_CODEC_TABLE_SOURCES) : 0;
    if (table_family) table_result_rows[table_index]=table_batches[table_index]=table_batches256[table_index]=0;
    bool pivot_family=family==WORKBOOK_CODEC_PIVOT_FIELDS || family==WORKBOOK_CODEC_MARKS;
    size_t pivot_index=family==WORKBOOK_CODEC_MARKS ? 1u : 0u;
    if(pivot_family)pivot_result_rows[pivot_index]=pivot_batches[pivot_index]=pivot_batches256[pivot_index]=0;
    bool grid_family=family==WORKBOOK_CODEC_COLUMNS || family==WORKBOOK_CODEC_CELLS;
    size_t grid_index=family==WORKBOOK_CODEC_CELLS ? 1u : 0u;
    if(grid_family)grid_result_rows[grid_index]=grid_batches256[grid_index]=grid_batches29[grid_index]=0;
    if (family==WORKBOOK_CODEC_FILTERS) filter_result_rows=filter_batches256=filter_batches29=0;
    if (family==WORKBOOK_CODEC_PREFERENCES) preference_result_rows=0;
    if (family==WORKBOOK_CODEC_QUERIES) query_result_rows=query_null_rows=query_batches256=query_batches64=0;
    if (family==WORKBOOK_CODEC_SOURCE_OPTIONS) option_result_rows=option_null_rows=option_batches256=option_batches64=0;
    duckdb_prepared_statement statement=NULL; CHECK(duckdb_prepare(connection,sql,&statement)==DuckDBSuccess);
    size_t columns=(size_t)duckdb_prepared_statement_column_count(statement); CHECK(columns<=25);
    WorkbookCodecColumn schema[25]={0}; duckdb_type types[25]={0}; size_t schema_bytes=sizeof(schema)+strlen(sql)+1;
    for (size_t c=0;c<columns;c++) {
        schema[c].name=duckdb_prepared_statement_column_name(statement,c); CHECK(schema[c].name);
        types[c]=duckdb_prepared_statement_column_type(statement,c); schema[c].kind=atom_kind(types[c]); schema_bytes+=strlen(schema[c].name)+1;
    }
    if (family==WORKBOOK_CODEC_SOURCE_OPTIONS) option_boolean_schema=columns==3 && types[2]==DUCKDB_TYPE_BOOLEAN;
    duckdb_result result={0}; CHECK(duckdb_execute_prepared(statement,&result)==DuckDBSuccess); executions++;
    idx_t chunks=duckdb_result_chunk_count(result); size_t first=request->first_row; WorkbookCodecReadStatus status=WORKBOOK_CODEC_READ_ACCEPTED;
    for (idx_t ci=0;ci<chunks && status==WORKBOOK_CODEC_READ_ACCEPTED;ci++) {
        duckdb_data_chunk chunk=duckdb_result_get_chunk(result,ci); CHECK(chunk && duckdb_data_chunk_get_column_count(chunk)==columns);
        size_t rows=(size_t)duckdb_data_chunk_get_size(chunk); CHECK(rows<=row_limit);
        if(grid_family)grid_result_rows[grid_index]+=(unsigned)rows;
        if (family==WORKBOOK_CODEC_FILTERS) filter_result_rows+=(unsigned)rows;
        if (family==WORKBOOK_CODEC_PREFERENCES) preference_result_rows+=(unsigned)rows;
        if (family==WORKBOOK_CODEC_QUERIES) query_result_rows+=(unsigned)rows;
        if (family==WORKBOOK_CODEC_SOURCE_OPTIONS) option_result_rows+=(unsigned)rows;
        if (table_family) table_result_rows[table_index]+=(unsigned)rows;
        if(pivot_family)pivot_result_rows[pivot_index]+=(unsigned)rows;
        WorkbookCodecAtom *atoms=rows ? dx_alloc(rows*columns*sizeof(*atoms)) : NULL;
        size_t owned=schema_bytes+rows*columns*sizeof(*atoms);
        for (size_t c=0;c<columns;c++) {
            duckdb_vector vector=duckdb_data_chunk_get_vector(chunk,c); CHECK(vector);
            duckdb_logical_type logical=duckdb_vector_get_column_type(vector); CHECK(duckdb_get_type_id(logical)==types[c]); duckdb_destroy_logical_type(&logical);
            const void *data=duckdb_vector_get_data(vector); uint64_t *validity=duckdb_vector_get_validity(vector);
            for (size_t row=0;row<rows;row++) {
                WorkbookCodecAtom *atom=&atoms[row*columns+c];
                if (validity && !duckdb_validity_row_is_valid(validity,row)) {
                    atom->kind=WORKBOOK_CODEC_NULL;if (family==WORKBOOK_CODEC_QUERIES && c==2) query_null_rows++;
                    if (family==WORKBOOK_CODEC_SOURCE_OPTIONS && c==2) option_null_rows++;continue;
                }
                atom->kind=schema[c].kind;
                switch (atom->kind) {
                case WORKBOOK_CODEC_I32:atom->value.i32=((const int32_t *)data)[row];break;
                case WORKBOOK_CODEC_U64:atom->value.u64=((const uint64_t *)data)[row];break;
                case WORKBOOK_CODEC_DOUBLE:atom->value.real=((const double *)data)[row];break;
                case WORKBOOK_CODEC_BOOLEAN:atom->value.boolean=((const bool *)data)[row];break;
                case WORKBOOK_CODEC_TEXT: {
                    duckdb_string_t *strings=(duckdb_string_t *)data; size_t bytes=duckdb_string_t_length(strings[row]);
                    const char *text=duckdb_string_t_data(strings+row); CHECK(text || !bytes);
                    atom->value.text=(WorkbookCodecText){text,bytes}; owned+=bytes;
                    if (bytes && memchr(text,0,bytes)) { CHECK(bytes==3 && text[0]=='a' && text[2]=='b'); nul_cells++; }
                    break;
                }
                default:CHECK(false);
                }
            }
        }
        if (!rows) {
            WorkbookCodecBatch batch={schema,columns,atoms,0,first,ci+1==chunks};
            if (family==WORKBOOK_CODEC_QUERIES) {query_last_owned=workbook_codec_read_owned_bytes(read);query_last_first=first;}
            if (family==WORKBOOK_CODEC_SOURCE_OPTIONS) {option_last_owned=workbook_codec_read_owned_bytes(read);option_last_first=first;}
            if (table_family) {table_last_owned=workbook_codec_read_owned_bytes(read);table_last_first=first;table_last_family=family;}
            if(pivot_family){pivot_last_owned=workbook_codec_read_owned_bytes(read);pivot_last_first=first;pivot_last_family=family;}
            if(family==WORKBOOK_CODEC_FILTERS){filter_last_owned=workbook_codec_read_owned_bytes(read);filter_last_first=first;}
            if(grid_family){grid_last_owned=workbook_codec_read_owned_bytes(read);grid_last_first=first;grid_last_family=family;}
            status=workbook_codec_read_feed(read,key,family,&batch,extra_retained>SIZE_MAX-owned ? SIZE_MAX : owned+extra_retained,error); feeds++;
        }
        for (size_t at=0;at<rows && status==WORKBOOK_CODEC_READ_ACCEPTED;) {
            size_t width=family==WORKBOOK_CODEC_PREFERENCES && at && (split==1 || split==32) ? 256u : split;
            size_t take=rows-at<width ? rows-at : width;
            WorkbookCodecBatch batch={schema,columns,atoms+at*columns,take,first,ci+1==chunks && at+take==rows};
            if (stale_first) {
                size_t before=workbook_codec_read_owned_bytes(read); WorkbookCodecKey stale=key; stale.snapshot_generation--;
                CHECK(workbook_codec_read_feed(read,stale,family,&batch,owned,error)==WORKBOOK_CODEC_READ_STALE && !*error && workbook_codec_read_owned_bytes(read)==before); stale_first=false;
            }
            if (family==WORKBOOK_CODEC_QUERIES) {query_last_owned=workbook_codec_read_owned_bytes(read);query_last_first=first;}
            if (family==WORKBOOK_CODEC_SOURCE_OPTIONS) {option_last_owned=workbook_codec_read_owned_bytes(read);option_last_first=first;}
            if (table_family) {table_last_owned=workbook_codec_read_owned_bytes(read);table_last_first=first;table_last_family=family;}
            if(pivot_family){pivot_last_owned=workbook_codec_read_owned_bytes(read);pivot_last_first=first;pivot_last_family=family;}
            if(family==WORKBOOK_CODEC_FILTERS){filter_last_owned=workbook_codec_read_owned_bytes(read);filter_last_first=first;}
            if(grid_family){grid_last_owned=workbook_codec_read_owned_bytes(read);grid_last_first=first;grid_last_family=family;}
            status=workbook_codec_read_feed(read,key,family,&batch,extra_retained>SIZE_MAX-owned ? SIZE_MAX : owned+extra_retained,error); feeds++;
            if (family==WORKBOOK_CODEC_QUERIES && status==WORKBOOK_CODEC_READ_ACCEPTED) {query_batches256+=take==256;query_batches64+=take==64;}
            if (family==WORKBOOK_CODEC_SOURCE_OPTIONS && status==WORKBOOK_CODEC_READ_ACCEPTED) {option_batches256+=take==256;option_batches64+=take==64;}
            if (table_family && status==WORKBOOK_CODEC_READ_ACCEPTED) {table_batches[table_index]++;table_batches256[table_index]+=take==256;}
            if(pivot_family && status==WORKBOOK_CODEC_READ_ACCEPTED){pivot_batches[pivot_index]++;pivot_batches256[pivot_index]+=take==256;}
            if(family==WORKBOOK_CODEC_FILTERS && status==WORKBOOK_CODEC_READ_ACCEPTED){filter_batches256+=take==256;filter_batches29+=take==29;}
            if(grid_family && status==WORKBOOK_CODEC_READ_ACCEPTED){grid_batches256[grid_index]+=take==256;grid_batches29[grid_index]+=take==29;}
            if (status==WORKBOOK_CODEC_READ_ACCEPTED) first+=take;
            at+=take;
        }
        free(atoms); duckdb_destroy_data_chunk(&chunk);
    }
    if (!chunks) {
        if (family==WORKBOOK_CODEC_QUERIES) {query_last_owned=workbook_codec_read_owned_bytes(read);query_last_first=first;}
        if (family==WORKBOOK_CODEC_SOURCE_OPTIONS) {option_last_owned=workbook_codec_read_owned_bytes(read);option_last_first=first;}
        if (table_family) {table_last_owned=workbook_codec_read_owned_bytes(read);table_last_first=first;table_last_family=family;}
        if(pivot_family){pivot_last_owned=workbook_codec_read_owned_bytes(read);pivot_last_first=first;pivot_last_family=family;}
        if(family==WORKBOOK_CODEC_FILTERS){filter_last_owned=workbook_codec_read_owned_bytes(read);filter_last_first=first;}
        if(grid_family){grid_last_owned=workbook_codec_read_owned_bytes(read);grid_last_first=first;grid_last_family=family;}
        WorkbookCodecBatch batch={schema,columns,NULL,0,first,true}; status=workbook_codec_read_feed(read,key,family,&batch,extra_retained>SIZE_MAX-schema_bytes ? SIZE_MAX : schema_bytes+extra_retained,error); feeds++;
    }
    duckdb_destroy_result(&result); duckdb_destroy_prepare(&statement);
    for (size_t c=0;c<columns;c++) duckdb_free((void *)schema[c].name);
    return status;
}
static bool text_same(const char *a,const char *b) { return a && b ? !strcmp(a,b) : a==b; }
static void compare(const WorkbookCodecHeader *header,const Workbook *native,unsigned version) {
    CHECK(workbook_codec_header_version(header)==version && workbook_codec_header_sheet_count(header)==native->count &&
        workbook_codec_header_active_sheet(header)==native->active && workbook_codec_header_user_scale(header)==native->user_scale);
    const WorkbookCodecKey *stored=workbook_codec_header_key(header); CHECK(stored && stored->document_epoch==key.document_epoch && stored->operation_ticket==key.operation_ticket && stored->snapshot_generation==key.snapshot_generation);
    for (size_t i=0;i<native->count;i++) {
        const Worksheet *sheet=native->sheets+i; const Analysis *a=workbook_codec_header_analysis(header,i),*b=&sheet->analysis;
        const SheetView *v=workbook_codec_header_view(header,i),*w=&sheet->view;
        CHECK(!strcmp(workbook_codec_header_name(header,i),sheet->name)); CHECK(a && v && workbook_codec_header_reference(header,i));
        const char *names[]={"path","schema","table","filter_column","filter_value","group_column","measure_column","sort_column"};
        const char *left[]={a->path,a->schema,a->table,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
        const char *right[]={b->path,b->schema,b->table,b->filter_column,b->filter_value,b->group_column,b->measure_column,b->sort_column};
        for (size_t j=0;j<sizeof(names)/sizeof(*names);j++) if (!text_same(left[j],right[j])) fprintf(stderr,"header/native sheet%zu %s: <%s> / <%s>\n",i,names[j],left[j] ? left[j] : "SQL NULL",right[j] ? right[j] : "SQL NULL");
        CHECK(a->kind==b->kind && text_same(a->path,b->path) && text_same(a->schema,b->schema) && text_same(a->table,b->table) &&
            text_same(a->filter_column,b->filter_column) && text_same(a->filter_value,b->filter_value) && a->filter_op==b->filter_op &&
            text_same(a->group_column,b->group_column) && text_same(a->measure_column,b->measure_column) && a->aggregate==b->aggregate &&
            text_same(a->sort_column,b->sort_column) && a->descending==b->descending);
        CHECK(v->row==w->row && v->col==w->col && v->anchor_row==w->anchor_row && v->anchor_col==w->anchor_col && v->end_row==w->end_row &&
            v->end_col==w->end_col && v->top==w->top && v->horizontal==w->horizontal && v->fraction==w->fraction && v->selection==w->selection && v->canvas_zoom==w->canvas_zoom);
        CHECK(!a->query && !a->source_version && !a->filter_count && !a->sort_count && !a->unique_rows && !v->column_count && !v->columns && !v->widths);
    }
    CHECK(native->sheets[0].analysis.query && strstr(native->sheets[0].analysis.query,"__BP1_SOURCE_MUST_NOT_EXECUTE"));
    CHECK(workbook_codec_header_analysis(header,0)->schema && !*workbook_codec_header_analysis(header,0)->schema);
    CHECK(!strcmp(workbook_codec_header_analysis(header,0)->table,"NULL") && !workbook_codec_header_analysis(header,0)->filter_value);
    CHECK(workbook_codec_header_analysis(header,1)->filter_value && !*workbook_codec_header_analysis(header,1)->filter_value);
}
static void fixture(const char *path,const char *missing,unsigned version) {
    Workbook book; workbook_init(&book); char *error=NULL;
    Analysis a={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),
        .query=dx_strdup("SELECT * FROM __BP1_SOURCE_MUST_NOT_EXECUTE"),.filter_column=dx_strdup("city"),.filter_op=FILTER_NULL,
        .sort_column=dx_strdup("city"),.descending=true};
    CHECK(workbook_add(&book,"Native 'İ' 🦆",&a,&error)); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_NONE,.path=dx_strdup(""),.filter_value=dx_strdup("")};
    CHECK(workbook_add(&book,"Empty string",&a,&error)); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_DEMO,.path=dx_strdup(""),.filter_value=dx_strdup("NULL")};
    CHECK(workbook_add(&book,"Literal NULL",&a,&error)); analysis_free(&a);
    book.active=1; book.user_scale=1.25f;
    for (size_t i=0;i<book.count;i++) book.sheets[i].view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
    if (version==13) book.sheets[0].objects.next_id=UINT64_C(9007199254740994);
    if (version==14) book.sheets[0].formats.source_aware=true;
    CHECK(workbook_save(&book,path,&error) && !error); workbook_free(&book);
}
static void sorts_fixture(const char *path,const char *missing,unsigned version) {
    Workbook book; workbook_init(&book); char *error=NULL;
    for (size_t sheet=0;sheet<64;sheet++) {
        Analysis a={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),
            .query=dx_strdup("SELECT * FROM __BP1_SOURCE_MUST_NOT_EXECUTE")};
        CHECK(analysis_sort_set(&a,"Primary 'İ'",sheet%2!=0,false,&error));
        for (size_t position=0;position<7;position++) {
            size_t ordinal=sheet*7+position; char name[64]; snprintf(name,sizeof(name),"Field 'İ' %zu",ordinal);
            CHECK(analysis_sort_set(&a,name,ordinal%2!=0,true,&error));
        }
        char name[64]; snprintf(name,sizeof(name),"Sorted 'İ' %zu",sheet);
        CHECK(workbook_add(&book,name,&a,&error)); analysis_free(&a);
        book.sheets[sheet].view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
    }
    book.active=63; book.user_scale=1.25f;
    if (version==13) book.sheets[0].objects.next_id=UINT64_C(9007199254740994);
    if (version==14) book.sheets[0].formats.source_aware=true;
    CHECK(workbook_save(&book,path,&error) && !error); workbook_free(&book);
}
static void mutate(const char *path,const char *sql) {
    duckdb_database database=NULL; duckdb_connection connection=NULL; CHECK(duckdb_open(path,&database)==DuckDBSuccess && duckdb_connect(database,&connection)==DuckDBSuccess);
    execute(connection,sql); duckdb_disconnect(&connection); duckdb_close(&database);
}
static void preferences_fixture(const char *path,const char *missing,unsigned version) {
    Workbook book; workbook_init(&book); char *error=NULL;
    for (size_t sheet=0;sheet<64;sheet++) {
        Analysis a={.kind=SOURCE_CSV,.path=dx_strdup(missing),.schema=dx_strdup(""),.table=dx_strdup("NULL"),
            .query=dx_strdup("SELECT * FROM __BP1_SOURCE_MUST_NOT_EXECUTE")};
        char name[64]; snprintf(name,sizeof(name),"Sorted 'İ' %zu",sheet); CHECK(workbook_add(&book,name,&a,&error)); analysis_free(&a);
        book.sheets[sheet].hidden=sheet!=63;
        book.sheets[sheet].view=(SheetView){.row=DX_SHEET_ROWS-1,.col=16383,.anchor_row=1,.anchor_col=2,.end_row=3,.end_col=4,.top=5,.horizontal=120.25f,.fraction=.25,.selection=2,.canvas_zoom=1.5f};
    }
    book.active=63; book.user_scale=1.25f; book.reduced_motion=true;
    if (version==13) book.sheets[0].objects.next_id=UINT64_C(9007199254740994);
    if (version==14) book.sheets[0].formats.source_aware=true;
    CHECK(workbook_save(&book,path,&error) && !error); workbook_free(&book);
    /* Native Save omits visible entries. This explicitly stored false is valid
       metadata and reaches65 rows without changing the active sheet. */
    mutate(path,"INSERT INTO _duxcel_workspace.preferences(name,value) VALUES ('sheet_hidden_63',false)");
}
static const char reorder_sql[]="CREATE TABLE _duxcel_workspace.reordered_sheets AS SELECT canvas_zoom,* EXCLUDE(canvas_zoom) FROM _duxcel_workspace.sheets; DROP TABLE _duxcel_workspace.sheets; ALTER TABLE _duxcel_workspace.reordered_sheets RENAME TO sheets; CREATE TABLE _duxcel_workspace.reordered_meta AS SELECT user_scale,active_sheet,version FROM _duxcel_workspace.meta; DROP TABLE _duxcel_workspace.meta; ALTER TABLE _duxcel_workspace.reordered_meta RENAME TO meta";
static const char sorts_reorder_sql[]="CREATE TABLE _duxcel_workspace.reordered_sorts AS SELECT descending,name,position,sheet_id FROM _duxcel_workspace.sorts; DROP TABLE _duxcel_workspace.sorts; ALTER TABLE _duxcel_workspace.reordered_sorts RENAME TO sorts";
static const char preferences_reorder_sql[]="CREATE TABLE _duxcel_workspace.reordered_preferences AS SELECT value,name FROM _duxcel_workspace.preferences; DROP TABLE _duxcel_workspace.preferences; ALTER TABLE _duxcel_workspace.reordered_preferences RENAME TO preferences";
static const char preferences_unknown_null[]="DELETE FROM _duxcel_workspace.preferences WHERE name='sheet_hidden_63'; INSERT INTO _duxcel_workspace.preferences(name,value) VALUES ('a',NULL)";
static const char preferences_unknown_nul[]="DELETE FROM _duxcel_workspace.preferences WHERE name='sheet_hidden_63'; INSERT INTO _duxcel_workspace.preferences(name,value) VALUES ('a'||chr(0)||'b',false)";

typedef struct { const char *name; unsigned version,layout; bool reordered; } EmitFixture;
static const EmitFixture emit_fixtures[]={
    {"v12.dxls",12,25,false},{"v12-reordered.dxls",12,25,true},{"v12-legacy24.dxls",12,24,true},
    {"v13.dxls",13,25,false},{"v13-reordered.dxls",13,25,true},
    {"v14.dxls",14,25,false},{"v14-reordered.dxls",14,25,true}
};
static const EmitFixture emit_sorts_fixtures[]={
    {"sorts-v12.dxls",12,25,false},{"sorts-v13.dxls",13,25,false},
    {"sorts-v14.dxls",14,25,false},{"sorts-v14-reordered.dxls",14,25,true}
};
static const EmitFixture emit_preferences_fixtures[]={
    {"preferences-v12.dxls",12,25,false},{"preferences-v13.dxls",13,25,false},
    {"preferences-v14.dxls",14,25,false},{"preferences-v14-reordered.dxls",14,25,true},
    {"preferences-v14-unknown-null.dxls",14,25,false},{"preferences-v14-unknown-nul.dxls",14,25,false}
};
static const char *const preferences_post_sql[]={NULL,NULL,NULL,NULL,preferences_unknown_null,preferences_unknown_nul};
static struct {
    char *directory,*missing,*stage,*wal,*outputs[7];
    dev_t devices[7]; ino_t inodes[7];
    bool published[7],complete;
} emission;
/* CHECK and the fixture SQL helper exit on failure. Retire only our private
   staging file and successfully linked output inodes, never a preexisting file. */
static void emit_cleanup(void) {
    if (emission.stage) unlink(emission.stage);
    if (emission.wal) unlink(emission.wal);
    for (size_t i=0;i<7;i++) {
        struct stat st;
        if (!emission.complete && emission.published[i] && !lstat(emission.outputs[i],&st) &&
            st.st_dev==emission.devices[i] && st.st_ino==emission.inodes[i]) unlink(emission.outputs[i]);
        free(emission.outputs[i]);
    }
    free(emission.directory); free(emission.missing); free(emission.stage); free(emission.wal);
}
static void require_absent(const char *path) {
    struct stat st;
    if (!lstat(path,&st) || errno!=ENOENT) { fprintf(stderr,"fixture output must not exist: %s\n",path); exit(1); }
}
static int emit_fixture_files(const char *directory,const EmitFixture *entries,size_t count,
    void (*make_fixture)(const char *,const char *,unsigned),const char *extra_reorder,const char *const *post_sql) {
    CHECK(count<=7);
    char *canonical=dx_absolute_path(directory); struct stat st;
    if (!canonical || stat(canonical,&st) || !S_ISDIR(st.st_mode)) {
        fprintf(stderr,"fixture destination must be an existing accessible directory\n"); free(canonical); return 1;
    }
    CHECK(!atexit(emit_cleanup));
    emission.directory=canonical;
    char *missing=emission.missing=dx_printf("%s/missing-source.csv",canonical); require_absent(missing);
    for (size_t i=0;i<count;i++) {
        emission.outputs[i]=dx_printf("%s/%s",canonical,entries[i].name); require_absent(emission.outputs[i]);
    }
    for (size_t i=0;i<count;i++) {
        const EmitFixture *entry=entries+i;
        /* Same-parent native Save keeps every stored source reference exactly
           missing-source.csv. Publish only after all DuckDB handles are closed. */
        emission.stage=dx_printf("%s/.bp1-codec-stage-XXXXXX",canonical);
        int descriptor=mkstemp(emission.stage);
        if (descriptor<0) { free(emission.stage); emission.stage=NULL; CHECK(false); }
        CHECK(!close(descriptor)); CHECK(!unlink(emission.stage));
        emission.wal=dx_printf("%s.wal",emission.stage);
        make_fixture(emission.stage,missing,entry->version);
        if (entry->reordered) { mutate(emission.stage,reorder_sql); if (extra_reorder) mutate(emission.stage,extra_reorder); }
        if (entry->layout==24) mutate(emission.stage,"ALTER TABLE _duxcel_workspace.sheets DROP COLUMN canvas_zoom");
        if (post_sql && post_sql[i]) mutate(emission.stage,post_sql[i]);
        CHECK(!lstat(emission.stage,&st) && S_ISREG(st.st_mode) && st.st_size>0);
        CHECK(!chmod(emission.stage,0444));
        /* link is an atomic no-replace operation, including a target created
           after preflight; cleanup never claims that conflicting target. */
        if (link(emission.stage,emission.outputs[i])) { fprintf(stderr,"cannot publish fixture: %s\n",emission.outputs[i]); exit(1); }
        emission.published[i]=true; emission.devices[i]=st.st_dev; emission.inodes[i]=st.st_ino;
        CHECK(!unlink(emission.stage)); free(emission.stage); emission.stage=NULL;
        require_absent(emission.wal); free(emission.wal); emission.wal=NULL;
    }
    require_absent(missing);
    /* Protocol metadata only: the consumer hashes and serves the actual closed
       native archives; no workbook data is represented by this JSON. */
    CHECK(fputs("{\"schema\":\"duxcel.codec-fixtures\",\"version\":1,\"files\":[",stdout)>=0);
    for (size_t i=0;i<count;i++) CHECK(fprintf(stdout,"%s{\"name\":\"%s\",\"version\":%u,\"layout\":%u}",i ? "," : "",entries[i].name,entries[i].version,entries[i].layout)>0);
    CHECK(fputs("]}\n",stdout)>=0 && !fflush(stdout)); emission.complete=true; return 0;
}
static void open_readonly(const char *path,duckdb_database *database,duckdb_connection *connection) {
    duckdb_config config=NULL; CHECK(duckdb_create_config(&config)==DuckDBSuccess);
    CHECK(duckdb_set_config(config,"access_mode","READ_ONLY")==DuckDBSuccess && duckdb_set_config(config,"enable_external_access","false")==DuckDBSuccess &&
        duckdb_set_config(config,"autoload_known_extensions","false")==DuckDBSuccess && duckdb_set_config(config,"autoinstall_known_extensions","false")==DuckDBSuccess);
    char *message=NULL;
    if (duckdb_open_ext(path,database,config,&message)!=DuckDBSuccess) { fprintf(stderr,"metadata open: %s\n",message); exit(1); }
    duckdb_free(message); duckdb_destroy_config(&config); CHECK(duckdb_connect(*database,connection)==DuckDBSuccess);
}
static void read_compare(const char *path,unsigned version,WorkbookCodecSheetsLayout layout,bool faults) {
    FileImage before=file_image(path); Workbook native; workbook_init(&native); char *error=NULL;
    CHECK(workbook_load(&native,path,&error) && !error && native.count==3);
    if (version==13) CHECK(native.sheets[0].objects.next_id==UINT64_C(9007199254740994));
    if (version==14) CHECK(native.sheets[0].formats.source_aware && !native.sheets[0].source_versions.count);
    duckdb_database database=NULL; duckdb_connection connection=NULL; open_readonly(path,&database,&connection);
    /* Native loading resolves against the canonical archive path; a caller's
       /tmp symlink spelling is not the locator resolver's document identity. */
    WorkbookCodecRead *read=NULL; CHECK(workbook_codec_read_begin(&read,key,version,layout,8u*1024u*1024u,resolve,native.path,&error));
    if (faults) {
        size_t owned=workbook_codec_read_owned_bytes(read); const char *sql=workbook_codec_read_request(read)->sql;
        char *bad=dx_printf("SELECT * REPLACE (version::VARCHAR AS version) FROM (%s)",sql);
        CHECK(result_feed(connection,read,bad,256,false,0,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_owned_bytes(read)==owned); clear_error(&error); free(bad);
        bad=dx_printf("SELECT * REPLACE (NULL::INTEGER AS version) FROM (%s)",sql);
        CHECK(result_feed(connection,read,bad,256,false,0,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_owned_bytes(read)==owned); clear_error(&error); free(bad);
    }
    CHECK(result_feed(connection,read,NULL,256,true,0,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    if (faults) {
        const char *sql=workbook_codec_read_request(read)->sql; size_t owned=workbook_codec_read_owned_bytes(read); char *bad;
        const char *replace[]={"NULL::VARCHAR AS name","'a'||chr(0)||'b' AS name","0::INTEGER AS id","-1e-300::DOUBLE AS horizontal"};
        for (size_t i=0;i<sizeof(replace)/sizeof(*replace);i++) {
            bad=dx_printf("SELECT * REPLACE (%s) FROM (%s)",replace[i],sql);
            CHECK(result_feed(connection,read,bad,256,false,0,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_owned_bytes(read)==owned && workbook_codec_read_request(read)->first_row==0); clear_error(&error); free(bad);
        }
        CHECK(nul_cells==3); /* Actual chunk VARCHAR length3, never a legacy strlen guess. */
        CHECK(result_feed(connection,read,NULL,256,false,SIZE_MAX,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_owned_bytes(read)==owned); clear_error(&error);
    }
    CHECK(result_feed(connection,read,NULL,1,true,0,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    WorkbookCodecHeader *header=NULL; CHECK(workbook_codec_read_finish(read,&header,&error) && !error); compare(header,&native,version);
    workbook_codec_read_free(read); duckdb_disconnect(&connection); duckdb_close(&database); workbook_free(&native);
    FileImage after=file_image(path); CHECK(after.bytes==before.bytes && !memcmp(after.data,before.data,before.bytes)); free(before.data); free(after.data);
    CHECK(workbook_codec_header_sheet_count(header)==3 && !strcmp(workbook_codec_header_name(header,0),"Native 'İ' 🦆"));
    workbook_codec_header_free(header); free(error);
}
static void kept_sorts_header(WorkbookCodecHeader *actual,const WorkbookCodecHeader *expected,size_t bytes) {
    CHECK(actual==expected && workbook_codec_header_owned_bytes(actual)==bytes);
    if (actual) {
        const Analysis *a=workbook_codec_header_analysis(actual,63);
        CHECK(workbook_codec_header_sorts_ready(actual) && workbook_codec_header_sheet_count(actual)==64 && a &&
            !strcmp(workbook_codec_header_name(actual,63),"Sorted 'İ' 63") && !strcmp(a->sort_column,"Primary 'İ'") &&
            (a->sort_count==0 || (a->sort_count==7 && !strcmp(a->sorts[6].column,"Field 'İ' 447"))));
    }
}
static void pending_sorts_header(WorkbookCodecRead *read,WorkbookCodecHeader **out) {
    WorkbookCodecHeader *identity=*out; size_t bytes=workbook_codec_header_owned_bytes(identity); char *error=NULL;
    CHECK(!workbook_codec_read_finish(read,out,&error)); clear_error(&error); kept_sorts_header(*out,identity,bytes);
}
static void sorts_compare(const char *path,unsigned version,bool present,bool faults,WorkbookCodecHeader **out) {
    FileImage before=file_image(path); Workbook native; workbook_init(&native); char *error=NULL;
    CHECK(workbook_load(&native,path,&error) && !error && native.count==64 && native.active==63);
    if (version==13) CHECK(native.sheets[0].objects.next_id==UINT64_C(9007199254740994));
    if (version==14) CHECK(native.sheets[0].formats.source_aware && !native.sheets[0].source_versions.count);
    duckdb_database database=NULL; duckdb_connection connection=NULL; open_readonly(path,&database,&connection);
    WorkbookCodecRead *read=NULL; CHECK(workbook_codec_read_begin(&read,key,version,WORKBOOK_CODEC_SHEETS_25,8u*1024u*1024u,resolve,native.path,&error));
    size_t old_bytes=workbook_codec_header_owned_bytes(*out);
    CHECK(result_feed(connection,read,NULL,256,true,old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(result_feed(connection,read,NULL,256,true,old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(workbook_codec_read_add_sorts(read,present,&error) && !error);
    if (present) {
        CHECK(workbook_codec_read_request(read)->family==WORKBOOK_CODEC_SORTS && workbook_codec_read_request(read)->row_limit==449);
        pending_sorts_header(read,out);
        if (faults) {
            const char *sql=workbook_codec_read_request(read)->sql;
            const char *replacements[]={
                "position::VARCHAR AS position","NULL::VARCHAR AS name",
                "CASE WHEN sheet_id=0 AND position=1 THEN 6 ELSE position END AS position",
                "CASE WHEN sheet_id=0 AND position=1 THEN 'pRIMARY ''İ''' ELSE name END AS name",
                "CASE WHEN sheet_id=0 AND position=1 THEN 'a'||chr(0)||'b' ELSE name END AS name",
                "CASE WHEN sheet_id=0 AND position=1 THEN '' ELSE name END AS name"
            };
            for (size_t i=0;i<sizeof(replacements)/sizeof(*replacements);i++) {
                size_t owned=workbook_codec_read_owned_bytes(read); char *bad=dx_printf("SELECT * REPLACE (%s) FROM (%s)",replacements[i],sql);
                CHECK(result_feed(connection,read,bad,256,false,old_bytes,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)->first_row);
                clear_error(&error); free(bad); pending_sorts_header(read,out);
            }
            CHECK(nul_cells==1); /* Actual vector byte length3, never strlen. */
            size_t owned=workbook_codec_read_owned_bytes(read); CHECK(owned+old_bytes<8u*1024u*1024u);
            size_t pressure_bytes=8u*1024u*1024u-owned-old_bytes-1; unsigned char *pressure=dx_alloc(pressure_bytes);
            CHECK(result_feed(connection,read,NULL,256,false,old_bytes+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)->first_row);
            clear_error(&error); free(pressure); pending_sorts_header(read,out);
        }
        unsigned old_executions=executions,old_feeds=feeds;
        CHECK(result_feed(connection,read,NULL,256,true,old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
        CHECK(executions==old_executions+1 && feeds==old_feeds+2); /* One448-row result ->256+192, no repeat SQL. */
    }
    CHECK(workbook_codec_read_finish(read,out,&error) && !error && workbook_codec_header_sorts_ready(*out));
    workbook_codec_read_free(read); duckdb_disconnect(&connection); duckdb_close(&database);
    /* The comparison runs after all result/chunk/connection lifetimes ended. */
    CHECK(workbook_codec_header_version(*out)==version && workbook_codec_header_sheet_count(*out)==64 && workbook_codec_header_active_sheet(*out)==63 && workbook_codec_header_user_scale(*out)==1.25f);
    for (size_t i=0;i<native.count;i++) {
        const Analysis *a=workbook_codec_header_analysis(*out,i),*b=&native.sheets[i].analysis;
        CHECK(analysis_sorts_equal(a,b) && a->sort_count==(present ? 7u : 0u) && a->kind==SOURCE_CSV && text_same(a->path,b->path));
        CHECK(!a->query && !a->source_version && !a->filter_count && !a->unique_rows && !strcmp(a->schema,"") && !strcmp(a->table,"NULL") &&
            !strcmp(b->query,"SELECT * FROM __BP1_SOURCE_MUST_NOT_EXECUTE") && !strcmp(workbook_codec_header_reference(*out,i),"missing-source.csv"));
        CHECK(!strcmp(workbook_codec_header_name(*out,i),native.sheets[i].name));
    }
    workbook_free(&native); FileImage after=file_image(path);
    CHECK(before.bytes==after.bytes && !memcmp(before.data,after.data,before.bytes)); free(before.data); free(after.data);
    CHECK(!strcmp(workbook_codec_header_analysis(*out,63)->sort_column,"Primary 'İ'") &&
        (!present || !strcmp(workbook_codec_header_analysis(*out,63)->sorts[6].column,"Field 'İ' 447"))); free(error);
}
static void sorts_archive_rejected(const char *path,bool missing_table,WorkbookCodecHeader **out) {
    FileImage before=file_image(path); Workbook native; workbook_init(&native); char *error=NULL; Analysis blank={0};
    CHECK(workbook_add(&native,"Retained native",&blank,&error)); Workbook old_native=native;
    CHECK(!workbook_load(&native,path,&error) && !memcmp(&native,&old_native,sizeof(native)) && !strcmp(native.sheets[0].name,"Retained native")); clear_error(&error);
    WorkbookCodecHeader *identity=*out; size_t old_bytes=workbook_codec_header_owned_bytes(*out);
    char *target=dx_absolute_path(path); CHECK(target); WorkbookCodecRead *read=NULL;
    CHECK(workbook_codec_read_begin(&read,key,14,WORKBOOK_CODEC_SHEETS_25,8u*1024u*1024u,resolve,target,&error));
    duckdb_database database=NULL; duckdb_connection connection=NULL; open_readonly(path,&database,&connection);
    CHECK(result_feed(connection,read,NULL,256,true,old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(result_feed(connection,read,NULL,256,true,old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    size_t owned=workbook_codec_read_owned_bytes(read);
    if (missing_table) {
        CHECK(!workbook_codec_read_add_sorts(read,false,&error) && workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)); clear_error(&error);
    } else {
        CHECK(workbook_codec_read_add_sorts(read,true,&error)); unsigned old_executions=executions,old_feeds=feeds;
        CHECK(result_feed(connection,read,NULL,256,true,old_bytes,&error)==WORKBOOK_CODEC_READ_ERROR && workbook_codec_read_request(read)->first_row==256 && workbook_codec_read_owned_bytes(read)>owned);
        CHECK(executions==old_executions+1 && feeds==old_feeds+2); clear_error(&error); pending_sorts_header(read,out);
    }
    kept_sorts_header(*out,identity,old_bytes); workbook_codec_read_free(read); free(target);
    duckdb_disconnect(&connection); duckdb_close(&database); workbook_free(&native);
    FileImage after=file_image(path); CHECK(before.bytes==after.bytes && !memcmp(before.data,after.data,before.bytes)); free(before.data); free(after.data); free(error);
}
static int sorts_only(void) {
    char temporary[]="/tmp/duxcel-codec-sorts-XXXXXX"; CHECK(mkdtemp(temporary));
    char *directory=dx_absolute_path(temporary); CHECK(directory);
    char *missing=dx_printf("%s/missing-source.csv",directory); CHECK(access(missing,F_OK)!=0); WorkbookCodecHeader *header=NULL;
    for (size_t i=0;i<sizeof(emit_sorts_fixtures)/sizeof(*emit_sorts_fixtures);i++) {
        const EmitFixture *entry=emit_sorts_fixtures+i; char *path=dx_printf("%s/%s",directory,entry->name); sorts_fixture(path,missing,entry->version);
        if (entry->reordered) { mutate(path,reorder_sql); mutate(path,sorts_reorder_sql); }
        sorts_compare(path,entry->version,true,entry->version==13,&header); CHECK(!unlink(path)); free(path);
    }
    char *path=dx_printf("%s/absent12.dxls",directory); sorts_fixture(path,missing,12); mutate(path,"DROP TABLE _duxcel_workspace.sorts");
    sorts_compare(path,12,false,false,&header); CHECK(!unlink(path)); free(path);
    path=dx_printf("%s/absent14.dxls",directory); sorts_fixture(path,missing,14); mutate(path,"DROP TABLE _duxcel_workspace.sorts");
    sorts_archive_rejected(path,true,&header); CHECK(!unlink(path)); free(path);
    path=dx_printf("%s/sentinel14.dxls",directory); sorts_fixture(path,missing,14); mutate(path,"INSERT INTO _duxcel_workspace.sorts(sheet_id,position,name,descending) VALUES (64,0,'Sentinel',false)");
    sorts_archive_rejected(path,false,&header); mutate(path,"DELETE FROM _duxcel_workspace.sorts WHERE sheet_id=64");
    sorts_compare(path,14,true,false,&header); CHECK(!unlink(path)); free(path); workbook_codec_header_free(header);
    CHECK(access(missing,F_OK)!=0); free(missing); CHECK(!rmdir(directory)); free(directory);
    printf("PASS: native optional sorts %u checks; %u metadata executions/%u typed feeds; four448-row native packages, legacy missing-table default, v14 missing/sentinel rejection, owned prior Header and fault/budget/stale recovery; source definitions never executed; native comparison only, no Worker/App Open claim; engine %s\n",checks,executions,feeds,duckdb_library_version()); return 0;
}
typedef struct { WorkbookCodecHeader *identity; size_t bytes,count; unsigned version; bool ready,motion; uint64_t hidden; } PreferenceHeaderStamp;
static PreferenceHeaderStamp preference_stamp(WorkbookCodecHeader *header) {
    PreferenceHeaderStamp stamp={header,workbook_codec_header_owned_bytes(header),workbook_codec_header_sheet_count(header),
        workbook_codec_header_version(header),workbook_codec_header_preferences_ready(header),workbook_codec_header_reduced_motion(header),0};
    for (size_t i=0;i<stamp.count;i++) if (workbook_codec_header_sheet_hidden(header,i)) stamp.hidden|=UINT64_C(1)<<i;
    return stamp;
}
static void kept_preferences_header(WorkbookCodecHeader *header,PreferenceHeaderStamp before) {
    PreferenceHeaderStamp after=preference_stamp(header);
    CHECK(after.identity==before.identity && after.bytes==before.bytes && after.count==before.count && after.version==before.version && after.ready==before.ready && after.motion==before.motion && after.hidden==before.hidden);
    if (header) CHECK(!strcmp(workbook_codec_header_name(header,63),"Sorted 'İ' 63") && !workbook_codec_header_analysis(header,63)->sort_column);
}
static void pending_preferences_header(WorkbookCodecRead *read,WorkbookCodecHeader **out) {
    PreferenceHeaderStamp before=preference_stamp(*out); char *error=NULL;
    CHECK(!workbook_codec_read_finish(read,out,&error)); clear_error(&error); kept_preferences_header(*out,before);
}
static void preferences_compare(const char *path,unsigned version,bool present,bool faults,WorkbookCodecHeader **out) {
    FileImage before=file_image(path); Workbook native; workbook_init(&native); char *error=NULL;
    CHECK(workbook_load(&native,path,&error) && !error && native.count==64 && native.active==63);
    duckdb_database database=NULL; duckdb_connection connection=NULL; open_readonly(path,&database,&connection);
    WorkbookCodecRead *read=NULL; CHECK(workbook_codec_read_begin(&read,key,version,WORKBOOK_CODEC_SHEETS_25,8u*1024u*1024u,resolve,native.path,&error));
    size_t old_bytes=workbook_codec_header_owned_bytes(*out);
    CHECK(result_feed(connection,read,NULL,256,true,old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(result_feed(connection,read,NULL,256,true,old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(workbook_codec_read_add_preferences(read,present,&error) && !error);
    if (present) {
        CHECK(workbook_codec_read_request(read)->family==WORKBOOK_CODEC_PREFERENCES && workbook_codec_read_request(read)->row_limit==66); pending_preferences_header(read,out);
        if (faults) {
            const char *sql=workbook_codec_read_request(read)->sql;
            const char *replacements[]={"value::INTEGER AS value","NULL::VARCHAR AS name",
                "CASE WHEN name='sheet_hidden_62' THEN 'sheet_hidden_bad' ELSE name END AS name",
                "CASE WHEN name='sheet_hidden_63' THEN true ELSE value END AS value"};
            for (size_t i=0;i<sizeof(replacements)/sizeof(*replacements);i++) {
                size_t owned=workbook_codec_read_owned_bytes(read); char *bad=dx_printf("SELECT * REPLACE (%s) FROM (%s)",replacements[i],sql);
                CHECK(result_feed(connection,read,bad,65,false,old_bytes,&error)==WORKBOOK_CODEC_READ_ERROR && preference_result_rows==65 && workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)->first_row);
                clear_error(&error); free(bad); pending_preferences_header(read,out);
            }
            size_t owned=workbook_codec_read_owned_bytes(read); CHECK(owned+old_bytes<8u*1024u*1024u);
            size_t pressure_bytes=8u*1024u*1024u-owned-old_bytes-1; unsigned char *pressure=dx_alloc(pressure_bytes);
            CHECK(result_feed(connection,read,NULL,32,false,old_bytes+pressure_bytes,&error)==WORKBOOK_CODEC_READ_ERROR && preference_result_rows==65 && workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)->first_row);
            clear_error(&error); free(pressure); pending_preferences_header(read,out);
        }
        unsigned old_executions=executions,old_feeds=feeds;
        CHECK(result_feed(connection,read,NULL,version==13 ? 32 : 1,true,old_bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
        CHECK(preference_result_rows==65 && executions==old_executions+1 && feeds==old_feeds+2); /* One65-row result ->1+64 or32+33. */
    }
    CHECK(workbook_codec_read_finish(read,out,&error) && !error && workbook_codec_header_preferences_ready(*out));
    workbook_codec_read_free(read); duckdb_disconnect(&connection); duckdb_close(&database);
    CHECK(workbook_codec_header_version(*out)==version && workbook_codec_header_user_scale(*out)==1.25f && workbook_codec_header_reduced_motion(*out)==native.reduced_motion && workbook_codec_header_sheet_count(*out)==64 && workbook_codec_header_active_sheet(*out)==63 && !workbook_codec_header_sorts_ready(*out));
    for (size_t i=0;i<native.count;i++) {
        const Analysis *a=workbook_codec_header_analysis(*out,i),*b=&native.sheets[i].analysis;
        CHECK(workbook_codec_header_sheet_hidden(*out,i)==native.sheets[i].hidden && native.sheets[i].hidden==(present && i!=63));
        CHECK(a->kind==SOURCE_CSV && text_same(a->path,b->path) && !a->sort_column && !a->sort_count && !a->query && !a->filter_column && !a->filter_count && !strcmp(a->schema,"") && !strcmp(a->table,"NULL"));
        CHECK(!strcmp(b->query,"SELECT * FROM __BP1_SOURCE_MUST_NOT_EXECUTE") && !strcmp(workbook_codec_header_reference(*out,i),"missing-source.csv") && !strcmp(workbook_codec_header_name(*out,i),native.sheets[i].name));
        const SheetView *view=workbook_codec_header_view(*out,i);
        CHECK(view->row==DX_SHEET_ROWS-1 && view->col==16383 && view->anchor_row==1 && view->anchor_col==2 && view->end_row==3 && view->end_col==4 && view->top==5 && view->horizontal==120.25f && view->fraction==.25 && view->selection==2 && view->canvas_zoom==1.5f && !view->column_count);
    }
    workbook_free(&native); FileImage after=file_image(path); CHECK(before.bytes==after.bytes && !memcmp(before.data,after.data,before.bytes)); free(before.data); free(after.data);
    CHECK(!workbook_codec_header_sheet_hidden(*out,63) && !workbook_codec_header_sheet_hidden(*out,64) && workbook_codec_header_reduced_motion(*out)==present); free(error);
}
static void preferences_rejected(const char *path,bool missing_table,unsigned rows,size_t split,size_t accepted,WorkbookCodecHeader **out) {
    FileImage before=file_image(path); Workbook native; workbook_init(&native); char *error=NULL; Analysis blank={0};
    CHECK(workbook_add(&native,"Retained native",&blank,&error)); Workbook old_native=native;
    CHECK(!workbook_load(&native,path,&error) && !memcmp(&native,&old_native,sizeof(native)) && !strcmp(native.sheets[0].name,"Retained native")); clear_error(&error);
    PreferenceHeaderStamp old=preference_stamp(*out); char *target=dx_absolute_path(path); CHECK(target); WorkbookCodecRead *read=NULL;
    CHECK(workbook_codec_read_begin(&read,key,14,WORKBOOK_CODEC_SHEETS_25,8u*1024u*1024u,resolve,target,&error));
    duckdb_database database=NULL; duckdb_connection connection=NULL; open_readonly(path,&database,&connection);
    CHECK(result_feed(connection,read,NULL,256,true,old.bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error);
    CHECK(result_feed(connection,read,NULL,256,true,old.bytes,&error)==WORKBOOK_CODEC_READ_ACCEPTED && !error); size_t owned=workbook_codec_read_owned_bytes(read);
    if (missing_table) {
        CHECK(!workbook_codec_read_add_preferences(read,false,&error) && workbook_codec_read_owned_bytes(read)==owned && !workbook_codec_read_request(read)); clear_error(&error);
    } else {
        CHECK(workbook_codec_read_add_preferences(read,true,&error)); unsigned old_executions=executions;
        CHECK(result_feed(connection,read,NULL,split,true,old.bytes,&error)==WORKBOOK_CODEC_READ_ERROR && preference_result_rows==rows && workbook_codec_read_owned_bytes(read)==owned && workbook_codec_read_request(read)->first_row==accepted);
        CHECK(executions==old_executions+1); clear_error(&error); pending_preferences_header(read,out);
    }
    kept_preferences_header(*out,old); workbook_codec_read_free(read); free(target); duckdb_disconnect(&connection); duckdb_close(&database); workbook_free(&native);
    FileImage after=file_image(path); CHECK(before.bytes==after.bytes && !memcmp(before.data,after.data,before.bytes)); free(before.data); free(after.data); free(error);
}
static int preferences_only(void) {
    char temporary[]="/tmp/duxcel-codec-preferences-XXXXXX"; CHECK(mkdtemp(temporary)); char *directory=dx_absolute_path(temporary); CHECK(directory);
    char *missing=dx_printf("%s/missing-source.csv",directory); CHECK(access(missing,F_OK)!=0); WorkbookCodecHeader *header=NULL;
    for (size_t i=0;i<6;i++) {
        const EmitFixture *entry=emit_preferences_fixtures+i; char *path=dx_printf("%s/%s",directory,entry->name); preferences_fixture(path,missing,entry->version);
        if (entry->reordered) { mutate(path,reorder_sql); mutate(path,preferences_reorder_sql); }
        if (preferences_post_sql[i]) { mutate(path,preferences_post_sql[i]); preferences_rejected(path,false,65,32,0,&header); }
        else {
            preferences_compare(path,entry->version,true,entry->version==13,&header);
            if (i==2) { mutate(path,"INSERT INTO _duxcel_workspace.preferences(name,value) VALUES ('Future ''İ'' flag',true)"); preferences_compare(path,14,true,false,&header); }
        }
        CHECK(!unlink(path)); free(path);
    }
    char *path=dx_printf("%s/absent12.dxls",directory); preferences_fixture(path,missing,12); mutate(path,"DROP TABLE _duxcel_workspace.preferences");
    preferences_compare(path,12,false,false,&header); CHECK(!unlink(path)); free(path);
    path=dx_printf("%s/absent14.dxls",directory); preferences_fixture(path,missing,14); mutate(path,"DROP TABLE _duxcel_workspace.preferences");
    preferences_rejected(path,true,0,32,0,&header); CHECK(!unlink(path)); free(path);
    const char *invalid[]={
        "CREATE TABLE _duxcel_workspace.bad_preferences AS SELECT * FROM _duxcel_workspace.preferences WHERE name<>'sheet_hidden_62'; INSERT INTO _duxcel_workspace.bad_preferences VALUES ('sheet_hidden_63',false); DROP TABLE _duxcel_workspace.preferences; ALTER TABLE _duxcel_workspace.bad_preferences RENAME TO preferences",
        "UPDATE _duxcel_workspace.preferences SET value=true WHERE name='sheet_hidden_63'",
        "UPDATE _duxcel_workspace.preferences SET name='sheet_hidden_bad' WHERE name='sheet_hidden_62'",
        "CREATE TABLE _duxcel_workspace.bad_preferences AS SELECT name,value::INTEGER AS value FROM _duxcel_workspace.preferences; DROP TABLE _duxcel_workspace.preferences; ALTER TABLE _duxcel_workspace.bad_preferences RENAME TO preferences",
        "CREATE TABLE _duxcel_workspace.bad_preferences AS SELECT * FROM _duxcel_workspace.preferences; INSERT INTO _duxcel_workspace.bad_preferences VALUES ('reduce_motion',false); DROP TABLE _duxcel_workspace.preferences; ALTER TABLE _duxcel_workspace.bad_preferences RENAME TO preferences"
    };
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        path=dx_printf("%s/rejected%zu.dxls",directory,i); preferences_fixture(path,missing,14); mutate(path,invalid[i]);
        preferences_rejected(path,false,i==4 ? 66 : 65,i==4 ? 66 : 32,i<3 ? 32 : 0,&header);
        if (i==4) { mutate(path,"DELETE FROM _duxcel_workspace.preferences WHERE name='reduce_motion' AND value=false"); preferences_compare(path,14,true,false,&header); }
        CHECK(!unlink(path)); free(path);
    }
    workbook_codec_header_free(header); CHECK(nul_cells==1 && access(missing,F_OK)!=0); free(missing); CHECK(!rmdir(directory)); free(directory);
    printf("PASS: native optional preferences %u checks; %u metadata executions/%u typed feeds;65-row native parity/reorder/defaults, unknownNULL/NUL, duplicatefalse/active/malformed/66sentinel rejection, owned state and budget/stale/recovery; source definitions never executed; native comparison only, no Worker/App Open claim; engine %s\n",checks,executions,feeds,duckdb_library_version()); return 0;
}
#include "test_workbook_codec_objects_native.inc"
#include "test_workbook_codec_queries_native.inc"
#include "test_workbook_codec_options_native.inc"
#include "test_workbook_codec_tables_native.inc"
#include "test_workbook_codec_pivot_marks_native.inc"
#include "test_workbook_codec_filters_native.inc"
#include "test_workbook_codec_grid_native.inc"
#include "test_workbook_codec_counters_native.inc"
int main(int argc,char **argv) {
    if (argc==3 && !strcmp(argv[1],"--emit-fixtures")) return emit_fixture_files(argv[2],emit_fixtures,7,fixture,NULL,NULL);
    if (argc==3 && !strcmp(argv[1],"--emit-sorts-fixtures")) return emit_fixture_files(argv[2],emit_sorts_fixtures,4,sorts_fixture,sorts_reorder_sql,NULL);
    if (argc==3 && !strcmp(argv[1],"--emit-preferences-fixtures")) return emit_fixture_files(argv[2],emit_preferences_fixtures,6,preferences_fixture,preferences_reorder_sql,preferences_post_sql);
    if (argc==2 && !strcmp(argv[1],"--sorts-only")) return sorts_only();
    if (argc==2 && !strcmp(argv[1],"--preferences-only")) return preferences_only();
    if (argc==3 && !strcmp(argv[1],"--emit-objects-fixtures")) return emit_fixture_files(argv[2],emit_objects_fixtures,4,objects_fixture,objects_reorder_sql,NULL);
    if (argc==2 && !strcmp(argv[1],"--objects-only")) return objects_only();
    if (argc==3 && !strcmp(argv[1],"--emit-queries-fixtures")) return emit_queries(argv[2]);
    if (argc==2 && !strcmp(argv[1],"--queries-only")) return queries_only();
    if (argc==3 && !strcmp(argv[1],"--emit-source-options-fixtures")) return emit_options(argv[2]);
    if (argc==2 && !strcmp(argv[1],"--source-options-only")) return options_only();
    if (argc==3 && !strcmp(argv[1],"--emit-tables-fixtures")) return emit_tables(argv[2]);
    if (argc==2 && !strcmp(argv[1],"--tables-only")) return tables_only();
    if(argc==3&&!strcmp(argv[1],"--emit-pivot-marks-fixtures"))return emit_pivot_marks(argv[2]);
    if(argc==2&&!strcmp(argv[1],"--pivot-marks-only"))return pivot_marks_only();
    if(argc==3&&!strcmp(argv[1],"--emit-filters-fixtures"))return emit_filters(argv[2]);
    if(argc==2&&!strcmp(argv[1],"--filters-only"))return filters_only();
    if(argc==3&&!strcmp(argv[1],"--emit-grid-fixtures"))return emit_grid(argv[2]);
    if(argc==2&&!strcmp(argv[1],"--grid-only"))return grid_only();
    if(argc==3&&!strcmp(argv[1],"--emit-counters-fixtures"))return emit_counters(argv[2]);
    if(argc==2&&!strcmp(argv[1],"--counters-only"))return counters_only();
    if (argc!=1) { fprintf(stderr,"usage: %s [--emit-fixtures DIR | --sorts-only | --emit-sorts-fixtures DIR | --preferences-only | --emit-preferences-fixtures DIR | --objects-only | --emit-objects-fixtures DIR | --queries-only | --emit-queries-fixtures DIR | --source-options-only | --emit-source-options-fixtures DIR | --tables-only | --emit-tables-fixtures DIR | --pivot-marks-only | --emit-pivot-marks-fixtures DIR | --filters-only | --emit-filters-fixtures DIR | --grid-only | --emit-grid-fixtures DIR | --counters-only | --emit-counters-fixtures DIR]\n",argv[0]); return 2; }
    char directory[]="/tmp/duxcel-codec-native-XXXXXX"; CHECK(mkdtemp(directory));
    char *missing=dx_printf("%s/missing-source.csv",directory); CHECK(access(missing,F_OK)!=0);
    for (unsigned version=12;version<=14;version++) {
        char *path=dx_printf("%s/v%u.dxls",directory,version); fixture(path,missing,version); read_compare(path,version,WORKBOOK_CODEC_SHEETS_25,version==12);
        mutate(path,reorder_sql);
        read_compare(path,version,WORKBOOK_CODEC_SHEETS_25,false);
        if (version==12) { mutate(path,"ALTER TABLE _duxcel_workspace.sheets DROP COLUMN canvas_zoom"); read_compare(path,version,WORKBOOK_CODEC_SHEETS_24,false); }
        CHECK(!unlink(path)); free(path);
    }
    CHECK(access(missing,F_OK)!=0); free(missing); CHECK(!rmdir(directory));
    printf("PASS: native workbook header %u checks; %u metadata executions/%u typed feeds, v12/v13/v14 and physical reorder/legacy24/native parity; source definitions never executed, partial-header component only; engine %s\n",checks,executions,feeds,duckdb_library_version()); return 0;
}
