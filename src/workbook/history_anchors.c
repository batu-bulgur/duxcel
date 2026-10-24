#include "history_anchors.h"
#include <string.h>
static bool key_equal(WorkbookVersionKey a,WorkbookVersionKey b) { return !memcmp(a.bytes,b.bytes,DX_VERSION_KEY_BYTES); }
static size_t find_anchor(const WorkbookHistoryAnchors *h,uint64_t id) {
    for (size_t i=0;i<h->count;i++) if (h->items[i].boundary.anchor==id) return i;
    return h->count;
}
static bool has_root(const WorkbookVersionIndex *v,WorkbookVersionKey key) {
    for (size_t i=0;i<v->root_count;i++) if (key_equal(v->roots[i].key,key)) return true;
    return false;
}
WorkbookHistoryAnchorsStatus workbook_history_anchors_check(const WorkbookVersionIndex *v,const WorkbookHistoryAnchors *h) {
    if (workbook_version_index_check(v)!=VERSION_OK) return HISTORY_ANCHORS_INDEX;
    if (!h || h->document_high!=v->document_high || h->document_low!=v->document_low) return HISTORY_ANCHORS_INVALID;
    if (h->count>DX_HISTORY_PUBLIC_ANCHORS || h->tag_count>DX_VERSION_TAGS) return HISTORY_ANCHORS_LIMIT;
    if ((h->count && !h->items) || (h->tag_count && !h->tags) || h->tag_count!=v->tag_count || h->count>v->tag_count+(v->saved.present ? 1u : 0u)) return HISTORY_ANCHORS_INVALID;
    if (!v->saved.present) return !h->count && !h->tag_count && !h->saved_anchor ? HISTORY_ANCHORS_OK : HISTORY_ANCHORS_INVALID;
    bool referenced[DX_HISTORY_PUBLIC_ANCHORS]={0};
    for (size_t i=0;i<h->count;i++) {
        const WorkbookHistoryBoundary *b=&h->items[i].boundary;
        if (b->anchor>h->high_water || b->anchor>h->saved_anchor || !has_root(v,b->root)) return HISTORY_ANCHORS_INVALID;
        for (size_t j=0;j<i;j++) if (h->items[j].boundary.anchor==b->anchor) return HISTORY_ANCHORS_INVALID;
    }
    size_t saved=find_anchor(h,h->saved_anchor);
    if (saved==h->count || !key_equal(h->items[saved].boundary.root,v->saved.key)) return HISTORY_ANCHORS_INVALID;
    referenced[saved]=true;
    for (size_t i=0;i<h->tag_count;i++) {
        const WorkbookHistoryTagAnchor *binding=h->tags+i; const WorkbookVersionTag *tag=NULL;
        for (size_t j=0;j<i;j++) if (h->tags[j].tag_created==binding->tag_created) return HISTORY_ANCHORS_INVALID;
        for (size_t j=0;j<v->tag_count;j++) if (v->tags[j].created==binding->tag_created) { tag=v->tags+j; break; }
        size_t at=find_anchor(h,binding->anchor);
        if (!tag || at==h->count || !key_equal(tag->root,h->items[at].boundary.root)) return HISTORY_ANCHORS_INVALID;
        referenced[at]=true;
    }
    for (size_t i=0;i<h->count;i++) if (!referenced[i]) return HISTORY_ANCHORS_INVALID;
    return HISTORY_ANCHORS_OK;
}
