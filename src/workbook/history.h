#ifndef DUXCEL_WORKBOOK_HISTORY_H
#define DUXCEL_WORKBOOK_HISTORY_H
#include "workbook.h"
typedef enum {
    WORKBOOK_HISTORY_READY,WORKBOOK_HISTORY_APPLIED,WORKBOOK_HISTORY_EMPTY,
    WORKBOOK_HISTORY_DELEGATE,WORKBOOK_HISTORY_INVALID,WORKBOOK_HISTORY_LIMIT
} WorkbookHistoryStatus;
typedef struct {
    WorkbookHistoryStatus status;
    size_t sheet;
    uint64_t sheet_id,order;
    EditKind kind;
    ColumnEdit column;
} WorkbookHistoryResult;
/* Session-only action ordering. One mutation owner; no wrap/recycling. Legacy
   void producers get order0 on exhaustion and the coordinator refuses replay. */
bool worksheet_history_order_available(void);
uint64_t worksheet_history_issue_order(void);
/* Trusted owned model, not an archive/pointer-ownership parser.
   Pure bounded query: global newest Undo / earliest Redo, stable sheet identity,
   exact constituent counts and retention floor. Never skips a missing interval.
   A new recorded edit makes all previous Redo unavailable across the workbook. */
WorkbookHistoryResult workbook_history_peek(const Workbook *book,bool redo,char **error);
bool workbook_history_available(const Workbook *book,bool redo);
/* Nonfallible after a successful edit; release every abandoned Redo constituent.
   No-op/rejected commands must not call this. sync handles legacy new records
   at the App frame boundary or before a direct C history transfer, retaining
   the new branch's subsequent Redo. */
void workbook_history_note_edit(Workbook *book);
void workbook_history_sync(Workbook *book);
/* Cell/format snapshot transfer plus matching action transfer, dirty publication
   and monotonic command revision. No sheet activation, IO, query or presentation
   preferences are replayed. Other kinds delegate to the existing App adapter.
   Empty/invalid/limit/delegate preserve the model; errors own optional *error. */
WorkbookHistoryResult workbook_history_step(Workbook *book,bool redo,char **error);
#endif
