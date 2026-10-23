#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/journal.h"
#include <duckdb.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL journal:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const WorkbookId document={UINT64_MAX,UINT64_C(9007199254740993)};
static JournalCellEdit edit(uint64_t revision,const char *before,const char *after) {
    return (JournalCellEdit){.operation={UINT64_MAX,revision},.previous_revision=revision-1,.revision=revision,
        .sheet=UINT64_C(9007199254740995),.row=DX_SHEET_ROWS-1,.column=16383,
        .before=before,.after=after,.before_bytes=strlen(before),.after_bytes=strlen(after)};
}
static void sql(const char *path,const char *sql) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0};
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,sql,&r)==DuckDBSuccess); duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
static bool cancel(void *unused) { (void)unused; return false; }
static bool before_hold(void *unused) { (void)unused; puts("BARRIER before-commit"); fflush(stdout); return getchar()!='x'; }
static void after_hold(void *unused) { (void)unused; puts("BARRIER after-commit"); fflush(stdout); (void)getchar(); }
typedef struct { const JournalCellEdit *expected; size_t next; } Read;
static bool read_entry(void *context,const JournalCellEdit *e,char **error) {
    (void)error; Read *read=context; const JournalCellEdit *expected=read->expected+read->next++;
    CHECK(e->operation.high==expected->operation.high && e->operation.low==expected->operation.low);
    CHECK(e->previous_revision==expected->previous_revision && e->revision==expected->revision);
    CHECK(e->sheet==expected->sheet && e->row==expected->row && e->column==expected->column);
    CHECK(e->before_bytes==expected->before_bytes && e->after_bytes==expected->after_bytes);
    CHECK((!e->before_bytes || !memcmp(e->before,expected->before,e->before_bytes)) && (!e->after_bytes || !memcmp(e->after,expected->after,e->after_bytes)));
    return true;
}
static bool count_entry(void *context,const JournalCellEdit *e,char **error) { (void)e; (void)error; (*(size_t *)context)++; return true; }
static void reject(WorkbookJournal *j,const JournalCellEdit *edits,size_t count) {
    JournalReceipt receipt={77,88,99},before=receipt; char *error=NULL;
    uint64_t head=workbook_journal_durable_revision(j);
    CHECK(!workbook_journal_append(j,edits,count,(JournalControl){0},&receipt,&error));
    CHECK(error && !memcmp(&receipt,&before,sizeof(receipt)) && workbook_journal_durable_revision(j)==head); free(error);
}
static void transactions(const char *path) {
    WorkbookJournal *j=NULL; char *error=NULL; JournalReceipt receipt={0};
    CHECK(!workbook_journal_open(&j,":memory:",document,100,&error)); CHECK(!j && error); free(error); error=NULL;
    CHECK(workbook_journal_open(&j,path,document,100,&error)); CHECK(j && !error && workbook_journal_durable_revision(j)==100);
    JournalCellEdit edits[]={edit(101,"","NULL"),edit(102,"NULL","9007199254740993.0123456789"),edit(103,"9007199254740993.0123456789","")};
    CHECK(workbook_journal_append(j,edits,1,(JournalControl){0},&receipt,&error));
    CHECK(receipt.durable_revision==101 && receipt.appended==1 && !receipt.replayed);
    CHECK(workbook_journal_append(j,edits,1,(JournalControl){0},&receipt,&error));
    CHECK(receipt.durable_revision==101 && !receipt.appended && receipt.replayed==1);
    CHECK(workbook_journal_append(j,edits,2,(JournalControl){0},&receipt,&error));
    CHECK(receipt.durable_revision==102 && receipt.appended==1 && receipt.replayed==1);
    JournalCellEdit conflict[]={edits[2],edit(104,"","Changed operation ID")}; conflict[1].operation=edits[0].operation;
    reject(j,conflict,2); /* First INSERT rolls back with the second conflict. */
    size_t visited=0; Read read={edits,0};
    CHECK(workbook_journal_read(j,100,256,read_entry,&read,&visited,&error)); CHECK(read.next==2 && visited==2);
    JournalReceipt before=receipt;
    CHECK(!workbook_journal_append(j,edits+2,1,(JournalControl){.before_commit=cancel},&receipt,&error));
    CHECK(error && !memcmp(&before,&receipt,sizeof(receipt)) && workbook_journal_durable_revision(j)==102); free(error); error=NULL;
    CHECK(workbook_journal_append(j,edits+2,1,(JournalControl){0},&receipt,&error)); CHECK(receipt.durable_revision==103);
    /* Old exact retries acknowledge the actual current head. */
    CHECK(workbook_journal_append(j,edits,1,(JournalControl){0},&receipt,&error)); CHECK(receipt.durable_revision==103 && receipt.replayed==1);
    JournalCellEdit invalid=edit(104,"","Quote '; DROP TABLE duxcel_journal_cells; --\nİstanbul");
    JournalCellEdit saved=invalid; invalid.after="a\0b"; invalid.after_bytes=3; reject(j,&invalid,1); invalid=saved;
    invalid.row=DX_SHEET_ROWS; reject(j,&invalid,1); invalid=saved;
    invalid.column=16384; reject(j,&invalid,1); invalid=saved;
    invalid.sheet=0; reject(j,&invalid,1); invalid=saved;
    invalid.operation=(WorkbookId){0}; reject(j,&invalid,1); invalid=saved;
    invalid.after_bytes=65537; reject(j,&invalid,1); invalid=saved;
    invalid.after=""; invalid.after_bytes=0; reject(j,&invalid,1); invalid=saved;
    invalid.revision++; reject(j,&invalid,1); invalid=saved;
    invalid.previous_revision++; invalid.revision++; reject(j,&invalid,1); invalid=saved;
    reject(j,&invalid,0); reject(j,&invalid,DX_JOURNAL_BATCH+1);
    CHECK(workbook_journal_append(j,&invalid,1,(JournalControl){0},&receipt,&error));
    CHECK(receipt.durable_revision==104 && receipt.appended==1 && !error);
    size_t seen=0; CHECK(workbook_journal_read(j,104,1,count_entry,&seen,&visited,&error)); CHECK(!visited && !seen);
    workbook_journal_close(j); j=NULL;
    CHECK(!workbook_journal_open(&j,path,(WorkbookId){3,4},100,&error)); CHECK(!j && error); free(error); error=NULL;
    CHECK(!workbook_journal_open(&j,path,document,99,&error)); CHECK(!j && error); free(error); error=NULL;
    CHECK(workbook_journal_open(&j,path,document,100,&error)); CHECK(workbook_journal_durable_revision(j)==104);
    read=(Read){edits,0}; CHECK(workbook_journal_read(j,100,1,read_entry,&read,&visited,&error)); CHECK(visited==1 && read.next==1);
    read=(Read){edits+1,0}; CHECK(workbook_journal_read(j,101,2,read_entry,&read,&visited,&error)); CHECK(visited==2);
    read=(Read){&invalid,0}; CHECK(workbook_journal_read(j,103,1,read_entry,&read,&visited,&error)); CHECK(visited==1);
    JournalCellEdit raw=edit(105,invalid.after,"\xff");
    CHECK(workbook_journal_append(j,&raw,1,(JournalControl){0},&receipt,&error));
    read=(Read){&raw,0}; CHECK(workbook_journal_read(j,104,1,read_entry,&read,&visited,&error)); CHECK(visited==1);
    workbook_journal_close(j);
}
static void limits(const char *path) {
    WorkbookJournal *j=NULL; char *error=NULL; JournalReceipt receipt={0};
    CHECK(workbook_journal_open(&j,path,document,UINT64_MAX-2,&error));
    JournalCellEdit edits[]={edit(UINT64_MAX-1,"","Value"),edit(UINT64_MAX,"Value","")};
    CHECK(workbook_journal_append(j,edits,2,(JournalControl){0},&receipt,&error)); CHECK(receipt.durable_revision==UINT64_MAX);
    workbook_journal_close(j); j=NULL; CHECK(workbook_journal_open(&j,path,document,UINT64_MAX-2,&error));
    Read read={edits,0}; size_t visited=0; CHECK(workbook_journal_read(j,UINT64_MAX-2,2,read_entry,&read,&visited,&error)); CHECK(visited==2);
    JournalCellEdit overflow=edit(0,"","Overflow"); reject(j,&overflow,1); workbook_journal_close(j);
}
static double now(void) { struct timespec t; CHECK(!clock_gettime(CLOCK_MONOTONIC,&t)); return t.tv_sec+t.tv_nsec/1e9; }
static void batching(const char *path) {
    WorkbookJournal *j=NULL; char *error=NULL; JournalReceipt receipt={0};
    CHECK(workbook_journal_open(&j,path,document,0,&error));
    double start=now();
    for (uint64_t i=1;i<=32;i++) { JournalCellEdit e=edit(i,"","Single"); e.row=i; CHECK(workbook_journal_append(j,&e,1,(JournalControl){0},&receipt,&error)); }
    double single=now()-start;
    JournalCellEdit batch[DX_JOURNAL_BATCH];
    for (uint64_t i=0;i<DX_JOURNAL_BATCH;i++) { batch[i]=edit(33+i,"","Batch"); batch[i].row=33+i; }
    start=now(); CHECK(workbook_journal_append(j,batch,DX_JOURNAL_BATCH,(JournalControl){0},&receipt,&error)); double batched=now()-start;
    CHECK(receipt.appended==DX_JOURNAL_BATCH && receipt.durable_revision==32+DX_JOURNAL_BATCH);
    char *large=dx_alloc(65537); memset(large,'x',65536); large[65536]=0;
    for (uint64_t i=0;i<DX_JOURNAL_BATCH;i++) { batch[i]=edit(289+i,"","Large"); batch[i].after=large; batch[i].after_bytes=65536; }
    reject(j,batch,DX_JOURNAL_BATCH); free(large);
    size_t visited=0,total=0; uint64_t cursor=0;
    while (cursor<288) { CHECK(workbook_journal_read(j,cursor,DX_JOURNAL_BATCH,count_entry,&total,&visited,&error)); CHECK(visited>0 && visited<=DX_JOURNAL_READ_BATCH); cursor+=visited; }
    CHECK(total==288 && workbook_journal_durable_revision(j)==288);
    printf("Journal engine %s: 32 single commits %.3f ms; 256 edits/one commit %.3f ms\n",duckdb_library_version(),single*1000,batched*1000);
    workbook_journal_close(j);
}
static int process_mode(int argc,char **argv) {
    if (argc!=3 && argc!=4) return 2;
    WorkbookJournal *j=NULL; char *error=NULL;
    if (!workbook_journal_open(&j,argv[2],document,0,&error)) { fprintf(stderr,"FAIL: %s\n",error); free(error); return 1; }
    if (!strcmp(argv[1],"--init")) { workbook_journal_close(j); puts("PASS: journal initialized"); return 0; }
    JournalCellEdit edits[]={edit(1,"","First"),edit(2,"First","Second")};
    bool any=!strcmp(argv[1],"--inspect-any");
    if (any || (!strcmp(argv[1],"--inspect") && argc==4)) {
        uint64_t expected=any ? workbook_journal_durable_revision(j) : (uint64_t)strtoull(argv[3],NULL,10);
        if (any) { CHECK(expected==0 || expected==2); printf("STATE durable-head=%llu\n",(unsigned long long)expected); } size_t visited=0; Read read={edits,0};
        CHECK(workbook_journal_durable_revision(j)==expected && expected<=2);
        CHECK(workbook_journal_read(j,0,256,read_entry,&read,&visited,&error)); CHECK(read.next==expected && visited==expected);
        workbook_journal_close(j); puts("PASS: complete journal state"); return 0;
    }
    JournalReceipt receipt={0};
    if (!strcmp(argv[1],"--expect-failure")) {
        receipt=(JournalReceipt){73,74,75}; JournalReceipt original=receipt;
        CHECK(!workbook_journal_append(j,edits,2,(JournalControl){0},&receipt,&error));
        CHECK(error && !memcmp(&receipt,&original,sizeof(receipt)) && !workbook_journal_durable_revision(j));
        printf("EXPECTED IO failure: %.400s\n",error); free(error); error=NULL;
        CHECK(!workbook_journal_append(j,edits,2,(JournalControl){0},&receipt,&error));
        CHECK(error && strstr(error,"reopen") && !memcmp(&receipt,&original,sizeof(receipt))); free(error);
        workbook_journal_close(j); puts("PASS: journal write failure preserved receipt"); return 0;
    }
    if (!strcmp(argv[1],"--close-fault")) {
        CHECK(workbook_journal_append(j,edits,2,(JournalControl){0},&receipt,&error)); CHECK(receipt.durable_revision==2);
        void *symbol=dlsym(RTLD_DEFAULT,"duxcel_journal_fault_arm"); void (*arm)(void)=NULL;
        CHECK(symbol && sizeof(symbol)==sizeof(arm)); memcpy(&arm,&symbol,sizeof(arm)); arm();
        workbook_journal_close(j); puts("PASS: journal close fault completed"); return 0;
    }
    JournalControl control={0};
    if (!strcmp(argv[1],"--hold-before")) control.before_commit=before_hold;
    else if (!strcmp(argv[1],"--hold-after")) control.after_commit=after_hold;
    else if (strcmp(argv[1],"--retry")) return 2;
    CHECK(workbook_journal_append(j,edits,2,control,&receipt,&error)); CHECK(receipt.durable_revision==2);
    workbook_journal_close(j); puts("PASS: journal commit"); return 0;
}
int main(int argc,char **argv) {
    if (argc>1) return process_mode(argc,argv);
    char directory[]="build/tests/journal-XXXXXX"; CHECK(mkdtemp(directory));
    char *path=dx_printf("%s/edits.duckdb",directory),*limit=dx_printf("%s/limit.duckdb",directory),*batch=dx_printf("%s/batch.duckdb",directory),*other=dx_printf("%s/other.duckdb",directory);
    transactions(path); limits(limit); batching(batch);
    WorkbookJournal *j=NULL; char *error=NULL;
    sql(other,"CREATE TABLE unrelated(a INTEGER)"); CHECK(!workbook_journal_open(&j,other,document,0,&error)); CHECK(error && !j); free(error); error=NULL;
    sql(other,"INSERT INTO unrelated VALUES (1)"); /* Failed open preserves existing user tables. */
    sql(path,"UPDATE duxcel_journal_meta SET version=2"); CHECK(!workbook_journal_open(&j,path,document,100,&error)); CHECK(error && !j); free(error); error=NULL;
    sql(path,"UPDATE duxcel_journal_meta SET version=1; DELETE FROM duxcel_journal_cells WHERE revision=102");
    CHECK(!workbook_journal_open(&j,path,document,100,&error)); CHECK(error && !j); free(error);
    unlink(path); unlink(limit); unlink(batch); unlink(other); CHECK(!rmdir(directory)); free(path); free(limit); free(batch); free(other);
    printf("PASS: %u workbook journal checks\n",checks); return 0;
}
