#include "history_profile_codec.h"
#include <stdlib.h>
#include <string.h>
#define ROOT_BYTES 48u
#define TAG_BYTES 48u
#define ANCHOR_BYTES 64u
#define PATH_BYTES 64u
#define RECORD_BYTES 112u
/* C99 has no max_align_t/_Alignof. This union covers every parsed descriptor's
   scalar/pointer alignment without assuming a platform-specific integer. */
typedef union { long double number; void *pointer; uint64_t integer; } Alignment;
typedef struct { char byte; Alignment value; } Aligned;
#define ALIGNMENT offsetof(Aligned,value)
typedef struct { const unsigned char *data; size_t bytes,at,parse; bool invalid,limit; unsigned char *arena; size_t used; } Reader;
typedef struct {
    WorkbookHistoryProfileView view;
    WorkbookVersionIndex index;
    WorkbookHistoryAnchors anchors;
    WorkbookHistoryPath paths[2];
} Parsed;
static uint64_t number(const void *input,unsigned n) { const unsigned char *p=input; uint64_t v=0; for (unsigned i=0;i<n;i++) v|=(uint64_t)p[i]<<(8*i); return v; }
static void put(unsigned char *p,uint64_t v,unsigned n) { for (unsigned i=0;i<n;i++) p[i]=(unsigned char)(v>>(8*i)); }
static bool add(size_t *n,size_t extra) { if (extra>SIZE_MAX-*n) return false; *n+=extra; return true; }
static bool extent(size_t *n,size_t count,size_t item) { return (!count || item<=SIZE_MAX/count) && add(n,count*item); }
static const unsigned char *take(Reader *r,size_t bytes) {
    if (bytes>r->bytes-r->at) { r->invalid=true; return NULL; }
    const unsigned char *p=r->data+r->at; r->at+=bytes; return p;
}
static bool zero(const unsigned char *p,size_t bytes) { for (size_t i=0;i<bytes;i++) if (p[i]) return false; return true; }
static void *array(Reader *r,size_t count,size_t item) {
    if (!count) return NULL;
    size_t bytes=0; if (!extent(&bytes,count,item) || bytes>SIZE_MAX-(ALIGNMENT-1u)) { r->limit=true; return NULL; }
    bytes=(bytes+ALIGNMENT-1u)/ALIGNMENT*ALIGNMENT;
    if (!add(&r->parse,bytes) || r->parse>DX_HISTORY_PROFILE_PARSE_BYTES) { r->limit=true; return NULL; }
    if (!r->arena) return NULL; void *p=r->arena+r->used; r->used+=bytes; memset(p,0,bytes); return p;
}
static bool key_less(WorkbookVersionKey a,WorkbookVersionKey b) { return memcmp(a.bytes,b.bytes,DX_VERSION_KEY_BYTES)<0; }
/* Read only canonical framing. Semantic validators run once after constructing
   descriptors in an already admitted, independently owned parse arena. */
