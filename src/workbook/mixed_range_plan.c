#include "mixed_range_plan.h"
#include "format_delta_codec.h"
#include "cells_tsv_scan.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *text) { if (error) { free(*error); *error=dx_strdup(text); } return false; }
static bool contains(SheetRange r,const SheetObject *o) { return o->row>=r.r0 && o->row<=r.r1 && o->rows-1<=r.r1-o->row && o->col>=r.c0 && o->col<=r.c1 && o->columns-1<=r.c1-o->col; }
static bool eligible(const Worksheet *s,const Analysis *primary) {
    if (!s || !primary || primary->kind!=SOURCE_NONE || s->analysis.kind!=SOURCE_NONE || s->formats.source_aware || s->formats.scope_count) return false;
    if (s->objects.count>DX_MAX_OBJECTS || (s->objects.count && !s->objects.items)) return false;
    for (size_t i=0;i<s->objects.count;i++) if (s->objects.items[i].kind==OBJECT_TABLE || s->objects.items[i].source_id || s->objects.items[i].source.kind!=SOURCE_NONE) return false;
    return true;
}
static uint32_t selected(const Worksheet *s,SheetRange r) {
    uint32_t mask=0; for (size_t i=0;i<s->objects.count;i++) if (contains(r,s->objects.items+i)) mask|=UINT32_C(1)<<i; return mask;
}
static bool geometry(const ObjectStore *objects,const CellStore *cells,char **error) {
    for (size_t i=0;i<objects->count;i++) { const SheetObject *o=objects->items+i;
        if (!objects_fits(objects,(int)i,o->row,o->col,o->rows,o->columns,cells,0,0,error)) return false;
    }
    return true;
}
static bool format_relevant(const FormatStore *f,SheetRange r,int64_t dr,int dc) {
    SheetRange to={(uint64_t)((int64_t)r.r0+dr),(uint64_t)((int64_t)r.r1+dr),(size_t)((int64_t)r.c0+dc),(size_t)((int64_t)r.c1+dc)};
    for (size_t i=0;i<f->count;i++) { FormatRange p=f->items[i].range;
        if ((p.r0<=r.r1 && r.r0<=p.r1 && p.c0<=r.c1 && r.c0<=p.c1) || (p.r0<=to.r1 && to.r0<=p.r1 && p.c0<=to.c1 && to.c0<=p.c1)) return true;
    }
    return false;
}
MixedRangeStatus mixed_range_measure(const Worksheet *s,const Analysis *primary,SheetRange r,int64_t dr,int dc,bool replace,MixedRangePlanInfo *out,char **error) {
    if (!out || !s || !primary) return MIXED_RANGE_INVALID;
    if (!eligible(s,primary)) { fail(error,"Mixed ranges require source-free templates and manual formats."); return MIXED_RANGE_PROTECTED; }
    if (!s->id || s->formats.count>DX_FORMAT_PATCHES || (s->formats.count && !s->formats.items)) return MIXED_RANGE_INVALID;
    ObjectSnapshot before={s->objects.items,s->objects.count}; ObjectsAssetInfo objects={0};
    if (objects_asset_measure(s->id,&before,s->objects.next_id,&objects)!=OBJECTS_ASSET_OK) return MIXED_RANGE_INVALID;
    CellRangePlanInfo cells={0}; CellRangeStatus status=cells_range_measure(&s->cells,r.r0,r.r1,r.c0,r.c1,dr,dc,replace,&cells,error);
    if (status!=CELL_RANGE_READY && status!=CELL_RANGE_NOOP) return status==CELL_RANGE_OCCUPIED ? MIXED_RANGE_OCCUPIED : status==CELL_RANGE_LIMIT ? MIXED_RANGE_LIMIT : MIXED_RANGE_INVALID;
    if (!geometry(&s->objects,&s->cells,error)) return MIXED_RANGE_INVALID;
    SheetMoveStatus fit=worksheet_check_move(s,r,dr,dc,0,0,NULL);
    if (fit==SHEET_MOVE_INVALID) { fail(error,"Select whole objects and an empty object destination inside the worksheet."); return MIXED_RANGE_INVALID; }
    if (fit==SHEET_MOVE_NEEDS_REPLACE && !replace) return MIXED_RANGE_OCCUPIED;
    uint32_t mask=selected(s,r);
    if (replace && mask) { fail(error,"Replacement is available only for manual cell ranges."); return MIXED_RANGE_PROTECTED; }
    if ((!dr && !dc) || (status==CELL_RANGE_NOOP && !mask && !format_relevant(&s->formats,r,dr,dc))) return MIXED_RANGE_NOOP;
    size_t object_bytes=objects_snapshot_owned_bytes(&before); if (object_bytes==SIZE_MAX) return MIXED_RANGE_INVALID;
    if (status==CELL_RANGE_NOOP) cells.owned=s->cells.count*sizeof(SheetCell)+s->cells.bytes;
    size_t scratch=objects.measure.validation_scratch>DX_FORMAT_VALIDATION_BYTES ? objects.measure.validation_scratch : DX_FORMAT_VALIDATION_BYTES;
    *out=(MixedRangePlanInfo){cells.owned+object_bytes+2u*DX_FORMAT_CURRENT_BYTES,scratch}; return MIXED_RANGE_READY;
}
void mixed_range_plan_free(MixedRangePlan *p) { if (p) { cells_snapshot_free(&p->cells); objects_snapshot_free(&p->objects); format_snapshot_free(&p->formats); column_width_free(&p->widths); *p=(MixedRangePlan){0}; } }
MixedRangeStatus mixed_range_plan(const Worksheet *s,const Analysis *primary,SheetRange r,int64_t dr,int dc,bool replace,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error) {
    if (!out) return MIXED_RANGE_INVALID; MixedRangePlanInfo info={0};
    MixedRangeStatus status=mixed_range_measure(s,primary,r,dr,dc,replace,&info,error); if (status!=MIXED_RANGE_READY) return status;
    if (info.owned>reserved || info.validation_scratch>validation_reserved) { fail(error,"Mixed range ownership or grammar exceeds its admitted reservation."); return MIXED_RANGE_LIMIT; }
    if (!objects_valid(&s->objects,error) || !format_valid(&s->formats,error)) return MIXED_RANGE_INVALID;
    MixedRangePlan next={.object_high_water=s->objects.next_id}; ObjectStore objects={0};
    CellRangeStatus cells=cells_range_plan(&s->cells,r.r0,r.r1,r.c0,r.c1,dr,dc,replace,info.owned,&next.cells,error);
    if (cells!=CELL_RANGE_READY && cells!=CELL_RANGE_NOOP) return MIXED_RANGE_INVALID;
    if (cells==CELL_RANGE_NOOP && s->cells.count) {
        next.cells=(CellSnapshot){.count=s->cells.count,.bytes=s->cells.bytes}; next.cells.cells=dx_alloc(s->cells.count*sizeof(SheetCell));
        for (size_t i=0;i<s->cells.count;i++) { next.cells.cells[i]=s->cells.cells[i]; next.cells.cells[i].text=dx_strdup(s->cells.cells[i].text); }
    }
    if (s->objects.count) { objects_copy(&objects,&s->objects); next.objects=(ObjectSnapshot){objects.items,objects.count}; objects.items=NULL; objects.count=0; objects_free(&objects); }
    uint32_t mask=selected(s,r);
    for (size_t i=0;i<next.objects.count;i++) if (mask&(UINT32_C(1)<<i)) { next.objects.items[i].row=(uint64_t)((int64_t)next.objects.items[i].row+dr); next.objects.items[i].col=(size_t)((int64_t)next.objects.items[i].col+dc); }
    if (!format_plan_move(&s->formats,(FormatRange){r.r0,r.r1,r.c0,r.c1},dr,dc,&next.formats,error)) { mixed_range_plan_free(&next); return MIXED_RANGE_LIMIT; }
    ObjectStore projected={.items=next.objects.items,.count=next.objects.count,.next_id=s->objects.next_id}; CellStore projected_cells={.cells=next.cells.cells,.count=next.cells.count,.capacity=next.cells.count,.bytes=next.cells.bytes};
    if (!objects_valid(&projected,error) || !geometry(&projected,&projected_cells,error)) { mixed_range_plan_free(&next); return MIXED_RANGE_INVALID; }
    next.effects=(cells==CELL_RANGE_READY ? 2u : 0u)|(mask ? 4u : 0u)|(!format_snapshot_matches(&s->formats,&next.formats) ? 8u : 0u);
    if (!next.effects) { mixed_range_plan_free(&next); return MIXED_RANGE_NOOP; }
    mixed_range_plan_free(out); *out=next; return MIXED_RANGE_READY;
}

