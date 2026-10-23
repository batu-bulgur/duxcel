#include "recovery_catalog.h"
#include "journal.h"
#include "../platform/platform.h"
#include "../platform/directory.h"
#include "../platform/file_lease.h"
#include <stdlib.h>
#include <string.h>
struct WorkbookRecoveryScan { char *root; DxDirectory *directory; WorkbookRecoveryScanState state; bool failed; };
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool generation_name(const char *name) {
    if (strlen(name)!=40 || strncmp(name,"session-",8)) return false;
    for (size_t i=8;i<40;i++) if (!((name[i]>='0' && name[i]<='9') || (name[i]>='a' && name[i]<='f'))) return false;
    return true;
}
bool workbook_recovery_scan_open(const char *root,WorkbookRecoveryScan **out,char **error) {
    if (!root || !*root || !out || *out) return fail(error,"Choose a recovery root and an empty scan.");
    WorkbookRecoveryScan *scan=dx_alloc(sizeof(*scan)); DxDirectoryOpen opened=dx_directory_open(root,&scan->directory,error);
    if (opened==DX_DIRECTORY_MISSING) { scan->state.done=true; *out=scan; return true; }
    if (opened!=DX_DIRECTORY_OPEN) { free(scan); return false; }
    scan->root=dx_absolute_path(root);
    if (!scan->root) { workbook_recovery_scan_free(scan); return fail(error,"Cannot resolve the recovery root."); }
    *out=scan; return true;
}
static bool closed_baseline(const char *checkpoint) { char *path=dx_printf("%s.wal",checkpoint); bool closed=dx_path_kind(path)==DX_PATH_MISSING; free(path); return closed; }
static bool independent_wal(const char *journal) {
    char *path=dx_printf("%s.wal",journal); DxPathKind kind=dx_path_kind(path);
    bool valid=kind==DX_PATH_MISSING || (kind==DX_PATH_FILE && dx_path_independent_file(path)); free(path); return valid;
}
static bool offer(WorkbookRecoveryScan *scan,const char *name,WorkbookRecoveryScanControl control) {
    WorkbookRecoveryEntry entry={.generation_name=name,.original_path="",.detail=""};
    char *directory=dx_printf("%s/%s",scan->root,name),*lease_path=dx_printf("%s/lease.lock",directory),*checkpoint=dx_printf("%s/checkpoint.dxls",directory),*journal_path=dx_printf("%s/journal.duckdb",directory);
    DxFileLease *lease=NULL; WorkbookJournal *journal=NULL; char *error=NULL;
    char *canonical=dx_absolute_path(directory); bool direct=canonical && !strcmp(canonical,directory); free(canonical);
    DxPathKind lease_kind=dx_path_kind(lease_path);
    if (!direct) { entry.kind=RECOVERY_ENTRY_INVALID; entry.detail="Recovery requires a direct generation without directory aliases."; scan->state.invalid++; }
    else if (lease_kind==DX_PATH_MISSING) { entry.kind=RECOVERY_ENTRY_LEGACY; entry.detail="Older recovery folder without a generation lease; original files are kept."; scan->state.legacy++; }
    else if (lease_kind!=DX_PATH_FILE) { entry.kind=RECOVERY_ENTRY_INVALID; entry.detail="The recovery ownership file is unavailable or aliased."; scan->state.invalid++; }
    else {
        DxFileLeaseStatus claim=dx_file_lease_acquire(lease_path,false,&lease,&error);
        if (claim==DX_FILE_LEASE_BUSY) { entry.kind=RECOVERY_ENTRY_ACTIVE; entry.detail="An active Duxcel instance owns this session."; scan->state.active++; }
        else if (claim!=DX_FILE_LEASE_ACQUIRED) { entry.kind=RECOVERY_ENTRY_INVALID; entry.detail="Exclusive recovery ownership could not be established."; scan->state.invalid++; }
        else {
            DxPathKind baseline=dx_path_kind(checkpoint),tail=dx_path_kind(journal_path);
            if (baseline!=DX_PATH_FILE || !dx_path_independent_file(checkpoint)) { entry.kind=baseline==DX_PATH_MISSING ? RECOVERY_ENTRY_INCOMPLETE : RECOVERY_ENTRY_INVALID; entry.detail="An independent closed checkpoint is unavailable."; scan->state.invalid++; }
            else if (!closed_baseline(checkpoint)) { entry.kind=RECOVERY_ENTRY_INCOMPLETE; entry.detail="The checkpoint has an unfinished or inaccessible WAL."; scan->state.invalid++; }
            else if (tail==DX_PATH_FILE && dx_path_independent_file(journal_path) && independent_wal(journal_path) && workbook_journal_inspect(&journal,journal_path,&error)) {
                JournalSummary summary=workbook_journal_summary(journal); entry.document=summary.document; entry.baseline_revision=summary.base_revision; entry.declared_head_revision=summary.declared_head_revision;
                if (summary.origin) { entry.kind=RECOVERY_ENTRY_BOUND; entry.generation=summary.origin->generation; entry.parent=summary.origin->parent; entry.original_path=summary.origin->original_path; entry.detail="Checkpoint and recorded tail are ready for verification."; }
                else { entry.kind=RECOVERY_ENTRY_UNBOUND; entry.detail="Legacy tail has no baseline fingerprint; checkpoint inspection is available."; }
            } else { entry.kind=RECOVERY_ENTRY_CHECKPOINT_ONLY; entry.detail="The tail is unavailable or unverified; the checkpoint can be inspected separately."; }
        }
    }
    scan->state.offered++; bool more=!control.visit || control.visit(control.context,&entry);
    workbook_journal_close(journal); dx_file_lease_release(lease); free(error); free(directory); free(lease_path); free(checkpoint); free(journal_path); return more;
}
WorkbookRecoveryScanResult workbook_recovery_scan_step(WorkbookRecoveryScan *scan,size_t budget,WorkbookRecoveryScanControl control,char **error) {
    if (!scan || !budget || budget>128) { fail(error,"Choose an open recovery scan and a budget from 1 to 128 entries."); return RECOVERY_SCAN_FAILED; }
    if (scan->failed) { fail(error,"Start a fresh recovery scan after an enumeration failure."); return RECOVERY_SCAN_FAILED; }
    if (scan->state.done) return RECOVERY_SCAN_DONE;
    for (size_t i=0;i<budget;i++) {
        if (control.cancelled && control.cancelled(control.context)) return RECOVERY_SCAN_CANCELLED;
        DxDirectoryEntry entry={0}; DxDirectoryNext next=dx_directory_next(scan->directory,&entry,error);
        if (next==DX_DIRECTORY_END) { scan->state.done=true; return RECOVERY_SCAN_DONE; }
        if (next==DX_DIRECTORY_FAILED) { scan->failed=true; return RECOVERY_SCAN_FAILED; }
        scan->state.visited++;
        if (entry.kind==DX_PATH_DIRECTORY && generation_name(entry.name) && !offer(scan,entry.name,control)) return RECOVERY_SCAN_MORE;
    }
    return RECOVERY_SCAN_MORE;
}
WorkbookRecoveryScanState workbook_recovery_scan_state(const WorkbookRecoveryScan *scan) { return scan ? scan->state : (WorkbookRecoveryScanState){0}; }
void workbook_recovery_scan_free(WorkbookRecoveryScan *scan) { if (!scan) return; dx_directory_close(scan->directory); free(scan->root); free(scan); }
