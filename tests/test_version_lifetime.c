#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/version_save.h"
#include "platform/platform.h"
#include "platform/file_lease.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL version lifetime:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { DxMutex mutex; DxCond ready; unsigned count,completed; } Gate;
typedef struct { Gate *gate; DxCond start; bool go,release,ok; WorkbookVersionCoreSet *owned; } Consumer;
static void *consume(void *context) {
    Consumer *c=context; Gate *gate=c->gate;
    dx_lock(&gate->mutex); gate->count++; dx_signal(&gate->ready);
    while (!c->go) dx_cond_wait(&c->start,&gate->mutex);
    dx_unlock(&gate->mutex);
    bool ok=true;
    for (unsigned i=0;i<1000;i++) {
        WorkbookVersionCoreSet *reference=workbook_version_core_set_retain(c->owned);
        const WorkbookVersionIndex *index=workbook_version_core_set_index(reference);
        if (!reference || !index || index->root_count!=1 || !index->saved.present || workbook_version_index_check(index)!=VERSION_OK || !workbook_version_core_set_leaf(reference,0)) ok=false;
        workbook_version_core_set_free(reference);
    }
    dx_lock(&gate->mutex); gate->completed++; dx_signal(&gate->ready);
    while (!c->release) dx_cond_wait(&c->start,&gate->mutex);
    dx_unlock(&gate->mutex);
    /* Last disposal intentionally runs on this background consumer. */
    workbook_version_core_set_free(c->owned); c->owned=NULL; c->ok=ok; return NULL;
}
int main(void) {
    char parent[]="build/tests/version-lifetime-XXXXXX"; CHECK(mkdtemp(parent));
    char *path=dx_printf("%s/workbook.dxls",parent); char *error=NULL;
    Workbook book; workbook_init(&book); CHECK(workbook_add(&book,"Shared immutable store",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,50,30,"9007199254740993.0123456789",&error));
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    WorkbookVersionSaveRequest request={.head=&book,.owner={book.id.high,book.id.low,482,VERSION_WORKING_HEAD},.purpose=VERSION_USER_SAVE,.path=path,.parent=parent,.reserved_bytes=UINT64_C(64)*1024*1024};
    WorkbookVersionReceipt *receipt=NULL;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && receipt && !error);
    const WorkbookVersionCoreSet *borrowed=workbook_archive_receipt_versions(workbook_version_receipt_archive(receipt));
    Gate gate={0}; dx_mutex_init(&gate.mutex); dx_cond_init(&gate.ready);
    Consumer consumers[2]={{.gate=&gate},{.gate=&gate}}; DxThread threads[2];
    const char *leaf=workbook_core_payload_path(workbook_version_core_set_leaf(borrowed,0)); char *owned_path=dx_strdup(leaf);
    char *directory=dx_strdup(workbook_core_payload_directory(workbook_version_core_set_leaf(borrowed,0))),*lease=dx_printf("%s/lease.lock",directory);
    for (size_t i=0;i<2;i++) {
        consumers[i].owned=workbook_version_core_set_retain(borrowed); CHECK(consumers[i].owned==borrowed);
        dx_cond_init(&consumers[i].start); CHECK(dx_thread_start(threads+i,consume,consumers+i));
    }
    dx_lock(&gate.mutex); while (gate.count!=2) dx_cond_wait(&gate.ready,&gate.mutex); dx_unlock(&gate.mutex);
    /* Release the UI-like receipt owner while worker/journal-like borrowers
       hold independent references. The paths/lease must remain alive. */
    workbook_version_receipt_free(receipt); receipt=NULL;
    CHECK(!access(owned_path,F_OK) && !access(lease,F_OK));
    DxFileLease *other=NULL;
    CHECK(dx_file_lease_acquire(lease,false,&other,&error)==DX_FILE_LEASE_BUSY && !other && !error);
    dx_lock(&gate.mutex);
    for (size_t i=0;i<2;i++) { consumers[i].go=true; dx_signal(&consumers[i].start); }
    while (gate.completed!=2) dx_cond_wait(&gate.ready,&gate.mutex);
    consumers[0].release=true; dx_signal(&consumers[0].start); dx_unlock(&gate.mutex);
    dx_thread_join(threads[0]); CHECK(consumers[0].ok && !consumers[0].owned);
    CHECK(!access(owned_path,F_OK) && workbook_version_core_set_index(consumers[1].owned)->root_count==1);
    CHECK(dx_file_lease_acquire(lease,false,&other,&error)==DX_FILE_LEASE_BUSY && !other && !error);
    dx_lock(&gate.mutex); consumers[1].release=true; dx_signal(&consumers[1].start); dx_unlock(&gate.mutex);
    dx_thread_join(threads[1]); CHECK(consumers[1].ok && !consumers[1].owned);
    CHECK(access(owned_path,F_OK) && access(lease,F_OK) && access(directory,F_OK));
    for (size_t i=0;i<2;i++) dx_cond_destroy(&consumers[i].start);
    dx_cond_destroy(&gate.ready); dx_mutex_destroy(&gate.mutex);
    free(owned_path); free(directory); free(lease); CHECK(!unlink(path));
#endif
    CHECK(!workbook_version_core_set_retain(NULL)); workbook_version_core_set_free(NULL);
    workbook_free(&book); free(path); CHECK(!rmdir(parent));
    printf("PASS: %u shared immutable version lifetime checks\n",checks); return 0;
}
