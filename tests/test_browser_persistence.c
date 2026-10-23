/* BP0 file-compatibility gate, not a browser workbook codec or Save feature.
   create uses the actual native workbook writer; verify loads both files using
   that same production reader, then inspects the Worker-written SQL sentinels.
   Run from the checkout root with private output paths; no GUI is opened. */
#define _POSIX_C_SOURCE 200809L
#include "workbook/workbook.h"
#include "platform/platform.h"
#include <duckdb.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* BP0 remains a legacy v12 compatibility proof, independent of v15 UI support. */
#include "support/legacy_workbook.h"

static unsigned checks;
static bool fail(const char *message,int line) {
    fprintf(stderr,"FAIL browser-persistence:%d: %.2048s\n",line,message); return false;
}
#define REQUIRE(test,message) do { checks++; if (!(test)) { fail((message),__LINE__); goto done; } } while (0)
static bool text_same(const char *a,const char *b) { return a && b ? !strcmp(a,b) : a==b; }
static bool near(double a,double b) { return isfinite(a) && isfinite(b) && fabs(a-b)<0.000001; }

static bool analysis_same(const Analysis *a,const Analysis *b) {
    if (a->kind!=b->kind || !text_same(a->path,b->path) || !text_same(a->schema,b->schema) ||
        !text_same(a->table,b->table) || !text_same(a->query,b->query) || a->unique_rows!=b->unique_rows ||
        !text_same(a->filter_column,b->filter_column) || !text_same(a->filter_value,b->filter_value) ||
        a->filter_op!=b->filter_op || !text_same(a->group_column,b->group_column) ||
        !text_same(a->measure_column,b->measure_column) || a->aggregate!=b->aggregate ||
        !text_same(a->sort_column,b->sort_column) || a->descending!=b->descending ||
        a->sort_count!=b->sort_count || a->filter_count!=b->filter_count) return false;
    for (size_t i=0;i<a->filter_count;i++) if (!text_same(a->filters[i].column,b->filters[i].column) ||
        !text_same(a->filters[i].value,b->filters[i].value) || a->filters[i].op!=b->filters[i].op) return false;
    for (size_t i=0;i<a->sort_count;i++) if (!text_same(a->sorts[i].column,b->sorts[i].column) || a->sorts[i].descending!=b->sorts[i].descending) return false;
    return true; /* Derived number bindings and session history are not saved. */
}
static bool pivot_same(const PivotDefinition *a,const PivotDefinition *b) {
    for (size_t role=0;role<4;role++) {
        if (a->counts[role]!=b->counts[role]) return false;
        for (size_t i=0;i<a->counts[role];i++) {
            const PivotField *x=&a->fields[role][i],*y=&b->fields[role][i];
            if (!text_same(x->name,y->name) || !text_same(x->value,y->value) || x->aggregate!=y->aggregate || x->filter!=y->filter || x->grain!=y->grain) return false;
        }
    }
    return true;
}
static bool format_same(CellFormat a,CellFormat b) {
    return a.flags==b.flags && a.align==b.align && a.color==b.color && a.size==b.size && a.number==b.number && a.decimals==b.decimals;
}
static bool compare_books(const Workbook *a,const Workbook *b) {
    bool ok=false;
    REQUIRE(a->count==b->count && a->active==b->active,"Workbook sheets/active sheet changed");
    REQUIRE(near(a->user_scale,b->user_scale) && a->reduced_motion==b->reduced_motion,"Workbook preferences changed");
    for (size_t i=0;i<a->count;i++) {
        const Worksheet *x=&a->sheets[i],*y=&b->sheets[i]; const SheetView *v=&x->view,*w=&y->view;
        REQUIRE(text_same(x->name,y->name) && x->hidden==y->hidden,"Worksheet name/visibility changed");
        REQUIRE(analysis_same(&x->analysis,&y->analysis),"Worksheet source/query/filter/sort definition changed");
        REQUIRE(v->row==w->row && v->col==w->col && v->anchor_row==w->anchor_row && v->anchor_col==w->anchor_col &&
            v->end_row==w->end_row && v->end_col==w->end_col && v->top==w->top && v->selection==w->selection &&
            near(v->horizontal,w->horizontal) && near(v->fraction,w->fraction) && near(v->canvas_zoom,w->canvas_zoom),"Worksheet selection/viewport changed");
        REQUIRE(v->column_count==w->column_count,"Column presentation count changed");
        for (size_t c=0;c<v->column_count;c++) REQUIRE(text_same(v->columns[c],w->columns[c]) && near(v->widths[c],w->widths[c]),"Column name/width changed");
        REQUIRE(x->cells.count==y->cells.count && x->cells.bytes==y->cells.bytes,"Sparse manual-cell counts/bytes changed");
        for (size_t c=0;c<x->cells.count;c++) {
            const SheetCell *p=&x->cells.cells[c],*q=&y->cells.cells[c];
            REQUIRE(p->row==q->row && p->col==q->col && text_same(p->text,q->text),"Sparse manual-cell coordinates/exact text changed");
        }
        REQUIRE(x->formats.count==y->formats.count,"Saved range-format count changed");
        for (size_t f=0;f<x->formats.count;f++) {
            const FormatPatch *p=&x->formats.items[f],*q=&y->formats.items[f];
            REQUIRE(p->range.r0==q->range.r0 && p->range.r1==q->range.r1 && p->range.c0==q->range.c0 && p->range.c1==q->range.c1 &&
                p->mask==q->mask && format_same(p->value,q->value),"Saved format range/properties changed");
        }
        REQUIRE(x->objects.count==y->objects.count && x->objects.next_id==y->objects.next_id,"Table/pivot/chart object count or allocation high-water changed");
        for (size_t o=0;o<x->objects.count;o++) {
            const SheetObject *p=&x->objects.items[o],*q=&y->objects.items[o];
            REQUIRE(p->id==q->id && p->row==q->row && p->col==q->col && p->rows==q->rows && p->columns==q->columns &&
                p->kind==q->kind && p->mark==q->mark && p->chart_measure==q->chart_measure && p->source_id==q->source_id && text_same(p->name,q->name),"Object identity/placement/presentation/source binding changed");
            REQUIRE(analysis_same(&p->source,&q->source) && pivot_same(&p->pivot,&q->pivot),"Object source or PIVOT definition changed");
            REQUIRE(marks_equal(&p->marks,&q->marks),"Saved Marks assignments changed");
        }
    }
    ok=true;
done: return ok;
}
static bool fixture_expected(const Workbook *book) {
    bool ok=false;
    REQUIRE(book->count==2 && book->active==1 && near(book->user_scale,1.25) && book->reduced_motion,"Realistic fixture workbook metadata missing");
    const Worksheet *sheet=&book->sheets[0],*notes=&book->sheets[1];
    REQUIRE(text_same(sheet->name,"Browser round trip") && text_same(notes->name,"Notes · UTF-8"),"Fixture sheet names missing");
    REQUIRE(sheet->analysis.kind==SOURCE_CSV && sheet->analysis.path && *sheet->analysis.path &&
        text_same(sheet->analysis.query,"SELECT * FROM input") && sheet->analysis.unique_rows,"Fixture source/query options missing");
    REQUIRE(sheet->analysis.filter_count==1 && sheet->analysis.sort_count==1,"Fixture secondary filters/sorts missing");
    REQUIRE(sheet->cells.count==6 && notes->cells.count==3,"Fixture sparse manual cells missing");
    REQUIRE(!strcmp(cells_get(&sheet->cells,20,1),"9007199254740993") &&
        !strcmp(cells_get(&sheet->cells,20,2),"12345678901234567890.0123456789"),"Fixture exact numeric text lost");
    REQUIRE(!strcmp(cells_get(&sheet->cells,21,1),"NULL") && !strcmp(cells_get(&sheet->cells,21,2),"İstanbul — 東京 🦆") &&
        !strcmp(cells_get(&sheet->cells,22,1),"line\tbreak\n\"quoted\""),"Fixture NULL literal/UTF-8/control characters lost");
    REQUIRE(!*cells_get(&sheet->cells,23,1),"Missing sparse cell became a stored value");
    for (size_t i=0;i<sheet->cells.count;i++) REQUIRE(sheet->cells.cells[i].row!=23 || sheet->cells.cells[i].col!=1,"Missing sparse cell was materialized");
    REQUIRE(sheet->formats.count==1 && notes->formats.count==1,"Fixture nondefault formats missing");
    CellFormat format=format_at(&sheet->formats,21,1);
    REQUIRE(format.flags==(FORMAT_BOLD|FORMAT_ITALIC) && format.align==FORMAT_RIGHT && format.color==2 && format.size==16 &&
        format.number==FORMAT_ACCOUNTING && format.decimals==4,"Fixture nondefault format values missing");
    REQUIRE(sheet->objects.count==3 && sheet->objects.items[0].kind==OBJECT_TABLE && sheet->objects.items[1].kind==OBJECT_PIVOT &&
        sheet->objects.items[2].kind==OBJECT_CHART,"Fixture table/pivot/chart definitions missing");
    const SheetObject *table=&sheet->objects.items[0],*pivot=&sheet->objects.items[1],*chart=&sheet->objects.items[2];
    REQUIRE(table->id==UINT64_C(9007199254740993) && pivot->id==table->id+1 && chart->id==table->id+2,"Metadata object IDs above the JavaScript exact integer limit changed");
    REQUIRE(pivot->source_id==table->id && chart->source_id==table->id && chart->mark==MARK_LINE && chart->chart_measure==1,"Fixture object bindings/chart presentation missing");
    REQUIRE(pivot->pivot.counts[PIVOT_ROWS]==1 && pivot->pivot.counts[PIVOT_COLUMNS]==1 && pivot->pivot.counts[PIVOT_VALUES]==2 &&
        pivot->pivot.counts[PIVOT_FILTERS]==1 && pivot->pivot.fields[PIVOT_COLUMNS][0].grain==PIVOT_DATE_YEAR,"Fixture real PIVOT shelf definitions missing");
    REQUIRE(chart->marks.counts[MARKS_LABEL]==1 && chart->marks.counts[MARKS_DETAIL]==1,"Fixture Marks assignments missing");
    ok=true;
done: return ok;
}
static bool version_expected(const char *path,int expected);
static bool create_fixture(const char *path) {
    bool ok=false; struct stat info; char *error=NULL; Workbook book,loaded; workbook_init(&book); workbook_init(&loaded);
    Analysis source={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/transactions.csv")},blank={0};
    PivotDefinition pivot={0}; MarkDefinition marks={0};
    REQUIRE(strlen(path)>=5 && !strcmp(path+strlen(path)-5,".dxls"),"Fixture output must end in .dxls");
    errno=0; REQUIRE(lstat(path,&info)!=0 && errno==ENOENT,"Refusing to overwrite an existing fixture path");
    REQUIRE(source.path,"Run fixture creation from the checkout root; transactions.csv is required");
    REQUIRE(analysis_query_set(&source,"SELECT * FROM input",&error),error ? error : "Set source query"); source.unique_rows=true;
    REQUIRE(analysis_filter_set(&source,"City",FILTER_IN,"[\"İstanbul\",\"Berlin\"]",&error) &&
        analysis_filter_set(&source,"Amount",FILTER_GREATER,"0.0001",&error),error ? error : "Set source filters");
    REQUIRE(analysis_sort_set(&source,"Date",false,false,&error) && analysis_sort_set(&source,"Amount",true,true,&error),error ? error : "Set source sorts");
    REQUIRE(workbook_add(&book,"Browser round trip",&source,&error) && workbook_add(&book,"Notes · UTF-8",&blank,&error),error ? error : "Create workbook sheets");
    book.user_scale=1.25f; book.reduced_motion=true; book.active=1;
    Worksheet *sheet=&book.sheets[0],*notes=&book.sheets[1];
    REQUIRE(cells_set(&sheet->cells,20,1,"9007199254740993",&error) && cells_set(&sheet->cells,20,2,"12345678901234567890.0123456789",&error) &&
        cells_set(&sheet->cells,21,1,"NULL",&error) && cells_set(&sheet->cells,21,2,"İstanbul — 東京 🦆",&error) &&
        cells_set(&sheet->cells,22,1,"line\tbreak\n\"quoted\"",&error) && cells_set(&sheet->cells,24,0,"Kept beside source",&error),error ? error : "Create sparse manual cells");
    REQUIRE(cells_set(&notes->cells,0,0,"Browser write-through",&error) && cells_set(&notes->cells,1,0,"-9223372036854775808",&error) &&
        cells_set(&notes->cells,2,0,"18446744073709551615",&error),error ? error : "Create second-sheet manual cells");
    CellFormat format=format_default(); format.flags=FORMAT_BOLD|FORMAT_ITALIC; format.align=FORMAT_RIGHT;
    format.color=2; format.size=16; format.number=FORMAT_ACCOUNTING; format.decimals=4;
    REQUIRE(format_apply(&sheet->formats,(FormatRange){20,24,0,2},FORMAT_ALL,format,&error),error ? error : "Set primary format");
    format=format_default(); format.flags=FORMAT_UNDERLINE; format.align=FORMAT_LEFT; format.size=11;
    REQUIRE(format_apply(&notes->formats,(FormatRange){0,2,0,0},FORMAT_ALL,format,&error),error ? error : "Set notes format");
    SheetView *view=&sheet->view; view->row=21; view->col=1; view->anchor_row=20; view->anchor_col=1;
    view->end_row=22; view->end_col=2; view->top=19; view->horizontal=13.25f; view->fraction=.375; view->canvas_zoom=1.25f;
    const char *names[]={"City","Amount","Date"}; const float widths[]={164,139.5f,124};
    view->column_count=3; view->columns=dx_alloc(3*sizeof(*view->columns)); view->widths=dx_alloc(sizeof(widths));
    memcpy(view->widths,widths,sizeof(widths)); for (size_t i=0;i<3;i++) view->columns[i]=dx_strdup(names[i]); notes->view.canvas_zoom=.75f;
    REQUIRE(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error) &&
        pivot_assign_grain(&pivot,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_YEAR,&error) &&
        pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_COUNT,&error) &&
        pivot_assign(&pivot,PIVOT_FILTERS,"Approved",AGG_SUM,&error),error ? error : "Create PIVOT shelves");
    pivot.fields[PIVOT_FILTERS][0].filter=FILTER_EQUAL; free(pivot.fields[PIVOT_FILTERS][0].value); pivot.fields[PIVOT_FILTERS][0].value=dx_strdup("true");
    sheet->objects.next_id=UINT64_C(9007199254740992);
    REQUIRE(objects_add_table(&sheet->objects,0,8,"Source table",&source,&error)==0 &&
        objects_add(&sheet->objects,0,14,"Amount by city and year",&error)==1 && objects_update(&sheet->objects,1,&pivot,&error),error ? error : "Create table-bound PIVOT");
    REQUIRE(objects_add_chart(&sheet->objects,8,14,"Amount trends",&pivot,&error)==2 &&
        objects_bind_source(&sheet->objects,1,sheet->objects.items[0].id,&error) &&
        objects_bind_source(&sheet->objects,2,sheet->objects.items[0].id,&error) && objects_mark(&sheet->objects,2,MARK_LINE,&error) &&
        objects_chart_measure(&sheet->objects,2,1,&error),error ? error : "Bind real chart definition");
    MarkField label={.name="Amount",.aggregate=AGG_SUM,.measure=true},detail={.name="City"};
    REQUIRE(marks_assign(&marks,MARKS_LABEL,&label,&error) && marks_assign(&marks,MARKS_DETAIL,&detail,&error) &&
        objects_marks(&sheet->objects,2,&marks,&error),error ? error : "Set chart Marks");
    REQUIRE(test_legacy_save(&book,path,&error),error ? error : "Native workbook_save failed");
    REQUIRE(version_expected(path,12),"The unchanged lossless BP0 fixture must still be written as native v12");
    REQUIRE(workbook_load(&loaded,path,&error),error ? error : "Native fixture readback failed");
    REQUIRE(fixture_expected(&loaded),"Native fixture readback content mismatch");
    printf("PASS browser-persistence create: native v12, 2 sheets, 9 sparse cells, 2 formats, 3 objects; %u checks\n",checks); ok=true;
