#ifndef DUXCEL_SHEET_SOURCE_VIEW_H
#define DUXCEL_SHEET_SOURCE_VIEW_H
#include "sheet_presence_asset.h"
#include "../data/page.h" /* Column schema only; no Row Page or renderer input. */
typedef struct SheetSourceView SheetSourceView;
typedef struct {
    const Column *columns;
    size_t count;
    SheetView navigation; /* Explicit scalar-only preferences; no owned arrays. Zoom is required. */
} SheetSourceViewSpec;
typedef struct { size_t owned,columns; } SheetSourceViewInfo;
typedef enum { SHEET_SOURCE_VIEW_OK,SHEET_SOURCE_VIEW_INVALID,SHEET_SOURCE_VIEW_LIMIT } SheetSourceViewStatus;
/* Fresh unbounded main-sheet source projection: min(16384,max(52,fields+8))
   columns, actual source names, empty virtual labels, timestamp190/other128,
   virtual tail96. No outgoing widths/cache, Row Pages, parsing, IDs or actions.
   Numeric/type inference is not interpreted beyond the existing timestamp width
   rule. Navigation/zoom is explicit and validated against current SheetView bounds.
   The owner is bounded by the current complete-presence owned limit. */
SheetSourceViewStatus sheet_source_view_measure(SheetSourceViewSpec spec,SheetSourceViewInfo *out);
/* Admit full new ownership BEFORE cloning. Failure preserves inputs/out; out
   must be empty. Source schema stays readable/immutable through this call. */
SheetSourceViewStatus sheet_source_view_build(SheetSourceViewSpec spec,size_t reserved,SheetSourceView **out);
const SheetView *sheet_source_view_get(const SheetSourceView *view);
size_t sheet_source_view_owned(const SheetSourceView *view);
/* Read-only comparison to actual typed schema and scalar preferences; no old
   borrowed Column strings are retained. This is NOT request/owner admission. */
SheetSourceViewStatus sheet_source_view_matches(const SheetSourceView *view,SheetSourceViewSpec spec,bool *out);
/* Nonallocating ownership move into a completely empty model destination.
   Plan coordinators can perform this into owned staging BEFORE real CREATE,
   then transfer that staged SheetView onto its guaranteed-empty new sheet. */
SheetSourceViewStatus sheet_source_view_take(SheetSourceView **view,SheetView *out);
void sheet_source_view_free(SheetSourceView **view);
#endif