static WorkbookHistoryProfileCodecStatus parse(const void *data,size_t bytes,void *arena,WorkbookHistoryProfileBinding binding,Parsed *out,WorkbookHistoryProfileWireInfo *info) {
    if (!data || !info || bytes<DX_HISTORY_PROFILE_HEADER_BYTES) return HISTORY_PROFILE_CODEC_INVALID;
    if (bytes>DX_HISTORY_PROFILE_WIRE_BYTES) return HISTORY_PROFILE_CODEC_LIMIT;
    const unsigned char *h=data;
    if (memcmp(h,"DXHP",4) || number(h+4,4)!=DX_HISTORY_PROFILE_ENCODING) return HISTORY_PROFILE_CODEC_UNSUPPORTED;
    if (number(h+8,8)!=bytes || !zero(h+116,4) || !zero(h+136,24)) return HISTORY_PROFILE_CODEC_INVALID;
    unsigned kind=(unsigned)number(h+32,4),saved=(unsigned)number(h+44,4);
    if (kind>HISTORY_PROFILE_RETAINED || number(h+36,4)!=16 || number(h+40,4)!=15) return HISTORY_PROFILE_CODEC_UNSUPPORTED;
    if (saved>1 || !(number(h+16,8)|number(h+24,8))) return HISTORY_PROFILE_CODEC_INVALID;
    size_t roots=(size_t)number(h+96,4),tags=(size_t)number(h+100,4),anchors=(size_t)number(h+104,4),bindings=(size_t)number(h+108,4),records=(size_t)number(h+112,4);
    if (roots>DX_VERSION_ROOTS || tags>DX_VERSION_TAGS || anchors>DX_HISTORY_PUBLIC_ANCHORS || bindings>DX_VERSION_TAGS || records>DX_HISTORY_PROFILE_RECORDS) return HISTORY_PROFILE_CODEC_LIMIT;
    if (kind==HISTORY_PROFILE_CHECKPOINT_ONLY && (anchors || bindings || records || number(h+120,8) || number(h+128,8))) return HISTORY_PROFILE_CODEC_INVALID;
    Reader r={.data=data,.bytes=bytes,.at=DX_HISTORY_PROFILE_HEADER_BYTES,.arena=arena}; Parsed p={0};
    p.index=(WorkbookVersionIndex){.document_high=number(h+16,8),.document_low=number(h+24,8),.generation=number(h+48,8),.saved_generation=number(h+56,8),.saved={saved!=0,{{0}}},.root_count=roots,.tag_count=tags};
    memcpy(p.index.saved.key.bytes,h+64,32);
    p.anchors=(WorkbookHistoryAnchors){p.index.document_high,p.index.document_low,number(h+120,8),number(h+128,8),NULL,anchors,NULL,bindings};
    p.view=(WorkbookHistoryProfileView){.document={p.index.document_high,p.index.document_low},.container_version=16,.core_version=15,.kind=(WorkbookHistoryProfileKind)kind,.count=records};
    WorkbookVersionRoot *root=array(&r,roots,sizeof(*root)); WorkbookVersionTag *tag=array(&r,tags,sizeof(*tag));
    WorkbookHistoryAnchor *anchor=array(&r,anchors,sizeof(*anchor)); WorkbookHistoryTagAnchor *label=array(&r,bindings,sizeof(*label));
    p.index.roots=root; p.index.tags=tag; p.anchors.items=anchor; p.anchors.tags=label;
    const unsigned char *previous=NULL;
    for (size_t i=0;i<roots;i++) {
        const unsigned char *s=take(&r,ROOT_BYTES); if (!s) break;
        if (!zero(s+44,4) || (previous && memcmp(previous,s,32)>=0) || !number(s+32,8)) r.invalid=true;
        if (number(s+40,4)!=15) return HISTORY_PROFILE_CODEC_UNSUPPORTED;
        if (root) { memcpy(root[i].key.bytes,s,32); root[i].bytes=number(s+32,8); root[i].encoding=15; } previous=s;
    }
    uint64_t previous_id=0;
    for (size_t i=0;i<tags && !r.invalid;i++) {
        const unsigned char *s=take(&r,TAG_BYTES); if (!s) break; size_t n=(size_t)number(s+40,4); uint64_t created=number(s,8);
        if (!created || created<=previous_id || !zero(s+44,4) || !n || n>DX_VERSION_LABEL_BYTES) { r.invalid=true; break; }
        const unsigned char *text=take(&r,n); if (!text) break;
        if (tag) { tag[i].created=created; memcpy(tag[i].root.bytes,s+8,32); tag[i].label=(WorkbookVersionText){(const char *)text,n}; } previous_id=created;
    }
    previous_id=0;
    for (size_t i=0;i<anchors && !r.invalid;i++) {
        const unsigned char *s=take(&r,ANCHOR_BYTES); if (!s) break; uint64_t id=number(s,8);
        if ((i && id<=previous_id) || !zero(s+56,8)) r.invalid=true;
        if (anchor) { anchor[i].boundary.anchor=id; anchor[i].boundary.position=number(s+8,8); memcpy(anchor[i].boundary.root.bytes,s+16,32); anchor[i].lineage=number(s+48,8); } previous_id=id;
    }
    previous_id=0;
    for (size_t i=0;i<bindings && !r.invalid;i++) {
        const unsigned char *s=take(&r,16); if (!s) break; uint64_t id=number(s,8);
        if (!id || id<=previous_id) r.invalid=true;
        if (label) label[i]=(WorkbookHistoryTagAnchor){id,number(s+8,8)}; previous_id=id;
    }
    for (unsigned side=0;side<2 && !r.invalid;side++) {
        const unsigned char *s=take(&r,PATH_BYTES); if (!s) break;
        size_t present=(size_t)number(s,4),segments=(size_t)number(s+4,4),events=(size_t)number(s+8,4);
        if (present>1 || !zero(s+12,4) || (!present && !zero(s,PATH_BYTES)) || (kind==HISTORY_PROFILE_CHECKPOINT_ONLY && present)) { r.invalid=true; break; }
        if (segments>DX_HISTORY_PROFILE_RECORDS+1u || events>DX_HISTORY_PROFILE_RECORDS) return HISTORY_PROFILE_CODEC_LIMIT;
        if (!present) continue;
        WorkbookHistorySegment *segment=array(&r,segments,sizeof(*segment)); WorkbookHistoryEventPosition *event=array(&r,events,sizeof(*event)); size_t *map=array(&r,events,sizeof(*map));
        p.paths[side]=(WorkbookHistoryPath){p.index.document_high,p.index.document_low,segment,segments,event,events,number(s+16,8),number(s+24,8),number(s+32,8),number(s+40,8),number(s+48,8),number(s+56,8)};
        for (size_t i=0;i<segments;i++) { const unsigned char *v=take(&r,24); if (!v) break; if (segment) segment[i]=(WorkbookHistorySegment){number(v,8),number(v+8,8),number(v+16,8)}; }
        for (size_t i=0;i<events && !r.invalid;i++) {
            const unsigned char *v=take(&r,32); if (!v) break;
            size_t at=(size_t)number(v+24,4); if (!zero(v+28,4) || at>=records) r.invalid=true;
            if (event) event[i]=(WorkbookHistoryEventPosition){number(v,8),number(v+8,8),number(v+16,8)}; if (map) map[i]=at;
        }
        if (arena) { WorkbookHistoryProfilePath path={p.paths+side,map,events}; if (!side) p.view.current=path; else p.view.saved=path; }
    }
    WorkbookHistoryProfileRecord *record=array(&r,records,sizeof(*record)); WorkbookHistoryEventInfo *frame=array(&r,records,sizeof(*frame)); previous_id=0;
    for (size_t i=0;i<records && !r.invalid;i++) {
        const unsigned char *s=take(&r,RECORD_BYTES); if (!s) break;
        uint64_t transition=number(s+40,8),payload=number(s+96,8),dependencies=number(s+104,8),inactive=number(s+88,8);
        if (!zero(s+20,4) || number(s+80,4)>1 || !transition || transition<=previous_id) { r.invalid=true; break; }
        if (payload>SIZE_MAX || dependencies>SIZE_MAX || inactive>SIZE_MAX) return HISTORY_PROFILE_CODEC_LIMIT;
        const unsigned char *body=take(&r,(size_t)payload),*deps=take(&r,(size_t)dependencies); if (r.invalid) break;
        if (record) {
            frame[i]=(WorkbookHistoryEventInfo){.owner=binding.owner,.edit_session=binding.edit_session,.family=(unsigned)number(s,4),.encoding=(unsigned)number(s+4,4),.revision=number(s+24,8),.action=number(s+32,8),.transition=transition,.lineage=number(s+48,8),.previous_position=number(s+56,8),.position=number(s+64,8),.parent_lineage=number(s+72,8),.forked=number(s+80,4)!=0,.dependency_encoding=(unsigned)number(s+84,4),.inactive_scopes=(size_t)inactive,.bytes=(size_t)payload,.dependency_bytes=(size_t)dependencies};
            record[i]=(WorkbookHistoryProfileRecord){.owner=binding.owner,.edit_session=binding.edit_session,.event=frame+i,.data=body,.dependencies=dependencies ? deps : NULL,.bytes=(size_t)payload,.dependency_bytes=(size_t)dependencies,.sheet=number(s+8,8)};
            if (frame[i].family==DX_HISTORY_SHEET_LIFECYCLE_FAMILY) record[i].intent.operation=(WorkbookSheetOperation)number(s+16,4); else record[i].intent.effects=(unsigned)number(s+16,4);
        }
        previous_id=transition;
    }
    if (r.limit) return HISTORY_PROFILE_CODEC_LIMIT;
    if (r.invalid || r.at!=bytes) return HISTORY_PROFILE_CODEC_INVALID;
    *info=(WorkbookHistoryProfileWireInfo){bytes,r.parse,ALIGNMENT,records};
    if (out) { *out=p; out->view.versions=&out->index; if (kind==HISTORY_PROFILE_RETAINED) out->view.anchors=&out->anchors; out->view.records=record;
        if (out->view.current.path) out->view.current.path=out->paths; if (out->view.saved.path) out->view.saved.path=out->paths+1; }
    return HISTORY_PROFILE_CODEC_OK;
}
WorkbookHistoryProfileCodecStatus workbook_history_profile_preflight(const void *data,size_t bytes,WorkbookHistoryProfileWireInfo *out) {
    if (!out) return HISTORY_PROFILE_CODEC_INVALID; return parse(data,bytes,NULL,(WorkbookHistoryProfileBinding){0},NULL,out);
}
WorkbookHistoryProfileCodecStatus workbook_history_profile_encoded_size(const WorkbookHistoryProfile *profile,size_t *out) {
    const WorkbookHistoryProfileView *v=workbook_history_profile_view(profile); if (!v || !out) return HISTORY_PROFILE_CODEC_INVALID;
    size_t bytes=DX_HISTORY_PROFILE_HEADER_BYTES+2u*PATH_BYTES;
    const WorkbookVersionIndex *index=v->versions;
    if (!extent(&bytes,index->root_count,ROOT_BYTES)) return HISTORY_PROFILE_CODEC_LIMIT;
    for (size_t i=0;i<index->tag_count;i++) if (!add(&bytes,TAG_BYTES) || !add(&bytes,index->tags[i].label.bytes)) return HISTORY_PROFILE_CODEC_LIMIT;
    if (v->anchors && (!extent(&bytes,v->anchors->count,ANCHOR_BYTES) || !extent(&bytes,v->anchors->tag_count,16))) return HISTORY_PROFILE_CODEC_LIMIT;
    WorkbookHistoryProfilePath paths[]={v->current,v->saved};
    for (unsigned i=0;i<2;i++) if (paths[i].path && (!extent(&bytes,paths[i].path->segment_count,24) || !extent(&bytes,paths[i].count,32))) return HISTORY_PROFILE_CODEC_LIMIT;
    for (size_t i=0;i<v->count;i++) if (!add(&bytes,RECORD_BYTES) || !add(&bytes,v->records[i].bytes) || !add(&bytes,v->records[i].dependency_bytes)) return HISTORY_PROFILE_CODEC_LIMIT;
    if (bytes>DX_HISTORY_PROFILE_WIRE_BYTES) return HISTORY_PROFILE_CODEC_LIMIT; *out=bytes; return HISTORY_PROFILE_CODEC_OK;
}
static const WorkbookVersionRoot *next_root(const WorkbookVersionIndex *index,const WorkbookVersionRoot *previous) {
    const WorkbookVersionRoot *result=NULL;
    for (size_t i=0;i<index->root_count;i++) if ((!previous || key_less(previous->key,index->roots[i].key)) && (!result || key_less(index->roots[i].key,result->key))) result=index->roots+i;
    return result;
}
WorkbookHistoryProfileCodecStatus workbook_history_profile_encode(const WorkbookHistoryProfile *profile,void *arena,size_t capacity,size_t *written) {
    if (!arena || !written) return HISTORY_PROFILE_CODEC_INVALID; size_t bytes=0;
    WorkbookHistoryProfileCodecStatus status=workbook_history_profile_encoded_size(profile,&bytes); if (status) return status; if (capacity<bytes) return HISTORY_PROFILE_CODEC_LIMIT;
    const WorkbookHistoryProfileView *v=workbook_history_profile_view(profile); const WorkbookVersionIndex *index=v->versions;
    unsigned char *h=arena,*p=h+DX_HISTORY_PROFILE_HEADER_BYTES; memset(h,0,bytes); memcpy(h,"DXHP",4); put(h+4,DX_HISTORY_PROFILE_ENCODING,4); put(h+8,bytes,8);
    put(h+16,v->document.high,8); put(h+24,v->document.low,8); put(h+32,v->kind,4); put(h+36,16,4); put(h+40,15,4); put(h+44,index->saved.present,4);
    put(h+48,index->generation,8); put(h+56,index->saved_generation,8); memcpy(h+64,index->saved.key.bytes,32); put(h+96,index->root_count,4); put(h+100,index->tag_count,4); put(h+112,v->count,4);
    if (v->anchors) { put(h+104,v->anchors->count,4); put(h+108,v->anchors->tag_count,4); put(h+120,v->anchors->high_water,8); put(h+128,v->anchors->saved_anchor,8); }
    const WorkbookVersionRoot *root=NULL;
    for (size_t i=0;i<index->root_count;i++) { root=next_root(index,root); memcpy(p,root->key.bytes,32); put(p+32,root->bytes,8); put(p+40,root->encoding,4); p+=ROOT_BYTES; }
    uint64_t previous=0;
    for (size_t i=0;i<index->tag_count;i++) {
        const WorkbookVersionTag *tag=NULL; for (size_t j=0;j<index->tag_count;j++) if (index->tags[j].created>previous && (!tag || index->tags[j].created<tag->created)) tag=index->tags+j;
        put(p,tag->created,8); memcpy(p+8,tag->root.bytes,32); put(p+40,tag->label.bytes,4); memcpy(p+TAG_BYTES,tag->label.data,tag->label.bytes); p+=TAG_BYTES+tag->label.bytes; previous=tag->created;
    }
    previous=0;
    for (size_t i=0;v->anchors && i<v->anchors->count;i++) {
        const WorkbookHistoryAnchor *a=NULL; for (size_t j=0;j<v->anchors->count;j++) if ((!i || v->anchors->items[j].boundary.anchor>previous) && (!a || v->anchors->items[j].boundary.anchor<a->boundary.anchor)) a=v->anchors->items+j;
        put(p,a->boundary.anchor,8); put(p+8,a->boundary.position,8); memcpy(p+16,a->boundary.root.bytes,32); put(p+48,a->lineage,8); p+=ANCHOR_BYTES; previous=a->boundary.anchor;
    }
    previous=0;
    for (size_t i=0;v->anchors && i<v->anchors->tag_count;i++) {
        const WorkbookHistoryTagAnchor *a=NULL; for (size_t j=0;j<v->anchors->tag_count;j++) if (v->anchors->tags[j].tag_created>previous && (!a || v->anchors->tags[j].tag_created<a->tag_created)) a=v->anchors->tags+j;
        put(p,a->tag_created,8); put(p+8,a->anchor,8); p+=16; previous=a->tag_created;
    }
    WorkbookHistoryProfilePath paths[]={v->current,v->saved};
    for (unsigned side=0;side<2;side++) {
        const WorkbookHistoryPath *path=paths[side].path; if (!path) { p+=PATH_BYTES; continue; }
        put(p,1,4); put(p+4,path->segment_count,4); put(p+8,path->event_count,4); put(p+16,path->lineage_high_water,8); put(p+24,path->operation_high_water,8); put(p+32,path->origin,8); put(p+40,path->available,8); put(p+48,path->cursor,8); put(p+56,path->tail,8); p+=PATH_BYTES;
        for (size_t i=0;i<path->segment_count;i++) { put(p,path->segments[i].lineage,8); put(p+8,path->segments[i].from,8); put(p+16,path->segments[i].through,8); p+=24; }
        for (size_t i=0;i<path->event_count;i++) { put(p,path->events[i].operation,8); put(p+8,path->events[i].lineage,8); put(p+16,path->events[i].position,8); put(p+24,paths[side].records[i],4); p+=32; }
    }
    for (size_t i=0;i<v->count;i++) {
        const WorkbookHistoryProfileRecord *r=v->records+i; const WorkbookHistoryEventInfo *e=r->event;
        put(p,e->family,4); put(p+4,e->encoding,4); put(p+8,r->sheet,8); put(p+16,e->family==DX_HISTORY_SHEET_LIFECYCLE_FAMILY ? (unsigned)r->intent.operation : r->intent.effects,4);
        put(p+24,e->revision,8); put(p+32,e->action,8); put(p+40,e->transition,8); put(p+48,e->lineage,8); put(p+56,e->previous_position,8); put(p+64,e->position,8); put(p+72,e->parent_lineage,8); put(p+80,e->forked,4); put(p+84,e->dependency_encoding,4); put(p+88,e->inactive_scopes,8); put(p+96,r->bytes,8); put(p+104,r->dependency_bytes,8); p+=RECORD_BYTES;
        memcpy(p,r->data,r->bytes); p+=r->bytes; if (r->dependency_bytes) memcpy(p,r->dependencies,r->dependency_bytes); p+=r->dependency_bytes;
    }
    *written=bytes; return HISTORY_PROFILE_CODEC_OK;
}
WorkbookHistoryProfileCodecStatus workbook_history_profile_decode(const void *data,size_t bytes,WorkbookHistoryProfileBinding binding,
    WorkbookHistoryProfileCaps caps,void *arena,size_t arena_bytes,WorkbookHistoryProfile **out) {
    if (!out || !binding.owner.session || binding.owner.mode!=VERSION_WORKING_HEAD || !binding.edit_session) return HISTORY_PROFILE_CODEC_INVALID;
    WorkbookHistoryProfileWireInfo info; WorkbookHistoryProfileCodecStatus status=workbook_history_profile_preflight(data,bytes,&info); if (status) return status;
    const unsigned char *h=data;
    if (binding.owner.document_high!=number(h+16,8) || binding.owner.document_low!=number(h+24,8)) return HISTORY_PROFILE_CODEC_ADMISSION;
    if (arena_bytes>DX_HISTORY_PROFILE_PARSE_BYTES || arena_bytes>caps.staging_bytes || arena_bytes<info.parse_bytes) return HISTORY_PROFILE_CODEC_LIMIT;
    if ((info.parse_bytes && !arena) || (arena && (uintptr_t)arena%ALIGNMENT)) return HISTORY_PROFILE_CODEC_INVALID;
    uintptr_t a=(uintptr_t)arena,d=(uintptr_t)data;
    if (arena_bytes && (a>UINTPTR_MAX-arena_bytes || d>UINTPTR_MAX-bytes || (a<d+bytes && d<a+arena_bytes))) return HISTORY_PROFILE_CODEC_INVALID;
    Parsed parsed; status=parse(data,bytes,arena,binding,&parsed,&info); if (status) return status;
    caps.staging_bytes-=arena_bytes;
    /* prepare performs the complete candidate/prior/serial-peak measure before
       allocating, so no separate semantic pass or private layout guess is needed. */
    WorkbookHistoryProfileStatus admitted=workbook_history_profile_prepare(parsed.view,caps,out);
    return admitted==HISTORY_PROFILE_OK ? HISTORY_PROFILE_CODEC_OK : admitted==HISTORY_PROFILE_LIMIT ? HISTORY_PROFILE_CODEC_LIMIT : HISTORY_PROFILE_CODEC_ADMISSION;
}