done:
    marks_free(&marks); pivot_free(&pivot); analysis_free(&source); workbook_free(&loaded); workbook_free(&book); free(error); return ok;
}
static bool sql(duckdb_connection connection,const char *query,duckdb_result *result) {
    if (duckdb_query(connection,query,result)==DuckDBSuccess) return true;
    return fail(duckdb_result_error(result) ? duckdb_result_error(result) : "DuckDB query failed",__LINE__);
}
static bool version_expected(const char *path,int expected) {
    bool ok=false; duckdb_config config=NULL; duckdb_database db=NULL; duckdb_connection connection=NULL; duckdb_result result={0}; char *message=NULL,*temporary=dx_temp_directory();
    REQUIRE(temporary && duckdb_create_config(&config)==DuckDBSuccess,"Configure private metadata version reader");
    REQUIRE(duckdb_set_config(config,"access_mode","READ_ONLY")==DuckDBSuccess && duckdb_set_config(config,"memory_limit","128MB")==DuckDBSuccess && duckdb_set_config(config,"threads","1")==DuckDBSuccess && duckdb_set_config(config,"temp_directory",temporary)==DuckDBSuccess,"Bound metadata version reader");
    REQUIRE(duckdb_open_ext(path,&db,config,&message)==DuckDBSuccess,message ? message : "Open native workbook version reader");
    REQUIRE(duckdb_connect(db,&connection)==DuckDBSuccess && sql(connection,"SELECT version FROM _duxcel_workspace.meta LIMIT 2",&result),"Read actual persisted workspace version");
    REQUIRE(duckdb_row_count(&result)==1 && duckdb_column_type(&result,0)==DUCKDB_TYPE_INTEGER && !duckdb_value_is_null(&result,0,0) && duckdb_value_int32(&result,0,0)==expected,"Unexpected actual persisted workspace version"); ok=true;
done:
    duckdb_destroy_result(&result); if (connection) duckdb_disconnect(&connection); if (db) duckdb_close(&db); if (config) duckdb_destroy_config(&config); duckdb_free(message); dx_remove_temp_directory(temporary); free(temporary); return ok;
}
static bool owner_fixture_expected(const Workbook *book) {
    bool ok=false; const uint64_t base=UINT64_C(9007199254740992);
    REQUIRE(book->count==3 && book->active==0,"Owner fixture sheet/active count changed");
    const Worksheet *live=&book->sheets[0],*exhausted=&book->sheets[1],*empty=&book->sheets[2];
    REQUIRE(live->objects.count==1 && live->objects.items[0].id==base+1 && live->objects.next_id==base+2,"Retired high-water above2^53 was lost");
    REQUIRE(!exhausted->objects.count && exhausted->objects.next_id==UINT64_MAX && !empty->objects.count && !empty->objects.next_id,"Empty/exhausted owner lifetimes changed");
    REQUIRE(live->formats.count==1 && empty->formats.count==1 && !live->formats.scope_count && !empty->formats.scope_count,"Spatial-only format package changed");
    REQUIRE(!strcmp(cells_get(&live->cells,20,0),"9007199254740993.1234567890") && !strcmp(cells_get(&live->cells,21,0),"NULL") && !*cells_get(&live->cells,22,0),"Exact/manual/missing owner fixture cells changed");
    REQUIRE(format_at(&live->formats,20,0).flags==FORMAT_BOLD && format_at(&empty->formats,500,1).flags==FORMAT_BOLD,"Spatial format precedence changed"); ok=true;
done: return ok;
}
static bool create_owner_fixture(const char *path) {
    bool ok=false; char *error=NULL; struct stat info; Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); Analysis blank={.path=""}; const uint64_t base=UINT64_C(9007199254740992);
    REQUIRE(strlen(path)>=5 && !strcmp(path+strlen(path)-5,".dxls"),"Owner fixture path must end in .dxls");
    errno=0; REQUIRE(lstat(path,&info)!=0 && errno==ENOENT,"Refusing to overwrite an existing owner fixture");
    REQUIRE(workbook_add(&book,"Owner lifetime",&blank,&error) && workbook_add(&book,"Exhausted owner",&blank,&error) && workbook_add(&book,"Never allocated",&blank,&error),error ? error : "Create owner fixture"); book.active=0;
    for (size_t i=0;i<book.count;i++) book.sheets[i].view.canvas_zoom=1;
    Worksheet *live=&book.sheets[0],*exhausted=&book.sheets[1]; live->objects.next_id=base;
    REQUIRE(objects_add(&live->objects,0,8,"Live owner",&error)==0 && objects_add(&live->objects,8,8,"Deleted owner",&error)==1 && objects_remove(&live->objects,1),error ? error : "Create retired owner ID");
    exhausted->objects.next_id=UINT64_MAX-1; REQUIRE(objects_add(&exhausted->objects,0,8,"Maximum owner",&error)==0 && objects_remove(&exhausted->objects,0),"Create exhausted owner lifetime");
    REQUIRE(cells_set(&live->cells,20,0,"9007199254740993.1234567890",&error) && cells_set(&live->cells,21,0,"NULL",&error),"Create exact sparse owner fixture cells");
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    REQUIRE(format_apply(&live->formats,(FormatRange){20,21,0,0},FORMAT_BOLD,bold,&error) && format_apply(&book.sheets[2].formats,(FormatRange){0,DX_SHEET_ROWS-1,1,1},FORMAT_BOLD,bold,&error),error ? error : "Create spatial owner fixture formats");
    REQUIRE(test_legacy_save(&book,path,&error),error ? error : "Write owner fixture"); REQUIRE(version_expected(path,13),"Owner fixture must use actual v13");
    REQUIRE(workbook_load(&loaded,path,&error) && owner_fixture_expected(&loaded) && compare_books(&book,&loaded),error ? error : "Owner fixture native readback");
    REQUIRE(objects_add(&loaded.sheets[0].objects,8,8,"After native reopen",&error)==1 && loaded.sheets[0].objects.items[1].id==base+3,"Reopen reused a retired owner ID");
    REQUIRE(objects_add(&loaded.sheets[1].objects,0,8,"Cannot wrap",&error)<0 && error && !loaded.sheets[1].objects.count,"Reopen lost uint64 exhaustion");
    printf("PASS browser-persistence create-owners: native owner-only v13, 3 sheets, retired IDs above2^53 and UINT64_MAX exhaustion; %u checks\n",checks); ok=true;