static bool row_mask(const ObjectStore *objects,uint64_t row,uint64_t count,bool insert,uint32_t *mask,char **error) {
    *mask=0;
    for (size_t i=0;i<objects->count;i++) {
        const SheetObject *o=objects->items+i;
        if ((insert && row>o->row && row<o->row+o->rows) || (!insert && row<o->row+o->rows && o->row<row+count)) return fail(error,"These rows intersect a pivot or chart. Choose rows outside its boundary.");
        if (insert && o->row>=row && o->row+o->rows>DX_SHEET_ROWS-count) return fail(error,"Insert would move a pivot or chart beyond the last row.");
        if (insert ? o->row>=row : o->row>=row+count) *mask|=UINT32_C(1)<<i;
    }
    return true;
}
MixedRangeStatus mixed_rows_measure(const Worksheet *s,const Analysis *primary,uint64_t row,uint64_t count,bool insert,MixedRangePlanInfo *out,char **error) {
    if (!out || !s || !primary) return MIXED_RANGE_INVALID;
    if (!eligible(s,primary)) { fail(error,"Row composites require source-free templates and manual formats."); return MIXED_RANGE_PROTECTED; }
    if (row>=DX_SHEET_ROWS || !count || count>DX_SHEET_ROWS-row || !s->id || s->formats.count>DX_FORMAT_PATCHES || (s->formats.count && !s->formats.items)) return MIXED_RANGE_INVALID;
    ObjectSnapshot before={s->objects.items,s->objects.count}; ObjectsAssetInfo objects={0};
    if (objects_asset_measure(s->id,&before,s->objects.next_id,&objects)!=OBJECTS_ASSET_OK) return MIXED_RANGE_INVALID;
    CellRangePlanInfo cells={0}; CellRangeStatus status=cells_rows_measure(&s->cells,row,count,insert,&cells,error);
    if (status!=CELL_RANGE_READY && status!=CELL_RANGE_NOOP) return status==CELL_RANGE_LIMIT ? MIXED_RANGE_LIMIT : MIXED_RANGE_INVALID;
    uint32_t mask=0;
    if (!geometry(&s->objects,&s->cells,error) || !row_mask(&s->objects,row,count,insert,&mask,error)) return MIXED_RANGE_INVALID;
    bool relevant=false;
    for (size_t i=0;i<s->formats.count;i++) { FormatRange r=s->formats.items[i].range; relevant|=r.r1>=row && (r.r0 || r.r1!=DX_SHEET_ROWS-1); }
    if (status==CELL_RANGE_NOOP && !mask && !relevant) return MIXED_RANGE_NOOP;
    size_t object_bytes=objects_snapshot_owned_bytes(&before); if (object_bytes==SIZE_MAX) return MIXED_RANGE_INVALID;
    if (status==CELL_RANGE_NOOP) cells.owned=s->cells.count*sizeof(SheetCell)+s->cells.bytes;
    size_t scratch=objects.measure.validation_scratch>DX_FORMAT_VALIDATION_BYTES ? objects.measure.validation_scratch : DX_FORMAT_VALIDATION_BYTES;
    *out=(MixedRangePlanInfo){cells.owned+object_bytes+2u*DX_FORMAT_CURRENT_BYTES,scratch}; return MIXED_RANGE_READY;
}
MixedRangeStatus mixed_rows_plan(const Worksheet *s,const Analysis *primary,uint64_t row,uint64_t count,bool insert,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error) {
    if (!out) return MIXED_RANGE_INVALID; MixedRangePlanInfo info={0};
    MixedRangeStatus status=mixed_rows_measure(s,primary,row,count,insert,&info,error); if (status!=MIXED_RANGE_READY) return status;
    if (info.owned>reserved || info.validation_scratch>validation_reserved) { fail(error,"Row composite ownership or grammar exceeds its admitted reservation."); return MIXED_RANGE_LIMIT; }
    if (!objects_valid(&s->objects,error) || !format_valid(&s->formats,error)) return MIXED_RANGE_INVALID;
    MixedRangePlan next={.object_high_water=s->objects.next_id}; ObjectStore objects={0};
    CellRangeStatus cells=cells_rows_plan(&s->cells,row,count,insert,info.owned,&next.cells,error);
    if (cells!=CELL_RANGE_READY && cells!=CELL_RANGE_NOOP) return MIXED_RANGE_INVALID;
    if (cells==CELL_RANGE_NOOP && s->cells.count) {
        next.cells=(CellSnapshot){.count=s->cells.count,.bytes=s->cells.bytes}; next.cells.cells=dx_alloc(s->cells.count*sizeof(SheetCell));
        for (size_t i=0;i<s->cells.count;i++) { next.cells.cells[i]=s->cells.cells[i]; next.cells.cells[i].text=dx_strdup(s->cells.cells[i].text); }
    }
    if (s->objects.count) { objects_copy(&objects,&s->objects); next.objects=(ObjectSnapshot){objects.items,objects.count}; objects.items=NULL; objects.count=0; objects_free(&objects); }
    uint32_t mask=0;
    if (!row_mask(&s->objects,row,count,insert,&mask,error)) { mixed_range_plan_free(&next); return MIXED_RANGE_INVALID; }
    for (size_t i=0;i<next.objects.count;i++) if (mask&(UINT32_C(1)<<i)) next.objects.items[i].row=insert ? next.objects.items[i].row+count : next.objects.items[i].row-count;
    if (!format_plan_axis(&s->formats,true,row,count,insert,&next.formats,error)) { mixed_range_plan_free(&next); return MIXED_RANGE_LIMIT; }
    ObjectStore projected={.items=next.objects.items,.count=next.objects.count,.next_id=s->objects.next_id}; CellStore projected_cells={.cells=next.cells.cells,.count=next.cells.count,.capacity=next.cells.count,.bytes=next.cells.bytes};
    if (!objects_valid(&projected,error) || !geometry(&projected,&projected_cells,error)) { mixed_range_plan_free(&next); return MIXED_RANGE_INVALID; }
    next.effects=(cells==CELL_RANGE_READY ? 2u : 0u)|(mask ? 4u : 0u)|(!format_snapshot_matches(&s->formats,&next.formats) ? 8u : 0u);
    if (!next.effects) { mixed_range_plan_free(&next); return MIXED_RANGE_NOOP; }
    mixed_range_plan_free(out); *out=next; return MIXED_RANGE_READY;
}

