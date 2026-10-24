#define _DARWIN_C_SOURCE
#define _XOPEN_SOURCE 700
#include "workbook/cell_command.h"
#include <duckdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL identity archive:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void sql(const char *path,const char *text) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0};
    CHECK(duckdb_open(path,&db)==DuckDBSuccess); CHECK(duckdb_connect(db,&c)==DuckDBSuccess);
    if (duckdb_query(c,text,&r)!=DuckDBSuccess) { fprintf(stderr,"SQL: %s\n",duckdb_result_error(&r)); exit(1); }
    duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
static unsigned char *image(const char *path,size_t *bytes) {
    FILE *f=fopen(path,"rb"); CHECK(f); CHECK(!fseek(f,0,SEEK_END)); long n=ftell(f); CHECK(n>0); *bytes=(size_t)n;
    CHECK(!fseek(f,0,SEEK_SET)); unsigned char *data=dx_alloc(*bytes); CHECK(fread(data,1,*bytes,f)==*bytes); CHECK(!fclose(f)); return data;
}
static void unchanged(const char *path,const unsigned char *before,size_t bytes) {
    size_t n; unsigned char *after=image(path,&n); CHECK(n==bytes && !memcmp(before,after,n)); free(after);
}
static bool same_id(WorkbookId a,WorkbookId b) { return a.high==b.high && a.low==b.low; }
int main(void) {
    char directory[]="build/tests/identity-XXXXXX"; CHECK(mkdtemp(directory));
    char *path=dx_printf("%s/archive.dxls",directory),*other=dx_printf("%s/other.dxls",directory);
    Workbook book,loaded; workbook_init(&book); workbook_init(&loaded); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"First",&blank,&error)); CHECK(workbook_add(&book,"Second",&blank,&error));
    CHECK(cells_set(&book.sheets[0].cells,0,0,"9007199254740993.0123456789",&error));
    CHECK(cells_set(&book.sheets[0].cells,1,0,"NULL",&error));
    uint64_t ids[]={UINT64_C(9007199254740993),UINT64_C(9007199254740995)};
    CHECK(workbook_identity_restore(&book,(WorkbookId){UINT64_MAX,UINT64_C(9007199254740997)},UINT64_MAX-1,ids,2,&error));
    book.command_revision=UINT64_MAX-2;
    CHECK(workbook_save(&book,path,&error)); CHECK(workbook_load(&loaded,path,&error));
    CHECK(loaded.command_revision==book.command_revision);
    CHECK(same_id(book.id,loaded.id) && loaded.sheet_high_water==book.sheet_high_water);
    CHECK(loaded.sheets[0].id==ids[0] && loaded.sheets[1].id==ids[1]);
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,0,0),"9007199254740993.0123456789"));
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,1,0),"NULL") && !*cells_get(&loaded.sheets[0].cells,2,0));
    CHECK(workbook_rename(&loaded,0,"Renamed",&error)); CHECK(workbook_reorder(&loaded,0,1,&error));
    CHECK(workbook_duplicate(&loaded,1,&error)); CHECK(loaded.sheets[2].id==UINT64_MAX);
    CHECK(workbook_save(&loaded,path,&error)); CHECK(workbook_load(&book,path,&error));
    CHECK(book.sheet_high_water==UINT64_MAX && book.sheets[1].id==ids[0] && book.sheets[2].id==UINT64_MAX);
    CHECK(!workbook_add(&book,"Exhausted",&blank,&error)); free(error); error=NULL;
    CHECK(workbook_save(&book,other,&error));
    /* Unsupported discriminators fail before changing either the caller or file. */
    const unsigned unsupported[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,16,17,999};
    for (size_t i=0;i<sizeof(unsupported)/sizeof(*unsupported);i++) {
        unlink(path); CHECK(workbook_save(&book,path,&error));
        char *change=dx_printf("UPDATE _duxcel_workspace.meta SET version=%u",unsupported[i]); sql(path,change); free(change);
        size_t bytes; unsigned char *before=image(path,&bytes); Workbook live=loaded;
        CHECK(!workbook_load(&loaded,path,&error) && error && !memcmp(&live,&loaded,sizeof(live))); free(error); error=NULL;
        unchanged(path,before,bytes);
        CHECK(!workbook_save(&loaded,path,&error) && error); free(error); error=NULL;
        unchanged(path,before,bytes); free(before);
    }
    const char *bad[]={
        "UPDATE _duxcel_workspace.sheet_identity SET stable_id=0 WHERE sheet_id=0",
        "UPDATE _duxcel_workspace.sheet_identity SET stable_id=1",
        "DELETE FROM _duxcel_workspace.sheet_identity WHERE sheet_id=0",
        "INSERT INTO _duxcel_workspace.sheet_identity SELECT * FROM _duxcel_workspace.sheet_identity LIMIT 1",
        "UPDATE _duxcel_workspace.sheet_identity SET sheet_id=64 WHERE sheet_id=0",
        "UPDATE _duxcel_workspace.sheet_identity SET stable_id=NULL WHERE sheet_id=0",
        "UPDATE _duxcel_workspace.workbook_identity SET high=0,low=0",
        "UPDATE _duxcel_workspace.workbook_identity SET sheet_high_water=0",
        "UPDATE _duxcel_workspace.workbook_identity SET command_revision=NULL",
        "DELETE FROM _duxcel_workspace.workbook_identity",
        "INSERT INTO _duxcel_workspace.workbook_identity SELECT * FROM _duxcel_workspace.workbook_identity",
        "ALTER TABLE _duxcel_workspace.sheet_identity ALTER stable_id TYPE VARCHAR",
        "DROP TABLE _duxcel_workspace.sheet_identity",
        "ALTER TABLE _duxcel_workspace.workbook_identity ADD extra INTEGER",
        "UPDATE _duxcel_workspace.meta SET version=14"
    };
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        unlink(path); CHECK(workbook_save(&loaded,path,&error)); sql(path,bad[i]);
        size_t bytes; unsigned char *before=image(path,&bytes); Workbook live=loaded; Worksheet *first=loaded.sheets;
        CHECK(!workbook_load(&loaded,path,&error)); CHECK(error && !memcmp(&live,&loaded,sizeof(live)) && first==loaded.sheets); free(error); error=NULL;
        unchanged(path,before,bytes);
        CHECK(!workbook_save(&loaded,path,&error)); CHECK(error); free(error); error=NULL; unchanged(path,before,bytes); free(before);
    }
    workbook_free(&book); workbook_free(&loaded); free(error); unlink(path); unlink(other); CHECK(!rmdir(directory)); free(path); free(other);
    printf("PASS: %u native current workbook identity checks\n",checks); return 0;
}
