#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/history_after_save.h"
#include "workbook/history_component.h"
#include "workbook/version_save.h"
#include "workbook/version_stage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL after-Save history:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
#include "support/history_native_fixture.h"
static WorkbookHistoryProfileCaps caps(void) { return (WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES}; }
static WorkbookHistoryNativeCaps native_caps(void) { return (WorkbookHistoryNativeCaps){256u*1024u*1024u,1024u*1024u}; }
static void exec(duckdb_connection c,const char *sql) { duckdb_result r={0}; duckdb_state state=duckdb_query(c,sql,&r); if (state!=DuckDBSuccess) fprintf(stderr,"SQL: %s\n",duckdb_result_error(&r)); CHECK(state==DuckDBSuccess); duckdb_destroy_result(&r); }
static bool cancel(void *context,size_t root,uint64_t bytes) { (void)context; (void)root; (void)bytes; return true; }
static WorkbookVersionCoreSet *after_set(duckdb_connection c,const char *parent,const WorkbookHistoryNativeCapture *source,WorkbookVersionPlan *save,char **error) {
    const WorkbookVersionCoreSet *base=workbook_history_native_roots(source); const WorkbookVersionIndex *old=workbook_version_core_set_index(base);
    const WorkbookCorePayload *head=workbook_history_native_head(source); const WorkbookCorePayloadInfo *info=workbook_core_payload_info(head);
    WorkbookVersionRoot root={.bytes=info->bytes,.encoding=15}; memcpy(root.key.bytes,info->sha256,32);
    CHECK(workbook_version_prepare(old,workbook_history_native_position(source)->owner,VERSION_USER_SAVE,&root,save)==VERSION_OK);
    WorkbookVersionRoot *roots=dx_alloc((old->root_count+1u)*sizeof(*roots)); const WorkbookCorePayload **leaves=dx_alloc((old->root_count+1u)*sizeof(*leaves));
    for (size_t i=0;i<old->root_count;i++) { roots[i]=old->roots[i]; leaves[i]=workbook_version_core_set_leaf(base,i); }
    if (save->adds_root) { roots[old->root_count]=root; leaves[old->root_count]=head; }
    WorkbookVersionIndex after=*old; after.generation=save->next_generation; after.saved_generation=save->next_saved_generation; after.saved=save->after_saved; after.roots=roots; after.root_count+=save->adds_root;
    CHECK(workbook_version_stage(c,&after,leaves,after.root_count,256u*1024u*1024u,(WorkbookVersionStageControl){0},error)==VERSION_STAGE_READY);
    WorkbookVersionCoreSet *prepared=NULL; CHECK(workbook_version_core_set_read(c,parent,256u*1024u*1024u,(WorkbookVersionRestoreControl){0},&prepared,error)==VERSION_RESTORE_READY);
    free(roots); free(leaves); return prepared;
}
static void rejected(const WorkbookHistoryNativeCapture *source,const WorkbookVersionCoreSet *after,const WorkbookVersionPlan *save,WorkbookHistoryProfileCaps profile,WorkbookHistoryNativeCaps native,WorkbookHistoryAfterSaveControl control,WorkbookHistoryNativeStatus expected) {
    WorkbookHistoryAfterSave *out=NULL; char *error=NULL; const WorkbookHistoryProfile *old=workbook_history_native_profile(source);
    CHECK(workbook_history_after_save_prepare(source,after,save,profile,native,control,&out,&error)==expected && !out && workbook_history_native_profile(source)==old); free(error);
}
typedef struct { FILE *file; uint64_t offset; } FileInput;
static bool input(void *context,uint64_t offset,void *data,size_t bytes,char **error) { (void)error; FileInput *f=context; if (offset!=f->offset || fread(data,1,bytes,f->file)!=bytes) return false; f->offset+=bytes; return true; }
static WorkbookCorePayload *restore_file(const char *path,const char *parent,WorkbookId doc,char **error) {
    SourceDigest digest; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; CHECK(source_digest_file(path,NULL,NULL,&digest,detail)==SOURCE_DIGEST_OK);
    WorkbookCorePayloadInfo expected={.document=doc,.bytes=digest.bytes,.encoding=15}; memcpy(expected.sha256,digest.sha256,32); FileInput f={fopen(path,"rb"),0}; CHECK(f.file);
    WorkbookCorePayload *head=NULL; CHECK(workbook_core_payload_restore(&expected,parent,digest.bytes,(WorkbookCorePayloadInput){input,NULL,&f},&head,error)==CORE_PAYLOAD_READY); CHECK(fclose(f.file)==0); return head;
}
static WorkbookHistoryNativeCapture *with_profile(const WorkbookHistoryProfileView *view,const WorkbookVersionCoreSet *roots,WorkbookCorePayload *head,WorkbookHistoryCapturedPosition position,char **error) {
    WorkbookHistoryProfile *profile=NULL; CHECK(workbook_history_profile_prepare(*view,caps(),&profile)==HISTORY_PROFILE_OK); WorkbookVersionCoreSet *owned=workbook_version_core_set_retain(roots); CHECK(owned);
    WorkbookHistoryNativeCapture *out=NULL; CHECK(workbook_history_native_take(&profile,&owned,&head,position,native_caps(),(WorkbookHistoryNativeControl){0},&out,error)==HISTORY_NATIVE_READY); return out;
}
static void unsaved(const char *parent,duckdb_connection c,char **error) {
    Workbook book; workbook_init(&book); CHECK(workbook_add(&book,"Unsaved",&(Analysis){0},error));
    WorkbookVersionIndex empty={.document_high=book.id.high,.document_low=book.id.low}; WorkbookVersionMetadata *metadata=NULL; CHECK(workbook_version_metadata_copy(&empty,0,&metadata,error));
    WorkbookVersionCoreSet *roots=NULL; CHECK(workbook_version_core_set_take(&metadata,NULL,0,0,(WorkbookVersionRestoreControl){0},&roots,error)==VERSION_RESTORE_READY);
    WorkbookHistoryAnchors anchors={.document_high=book.id.high,.document_low=book.id.low}; WorkbookHistorySegment segment={0,0,0}; WorkbookHistoryPath path={.document_high=book.id.high,.document_low=book.id.low,.segments=&segment,.segment_count=1};
    WorkbookHistoryProfileView view={.document=book.id,.container_version=16,.core_version=15,.kind=HISTORY_PROFILE_RETAINED,.versions=&empty,.anchors=&anchors,.current={&path,NULL,0}};
    WorkbookCorePayload *head=NULL; CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&head,error)==CORE_PAYLOAD_READY);
    WorkbookHistoryNativeCapture *source=with_profile(&view,roots,head,(WorkbookHistoryCapturedPosition){{book.id.high,book.id.low,77,VERSION_WORKING_HEAD},0},error); workbook_version_core_set_free(roots);
    exec(c,"BEGIN"); WorkbookVersionPlan save; WorkbookVersionCoreSet *after=after_set(c,parent,source,&save,error); WorkbookHistoryAfterSave *plan=NULL;
    CHECK(workbook_history_after_save_prepare(source,after,&save,caps(),native_caps(),(WorkbookHistoryAfterSaveControl){0},&plan,error)==HISTORY_NATIVE_READY);
    const WorkbookHistoryProfileView *state=workbook_history_profile_view(workbook_history_after_save_profile(plan)); const WorkbookHistoryProtection *window=workbook_history_profile_protection(workbook_history_after_save_profile(plan));
    CHECK(state->versions->saved.present && state->versions->root_count==1 && state->anchors->count==1 && !state->anchors->saved_anchor && !state->anchors->high_water && window->has_saved && !window->current_cursor && !window->current_through && !state->count);
    CHECK(!workbook_history_profile_view(workbook_history_native_profile(source))->versions->saved.present);
    workbook_history_after_save_free(&plan); workbook_version_core_set_free(after); exec(c,"ROLLBACK"); workbook_history_native_free(&source); workbook_free(&book);
}
int main(void) {
    char parent[]="build/tests/history-after-save-XXXXXX"; CHECK(mkdtemp(parent)); Workbook book; char *path=NULL,*error=NULL; WorkbookHistoryNativeCapture *source=native_fixture(parent,&book,&path,&error);
    duckdb_database database=NULL; duckdb_connection c=NULL; CHECK(duckdb_open(NULL,&database)==DuckDBSuccess && duckdb_connect(database,&c)==DuckDBSuccess);
    exec(c,"BEGIN"); WorkbookVersionPlan save; WorkbookVersionCoreSet *after=after_set(c,parent,source,&save,&error);
    WorkbookHistoryProfileCaps limited=caps(); limited.staging_bytes=0; rejected(source,after,&save,limited,native_caps(),(WorkbookHistoryAfterSaveControl){0},HISTORY_NATIVE_LIMIT);
    WorkbookHistoryNativeCaps native=native_caps(); native.additional_bytes=0; rejected(source,after,&save,caps(),native,(WorkbookHistoryAfterSaveControl){0},HISTORY_NATIVE_LIMIT);
    native=native_caps(); native.physical_bytes=1; rejected(source,after,&save,caps(),native,(WorkbookHistoryAfterSaveControl){0},HISTORY_NATIVE_LIMIT);
    WorkbookVersionPlan bad=save; bad.purpose=VERSION_RECOVERY_CHECKPOINT; rejected(source,after,&bad,caps(),native_caps(),(WorkbookHistoryAfterSaveControl){0},HISTORY_NATIVE_INVALID);
    bad=save; bad.next_generation++; rejected(source,after,&bad,caps(),native_caps(),(WorkbookHistoryAfterSaveControl){0},HISTORY_NATIVE_ASSOCIATION);
    rejected(source,workbook_history_native_roots(source),&save,caps(),native_caps(),(WorkbookHistoryAfterSaveControl){0},HISTORY_NATIVE_ASSOCIATION);
    rejected(source,after,&save,caps(),native_caps(),(WorkbookHistoryAfterSaveControl){.verify={cancel,NULL}},HISTORY_NATIVE_CANCELLED);
    WorkbookHistoryAfterSave *plan=NULL; CHECK(workbook_history_after_save_prepare(source,after,&save,caps(),native_caps(),(WorkbookHistoryAfterSaveControl){0},&plan,&error)==HISTORY_NATIVE_READY);
    const WorkbookHistoryProfile *profile=workbook_history_after_save_profile(plan); const WorkbookHistoryProfileView *state=workbook_history_profile_view(profile);
    CHECK(state->anchors->saved_anchor==1 && state->anchors->high_water==1 && state->anchors->count==1 && state->anchors->items[0].boundary.position==1 && state->current.path->tail==1 && state->saved.path->tail==1 && state->count==1);
    CHECK(workbook_history_profile_view(workbook_history_native_profile(source))->anchors->saved_anchor==0 && !workbook_history_after_save_semantic_reuse(plan));
    const WorkbookCorePayloadInfo *selected_info=workbook_core_payload_info(workbook_history_after_save_selected(plan));
    CHECK(selected_info && !memcmp(selected_info->sha256,state->versions->saved.key.bytes,32));
    /* Borrowed stage association permits the selected leaf role; the owning
       capsule constructor must refuse it without consuming either owner. */
    WorkbookHistoryProfile *alias_profile=NULL; CHECK(workbook_history_profile_prepare(*state,caps(),&alias_profile)==HISTORY_PROFILE_OK);
    WorkbookVersionCoreSet *alias_roots=workbook_version_core_set_retain(after); CHECK(alias_roots);
    WorkbookCorePayload *alias=(WorkbookCorePayload *)workbook_history_after_save_selected(plan); WorkbookHistoryNativeCapture *invalid=NULL;
    CHECK(workbook_history_native_take(&alias_profile,&alias_roots,&alias,*workbook_history_after_save_position(plan),native_caps(),(WorkbookHistoryNativeControl){0},&invalid,&error)==HISTORY_NATIVE_ASSOCIATION && alias_profile && alias_roots && !invalid);
    workbook_history_profile_free(&alias_profile); workbook_version_core_set_free(alias_roots);
    /* Stage order is acyclic: root stage/read already returned a real CoreSet;
       only now is the after-profile component staged on that same transaction. */
    size_t bytes=0; CHECK(workbook_history_profile_encoded_size(profile,&bytes)==HISTORY_PROFILE_CODEC_OK); void *wire=dx_alloc(bytes),*arena=dx_alloc(DX_HISTORY_PROFILE_PARSE_BYTES);
    uint64_t storage=workbook_core_payload_info(workbook_history_after_save_selected(plan))->bytes+bytes;
    CHECK(workbook_history_component_stage_after_save(c,plan,wire,bytes,bytes+DX_HISTORY_COMPONENT_IO_BYTES,storage,(WorkbookHistoryComponentControl){0},&error)==HISTORY_COMPONENT_READY);
    WorkbookHistoryNativeCapture *read=NULL; WorkbookHistoryProfileBinding binding={workbook_history_after_save_position(plan)->owner,17};
    CHECK(workbook_history_component_read(c,after,binding,parent,caps(),native_caps(),wire,bytes,arena,DX_HISTORY_PROFILE_PARSE_BYTES,(WorkbookHistoryComponentControl){0},&read,&error)==HISTORY_COMPONENT_READY);
    CHECK(workbook_history_profile_view(workbook_history_native_profile(read))->anchors->saved_anchor==1); workbook_history_native_free(&read); free(wire); free(arena); exec(c,"ROLLBACK");
    /* Old Saved cannot be chosen by matching only doc/revision metadata. */
    const WorkbookVersionIndex *before=workbook_version_core_set_index(workbook_history_native_roots(source)); WorkbookVersionPlan wrong;
    CHECK(workbook_version_prepare(before,workbook_history_native_position(source)->owner,VERSION_USER_SAVE,before->roots,&wrong)==VERSION_OK);
    rejected(source,workbook_history_native_roots(source),&wrong,caps(),native_caps(),(WorkbookHistoryAfterSaveControl){0},HISTORY_NATIVE_ASSOCIATION);
    /* Produce a different exact current15 file with identical persisted tables,
       using only current save/SQL/restore authorities, not a second decoder. */
    char *equivalent=dx_printf("%s/equivalent.dxls",parent); CHECK(workbook_save_core_payload(&book,equivalent,(WorkbookSaveControl){0},&error)==WORKBOOK_SAVE_COMPLETE);
    duckdb_database eqdb=NULL; duckdb_connection eq=NULL; CHECK(duckdb_open(equivalent,&eqdb)==DuckDBSuccess && duckdb_connect(eqdb,&eq)==DuckDBSuccess);
    exec(eq,"ALTER TABLE _duxcel_workspace.cells RENAME TO cells_copy; CREATE TABLE _duxcel_workspace.cells AS SELECT * FROM _duxcel_workspace.cells_copy; DROP TABLE _duxcel_workspace.cells_copy"); duckdb_disconnect(&eq); duckdb_close(&eqdb);
    WorkbookCorePayload *different=restore_file(equivalent,parent,book.id,&error);
    CHECK(memcmp(workbook_core_payload_info(different)->sha256,workbook_core_payload_info(workbook_history_after_save_selected(plan))->sha256,32));
    WorkbookHistoryProfileView reuse_view=*state; WorkbookHistoryAnchors exhausted=*state->anchors; exhausted.high_water=UINT64_MAX; reuse_view.anchors=&exhausted;
    WorkbookHistoryNativeCapture *reuse=with_profile(&reuse_view,after,different,*workbook_history_native_position(source),&error);
    const WorkbookVersionIndex *stable=workbook_version_core_set_index(after); size_t saved=0; while (memcmp(stable->roots[saved].key.bytes,stable->saved.key.bytes,32)) saved++;
    WorkbookVersionPlan noop; CHECK(workbook_version_prepare(stable,workbook_history_native_position(source)->owner,VERSION_USER_SAVE,stable->roots+saved,&noop)==VERSION_OK && !noop.updates_saved);
    WorkbookHistoryAfterSave *reused=NULL; CHECK(workbook_history_after_save_prepare(reuse,after,&noop,caps(),native_caps(),(WorkbookHistoryAfterSaveControl){0},&reused,&error)==HISTORY_NATIVE_READY && workbook_history_after_save_semantic_reuse(reused));
    CHECK(workbook_history_after_save_selected(reused)==workbook_version_core_set_leaf(after,saved) && workbook_history_profile_view(workbook_history_after_save_profile(reused))->anchors->high_water==UINT64_MAX && workbook_history_profile_view(workbook_history_after_save_profile(reused))->anchors->saved_anchor==1);
    workbook_history_after_save_free(&reused); workbook_history_native_free(&reuse);
    /* Changed Saved refuses exhausted anchor space without consuming a source. */
    different=restore_file(equivalent,parent,book.id,&error); WorkbookHistoryProfileView exhausted_view=*workbook_history_profile_view(workbook_history_native_profile(source)); exhausted=*exhausted_view.anchors; exhausted.high_water=UINT64_MAX; exhausted_view.anchors=&exhausted;
    WorkbookHistoryNativeCapture *full=with_profile(&exhausted_view,workbook_history_native_roots(source),different,*workbook_history_native_position(source),&error);
    exec(c,"BEGIN"); WorkbookVersionPlan full_save; WorkbookVersionCoreSet *full_after=after_set(c,parent,full,&full_save,&error);
    rejected(full,full_after,&full_save,caps(),native_caps(),(WorkbookHistoryAfterSaveControl){0},HISTORY_NATIVE_LIMIT); workbook_version_core_set_free(full_after); exec(c,"ROLLBACK"); workbook_history_native_free(&full);
    workbook_history_after_save_free(&plan); workbook_version_core_set_free(after);
    unsaved(parent,c,&error);
    workbook_history_native_free(&source); workbook_free(&book); duckdb_disconnect(&c); duckdb_close(&database);
    CHECK(!workbook_history_after_save_profile(NULL) && !workbook_history_after_save_roots(NULL) && !workbook_history_after_save_selected(NULL) && !workbook_history_after_save_position(NULL)); workbook_history_after_save_free(NULL);
    CHECK(remove(path)==0 && remove(equivalent)==0 && rmdir(parent)==0); free(path); free(equivalent); free(error);
    printf("PASS: %u prepared after-Save history checks\n",checks); return 0;
}