static bool column_mask(const ObjectStore *objects,size_t column,bool insert,uint32_t *mask,char **error) {
    *mask=0;
    for (size_t i=0;i<objects->count;i++) {
        const SheetObject *o=objects->items+i;
        if ((insert && column>o->col && column<o->col+o->columns) || (!insert && column>=o->col && column<o->col+o->columns)) return fail(error,"This column intersects a pivot or chart. Choose a column outside its boundary.");
        if (insert && o->col>=column && o->col+o->columns>=16384) return fail(error,"Insert would move a pivot or chart beyond the last column.");
        if (insert ? o->col>=column : o->col>column) *mask|=UINT32_C(1)<<i;
    }
    return true;
}
static bool width_shape(const SheetView *view,size_t column,size_t *sparse,bool *changed) {
    if (view->column_count>DX_COLUMN_WIDTH_COLUMNS || (view->column_count && (!view->widths || !view->columns))) return false;
    *sparse=0; *changed=false;
    for (size_t i=0;i<view->column_count;i++) {
        uint32_t bits=column_width_bits(view->widths[i]);
        if (!view->columns[i] || bits<column_width_bits(52) || bits>column_width_bits(1200)) return false;
        if (bits!=DX_COLUMN_WIDTH_DEFAULT_BITS) { (*sparse)++; *changed|=i>=column; }
    }
    return true;
}
MixedRangeStatus mixed_column_measure(const Worksheet *s,const Analysis *primary,size_t column,bool insert,MixedRangePlanInfo *out,char **error) {
    if (!out || !s || !primary || column>=16384) return MIXED_RANGE_INVALID;
    if (!eligible(s,primary)) { fail(error,"Column composites require source-free templates and manual formats."); return MIXED_RANGE_PROTECTED; }
    if (!s->id || s->formats.count>DX_FORMAT_PATCHES || (s->formats.count && !s->formats.items)) return MIXED_RANGE_INVALID;
    ObjectSnapshot before={s->objects.items,s->objects.count}; ObjectsAssetInfo objects={0};
    if (objects_asset_measure(s->id,&before,s->objects.next_id,&objects)!=OBJECTS_ASSET_OK) return MIXED_RANGE_INVALID;
    CellRangePlanInfo cells={0}; CellRangeStatus status=cells_columns_measure(&s->cells,column,1,insert,&cells,error);
    if (status!=CELL_RANGE_READY && status!=CELL_RANGE_NOOP) return status==CELL_RANGE_LIMIT ? MIXED_RANGE_LIMIT : MIXED_RANGE_INVALID;
    uint32_t mask=0; size_t sparse=0; bool widths=false;
    if (!geometry(&s->objects,&s->cells,error) || !column_mask(&s->objects,column,insert,&mask,error) || !width_shape(&s->view,column,&sparse,&widths)) return MIXED_RANGE_INVALID;
    bool relevant=false;
    for (size_t i=0;i<s->formats.count;i++) { FormatRange r=s->formats.items[i].range; relevant|=r.c1>=column && (r.c0 || r.c1!=16383 || r.r0 || r.r1!=DX_SHEET_ROWS-1); }
    if (status==CELL_RANGE_NOOP && !mask && !relevant && !widths) return MIXED_RANGE_NOOP;
    size_t object_bytes=objects_snapshot_owned_bytes(&before); if (object_bytes==SIZE_MAX) return MIXED_RANGE_INVALID;
    if (status==CELL_RANGE_NOOP) cells.owned=s->cells.count*sizeof(SheetCell)+s->cells.bytes;
    size_t scratch=objects.measure.validation_scratch>DX_FORMAT_VALIDATION_BYTES ? objects.measure.validation_scratch : DX_FORMAT_VALIDATION_BYTES;
    *out=(MixedRangePlanInfo){cells.owned+object_bytes+2u*DX_FORMAT_CURRENT_BYTES+2*sparse*sizeof(ColumnWidthEntry),scratch}; return MIXED_RANGE_READY;
}
MixedRangeStatus mixed_column_plan(const Worksheet *s,const Analysis *primary,size_t column,bool insert,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error) {
    if (!out) return MIXED_RANGE_INVALID; MixedRangePlanInfo info={0};
    MixedRangeStatus status=mixed_column_measure(s,primary,column,insert,&info,error); if (status!=MIXED_RANGE_READY) return status;
    if (info.owned>reserved || info.validation_scratch>validation_reserved) { fail(error,"Column composite ownership or grammar exceeds its admitted reservation."); return MIXED_RANGE_LIMIT; }
    if (!objects_valid(&s->objects,error) || !format_valid(&s->formats,error)) return MIXED_RANGE_INVALID;
    MixedRangePlan next={.object_high_water=s->objects.next_id}; ObjectStore objects={0}; ColumnWidthState before_widths={0};
    if (column_width_capture(s->view.widths,s->view.column_count,info.owned,&before_widths)!=COLUMN_WIDTH_OK) return MIXED_RANGE_INVALID;
    ColumnWidthStatus shifted=column_width_shift(column_width_view(&before_widths),column,insert,info.owned,&next.widths);
    if (shifted==COLUMN_WIDTH_NOOP) shifted=column_width_copy(column_width_view(&before_widths),info.owned,&next.widths);
    bool width_changed=!column_width_equal(column_width_view(&before_widths),column_width_view(&next.widths)); column_width_free(&before_widths);
    if (shifted!=COLUMN_WIDTH_OK) { mixed_range_plan_free(&next); return MIXED_RANGE_INVALID; }
    CellRangeStatus cells=cells_columns_plan(&s->cells,column,1,insert,info.owned,&next.cells,error);
    if (cells!=CELL_RANGE_READY && cells!=CELL_RANGE_NOOP) { mixed_range_plan_free(&next); return MIXED_RANGE_INVALID; }
    if (cells==CELL_RANGE_NOOP && s->cells.count) {
        next.cells=(CellSnapshot){.count=s->cells.count,.bytes=s->cells.bytes}; next.cells.cells=dx_alloc(s->cells.count*sizeof(SheetCell));
        for (size_t i=0;i<s->cells.count;i++) { next.cells.cells[i]=s->cells.cells[i]; next.cells.cells[i].text=dx_strdup(s->cells.cells[i].text); }
    }
    if (s->objects.count) { objects_copy(&objects,&s->objects); next.objects=(ObjectSnapshot){objects.items,objects.count}; objects.items=NULL; objects.count=0; objects_free(&objects); }
    uint32_t mask=0;
    if (!column_mask(&s->objects,column,insert,&mask,error)) { mixed_range_plan_free(&next); return MIXED_RANGE_INVALID; }
    for (size_t i=0;i<next.objects.count;i++) if (mask&(UINT32_C(1)<<i)) next.objects.items[i].col=insert ? next.objects.items[i].col+1 : next.objects.items[i].col-1;
    if (!format_plan_axis(&s->formats,false,column,1,insert,&next.formats,error)) { mixed_range_plan_free(&next); return MIXED_RANGE_LIMIT; }
    ObjectStore projected={.items=next.objects.items,.count=next.objects.count,.next_id=s->objects.next_id}; CellStore projected_cells={.cells=next.cells.cells,.count=next.cells.count,.capacity=next.cells.count,.bytes=next.cells.bytes};
    if (!objects_valid(&projected,error) || !geometry(&projected,&projected_cells,error)) { mixed_range_plan_free(&next); return MIXED_RANGE_INVALID; }
    next.effects=(cells==CELL_RANGE_READY ? 2u : 0u)|(mask ? 4u : 0u)|(!format_snapshot_matches(&s->formats,&next.formats) ? 8u : 0u)|(width_changed ? 16u : 0u);
    if (!next.effects) { mixed_range_plan_free(&next); return MIXED_RANGE_NOOP; }
    mixed_range_plan_free(out); *out=next; return MIXED_RANGE_READY;
}

