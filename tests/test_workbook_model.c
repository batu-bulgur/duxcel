#include "workbook/workbook.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef DUXCEL_BROWSER_HOST
#error Build this boundary test with DUXCEL_BROWSER_HOST and without DuckDB.
#endif

static unsigned checks;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL workbook model:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
static const char *exact="-12345678901234567890.123456789012345678";

static void replay_composite(Worksheet *sheet,bool redo) {
    CHECK(cells_history(&sheet->cells,redo));
    CHECK(objects_history(&sheet->objects,redo));
    CHECK(format_history(&sheet->formats,redo));
    worksheet_transfer_history(sheet,redo);
}
static void populate_and_move(Workbook *book) {
    char *error=NULL; Analysis blank={0}; CHECK(workbook_add(book,"Canvas",&blank,&error));
    Worksheet *sheet=&book->sheets[0];
    CHECK(cells_set(&sheet->cells,30,10,"NULL",&error)); worksheet_record(sheet,EDIT_CELLS);
    CHECK(cells_set(&sheet->cells,31,10,exact,&error)); worksheet_record(sheet,EDIT_CELLS);
    CHECK(sheet->cells.count==2 && !strcmp(cells_get(&sheet->cells,30,10),"NULL") && !*cells_get(&sheet->cells,30,11));
    CHECK(objects_add(&sheet->objects,30,12,"Adjacent pivot",&error)==0); worksheet_record(sheet,EDIT_OBJECTS);
    PivotDefinition pivot={0};
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(objects_update(&sheet->objects,0,&pivot,&error)); worksheet_record(sheet,EDIT_OBJECTS); pivot_free(&pivot);
    MarkDefinition marks={0}; MarkField field={.name="City"};
    CHECK(marks_assign(&marks,MARKS_TOOLTIP,&field,&error));
    CHECK(objects_marks(&sheet->objects,0,&marks,&error)); worksheet_record(sheet,EDIT_OBJECTS); marks_free(&marks);
    CellFormat format=format_default(); format.flags=FORMAT_BOLD; format.number=FORMAT_ACCOUNTING; format.decimals=2;
    CHECK(format_apply(&sheet->formats,(FormatRange){30,31,10,10},FORMAT_BOLD|FORMAT_NUMERIC|FORMAT_DECIMALS,format,&error)); worksheet_record(sheet,EDIT_FORMAT);
    uint64_t revision=sheet->objects.items[0].revision;
    CHECK(worksheet_move_range(sheet,(SheetRange){30,34,10,15},10,10,0,0,&error));
    CHECK(sheet->undo_actions[sheet->undo_actions_count-1]==EDIT_RANGE_FORMAT);
    CHECK(!strcmp(cells_get(&sheet->cells,40,20),"NULL") && !strcmp(cells_get(&sheet->cells,41,20),exact));
    CHECK(!*cells_get(&sheet->cells,30,10) && sheet->objects.items[0].row==40 && sheet->objects.items[0].col==22);
    CHECK(sheet->objects.items[0].revision==revision && (format_at(&sheet->formats,40,20).flags&FORMAT_BOLD));
    replay_composite(sheet,false);
    CHECK(!strcmp(cells_get(&sheet->cells,31,10),exact) && sheet->objects.items[0].row==30 && sheet->redo_actions_count==1);
    CHECK((format_at(&sheet->formats,30,10).flags&FORMAT_BOLD) && !(format_at(&sheet->formats,40,20).flags&FORMAT_BOLD));
    replay_composite(sheet,true);
    CHECK(!strcmp(cells_get(&sheet->cells,41,20),exact) && sheet->objects.items[0].row==40 && !sheet->redo_actions_count);
    CHECK(cells_set(&sheet->cells,50,30,"Occupied",&error)); worksheet_record(sheet,EDIT_CELLS);
    size_t history=sheet->undo_actions_count; revision=sheet->cells.revision;
    CHECK(!worksheet_move_range(sheet,(SheetRange){40,44,20,25},10,10,0,0,&error));
    CHECK(error && sheet->undo_actions_count==history && sheet->cells.revision==revision && sheet->objects.items[0].row==40);
    free(error);
}
static void test_owned_snapshot_and_sheets(Workbook *book) {
    char *error=NULL; Worksheet *sheet=&book->sheets[0];
    sheet->view=(SheetView){.row=9000,.col=80,.top=8990,.canvas_zoom=1.25f,.column_count=1};
    sheet->view.columns=dx_alloc(sizeof(char *)); sheet->view.columns[0]=dx_strdup("Amount");
    sheet->view.widths=dx_alloc(sizeof(float)); sheet->view.widths[0]=188;
    Workbook *snapshot=workbook_snapshot(book); Worksheet *copy=&snapshot->sheets[0];
    CHECK(snapshot->sheets!=book->sheets && copy->name!=sheet->name && !strcmp(copy->name,"Canvas"));
    CHECK(copy->view.columns!=sheet->view.columns && copy->view.columns[0]!=sheet->view.columns[0] && copy->view.widths[0]==188 && copy->view.row==9000);
    CHECK(!copy->undo_actions_count && !copy->cells.undo_count && !copy->objects.undo_count && !copy->formats.undo_count);
    CHECK(copy->objects.items[0].pivot.fields[PIVOT_ROWS][0].name!=sheet->objects.items[0].pivot.fields[PIVOT_ROWS][0].name);
    CHECK(!strcmp(copy->objects.items[0].marks.fields[MARKS_TOOLTIP][0].name,"City"));
    CHECK(cells_set(&sheet->cells,41,20,"Changed after snapshot",&error)); worksheet_record(sheet,EDIT_CELLS);
    CHECK(!strcmp(cells_get(&copy->cells,41,20),exact));
    workbook_free(snapshot); free(snapshot);
    CHECK(workbook_duplicate(book,0,&error));
    CHECK(book->count==2 && book->active==1 && !strcmp(book->sheets[1].name,"Canvas (2)"));
    CHECK(!book->sheets[1].undo_actions_count && !book->sheets[1].cells.undo_count);
    CHECK(book->sheets[1].objects.items[0].name!=book->sheets[0].objects.items[0].name);
    CHECK(workbook_hide(book,0,true,&error) && workbook_visible_count(book)==1 && workbook_visible_sheet(book,0)==1);
    CHECK(!workbook_hide(book,1,true,&error) && !book->sheets[1].hidden); free(error); error=NULL;
    CHECK(workbook_hide(book,0,false,&error));
    CHECK(workbook_reorder(book,1,0,&error) && book->active==0 && !strcmp(book->sheets[0].name,"Canvas (2)"));
    CHECK(workbook_rename(book,0,"  Analysis view  ",&error) && !strcmp(book->sheets[0].name,"Analysis view"));
    CHECK(!workbook_rename(book,0,"canvas",&error) && !strcmp(book->sheets[0].name,"Analysis view")); free(error);
}
static void test_unchanged_path_helpers(void) {
    CHECK(workbook_is_path("Example.dxls") && workbook_is_path("Example.DXLS") && workbook_is_path("Legacy.duxcel"));
    CHECK(!workbook_is_path("Example.dxls.parquet"));
    char *relative=workbook_relative_path("/virtual/books/book.dxls","/virtual/data/source.parquet");
    CHECK(!strcmp(relative,"../data/source.parquet")); free(relative);
    char *target=workbook_target_path("tests/fixtures/model-target-does-not-exist.dxls");
    CHECK(target && strstr(target,"/tests/fixtures/model-target-does-not-exist.dxls"));
    char *source=workbook_source_path(target,"transactions.csv"),*expected=dx_absolute_path("tests/fixtures/transactions.csv");
    CHECK(source && expected && !strcmp(source,expected));
    relative=workbook_relative_path(target,source); CHECK(!strcmp(relative,"transactions.csv"));
    free(relative); free(target); free(source); free(expected);
}
static void test_object_high_water(void) {
    ObjectStore store={0},copy={0}; char *error=NULL; const uint64_t base=UINT64_C(9007199254740992);
    CHECK(objects_valid(&store,&error)); store.next_id=base;
    CHECK(objects_add(&store,0,0,"First lifetime owner",&error)==0 && store.items[0].id==base+1);
    CHECK(objects_add(&store,0,8,"Deleted lifetime owner",&error)==1 && store.items[1].id==base+2);
    CHECK(objects_remove(&store,1) && store.count==1 && store.next_id==base+2);
    CHECK(objects_history(&store,false) && store.count==2 && store.items[1].id==base+2 && store.next_id==base+2);
    CHECK(objects_history(&store,true) && store.count==1 && store.next_id==base+2);
    CHECK(objects_add(&store,0,16,"Later lifetime owner",&error)==1 && store.items[1].id==base+3 && !store.redo_count);
    CHECK(objects_history(&store,false) && store.count==1 && store.next_id==base+3);
    CHECK(objects_history(&store,true) && store.count==2 && store.items[1].id==base+3 && store.next_id==base+3);
    CHECK(objects_history(&store,false) && store.count==1 && store.next_id==base+3);
    CHECK(objects_add(&store,0,24,"Owner after Undo",&error)==1 && store.items[1].id==base+4 && !store.redo_count);
    CHECK(objects_valid(&store,&error)); store.next_id=base+1;
    CHECK(!objects_valid(&store,&error) && error && strstr(error,"high-water")); free(error); error=NULL; store.next_id=base+4;
    CHECK(objects_remove(&store,1) && store.count==1 && store.next_id==base+4 && objects_valid(&store,&error));
    objects_copy(&copy,&store);
    CHECK(copy.next_id==base+4 && copy.count==1 && !copy.undo_count && !copy.redo_count && copy.items[0].name!=store.items[0].name);
    CHECK(objects_add(&copy,0,8,"Copied lifetime",&error)==1 && copy.items[1].id==base+5 && store.next_id==base+4);
    objects_copy(&store,&store); CHECK(store.next_id==base+4 && store.count==1 && !store.undo_count && objects_valid(&store,&error));
    Workbook book; workbook_init(&book); Analysis blank={0}; CHECK(workbook_add(&book,"Owner lifetime",&blank,&error));
    objects_copy(&book.sheets[0].objects,&store); Workbook *snapshot=workbook_snapshot(&book);
    CHECK(snapshot->sheets[0].objects.next_id==base+4 && snapshot->sheets[0].objects.items[0].id==base+1 && snapshot->sheets[0].objects.items[0].name!=store.items[0].name);
    CHECK(workbook_duplicate(&book,0,&error) && book.sheets[0].objects.next_id==base+4 && book.sheets[1].objects.next_id==base+4);
    CHECK(objects_add(&book.sheets[1].objects,0,8,"Duplicated lifetime",&error)==1 && book.sheets[1].objects.items[1].id==base+5 && book.sheets[0].objects.next_id==base+4 && snapshot->sheets[0].objects.next_id==base+4);
    workbook_free(snapshot); free(snapshot); workbook_free(&book); objects_free(&store); objects_free(&copy);
    store.next_id=UINT64_MAX-1;
    CHECK(objects_add(&store,0,0,"Final lifetime owner",&error)==0 && store.items[0].id==UINT64_MAX && store.next_id==UINT64_MAX && objects_valid(&store,&error));
    ObjectStore before=store;
    CHECK(objects_add(&store,0,8,"Cannot wrap",&error)<0 && error && strstr(error,"exhausted") && !memcmp(&before,&store,sizeof(store))); free(error); error=NULL;
    CHECK(objects_remove(&store,0) && !store.count && store.next_id==UINT64_MAX && objects_valid(&store,&error)); before=store;
    CHECK(objects_add(&store,0,0,"Cannot reuse deleted maximum",&error)<0 && error && !memcmp(&before,&store,sizeof(store))); free(error); error=NULL;
    CHECK(objects_history(&store,false) && store.count==1 && store.items[0].id==UINT64_MAX && store.next_id==UINT64_MAX);
    CHECK(objects_history(&store,true) && !store.count && store.next_id==UINT64_MAX);
    objects_copy(&copy,&store); CHECK(!copy.count && copy.next_id==UINT64_MAX && objects_valid(&copy,&error)); before=copy;
    CHECK(objects_add(&copy,0,0,"Copied exhaustion",&error)<0 && error && !memcmp(&before,&copy,sizeof(copy))); free(error); objects_free(&copy); objects_free(&store);
}
static void test_unavailable_persistence(Workbook *book) {
    char *error=dx_strdup("Previous error"); book->path=dx_strdup("Owned workbook.dxls");
    Workbook before=*book; Worksheet *sheet=&book->sheets[0]; CellStore cells=sheet->cells; ObjectStore objects=sheet->objects;
    CHECK(!workbook_save(book,"new-file.dxls",&error) && error && strstr(error,"saving is not available") && strstr(error,"browser"));
    CHECK(!memcmp(&before,book,sizeof(before)) && sheet->cells.cells==cells.cells && sheet->objects.items==objects.items);
    CHECK(!workbook_load(book,"existing-file.dxls",&error) && error && strstr(error,"loading is not available") && strstr(error,"browser"));
    CHECK(!memcmp(&before,book,sizeof(before)) && !strcmp(cells_get(&sheet->cells,41,20),"Changed after snapshot"));
    CHECK(sheet->undo_actions_count==0 && sheet->objects.count==1 && !strcmp(sheet->name,"Analysis view"));
    CHECK(!workbook_save(NULL,NULL,NULL) && !workbook_load(NULL,NULL,NULL));
    free(error);
}
int main(void) {
    Workbook book; workbook_init(&book);
    populate_and_move(&book); test_owned_snapshot_and_sheets(&book); test_unchanged_path_helpers(); test_object_high_water(); test_unavailable_persistence(&book);
    workbook_free(&book); CHECK(!book.sheets && !book.count && book.user_scale==1);
    printf("PASS: %u browser workbook model checks\n",checks); return 0;
}
