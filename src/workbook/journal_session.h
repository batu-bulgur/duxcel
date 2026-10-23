#ifndef DUXCEL_WORKBOOK_JOURNAL_SESSION_H
#define DUXCEL_WORKBOOK_JOURNAL_SESSION_H
#include "journal_queue.h"
#include "cell_command.h"
typedef struct WorkbookJournalSession WorkbookJournalSession;
typedef struct {
    JournalQueueState queue;
    WorkbookId generation,parent; /* Immutable current generation lineage, even while pending. */
    bool active,covered;
    size_t checkpoint_format_bytes;
} JournalSessionState;
/* Native checkpoint-backed admission for the existing one-cell producer.
   No App/raylib pointers cross the writer boundary. Callers capture their live
   Analysis/view before admission and account other App-owned staging buffers.
   Root is copied; no IO/thread starts until a changed, valid cell command. */
WorkbookJournalSession *workbook_journal_session_new(const char *root,JournalQueueOptions options,char **error);
typedef struct { WorkbookId document,parent; const char *original_path; } JournalSessionLineage;
/* Seed a recovered branch before its first generation. Metadata is copied;
   the document must match admission, and it never changes the Save target. */
bool workbook_journal_session_lineage(WorkbookJournalSession *session,JournalSessionLineage lineage,char **error);
bool workbook_journal_session_needs_checkpoint(const WorkbookJournalSession *session,const Workbook *book);
CellEditResult workbook_journal_session_apply(WorkbookJournalSession *session,Workbook *book,const CellEditCommand *command,CellEditPolicy policy,char **error);
JournalSessionState workbook_journal_session_state(WorkbookJournalSession *session,const Workbook *book);
void workbook_journal_session_poll(WorkbookJournalSession *session);
bool workbook_journal_session_retry(WorkbookJournalSession *session,char **error);
/* Only current-session owned files may be discarded. Keep preserves committed
   checkpoint/journal files for recovery; pending failures remain a caller-owned
   Save/close concern. No startup discovery or retained Undo is implemented. */
bool workbook_journal_session_close(WorkbookJournalSession *session,bool discard,char **error);
/* Normal Open/Save must not publish or adopt a current private working file. */
bool workbook_journal_session_owns_path(const WorkbookJournalSession *session,const char *path);
const char *workbook_journal_session_checkpoint(const WorkbookJournalSession *session);
const char *workbook_journal_session_journal(const WorkbookJournalSession *session);
#endif