static bool paste_shape_field(void *context,uint64_t row,size_t column,const char *text,size_t bytes,char **error) {
    (void)context; (void)row; (void)column; (void)text; (void)bytes; (void)error; return true;
}
MixedRangeStatus mixed_paste_measure(const Worksheet *s,const Analysis *primary,uint64_t row,size_t column,const char *text,MixedPastePlanInfo *out,char **error) {
    if (!out || !s || !primary) return MIXED_RANGE_INVALID;
    if (!eligible(s,primary)) { fail(error,"Paste requires source-free templates and manual formats."); return MIXED_RANGE_PROTECTED; }
    ObjectSnapshot before={s->objects.items,s->objects.count}; ObjectsAssetInfo objects={0};
    if (!s->id || objects_asset_measure(s->id,&before,s->objects.next_id,&objects)!=OBJECTS_ASSET_OK) return MIXED_RANGE_INVALID;
    size_t rows=0,columns=0;
    if (!cells_tsv_scan(row,column,text,paste_shape_field,NULL,&rows,&columns,error)) return MIXED_RANGE_INVALID;
    if (!geometry(&s->objects,&s->cells,error)) return MIXED_RANGE_INVALID;
    if (!objects_fits(&s->objects,-1,row,column,rows,columns,NULL,0,0,error)) return MIXED_RANGE_PROTECTED;
    CellPastePlanInfo cells={0}; CellRangeStatus status=cells_paste_measure(&s->cells,row,column,text,&cells,error);
    if (status!=CELL_RANGE_READY) return status==CELL_RANGE_NOOP ? MIXED_RANGE_NOOP : status==CELL_RANGE_LIMIT ? MIXED_RANGE_LIMIT : MIXED_RANGE_INVALID;
    *out=(MixedPastePlanInfo){cells,objects.measure.validation_scratch}; return MIXED_RANGE_READY;
}
MixedRangeStatus mixed_paste_plan(const Worksheet *s,const Analysis *primary,uint64_t row,size_t column,const char *text,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error) {
    if (!out || (s && out->cells && out->cells==s->cells.cells)) { fail(error,"Choose independent paste cell ownership."); return MIXED_RANGE_INVALID; }
    MixedPastePlanInfo info={0}; MixedRangeStatus status=mixed_paste_measure(s,primary,row,column,text,&info,error);
    if (status!=MIXED_RANGE_READY) return status;
    if (info.cells.owned>reserved || info.validation_scratch>validation_reserved) { fail(error,"Paste exceeds its complete cell/validation reservation."); return MIXED_RANGE_LIMIT; }
    if (!objects_valid(&s->objects,error)) return MIXED_RANGE_INVALID;
    CellSnapshot next={0}; CellRangeStatus cells=cells_paste_plan(&s->cells,row,column,text,reserved,&next,error);
    if (cells!=CELL_RANGE_READY) return cells==CELL_RANGE_LIMIT ? MIXED_RANGE_LIMIT : MIXED_RANGE_INVALID;
    CellStore projected={.cells=next.cells,.count=next.count,.capacity=next.count,.bytes=next.bytes};
    if (!geometry(&s->objects,&projected,error)) { cells_snapshot_free(&next); return MIXED_RANGE_INVALID; }
    cells_snapshot_free(out); *out=next; return MIXED_RANGE_READY;
}

