#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/core_payload.h"
#include "platform/platform.h"
#include "platform/directory.h"
#include "platform/file_lease.h"
#include <duckdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sealed core:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void clear_error(char **error) { CHECK(*error); free(*error); *error=NULL; }
static size_t children(const char *path) {
    DxDirectory *d=NULL; CHECK(dx_directory_open(path,&d,NULL)==DX_DIRECTORY_OPEN);
    DxDirectoryEntry entry={0}; DxDirectoryNext next; size_t count=0;
    while ((next=dx_directory_next(d,&entry,NULL))==DX_DIRECTORY_ENTRY) count++;
    CHECK(next==DX_DIRECTORY_END); dx_directory_close(d); return count;
}
typedef struct { uint64_t after,last; unsigned calls; } Cancel;
static bool cancel(void *context,uint64_t bytes) {
    Cancel *c=context; CHECK(!c->calls || bytes>=c->last); c->last=bytes; c->calls++;
    return bytes>=c->after;
}
static bool cancel_write(void *context,WorkbookSaveProgress progress) {
    (void)context; return progress.phase!=SAVE_PHASE_PUBLISH;
}
static void values(const WorkbookCorePayload *p,WorkbookId id) {
    Workbook loaded; workbook_init(&loaded); char *error=NULL;
    CHECK(workbook_load(&loaded,workbook_core_payload_path(p),&error) && !error);
    CHECK(loaded.id.high==id.high && loaded.id.low==id.low && loaded.count==1);
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,0,0),"9007199254740993.0123456789"));
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,1,0),"NULL"));
    CHECK(!strcmp(loaded.sheets[0].analysis.path,"/offline/source.parquet"));
    workbook_free(&loaded);
    duckdb_config config=NULL; duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0};
    CHECK(duckdb_create_config(&config)==DuckDBSuccess);
    CHECK(duckdb_set_config(config,"access_mode","READ_ONLY")==DuckDBSuccess);
    CHECK(duckdb_open_ext(workbook_core_payload_path(p),&db,config,NULL)==DuckDBSuccess);
    duckdb_destroy_config(&config); CHECK(duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name LIKE 'version_%'",&r)==DuckDBSuccess);
    CHECK(duckdb_value_uint64(&r,0,0)==0); /* A core leaf never copies a version archive into itself. */
    duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
static void write_byte(const char *path) {
    FILE *f=fopen(path,"wb"); CHECK(f && fputc('x',f)!=EOF && !fclose(f));
}
static void same_info(WorkbookCorePayloadInfo a,const WorkbookCorePayloadInfo *b) {
    CHECK(b && a.document.high==b->document.high && a.document.low==b->document.low && a.encoding==b->encoding && a.bytes==b->bytes && !memcmp(a.sha256,b->sha256,DX_SOURCE_DIGEST_BYTES));
}
int main(void) {
    char parent[]="build/tests/core-payload-XXXXXX"; CHECK(mkdtemp(parent));
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Sealed snapshot",&(Analysis){.kind=SOURCE_PARQUET,.path="/offline/source.parquet"},&error));
    CHECK(cells_set(&book.sheets[0].cells,0,0,"9007199254740993.0123456789",&error));
    CHECK(cells_set(&book.sheets[0].cells,1,0,"NULL",&error)); book.dirty=true;
    WorkbookCorePayload *p=NULL,*other=NULL;
#if !defined(__APPLE__) || defined(__EMSCRIPTEN__)
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&p,&error)==CORE_PAYLOAD_UNSUPPORTED && !p);
    clear_error(&error); CHECK(!children(parent));
