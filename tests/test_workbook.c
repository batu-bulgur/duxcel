#include "workbook/workbook.h"
#include "data/database.h"
#include "platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL workbook:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void sql_file(const char *path,const char *sql) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r;
    CHECK(duckdb_open(path,&db)==DuckDBSuccess); CHECK(duckdb_connect(db,&c)==DuckDBSuccess);
    if (duckdb_query(c,sql,&r)!=DuckDBSuccess) { fprintf(stderr,"%s\n",duckdb_result_error(&r)); exit(1); }
    duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
static void test_cells(void) {
    CellStore s={0},copy={0}; char *error=NULL;
    CHECK(cells_set(&copy,5,2,"İstanbul",&error)); CHECK(cells_set(&copy,5,2,cells_get(&copy,5,2)+2,&error)); CHECK(!strcmp(cells_get(&copy,5,2),"stanbul"));
    CHECK(cells_history(&copy,false) && !strcmp(cells_get(&copy,5,2),"İstanbul")); cells_free(&copy);
    CHECK(cells_set(&copy,0,0,"NULL",&error)); CHECK(cells_set(&copy,0,0,NULL,&error) && !copy.count); cells_free(&copy);
    CHECK(!strcmp(cells_get(&s,0,0),"")); CHECK(cells_set(&s,0,0,"City",&error));
    CHECK(cells_set(&s,1,0,"İstanbul",&error)); CHECK(cells_set(&s,1,1,"a\tb\n\"c\"",&error));
    cells_copy(&copy,&s); CHECK(cells_set(&s,0,0,"Changed",&error)); CHECK(!strcmp(cells_get(&copy,0,0),"City"));
    CHECK(cells_history(&s,false)); CHECK(!strcmp(cells_get(&s,0,0),"City")); CHECK(cells_history(&s,true)); CHECK(!strcmp(cells_get(&s,0,0),"Changed"));
    CHECK(cells_move(&s,0,1,0,1,3,2,&error)); CHECK(!strcmp(cells_get(&s,4,2),"İstanbul") && !*cells_get(&s,1,0));
    CHECK(cells_set(&s,7,5,"Occupied",&error)); CHECK(!cells_move(&s,3,4,2,3,4,3,&error)); free(error); error=NULL;
    CHECK(!strcmp(cells_get(&s,3,2),"Changed")); CHECK(cells_column(&s,2,true,&error)); CHECK(!strcmp(cells_get(&s,4,3),"İstanbul"));
    CHECK(cells_column(&s,3,false,&error)); CHECK(!strcmp(cells_get(&s,4,3),"a\tb\n\"c\"")); CHECK(cells_history(&s,false)); CHECK(!strcmp(cells_get(&s,4,3),"İstanbul"));
    uint64_t r0,r1; size_t c0,c1; cells_region(&copy,0,0,&r0,&r1,&c0,&c1,false); CHECK(r0==0 && r1==1 && c0==0 && c1==1);
    char *text=cells_tsv(&copy,0,1,0,1,&error); CHECK(text && !strcmp(text,"City\t\nİstanbul\t\"a\tb\n\"\"c\"\"\"")); free(text);
    CHECK(cells_clear(&copy,0,1,0,1)); CHECK(copy.count==0); CHECK(cells_history(&copy,false)); CHECK(copy.count==3);
    CHECK(!cells_set(&s,DX_SHEET_ROWS,0,"Outside",&error)); free(error); error=NULL;
    CHECK(!cells_move(&s,3,4,3,4,-5,0,&error)); free(error); error=NULL;
    cells_free(&s); cells_free(&copy);
    size_t rows=0,columns=0;
    CHECK(cells_paste(&s,2,3,"City\tNote\r\nİstanbul\t\"a\tb\n\"\"c\"\"\"\r\n",&rows,&columns,&error));
    CHECK(rows==2 && columns==2 && s.count==4 && s.undo_count==1);
    CHECK(!strcmp(cells_get(&s,3,3),"İstanbul") && !strcmp(cells_get(&s,3,4),"a\tb\n\"c\""));
    text=cells_tsv(&s,2,3,3,4,&error); CHECK(text!=NULL);
    CHECK(cells_paste(&copy,0,0,text,&rows,&columns,&error)); free(text);
    CHECK(!strcmp(cells_get(&copy,1,1),"a\tb\n\"c\""));
    CHECK(cells_history(&s,false) && s.count==0); CHECK(cells_history(&s,true) && s.count==4);
    CHECK(cells_paste(&s,2,3,"\tNew",&rows,&columns,&error));
    CHECK(!*cells_get(&s,2,3) && !strcmp(cells_get(&s,2,4),"New") && s.count==3);
    size_t history=s.undo_count;
    CHECK(!cells_paste(&s,DX_SHEET_ROWS-1,0,"one\ntwo",&rows,&columns,&error)); free(error); error=NULL;
    CHECK(s.count==3 && s.undo_count==history && !*cells_get(&s,DX_SHEET_ROWS-1,0));
    CHECK(!cells_paste(&s,0,0,"\"unterminated",&rows,&columns,&error)); free(error); error=NULL;
    CHECK(s.count==3 && s.undo_count==history);
    CHECK(!cells_paste(&s,0,0,"\"closed\"garbage",&rows,&columns,&error)); free(error); error=NULL;
    CHECK(cells_paste(&s,2,4,"New",&rows,&columns,&error) && s.undo_count==history);
    cells_free(&s); cells_free(&copy);
}
static void test_chart_optional_errors(void) {
    ObjectStore store={0}; PivotDefinition definition={0}; char *error=NULL;
    CHECK(objects_add_chart(&store,0,0,"Existing chart",&definition,&error)==0);
    size_t undo=store.undo_count; uint64_t next=store.next_id;
    CHECK(objects_add_chart(&store,DX_SHEET_ROWS-11,0,"Outside rows",&definition,NULL)==-1);
    CHECK(objects_add_chart(&store,0,16384-5,"Outside columns",&definition,NULL)==-1);
    PivotDefinition invalid={0}; invalid.counts[PIVOT_ROWS]=DX_PIVOT_FIELDS+1;
    CHECK(objects_add_chart(&store,0,0,"Invalid definition",&invalid,NULL)==-1);
    CHECK(store.count==1 && store.next_id==next && store.undo_count==undo && !store.redo_count);
    CHECK(!strcmp(store.items[0].name,"Existing chart") && !store.items[0].row && !store.items[0].col);
    CHECK(objects_add_chart(&store,DX_SHEET_ROWS-11,0,"Outside rows",&definition,&error)==-1);
    CHECK(error && !strcmp(error,"Choose space for a chart inside the sheet."));
    free(error); error=NULL;
    CHECK(objects_add_chart(&store,0,16384-5,"Outside columns",&definition,&error)==-1);
    CHECK(error && !strcmp(error,"Choose space for a chart inside the sheet."));
    free(error); error=NULL;
    CHECK(objects_add_chart(&store,0,0,"Invalid definition",&invalid,&error)==-1);
    CHECK(error && strstr(error,"eight") && !strstr(error,"space"));
    CHECK(store.count==1 && store.next_id==next && store.undo_count==undo && !store.redo_count);
    free(error); objects_free(&store);
}
static void test_objects(void) {
    Workbook b,loaded; workbook_init(&b); workbook_init(&loaded); char *error=NULL;
    Analysis a={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/transactions.csv")};
    CHECK(workbook_add(&b,"Source and pivots",&a,&error)); analysis_free(&a);
    ObjectStore *s=&b.sheets[0].objects; CellStore *cells=&b.sheets[0].cells;
    CHECK(objects_add(s,0,8,"Empty pivot",&error)==0);
    CHECK(objects_fits(s,0,0,8,5,4,cells,5,6,&error));
    CHECK(objects_fits(s,-1,8,0,5,4,cells,6,16384,&error));
    CHECK(!objects_move(s,0,0,2,cells,5,6,&error)); free(error); error=NULL;
    PivotDefinition p={0}; CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign(&p,PIVOT_COLUMNS,"Approved",AGG_SUM,&error)); CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(objects_update(s,0,&p,&error)); CHECK(s->items[0].pivot.counts[PIVOT_VALUES]==1);
    size_t undo=s->undo_count; uint64_t revision=s->items[0].revision;
    CHECK(objects_update(s,0,&p,&error) && s->undo_count==undo && s->items[0].revision==revision);
    CHECK(objects_add(s,10,8,"Another pivot",&error)==1); CHECK(objects_hit(s,11,9)==1 && objects_hit(s,7,9)==-1);
    CHECK(!objects_move(s,1,2,8,cells,5,6,&error)); free(error); error=NULL;
    CHECK(objects_move(s,1,12,12,cells,5,6,&error)); CHECK(s->items[1].row==12 && s->items[1].col==12);
    CHECK(objects_history(s,false)); CHECK(s->items[1].row==10); CHECK(objects_history(s,true)); CHECK(s->items[1].col==12);
    CHECK(cells_set(cells,20,12,"Occupied",&error)); CHECK(!objects_move(s,1,18,12,cells,5,6,&error)); free(error); error=NULL;
    SheetRange region=objects_region(s,cells,5,6,1,9,false); CHECK(region.r0==0 && region.r1==4 && region.c0==8 && region.c1==11);
    region=objects_region(s,cells,5,6,2,2,false); CHECK(region.r0==0 && region.r1==4 && region.c0==0 && region.c1==5);
    region=objects_region(s,cells,5,6,2,2,true); CHECK(region.r0==0 && region.r1==20 && region.c0==0 && region.c1==15);
    Workbook *copy=workbook_snapshot(&b); CHECK(copy->sheets[0].objects.count==2 && !copy->sheets[0].objects.undo_count);
    pivot_remove(&p,PIVOT_ROWS,0); CHECK(objects_update(s,0,&p,&error)); CHECK(copy->sheets[0].objects.items[0].pivot.counts[PIVOT_ROWS]==1);
    CHECK(workbook_save(copy,"tests/generated/pivot-workspace.dxls",&error));
    CHECK(workbook_load(&loaded,"tests/generated/pivot-workspace.dxls",&error));
    CHECK(loaded.sheets[0].objects.count==2 && loaded.sheets[0].objects.items[1].col==12);
    CHECK(!strcmp(loaded.sheets[0].objects.items[0].pivot.fields[PIVOT_ROWS][0].name,"City"));
    CHECK(loaded.sheets[0].objects.items[1].pivot.counts[PIVOT_VALUES]==0);
    CHECK(objects_add_chart(s,30,8,"Revenue by city",&p,&error)==2);
    CHECK(s->items[2].kind==OBJECT_CHART && s->items[2].rows==12 && s->items[2].columns==6);
    revision=s->items[2].revision; undo=s->undo_count;
    CHECK(objects_mark(s,2,MARK_BAR,&error) && s->undo_count==undo);
    CHECK(objects_mark(s,2,MARK_LINE,&error) && s->undo_count==undo+1 && s->items[2].revision==revision);
    CHECK(objects_history(s,false) && s->items[2].mark==MARK_BAR);
    CHECK(objects_history(s,true) && s->items[2].mark==MARK_LINE);
    CHECK(objects_mark(s,2,MARK_SCATTER,&error) && s->items[2].revision==revision);
    CHECK(objects_history(s,false) && s->items[2].mark==MARK_LINE);
    CHECK(!objects_mark(s,0,MARK_LINE,&error)); free(error); error=NULL;
    CHECK(!objects_mark(s,2,(ChartMark)4,&error)); free(error); error=NULL;
    revision=s->items[2].revision; uint64_t identity=s->items[2].id; undo=s->undo_count;
    CHECK(objects_representation(s,2,OBJECT_PIVOT,MARK_BAR,5,4,cells,5,6,&error));
    CHECK(s->items[2].kind==OBJECT_PIVOT && s->items[2].mark==MARK_BAR && s->items[2].rows==5 && s->items[2].columns==4);
    CHECK(s->items[2].revision==revision && s->items[2].id==identity && s->undo_count==undo+1 && s->items[2].pivot.counts[PIVOT_VALUES]==1);
    CHECK(objects_representation(s,2,OBJECT_PIVOT,MARK_BAR,5,4,cells,5,6,&error) && s->undo_count==undo+1);
    CHECK(objects_history(s,false) && s->items[2].kind==OBJECT_CHART && s->items[2].mark==MARK_LINE && s->items[2].rows==12);
    CHECK(objects_history(s,true) && s->items[2].kind==OBJECT_PIVOT && s->items[2].revision==revision);
    CHECK(!objects_representation(s,2,OBJECT_CHART,MARK_LINE,7,6,cells,5,6,&error)); free(error); error=NULL;
    CHECK(!objects_representation(s,2,OBJECT_TABLE,MARK_BAR,12,6,cells,5,6,&error)); free(error); error=NULL;
    CHECK(!objects_representation(s,2,OBJECT_PIVOT,MARK_LINE,5,4,cells,5,6,&error)); free(error); error=NULL;
    CHECK(cells_set(cells,38,13,"Protect this note",&error));
    CHECK(!objects_representation(s,2,OBJECT_CHART,MARK_LINE,12,6,cells,5,6,&error)); free(error); error=NULL;
    CHECK(s->items[2].kind==OBJECT_PIVOT && s->items[2].revision==revision && s->items[2].rows==5);
    CHECK(cells_clear(cells,38,38,13,13));
    CHECK(objects_representation(s,2,OBJECT_CHART,MARK_LINE,12,6,cells,5,6,&error));
    CHECK(s->items[2].revision==revision && s->items[2].id==identity && s->items[2].mark==MARK_LINE);
    CHECK(analysis_filter_set(&b.sheets[0].analysis,"City",FILTER_EQUAL,"İstanbul",&error));
    CHECK(analysis_filter_set(&b.sheets[0].analysis,"Amount",FILTER_GREATER,"10",&error));
    CHECK(workbook_save(&b,"tests/generated/chart-workspace.dxls",&error));
    CHECK(workbook_load(&loaded,"tests/generated/chart-workspace.dxls",&error));
    CHECK(loaded.sheets[0].objects.count==3 && loaded.sheets[0].objects.items[2].kind==OBJECT_CHART);
    CHECK(loaded.sheets[0].objects.items[2].mark==MARK_LINE);
    CHECK(loaded.sheets[0].analysis.filter_count==1 && !strcmp(loaded.sheets[0].analysis.filters[0].column,"Amount") && !strcmp(loaded.sheets[0].analysis.filters[0].value,"10"));
    CHECK(!strcmp(loaded.sheets[0].objects.items[2].name,"Revenue by city") && loaded.sheets[0].objects.items[2].pivot.counts[PIVOT_VALUES]==1);
    CHECK(objects_remove(s,0) && s->count==2); CHECK(objects_history(s,false) && s->count==3);
    sql_file("tests/generated/chart-workspace.dxls","UPDATE _duxcel_workspace.objects SET mark=4 WHERE kind=1");
    CHECK(!workbook_load(&loaded,"tests/generated/chart-workspace.dxls",&error) && loaded.sheets[0].objects.items[2].mark==MARK_LINE); free(error); error=NULL;
    sql_file("tests/generated/chart-workspace.dxls","UPDATE _duxcel_workspace.objects SET mark=NULL WHERE kind=1");
    CHECK(!workbook_load(&loaded,"tests/generated/chart-workspace.dxls",&error) && loaded.sheets[0].objects.items[2].mark==MARK_LINE); free(error); error=NULL;
    sql_file("tests/generated/chart-workspace.dxls","ALTER TABLE _duxcel_workspace.objects DROP COLUMN mark");
    CHECK(workbook_load(&loaded,"tests/generated/chart-workspace.dxls",&error) && loaded.sheets[0].objects.items[2].mark==MARK_BAR);
    pivot_free(&p); workbook_free(copy); free(copy); workbook_free(&b); workbook_free(&loaded); free(error);
}
static void test_chart_measure(void) {
    Workbook b,loaded; workbook_init(&b); workbook_init(&loaded); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&b,"Plot measure",&blank,&error)); ObjectStore *s=&b.sheets[0].objects;
    PivotDefinition p={0};
    CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_COUNT,&error));
    CHECK(objects_add_chart(s,0,8,"Cash flow",&p,&error)==0);
    uint64_t revision=s->items[0].revision,id=s->items[0].id; size_t undo=s->undo_count;
    CHECK(objects_chart_measure(s,0,1,&error) && s->items[0].chart_measure==1 && s->undo_count==undo+1);
    CHECK(s->items[0].id==id && s->items[0].revision==revision);
    CHECK(objects_chart_measure(s,0,1,&error) && s->undo_count==undo+1);
    CHECK(!objects_chart_measure(s,0,2,&error) && s->items[0].chart_measure==1); free(error); error=NULL;
    CHECK(!objects_chart_measure(s,1,0,&error)); free(error); error=NULL;
    CHECK(objects_history(s,false) && !s->items[0].chart_measure && s->items[0].revision==revision);
    CHECK(objects_history(s,true) && s->items[0].chart_measure==1 && s->items[0].revision==revision);
    Workbook *copy=workbook_snapshot(&b);
    CHECK(objects_chart_measure(s,0,0,&error) && copy->sheets[0].objects.items[0].chart_measure==1);
    CHECK(objects_history(s,false));
    CHECK(pivot_move(&p,PIVOT_VALUES,1,PIVOT_VALUES,0,&error) && objects_update(s,0,&p,&error));
    CHECK(s->items[0].chart_measure==0 && p.fields[PIVOT_VALUES][0].aggregate==AGG_COUNT);
    CHECK(objects_chart_measure(s,0,1,&error)); p.fields[PIVOT_VALUES][1].aggregate=AGG_AVG;
    CHECK(objects_update(s,0,&p,&error) && s->items[0].chart_measure==1);
    CHECK(workbook_save(&b,"tests/generated/chart-measure.dxls",&error));
    CHECK(workbook_load(&loaded,"tests/generated/chart-measure.dxls",&error) && loaded.sheets[0].objects.items[0].chart_measure==1);
    const char *invalid[]={"NULL","-1","8","2"};
    for (size_t i=0;i<4;i++) {
        char sql[128]; snprintf(sql,sizeof(sql),"UPDATE _duxcel_workspace.objects SET chart_measure=%s",invalid[i]); sql_file("tests/generated/chart-measure.dxls",sql);
        CHECK(!workbook_load(&loaded,"tests/generated/chart-measure.dxls",&error) && loaded.sheets[0].objects.items[0].chart_measure==1); free(error); error=NULL;
    }
    sql_file("tests/generated/chart-measure.dxls","ALTER TABLE _duxcel_workspace.objects DROP COLUMN chart_measure");
    CHECK(workbook_load(&loaded,"tests/generated/chart-measure.dxls",&error) && !loaded.sheets[0].objects.items[0].chart_measure);
    pivot_remove(&p,PIVOT_VALUES,1); CHECK(objects_update(s,0,&p,&error) && !s->items[0].chart_measure);
    CHECK(objects_valid(s,&error));
    s->items[0].chart_measure=1; CHECK(!objects_valid(s,&error)); free(error); error=NULL; s->items[0].chart_measure=0;
    pivot_free(&p); workbook_free(copy); free(copy); workbook_free(&b); workbook_free(&loaded); free(error);
}
static bool test_paste_guard(void *context,uint64_t row,size_t col,size_t rows,size_t columns,char **error) {
    return objects_fits(context,-1,row,col,rows,columns,NULL,5,6,error);
}
static void test_mixed_history(void) {
    Workbook b; workbook_init(&b); Analysis a={0}; char *error=NULL;
    CHECK(workbook_add(&b,"Mixed edits",&a,&error)); Worksheet *s=&b.sheets[0]; size_t rows,cols;
    CHECK(cells_set(&s->cells,0,7,"Manual note",&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(objects_add(&s->objects,2,8,"Pivot",&error)==0); worksheet_record(s,EDIT_OBJECTS);
    uint64_t revision=s->cells.revision;
    CHECK(!cells_paste_checked(&s->cells,2,7,"one\ttwo",&rows,&cols,test_paste_guard,&s->objects,&error)); free(error); error=NULL;
    CHECK(s->cells.revision==revision && s->cells.count==1 && s->undo_actions_count==2);
    CHECK(!cells_paste_checked(&s->cells,0,0,"Cannot replace source",&rows,&cols,test_paste_guard,&s->objects,&error)); free(error); error=NULL;
    CHECK(cells_paste_checked(&s->cells,0,7,"Revised note",&rows,&cols,test_paste_guard,&s->objects,&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(s->undo_actions_count==3 && s->undo_actions[2]==EDIT_CELLS);
    CHECK(cells_history(&s->cells,false)); worksheet_transfer_history(s,false); CHECK(!strcmp(cells_get(&s->cells,0,7),"Manual note"));
    CHECK(s->undo_actions[1]==EDIT_OBJECTS && objects_history(&s->objects,false)); worksheet_transfer_history(s,false); CHECK(s->objects.count==0);
    CHECK(cells_history(&s->cells,false)); worksheet_transfer_history(s,false); CHECK(!s->cells.count && !s->undo_actions_count && s->redo_actions_count==3);
    CHECK(cells_history(&s->cells,true)); worksheet_transfer_history(s,true);
    CHECK(objects_history(&s->objects,true)); worksheet_transfer_history(s,true);
    CHECK(cells_history(&s->cells,true)); worksheet_transfer_history(s,true);
    CHECK(s->objects.count==1 && !strcmp(cells_get(&s->cells,0,7),"Revised note") && s->undo_actions_count==3);
    CHECK(!cells_move(&s->cells,0,1,0,1,INT64_MIN,0,&error)); free(error); error=NULL;
    CHECK(!cells_move(&s->cells,0,UINT64_MAX,0,1,1,0,&error)); free(error); error=NULL;
    workbook_free(&b);
}
static void range_history(Worksheet *s,bool redo) {
    size_t n=redo ? s->redo_actions_count : s->undo_actions_count; CHECK(n>0);
    EditKind kind=(redo ? s->redo_actions : s->undo_actions)[n-1];
    CHECK(kind!=EDIT_ANALYSIS);
    if (kind==EDIT_CELLS || kind==EDIT_RANGE) CHECK(cells_history(&s->cells,redo));
    if (kind==EDIT_OBJECTS || kind==EDIT_RANGE) CHECK(objects_history(&s->objects,redo));
    worksheet_transfer_history(s,redo);
}
static void test_range_move(void) {
    Workbook b,loaded; workbook_init(&b); workbook_init(&loaded); Analysis a={0}; char *error=NULL;
    CHECK(workbook_add(&b,"Move mixed selection",&a,&error)); Worksheet *s=&b.sheets[0];
    CHECK(objects_add(&s->objects,10,8,"Pivot",&error)==0); worksheet_record(s,EDIT_OBJECTS);
    PivotDefinition p={0}; CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(objects_add_chart(&s->objects,25,8,"Chart",&p,&error)==1); worksheet_record(s,EDIT_OBJECTS); pivot_free(&p);
    CHECK(cells_set(&s->cells,22,8,"NULL is a literal note",&error)); worksheet_record(s,EDIT_CELLS);
    SheetRange range={10,36,8,13}; uint64_t revision=s->objects.items[0].revision;
    size_t actions=s->undo_actions_count,cell_undo=s->cells.undo_count,object_undo=s->objects.undo_count;
    CHECK(worksheet_range_fits(s,range,0,0,6,6,NULL));
    CHECK(worksheet_move_range(s,range,0,0,6,6,&error) && s->undo_actions_count==actions);
    CHECK(!worksheet_move_range(s,(SheetRange){11,36,8,13},3,2,6,6,&error)); free(error); error=NULL;
    CHECK(!worksheet_move_range(s,range,-10,-8,6,6,&error)); free(error); error=NULL;
    CHECK(!worksheet_move_range(s,range,INT64_MIN,0,6,6,&error)); free(error); error=NULL;
    CHECK(!worksheet_move_range(s,(SheetRange){0,2,0,2},30,30,6,6,&error)); free(error); error=NULL;
    CHECK(s->undo_actions_count==actions && s->cells.undo_count==cell_undo && s->objects.undo_count==object_undo && s->objects.items[0].row==10);
    CHECK(worksheet_move_range(s,range,3,2,6,6,&error));
    CHECK(s->undo_actions_count==actions+1 && s->undo_actions[actions]==EDIT_RANGE);
    CHECK(s->objects.items[0].row==13 && s->objects.items[1].row==28 && s->objects.items[1].col==10 && s->objects.items[0].revision==revision);
    CHECK(!strcmp(cells_get(&s->cells,25,10),"NULL is a literal note") && !*cells_get(&s->cells,22,8));
    range_history(s,false); CHECK(s->objects.items[0].row==10 && !strcmp(cells_get(&s->cells,22,8),"NULL is a literal note"));
    range_history(s,true); CHECK(s->objects.items[1].row==28 && !strcmp(cells_get(&s->cells,25,10),"NULL is a literal note"));
    const char *path="tests/generated/range-move.dxls"; remove(path); CHECK(workbook_save(&b,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    CHECK(loaded.sheets[0].objects.items[1].row==28 && loaded.sheets[0].objects.items[1].pivot.counts[PIVOT_ROWS]==1 && !strcmp(cells_get(&loaded.sheets[0].cells,25,10),"NULL is a literal note"));
    CHECK(cells_set(&s->cells,15,17,"Occupied gap",&error)); worksheet_record(s,EDIT_CELLS);
    actions=s->undo_actions_count; CHECK(!worksheet_move_range(s,(SheetRange){13,39,10,15},0,2,6,6,&error)); free(error); error=NULL;
    CHECK(s->undo_actions_count==actions && s->objects.items[0].col==10 && !strcmp(cells_get(&s->cells,25,10),"NULL is a literal note"));
    CHECK(objects_add(&s->objects,50,10,"External pivot",&error)==2); worksheet_record(s,EDIT_OBJECTS);
    CHECK(!worksheet_move_range(s,(SheetRange){13,39,10,15},20,0,6,6,&error)); free(error); error=NULL;
    range_history(s,false); CHECK(s->objects.count==2);
    CHECK(cells_set(&s->cells,70,20,"New branch",&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(!s->redo_actions_count && !s->objects.redo_count && !s->cells.redo_count);
    workbook_free(&b); workbook_free(&loaded); free(error);
}
static void test_composite_history_budget(void) {
    Workbook b; workbook_init(&b); Analysis a={0}; char *error=NULL; CHECK(workbook_add(&b,"Budget",&a,&error)); Worksheet *s=&b.sheets[0];
    char *text=dx_alloc(60001); memset(text,'a',60000); text[60000]=0;
    for (size_t i=0;i<10;i++) { CHECK(cells_set(&s->cells,10+i,8,text,&error)); worksheet_record(s,EDIT_CELLS); }
    CHECK(objects_add(&s->objects,25,8,"Pivot",&error)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(worksheet_move_range(s,(SheetRange){10,29,8,11},0,8,0,0,&error));
    CHECK(s->undo_actions[s->undo_actions_count-1]==EDIT_RANGE);
    for (size_t i=0;i<40;i++) { text[0]=(char)('a'+i%26); text[1]=(char)('A'+i/26); CHECK(cells_set(&s->cells,10,16,text,&error)); worksheet_record(s,EDIT_CELLS); }
    CHECK(s->undo_actions_count==s->cells.undo_count && s->undo_actions_count<32 && !s->objects.undo_count);
    size_t n=s->undo_actions_count;
    for (size_t i=0;i<n;i++) range_history(s,false);
    CHECK(!s->undo_actions_count && !s->cells.undo_count && s->objects.items[0].col==16 && !s->objects.redo_count);
    for (size_t i=0;i<n;i++) range_history(s,true);
    CHECK(s->undo_actions_count==n && !strcmp(cells_get(&s->cells,10,16),text));
    range_history(s,false); CHECK(cells_set(&s->cells,80,20,"Branch",&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(!s->redo_actions_count && !s->cells.redo_count && !s->objects.redo_count);
    free(text); free(error); workbook_free(&b);
}
static void test_worksheet_columns(void) {
    Workbook b,loaded; workbook_init(&b); workbook_init(&loaded); Analysis a={0}; char *error=NULL;
    CHECK(workbook_add(&b,"Column shifts",&a,&error)); Worksheet *s=&b.sheets[0];
    CHECK(cells_set(&s->cells,0,4,"Note",&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(objects_add(&s->objects,10,4,"Pivot",&error)==0); worksheet_record(s,EDIT_OBJECTS);
    uint64_t revision=s->objects.items[0].revision; size_t actions=s->undo_actions_count;
    CHECK(worksheet_column(s,2,true,0,&error));
    CHECK(!strcmp(cells_get(&s->cells,0,5),"Note") && !*cells_get(&s->cells,0,4) && s->objects.items[0].col==5 && s->objects.items[0].revision==revision);
    CHECK(s->undo_actions_count==actions+1 && s->undo_actions[actions]==EDIT_RANGE && s->undo_columns[actions].active && s->undo_columns[actions].column==2);
    range_history(s,false); CHECK(s->redo_columns[s->redo_actions_count-1].active && s->redo_columns[s->redo_actions_count-1].insert); CHECK(s->objects.items[0].col==4 && !strcmp(cells_get(&s->cells,0,4),"Note"));
    range_history(s,true); CHECK(s->undo_columns[s->undo_actions_count-1].active && s->undo_columns[s->undo_actions_count-1].column==2); CHECK(s->objects.items[0].col==5 && !strcmp(cells_get(&s->cells,0,5),"Note"));
    actions=s->undo_actions_count; char *owned=s->cells.cells[0].text;
    CHECK(!worksheet_column(s,6,true,0,&error)); free(error); error=NULL;
    CHECK(!worksheet_column(s,5,false,0,&error)); free(error); error=NULL;
    CHECK(!worksheet_column(s,2,true,6,&error)); free(error); error=NULL;
    CHECK(!worksheet_column(s,16384,true,0,&error)); free(error); error=NULL;
    CHECK(s->undo_actions_count==actions && s->cells.cells[0].text==owned && s->objects.items[0].col==5);
    CHECK(worksheet_column(s,2,false,0,&error)); CHECK(s->objects.items[0].col==4 && !strcmp(cells_get(&s->cells,0,4),"Note"));
    range_history(s,false); CHECK(s->objects.items[0].col==5); range_history(s,true); CHECK(s->objects.items[0].col==4);
    CHECK(worksheet_column(s,4,true,0,&error) && s->objects.items[0].col==5); /* At the left edge moves the whole object. */
    CHECK(workbook_save(&b,"tests/generated/column-shifts.dxls",&error)); CHECK(workbook_load(&loaded,"tests/generated/column-shifts.dxls",&error));
    CHECK(loaded.sheets[0].objects.items[0].col==5 && !strcmp(cells_get(&loaded.sheets[0].cells,0,5),"Note"));
    CHECK(cells_set(&s->cells,0,16383,"Edge",&error)); worksheet_record(s,EDIT_CELLS); actions=s->undo_actions_count;
    CHECK(!worksheet_column(s,2,true,0,&error)); free(error); error=NULL;
    CHECK(s->objects.items[0].col==5 && s->undo_actions_count==actions && !strcmp(cells_get(&s->cells,0,16383),"Edge"));
    CHECK(objects_add(&s->objects,100,16380,"Last pivot",&error)==1); worksheet_record(s,EDIT_OBJECTS); actions=s->undo_actions_count;
    CHECK(!worksheet_column(s,2,true,0,&error)); free(error); error=NULL;
    CHECK(s->objects.items[1].col==16380 && s->objects.items[0].col==5 && s->undo_actions_count==actions);
    CHECK(worksheet_column(s,10,false,6,&error)); CHECK(s->objects.items[1].col==16379 && !strcmp(cells_get(&s->cells,0,16382),"Edge"));
    range_history(s,false); CHECK(s->objects.items[1].col==16380 && !strcmp(cells_get(&s->cells,0,16383),"Edge"));
    free(error); workbook_free(&b); workbook_free(&loaded);
}
static void test_worksheet_rows(void) {
    Workbook b,loaded; workbook_init(&b); workbook_init(&loaded); Analysis a={0}; char *error=NULL;
    CHECK(workbook_add(&b,"Row shifts",&a,&error)); Worksheet *s=&b.sheets[0];
    CHECK(cells_set(&s->cells,4,8,"İstanbul",&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(objects_add(&s->objects,10,4,"Pivot",&error)==0); worksheet_record(s,EDIT_OBJECTS);
    PivotDefinition definition={0}; CHECK(objects_add_chart(&s->objects,30,2,"Chart",&definition,&error)==1); worksheet_record(s,EDIT_OBJECTS);
    uint64_t revision=s->objects.items[0].revision; size_t actions=s->undo_actions_count;
    CHECK(worksheet_row(s,2,true,0,&error));
    CHECK(s->undo_actions_count==actions+1 && s->undo_actions[actions]==EDIT_RANGE && !s->undo_columns[actions].active);
    CHECK(!*cells_get(&s->cells,4,8) && !strcmp(cells_get(&s->cells,5,8),"İstanbul"));
    CHECK(s->objects.items[0].row==11 && s->objects.items[1].row==31 && s->objects.items[0].revision==revision);
    range_history(s,false); CHECK(s->objects.items[0].row==10 && s->objects.items[1].row==30 && !strcmp(cells_get(&s->cells,4,8),"İstanbul"));
    range_history(s,true); CHECK(s->objects.items[0].row==11 && !strcmp(cells_get(&s->cells,5,8),"İstanbul"));
    actions=s->undo_actions_count; char *owned=s->cells.cells[0].text;
    CHECK(!worksheet_row(s,12,true,0,&error)); free(error); error=NULL;
    CHECK(!worksheet_row(s,11,false,0,&error)); free(error); error=NULL;
    CHECK(!worksheet_row(s,2,true,6,&error)); free(error); error=NULL;
    CHECK(!worksheet_row(s,2,false,UINT64_MAX,&error)); free(error); error=NULL;
    CHECK(!worksheet_row(s,DX_SHEET_ROWS,true,0,&error)); free(error); error=NULL;
    CHECK(s->undo_actions_count==actions && s->cells.cells[0].text==owned && s->objects.items[0].row==11);
    CHECK(worksheet_row(s,5,false,5,&error) && !s->cells.count && s->objects.items[0].row==10 && s->objects.items[1].row==30);
    range_history(s,false); CHECK(!strcmp(cells_get(&s->cells,5,8),"İstanbul") && s->objects.items[0].row==11);
    range_history(s,true); CHECK(!s->cells.count && s->objects.items[0].row==10);
    CHECK(worksheet_row(s,10,true,5,&error) && s->objects.items[0].row==11 && s->undo_actions[s->undo_actions_count-1]==EDIT_OBJECTS);
    actions=s->undo_actions_count; revision=s->cells.revision;
    CHECK(worksheet_row(s,100,true,5,&error) && s->undo_actions_count==actions && s->cells.revision==revision);
    CHECK(workbook_save(&b,"tests/generated/row-shifts.dxls",&error)); CHECK(workbook_load(&loaded,"tests/generated/row-shifts.dxls",&error));
    CHECK(loaded.sheets[0].objects.items[0].row==11 && loaded.sheets[0].objects.items[1].row==31);
    CHECK(cells_set(&s->cells,DX_SHEET_ROWS-1,8,"Edge",&error)); worksheet_record(s,EDIT_CELLS); actions=s->undo_actions_count;
    CHECK(!worksheet_row(s,2,true,0,&error)); free(error); error=NULL;
    CHECK(s->undo_actions_count==actions && !strcmp(cells_get(&s->cells,DX_SHEET_ROWS-1,8),"Edge") && s->objects.items[0].row==11);
    CHECK(cells_clear(&s->cells,DX_SHEET_ROWS-1,DX_SHEET_ROWS-1,8,8)); worksheet_record(s,EDIT_CELLS);
    CHECK(objects_add(&s->objects,DX_SHEET_ROWS-5,30,"Edge pivot",&error)==2); worksheet_record(s,EDIT_OBJECTS); actions=s->undo_actions_count;
    CHECK(!worksheet_row(s,2,true,0,&error)); free(error); error=NULL;
    CHECK(s->undo_actions_count==actions && s->objects.items[2].row==DX_SHEET_ROWS-5);
    CHECK(worksheet_row(s,2,false,0,&error)); range_history(s,false); CHECK(s->objects.items[2].row==DX_SHEET_ROWS-5);
    workbook_free(&b); workbook_free(&loaded); free(error);
}
static void test_chart_resize(void) {
    Workbook b,loaded; workbook_init(&b); workbook_init(&loaded); Analysis a={0}; PivotDefinition definition={0}; char *error=NULL;
    CHECK(workbook_add(&b,"Chart layout",&a,&error)); Worksheet *s=&b.sheets[0];
    CHECK(objects_add_chart(&s->objects,10,8,"Chart",&definition,&error)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add(&s->objects,30,8,"Pivot",&error)==1); worksheet_record(s,EDIT_OBJECTS);
    CHECK(cells_set(&s->cells,25,15,"Keep me",&error)); worksheet_record(s,EDIT_CELLS);
    uint64_t revision=s->objects.items[0].revision; size_t actions=s->objects.undo_count;
    CHECK(objects_resize_chart(&s->objects,0,14,7,&s->cells,5,6,&error)); worksheet_record(s,EDIT_OBJECTS);
    CHECK(s->objects.items[0].rows==14 && s->objects.items[0].columns==7 && s->objects.items[0].revision==revision && s->objects.undo_count==actions+1);
    range_history(s,false); CHECK(s->objects.items[0].rows==12 && s->objects.items[0].columns==6);
    range_history(s,true); CHECK(s->objects.items[0].rows==14 && s->objects.items[0].columns==7);
    actions=s->objects.undo_count; char *name=s->objects.items[0].name;
    CHECK(objects_resize_chart(&s->objects,0,14,7,&s->cells,5,6,&error) && s->objects.undo_count==actions && s->objects.items[0].name==name);
    CHECK(!objects_resize_chart(&s->objects,0,7,7,&s->cells,5,6,&error)); free(error); error=NULL;
    CHECK(!objects_resize_chart(&s->objects,0,14,2,&s->cells,5,6,&error)); free(error); error=NULL;
    CHECK(!objects_resize_chart(&s->objects,0,UINT64_MAX,SIZE_MAX,&s->cells,5,6,&error)); free(error); error=NULL;
    CHECK(!objects_resize_chart(&s->objects,1,14,7,&s->cells,5,6,&error)); free(error); error=NULL;
    CHECK(!objects_resize_chart(&s->objects,2,14,7,&s->cells,5,6,&error)); free(error); error=NULL;
    CHECK(!objects_resize_chart(&s->objects,0,16,8,&s->cells,5,6,&error)); free(error); error=NULL;
    CHECK(!objects_resize_chart(&s->objects,0,22,7,&s->cells,5,6,&error)); free(error); error=NULL;
    CHECK(!objects_resize_chart(&s->objects,0,14,7,&s->cells,20,9,&error)); free(error); error=NULL;
    CHECK(s->objects.undo_count==actions && s->objects.items[0].name==name && s->objects.items[0].rows==14 && !strcmp(cells_get(&s->cells,25,15),"Keep me"));
    CHECK(workbook_save(&b,"tests/generated/chart-size.dxls",&error)); CHECK(workbook_load(&loaded,"tests/generated/chart-size.dxls",&error));
    CHECK(loaded.sheets[0].objects.items[0].rows==14 && loaded.sheets[0].objects.items[0].columns==7 && loaded.sheets[0].objects.items[1].row==30);
    CHECK(objects_move(&s->objects,0,DX_SHEET_ROWS-14,16377,&s->cells,0,0,&error)); worksheet_record(s,EDIT_OBJECTS);
    CHECK(!objects_resize_chart(&s->objects,0,15,7,&s->cells,0,0,&error)); free(error); error=NULL;
    CHECK(!objects_resize_chart(&s->objects,0,14,8,&s->cells,0,0,&error)); free(error); error=NULL;
    CHECK(s->objects.items[0].rows==14 && s->objects.items[0].columns==7 && s->objects.items[0].revision==revision);
    workbook_free(&b); workbook_free(&loaded); free(error);
}
static void test_object_names(void) {
    Workbook b,loaded; workbook_init(&b); workbook_init(&loaded); Analysis a={0}; PivotDefinition definition={0}; char *error=NULL;
    CHECK(workbook_add(&b,"Named objects",&a,&error)); Worksheet *s=&b.sheets[0];
    CHECK(objects_add(&s->objects,0,5,"Pivot 1",&error)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add_chart(&s->objects,10,8,"Chart 2",&definition,&error)==1); worksheet_record(s,EDIT_OBJECTS);
    uint64_t revision=s->objects.items[1].revision,id=s->objects.items[1].id,titles=s->objects.title_revision;
    CHECK(objects_rename(&s->objects,1,"  İstanbul analizi  ",&error)); worksheet_record(s,EDIT_OBJECTS);
    CHECK(!strcmp(s->objects.items[1].name,"İstanbul analizi") && s->objects.title_revision==titles+1 && s->objects.items[1].revision==revision && s->objects.items[1].id==id);
    range_history(s,false); CHECK(!strcmp(s->objects.items[1].name,"Chart 2") && s->objects.title_revision==titles+2);
    range_history(s,true); CHECK(!strcmp(s->objects.items[1].name,"İstanbul analizi") && s->objects.title_revision==titles+3);
    size_t actions=s->objects.undo_count; char *name=s->objects.items[1].name; titles=s->objects.title_revision;
    CHECK(objects_rename(&s->objects,1,name,&error) && s->objects.undo_count==actions && s->objects.title_revision==titles);
    CHECK(!objects_rename(&s->objects,1,"pivot 1",&error)); free(error); error=NULL;
    CHECK(!objects_rename(&s->objects,1," \t ",&error)); free(error); error=NULL;
    CHECK(!objects_rename(&s->objects,1,"Two\nnames",&error)); free(error); error=NULL;
    CHECK(!objects_rename(&s->objects,2,"No object",&error)); free(error); error=NULL;
    char oversized[4098]; memset(oversized,'x',sizeof(oversized)-1); oversized[sizeof(oversized)-1]=0;
    CHECK(!objects_rename(&s->objects,1,oversized,&error)); free(error); error=NULL;
    CHECK(s->objects.undo_count==actions && s->objects.title_revision==titles && s->objects.items[1].name==name);
    CHECK(objects_rename(&s->objects,1,name+2,&error)); worksheet_record(s,EDIT_OBJECTS); CHECK(!strcmp(s->objects.items[1].name,"stanbul analizi"));
    CHECK(workbook_save(&b,"tests/generated/object-names.dxls",&error)); CHECK(workbook_load(&loaded,"tests/generated/object-names.dxls",&error));
    CHECK(!strcmp(loaded.sheets[0].objects.items[1].name,"stanbul analizi") && loaded.sheets[0].objects.items[1].id==id);
    titles=s->objects.title_revision; uint64_t next=s->objects.next_id;
    objects_copy(&s->objects,&s->objects);
    CHECK(s->objects.title_revision==titles && s->objects.next_id==next && s->objects.count==2 && !s->objects.undo_count && !strcmp(s->objects.items[1].name,"stanbul analizi"));
    workbook_free(&b); workbook_free(&loaded); free(error);
}
static void test_multiple_rows(void) {
    Workbook b,loaded; workbook_init(&b); workbook_init(&loaded); Analysis a={0}; PivotDefinition p={0}; char *error=NULL;
    CHECK(workbook_add(&b,"Multiple rows",&a,&error)); Worksheet *s=&b.sheets[0];
    size_t rows,columns; CHECK(cells_paste(&s->cells,7,8,"A\nB\nC",&rows,&columns,&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(objects_add(&s->objects,20,4,"Pivot",&error)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add_chart(&s->objects,40,2,"Chart",&p,&error)==1); worksheet_record(s,EDIT_OBJECTS);
    uint64_t revision=s->objects.items[0].revision; size_t actions=s->undo_actions_count;
    CHECK(worksheet_rows(s,8,3,true,7,&error));
    CHECK(s->undo_actions_count==actions+1 && s->undo_actions[actions]==EDIT_RANGE && !strcmp(cells_get(&s->cells,7,8),"A") && !strcmp(cells_get(&s->cells,11,8),"B") && !strcmp(cells_get(&s->cells,12,8),"C"));
    CHECK(s->objects.items[0].row==23 && s->objects.items[1].row==43 && s->objects.items[0].revision==revision);
    range_history(s,false); CHECK(s->objects.items[0].row==20 && !strcmp(cells_get(&s->cells,8,8),"B"));
    range_history(s,true); CHECK(s->objects.items[0].row==23 && !strcmp(cells_get(&s->cells,11,8),"B"));
    CHECK(worksheet_rows(s,8,3,false,7,&error)); CHECK(s->objects.items[0].row==20 && s->objects.items[1].row==40 && !strcmp(cells_get(&s->cells,8,8),"B"));
    actions=s->undo_actions_count; CHECK(worksheet_rows(s,7,3,false,7,&error));
    CHECK(!s->cells.count && s->objects.items[0].row==17 && s->objects.items[1].row==37 && s->undo_actions_count==actions+1);
    range_history(s,false); CHECK(s->cells.count==3 && !strcmp(cells_get(&s->cells,9,8),"C") && s->objects.items[0].row==20 && s->objects.items[1].row==40);
    actions=s->undo_actions_count; char *owned=s->cells.cells[0].text;
    CHECK(!worksheet_rows(s,10,11,false,7,&error)); free(error); error=NULL;
    CHECK(!worksheet_rows(s,21,3,true,7,&error)); free(error); error=NULL;
    CHECK(!worksheet_rows(s,6,3,false,7,&error)); free(error); error=NULL;
    CHECK(!worksheet_rows(s,8,0,true,7,&error)); free(error); error=NULL;
    CHECK(!worksheet_rows(s,8,UINT64_MAX,true,7,&error)); free(error); error=NULL;
    CHECK(s->undo_actions_count==actions && s->cells.cells[0].text==owned && s->objects.items[0].row==20);
    CHECK(worksheet_rows(s,20,3,true,7,&error) && s->objects.items[0].row==23 && s->objects.items[1].row==43);
    CHECK(workbook_save(&b,"tests/generated/multiple-rows.dxls",&error)); CHECK(workbook_load(&loaded,"tests/generated/multiple-rows.dxls",&error));
    CHECK(loaded.sheets[0].objects.items[0].row==23 && loaded.sheets[0].objects.items[1].row==43 && !strcmp(cells_get(&loaded.sheets[0].cells,9,8),"C"));
    CHECK(cells_set(&s->cells,DX_SHEET_ROWS-2,8,"Edge",&error)); worksheet_record(s,EDIT_CELLS); actions=s->undo_actions_count;
    CHECK(!worksheet_rows(s,8,3,true,7,&error)); free(error); error=NULL;
    CHECK(s->undo_actions_count==actions && s->objects.items[0].row==23 && !strcmp(cells_get(&s->cells,DX_SHEET_ROWS-2,8),"Edge"));
    workbook_free(&b); workbook_free(&loaded); free(error);
}
static void test_table_sources(void) {
    ObjectStore store={0},copy={0}; char *error=NULL;
    Analysis source={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/transactions.csv")};
    CHECK(analysis_filter_set(&source,"City",FILTER_EQUAL,"İstanbul",&error));
    CHECK(analysis_filter_set(&source,"Amount",FILTER_GREATER,"2",&error));
    CHECK(analysis_sort_set(&source,"Date",true,true,&error));
    CHECK(objects_add_table(&store,0,4,"Sales",&source,&error)==0);
    CHECK(store.items[0].kind==OBJECT_TABLE && store.items[0].source.path!=source.path && store.items[0].source.filter_value!=source.filter_value);
    uint64_t id=store.items[0].id,revision=store.items[0].revision;
    CHECK(objects_source(&store,&store.items[0])==store.items && objects_valid(&store,&error));
    CHECK(objects_add(&store,0,12,"Summary",&error)==1);
    CHECK(!objects_source(&store,&store.items[1]));
    CHECK(objects_bind_source(&store,1,id,&error) && objects_source(&store,&store.items[1])==store.items);
    size_t count=store.undo_count; uint64_t pivot_revision=store.items[1].revision;
    CHECK(objects_bind_source(&store,1,id,&error) && store.undo_count==count && store.items[1].revision==pivot_revision);
    CHECK(!objects_bind_source(&store,0,id,&error) && store.undo_count==count); free(error); error=NULL;
    CHECK(!objects_bind_source(&store,1,999,&error) && store.items[1].source_id==id && store.undo_count==count); free(error); error=NULL;
    CHECK(!objects_remove(&store,0) && store.count==2 && store.undo_count==count);
    CHECK(objects_set_source(&store,0,&store.items[0].source,&error) && store.undo_count==count);
    CHECK(analysis_filter_move(&source,0,1));
    CHECK(objects_set_source(&store,0,&source,&error) && store.undo_count==count+1 && store.items[0].revision==revision);
    CHECK(!strcmp(store.items[0].source.filter_column,"Amount"));
    CHECK(analysis_filter_set(&source,"City",FILTER_EQUAL,"Ankara",&error));
    CHECK(objects_set_source(&store,0,&source,&error) && store.items[0].revision==revision+1 && store.items[1].revision==pivot_revision);
    CHECK(objects_history(&store,false) && !strcmp(store.items[0].source.filters[0].value,"İstanbul") && store.items[0].revision==revision);
    CHECK(objects_history(&store,true) && !strcmp(store.items[0].source.filters[0].value,"Ankara"));
    objects_copy(&copy,&store);
    CHECK(copy.items[0].source.path!=store.items[0].source.path && copy.items[1].source_id==id && objects_valid(&copy,&error));
    Analysis invalid={.kind=SOURCE_NONE}; count=store.undo_count;
    CHECK(!objects_set_source(&store,0,&invalid,&error) && store.undo_count==count); free(error); error=NULL;
    invalid=(Analysis){.kind=SOURCE_CSV,.path=""};
    CHECK(objects_add_table(&store,20,4,"Invalid",&invalid,&error)<0 && store.count==2 && store.undo_count==count); free(error); error=NULL;
    CHECK(!objects_update(&store,0,&store.items[1].pivot,&error));
    uint64_t saved=store.items[1].source_id; store.items[1].source_id=999;
    CHECK(!objects_valid(&store,&error)); free(error); error=NULL; store.items[1].source_id=saved;
    CHECK(objects_remove(&store,1) && objects_remove(&store,0) && !store.count);
    CHECK(objects_history(&store,false) && store.count==1 && !strcmp(store.items[0].source.filters[0].value,"Ankara"));
    CHECK(objects_history(&store,false) && store.count==2 && objects_source(&store,&store.items[1])==store.items);
    objects_copy(&store,&store); CHECK(objects_valid(&store,&error) && store.next_id==2 && !store.undo_count);
    CHECK(objects_bind_source(&store,1,0,&error) && !objects_source(&store,&store.items[1]));
    CHECK(objects_remove(&store,0) && store.count==1 && objects_valid(&store,&error));
    CHECK(objects_valid(&copy,&error) && !strcmp(copy.items[0].source.filters[0].value,"Ankara"));
    analysis_free(&source); objects_free(&store); objects_free(&copy); free(error);
}
static void test_table_resolution(void) {
    ObjectStore store={0}; char *error=NULL;
    Analysis source={.kind=SOURCE_DATABASE,.path=dx_strdup("tests/generated/fixture.duckdb")};
    CHECK(objects_add_table(&store,0,0,"Database table",&source,&error)==0);
    Analysis resolved=analysis_copy(&source); resolved.schema=dx_strdup("main"); resolved.table=dx_strdup("a view");
    size_t edits=store.undo_count; uint64_t revision=store.items[0].revision;
    char *path=resolved.path; resolved.path=dx_strdup("different.duckdb");
    CHECK(!objects_resolve_source(&store,0,&resolved,&error) && !store.items[0].source.table); free(error); error=NULL;
    free(resolved.path); resolved.path=path;
    char *table=resolved.table; resolved.table=NULL;
    CHECK(!objects_resolve_source(&store,0,&resolved,&error) && !store.items[0].source.table); free(error); error=NULL; resolved.table=table;
    char *schema=resolved.schema; resolved.schema=dx_alloc(4098); memset(resolved.schema,'x',4097); resolved.schema[4097]=0;
    CHECK(!objects_resolve_source(&store,0,&resolved,&error)); free(error); error=NULL; free(resolved.schema); resolved.schema=schema;
    CHECK(objects_resolve_source(&store,0,&resolved,&error));
    CHECK(store.undo_count==edits && store.items[0].revision==revision && objects_valid(&store,&error));
    CHECK(!strcmp(store.items[0].source.table,"a view") && !strcmp(store.items[0].source.schema,"main"));
    CHECK(store.items[0].source.table!=resolved.table && store.items[0].source.schema!=resolved.schema);
    free(resolved.table); resolved.table=dx_strdup("table with spaces");
    CHECK(objects_resolve_source(&store,0,&resolved,&error) && !strcmp(store.items[0].source.table,"a view"));
    CHECK(objects_resolve_source(&store,0,&store.items[0].source,&error));
    CHECK(objects_set_source(&store,0,&resolved,&error) && store.undo_count==edits+1 && store.items[0].revision==revision+1);
    CHECK(objects_history(&store,false) && !strcmp(store.items[0].source.table,"a view"));
    CHECK(objects_history(&store,true) && !strcmp(store.items[0].source.table,"table with spaces"));
    CHECK(!objects_resolve_source(&store,1,&resolved,&error)); free(error); error=NULL;
    analysis_free(&source); analysis_free(&resolved); objects_free(&store);
}
static void test_table_workspace(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); char *error=NULL;
    Analysis blank={0},source={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/transactions.csv")};
    Analysis other={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/null-text.csv")};
    CHECK(analysis_filter_set(&source,"City",FILTER_EQUAL,"İstanbul",&error));
    CHECK(analysis_filter_set(&source,"Amount",FILTER_GREATER,"2",&error));
    CHECK(analysis_sort_set(&source,"Date",true,true,&error)); CHECK(analysis_sort_set(&source,"ID",false,true,&error));
    CHECK(workbook_add(&book,"Independent tables",&blank,&error)); ObjectStore *objects=&book.sheets[0].objects;
    CHECK(objects_add_table(objects,0,0,"Sales",&source,&error)==0);
    CHECK(objects_add_table(objects,0,20,"Null values",&other,&error)==1);
    CHECK(objects_add(objects,0,10,"Sales summary",&error)==2 && objects_bind_source(objects,2,objects->items[0].id,&error));
    PivotDefinition pivot={0}; CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(objects_update(objects,2,&pivot,&error));
    CHECK(objects_add_chart(objects,12,10,"Sales chart",&pivot,&error)==3 && objects_bind_source(objects,3,objects->items[0].id,&error));
    CHECK(cells_set(&book.sheets[0].cells,10,30,"Keep note",&error));
    CHECK(!workbook_save(&book,source.path,&error) && error && strstr(error,"table source")); free(error); error=NULL;
    CHECK(!workbook_save(&book,other.path,&error) && error && strstr(error,"table source")); free(error); error=NULL;
    const char *path="tests/generated/table-sources.dxls"; remove(path);
    CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    ObjectStore *restored=&loaded.sheets[0].objects;
    CHECK(restored->count==4 && restored->items[0].kind==OBJECT_TABLE && restored->items[1].kind==OBJECT_TABLE);
    CHECK(analysis_same_result(&restored->items[0].source,&source) && analysis_same_result(&restored->items[1].source,&other));
    CHECK(objects_source(restored,&restored->items[2])==restored->items && objects_source(restored,&restored->items[3])==restored->items);
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,10,30),"Keep note"));
    sql_file(path,"SELECT CASE WHEN version=12 THEN true ELSE error('wrong version') END FROM _duxcel_workspace.meta; SELECT CASE WHEN count(*)=2 AND bool_and(NOT starts_with(path,'/')) THEN true ELSE error('sources must be relative') END FROM _duxcel_workspace.table_sources");
    Database db; Page page={0}; CHECK(database_open(&db,&error));
    CHECK(database_fetch(&db,&restored->items[0].source,0,-1,&page,&error) && page.total==2); page_free(&page);
    CHECK(database_fetch(&db,&restored->items[1].source,0,-1,&page,&error) && page.total==6 && page.cells[1].is_null); page_free(&page);
    CHECK(database_pivot(&db,&restored->items[0].source,&restored->items[2].pivot,0,-1,&page,&error) && page.total==1 && page.column_count==2 && !strcmp(page.cells[1].text,"19.75")); page_free(&page); database_close(&db);
    Workbook *copy=workbook_snapshot(&loaded);
    CHECK(copy->sheets[0].objects.items[0].source.path!=restored->items[0].source.path && objects_source(&copy->sheets[0].objects,&copy->sheets[0].objects.items[2])==copy->sheets[0].objects.items);
    workbook_free(copy); free(copy);
    const char *invalid[]={
        "DELETE FROM _duxcel_workspace.table_sources WHERE object_id=1",
        "UPDATE _duxcel_workspace.objects SET source_id=999 WHERE id=3",
        "UPDATE _duxcel_workspace.objects SET source_id=3 WHERE id=4",
        "UPDATE _duxcel_workspace.objects SET source_id=NULL WHERE id=3",
        "UPDATE _duxcel_workspace.objects SET source_id=1 WHERE id=1",
        "UPDATE _duxcel_workspace.table_sources SET kind=0 WHERE object_id=1",
        "UPDATE _duxcel_workspace.table_sources SET path='' WHERE object_id=1",
        "UPDATE _duxcel_workspace.table_sources SET path=repeat('x',65537) WHERE object_id=1",
        "UPDATE _duxcel_workspace.table_sources SET descending=NULL WHERE object_id=1",
        "UPDATE _duxcel_workspace.table_sources SET filter_value=repeat('x',8388609) WHERE object_id=1",
        "UPDATE _duxcel_workspace.table_filters SET position=5",
        "UPDATE _duxcel_workspace.table_filters SET name='CITY'",
        "UPDATE _duxcel_workspace.table_sorts SET object_id=999",
        "UPDATE _duxcel_workspace.table_sorts SET descending=NULL",
        "ALTER TABLE _duxcel_workspace.table_sources ALTER kind TYPE VARCHAR",
        "ALTER TABLE _duxcel_workspace.table_filters ALTER op TYPE VARCHAR",
        "ALTER TABLE _duxcel_workspace.table_sorts ALTER descending TYPE VARCHAR",
        "ALTER TABLE _duxcel_workspace.objects ALTER source_id TYPE VARCHAR",
        "ALTER TABLE _duxcel_workspace.objects ALTER kind TYPE VARCHAR",
        "ALTER TABLE _duxcel_workspace.objects ALTER mark TYPE VARCHAR"
    };
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        /* A forged column type needs a fresh schema before restoring the valid fixture. */
        remove(path); CHECK(workbook_save(&book,path,&error)); sql_file(path,invalid[i]);
        bool rejected=!workbook_load(&loaded,path,&error); if (!rejected) fprintf(stderr,"Accepted invalid table fixture %zu: %s\n",i,invalid[i]); CHECK(rejected);
        CHECK(loaded.sheets[0].objects.count==4 && analysis_same_result(&loaded.sheets[0].objects.items[0].source,&source) && !strcmp(cells_get(&loaded.sheets[0].cells,10,30),"Keep note"));
        free(error); error=NULL;
        if (i>=14) {
            CHECK(!workbook_save(&book,path,&error) && error && strstr(error,"column types")); free(error); error=NULL;
            CHECK(!workbook_load(&loaded,path,&error)); free(error); error=NULL; /* Failed save rolls back instead of hiding corruption. */
        }
    }
    remove(path); CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    pivot_free(&pivot); analysis_free(&source); analysis_free(&other); workbook_free(&book); workbook_free(&loaded); free(error);
}
static void test_sort_workspace(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); char *error=NULL;
    Analysis source={.kind=SOURCE_DEMO};
    CHECK(analysis_sort_set(&source,"City",false,true,&error)); CHECK(analysis_sort_set(&source,"Date",true,true,&error)); CHECK(analysis_sort_set(&source,"Transaction ID",false,true,&error));
    CHECK(workbook_add(&book,"Sorted",&source,&error));
    const char *path="tests/generated/sort-order.dxls"; remove(path);
    CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error)); CHECK(analysis_sorts_equal(&loaded.sheets[0].analysis,&source));
    sql_file(path,"SELECT CASE WHEN version=12 THEN true ELSE error('wrong version') END FROM _duxcel_workspace.meta");
    Workbook *copy=workbook_snapshot(&book); CHECK(copy->sheets[0].analysis.sorts[0].column!=book.sheets[0].analysis.sorts[0].column);
    analysis_sort_remove(&book.sheets[0].analysis,1); CHECK(copy->sheets[0].analysis.sort_count==2); workbook_free(copy); free(copy);
    analysis_free(&book.sheets[0].analysis); book.sheets[0].analysis=analysis_copy(&source);
    const char *invalid[]={"UPDATE _duxcel_workspace.sorts SET position=6 WHERE position=0", "UPDATE _duxcel_workspace.sorts SET sheet_id=99", "UPDATE _duxcel_workspace.sorts SET name='city' WHERE position=0", "UPDATE _duxcel_workspace.sorts SET descending=NULL WHERE position=0", "UPDATE _duxcel_workspace.sorts SET name=repeat('x',33000) WHERE position=0"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        CHECK(workbook_save(&book,path,&error)); sql_file(path,invalid[i]); CHECK(!workbook_load(&loaded,path,&error)); CHECK(analysis_sorts_equal(&loaded.sheets[0].analysis,&source)); free(error); error=NULL;
    }
    CHECK(workbook_save(&book,path,&error));
    sql_file(path,"DROP TABLE _duxcel_workspace.sorts; UPDATE _duxcel_workspace.meta SET version=4");
    CHECK(workbook_load(&loaded,path,&error)); CHECK(!loaded.sheets[0].analysis.sort_count && !strcmp(loaded.sheets[0].analysis.sort_column,"City"));
    CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error)); CHECK(analysis_sorts_equal(&loaded.sheets[0].analysis,&source));
    analysis_free(&source); workbook_free(&book); workbook_free(&loaded); free(error);
}
static void test_large_canvas(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"Large canvas",&blank,&error)); Worksheet *sheet=&book.sheets[0];
    CHECK(cells_set(&sheet->cells,3475225,8,"Beside the final transaction",&error));
    size_t rows=0,columns=0; CHECK(cells_paste(&sheet->cells,3500000,8,"First\tSecond\nThird\tFourth",&rows,&columns,&error)); CHECK(rows==2 && columns==2);
    CHECK(cells_move(&sheet->cells,3500000,3500001,8,9,100,0,&error)); CHECK(!strcmp(cells_get(&sheet->cells,3500101,9),"Fourth"));
    CHECK(cells_history(&sheet->cells,false) && !strcmp(cells_get(&sheet->cells,3500001,9),"Fourth"));
    CHECK(objects_add(&sheet->objects,3500000,12,"Deep pivot",&error)==0);
    CHECK(objects_move(&sheet->objects,0,4000000,12,&sheet->cells,3475226,8,&error));
    CHECK(!objects_move(&sheet->objects,0,DX_SHEET_ROWS-2,12,&sheet->cells,0,0,&error)); free(error); error=NULL;
    sheet->view=(SheetView){.row=4000000,.anchor_row=4000000,.end_row=4000000,.top=3999990,.col=12,.anchor_col=12,.end_col=12};
    const char *path="tests/generated/large-canvas.dxls"; remove(path); CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    CHECK(loaded.sheets[0].view.top==3999990 && loaded.sheets[0].objects.items[0].row==4000000 && !strcmp(cells_get(&loaded.sheets[0].cells,3475225,8),"Beside the final transaction"));
    sheet->view.top=DX_SHEET_ROWS; CHECK(!workbook_save(&book,path,&error)); free(error); error=NULL;
    CHECK(workbook_load(&loaded,path,&error) && loaded.sheets[0].view.top==3999990);
    workbook_free(&book); workbook_free(&loaded); free(error);
}
static void test_sheet_management(void) {
    Workbook b,loaded; workbook_init(&b); workbook_init(&loaded); char *error=NULL; Analysis blank={0};
    CHECK(workbook_add(&b,"Source",&blank,&error)); CHECK(workbook_add(&b,"Notes",&blank,&error));
    CHECK(cells_set(&b.sheets[0].cells,8,3,"Original İ",&error)); worksheet_record(&b.sheets[0],EDIT_CELLS);
    CHECK(objects_add(&b.sheets[0].objects,12,5,"Pivot",&error)==0); worksheet_record(&b.sheets[0],EDIT_OBJECTS);
    SheetView *v=&b.sheets[0].view; v->row=8; v->col=3; v->canvas_zoom=1.25f; v->column_count=1;
    v->columns=dx_alloc(sizeof(char *)); v->columns[0]=dx_strdup("A"); v->widths=dx_alloc(sizeof(float)); v->widths[0]=180;
    CHECK(workbook_duplicate(&b,0,&error)); CHECK(b.count==3 && b.active==1 && !strcmp(b.sheets[1].name,"Source (2)"));
    CHECK(b.sheets[1].cells.undo_count==0 && b.sheets[1].objects.undo_count==0 && !b.sheets[1].undo_actions_count);
    CHECK(b.sheets[1].view.columns!=b.sheets[0].view.columns && b.sheets[1].view.widths!=b.sheets[0].view.widths);
    CHECK(b.sheets[1].view.row==8 && b.sheets[1].view.canvas_zoom==1.25f && b.sheets[1].view.widths[0]==180);
    CHECK(cells_set(&b.sheets[1].cells,8,3,"Independent",&error)); CHECK(!strcmp(cells_get(&b.sheets[0].cells,8,3),"Original İ"));
    CHECK(objects_remove(&b.sheets[1].objects,0)); CHECK(b.sheets[0].objects.count==1);
    CHECK(workbook_duplicate(&b,0,&error)); CHECK(b.active==1 && !strcmp(b.sheets[1].name,"Source (3)"));
    CHECK(workbook_rename(&b,1,"  İstanbul workspace  ",&error)); CHECK(!strcmp(b.sheets[1].name,"İstanbul workspace"));
    CHECK(workbook_rename(&b,1,b.sheets[1].name,&error)); /* alias-safe no-op */
    CHECK(!workbook_rename(&b,1,"sOuRcE",&error)); free(error); error=NULL;
    CHECK(!workbook_rename(&b,1,"\t\n",&error)); free(error); error=NULL;
    CHECK(!workbook_rename(&b,1,"bad\nname",&error)); free(error); error=NULL;
    CHECK(!workbook_rename(&b,99,"Missing",&error)); free(error); error=NULL;
    CellStore *cells=&b.sheets[0].cells; SheetCell *owned=cells->cells;
    CHECK(workbook_reorder(&b,0,3,&error)); CHECK(b.active==0 && !strcmp(b.sheets[0].name,"İstanbul workspace"));
    CHECK(b.sheets[3].cells.cells==owned && b.sheets[3].undo_actions_count==2);
    CHECK(workbook_reorder(&b,3,0,&error)); CHECK(b.active==1 && b.sheets[0].cells.cells==owned);
    CHECK(!workbook_reorder(&b,4,0,&error)); free(error); error=NULL;
    CHECK(workbook_hide(&b,1,true,&error)); CHECK(b.active==2 && workbook_visible_count(&b)==3);
    CHECK(workbook_visible_sheet(&b,0)==0 && workbook_visible_sheet(&b,1)==2 && workbook_visible_sheet(&b,2)==3 && workbook_visible_sheet(&b,3)==4);
    CHECK(workbook_hide(&b,0,true,&error)); CHECK(workbook_hide(&b,3,true,&error));
    CHECK(!workbook_hide(&b,2,true,&error)); free(error); error=NULL;
    CHECK(b.active==2 && !b.sheets[2].hidden && workbook_visible_count(&b)==1);
    const char *path="tests/generated/sheet-management.dxls"; remove(path); CHECK(workbook_save(&b,path,&error));
    CHECK(workbook_load(&loaded,path,&error)); CHECK(loaded.count==4 && loaded.active==2 && workbook_visible_count(&loaded)==1);
    CHECK(!strcmp(loaded.sheets[1].name,"İstanbul workspace") && loaded.sheets[1].hidden);
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,8,3),"Original İ") && loaded.sheets[0].objects.count==1);
    Workbook *snapshot=workbook_snapshot(&loaded); CHECK(snapshot->sheets[1].hidden && snapshot->active==2); workbook_free(snapshot); free(snapshot);
    CHECK(workbook_hide(&loaded,1,false,&error)); CHECK(loaded.active==2 && workbook_visible_count(&loaded)==2);
    CHECK(workbook_save(&loaded,path,&error)); CHECK(workbook_load(&b,path,&error)); CHECK(!b.sheets[1].hidden && b.sheets[0].hidden);
    sql_file(path,"INSERT INTO _duxcel_workspace.preferences VALUES ('sheet_hidden_2',true)");
    CHECK(!workbook_load(&loaded,path,&error)); CHECK(loaded.active==2 && !loaded.sheets[2].hidden); free(error); error=NULL;
    sql_file(path,"DELETE FROM _duxcel_workspace.preferences WHERE name='sheet_hidden_2'; INSERT INTO _duxcel_workspace.preferences VALUES ('sheet_hidden_99',true)");
    CHECK(!workbook_load(&loaded,path,&error)); free(error); error=NULL;
    workbook_free(&b); workbook_free(&loaded);
}
static void test_query_workspace(void) {
    char *error=NULL; Workbook book,loaded; workbook_init(&book); workbook_init(&loaded);
    Analysis source={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/transactions.csv")};
    CHECK(analysis_query_set(&source,"SELECT City, Amount::DECIMAL(12,2) AS Money FROM input",&error));
    CHECK(workbook_add(&book,"Queries",&source,&error));
    Worksheet *sheet=&book.sheets[0];
    CHECK(objects_add_table(&sheet->objects,10,9,"Independent query",&source,&error)==0);
    CHECK(objects_add(&sheet->objects,20,15,"Bound pivot",&error)==1);
    CHECK(objects_bind_source(&sheet->objects,1,sheet->objects.items[0].id,&error));
    Analysis changed=analysis_copy(&source); CHECK(analysis_query_set(&changed,"SELECT City, Amount::DECIMAL(12,2) AS Money FROM input WHERE Approved",&error));
    CHECK(objects_set_source(&sheet->objects,0,&changed,&error));
    CHECK(objects_history(&sheet->objects,false) && !strcmp(sheet->objects.items[0].source.query,source.query));
    CHECK(objects_history(&sheet->objects,true) && !strcmp(sheet->objects.items[0].source.query,changed.query));
    const char *path="tests/generated/query-workspace.dxls"; remove(path);
    CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    CHECK(!strcmp(loaded.sheets[0].analysis.query,source.query) && !strcmp(loaded.sheets[0].objects.items[0].source.query,changed.query));
    CHECK(loaded.sheets[0].objects.items[1].source_id==sheet->objects.items[0].id && !loaded.sheets[0].objects.items[1].source.query);
    Database db; CHECK(database_open(&db,&error)); Page page={0};
    CHECK(database_fetch_table(&db,&loaded.sheets[0].analysis,0,-1,&page,&error) && page.total==5); page_free(&page);
    CHECK(database_fetch_table(&db,&loaded.sheets[0].objects.items[0].source,0,-1,&page,&error) && page.total==3); page_free(&page); database_close(&db);
    const char *invalid[]={"UPDATE _duxcel_workspace.queries SET object_id=999 WHERE object_id<>0", "UPDATE _duxcel_workspace.queries SET sheet_id=-1", "UPDATE _duxcel_workspace.queries SET query=NULL", "UPDATE _duxcel_workspace.queries SET query=repeat('x',65537)", "UPDATE _duxcel_workspace.queries SET query=' '", "UPDATE _duxcel_workspace.queries SET query='SELECT 1'||chr(0)||'malformed'", "CREATE OR REPLACE TABLE _duxcel_workspace.queries AS SELECT sheet_id,object_id::VARCHAR AS object_id,query FROM _duxcel_workspace.queries", "DROP TABLE _duxcel_workspace.queries"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        CHECK(workbook_save(&book,path,&error)); sql_file(path,invalid[i]);
        CHECK(!workbook_load(&loaded,path,&error) && loaded.count==1 && !strcmp(loaded.sheets[0].analysis.query,source.query)); free(error); error=NULL;
    }
    CHECK(workbook_save(&book,path,&error));
    sql_file(path,"UPDATE _duxcel_workspace.meta SET version=6; DROP TABLE _duxcel_workspace.queries");
    CHECK(workbook_load(&loaded,path,&error) && !loaded.sheets[0].analysis.query && !loaded.sheets[0].objects.items[0].source.query);
    CHECK(workbook_save(&loaded,path,&error));
    analysis_free(&source); analysis_free(&changed); workbook_free(&book); workbook_free(&loaded); free(error);
}
static void test_unique_workspace(void) {
    char *error=NULL; Workbook book,loaded; workbook_init(&book); workbook_init(&loaded);
    Analysis source={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/duplicates.csv"),.unique_rows=true};
    CHECK(analysis_query_set(&source,"SELECT City, Amount::DECIMAL(12,2) AS Amount, Value FROM input",&error));
    CHECK(workbook_add(&book,"Unique rows",&source,&error)); Worksheet *sheet=&book.sheets[0];
    CHECK(objects_add_table(&sheet->objects,15,8,"Independent unique rows",&source,&error)==0);
    CHECK(objects_add(&sheet->objects,15,15,"Bound pivot",&error)==1);
    CHECK(objects_bind_source(&sheet->objects,1,sheet->objects.items[0].id,&error));
    Analysis changed=analysis_copy(&source); changed.unique_rows=false;
    CHECK(objects_set_source(&sheet->objects,0,&changed,&error));
    CHECK(objects_history(&sheet->objects,false) && sheet->objects.items[0].source.unique_rows);
    CHECK(objects_history(&sheet->objects,true) && !sheet->objects.items[0].source.unique_rows);
    CHECK(objects_set_source(&sheet->objects,0,&source,&error));
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot->sheets[0].analysis.unique_rows && snapshot->sheets[0].objects.items[0].source.unique_rows); workbook_free(snapshot); free(snapshot);
    const char *path="tests/generated/unique-workspace.dxls"; remove(path);
    CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    CHECK(loaded.sheets[0].analysis.unique_rows && loaded.sheets[0].objects.items[0].source.unique_rows && loaded.sheets[0].objects.items[1].source_id==sheet->objects.items[0].id);
    Database db; CHECK(database_open(&db,&error)); Page page={0};
    CHECK(database_fetch_table(&db,&loaded.sheets[0].objects.items[0].source,0,-1,&page,&error) && page.total==4); page_free(&page); database_close(&db);
    const char *invalid[]={
        "UPDATE _duxcel_workspace.source_options SET sheet_id=-1",
        "UPDATE _duxcel_workspace.source_options SET sheet_id=64",
        "UPDATE _duxcel_workspace.source_options SET object_id=999 WHERE object_id<>0",
        "UPDATE _duxcel_workspace.source_options SET object_id=2 WHERE object_id<>0",
        "UPDATE _duxcel_workspace.source_options SET unique_rows=NULL",
        "CREATE OR REPLACE TABLE _duxcel_workspace.source_options AS SELECT sheet_id,object_id,unique_rows::INTEGER AS unique_rows FROM _duxcel_workspace.source_options",
        "CREATE OR REPLACE TABLE _duxcel_workspace.source_options AS SELECT sheet_id,object_id::BIGINT AS object_id,unique_rows FROM _duxcel_workspace.source_options",
        "CREATE OR REPLACE TABLE _duxcel_workspace.source_options AS SELECT sheet_id,object_id,false AS unique_rows FROM _duxcel_workspace.source_options UNION ALL SELECT * FROM _duxcel_workspace.source_options",
        "CREATE OR REPLACE TABLE _duxcel_workspace.source_options AS SELECT NULL::INTEGER AS sheet_id,0::UBIGINT AS object_id,true AS unique_rows",
        "CREATE OR REPLACE TABLE _duxcel_workspace.source_options AS SELECT 0::INTEGER AS sheet_id,NULL::UBIGINT AS object_id,true AS unique_rows",
        "CREATE OR REPLACE TABLE _duxcel_workspace.source_options AS SELECT 0::INTEGER AS sheet_id,0::UBIGINT AS object_id,true AS unique_rows FROM range(2113)",
        "UPDATE _duxcel_workspace.sheets SET kind=0; DELETE FROM _duxcel_workspace.queries WHERE object_id=0",
        "DROP TABLE _duxcel_workspace.source_options"
    };
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        /* Restore schema as well as contents after intentionally malformed table types. */
        remove(path); CHECK(workbook_save(&book,path,&error)); sql_file(path,invalid[i]);
        CHECK(!workbook_load(&loaded,path,&error) && loaded.count==1 && loaded.sheets[0].analysis.unique_rows && loaded.sheets[0].objects.items[0].source.unique_rows); free(error); error=NULL;
    }
    remove(path); CHECK(workbook_save(&book,path,&error));
    sql_file(path,"UPDATE _duxcel_workspace.meta SET version=7; DROP TABLE _duxcel_workspace.source_options");
    CHECK(workbook_load(&loaded,path,&error) && !loaded.sheets[0].analysis.unique_rows && !loaded.sheets[0].objects.items[0].source.unique_rows && loaded.sheets[0].analysis.query);
    CHECK(workbook_save(&loaded,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    CHECK(!loaded.sheets[0].analysis.unique_rows && !loaded.sheets[0].objects.items[0].source.unique_rows);
    loaded.sheets[0].analysis.kind=SOURCE_NONE; loaded.sheets[0].analysis.unique_rows=true;
    CHECK(!workbook_save(&loaded,path,&error)); free(error); error=NULL;
    sheet->objects.items[1].source.unique_rows=true; CHECK(!objects_valid(&sheet->objects,&error));
    analysis_free(&source); analysis_free(&changed); workbook_free(&book); workbook_free(&loaded); free(error);
}
static void test_pivot_grain_workspace(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); char *error=NULL;
    Analysis source={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/transactions.csv")};
    CHECK(workbook_add(&book,"Date grains",&source,&error)); ObjectStore *store=&book.sheets[0].objects;
    CHECK(objects_add(store,12,10,"Monthly flow",&error)==0);
    PivotDefinition pivot={0}; CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign_grain(&pivot,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_YEAR,&error));
    CHECK(pivot_assign_grain(&pivot,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_QUARTER,&error));
    CHECK(pivot_assign_grain(&pivot,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_MONTH,&error));
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error)); CHECK(objects_update(store,0,&pivot,&error));
    uint64_t revision=store->items[0].revision; size_t history=store->undo_count;
    CHECK(objects_update(store,0,&pivot,&error) && store->items[0].revision==revision && store->undo_count==history);
    CHECK(pivot_move(&pivot,PIVOT_COLUMNS,2,PIVOT_ROWS,0,&error)); CHECK(objects_update(store,0,&pivot,&error));
    CHECK(store->items[0].revision==revision+1 && store->items[0].pivot.fields[PIVOT_ROWS][0].grain==PIVOT_DATE_MONTH);
    CHECK(objects_history(store,false) && store->items[0].pivot.fields[PIVOT_COLUMNS][2].grain==PIVOT_DATE_MONTH);
    CHECK(objects_history(store,true) && store->items[0].pivot.fields[PIVOT_ROWS][0].grain==PIVOT_DATE_MONTH);
    CHECK(objects_history(store,false)); const char *path="tests/generated/date-grains.dxls"; remove(path);
    CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    PivotDefinition *restored=&loaded.sheets[0].objects.items[0].pivot;
    CHECK(restored->counts[PIVOT_COLUMNS]==3 && restored->fields[PIVOT_COLUMNS][0].grain==PIVOT_DATE_YEAR && restored->fields[PIVOT_COLUMNS][1].grain==PIVOT_DATE_QUARTER && restored->fields[PIVOT_COLUMNS][2].grain==PIVOT_DATE_MONTH);
    CHECK(restored->fields[PIVOT_COLUMNS][0].name!=store->items[0].pivot.fields[PIVOT_COLUMNS][0].name && !strcmp(restored->fields[PIVOT_COLUMNS][0].name,"Date"));
    Workbook *copy=workbook_snapshot(&loaded); CHECK(copy->sheets[0].objects.items[0].pivot.fields[PIVOT_COLUMNS][2].grain==PIVOT_DATE_MONTH); workbook_free(copy); free(copy);
    Database db; Page page={0}; CHECK(database_open(&db,&error));
    CHECK(database_pivot(&db,&loaded.sheets[0].analysis,restored,0,-1,&page,&error) && page.total==4 && page.column_count==2 && strstr(page.columns[1].caption,"Jan 2026")); page_free(&page); database_close(&db);
    const char *invalid[]={
        "UPDATE _duxcel_workspace.pivot_fields SET grain=4 WHERE role=1 AND position=0",
        "UPDATE _duxcel_workspace.pivot_fields SET grain=-1 WHERE role=1 AND position=0",
        "UPDATE _duxcel_workspace.pivot_fields SET grain=NULL WHERE role=1 AND position=0",
        "UPDATE _duxcel_workspace.pivot_fields SET grain=1 WHERE role=2",
        "UPDATE _duxcel_workspace.pivot_fields SET grain=1 WHERE role=1 AND position=1",
        "ALTER TABLE _duxcel_workspace.pivot_fields ALTER COLUMN grain TYPE VARCHAR",
        "ALTER TABLE _duxcel_workspace.pivot_fields DROP COLUMN grain"
    };
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        remove(path); CHECK(workbook_save(&book,path,&error)); sql_file(path,invalid[i]);
        CHECK(!workbook_load(&loaded,path,&error) && loaded.sheets[0].objects.items[0].pivot.fields[PIVOT_COLUMNS][2].grain==PIVOT_DATE_MONTH); free(error); error=NULL;
    }
    /* A genuine version-8 file has raw fields and no grain column. */
    pivot_free(&pivot); CHECK(pivot_assign(&pivot,PIVOT_COLUMNS,"Date",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error)); CHECK(objects_update(store,0,&pivot,&error));
    remove(path); CHECK(workbook_save(&book,path,&error)); sql_file(path,"UPDATE _duxcel_workspace.meta SET version=8; ALTER TABLE _duxcel_workspace.pivot_fields DROP COLUMN grain");
    CHECK(workbook_load(&loaded,path,&error) && loaded.sheets[0].objects.items[0].pivot.fields[PIVOT_COLUMNS][0].grain==PIVOT_DATE_EXACT);
    CHECK(workbook_save(&loaded,path,&error)); sql_file(path,"SELECT CASE WHEN version=12 THEN true ELSE error('wrong version') END FROM _duxcel_workspace.meta; SELECT CASE WHEN bool_and(grain=0) THEN true ELSE error('raw date grain') END FROM _duxcel_workspace.pivot_fields");
    pivot_free(&pivot); analysis_free(&source); workbook_free(&book); workbook_free(&loaded);
}
static void format_model_history(Worksheet *sheet,bool redo) {
    size_t n=redo ? sheet->redo_actions_count : sheet->undo_actions_count; CHECK(n);
    unsigned effects=worksheet_edit_effects((redo ? sheet->redo_actions : sheet->undo_actions)[n-1]);
    if (effects&2) CHECK(cells_history(&sheet->cells,redo)); if (effects&4) CHECK(objects_history(&sheet->objects,redo)); if (effects&8) CHECK(format_history(&sheet->formats,redo));
    worksheet_transfer_history(sheet,redo);
}
static void test_format_workspace(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"Formats",&blank,&error)); Worksheet *sheet=&book.sheets[0]; CellFormat f=format_default(); f.flags=FORMAT_BOLD|FORMAT_ITALIC; f.color=2; f.number=FORMAT_ACCOUNTING;
    CHECK(format_apply(&sheet->formats,(FormatRange){2,8,2,9},FORMAT_BOLD|FORMAT_ITALIC|FORMAT_COLOR|FORMAT_NUMERIC,f,&error)); worksheet_record(sheet,EDIT_FORMAT);
    CHECK(cells_set(&sheet->cells,4,9,"Original raw text",&error)); worksheet_record(sheet,EDIT_CELLS);
    CHECK(objects_add(&sheet->objects,3,3,"Styled view",&error)==0); worksheet_record(sheet,EDIT_OBJECTS);
    CHECK(worksheet_move_range(sheet,(SheetRange){3,7,3,9},10,5,0,0,&error));
    CHECK(sheet->undo_actions[sheet->undo_actions_count-1]==EDIT_RANGE_FORMAT && sheet->objects.items[0].row==13 && !strcmp(cells_get(&sheet->cells,14,14),"Original raw text") && format_at(&sheet->formats,14,14).color==2 && !format_at(&sheet->formats,4,9).flags);
    format_model_history(sheet,false); CHECK(sheet->objects.items[0].row==3 && !strcmp(cells_get(&sheet->cells,4,9),"Original raw text") && format_at(&sheet->formats,4,9).color==2);
    format_model_history(sheet,true); CHECK(sheet->objects.items[0].row==13 && format_at(&sheet->formats,14,14).flags==(FORMAT_BOLD|FORMAT_ITALIC));
    CHECK(worksheet_column(sheet,8,true,0,&error) && sheet->undo_actions[sheet->undo_actions_count-1]==EDIT_RANGE_FORMAT && format_at(&sheet->formats,14,15).color==2);
    format_model_history(sheet,false); CHECK(sheet->objects.items[0].col==8 && format_at(&sheet->formats,14,14).color==2);
    CHECK(worksheet_rows(sheet,10,2,true,0,&error) && sheet->undo_actions[sheet->undo_actions_count-1]==EDIT_RANGE_FORMAT && format_at(&sheet->formats,16,14).color==2);
    format_model_history(sheet,false); CHECK(sheet->objects.items[0].row==13 && format_at(&sheet->formats,14,14).color==2);
    f.flags=FORMAT_UNDERLINE; f.size=16; f.align=FORMAT_CENTER; f.number=FORMAT_NUMBER; f.decimals=4;
    CHECK(format_apply(&sheet->formats,(FormatRange){0,DX_SHEET_ROWS-1,20,20},FORMAT_ALL,f,&error)); worksheet_record(sheet,EDIT_FORMAT);
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot->sheets[0].formats.items!=sheet->formats.items && !snapshot->sheets[0].formats.undo_count && format_at(&snapshot->sheets[0].formats,DX_SHEET_ROWS-1,20).decimals==4); workbook_free(snapshot); free(snapshot);
    CHECK(workbook_duplicate(&book,0,&error)); sheet=&book.sheets[0]; CHECK(book.sheets[1].formats.count==sheet->formats.count && book.sheets[1].formats.items!=sheet->formats.items);
    const char *path="tests/generated/cell-formats.dxls"; remove(path); CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    CHECK(format_at(&loaded.sheets[0].formats,14,14).color==2 && format_at(&loaded.sheets[0].formats,DX_SHEET_ROWS-1,20).flags==FORMAT_UNDERLINE && !loaded.sheets[0].formats.undo_count && !strcmp(cells_get(&loaded.sheets[0].cells,14,14),"Original raw text"));
    const char *invalid[]={
        "UPDATE _duxcel_workspace.formats SET flags=256",
        "UPDATE _duxcel_workspace.formats SET mask=512",
        "UPDATE _duxcel_workspace.formats SET font_size=17",
        "UPDATE _duxcel_workspace.formats SET decimals=5",
        "UPDATE _duxcel_workspace.formats SET numeric_format=-1",
        "UPDATE _duxcel_workspace.formats SET color=NULL",
        "UPDATE _duxcel_workspace.formats SET c0=-1",
        "UPDATE _duxcel_workspace.formats SET r0=9,r1=8",
        "UPDATE _duxcel_workspace.formats SET sheet_id=64 WHERE sheet_id=0 AND position=0",
        "UPDATE _duxcel_workspace.formats SET position=999 WHERE position=0",
        "ALTER TABLE _duxcel_workspace.formats ALTER COLUMN r0 TYPE BIGINT",
        "DROP TABLE _duxcel_workspace.formats"
    };
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        remove(path); CHECK(workbook_save(&book,path,&error)); sql_file(path,invalid[i]); CHECK(!workbook_load(&loaded,path,&error) && loaded.count==2 && format_at(&loaded.sheets[0].formats,14,14).color==2); free(error); error=NULL;
    }
    remove(path); CHECK(workbook_save(&book,path,&error)); sql_file(path,"UPDATE _duxcel_workspace.meta SET version=9; DROP TABLE _duxcel_workspace.formats"); CHECK(workbook_load(&loaded,path,&error) && !loaded.sheets[0].formats.count); CHECK(workbook_save(&loaded,path,&error));
    /* Chronological pruning remains aligned across the independent histories. */
    for (unsigned i=0;i<70;i++) {
        f.flags=(unsigned char)(i%2 ? FORMAT_BOLD : FORMAT_ITALIC); CHECK(format_apply(&sheet->formats,(FormatRange){40,40,25,25},FORMAT_BOLD|FORMAT_ITALIC,f,&error)); worksheet_record(sheet,EDIT_FORMAT);
        char raw[32]; snprintf(raw,sizeof(raw),"Raw %u",i); CHECK(cells_set(&sheet->cells,40,25,raw,&error)); worksheet_record(sheet,EDIT_CELLS);
    }
    CHECK(sheet->undo_actions_count==32 && sheet->cells.undo_count==16 && sheet->formats.undo_count==16);
    for (unsigned i=0;i<32;i++) format_model_history(sheet,false); CHECK(!sheet->undo_actions_count && sheet->redo_actions_count==32);
    for (unsigned i=0;i<32;i++) format_model_history(sheet,true); CHECK(!strcmp(cells_get(&sheet->cells,40,25),"Raw 69") && format_at(&sheet->formats,40,25).flags==FORMAT_BOLD);
    workbook_free(&book); workbook_free(&loaded); free(error);
}
static void test_scoped_format_save_guard(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"Scoped save guard",&blank,&error)); Worksheet *sheet=&book.sheets[0];
    CHECK(cells_set(&sheet->cells,0,0,"Original file value",&error));
    const char *path="tests/generated/scoped-save-guard.dxls",*fresh="tests/generated/scoped-save-new.dxls";
    remove(path); remove(fresh); CHECK(workbook_save(&book,path,&error));
    FILE *file=fopen(path,"rb"); CHECK(file && fseek(file,0,SEEK_END)==0); long length=ftell(file); CHECK(length>0 && fseek(file,0,SEEK_SET)==0);
    unsigned char *before=dx_alloc((size_t)length),*after=dx_alloc((size_t)length);
    CHECK(fread(before,1,(size_t)length,file)==(size_t)length && !fclose(file));
    const char definition[]="exact source/version/schema definition"; FormatScopeView scope={9,definition,sizeof(definition)-1}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply_scoped(&sheet->formats,(FormatRange){0,0,0,0},FORMAT_BOLD,bold,&scope,1,false,&error)); worksheet_record(sheet,EDIT_FORMAT);
    Workbook *snapshot=workbook_snapshot(&book);
    CHECK(snapshot->sheets[0].formats.scopes[0].definition!=sheet->formats.scopes[0].definition && snapshot->sheets[0].formats.items[0].guard_ids!=sheet->formats.items[0].guard_ids);
    CHECK(!workbook_save(snapshot,path,&error) && error && strstr(error,"v12")); free(error); error=NULL;
    workbook_free(snapshot); free(snapshot);
    file=fopen(path,"rb"); CHECK(file && fread(after,1,(size_t)length,file)==(size_t)length && fgetc(file)==EOF && !fclose(file));
    CHECK(!memcmp(before,after,(size_t)length)); free(before); free(after);
    CHECK(!workbook_save(&book,fresh,&error) && error && strstr(error,"v12")); free(error); error=NULL;
    file=fopen(fresh,"rb"); CHECK(!file);
    CHECK(workbook_load(&loaded,path,&error) && !loaded.sheets[0].formats.count && !strcmp(cells_get(&loaded.sheets[0].cells,0,0),"Original file value"));
    CHECK(workbook_duplicate(&book,0,&error));
    CHECK(book.sheets[1].formats.scopes[0].definition!=book.sheets[0].formats.scopes[0].definition);
    format_free(&book.sheets[1].formats); sheet=&book.sheets[0];
    format_model_history(sheet,false); CHECK(!sheet->formats.count && sheet->formats.redo_count==1);
    /* Dormant session history is not serialized and must not block an ordinary save. */
    CHECK(workbook_save(&book,path,&error) && workbook_load(&loaded,path,&error));
    format_model_history(sheet,true); CHECK(format_resolve(&sheet->formats,0,0,&scope).flags==FORMAT_BOLD);
    CHECK(!workbook_save(&book,path,&error) && error && strstr(error,"v12")); free(error);
    workbook_free(&book); workbook_free(&loaded);
}
static unsigned char *workspace_bytes(const char *path,size_t *bytes) {
    FILE *file=fopen(path,"rb"); CHECK(file && fseek(file,0,SEEK_END)==0); long length=ftell(file);
    CHECK(length>0 && fseek(file,0,SEEK_SET)==0); *bytes=(size_t)length;
    unsigned char *data=dx_alloc(*bytes); CHECK(fread(data,1,*bytes,file)==*bytes && fgetc(file)==EOF && !fclose(file)); return data;
}
static void test_extended_format_metadata_guard(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"Supported workspace",&blank,&error));
    CHECK(cells_set(&book.sheets[0].cells,0,0,"Saved value",&error));
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&book.sheets[0].formats,(FormatRange){1,2,3,4},FORMAT_BOLD,bold,&error));
    const char *path="tests/generated/extended-format-metadata.dxls"; remove(path);
    CHECK(workbook_save(&book,path,&error) && workbook_load(&loaded,path,&error));
    CHECK(cells_set(&loaded.sheets[0].cells,0,0,"Keep unsaved live value",&error)); worksheet_record(&loaded.sheets[0],EDIT_CELLS); loaded.dirty=true;
    Worksheet *sheets=loaded.sheets; FormatPatch *items=sheets[0].formats.items; char *loaded_path=loaded.path;
    const char *invalid[]={
        "CREATE TABLE _duxcel_workspace.object_counters(sheet_id INTEGER,high_water UBIGINT)",
        "CREATE VIEW _duxcel_workspace.object_counters AS SELECT 0 AS sheet_id,0::UBIGINT AS high_water",
        "CREATE TABLE _duxcel_workspace.format_scopes(sheet_id INTEGER,scope_id INTEGER,owner UBIGINT,definition BLOB)",
        "CREATE TABLE _duxcel_workspace.format_scopes AS SELECT 0 AS sheet_id,0 AS scope_id,18446744073709551615::UBIGINT AS owner,'opaque'::BLOB AS definition",
        "CREATE VIEW _duxcel_workspace.format_scopes AS SELECT 0 AS sheet_id WHERE false",
        "CREATE TABLE _duxcel_workspace.format_selectors(sheet_id INTEGER,position INTEGER,kind INTEGER)",
        "CREATE VIEW _duxcel_workspace.format_selectors AS SELECT 0 AS kind",
        "CREATE TABLE _duxcel_workspace.format_guards(sheet_id INTEGER,position INTEGER,scope_id INTEGER)",
        "CREATE VIEW _duxcel_workspace.format_guards AS SELECT 0 AS scope_id WHERE false",
        "ALTER TABLE _duxcel_workspace.formats ADD COLUMN selector INTEGER DEFAULT 1",
        "ALTER TABLE _duxcel_workspace.formats ADD COLUMN scope_id UBIGINT DEFAULT 9007199254740993",
        "ALTER TABLE _duxcel_workspace.formats ADD COLUMN guard_ids UBIGINT[] DEFAULT [0]",
        "ALTER TABLE _duxcel_workspace.formats ADD COLUMN annotation VARCHAR",
        "ALTER TABLE _duxcel_workspace.formats DROP COLUMN color",
        "ALTER TABLE _duxcel_workspace.formats RENAME COLUMN color TO unknown_property",
        "ALTER TABLE _duxcel_workspace.formats ALTER COLUMN r0 TYPE BIGINT"
    };
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        remove(path); CHECK(workbook_save(&book,path,&error)); sql_file(path,invalid[i]);
        size_t before_bytes=0,after_bytes=0; unsigned char *before=workspace_bytes(path,&before_bytes);
        CHECK(!workbook_load(&loaded,path,&error) && error && *error); free(error); error=NULL;
        CHECK(loaded.sheets==sheets && loaded.path==loaded_path && loaded.dirty && loaded.count==1 && loaded.active==0 && loaded.user_scale==1);
        CHECK(sheets[0].formats.items==items && sheets[0].undo_actions_count==1 && sheets[0].cells.undo_count==1 && format_at(&sheets[0].formats,1,3).flags==FORMAT_BOLD);
        CHECK(!strcmp(cells_get(&sheets[0].cells,0,0),"Keep unsaved live value"));
        CHECK(!workbook_save(&book,path,&error) && error && *error); free(error); error=NULL;
        unsigned char *after=workspace_bytes(path,&after_bytes); CHECK(before_bytes==after_bytes && !memcmp(before,after,before_bytes)); free(before); free(after);
    }
    /* User tables and unrelated metadata are not unsupported format payloads.
       Supported columns may have a different physical order. */
    remove(path); CHECK(workbook_save(&book,path,&error));
    sql_file(path,"CREATE TABLE main.format_scopes(value VARCHAR); INSERT INTO main.format_scopes VALUES ('Keep user data'); CREATE TABLE _duxcel_workspace.custom_note(value VARCHAR); INSERT INTO _duxcel_workspace.custom_note VALUES ('Keep metadata'); CREATE TABLE _duxcel_workspace.reordered_formats AS SELECT decimals,numeric_format,font_size,color,alignment,flags,mask,c1,c0,r1,r0,position,sheet_id FROM _duxcel_workspace.formats; DROP TABLE _duxcel_workspace.formats; ALTER TABLE _duxcel_workspace.reordered_formats RENAME TO formats");
    CHECK(workbook_load(&loaded,path,&error) && format_at(&loaded.sheets[0].formats,1,3).flags==FORMAT_BOLD);
    CHECK(workbook_save(&book,path,&error) && workbook_load(&loaded,path,&error));
    sql_file(path,"SELECT CASE WHEN count(*)=1 AND min(value)='Keep user data' THEN true ELSE error('user data changed') END FROM main.format_scopes; SELECT CASE WHEN count(*)=1 AND min(value)='Keep metadata' THEN true ELSE error('unrelated metadata changed') END FROM _duxcel_workspace.custom_note; SELECT CASE WHEN version=12 THEN true ELSE error('workspace version changed') END FROM _duxcel_workspace.meta");
    workbook_free(&book); workbook_free(&loaded); free(error);
}
static void require_workspace_version(const char *path,int version) {
    char sql[180]; snprintf(sql,sizeof(sql),"SELECT CASE WHEN version=%d THEN true ELSE error('Unexpected workspace version') END FROM _duxcel_workspace.meta",version); sql_file(path,sql);
}
static void test_owner_counter_workspace(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); Analysis blank={0}; char *error=NULL;
    const char *path="tests/generated/owner-counter-v13.dxls",*ordinary="tests/generated/owner-counter-v12.dxls"; remove(path); remove(ordinary);
    CHECK(workbook_add(&book,"Durable owners",&blank,&error)); Worksheet *sheet=&book.sheets[0]; const uint64_t base=UINT64_C(9007199254740992);
    CHECK(cells_set(&sheet->cells,20,0,"NULL",&error) && cells_set(&sheet->cells,21,0,"12345678901234567890.0123456789",&error));
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&sheet->formats,(FormatRange){20,21,0,0},FORMAT_BOLD,bold,&error));
    sheet->objects.next_id=base; CHECK(objects_add(&sheet->objects,0,8,"Live owner",&error)==0 && objects_add(&sheet->objects,8,8,"Retired owner",&error)==1);
    CHECK(workbook_save(&book,ordinary,&error)); require_workspace_version(ordinary,12);
    CHECK(objects_remove(&sheet->objects,1) && sheet->objects.next_id==base+2);
    CHECK(workbook_save(&book,path,&error)); require_workspace_version(path,13);
    CHECK(workbook_load(&loaded,path,&error) && loaded.sheets[0].objects.next_id==base+2 && loaded.sheets[0].objects.items[0].id==base+1);
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,20,0),"NULL") && !strcmp(cells_get(&loaded.sheets[0].cells,21,0),"12345678901234567890.0123456789") && format_at(&loaded.sheets[0].formats,20,0).flags==FORMAT_BOLD);
    CHECK(objects_add(&loaded.sheets[0].objects,8,8,"After reopen",&error)==1 && loaded.sheets[0].objects.items[1].id==base+3);
    CHECK(objects_history(&loaded.sheets[0].objects,false) && loaded.sheets[0].objects.next_id==base+3 && objects_history(&loaded.sheets[0].objects,true));
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot->sheets[0].objects.next_id==base+2); CHECK(workbook_duplicate(snapshot,0,&error));
    remove(path); CHECK(workbook_save(snapshot,path,&error) && workbook_load(&loaded,path,&error) && loaded.count==2 && loaded.sheets[0].objects.next_id==base+2 && loaded.sheets[1].objects.next_id==base+2);
    CHECK(objects_add(&loaded.sheets[1].objects,8,8,"Duplicate after reopen",&error)==1 && loaded.sheets[1].objects.items[1].id==base+3 && loaded.sheets[0].objects.next_id==base+2); workbook_free(snapshot); free(snapshot);
    CHECK(objects_remove(&sheet->objects,0) && !sheet->objects.count && sheet->objects.next_id==base+2);
    remove(path); CHECK(workbook_save(&book,path,&error) && workbook_load(&loaded,path,&error) && !loaded.sheets[0].objects.count && loaded.sheets[0].objects.next_id==base+2);
    CHECK(objects_add(&loaded.sheets[0].objects,0,8,"Empty-store lifetime",&error)==0 && loaded.sheets[0].objects.items[0].id==base+3);
    sheet->objects.next_id=UINT64_MAX; CHECK(workbook_save(&book,path,&error) && workbook_load(&loaded,path,&error) && loaded.sheets[0].objects.next_id==UINT64_MAX);
    CHECK(objects_add(&loaded.sheets[0].objects,0,8,"Exhausted after reopen",&error)<0 && error && !loaded.sheets[0].objects.count); free(error); error=NULL;
    Workbook fresh; workbook_init(&fresh); CHECK(workbook_add(&fresh,"Fresh overwrite",&blank,&error));
    CHECK(workbook_save(&fresh,path,&error)); require_workspace_version(path,13); /* Existing v13 never silently downgrades. */
    remove(ordinary); CHECK(workbook_save(&fresh,ordinary,&error)); require_workspace_version(ordinary,12); /* A fresh lossless save remains v12. */
    CHECK(objects_add(&fresh.sheets[0].objects,0,8,"Validated live owner",&error)==0); fresh.sheets[0].objects.next_id=0;
    size_t bytes=0,after_bytes=0; unsigned char *before=workspace_bytes(path,&bytes);
    CHECK(!workbook_save(&fresh,path,&error) && error && strstr(error,"high-water")); free(error); error=NULL;
    unsigned char *after=workspace_bytes(path,&after_bytes); CHECK(bytes==after_bytes && !memcmp(before,after,bytes)); free(before); free(after);
    workbook_free(&fresh); workbook_free(&book); workbook_free(&loaded); free(error);
}
static void test_invalid_owner_package(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); Analysis blank={0}; char *error=NULL;
    const char *path="tests/generated/invalid-owner-package.dxls"; CHECK(workbook_add(&book,"Owner package",&blank,&error));
    Worksheet *sheet=&book.sheets[0]; CHECK(objects_add(&sheet->objects,0,8,"Live",&error)==0); sheet->objects.next_id=UINT64_C(9007199254740993);
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&sheet->formats,(FormatRange){20,20,0,0},FORMAT_BOLD,bold,&error));
    remove(path); CHECK(workbook_save(&book,path,&error) && workbook_load(&loaded,path,&error)); CHECK(cells_set(&loaded.sheets[0].cells,30,0,"Keep unsaved value",&error)); worksheet_record(&loaded.sheets[0],EDIT_CELLS); loaded.dirty=true;
    Worksheet *owned=loaded.sheets; char *owned_path=loaded.path; FormatPatch *formats=owned[0].formats.items;
    const char *invalid[]={
        "DROP TABLE _duxcel_workspace.object_counters", "DROP TABLE _duxcel_workspace.format_scopes", "DROP TABLE _duxcel_workspace.format_selectors", "DROP TABLE _duxcel_workspace.format_guards",
        "DROP TABLE _duxcel_workspace.format_scopes; CREATE VIEW _duxcel_workspace.format_scopes AS SELECT 0::INTEGER AS sheet_id,0::INTEGER AS scope_id,0::UBIGINT AS owner,0::INTEGER AS encoding,''::BLOB AS definition WHERE false",
        "ALTER TABLE _duxcel_workspace.objects ALTER COLUMN id TYPE VARCHAR", "ALTER TABLE _duxcel_workspace.objects ALTER COLUMN source_id TYPE BIGINT",
        "ALTER TABLE _duxcel_workspace.meta ADD COLUMN unknown_feature INTEGER", "ALTER TABLE _duxcel_workspace.meta ALTER COLUMN version TYPE BIGINT",
        "ALTER TABLE _duxcel_workspace.format_scopes ADD COLUMN extra INTEGER", "ALTER TABLE _duxcel_workspace.object_counters ALTER COLUMN high_water TYPE BIGINT",
        "INSERT INTO _duxcel_workspace.format_scopes VALUES(0,0,0,0,'opaque'::BLOB)", "INSERT INTO _duxcel_workspace.format_guards VALUES(0,0,0,0)", "UPDATE _duxcel_workspace.format_selectors SET kind=1",
        "CREATE TABLE _duxcel_workspace.duplicate_counters AS SELECT * FROM _duxcel_workspace.object_counters; INSERT INTO _duxcel_workspace.duplicate_counters SELECT * FROM _duxcel_workspace.object_counters; DROP TABLE _duxcel_workspace.object_counters; ALTER TABLE _duxcel_workspace.duplicate_counters RENAME TO object_counters",
        "CREATE OR REPLACE TABLE _duxcel_workspace.object_counters AS SELECT NULL::INTEGER AS sheet_id,9::UBIGINT AS high_water",
        "CREATE TABLE _duxcel_workspace.duplicate_selectors AS SELECT * FROM _duxcel_workspace.format_selectors; INSERT INTO _duxcel_workspace.duplicate_selectors SELECT * FROM _duxcel_workspace.format_selectors; DROP TABLE _duxcel_workspace.format_selectors; ALTER TABLE _duxcel_workspace.duplicate_selectors RENAME TO format_selectors",
        "DELETE FROM _duxcel_workspace.object_counters", "UPDATE _duxcel_workspace.object_counters SET high_water=NULL", "UPDATE _duxcel_workspace.object_counters SET high_water=0", "UPDATE _duxcel_workspace.object_counters SET sheet_id=1",
        "DELETE FROM _duxcel_workspace.format_selectors", "UPDATE _duxcel_workspace.format_selectors SET position=1", "UPDATE _duxcel_workspace.format_selectors SET kind=NULL", "UPDATE _duxcel_workspace.format_selectors SET sheet_id=1",
        "UPDATE _duxcel_workspace.meta SET version=12"
    };
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        remove(path); CHECK(workbook_save(&book,path,&error)); sql_file(path,invalid[i]); size_t bytes=0,after_bytes=0; unsigned char *before=workspace_bytes(path,&bytes);
        CHECK(!workbook_load(&loaded,path,&error) && error && *error); free(error); error=NULL;
        CHECK(loaded.sheets==owned && loaded.path==owned_path && loaded.dirty && owned[0].formats.items==formats && owned[0].objects.next_id==UINT64_C(9007199254740993) && owned[0].undo_actions_count==1 && !strcmp(cells_get(&owned[0].cells,30,0),"Keep unsaved value"));
        CHECK(!workbook_save(&book,path,&error) && error && *error); free(error); error=NULL;
        unsigned char *after=workspace_bytes(path,&after_bytes); CHECK(bytes==after_bytes && !memcmp(before,after,bytes)); free(before); free(after);
    }
    remove(path); CHECK(workbook_save(&book,path,&error));
    const char definition[]="Unsupported live scope"; FormatScopeView scope={1,definition,sizeof(definition)-1}; CHECK(format_apply_scoped(&sheet->formats,(FormatRange){0,0,0,0},FORMAT_BOLD,bold,&scope,1,false,&error));
    size_t bytes=0,after_bytes=0; unsigned char *before=workspace_bytes(path,&bytes); CHECK(!workbook_save(&book,path,&error) && error && strstr(error,"v12")); free(error); error=NULL;
    unsigned char *after=workspace_bytes(path,&after_bytes); CHECK(bytes==after_bytes && !memcmp(before,after,bytes)); free(before); free(after);
    workbook_free(&book); workbook_free(&loaded); free(error);
}
static void test_combined_filter_workspace(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); char *error=NULL;
    Analysis source={.kind=SOURCE_DEMO,.path=dx_strdup("")}; char *payload=analysis_filter_pack(FILTER_NOT_IN,"[\"Bursa\"]",FILTER_CONTAINS,"stan");
    CHECK(payload && analysis_filter_set(&source,"City",FILTER_COMPOSITE,payload,&error));
    CHECK(analysis_filter_set(&source,"Amount",FILTER_BETWEEN,"[\"1.2\",\"99.9\"]",&error));
    CHECK(workbook_add(&book,"Combined",&source,&error)); ObjectStore *objects=&book.sheets[0].objects;
    CHECK(objects_add_table(objects,0,10,"Independent",&source,&error)==0); CHECK(objects_add(objects,20,10,"Filtered pivot",&error)==1);
    PivotDefinition pivot={0}; CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_FILTERS,"City",AGG_SUM,&error));
    pivot.fields[PIVOT_FILTERS][0].filter=FILTER_COMPOSITE; free(pivot.fields[PIVOT_FILTERS][0].value); pivot.fields[PIVOT_FILTERS][0].value=dx_strdup(payload);
    CHECK(objects_update(objects,1,&pivot,&error)); CHECK(objects_bind_source(objects,1,objects->items[0].id,&error));
    CHECK(objects_history(objects,false)); CHECK(objects_history(objects,true)); CHECK(!strcmp(objects->items[1].pivot.fields[PIVOT_FILTERS][0].value,payload));
    Workbook *copy=workbook_snapshot(&book); CHECK(copy && copy->sheets[0].analysis.filter_value!=source.filter_value); workbook_free(copy); free(copy);
    const char *path="tests/generated/combined-filters.dxls"; remove(path);
    CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    CHECK(analysis_same_result(&source,&loaded.sheets[0].analysis));
    CHECK(loaded.sheets[0].objects.items[0].source.filter_op==FILTER_COMPOSITE && !strcmp(loaded.sheets[0].objects.items[0].source.filter_value,payload));
    CHECK(loaded.sheets[0].objects.items[1].pivot.fields[PIVOT_FILTERS][0].filter==FILTER_COMPOSITE && !strcmp(loaded.sheets[0].objects.items[1].pivot.fields[PIVOT_FILTERS][0].value,payload));
    const char *invalid[]={"UPDATE _duxcel_workspace.sheets SET filter_value='[]'", "UPDATE _duxcel_workspace.pivot_fields SET value='[]' WHERE role=3", "UPDATE _duxcel_workspace.table_sources SET filter_value='[]'"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        CHECK(workbook_save(&book,path,&error)); sql_file(path,invalid[i]); CHECK(!workbook_load(&loaded,path,&error)); CHECK(analysis_same_result(&source,&loaded.sheets[0].analysis)); free(error); error=NULL;
    }
    pivot_free(&pivot); analysis_free(&source); free(payload); workbook_free(&book); workbook_free(&loaded); free(error);
}
static void test_marks_workspace(void) {
    char *error=NULL; MarkDefinition marks={0};
    MarkField account={.name="Account"},category={.name="Category"},amount={.name="Amount",.measure=true,.aggregate=AGG_SUM},average={.name="Amount",.measure=true,.aggregate=AGG_AVG};
    CHECK(marks_assign(&marks,MARKS_COLOR,&account,&error)); CHECK(marks_assign(&marks,MARKS_COLOR,&category,&error)); CHECK(marks.counts[MARKS_COLOR]==2 && marks.fields[MARKS_COLOR][0].name!=account.name);
    char *original=marks.fields[MARKS_COLOR][0].name; CHECK(marks_assign(&marks,MARKS_COLOR,&account,&error)); CHECK(marks.counts[MARKS_COLOR]==2 && marks.fields[MARKS_COLOR][0].name==original);
    CHECK(marks_assign(&marks,MARKS_SIZE,&amount,&error)); CHECK(marks_assign(&marks,MARKS_SIZE,&average,&error)); CHECK(marks.counts[MARKS_SIZE]==1 && marks.fields[MARKS_SIZE][0].aggregate==AGG_AVG);
    CHECK(marks_assign(&marks,MARKS_LABEL,&amount,&error)); CHECK(marks_assign(&marks,MARKS_DETAIL,&account,&error)); CHECK(marks_assign(&marks,MARKS_TOOLTIP,&amount,&error)); CHECK(marks_assign(&marks,MARKS_SHAPE,&category,&error));
    CHECK(marks_move(&marks,MARKS_COLOR,0,1)); CHECK(!strcmp(marks.fields[MARKS_COLOR][0].name,"Category"));
    MarkDefinition copy=marks_copy(&marks); CHECK(marks_equal(&marks,&copy) && copy.fields[MARKS_COLOR][0].name!=marks.fields[MARKS_COLOR][0].name);
    marks_remove(&copy,MARKS_COLOR,0); CHECK(!marks_equal(&marks,&copy) && copy.counts[MARKS_COLOR]==1 && marks.counts[MARKS_COLOR]==2); marks_free(&copy);
    MarkField bad=amount; bad.grain=PIVOT_DATE_MONTH; CHECK(!marks_assign(&marks,MARKS_LABEL,&bad,&error)); free(error); error=NULL;
    for (int i=0;i<7;i++) { char name[16]; snprintf(name,sizeof(name),"Field %d",i); MarkField f={.name=name}; CHECK(marks_assign(&marks,MARKS_LABEL,&f,&error)); }
    CHECK(marks.counts[MARKS_LABEL]==8 && !marks_assign(&marks,MARKS_LABEL,&account,&error)); free(error); error=NULL;
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); Analysis source={.kind=SOURCE_DEMO}; CHECK(workbook_add(&book,"Marks",&source,&error));
    ObjectStore *objects=&book.sheets[0].objects; CHECK(objects_add(objects,0,8,"Encoded view",&error)==0);
    uint64_t revision=objects->items[0].revision,title=objects->title_revision; size_t undo=objects->undo_count;
    CHECK(objects_marks(objects,0,&marks,&error)); CHECK(objects->items[0].revision==revision && objects->title_revision==title && objects->marks_revision==1 && objects->undo_count==undo+1);
    CHECK(objects_marks(objects,0,&marks,&error) && objects->undo_count==undo+1); CHECK(marks_equal(&objects->items[0].marks,&marks));
    CHECK(objects_history(objects,false) && !objects->items[0].marks.counts[MARKS_COLOR] && objects->items[0].revision==revision);
    CHECK(objects_history(objects,true) && marks_equal(&objects->items[0].marks,&marks) && objects->items[0].revision==revision);
    CHECK(objects_representation(objects,0,OBJECT_CHART,MARK_LINE,12,6,&book.sheets[0].cells,0,0,&error)); CHECK(marks_equal(&objects->items[0].marks,&marks));
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot && marks_equal(&snapshot->sheets[0].objects.items[0].marks,&marks) && snapshot->sheets[0].objects.items[0].marks.fields[MARKS_COLOR][0].name!=objects->items[0].marks.fields[MARKS_COLOR][0].name); workbook_free(snapshot); free(snapshot);
    const char *path="tests/generated/marks-workspace.dxls"; remove(path); CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error)); CHECK(marks_equal(&loaded.sheets[0].objects.items[0].marks,&marks));
    const char *invalid[]={"UPDATE _duxcel_workspace.marks SET role=6 WHERE role=5", "UPDATE _duxcel_workspace.marks SET position=7 WHERE role=1", "UPDATE _duxcel_workspace.marks SET name=NULL WHERE role=0", "UPDATE _duxcel_workspace.marks SET is_measure=NULL WHERE role=0", "UPDATE _duxcel_workspace.marks SET grain=3 WHERE is_measure", "UPDATE _duxcel_workspace.marks SET object_id=999", "UPDATE _duxcel_workspace.marks SET name='Account' WHERE role=0", "ALTER TABLE _duxcel_workspace.marks ALTER COLUMN is_measure TYPE INTEGER", "DROP TABLE _duxcel_workspace.marks"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) { CHECK(workbook_save(&book,path,&error)); sql_file(path,invalid[i]); CHECK(!workbook_load(&loaded,path,&error)); CHECK(marks_equal(&loaded.sheets[0].objects.items[0].marks,&marks)); free(error); error=NULL; remove(path); }
    CHECK(workbook_save(&book,path,&error)); sql_file(path,"DROP TABLE _duxcel_workspace.marks; UPDATE _duxcel_workspace.meta SET version=11"); CHECK(workbook_load(&loaded,path,&error)); CHECK(!loaded.sheets[0].objects.items[0].marks.counts[MARKS_COLOR]);
    marks_free(&marks); workbook_free(&book); workbook_free(&loaded); free(error);
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--owners-only")) {
        test_owner_counter_workspace(); test_invalid_owner_package();
        printf("PASS: %u focused owner-counter workspace checks (v12 compatibility, v13 retired IDs, strict spatial package and atomic rejection)\n",checks); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--formats-only")) {
        test_format_workspace();
        test_scoped_format_save_guard();
        test_extended_format_metadata_guard();
        printf("PASS: %u focused workspace formatting checks (format round-trip, validation, scoped-save rejection and original-file preservation)\n",checks);
        return 0;
    }
    if (argc!=1) { fprintf(stderr,"Usage: %s [--formats-only|--owners-only]\n",argv[0]); return 2; }
    test_owner_counter_workspace(); test_invalid_owner_package();
    test_chart_optional_errors();
    test_marks_workspace();
    test_combined_filter_workspace();
    test_pivot_grain_workspace();
    test_format_workspace();
    test_scoped_format_save_guard();
    test_extended_format_metadata_guard();
    test_unique_workspace();
    test_query_workspace();
    test_cells();
    test_objects(); test_chart_measure(); test_mixed_history(); test_range_move(); test_worksheet_columns(); test_worksheet_rows(); test_multiple_rows(); test_chart_resize(); test_object_names(); test_table_sources(); test_table_resolution(); test_table_workspace(); test_composite_history_budget(); test_sort_workspace(); test_large_canvas(); test_sheet_management();
    char *error=NULL; Workbook b,loaded; workbook_init(&b); workbook_init(&loaded);
    CHECK(workbook_is_path("Book.dxls")); CHECK(workbook_is_path("Book.DXLs"));
    CHECK(workbook_is_path("legacy.duxcel")); CHECK(!workbook_is_path("data.parquet"));
    CHECK(!workbook_is_path("dxls")); CHECK(!workbook_is_path("Book.dxls.csv"));
    Analysis a={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/transactions.csv"),.filter_column=dx_strdup("Amount"),
        .filter_value=dx_strdup("10"),.filter_op=FILTER_GREATER,.sort_column=dx_strdup("City")};
    CHECK(workbook_add(&b,"İstanbul analysis",&a,&error));
    CHECK(!workbook_save(&b,a.path,&error) && error && strstr(error,"data source")); free(error); error=NULL;
    /* This intentionally passes a definition inside a reallocating workbook. */
    CHECK(workbook_add(&b,b.sheets[0].name,&b.sheets[0].analysis,&error));
    free(b.sheets[1].name); b.sheets[1].name=dx_strdup("Grouped");
    b.sheets[1].analysis.group_column=dx_strdup("City"); b.sheets[1].analysis.measure_column=dx_strdup("Amount");
    b.sheets[1].analysis.aggregate=AGG_SUM; b.user_scale=1.2f;
    SheetView *v=&b.sheets[0].view;
    *v=(SheetView){.row=3,.col=1,.anchor_row=2,.anchor_col=0,.end_row=3,.end_col=1,.top=2,.horizontal=12.5f,.fraction=.25,.selection=0,.column_count=2};
    v->columns=dx_alloc(2*sizeof(char *)); v->widths=dx_alloc(2*sizeof(float));
    v->columns[0]=dx_strdup("ID"); v->columns[1]=dx_strdup("City"); v->widths[0]=200; v->widths[1]=220; v->canvas_zoom=1.5f;
    const char *path="tests/generated/workspace.dxls"; remove(path);
    b.reduced_motion=true;
    CHECK(workbook_save(&b,path,&error)); CHECK(workbook_load(&loaded,path,&error)); CHECK(loaded.reduced_motion);
    CHECK(loaded.count==2 && loaded.active==1 && fabsf(loaded.user_scale-1.2f)<.0001f);
    CHECK(!strcmp(loaded.sheets[0].name,"İstanbul analysis")); CHECK(!strcmp(loaded.sheets[0].analysis.path,a.path));
    CHECK(loaded.sheets[0].view.widths[0]==200 && loaded.sheets[0].view.end_row==3 && loaded.sheets[0].view.fraction==.25);
    CHECK(loaded.sheets[0].view.canvas_zoom==1.5f);
    CHECK(loaded.sheets[0].undo_count==0 && loaded.sheets[0].redo_count==0);
    for (size_t i=0;i<2;i++) {
        char *before=analysis_sql(&b.sheets[i].analysis,true),*after=analysis_sql(&loaded.sheets[i].analysis,true);
        CHECK(!strcmp(before,after)); free(before); free(after);
        Database db; Page p; CHECK(database_open(&db,&error));
        CHECK(database_fetch(&db,&loaded.sheets[i].analysis,0,-1,&p,&error)); CHECK(p.total==3); page_free(&p); database_close(&db);
    }
    char *target=workbook_target_path(path),*relative=workbook_relative_path(target,a.path),*resolved=workbook_source_path(target,relative);
    CHECK(relative[0]!='/' && !strcmp(resolved,a.path)); free(target); free(relative); free(resolved);
    Workbook *snapshot=workbook_snapshot(&b); b.sheets[0].view.widths[0]=210;
    CHECK(snapshot->sheets[0].view.widths[0]==200); workbook_free(snapshot); free(snapshot); b.sheets[0].view.widths[0]=200;
    sql_file(path,"UPDATE _duxcel_workspace.meta SET version=999");
    CHECK(!workbook_load(&loaded,path,&error)); CHECK(loaded.count==2 && loaded.active==1); free(error); error=NULL;
    CHECK(!workbook_save(&b,path,&error)); free(error); error=NULL;
    sql_file(path,"UPDATE _duxcel_workspace.meta SET version=1");
    /* A failed INSERT must roll back earlier deletes/inserts in the same save. */
    sql_file(path,"ALTER TABLE _duxcel_workspace.columns RENAME TO columns_old;"
        "CREATE TABLE _duxcel_workspace.columns(sheet_id INTEGER,column_id INTEGER,name VARCHAR,width DOUBLE CHECK(width<250),PRIMARY KEY(sheet_id,column_id));"
        "INSERT INTO _duxcel_workspace.columns SELECT * FROM _duxcel_workspace.columns_old;");
    b.sheets[0].view.widths[0]=400; CHECK(!workbook_save(&b,path,&error)); free(error); error=NULL;
    CHECK(workbook_load(&loaded,path,&error)); CHECK(loaded.sheets[0].view.widths[0]==200);
    const char *unrelated="tests/generated/user-data.duckdb"; remove(unrelated); sql_file(unrelated,"CREATE TABLE important_data AS SELECT 42 AS value");
    CHECK(!workbook_save(&b,unrelated,&error)); CHECK(strstr(error,"not a dxls workbook")!=NULL); free(error); error=NULL;
    sql_file(unrelated,"SELECT CASE WHEN value=42 THEN true ELSE error('data overwritten') END FROM important_data");
    /* Move a workspace and its referenced file together; source resolution must follow it. */
    sql_file(NULL,"COPY (SELECT * FROM read_csv_auto('tests/fixtures/transactions.csv')) TO 'tests/generated/workspace-old/data.csv' (HEADER)");
    Workbook portable; workbook_init(&portable); Analysis local={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/generated/workspace-old/data.csv")};
    CHECK(workbook_add(&portable,"Portable",&local,&error)); remove("tests/generated/workspace-old/book.duxcel");
    CHECK(workbook_save(&portable,"tests/generated/workspace-old/book.duxcel",&error));
    remove("tests/generated/workspace-new/book.duxcel"); remove("tests/generated/workspace-new/data.csv");
    CHECK(rename("tests/generated/workspace-old/book.duxcel","tests/generated/workspace-new/book.duxcel")==0);
    CHECK(rename("tests/generated/workspace-old/data.csv","tests/generated/workspace-new/data.csv")==0);
    CHECK(workbook_load(&loaded,"tests/generated/workspace-new/book.duxcel",&error));
    char *moved=dx_absolute_path("tests/generated/workspace-new/data.csv"); CHECK(!strcmp(loaded.sheets[0].analysis.path,moved)); free(moved);
    remove("tests/generated/workspace-new/data.csv");
    CHECK(workbook_load(&loaded,"tests/generated/workspace-new/book.duxcel",&error));
    Database db; Page p; CHECK(database_open(&db,&error)); CHECK(!database_fetch(&db,&loaded.sheets[0].analysis,0,-1,&p,&error));
    page_free(&p); free(error); error=NULL;
    free(loaded.sheets[0].analysis.path); loaded.sheets[0].analysis.path=dx_strdup(a.path);
    CHECK(database_fetch(&db,&loaded.sheets[0].analysis,0,-1,&p,&error)); CHECK(p.total==5); page_free(&p); database_close(&db);
    Workbook empty; workbook_init(&empty); Analysis blank={0};
    CHECK(workbook_add(&empty,"Blank canvas",&blank,&error));
    empty.sheets[0].view=(SheetView){.row=9000,.col=80,.anchor_row=9000,.end_row=9000,.anchor_col=80,.end_col=80,.top=8990,.fraction=.5};
    CHECK(cells_set(&empty.sheets[0].cells,9000,80,"Saved cell İ",&error));
    remove("tests/generated/blank.duxcel"); CHECK(workbook_save(&empty,"tests/generated/blank.duxcel",&error));
    CHECK(workbook_load(&loaded,"tests/generated/blank.duxcel",&error));
    CHECK(loaded.count==1 && loaded.sheets[0].analysis.kind==SOURCE_NONE && loaded.sheets[0].view.row==9000 && loaded.sheets[0].view.col==80);
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,9000,80),"Saved cell İ"));
    CHECK(loaded.sheets[0].cells.undo_count==0);
    empty.user_scale=NAN; CHECK(!workbook_save(&empty,"tests/generated/blank.duxcel",&error)); free(error); error=NULL;
    CHECK(workbook_load(&loaded,"tests/generated/blank.duxcel",&error)); CHECK(loaded.user_scale==1);
    workbook_free(&empty);
    for (size_t i=b.count;i<DX_MAX_SHEETS;i++) CHECK(workbook_add(&b,"Sheet",&a,&error));
    CHECK(!workbook_add(&b,"Too many",&a,&error)); CHECK(b.count==DX_MAX_SHEETS); free(error);
    analysis_free(&local); workbook_free(&portable); analysis_free(&a); workbook_free(&b); workbook_free(&loaded);
    printf("PASS: %u workspace checks (definitions, view, transactions, versioning, ownership, relative paths, relocation and missing-source recovery)\n",checks); return 0;
}
