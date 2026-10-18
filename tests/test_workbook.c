#include "workbook/workbook.h"
#include "data/database.h"
#include "platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
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
        "CREATE TABLE _duxcel_workspace.text_object_ids AS SELECT sheet_id,id::VARCHAR AS id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure FROM _duxcel_workspace.objects; DROP TABLE _duxcel_workspace.objects; ALTER TABLE _duxcel_workspace.text_object_ids RENAME TO objects", "ALTER TABLE _duxcel_workspace.objects ALTER COLUMN source_id TYPE BIGINT",
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
static void source_version_issue(Worksheet *sheet,uint64_t owner,uint64_t version,bool complete) {
    char *error=NULL; Analysis source={.kind=SOURCE_CSV,.path="/private/source-version-model.csv"};
    SourcePacket raw={0};
    if (complete) {
        Column field={.name="Amount",.type_name="DECIMAL(38,4)",.type=DUCKDB_TYPE_DECIMAL,.number_type={NUM_DECIMAL,38,4},.numeric=true};
        Page schema={.columns=&field,.column_count=1}; SourceDigest digest={.bytes=3,.identity={.size=3}};
        memset(digest.sha256,0x5a,sizeof(digest.sha256));
        CHECK(source_scope_raw_build(owner,version,&source,&schema,&digest,"v1.5.6",&raw,&error));
    }
    SourceVersion entry={.owner=owner,.version=version,.kind=SOURCE_CSV,.path=source.path,.raw=raw}; SourceVersions next={0};
    CHECK(source_versions_plan(&sheet->source_versions,&entry,true,&next,&error));
    source_versions_free(&sheet->source_versions); sheet->source_versions=next; source_packet_free(&raw);
}
/* Exercise the existing Analysis stacks/journal, without introducing the future
   combined format action or pretending the App already creates source bindings. */
static void source_version_remember(Worksheet *sheet,uint64_t version) {
    CHECK(sheet->undo_count<DX_HISTORY); sheet->undo[sheet->undo_count++]=analysis_copy(&sheet->analysis);
    sheet->analysis.source_version=version; worksheet_record(sheet,EDIT_ANALYSIS);
}
static void source_version_history(Worksheet *sheet,bool redo) {
    Analysis *from=redo ? sheet->redo : sheet->undo,*to=redo ? sheet->undo : sheet->redo;
    size_t *fc=redo ? &sheet->redo_count : &sheet->undo_count,*tc=redo ? &sheet->undo_count : &sheet->redo_count;
    CHECK(*fc && *tc<DX_HISTORY); to[(*tc)++]=analysis_copy(&sheet->analysis); analysis_free(&sheet->analysis);
    sheet->analysis=from[--*fc]; memset(from+*fc,0,sizeof(*from)); worksheet_transfer_history(sheet,redo);
}
static void test_source_version_model(void) {
    char *error=NULL; const uint64_t base=UINT64_C(9007199254740992);
    Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("/private/source-version-model.csv"),.source_version=UINT64_MAX};
    Analysis copied=analysis_copy(&source); CHECK(copied.source_version==UINT64_MAX && copied.path!=source.path && !strcmp(copied.path,source.path));
    copied.source_version=0; CHECK(analysis_same_result(&source,&copied) && analysis_text_bytes(&source)==analysis_text_bytes(&copied));
    char *sql=analysis_sql(&source,false),*other=analysis_sql(&copied,false); CHECK(!strcmp(sql,other)); free(sql); free(other); analysis_free(&copied); CHECK(!copied.source_version);
    source.source_version=0; Workbook book; workbook_init(&book); CHECK(workbook_add(&book,"Source lifetime",&source,&error)); Worksheet *sheet=&book.sheets[0];
    sheet->source_versions.high_water=base; source_version_issue(sheet,0,base+1,true); source_version_remember(sheet,base+1);
    CHECK(sheet->undo[0].source_version==0 && sheet->analysis.source_version==base+1);
    source_version_issue(sheet,0,base+2,false); source_version_remember(sheet,base+2); SourceVersion *entries=sheet->source_versions.items;
    source_version_history(sheet,false); CHECK(sheet->analysis.source_version==base+1 && sheet->source_versions.high_water==base+2 && sheet->source_versions.items==entries);
    source_version_history(sheet,false); CHECK(!sheet->analysis.source_version && sheet->source_versions.high_water==base+2 && sheet->source_versions.count==2);
    CHECK(!source_versions_scope_matches(&sheet->source_versions,0,sheet->analysis.source_version,&sheet->analysis,entries[0].raw.data,entries[0].raw.bytes,&error)); free(error); error=NULL;
    source_version_history(sheet,true); source_version_history(sheet,true); CHECK(sheet->analysis.source_version==base+2 && sheet->source_versions.high_water==base+2);
    source_version_history(sheet,false); CHECK(sheet->analysis.source_version==base+1);
    SourceVersion reused={.version=base+2,.kind=SOURCE_CSV,.path=source.path}; SourceVersions plan={0};
    CHECK(!source_versions_plan(&sheet->source_versions,&reused,true,&plan,&error) && !plan.items && sheet->source_versions.high_water==base+2); free(error); error=NULL;
    source_version_issue(sheet,0,base+3,true); source_version_remember(sheet,base+3); CHECK(!sheet->redo_count && !sheet->redo_actions_count && sheet->source_versions.high_water==base+3);
    CHECK(objects_add_table(&sheet->objects,10,10,"Bound table",&source,&error)==0); uint64_t owner=sheet->objects.items[0].id;
    source_version_issue(sheet,owner,base+4,true); sheet->objects.items[0].source.source_version=base+4;
    CHECK(source_versions_valid(&sheet->source_versions,sheet->objects.next_id,&error));
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot && snapshot->sheets[0].analysis.source_version==base+3 && snapshot->sheets[0].objects.items[0].source.source_version==base+4);
    Worksheet *saved=&snapshot->sheets[0]; CHECK(!saved->undo_count && !saved->redo_count && !saved->undo_actions_count && !saved->objects.undo_count);
    CHECK(saved->source_versions.high_water==base+4 && saved->source_versions.count==4 && saved->source_versions.items!=sheet->source_versions.items);
    for (size_t i=0;i<4;i++) {
        CHECK(saved->source_versions.items[i].path!=sheet->source_versions.items[i].path && !strcmp(saved->source_versions.items[i].path,sheet->source_versions.items[i].path));
        if (saved->source_versions.items[i].raw.bytes) CHECK(saved->source_versions.items[i].raw.data!=sheet->source_versions.items[i].raw.data && !memcmp(saved->source_versions.items[i].raw.data,sheet->source_versions.items[i].raw.data,saved->source_versions.items[i].raw.bytes));
    }
    CHECK(workbook_duplicate(&book,0,&error) && book.count==2); sheet=&book.sheets[0]; Worksheet *duplicate=&book.sheets[1];
    CHECK(duplicate->analysis.source_version==base+3 && duplicate->objects.items[0].source.source_version==base+4 && duplicate->source_versions.high_water==base+4 && duplicate->source_versions.items!=sheet->source_versions.items && !duplicate->undo_count);
    CHECK(duplicate->source_versions.items[0].raw.data!=sheet->source_versions.items[0].raw.data);
    unsigned char first=saved->source_versions.items[0].raw.data[48]; sheet->source_versions.items[0].raw.data[48]^=1;
    CHECK(saved->source_versions.items[0].raw.data[48]==first && duplicate->source_versions.items[0].raw.data[48]==first);
    SourceVersion *owned=sheet->source_versions.items; CHECK(workbook_reorder(&book,0,1,&error) && book.sheets[1].source_versions.items==owned);
    workbook_free(&book); CHECK(!book.count && !book.sheets && saved->source_versions.high_water==base+4 && source_versions_valid(&saved->source_versions,saved->objects.next_id,&error));
    workbook_free(snapshot); free(snapshot);
    CHECK(workbook_add(&book,"Exhausted versions",&source,&error)); book.sheets[0].source_versions.high_water=UINT64_MAX;
    snapshot=workbook_snapshot(&book); CHECK(snapshot && snapshot->sheets[0].source_versions.high_water==UINT64_MAX && !snapshot->sheets[0].source_versions.count);
    CHECK(workbook_duplicate(&book,0,&error) && book.sheets[1].source_versions.high_water==UINT64_MAX);
    reused.version=UINT64_MAX; CHECK(!source_versions_plan(&book.sheets[1].source_versions,&reused,true,&plan,&error)); free(error); error=NULL;
    workbook_free(snapshot); free(snapshot); workbook_free(&book);
    CHECK(workbook_add(&book,"Valid first",&source,&error)); source_version_issue(&book.sheets[0],0,1,true);
    CHECK(workbook_add(&book,"Invalid second",&source,&error)); source_version_issue(&book.sheets[1],0,1,true); book.sheets[1].source_versions.high_water=0;
    Workbook before=book; CHECK(!workbook_snapshot(&book)); CHECK(!workbook_duplicate(&book,1,&error) && !memcmp(&before,&book,sizeof(book))); free(error); error=NULL;
    book.sheets[1].source_versions.high_water=1; CHECK(workbook_duplicate(&book,1,&error)); workbook_free(&book); analysis_free(&source);
}
static unsigned char *source_version_file(const char *path,size_t *length) {
    FILE *file=fopen(path,"rb"); CHECK(file && fseek(file,0,SEEK_END)==0); long bytes=ftell(file); CHECK(bytes>0 && fseek(file,0,SEEK_SET)==0);
    unsigned char *data=dx_alloc((size_t)bytes); CHECK(fread(data,1,(size_t)bytes,file)==(size_t)bytes && !fclose(file)); *length=(size_t)bytes; return data;
}
static void test_source_version_save_guard(void) {
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); Analysis blank={0},table={.kind=SOURCE_CSV,.path="/private/missing-version-source.csv"}; char *error=NULL;
    CHECK(workbook_add(&book,"Legacy remains supported",&blank,&error)); CHECK(cells_set(&book.sheets[0].cells,0,0,"Preserved original file",&error));
    CHECK(objects_add_table(&book.sheets[0].objects,10,10,"Ordinary table",&table,&error)==0);
    const char *paths[]={"tests/generated/source-version-v12.dxls","tests/generated/source-version-v13.dxls"},*fresh="tests/generated/source-version-rejected.dxls";
    remove(paths[0]); remove(paths[1]); remove(fresh); CHECK(workbook_save(&book,paths[0],&error));
    book.sheets[0].objects.next_id++; CHECK(workbook_save(&book,paths[1],&error));
    sql_file(paths[0],"SELECT CASE WHEN version=12 THEN true ELSE error('ordinary v12 changed') END FROM _duxcel_workspace.meta");
    sql_file(paths[1],"SELECT CASE WHEN version=13 THEN true ELSE error('owner-only v13 changed') END FROM _duxcel_workspace.meta");
    size_t lengths[2]; unsigned char *originals[2]; for (size_t i=0;i<2;i++) originals[i]=source_version_file(paths[i],lengths+i);
    for (unsigned kind=0;kind<9;kind++) {
        Workbook *trial=workbook_snapshot(&book); CHECK(trial); Worksheet *sheet=&trial->sheets[0];
        if (kind==0) sheet->analysis.source_version=UINT64_MAX;
        if (kind==1) sheet->objects.items[0].source.source_version=UINT64_MAX;
        if (kind==2) sheet->source_versions.high_water=UINT64_MAX;
        if (kind==3 || kind==4) source_version_issue(sheet,0,1,kind==4);
        if (kind==5 || kind==6) {
            Analysis *history=kind==5 ? sheet->undo : sheet->redo; history[0]=analysis_copy(&sheet->analysis); history[0].source_version=1;
            if (kind==5) sheet->undo_count=1; else sheet->redo_count=1;
        }
        if (kind==7 || kind==8) {
            ObjectStore copy={0}; objects_copy(&copy,&sheet->objects); copy.items[0].source.source_version=1;
            ObjectSnapshot history={copy.items,copy.count}; copy.items=NULL; copy.count=0; objects_free(&copy);
            if (kind==7) { sheet->objects.undo[0]=history; sheet->objects.undo_count=1; } else { sheet->objects.redo[0]=history; sheet->objects.redo_count=1; }
        }
        if (kind>=2 && kind<=4) {
            /* A retained counter and pending/complete archival entries now have
               a v14 representation; they must not force a live binding. */
            CHECK(workbook_save(trial,fresh,&error)); require_workspace_version(fresh,14);
            CHECK(workbook_load(&loaded,fresh,&error));
            CHECK(loaded.sheets[0].source_versions.high_water==sheet->source_versions.high_water && loaded.sheets[0].source_versions.count==sheet->source_versions.count);
            CHECK(!loaded.sheets[0].analysis.source_version && !loaded.sheets[0].objects.items[0].source.source_version);
            if (kind==4) CHECK(loaded.sheets[0].source_versions.items[0].raw.bytes==sheet->source_versions.items[0].raw.bytes && !memcmp(loaded.sheets[0].source_versions.items[0].raw.data,sheet->source_versions.items[0].raw.data,sheet->source_versions.items[0].raw.bytes));
            remove(fresh); workbook_free(trial); free(trial); continue;
        }
        Workbook before=*trial;
        for (size_t i=0;i<3;i++) {
            CHECK(!workbook_save(trial,i<2 ? paths[i] : fresh,&error) && error && strstr(error,"v14")); free(error); error=NULL;
            CHECK(!memcmp(&before,trial,sizeof(before)));
        }
        CHECK(!workbook_save(trial,"tests/generated/no-source-version-parent/book.dxls",&error) && strstr(error,"v14")); free(error); error=NULL; /* Before target resolution. */
        FILE *file=fopen(fresh,"rb"); CHECK(!file);
        for (size_t i=0;i<2;i++) { size_t length; unsigned char *after=source_version_file(paths[i],&length); CHECK(length==lengths[i] && !memcmp(originals[i],after,length)); free(after); }
        workbook_free(trial); free(trial);
    }
    CHECK(!workbook_save(NULL,paths[0],&error)); free(error); error=NULL; CHECK(!workbook_save(NULL,NULL,NULL));
    for (size_t i=0;i<2;i++) {
        CHECK(workbook_load(&loaded,paths[i],&error) && !loaded.sheets[0].analysis.source_version && !loaded.sheets[0].objects.items[0].source.source_version);
        CHECK(!loaded.sheets[0].source_versions.count && !loaded.sheets[0].source_versions.high_water && !strcmp(cells_get(&loaded.sheets[0].cells,0,0),"Preserved original file")); free(originals[i]); remove(paths[i]);
    }
    workbook_free(&book); workbook_free(&loaded);
}
/* These are codec fixtures, not filesystem-verification or App activation
   fixtures. Their packet proofs deliberately use a fixed synthetic digest. */
