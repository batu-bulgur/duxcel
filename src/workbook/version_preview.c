#include "version_preview.h"
#include <stdlib.h>
#include <string.h>
struct WorkbookVersionPreview {
    Workbook *book;
    WorkbookVersionCoreSet *store;
    WorkbookVersionOwner owner;
    WorkbookVersionKey root;
};
typedef struct { WorkbookVersionPreviewControl control; WorkbookVersionPreviewPhase phase; bool stopped; } Read;
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool cancelled(void *context,uint64_t bytes) {
    Read *r=context;
    if (!r->stopped && r->control.cancelled) r->stopped=r->control.cancelled(r->control.context,r->phase,bytes);
    return r->stopped;
}
const Workbook *workbook_version_preview_book(const WorkbookVersionPreview *p) { return p ? p->book : NULL; }
const WorkbookVersionCoreSet *workbook_version_preview_versions(const WorkbookVersionPreview *p) { return p ? p->store : NULL; }
const WorkbookVersionOwner *workbook_version_preview_owner(const WorkbookVersionPreview *p) { return p ? &p->owner : NULL; }
const WorkbookVersionKey *workbook_version_preview_root(const WorkbookVersionPreview *p) { return p ? &p->root : NULL; }
Workbook *workbook_version_preview_take_book(WorkbookVersionPreview *p) { if (!p) return NULL; Workbook *b=p->book; p->book=NULL; return b; }
void workbook_version_preview_free(WorkbookVersionPreview *p) {
    if (!p) return;
    if (p->book) { workbook_free(p->book); free(p->book); }
    workbook_version_core_set_free(p->store); free(p);
}
WorkbookVersionPreviewResult workbook_version_preview_prepare(const WorkbookVersionCoreSet *store,
    WorkbookVersionKey root,WorkbookVersionOwner reader,WorkbookVersionPreviewControl control,
    WorkbookVersionPreview **out,char **error) {
    const WorkbookVersionIndex *index=workbook_version_core_set_index(store);
    if (!out || *out || !index || workbook_version_index_check(index)!=VERSION_OK || !reader.session || reader.mode!=VERSION_HISTORICAL_VIEW ||
        reader.document_high!=index->document_high || reader.document_low!=index->document_low) {
        fail(error,"Choose an admitted version store, matching historical reader and empty preview."); return VERSION_PREVIEW_FAILED;
    }
    size_t at=0; while (at<index->root_count && memcmp(index->roots[at].key.bytes,root.bytes,DX_VERSION_KEY_BYTES)) at++;
    if (at==index->root_count) { fail(error,"The requested historical root is not retained in this workbook."); return VERSION_PREVIEW_FAILED; }
    WorkbookVersionPreview *p=dx_alloc(sizeof(*p)); p->store=workbook_version_core_set_retain(store); p->owner=reader; p->root=root;
    if (!p->store) { workbook_version_preview_free(p); fail(error,"Cannot retain the historical reader's protected roots."); return VERSION_PREVIEW_FAILED; }
    Read r={.control=control,.phase=VERSION_PREVIEW_VERIFY}; WorkbookVersionPreviewResult result=VERSION_PREVIEW_FAILED;
    const WorkbookCorePayload *leaf=workbook_version_core_set_leaf(p->store,at);
    if (cancelled(&r,0)) goto done;
    if (workbook_core_payload_verify(leaf,cancelled,&r,error)!=CORE_PAYLOAD_READY) goto done;
    r.phase=VERSION_PREVIEW_LOAD; if (cancelled(&r,0)) goto done;
    p->book=dx_alloc(sizeof(*p->book)); workbook_init(p->book);
    if (!workbook_load_core_payload(p->book,workbook_core_payload_path(leaf),error)) goto done;
    if (p->book->id.high!=reader.document_high || p->book->id.low!=reader.document_low) { fail(error,"The historical core belongs to another document."); goto done; }
    r.phase=VERSION_PREVIEW_RECHECK; if (cancelled(&r,0)) goto done;
    if (workbook_core_payload_verify(leaf,cancelled,&r,error)!=CORE_PAYLOAD_READY) goto done;
    free(p->book->path); p->book->path=NULL; p->book->dirty=false;
    r.phase=VERSION_PREVIEW_READY; if (cancelled(&r,0)) goto done;
    *out=p; p=NULL; result=VERSION_PREVIEW_PREPARED;
done:
    workbook_version_preview_free(p);
    if (r.stopped) { fail(error,"Historical preview cancelled; the working state is unchanged."); result=VERSION_PREVIEW_CANCELLED; }
    return result;
}
