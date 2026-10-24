#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/version_seed.h"
#include "workbook/version_save.h"
#include "platform/directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL legacy version seed:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void sql(const char *path,const char *text) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0};
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,text,&r)==DuckDBSuccess); duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
static size_t children(const char *path) {
    DxDirectory *d=NULL; CHECK(dx_directory_open(path,&d,NULL)==DX_DIRECTORY_OPEN);
    DxDirectoryEntry entry={0}; DxDirectoryNext next; size_t count=0;
    while ((next=dx_directory_next(d,&entry,NULL))==DX_DIRECTORY_ENTRY) count++;
    CHECK(next==DX_DIRECTORY_END); dx_directory_close(d); return count;
}
static SourceDigest digest(const char *path) {
    SourceDigest d={0}; CHECK(source_digest_file(path,NULL,NULL,&d,NULL)==SOURCE_DIGEST_OK); return d;
}
static void unchanged(const char *path,SourceDigest before) { SourceDigest after=digest(path); CHECK(before.bytes==after.bytes && !memcmp(before.sha256,after.sha256,DX_SOURCE_DIGEST_BYTES)); }
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
static bool cancel_publish(void *context,WorkbookSaveProgress p) { (void)context; return p.phase!=SAVE_PHASE_PUBLISH; }
static bool cancel_admit(void *context,WorkbookVersionRestorePhase phase,size_t root,uint64_t bytes) { (void)context; (void)phase; (void)root; (void)bytes; return true; }
int main(void) {
    char parent[]="build/tests/version-seed-XXXXXX"; CHECK(mkdtemp(parent));
    char *path=dx_printf("%s/legacy.dxls",parent),*checkpoint=dx_printf("%s/checkpoint.dxls",parent); char *error=NULL;
    Workbook book; workbook_init(&book); CHECK(workbook_add(&book,"Legacy saved R",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,50,30,"Saved R",&error)); CHECK(workbook_save(&book,path,&error));
    WorkbookVersionCoreSet *out=NULL; WorkbookArchiveCandidate *candidate=NULL;
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    sql(path,"UPDATE _duxcel_workspace.meta SET version=14; DROP TABLE _duxcel_workspace.workbook_identity; DROP TABLE _duxcel_workspace.sheet_identity");
    SourceDigest before=digest(path); uint64_t reserved=UINT64_C(64)*1024*1024;
    CHECK(workbook_archive_read(path,parent,reserved,(WorkbookArchiveReadControl){0},&candidate,&error)==ARCHIVE_READ_READY && candidate && !error);
    const Workbook *opened=workbook_archive_book(candidate); CHECK(!opened->dirty && !workbook_archive_versions(candidate));
    CHECK(workbook_version_seed_legacy(candidate,parent,reserved,(WorkbookVersionSeedControl){0},&out,&error)==WORKBOOK_SAVE_COMPLETE && out && !error);
    const WorkbookVersionIndex *index=workbook_version_core_set_index(out);
    CHECK(index->generation==1 && index->saved_generation==1 && index->root_count==1 && index->saved.present && !index->tag_count);
    CHECK(index->document_high==opened->id.high && index->document_low==opened->id.low); unchanged(path,before);
    Workbook *modified=workbook_snapshot(opened); CHECK(modified && cells_set(&modified->sheets[0].cells,50,30,"Working R+1",&error)); modified->dirty=true; modified->command_revision=1;
    WorkbookVersionSaveRequest request={.head=modified,.base=out,.owner={modified->id.high,modified->id.low,482,VERSION_WORKING_HEAD},.purpose=VERSION_RECOVERY_CHECKPOINT,.path=checkpoint,.parent=parent,.reserved_bytes=reserved};
    WorkbookVersionReceipt *receipt=NULL;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && receipt && !error);
    WorkbookVersionCoreSet *saved=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt);
    Workbook old; workbook_init(&old); CHECK(workbook_load_core_payload(&old,workbook_core_payload_path(workbook_version_core_set_leaf(saved,0)),&error));
    CHECK(!strcmp(cells_get(&old.sheets[0].cells,50,30),"Saved R")); workbook_free(&old); workbook_version_core_set_free(saved);
    workbook_free(modified); free(modified); unchanged(path,before);
    /* Metadata copies own exact non-NUL-terminated label spans. */
    const char label[]={'T','a','g'}; WorkbookVersionTag tag={{label,sizeof(label)},index->roots[0].key,1};
    WorkbookVersionIndex tagged=*index; tagged.generation=tagged.saved_generation=2; tagged.tags=&tag; tagged.tag_count=1;
    WorkbookVersionMetadata *metadata=NULL;
    CHECK(workbook_version_metadata_copy(&tagged,reserved,&metadata,&error) && metadata && !error);
    const WorkbookVersionIndex *copy=workbook_version_metadata_index(metadata);
    CHECK(copy->tags[0].label.bytes==3 && copy->tags[0].label.data!=label && !strcmp(copy->tags[0].label.data,"Tag"));
    workbook_version_metadata_free(metadata); metadata=NULL;
    size_t baseline=children(parent); WorkbookVersionCoreSet *rejected=NULL;
    WorkbookVersionSeedControl stops[]={ {.capture={.save={cancel_publish,NULL}}}, {.admit={cancel_admit,NULL}} };
    for (size_t i=0;i<sizeof(stops)/sizeof(*stops);i++) {
        CHECK(workbook_version_seed_legacy(candidate,parent,reserved,stops[i],&rejected,&error)==WORKBOOK_SAVE_CANCELLED && !rejected); clear(&error);
        CHECK(children(parent)==baseline); unchanged(path,before);
    }
    CHECK(workbook_version_seed_legacy(candidate,parent,0,(WorkbookVersionSeedControl){0},&rejected,&error)==WORKBOOK_SAVE_FAILED && !rejected); clear(&error); CHECK(children(parent)==baseline);
    WorkbookCorePayload *leaf=NULL; CHECK(workbook_core_payload_capture(opened,parent,(WorkbookCorePayloadControl){0},&leaf,&error)==CORE_PAYLOAD_READY);
    const WorkbookCorePayloadInfo *info=workbook_core_payload_info(leaf); WorkbookVersionRoot root={.bytes=info->bytes+1,.encoding=15}; memcpy(root.key.bytes,info->sha256,DX_VERSION_KEY_BYTES);
    WorkbookVersionIndex owned={.document_high=opened->id.high,.document_low=opened->id.low,.generation=1,.saved_generation=1,.saved={true,root.key},.roots=&root,.root_count=1};
    CHECK(workbook_version_metadata_copy(&owned,reserved,&metadata,&error)); WorkbookCorePayload *same_leaf=leaf; WorkbookVersionMetadata *same_metadata=metadata;
    CHECK(workbook_version_core_set_take(&metadata,&leaf,1,reserved,(WorkbookVersionRestoreControl){0},&rejected,&error)==VERSION_RESTORE_FAILED && !rejected && leaf==same_leaf && metadata==same_metadata); clear(&error);
    workbook_version_metadata_free(metadata); metadata=NULL; root.bytes--;
    CHECK(workbook_version_metadata_copy(&owned,reserved,&metadata,&error)); same_metadata=metadata;
    CHECK(workbook_version_core_set_take(&metadata,&leaf,1,reserved,(WorkbookVersionRestoreControl){cancel_admit,NULL},&rejected,&error)==VERSION_RESTORE_CANCELLED && !rejected && leaf==same_leaf && metadata==same_metadata); clear(&error);
    CHECK(workbook_version_core_set_take(&metadata,&leaf,1,reserved,(WorkbookVersionRestoreControl){0},&rejected,&error)==VERSION_RESTORE_READY && rejected && !leaf && !metadata);
    workbook_version_core_set_free(rejected); rejected=NULL; CHECK(children(parent)==baseline);
    sql(path,"UPDATE _duxcel_workspace.sheets SET name='Changed on disk'"); before=digest(path);
    CHECK(workbook_version_seed_legacy(candidate,parent,reserved,(WorkbookVersionSeedControl){0},&rejected,&error)==WORKBOOK_SAVE_FAILED && !rejected); clear(&error);
    CHECK(children(parent)==baseline); unchanged(path,before);
    workbook_archive_candidate_free(candidate); candidate=NULL; workbook_version_core_set_free(out); out=NULL; CHECK(!unlink(checkpoint));
#endif
    CHECK(workbook_version_seed_legacy(NULL,parent,0,(WorkbookVersionSeedControl){0},&out,&error)==WORKBOOK_SAVE_FAILED && !out); clear(&error);
    workbook_free(&book); CHECK(!unlink(path) && !children(parent) && !rmdir(parent)); free(path); free(checkpoint);
    printf("PASS: %u non-writing legacy Saved baseline checks\n",checks); return 0;
}
