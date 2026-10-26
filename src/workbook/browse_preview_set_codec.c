#include "browse_preview_set_codec.h"
#include "browse_preview_set_internal.h"
#include "browse_preview_internal.h"
#include "definition_wire.h"
#include <stdlib.h>
#include <string.h>
static bool add(size_t *n,size_t x) { if (*n>SIZE_MAX-x) return false; *n+=x; return true; }
static BrowsePreviewSetStatus mapped(BrowsePreviewStatus s) {
    return s==BROWSE_PREVIEW_OK ? BROWSE_PREVIEW_SET_OK : s==BROWSE_PREVIEW_LIMIT ? BROWSE_PREVIEW_SET_LIMIT :
        s==BROWSE_PREVIEW_UNSUPPORTED ? BROWSE_PREVIEW_SET_UNSUPPORTED : BROWSE_PREVIEW_SET_INVALID;
}
static uint64_t get(const unsigned char *p,size_t n) { uint64_t v=0; for (size_t i=0;i<n;i++) v|=(uint64_t)p[i]<<(8*i); return v; }
BrowsePreviewSetStatus browse_preview_set_encode_measure(const BrowsePreviewSet *set,const BrowsePreviewPacket *prior,BrowsePreviewSetWireInfo *out) {
    if (!set || !out || set->info.scope!=BROWSE_PREVIEW_AUXILIARY_CURRENT_ARTIFACT || set->info.count>DX_BROWSE_PREVIEW_SET_ENTRIES ||
        (prior && ((prior->bytes && !prior->data) || (!prior->bytes && prior->data)))) return BROWSE_PREVIEW_SET_INVALID;
    BrowsePreviewSetWireInfo info={.set=set->info,.bytes=DX_BROWSE_PREVIEW_SET_HEADER_BYTES+set->info.count*DX_BROWSE_PREVIEW_SET_ENTRY_BYTES};
    for (size_t i=0;i<set->info.count;i++) {
        BrowsePreviewWireInfo child={0}; BrowsePreviewSetStatus s=mapped(browse_preview_encode_measure(set->entries[i].asset,NULL,&child));
        if (s!=BROWSE_PREVIEW_SET_OK) return s;
        if (child.bytes!=set->entries[i].manifest.encoded_bytes) return BROWSE_PREVIEW_SET_INVALID;
        if (!add(&info.bytes,child.bytes)) return BROWSE_PREVIEW_SET_LIMIT;
        if (child.bytes>info.validation_scratch) info.validation_scratch=child.bytes;
    }
    info.coexistence=info.bytes;
    if (!add(&info.coexistence,set->info.owned) || !add(&info.coexistence,prior ? prior->bytes : 0) || !add(&info.coexistence,info.validation_scratch)) return BROWSE_PREVIEW_SET_LIMIT;
    *out=info; return BROWSE_PREVIEW_SET_OK;
}
BrowsePreviewSetStatus browse_preview_set_encode(const BrowsePreviewSet *set,size_t reserved,BrowsePreviewPacket *out) {
    if (!out) return BROWSE_PREVIEW_SET_INVALID;
    BrowsePreviewSetWireInfo info={0}; BrowsePreviewSetStatus s=browse_preview_set_encode_measure(set,out,&info);
    if (s!=BROWSE_PREVIEW_SET_OK) return s;
    if (reserved<info.coexistence) return BROWSE_PREVIEW_SET_LIMIT;
    BrowsePreviewPacket next={dx_alloc(info.bytes),info.bytes}; DefinitionWireWriter w={next.data};
    memcpy(w.p,"DXPS",4); w.p+=4; definition_wire_put(&w,DX_BROWSE_PREVIEW_SET_ENCODING,2); definition_wire_put(&w,0,2);
    definition_wire_put(&w,set->info.document.high,8); definition_wire_put(&w,set->info.document.low,8); definition_wire_put(&w,set->info.snapshot_revision,8);
    definition_wire_put(&w,set->info.count,4); definition_wire_put(&w,set->info.scope,4); definition_wire_put(&w,set->info.count*DX_BROWSE_PREVIEW_SET_ENTRY_BYTES,4); definition_wire_put(&w,0,4);
    definition_wire_put(&w,info.bytes,8); definition_wire_put(&w,0,8);
    for (size_t i=0;i<set->info.count;i++) {
        const BrowsePreviewManifestEntry *entry=&set->entries[i].manifest;
        definition_wire_put(&w,entry->sheet,8); definition_wire_put(&w,entry->owner,8); definition_wire_put(&w,entry->encoded_bytes,8); definition_wire_put(&w,0,8);
    }
    for (size_t i=0;i<set->info.count;i++) {
        BrowsePreviewPacket child={0}; s=mapped(browse_preview_encode(set->entries[i].asset,SIZE_MAX,&child));
        if (s!=BROWSE_PREVIEW_SET_OK) { browse_preview_packet_free(&next); return s; }
        memcpy(w.p,child.data,child.bytes); w.p+=child.bytes; browse_preview_packet_free(&child);
    }
    browse_preview_packet_free(out); *out=next; return BROWSE_PREVIEW_SET_OK;
}
BrowsePreviewSetStatus browse_preview_set_read_measure(const void *data,size_t bytes,const Workbook *actual,
    const BrowsePreviewSet *prior,size_t retained_limit,size_t other_owned,BrowsePreviewSetWireInfo *out) {
    if (!data || !out || bytes<DX_BROWSE_PREVIEW_SET_HEADER_BYTES || !actual || !workbook_identity_valid(actual)) return BROWSE_PREVIEW_SET_INVALID;
    const unsigned char *p=data;
    if (memcmp(p,"DXPS",4)) return BROWSE_PREVIEW_SET_INVALID;
    if (get(p+4,2)!=DX_BROWSE_PREVIEW_SET_ENCODING) return BROWSE_PREVIEW_SET_UNSUPPORTED;
    if (get(p+6,2) || get(p+44,4) || get(p+56,8) || get(p+48,8)!=bytes) return BROWSE_PREVIEW_SET_INVALID;
    if (get(p+36,4)!=BROWSE_PREVIEW_AUXILIARY_CURRENT_ARTIFACT) return BROWSE_PREVIEW_SET_UNSUPPORTED;
    BrowsePreviewSetWireInfo info={.bytes=bytes};
    info.set=(BrowsePreviewSetInfo){.document={get(p+8,8),get(p+16,8)},.snapshot_revision=get(p+24,8),
        .scope=BROWSE_PREVIEW_AUXILIARY_CURRENT_ARTIFACT,.count=(size_t)get(p+32,4),.owned=sizeof(BrowsePreviewSet)};
    if ((!info.set.document.high && !info.set.document.low)) return BROWSE_PREVIEW_SET_INVALID;
    if (info.set.document.high!=actual->id.high || info.set.document.low!=actual->id.low || info.set.snapshot_revision!=actual->command_revision) return BROWSE_PREVIEW_SET_ASSOCIATION;
    size_t count=info.set.count; if (count>DX_BROWSE_PREVIEW_SET_ENTRIES) return BROWSE_PREVIEW_SET_LIMIT;
    size_t directory=count*DX_BROWSE_PREVIEW_SET_ENTRY_BYTES,at=DX_BROWSE_PREVIEW_SET_HEADER_BYTES;
    if (get(p+40,4)!=directory || directory>bytes-at) return BROWSE_PREVIEW_SET_INVALID;
    at+=directory; info.set.capacity=count; info.parser_owned=count*sizeof(BrowsePreview *);
    if (!add(&info.set.owned,count*sizeof(BrowsePreviewSetEntry))) return BROWSE_PREVIEW_SET_LIMIT;
    uint64_t last_sheet=0,last_owner=0;
    for (size_t i=0;i<count;i++) {
        const unsigned char *entry=p+DX_BROWSE_PREVIEW_SET_HEADER_BYTES+i*DX_BROWSE_PREVIEW_SET_ENTRY_BYTES;
        uint64_t sheet=get(entry,8),owner=get(entry+8,8),length=get(entry+16,8);
        if (!sheet || get(entry+24,8) || (i && (sheet<last_sheet || (sheet==last_sheet && owner<=last_owner))) || length>bytes-at) return BROWSE_PREVIEW_SET_INVALID;
        BrowsePreviewWireInfo child={0}; BrowsePreviewSetStatus s=mapped(browse_preview_preflight(p+at,(size_t)length,&child));
        if (s!=BROWSE_PREVIEW_SET_OK) return s;
        if (child.preview.sheet!=sheet || child.preview.owner!=owner || child.preview.document.high!=info.set.document.high || child.preview.document.low!=info.set.document.low) return BROWSE_PREVIEW_SET_INVALID;
        if (!add(&info.set.assets_owned,child.owned) || !add(&info.set.owned,child.owned)) return BROWSE_PREVIEW_SET_LIMIT;
        if (child.validation_scratch>info.validation_scratch) info.validation_scratch=child.validation_scratch;
        last_sheet=sheet; last_owner=owner; at+=(size_t)length;
    }
    if (at!=bytes) return BROWSE_PREVIEW_SET_INVALID;
    if (info.set.owned>retained_limit) return BROWSE_PREVIEW_SET_LIMIT;
    info.set.validation_scratch=info.validation_scratch;
    info.set.coexistence=info.set.owned;
    if (!add(&info.set.coexistence,prior ? prior->info.owned : 0) || !add(&info.set.coexistence,other_owned) || !add(&info.set.coexistence,info.validation_scratch)) return BROWSE_PREVIEW_SET_LIMIT;
    info.coexistence=info.set.coexistence;
    if (!add(&info.coexistence,bytes) || !add(&info.coexistence,info.parser_owned)) return BROWSE_PREVIEW_SET_LIMIT;
    *out=info; return BROWSE_PREVIEW_SET_OK;
}
BrowsePreviewSetStatus browse_preview_set_decode(const void *data,size_t bytes,const Workbook *actual,
    size_t retained_limit,size_t other_owned,size_t reserved,BrowsePreviewSetControl control,BrowsePreviewSet **out) {
    if (!out) return BROWSE_PREVIEW_SET_INVALID;
    BrowsePreviewSetWireInfo info={0}; BrowsePreviewSetStatus s=browse_preview_set_read_measure(data,bytes,actual,*out,retained_limit,other_owned,&info);
    if (s!=BROWSE_PREVIEW_SET_OK) return s;
    if (reserved<info.coexistence) return BROWSE_PREVIEW_SET_LIMIT;
    BrowsePreview **assets=info.set.count ? dx_alloc(info.parser_owned) : NULL;
    const unsigned char *p=data; size_t at=DX_BROWSE_PREVIEW_SET_HEADER_BYTES+info.set.count*DX_BROWSE_PREVIEW_SET_ENTRY_BYTES;
    for (size_t i=0;i<info.set.count;i++) {
        if (control.cancelled && control.cancelled(control.context,i)) { s=BROWSE_PREVIEW_SET_CANCELLED; goto done; }
        size_t length=(size_t)get(p+DX_BROWSE_PREVIEW_SET_HEADER_BYTES+i*DX_BROWSE_PREVIEW_SET_ENTRY_BYTES+16,8);
        s=mapped(browse_preview_decode_staged(p+at,length,SIZE_MAX,assets+i));
        if (s!=BROWSE_PREVIEW_SET_OK) goto done;
        at+=length;
    }
    /* The only semantic child admission, against actual already-loaded owners.
       All fallible association/cancellation precedes reference publication. */
    s=browse_preview_set_take(actual,assets,info.set.count,retained_limit,other_owned,info.set.coexistence,control,out);
 done:
    for (size_t i=0;i<info.set.count;i++) browse_preview_free(assets[i]);
    free(assets); return s;
}
