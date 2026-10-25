#include "sheet_presence_asset.h"
#include "analysis_asset.h"
#include "objects_asset.h"
#include "objects_asset_internal.h"
#include "definition_dependencies.h"
#include "format_dependencies.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct SheetPresenceAsset { Workbook *snapshot; };
static bool add(size_t *value,size_t extra) { if (*value>SIZE_MAX-extra) return false; *value+=extra; return true; }
static bool text(const char *s,size_t max,size_t *owned,bool nonempty) {
    if (!s) return false;
    size_t n=0; while (n<=max && s[n]) n++;
    if (n>max || (nonempty && !n)) return false;
    return add(owned,n+1);
}
static bool viewport(const SheetView *v) {
    return isfinite(v->horizontal) && v->horizontal>=0 && isfinite(v->fraction) && v->fraction>=0 && v->fraction<1 &&
        (!v->canvas_zoom || (isfinite(v->canvas_zoom) && v->canvas_zoom>=.5f && v->canvas_zoom<=2.5f)) &&
        v->row<DX_SHEET_ROWS && v->anchor_row<DX_SHEET_ROWS && v->end_row<DX_SHEET_ROWS && v->top<DX_SHEET_ROWS &&
        v->col<16384 && v->anchor_col<16384 && v->end_col<16384 && v->selection<=3 && v->column_count<=16384 &&
        (!v->column_count || (v->columns && v->widths));
}
SheetPresenceStatus sheet_presence_measure(SheetPresenceView view,SheetPresenceMeasure *out) {
    if (!out || !view.sheet || !view.primary || (!view.document.high && !view.document.low) || !view.sheet->id || view.sheet->id>view.sheet_high_water) return SHEET_PRESENCE_INVALID;
    const Worksheet *s=view.sheet; const SheetView *v=&s->view;
    SheetPresenceMeasure next={.owned=sizeof(SheetPresenceAsset)+sizeof(Workbook)+sizeof(Worksheet)};
    if (!text(s->name,4096,&next.owned,true) || !viewport(v)) return SHEET_PRESENCE_INVALID;
    for (const unsigned char *p=(const unsigned char *)s->name;*p;p++) if (*p<32 || *p==127) return SHEET_PRESENCE_INVALID;
    AnalysisAssetMeasure primary={0}; ObjectSnapshot snapshot={s->objects.items,s->objects.count}; ObjectsAssetInfo objects={0};
    if (analysis_asset_measure(s->id,0,view.primary,s->objects.next_id,&primary)!=ANALYSIS_ASSET_OK ||
        objects_asset_measure(s->id,&snapshot,s->objects.next_id,&objects)!=OBJECTS_ASSET_OK) return SHEET_PRESENCE_INVALID;
    if (!add(&next.owned,primary.owned-sizeof(Analysis)) || !add(&next.owned,objects.measure.owned)) return SHEET_PRESENCE_LIMIT;
    const CellStore *cells=&s->cells; size_t cell_text=0;
    if (cells->count>DX_MAX_EDIT_CELLS || cells->count>cells->capacity || (cells->count && !cells->cells)) return SHEET_PRESENCE_INVALID;
    for (size_t i=0;i<cells->count;i++) {
        const SheetCell *c=cells->cells+i;
        if (c->row>=DX_SHEET_ROWS || c->col>=16384 || !text(c->text,65536,&cell_text,true) ||
            (i && (cells->cells[i-1].row>c->row || (cells->cells[i-1].row==c->row && cells->cells[i-1].col>=c->col)))) return SHEET_PRESENCE_INVALID;
    }
    if (cell_text!=cells->bytes || cell_text>DX_CELL_BYTES) return SHEET_PRESENCE_INVALID;
    size_t formats=format_owned_bytes(&s->formats,false);
    if (formats==SIZE_MAX || formats>DX_FORMAT_CURRENT_BYTES) return SHEET_PRESENCE_INVALID;
    if (!add(&next.owned,cells->count*sizeof(SheetCell)) || !add(&next.owned,cell_text) || !add(&next.owned,formats) ||
        !add(&next.owned,v->column_count*(sizeof(char *)+sizeof(float)))) return SHEET_PRESENCE_LIMIT;
    for (size_t i=0;i<v->column_count;i++) if (!isfinite(v->widths[i]) || v->widths[i]<52 || v->widths[i]>1200 || !text(v->columns[i],65536,&next.owned,false)) return SHEET_PRESENCE_INVALID;
    const SourceVersions *ledger=&s->source_versions;
    if (ledger->count>DX_SOURCE_VERSIONS || ledger->capacity>DX_SOURCE_VERSIONS || ledger->count>ledger->capacity || (ledger->capacity && !ledger->items)) return SHEET_PRESENCE_INVALID;
    size_t source_bytes=ledger->count*sizeof(SourceVersion);
    for (size_t i=0;i<ledger->count;i++) {
        const SourceVersion *row=ledger->items+i;
        if (row->raw.bytes>DX_SOURCE_VERSIONS_BYTES || (row->raw.bytes && !row->raw.data) || !text(row->path,65536,&source_bytes,true) || !add(&source_bytes,row->raw.bytes)) return SHEET_PRESENCE_INVALID;
    }
    if (source_bytes>DX_SOURCE_VERSIONS_BYTES || !add(&next.owned,source_bytes)) return SHEET_PRESENCE_LIMIT;
    size_t grammar=analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true);
    next.validation_scratch=primary.validation_scratch;
    if (!add(&next.validation_scratch,objects.measure.validation_scratch) || !add(&next.validation_scratch,DX_FORMAT_VALIDATION_BYTES) ||
        !add(&next.validation_scratch,DX_FORMAT_PATCHES*DX_FORMAT_GUARDS) || !add(&next.validation_scratch,grammar)) return SHEET_PRESENCE_LIMIT;
    if (next.owned>DX_SHEET_PRESENCE_OWNED_BYTES) return SHEET_PRESENCE_LIMIT;
    next.staging=next.owned; if (!add(&next.staging,next.validation_scratch)) return SHEET_PRESENCE_LIMIT;
    *out=next; return SHEET_PRESENCE_OK;
}
static bool valid(SheetPresenceView view,char **error) {
    const Worksheet *s=view.sheet; const Analysis *a=view.primary; ObjectSnapshot objects={s->objects.items,s->objects.count};
    if (!analysis_query_valid(a,error) || !analysis_filters_valid(a,error) || !analysis_sorts_valid(a,error) ||
        !objects_valid(&s->objects,error) || !format_valid(&s->formats,error) || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) return false;
    for (size_t i=0;i<s->objects.count;i++) {
        const SheetObject *o=s->objects.items+i;
        if (!objects_fits(&s->objects,(int)i,o->row,o->col,o->rows,o->columns,&s->cells,0,0,error)) return false;
    }
    DefinitionDependencyView definitions={a,&objects}; DefinitionDependencySelection dependencies={0}; FormatDependencySelection formats={0};
    FormatSnapshotView fv=format_delta_store_view(&s->formats);
    if (definition_dependencies_select(s->id,definitions,definitions,&s->source_versions,s->objects.next_id,&dependencies)!=DEFINITION_DEPENDENCIES_OK ||
        format_dependencies_select(s->id,fv,fv,&s->source_versions,s->objects.next_id,&formats)!=FORMAT_DEPENDENCIES_OK) return false;
    for (size_t i=0;i<s->formats.scope_count;i++) {
        const FormatScope *scope=s->formats.scopes+i; SourceScopeInfo info={0};
        if (!source_scope_validate(scope->definition,scope->bytes,&info,error) || info.owner!=scope->owner) return false;
        const SourceVersion *row=source_versions_find(&s->source_versions,info.version);
        if (!row || !row->raw.bytes || row->owner!=scope->owner || row->kind!=info.source_kind || row->raw.bytes!=info.raw_bytes || memcmp(row->raw.data,info.raw,info.raw_bytes)) return false;
    }
    /* Every ledger row is retained, including retired/pending unreferenced rows;
       a live analytical object can never own raw source metadata. */
    for (size_t i=0;i<s->source_versions.count;i++) for (size_t j=0;j<s->objects.count;j++)
        if (s->source_versions.items[i].owner==s->objects.items[j].id && s->objects.items[j].kind!=OBJECT_TABLE) return false;
    return true;
}
SheetPresenceStatus sheet_presence_capture(SheetPresenceView view,size_t reserved,SheetPresenceAsset **out,char **error) {
    if (!out) return SHEET_PRESENCE_INVALID;
    SheetPresenceMeasure measure; SheetPresenceStatus status=sheet_presence_measure(view,&measure); if (status) return status;
    if (measure.staging>reserved) return SHEET_PRESENCE_LIMIT;
    char *local=NULL; bool checked=valid(view,error ? error : &local); free(local); if (!checked) return SHEET_PRESENCE_INVALID;
    Worksheet projected=*view.sheet; projected.analysis=*view.primary;
    Workbook source={.sheets=&projected,.id=view.document,.sheet_high_water=view.sheet_high_water,.count=1,.user_scale=1};
    Workbook *snapshot=workbook_snapshot(&source); if (!snapshot) return SHEET_PRESENCE_INVALID;
    SheetPresenceAsset *next=dx_alloc(sizeof(*next)); next->snapshot=snapshot;
    sheet_presence_free(out); *out=next; return SHEET_PRESENCE_OK;
}
SheetPresenceView sheet_presence_view(const SheetPresenceAsset *asset) {
    if (!asset) return (SheetPresenceView){0};
    const Workbook *b=asset->snapshot; return (SheetPresenceView){b->id,b->sheet_high_water,b->sheets,&b->sheets[0].analysis};
}
void sheet_presence_free(SheetPresenceAsset **asset) {
    if (!asset || !*asset) return; workbook_free((*asset)->snapshot); free((*asset)->snapshot); free(*asset); *asset=NULL;
}

