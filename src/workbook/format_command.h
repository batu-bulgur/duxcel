#ifndef DUXCEL_FORMAT_COMMAND_H
#define DUXCEL_FORMAT_COMMAND_H
#include "cell_command.h"
#include "history_format.h"
typedef struct {
    CellEditTarget owner; /* Shared durable/session/cell/document fences; coordinate0. */
    uint64_t expected_format_revision;
} ManualFormatTarget;
typedef enum {
    MANUAL_FORMAT_APPLIED,MANUAL_FORMAT_NOOP,MANUAL_FORMAT_STALE,MANUAL_FORMAT_INVALID,
    MANUAL_FORMAT_PROTECTED,MANUAL_FORMAT_LIMIT,MANUAL_FORMAT_READY
} ManualFormatStatus;
typedef struct {
    ManualFormatStatus status;
    uint64_t before_revision,after_revision,before_document_revision,after_document_revision;
} ManualFormatResult;
/* First admitted format producer: a source-free, object-free worksheet with
   ordinary spatial/manual formats. Linked/source-aware/composite commands stay
   with their dedicated owner until complete constituent admission exists.
   Targets resolve stable sheet ID across reorder, with independent reopen/session,
   cell, format and admitted-document fences. Prepare assigns session IDs lazily
   through the same owner as cell commands, without data/history/dirty changes. */
bool workbook_prepare_manual_format(Workbook *book,size_t sheet,ManualFormatTarget *out,char **error);
/* Trusted live model and independent owned candidate from a format planner.
   Candidate graphs cannot alias live/current/history state. Rejection preserves
   the candidate, model, history and dirty state. No-op consumes the candidate but
   reserves no event or clock; changed success consumes it and records one action.
   Reserve precedes any mutation/eviction, then fences are rechecked; publish
   follows coherent format/action/document-clock/dirty/branch publication.
   Recorder callbacks are single-owner, nonreentrant and obey history_format.h.
   This does not compose recovery storage, activate retained App history or prove
   a durable receipt; global App IO/job transient accounting stays with the App. */
ManualFormatResult workbook_apply_manual_format_recorded(Workbook *book,const ManualFormatTarget *target,
    FormatSnapshot *owned,const WorkbookHistoryFormatRecorder *recorder,char **error);
#endif
