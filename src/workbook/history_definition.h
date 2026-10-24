#ifndef DUXCEL_HISTORY_DEFINITION_H
#define DUXCEL_HISTORY_DEFINITION_H
#include "history_stream.h"
#include "definition_event.h"
#define DX_HISTORY_DEFINITION_FAMILY 3u
typedef struct {
    WorkbookId workbook;
    uint64_t edit_session,sheet_session,expected_revision,revision;
    DefinitionEventView view;
    size_t validation_reserved; /* Scratch after outer model/job/stream accounting. */
} WorkbookHistoryDefinitionRecord;
typedef struct {
    bool (*reserve)(void *context,const WorkbookHistoryDefinitionRecord *record,void **ticket,char **error);
    void (*publish)(void *context,void *ticket);
    void (*cancel)(void *context,void *ticket);
    void *context;
} WorkbookHistoryDefinitionRecorder;
/* Typed complete-compound reservation over the SAME FIFO/clocks as cells/formats.
   The producer proves complete runtime/sheet/source/request/model fences, owns
   immutable after-plans, detects no-op before calling reserve and budgets grammar
   scratch separately. Recheck before coherent model/action/dirty/clock adoption;
   publish only after ALL constituents commit, otherwise cancel exactly once.
   Reserve encodes directly in the admitted event allocation. Queue pressure and
   invalid/unsupported/partial input consume no clocks/Redo/model changes. No IO.
   Larger explicit stream capacities require the outer App governor, not automatic
   memory growth; this adapter does not activate App history or prove durability. */
WorkbookHistoryDefinitionRecorder workbook_history_stream_definition_recorder(WorkbookHistoryStream *stream);
#endif
