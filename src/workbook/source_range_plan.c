#include "source_range_plan.h"
#include "objects_asset.h"
#include "definition_dependencies.h"
#include "format_dependencies.h"
#include "cells_tsv_scan.h"
#include <stdlib.h>
static bool fail(char **error,const char *text) { if (error) { free(*error); *error=dx_strdup(text); } return false; }
static SourceRangeStatus mapped(CellRangeStatus s) { return s==CELL_RANGE_READY ? SOURCE_RANGE_READY : s==CELL_RANGE_NOOP ? SOURCE_RANGE_NOOP : s==CELL_RANGE_LIMIT ? SOURCE_RANGE_LIMIT : SOURCE_RANGE_INVALID; }
static SourceRangeStatus shape(SourceRangeContext c,size_t *scratch,char **error) {
    if (!c.sheet || !c.primary || !scratch || !c.sheet->id) return SOURCE_RANGE_INVALID;
    if (!c.ready) { fail(error,"Wait for the actual worksheet/result owner before editing this range."); return SOURCE_RANGE_PROTECTED; }
    const Worksheet *s=c.sheet; const FormatStore *f=&s->formats;
    if (f->count>DX_FORMAT_PATCHES || (f->count && !f->items) || f->scope_count>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS || (f->scope_count && !f->scopes) || f->undo_count>DX_FORMAT_HISTORY || f->redo_count>DX_FORMAT_HISTORY || format_owned_bytes(f,false)>DX_FORMAT_CURRENT_BYTES) return SOURCE_RANGE_INVALID;
    ObjectSnapshot objects={s->objects.items,s->objects.count}; ObjectsAssetInfo graph={0}; AnalysisAssetMeasure primary={0};
    if (objects_asset_measure(s->id,&objects,s->objects.next_id,&graph)!=OBJECTS_ASSET_OK || analysis_asset_measure(s->id,0,c.primary,s->objects.next_id,&primary)!=ANALYSIS_ASSET_OK) return SOURCE_RANGE_INVALID;
    size_t need=graph.measure.validation_scratch>primary.validation_scratch ? graph.measure.validation_scratch : primary.validation_scratch,formats=DX_FORMAT_VALIDATION_BYTES;
    if (f->scope_count) formats+=analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true)+f->scope_count;
    *scratch=need>formats ? need : formats; return SOURCE_RANGE_READY;
}
static uint64_t rows(SourceRangeContext c) { return c.count_pending ? UINT64_MAX : c.source_rows; }
static bool geometry(SourceRangeContext c,const CellStore *cells,char **error) {
    const ObjectStore *objects=&c.sheet->objects;
    for (size_t i=0;i<objects->count;i++) { const SheetObject *o=objects->items+i;
        if (!objects_fits(objects,(int)i,o->row,o->col,o->rows,o->columns,cells,rows(c),c.source_columns,error)) return false;
    }
    for (size_t i=0;i<cells->count;i++) if (cells->cells[i].row<rows(c) && cells->cells[i].col<c.source_columns) return fail(error,"Manual cells overlap the actual primary result.");
    return true;
}
static bool valid(SourceRangeContext c,char **error) {
    const Worksheet *s=c.sheet; ObjectSnapshot objects={s->objects.items,s->objects.count}; DefinitionDependencyView definitions={c.primary,&objects};
    DefinitionDependencySelection sources={0}; FormatDependencySelection formats={0}; FormatSnapshotView format=format_delta_store_view(&s->formats);
    return analysis_query_valid(c.primary,error) && analysis_filters_valid(c.primary,error) && analysis_sorts_valid(c.primary,error) && objects_valid(&s->objects,error) && format_valid(&s->formats,error) && source_versions_valid(&s->source_versions,s->objects.next_id,error) &&
        definition_dependencies_select(s->id,definitions,definitions,&s->source_versions,s->objects.next_id,&sources)==DEFINITION_DEPENDENCIES_OK && format_dependencies_select(s->id,format,format,&s->source_versions,s->objects.next_id,&formats)==FORMAT_DEPENDENCIES_OK;
}
static bool field(void *context,uint64_t row,size_t column,const char *text,size_t bytes,char **error) {
    (void)context; (void)row; (void)column; (void)text; (void)bytes; (void)error; return true;
}
SourceRangeStatus source_range_paste_measure(SourceRangeContext c,uint64_t row,size_t column,const char *text,SourceRangePlanInfo *out,char **error) {
    if (!out) return SOURCE_RANGE_INVALID; size_t scratch=0; SourceRangeStatus status=shape(c,&scratch,error); if (status!=SOURCE_RANGE_READY) return status;
    if (c.primary->kind!=SOURCE_NONE && !c.source_columns && !c.fetch_failed) { fail(error,"Wait for the source columns before editing this range."); return SOURCE_RANGE_PROTECTED; }
    size_t r=0,columns=0;
    if (!cells_tsv_scan(row,column,text,field,NULL,&r,&columns,error)) return SOURCE_RANGE_INVALID;
    if (!objects_fits(&c.sheet->objects,-1,row,column,r,columns,NULL,rows(c),c.source_columns,error)) return SOURCE_RANGE_PROTECTED;
    CellPastePlanInfo cells={0}; CellRangeStatus measured=cells_paste_measure(&c.sheet->cells,row,column,text,&cells,error); if (measured!=CELL_RANGE_READY) return mapped(measured);
    *out=(SourceRangePlanInfo){cells.owned,scratch,cells.rows,cells.columns}; return SOURCE_RANGE_READY;
}
SourceRangeStatus source_range_clear_measure(SourceRangeContext c,SheetRange r,SourceRangePlanInfo *out,char **error) {
    if (!out) return SOURCE_RANGE_INVALID; size_t scratch=0; SourceRangeStatus status=shape(c,&scratch,error); if (status!=SOURCE_RANGE_READY) return status;
    CellRangePlanInfo cells={0}; CellRangeStatus measured=cells_clear_measure(&c.sheet->cells,r.r0,r.r1,r.c0,r.c1,&cells,error); if (measured!=CELL_RANGE_READY) return mapped(measured);
    *out=(SourceRangePlanInfo){cells.owned,scratch,0,0}; return SOURCE_RANGE_READY;
}
static SourceRangeStatus preflight(SourceRangeContext c,SourceRangePlanInfo info,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error) {
    if (!out || (out->cells && out->cells==c.sheet->cells.cells)) { fail(error,"Choose independent manual range cell ownership."); return SOURCE_RANGE_INVALID; }
    if (info.owned>reserved || info.validation_scratch>validation_reserved) { fail(error,"The complete source range planning budget is unavailable."); return SOURCE_RANGE_LIMIT; }
    if (!valid(c,error) || !geometry(c,&c.sheet->cells,error)) { if (!error || !*error) fail(error,"The actual source range graph or dependencies are invalid."); return SOURCE_RANGE_INVALID; }
    return SOURCE_RANGE_READY;
}
SourceRangeStatus source_range_paste_plan(SourceRangeContext c,uint64_t row,size_t column,const char *text,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error) {
    SourceRangePlanInfo info={0}; SourceRangeStatus status=source_range_paste_measure(c,row,column,text,&info,error); if (status!=SOURCE_RANGE_READY) return status;
    status=preflight(c,info,reserved,validation_reserved,out,error); if (status!=SOURCE_RANGE_READY) return status;
    CellSnapshot next={0}; CellRangeStatus planned=cells_paste_plan(&c.sheet->cells,row,column,text,reserved,&next,error); if (planned!=CELL_RANGE_READY) return mapped(planned);
    CellStore projected={.cells=next.cells,.count=next.count,.capacity=next.count,.bytes=next.bytes};
    if (!geometry(c,&projected,error)) { cells_snapshot_free(&next); return SOURCE_RANGE_INVALID; }
    cells_snapshot_free(out); *out=next; return SOURCE_RANGE_READY;
}
SourceRangeStatus source_range_clear_plan(SourceRangeContext c,SheetRange r,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error) {
    SourceRangePlanInfo info={0}; SourceRangeStatus status=source_range_clear_measure(c,r,&info,error); if (status!=SOURCE_RANGE_READY) return status;
    status=preflight(c,info,reserved,validation_reserved,out,error); if (status!=SOURCE_RANGE_READY) return status;
    CellSnapshot next={0}; CellRangeStatus planned=cells_clear_plan(&c.sheet->cells,r.r0,r.r1,r.c0,r.c1,reserved,&next,error); if (planned!=CELL_RANGE_READY) return mapped(planned);
    cells_snapshot_free(out); *out=next; return SOURCE_RANGE_READY;
}
