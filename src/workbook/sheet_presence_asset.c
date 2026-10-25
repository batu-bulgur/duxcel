#include "sheet_presence_asset.h"
#include "analysis_asset.h"
#include "objects_asset.h"
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