static void source_format_issue(Worksheet *sheet,uint64_t owner,uint64_t version,const Analysis *source,bool complete) {
    Column columns[]={
        {.name="Id",.type_name="UBIGINT",.type=DUCKDB_TYPE_UBIGINT,.number_type={NUM_U64,0,0},.numeric=true},
        {.name="City",.type_name="VARCHAR",.type=DUCKDB_TYPE_VARCHAR},
        {.name="Amount",.type_name="DECIMAL(38,4)",.type=DUCKDB_TYPE_DECIMAL,.number_type={NUM_DECIMAL,38,4},.numeric=true}
    };
    Page schema={.columns=columns,.column_count=3}; SourceDigest digest={.bytes=3,.identity={.size=3}}; memset(digest.sha256,0x5a,sizeof(digest.sha256));
    SourcePacket raw={0}; char *error=NULL;
    if (complete) CHECK(source_scope_raw_build(owner,version,source,&schema,&digest,"v1.5.6",&raw,&error));
    SourceVersion entry={.owner=owner,.version=version,.kind=source->kind,.path=source->path,.raw=raw}; SourceVersions next={0};
    CHECK(source_versions_plan(&sheet->source_versions,&entry,true,&next,&error));
    source_versions_free(&sheet->source_versions); sheet->source_versions=next; source_packet_free(&raw); free(error);
}
static void source_format_fixture(Workbook *book) {
    const uint64_t base=UINT64_C(9007199254740992); char *error=NULL;
    FILE *file=fopen("tests/generated/source-format-proof.csv","wb"); CHECK(file && fwrite("Id,City,Amount\n1,NULL,1.0000\n",1,27,file)==27 && !fclose(file));
    Analysis source={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/generated/source-format-proof.csv")},blank={0}; CHECK(source.path);
    CHECK(analysis_filter_set(&source,"Amount",FILTER_GREATER,"9007199254740993.1250",&error)); analysis_filter_number(&source,"Amount",(NumberType){NUM_DECIMAL,38,4});
    CHECK(analysis_sort_set(&source,"City",true,false,&error));
    CHECK(workbook_add(book,"Source records",&source,&error)); Worksheet *sheet=book->sheets;
    sheet->source_versions.high_water=base; source_format_issue(sheet,0,base+1,&source,true); source_format_issue(sheet,0,base+2,&source,false); source_format_issue(sheet,0,base+3,&source,true);
    sheet->analysis.source_version=base+1;
    CellFormat value=format_default(); value.flags=FORMAT_BOLD;
    FormatScopeView raw={0,sheet->source_versions.items[0].raw.data,sheet->source_versions.items[0].raw.bytes};
    uint64_t ids[]={1,UINT64_C(9007199254740993),UINT64_MAX}; size_t fields[]={0,2};
    CHECK(format_apply_records(&sheet->formats,&raw,ids,3,fields,2,FORMAT_BOLD,value,&error));
    SourcePacket result={0}; CHECK(source_scope_result_build(&sheet->source_versions.items[0].raw,&sheet->analysis,&result,&error));
    FormatScopeView exact={0,result.data,result.bytes}; value.flags=FORMAT_ITALIC;
    CHECK(format_apply_scoped(&sheet->formats,(FormatRange){4,7,1,2},FORMAT_ITALIC,value,&exact,1,false,&error)); source_packet_free(&result);
    value.flags=FORMAT_UNDERLINE; CHECK(format_apply(&sheet->formats,(FormatRange){20,20,5,5},FORMAT_UNDERLINE,value,&error));
    CHECK(cells_set(&sheet->cells,20,5,"NULL",&error)); CHECK(cells_set(&sheet->cells,21,5,"12345678901234567890.0123456789",&error));
    CHECK(workbook_add(book,"Table owner",&blank,&error)); sheet=book->sheets+1; sheet->objects.next_id=base;
    Analysis table={.kind=SOURCE_PARQUET,.path=source.path}; CHECK(objects_add_table(&sheet->objects,0,8,"Bound source",&table,&error)==0);
    uint64_t owner=sheet->objects.items[0].id; sheet->source_versions.high_water=UINT64_MAX-1; source_format_issue(sheet,owner,UINT64_MAX,&table,true); sheet->objects.items[0].source.source_version=UINT64_MAX;
    raw=(FormatScopeView){owner,sheet->source_versions.items[0].raw.data,sheet->source_versions.items[0].raw.bytes}; uint64_t table_ids[]={2,5}; size_t table_field=1; value.flags=FORMAT_BOLD;
    CHECK(format_apply_records(&sheet->formats,&raw,table_ids,2,&table_field,1,FORMAT_BOLD,value,&error));
    CHECK(workbook_add(book,"Unbound",&blank,&error)); CHECK(workbook_add(book,"Retired versions",&blank,&error)); book->sheets[3].source_versions.high_water=UINT64_MAX; book->active=0;
    analysis_free(&source); free(error);
}
static void source_format_check(const Workbook *book,const Workbook *original) {
    const uint64_t base=UINT64_C(9007199254740992); CHECK(book->count==4 && book->active==0);
    const Worksheet *sheet=book->sheets; const SourceVersions *versions=&sheet->source_versions;
    CHECK(versions->high_water==base+3 && versions->count==3 && sheet->analysis.source_version==base+1);
    CHECK(versions->items[0].version==base+1 && versions->items[1].version==base+2 && versions->items[2].version==base+3 && !versions->items[1].raw.bytes);
    CHECK(!strcmp(versions->items[0].path,sheet->analysis.path) && versions->items[0].raw.data!=original->sheets[0].source_versions.items[0].raw.data);
    CHECK(!memcmp(versions->items[0].raw.data,original->sheets[0].source_versions.items[0].raw.data,versions->items[0].raw.bytes));
    CHECK(sheet->formats.source_aware && sheet->formats.scope_count==2 && sheet->formats.count==3 && sheet->formats.scopes!=original->sheets[0].formats.scopes);
    const FormatPatch *records=sheet->formats.items,*result=records+1;
    CHECK(records->selector==FORMAT_RECORDS && records->record_count==3 && records->field_count==2 && records->record_ids[0]==1 && records->record_ids[1]==base+1 && records->record_ids[2]==UINT64_MAX && records->fields[0]==0 && records->fields[1]==2);
    CHECK(result->selector==FORMAT_RESULT && result->range.r0==4 && result->range.r1==7 && result->guard_count==1);
    CHECK(!format_at(&sheet->formats,4,1).flags && !format_at(&sheet->formats,0,0).flags && format_at(&sheet->formats,20,5).flags==FORMAT_UNDERLINE); /* Dormant through legacy consumer. */
    CHECK(!strcmp(cells_get(&sheet->cells,20,5),"NULL") && !strcmp(cells_get(&sheet->cells,21,5),"12345678901234567890.0123456789"));
    CHECK(!sheet->analysis.filter_number.kind && !sheet->undo_count && !sheet->formats.undo_count); /* Never trust restored derived numeric bindings. */
    sheet=book->sheets+1; CHECK(sheet->objects.next_id==base+1 && sheet->objects.items[0].id==base+1 && sheet->objects.items[0].source.source_version==UINT64_MAX);
    CHECK(sheet->source_versions.high_water==UINT64_MAX && sheet->source_versions.items[0].owner==base+1 && sheet->source_versions.items[0].version==UINT64_MAX);
    CHECK(!strcmp(sheet->source_versions.items[0].path,sheet->objects.items[0].source.path) && sheet->formats.items[0].fields[0]==1);
    CHECK(!book->sheets[2].analysis.source_version && !book->sheets[2].source_versions.high_water && book->sheets[3].source_versions.high_water==UINT64_MAX && !book->sheets[3].source_versions.count && !book->sheets[3].formats.source_aware);
}
static void source_format_reject(const char *path,const Workbook *replacement,Workbook *caller,const char *mutation) {
    char *error=NULL; size_t before_bytes=0,after_bytes=0; unsigned char *before=workspace_bytes(path,&before_bytes);
    Workbook previous=*caller,replacement_before=*replacement; Worksheet sheet=caller->sheets[0],replacement_sheet=replacement->sheets[0];
    bool accepted=workbook_load(caller,path,&error); if (accepted || !error || !*error) fprintf(stderr,"Unexpected v14 load outcome for %s: %s\n",mutation,error ? error : "no error");
    CHECK(!accepted && error && *error); free(error); error=NULL;
    CHECK(!memcmp(&previous,caller,sizeof(previous)) && !memcmp(&sheet,caller->sheets,sizeof(sheet)) && !strcmp(cells_get(&caller->sheets[0].cells,30,0),"Keep unsaved source state"));
    CHECK(!workbook_save(replacement,path,&error) && error && *error); free(error); error=NULL;
    CHECK(!memcmp(&replacement_before,replacement,sizeof(replacement_before)) && !memcmp(&replacement_sheet,replacement->sheets,sizeof(replacement_sheet)));
    unsigned char *after=workspace_bytes(path,&after_bytes); CHECK(before_bytes==after_bytes && !memcmp(before,after,before_bytes)); free(before); free(after);
}
static void test_source_format_workspace(void) {
    Workbook book,loaded,plain; workbook_init(&book); workbook_init(&loaded); workbook_init(&plain); char *error=NULL; Analysis blank={0};
    const char *path="tests/generated/source-format-v14.dxls",*second="tests/generated/source-format-other.dxls"; remove(path); remove(second); source_format_fixture(&book);
    CHECK(workbook_save(&book,path,&error)); require_workspace_version(path,14); CHECK(workbook_load(&loaded,path,&error)); source_format_check(&loaded,&book);
    sql_file(path,"SELECT CASE WHEN count(*)=26 THEN true ELSE error('v14 package incomplete') END FROM information_schema.tables WHERE table_schema='_duxcel_workspace'; SELECT CASE WHEN count(*)=5 AND sum(CASE WHEN version=0 THEN 1 ELSE 0 END)=3 THEN true ELSE error('bindings are not complete') END FROM _duxcel_workspace.source_bindings");
    CHECK(workbook_save(&loaded,second,&error) && workbook_load(&loaded,second,&error)); source_format_check(&loaded,&book);
    const char *reordered="tests/generated/source-format-reordered.dxls"; remove(reordered); CHECK(workbook_save(&book,reordered,&error));
    sql_file(reordered,"CREATE TABLE _duxcel_workspace.reordered_sheets AS SELECT canvas_zoom,selection,fraction,horizontal,top_row,end_col,end_row,anchor_col,anchor_row,active_col,active_row,descending,sort_column,aggregate,measure_column,group_column,filter_op,filter_value,filter_column,source_table,source_schema,path,kind,name,id FROM _duxcel_workspace.sheets; DROP TABLE _duxcel_workspace.sheets; ALTER TABLE _duxcel_workspace.reordered_sheets RENAME TO sheets; CREATE TABLE _duxcel_workspace.reordered_versions AS SELECT definition,encoding,source_reference,source_kind,owner,version,sheet_id FROM _duxcel_workspace.source_versions; DROP TABLE _duxcel_workspace.source_versions; ALTER TABLE _duxcel_workspace.reordered_versions RENAME TO source_versions");
    CHECK(workbook_load(&loaded,reordered,&error)); source_format_check(&loaded,&book);
    CHECK(workbook_save(&book,reordered,&error) && workbook_load(&loaded,reordered,&error)); source_format_check(&loaded,&book);
    const char *nullable[]={NULL,"","NULL"}; SourcePacket previous={0}; remove("tests/generated/source-format-null-empty-literal.dxls");
    for (size_t i=0;i<3;i++) {
        Workbook *literal=workbook_snapshot(&book); CHECK(literal); Worksheet *sheet=literal->sheets; analysis_filters_clear(&sheet->analysis);
        CHECK(analysis_filter_set(&sheet->analysis,"City",FILTER_EQUAL,nullable[i],&error));
        SourcePacket result={0}; CHECK(source_scope_result_build(&sheet->source_versions.items[0].raw,&sheet->analysis,&result,&error));
        if (i) CHECK(result.bytes!=previous.bytes || memcmp(result.data,previous.data,result.bytes));
        format_free(&sheet->formats); FormatScopeView scope={0,result.data,result.bytes}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
        CHECK(format_apply_scoped(&sheet->formats,(FormatRange){0,1,1,1},FORMAT_BOLD,bold,&scope,1,false,&error));
        const char *literal_path="tests/generated/source-format-null-empty-literal.dxls";
        CHECK(workbook_save(literal,literal_path,&error) && workbook_load(&loaded,literal_path,&error));
        const char *value=loaded.sheets[0].analysis.filter_value;
        CHECK(nullable[i] ? value && !strcmp(value,nullable[i]) : value==NULL);
        CHECK(loaded.sheets[0].formats.scopes[0].bytes==result.bytes && !memcmp(loaded.sheets[0].formats.scopes[0].definition,result.data,result.bytes));
        source_packet_free(&previous); previous=result; workbook_free(literal); free(literal);
    }
    source_packet_free(&previous);
    /* A missing source stays an archived locator/proof: opening must not read it. */
    remove("tests/generated/source-format-proof.csv"); CHECK(workbook_load(&loaded,path,&error)); source_format_check(&loaded,&book);
    loaded.sheets[0].analysis.source_version=UINT64_C(9007199254740994); CHECK(workbook_save(&loaded,second,&error) && workbook_load(&loaded,second,&error));
    CHECK(loaded.sheets[0].analysis.source_version==UINT64_C(9007199254740994) && !loaded.sheets[0].source_versions.items[1].raw.bytes && loaded.sheets[0].formats.scope_count==2);
    CHECK(!format_at(&loaded.sheets[0].formats,4,1).flags); /* Current pending version cannot activate archived scopes. */
    CHECK(workbook_add(&plain,"Plain overwrite",&blank,&error)); CHECK(workbook_save(&plain,second,&error)); require_workspace_version(second,14); CHECK(workbook_load(&loaded,second,&error) && loaded.count==1 && !loaded.sheets[0].source_versions.count);
    /* A mode-only sheet retains its semantic boundary after all scopes/patches
       are gone. Neither a ledger counter nor active scope count can infer it. */
    plain.sheets[0].formats.source_aware=true; const char *mode_path="tests/generated/source-format-mode.dxls"; remove(mode_path);
    CHECK(workbook_save(&plain,mode_path,&error)); require_workspace_version(mode_path,14); CHECK(workbook_load(&loaded,mode_path,&error));
    CHECK(loaded.sheets[0].formats.source_aware && !loaded.sheets[0].formats.count && !loaded.sheets[0].formats.scope_count && !loaded.sheets[0].source_versions.high_water);
    CHECK(!format_persistable(&loaded.sheets[0].formats,&error) && error); free(error); error=NULL;
    plain.sheets[0].formats.source_aware=false; CHECK(workbook_save(&plain,mode_path,&error)); require_workspace_version(mode_path,14); CHECK(workbook_load(&loaded,mode_path,&error) && !loaded.sheets[0].formats.source_aware);
    /* Rebase into a different directory without requiring the source to exist. */
    CHECK(!mkdir("tests/generated/source-format-subdir",0700) || errno==EEXIST);
    const char *nested="tests/generated/source-format-subdir/rebased.dxls"; remove(nested); CHECK(workbook_save(&book,nested,&error)); CHECK(workbook_load(&loaded,nested,&error)); source_format_check(&loaded,&book);
    CHECK(strstr(loaded.sheets[0].analysis.path,"../") && !strcmp(loaded.sheets[0].analysis.path,loaded.sheets[0].source_versions.items[0].path));
    Workbook *retired=workbook_snapshot(&book); CHECK(retired && objects_remove(&retired->sheets[1].objects,0));
    remove(second); CHECK(workbook_save(retired,second,&error) && workbook_load(&loaded,second,&error));
    CHECK(!loaded.sheets[1].objects.count && loaded.sheets[1].objects.next_id==UINT64_C(9007199254740993) && loaded.sheets[1].source_versions.items[0].owner==UINT64_C(9007199254740993) && loaded.sheets[1].formats.items[0].selector==FORMAT_RECORDS);
    CHECK(!format_at(&loaded.sheets[1].formats,0,8).flags); workbook_free(retired); free(retired);
    /* Legacy unrelated metadata is still valid for legacy saves, but cannot be
       silently carried into v14's exact supported package. Fail before BEGIN. */
    const char *upgrade="tests/generated/source-format-upgrade.dxls";
    for (unsigned owner_only=0;owner_only<2;owner_only++) {
        remove(upgrade); plain.sheets[0].objects.next_id=owner_only; CHECK(workbook_save(&plain,upgrade,&error)); require_workspace_version(upgrade,12+(int)owner_only);
        sql_file(upgrade,"CREATE TABLE _duxcel_workspace.custom_note(value VARCHAR); INSERT INTO _duxcel_workspace.custom_note VALUES ('Preserve my legacy metadata')");
        CHECK(workbook_save(&plain,upgrade,&error)); size_t before_bytes,after_bytes; unsigned char *before=workspace_bytes(upgrade,&before_bytes);
        CHECK(!workbook_save(&book,upgrade,&error) && error && strstr(error,"known base")); free(error); error=NULL;
        unsigned char *after=workspace_bytes(upgrade,&after_bytes); CHECK(before_bytes==after_bytes && !memcmp(before,after,before_bytes)); free(before); free(after);
        CHECK(workbook_load(&loaded,upgrade,&error) && loaded.count==1 && !loaded.sheets[0].source_versions.count); require_workspace_version(upgrade,12+(int)owner_only);
        sql_file(upgrade,"SELECT CASE WHEN min(value)='Preserve my legacy metadata' THEN true ELSE error('Legacy metadata lost') END FROM _duxcel_workspace.custom_note");
    }
    workbook_free(&book); workbook_free(&loaded); workbook_free(&plain); free(error);
}
static void test_invalid_source_format_package(void) {
    Workbook book,caller,plain; workbook_init(&book); workbook_init(&caller); workbook_init(&plain); char *error=NULL; Analysis blank={0}; source_format_fixture(&book);
    CHECK(workbook_add(&plain,"Replacement",&blank,&error)); const char *path="tests/generated/invalid-source-format.dxls"; remove(path);
    CHECK(workbook_save(&book,path,&error) && workbook_load(&caller,path,&error)); CHECK(cells_set(&caller.sheets[0].cells,30,0,"Keep unsaved source state",&error)); worksheet_record(caller.sheets,EDIT_CELLS); caller.dirty=true;
    size_t pristine_bytes=0; unsigned char *pristine=workspace_bytes(path,&pristine_bytes);
    const char *invalid[]={
        "DELETE FROM _duxcel_workspace.format_state WHERE sheet_id=3",
        "UPDATE _duxcel_workspace.format_state SET source_aware=NULL WHERE sheet_id=0",
        "UPDATE _duxcel_workspace.format_state SET source_aware=false WHERE sheet_id=0",
        "UPDATE _duxcel_workspace.format_state SET sheet_id=4 WHERE sheet_id=3",
        "CREATE TABLE _duxcel_workspace.duplicate_state AS SELECT * FROM _duxcel_workspace.format_state; INSERT INTO _duxcel_workspace.duplicate_state SELECT * FROM _duxcel_workspace.format_state LIMIT 1; DROP TABLE _duxcel_workspace.format_state; ALTER TABLE _duxcel_workspace.duplicate_state RENAME TO format_state",
        "UPDATE _duxcel_workspace.meta SET active_sheet=4",
        "UPDATE _duxcel_workspace.meta SET user_scale=2",
        "UPDATE _duxcel_workspace.sheets SET active_col=16384 WHERE id=0",
        "UPDATE _duxcel_workspace.sheets SET canvas_zoom=3 WHERE id=0",
        "UPDATE _duxcel_workspace.sheets SET horizontal=-1 WHERE id=0",
        "UPDATE _duxcel_workspace.sheets SET fraction=1 WHERE id=0",
        "INSERT INTO _duxcel_workspace.columns VALUES(0,0,'Id',0)",
        "UPDATE _duxcel_workspace.cells SET row_id=18446744073709551615 WHERE sheet_id=0 AND row_id=20",
        "INSERT INTO _duxcel_workspace.cells VALUES(1,0,8,'Cannot overlap table')",
        "INSERT INTO _duxcel_workspace.preferences VALUES('sheet_hidden_0',true)",
        "UPDATE _duxcel_workspace.meta SET active_sheet=NULL",
        "UPDATE _duxcel_workspace.sheets SET active_row=NULL WHERE id=0",
        "UPDATE _duxcel_workspace.sheets SET filter_op=NULL WHERE id=0",
        "UPDATE _duxcel_workspace.formats SET flags=NULL WHERE sheet_id=0 AND position=2",
        "UPDATE _duxcel_workspace.table_sources SET filter_op=NULL WHERE sheet_id=1",
        "UPDATE _duxcel_workspace.format_selectors SET kind=NULL WHERE sheet_id=0 AND position=0",
        "UPDATE _duxcel_workspace.sheets SET name='prefix'||chr(0)||'suffix' WHERE id=0",
        "UPDATE _duxcel_workspace.sheets SET filter_value='9'||chr(0)||'99' WHERE id=0",
        "UPDATE _duxcel_workspace.table_sources SET filter_value='NULL'||chr(0)||'suffix' WHERE sheet_id=1",
        "INSERT INTO _duxcel_workspace.filters VALUES(0,0,'City'||chr(0)||'suffix',1,'NULL')",
        "UPDATE _duxcel_workspace.cells SET text='NULL'||chr(0)||'suffix' WHERE sheet_id=0 AND row_id=20",
        "UPDATE _duxcel_workspace.format_scopes SET encoding=2 WHERE sheet_id=0 AND scope_id=0",
        "UPDATE _duxcel_workspace.source_versions SET encoding=2 WHERE sheet_id=0 AND version=9007199254740993",
        "UPDATE _duxcel_workspace.source_versions SET encoding=1 WHERE sheet_id=0 AND version=9007199254740994",
        "UPDATE _duxcel_workspace.source_versions SET encoding=0 WHERE sheet_id=0 AND version=9007199254740993",
        "UPDATE _duxcel_workspace.source_version_counters SET high_water=0 WHERE sheet_id=0",
        "DELETE FROM _duxcel_workspace.source_version_counters WHERE sheet_id=3",
        "UPDATE _duxcel_workspace.source_versions SET version=0 WHERE sheet_id=0 AND version=9007199254740994",
        "UPDATE _duxcel_workspace.source_versions SET owner=1 WHERE sheet_id=0 AND version=9007199254740994",
        "UPDATE _duxcel_workspace.source_versions SET source_reference='elsewhere.csv' WHERE sheet_id=0 AND version=9007199254740993",
        "UPDATE _duxcel_workspace.source_versions SET source_kind=2 WHERE sheet_id=0 AND version=9007199254740993",
        "UPDATE _duxcel_workspace.source_versions SET definition=definition||from_hex('00') WHERE sheet_id=0 AND version=9007199254740993",
        "UPDATE _duxcel_workspace.format_scopes SET definition=definition||from_hex('00') WHERE sheet_id=0 AND scope_id=0",
        "UPDATE _duxcel_workspace.format_scopes SET owner=1 WHERE sheet_id=0 AND scope_id=0",
        "UPDATE _duxcel_workspace.format_scopes SET definition=from_hex(substr(hex(definition),1,96)||'00'||substr(hex(definition),99)) WHERE sheet_id=0 AND scope_id=0",
        "INSERT INTO _duxcel_workspace.format_scopes SELECT 0,2,0,1,definition FROM _duxcel_workspace.source_versions WHERE sheet_id=0 AND version=9007199254740995",
        "UPDATE _duxcel_workspace.format_selectors SET kind=2 WHERE sheet_id=0 AND position=0",
        "UPDATE _duxcel_workspace.format_selectors SET kind=1 WHERE sheet_id=0 AND position=0",
        "DELETE FROM _duxcel_workspace.source_bindings WHERE sheet_id=2",
        "UPDATE _duxcel_workspace.source_bindings SET owner=1 WHERE sheet_id=2",
        "UPDATE _duxcel_workspace.source_bindings SET version=1 WHERE sheet_id=0",
        "INSERT INTO _duxcel_workspace.format_guards VALUES(0,2,0,0)",
        "UPDATE _duxcel_workspace.format_guards SET guard_position=1 WHERE sheet_id=0 AND position=0",
        "INSERT INTO _duxcel_workspace.format_guards VALUES(0,0,1,0)",
        "DELETE FROM _duxcel_workspace.format_records WHERE sheet_id=0 AND position=0; INSERT INTO _duxcel_workspace.format_records SELECT 0,0,i::INTEGER,(i+1)::UBIGINT FROM range(20001)t(i)",
        "INSERT INTO _duxcel_workspace.source_versions SELECT 0,(i+1)::UBIGINT,0,1,'missing.csv',0,NULL FROM range(1024)t(i)",
        "UPDATE _duxcel_workspace.format_records SET member_position=7 WHERE sheet_id=0 AND member_position=1",
        "UPDATE _duxcel_workspace.format_records SET record_id=0 WHERE sheet_id=0 AND member_position=0",
        "UPDATE _duxcel_workspace.format_records SET record_id=1 WHERE sheet_id=0 AND member_position=1",
        "UPDATE _duxcel_workspace.format_records SET position=2 WHERE sheet_id=0",
        "UPDATE _duxcel_workspace.format_fields SET source_ordinal=3 WHERE sheet_id=0 AND field_position=1",
        "UPDATE _duxcel_workspace.format_fields SET field_position=2 WHERE sheet_id=0 AND field_position=1",
        "UPDATE _duxcel_workspace.format_fields SET source_ordinal=0 WHERE sheet_id=0 AND field_position=1",
        "ALTER TABLE _duxcel_workspace.source_versions ADD COLUMN unknown INTEGER",
        "CREATE TABLE _duxcel_workspace.typed_versions AS SELECT sheet_id,version::VARCHAR AS version,owner,source_kind,source_reference,encoding,definition FROM _duxcel_workspace.source_versions; DROP TABLE _duxcel_workspace.source_versions; ALTER TABLE _duxcel_workspace.typed_versions RENAME TO source_versions",
        "DROP TABLE _duxcel_workspace.source_bindings; CREATE VIEW _duxcel_workspace.source_bindings AS SELECT 0::INTEGER AS sheet_id,0::UBIGINT AS owner,0::UBIGINT AS version",
        "CREATE TABLE _duxcel_workspace.unknown_feature(value INTEGER)",
        "UPDATE _duxcel_workspace.format_scopes SET definition=encode(repeat('x',262144)) WHERE sheet_id=0 AND scope_id=0",
        "UPDATE _duxcel_workspace.source_versions SET source_reference=repeat('x',65537) WHERE sheet_id=0 AND version=9007199254740994",
        "UPDATE _duxcel_workspace.meta SET version=13",
        "UPDATE _duxcel_workspace.meta SET version=12",
        "CREATE TABLE _duxcel_workspace.duplicate_bindings AS SELECT * FROM _duxcel_workspace.source_bindings; INSERT INTO _duxcel_workspace.duplicate_bindings SELECT * FROM _duxcel_workspace.source_bindings LIMIT 1; DROP TABLE _duxcel_workspace.source_bindings; ALTER TABLE _duxcel_workspace.duplicate_bindings RENAME TO source_bindings",
        "CREATE TABLE _duxcel_workspace.duplicate_versions AS SELECT * FROM _duxcel_workspace.source_versions; INSERT INTO _duxcel_workspace.duplicate_versions SELECT * FROM _duxcel_workspace.source_versions LIMIT 1; DROP TABLE _duxcel_workspace.source_versions; ALTER TABLE _duxcel_workspace.duplicate_versions RENAME TO source_versions"
    };
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        remove(path); FILE *file=fopen(path,"wb"); CHECK(file && fwrite(pristine,1,pristine_bytes,file)==pristine_bytes && !fclose(file));
        sql_file(path,invalid[i]); source_format_reject(path,&plain,&caller,invalid[i]);
    }
    /* A syntactically valid RAW proof is not a matching RESULT guard. */
    remove(path); CHECK(workbook_save(&book,path,&error)); sql_file(path,"UPDATE _duxcel_workspace.format_scopes SET definition=(SELECT definition FROM _duxcel_workspace.source_versions WHERE sheet_id=0 AND version=9007199254740993) WHERE sheet_id=0 AND scope_id=1"); source_format_reject(path,&plain,&caller,"RAW packet substituted for RESULT");
    for (unsigned owner_only=0;owner_only<2;owner_only++) {
        remove(path); plain.sheets[0].objects.next_id=owner_only; CHECK(workbook_save(&plain,path,&error));
        sql_file(path,"CREATE TABLE _duxcel_workspace.format_state(sheet_id INTEGER,source_aware BOOLEAN); INSERT INTO _duxcel_workspace.format_state VALUES(0,false)");
        source_format_reject(path,&plain,&caller,"Reserved v14 format_state under a legacy workspace version");
    }
    free(pristine); workbook_free(&book); workbook_free(&caller); workbook_free(&plain); free(error);
}
static void source_format_full_ledger(Worksheet *sheet) {
    SourceVersions *versions=&sheet->source_versions; size_t count=DX_SOURCE_VERSIONS,path_bytes=DX_SOURCE_VERSIONS_BYTES/count-sizeof(SourceVersion);
    CHECK(path_bytes>2 && path_bytes<=DX_SOURCE_SCOPE_TEXT_BYTES); char *path=dx_alloc(path_bytes); memset(path,'x',path_bytes-1); path[0]='/'; path[path_bytes-1]=0;
    *versions=(SourceVersions){.high_water=count,.items=dx_alloc(count*sizeof(SourceVersion)),.count=count,.capacity=count,.bytes=count*(sizeof(SourceVersion)+path_bytes)};
    for (size_t i=0;i<count;i++) versions->items[i]=(SourceVersion){.owner=0,.version=i+1,.kind=SOURCE_CSV,.path=dx_strdup(path)};
    free(path); char *error=NULL; CHECK(source_versions_valid(versions,0,&error)); free(error);
}
/* Pure codec proofs use synthetic digests, not verified source activation. */
static void guarded_format_fixture(Workbook *book,size_t owners) {
    CHECK(owners && owners<=DX_FORMAT_GUARDS); char *error=NULL; const char *file_path="tests/generated/guarded-format-proof.csv",*data="Id,City,Amount\n1,NULL,1.2500\n";
    FILE *file=fopen(file_path,"wb"); CHECK(file && fwrite(data,1,strlen(data),file)==strlen(data) && !fclose(file));
    Analysis source={.kind=SOURCE_CSV,.path=dx_absolute_path(file_path)}; CHECK(source.path && workbook_add(book,"Typed guarded legacy rectangles",&source,&error));
    Worksheet *sheet=book->sheets; sheet->objects.next_id=UINT64_C(9007199254740992);
    for (size_t i=1;i<owners;i++) { char name[48]; snprintf(name,sizeof(name),"Guarded owner %zu",i); CHECK(objects_add_table(&sheet->objects,i*6,8,name,&source,&error)==(int)i-1); }
    SourcePacket results[DX_FORMAT_GUARDS]={0}; FormatScopeView views[DX_FORMAT_GUARDS]={0};
    const char *values[]={NULL,"","NULL"};
    for (size_t i=0;i<owners;i++) {
        Analysis *analysis=i ? &sheet->objects.items[i-1].source : &sheet->analysis; uint64_t owner=i ? sheet->objects.items[i-1].id : 0;
        CHECK(analysis_filter_set(analysis,"City",FILTER_EQUAL,values[i%3],&error)); source_format_issue(sheet,owner,i+1,analysis,true); analysis->source_version=i+1;
        CHECK(source_scope_result_build(&sheet->source_versions.items[i].raw,analysis,results+i,&error)); views[owners-i-1]=(FormatScopeView){owner,results[i].data,results[i].bytes};
    }
    FormatStore *formats=&sheet->formats; CellFormat value=format_default(); value.flags=FORMAT_ITALIC;
    CHECK(format_apply(formats,(FormatRange){0,DX_SHEET_ROWS-1,6,6},FORMAT_ITALIC,value,&error)); value.flags=FORMAT_BOLD;
    CHECK(format_apply(formats,(FormatRange){20,21,5,5},FORMAT_BOLD,value,&error)); value.flags=FORMAT_UNDERLINE;
    FormatScopeView primary={0,results[0].data,results[0].bytes}; CHECK(format_apply_scoped(formats,(FormatRange){22,22,5,5},FORMAT_UNDERLINE,value,&primary,1,false,&error));
    FormatScopeView raw={0,sheet->source_versions.items[0].raw.data,sheet->source_versions.items[0].raw.bytes}; uint64_t ids[]={1,UINT64_MAX}; size_t field=2;
    CHECK(format_apply_records(formats,&raw,ids,2,&field,1,FORMAT_UNDERLINE,value,&error));
    Analysis alternate=analysis_copy(&sheet->analysis); CHECK(analysis_sort_set(&alternate,"Id",true,false,&error)); SourcePacket exact={0};
    CHECK(source_scope_result_build(&sheet->source_versions.items[0].raw,&alternate,&exact,&error)); FormatScopeView changed={0,exact.data,exact.bytes};
    CHECK(format_apply_scoped(formats,(FormatRange){24,24,5,5},FORMAT_UNDERLINE,value,&changed,1,false,&error)); source_packet_free(&exact); analysis_free(&alternate);
    /* Existing IDs are reused in reverse input-owner order. The current shared
       planner sorts its stored vectors, and the codec preserves that order. */
    FormatSnapshot plan={0}; CHECK(format_plan_guard_legacy(formats,views,owners,&plan,&error) && format_commit(formats,&plan));
    CHECK(formats->count==5 && formats->items[1].selector==FORMAT_GUARDED && formats->items[1].guard_count==owners && format_valid(formats,&error));
    CHECK(cells_set(&sheet->cells,20,5,"NULL",&error) && cells_set(&sheet->cells,21,5,"12345678901234567890.0123456789",&error));
    for (size_t i=0;i<owners;i++) source_packet_free(results+i); analysis_free(&source); free(error);
}
static void guarded_format_check(const Workbook *book,const Workbook *original,size_t owners) {
    CHECK(book->count==1 && book->sheets[0].objects.count==owners-1); const Worksheet *sheet=book->sheets,*prior=original->sheets;
    const FormatStore *formats=&sheet->formats,*old=&prior->formats; CHECK(formats->source_aware && formats->count==5 && formats->scope_count==owners+2 && !formats->undo_count && !formats->redo_count);
    CHECK(formats->items[1].selector==FORMAT_GUARDED && formats->items[1].guard_count==owners && formats->items[1].guard_ids!=old->items[1].guard_ids);
    CHECK(!memcmp(formats->items[1].guard_ids,old->items[1].guard_ids,owners*sizeof(size_t)) && !memcmp(&formats->items[1].range,&old->items[1].range,sizeof(FormatRange)));
    CHECK(format_at(formats,20,5).flags==FORMAT_BOLD && format_at(formats,21,5).flags==FORMAT_BOLD && format_at(formats,20,6).flags==FORMAT_ITALIC);
    CHECK(!format_at(formats,22,5).flags && !format_resolve_token(formats,20,5,SIZE_MAX).flags); /* Imported tokens remain a separate activation gate. */
    CHECK(sheet->source_versions.count==owners && sheet->source_versions.high_water==owners);
    for (size_t i=0;i<formats->scope_count;i++) {
        CHECK(formats->scopes[i].owner==old->scopes[i].owner && formats->scopes[i].bytes==old->scopes[i].bytes && formats->scopes[i].definition!=old->scopes[i].definition);
        CHECK(!memcmp(formats->scopes[i].definition,old->scopes[i].definition,formats->scopes[i].bytes));
    }
    for (size_t i=0;i<owners;i++) {
        size_t guard=formats->items[1].guard_ids[i]; SourceScopeInfo info={0}; CHECK(source_scope_validate(formats->scopes[guard].definition,formats->scopes[guard].bytes,&info,NULL) && info.kind==SOURCE_SCOPE_RESULT);
        CHECK(format_resolve_token(formats,20,5,guard+1).flags==FORMAT_BOLD); /* Explicit pure-model context, never automatic verification. */
        CHECK(sheet->source_versions.items[i].owner==prior->source_versions.items[i].owner && sheet->source_versions.items[i].version==i+1);
    }
    CHECK(!strcmp(cells_get(&sheet->cells,20,5),"NULL") && !strcmp(cells_get(&sheet->cells,21,5),"12345678901234567890.0123456789"));
}
static void test_guarded_format_workspace(void) {
    Workbook book,loaded,plain; workbook_init(&book); workbook_init(&loaded); workbook_init(&plain); Analysis blank={0}; char *error=NULL;
    const char *path="tests/generated/guarded-format-v14.dxls",*second="tests/generated/guarded-format-copy.dxls"; remove(path); remove(second); guarded_format_fixture(&book,3);
    CHECK(book.sheets[0].formats.items[1].guard_ids[0]==0 && book.sheets[0].formats.items[1].guard_ids[1]==3 && book.sheets[0].formats.items[1].guard_ids[2]==4);
    CHECK(workbook_save(&book,path,&error)); require_workspace_version(path,14); CHECK(workbook_load(&loaded,path,&error)); guarded_format_check(&loaded,&book,3);
    sql_file(path,"SELECT CASE WHEN count(*)=26 THEN true ELSE error('Guarded v14 schema changed') END FROM information_schema.tables WHERE table_schema='_duxcel_workspace'; SELECT CASE WHEN count(*)=3 AND min(guard_position)=0 AND max(guard_position)=2 THEN true ELSE error('Guard vector lost') END FROM _duxcel_workspace.format_guards WHERE position=1");
    remove("tests/generated/guarded-format-proof.csv"); CHECK(workbook_load(&loaded,path,&error)); guarded_format_check(&loaded,&book,3);
    CHECK(workbook_save(&loaded,second,&error) && workbook_load(&loaded,second,&error)); guarded_format_check(&loaded,&book,3);
    /* Current bindings may be unbound/pending/different without erasing valid
       archived guards; complete ledger proof association is what is persisted. */
    loaded.sheets[0].analysis.source_version=0; loaded.sheets[0].objects.items[0].source.source_version=0;
    CHECK(objects_remove(&loaded.sheets[0].objects,1)); CHECK(workbook_save(&loaded,second,&error) && workbook_load(&loaded,second,&error));
    CHECK(!loaded.sheets[0].analysis.source_version && loaded.sheets[0].objects.count==1 && loaded.sheets[0].formats.items[1].guard_count==3 && loaded.sheets[0].source_versions.count==3);
    CHECK(workbook_add(&plain,"Plain v14 overwrite",&blank,&error) && workbook_save(&plain,second,&error)); require_workspace_version(second,14);
    CHECK(workbook_load(&loaded,second,&error) && !loaded.sheets[0].formats.count && !loaded.sheets[0].source_versions.count);
    workbook_free(&book); guarded_format_fixture(&book,DX_FORMAT_GUARDS); remove(path);
    CHECK(workbook_save(&book,path,&error) && workbook_load(&loaded,path,&error)); guarded_format_check(&loaded,&book,DX_FORMAT_GUARDS);
    CHECK(loaded.sheets[0].objects.next_id==UINT64_C(9007199254741024));
    workbook_free(&book); workbook_free(&loaded); workbook_free(&plain); free(error);
}
static void test_guarded_format_rejection(void) {
    Workbook book,caller,plain; workbook_init(&book); workbook_init(&caller); workbook_init(&plain); Analysis blank={0}; char *error=NULL; guarded_format_fixture(&book,3);
    const char *path="tests/generated/invalid-guarded-format.dxls"; remove(path); CHECK(workbook_save(&book,path,&error) && workbook_load(&caller,path,&error));
    CHECK(cells_set(&caller.sheets[0].cells,30,0,"Keep unsaved source state",&error)); worksheet_record(caller.sheets,EDIT_CELLS); caller.dirty=true;
    CHECK(workbook_add(&plain,"Valid replacement",&blank,&error)); size_t pristine_bytes=0; unsigned char *pristine=workspace_bytes(path,&pristine_bytes);
    const char *invalid[]={
        "DELETE FROM _duxcel_workspace.format_guards WHERE position=1",
        "UPDATE _duxcel_workspace.format_guards SET guard_position=3 WHERE position=1 AND guard_position=2",
        "UPDATE _duxcel_workspace.format_guards SET scope_id=0 WHERE position=1 AND guard_position=1",
        "UPDATE _duxcel_workspace.format_guards SET scope_id=2 WHERE position=1 AND guard_position=1", /* Different RESULT, same owner. */
        "UPDATE _duxcel_workspace.format_guards SET scope_id=1 WHERE position=1 AND guard_position=1", /* Valid RAW is not a RESULT. */
        "UPDATE _duxcel_workspace.format_guards SET scope_id=CASE guard_position WHEN 1 THEN 4 WHEN 2 THEN 3 ELSE scope_id END WHERE position=1", /* Existing model requires sorted IDs. */
        "UPDATE _duxcel_workspace.format_guards SET scope_id=512 WHERE position=1 AND guard_position=1",
        "INSERT INTO _duxcel_workspace.format_guards VALUES(0,0,0,0)",
        "INSERT INTO _duxcel_workspace.format_guards VALUES(0,2,1,3)", /* RESULT remains exactly one. */
        "INSERT INTO _duxcel_workspace.format_guards VALUES(0,3,1,3)", /* RECORDS remains exactly one. */
        "UPDATE _duxcel_workspace.format_scopes SET definition=from_hex('00') WHERE scope_id=3",
        "UPDATE _duxcel_workspace.format_scopes SET owner=0 WHERE scope_id=3",
        "UPDATE _duxcel_workspace.source_versions SET encoding=0,definition=NULL WHERE version=2",
        "UPDATE _duxcel_workspace.source_bindings SET version=0 WHERE version=2; DELETE FROM _duxcel_workspace.source_versions WHERE version=2",
        "UPDATE _duxcel_workspace.source_versions SET definition=from_hex(substr(hex(definition),1,96)||'5B'||substr(hex(definition),99)) WHERE version=2", /* Different complete typed raw proof. */
        "DELETE FROM _duxcel_workspace.format_guards WHERE position=1; INSERT INTO _duxcel_workspace.format_guards SELECT 0,1,i::INTEGER,(i%5)::INTEGER FROM range(34)t(i)",
        "CREATE TABLE _duxcel_workspace.duplicate_guards AS SELECT * FROM _duxcel_workspace.format_guards; INSERT INTO _duxcel_workspace.duplicate_guards SELECT * FROM _duxcel_workspace.format_guards WHERE position=1 AND guard_position=0; DROP TABLE _duxcel_workspace.format_guards; ALTER TABLE _duxcel_workspace.duplicate_guards RENAME TO format_guards"
    };
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        remove(path); FILE *file=fopen(path,"wb"); CHECK(file && fwrite(pristine,1,pristine_bytes,file)==pristine_bytes && !fclose(file)); sql_file(path,invalid[i]); source_format_reject(path,&plain,&caller,invalid[i]);
    }
    /* Invalid live guarded models must fail before touching even a legacy
       destination, not merely rely on rolling back a partially written v14. */
    remove(path); CHECK(workbook_save(&plain,path,&error)); size_t before_bytes=0,after_bytes=0; unsigned char *before=workspace_bytes(path,&before_bytes);
    for (size_t i=0;i<5;i++) {
        Workbook *trial=workbook_snapshot(&book); CHECK(trial); Worksheet *sheet=trial->sheets; FormatPatch *patch=sheet->formats.items+1;
        if (i==0) { FormatScope *scope=sheet->formats.scopes+3; free(scope->definition); scope->definition=dx_strdup("opaque"); scope->bytes=6; }
        if (i==1) { SourceVersion *entry=sheet->source_versions.items+1; sheet->source_versions.bytes-=entry->raw.bytes; source_packet_free(&entry->raw); }
        if (i==2) { source_versions_free(&sheet->source_versions); sheet->analysis.source_version=0; for (size_t j=0;j<sheet->objects.count;j++) sheet->objects.items[j].source.source_version=0; }
        if (i==3) patch->guard_ids[1]=2;
        if (i==4) { free(patch->guard_ids); patch->guard_ids=NULL; patch->guard_count=0; }
        Workbook state=*trial; Worksheet prior=*sheet; CHECK(!workbook_save(trial,path,&error) && error && *error); free(error); error=NULL;
        CHECK(!memcmp(trial,&state,sizeof(state)) && !memcmp(sheet,&prior,sizeof(prior))); unsigned char *after=workspace_bytes(path,&after_bytes);
        CHECK(before_bytes==after_bytes && !memcmp(before,after,before_bytes)); free(after); workbook_free(trial); free(trial);
    }
    free(before); free(pristine); workbook_free(&book); workbook_free(&caller); workbook_free(&plain); free(error);
}
static void test_source_format_budget(void) {
    Workbook book,plain,loaded; workbook_init(&book); workbook_init(&plain); workbook_init(&loaded); Analysis blank={0}; char *error=NULL;
    const char *path="tests/generated/source-format-budget.dxls"; remove(path); CHECK(workbook_add(&plain,"Keep ordinary file",&blank,&error)); CHECK(workbook_save(&plain,path,&error));
    CHECK(workbook_add(&book,"Combined current budget",&blank,&error)); source_format_full_ledger(book.sheets); CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&book.sheets[0].formats,(FormatRange){0,0,0,0},FORMAT_BOLD,bold,&error));
    size_t before_bytes,after_bytes; unsigned char *before=workspace_bytes(path,&before_bytes); CHECK(!workbook_save(&book,path,&error) && error && strstr(error,"owned")); free(error); error=NULL;
    unsigned char *after=workspace_bytes(path,&after_bytes); CHECK(before_bytes==after_bytes && !memcmp(before,after,before_bytes)); free(after);
    /* Each sheet is individually valid, but v14's new aggregate must be checked
       before mutating a previously valid v12 destination. */
    format_free(&book.sheets[0].formats);
    CHECK(!workbook_save(&book,path,&error) && error && strstr(error,"restored")); free(error); error=NULL; /* Missing-path Save As expansion cannot write an unloadable archive. */
    after=workspace_bytes(path,&after_bytes); CHECK(before_bytes==after_bytes && !memcmp(before,after,before_bytes)); free(after);
    for (size_t i=1;i<9;i++) { CHECK(workbook_add(&book,"Global budget",&blank,&error)); source_format_full_ledger(book.sheets+i); }
    CHECK(!workbook_save(&book,path,&error) && error && strstr(error,"8 MiB")); free(error); error=NULL;
    after=workspace_bytes(path,&after_bytes); CHECK(before_bytes==after_bytes && !memcmp(before,after,before_bytes)); free(before); free(after);
    CHECK(workbook_load(&loaded,path,&error) && loaded.count==1 && !loaded.sheets[0].source_versions.count); require_workspace_version(path,12);
    workbook_free(&book); workbook_free(&plain); workbook_free(&loaded); free(error);
}
static void journal_counts(const Worksheet *s) {
    const unsigned expected[]={1,2,4,6,8,10,12,14,9};
    for (size_t i=0;i<sizeof(expected)/sizeof(*expected);i++) CHECK(worksheet_edit_effects((EditKind)i)==expected[i]);
    CHECK(!worksheet_edit_effects((EditKind)9) && !worksheet_edit_effects((EditKind)-1));
    for (size_t h=0;h<2;h++) {
        const EditKind *actions=h ? s->redo_actions : s->undo_actions; size_t count=h ? s->redo_actions_count : s->undo_actions_count,n[4]={0}; CHECK(count<=DX_HISTORY);
        for (size_t i=0;i<count;i++) { unsigned effects=worksheet_edit_effects(actions[i]); CHECK(effects); for (size_t k=0;k<4;k++) n[k]+=!!(effects&(1u<<k)); }
        CHECK(n[0]==(h ? s->redo_count : s->undo_count)); CHECK(n[1]==(h ? s->cells.redo_count : s->cells.undo_count));
        CHECK(n[2]==(h ? s->objects.redo_count : s->objects.undo_count)); CHECK(n[3]==(h ? s->formats.redo_count : s->formats.undo_count));
    }
}
static void journal_history(Worksheet *s,Analysis *live,bool redo) {
    char *error=NULL; size_t n=redo ? s->redo_actions_count : s->undo_actions_count; CHECK(n); unsigned effects=worksheet_edit_effects((redo ? s->redo_actions : s->undo_actions)[n-1]);
    if (effects&1) { CHECK(worksheet_analysis_history_preflight(s,live,redo,&error)); worksheet_analysis_history_adopt(s,live,redo); }
    if (effects&2) CHECK(cells_history(&s->cells,redo)); if (effects&4) CHECK(objects_history(&s->objects,redo)); if (effects&8) CHECK(format_history(&s->formats,redo));
    worksheet_transfer_history(s,redo); journal_counts(s); free(error);
}
static void journal_analysis(Worksheet *s,Analysis *live,Analysis *next) {
    char *error=NULL; bool changed=false; CHECK(worksheet_analysis_preflight(s,live,next,&changed,&error) && changed);
    worksheet_analysis_adopt(s,live,next); worksheet_record(s,EDIT_ANALYSIS); journal_counts(s); free(error);
}
static void journal_composite(Worksheet *s,Analysis *live,Analysis *next) {
    char *error=NULL; bool changed=false; FormatSnapshot plan={0}; CHECK(format_plan_move(&s->formats,(FormatRange){0,0,0,0},0,0,&plan,&error)); plan.source_aware=!s->formats.source_aware;
    CHECK(worksheet_analysis_preflight(s,live,next,&changed,&error) && changed);
    worksheet_analysis_adopt(s,live,next); CHECK(format_commit(&s->formats,&plan)); worksheet_record(s,EDIT_ANALYSIS_FORMAT); journal_counts(s); free(error);
}
static void journal_reject(Worksheet *s,Analysis *live,Analysis *next) {
    Worksheet before=*s; Analysis prior=*live; char *error=NULL; bool changed=true;
    CHECK(!worksheet_analysis_preflight(s,live,next,&changed,&error) && changed && error && *error);
    CHECK(!memcmp(s,&before,sizeof(before)) && !memcmp(live,&prior,sizeof(prior))); free(error);
}
static void test_analysis_journal_owned(void) {
    Workbook book; workbook_init(&book); char *error=NULL; Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("/private/journal-source.csv")};
    CHECK(analysis_filter_set(&source,"Amount",FILTER_GREATER,"1.2500",&error)); analysis_filter_number(&source,"Amount",(NumberType){NUM_DECIMAL,38,4});
    CHECK(analysis_filter_set(&source,"City",FILTER_EQUAL,"NULL",&error)); CHECK(analysis_sort_set(&source,"City",false,false,&error)); CHECK(analysis_sort_set(&source,"Id",true,true,&error));
    CHECK(workbook_add(&book,"Owned source journal",&source,&error)); analysis_free(&source); Worksheet *s=book.sheets; Analysis live=analysis_copy(&s->analysis),next=analysis_copy(&live); char *cache=s->analysis.path;
    source_format_issue(s,0,1,&live,true); FormatScopeView raw={0,s->source_versions.items[0].raw.data,s->source_versions.items[0].raw.bytes}; uint64_t ids[]={1,7}; size_t field=2;
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; FormatSnapshot plan={0}; CHECK(format_plan_records(&s->formats,&raw,ids,2,&field,1,FORMAT_BOLD,bold,&plan,&error));
    bool changed=true; Worksheet before=*s; Analysis current=live,prepared=next;
    CHECK(worksheet_analysis_preflight(s,&live,&next,&changed,&error) && !changed);
    CHECK(!memcmp(s,&before,sizeof(before)) && !memcmp(&live,&current,sizeof(live)) && !memcmp(&next,&prepared,sizeof(next))); analysis_free(&next);
    /* Canceling independent plans publishes nothing and retains the separately
       issued high-water. The helpers own neither reservation nor dirty state. */
    next=analysis_copy(&live); next.source_version=1; CHECK(worksheet_analysis_preflight(s,&live,&next,&changed,&error) && changed && analysis_same_result(&live,&next));
    CHECK(!memcmp(s,&before,sizeof(before))); analysis_free(&next); format_snapshot_free(&plan); CHECK(s->source_versions.high_water==1 && !s->undo_count && !s->formats.source_aware);
    next=analysis_copy(&live); next.source_version=1; CHECK(format_plan_records(&s->formats,&raw,ids,2,&field,1,FORMAT_BOLD,bold,&plan,&error));
    char *old_path=live.path,*new_path=next.path,*old_filter=live.filter_value,*new_filter=next.filter_value,*old_secondary=live.filters[0].value,*new_secondary=next.filters[0].value;
    CHECK(worksheet_analysis_preflight(s,&live,&next,&changed,&error) && changed);
    /* A separately prepared ledger may be adopted after preflight. It cannot
       invalidate the independently owned Analysis/format plans. */
    source_format_issue(s,0,2,&live,true); SourceVersion *ledger=s->source_versions.items;
    worksheet_analysis_adopt(s,&live,&next); CHECK(format_commit(&s->formats,&plan)); worksheet_record(s,EDIT_ANALYSIS_FORMAT);
    CHECK(!next.path && !next.filter_column && !next.filter_count && !next.source_version);
    CHECK(live.path==new_path && live.filter_value==new_filter && live.filters[0].value==new_secondary && s->undo[0].path==old_path && s->undo[0].filter_value==old_filter && s->undo[0].filters[0].value==old_secondary);
    CHECK(s->analysis.path==cache && !s->analysis.source_version && s->undo_actions_count==1 && s->undo_actions[0]==EDIT_ANALYSIS_FORMAT && s->formats.source_aware && live.source_version==1); journal_counts(s);
    journal_history(s,&live,false); CHECK(live.path==old_path && !live.source_version && !s->formats.source_aware && s->redo[0].path==new_path && s->source_versions.items==ledger && s->source_versions.high_water==2);
    next=analysis_copy(&live); before=*s; current=live; prepared=next; changed=true;
    CHECK(worksheet_analysis_preflight(s,&live,&next,&changed,&error) && !changed && !memcmp(s,&before,sizeof(before)) && !memcmp(&live,&current,sizeof(live)) && !memcmp(&next,&prepared,sizeof(next)) && s->redo_count==1 && s->formats.redo_count==1); analysis_free(&next);
    journal_history(s,&live,true); CHECK(live.path==new_path && live.source_version==1 && s->formats.source_aware && s->source_versions.items==ledger && s->source_versions.high_water==2);
    next=analysis_copy(&live); next.filter_number.scale=3; CHECK(worksheet_analysis_preflight(s,&live,&next,&changed,&error) && changed); analysis_free(&next);
    const char *values[]={NULL,"","NULL"};
    for (size_t i=0;i<3;i++) { next=analysis_copy(&live); free(next.filters[0].value); next.filters[0].value=values[i] ? dx_strdup(values[i]) : NULL; CHECK(worksheet_analysis_preflight(s,&live,&next,&changed,&error) && changed==(i!=2)); analysis_free(&next); }
    CHECK(live.path==new_path && live.source_version==1 && s->source_versions.high_water==2 && s->analysis.path==cache);
    analysis_free(&live); workbook_free(&book); free(error);
}
static void test_analysis_journal_rejection(void) {
    Workbook book; workbook_init(&book); Analysis source={.kind=SOURCE_DEMO}; char *error=NULL; CHECK(workbook_add(&book,"Rejected journal plans",&source,&error)); Worksheet *s=book.sheets;
    Analysis live=analysis_copy(&s->analysis); live.path=dx_strdup("owned-live-text"); Analysis next=analysis_copy(&live); next.unique_rows=true;
    journal_reject(s,&live,&live); journal_reject(s,&live,&s->analysis); journal_reject(s,&live,s->undo); journal_reject(s,&live,(Analysis *)((unsigned char *)&live+1)); journal_reject(s,&live,NULL);
    Analysis borrowed=live; journal_reject(s,&live,&borrowed); borrowed=(Analysis){0};
    free(next.path); next.path=live.path+1; journal_reject(s,&live,&next); next.path=NULL;
    next.path=(char *)&next.source_version; journal_reject(s,&live,&next); next.path=NULL;
    size_t undo=s->undo_count; s->undo_count=DX_HISTORY+1; journal_reject(s,&live,&next); s->undo_count=undo;
    s->formats.redo_count=DX_HISTORY+1; journal_reject(s,&live,&next); s->formats.redo_count=0;
    s->undo_actions_count=1; s->undo_actions[0]=EDIT_ANALYSIS; journal_reject(s,&live,&next); s->undo_actions_count=0;
    size_t filters=next.filter_count; next.filter_count=SIZE_MAX; journal_reject(s,&live,&next); next.filter_count=filters;
    next.sort_count=DX_ANALYSIS_SORTS; journal_reject(s,&live,&next); next.sort_count=0;
    next.filters[0].value=dx_strdup("Unused owned field"); journal_reject(s,&live,&next); free(next.filters[0].value); next.filters[0].value=NULL;
    next.sorts[0].column=dx_strdup("Unused sort"); journal_reject(s,&live,&next); free(next.sorts[0].column); next.sorts[0].column=NULL;
    next.kind=(SourceKind)99; journal_reject(s,&live,&next); next.kind=SOURCE_DEMO;
    next.path=dx_alloc(DX_ANALYSIS_HISTORY_BYTES+1); memset(next.path,'x',DX_ANALYSIS_HISTORY_BYTES); journal_reject(s,&live,&next); free(next.path); next.path=NULL;
    CHECK(objects_add_table(&s->objects,10,10,"Owned table source",&source,&error)==0); worksheet_record(s,EDIT_OBJECTS); journal_reject(s,&live,&s->objects.items[0].source);
    Worksheet before=*s; Analysis prior=live; CHECK(!worksheet_analysis_history_preflight(s,&live,false,&error) && error); free(error); error=NULL; CHECK(!memcmp(s,&before,sizeof(before)) && !memcmp(&live,&prior,sizeof(prior)));
    analysis_free(&next); analysis_free(&live); workbook_free(&book); free(error);
    /* Every entry fits individually, but an already-invalid retained history
       cannot be made safe by silently pruning during publication preflight. */
    workbook_init(&book); CHECK(workbook_add(&book,"Oversized retained history",&source,&error)); s=book.sheets; live=(Analysis){.kind=SOURCE_DEMO}; next=(Analysis){.kind=SOURCE_DEMO};
    char *large=dx_alloc(4300001); memset(large,'x',4300000); Analysis entry={.kind=SOURCE_DEMO,.path=large};
    s->undo[0]=analysis_copy(&entry); s->undo[1]=analysis_copy(&entry); s->undo_count=2; s->undo_actions[0]=s->undo_actions[1]=EDIT_ANALYSIS; s->undo_actions_count=2;
    journal_reject(s,&live,&next); analysis_free(&entry); workbook_free(&book);
}
static void test_analysis_journal_chronology(void) {
    Workbook book; workbook_init(&book); Analysis source={.kind=SOURCE_DEMO,.filter_column=dx_strdup("City"),.filter_value=dx_strdup("initial"),.filter_op=FILTER_EQUAL}; char *error=NULL;
    CHECK(workbook_add(&book,"Interleaved history",&source,&error)); analysis_free(&source); Worksheet *s=book.sheets; Analysis *live=&s->analysis; s->source_versions.high_water=77;
    CHECK(objects_add(&s->objects,100,10,"Persistent object",&error)==0); worksheet_record(s,EDIT_OBJECTS); CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    for (size_t i=0;i<80;i++) {
        char text[24]; snprintf(text,sizeof(text),"%zu",i);
        if (i%4==0 || i%4==3) { Analysis next=analysis_copy(live); free(next.filter_value); next.filter_value=dx_strdup(text); if (i%4) journal_composite(s,live,&next); else journal_analysis(s,live,&next); CHECK(!next.filter_value); }
        else if (i%4==1) { CHECK(format_apply(&s->formats,(FormatRange){i,i,0,0},FORMAT_BOLD,bold,&error)); worksheet_record(s,EDIT_FORMAT); }
        else { CHECK(cells_set(&s->cells,i,4,text,&error)); worksheet_record(s,EDIT_CELLS); }
        journal_counts(s); CHECK(s->source_versions.high_water==77);
    }
    CHECK(s->undo_actions_count==32 && s->undo_count==16 && s->formats.undo_count==16 && s->cells.undo_count==8 && !s->objects.undo_count && !strcmp(live->filter_value,"79"));
    for (size_t i=0;i<32;i++) journal_history(s,live,false);
    CHECK(!s->undo_actions_count && !strcmp(live->filter_value,"47") && s->source_versions.high_water==77 && s->objects.count==1);
    for (size_t i=0;i<32;i++) journal_history(s,live,true);
    CHECK(!strcmp(live->filter_value,"79") && s->source_versions.high_water==77);
    CHECK(objects_rename(&s->objects,0,"Renamed at the branch",&error)); worksheet_record(s,EDIT_OBJECTS);
    for (size_t i=0;i<5;i++) journal_history(s,live,false);
    CHECK(s->redo_count==2 && s->formats.redo_count==2 && s->cells.redo_count==1 && s->objects.redo_count==1);
    Analysis next=analysis_copy(live); free(next.filter_value); next.filter_value=dx_strdup("branch"); journal_composite(s,live,&next);
    CHECK(!s->redo_actions_count && !s->redo_count && !s->formats.redo_count && !s->cells.redo_count && !s->objects.redo_count && s->source_versions.high_water==77 && !strcmp(s->objects.items[0].name,"Persistent object"));
    workbook_free(&book); free(error);
    /* Crossing the Analysis byte budget must prune the chronological prefix
       through a composite action, including its companion format/cell states. */
    workbook_init(&book); source=(Analysis){.kind=SOURCE_DEMO,.filter_column=dx_strdup("City"),.filter_op=FILTER_EQUAL,.filter_value=dx_alloc(3u*1024u*1024u+1)}; memset(source.filter_value,'a',3u*1024u*1024u);
    CHECK(workbook_add(&book,"Byte-budget composite prefix",&source,&error)); analysis_free(&source); s=book.sheets; live=&s->analysis; s->source_versions.high_water=99;
    next=analysis_copy(live); next.filter_value[0]='b'; journal_analysis(s,live,&next);
    CHECK(format_apply(&s->formats,(FormatRange){0,0,0,0},FORMAT_BOLD,bold,&error)); worksheet_record(s,EDIT_FORMAT); CHECK(cells_set(&s->cells,2,4,"Keep current cell",&error)); worksheet_record(s,EDIT_CELLS);
    next=analysis_copy(live); next.filter_value[0]='c'; journal_composite(s,live,&next);
    next=analysis_copy(live); next.filter_value[0]='d'; journal_analysis(s,live,&next);
    CHECK(s->undo_actions_count==4 && s->undo_actions[0]==EDIT_FORMAT && s->undo_actions[2]==EDIT_ANALYSIS_FORMAT && s->undo_count==2 && s->formats.undo_count==2);
    next=analysis_copy(live); next.filter_value[0]='e'; journal_analysis(s,live,&next);
    CHECK(s->undo_actions_count==2 && s->undo_actions[0]==EDIT_ANALYSIS && s->undo_actions[1]==EDIT_ANALYSIS && s->undo_count==2 && !s->formats.undo_count && !s->cells.undo_count && s->formats.source_aware && s->source_versions.high_water==99);
    journal_history(s,live,false); journal_history(s,live,false); CHECK(live->filter_value[0]=='c' && s->formats.source_aware && !strcmp(cells_get(&s->cells,2,4),"Keep current cell") && s->source_versions.high_water==99);
    workbook_free(&book); free(error);
}
static void budget_usage_reject(const Workbook *book,const WorkbookFormatReplacement *replacements,size_t count,FormatBudgetStatus expected) {
    WorkbookFormatUsage out={.sheet_count=19,.current=23,.retained=29,.max_sheet_current=31,.max_sheet=37},before=out;
    CHECK(workbook_format_usage(book,replacements,count,&out)==expected && !memcmp(&out,&before,sizeof(out)));
}
static void budget_reject(const WorkbookFormatUsage *live,const WorkbookFormatUsage *next,const FormatTransientUsage *transient,FormatBudgetStatus expected) {
    FormatBudget out={.before=11,.after=13,.peak=17},before=out;
    CHECK(workbook_format_preflight(live,next,transient,&out)==expected && !memcmp(&out,&before,sizeof(out)));
}
static void test_format_budget_models(void) {
    Workbook book; workbook_init(&book); char *error=NULL; Analysis source={.kind=SOURCE_CSV,.path="/private/budget-source.csv"},blank={0};
    CHECK(workbook_add(&book,"Dormant owned formats",&source,&error)); CHECK(workbook_add(&book,"Retired issuance only",&blank,&error));
    Worksheet *sheet=book.sheets; sheet->objects.next_id=7; book.sheets[1].source_versions.high_water=UINT64_MAX;
    source_format_issue(sheet,0,1,&source,true); source_format_issue(sheet,7,2,&source,false); sheet->analysis.source_version=1;
    FormatScopeView raw={0,sheet->source_versions.items[0].raw.data,sheet->source_versions.items[0].raw.bytes};
    uint64_t ids[]={1,7,UINT64_MAX}; size_t fields[]={0,2}; CellFormat value=format_default(); value.flags=FORMAT_BOLD;
    CHECK(format_apply_records(&sheet->formats,&raw,ids,3,fields,2,FORMAT_BOLD,value,&error));
    SourcePacket result={0}; CHECK(source_scope_result_build(&sheet->source_versions.items[0].raw,&sheet->analysis,&result,&error));
    FormatScopeView exact={0,result.data,result.bytes}; value.flags=FORMAT_ITALIC;
    CHECK(format_apply_scoped(&sheet->formats,(FormatRange){2,4,1,2},FORMAT_ITALIC,value,&exact,1,false,&error)); source_packet_free(&result);
    value.flags=FORMAT_UNDERLINE; CHECK(format_apply(&sheet->formats,(FormatRange){20,20,8,8},FORMAT_UNDERLINE,value,&error));
    CHECK(format_history(&sheet->formats,false) && sheet->formats.undo_count && sheet->formats.redo_count);
    WorkbookFormatUsage live={0}; CHECK(workbook_format_usage(&book,NULL,0,&live)==FORMAT_BUDGET_OK);
    CHECK(live.sheet_count==2 && live.current==format_owned_bytes(&sheet->formats,false)+sheet->source_versions.bytes);
    CHECK(live.retained==format_owned_bytes(&sheet->formats,true)+sheet->source_versions.bytes && live.retained>live.current);
    CHECK(live.max_sheet_current==live.current && live.max_sheet==0 && sheet->source_versions.count==2);
    /* Save snapshots own a separate current-only copy. Dormant raw packets and
       pending entries count once in each model, never once per format history. */
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot); WorkbookFormatUsage saved={0};
    CHECK(workbook_format_usage(snapshot,NULL,0,&saved)==FORMAT_BUDGET_OK && saved.current==live.current && saved.retained==saved.current);
    CHECK(snapshot->sheets[0].formats.items!=sheet->formats.items && snapshot->sheets[0].source_versions.items!=sheet->source_versions.items);
    CHECK(!snapshot->sheets[0].formats.undo_count && !snapshot->sheets[0].formats.redo_count && snapshot->sheets[1].source_versions.high_water==UINT64_MAX);
    Worksheet prior=*sheet; Workbook before=book; FormatSnapshot plan={0}; value.flags=FORMAT_UNDERLINE;
    uint64_t next_ids[]={2,8}; CHECK(format_plan_records(&sheet->formats,&raw,next_ids,2,fields,2,FORMAT_UNDERLINE,value,&plan,&error));
    SourceVersion entry={.owner=0,.version=3,.kind=source.kind,.path=source.path}; SourceVersions versions={0};
    CHECK(source_versions_plan(&sheet->source_versions,&entry,true,&versions,&error));
    /* This is the complete logical post-adoption history: old current is pushed,
       redo is discarded, and no implicit pruning is guessed by the meter. */
    FormatStore projected=sheet->formats;
    projected.undo[projected.undo_count++]=(FormatSnapshot){.items=sheet->formats.items,.count=sheet->formats.count,.scopes=sheet->formats.scopes,.scope_count=sheet->formats.scope_count,.source_aware=sheet->formats.source_aware};
    projected.items=plan.items; projected.count=plan.count; projected.scopes=plan.scopes; projected.scope_count=plan.scope_count; projected.source_aware=plan.source_aware; projected.redo_count=0;
    FormatStore planned_current={.items=plan.items,.count=plan.count,.scopes=plan.scopes,.scope_count=plan.scope_count,.source_aware=plan.source_aware};
    WorkbookFormatReplacement replacement={.sheet=0,.formats=&projected,.versions=&versions}; WorkbookFormatUsage next={0};
    CHECK(workbook_format_usage(&book,&replacement,1,&next)==FORMAT_BUDGET_OK);
    CHECK(next.current==format_owned_bytes(&projected,false)+versions.bytes && next.retained==format_owned_bytes(&projected,true)+versions.bytes);
    FormatTransientUsage transient={.adopted_staging=format_owned_bytes(&planned_current,false)+versions.bytes,.io_snapshots=saved.retained,.other_staging=1,.collector_owned=2,.page_metadata_owned=3,.pending_reserved=4}; FormatBudget budget={0};
    CHECK(workbook_format_preflight(&live,&next,&transient,&budget)==FORMAT_BUDGET_OK);
    CHECK(budget.before==live.retained+transient.adopted_staging+saved.retained+10 && budget.after==next.retained+saved.retained+10);
    CHECK(budget.peak==(budget.before>budget.after ? budget.before : budget.after));
    CHECK(!memcmp(&book,&before,sizeof(book)) && !memcmp(sheet,&prior,sizeof(prior)));
    /* Cancel the independently owned next models. Live history/ledger and every
       measured reservation remain until their owner explicitly releases them. */
    format_snapshot_free(&plan); source_versions_free(&versions); workbook_free(snapshot); free(snapshot);
    WorkbookFormatUsage again={0}; CHECK(workbook_format_usage(&book,NULL,0,&again)==FORMAT_BUDGET_OK && !memcmp(&again,&live,sizeof(live)));
    budget_usage_reject(NULL,NULL,0,FORMAT_BUDGET_INPUT); CHECK(workbook_format_usage(&book,NULL,0,NULL)==FORMAT_BUDGET_INPUT);
    budget_usage_reject(&book,NULL,1,FORMAT_BUDGET_INPUT); budget_usage_reject(&book,NULL,DX_MAX_SHEETS+1,FORMAT_BUDGET_INPUT);
    Workbook invalid=book; invalid.count=DX_MAX_SHEETS+1; budget_usage_reject(&invalid,NULL,0,FORMAT_BUDGET_INPUT); invalid=book; invalid.sheets=NULL; budget_usage_reject(&invalid,NULL,0,FORMAT_BUDGET_INPUT);
    replacement=(WorkbookFormatReplacement){.sheet=book.count}; budget_usage_reject(&book,&replacement,1,FORMAT_BUDGET_INPUT);
    WorkbookFormatReplacement duplicate[]={{.sheet=0},{.sheet=0}}; budget_usage_reject(&book,duplicate,2,FORMAT_BUDGET_INPUT);
    replacement=(WorkbookFormatReplacement){0}; CHECK(workbook_format_usage(&book,&replacement,1,&again)==FORMAT_BUDGET_OK && !memcmp(&again,&live,sizeof(live)));
    FormatStore malformed=sheet->formats; replacement.formats=&malformed; malformed.count=DX_FORMAT_PATCHES+1; budget_usage_reject(&book,&replacement,1,FORMAT_BUDGET_MODEL);
    malformed=sheet->formats; malformed.undo_count=DX_FORMAT_HISTORY+1; budget_usage_reject(&book,&replacement,1,FORMAT_BUDGET_MODEL);
    /* Retained snapshot contents are checked too, even when their byte lengths
       alone would fit: an owned guard vector cannot be missing. */
    FormatPatch invalid_patch={.range={0},.mask=FORMAT_BOLD,.value=format_default(),.selector=FORMAT_RESULT,.guard_count=1};
    malformed=sheet->formats; malformed.undo[0]=(FormatSnapshot){.items=&invalid_patch,.count=1,.source_aware=true}; budget_usage_reject(&book,&replacement,1,FORMAT_BUDGET_MODEL);
    replacement.formats=NULL; SourceVersions invalid_versions=sheet->source_versions; invalid_versions.bytes++; replacement.versions=&invalid_versions; budget_usage_reject(&book,&replacement,1,FORMAT_BUDGET_MODEL);
    replacement.versions=NULL; sheet->objects.next_id=6; budget_usage_reject(&book,&replacement,1,FORMAT_BUDGET_MODEL); sheet->objects.next_id=7;
    entry=(SourceVersion){.owner=8,.version=3,.kind=source.kind,.path=source.path}; CHECK(source_versions_plan(&sheet->source_versions,&entry,true,&versions,&error));
    replacement.versions=&versions; budget_usage_reject(&book,&replacement,1,FORMAT_BUDGET_MODEL); sheet->objects.next_id=8;
    CHECK(workbook_format_usage(&book,&replacement,1,&again)==FORMAT_BUDGET_OK); sheet->objects.next_id=7; source_versions_free(&versions);
    CHECK(!memcmp(sheet,&prior,sizeof(prior))); workbook_free(&book); free(error);
}
static size_t budget_analysis_bytes(const Analysis *analysis) { return sizeof(*analysis)+analysis_text_bytes(analysis); }
static WorkbookFormatUsage budget_analysis_usage(const Workbook *book,size_t extra) {
    WorkbookFormatUsage usage={0}; const Worksheet *sheet=book->sheets; Worksheet before=*sheet;
    CHECK(workbook_format_usage(book,NULL,0,&usage)==FORMAT_BUDGET_OK);
    CHECK(usage.current==format_owned_bytes(&sheet->formats,false)+sheet->source_versions.bytes);
    CHECK(usage.retained==format_owned_bytes(&sheet->formats,true)+sheet->source_versions.bytes+extra);
    CHECK(!memcmp(sheet,&before,sizeof(before))); return usage;
}
static void test_format_budget_analysis_journal(void) {
    Workbook book; workbook_init(&book); Analysis source={.kind=SOURCE_DEMO,.path=dx_strdup("baseline-current-source"),.filter_column=dx_strdup("City"),.filter_op=FILTER_EQUAL}; char *error=NULL;
    CHECK(workbook_add(&book,"Composite Analysis retained charge",&source,&error)); analysis_free(&source); Worksheet *sheet=book.sheets; Analysis *live=&sheet->analysis;
    sheet->source_versions.high_water=91; Analysis next=analysis_copy(live); next.filter_value=dx_strdup("ordinary-before-first-binding"); journal_analysis(sheet,live,&next);
    WorkbookFormatUsage before=budget_analysis_usage(&book,0);
    /* Before the first composite enters the journal, the producer explicitly
       projects the old live graph as retained and reserves the prepared next.
       Each graph switches ownership role once at adoption, without double charge. */
    next=analysis_copy(live); free(next.filter_value); next.filter_value=dx_strdup(""); next.source_version=1;
    WorkbookFormatUsage projected=before; projected.retained+=budget_analysis_bytes(live);
    FormatTransientUsage transient={.adopted_staging=budget_analysis_bytes(&next)}; FormatBudget budget={0};
    CHECK(workbook_format_preflight(&before,&projected,&transient,&budget)==FORMAT_BUDGET_OK && budget.before==before.retained+budget_analysis_bytes(&next) && budget.after==projected.retained);
    journal_composite(sheet,live,&next); size_t first=budget_analysis_bytes(sheet->undo+1); budget_analysis_usage(&book,first);
    CHECK(cells_set(&sheet->cells,7,8,"Interleaved manual value",&error)); worksheet_record(sheet,EDIT_CELLS);
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&sheet->formats,(FormatRange){3,3,6,6},FORMAT_BOLD,bold,&error)); worksheet_record(sheet,EDIT_FORMAT);
    next=analysis_copy(live); free(next.filter_value); next.filter_value=dx_strdup("ordinary-between-bindings"); journal_analysis(sheet,live,&next);
    next=analysis_copy(live); free(next.filter_value); next.filter_value=dx_strdup("NULL"); next.source_version=2; journal_composite(sheet,live,&next);
    CHECK(sheet->undo_actions_count==6 && sheet->undo_count==4 && sheet->undo_actions[1]==EDIT_ANALYSIS_FORMAT && sheet->undo_actions[5]==EDIT_ANALYSIS_FORMAT);
    size_t second=budget_analysis_bytes(sheet->undo+3); WorkbookFormatUsage complete=budget_analysis_usage(&book,first+second);
    /* Format/ledger replacements retain the ORIGINAL Analysis journal charge;
       complete prospective Analysis ownership is an explicit caller adjustment. */
    FormatStore format_view=sheet->formats; format_view.undo_count=format_view.redo_count=0;
    SourceVersions versions={.high_water=92}; WorkbookFormatReplacement replacement={.sheet=0,.formats=&format_view,.versions=&versions};
    WorkbookFormatUsage replaced={0}; CHECK(workbook_format_usage(&book,&replacement,1,&replaced)==FORMAT_BUDGET_OK && replaced.retained==format_owned_bytes(&format_view,true)+first+second);
    CHECK(sheet->source_versions.high_water==91 && sheet->undo_count==4 && sheet->formats.undo_count);
    journal_history(sheet,live,false); size_t redo_bound=budget_analysis_bytes(sheet->redo); budget_analysis_usage(&book,first+redo_bound);
    CHECK(sheet->redo_actions_count==1 && sheet->redo_actions[0]==EDIT_ANALYSIS_FORMAT && !strcmp(sheet->redo[0].filter_value,"NULL"));
    journal_history(sheet,live,false); budget_analysis_usage(&book,first+redo_bound); /* Ordinary Analysis entry is excluded. */
    journal_history(sheet,live,false); budget_analysis_usage(&book,first+redo_bound); /* Format-only action has no Analysis slot. */
    journal_history(sheet,live,false); budget_analysis_usage(&book,first+redo_bound); /* Cell-only action has no Analysis slot. */
    journal_history(sheet,live,false); CHECK(sheet->redo_count==3 && sheet->redo_actions[4]==EDIT_ANALYSIS_FORMAT && !*sheet->redo[2].filter_value);
    budget_analysis_usage(&book,redo_bound+budget_analysis_bytes(sheet->redo+2));
    for (size_t i=0;i<5;i++) journal_history(sheet,live,true);
    WorkbookFormatUsage restored=budget_analysis_usage(&book,first+second); CHECK(!memcmp(&complete,&restored,sizeof(complete)) && sheet->source_versions.high_water==91);
    /* An ordinary branch clears the composite redo and its retained Analysis
       charge through the existing single journal, never through the meter. */
    journal_history(sheet,live,false); next=analysis_copy(live); free(next.filter_value); next.filter_value=dx_strdup("ordinary branch"); journal_analysis(sheet,live,&next);
    CHECK(!sheet->redo_count && !sheet->redo_actions_count); budget_analysis_usage(&book,first);
    workbook_free(&book); free(error);
}
static void test_format_budget_analysis_rejection(void) {
    Workbook book; workbook_init(&book); Analysis source={.kind=SOURCE_DEMO,.path=dx_strdup("independent baseline text"),.filter_column=dx_strdup("City"),.filter_op=FILTER_EQUAL}; char *error=NULL;
    CHECK(workbook_add(&book,"Composite Analysis validation",&source,&error)); analysis_free(&source); Worksheet *sheet=book.sheets; Analysis *live=&sheet->analysis;
    Analysis next=analysis_copy(live); next.filter_value=dx_strdup("ordinary first"); journal_analysis(sheet,live,&next);
    next=analysis_copy(live); next.source_version=1; free(next.filter_value); next.filter_value=dx_strdup(""); journal_composite(sheet,live,&next);
    next=analysis_copy(live); free(next.filter_value); next.filter_value=dx_strdup("ordinary last"); journal_analysis(sheet,live,&next);
    CHECK(sheet->undo_count==3 && sheet->undo_actions_count==3); budget_analysis_usage(&book,budget_analysis_bytes(sheet->undo+1));
    Worksheet before=*sheet; size_t saved_count=sheet->undo_actions_count;
    sheet->undo_actions_count=DX_HISTORY+1; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->undo_actions_count=saved_count;
    saved_count=sheet->undo_count; sheet->undo_count=SIZE_MAX; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->undo_count=saved_count;
    sheet->undo_count--; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->undo_count+=2; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->undo_count--;
    EditKind saved_action=sheet->undo_actions[0]; sheet->undo_actions[0]=(EditKind)99; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL);
    sheet->undo_actions[0]=EDIT_FORMAT; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->undo_actions[0]=saved_action;
    Analysis *bound=sheet->undo+1; char *owned=bound->path;
    bound->path=sheet->undo[0].path; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL);
    bound->path=sheet->undo[0].path+1; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL);
    bound->path=sheet->analysis.path; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL);
    bound->path=(char *)&bound->source_version; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->path=owned;
    bound->schema=bound->path; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->schema=NULL;
    bound->filter_count=SIZE_MAX; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->filter_count=0;
    bound->sort_count=DX_ANALYSIS_SORTS; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->sort_count=0;
    bound->filters[0].value=dx_strdup("unused owned nested value"); budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); free(bound->filters[0].value); bound->filters[0].value=NULL;
    char *large=malloc(DX_ANALYSIS_HISTORY_BYTES+1); CHECK(large); memset(large,'x',DX_ANALYSIS_HISTORY_BYTES); large[DX_ANALYSIS_HISTORY_BYTES]=0;
    bound->path=large; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->path=owned; free(large);
    char *ordinary=sheet->undo[0].path,*large_a=malloc(4200001),*large_b=malloc(4200001); CHECK(large_a && large_b); memset(large_a,'a',4200000); large_a[4200000]=0; memset(large_b,'b',4200000); large_b[4200000]=0;
    sheet->undo[0].path=large_a; bound->path=large_b; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->undo[0].path=ordinary; bound->path=owned; free(large_a); free(large_b);
    CHECK(!memcmp(sheet,&before,sizeof(before)));
    journal_history(sheet,live,false); CHECK(sheet->redo_count==1 && sheet->redo_actions[0]==EDIT_ANALYSIS);
    char *redo_owned=sheet->redo[0].path; sheet->redo[0].path=bound->path; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->redo[0].path=redo_owned;
    journal_history(sheet,live,false); CHECK(sheet->redo_actions[1]==EDIT_ANALYSIS_FORMAT);
    saved_count=sheet->redo_count; sheet->redo_count--; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->redo_count=saved_count;
    saved_count=sheet->redo_actions_count; sheet->redo_actions_count=SIZE_MAX; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->redo_actions_count=saved_count;
    workbook_free(&book); free(error); error=NULL;
    /* With no new composite action, ordinary legacy definitions are excluded
       baseline, including large text. The meter does not retroactively impose
       the new producer's Analysis contract on these existing models. */
    workbook_init(&book); source=(Analysis){.kind=SOURCE_DEMO}; CHECK(workbook_add(&book,"Ordinary baseline remains excluded",&source,&error)); sheet=book.sheets;
    sheet->undo_count=sheet->undo_actions_count=1; sheet->undo_actions[0]=EDIT_ANALYSIS; sheet->undo[0]=(Analysis){.kind=SOURCE_DEMO,.path=malloc(DX_ANALYSIS_HISTORY_BYTES+1)}; CHECK(sheet->undo[0].path);
    memset(sheet->undo[0].path,'x',DX_ANALYSIS_HISTORY_BYTES); sheet->undo[0].path[DX_ANALYSIS_HISTORY_BYTES]=0; budget_analysis_usage(&book,0); workbook_free(&book); free(error);
}
static void budget_object_fixture(Workbook *book) {
    Analysis blank={.kind=SOURCE_DEMO},source={.kind=SOURCE_CSV,.path="/private/table-format-accounting.csv"}; char *error=NULL; CHECK(workbook_add(book,"Object composite accounting",&blank,&error));
    ObjectStore *objects=&book->sheets[0].objects; CHECK(objects_add_table(objects,0,8,"First table",&source,&error)==0);
    PivotDefinition pivot={0}; CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_AVG,&error));
    CHECK(objects_add_chart(objects,10,8,"Independent analytical chart",&pivot,&error)==1); pivot_free(&pivot);
    MarkDefinition marks={0}; MarkField color={.name="City",.aggregate=AGG_SUM}; CHECK(marks_assign(&marks,MARKS_COLOR,&color,&error) && objects_marks(objects,1,&marks,&error)); marks_free(&marks);
    CHECK(objects_add_table(objects,30,8,"Unrelated table",&source,&error)==2); objects_trim_history(objects,false,0); objects_trim_history(objects,true,0);
    SourceVersion entry={.owner=objects->items[0].id,.version=1,.kind=source.kind,.path=source.path};
    SourceVersions versions={0}; CHECK(source_versions_plan(&book->sheets[0].source_versions,&entry,true,&versions,&error)); book->sheets[0].source_versions=versions; free(error);
}
static WorkbookFormatUsage budget_object_usage(const Workbook *book,const ObjectStore *objects,size_t extra) {
    WorkbookFormatUsage usage={0}; WorkbookFormatReplacement replacement={.sheet=0,.objects=objects};
    CHECK(workbook_format_usage(book,&replacement,1,&usage)==FORMAT_BUDGET_OK);
    CHECK(usage.current==format_owned_bytes(&book->sheets[0].formats,false)+book->sheets[0].source_versions.bytes);
    CHECK(usage.retained==format_owned_bytes(&book->sheets[0].formats,true)+book->sheets[0].source_versions.bytes+extra); return usage;
}
static void budget_object_composite(Worksheet *sheet,uint64_t version,bool range) {
    char *error=NULL; bool changed=false; ObjectSnapshot next={0}; ObjectStore *objects=&sheet->objects;
    CHECK(objects_plan_source_version(objects,objects->items[0].id,objects->items[0].source.source_version,version,&next,&changed,&error) && changed);
    FormatSnapshot plan={0}; CHECK(format_plan_move(&sheet->formats,(FormatRange){0},0,0,&plan,&error)); plan.source_aware=!sheet->formats.source_aware;
    if (range) CHECK(cells_set(&sheet->cells,100,20,"Range/objects/format constituent",&error));
    objects_adopt_source_version(objects,&next); CHECK(!next.items && !next.count && format_commit(&sheet->formats,&plan));
    worksheet_record(sheet,range ? EDIT_RANGE_FORMAT : EDIT_OBJECTS_FORMAT); journal_counts(sheet); free(error);
}
static void test_format_budget_objects(void) {
    Workbook book; workbook_init(&book); budget_object_fixture(&book); Worksheet *sheet=book.sheets; ObjectStore *objects=&sheet->objects; char *error=NULL;
    CHECK(objects_rename(objects,1,"Ordinary chart rename",&error)); worksheet_record(sheet,EDIT_OBJECTS); budget_object_usage(&book,NULL,0);
    ObjectSnapshot current={objects->items,objects->count},prepared={0}; bool changed=false; CHECK(objects_plan_source_version(objects,objects->items[0].id,0,1,&prepared,&changed,&error) && changed);
    size_t whole=objects_snapshot_owned_bytes(&current),next_bytes=objects_snapshot_owned_bytes(&prepared),expected=DX_MAX_OBJECTS*sizeof(SheetObject);
    for (size_t i=0;i<objects->count;i++) {
        const SheetObject *object=objects->items+i; expected+=strlen(object->name)+1+analysis_text_bytes(&object->source)+marks_text_bytes(&object->marks);
        for (size_t r=0;r<4;r++) for (size_t j=0;j<object->pivot.counts[r];j++) expected+=strlen(object->pivot.fields[r][j].name)+1+(object->pivot.fields[r][j].value ? strlen(object->pivot.fields[r][j].value)+1 : 0);
    }
    CHECK(whole==expected && next_bytes==whole && whole>sizeof(SheetObject)+strlen(objects->items[0].name)+1+analysis_text_bytes(&objects->items[0].source));
    CHECK(prepared.items!=objects->items && prepared.items[1].name!=objects->items[1].name && prepared.items[1].marks.fields[MARKS_COLOR][0].name!=objects->items[1].marks.fields[MARKS_COLOR][0].name);
    WorkbookFormatUsage before=budget_object_usage(&book,NULL,0),projected=before; projected.retained+=whole;
    FormatTransientUsage transient={.adopted_staging=next_bytes}; FormatBudget budget={0};
    CHECK(workbook_format_preflight(&before,&projected,&transient,&budget)==FORMAT_BUDGET_OK && budget.before==before.retained+next_bytes && budget.after==projected.retained);
    objects_snapshot_free(&prepared); budget_object_usage(&book,NULL,0); CHECK(sheet->source_versions.high_water==1); /* Cancel keeps separate issued lifetime. */
    budget_object_composite(sheet,1,false); CHECK(objects->undo_count==2 && sheet->undo_actions[1]==EDIT_OBJECTS_FORMAT); size_t first=objects_snapshot_owned_bytes(objects->undo+1); budget_object_usage(&book,NULL,first);
    CHECK(objects_rename(objects,2,"Ordinary range table rename",&error) && cells_set(&sheet->cells,101,20,"Ordinary range constituent",&error)); worksheet_record(sheet,EDIT_RANGE);
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&sheet->formats,(FormatRange){120,120,7,7},FORMAT_BOLD,bold,&error)); worksheet_record(sheet,EDIT_FORMAT);
    SourceVersion issued={.owner=objects->items[0].id,.version=2,.kind=SOURCE_CSV,.path=objects->items[0].source.path}; SourceVersions versions={0};
    CHECK(source_versions_plan(&sheet->source_versions,&issued,true,&versions,&error)); source_versions_free(&sheet->source_versions); sheet->source_versions=versions;
    budget_object_composite(sheet,2,true); CHECK(objects->undo_count==4 && sheet->undo_actions_count==5 && sheet->undo_actions[4]==EDIT_RANGE_FORMAT);
    size_t second=objects_snapshot_owned_bytes(objects->undo+3); WorkbookFormatUsage complete=budget_object_usage(&book,NULL,first+second);
    journal_history(sheet,&sheet->analysis,false); size_t redo=objects_snapshot_owned_bytes(objects->redo); budget_object_usage(&book,NULL,first+redo);
    journal_history(sheet,&sheet->analysis,false); budget_object_usage(&book,NULL,first+redo); /* Format-only skips object index. */
    journal_history(sheet,&sheet->analysis,false); budget_object_usage(&book,NULL,first+redo); /* Ordinary range snapshot is excluded. */
    journal_history(sheet,&sheet->analysis,false); CHECK(objects->redo_count==3); budget_object_usage(&book,NULL,redo+objects_snapshot_owned_bytes(objects->redo+2));
    for (size_t i=0;i<4;i++) journal_history(sheet,&sheet->analysis,true);
    WorkbookFormatUsage restored=budget_object_usage(&book,NULL,first+second); CHECK(!memcmp(&complete,&restored,sizeof(complete)) && sheet->source_versions.high_water==2);
    journal_history(sheet,&sheet->analysis,false); CHECK(objects_rename(objects,1,"Ordinary object branch",&error)); worksheet_record(sheet,EDIT_OBJECTS);
    CHECK(!objects->redo_count && !sheet->redo_actions_count); budget_object_usage(&book,NULL,first);
    /* Actual ObjectStore authority can be external to the save cache. Its owned
       histories and high-water map against the original worksheet journal. */
    ObjectStore authority=*objects; *objects=(ObjectStore){0}; WorkbookFormatReplacement replacement={.sheet=0,.objects=&authority}; WorkbookFormatUsage external={0};
    budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); CHECK(workbook_format_usage(&book,&replacement,1,&external)==FORMAT_BUDGET_OK);
    CHECK(external.retained==format_owned_bytes(&sheet->formats,true)+sheet->source_versions.bytes+first);
    FormatStore format_view=sheet->formats; format_view.undo_count=format_view.redo_count=0; replacement.formats=&format_view;
    CHECK(workbook_format_usage(&book,&replacement,1,&external)==FORMAT_BUDGET_OK && external.retained==format_owned_bytes(&format_view,true)+sheet->source_versions.bytes+first);
    uint64_t high_water=authority.next_id; authority.next_id=0; budget_usage_reject(&book,&replacement,1,FORMAT_BUDGET_MODEL); authority.next_id=high_water;
    authority.undo_count++; budget_usage_reject(&book,&replacement,1,FORMAT_BUDGET_MODEL); authority.undo_count--; /* An unrecorded next push is not an authority view. */
    *objects=authority; authority=(ObjectStore){0}; workbook_free(&book); free(error);
}
static void test_format_budget_object_rejection(void) {
    Workbook book; workbook_init(&book); budget_object_fixture(&book); Worksheet *sheet=book.sheets; ObjectStore *objects=&sheet->objects; char *error=NULL;
    CHECK(objects_rename(objects,1,"Ordinary prior object",&error)); worksheet_record(sheet,EDIT_OBJECTS); budget_object_composite(sheet,1,false);
    Worksheet before=*sheet; size_t count=objects->undo_count;
    objects->undo_count=SIZE_MAX; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); objects->undo_count=count;
    objects->undo_count--; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); objects->undo_count+=2; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); objects->undo_count--;
    count=sheet->undo_actions_count; sheet->undo_actions_count=DX_HISTORY+1; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->undo_actions_count=count;
    EditKind action=sheet->undo_actions[0]; sheet->undo_actions[0]=(EditKind)99; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->undo_actions[0]=EDIT_FORMAT; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->undo_actions[0]=action;
    ObjectSnapshot *bound=objects->undo+1,retained=*bound; bound->items=malloc(1); bound->count=0; CHECK(bound->items);
    CHECK(objects_snapshot_owned_bytes(bound)==DX_MAX_OBJECTS*sizeof(SheetObject)); budget_object_usage(&book,NULL,DX_MAX_OBJECTS*sizeof(SheetObject)); free(bound->items); *bound=retained;
    count=bound->count; bound->count=DX_MAX_OBJECTS+1; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->count=count;
    SheetObject *items=bound->items; bound->items=NULL; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->items=items;
    bound->items[0].source.filter_count=SIZE_MAX; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->items[0].source.filter_count=0;
    bound->items[0].source.filters[0].value=dx_strdup("inactive owned text"); budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); free(bound->items[0].source.filters[0].value); bound->items[0].source.filters[0].value=NULL;
    count=bound->items[1].pivot.counts[PIVOT_ROWS]; bound->items[1].pivot.counts[PIVOT_ROWS]=DX_PIVOT_FIELDS+1; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->items[1].pivot.counts[PIVOT_ROWS]=count;
    count=bound->items[1].marks.counts[MARKS_COLOR]; bound->items[1].marks.counts[MARKS_COLOR]=DX_MARK_FIELDS+1; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->items[1].marks.counts[MARKS_COLOR]=count;
    char *name=bound->items[0].name; bound->items[0].name=NULL; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->items[0].name=name;
    char *path=bound->items[0].source.path,*large=malloc(DX_OBJECT_HISTORY_BYTES+1); CHECK(large); memset(large,'x',DX_OBJECT_HISTORY_BYTES); large[DX_OBJECT_HISTORY_BYTES]=0;
    bound->items[0].source.path=large; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); bound->items[0].source.path=path; free(large);
    char *paths[]={objects->undo[0].items[0].source.path,bound->items[0].source.path}; char *large_a=malloc(4190001),*large_b=malloc(4190001); CHECK(large_a && large_b); memset(large_a,'a',4190000); large_a[4190000]=0; memset(large_b,'b',4190000); large_b[4190000]=0;
    objects->undo[0].items[0].source.path=large_a; bound->items[0].source.path=large_b; CHECK(objects_snapshot_owned_bytes(objects->undo)+objects_snapshot_owned_bytes(bound)>DX_OBJECT_HISTORY_BYTES); budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL);
    objects->undo[0].items[0].source.path=paths[0]; bound->items[0].source.path=paths[1]; free(large_a); free(large_b); CHECK(!memcmp(sheet,&before,sizeof(before)));
    journal_history(sheet,&sheet->analysis,false); count=objects->redo_count; objects->redo_count=DX_HISTORY+1; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); objects->redo_count=count;
    count=sheet->redo_actions_count; sheet->redo_actions_count=SIZE_MAX; budget_usage_reject(&book,NULL,0,FORMAT_BUDGET_MODEL); sheet->redo_actions_count=count;
    workbook_free(&book); free(error); error=NULL;
    /* No object+format action means ordinary data definitions remain excluded;
       their existing behavior is not gated by this formatting-only policy. */
    workbook_init(&book); budget_object_fixture(&book); sheet=book.sheets; objects=&sheet->objects; CHECK(objects_rename(objects,1,"Ordinary-only history",&error)); worksheet_record(sheet,EDIT_OBJECTS);
    path=objects->undo[0].items[0].source.path; large=malloc(DX_OBJECT_HISTORY_BYTES+1); CHECK(large); memset(large,'x',DX_OBJECT_HISTORY_BYTES); large[DX_OBJECT_HISTORY_BYTES]=0;
    objects->undo[0].items[0].source.path=large; budget_object_usage(&book,NULL,0); objects->undo[0].items[0].source.path=path; free(large); workbook_free(&book); free(error);
}
static size_t budget_object_logical(const ObjectSnapshot *state) {
    size_t bytes=state->count*sizeof(SheetObject);
    for (size_t i=0;i<state->count;i++) {
        const SheetObject *object=state->items+i; bytes+=strlen(object->name)+1+analysis_text_bytes(&object->source)+marks_text_bytes(&object->marks);
        for (size_t r=0;r<4;r++) for (size_t j=0;j<object->pivot.counts[r];j++) bytes+=strlen(object->pivot.fields[r][j].name)+1+(object->pivot.fields[r][j].value ? strlen(object->pivot.fields[r][j].value)+1 : 0);
    }
    return bytes;
}
static void test_format_budget_object_legacy_slack(void) {
    Workbook book; workbook_init(&book); budget_object_fixture(&book); Worksheet *sheet=book.sheets; ObjectStore *objects=&sheet->objects; char *error=NULL;
    char *value=malloc(180001); CHECK(value); memset(value,'x',180000); value[180000]=0;
    CHECK(analysis_filter_set(&objects->items[0].source,"City",FILTER_CONTAINS,value,&error)); free(value);
    budget_object_composite(sheet,1,false);
    for (size_t i=0;i<DX_HISTORY-1;i++) { char name[48]; snprintf(name,sizeof(name),"Ordinary chart history %zu",i); CHECK(objects_rename(objects,1,name,&error)); worksheet_record(sheet,EDIT_OBJECTS); }
    CHECK(objects->undo_count==DX_HISTORY && sheet->undo_actions_count==DX_HISTORY && sheet->undo_actions[0]==EDIT_OBJECTS_FORMAT && sheet->formats.undo_count==1);
    size_t logical=0,reserved=0;
    for (size_t i=0;i<objects->undo_count;i++) { logical+=budget_object_logical(objects->undo+i); reserved+=objects_snapshot_owned_bytes(objects->undo+i); }
    CHECK(logical<=DX_OBJECT_HISTORY_BYTES && reserved>DX_OBJECT_HISTORY_BYTES && objects_valid(objects,&error));
    ObjectSnapshot unchanged={0}; bool changed=true;
    CHECK(objects_plan_source_version(objects,objects->items[0].id,1,1,&unchanged,&changed,&error) && !changed && !unchanged.items && !unchanged.count);
    Worksheet before=*sheet; size_t composite=objects_snapshot_owned_bytes(objects->undo); WorkbookFormatUsage measured=budget_object_usage(&book,NULL,composite);
    CHECK(measured.retained==format_owned_bytes(&sheet->formats,true)+sheet->source_versions.bytes+composite && !memcmp(sheet,&before,sizeof(before)));
    /* A valid current graph may similarly approach its legacy logical bound
       while the conservative array reservation crosses that bound. */
    Analysis *source=&objects->items[0].source; CHECK(analysis_filter_set(source,"City",FILTER_CONTAINS,"a",&error) && analysis_filter_set(source,"Id",FILTER_CONTAINS,"b",&error));
    ObjectSnapshot current={objects->items,objects->count}; size_t base=budget_object_logical(&current)-4,target=DX_OBJECT_CURRENT_BYTES-1;
    size_t first=4u*1024u*1024u,second=target-base-first; CHECK(second>1 && second<=first);
    char *a=malloc(first),*b=malloc(second); CHECK(a && b); memset(a,'a',first-1); a[first-1]=0; memset(b,'b',second-1); b[second-1]=0;
    free(source->filter_value); source->filter_value=a; free(source->filters[0].value); source->filters[0].value=b;
    CHECK(budget_object_logical(&current)==target && objects_snapshot_owned_bytes(&current)>DX_OBJECT_CURRENT_BYTES && objects_valid(objects,&error));
    changed=true; CHECK(objects_plan_source_version(objects,objects->items[0].id,1,1,&unchanged,&changed,&error) && !changed);
    budget_object_usage(&book,NULL,composite);
    /* Unknown retained capacity for a nonnull empty current allocation is a
       reservation category, not extra logical object definitions. */
    SheetObject *owned=objects->items; size_t count=objects->count; objects->items=malloc(1); objects->count=0; CHECK(objects->items); budget_object_usage(&book,NULL,composite); free(objects->items); objects->items=owned; objects->count=count;
    workbook_free(&book); free(error);
}
static void test_format_budget_combined_sheet(void) {
    Workbook book; workbook_init(&book); Analysis blank={0}; char *error=NULL; CHECK(workbook_add(&book,"Combined owned limit",&blank,&error));
    Worksheet *sheet=book.sheets; CellFormat value=format_default(); value.flags=FORMAT_BOLD;
    CHECK(format_apply(&sheet->formats,(FormatRange){1,1,0,0},FORMAT_BOLD,value,&error));
    SourceVersions *versions=&sheet->source_versions; versions->count=versions->capacity=16; versions->high_water=16;
    versions->items=calloc(versions->capacity,sizeof(*versions->items)); CHECK(versions->items); versions->bytes=versions->capacity*sizeof(*versions->items);
    const size_t per_path=(DX_SOURCE_VERSIONS_BYTES-versions->bytes)/versions->count;
    CHECK(per_path<=DX_SOURCE_SCOPE_TEXT_BYTES && per_path>sizeof(FormatPatch)+1);
    for (size_t i=0;i<versions->count;i++) {
        size_t bytes=per_path-(i ? 0 : sizeof(FormatPatch)); char *path=malloc(bytes); CHECK(path); memset(path,'p',bytes-1); path[bytes-1]=0;
        versions->items[i]=(SourceVersion){.owner=0,.version=i+1,.kind=SOURCE_CSV,.path=path}; versions->bytes+=bytes;
    }
    CHECK(source_versions_valid(versions,0,&error) && format_valid(&sheet->formats,&error)); WorkbookFormatUsage usage={0}; FormatBudget budget={0};
    CHECK(workbook_format_usage(&book,NULL,0,&usage)==FORMAT_BUDGET_OK && usage.current==DX_FORMAT_CURRENT_BYTES && usage.retained==usage.current);
    CHECK(workbook_format_preflight(&usage,&usage,NULL,&budget)==FORMAT_BUDGET_OK);
    char *old=versions->items[0].path; size_t length=strlen(old); char *longer=malloc(length+2); CHECK(longer); memcpy(longer,old,length); longer[length]='p'; longer[length+1]=0; free(old); versions->items[0].path=longer; versions->bytes++;
    CHECK(source_versions_valid(versions,0,&error) && versions->bytes<=DX_SOURCE_VERSIONS_BYTES && format_valid(&sheet->formats,&error));
    CHECK(workbook_format_usage(&book,NULL,0,&usage)==FORMAT_BUDGET_OK && usage.current==DX_FORMAT_CURRENT_BYTES+1);
    Worksheet before=*sheet; budget_reject(&usage,&usage,NULL,FORMAT_BUDGET_SHEET); CHECK(!memcmp(sheet,&before,sizeof(before)));
    workbook_free(&book); free(error);
}
static void test_format_budget_policy(void) {
    const size_t mib=1024u*1024u; WorkbookFormatUsage empty={0},next={.sheet_count=8,.current=8*mib,.retained=32*mib,.max_sheet_current=mib,.max_sheet=7},live=next;
    FormatTransientUsage transient={.adopted_staging=32*mib}; FormatBudget budget={0};
    CHECK(workbook_format_preflight(&live,&next,&transient,&budget)==FORMAT_BUDGET_OK && budget.before==64*mib && budget.after==32*mib && budget.peak==64*mib);
    transient.adopted_staging++; budget_reject(&live,&next,&transient,FORMAT_BUDGET_APP);
    transient=(FormatTransientUsage){.other_staging=32*mib}; CHECK(workbook_format_preflight(&empty,&next,&transient,&budget)==FORMAT_BUDGET_OK && budget.before==32*mib && budget.after==64*mib);
    transient.other_staging++; budget_reject(&empty,&next,&transient,FORMAT_BUDGET_APP);
    WorkbookFormatUsage bad=next; bad.max_sheet_current++; budget_reject(&empty,&bad,NULL,FORMAT_BUDGET_SHEET);
    bad=next; bad.sheet_count=9; bad.current++; budget_reject(&empty,&bad,NULL,FORMAT_BUDGET_CURRENT);
    bad=next; bad.retained++; budget_reject(&empty,&bad,NULL,FORMAT_BUDGET_RETAINED);
    /* Existing over-cap history can be measured and then explicitly pruned in a
       complete projected model; preflight never silently repairs live state. */
    live.retained++; CHECK(workbook_format_preflight(&live,&next,NULL,&budget)==FORMAT_BUDGET_OK && budget.before==32*mib+1);
    budget_reject(&live,&live,NULL,FORMAT_BUDGET_RETAINED);
    WorkbookFormatUsage malformed[]={
        {.sheet_count=DX_MAX_SHEETS+1},{.sheet_count=1,.current=2,.retained=1,.max_sheet_current=1},
        {.sheet_count=1,.current=1,.retained=1,.max_sheet_current=2},{.retained=1},
        {.max_sheet=1},{.sheet_count=1,.max_sheet=1},{.sheet_count=1,.current=1,.retained=1},
        {.sheet_count=1,.current=2,.retained=2,.max_sheet_current=1}
    };
    for (size_t i=0;i<sizeof(malformed)/sizeof(*malformed);i++) { budget_reject(&empty,malformed+i,NULL,FORMAT_BUDGET_INPUT); budget_reject(malformed+i,&empty,NULL,FORMAT_BUDGET_INPUT); }
    budget_reject(NULL,&empty,NULL,FORMAT_BUDGET_INPUT); budget_reject(&empty,NULL,NULL,FORMAT_BUDGET_INPUT); CHECK(workbook_format_preflight(&empty,&empty,NULL,NULL)==FORMAT_BUDGET_INPUT);
    CHECK(workbook_format_preflight(&empty,&empty,NULL,&budget)==FORMAT_BUDGET_OK && !budget.before && !budget.after && !budget.peak);
    /* All active, latest-pending, published, I/O and Page owners coexist until
       explicitly freed; cancellation alone supplies no memory discount. */
    live=(WorkbookFormatUsage){.sheet_count=1,.current=mib,.retained=4*mib,.max_sheet_current=mib}; next=live;
    transient=(FormatTransientUsage){.io_snapshots=5*mib,.other_staging=6*mib,.collector_owned=24*mib,.page_metadata_owned=9*mib,.pending_reserved=16*mib};
    CHECK(workbook_format_preflight(&live,&next,&transient,&budget)==FORMAT_BUDGET_OK && budget.peak==64*mib);
    transient.collector_owned++; budget_reject(&live,&next,&transient,FORMAT_BUDGET_APP); transient.collector_owned--;
    transient.collector_owned-=8*mib; CHECK(workbook_format_preflight(&live,&next,&transient,&budget)==FORMAT_BUDGET_OK && budget.peak==56*mib);
    /* Upper-bound reservation first, then conversion to actual owned staging
       before adoption. The after count excludes consumed staging exactly once. */
    transient=(FormatTransientUsage){.pending_reserved=3*mib}; CHECK(workbook_format_preflight(&live,&next,&transient,&budget)==FORMAT_BUDGET_OK && budget.before==7*mib && budget.after==7*mib);
    transient=(FormatTransientUsage){.adopted_staging=2*mib}; next.retained+=2*mib;
    CHECK(workbook_format_preflight(&live,&next,&transient,&budget)==FORMAT_BUDGET_OK && budget.before==6*mib && budget.after==6*mib);
    for (size_t i=0;i<5;i++) {
        transient=(FormatTransientUsage){0}; size_t *fields[]={&transient.io_snapshots,&transient.other_staging,&transient.collector_owned,&transient.page_metadata_owned,&transient.pending_reserved};
        *fields[i]=SIZE_MAX; *fields[(i+1)%5]=1; budget_reject(&empty,&empty,&transient,FORMAT_BUDGET_OVERFLOW);
    }
    transient=(FormatTransientUsage){.adopted_staging=SIZE_MAX}; budget_reject(&live,&next,&transient,FORMAT_BUDGET_OVERFLOW);
    transient=(FormatTransientUsage){.other_staging=SIZE_MAX}; budget_reject(&empty,&next,&transient,FORMAT_BUDGET_OVERFLOW);
    for (int i=FORMAT_BUDGET_OK;i<=FORMAT_BUDGET_APP;i++) { const char *message=workbook_format_budget_message((FormatBudgetStatus)i); CHECK(message && *message && message==workbook_format_budget_message((FormatBudgetStatus)i)); }
    CHECK(!strcmp(workbook_format_budget_message((FormatBudgetStatus)-1),"Unknown formatting budget status."));
}
static void test_format_budget_legacy_measurement(void) {
    /* A legacy spatial-only book can retain more than the proposed workbook-wide
       history cap while each existing sheet remains within its accepted bound.
       Accounting must observe it without changing legacy Save behavior. */
    const size_t per_state=DX_FORMAT_PATCHES*sizeof(FormatPatch),per_sheet=per_state*(DX_FORMAT_HISTORY+1);
    size_t sheets=DX_WORKBOOK_FORMAT_RETAINED_BYTES/per_sheet+1; CHECK(sheets<=DX_MAX_SHEETS);
    Workbook book; workbook_init(&book); Analysis blank={0}; char *error=NULL;
    for (size_t i=0;i<sheets;i++) {
        CHECK(workbook_add(&book,"Legacy spatial histories",&blank,&error)); FormatStore *formats=&book.sheets[i].formats;
        formats->count=DX_FORMAT_PATCHES; formats->items=calloc(formats->count,sizeof(*formats->items)); CHECK(formats->items);
        for (size_t n=0;n<formats->count;n++) formats->items[n]=(FormatPatch){.range={n,n,0,0},.mask=FORMAT_BOLD,.value=format_default()};
        formats->undo_count=DX_FORMAT_HISTORY;
        for (size_t n=0;n<formats->undo_count;n++) {
            formats->undo[n]=(FormatSnapshot){.items=malloc(per_state),.count=formats->count}; CHECK(formats->undo[n].items); memcpy(formats->undo[n].items,formats->items,per_state);
        }
        CHECK(format_valid(formats,&error) && format_persistable(formats,&error) && format_owned_bytes(formats,true)==per_sheet);
    }
    Workbook prior=book; WorkbookFormatUsage usage={0}; CHECK(workbook_format_usage(&book,NULL,0,&usage)==FORMAT_BUDGET_OK);
    CHECK(usage.current==per_state*sheets && usage.retained==per_sheet*sheets && usage.retained>DX_WORKBOOK_FORMAT_RETAINED_BYTES);
    budget_reject(&usage,&usage,NULL,FORMAT_BUDGET_RETAINED); CHECK(!memcmp(&prior,&book,sizeof(book)));
    /* The caller supplies the explicit post-pruning histories. These views borrow
       current allocations, add no ownership and are never freed as new stores. */
    WorkbookFormatReplacement replacements[DX_MAX_SHEETS]={0}; FormatStore projected[DX_MAX_SHEETS]={0};
    for (size_t i=0;i<sheets;i++) { projected[i]=book.sheets[i].formats; projected[i].undo_count=0; replacements[i]=(WorkbookFormatReplacement){.sheet=i,.formats=projected+i}; }
    WorkbookFormatUsage next={0}; FormatBudget budget={0}; CHECK(workbook_format_usage(&book,replacements,sheets,&next)==FORMAT_BUDGET_OK && next.retained==usage.current);
    CHECK(workbook_format_preflight(&usage,&next,NULL,&budget)==FORMAT_BUDGET_OK && budget.before==usage.retained && budget.after==next.retained);
    for (size_t i=0;i<sheets;i++) CHECK(book.sheets[i].formats.undo_count==DX_FORMAT_HISTORY && format_persistable(&book.sheets[i].formats,&error));
    workbook_free(&book); free(error);
}
static void legacy_history_accept(const Worksheet *sheet) {
    Worksheet before=*sheet; char *error=dx_strdup("Keep prior diagnostic"),*prior=error;
    CHECK(worksheet_legacy_format_history_eligible(sheet,&error) && error==prior && !strcmp(error,"Keep prior diagnostic"));
    CHECK(!memcmp(&before,sheet,sizeof(before))); free(error);
}
static void legacy_history_reject(const Worksheet *sheet,const char *message) {
    Worksheet before=*sheet; char *error=dx_strdup("Replace prior diagnostic");
    CHECK(!worksheet_legacy_format_history_eligible(sheet,&error) && error && strstr(error,message));
    CHECK(!memcmp(&before,sheet,sizeof(before)) && !worksheet_legacy_format_history_eligible(sheet,NULL)); free(error);
}
static void test_legacy_format_history(void) {
    Workbook book={0}; char *error=NULL; Analysis blank={0}; CHECK(workbook_add(&book,"Legacy journal",&blank,&error));
    Worksheet *s=book.sheets; CellFormat value=format_default(); value.flags=FORMAT_BOLD; legacy_history_accept(s);
    CHECK(cells_set(&s->cells,40,8,"First note",&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(format_apply(&s->formats,(FormatRange){2,8,1,3},FORMAT_BOLD,value,&error)); worksheet_record(s,EDIT_FORMAT);
    CHECK(cells_set(&s->cells,41,8,"Second note",&error)); value.color=2;
    CHECK(format_apply(&s->formats,(FormatRange){40,41,8,8},FORMAT_COLOR,value,&error)); worksheet_record(s,EDIT_CELLS_FORMAT);
    CHECK(cells_set(&s->cells,42,8,"Third note",&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(s->undo_actions_count==4 && s->cells.undo_count==3 && s->formats.undo_count==2); legacy_history_accept(s);
    while (s->undo_actions_count) { format_model_history(s,false); legacy_history_accept(s); }
    CHECK(!s->cells.count && !s->cells.capacity && s->cells.cells); /* Valid owned empty snapshot. */
    while (s->redo_actions_count) { format_model_history(s,true); legacy_history_accept(s); }
    /* Already-aware current/retained state is not reinterpreted. History-only
       eligibility does not certify the sentinel packet's provenance. */
    const char packet[]="caller-verified-result"; FormatScopeView scope={0,packet,sizeof(packet)-1}; value.size=16;
    CHECK(format_apply_scoped(&s->formats,(FormatRange){2,8,1,3},FORMAT_SIZE,value,&scope,1,false,&error)); worksheet_record(s,EDIT_FORMAT);
    legacy_history_accept(s); format_model_history(s,false); legacy_history_accept(s);
    CHECK(!s->formats.source_aware && s->formats.redo[0].source_aware && s->undo_actions_count==4 && s->redo_actions_count==1);
    const EditKind forbidden[]={EDIT_ANALYSIS,EDIT_OBJECTS,EDIT_RANGE,EDIT_OBJECTS_FORMAT,EDIT_RANGE_FORMAT,EDIT_ANALYSIS_FORMAT,(EditKind)99,(EditKind)-1};
    for (size_t h=0;h<2;h++) for (size_t i=0;i<sizeof(forbidden)/sizeof(*forbidden);i++) {
        Worksheet invalid=*s; (h ? invalid.redo_actions : invalid.undo_actions)[0]=forbidden[i];
        legacy_history_reject(&invalid,"only retained Cell and Format");
    }
    Worksheet invalid=*s; invalid.undo_count=1; legacy_history_reject(&invalid,"Analysis or object");
    invalid=*s; invalid.redo_count=SIZE_MAX; legacy_history_reject(&invalid,"Analysis or object");
    invalid=*s; invalid.objects.undo_count=1; legacy_history_reject(&invalid,"Analysis or object");
    invalid=*s; invalid.objects.redo_count=SIZE_MAX; legacy_history_reject(&invalid,"Analysis or object");
    invalid=*s; invalid.undo_actions_count=SIZE_MAX; legacy_history_reject(&invalid,"bounded worksheet history");
    invalid=*s; invalid.redo_actions_count=DX_HISTORY+1; legacy_history_reject(&invalid,"bounded worksheet history");
    invalid=*s; invalid.cells.undo_count=DX_HISTORY+1; legacy_history_reject(&invalid,"bounded worksheet history");
    invalid=*s; invalid.formats.redo_count=SIZE_MAX; legacy_history_reject(&invalid,"bounded worksheet history");
    invalid=*s; invalid.undo_actions_count=0; legacy_history_reject(&invalid,"aligned Cell and Format");
    invalid=*s; invalid.redo_actions_count=0; legacy_history_reject(&invalid,"aligned Cell and Format");
    invalid=*s; invalid.cells.undo_count--; legacy_history_reject(&invalid,"aligned Cell and Format");
    invalid=*s; invalid.formats.undo_count++; legacy_history_reject(&invalid,"aligned Cell and Format");
    invalid=*s; invalid.formats.redo_count=0; legacy_history_reject(&invalid,"aligned Cell and Format");
    invalid=*s; invalid.cells.redo_count=1; legacy_history_reject(&invalid,"aligned Cell and Format");
    invalid=*s; invalid.cells.count=DX_MAX_EDIT_CELLS+1; legacy_history_reject(&invalid,"editable-cell array shape");
    invalid=*s; invalid.cells.capacity=0; legacy_history_reject(&invalid,"editable-cell array shape");
    invalid=*s; invalid.cells.cells=NULL; legacy_history_reject(&invalid,"editable-cell array shape");
    invalid=*s; invalid.cells.bytes=SIZE_MAX; legacy_history_reject(&invalid,"editable-cell array shape");
    invalid=*s; invalid.cells.undo[1].count=DX_MAX_EDIT_CELLS+1; legacy_history_reject(&invalid,"retained editable-cell array shape");
    invalid=*s; invalid.cells.undo[1].cells=NULL; legacy_history_reject(&invalid,"retained editable-cell array shape");
    invalid=*s; invalid.cells.undo[1].bytes=DX_CELL_BYTES+1; legacy_history_reject(&invalid,"retained editable-cell array shape");
    invalid=*s; invalid.formats.count=DX_FORMAT_PATCHES+1; legacy_history_reject(&invalid,"invalid current or retained cell formats");
    invalid=*s; invalid.formats.redo[0].source_aware=false; legacy_history_reject(&invalid,"invalid current or retained cell formats");
    FormatPatch *patch=s->formats.undo[1].items; CHECK(patch && s->formats.undo[1].count);
    unsigned mask=patch->mask; patch->mask=0; legacy_history_reject(s,"invalid current or retained cell formats"); patch->mask=mask;
    CHECK(!worksheet_legacy_format_history_eligible(NULL,&error) && error && strstr(error,"bounded worksheet history")); free(error); error=NULL;
    legacy_history_accept(s); format_model_history(s,true); legacy_history_accept(s);
    CHECK(cells_clear(&s->cells,0,DX_SHEET_ROWS-1,0,16383)); worksheet_record(s,EDIT_CELLS); legacy_history_accept(s);
    CHECK(!s->cells.count && s->cells.cells); format_model_history(s,false); legacy_history_accept(s);
    format_model_history(s,true); legacy_history_accept(s); workbook_free(&book);
    /* Existing chronological prefix pruning is not performed by eligibility;
       after many valid edits both bounded stacks still map exactly. */
    CHECK(workbook_add(&book,"Bounded journal",&blank,&error)); s=book.sheets;
    for (size_t i=0;i<80;i++) {
        if (i%3!=1) { char text[32]; snprintf(text,sizeof(text),"Note %zu",i); CHECK(cells_set(&s->cells,i,9,text,&error)); }
        if (i%3!=0) { value.flags=i%2 ? FORMAT_BOLD : FORMAT_ITALIC; CHECK(format_apply(&s->formats,(FormatRange){i,i,2,2},FORMAT_BOLD|FORMAT_ITALIC,value,&error)); }
        worksheet_record(s,i%3==0 ? EDIT_CELLS : i%3==1 ? EDIT_FORMAT : EDIT_CELLS_FORMAT); legacy_history_accept(s);
    }
    CHECK(s->undo_actions_count==DX_HISTORY);
    for (size_t i=0;i<DX_HISTORY/2;i++) { format_model_history(s,false); legacy_history_accept(s); }
    CHECK(s->undo_actions_count==DX_HISTORY/2 && s->redo_actions_count==DX_HISTORY/2);
    while (s->redo_actions_count) { format_model_history(s,true); legacy_history_accept(s); }
    workbook_free(&book); free(error);
}
static void manual_history_accept(const Worksheet *sheet,const Analysis *live) {
    Worksheet before=*sheet; Analysis prior=*live; char *error=dx_strdup("Keep manual diagnostic"),*owned=error;
    CHECK(worksheet_manual_format_history_eligible(sheet,live,&error) && error==owned && !strcmp(error,"Keep manual diagnostic"));
    CHECK(!memcmp(sheet,&before,sizeof(before)) && !memcmp(live,&prior,sizeof(prior)));
    CHECK(worksheet_manual_format_history_eligible(sheet,live,NULL)); free(error);
}
static void manual_history_reject(const Worksheet *sheet,const Analysis *live,const char *message) {
    Worksheet before=*sheet; Analysis prior=*live; char *error=dx_strdup("Replace manual diagnostic");
    CHECK(!worksheet_manual_format_history_eligible(sheet,live,&error) && error && strstr(error,message));
    CHECK(!memcmp(sheet,&before,sizeof(before)) && !memcmp(live,&prior,sizeof(prior)));
    CHECK(!worksheet_manual_format_history_eligible(sheet,live,NULL)); free(error);
}
static void test_manual_format_history(void) {
    Workbook book={0}; Analysis blank={0},live={0}; char *error=NULL; CHECK(workbook_add(&book,"Manual template history",&blank,&error));
    Worksheet *s=book.sheets; manual_history_accept(s,&live); manual_history_accept(s,&s->analysis);
    CellFormat value=format_default(); value.flags=FORMAT_BOLD;
    CHECK(format_apply(&s->formats,(FormatRange){2,9,0,0},FORMAT_BOLD,value,&error)); worksheet_record(s,EDIT_FORMAT);
    CHECK(cells_set(&s->cells,3,0,"Keep NULL literal",&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(objects_add(&s->objects,20,6,"Blank pivot",&error)==0); worksheet_record(s,EDIT_OBJECTS);
    PivotDefinition empty={0}; CHECK(objects_add_chart(&s->objects,40,8,"Blank chart",&empty,&error)==1); worksheet_record(s,EDIT_OBJECTS);
    Analysis next=analysis_copy(&live); next.descending=true; journal_analysis(s,&live,&next);
    CHECK(cells_set(&s->cells,4,0,"Range cell",&error)); objects_translate_mask(&s->objects,1,1,0); worksheet_record(s,EDIT_RANGE);
    value.color=2; CHECK(cells_set(&s->cells,5,0,"Cell-format",&error)); CHECK(format_apply(&s->formats,(FormatRange){5,5,0,0},FORMAT_COLOR,value,&error)); worksheet_record(s,EDIT_CELLS_FORMAT);
    value.size=16; CHECK(objects_rename(&s->objects,0,"Renamed blank pivot",&error)); CHECK(format_apply(&s->formats,(FormatRange){6,6,0,0},FORMAT_SIZE,value,&error)); worksheet_record(s,EDIT_OBJECTS_FORMAT);
    value.flags=FORMAT_ITALIC; CHECK(cells_set(&s->cells,7,0,"Range-format",&error)); objects_translate_mask(&s->objects,2,1,0);
    CHECK(format_apply(&s->formats,(FormatRange){7,7,0,0},FORMAT_ITALIC,value,&error)); worksheet_record(s,EDIT_RANGE_FORMAT);
    next=analysis_copy(&live); next.aggregate=AGG_COUNT; journal_composite(s,&live,&next);
    CHECK(s->undo_actions_count==10 && s->undo_count==2 && s->objects.undo_count==5 && s->cells.undo_count==4 && s->formats.undo_count==5);
    CHECK(s->formats.source_aware && !s->formats.undo[0].source_aware && s->analysis.kind==SOURCE_NONE && live.kind==SOURCE_NONE);
    manual_history_accept(s,&live);
    while (s->undo_actions_count) { journal_history(s,&live,false); manual_history_accept(s,&live); }
    CHECK(!s->objects.count && s->objects.items && !s->cells.count && s->cells.cells && !s->formats.source_aware);
    while (s->redo_actions_count) { journal_history(s,&live,true); manual_history_accept(s,&live); }
    CHECK(s->objects.count==2 && !strcmp(s->objects.items[0].name,"Renamed blank pivot") && !strcmp(cells_get(&s->cells,3,0),"Keep NULL literal"));
    /* A failed prospective first import does not authorize any mode change. */
    FormatStore before=s->formats; ObjectStore objects=s->objects;
    CHECK(objects_add_table(&s->objects,0,13,"Invalid first table",&blank,&error)<0 && error); free(error); error=NULL;
    CHECK(!memcmp(&before,&s->formats,sizeof(before)) && !memcmp(&objects,&s->objects,sizeof(objects))); manual_history_accept(s,&live);
    /* Exercise all legacy/aware retained modes without reinterpreting them. */
    s->formats.revision=UINT64_MAX; manual_history_reject(s,&live,"exhausted format revision"); s->formats.revision=19;
    for (size_t i=0;i<s->formats.undo_count;i++) s->formats.undo[i].source_aware=true;
    s->formats.revision=UINT64_MAX; manual_history_accept(s,&live); s->formats.revision=19;
    /* Independent App live and save-cache authorities must both stay blank. */
    for (int kind=SOURCE_CSV;kind<=SOURCE_DEMO;kind++) {
        live.kind=(SourceKind)kind; manual_history_reject(s,&live,"current data source"); live.kind=SOURCE_NONE;
        s->analysis.kind=(SourceKind)kind; manual_history_reject(s,&live,"current data source"); s->analysis.kind=SOURCE_NONE;
        s->undo[0].kind=(SourceKind)kind; manual_history_reject(s,&live,"Retained data-source history"); s->undo[0].kind=SOURCE_NONE;
    }
    /* Validate shallow malformed views without transferring their ownership. */
    Worksheet invalid=*s;
    invalid.undo_actions_count=SIZE_MAX; manual_history_reject(&invalid,&live,"bounded worksheet history");
    invalid=*s; invalid.objects.redo_count=DX_HISTORY+1; manual_history_reject(&invalid,&live,"bounded worksheet history");
    invalid=*s; invalid.cells.undo_count=0; manual_history_reject(&invalid,&live,"not aligned");
    invalid=*s; invalid.formats.redo_count=1; manual_history_reject(&invalid,&live,"not aligned");
    invalid=*s; invalid.undo_actions[0]=(EditKind)99; manual_history_reject(&invalid,&live,"Unknown worksheet history");
    invalid=*s; invalid.objects.count=DX_MAX_OBJECTS+1; manual_history_reject(&invalid,&live,"independent");
    invalid=*s; invalid.objects.items=NULL; manual_history_reject(&invalid,&live,"independent");
    invalid=*s; invalid.objects.undo[1].items=NULL; manual_history_reject(&invalid,&live,"independent");
    invalid=*s; invalid.cells.capacity=0; manual_history_reject(&invalid,&live,"editable-cell array shape");
    invalid=*s; invalid.cells.cells=NULL; manual_history_reject(&invalid,&live,"editable-cell array shape");
    invalid=*s; invalid.cells.bytes=DX_CELL_BYTES+1; manual_history_reject(&invalid,&live,"editable-cell array shape");
    invalid=*s; invalid.cells.undo[1].count=DX_MAX_EDIT_CELLS+1; manual_history_reject(&invalid,&live,"retained editable-cell array shape");
    invalid=*s; invalid.cells.undo[1].cells=NULL; manual_history_reject(&invalid,&live,"retained editable-cell array shape");
    invalid=*s; invalid.formats.count=DX_FORMAT_PATCHES+1; manual_history_reject(&invalid,&live,"invalid current or retained cell formats");
    unsigned mask=s->formats.undo[1].items[0].mask; s->formats.undo[1].items[0].mask=0;
    manual_history_reject(s,&live,"invalid current or retained cell formats"); s->formats.undo[1].items[0].mask=mask;
    SheetObject *pivot=s->objects.items; uint64_t id=pivot->id; pivot->id=s->objects.next_id+1;
    manual_history_reject(s,&live,"high-water"); pivot->id=id;
    pivot->source_id=91; manual_history_reject(s,&live,"source table binding"); pivot->source_id=0;
    pivot->source.kind=SOURCE_DEMO; manual_history_reject(s,&live,"source table binding"); pivot->source.kind=SOURCE_NONE;
    pivot->pivot.counts[PIVOT_ROWS]=1; pivot->pivot.fields[PIVOT_ROWS][0].name=dx_strdup("");
    manual_history_reject(s,&live,"Invalid sheet object definition"); pivot_free(&pivot->pivot);
    pivot->pivot.fields[PIVOT_ROWS][7].name=dx_strdup("Inactive owned field"); manual_history_reject(s,&live,"invalid owned object state"); free(pivot->pivot.fields[PIVOT_ROWS][7].name); pivot->pivot.fields[PIVOT_ROWS][7].name=NULL;
    /* Each retained blank-primary definition is valid by itself, but the
       existing per-stack logical bound still applies without reserved slack. */
    for (size_t i=1;i<=2;i++) {
        PivotDefinition *p=&s->objects.undo[i].items[0].pivot; CHECK(!p->counts[PIVOT_FILTERS]);
        char *text=malloc(4200001); CHECK(text); memset(text,'x',4200000); text[4200000]=0;
        p->counts[PIVOT_FILTERS]=1; p->fields[PIVOT_FILTERS][0]=(PivotField){.name=dx_strdup("City"),.value=text,.filter=FILTER_CONTAINS};
    }
    manual_history_reject(s,&live,"existing object-history limit");
    for (size_t i=1;i<=2;i++) pivot_free(&s->objects.undo[i].items[0].pivot);
    CHECK(!worksheet_manual_format_history_eligible(NULL,&live,&error) && error && strstr(error,"live Analysis")); free(error); error=NULL;
    CHECK(!worksheet_manual_format_history_eligible(s,NULL,&error) && error && strstr(error,"live Analysis")); free(error); error=NULL;
    CHECK(!worksheet_manual_format_history_eligible(s,s->undo,&error) && error && strstr(error,"independent")); free(error); error=NULL;
    manual_history_accept(s,&live); analysis_free(&live); workbook_free(&book);
}
static void test_manual_format_retained_sources(void) {
    Workbook book={0}; Analysis blank={0},live={0}; char *error=NULL; CHECK(workbook_add(&book,"Prior imported owners",&blank,&error)); Worksheet *s=book.sheets;
    Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("/private/existing-input.csv")};
    Analysis next=analysis_copy(&source); journal_analysis(s,&live,&next); next=(Analysis){0}; journal_analysis(s,&live,&next);
    CHECK(live.kind==SOURCE_NONE && s->undo_count==2 && s->undo[1].kind==SOURCE_CSV); manual_history_reject(s,&live,"Retained data-source history");
    journal_history(s,&live,false); manual_history_reject(s,&live,"current data source");
    journal_history(s,&live,false); CHECK(live.kind==SOURCE_NONE && s->redo_count==2 && s->redo[1].kind==SOURCE_CSV); manual_history_reject(s,&live,"Retained data-source history");
    analysis_free(&live); workbook_free(&book);
    CHECK(workbook_add(&book,"Removed table history",&blank,&error)); s=book.sheets;
    CHECK(objects_add(&s->objects,20,0,"Blank template",&error)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add_table(&s->objects,0,13,"Prior actual table",&source,&error)==1); worksheet_record(s,EDIT_OBJECTS); manual_history_reject(s,&s->analysis,"retained Tables");
    CHECK(objects_remove(&s->objects,1)); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_rename(&s->objects,0,"Still blank",&error)); worksheet_record(s,EDIT_OBJECTS);
    CHECK(s->objects.count==1 && s->objects.undo[2].count==2 && s->objects.undo[3].count==1); manual_history_reject(s,&s->analysis,"retained Tables");
    while (s->undo_actions_count) journal_history(s,&s->analysis,false);
    CHECK(!s->objects.count && s->objects.redo_count==4); manual_history_reject(s,&s->analysis,"retained Tables");
    /* Removing all history, rather than just undoing/deleting the Table, is a
       caller operation: eligibility neither clears it nor infers owner origin. */
    objects_trim_history(&s->objects,true,0); s->redo_actions_count=0; manual_history_accept(s,&s->analysis);
    analysis_free(&source); workbook_free(&book); free(error);
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--manual-format-history-only")) {
        test_manual_format_history(); test_manual_format_retained_sources();
        printf("PASS: %u focused pre-import manual-format history checks (blank primary/template owners across both aligned journals, retained source rejection, bounded models and unchanged ownership)\n",checks); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--legacy-format-history-only")) {
        test_legacy_format_history();
        printf("PASS: %u focused legacy-format history eligibility checks (both aligned Cell/Format journals, retained aware states, bounded shapes, atomic rejection and unchanged ownership)\n",checks); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--guarded-formats-only")) {
        test_guarded_format_workspace(); test_guarded_format_rejection(); test_source_format_workspace();
        printf("PASS: %u focused typed guarded-v14 codec checks (manual rectangles,1..33 exact RESULT owners,archived proofs,deep ownership and transactional rejection)\n",checks); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--format-budget-only")) {
        test_format_budget_models(); test_format_budget_analysis_journal(); test_format_budget_analysis_rejection(); test_format_budget_objects(); test_format_budget_object_rejection(); test_format_budget_object_legacy_slack(); test_format_budget_combined_sheet(); test_format_budget_policy(); test_format_budget_legacy_measurement();
        printf("PASS: %u focused formatting-accounting checks (dormant ledgers, complete projected histories, explicit staging/reservations, exact caps and atomic failure)\n",checks); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--journal-only")) {
        test_analysis_journal_owned(); test_analysis_journal_rejection(); test_analysis_journal_chronology();
        printf("PASS: %u focused owned Analysis/format journal checks (pointer moves, composite chronology, budgets, aliases, cancellation and high-water)\n",checks); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--source-formats-only")) {
        test_source_format_workspace(); test_invalid_source_format_package(); test_source_format_budget();
        printf("PASS: %u focused v14 source-format codec checks (dormant typed scopes, source ledgers, exact uint64, budgets and transactional rejection)\n",checks); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--source-versions-only")) {
        test_source_version_model(); test_source_version_save_guard();
        printf("PASS: %u focused source-version model checks (deep ownership, Analysis history, lifetime high-water, v14 archival and dangling-binding rejection)\n",checks); return 0;
    }
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
    if (argc!=1) { fprintf(stderr,"Usage: %s [--formats-only|--owners-only|--source-versions-only|--source-formats-only|--journal-only|--format-budget-only|--guarded-formats-only|--legacy-format-history-only|--manual-format-history-only]\n",argv[0]); return 2; }
    test_manual_format_history(); test_manual_format_retained_sources();
    test_legacy_format_history();
    test_guarded_format_workspace(); test_guarded_format_rejection();
    test_format_budget_models(); test_format_budget_analysis_journal(); test_format_budget_analysis_rejection(); test_format_budget_objects(); test_format_budget_object_rejection(); test_format_budget_object_legacy_slack(); test_format_budget_combined_sheet(); test_format_budget_policy(); test_format_budget_legacy_measurement();
    test_analysis_journal_owned(); test_analysis_journal_rejection(); test_analysis_journal_chronology();
    test_source_version_model(); test_source_version_save_guard();
    test_source_format_workspace(); test_invalid_source_format_package(); test_source_format_budget();
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
