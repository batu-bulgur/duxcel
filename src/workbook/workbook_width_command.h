#ifndef DUXCEL_WORKBOOK_WIDTH_COMMAND_H
#define DUXCEL_WORKBOOK_WIDTH_COMMAND_H
#include "column_width_command.h"
#include "history_range.h"
typedef struct {
    Workbook *book;
    const uint64_t *request_epoch;
    size_t reserved;
    bool ready,read_only;
    /* BEFORE allocation; existing journals are in workbook_format_usage. */
    bool (*admit)(void *context,size_t staging,size_t retained_add,char **error);
    void *admission_context;
} WorkbookWidthContext;
typedef struct {
    WorkbookId workbook;
    uint64_t document,sheet,sheet_id,document_revision,width_revision,request,history_last,generation,branch,order;
    const ColumnWidthHistory *history;
    const float *widths; char *const *columns; size_t count;
    float values[DX_COLUMN_WIDTH_COLUMNS];
} WorkbookWidthTarget;
typedef struct { ColumnWidthStatus status; unsigned effects; uint64_t before_revision,after_revision; } WorkbookWidthResult;
/* Presentation widths apply to manual/source/Table sheets, independently of
   source-cell editing policy. Preparation assigns transient sessions only. */
bool workbook_prepare_width_command(WorkbookWidthContext *context,size_t sheet,WorkbookWidthTarget *out,char **error);
WorkbookWidthResult workbook_apply_width_command_recorded(WorkbookWidthContext *context,const WorkbookWidthTarget *target,
    ColumnWidthView after,const WorkbookHistoryRangeRecorder *recorder,char **error);
#endif
