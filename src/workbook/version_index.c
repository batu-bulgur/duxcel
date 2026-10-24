#include "version_index.h"
#include <string.h>

static bool key_equal(WorkbookVersionKey a,WorkbookVersionKey b) {
    return !memcmp(a.bytes,b.bytes,DX_VERSION_KEY_BYTES);
}
static bool ref_equal(WorkbookVersionRef a,WorkbookVersionRef b) {
    return a.present==b.present && key_equal(a.key,b.key);
}
static bool root_equal(WorkbookVersionRoot a,WorkbookVersionRoot b) {
    return key_equal(a.key,b.key) && a.bytes==b.bytes && a.encoding==b.encoding;
}
static bool root_valid(const WorkbookVersionRoot *root) {
    return root && root->bytes && root->encoding;
}
static const WorkbookVersionRoot *find_root(const WorkbookVersionIndex *index,WorkbookVersionKey key) {
    for (size_t i=0;i<index->root_count;i++) if (key_equal(index->roots[i].key,key)) return index->roots+i;
    return NULL;
}
static bool label_valid(WorkbookVersionText text) {
    if (!text.data || !text.bytes || text.bytes>DX_VERSION_LABEL_BYTES) return false;
    const unsigned char *p=(const unsigned char *)text.data;
    bool visible=false;
    for (size_t i=0;i<text.bytes;) {
        unsigned char c=p[i++]; size_t following;
        if (c<0x80) {
            if (c<0x20 || c==0x7f) return false;
            visible|=c!=' '; continue;
        }
        visible=true;
        if (c>=0xc2 && c<=0xdf) following=1;
        else if (c>=0xe0 && c<=0xef) following=2;
        else if (c>=0xf0 && c<=0xf4) following=3;
        else return false;
        if (following>text.bytes-i) return false;
        unsigned char first=p[i];
        if ((c==0xe0 && first<0xa0) || (c==0xed && first>=0xa0) ||
            (c==0xf0 && first<0x90) || (c==0xf4 && first>=0x90)) return false;
        for (size_t j=0;j<following;j++) if ((p[i+j]&0xc0)!=0x80) return false;
        i+=following;
    }
    return visible;
}
static bool label_equal(WorkbookVersionText a,WorkbookVersionText b) {
    return a.bytes==b.bytes && !memcmp(a.data,b.data,a.bytes);
}
WorkbookVersionStatus workbook_version_index_check(const WorkbookVersionIndex *index) {
    if (!index || (!index->document_high && !index->document_low)) return VERSION_INVALID_INDEX;
    if (index->root_count>DX_VERSION_ROOTS || index->tag_count>DX_VERSION_TAGS) return VERSION_LIMIT;
    if ((index->root_count && !index->roots) || (index->tag_count && !index->tags)) return VERSION_INVALID_INDEX;
    if (!index->root_count) {
        WorkbookVersionKey zero={{0}};
        return !index->generation && !index->saved_generation && !index->saved.present &&
            key_equal(index->saved.key,zero) && !index->tag_count ? VERSION_OK : VERSION_INVALID_INDEX;
    }
    if (!index->generation || !index->saved.present || !index->saved_generation ||
        index->saved_generation>index->generation) return VERSION_INVALID_INDEX;
    uint64_t bytes=0;
    for (size_t i=0;i<index->root_count;i++) {
        const WorkbookVersionRoot *root=index->roots+i;
        if (!root_valid(root)) return VERSION_INVALID_ROOT;
        if (root->bytes>UINT64_MAX-bytes) return VERSION_LIMIT;
        bytes+=root->bytes;
        for (size_t j=0;j<i;j++) if (key_equal(root->key,index->roots[j].key)) return VERSION_ROOT_CONFLICT;
    }
    if (!find_root(index,index->saved.key)) return VERSION_INVALID_INDEX;
    for (size_t i=0;i<index->tag_count;i++) {
        const WorkbookVersionTag *tag=index->tags+i;
        if (!label_valid(tag->label)) return VERSION_INVALID_LABEL;
        if (!tag->created || tag->created>index->saved_generation || !find_root(index,tag->root)) return VERSION_INVALID_INDEX;
        for (size_t j=0;j<i;j++) {
            if (label_equal(tag->label,index->tags[j].label)) return VERSION_DUPLICATE_TAG;
            if (tag->created==index->tags[j].created) return VERSION_INVALID_INDEX;
        }
    }
    return VERSION_OK;
}
static bool owner_equal(WorkbookVersionOwner a,WorkbookVersionOwner b) {
    return a.document_high==b.document_high && a.document_low==b.document_low &&
        a.session==b.session && a.mode==b.mode;
}
WorkbookVersionStatus workbook_version_prepare(const WorkbookVersionIndex *index,
    WorkbookVersionOwner owner,WorkbookVersionPurpose purpose,
    const WorkbookVersionRoot *root,WorkbookVersionPlan *out) {
    if (!out) return VERSION_INVALID_INDEX;
    WorkbookVersionStatus status=workbook_version_index_check(index);
    if (status) return status;
    if (!owner.session || owner.document_high!=index->document_high || owner.document_low!=index->document_low ||
        (owner.mode!=VERSION_WORKING_HEAD && owner.mode!=VERSION_HISTORICAL_VIEW)) return VERSION_INVALID_OWNER;
    if (purpose!=VERSION_USER_SAVE && purpose!=VERSION_RECOVERY_CHECKPOINT && purpose!=VERSION_STATE_EXPORT) return VERSION_INVALID_PURPOSE;
    if (owner.mode==VERSION_HISTORICAL_VIEW && purpose!=VERSION_STATE_EXPORT) return VERSION_READ_ONLY;
    WorkbookVersionPlan next={.owner=owner,.purpose=purpose,.generation=index->generation,
        .next_generation=index->generation,.saved_generation=index->saved_generation,
        .next_saved_generation=index->saved_generation,.root_count=index->root_count,
        .tag_count=index->tag_count,.before_saved=index->saved,.after_saved=index->saved};
    if (purpose!=VERSION_USER_SAVE) {
        if (root) return VERSION_INVALID_ROOT;
        *out=next; return VERSION_OK;
    }
    if (!root_valid(root)) return VERSION_INVALID_ROOT;
    const WorkbookVersionRoot *existing=find_root(index,root->key);
    if (existing && !root_equal(*existing,*root)) return VERSION_ROOT_CONFLICT;
    next.root=*root; next.adds_root=!existing;
    next.updates_saved=!index->saved.present || !key_equal(index->saved.key,root->key);
    if (next.adds_root) {
        if (index->root_count==DX_VERSION_ROOTS) return VERSION_LIMIT;
        uint64_t bytes=root->bytes;
        for (size_t i=0;i<index->root_count;i++) {
            if (index->roots[i].bytes>UINT64_MAX-bytes) return VERSION_LIMIT;
            bytes+=index->roots[i].bytes;
        }
    }
    if (next.updates_saved) {
        if (index->generation==UINT64_MAX) return VERSION_EXHAUSTED;
        next.next_generation=index->generation+1;
        next.next_saved_generation=next.next_generation;
        next.after_saved=(WorkbookVersionRef){.present=true,.key=root->key};
    }
    *out=next; return VERSION_OK;
}
bool workbook_version_can_publish(const WorkbookVersionIndex *index,
    WorkbookVersionOwner owner,const WorkbookVersionPlan *plan) {
    if (!index || !plan || !owner_equal(owner,plan->owner) ||
        index->generation!=plan->generation || index->saved_generation!=plan->saved_generation ||
        index->root_count!=plan->root_count || index->tag_count!=plan->tag_count ||
        !ref_equal(index->saved,plan->before_saved)) return false;
    WorkbookVersionPlan next;
    if (workbook_version_prepare(index,owner,plan->purpose,
        plan->purpose==VERSION_USER_SAVE ? &plan->root : NULL,&next)!=VERSION_OK) return false;
    return next.next_generation==plan->next_generation &&
        next.next_saved_generation==plan->next_saved_generation &&
        ref_equal(next.after_saved,plan->after_saved) && root_equal(next.root,plan->root) &&
        next.adds_root==plan->adds_root && next.updates_saved==plan->updates_saved;
}
const char *workbook_version_message(WorkbookVersionStatus status) {
    switch (status) {
    case VERSION_OK: return "Version index is ready.";
    case VERSION_INVALID_INDEX: return "Invalid or unresolved workbook version metadata.";
    case VERSION_INVALID_ROOT: return "A complete version payload descriptor is required.";
    case VERSION_INVALID_LABEL: return "Use a nonblank version label with 1-256 UTF-8 bytes and no control characters.";
    case VERSION_DUPLICATE_TAG: return "Version labels must be unique.";
    case VERSION_LIMIT: return "The version index storage limit was reached; no retained state was removed.";
    case VERSION_EXHAUSTED: return "The version publication counter is exhausted.";
    case VERSION_READ_ONLY: return "Historical versions can only be exported.";
    case VERSION_INVALID_OWNER: return "The workbook write owner changed.";
    case VERSION_INVALID_PURPOSE: return "Unknown workbook version publication purpose.";
    case VERSION_ROOT_CONFLICT: return "Conflicting immutable version payload metadata.";
    }
    return "Unknown version metadata error.";
}
