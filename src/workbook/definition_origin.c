#include "definition_origin.h"
#include "objects_asset_internal.h"
#include <stdlib.h>
#include <string.h>
struct DefinitionOrigin {
    SourceFormatCommandTarget before; /* Native fence storage only, never encoded. */
    FormatStore formats;
    uint64_t primary_epoch,branch,epoch;
    DefinitionOriginPhase phase;
    void *arena;
    size_t arena_bytes;
    DefinitionOriginAssets assets;
};
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static const SheetObject *object(const ObjectStore *s,uint64_t id) { for (size_t i=0;i<s->count;i++) if (s->items[i].id==id) return s->items+i; return NULL; }
static Analysis *source(Worksheet *s,Analysis *primary,uint64_t id) {
    if (!id) return primary;
    for (size_t i=0;i<s->objects.count;i++) if (s->objects.items[i].id==id && s->objects.items[i].kind==OBJECT_TABLE) return &s->objects.items[i].source;
    return NULL;
}
static bool context_valid(const DefinitionOriginContext *c) { return c && c->primary && c->request_epoch && c->primary_epoch && workbook_identity_valid(c->book); }
static void capture(DefinitionOrigin *p,DefinitionOriginContext *c,Worksheet *s) {
    SourceFormatCommandTarget *t=&p->before;
    t->authority=c->primary==&s->analysis ? NULL : c->primary; t->request_epoch=*c->request_epoch; t->format_revision=s->formats.revision; t->history_last=s->history_last;
    memcpy(&t->primary,c->primary,sizeof(t->primary)); memcpy(&t->cached,&s->analysis,sizeof(t->cached)); memcpy(&t->objects,&s->objects,sizeof(t->objects)); memcpy(&t->sources,&s->source_versions,sizeof(t->sources));
    memset(t->items,0,sizeof(t->items)); memset(t->versions,0,sizeof(t->versions));
    if (s->objects.count) memcpy(t->items,s->objects.items,s->objects.count*sizeof(*t->items));
    if (s->source_versions.count) memcpy(t->versions,s->source_versions.items,s->source_versions.count*sizeof(*t->versions));
    memcpy(&p->formats,&s->formats,sizeof(p->formats)); p->primary_epoch=*c->primary_epoch; p->branch=c->book->history_branch;
}
static DefinitionOriginStatus check(DefinitionOriginContext *c,const DefinitionOrigin *p,char **error) {
    if (!context_valid(c) || !p) { fail(error,"Choose a valid working definition origin."); return DEFINITION_ORIGIN_INVALID; }
    if (p->phase==DEFINITION_ORIGIN_ADMITTED) { fail(error,"The working definition origin is immutable after admission."); return DEFINITION_ORIGIN_PROTECTED; }
    const SourceFormatCommandTarget *t=&p->before; Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id);
    if (!s || b->id.high!=t->owner.workbook.high || b->id.low!=t->owner.workbook.low || b->edit_session_id!=t->owner.document || s->edit_session_id!=t->owner.sheet ||
        b->command_revision!=t->owner.expected_document_revision || s->cells.revision!=t->owner.expected_revision || s->history_last!=t->history_last || b->history_branch!=p->branch ||
        *c->request_epoch!=t->request_epoch || *c->primary_epoch!=p->primary_epoch || c->primary!=(t->authority ? t->authority : &s->analysis) ||
        memcmp(c->primary,&t->primary,sizeof(t->primary)) || memcmp(&s->analysis,&t->cached,sizeof(t->cached)) || memcmp(&s->objects,&t->objects,sizeof(t->objects)) ||
        memcmp(&s->source_versions,&t->sources,sizeof(t->sources)) || memcmp(&s->formats,&p->formats,sizeof(p->formats)) ||
        s->objects.count>DX_MAX_OBJECTS || s->source_versions.count>DX_SOURCE_VERSIONS ||
        (s->objects.count && (!s->objects.items || memcmp(s->objects.items,t->items,s->objects.count*sizeof(*t->items)))) ||
        (s->source_versions.count && (!s->source_versions.items || memcmp(s->source_versions.items,t->versions,s->source_versions.count*sizeof(*t->versions))))) {
        fail(error,"The working definition origin changed; prepare its current owner again."); return DEFINITION_ORIGIN_STALE;
    }
    if (!c->ready || c->read_only) { fail(error,"The working definition origin is unavailable or read-only."); return DEFINITION_ORIGIN_PROTECTED; }
    return DEFINITION_ORIGIN_READY;
}
size_t definition_origin_owner_bytes(void) { return sizeof(DefinitionOrigin); }
DefinitionOriginState definition_origin_state(const DefinitionOrigin *p) { return p ? (DefinitionOriginState){p->phase,p->before.owner.sheet_id,p->epoch,sizeof(*p),p->arena_bytes} : (DefinitionOriginState){0}; }
DefinitionOriginStatus definition_origin_begin(DefinitionOriginContext *c,size_t sheet,size_t reserved,DefinitionOrigin **out,char **error) {
    if (!context_valid(c) || !out || *out || sheet>=c->book->count) { fail(error,"Choose an independent working definition origin."); return DEFINITION_ORIGIN_INVALID; }
    if (!c->ready || c->read_only) { fail(error,"Wait for an editable working definition owner."); return DEFINITION_ORIGIN_PROTECTED; }
    Worksheet *s=c->book->sheets+sheet;
    if (s->objects.count>DX_MAX_OBJECTS || (s->objects.count && !s->objects.items) || s->source_versions.count>DX_SOURCE_VERSIONS || (s->source_versions.count && !s->source_versions.items)) {
        fail(error,"Invalid bounded definition origin ownership."); return DEFINITION_ORIGIN_INVALID;
    }
    if (reserved<sizeof(DefinitionOrigin)) { fail(error,"The definition origin owner exceeds its admitted reservation."); return DEFINITION_ORIGIN_LIMIT; }
    CellEditTarget target={0}; if (!workbook_prepare_cell_edit(c->book,sheet,0,0,&target,error)) return DEFINITION_ORIGIN_INVALID;
    DefinitionOrigin *p=dx_alloc(sizeof(*p)); memset(p,0,sizeof(*p)); p->before.owner=target; p->epoch=1; capture(p,c,s); *out=p; return DEFINITION_ORIGIN_READY;
}
static DefinitionOriginStatus projection(DefinitionOriginContext *c,const DefinitionOrigin *p,const DefinitionOriginReply *r,SheetObject items[DX_MAX_OBJECTS],Analysis *primary,DefinitionOriginCompletionInfo *info,char **error) {
    DefinitionOriginStatus status=check(c,p,error); if (status!=DEFINITION_ORIGIN_READY) return status;
    if (!r || !r->analysis || !info || r->request!=p->before.request_epoch) { fail(error,"The default-relation reply does not match this origin request."); return DEFINITION_ORIGIN_STALE; }
    Worksheet *s=resolve(c->book,p->before.owner.sheet_id); const Analysis *old=source(s,c->primary,r->owner);
    const SheetObject *route=r->route ? object(&s->objects,r->route) : NULL;
    if ((r->route && (!route || (route->kind==OBJECT_TABLE ? route->id : route->source_id)!=r->owner || r->revision!=route->revision)) ||
        (!r->route && (r->owner || r->revision!=p->primary_epoch)) || !old) {
        fail(error,"The source default reply belongs to another query owner."); return DEFINITION_ORIGIN_STALE;
    }
    if (old->kind!=SOURCE_DATABASE || old->source_version || old->table || !old->path || !*old->path || r->analysis->kind!=SOURCE_DATABASE ||
        definition_baseline_status((DefinitionDependencyView){r->analysis,NULL})!=DEFINITION_BASELINE_READY) {
        fail(error,"Only a pending DATABASE default relation can enrich an open origin."); return DEFINITION_ORIGIN_PROTECTED;
    }
    if (old->schema && (!*old->schema || strcmp(old->schema,r->analysis->schema))) {
        fail(error,"A resolved default cannot replace an explicitly selected schema."); return DEFINITION_ORIGIN_STALE;
    }
    Analysis unchanged=*r->analysis; unchanged.schema=old->schema; unchanged.table=old->table;
    if (!definition_analysis_equal(old,&unchanged)) { fail(error,"The default relation reply changed authored source settings."); return DEFINITION_ORIGIN_STALE; }
    *primary=*c->primary;
    if (s->objects.count) memcpy(items,s->objects.items,s->objects.count*sizeof(*items));
    Analysis candidate=*old; candidate.schema=r->analysis->schema; candidate.table=r->analysis->table;
    if (!r->owner) *primary=candidate;
    else for (size_t i=0;i<s->objects.count;i++) if (items[i].id==r->owner) items[i].source=candidate;
    AnalysisAssetMeasure a={0},b={0}; ObjectSnapshot before={s->objects.items,s->objects.count},after={items,s->objects.count}; ObjectsAssetInfo x={0},y={0};
    if (analysis_asset_measure(s->id,0,c->primary,s->objects.next_id,&a)!=ANALYSIS_ASSET_OK || analysis_asset_measure(s->id,0,primary,s->objects.next_id,&b)!=ANALYSIS_ASSET_OK ||
        objects_asset_measure(s->id,&before,s->objects.next_id,&x)!=OBJECTS_ASSET_OK || objects_asset_measure(s->id,&after,s->objects.next_id,&y)!=OBJECTS_ASSET_OK) {
        fail(error,"The complete default-relation projection is invalid or too large."); return DEFINITION_ORIGIN_INVALID;
    }
    size_t scratch=a.validation_scratch>b.validation_scratch ? a.validation_scratch : b.validation_scratch;
    if (x.measure.validation_scratch>scratch) scratch=x.measure.validation_scratch;
    if (y.measure.validation_scratch>scratch) scratch=y.measure.validation_scratch;
    *info=(DefinitionOriginCompletionInfo){strlen(candidate.schema)+1+strlen(candidate.table)+1,scratch,r->owner}; return DEFINITION_ORIGIN_READY;
}
DefinitionOriginStatus definition_origin_completion_measure(DefinitionOriginContext *c,const DefinitionOrigin *p,const DefinitionOriginReply *r,DefinitionOriginCompletionInfo *out,char **error) {
    if (!out) { fail(error,"Choose a default relation projection result."); return DEFINITION_ORIGIN_INVALID; }
    SheetObject items[DX_MAX_OBJECTS]; Analysis primary={0}; DefinitionOriginCompletionInfo info={0};
    DefinitionOriginStatus status=projection(c,p,r,items,&primary,&info,error); if (status==DEFINITION_ORIGIN_READY) *out=info; return status;
}
DefinitionOriginStatus definition_origin_complete(DefinitionOriginContext *c,DefinitionOrigin *p,const DefinitionOriginReply *r,size_t reserved,char **error) {
    SheetObject items[DX_MAX_OBJECTS]; Analysis primary={0}; DefinitionOriginCompletionInfo info={0};
    DefinitionOriginStatus status=projection(c,p,r,items,&primary,&info,error); if (status!=DEFINITION_ORIGIN_READY) return status;
    if (info.owned>reserved || info.validation_scratch>c->validation_reserved || p->epoch==UINT64_MAX) { fail(error,"Default-relation ownership or grammar exceeds its admitted budget."); return DEFINITION_ORIGIN_LIMIT; }
    Worksheet *s=resolve(c->book,p->before.owner.sheet_id); ObjectStore projected={.items=items,.count=s->objects.count,.next_id=s->objects.next_id};
    if (!analysis_filters_valid(c->primary,error) || !analysis_filters_valid(&primary,error) || !objects_valid(&s->objects,error) || !objects_valid(&projected,error)) return DEFINITION_ORIGIN_INVALID;
    char *schema=dx_strdup(r->analysis->schema),*table=dx_strdup(r->analysis->table);
    status=check(c,p,error); if (status!=DEFINITION_ORIGIN_READY) { free(schema); free(table); return status; }
    Analysis *selected=source(s,c->primary,r->owner); free(selected->schema); selected->schema=schema; selected->table=table; c->book->dirty=true;
    capture(p,c,s); p->epoch++; return DEFINITION_ORIGIN_APPLIED;
}
static bool unowned_imported_formats(const Worksheet *s,const Analysis *primary) {
    bool imported=primary->kind!=SOURCE_NONE;
    for (size_t i=0;!imported && i<s->objects.count;i++) imported=s->objects.items[i].kind==OBJECT_TABLE;
    if (s->formats.source_aware || !imported) return false;
    for (size_t i=0;i<s->formats.count;i++) if (s->formats.items[i].range.r0 || s->formats.items[i].range.r1!=DX_SHEET_ROWS-1) return true;
    return false;
}
DefinitionOriginStatus definition_origin_scratch(DefinitionOriginContext *c,const DefinitionOrigin *p,size_t *out,char **error) {
    DefinitionOriginStatus status=check(c,p,error); if (status!=DEFINITION_ORIGIN_READY) return status;
    if (!out) { fail(error,"Choose a definition origin scratch result."); return DEFINITION_ORIGIN_INVALID; }
    Worksheet *s=resolve(c->book,p->before.owner.sheet_id); ObjectSnapshot objects={s->objects.items,s->objects.count};
    AnalysisAssetMeasure a={0}; ObjectsAssetInfo o={0};
    if (analysis_asset_measure(s->id,0,c->primary,s->objects.next_id,&a)!=ANALYSIS_ASSET_OK || objects_asset_measure(s->id,&objects,s->objects.next_id,&o)!=OBJECTS_ASSET_OK || s->formats.scope_count>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS) {
        fail(error,"Invalid bounded definition origin assets."); return DEFINITION_ORIGIN_INVALID;
    }
    size_t validation=a.validation_scratch>o.measure.validation_scratch ? a.validation_scratch : o.measure.validation_scratch;
    if (s->formats.scope_count) {
        size_t n=analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true)+s->formats.scope_count;
        if (s->formats.scope_count>DX_FORMAT_GUARDS && s->formats.scope_count*sizeof(const FormatScope *)>n) n=s->formats.scope_count*sizeof(const FormatScope *);
        if (n>validation) validation=n;
    }
    *out=validation; return DEFINITION_ORIGIN_READY;
}
static DefinitionOriginStatus seal_measure(DefinitionOriginContext *c,const DefinitionOrigin *p,DefinitionOriginSealInfo *out,char **error) {
    DefinitionOriginStatus status=check(c,p,error); if (status!=DEFINITION_ORIGIN_READY) return status;
    Worksheet *s=resolve(c->book,p->before.owner.sheet_id); ObjectSnapshot objects={s->objects.items,s->objects.count};
    if (definition_baseline_status((DefinitionDependencyView){c->primary,&objects})!=DEFINITION_BASELINE_READY) { fail(error,"Resolve all primary and Table defaults before admitting this definition origin."); return DEFINITION_ORIGIN_UNRESOLVED; }
    AnalysisAssetMeasure a={0}; ObjectsAssetInfo o={0};
    if (analysis_asset_measure(s->id,0,c->primary,s->objects.next_id,&a)!=ANALYSIS_ASSET_OK || objects_asset_measure(s->id,&objects,s->objects.next_id,&o)!=OBJECTS_ASSET_OK || s->formats.scope_count>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS) {
        fail(error,"Invalid bounded definition origin assets."); return DEFINITION_ORIGIN_INVALID;
    }
    size_t validation=a.validation_scratch>o.measure.validation_scratch ? a.validation_scratch : o.measure.validation_scratch;
    if (s->formats.scope_count) {
        size_t n=analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true)+s->formats.scope_count;
        if (s->formats.scope_count>DX_FORMAT_GUARDS && s->formats.scope_count*sizeof(const FormatScope *)>n) n=s->formats.scope_count*sizeof(const FormatScope *);
        if (n>validation) validation=n;
    }
    if (validation>c->validation_reserved || p->epoch==UINT64_MAX) { fail(error,"Definition origin grammar exceeds its admitted scratch budget."); return DEFINITION_ORIGIN_LIMIT; }
    size_t source_bytes=0,format_bytes=0;
    FormatDeltaStatus formats=format_delta_encoded_size(s->id,(FormatSnapshotView){0},format_delta_store_view(&s->formats),&format_bytes);
    DefinitionDependencySelection definitions={0}; FormatDependencySelection styling={0};
    if ((formats!=FORMAT_DELTA_OK && formats!=FORMAT_DELTA_NOOP) || source_versions_asset_size(s->id,&s->source_versions,s->objects.next_id,&source_bytes)!=SOURCE_VERSIONS_CODEC_OK ||
        definition_dependencies_select(s->id,(DefinitionDependencyView){c->primary,&objects},(DefinitionDependencyView){c->primary,&objects},&s->source_versions,s->objects.next_id,&definitions)!=DEFINITION_DEPENDENCIES_OK ||
        format_dependencies_select(s->id,(FormatSnapshotView){0},format_delta_store_view(&s->formats),&s->source_versions,s->objects.next_id,&styling)!=FORMAT_DEPENDENCIES_OK) {
        fail(error,"The definition origin source/format dependency graph is incomplete."); return DEFINITION_ORIGIN_INVALID;
    }
    if (unowned_imported_formats(s,c->primary)) { fail(error,"Imported finite formats require source-aware ownership before origin admission."); return DEFINITION_ORIGIN_UNRESOLVED; }
    size_t bytes=a.bytes+o.measure.bytes+source_bytes+format_bytes;
    if (formats==FORMAT_DELTA_NOOP && (s->formats.count || s->formats.scope_count || s->formats.source_aware)) {
        fail(error,"The empty definition origin format asset must have no dormant payload."); return DEFINITION_ORIGIN_INVALID;
    }
    *out=(DefinitionOriginSealInfo){bytes,validation}; return DEFINITION_ORIGIN_READY;
}
DefinitionOriginStatus definition_origin_seal_measure(DefinitionOriginContext *c,const DefinitionOrigin *p,DefinitionOriginSealInfo *out,char **error) {
    if (!out) { fail(error,"Choose a complete definition origin asset measure."); return DEFINITION_ORIGIN_INVALID; }
    DefinitionOriginSealInfo info={0}; DefinitionOriginStatus status=seal_measure(c,p,&info,error);
    if (status==DEFINITION_ORIGIN_READY) *out=info; return status;
}
DefinitionOriginStatus definition_origin_seal(DefinitionOriginContext *c,DefinitionOrigin *p,size_t reserved,char **error) {
    DefinitionOriginSealInfo info={0}; DefinitionOriginStatus status=seal_measure(c,p,&info,error); if (status!=DEFINITION_ORIGIN_READY) return status;
    if (info.asset_bytes>reserved) { fail(error,"The complete definition origin exceeds its admitted asset reservation."); return DEFINITION_ORIGIN_LIMIT; }
    Worksheet *s=resolve(c->book,p->before.owner.sheet_id); ObjectSnapshot objects={s->objects.items,s->objects.count};
    AnalysisAssetMeasure a={0}; ObjectsAssetInfo o={0}; size_t source_bytes=0,format_bytes=0;
    analysis_asset_measure(s->id,0,c->primary,s->objects.next_id,&a); objects_asset_measure(s->id,&objects,s->objects.next_id,&o);
    source_versions_asset_size(s->id,&s->source_versions,s->objects.next_id,&source_bytes);
    format_delta_encoded_size(s->id,(FormatSnapshotView){0},format_delta_store_view(&s->formats),&format_bytes);
    size_t bytes=info.asset_bytes;
    unsigned char *arena=dx_alloc(bytes),*at=arena; size_t written=0; bool ok=true;
    DefinitionOriginAssets assets={.sheet=s->id,.object_high_water=s->objects.next_id,.source_high_water=s->source_versions.high_water,
        .primary=at,.primary_bytes=a.bytes,.object_bytes=o.measure.bytes,.source_bytes=source_bytes,.format_bytes=format_bytes};
    ok=analysis_asset_encode(s->id,0,c->primary,s->objects.next_id,c->validation_reserved,at,a.bytes,&written)==ANALYSIS_ASSET_OK && written==a.bytes; at+=a.bytes; assets.objects=at;
    if (ok) ok=objects_asset_encode_unpublished(s->id,&objects,s->objects.next_id,c->validation_reserved,at,o.measure.bytes,&written)==OBJECTS_ASSET_OK && written==o.measure.bytes;
    at+=o.measure.bytes; assets.sources=at;
    if (ok) ok=source_versions_asset_encode(s->id,&s->source_versions,s->objects.next_id,at,source_bytes,&written)==SOURCE_VERSIONS_CODEC_OK && written==source_bytes;
    at+=source_bytes;
    if (format_bytes) { assets.formats=at; if (ok) ok=format_delta_encode(s->id,(FormatSnapshotView){0},format_delta_store_view(&s->formats),at,format_bytes,&written)==FORMAT_DELTA_OK && written==format_bytes; }
    if (!ok) { free(arena); fail(error,"The canonical definition origin assets could not be admitted together."); return DEFINITION_ORIGIN_INVALID; }
    status=check(c,p,error); if (status!=DEFINITION_ORIGIN_READY) { free(arena); return status; }
    p->arena=arena; p->arena_bytes=bytes; p->assets=assets; p->phase=DEFINITION_ORIGIN_ADMITTED; p->epoch++; return DEFINITION_ORIGIN_APPLIED;
}
DefinitionOriginAssets definition_origin_assets(const DefinitionOrigin *p) { return p && p->phase==DEFINITION_ORIGIN_ADMITTED ? p->assets : (DefinitionOriginAssets){0}; }
void definition_origin_free(DefinitionOrigin **owned) { if (owned && *owned) { free((*owned)->arena); free(*owned); *owned=NULL; } }
