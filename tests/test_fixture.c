/* Product acceptance for Python-authored finite fixtures, not a generator. */
#include "workbook/workbook.h"
#include "data/database.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { checks++; if (!(value)) { fprintf(stderr,"FAIL fixture:%d: %s (%s)\n",__LINE__,#value,error ? error : "no engine error"); exit(1); } } while (0)

static void verify(bool large) {
    char *error=NULL; const char *prefix=large ? "design-large" : "design-definition";
    char *path=dx_printf("build/tests/generated/%s.dxls",prefix);
    char *saved=dx_printf("build/tests/generated/%s-native-roundtrip.dxls",prefix);
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded);
    CHECK(workbook_load(&book,path,&error));
    CHECK(book.count==4 && book.active==0);
    Worksheet *sheet=&book.sheets[0];
    CHECK(!strcmp(sheet->name,"Transactions") && sheet->analysis.kind==SOURCE_PARQUET);
    CHECK(sheet->view.column_count==12 && !strcmp(sheet->view.columns[5],"Amount (USD)"));
    CHECK(sheet->view.row==2 && sheet->view.col==12 && sheet->view.widths[11]==220);
    CHECK(sheet->objects.count==1 && sheet->objects.items[0].row==0 && sheet->objects.items[0].col==12);
    CHECK(sheet->formats.count==7);
    Database db; CHECK(database_open(&db,&error)); Page page={0};
    CHECK(database_pivot(&db,&sheet->analysis,&sheet->objects.items[0].pivot,0,-1,&page,&error));
    int64_t cents=0,count=0;
    for (size_t row=0;row<page.row_count;row++) for (size_t col=2;col<page.column_count;col++) {
        const Cell *cell=&page.cells[row*page.column_count+col]; if (cell->is_null) continue;
        if ((col-2)%2) count+=(int64_t)cell->number; else cents+=(int64_t)llround(cell->number*100);
    }
    CHECK(page.total==30 && page.column_count==8);
    CHECK(cents==(large ? INT64_C(9686461450) : INT64_C(-128883430)));
    CHECK(count==(large ? 36984 : 240));
    page_free(&page);
    char *source=dx_quote(sheet->analysis.path,'\'');
    char *sql=dx_printf("SELECT count(*),count(*) FILTER (WHERE \"Memo\" IS NULL),count(*) FILTER (WHERE \"Memo\"=''),count(*) FILTER (WHERE \"Memo\"='NULL'),bool_and(typeof(\"Amount (USD)\")='DECIMAL(18,2)') FROM read_parquet(%s)",source);
    duckdb_result result={0}; CHECK(database_execute(&db,sql,&result,&error));
    CHECK(duckdb_value_uint64(&result,0,0)==(large ? 48216u : 310u));
    CHECK(duckdb_value_uint64(&result,1,0)>0 && duckdb_value_uint64(&result,2,0)>0 && duckdb_value_uint64(&result,3,0)>0);
    CHECK(duckdb_value_boolean(&result,4,0));
    duckdb_destroy_result(&result); free(sql); free(source);
    CHECK(workbook_save(&book,saved,&error) && workbook_load(&loaded,saved,&error));
    CHECK(loaded.count==book.count && loaded.sheets[0].objects.count==1 && loaded.sheets[0].formats.count==7);
    CHECK(!strcmp(loaded.sheets[0].analysis.path,sheet->analysis.path));
    CHECK(database_pivot(&db,&loaded.sheets[0].analysis,&loaded.sheets[0].objects.items[0].pivot,0,-1,&page,&error));
    CHECK(page.total==30 && page.column_count==8);
    page_free(&page); database_close(&db); workbook_free(&book); workbook_free(&loaded);
    CHECK(remove(saved)==0); free(path); free(saved); free(error);
}

int main(void) {
    verify(false); verify(true);
    printf("PASS: %u native fixture checks; canonical/large real PIVOT, exact DECIMAL, NULL/empty/literal text and workbook save/reload\n",checks);
    return 0;
}
