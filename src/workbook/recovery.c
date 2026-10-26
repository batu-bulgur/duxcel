#include "recovery.h"
#include "archive_read.h"
#include "journal.h"
#include "../platform/platform.h"
#include "../platform/file_lease.h"
#include "../platform/directory.h"
#include "../data/source_digest.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
struct WorkbookRecoveryCandidate {
    Workbook *book;
    WorkbookVersionCoreSet *versions;
    char *version_parent;
    WorkbookRecoveryInfo info;
    DxFileLease *lease;
    char *directory,*checkpoint,*journal,*original;
    SourceFileIdentity artifacts[5]; bool present[5],retirement_valid,retired;
};
static const char *const artifact_names[]={"checkpoint.dxls","journal.duckdb","lease.lock","checkpoint.dxls.wal","journal.duckdb.wal"};
typedef struct {
    WorkbookRecoveryControl control;
    WorkbookRecoveryCandidate *candidate;
    uint64_t total;
    bool cancelled;
} Recovery;
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool progress(Recovery *r,WorkbookRecoveryPhase phase,uint64_t done,uint64_t total) {
    if (r->control.update && !r->control.update(r->control.context,(WorkbookRecoveryProgress){phase,done,total})) r->cancelled=true;
    return !r->cancelled;
}
static bool digest_cancel(void *context,uint64_t bytes) {
    Recovery *r=context; return !progress(r,RECOVERY_VERIFY,bytes,r->total);
}
static bool archive_cancel(void *context,WorkbookArchiveReadPhase phase,size_t root,uint64_t bytes) {
    (void)phase; (void)root; Recovery *r=context;
    return !progress(r,RECOVERY_LOAD,bytes,0);
}
static bool generation_name(const char *name) {
    if (!name || strlen(name)!=40 || strncmp(name,"session-",8)) return false;
    for (size_t i=8;i<40;i++) if (!((name[i]>='0' && name[i]<='9') || (name[i]>='a' && name[i]<='f'))) return false;
    return true;
}
static bool replay(void *context,const JournalCellEdit *edit,char **error) {
    Recovery *r=context; Workbook *b=r->candidate->book;
    if (edit->previous_revision!=b->command_revision) return fail(error,"The recovery tail is not contiguous with its checkpoint.");
    size_t sheet=0; while (sheet<b->count && b->sheets[sheet].id!=edit->sheet) sheet++;
    if (sheet==b->count) return fail(error,"The recovery tail references a missing worksheet.");
    const char *before=cells_get(&b->sheets[sheet].cells,edit->row,edit->column);
    if (strlen(before)!=edit->before_bytes || (edit->before_bytes && memcmp(before,edit->before,edit->before_bytes))) return fail(error,"The recovery tail does not match the checkpoint cell values.");
    Worksheet *target=b->sheets+sheet;
    if (target->cells.revision==UINT64_MAX || !objects_fits(&target->objects,-1,edit->row,edit->column,1,1,NULL,0,0,error)) return fail(error,"The recovery cell targets a protected or exhausted store.");
    char *after=dx_alloc(edit->after_bytes+1); if (edit->after_bytes) memcpy(after,edit->after,edit->after_bytes); after[edit->after_bytes]=0;
    bool applied=cells_set(&target->cells,edit->row,edit->column,after,error); free(after);
    if (!applied) return false;
    /* State projection is not a new user command. Keep worker preparation out of
       the process-wide draft/history issuers; durable Undo remains W12. */
    cells_trim_history(&target->cells,false,0); cells_trim_history(&target->cells,true,0);
    b->command_revision=edit->revision; b->dirty=true;
    return progress(r,RECOVERY_REPLAY,b->command_revision-r->candidate->info.baseline_revision,r->total);
}
const Workbook *workbook_recovery_book(const WorkbookRecoveryCandidate *c) { return c ? c->book : NULL; }
const WorkbookRecoveryInfo *workbook_recovery_info(const WorkbookRecoveryCandidate *c) { return c ? &c->info : NULL; }
Workbook *workbook_recovery_take_book(WorkbookRecoveryCandidate *c) { if (!c) return NULL; Workbook *b=c->book; c->book=NULL; return b; }
const WorkbookVersionCoreSet *workbook_recovery_versions(const WorkbookRecoveryCandidate *c) { return c ? c->versions : NULL; }
WorkbookVersionCoreSet *workbook_recovery_take_versions(WorkbookRecoveryCandidate *c) { if (!c) return NULL; WorkbookVersionCoreSet *set=c->versions; c->versions=NULL; return set; }
void workbook_recovery_free(WorkbookRecoveryCandidate *c) {
    if (!c) return;
    if (c->book) { workbook_free(c->book); free(c->book); }
    workbook_version_core_set_free(c->versions);
    if (c->version_parent) dx_remove_temp_directory(c->version_parent);
    free(c->version_parent);
    dx_file_lease_release(c->lease); free(c->directory); free(c->checkpoint); free(c->journal); free(c->original); free(c);
}
static bool capture_artifacts(WorkbookRecoveryCandidate *c,bool verify) {
    for (size_t i=0;i<5;i++) {
        char *path=dx_printf("%s/%s",c->directory,artifact_names[i]); DxPathKind kind=dx_path_kind(path); bool present=kind!=DX_PATH_MISSING; SourceFileIdentity identity={0}; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
        bool valid=!present || (kind==DX_PATH_FILE && source_file_identity(path,&identity,detail)==SOURCE_DIGEST_OK && identity.links==1);
        free(path);
        if (!valid || (verify && (present!=c->present[i] || (present && !source_file_identity_same(&identity,c->artifacts+i))))) return false;
        if (!verify) { c->present[i]=present; c->artifacts[i]=identity; }
    }
    return c->present[0] && c->present[1] && c->present[2] && !c->present[3];
}
bool workbook_recovery_retire(WorkbookRecoveryCandidate *c,char **error) {
    if (c && c->retired) return true;
    if (!c || !c->lease || !c->info.bound || c->info.tail_deferred || c->info.tail_unknown || !c->retirement_valid) return fail(error,"Deferred, unknown or unbound recovery material must be kept.");
    if (!capture_artifacts(c,true)) return fail(error,"Recovery artifacts changed after preparation; they were kept.");
    DxDirectory *directory=NULL;
    if (dx_directory_open(c->directory,&directory,error)!=DX_DIRECTORY_OPEN) return false;
    DxDirectoryEntry entry={0}; DxDirectoryNext next; bool known=true;
    while ((next=dx_directory_next(directory,&entry,error))==DX_DIRECTORY_ENTRY) {
        bool match=false; for (size_t i=0;i<5;i++) if (!strcmp(entry.name,artifact_names[i])) match=true;
        if (!match) { known=false; break; }
    }
    dx_directory_close(directory);
    if (!known || next==DX_DIRECTORY_FAILED) return fail(error,"Additional or unreadable recovery files were kept; no cleanup was performed.");
    /* The lease remains held through fixed-file deletion, including its inode.
       A second scanner cannot claim a live generation during retirement. */
    for (size_t i=0;i<5;i++) if (c->present[i]) {
        char *path=dx_printf("%s/%s",c->directory,artifact_names[i]); bool removed=remove(path)==0; free(path);
        if (!removed) return fail(error,"Some recovery files could not be removed. The named Save result is unchanged.");
    }
    if (remove(c->directory)) return fail(error,"The empty recovery folder could not be removed. The named Save result is unchanged.");
    c->retired=true; return true;
}
WorkbookRecoveryResult workbook_recovery_prepare(const char *root,const char *generation,WorkbookRecoveryMode mode,WorkbookRecoveryControl control,WorkbookRecoveryCandidate **out,char **error) {
    char *ignored_error=NULL; if (!error) error=&ignored_error;
    if (!root || !*root || !generation_name(generation) || !out || *out || (unsigned)mode>RECOVERY_OPEN_CHECKPOINT) { fail(error,"Choose a private recovery generation and an empty candidate."); free(ignored_error); return RECOVERY_FAILED; }
    WorkbookRecoveryCandidate *c=dx_alloc(sizeof(*c)); Recovery r={.control=control,.candidate=c}; WorkbookJournal *journal=NULL; WorkbookRecoveryResult result=RECOVERY_FAILED;
    char *resolved=dx_absolute_path(root); if (!resolved) { fail(error,"The recovery folder is unavailable."); goto done; }
    c->version_parent=dx_printf("%s-versions",resolved);
    char *requested=dx_printf("%s/%s",resolved,generation); free(resolved); c->directory=dx_absolute_path(requested);
    bool direct=c->directory && !strcmp(c->directory,requested); free(requested);
    if (!direct) { fail(error,"Recovery requires a direct private generation, without directory aliases."); goto done; }
    c->checkpoint=dx_printf("%s/checkpoint.dxls",c->directory); c->journal=dx_printf("%s/journal.duckdb",c->directory);
    if (!progress(&r,RECOVERY_CLAIM,0,1)) goto done;
    char *lease=dx_printf("%s/lease.lock",c->directory); DxFileLeaseStatus claim=dx_file_lease_acquire(lease,false,&c->lease,error); free(lease);
    if (claim!=DX_FILE_LEASE_ACQUIRED) { if (claim==DX_FILE_LEASE_BUSY) { result=RECOVERY_BUSY; fail(error,"This recovery generation is currently in use."); } goto done; }
    char *inspection_error=NULL; SourceFileIdentity journal_identity={0}; char journal_detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    bool regular=source_file_identity(c->journal,&journal_identity,journal_detail)==SOURCE_DIGEST_OK && journal_identity.links==1;
    char *journal_wal=dx_printf("%s.wal",c->journal); DxPathKind wal_kind=dx_path_kind(journal_wal);
    if (wal_kind!=DX_PATH_MISSING) {
        SourceFileIdentity wal_identity={0}; regular=regular && wal_kind==DX_PATH_FILE && source_file_identity(journal_wal,&wal_identity,journal_detail)==SOURCE_DIGEST_OK && wal_identity.links==1;
    }
    free(journal_wal);
    if (!regular && mode==RECOVERY_REPLAY_TAIL) { result=RECOVERY_UNBOUND; fail(error,*journal_detail ? journal_detail : "The recovery journal is not an independent regular file."); goto done; }
    bool inspected=regular && workbook_journal_inspect(&journal,c->journal,&inspection_error);
    JournalSummary summary=workbook_journal_summary(journal); const JournalOrigin *origin=summary.origin;
    if ((!inspected || !origin) && mode==RECOVERY_REPLAY_TAIL) {
        result=RECOVERY_UNBOUND; fail(error,inspection_error ? inspection_error : "This recovery tail has no verified checkpoint origin. Inspect its checkpoint instead."); free(inspection_error); goto done;
    }
    free(inspection_error);
    c->info.bound=origin!=NULL; c->info.tail_unknown=!inspected; c->info.document=summary.document;
    c->info.baseline_revision=summary.base_revision; c->info.head_revision=summary.declared_head_revision;
    c->original=dx_strdup(origin ? origin->original_path : "");
    if (origin) { c->info.generation=origin->generation; c->info.parent=origin->parent; r.total=origin->baseline_bytes; }
    char *wal=dx_printf("%s.wal",c->checkpoint); DxPathKind baseline_wal=dx_path_kind(wal); free(wal);
    if (baseline_wal!=DX_PATH_MISSING) { fail(error,"The recovery checkpoint has an unfinished or inaccessible WAL; its closed baseline cannot be verified."); goto done; }
    SourceDigest digest={0}; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; SourceFileIdentity observed={0};
    if (source_file_identity(c->checkpoint,&observed,detail)!=SOURCE_DIGEST_OK || observed.links!=1) { fail(error,*detail ? detail : "Cannot inspect the recovery checkpoint."); goto done; }
    if (origin && observed.size!=origin->baseline_bytes) { fail(error,"The recovery checkpoint size does not match its journal origin."); goto done; }
    r.total=observed.size;
    SourceDigestStatus hashed=source_digest_file(c->checkpoint,digest_cancel,&r,&digest,detail);
    if (hashed!=SOURCE_DIGEST_OK) { if (!r.cancelled) fail(error,*detail ? detail : "Cannot verify the recovery checkpoint."); goto done; }
    if (origin && memcmp(digest.sha256,origin->baseline_sha256,DX_JOURNAL_DIGEST_BYTES)) { fail(error,"The recovery checkpoint digest does not match its journal origin."); goto done; }
    if (!progress(&r,RECOVERY_LOAD,0,1)) goto done;
    if (!dx_make_directories(c->version_parent)) { fail(error,"Cannot create private staging for recovery versions."); goto done; }
    WorkbookArchiveCandidate *archive=NULL;
    if (workbook_archive_read(c->checkpoint,c->version_parent,UINT64_C(8)*1024*1024*1024,
        (WorkbookArchiveReadControl){.cancelled=archive_cancel,.context=&r},&archive,error)!=ARCHIVE_READ_READY) goto done;
    c->book=workbook_archive_take_book(archive); c->versions=workbook_archive_take_versions(archive);
    workbook_archive_candidate_free(archive);
    SourceFileIdentity after={0};
    if (source_file_identity(c->checkpoint,&after,detail)!=SOURCE_DIGEST_OK || !source_file_identity_same(&digest.identity,&after)) { fail(error,"The recovery checkpoint changed while it was loading."); goto done; }
    if (inspected && (c->book->id.high!=summary.document.high || c->book->id.low!=summary.document.low || c->book->command_revision!=summary.base_revision)) { fail(error,"The recovery document identity/revision does not match its journal."); goto done; }
    if (!progress(&r,RECOVERY_LOAD,1,1)) goto done;
    c->info.document=c->book->id; c->info.baseline_revision=c->book->command_revision;
    if (!inspected) c->info.head_revision=c->info.baseline_revision;
    workbook_journal_close(journal); journal=NULL; origin=NULL;
    if (mode==RECOVERY_REPLAY_TAIL) {
        if (!workbook_journal_open_readonly(&journal,c->journal,c->book->id,c->book->command_revision,error)) goto done;
        const JournalOrigin *matched=workbook_journal_origin(journal); SourceFileIdentity journal_after={0};
        if (!matched || matched->generation.high!=c->info.generation.high || matched->generation.low!=c->info.generation.low || matched->parent.high!=c->info.parent.high || matched->parent.low!=c->info.parent.low ||
            matched->baseline_bytes!=digest.bytes || memcmp(matched->baseline_sha256,digest.sha256,DX_JOURNAL_DIGEST_BYTES) || strcmp(matched->original_path,c->original) ||
            workbook_journal_durable_revision(journal)!=c->info.head_revision || source_file_identity(c->journal,&journal_after,detail)!=SOURCE_DIGEST_OK || !source_file_identity_same(&journal_identity,&journal_after)) {
            fail(error,"The recovery journal origin changed while preparing its candidate."); goto done;
        }
        r.total=c->info.head_revision-c->info.baseline_revision;
        if (!progress(&r,RECOVERY_REPLAY,0,r.total)) goto done;
        while (c->book->command_revision<c->info.head_revision) {
            size_t visited=0;
            if (!workbook_journal_read(journal,c->book->command_revision,DX_JOURNAL_READ_BATCH,replay,&r,&visited,error)) goto done;
            if (!visited) { fail(error,"The recovery tail ended before its declared head."); goto done; }
        }
    } else c->info.tail_deferred=c->info.tail_unknown || c->info.head_revision>c->info.baseline_revision;
    free(c->book->path); c->book->path=NULL; c->book->dirty=true;
    c->info.directory=c->directory; c->info.original_path=c->original;
    workbook_journal_close(journal); journal=NULL;
    c->retirement_valid=c->info.bound && !c->info.tail_deferred && !c->info.tail_unknown && capture_artifacts(c,false);
    if (!progress(&r,RECOVERY_READY,1,1)) goto done;
    *out=c; c=NULL; result=RECOVERY_PREPARED;
done:
    workbook_journal_close(journal); workbook_recovery_free(c);
    if (r.cancelled) { result=RECOVERY_CANCELLED; fail(error,"Recovery cancelled. The workbook and recovery files were preserved."); }
    free(ignored_error); return result;
}
