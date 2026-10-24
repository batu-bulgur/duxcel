#ifndef DUXCEL_HISTORY_STREAM_H
#define DUXCEL_HISTORY_STREAM_H
#include "cell_command.h"
#include "version_index.h"
#define DX_HISTORY_STREAM_ENTRIES 256u
#define DX_HISTORY_STREAM_DEFAULT_BYTES (8u*1024u*1024u)
/* Explicit options can admit complete definition pairs; this is a hard bound,
   never preallocated memory or retained-history truncation. Outer resource
   accounting must cover resident events plus validation and prepared models. */
#define DX_HISTORY_STREAM_BYTES (32u*1024u*1024u)
#define DX_HISTORY_CELL_FAMILY 1u
typedef struct WorkbookHistoryStream WorkbookHistoryStream;
typedef struct WorkbookHistoryEvent WorkbookHistoryEvent;
typedef struct {
    WorkbookVersionOwner owner; /* Current working writer; runtime fence, not wire data. */
    uint64_t edit_session,command_revision;
    uint64_t origin,available,cursor,tail,lineage;
    uint64_t action_high_water,transition_high_water,lineage_high_water;
} WorkbookHistoryStreamOrigin;
typedef struct { size_t entries,bytes; } WorkbookHistoryStreamOptions;
typedef struct {
    WorkbookHistoryStreamOrigin current;
    size_t pending,owned_bytes;
    bool reserved,in_flight;
} WorkbookHistoryStreamState;
typedef struct {
    WorkbookVersionOwner owner;
    uint64_t edit_session,revision,action,transition,lineage;
    uint64_t previous_position,position,parent_lineage;
    bool forked;
    unsigned family,encoding;
    size_t bytes;
    unsigned dependency_encoding;
    size_t dependency_bytes,inactive_scopes;
} WorkbookHistoryEventInfo;
typedef enum {
    HISTORY_STREAM_OK,HISTORY_STREAM_INVALID,HISTORY_STREAM_READ_ONLY,
    HISTORY_STREAM_LIMIT,HISTORY_STREAM_BUSY,HISTORY_STREAM_EMPTY,HISTORY_STREAM_STALE
} WorkbookHistoryStreamStatus;
/* Bounded pre-publication staging owner, not the retained-history byte budget.
   The adapter independently proves the supplied origin/path/asset closure.
   No IO/thread/model mutation or file profile. Allocation follows dx_alloc.
   out must be NULL; failure preserves it. Zero/exhausted clocks remain readable.
   Transition IDs are separate from user-action IDs, positions and command clocks;
   typed producers admit new edits only, not Undo/Redo transitions. */
WorkbookHistoryStreamStatus workbook_history_stream_new(WorkbookHistoryStreamOrigin origin,
    WorkbookHistoryStreamOptions options,WorkbookHistoryStream **out);
WorkbookHistoryStreamState workbook_history_stream_state(const WorkbookHistoryStream *stream);
/* Borrowed producer adapter; stream outlives the whole reserve/apply/finish call.
   Exact canonical cell bytes are owned before mutation. Rejection/cancel consumes
   no durable clocks or Redo; publish alone advances the proposed current path.
   A fork retains all previously owned assets, including the abandoned suffix.
   Runtime document/edit-session/revision fences reject rebinding by coincidence.
   App activation waits for every producer and ordered publication support. */
CellEditRecorder workbook_history_stream_cell_recorder(WorkbookHistoryStream *stream);
/* Single owner dispatches one immutable event to a writer, which returns it to
   the owner for completion/retry. No stream access from the writer thread.
   Pending/in-flight/reserved memory all stays charged to one bound. */
WorkbookHistoryStreamStatus workbook_history_stream_dispatch(WorkbookHistoryStream *stream,WorkbookHistoryEvent **out);
const WorkbookHistoryEventInfo *workbook_history_event_info(const WorkbookHistoryEvent *event);
const void *workbook_history_event_payload(const WorkbookHistoryEvent *event);
/* Optional immutable source-metadata asset; encoding/length are in event info. */
const void *workbook_history_event_dependencies(const WorkbookHistoryEvent *event);
/* Completion requires the adapter's independently proved durable asset/metadata
   handoff. This releases staging memory only, not an IO receipt or GC authority.
   Retry restores FIFO ownership without releasing the byte/entry charge.
   Wrong owner/event preserves both stream and the caller's owned pointer. */
WorkbookHistoryStreamStatus workbook_history_stream_complete(WorkbookHistoryStream *stream,WorkbookHistoryEvent **owned);
WorkbookHistoryStreamStatus workbook_history_stream_retry(WorkbookHistoryStream *stream,WorkbookHistoryEvent **owned);
/* Never free an outstanding reservation/in-flight event. Pending events require
   explicit discard after the caller has preserved the accepted history/state;
   no capacity policy silently drops them. Failure preserves *stream. */
WorkbookHistoryStreamStatus workbook_history_stream_free(WorkbookHistoryStream **stream,bool discard_pending);
#endif