#else
    CHECK(workbook_core_payload_capture(NULL,parent,(WorkbookCorePayloadControl){0},&p,&error)==CORE_PAYLOAD_FAILED && !p);
    clear_error(&error); CHECK(!children(parent));
    CHECK(workbook_core_payload_capture(&book,"/does-not-exist/payload",(WorkbookCorePayloadControl){0},&p,&error)==CORE_PAYLOAD_FAILED && !p);
    clear_error(&error); CHECK(!children(parent));
    Cancel stop={0};
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){.cancelled=cancel,.context=&stop},&p,&error)==CORE_PAYLOAD_CANCELLED && !p);
    clear_error(&error); CHECK(stop.calls && !stop.last && !children(parent));
    stop=(Cancel){.after=DX_SOURCE_DIGEST_BUFFER_BYTES};
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){.cancelled=cancel,.context=&stop},&p,&error)==CORE_PAYLOAD_CANCELLED && !p);
    clear_error(&error); CHECK(stop.calls>=2 && stop.last==DX_SOURCE_DIGEST_BUFFER_BYTES && !children(parent));
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){.save={cancel_write,NULL}},&p,&error)==CORE_PAYLOAD_CANCELLED && !p);
    clear_error(&error); CHECK(!children(parent));
    char *original=book.sheets[0].analysis.path; book.sheets[0].analysis.path="unresolved.parquet";
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&p,&error)==CORE_PAYLOAD_FAILED && !p);
    book.sheets[0].analysis.path=original; clear_error(&error); CHECK(!children(parent));
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&p,&error)==CORE_PAYLOAD_READY && p && !error);
    CHECK(children(parent)==1 && children(workbook_core_payload_directory(p))==2);
    WorkbookCorePayloadInfo info=*workbook_core_payload_info(p);
    CHECK(info.encoding==15 && info.bytes && info.document.high==book.id.high && info.document.low==book.id.low);
    SourceDigest digest={0}; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    CHECK(source_digest_file(workbook_core_payload_path(p),NULL,NULL,&digest,detail)==SOURCE_DIGEST_OK);
    CHECK(info.bytes==digest.bytes && !memcmp(info.sha256,digest.sha256,DX_SOURCE_DIGEST_BYTES));
    CHECK(book.dirty && !book.path && book.sheets[0].analysis.path==original);
    char *directory=dx_strdup(workbook_core_payload_directory(p)),*path=dx_strdup(workbook_core_payload_path(p));
    char *lease=dx_printf("%s/lease.lock",directory); DxFileLease *claim=NULL;
    CHECK(dx_file_lease_acquire(lease,false,&claim,&error)==DX_FILE_LEASE_BUSY && !claim && !error);
    values(p,book.id); CHECK(workbook_core_payload_verify(p,NULL,NULL,&error)==CORE_PAYLOAD_READY && !error);
    stop=(Cancel){0};
    CHECK(workbook_core_payload_verify(p,cancel,&stop,&error)==CORE_PAYLOAD_CANCELLED); clear_error(&error);
    same_info(info,workbook_core_payload_info(p)); CHECK(!access(path,F_OK));
    CHECK(workbook_core_payload_verify(p,NULL,NULL,&error)==CORE_PAYLOAD_READY);
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&p,&error)==CORE_PAYLOAD_FAILED);
    clear_error(&error); CHECK(children(parent)==1); same_info(info,workbook_core_payload_info(p));
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&other,&error)==CORE_PAYLOAD_READY);
    CHECK(strcmp(directory,workbook_core_payload_directory(other)) && children(parent)==2);
    CHECK(cells_set(&book.sheets[0].cells,0,0,"later working edit",&error));
    values(p,book.id); values(other,book.id);
    workbook_core_payload_free(other); other=NULL; CHECK(children(parent)==1);
    char *extra=dx_printf("%s/unexpected",directory); write_byte(extra);
    CHECK(!workbook_core_payload_retire(p,&error)); clear_error(&error);
    CHECK(!access(path,F_OK) && !access(lease,F_OK) && !access(extra,F_OK));
    CHECK(workbook_core_payload_verify(p,NULL,NULL,&error)==CORE_PAYLOAD_FAILED); clear_error(&error);
    CHECK(!unlink(extra));
    CHECK(workbook_core_payload_retire(p,&error) && !error);
    CHECK(workbook_core_payload_retire(p,&error) && !workbook_core_payload_info(p) && !workbook_core_payload_path(p));
    CHECK(!workbook_core_payload_directory(p) && !children(parent));
    workbook_core_payload_free(p); p=NULL; free(path); free(directory); free(lease); free(extra);
    /* A final-component alias or modified artifact is never deleted on free. */
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&p,&error)==CORE_PAYLOAD_READY);
    directory=dx_strdup(workbook_core_payload_directory(p)); path=dx_strdup(workbook_core_payload_path(p));
    lease=dx_printf("%s/lease.lock",directory); char *renamed=dx_printf("%s/moved-core",directory);
    CHECK(!rename(path,renamed) && !symlink("moved-core",path));
    CHECK(workbook_core_payload_verify(p,NULL,NULL,&error)==CORE_PAYLOAD_FAILED); clear_error(&error);
    CHECK(!workbook_core_payload_retire(p,&error)); clear_error(&error);
    workbook_core_payload_free(p); p=NULL;
    CHECK(!access(renamed,F_OK) && !access(lease,F_OK));
    CHECK(dx_file_lease_acquire(lease,false,&claim,&error)==DX_FILE_LEASE_ACQUIRED); dx_file_lease_release(claim); claim=NULL;
    CHECK(!unlink(path) && !unlink(renamed) && !unlink(lease) && !rmdir(directory));
    free(path); free(directory); free(lease); free(renamed); CHECK(!children(parent));
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&p,&error)==CORE_PAYLOAD_READY);
    directory=dx_strdup(workbook_core_payload_directory(p)); path=dx_strdup(workbook_core_payload_path(p));
    lease=dx_printf("%s/lease.lock",directory);
    FILE *changed=fopen(path,"r+b"); CHECK(changed && !fseek(changed,-1,SEEK_END));
    int last=fgetc(changed); CHECK(last!=EOF && !fseek(changed,-1,SEEK_END));
    CHECK(fputc(last^1,changed)!=EOF && !fclose(changed));
    CHECK(workbook_core_payload_verify(p,NULL,NULL,&error)==CORE_PAYLOAD_FAILED); clear_error(&error);
    CHECK(!workbook_core_payload_retire(p,&error)); clear_error(&error);
    workbook_core_payload_free(p); p=NULL; CHECK(!access(path,F_OK) && !access(lease,F_OK));
    CHECK(!unlink(path) && !unlink(lease) && !rmdir(directory));
    free(path); free(directory); free(lease); CHECK(!children(parent));
#endif
    CHECK(!workbook_core_payload_info(NULL) && !workbook_core_payload_path(NULL) && !workbook_core_payload_directory(NULL));
    CHECK(workbook_core_payload_verify(NULL,NULL,NULL,&error)==CORE_PAYLOAD_FAILED); clear_error(&error);
    CHECK(!workbook_core_payload_retire(NULL,&error)); clear_error(&error); workbook_core_payload_free(NULL);
    workbook_free(&book); CHECK(!rmdir(parent));
    printf("PASS: %u sealed core payload checks\n",checks); return 0;
}
