#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/browse_preview_component.h"
#include "platform/directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL native preview component:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { Page page; Analysis *definition; unsigned owner; } Result;
static bool resolve(void *context,uint64_t object,RangeResultProof *out) {
    Result *r=context; if (object) return false;
    *out=(RangeResultProof){.owner=&r->owner,.definition=r->definition,.page=&r->page,.ready=true,.request=5,.query_epoch=9,.source_epoch=9}; return true;
}
static BrowsePreviewSet *preview(const Workbook *book) {
    const Worksheet *sheet=book->sheets; Result r={.definition=(Analysis *)&sheet->analysis};
    r.page=(Page){.column_count=1,.row_count=100,.total=100,.columns=dx_alloc(sizeof(Column)),.cells=dx_alloc(100*sizeof(Cell))};
    r.page.columns[0]=(Column){.name=dx_strdup("exact"),.type_name=dx_strdup("VARCHAR"),.type=DUCKDB_TYPE_VARCHAR};
    for (size_t i=0;i<100;i++) { r.page.cells[i].text=dx_alloc(1025); memset(r.page.cells[i].text,'a'+(i%20),1024); }
    free(r.page.cells[0].text); r.page.cells[0]=(Cell){.is_null=true};
    free(r.page.cells[1].text); r.page.cells[1]=(Cell){.text=dx_strdup("")};
    free(r.page.cells[2].text); r.page.cells[2]=(Cell){.text=dx_strdup("NULL")};
    free(r.page.cells[3].text); r.page.cells[3]=(Cell){.text=dx_strdup("9007199254740993.012345678901234567")};
    uint64_t session=1,request=5,epoch=9; RangeResultContext results={resolve,&r};
    BrowsePreviewContext c={.document=&book->id,.document_session=&session,.sheet=&sheet->id,.sheet_session=&session,
        .object_high_water=&sheet->objects.next_id,.metadata_clock=&book->command_revision,.request_epoch=&request,.query_epoch=&epoch,.source_epoch=&epoch,
        .definition=r.definition,.result_owner=&r.owner,.sources=&sheet->source_versions,.results=&results};
    BrowsePreviewTarget target; CHECK(browse_preview_prepare(&c,&target)==BROWSE_PREVIEW_OK);
    BrowsePreview *asset=NULL; CHECK(browse_preview_capture(&c,&target,SIZE_MAX,&asset)==BROWSE_PREVIEW_OK);
    BrowsePreviewSet *set=NULL; CHECK(browse_preview_set_take(book,&asset,1,SIZE_MAX,0,SIZE_MAX,(BrowsePreviewSetControl){0},&set)==BROWSE_PREVIEW_SET_OK && !asset);
    page_free(&r.page); return set;
}
typedef enum { NORMAL,TOO_SMALL,DISK_SMALL,STOP_VERIFY,STOP_ASSOCIATE,STOP_DESCRIPTOR,STOP_CHUNKS,STOP_RECHECK,DUPLICATE,REJECT_INTENT } Mode;
typedef struct {
    const Workbook *actual; const BrowsePreviewSet *set; const WorkbookVersionCoreSet *base;
    Mode mode; BrowsePreviewComponentInfo info; BrowsePreviewComponentStatus status;
    unsigned prepared,cancelled; bool reused;
} Stage;
static bool stop(void *context,BrowsePreviewComponentPhase phase,size_t entry,uint64_t bytes) {
    (void)entry; (void)bytes; Stage *s=context;
    return (s->mode==STOP_VERIFY && phase==BROWSE_COMPONENT_VERIFY) || (s->mode==STOP_ASSOCIATE && phase==BROWSE_COMPONENT_ASSOCIATE) ||
        (s->mode==STOP_DESCRIPTOR && phase==BROWSE_COMPONENT_DESCRIPTOR) || (s->mode==STOP_CHUNKS && phase==BROWSE_COMPONENT_CHUNKS) || (s->mode==STOP_RECHECK && phase==BROWSE_COMPONENT_RECHECK);
}
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
static void cancelled(void *context,const WorkbookVersionPublication *publication) { Stage *s=context; CHECK(publication); s->cancelled++; }
static bool stage(void *context,const WorkbookVersionPublication *p,duckdb_connection c,const WorkbookVersionCoreSet *after,char **error) {
    Stage *s=context; s->prepared++;
    if (s->mode==REJECT_INTENT) { s->status=browse_preview_component_stage(p,c,after,s->set,0,SIZE_MAX,UINT64_MAX,(BrowsePreviewComponentControl){0},&s->info,error); return false; }
    CHECK(workbook_version_publication_snapshot(p,c,after)==s->actual);
    CHECK(!workbook_version_publication_snapshot(p,c,s->base));
    duckdb_database wrong_db=NULL; duckdb_connection wrong=NULL; CHECK(duckdb_open(NULL,&wrong_db)==DuckDBSuccess && duckdb_connect(wrong_db,&wrong)==DuckDBSuccess);
    BrowsePreviewComponentInfo sentinel; memset(&sentinel,0xa5,sizeof(sentinel)); BrowsePreviewComponentInfo bad=sentinel;
    CHECK(browse_preview_component_measure(p,wrong,after,s->set,0,&bad,error)==BROWSE_COMPONENT_FAILED && !memcmp(&bad,&sentinel,sizeof(bad))); clear(error);
    duckdb_disconnect(&wrong); duckdb_close(&wrong_db);
    CHECK(browse_preview_component_measure(p,c,after,s->set,0,&bad,error)==BROWSE_COMPONENT_READY);
    CHECK(bad.entries==1 && bad.snapshot_revision==s->actual->command_revision && bad.set_bytes>DX_VERSION_CHUNK_BYTES);
    const WorkbookCorePayloadInfo *selected=workbook_core_payload_info(workbook_version_publication_selected(p));
    CHECK(selected && selected->bytes==bad.target_bytes && !memcmp(selected->sha256,bad.target_sha256,DX_SOURCE_DIGEST_BYTES));
    s->reused=workbook_version_publication_compared(p);
    size_t reserved=s->mode==TOO_SMALL ? bad.coexistence-1 : bad.coexistence;
    uint64_t disk=s->mode==DISK_SMALL ? bad.set_bytes-1 : bad.set_bytes;
    s->info=sentinel;
    s->status=browse_preview_component_stage(p,c,after,s->set,0,reserved,disk,(BrowsePreviewComponentControl){stop,s},&s->info,error);
    if (s->status!=BROWSE_COMPONENT_READY) { CHECK(!memcmp(&s->info,&sentinel,sizeof(s->info))); return false; }
    CHECK(s->info.set_bytes==bad.set_bytes && !memcmp(s->info.target_sha256,bad.target_sha256,DX_SOURCE_DIGEST_BYTES));
    if (s->mode==DUPLICATE) {
        bad=sentinel; CHECK(browse_preview_component_stage(p,c,after,s->set,0,SIZE_MAX,UINT64_MAX,(BrowsePreviewComponentControl){0},&bad,error)==BROWSE_COMPONENT_FAILED && !memcmp(&bad,&sentinel,sizeof(bad))); return false;
    }
    return true;
}
static SourceDigest digest(const char *path) { SourceDigest d={0}; char error[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; CHECK(source_digest_file(path,NULL,NULL,&d,error)==SOURCE_DIGEST_OK); return d; }
static size_t children(const char *parent) { DxDirectory *d=NULL; CHECK(dx_directory_open(parent,&d,NULL)==DX_DIRECTORY_OPEN); size_t n=0; DxDirectoryEntry e={0}; DxDirectoryNext next; while ((next=dx_directory_next(d,&e,NULL))==DX_DIRECTORY_ENTRY) n++; CHECK(next==DX_DIRECTORY_END); dx_directory_close(d); return n; }
static void readback(const char *path,const Workbook *actual,BrowsePreviewComponentInfo expected) {
    duckdb_config config=NULL; duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0}; char *error=NULL;
    CHECK(duckdb_create_config(&config)==DuckDBSuccess && duckdb_set_config(config,"access_mode","READ_ONLY")==DuckDBSuccess && duckdb_set_config(config,"threads","1")==DuckDBSuccess);
    CHECK(duckdb_open_ext(path,&db,config,NULL)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,"SELECT codec,container_encoding,core_encoding,set_encoding,scope,document_high,document_low,snapshot_revision,target_bytes,target_hash,set_bytes,set_hash FROM _duxcel_previews.meta",&r)==DuckDBSuccess && duckdb_row_count(&r)==1);
    for (size_t i=0;i<5;i++) CHECK(duckdb_value_int32(&r,i,0)==(i==1 ? 16 : i==2 ? 15 : 1));
    CHECK(duckdb_value_uint64(&r,5,0)==actual->id.high && duckdb_value_uint64(&r,6,0)==actual->id.low && duckdb_value_uint64(&r,7,0)==actual->command_revision && duckdb_value_uint64(&r,8,0)==expected.target_bytes && duckdb_value_uint64(&r,10,0)==expected.set_bytes);
    for (size_t i=0;i<2;i++) { duckdb_blob hash=duckdb_value_blob(&r,i ? 11 : 9,0); CHECK(hash.size==32 && !memcmp(hash.data,i ? expected.set_sha256 : expected.target_sha256,32)); duckdb_free(hash.data); }
    duckdb_destroy_result(&r);
    CHECK(duckdb_query(c,"SELECT position,data FROM _duxcel_previews.chunks ORDER BY position",&r)==DuckDBSuccess);
    WorkbookChunks spans={0}; CHECK(workbook_chunks_begin(expected.set_bytes,expected.set_bytes,&spans)==VERSION_CHUNKS_OK && spans.count==duckdb_row_count(&r));
    unsigned char *wire=dx_alloc((size_t)expected.set_bytes);
    for (idx_t i=0;i<duckdb_row_count(&r);i++) { duckdb_blob blob=duckdb_value_blob(&r,1,i); uint64_t offset=spans.received; CHECK(workbook_chunks_offer(&spans,duckdb_value_uint64(&r,0,i),blob.data,blob.size)==VERSION_CHUNKS_OK); memcpy(wire+(size_t)offset,blob.data,(size_t)blob.size); duckdb_free(blob.data); }
    CHECK(workbook_chunks_finish(&spans)==VERSION_CHUNKS_OK); duckdb_destroy_result(&r);
    unsigned char hash[32]; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; CHECK(source_digest_bytes(wire,(size_t)expected.set_bytes,NULL,NULL,hash,detail)==SOURCE_DIGEST_OK && !memcmp(hash,expected.set_sha256,32));
    BrowsePreviewSet *set=NULL; CHECK(browse_preview_set_decode(wire,(size_t)expected.set_bytes,actual,SIZE_MAX,0,SIZE_MAX,(BrowsePreviewSetControl){0},&set)==BROWSE_PREVIEW_SET_OK);
    const Page *page=browse_preview_page(browse_preview_set_asset(set,0)); CHECK(page->cells[0].is_null && !page->cells[0].text && !strcmp(page->cells[1].text,"") && !strcmp(page->cells[2].text,"NULL") && !strcmp(page->cells[3].text,"9007199254740993.012345678901234567"));
    browse_preview_set_free(set); free(wire); duckdb_disconnect(&c); duckdb_close(&db); duckdb_destroy_config(&config); CHECK(!error);
}
typedef struct { BrowsePreviewSet *set; BrowsePreviewSet *prior; size_t reserved; bool cancel; unsigned called; BrowsePreviewComponentStatus status; } Read;
static bool cancel_read(void *context,BrowsePreviewComponentPhase phase,size_t entry,uint64_t bytes) { (void)entry; (void)bytes; return ((Read *)context)->cancel && phase==BROWSE_COMPONENT_RECHECK; }
static bool read_component(void *context,WorkbookArchiveReadScope *scope,duckdb_connection c,const Workbook *book,const WorkbookVersionCoreSet *versions,char **error) {
    Read *r=context; r->called++; BrowsePreviewComponentInfo info={0};
    CHECK(browse_preview_component_probe(c,UINT64_MAX,SIZE_MAX,&info,error)==BROWSE_COMPONENT_READY);
    CHECK(info.snapshot_revision==book->command_revision && info.set_bytes>DX_VERSION_CHUNK_BYTES);
    BrowsePreviewComponentInfo sentinel=info;
    CHECK(browse_preview_component_probe(c,info.target_bytes-1,SIZE_MAX,&info,error)==BROWSE_COMPONENT_LIMIT && !memcmp(&info,&sentinel,sizeof(info)));
    CHECK(browse_preview_component_probe(c,UINT64_MAX,(size_t)info.set_bytes-1,&info,error)==BROWSE_COMPONENT_LIMIT && !memcmp(&info,&sentinel,sizeof(info)));
    r->status=browse_preview_component_read(scope,c,book,versions,SIZE_MAX,SIZE_MAX,0,r->reserved,(BrowsePreviewComponentControl){cancel_read,r},&r->set,error);
    if (r->status!=BROWSE_COMPONENT_READY) { CHECK(r->set==r->prior); return false; }
    CHECK(browse_preview_set_info(r->set).snapshot_revision==book->command_revision);
    CHECK(browse_preview_set_manifest(r->set,0)->admission.classification==BROWSE_PREVIEW_CAPTURED_ONLY);
    return true;
}
static void archive_readback(const char *path,const char *parent) {
    Read read={.reserved=SIZE_MAX}; WorkbookArchiveCandidate *candidate=NULL; char *error=NULL;
    CHECK(workbook_archive_read(path,parent,UINT64_C(64)*1024*1024,(WorkbookArchiveReadControl){.admit=read_component,.context=&read},&candidate,&error)==ARCHIVE_READ_READY && !error && read.called==1 && read.status==BROWSE_COMPONENT_READY);
    CHECK(browse_preview_set_info(read.set).count==1); workbook_archive_candidate_free(candidate); candidate=NULL;
    const Page *page=browse_preview_page(browse_preview_set_asset(read.set,0)); CHECK(page->cells[0].is_null && !page->cells[0].text && !strcmp(page->cells[3].text,"9007199254740993.012345678901234567"));
    read.prior=read.set; read.reserved=DX_ARCHIVE_SCOPE_COMPARE_BYTES-1; read.called=0;
    CHECK(workbook_archive_read(path,parent,UINT64_C(64)*1024*1024,(WorkbookArchiveReadControl){.admit=read_component,.context=&read},&candidate,&error)==ARCHIVE_READ_FAILED && !candidate && read.called==1 && read.status==BROWSE_COMPONENT_LIMIT && read.set==read.prior); if (error) { free(error); error=NULL; }
    read.reserved=SIZE_MAX; read.cancel=true; read.called=0;
    CHECK(workbook_archive_read(path,parent,UINT64_C(64)*1024*1024,(WorkbookArchiveReadControl){.admit=read_component,.context=&read},&candidate,&error)==ARCHIVE_READ_FAILED && !candidate && read.called==1 && read.status==BROWSE_COMPONENT_CANCELLED && read.set==read.prior); if (error) { free(error); error=NULL; }
    browse_preview_set_free(read.set);
}
static void mutate(const char *path,const char *sql) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result result={0};
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,sql,&result)==DuckDBSuccess); duckdb_destroy_result(&result); duckdb_disconnect(&c); duckdb_close(&db);
}
static bool reject_component(void *context,WorkbookArchiveReadScope *scope,duckdb_connection c,const Workbook *book,const WorkbookVersionCoreSet *versions,char **error) {
    Read *r=context; r->called++;
    r->status=browse_preview_component_read(scope,c,book,versions,SIZE_MAX,SIZE_MAX,0,SIZE_MAX,(BrowsePreviewComponentControl){0},&r->set,error);
    CHECK(r->set==r->prior); return false;
}
static void corruption(const char *path,const char *parent,const BrowsePreviewSet *original) {
    Read r={.set=browse_preview_set_retain(original)}; r.prior=r.set;
    static const char *bad[]={
        "UPDATE _duxcel_previews.meta SET codec=0",
        "UPDATE _duxcel_previews.meta SET target_hash=NULL",
        "CREATE TABLE _duxcel_previews.unknown(value INTEGER)",
        "INSERT INTO _duxcel_previews.meta SELECT * FROM _duxcel_previews.meta",
        "UPDATE _duxcel_previews.chunks SET position=position+1",
        "INSERT INTO _duxcel_previews.chunks SELECT * FROM _duxcel_previews.chunks WHERE position=0",
        "UPDATE _duxcel_previews.meta SET snapshot_revision=snapshot_revision+1",
        "UPDATE _duxcel_previews.meta SET target_hash=from_hex(repeat('00',32))",
        "INSERT INTO _duxcel_workspace.cells VALUES(0,900,30,'changed body with same doc and revision')"
    };
    static const char *undo[]={
        "UPDATE _duxcel_previews.meta SET codec=1",
        "", /* restore the actual target hash from the immutable retained index */
        "DROP TABLE _duxcel_previews.unknown",
        "DELETE FROM _duxcel_previews.meta WHERE rowid NOT IN (SELECT min(rowid) FROM _duxcel_previews.meta)",
        "UPDATE _duxcel_previews.chunks SET position=position-1",
        "DELETE FROM _duxcel_previews.chunks WHERE rowid NOT IN (SELECT min(rowid) FROM _duxcel_previews.chunks GROUP BY position)",
        "UPDATE _duxcel_previews.meta SET snapshot_revision=snapshot_revision-1",
        "", "DELETE FROM _duxcel_workspace.cells WHERE row_id=900 AND column_id=30"
    };
    /* Preserve the actual descriptor bytes, never replace authority with a
       guessed root. Temporary SQL mutations touch only this private artifact. */
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result result={0};
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,"SELECT hex(target_hash) FROM _duxcel_previews.meta",&result)==DuckDBSuccess);
    char *target=duckdb_value_varchar(&result,0,0); CHECK(target && strlen(target)==64); char *restore=dx_printf("UPDATE _duxcel_previews.meta SET target_hash=from_hex('%s')",target); duckdb_free(target); duckdb_destroy_result(&result); duckdb_disconnect(&c); duckdb_close(&db);
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        mutate(path,bad[i]); WorkbookArchiveCandidate *candidate=NULL; char *error=NULL; r.called=0;
        CHECK(workbook_archive_read(path,parent,UINT64_C(64)*1024*1024,(WorkbookArchiveReadControl){.admit=reject_component,.context=&r},&candidate,&error)==ARCHIVE_READ_FAILED && !candidate && r.called==1 && r.status!=BROWSE_COMPONENT_READY && r.set==r.prior);
        if (error) free(error); mutate(path,*undo[i] ? undo[i] : restore);
    }
    /* Same-length corrupt payload must fail its hash; restore exact immutable
       DXPS bytes using the same canonical chunk boundaries. */
    mutate(path,"UPDATE _duxcel_previews.chunks SET data=from_hex(repeat('00',octet_length(data))) WHERE position=0");
    WorkbookArchiveCandidate *candidate=NULL; char *error=NULL; r.called=0;
    CHECK(workbook_archive_read(path,parent,UINT64_C(64)*1024*1024,(WorkbookArchiveReadControl){.admit=reject_component,.context=&r},&candidate,&error)==ARCHIVE_READ_FAILED && !candidate && r.called==1 && r.status!=BROWSE_COMPONENT_READY && r.set==r.prior); if (error) free(error);
    BrowsePreviewPacket packet={0}; CHECK(browse_preview_set_encode(original,SIZE_MAX,&packet)==BROWSE_PREVIEW_SET_OK);
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess); duckdb_prepared_statement statement=NULL;
    CHECK(duckdb_prepare(c,"UPDATE _duxcel_previews.chunks SET data=? WHERE position=0",&statement)==DuckDBSuccess);
    CHECK(duckdb_bind_blob(statement,1,packet.data,DX_VERSION_CHUNK_BYTES)==DuckDBSuccess && duckdb_execute_prepared(statement,&result)==DuckDBSuccess);
    duckdb_destroy_result(&result); duckdb_destroy_prepare(&statement); duckdb_disconnect(&c); duckdb_close(&db); browse_preview_packet_free(&packet);
    archive_readback(path,parent); free(restore); browse_preview_set_free(r.set);
}
int main(void) {
    char parent[]="build/tests/browse-preview-component-XXXXXX"; CHECK(mkdtemp(parent));
    char *path=dx_printf("%s/preview.dxls",parent),*second=dx_printf("%s/reused.dxls",parent),*protected=dx_printf("%s/protected.dxls",parent); char *source=realpath("tests/fixtures/transactions.csv",NULL); CHECK(source);
    Workbook book; workbook_init(&book); char *error=NULL; CHECK(workbook_add(&book,"Connected",&(Analysis){.kind=SOURCE_CSV,.path=source},&error)); book.command_revision=17;
    BrowsePreviewSet *set=preview(&book); Stage state={.actual=&book,.set=set}; WorkbookVersionReceipt *receipt=NULL; WorkbookVersionCoreSet *base=NULL;
    WorkbookVersionSaveRequest request={.head=&book,.owner={book.id.high,book.id.low,441,VERSION_WORKING_HEAD},.purpose=VERSION_USER_SAVE,.path=path,.parent=parent,.reserved_bytes=UINT64_C(64)*1024*1024};
    /* The module does not activate the normal archive reader/registry. Stage
       successful auxiliaries into fresh artifacts; rollback attempts protect an
       ordinary current artifact that has no unadvertised component. */
    request.path=protected;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !error);
    workbook_version_receipt_free(receipt); receipt=NULL; request.path=path;
    WorkbookVersionSaveControl control={.publication={stage,cancelled,&state}};
    CHECK(workbook_version_save(request,control,&receipt,&error)==WORKBOOK_SAVE_COMPLETE && receipt && !error && state.status==BROWSE_COMPONENT_READY && state.prepared==1 && !state.cancelled && !state.reused);
    readback(path,&book,state.info); archive_readback(path,parent); base=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt); receipt=NULL;
    request.base=base; state.base=base; state.prepared=0; request.path=second;
    CHECK(workbook_version_save(request,control,&receipt,&error)==WORKBOOK_SAVE_COMPLETE && receipt && !error && state.reused && state.prepared==1);
    readback(second,&book,state.info); archive_readback(second,parent); corruption(second,parent,set); workbook_version_receipt_free(receipt); receipt=NULL; request.path=protected;
    SourceDigest before=digest(protected); size_t inventory=children(parent);
    for (Mode mode=TOO_SMALL;mode<=DUPLICATE;mode++) {
        state.mode=mode; state.prepared=state.cancelled=0;
        CHECK(workbook_version_save(request,control,&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt && state.prepared==1 && state.cancelled==1);
        if (mode==TOO_SMALL || mode==DISK_SMALL) CHECK(state.status==BROWSE_COMPONENT_LIMIT);
        else if (mode!=DUPLICATE) CHECK(state.status==BROWSE_COMPONENT_CANCELLED);
        if (error) { free(error); error=NULL; }
        SourceDigest after=digest(protected); CHECK(before.bytes==after.bytes && !memcmp(before.sha256,after.sha256,32) && children(parent)==inventory);
    }
    state.mode=REJECT_INTENT; state.prepared=state.cancelled=0;
    CHECK(workbook_version_tag_save(request,(WorkbookVersionText){"Preview tag",11},control,&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt && state.prepared==1 && state.cancelled==1 && state.status==BROWSE_COMPONENT_UNSUPPORTED); if (error) { free(error); error=NULL; }
    request.purpose=VERSION_RECOVERY_CHECKPOINT; state.prepared=0;
    CHECK(workbook_version_save(request,control,&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt && !state.prepared); if (error) { free(error); error=NULL; }
    request.purpose=VERSION_STATE_EXPORT;
    CHECK(workbook_version_save(request,control,&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt && !state.prepared); if (error) { free(error); error=NULL; }
    CHECK(children(parent)==inventory); SourceDigest after=digest(protected); CHECK(before.bytes==after.bytes && !memcmp(before.sha256,after.sha256,32));
    workbook_version_core_set_free(base); browse_preview_set_free(set); workbook_free(&book); free(source); CHECK(!unlink(path) && !unlink(second) && !unlink(protected)); free(path); free(second); free(protected); CHECK(!rmdir(parent));
    printf("PASS: %u genuine native User Save preview stage checks\n",checks); return 0;
}
