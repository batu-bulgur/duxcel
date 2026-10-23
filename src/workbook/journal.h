#ifndef DUXCEL_WORKBOOK_JOURNAL_H
#define DUXCEL_WORKBOOK_JOURNAL_H
#include "workbook.h"
#define DX_JOURNAL_BATCH 256u
#define DX_JOURNAL_READ_BATCH 31u /* Worst-case text spans stay below 4 MiB/page. */
#define DX_JOURNAL_BATCH_BYTES (4u*1024u*1024u)
typedef struct WorkbookJournal WorkbookJournal;
#define DX_JOURNAL_DIGEST_BYTES 32u
typedef struct {
    WorkbookId generation,parent;
    uint64_t baseline_bytes;
    unsigned char baseline_sha256[DX_JOURNAL_DIGEST_BYTES];
    const char *original_path; /* Absolute named-workbook locator, or empty; borrowed input/handle-owned output. */
} JournalOrigin;
typedef struct {
    WorkbookId operation;
    uint64_t previous_revision,revision,sheet,row;
    size_t column;
    const char *before,*after;
    size_t before_bytes,after_bytes;
} JournalCellEdit;
/* Pure validation shared by reservation and transactional admission. */
bool workbook_journal_edit_valid(const JournalCellEdit *edit);
typedef struct { uint64_t durable_revision; size_t appended,replayed; } JournalReceipt;
typedef struct {
    /* Writer-thread callbacks, never reentrant. False cancels before COMMIT.
       after_commit is observational: durability cannot be cancelled afterwards. */
    bool (*before_commit)(void *context);
    void (*after_commit)(void *context);
    void *context;
} JournalControl;
typedef bool (*JournalVisitor)(void *context,const JournalCellEdit *borrowed,char **error);
/* Native DuckDB storage primitive for a private journal, not a workbook file.
   One owner/connection. The immutable base identifies its checkpoint lineage.
   out must point to NULL; failure leaves it unchanged. No model/UI mutation.
   Initial construction and every append are transactions. A failed first open
   removes only its own new incomplete file/WAL; existing files are preserved. Payloads admit only
   W05 sparse text/clear edits, not typed cells, paste, lifecycle or full history. */
bool workbook_journal_open(WorkbookJournal **out,const char *path,WorkbookId document,uint64_t base_revision,char **error);
/* Existing archive inspection only. Never creates, binds or appends; DuckDB
   opens with READ_ONLY. Caller must still hold the generation lease and match
   the actual baseline/origin before candidate replay. */
bool workbook_journal_open_readonly(WorkbookJournal **out,const char *path,WorkbookId document,uint64_t base_revision,char **error);
typedef struct {
    WorkbookId document;
    uint64_t base_revision,declared_head_revision;
    const JournalOrigin *origin; /* Handle-owned; NULL for v1. */
} JournalSummary;
/* Read-only metadata discovery. It validates inventory/schema/origin but does
   not scan/prove the tail, and its handle refuses tail reads. Reopen with the
   matched document/base through open_readonly before candidate replay. */
bool workbook_journal_inspect(WorkbookJournal **out,const char *path,char **error);
JournalSummary workbook_journal_summary(const WorkbookJournal *journal);
void workbook_journal_close(WorkbookJournal *journal);
/* Bind before the first edit, transactionally. Exact retries are idempotent;
   an existing origin cannot be replaced, nor inferred from an admitted clock.
   Engine failure poisons the handle; reopen and retry the same origin. An old
   unbound nonempty tail cannot be upgraded without validating its baseline. */
bool workbook_journal_bind_origin(WorkbookJournal *journal,const JournalOrigin *origin,char **error);
/* NULL means legacy/unbound. Borrowed metadata lives until close. Recovery must
   additionally verify the actual checkpoint bytes/digest before replay. */
const JournalOrigin *workbook_journal_origin(const WorkbookJournal *journal);
/* Ordered bounded batch, all new entries in one transaction. Exact operation-ID
   retries replay receipts; changed payload, revision gaps and reused revisions
   are rejected. A receipt is published only after successful COMMIT. SQL/commit
   failure poisons the handle: close/reopen and retry the SAME operation IDs to
   resolve uncertain durability. No-op/cancel/conflict preserves the prior receipt. */
bool workbook_journal_append(WorkbookJournal *journal,const JournalCellEdit *edits,size_t count,JournalControl control,JournalReceipt *receipt,char **error);
/* Reads at most min(limit,DX_JOURNAL_READ_BATCH) entries after revision
   (base..head), even when the caller requests more. Borrowed spans live
   only for one callback. The caller owns checkpoint matching and before-value
   validation/model replay. No full-dataset or full-journal materialization. */
bool workbook_journal_read(WorkbookJournal *journal,uint64_t after,size_t limit,JournalVisitor visitor,void *context,size_t *visited,char **error);
uint64_t workbook_journal_durable_revision(const WorkbookJournal *journal);
#endif