done: workbook_free(&book); workbook_free(&loaded); free(error); return ok;
}
static bool verify_owner_files(const char *original,const char *exported) {
    bool ok=false; char *error=NULL; struct stat info; Workbook a,b; workbook_init(&a); workbook_init(&b);
    REQUIRE(lstat(original,&info)==0 && S_ISREG(info.st_mode) && info.st_size>0 && info.st_size<=32*1024*1024,"Original owner fixture must be a bounded regular file");
    REQUIRE(lstat(exported,&info)==0 && S_ISREG(info.st_mode) && info.st_size>0 && info.st_size<=64*1024*1024 && !dx_same_file(original,exported),"Owner export must be a different bounded regular file");
    REQUIRE(version_expected(original,13) && version_expected(exported,13),"Owner fixture/export must retain actual v13");
    REQUIRE(workbook_load(&a,original,&error) && workbook_load(&b,exported,&error),error ? error : "Read owner fixture/export");
    REQUIRE(owner_fixture_expected(&a) && owner_fixture_expected(&b) && compare_books(&a,&b),"Official Worker changed the native owner model");
    REQUIRE(objects_add(&b.sheets[0].objects,8,8,"After Worker export",&error)==1 && b.sheets[0].objects.items[1].id==UINT64_C(9007199254740995),"Worker export reused a retired owner ID");
    REQUIRE(objects_add(&b.sheets[1].objects,0,8,"Cannot wrap",&error)<0 && error && !b.sheets[1].objects.count,"Worker export lost owner exhaustion");
    printf("PASS browser-persistence verify-owners: native owner-only v13 metadata and spatial formats retained with exact uint64 lifetimes; %u checks\n",checks); ok=true;
done: workbook_free(&a); workbook_free(&b); free(error); return ok;
}
static bool scalar_text(duckdb_result *result,idx_t column,const char *expected) {
    if (duckdb_value_is_null(result,column,0)) return expected==NULL;
    char *text=duckdb_value_varchar(result,column,0); bool same=expected && text && !strcmp(text,expected); duckdb_free(text); return same;
}
static bool verify_sentinels(const char *path) {
    bool ok=false; duckdb_config config=NULL; duckdb_database database=NULL; duckdb_connection connection=NULL;
    duckdb_result result={0}; duckdb_logical_type logical=NULL; char *message=NULL,*temporary=dx_temp_directory();
    REQUIRE(temporary && duckdb_create_config(&config)==DuckDBSuccess,"Configure private read-only native sentinel reader");
    REQUIRE(duckdb_set_config(config,"access_mode","READ_ONLY")==DuckDBSuccess && duckdb_set_config(config,"memory_limit","128MB")==DuckDBSuccess &&
        duckdb_set_config(config,"threads","1")==DuckDBSuccess && duckdb_set_config(config,"temp_directory",temporary)==DuckDBSuccess,"Configure bounded sentinel reader");
    REQUIRE(duckdb_open_ext(path,&database,config,&message)==DuckDBSuccess,message ? message : "Open Worker export in native DuckDB");
    REQUIRE(duckdb_connect(database,&connection)==DuckDBSuccess,"Connect native sentinel reader");
    REQUIRE(sql(connection,"SELECT version FROM _duxcel_workspace.meta LIMIT 2",&result) && duckdb_row_count(&result)==1 &&
        duckdb_value_int32(&result,0,0)==12,"Worker export lost native v12 workspace version");
    duckdb_destroy_result(&result); result=(duckdb_result){0};
    REQUIRE(sql(connection,"SELECT signed_min,above_js_integer,max_unsigned,exact_decimal,negative_decimal,missing,zero_length,literal_null,utf8 FROM _duxcel_browser_probe.sentinels LIMIT 2",&result),"Read isolated Worker-written sentinels");
    REQUIRE(duckdb_row_count(&result)==1 && duckdb_column_count(&result)==9,"Worker sentinel row/column count changed");
    const duckdb_type types[]={DUCKDB_TYPE_BIGINT,DUCKDB_TYPE_BIGINT,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_DECIMAL,DUCKDB_TYPE_DECIMAL,
        DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_VARCHAR};
    const char *values[]={"-9223372036854775808","9007199254740993","18446744073709551615","9007199254740993.1234567890",
        "-0.0000000001",NULL,"","NULL","İstanbul 😀"};
    for (idx_t i=0;i<9;i++) {
        REQUIRE(duckdb_column_type(&result,i)==types[i],"Worker sentinel SQL type changed");
        REQUIRE(scalar_text(&result,i,values[i]),"Worker sentinel exact text/NULL value changed");
    }
    REQUIRE(duckdb_value_int64(&result,0,0)==INT64_MIN && duckdb_value_int64(&result,1,0)==INT64_C(9007199254740993) &&
        duckdb_value_uint64(&result,2,0)==UINT64_MAX,"Worker BIGINT/UBIGINT changed through native typed getters");
    REQUIRE(duckdb_value_is_null(&result,5,0) && !duckdb_value_is_null(&result,6,0) && !duckdb_value_is_null(&result,7,0),"SQL NULL, empty VARCHAR and literal NULL were conflated");
    for (idx_t i=3;i<5;i++) {
        logical=duckdb_column_logical_type(&result,i);
        REQUIRE(logical && duckdb_get_type_id(logical)==DUCKDB_TYPE_DECIMAL && duckdb_decimal_width(logical)==38 &&
            duckdb_decimal_scale(logical)==10,"Worker DECIMAL precision/scale changed");
        duckdb_destroy_logical_type(&logical);
    }
    ok=true;
done:
    if (logical) duckdb_destroy_logical_type(&logical); duckdb_destroy_result(&result);
    if (connection) duckdb_disconnect(&connection); if (database) duckdb_close(&database);
    if (config) duckdb_destroy_config(&config); duckdb_free(message); dx_remove_temp_directory(temporary); free(temporary); return ok;
}
static bool verify_files(const char *original,const char *exported) {
    bool ok=false; struct stat info; char *error=NULL; Workbook a,b; workbook_init(&a); workbook_init(&b);
    REQUIRE(lstat(original,&info)==0 && S_ISREG(info.st_mode) && info.st_size>0 && info.st_size<=32*1024*1024,"Original fixture must be a bounded regular owned file");
    REQUIRE(lstat(exported,&info)==0 && S_ISREG(info.st_mode) && info.st_size>0 && info.st_size<=64*1024*1024,"Worker export must be a bounded regular owned file");
    REQUIRE(!dx_same_file(original,exported),"Original and Worker export must be different files");
    REQUIRE(workbook_load(&a,original,&error),error ? error : "Native workbook_load original failed");
    REQUIRE(workbook_load(&b,exported,&error),error ? error : "Native workbook_load Worker export failed");
    REQUIRE(fixture_expected(&a) && fixture_expected(&b),"Original/export fixture contents are incomplete");
    REQUIRE(compare_books(&a,&b),"Native model changed after official Worker copy/export");
    REQUIRE(verify_sentinels(exported),"Worker-persisted exact SQL sentinel verification failed");
    printf("PASS browser-persistence verify: native v12 model/definitions retained; BIGINT, UBIGINT, DECIMAL(38,10), SQL NULL, empty text, literal NULL and UTF-8 exact; %u checks\n",checks); ok=true;
done: workbook_free(&a); workbook_free(&b); free(error); return ok;
}
int main(int argc,char **argv) {
    if (argc==3 && !strcmp(argv[1],"create")) return create_fixture(argv[2]) ? 0 : 1;
    if (argc==3 && !strcmp(argv[1],"create-owners")) return create_owner_fixture(argv[2]) ? 0 : 1;
    if (argc==4 && !strcmp(argv[1],"verify-owners")) return verify_owner_files(argv[2],argv[3]) ? 0 : 1;
    if (argc==4 && !strcmp(argv[1],"verify")) return verify_files(argv[2],argv[3]) ? 0 : 1;
    fprintf(stderr,"Usage: %s create|create-owners <new-owned-fixture.dxls> | verify|verify-owners <original.dxls> <browser-export.dxls>\n",argv[0]); return 2;
}