MixedRangeStatus mixed_clear_measure(const Worksheet *s,const Analysis *primary,SheetRange r,MixedClearPlanInfo *out,char **error) {
    if (!out || !s || !primary) return MIXED_RANGE_INVALID;
    if (!eligible(s,primary)) { fail(error,"Clear requires source-free templates and manual formats."); return MIXED_RANGE_PROTECTED; }
    ObjectSnapshot before={s->objects.items,s->objects.count}; ObjectsAssetInfo objects={0};
    if (!s->id || objects_asset_measure(s->id,&before,s->objects.next_id,&objects)!=OBJECTS_ASSET_OK) return MIXED_RANGE_INVALID;
    CellRangePlanInfo cells={0}; CellRangeStatus status=cells_clear_measure(&s->cells,r.r0,r.r1,r.c0,r.c1,&cells,error);
    if (status!=CELL_RANGE_READY && status!=CELL_RANGE_NOOP) return status==CELL_RANGE_LIMIT ? MIXED_RANGE_LIMIT : MIXED_RANGE_INVALID;
    if (!geometry(&s->objects,&s->cells,error)) return MIXED_RANGE_INVALID;
    if (status==CELL_RANGE_NOOP) return MIXED_RANGE_NOOP;
    *out=(MixedClearPlanInfo){cells,objects.measure.validation_scratch}; return MIXED_RANGE_READY;
}
MixedRangeStatus mixed_clear_plan(const Worksheet *s,const Analysis *primary,SheetRange r,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error) {
    if (!out || (s && out->cells && out->cells==s->cells.cells)) { fail(error,"Choose independent clear cell ownership."); return MIXED_RANGE_INVALID; }
    MixedClearPlanInfo info={0}; MixedRangeStatus status=mixed_clear_measure(s,primary,r,&info,error);
    if (status!=MIXED_RANGE_READY) return status;
    if (info.cells.owned>reserved || info.validation_scratch>validation_reserved) { fail(error,"Clear exceeds its complete cell/validation reservation."); return MIXED_RANGE_LIMIT; }
    if (!objects_valid(&s->objects,error)) return MIXED_RANGE_INVALID;
    CellSnapshot next={0}; CellRangeStatus cells=cells_clear_plan(&s->cells,r.r0,r.r1,r.c0,r.c1,reserved,&next,error);
    if (cells!=CELL_RANGE_READY) return cells==CELL_RANGE_LIMIT ? MIXED_RANGE_LIMIT : MIXED_RANGE_INVALID;
    cells_snapshot_free(out); *out=next; return MIXED_RANGE_READY;
}
