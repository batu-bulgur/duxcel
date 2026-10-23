#ifndef DUXCEL_WORKBOOK_JOURNAL_QUEUE_H
#define DUXCEL_WORKBOOK_JOURNAL_QUEUE_H
#include "journal.h"
typedef struct WorkbookJournalQueue WorkbookJournalQueue;
typedef struct JournalReservation JournalReservation;
typedef struct {
    size_t entries,bytes,batch; /* Nonzero, at most the journal's hard limits. */
    JournalControl observer; /* Optional writer-thread observation/cancellation. */
} JournalQueueOptions;
typedef struct {
    uint64_t accepted_revision,durable_revision;
    size_t pending,owned_bytes;
    bool reserved,opening,writing,failed,stopping,stopped;
    char error[512];
} JournalQueueState;
/* Native single producer, independent writer thread. Start performs no file IO.
   expected_head must match the chosen existing journal/checkpoint lineage.
   All owned pending/in-flight/reserved payloads share one entries/bytes bound.
   This is infrastructure; callers still own checkpoint creation and admission. */
WorkbookJournalQueue *workbook_journal_queue_start(const char *path,WorkbookId document,uint64_t base,uint64_t expected_head,JournalQueueOptions options,char **error);
/* Reserve immutable bytes BEFORE model mutation. One outstanding reservation.
   Failure leaves out unchanged and does not publish an edit. Cancel on model
   rejection/no-op; publish only after success. Publish cannot fail due to a
   concurrent writer error: the already-reserved edit remains pending for retry.
   Only the producer may reserve/publish/cancel/retry/stop/free. */
bool workbook_journal_queue_reserve(WorkbookJournalQueue *queue,const JournalCellEdit *edit,JournalReservation **out,char **error);
void workbook_journal_queue_cancel(JournalReservation *reservation);
void workbook_journal_queue_publish(JournalReservation *reservation);
JournalQueueState workbook_journal_queue_state(WorkbookJournalQueue *queue);
bool workbook_journal_queue_retry(WorkbookJournalQueue *queue,char **error);
/* Joins the owned writer. Drain failure retains payloads for inspection until
   free; callers must preserve/save their live model before disposing them.
   Rejects an outstanding reservation without stopping the queue. Non-draining
   stop can commit an in-flight batch, but never acknowledges uncommitted edits.
   There is no recovery/Save guarantee from freeing a queue. */
bool workbook_journal_queue_stop(WorkbookJournalQueue *queue,bool drain,char **error);
void workbook_journal_queue_free(WorkbookJournalQueue *queue);
#endif
