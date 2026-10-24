#define _DARWIN_C_SOURCE
#define _XOPEN_SOURCE 700
/* Product acceptance for Python-authored finite fixtures, not a generator. */
#include "workbook/version_save.h"
#include "data/database.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned checks;
#define CHECK(value) do { checks++; if (!(value)) { fprintf(stderr,"FAIL fixture:%d: %s (%s)\n",__LINE__,#value,error ? error : "no engine error"); exit(1); } } while (0)

static void verify(bool large) {
    char *error=NULL; const char *prefix=large ? "design-large" : "design-definition";
    char *path=dx_printf("build/tests/generated/%s.dxls",prefix);
    char *saved=dx_printf("build/tests/generated/%s-native-roundtrip.dxls",prefix);
    char parent[]="build/tests/fixture-native-XXXXXX"; CHECK(mkdtemp(parent));
    const uint64_t reserved=64u*1024u*1024u;
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded);
    WorkbookArchiveCandidate *candidate=NULL;
    CHECK(workbook_archive_read(path,parent,reserved,(WorkbookArchiveReadControl){0},&candidate,&error)==ARCHIVE_READ_READY);
    CHECK(workbook_archive_file_version(candidate)==16);
    Workbook *taken=workbook_archive_take_book(candidate);
    WorkbookVersionCoreSet *versions=workbook_archive_take_versions(candidate);
    CHECK(taken && versions && !workbook_archive_book(candidate) && !workbook_archive_versions(candidate));
    book=*taken; free(taken); workbook_archive_candidate_free(candidate); candidate=NULL;
    const WorkbookVersionIndex *index=workbook_version_core_set_index(versions);
    CHECK(index && index->root_count==1 && !index->tag_count && index->saved.present && index->generation==1 && index->saved_generation==1);
    CHECK(index->document_high==book.id.high && index->document_low==book.id.low);
    CHECK(book.id.high==UINT64_C(4923925674580705281) && book.id.low==(large ? 2u : 1u) && book.sheet_high_water==4);
    Workbook root; workbook_init(&root);
    CHECK(workbook_load_core_payload(&root,workbook_core_payload_path(workbook_version_core_set_leaf(versions,0)),&error));
    CHECK(root.count==4 && root.id.high==book.id.high && root.id.low==book.id.low && root.sheets!=book.sheets);
    CHECK(root.sheets[0].analysis.path!=book.sheets[0].analysis.path && !strcmp(root.sheets[0].analysis.path,book.sheets[0].analysis.path));
    CHECK(root.sheets[0].analysis.path[0]=='/'); workbook_free(&root);
    CHECK(book.count==4 && book.active==0);
    Worksheet *sheet=&book.sheets[0];
    CHECK(!strcmp(sheet->name,"Transactions") && sheet->analysis.kind==SOURCE_PARQUET);
    CHECK(sheet->view.column_count==12 && !strcmp(sheet->view.columns[5],"Amount (USD)"));
    CHECK(sheet->view.row==2 && sheet->view.col==12 && sheet->view.widths[11]==220);
    CHECK(sheet->objects.count==1 && sheet->objects.items[0].row==0 && sheet->objects.items[0].col==12);
    CHECK(sheet->formats.count==7 && sheet->formats.source_aware && sheet->objects.next_id==1);
    for (size_t i=0;i<book.count;i++) CHECK(book.sheets[i].id==i+1);
    for (size_t i=0;i<sheet->formats.count;i++) CHECK(sheet->formats.items[i].selector==FORMAT_SPATIAL);
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
    remove(saved); Workbook before=book; WorkbookVersionReceipt *receipt=NULL;
    WorkbookVersionOwner owner={book.id.high,book.id.low,1,VERSION_WORKING_HEAD};
    WorkbookVersionSaveRequest request={&book,versions,owner,VERSION_USER_SAVE,saved,parent,reserved};
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE);
    CHECK(receipt && !memcmp(&book,&before,sizeof(book)) && workbook_version_core_set_index(versions)==index);
    CHECK(workbook_archive_read(saved,parent,reserved,(WorkbookArchiveReadControl){0},&candidate,&error)==ARCHIVE_READ_READY);
    CHECK(workbook_archive_file_version(candidate)==16);
    taken=workbook_archive_take_book(candidate); CHECK(taken); loaded=*taken; free(taken);
    workbook_archive_candidate_free(candidate); candidate=NULL;
    CHECK(loaded.id.high==book.id.high && loaded.id.low==book.id.low && loaded.sheet_high_water==4);
    CHECK(loaded.sheets!=book.sheets && loaded.sheets[0].formats.items!=sheet->formats.items && loaded.sheets[0].objects.items!=sheet->objects.items);
    workbook_version_receipt_free(receipt); workbook_version_core_set_free(versions);
    CHECK(loaded.count==book.count && loaded.sheets[0].objects.count==1 && loaded.sheets[0].formats.count==7);
    CHECK(!strcmp(loaded.sheets[0].analysis.path,sheet->analysis.path));
    CHECK(database_pivot(&db,&loaded.sheets[0].analysis,&loaded.sheets[0].objects.items[0].pivot,0,-1,&page,&error));
    CHECK(page.total==30 && page.column_count==8);
    page_free(&page); database_close(&db); workbook_free(&book); workbook_free(&loaded);
    CHECK(remove(saved)==0 && !rmdir(parent)); free(path); free(saved); free(error);
}

int main(void) {
    verify(false); verify(true);
    printf("PASS: %u native fixture checks; canonical/large real PIVOT, exact DECIMAL, NULL/empty/literal text and current16 archive save/reload with independent ownership\n",checks);
    return 0;
}
