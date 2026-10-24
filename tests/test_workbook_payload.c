#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/save.h"
#include "platform/platform.h"
#include <duckdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL core payload:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { WorkbookSavePhase phase; WorkbookSaveProgress last; unsigned calls; bool cancel,complete; } Progress;
static bool update(void *context,WorkbookSaveProgress progress) {
    Progress *p=context;
    CHECK(progress.total && progress.completed<=progress.total);
    CHECK(!p->calls || (progress.completed>=p->last.completed && progress.phase>=p->last.phase && progress.total==p->last.total));
    CHECK((progress.completed==progress.total)==(progress.phase==SAVE_PHASE_COMPLETE));
    p->calls++; p->last=progress; p->complete=progress.phase==SAVE_PHASE_COMPLETE;
    return !p->cancel || progress.phase!=p->phase;
}
static void clear_error(char **error) { CHECK(*error); free(*error); *error=NULL; }
static void reference(const char *path,const char *query,const char *expected) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0};
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,query,&r)==DuckDBSuccess && duckdb_row_count(&r)==1);
    char *text=duckdb_value_varchar(&r,0,0); CHECK(text && !strcmp(text,expected));
    duckdb_free(text); duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
static void verify(const char *path,const Workbook *book,const char *primary,const char *table,const char *offline) {
    Workbook loaded; workbook_init(&loaded); char *error=NULL;
    CHECK(workbook_load(&loaded,path,&error) && !error);
    CHECK(loaded.count==2 && loaded.id.high==book->id.high && loaded.id.low==book->id.low);
    CHECK(loaded.sheets[0].id==book->sheets[0].id && loaded.sheets[1].id==book->sheets[1].id);
    CHECK(loaded.command_revision==book->command_revision);
    const Worksheet *s=loaded.sheets;
    CHECK(!strcmp(s->analysis.path,primary) && s->analysis.source_version==1);
    CHECK(s->objects.count==1 && !strcmp(s->objects.items[0].source.path,table));
    CHECK(s->source_versions.count==1 && !strcmp(s->source_versions.items[0].path,primary));
    CHECK(!strcmp(loaded.sheets[1].analysis.path,offline));
    CHECK(!strcmp(cells_get(&s->cells,50,30),"9007199254740993.0123456789"));
    CHECK(!strcmp(cells_get(&s->cells,51,30),""));
    CHECK(!strcmp(cells_get(&s->cells,52,30),"NULL"));
    CHECK(!*cells_get(&s->cells,53,30) && s->cells.count==2);
    workbook_free(&loaded);
}
static void unresolved(Workbook *book,char **locator,const char *existing,const char *fresh) {
    char *before=*locator,*error=NULL; *locator="tests/fixtures/transactions.csv";
    SourceVersions *versions=&book->sheets[0].source_versions;
    size_t bytes=versions->bytes;
    if (locator==&versions->items[0].path) versions->bytes=bytes-strlen(before)+strlen(*locator);
    for (unsigned i=0;i<2;i++) {
        CHECK(workbook_save_core_payload(book,i ? fresh : existing,(WorkbookSaveControl){0},&error)==WORKBOOK_SAVE_FAILED);
        CHECK(error && strstr(error,"resolved absolute")); clear_error(&error);
        CHECK(access(fresh,F_OK));
        FILE *f=fopen(existing,"rb"); CHECK(f); char bytes[8]={0};
        CHECK(fread(bytes,1,8,f)==8 && !memcmp(bytes,"sentinel",8)); CHECK(fgetc(f)==EOF && !fclose(f));
    }
    *locator=before;
    versions->bytes=bytes;
}
int main(void) {
    char directory[]="build/tests/workbook-payload-XXXXXX"; CHECK(mkdtemp(directory));
    char *a=dx_printf("%s/capture",directory),*b=dx_printf("%s/relocated",directory);
    CHECK(!mkdir(a,0700) && !mkdir(b,0700));
    char *core=dx_printf("%s/core.dxls",a),*moved=dx_printf("%s/core.dxls",b),*ordinary=dx_printf("%s/normal.dxls",a);
    char *sentinel=dx_printf("%s/sentinel",a),*fresh=dx_printf("%s/fresh.dxls",a);
    char *root=dx_absolute_path(directory),*offline=dx_printf("%s/unavailable.parquet",root);
    Analysis primary={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/transactions.csv")};
    Analysis table={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/chart-series.csv")};
    CHECK(primary.path && table.path && root && access(offline,F_OK));
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Connected",&primary,&error));
    CHECK(workbook_add(&book,"Offline",&(Analysis){.kind=SOURCE_PARQUET,.path=offline},&error));
    Worksheet *s=book.sheets;
    CHECK(objects_add_table(&s->objects,0,8,"Table",&table,&error)==0);
    SourceVersion entry={.version=1,.kind=primary.kind,.path=primary.path}; SourceVersions next={0};
    CHECK(source_versions_plan(&s->source_versions,&entry,true,&next,&error));
    s->source_versions=next; s->analysis.source_version=1; s->formats.source_aware=true;
    CHECK(cells_set(&s->cells,50,30,"9007199254740993.0123456789",&error));
    CHECK(cells_set(&s->cells,51,30,"",&error)); CHECK(cells_set(&s->cells,52,30,"NULL",&error));
    book.command_revision=UINT64_C(9007199254740993); book.dirty=true;
    Progress progress={0};
    CHECK(workbook_save_core_payload(&book,core,(WorkbookSaveControl){update,&progress},&error)==WORKBOOK_SAVE_COMPLETE);
    CHECK(!error && progress.complete && book.dirty && !book.path && s->analysis.path!=primary.path);
    const char *primary_sql="SELECT path FROM _duxcel_workspace.sheets WHERE id=0";
    const char *table_sql="SELECT path FROM _duxcel_workspace.table_sources";
    const char *ledger_sql="SELECT source_reference FROM _duxcel_workspace.source_versions";
    reference(core,primary_sql,primary.path); reference(core,table_sql,table.path); reference(core,ledger_sql,primary.path);
    CHECK(workbook_save(&book,ordinary,&error));
    char *absolute_ordinary=workbook_target_path(ordinary);
    char *relative=workbook_relative_path(absolute_ordinary,primary.path),*table_relative=workbook_relative_path(absolute_ordinary,table.path);
    CHECK(relative[0]!='/' && table_relative[0]!='/');
    reference(ordinary,primary_sql,relative); reference(ordinary,table_sql,table_relative); reference(ordinary,ledger_sql,relative);
    char *offline_relative=workbook_relative_path(absolute_ordinary,offline);
    char *offline_resolved=workbook_source_path(absolute_ordinary,offline_relative);
    verify(ordinary,&book,primary.path,table.path,offline_resolved);
    CHECK(!rename(core,moved)); verify(moved,&book,primary.path,table.path,offline);
    reference(moved,primary_sql,primary.path); reference(moved,table_sql,table.path); reference(moved,ledger_sql,primary.path);
    FILE *f=fopen(sentinel,"wb"); CHECK(f && fwrite("sentinel",1,8,f)==8 && !fclose(f));
    /* Unbind while testing locator admission independently of binding equality. */
    s->analysis.source_version=0;
    unresolved(&book,&s->analysis.path,sentinel,fresh);
    unresolved(&book,&s->objects.items[0].source.path,sentinel,fresh);
    unresolved(&book,&s->source_versions.items[0].path,sentinel,fresh);
    s->analysis.source_version=1;
    WorkbookSavePhase phases[]={SAVE_PHASE_PREPARE,SAVE_PHASE_WRITE,SAVE_PHASE_PUBLISH};
    for (size_t i=0;i<sizeof(phases)/sizeof(*phases);i++) for (unsigned exists=0;exists<2;exists++) {
        progress=(Progress){.phase=phases[i],.cancel=true};
        CHECK(workbook_save_core_payload(&book,exists ? moved : fresh,(WorkbookSaveControl){update,&progress},&error)==WORKBOOK_SAVE_CANCELLED);
        clear_error(&error); CHECK(!progress.complete && access(fresh,F_OK));
        verify(moved,&book,primary.path,table.path,offline);
    }
    CHECK(!strcmp(s->analysis.path,primary.path) && !strcmp(s->objects.items[0].source.path,table.path));
    CHECK(book.command_revision==UINT64_C(9007199254740993) && book.dirty && !book.path);
    CHECK(!unlink(moved) && !unlink(ordinary) && !unlink(sentinel) && !rmdir(a) && !rmdir(b) && !rmdir(directory));
    workbook_free(&book); analysis_free(&primary); analysis_free(&table);
    free(a); free(b); free(core); free(moved); free(ordinary); free(sentinel); free(fresh); free(root); free(offline); free(relative); free(table_relative); free(absolute_ordinary); free(offline_relative); free(offline_resolved);
    printf("PASS: %u relocatable core payload checks\n",checks); return 0;
}
