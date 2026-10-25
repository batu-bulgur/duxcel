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
    *out=(SourceRangePlanInfo){cells.owned,scratch,cells.rows,cells.columns,cells.count}; return SOURCE_RANGE_READY;
}
SourceRangeStatus source_range_clear_measure(SourceRangeContext c,SheetRange r,SourceRangePlanInfo *out,char **error) {
    if (!out) return SOURCE_RANGE_INVALID; size_t scratch=0; SourceRangeStatus status=shape(c,&scratch,error); if (status!=SOURCE_RANGE_READY) return status;
    CellRangePlanInfo cells={0}; CellRangeStatus measured=cells_clear_measure(&c.sheet->cells,r.r0,r.r1,r.c0,r.c1,&cells,error); if (measured!=CELL_RANGE_READY) return mapped(measured);
    *out=(SourceRangePlanInfo){cells.owned,scratch,0,0,cells.count}; return SOURCE_RANGE_READY;
}
static SourceRangeStatus preflight(SourceRangeContext c,SourceRangePlanInfo info,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error) {
    if (!out || (out->cells && out->cells==c.sheet->cells.cells)) { fail(error,"Choose independent manual range cell ownership."); return SOURCE_RANGE_INVALID; }
    if (info.owned>reserved || info.validation_scratch>validation_reserved) { fail(error,"The complete source range planning budget is unavailable."); return SOURCE_RANGE_LIMIT; }
    if (!valid(c,error)) { if (!error || !*error) fail(error,"The actual source range graph or dependencies are invalid."); return SOURCE_RANGE_INVALID; }
    return SOURCE_RANGE_READY;
}
SourceRangeStatus source_range_paste_plan(SourceRangeContext c,uint64_t row,size_t column,const char *text,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error) {
    SourceRangePlanInfo info={0}; SourceRangeStatus status=source_range_paste_measure(c,row,column,text,&info,error); if (status!=SOURCE_RANGE_READY) return status;
    status=preflight(c,info,reserved,validation_reserved,out,error); if (status!=SOURCE_RANGE_READY) return status;
    CellSnapshot next={0}; CellRangeStatus planned=cells_paste_plan(&c.sheet->cells,row,column,text,reserved,&next,error); if (planned!=CELL_RANGE_READY) return mapped(planned);
    cells_snapshot_free(out); *out=next; return SOURCE_RANGE_READY;
}
SourceRangeStatus source_range_clear_plan(SourceRangeContext c,SheetRange r,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error) {
    SourceRangePlanInfo info={0}; SourceRangeStatus status=source_range_clear_measure(c,r,&info,error); if (status!=SOURCE_RANGE_READY) return status;
    status=preflight(c,info,reserved,validation_reserved,out,error); if (status!=SOURCE_RANGE_READY) return status;
    CellSnapshot next={0}; CellRangeStatus planned=cells_clear_plan(&c.sheet->cells,r.r0,r.r1,r.c0,r.c1,reserved,&next,error); if (planned!=CELL_RANGE_READY) return mapped(planned);
    cells_snapshot_free(out); *out=next; return SOURCE_RANGE_READY;
}
static SourceRangeStatus move_policy(SourceMoveContext c,size_t *scratch,char **error) {
    SourceRangeStatus status=shape(c.range,scratch,error); if (status!=SOURCE_RANGE_READY) return status;
    if (c.range.primary->kind!=SOURCE_NONE && (c.range.count_pending || c.range.fetch_failed || !c.range.source_columns)) { fail(error,"Wait for complete primary result extents before moving this range."); return SOURCE_RANGE_PROTECTED; }
    const ObjectStore *objects=&c.range.sheet->objects;
    if (c.table_count>DX_MAX_OBJECTS || (c.table_count && !c.tables)) return SOURCE_RANGE_INVALID;
    size_t tables=0;
    for (size_t i=0;i<objects->count;i++) if (objects->items[i].kind==OBJECT_TABLE) {
        const SheetObject *o=objects->items+i; const SourceTableExtent *proof=NULL; tables++;
        for (size_t j=0;j<c.table_count;j++) if (c.tables[j].object==o->id) { if (proof) return SOURCE_RANGE_INVALID; proof=c.tables+j; }
        if (!proof || !proof->ready || proof->count_pending || proof->failed) { fail(error,"Wait for complete Table result extents before moving this range."); return SOURCE_RANGE_PROTECTED; }
        if (proof->rows!=o->rows || proof->columns!=o->columns) { fail(error,"The Table footprint differs from its actual complete result extent."); return SOURCE_RANGE_INVALID; }
    }
    return tables==c.table_count ? SOURCE_RANGE_READY : SOURCE_RANGE_INVALID;
}
static uint32_t move_mask(const Worksheet *s,SheetRange r) {
    uint32_t mask=0; for (size_t i=0;i<s->objects.count;i++) { const SheetObject *o=s->objects.items+i;
        if (o->row>=r.r0 && o->row<=r.r1 && o->rows-1<=r.r1-o->row && o->col>=r.c0 && o->col<=r.c1 && o->columns-1<=r.c1-o->col) mask|=UINT32_C(1)<<i;
    }
    return mask;
}
static bool move_formats(const FormatStore *f,SheetRange r,int64_t dr,int dc) {
    SheetRange to={(uint64_t)((int64_t)r.r0+dr),(uint64_t)((int64_t)r.r1+dr),(size_t)((int64_t)r.c0+dc),(size_t)((int64_t)r.c1+dc)};
    for (size_t i=0;i<f->count;i++) { FormatRange p=f->items[i].range;
        if (f->items[i].selector!=FORMAT_RECORDS && ((p.r0<=r.r1 && r.r0<=p.r1 && p.c0<=r.c1 && r.c0<=p.c1) || (p.r0<=to.r1 && to.r0<=p.r1 && p.c0<=to.c1 && to.c0<=p.c1))) return true;
    }
    return false;
}
SourceRangeStatus source_range_move_measure(SourceMoveContext c,SheetRange r,int64_t dr,int dc,bool replace,MixedRangePlanInfo *out,char **error) {
    if (!out) return SOURCE_RANGE_INVALID; size_t scratch=0; SourceRangeStatus status=move_policy(c,&scratch,error); if (status!=SOURCE_RANGE_READY) return status;
    const Worksheet *s=c.range.sheet; CellRangePlanInfo cells={0}; CellRangeStatus measured=cells_range_measure(&s->cells,r.r0,r.r1,r.c0,r.c1,dr,dc,replace,&cells,error);
    if (measured!=CELL_RANGE_READY && measured!=CELL_RANGE_NOOP && measured!=CELL_RANGE_OCCUPIED) return mapped(measured);
    SheetMoveStatus fit=worksheet_check_move(s,r,dr,dc,c.range.source_rows,c.range.source_columns,error);
    if (fit==SHEET_MOVE_INVALID) return SOURCE_RANGE_INVALID;
    if (fit==SHEET_MOVE_NEEDS_REPLACE && !replace) return SOURCE_RANGE_OCCUPIED;
    if (measured==CELL_RANGE_OCCUPIED) return SOURCE_RANGE_OCCUPIED;
    uint32_t mask=move_mask(s,r);
    if (replace && mask) { fail(error,"Replacement is available only for manual cell ranges."); return SOURCE_RANGE_PROTECTED; }
    if ((!dr && !dc) || (measured==CELL_RANGE_NOOP && !mask && !move_formats(&s->formats,r,dr,dc))) return SOURCE_RANGE_NOOP;
    ObjectSnapshot before={s->objects.items,s->objects.count}; size_t objects=objects_snapshot_owned_bytes(&before); if (objects==SIZE_MAX) return SOURCE_RANGE_INVALID;
    if (measured==CELL_RANGE_NOOP) cells.owned=s->cells.count*sizeof(SheetCell)+s->cells.bytes;
    *out=(MixedRangePlanInfo){cells.owned+objects+2u*DX_FORMAT_CURRENT_BYTES,scratch}; return SOURCE_RANGE_READY;
}
static bool projection_valid(SourceRangeContext c,const MixedRangePlan *plan,char **error) {
    Worksheet projected=*c.sheet; projected.objects.items=plan->objects.items; projected.objects.count=plan->objects.count;
    projected.formats.items=plan->formats.items; projected.formats.count=plan->formats.count; projected.formats.scopes=plan->formats.scopes; projected.formats.scope_count=plan->formats.scope_count; projected.formats.source_aware=plan->formats.source_aware;
    SourceRangeContext actual=c; actual.sheet=&projected;
    CellStore cells={.cells=plan->cells.cells,.count=plan->cells.count,.capacity=plan->cells.count,.bytes=plan->cells.bytes};
    for (size_t i=0;i<projected.objects.count;i++) { const SheetObject *o=projected.objects.items+i;
        if (!objects_fits(&projected.objects,(int)i,o->row,o->col,o->rows,o->columns,&cells,c.source_rows,c.source_columns,error)) return false;
    }
    return valid(actual,error);
}
SourceRangeStatus source_range_move_plan(SourceMoveContext c,SheetRange r,int64_t dr,int dc,bool replace,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error) {
    if (!out) return SOURCE_RANGE_INVALID; MixedRangePlanInfo info={0}; SourceRangeStatus status=source_range_move_measure(c,r,dr,dc,replace,&info,error); if (status!=SOURCE_RANGE_READY) return status;
    if (info.owned>reserved || info.validation_scratch>validation_reserved) { fail(error,"Complete source range movement exceeds its admitted reservation."); return SOURCE_RANGE_LIMIT; }
    if (!valid(c.range,error)) return SOURCE_RANGE_INVALID;
    const Worksheet *s=c.range.sheet; MixedRangePlan next={.object_high_water=s->objects.next_id};
    CellRangeStatus cells=cells_range_plan(&s->cells,r.r0,r.r1,r.c0,r.c1,dr,dc,replace,reserved,&next.cells,error);
    if (cells!=CELL_RANGE_READY && cells!=CELL_RANGE_NOOP) return mapped(cells);
    if (cells==CELL_RANGE_NOOP && s->cells.count) {
        next.cells=(CellSnapshot){.count=s->cells.count,.bytes=s->cells.bytes}; next.cells.cells=dx_alloc(s->cells.count*sizeof(SheetCell));
        for (size_t i=0;i<s->cells.count;i++) { next.cells.cells[i]=s->cells.cells[i]; next.cells.cells[i].text=dx_strdup(s->cells.cells[i].text); }
    }
    ObjectStore objects={0}; if (s->objects.count) { objects_copy(&objects,&s->objects); next.objects=(ObjectSnapshot){objects.items,objects.count}; objects.items=NULL; objects.count=0; objects_free(&objects); }
    uint32_t mask=move_mask(s,r);
    for (size_t i=0;i<next.objects.count;i++) if (mask&(UINT32_C(1)<<i)) { next.objects.items[i].row=(uint64_t)((int64_t)next.objects.items[i].row+dr); next.objects.items[i].col=(size_t)((int64_t)next.objects.items[i].col+dc); }
    if (!format_plan_move(&s->formats,(FormatRange){r.r0,r.r1,r.c0,r.c1},dr,dc,&next.formats,error)) { mixed_range_plan_free(&next); return SOURCE_RANGE_LIMIT; }
    if (!projection_valid(c.range,&next,error)) { mixed_range_plan_free(&next); return SOURCE_RANGE_INVALID; }
    next.effects=(cells==CELL_RANGE_READY ? 2u : 0u)|(mask ? 4u : 0u)|(!format_snapshot_matches(&s->formats,&next.formats) ? 8u : 0u);
    if (!next.effects) { mixed_range_plan_free(&next); return SOURCE_RANGE_NOOP; }
    mixed_range_plan_free(out); *out=next; return SOURCE_RANGE_READY;
}