/* The frame owns current-state components only, never workbook file IO. */
typedef char PresenceFloatWidths[(sizeof(float)==4 && sizeof(double)==8) ? 1 : -1];
typedef struct {
    SheetPresenceWireInfo info;
    SheetView view;
    uint32_t lengths[4];
    const unsigned char *name,*columns,*cells,*parts[4];
    size_t name_bytes;
    bool hidden;
} PresencePlan;
static uint32_t float_bits(float value) { uint32_t bits; memcpy(&bits,&value,4); return bits; }
static uint64_t double_bits(double value) { uint64_t bits; memcpy(&bits,&value,8); return bits; }
static float read_float(uint32_t bits) { float value; memcpy(&value,&bits,4); return value; }
static double read_double(uint64_t bits) { double value; memcpy(&value,&bits,8); return value; }
static SheetPresenceStatus wire_plan(const SheetPresenceAsset *asset,PresencePlan *out) {
    if (!asset || !out) return SHEET_PRESENCE_INVALID;
    SheetPresenceView v=sheet_presence_view(asset); const Worksheet *s=v.sheet; SheetPresenceMeasure m;
    SheetPresenceStatus status=sheet_presence_measure(v,&m); if (status) return status;
    AnalysisAssetMeasure a; ObjectsAssetInfo o; ObjectSnapshot objects={s->objects.items,s->objects.count}; FormatSnapshotBodyInfo f;
    if (analysis_asset_measure(s->id,0,v.primary,s->objects.next_id,&a)!=ANALYSIS_ASSET_OK || objects_asset_measure(s->id,&objects,s->objects.next_id,&o)!=OBJECTS_ASSET_OK ||
        format_snapshot_body_measure(format_delta_store_view(&s->formats),&f)!=FORMAT_DELTA_OK) return SHEET_PRESENCE_INVALID;
    size_t ledger=DX_SOURCE_VERSIONS_HEADER_BYTES+s->source_versions.count*DX_SOURCE_VERSION_ROW_BYTES;
    for (size_t i=0;i<s->source_versions.count;i++) if (!add(&ledger,strlen(s->source_versions.items[i].path)) || !add(&ledger,s->source_versions.items[i].raw.bytes)) return SHEET_PRESENCE_LIMIT;
    PresencePlan p={.info={v.document,s->id,v.sheet_high_water,s->objects.next_id,s->source_versions.high_water,0,m.owned,m.validation_scratch,s->cells.count,s->view.column_count},
        .view=s->view,.name_bytes=strlen(s->name),.hidden=s->hidden};
    const size_t sizes[]={a.bytes,o.measure.bytes,f.bytes,ledger}; size_t bytes=DX_SHEET_PRESENCE_HEADER_BYTES+p.name_bytes;
    for (size_t i=0;i<4;i++) { if (sizes[i]>UINT32_MAX || !add(&bytes,sizes[i])) return SHEET_PRESENCE_LIMIT; p.lengths[i]=(uint32_t)sizes[i]; }
    for (size_t i=0;i<s->view.column_count;i++) if (!add(&bytes,8+strlen(s->view.columns[i]))) return SHEET_PRESENCE_LIMIT;
    for (size_t i=0;i<s->cells.count;i++) if (!add(&bytes,16+strlen(s->cells.cells[i].text))) return SHEET_PRESENCE_LIMIT;
    if (bytes>DX_SHEET_PRESENCE_WIRE_BYTES) return SHEET_PRESENCE_LIMIT;
    p.info.bytes=bytes; *out=p; return SHEET_PRESENCE_OK;
}
SheetPresenceStatus sheet_presence_wire_measure(const SheetPresenceAsset *asset,SheetPresenceWireInfo *out) {
    if (!out) return SHEET_PRESENCE_INVALID; PresencePlan p; SheetPresenceStatus status=wire_plan(asset,&p); if (!status) *out=p.info; return status;
}
void sheet_presence_packet_free(SheetPresencePacket *packet) { if (packet) { free(packet->data); *packet=(SheetPresencePacket){0}; } }
SheetPresenceStatus sheet_presence_encode(const SheetPresenceAsset *asset,size_t reserved,SheetPresencePacket *out) {
    if (!out) return SHEET_PRESENCE_INVALID; PresencePlan p; SheetPresenceStatus status=wire_plan(asset,&p); if (status) return status;
    if (p.info.bytes>reserved || p.info.validation_scratch>reserved-p.info.bytes) return SHEET_PRESENCE_LIMIT;
    SheetPresenceView view=sheet_presence_view(asset); const Worksheet *s=view.sheet;
    SheetPresencePacket next={dx_alloc(p.info.bytes),p.info.bytes}; DefinitionWireWriter w={next.data};
    memcpy(w.p,"DXSP",4); w.p+=4; definition_wire_put(&w,DX_SHEET_PRESENCE_ENCODING,2); definition_wire_put(&w,0,2);
    definition_wire_put(&w,view.document.high,8); definition_wire_put(&w,view.document.low,8); definition_wire_put(&w,view.sheet_high_water,8); definition_wire_put(&w,s->id,8); definition_wire_put(&w,s->objects.next_id,8);
    definition_wire_put(&w,p.name_bytes,4); definition_wire_put(&w,s->view.column_count,4); definition_wire_put(&w,s->cells.count,4);
    for (size_t i=0;i<4;i++) definition_wire_put(&w,p.lengths[i],4); definition_wire_put(&w,s->hidden,4);
    const SheetView *v=&s->view; definition_wire_put(&w,v->row,8); definition_wire_put(&w,v->anchor_row,8); definition_wire_put(&w,v->end_row,8); definition_wire_put(&w,v->top,8);
    definition_wire_put(&w,v->col,2); definition_wire_put(&w,v->anchor_col,2); definition_wire_put(&w,v->end_col,2); definition_wire_put(&w,v->selection,2);
    definition_wire_put(&w,float_bits(v->horizontal),4); definition_wire_put(&w,float_bits(v->canvas_zoom),4); definition_wire_put(&w,double_bits(v->fraction),8);
    memcpy(w.p,s->name,p.name_bytes); w.p+=p.name_bytes;
    for (size_t i=0;i<v->column_count;i++) { size_t n=strlen(v->columns[i]); definition_wire_put(&w,n,4); definition_wire_put(&w,float_bits(v->widths[i]),4); memcpy(w.p,v->columns[i],n); w.p+=n; }
    for (size_t i=0;i<s->cells.count;i++) { const SheetCell *c=s->cells.cells+i; size_t n=strlen(c->text); definition_wire_put(&w,c->row,8); definition_wire_put(&w,c->col,2); definition_wire_put(&w,0,2); definition_wire_put(&w,n,4); memcpy(w.p,c->text,n); w.p+=n; }
    ObjectSnapshot objects={s->objects.items,s->objects.count}; size_t written=0;
    bool ok=analysis_asset_encode(s->id,0,&s->analysis,s->objects.next_id,p.info.validation_scratch,w.p,p.lengths[0],&written)==ANALYSIS_ASSET_OK && written==p.lengths[0]; w.p+=p.lengths[0]; written=0;
    ok=ok && objects_asset_encode_unpublished(s->id,&objects,s->objects.next_id,p.info.validation_scratch,w.p,p.lengths[1],&written)==OBJECTS_ASSET_OK && written==p.lengths[1]; w.p+=p.lengths[1]; written=0;
    ok=ok && format_snapshot_body_encode(format_delta_store_view(&s->formats),p.info.validation_scratch,w.p,p.lengths[2],&written)==FORMAT_DELTA_OK && written==p.lengths[2]; w.p+=p.lengths[2]; written=0;
    ok=ok && source_versions_asset_encode(s->id,&s->source_versions,s->objects.next_id,w.p,p.lengths[3],&written)==SOURCE_VERSIONS_CODEC_OK && written==p.lengths[3]; w.p+=p.lengths[3];
    if (!ok || (size_t)(w.p-(unsigned char *)next.data)!=next.bytes) { sheet_presence_packet_free(&next); return SHEET_PRESENCE_INVALID; }
    sheet_presence_packet_free(out); *out=next; return SHEET_PRESENCE_OK;
}
typedef struct { const unsigned char *data; size_t bytes,at; bool ok; } PresenceReader;
static const unsigned char *take(PresenceReader *r,size_t bytes) { if (!r->ok || bytes>r->bytes-r->at) { r->ok=false; return NULL; } const unsigned char *p=r->data+r->at; r->at+=bytes; return p; }
static uint64_t number(PresenceReader *r,unsigned bytes) { const unsigned char *p=take(r,bytes); uint64_t n=0; if (p) for (unsigned i=0;i<bytes;i++) n|=(uint64_t)p[i]<<(8*i); return n; }
static const unsigned char *wire_text(PresenceReader *r,size_t bytes,bool nonempty) { if (bytes>65536 || (nonempty && !bytes)) { r->ok=false; return NULL; } const unsigned char *p=take(r,bytes); if (p && memchr(p,0,bytes)) r->ok=false; return p; }
static SheetPresenceStatus inspect_presence(const void *data,size_t bytes,PresencePlan *out) {
    if (!data || !out || bytes<6 || bytes>DX_SHEET_PRESENCE_WIRE_BYTES || memcmp(data,"DXSP",4)) return SHEET_PRESENCE_INVALID;
    PresenceReader r={data,bytes,4,true}; if (number(&r,2)!=DX_SHEET_PRESENCE_ENCODING) return SHEET_PRESENCE_UNSUPPORTED;
    if (bytes<DX_SHEET_PRESENCE_HEADER_BYTES || number(&r,2)) return SHEET_PRESENCE_INVALID;
    PresencePlan p={0}; p.info.bytes=bytes; p.info.document.high=number(&r,8); p.info.document.low=number(&r,8); p.info.sheet_high_water=number(&r,8); p.info.sheet=number(&r,8); p.info.object_high_water=number(&r,8);
    p.name_bytes=(size_t)number(&r,4); p.info.columns=(size_t)number(&r,4); p.info.cells=(size_t)number(&r,4);
    for (size_t i=0;i<4;i++) p.lengths[i]=(uint32_t)number(&r,4); unsigned hidden=(unsigned)number(&r,4);
    SheetView *v=&p.view; v->row=number(&r,8); v->anchor_row=number(&r,8); v->end_row=number(&r,8); v->top=number(&r,8);
    v->col=(size_t)number(&r,2); v->anchor_col=(size_t)number(&r,2); v->end_col=(size_t)number(&r,2); v->selection=(unsigned)number(&r,2);
    v->horizontal=read_float((uint32_t)number(&r,4)); v->canvas_zoom=read_float((uint32_t)number(&r,4)); v->fraction=read_double(number(&r,8));
    v->column_count=p.info.columns; SheetView preferences=*v; preferences.column_count=0;
    if (!r.ok || (!p.info.document.high && !p.info.document.low) || !p.info.sheet || p.info.sheet>p.info.sheet_high_water || hidden>1 || !p.name_bytes || p.name_bytes>4096 || p.info.cells>DX_MAX_EDIT_CELLS || p.info.columns>16384 || !viewport(&preferences)) return SHEET_PRESENCE_INVALID;
    p.hidden=hidden!=0; p.name=wire_text(&r,p.name_bytes,true); if (!r.ok) return SHEET_PRESENCE_INVALID;
    for (size_t i=0;i<p.name_bytes;i++) if (p.name[i]<32 || p.name[i]==127) return SHEET_PRESENCE_INVALID;
    p.info.owned=sizeof(SheetPresenceAsset)+sizeof(Workbook)+sizeof(Worksheet)+p.name_bytes+1+p.info.columns*(sizeof(char *)+sizeof(float))+p.info.cells*sizeof(SheetCell);
    p.columns=r.data+r.at;
    for (size_t i=0;i<p.info.columns;i++) { size_t n=(size_t)number(&r,4); float width=read_float((uint32_t)number(&r,4)); wire_text(&r,n,false); if (!r.ok || !isfinite(width) || width<52 || width>1200 || !add(&p.info.owned,n+1)) return SHEET_PRESENCE_INVALID; }
    p.cells=r.data+r.at; size_t texts=0; uint64_t previous_row=0; size_t previous_col=0;
    for (size_t i=0;i<p.info.cells;i++) {
        uint64_t row=number(&r,8); size_t col=(size_t)number(&r,2); unsigned zero=(unsigned)number(&r,2); size_t n=(size_t)number(&r,4); wire_text(&r,n,true);
        if (!r.ok || zero || row>=DX_SHEET_ROWS || col>=16384 || (i && (row<previous_row || (row==previous_row && col<=previous_col))) || !add(&texts,n+1) || texts>DX_CELL_BYTES) return SHEET_PRESENCE_INVALID;
        previous_row=row; previous_col=col;
    }
    if (!add(&p.info.owned,texts)) return SHEET_PRESENCE_INVALID;
    for (size_t i=0;i<4;i++) { p.parts[i]=take(&r,p.lengths[i]); if (!r.ok) return SHEET_PRESENCE_INVALID; }
    if (r.at!=r.bytes) return SHEET_PRESENCE_INVALID;
    DefinitionAssetInfo a; ObjectsAssetInfo o; FormatSnapshotBodyInfo f; SourceVersionsAssetInfo ledger;
    AnalysisAssetStatus as=analysis_asset_preflight(p.parts[0],p.lengths[0],p.info.object_high_water,&a);
    ObjectsAssetStatus os=objects_asset_preflight(p.parts[1],p.lengths[1],&o);
    SourceVersionsCodecStatus ls=source_versions_asset_preflight(p.parts[3],p.lengths[3],p.info.object_high_water,&ledger);
    if (as==ANALYSIS_ASSET_UNSUPPORTED || os==OBJECTS_ASSET_UNSUPPORTED || ls==SOURCE_VERSIONS_CODEC_UNSUPPORTED) return SHEET_PRESENCE_UNSUPPORTED;
    if (as!=ANALYSIS_ASSET_OK || os!=OBJECTS_ASSET_OK || ls!=SOURCE_VERSIONS_CODEC_OK || format_snapshot_body_preflight(p.parts[2],p.lengths[2],&f)!=FORMAT_DELTA_OK ||
        a.sheet!=p.info.sheet || a.owner || o.sheet!=p.info.sheet || o.high_water!=p.info.object_high_water || ledger.sheet!=p.info.sheet) return SHEET_PRESENCE_INVALID;
    if (!add(&p.info.owned,a.measure.owned-sizeof(Analysis)) || !add(&p.info.owned,o.measure.owned) || !add(&p.info.owned,f.owned) || !add(&p.info.owned,ledger.owned) || p.info.owned>DX_SHEET_PRESENCE_OWNED_BYTES) return SHEET_PRESENCE_LIMIT;
    p.info.source_high_water=ledger.high_water;
    p.info.validation_scratch=a.measure.validation_scratch;
    size_t grammar=analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true);
    if (!add(&p.info.validation_scratch,o.measure.validation_scratch) || !add(&p.info.validation_scratch,DX_FORMAT_VALIDATION_BYTES+DX_FORMAT_PATCHES*DX_FORMAT_GUARDS) || !add(&p.info.validation_scratch,grammar)) return SHEET_PRESENCE_LIMIT;
    *out=p; return SHEET_PRESENCE_OK;
}
SheetPresenceStatus sheet_presence_preflight(const void *data,size_t bytes,SheetPresenceWireInfo *out) {
    if (!out) return SHEET_PRESENCE_INVALID; PresencePlan p; SheetPresenceStatus status=inspect_presence(data,bytes,&p); if (!status) *out=p.info; return status;
}
static char *copy_text(const unsigned char *data,size_t bytes) { char *s=dx_alloc(bytes+1); memcpy(s,data,bytes); s[bytes]=0; return s; }
SheetPresenceStatus sheet_presence_decode(const void *data,size_t bytes,size_t reserved,SheetPresenceAsset **out,char **error) {
    if (!out) return SHEET_PRESENCE_INVALID;
    PresencePlan p; SheetPresenceStatus status=inspect_presence(data,bytes,&p); if (status) return status;
    if (p.info.owned>reserved || p.info.validation_scratch>reserved-p.info.owned) return SHEET_PRESENCE_LIMIT;
    Workbook *book=dx_alloc(sizeof(*book)); workbook_init(book); book->id=p.info.document; book->sheet_high_water=p.info.sheet_high_water; book->count=1; book->sheets=dx_alloc(sizeof(Worksheet));
    Worksheet *s=book->sheets; *s=(Worksheet){.id=p.info.sheet,.hidden=p.hidden,.name=copy_text(p.name,p.name_bytes),.view=p.view};
    s->view.columns=dx_alloc(p.info.columns*sizeof(char *)); s->view.widths=dx_alloc(p.info.columns*sizeof(float));
    PresenceReader columns={p.columns,bytes-(size_t)(p.columns-(const unsigned char *)data),0,true};
    for (size_t i=0;i<p.info.columns;i++) { size_t n=(size_t)number(&columns,4); s->view.widths[i]=read_float((uint32_t)number(&columns,4)); s->view.columns[i]=copy_text(take(&columns,n),n); }
    s->cells.count=s->cells.capacity=p.info.cells; s->cells.cells=dx_alloc(p.info.cells*sizeof(SheetCell)); PresenceReader cells={p.cells,bytes-(size_t)(p.cells-(const unsigned char *)data),0,true};
    for (size_t i=0;i<p.info.cells;i++) { SheetCell *c=s->cells.cells+i; c->row=number(&cells,8); c->col=(size_t)number(&cells,2); number(&cells,2); size_t n=(size_t)number(&cells,4); c->text=copy_text(take(&cells,n),n); s->cells.bytes+=n+1; }
    AnalysisAsset a={0}; ObjectsAsset o={0}; FormatSnapshot f={0}; SourceVersionsAsset ledger={0};
    bool ok=analysis_asset_decode(p.parts[0],p.lengths[0],p.info.object_high_water,reserved,&a)==ANALYSIS_ASSET_OK &&
        objects_asset_decode(p.parts[1],p.lengths[1],reserved,&o)==OBJECTS_ASSET_OK && format_snapshot_body_decode(p.parts[2],p.lengths[2],reserved,&f)==FORMAT_DELTA_OK &&
        source_versions_asset_decode(p.parts[3],p.lengths[3],p.info.object_high_water,reserved,&ledger)==SOURCE_VERSIONS_CODEC_OK;
    if (ok) {
        s->analysis=a.definition; a=(AnalysisAsset){0}; s->objects.items=o.objects.items; s->objects.count=o.objects.count; s->objects.next_id=o.high_water; o=(ObjectsAsset){0};
        s->formats=(FormatStore){.items=f.items,.count=f.count,.scopes=f.scopes,.scope_count=f.scope_count,.source_aware=f.source_aware}; f=(FormatSnapshot){0};
        s->source_versions=ledger.versions; ledger=(SourceVersionsAsset){0};
        SheetPresenceView view={book->id,book->sheet_high_water,s,&s->analysis}; char *local=NULL; ok=valid(view,error ? error : &local); free(local);
    }
    analysis_asset_free(&a); objects_asset_free(&o); format_snapshot_free(&f); source_versions_asset_free(&ledger);
    if (!ok) { workbook_free(book); free(book); return SHEET_PRESENCE_INVALID; }
    SheetPresenceAsset *next=dx_alloc(sizeof(*next)); next->snapshot=book; sheet_presence_free(out); *out=next; return SHEET_PRESENCE_OK;
}
