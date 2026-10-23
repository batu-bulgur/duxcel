#ifndef DUXCEL_CELL_COMMAND_H
#define DUXCEL_CELL_COMMAND_H
#include "workbook.h"

#define CELL_EDIT_COMMAND_VERSION 2u
typedef struct {
    uint64_t document,sheet; /* Process-lifetime fences, independent of durable IDs. */
    uint64_t expected_revision,row;
    size_t column;
    WorkbookId workbook;
    uint64_t expected_document_revision;
    uint64_t sheet_id; /* Durable target plus independent reopen/snapshot fences. */
} CellEditTarget;
typedef struct {
    unsigned version;
    CellEditTarget target;
    const char *text; size_t bytes; /* Borrowed byte span; no terminator required. */
} CellEditCommand;
typedef struct {
    uint64_t source_rows;
    size_t source_columns;
    bool source_ready; /* App resolves its independent live primary authority. */
} CellEditPolicy;
typedef enum { CELL_EDIT_APPLIED,CELL_EDIT_NOOP,CELL_EDIT_STALE,CELL_EDIT_INVALID,
    CELL_EDIT_PROTECTED,CELL_EDIT_LIMIT,CELL_EDIT_READY } CellEditStatus;
typedef struct {
    CellEditStatus status;
    uint64_t before_revision,after_revision; /* Target CellStore. */
    uint64_t before_document_revision,after_document_revision; /* Admitted commands. */
} CellEditResult;

/* Single mutation owner only (not thread safe). Assign non-recycled process-
   lifetime identities lazily, without dirtying data or entering Undo. Resolve
   before opening a draft; never prepare a replacement target at commit time.
   Durable IDs select the owner; session fences reject drafts across reopen or
   snapshots. These are not durable journal entries.
   Snapshots/new loads get independent session fences. out is unchanged on failure. */
bool workbook_prepare_cell_edit(Workbook *book,size_t sheet,uint64_t row,size_t column,CellEditTarget *out,char **error);
/* Resolved bytes, durable/session target and both expected revisions are input.
   Revalidate source/object protection before adoption. A changed value creates
   exactly one existing worksheet Undo action and marks the workbook dirty.
   No-op/stale/invalid/protected/limit results preserve data, histories and dirty
   state. Errors own optional initialized *error; successful calls leave it alone.
   The persisted document clock advances only for admitted changed C commands;
   legacy mutations still use their store fences. No IO/query, durable receipt,
   global history or idempotent retry is implied. Allocation exhaustion follows
   the existing process-fatal allocator. */
/* Pure admission check; READY still requires apply after any journal reservation. */
CellEditResult workbook_check_cell_edit(const Workbook *book,const CellEditCommand *command,CellEditPolicy policy,char **error);
CellEditResult workbook_apply_cell_edit(Workbook *book,const CellEditCommand *command,CellEditPolicy policy,char **error);
#endif
