/* Frame-stack borrowing only. Resolve the current owner before reading any
   captured coordinate array; no text/type layout is assumed. */
typedef struct {
    App *app; const Worksheet *sheet; const SheetCell *entries;
    WorkbookId document; uint64_t document_session,sheet_session,sheet_id,document_revision,cell_revision,request;
    size_t count,capacity;
} AppGridBoundary;
static bool app_grid_boundary_ready(App *a) {
    Worksheet *sheet=active_sheet(a);
    if (!sheet || a->analysis.kind!=SOURCE_NONE || sheet->objects.count || a->busy || a->io_busy || a->count_request_id || history_locked(a)) return false;
#ifndef DUXCEL_BROWSER_HOST
    if (a->primary_source_pending_owner || a->object_import.active) return false;
#endif
    return true;
}
static bool app_grid_boundary_read(const void *context,size_t index,GridBoundaryCell *out) {
    const AppGridBoundary *target=context; App *a=target->app; Worksheet *sheet=active_sheet(a);
    if (!app_grid_boundary_ready(a) || sheet!=target->sheet || a->book.id.high!=target->document.high || a->book.id.low!=target->document.low ||
        a->book.edit_session_id!=target->document_session || sheet->edit_session_id!=target->sheet_session || sheet->id!=target->sheet_id ||
        a->book.command_revision!=target->document_revision || sheet->cells.revision!=target->cell_revision || a->request_id!=target->request ||
        sheet->cells.cells!=target->entries || sheet->cells.count!=target->count || sheet->cells.capacity!=target->capacity || index>=sheet->cells.count || !sheet->cells.cells) return false;
    const SheetCell *cell=sheet->cells.cells+index;
    *out=(GridBoundaryCell){cell->row,cell->col,true}; return true;
}
static void app_grid_boundary_end(App *a) { a->grid.manual_occupancy=(GridBoundaryOccupancy){0}; a->grid.manual_occupancy_active=false; }
static void app_grid_boundary_begin(App *a,AppGridBoundary *target) {
    app_grid_boundary_end(a); if (!app_grid_boundary_ready(a)) return;
    Worksheet *sheet=active_sheet(a);
    *target=(AppGridBoundary){.app=a,.sheet=sheet,.entries=sheet->cells.cells,.document=a->book.id,.document_session=a->book.edit_session_id,
        .sheet_session=sheet->edit_session_id,.sheet_id=sheet->id,.document_revision=a->book.command_revision,.cell_revision=sheet->cells.revision,
        .request=a->request_id,.count=sheet->cells.count,.capacity=sheet->cells.capacity};
    a->grid.manual_occupancy=(GridBoundaryOccupancy){app_grid_boundary_read,target,target->count,GRID_BOUNDARY_CELLS_MAX}; a->grid.manual_occupancy_active=true;
}
