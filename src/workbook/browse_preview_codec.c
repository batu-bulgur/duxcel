#include "browse_preview_codec.h"
#include "browse_preview_internal.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
static bool supported(void) { return sizeof(double)==8 && FLT_RADIX==2 && DBL_MANT_DIG==53 && DBL_MAX_EXP==1024; }
static bool plus(size_t *n,size_t extra) { if (*n>SIZE_MAX-extra) return false; *n+=extra; return true; }
static bool text_size(const char *text,size_t *bytes) { return plus(bytes,4) && (!text || plus(bytes,strlen(text))); }
static const unsigned char *take(DefinitionWireReader *r,size_t bytes) {
    if (!r->ok || r->at>r->bytes || bytes>r->bytes-r->at) { r->ok=false; return NULL; }
    const unsigned char *result=r->data+r->at; r->at+=bytes; return result;
}
static int64_t signed64(uint64_t value) { return value<=INT64_MAX ? (int64_t)value : -(int64_t)(UINT64_MAX-value)-1; }
static void identity_write(DefinitionWireWriter *w,const SourceFileIdentity *id) {
    definition_wire_put(w,id->device,8); definition_wire_put(w,id->inode,8); definition_wire_put(w,id->size,8); definition_wire_put(w,id->links,8);
    definition_wire_put(w,id->mode,4); definition_wire_put(w,id->uid,4); definition_wire_put(w,id->gid,4); definition_wire_put(w,id->flags,4); definition_wire_put(w,id->generation,4); definition_wire_put(w,0,4);
    definition_wire_put(w,(uint64_t)id->modified_seconds,8); definition_wire_put(w,(uint64_t)id->changed_seconds,8); definition_wire_put(w,(uint64_t)id->created_seconds,8);
    definition_wire_put(w,id->modified_nanoseconds,4); definition_wire_put(w,id->changed_nanoseconds,4); definition_wire_put(w,id->created_nanoseconds,4); definition_wire_put(w,0,4);
}
static SourceFileIdentity identity_read(DefinitionWireReader *r) {
    SourceFileIdentity id={0}; id.device=definition_wire_get(r,8); id.inode=definition_wire_get(r,8); id.size=definition_wire_get(r,8); id.links=definition_wire_get(r,8);
    id.mode=(uint32_t)definition_wire_get(r,4); id.uid=(uint32_t)definition_wire_get(r,4); id.gid=(uint32_t)definition_wire_get(r,4); id.flags=(uint32_t)definition_wire_get(r,4); id.generation=(uint32_t)definition_wire_get(r,4);
    if (definition_wire_get(r,4)) r->ok=false;
    id.modified_seconds=signed64(definition_wire_get(r,8)); id.changed_seconds=signed64(definition_wire_get(r,8)); id.created_seconds=signed64(definition_wire_get(r,8));
    id.modified_nanoseconds=(uint32_t)definition_wire_get(r,4); id.changed_nanoseconds=(uint32_t)definition_wire_get(r,4); id.created_nanoseconds=(uint32_t)definition_wire_get(r,4);
    if (definition_wire_get(r,4)) r->ok=false;
    return id;
}
static void header_write(DefinitionWireWriter *w,const BrowsePreview *p,size_t schema,size_t cells,size_t key) {
    const BrowsePreviewInfo *i=&p->info; const Page *page=&p->page;
    memcpy(w->p,"DXBP",4); w->p+=4; definition_wire_put(w,DX_BROWSE_PREVIEW_ENCODING,2); definition_wire_put(w,0,2);
    const uint64_t values[]={i->document.high,i->document.low,i->sheet,i->owner,i->object_high_water,i->source_high_water,i->source_version,
        i->offset,i->total,i->metadata_clock,i->request,i->query_epoch,i->source_epoch};
    for (size_t n=0;n<sizeof(values)/sizeof(*values);n++) definition_wire_put(w,values[n],8);
    definition_wire_put(w,i->rows,4); definition_wire_put(w,i->columns,4); definition_wire_put(w,i->records,4);
    definition_wire_put(w,(i->count_pending ? 1u : 0u)|(i->binding_pending ? 2u : 0u)|(page->records.file_identity_valid ? 4u : 0u),4);
    definition_wire_put(w,i->definition_bytes,4); definition_wire_put(w,i->source_bytes,4); definition_wire_put(w,schema,4); definition_wire_put(w,cells,4); definition_wire_put(w,key,4); definition_wire_put(w,0,4);
    definition_wire_put(w,page->records.stamp,8); identity_write(w,&page->records.file_identity);
}
static bool sections(const BrowsePreview *preview,size_t *schema,size_t *cells,size_t *key) {
    const Page *page=&preview->page; *schema=0; *cells=0; *key=0;
    for (size_t i=0;i<page->column_count;i++) {
        const Column *column=page->columns+i;
        if (browse_preview_column_status(column)!=BROWSE_PREVIEW_OK || !plus(schema,12+DX_PIVOT_FIELDS)) return false;
        const char *texts[]={column->name,column->type_name,column->caption,column->detail};
        for (size_t n=0;n<4;n++) if (!text_size(texts[n],schema)) return false;
    }
    for (size_t i=0;i<page->row_count*page->column_count;i++) if (!plus(cells,9) || !text_size(page->cells[i].text,cells)) return false;
    return text_size(page->records.source_key,key);
}
BrowsePreviewStatus browse_preview_encode_measure(const BrowsePreview *preview,const BrowsePreviewPacket *prior,BrowsePreviewWireInfo *out) {
    if (!preview || !out || (prior && ((prior->data!=NULL)!=(prior->bytes!=0)))) return BROWSE_PREVIEW_INVALID;
    if (!supported()) return BROWSE_PREVIEW_UNSUPPORTED;
    size_t schema,cells,key; if (!sections(preview,&schema,&cells,&key)) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewMetadataInfo metadata={0}; BrowsePreviewStatus status=browse_preview_metadata_preflight(preview,&metadata); if (status!=BROWSE_PREVIEW_OK) return status;
    size_t bytes=DX_BROWSE_PREVIEW_HEADER_BYTES;
    const size_t parts[]={preview->info.definition_bytes,preview->info.source_bytes,schema,cells,preview->info.records*8,key};
    for (size_t i=0;i<sizeof(parts)/sizeof(*parts);i++) if (!plus(&bytes,parts[i])) return BROWSE_PREVIEW_LIMIT;
    if (bytes>DX_BROWSE_PREVIEW_WIRE_BYTES || schema>UINT32_MAX || cells>UINT32_MAX || key>UINT32_MAX) return BROWSE_PREVIEW_LIMIT;
    size_t coexistence=bytes; if (!plus(&coexistence,preview->info.owned) || !plus(&coexistence,prior ? prior->bytes : 0)) return BROWSE_PREVIEW_LIMIT;
    *out=(BrowsePreviewWireInfo){.preview=preview->info,.bytes=bytes,.owned=bytes,.coexistence=coexistence}; return BROWSE_PREVIEW_OK;
}
BrowsePreviewStatus browse_preview_encode(const BrowsePreview *preview,size_t reserved,BrowsePreviewPacket *out) {
    if (!out) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewWireInfo info={0}; BrowsePreviewStatus status=browse_preview_encode_measure(preview,out,&info); if (status!=BROWSE_PREVIEW_OK) return status;
    if (reserved<info.coexistence) return BROWSE_PREVIEW_LIMIT;
    size_t schema,cells,key; if (!sections(preview,&schema,&cells,&key)) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewPacket next={dx_alloc(info.bytes),info.bytes}; DefinitionWireWriter w={next.data}; header_write(&w,preview,schema,cells,key);
    memcpy(w.p,preview->definition,preview->info.definition_bytes); w.p+=preview->info.definition_bytes;
    memcpy(w.p,preview->sources,preview->info.source_bytes); w.p+=preview->info.source_bytes;
    const Page *page=&preview->page;
    for (size_t i=0;i<page->column_count;i++) {
        const Column *column=page->columns+i;
        definition_wire_put(&w,column->type,4); definition_wire_put(&w,column->number_type.kind,1); definition_wire_put(&w,column->number_type.precision,1); definition_wire_put(&w,column->number_type.scale,1);
        definition_wire_put(&w,(column->numeric ? 1u : 0u)|(column->filtered ? 2u : 0u)|(column->sort_descending ? 4u : 0u),1);
        definition_wire_put(&w,column->sort_priority,1); definition_wire_put(&w,column->date_grain,1); definition_wire_put(&w,0,2);
        for (size_t n=0;n<DX_PIVOT_FIELDS;n++) definition_wire_put(&w,column->category_grains[n],1);
        definition_wire_text_write(&w,column->name); definition_wire_text_write(&w,column->type_name); definition_wire_text_write(&w,column->caption); definition_wire_text_write(&w,column->detail);
    }
    for (size_t i=0;i<page->row_count*page->column_count;i++) {
        uint64_t number=0; memcpy(&number,&page->cells[i].number,8); definition_wire_put(&w,page->cells[i].is_null,1); definition_wire_put(&w,number,8); definition_wire_text_write(&w,page->cells[i].text);
    }
    for (size_t i=0;i<page->records.count;i++) definition_wire_put(&w,page->records.ids[i],8);
    definition_wire_text_write(&w,page->records.source_key);
    if ((size_t)(w.p-(unsigned char *)next.data)!=next.bytes) { browse_preview_packet_free(&next); return BROWSE_PREVIEW_INVALID; }
    browse_preview_packet_free(out); *out=next; return BROWSE_PREVIEW_OK;
}
void browse_preview_packet_free(BrowsePreviewPacket *packet) { if (packet) { free(packet->data); *packet=(BrowsePreviewPacket){0}; } }
static bool field_text(DefinitionWireReader *r,char **out,bool required,size_t *length) {
    /* Keep a real owned-limit refusal distinct from malformed framing/UTF-8. */
    if (r->ok && r->at<=r->bytes && r->bytes-r->at>=4) {
        DefinitionWireReader probe=*r; uint64_t n=definition_wire_get(&probe,4);
        if (n!=UINT32_MAX && n<=probe.bytes-probe.at && (r->owned>r->owned_limit || n+1>r->owned_limit-r->owned)) r->scratch=1;
    }
    bool present=definition_wire_text_read(r,out,length); if (!present && required) r->ok=false; return present;
}
static BrowsePreviewStatus parse(const void *data,size_t bytes,BrowsePreview *out,BrowsePreviewWireInfo *wire) {
    if (!data || bytes<DX_BROWSE_PREVIEW_HEADER_BYTES || !wire) return BROWSE_PREVIEW_INVALID;
    if (!supported()) return BROWSE_PREVIEW_UNSUPPORTED;
    if (bytes>DX_BROWSE_PREVIEW_WIRE_BYTES) return BROWSE_PREVIEW_LIMIT;
    DefinitionWireReader r={.data=data,.bytes=bytes,.ok=true,.owned_limit=DX_BROWSE_PREVIEW_OWNED_BYTES};
    const unsigned char *magic=take(&r,4); if (!magic || memcmp(magic,"DXBP",4)) return BROWSE_PREVIEW_INVALID;
    if (definition_wire_get(&r,2)!=DX_BROWSE_PREVIEW_ENCODING) return BROWSE_PREVIEW_UNSUPPORTED;
    if (definition_wire_get(&r,2)) return BROWSE_PREVIEW_INVALID;
    BrowsePreview candidate={0}; BrowsePreviewInfo *info=&candidate.info;
    info->document.high=definition_wire_get(&r,8); info->document.low=definition_wire_get(&r,8); info->sheet=definition_wire_get(&r,8); info->owner=definition_wire_get(&r,8); info->object_high_water=definition_wire_get(&r,8); info->source_high_water=definition_wire_get(&r,8); info->source_version=definition_wire_get(&r,8);
    info->offset=definition_wire_get(&r,8); info->total=definition_wire_get(&r,8); info->metadata_clock=definition_wire_get(&r,8); info->request=definition_wire_get(&r,8); info->query_epoch=definition_wire_get(&r,8); info->source_epoch=definition_wire_get(&r,8);
    info->rows=(size_t)definition_wire_get(&r,4); info->columns=(size_t)definition_wire_get(&r,4); info->records=(size_t)definition_wire_get(&r,4);
    unsigned flags=(unsigned)definition_wire_get(&r,4); info->count_pending=(flags&1)!=0; info->binding_pending=(flags&2)!=0;
    info->definition_bytes=(size_t)definition_wire_get(&r,4); info->source_bytes=(size_t)definition_wire_get(&r,4);
    size_t schema_bytes=(size_t)definition_wire_get(&r,4),cells_bytes=(size_t)definition_wire_get(&r,4),key_bytes=(size_t)definition_wire_get(&r,4);
    if (definition_wire_get(&r,4)) r.ok=false;
    candidate.page.records.stamp=definition_wire_get(&r,8); candidate.page.records.file_identity=identity_read(&r); candidate.page.records.file_identity_valid=(flags&4)!=0;
    if (!r.ok || flags&~7u || (!info->document.high && !info->document.low) || !info->sheet || info->owner>info->object_high_water ||
        info->source_version>info->source_high_water || (!info->source_version && info->binding_pending) || !info->columns ||
        info->records>info->rows || (info->rows && (info->offset>UINT64_MAX-info->rows || info->offset+info->rows>info->total))) return BROWSE_PREVIEW_INVALID;
    if (info->rows>DX_BROWSE_PREVIEW_ROWS || info->columns>DX_BROWSE_PREVIEW_COLUMNS) return BROWSE_PREVIEW_LIMIT;
    candidate.definition=(void *)take(&r,info->definition_bytes); candidate.sources=(void *)take(&r,info->source_bytes); if (!r.ok) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewMetadataInfo metadata={0}; BrowsePreviewStatus status=browse_preview_metadata_preflight(&candidate,&metadata); if (status!=BROWSE_PREVIEW_OK) return status;
    size_t cell_count=info->rows*info->columns;
    r.owned=sizeof(BrowsePreview)+info->columns*sizeof(Column)+cell_count*sizeof(Cell)+info->records*sizeof(uint64_t)+info->definition_bytes+info->source_bytes;
    if (r.owned>r.owned_limit) return BROWSE_PREVIEW_LIMIT;
    Page *page=out ? &out->page : NULL;
    if (out) {
        out->info=*info; page->offset=info->offset; page->total=info->total; page->count_pending=info->count_pending; page->column_count=info->columns; page->row_count=info->rows;
        page->columns=dx_alloc(info->columns*sizeof(Column)); page->cells=cell_count ? dx_alloc(cell_count*sizeof(Cell)) : NULL;
        page->records=candidate.page.records; page->records.count=info->records; page->records.ids=info->records ? dx_alloc(info->records*sizeof(uint64_t)) : NULL;
    }
    size_t start=r.at;
    for (size_t i=0;i<info->columns;i++) {
        Column local={0}; Column *column=page ? page->columns+i : &local;
        column->type=(duckdb_type)definition_wire_get(&r,4); column->number_type.kind=(NumberKind)definition_wire_get(&r,1); column->number_type.precision=(unsigned char)definition_wire_get(&r,1); column->number_type.scale=(unsigned char)definition_wire_get(&r,1);
        unsigned column_flags=(unsigned)definition_wire_get(&r,1); column->numeric=(column_flags&1)!=0; column->filtered=(column_flags&2)!=0; column->sort_descending=(column_flags&4)!=0;
        column->sort_priority=(unsigned char)definition_wire_get(&r,1); column->date_grain=(PivotDateGrain)definition_wire_get(&r,1);
        if (definition_wire_get(&r,2) || column_flags&~7u) r.ok=false;
        for (size_t n=0;n<DX_PIVOT_FIELDS;n++) column->category_grains[n]=(PivotDateGrain)definition_wire_get(&r,1);
        size_t length=0; bool present=field_text(&r,page ? &column->name : NULL,true,&length); if (!page && present) column->name="";
        present=field_text(&r,page ? &column->type_name : NULL,true,&length); if (!page && present) column->type_name="";
        field_text(&r,page ? &column->caption : NULL,false,&length); field_text(&r,page ? &column->detail : NULL,false,&length);
        if (!r.ok) return r.scratch ? BROWSE_PREVIEW_LIMIT : BROWSE_PREVIEW_INVALID;
        status=browse_preview_column_status(column); if (status!=BROWSE_PREVIEW_OK) return status;
    }
    if (r.at-start!=schema_bytes) return BROWSE_PREVIEW_INVALID;
    start=r.at; size_t value_bytes=0;
    for (size_t i=0;i<cell_count;i++) {
        unsigned null=(unsigned)definition_wire_get(&r,1); uint64_t number=definition_wire_get(&r,8); size_t length=0;
        bool present=field_text(&r,page ? &page->cells[i].text : NULL,false,&length);
        if (!r.ok) return r.scratch ? BROWSE_PREVIEW_LIMIT : BROWSE_PREVIEW_INVALID;
        if (null>1 || (!null && !present)) return BROWSE_PREVIEW_INVALID;
        if (length>DX_BROWSE_PREVIEW_TEXT_BYTES || value_bytes>DX_BROWSE_PREVIEW_TEXT_BYTES-length) return BROWSE_PREVIEW_LIMIT;
        value_bytes+=length;
        if (page) { page->cells[i].is_null=null!=0; memcpy(&page->cells[i].number,&number,8); }
    }
    if (r.at-start!=cells_bytes) return BROWSE_PREVIEW_INVALID;
    uint64_t ids[DX_BROWSE_PREVIEW_ROWS];
    for (size_t i=0;i<info->records;i++) {
        ids[i]=definition_wire_get(&r,8); if (!ids[i]) return BROWSE_PREVIEW_INVALID;
        for (size_t n=0;n<i;n++) if (ids[n]==ids[i]) return BROWSE_PREVIEW_INVALID;
        if (page) page->records.ids[i]=ids[i];
    }
    start=r.at; size_t length=0; field_text(&r,page ? &page->records.source_key : NULL,false,&length);
    if (!r.ok) return r.scratch ? BROWSE_PREVIEW_LIMIT : BROWSE_PREVIEW_INVALID;
    if (r.at-start!=key_bytes || r.at!=r.bytes) return BROWSE_PREVIEW_INVALID;
    info->owned=r.owned; info->validation_scratch=metadata.validation_scratch;
    size_t coexistence=bytes;
    if (!plus(&coexistence,r.owned) || !plus(&coexistence,metadata.owned) || !plus(&coexistence,metadata.validation_scratch)) return BROWSE_PREVIEW_LIMIT;
    info->coexistence=coexistence;
    if (out) { out->info=*info; out->page.bytes=info->owned-sizeof(*out)-info->definition_bytes-info->source_bytes; }
    *wire=(BrowsePreviewWireInfo){.preview=*info,.bytes=bytes,.owned=info->owned,.validation_scratch=metadata.owned+metadata.validation_scratch,.coexistence=coexistence};
    return BROWSE_PREVIEW_OK;
}
BrowsePreviewStatus browse_preview_preflight(const void *data,size_t bytes,BrowsePreviewWireInfo *out) {
    if (!out) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewWireInfo info={0}; BrowsePreviewStatus status=parse(data,bytes,NULL,&info); if (status==BROWSE_PREVIEW_OK) *out=info; return status;
}
BrowsePreviewStatus browse_preview_decode(const void *data,size_t bytes,size_t reserved,BrowsePreview **out) {
    if (!out) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewWireInfo info={0}; BrowsePreviewStatus status=browse_preview_preflight(data,bytes,&info); if (status!=BROWSE_PREVIEW_OK) return status;
    size_t coexistence=info.coexistence; if (!plus(&coexistence,*out ? (*out)->info.owned : 0) || reserved<coexistence) return BROWSE_PREVIEW_LIMIT;
    BrowsePreview borrowed={.info=info.preview}; borrowed.definition=(unsigned char *)data+DX_BROWSE_PREVIEW_HEADER_BYTES; borrowed.sources=(unsigned char *)borrowed.definition+info.preview.definition_bytes;
    BrowsePreviewMetadataInfo metadata={0}; status=browse_preview_metadata_preflight(&borrowed,&metadata); if (status!=BROWSE_PREVIEW_OK) return status;
    AnalysisAsset definition={0}; SourceVersionsAsset sources={0};
    status=browse_preview_metadata_decode(&borrowed,&metadata,metadata.owned+metadata.validation_scratch,&definition,&sources);
    if (status!=BROWSE_PREVIEW_OK) return status;
    analysis_asset_free(&definition); source_versions_asset_free(&sources);
    BrowsePreview *next=dx_alloc(sizeof(*next)); status=parse(data,bytes,next,&info);
    if (status!=BROWSE_PREVIEW_OK) { browse_preview_free(next); return status; }
    next->definition=dx_alloc(next->info.definition_bytes); memcpy(next->definition,borrowed.definition,next->info.definition_bytes);
    next->sources=dx_alloc(next->info.source_bytes); memcpy(next->sources,borrowed.sources,next->info.source_bytes);
    BrowsePreview *old=*out; *out=next; browse_preview_free(old); return BROWSE_PREVIEW_OK;
}
