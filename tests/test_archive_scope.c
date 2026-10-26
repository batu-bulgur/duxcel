#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/archive_read.h"
#include "support/current_archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL archive scope:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { const WorkbookCorePayload *foreign; unsigned calls; bool reject,cancel,in_scope,mismatch; } Probe;
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
static void sql(duckdb_connection c,const char *text) {
    duckdb_result r={0}; CHECK(duckdb_query(c,text,&r)==DuckDBSuccess); duckdb_destroy_result(&r);
}
static bool stop(void *context,WorkbookArchiveReadPhase phase,size_t root,uint64_t bytes) {
    (void)bytes; Probe *p=context; return p->in_scope && p->cancel && phase==ARCHIVE_READ_ADMIT && root==20;
}
static bool admit(void *context,WorkbookArchiveReadScope *scope,duckdb_connection c,const Workbook *book,const WorkbookVersionCoreSet *versions,char **error) {
    Probe *p=context; p->calls++; p->in_scope=true;
    const WorkbookCorePayload *leaf=workbook_version_core_set_leaf(versions,0);
    CHECK(leaf && !workbook_archive_scope_target(scope,c,book,versions));
    Workbook fake=*book;
    CHECK(!workbook_archive_scope_associate(scope,c,&fake,versions,leaf,DX_ARCHIVE_SCOPE_COMPARE_BYTES,error)); clear(error);
    CHECK(!workbook_archive_scope_associate(scope,c,book,versions,p->foreign,DX_ARCHIVE_SCOPE_COMPARE_BYTES,error)); clear(error);
    CHECK(!workbook_archive_scope_associate(scope,c,book,versions,leaf,DX_ARCHIVE_SCOPE_COMPARE_BYTES-1,error)); clear(error);
    CHECK(!workbook_archive_scope_target(scope,c,book,versions));
    bool ready=workbook_archive_scope_associate(scope,c,book,versions,leaf,DX_ARCHIVE_SCOPE_COMPARE_BYTES,error);
    if (p->mismatch || p->cancel) { CHECK(!ready && *error && !workbook_archive_scope_target(scope,c,book,versions)); p->in_scope=false; return false; }
    if (!ready) fprintf(stderr,"Scope detail: %s\n",*error ? *error : "none");
    CHECK(ready && !*error && workbook_archive_scope_target(scope,c,book,versions)==leaf);
    CHECK(!workbook_archive_scope_target(scope,NULL,book,versions) && !workbook_archive_scope_target(scope,c,&fake,versions));
    CHECK(!workbook_archive_scope_target(scope,c,book,NULL));
    CHECK(workbook_archive_scope_recheck(scope,c,book,versions,error));
    CHECK(workbook_archive_scope_associate(scope,c,book,versions,leaf,DX_ARCHIVE_SCOPE_COMPARE_BYTES,error));
    /* The read-only attachment stays pinned until the ARCHIVE OWNER ends and
       closes this transaction. The callback must not detach/commit/rollback. */
    duckdb_result r={0}; CHECK(duckdb_query(c,"SELECT count(*) FROM duckdb_databases() WHERE database_name='dx_other'",&r)==DuckDBSuccess);
    CHECK(duckdb_value_uint64(&r,0,0)==1); duckdb_destroy_result(&r);
    sql(c,"SELECT * FROM _duxcel_workspace.workbook_identity");
    p->in_scope=false; return !p->reject;
}
int main(void) {
    char parent[]="build/tests/archive-scope-XXXXXX"; CHECK(mkdtemp(parent)); char *path=dx_printf("%s/current.dxls",parent),*error=NULL;
    Workbook book; workbook_init(&book); CHECK(workbook_add(&book,"Scoped",&(Analysis){.kind=SOURCE_PARQUET,.path="/offline/never-query.parquet"},&error));
    CHECK(cells_set(&book.sheets[0].cells,5,4,"NULL",&error));
    WorkbookCorePayload *foreign=NULL;
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&foreign,&error)==CORE_PAYLOAD_READY);
    CHECK(test_current_archive_save(&book,path,&error));
    WorkbookArchiveCandidate *out=NULL; Probe p={.foreign=foreign};
    WorkbookArchiveReadControl control={.cancelled=stop,.context=&p,.admit=admit};
    CHECK(workbook_archive_read(path,parent,128u*1024u*1024u,control,&out,&error)==ARCHIVE_READ_READY && out && !error && p.calls==1);
    CHECK(!strcmp(cells_get(&workbook_archive_book(out)->sheets[0].cells,5,4),"NULL")); workbook_archive_candidate_free(out); out=NULL;
    p.reject=true;
    CHECK(workbook_archive_read(path,parent,128u*1024u*1024u,control,&out,&error)==ARCHIVE_READ_FAILED && !out); clear(&error); p.reject=false;
    p.cancel=true;
    CHECK(workbook_archive_read(path,parent,128u*1024u*1024u,control,&out,&error)==ARCHIVE_READ_CANCELLED && !out); clear(&error); p.cancel=false;
    /* Cancellation/failure closed its connection and attachments: the same
       artifact can immediately be admitted again without residual ownership. */
    CHECK(workbook_archive_read(path,parent,128u*1024u*1024u,control,&out,&error)==ARCHIVE_READ_READY && out && !error); workbook_archive_candidate_free(out); out=NULL;
    duckdb_database db=NULL; duckdb_connection c=NULL;
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    sql(c,"UPDATE _duxcel_workspace.meta SET user_scale=1.25"); duckdb_disconnect(&c); duckdb_close(&db); p.mismatch=true;
    CHECK(workbook_archive_read(path,parent,128u*1024u*1024u,control,&out,&error)==ARCHIVE_READ_FAILED && !out); clear(&error);
    /* Plain admission still supports current checkpoints whose body differs
       from Saved; the optional component association must not infer Saved. */
    CHECK(workbook_archive_read(path,parent,128u*1024u*1024u,(WorkbookArchiveReadControl){0},&out,&error)==ARCHIVE_READ_READY && out);
    CHECK(workbook_archive_book(out)->user_scale==1.25f); workbook_archive_candidate_free(out);
    CHECK(!workbook_archive_scope_target(NULL,NULL,NULL,NULL));
    workbook_core_payload_free(foreign); workbook_free(&book); CHECK(!unlink(path) && !rmdir(parent)); free(path); free(error);
    printf("PASS: %u actual archive-scoped target association checks\n",checks); return 0;
}
