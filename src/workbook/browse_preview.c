#include "browse_preview_internal.h"
#include "analysis_asset.h"
#include <stdlib.h>
#include <string.h>
static bool context_valid(const BrowsePreviewContext *c) {
    return c && c->document && c->document_session && c->sheet && c->sheet_session && c->object_high_water && c->metadata_clock &&
        c->request_epoch && c->query_epoch && c->source_epoch && c->definition && c->result_owner && c->sources && c->results && c->results->resolve &&
        (c->document->high || c->document->low) && *c->document_session && *c->sheet && *c->sheet_session && c->owner<=*c->object_high_water &&
        (!c->owner ? !c->table : c->table && c->table->kind==OBJECT_TABLE && c->table->id==c->owner &&
            c->definition==&c->table->source && c->query_epoch==&c->table->revision && c->source_epoch==&c->table->revision);
}
static BrowsePreviewStatus resolve(const BrowsePreviewContext *c,RangeResultProof *proof,const SourceVersion **binding) {
    if (!context_valid(c)) return BROWSE_PREVIEW_INVALID;
    if (!c->results->resolve(c->results->context,c->owner,proof) || !proof->ready || proof->failed) return BROWSE_PREVIEW_UNAVAILABLE;
    if (proof->object!=c->owner || proof->owner!=c->result_owner || proof->definition!=c->definition || !proof->page ||
        proof->request!=*c->request_epoch || proof->query_epoch!=*c->query_epoch || proof->source_epoch!=*c->source_epoch) return BROWSE_PREVIEW_STALE;
    if (c->definition->kind!=SOURCE_CSV && c->definition->kind!=SOURCE_PARQUET && c->definition->kind!=SOURCE_DATABASE) return BROWSE_PREVIEW_UNSUPPORTED;
    if (c->definition->kind==SOURCE_DATABASE && (!c->definition->table || !*c->definition->table)) return BROWSE_PREVIEW_UNSUPPORTED;
    if (!source_versions_valid(c->sources,*c->object_high_water,NULL)) return BROWSE_PREVIEW_INVALID;
    *binding=NULL;
    if (c->definition->source_version) {
        for (size_t i=0;i<c->sources->count;i++) if (c->sources->items[i].version==c->definition->source_version) {
            if (*binding) return BROWSE_PREVIEW_INVALID;
            *binding=c->sources->items+i;
        }
        if (!*binding || (*binding)->version>c->sources->high_water || (*binding)->owner!=c->owner || (*binding)->kind!=c->definition->kind ||
            !(*binding)->path || !c->definition->path || strcmp((*binding)->path,c->definition->path)) return BROWSE_PREVIEW_INVALID;
    }
    return BROWSE_PREVIEW_OK;
}
BrowsePreviewStatus browse_preview_prepare(const BrowsePreviewContext *c,BrowsePreviewTarget *out) {
    if (!out) return BROWSE_PREVIEW_INVALID;
    RangeResultProof proof={0}; const SourceVersion *binding=NULL; BrowsePreviewStatus status=resolve(c,&proof,&binding);
    if (status!=BROWSE_PREVIEW_OK) return status;
    BrowsePreviewTarget next={.context=*c,.document=*c->document,.document_session=*c->document_session,.sheet=*c->sheet,.sheet_session=*c->sheet_session,
        .object_high_water=*c->object_high_water,.metadata_clock=*c->metadata_clock,.request=*c->request_epoch,.query_epoch=*c->query_epoch,.source_epoch=*c->source_epoch,
        .resolver=*c->results,.result.proof=proof,.bound=binding!=NULL};
    memcpy(&next.result.definition,c->definition,sizeof(next.result.definition)); memcpy(&next.result.page,proof.page,sizeof(next.result.page));
    memcpy(&next.sources,c->sources,sizeof(next.sources));
    if (binding) memcpy(&next.binding,binding,sizeof(next.binding));
    memcpy(out,&next,sizeof(next)); return BROWSE_PREVIEW_OK;
}
static BrowsePreviewStatus check(const BrowsePreviewContext *c,const BrowsePreviewTarget *t,RangeResultProof *proof) {
    if (!context_valid(c) || !t) return BROWSE_PREVIEW_INVALID;
    const BrowsePreviewContext *old=&t->context;
    if (c->document!=old->document || c->document_session!=old->document_session || c->sheet!=old->sheet || c->sheet_session!=old->sheet_session ||
        c->object_high_water!=old->object_high_water || c->metadata_clock!=old->metadata_clock || c->request_epoch!=old->request_epoch ||
        c->query_epoch!=old->query_epoch || c->source_epoch!=old->source_epoch || c->owner!=old->owner || c->definition!=old->definition ||
        c->table!=old->table || c->result_owner!=old->result_owner || c->sources!=old->sources || c->results!=old->results ||
        c->results->resolve!=t->resolver.resolve || c->results->context!=t->resolver.context) return BROWSE_PREVIEW_STALE;
    if (c->document->high!=t->document.high || c->document->low!=t->document.low || *c->document_session!=t->document_session || *c->sheet!=t->sheet ||
        *c->sheet_session!=t->sheet_session || *c->object_high_water!=t->object_high_water || *c->metadata_clock!=t->metadata_clock ||
        *c->request_epoch!=t->request || *c->query_epoch!=t->query_epoch || *c->source_epoch!=t->source_epoch) return BROWSE_PREVIEW_STALE;
    /* Re-resolve CURRENT authority before touching any prior borrowed content.
       The snapshot structs below are pointer/scalar fences only. */
    const SourceVersion *binding=NULL; BrowsePreviewStatus status=resolve(c,proof,&binding);
    if (status!=BROWSE_PREVIEW_OK) return status;
    const RangeResultProof *prior=&t->result.proof;
    if (proof->owner!=prior->owner || proof->definition!=prior->definition || proof->page!=prior->page ||
        memcmp(c->definition,&t->result.definition,sizeof(t->result.definition)) || memcmp(proof->page,&t->result.page,sizeof(t->result.page)) ||
        memcmp(c->sources,&t->sources,sizeof(t->sources)) || (binding!=NULL)!=t->bound ||
        (binding && memcmp(binding,&t->binding,sizeof(t->binding)))) return BROWSE_PREVIEW_STALE;
    return BROWSE_PREVIEW_OK;
}
BrowsePreviewStatus browse_preview_column_status(const Column *column) {
    if (!column || !column->name || !column->type_name || !column->type) return BROWSE_PREVIEW_INVALID;
    /* Current shared DuckDB type IDs, retained exactly without text fallback. */
    if ((unsigned)column->type>41u) return BROWSE_PREVIEW_UNSUPPORTED;
    if (column->category_count || column->categories) return BROWSE_PREVIEW_UNSUPPORTED;
    NumberKind kind=NUM_NONE;
    switch (column->type) {
    case DUCKDB_TYPE_TINYINT: kind=NUM_I8; break; case DUCKDB_TYPE_SMALLINT: kind=NUM_I16; break;
    case DUCKDB_TYPE_INTEGER: kind=NUM_I32; break; case DUCKDB_TYPE_BIGINT: kind=NUM_I64; break;
    case DUCKDB_TYPE_HUGEINT: kind=NUM_I128; break; case DUCKDB_TYPE_UTINYINT: kind=NUM_U8; break;
    case DUCKDB_TYPE_USMALLINT: kind=NUM_U16; break; case DUCKDB_TYPE_UINTEGER: kind=NUM_U32; break;
    case DUCKDB_TYPE_UBIGINT: kind=NUM_U64; break; case DUCKDB_TYPE_UHUGEINT: kind=NUM_U128; break;
    case DUCKDB_TYPE_DECIMAL: kind=NUM_DECIMAL; break;
    case DUCKDB_TYPE_FLOAT: case DUCKDB_TYPE_DOUBLE: kind=NUM_FLOAT; break;
    default: break;
    }
    NumberType number=column->number_type;
    if (number.kind!=kind || column->numeric!=(kind!=NUM_NONE) ||
        (kind==NUM_DECIMAL ? !number.precision || number.precision>38 || number.scale>number.precision : number.precision || number.scale) ||
        (unsigned)column->date_grain>PIVOT_DATE_MONTH) return BROWSE_PREVIEW_INVALID;
    for (size_t i=0;i<DX_PIVOT_FIELDS;i++) if ((unsigned)column->category_grains[i]>PIVOT_DATE_MONTH) return BROWSE_PREVIEW_INVALID;
    return BROWSE_PREVIEW_OK;
}
static bool add(size_t *total,size_t bytes) { if (bytes>DX_BROWSE_PREVIEW_OWNED_BYTES || *total>DX_BROWSE_PREVIEW_OWNED_BYTES-bytes) return false; *total+=bytes; return true; }
static BrowsePreviewStatus text_owned(const char *text,size_t *owned,size_t *value_bytes) {
    if (!text) return BROWSE_PREVIEW_OK;
    size_t length=0; while (length<=DX_BROWSE_PREVIEW_OWNED_BYTES && text[length]) length++;
    if (length>DX_BROWSE_PREVIEW_OWNED_BYTES) return BROWSE_PREVIEW_LIMIT;
    size_t measured_length=0; DefinitionWireMeasure measure={0}; if (!definition_wire_text_measure(text,&measure,SIZE_MAX,&measured_length)) return BROWSE_PREVIEW_INVALID;
    if (value_bytes) { if (length>DX_BROWSE_PREVIEW_TEXT_BYTES || *value_bytes>DX_BROWSE_PREVIEW_TEXT_BYTES-length) return BROWSE_PREVIEW_LIMIT; *value_bytes+=length; }
    return add(owned,length+1) ? BROWSE_PREVIEW_OK : BROWSE_PREVIEW_LIMIT;
}
static BrowsePreviewStatus measure(const BrowsePreviewContext *c,const BrowsePreviewTarget *target,const BrowsePreview *prior,BrowsePreviewInfo *out) {
    RangeResultProof proof={0}; BrowsePreviewStatus status=check(c,target,&proof); if (status!=BROWSE_PREVIEW_OK) return status;
    const Page *page=proof.page;
    if (page->totals || page->other || page->other_status!=PAGE_OTHER_NONE) return BROWSE_PREVIEW_UNSUPPORTED;
    if (page->row_count>DX_BROWSE_PREVIEW_ROWS || page->column_count>DX_BROWSE_PREVIEW_COLUMNS) return BROWSE_PREVIEW_LIMIT;
    if (!page->column_count || !page->columns || (page->row_count && !page->cells) ||
        (page->row_count && (page->offset>UINT64_MAX-page->row_count || page->offset+page->row_count>page->total)) ||
        page->records.count>page->row_count || (page->records.count && !page->records.ids)) return BROWSE_PREVIEW_INVALID;
    size_t cells=page->row_count*page->column_count,owned=sizeof(BrowsePreview),value_bytes=0;
    if (!add(&owned,page->column_count*sizeof(Column)) || !add(&owned,cells*sizeof(Cell)) || !add(&owned,page->records.count*sizeof(uint64_t))) return BROWSE_PREVIEW_LIMIT;
    for (size_t col=0;col<page->column_count;col++) {
        const Column *column=page->columns+col;
        status=browse_preview_column_status(column); if (status!=BROWSE_PREVIEW_OK) return status;
        const char *texts[]={column->name,column->type_name,column->caption,column->detail};
        for (size_t i=0;i<4;i++) { status=text_owned(texts[i],&owned,NULL); if (status!=BROWSE_PREVIEW_OK) return status; }
    }
    for (size_t i=0;i<cells;i++) {
        const Cell *cell=page->cells+i; if (!cell->is_null && !cell->text) return BROWSE_PREVIEW_INVALID;
        status=text_owned(cell->text,&owned,&value_bytes); if (status!=BROWSE_PREVIEW_OK) return status;
    }
    for (size_t i=0;i<page->records.count;i++) {
        if (!page->records.ids[i]) return BROWSE_PREVIEW_INVALID;
        for (size_t j=0;j<i;j++) if (page->records.ids[i]==page->records.ids[j]) return BROWSE_PREVIEW_INVALID;
    }
    status=text_owned(page->records.source_key,&owned,NULL); if (status!=BROWSE_PREVIEW_OK) return status;
    AnalysisAssetMeasure definition={0}; AnalysisAssetStatus encoded=analysis_asset_measure(target->sheet,c->owner,c->definition,target->object_high_water,&definition);
    if (encoded!=ANALYSIS_ASSET_OK) return encoded==ANALYSIS_ASSET_LIMIT ? BROWSE_PREVIEW_LIMIT : encoded==ANALYSIS_ASSET_UNSUPPORTED ? BROWSE_PREVIEW_UNSUPPORTED : BROWSE_PREVIEW_INVALID;
    bool selected[DX_SOURCE_VERSIONS]={0};
    for (size_t i=0;i<c->sources->count;i++) selected[i]=c->definition->source_version && c->sources->items[i].version==c->definition->source_version;
    SourceVersionsSelectionMeasure sources={0};
    if (source_versions_selection_measure(target->sheet,c->sources,target->object_high_water,selected,c->sources->count,&sources)!=SOURCE_VERSIONS_CODEC_OK) return BROWSE_PREVIEW_INVALID;
    if (!add(&owned,definition.bytes) || !add(&owned,sources.bytes)) return BROWSE_PREVIEW_LIMIT;
    /* Source ledger validation has a fixed bounded field-name stack. Charge
       it conservatively alongside admitted definition grammar scratch, even
       though their peaks do not overlap. No source packet is cloned here. */
    size_t scratch=definition.validation_scratch+DX_SOURCE_SCOPE_FIELDS*sizeof(const char *)+DX_SOURCE_SCOPE_TEXT_BYTES;
    size_t coexistence=owned;
    const size_t parts[]={scratch,sizeof(*target),c->other_owned,prior ? prior->info.owned : 0};
    for (size_t i=0;i<4;i++) { if (coexistence>SIZE_MAX-parts[i]) return BROWSE_PREVIEW_LIMIT; coexistence+=parts[i]; }
    *out=(BrowsePreviewInfo){.document=target->document,.sheet=target->sheet,.owner=c->owner,.object_high_water=target->object_high_water,
        .source_version=c->definition->source_version,.source_high_water=c->sources->high_water,
        .metadata_clock=target->metadata_clock,.request=target->request,.query_epoch=target->query_epoch,.source_epoch=target->source_epoch,.offset=page->offset,.total=page->total,
        .rows=page->row_count,.columns=page->column_count,.records=page->records.count,.definition_bytes=definition.bytes,.source_bytes=sources.bytes,.owned=owned,
        .validation_scratch=scratch,.coexistence=coexistence,.count_pending=page->count_pending,
        .binding_pending=target->bound && !target->binding.raw.bytes};
    return BROWSE_PREVIEW_OK;
}
BrowsePreviewStatus browse_preview_measure(const BrowsePreviewContext *c,const BrowsePreviewTarget *target,const BrowsePreview *prior,BrowsePreviewInfo *out) {
    if (!out) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewInfo info={0}; BrowsePreviewStatus status=measure(c,target,prior,&info); if (status==BROWSE_PREVIEW_OK) *out=info; return status;
}
void browse_preview_free(BrowsePreview *preview) { if (preview) { page_free(&preview->page); free(preview->definition); free(preview->sources); free(preview); } }
BrowsePreviewInfo browse_preview_info(const BrowsePreview *preview) { return preview ? preview->info : (BrowsePreviewInfo){0}; }
const Page *browse_preview_page(const BrowsePreview *preview) { return preview ? &preview->page : NULL; }
const void *browse_preview_definition(const BrowsePreview *preview,size_t *bytes) { if (bytes) *bytes=preview ? preview->info.definition_bytes : 0; return preview ? preview->definition : NULL; }
const void *browse_preview_sources(const BrowsePreview *preview,size_t *bytes) { if (bytes) *bytes=preview ? preview->info.source_bytes : 0; return preview ? preview->sources : NULL; }
BrowsePreviewStatus browse_preview_capture(const BrowsePreviewContext *c,const BrowsePreviewTarget *target,size_t reserved,BrowsePreview **out) {
    if (!out) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewInfo info={0}; BrowsePreviewStatus status=measure(c,target,*out,&info); if (status!=BROWSE_PREVIEW_OK) return status;
    if (reserved<info.coexistence) return BROWSE_PREVIEW_LIMIT;
    BrowsePreview *next=dx_alloc(sizeof(*next)); next->info=info; next->definition=dx_alloc(info.definition_bytes);
    size_t written=0; AnalysisAssetStatus encoded=analysis_asset_encode(target->sheet,c->owner,c->definition,target->object_high_water,info.validation_scratch,next->definition,info.definition_bytes,&written);
    if (encoded!=ANALYSIS_ASSET_OK || written!=info.definition_bytes) { browse_preview_free(next); return encoded==ANALYSIS_ASSET_LIMIT ? BROWSE_PREVIEW_LIMIT : BROWSE_PREVIEW_INVALID; }
    bool selected[DX_SOURCE_VERSIONS]={0};
    for (size_t i=0;i<c->sources->count;i++) selected[i]=c->definition->source_version && c->sources->items[i].version==c->definition->source_version;
    next->sources=dx_alloc(info.source_bytes); written=0;
    if (source_versions_selection_encode(target->sheet,c->sources,target->object_high_water,selected,c->sources->count,next->sources,info.source_bytes,&written)!=SOURCE_VERSIONS_CODEC_OK || written!=info.source_bytes) { browse_preview_free(next); return BROWSE_PREVIEW_INVALID; }
    RangeResultProof current={0}; status=check(c,target,&current);
    if (status!=BROWSE_PREVIEW_OK) { browse_preview_free(next); return status; }
    const Page *page=current.page; Page *copy=&next->page;
    copy->column_count=page->column_count; copy->row_count=page->row_count; copy->offset=page->offset; copy->total=page->total; copy->count_pending=page->count_pending;
    copy->columns=dx_alloc(copy->column_count*sizeof(Column)); copy->cells=copy->row_count ? dx_alloc(copy->row_count*copy->column_count*sizeof(Cell)) : NULL;
    for (size_t i=0;i<copy->column_count;i++) {
        memcpy(copy->columns+i,page->columns+i,sizeof(Column)); Column *column=copy->columns+i;
        column->name=dx_strdup(column->name); column->type_name=dx_strdup(column->type_name);
        column->caption=column->caption ? dx_strdup(column->caption) : NULL; column->detail=column->detail ? dx_strdup(column->detail) : NULL;
        column->categories=NULL;
    }
    for (size_t i=0;i<copy->row_count*copy->column_count;i++) { memcpy(copy->cells+i,page->cells+i,sizeof(Cell)); copy->cells[i].text=page->cells[i].text ? dx_strdup(page->cells[i].text) : NULL; }
    copy->records=page->records; copy->records.ids=copy->records.count ? dx_alloc(copy->records.count*sizeof(uint64_t)) : NULL;
    if (copy->records.count) memcpy(copy->records.ids,page->records.ids,copy->records.count*sizeof(uint64_t));
    copy->records.source_key=page->records.source_key ? dx_strdup(page->records.source_key) : NULL;
    copy->bytes=info.owned-sizeof(*next)-info.definition_bytes-info.source_bytes;
    RangeResultProof now={0}; status=check(c,target,&now);
    if (status!=BROWSE_PREVIEW_OK) { browse_preview_free(next); return status; }
    BrowsePreview *old=*out; *out=next; browse_preview_free(old); return BROWSE_PREVIEW_OK;
}