static bool source_row_mask(const ObjectStore *objects,uint64_t row,uint64_t count,bool insert,uint32_t *mask,char **error) {
    *mask=0;
    for (size_t i=0;i<objects->count;i++) {
        const SheetObject *o=objects->items+i;
        if ((insert && row>o->row && row<o->row+o->rows) || (!insert && row<o->row+o->rows && o->row<row+count)) return fail(error,"These rows intersect a pivot or chart. Choose rows outside its boundary.");
        if (insert && o->row>=row && o->row+o->rows>DX_SHEET_ROWS-count) return fail(error,"Insert would move a pivot or chart beyond the last row.");
        if (insert ? o->row>=row : o->row>=row+count) *mask|=UINT32_C(1)<<i;
    }
    return true;
}
SourceRangeStatus source_range_rows_measure(SourceMoveContext c,uint64_t row,uint64_t count,bool insert,MixedRangePlanInfo *out,char **error) {
    if (!out) return SOURCE_RANGE_INVALID; size_t source_scratch=0; SourceRangeStatus ready=move_policy(c,&source_scratch,error); if (ready!=SOURCE_RANGE_READY) return ready;
    const Worksheet *s=c.range.sheet;
    if (row<c.range.source_rows) { fail(error,"Imported rows are read-only. Choose worksheet rows below the source."); return SOURCE_RANGE_PROTECTED; }
    if (row>=DX_SHEET_ROWS || !count || count>DX_SHEET_ROWS-row || !s->id || s->formats.count>DX_FORMAT_PATCHES || (s->formats.count && !s->formats.items)) return SOURCE_RANGE_INVALID;
    ObjectSnapshot before={s->objects.items,s->objects.count}; ObjectsAssetInfo objects={0};
    if (objects_asset_measure(s->id,&before,s->objects.next_id,&objects)!=OBJECTS_ASSET_OK) return SOURCE_RANGE_INVALID;
    CellRangePlanInfo cells={0}; CellRangeStatus status=cells_rows_measure(&s->cells,row,count,insert,&cells,error);
    if (status!=CELL_RANGE_READY && status!=CELL_RANGE_NOOP) return status==CELL_RANGE_LIMIT ? SOURCE_RANGE_LIMIT : SOURCE_RANGE_INVALID;
    uint32_t mask=0;
    if (!source_row_mask(&s->objects,row,count,insert,&mask,error)) return SOURCE_RANGE_INVALID;
    bool relevant=false;
    for (size_t i=0;i<s->formats.count;i++) { const FormatPatch *p=s->formats.items+i; FormatRange r=p->range; relevant|=p->selector!=FORMAT_RECORDS && r.r1>=row && !(p->selector==FORMAT_SPATIAL && !r.r0 && r.r1==DX_SHEET_ROWS-1); }
    if (status==CELL_RANGE_NOOP && !mask && !relevant) return SOURCE_RANGE_NOOP;
    size_t object_bytes=objects_snapshot_owned_bytes(&before); if (object_bytes==SIZE_MAX) return SOURCE_RANGE_INVALID;
    if (status==CELL_RANGE_NOOP) cells.owned=s->cells.count*sizeof(SheetCell)+s->cells.bytes;
    size_t scratch=objects.measure.validation_scratch>DX_FORMAT_VALIDATION_BYTES ? objects.measure.validation_scratch : DX_FORMAT_VALIDATION_BYTES;
    if (scratch<source_scratch) scratch=source_scratch;
    *out=(MixedRangePlanInfo){cells.owned+object_bytes+2u*DX_FORMAT_CURRENT_BYTES,scratch}; return SOURCE_RANGE_READY;
}
SourceRangeStatus source_range_rows_plan(SourceMoveContext c,uint64_t row,uint64_t count,bool insert,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error) {
    if (!out) return SOURCE_RANGE_INVALID; MixedRangePlanInfo info={0};
    SourceRangeStatus status=source_range_rows_measure(c,row,count,insert,&info,error); if (status!=SOURCE_RANGE_READY) return status;
    if (info.owned>reserved || info.validation_scratch>validation_reserved) { fail(error,"Row composite ownership or grammar exceeds its admitted reservation."); return SOURCE_RANGE_LIMIT; }
    const Worksheet *s=c.range.sheet;
    if (!valid(c.range,error)) return SOURCE_RANGE_INVALID;
    MixedRangePlan next={.object_high_water=s->objects.next_id}; ObjectStore objects={0};
    CellRangeStatus cells=cells_rows_plan(&s->cells,row,count,insert,info.owned,&next.cells,error);
    if (cells!=CELL_RANGE_READY && cells!=CELL_RANGE_NOOP) return SOURCE_RANGE_INVALID;
    if (cells==CELL_RANGE_NOOP && s->cells.count) {
        next.cells=(CellSnapshot){.count=s->cells.count,.bytes=s->cells.bytes}; next.cells.cells=dx_alloc(s->cells.count*sizeof(SheetCell));
        for (size_t i=0;i<s->cells.count;i++) { next.cells.cells[i]=s->cells.cells[i]; next.cells.cells[i].text=dx_strdup(s->cells.cells[i].text); }
    }
    if (s->objects.count) { objects_copy(&objects,&s->objects); next.objects=(ObjectSnapshot){objects.items,objects.count}; objects.items=NULL; objects.count=0; objects_free(&objects); }
    uint32_t mask=0;
    if (!source_row_mask(&s->objects,row,count,insert,&mask,error)) { mixed_range_plan_free(&next); return SOURCE_RANGE_INVALID; }
    for (size_t i=0;i<next.objects.count;i++) if (mask&(UINT32_C(1)<<i)) next.objects.items[i].row=insert ? next.objects.items[i].row+count : next.objects.items[i].row-count;
    if (!format_plan_axis(&s->formats,true,row,count,insert,&next.formats,error)) { mixed_range_plan_free(&next); return SOURCE_RANGE_LIMIT; }
    if (!projection_valid(c.range,&next,error)) { mixed_range_plan_free(&next); return SOURCE_RANGE_INVALID; }
    next.effects=(cells==CELL_RANGE_READY ? 2u : 0u)|(mask ? 4u : 0u)|(!format_snapshot_matches(&s->formats,&next.formats) ? 8u : 0u);
    if (!next.effects) { mixed_range_plan_free(&next); return SOURCE_RANGE_NOOP; }
    mixed_range_plan_free(out); *out=next; return SOURCE_RANGE_READY;
}
