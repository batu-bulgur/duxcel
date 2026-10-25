#include "sheet_source_view.h"
#include "definition_wire.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct SheetSourceView { SheetView view; size_t owned,fields; };
static bool add(size_t *sum,size_t bytes) { if (*sum>SIZE_MAX-bytes) return false; *sum+=bytes; return true; }
static bool navigation(SheetView v) {
    return !v.columns && !v.widths && !v.column_count && v.row<DX_SHEET_ROWS && v.anchor_row<DX_SHEET_ROWS && v.end_row<DX_SHEET_ROWS && v.top<DX_SHEET_ROWS &&
        v.col<16384 && v.anchor_col<16384 && v.end_col<16384 && v.selection<=3 && isfinite(v.horizontal) && v.horizontal>=0 &&
        isfinite(v.fraction) && v.fraction>=0 && v.fraction<1 && isfinite(v.canvas_zoom) && v.canvas_zoom>=.5f && v.canvas_zoom<=2.5f;
}
static float width(SheetSourceViewSpec spec,size_t column) { return column>=spec.count ? 96 : spec.columns[column].type==DUCKDB_TYPE_TIMESTAMP ? 190 : 128; }
SheetSourceViewStatus sheet_source_view_measure(SheetSourceViewSpec spec,SheetSourceViewInfo *out) {
    if (!out || !spec.columns || !spec.count || spec.count>16384 || !navigation(spec.navigation)) return SHEET_SOURCE_VIEW_INVALID;
    size_t columns=spec.count>16384-8 ? 16384 : spec.count+8; if (columns<52) columns=52;
    size_t owned=sizeof(SheetSourceView)+columns*(sizeof(char *)+sizeof(float))+(columns-spec.count);
    for (size_t i=0;i<spec.count;i++) {
        DefinitionWireMeasure text={0}; size_t length=0;
        if (!spec.columns[i].name || !definition_wire_text_measure(spec.columns[i].name,&text,65537,&length)) return SHEET_SOURCE_VIEW_INVALID;
        if (!add(&owned,length+1) || owned>DX_SHEET_PRESENCE_OWNED_BYTES) return SHEET_SOURCE_VIEW_LIMIT;
    }
    *out=(SheetSourceViewInfo){owned,columns}; return SHEET_SOURCE_VIEW_OK;
}
SheetSourceViewStatus sheet_source_view_build(SheetSourceViewSpec spec,size_t reserved,SheetSourceView **out) {
    if (!out || *out) return SHEET_SOURCE_VIEW_INVALID;
    SheetSourceViewInfo info; SheetSourceViewStatus status=sheet_source_view_measure(spec,&info); if (status) return status;
    if (reserved<info.owned) return SHEET_SOURCE_VIEW_LIMIT;
    SheetSourceView *owned=dx_alloc(sizeof(*owned)); *owned=(SheetSourceView){.view=spec.navigation,.owned=info.owned,.fields=spec.count};
    owned->view.column_count=info.columns; owned->view.columns=dx_alloc(info.columns*sizeof(char *)); owned->view.widths=dx_alloc(info.columns*sizeof(float));
    for (size_t i=0;i<info.columns;i++) { owned->view.columns[i]=dx_strdup(i<spec.count ? spec.columns[i].name : ""); owned->view.widths[i]=width(spec,i); }
    *out=owned; return SHEET_SOURCE_VIEW_OK;
}
const SheetView *sheet_source_view_get(const SheetSourceView *owned) { return owned ? &owned->view : NULL; }
size_t sheet_source_view_owned(const SheetSourceView *owned) { return owned ? owned->owned : 0; }
static bool same_preferences(SheetView a,SheetView b) {
    return a.row==b.row && a.col==b.col && a.anchor_row==b.anchor_row && a.anchor_col==b.anchor_col && a.end_row==b.end_row && a.end_col==b.end_col &&
        a.top==b.top && a.selection==b.selection && !memcmp(&a.horizontal,&b.horizontal,sizeof(float)) && !memcmp(&a.fraction,&b.fraction,sizeof(double)) && !memcmp(&a.canvas_zoom,&b.canvas_zoom,sizeof(float));
}
SheetSourceViewStatus sheet_source_view_matches(const SheetSourceView *owned,SheetSourceViewSpec spec,bool *out) {
    if (!owned || !out) return SHEET_SOURCE_VIEW_INVALID;
    SheetSourceViewInfo info; SheetSourceViewStatus status=sheet_source_view_measure(spec,&info); if (status) return status;
    bool same=owned->fields==spec.count && owned->view.column_count==info.columns && same_preferences(owned->view,spec.navigation);
    for (size_t i=0;same && i<info.columns;i++) same=!strcmp(owned->view.columns[i],i<spec.count ? spec.columns[i].name : "") && owned->view.widths[i]==width(spec,i);
    *out=same; return SHEET_SOURCE_VIEW_OK;
}
SheetSourceViewStatus sheet_source_view_take(SheetSourceView **owned,SheetView *out) {
    if (!owned || !*owned || !out || memcmp(out,&(SheetView){0},sizeof(*out))) return SHEET_SOURCE_VIEW_INVALID;
    *out=(*owned)->view; (*owned)->view=(SheetView){0}; sheet_source_view_free(owned); return SHEET_SOURCE_VIEW_OK;
}
void sheet_source_view_free(SheetSourceView **owned) { if (owned && *owned) { view_free(&(*owned)->view); free(*owned); *owned=NULL; } }
