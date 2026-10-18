#include "workbook/workbook_codec_read.h"
#include <duckdb.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned checks,executions,feeds,nul_cells;
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
    WorkbookCodecFamily family=request->family; const char *sql=override ? override : request->sql;
    duckdb_prepared_statement statement=NULL; CHECK(duckdb_prepare(connection,sql,&statement)==DuckDBSuccess);
    size_t columns=(size_t)duckdb_prepared_statement_column_count(statement); CHECK(columns<=25);
    WorkbookCodecColumn schema[25]={0}; duckdb_type types[25]={0}; size_t schema_bytes=sizeof(schema)+strlen(sql)+1;
    for (size_t c=0;c<columns;c++) {
        schema[c].name=duckdb_prepared_statement_column_name(statement,c); CHECK(schema[c].name);
        types[c]=duckdb_prepared_statement_column_type(statement,c); schema[c].kind=atom_kind(types[c]); schema_bytes+=strlen(schema[c].name)+1;
    }
    duckdb_result result={0}; CHECK(duckdb_execute_prepared(statement,&result)==DuckDBSuccess); executions++;
    idx_t chunks=duckdb_result_chunk_count(result); size_t first=request->first_row; WorkbookCodecReadStatus status=WORKBOOK_CODEC_READ_ACCEPTED;
    for (idx_t ci=0;ci<chunks && status==WORKBOOK_CODEC_READ_ACCEPTED;ci++) {
        duckdb_data_chunk chunk=duckdb_result_get_chunk(result,ci); CHECK(chunk && duckdb_data_chunk_get_column_count(chunk)==columns);
        size_t rows=(size_t)duckdb_data_chunk_get_size(chunk); CHECK(rows<=256);
        WorkbookCodecAtom *atoms=rows ? dx_alloc(rows*columns*sizeof(*atoms)) : NULL;
        size_t owned=schema_bytes+rows*columns*sizeof(*atoms);
        for (size_t c=0;c<columns;c++) {
            duckdb_vector vector=duckdb_data_chunk_get_vector(chunk,c); CHECK(vector);
            duckdb_logical_type logical=duckdb_vector_get_column_type(vector); CHECK(duckdb_get_type_id(logical)==types[c]); duckdb_destroy_logical_type(&logical);
            const void *data=duckdb_vector_get_data(vector); uint64_t *validity=duckdb_vector_get_validity(vector);
            for (size_t row=0;row<rows;row++) {
                WorkbookCodecAtom *atom=&atoms[row*columns+c];
                if (validity && !duckdb_validity_row_is_valid(validity,row)) { atom->kind=WORKBOOK_CODEC_NULL; continue; }
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
            status=workbook_codec_read_feed(read,key,family,&batch,extra_retained>SIZE_MAX-owned ? SIZE_MAX : owned+extra_retained,error); feeds++;
        }
        for (size_t at=0;at<rows && status==WORKBOOK_CODEC_READ_ACCEPTED;at+=split) {
            size_t take=rows-at<split ? rows-at : split;
            WorkbookCodecBatch batch={schema,columns,atoms+at*columns,take,first,ci+1==chunks && at+take==rows};
            if (stale_first) {
                size_t before=workbook_codec_read_owned_bytes(read); WorkbookCodecKey stale=key; stale.snapshot_generation--;
                CHECK(workbook_codec_read_feed(read,stale,family,&batch,owned,error)==WORKBOOK_CODEC_READ_STALE && !*error && workbook_codec_read_owned_bytes(read)==before); stale_first=false;
            }
            status=workbook_codec_read_feed(read,key,family,&batch,extra_retained>SIZE_MAX-owned ? SIZE_MAX : owned+extra_retained,error); feeds++;
            if (status==WORKBOOK_CODEC_READ_ACCEPTED) first+=take;
        }
        free(atoms); duckdb_destroy_data_chunk(&chunk);
    }
    if (!chunks) {
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
static void mutate(const char *path,const char *sql) {
    duckdb_database database=NULL; duckdb_connection connection=NULL; CHECK(duckdb_open(path,&database)==DuckDBSuccess && duckdb_connect(database,&connection)==DuckDBSuccess);
    execute(connection,sql); duckdb_disconnect(&connection); duckdb_close(&database);
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
int main(void) {
    char directory[]="/tmp/duxcel-codec-native-XXXXXX"; CHECK(mkdtemp(directory));
    char *missing=dx_printf("%s/missing-source.csv",directory); CHECK(access(missing,F_OK)!=0);
    for (unsigned version=12;version<=14;version++) {
        char *path=dx_printf("%s/v%u.dxls",directory,version); fixture(path,missing,version); read_compare(path,version,WORKBOOK_CODEC_SHEETS_25,version==12);
        mutate(path,"CREATE TABLE _duxcel_workspace.reordered_sheets AS SELECT canvas_zoom,* EXCLUDE(canvas_zoom) FROM _duxcel_workspace.sheets; DROP TABLE _duxcel_workspace.sheets; ALTER TABLE _duxcel_workspace.reordered_sheets RENAME TO sheets; CREATE TABLE _duxcel_workspace.reordered_meta AS SELECT user_scale,active_sheet,version FROM _duxcel_workspace.meta; DROP TABLE _duxcel_workspace.meta; ALTER TABLE _duxcel_workspace.reordered_meta RENAME TO meta");
        read_compare(path,version,WORKBOOK_CODEC_SHEETS_25,false);
        if (version==12) { mutate(path,"ALTER TABLE _duxcel_workspace.sheets DROP COLUMN canvas_zoom"); read_compare(path,version,WORKBOOK_CODEC_SHEETS_24,false); }
        CHECK(!unlink(path)); free(path);
    }
    CHECK(access(missing,F_OK)!=0); free(missing); CHECK(!rmdir(directory));
    printf("PASS: native workbook header %u checks; %u metadata executions/%u typed feeds, v12/v13/v14 and physical reorder/legacy24/native parity; source definitions never executed, partial-header component only; engine %s\n",checks,executions,feeds,duckdb_library_version()); return 0;
}
