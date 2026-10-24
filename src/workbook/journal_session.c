#include "journal_session.h"
#include "version_save.h"
#include "../platform/platform.h"
#include "../data/source_digest.h"
#include "../platform/file_lease.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    uint64_t id,revision,row,rows,source,source_version;
    size_t col,columns;
    ObjectKind kind;
} ObjectFence;
typedef struct {
    uint64_t id,cells,formats,last,object_high,title,marks,source_high,source_version;
    size_t source_bytes,source_count,objects;
    bool hidden;
    char name[4097];
    ObjectFence object[DX_MAX_OBJECTS];
} SheetFence;
typedef struct {
    WorkbookId id;
    uint64_t revision;
    size_t count;
    SheetFence sheet[DX_MAX_SHEETS];
} ContentFence;
typedef struct {
    Workbook *snapshot;
    WorkbookVersionCoreSet *versions;
    JournalCheckpointVersions binding;
    bool versioned;
    char *version_parent;
    char *directory,*checkpoint,*journal;
    size_t format_bytes;
    JournalOrigin origin;
    DxFileLease *lease;
    char *lease_path;
} Checkpoint;
typedef struct { WorkbookJournalQueue *queue; Checkpoint *checkpoint; } Generation;
struct WorkbookJournalSession {
    char *root,*inherited_original;
    WorkbookId inherited_document,inherited_parent;
    JournalQueueOptions options;
    WorkbookId operation;
    ContentFence expected;
    Generation active,retired;
    bool version_gap;
};
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static void sheet_fence(SheetFence *f,const Worksheet *s) {
    memset(f,0,sizeof(*f));
    f->id=s->id; f->cells=s->cells.revision; f->formats=s->formats.revision; f->last=s->history_last;
    f->object_high=s->objects.next_id; f->title=s->objects.title_revision; f->marks=s->objects.marks_revision;
    f->source_high=s->source_versions.high_water; f->source_bytes=s->source_versions.bytes; f->source_count=s->source_versions.count;
    f->source_version=s->analysis.source_version; f->objects=s->objects.count; f->hidden=s->hidden;
    snprintf(f->name,sizeof(f->name),"%s",s->name ? s->name : "");
    for (size_t i=0;i<s->objects.count && i<DX_MAX_OBJECTS;i++) {
        const SheetObject *o=s->objects.items+i; ObjectFence *v=f->object+i;
        v->id=o->id; v->revision=o->revision; v->row=o->row; v->rows=o->rows; v->source=o->source_id; v->source_version=o->source.source_version;
        v->col=o->col; v->columns=o->columns; v->kind=o->kind;
    }
}
static void capture(ContentFence *f,const Workbook *b) {
    f->id=b->id; f->revision=b->command_revision; f->count=b->count;
    for (size_t i=0;i<b->count;i++) sheet_fence(f->sheet+i,b->sheets+i);
}
static bool matches(const ContentFence *f,const Workbook *b) {
    if (!workbook_identity_valid(b) || f->count!=b->count || f->id.high!=b->id.high || f->id.low!=b->id.low || f->revision!=b->command_revision) return false;
    for (size_t i=0;i<b->count;i++) { SheetFence current; sheet_fence(&current,b->sheets+i); if (memcmp(&current,f->sheet+i,sizeof(current))) return false; }
    return true;
}
static void checkpoint_release(void *context) {
    Checkpoint *c=context;
    if (c->snapshot) { workbook_free(c->snapshot); free(c->snapshot); }
    if (c->binding.release) c->binding.release(c->binding.context,&c->versions);
    else workbook_version_core_set_free(c->versions);
    free(c->version_parent);
    dx_file_lease_release(c->lease); free(c->lease_path);
    free((void *)c->origin.original_path); free(c->directory); free(c->checkpoint); free(c->journal); free(c);
}
static bool checkpoint_prepare(void *context,char **error) {
    Checkpoint *c=context;
    if (!c->snapshot) return true;
    if (c->versioned) {
        if (!dx_make_directories(c->version_parent)) return fail(error,"Cannot create the version working folder for local backup.");
        WorkbookVersionReceipt *receipt=NULL;
        WorkbookSaveResult result=workbook_version_save((WorkbookVersionSaveRequest){
            .head=c->snapshot,.base=c->versions,.owner=c->binding.owner,.purpose=VERSION_RECOVERY_CHECKPOINT,
            .path=c->checkpoint,.parent=c->version_parent,.reserved_bytes=c->binding.reserved_bytes
        },(WorkbookVersionSaveControl){0},&receipt,error);
        workbook_version_receipt_free(receipt);
        if (result!=WORKBOOK_SAVE_COMPLETE) return false;
    } else if (!workbook_save(c->snapshot,c->checkpoint,error)) return false;
#ifdef __APPLE__
    /* The existing streaming digest adapter is validated on native macOS.
       Other native hosts retain unbound v1 backup behavior until their digest
       boundary is implemented; no SDK dependency enters this C controller. */
    char *wal=dx_printf("%s.wal",c->checkpoint),*unfinished=dx_absolute_path(wal); free(wal);
    if (unfinished) { free(unfinished); return fail(error,"The recovery checkpoint still has a pending WAL. Retry after the storage error is resolved."); }
    SourceDigest digest={0}; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    if (source_digest_file(c->checkpoint,NULL,NULL,&digest,detail)!=SOURCE_DIGEST_OK) return fail(error,*detail ? detail : "Cannot fingerprint the recovery checkpoint.");
    c->origin.baseline_bytes=digest.bytes; memcpy(c->origin.baseline_sha256,digest.sha256,DX_JOURNAL_DIGEST_BYTES);
#endif
    workbook_free(c->snapshot); free(c->snapshot); c->snapshot=NULL; return true;
}
#ifdef __APPLE__
static const JournalOrigin *checkpoint_origin(void *context) { return &((Checkpoint *)context)->origin; }
#endif
static void remove_owned(const Checkpoint *c) {
    /* Fixed private filenames only. Never recursively remove an unrecognized
       file or touch an external source/named workbook. */
    remove(c->checkpoint); remove(c->journal); if (c->lease_path) remove(c->lease_path);
    char *wal=dx_printf("%s.wal",c->journal); remove(wal); free(wal);
    wal=dx_printf("%s.wal",c->checkpoint); remove(wal); free(wal);
    dx_remove_temp_directory(c->directory);
}
static bool generation_close(Generation *g,bool discard,bool *published,char **error) {
    if (!g->queue) return true;
    bool complete=workbook_journal_queue_stop(g->queue,!discard,discard ? NULL : error);
    if (published) *published=complete && !discard && workbook_journal_queue_state(g->queue).prepared;
    if (discard) remove_owned(g->checkpoint);
    workbook_journal_queue_free(g->queue); *g=(Generation){0}; return discard || complete;
}
WorkbookJournalSession *workbook_journal_session_new(const char *root,JournalQueueOptions options,char **error) {
    if (!root || !*root || options.setup.prepare || options.setup.release || options.setup.context || options.origin ||
        !options.entries || options.entries>DX_JOURNAL_BATCH || !options.bytes || options.bytes>DX_JOURNAL_BATCH_BYTES || !options.batch || options.batch>options.entries) {
        fail(error,"Choose a recovery root and bounded journal options."); return NULL;
    }
    WorkbookId operation;
    if (!dx_random_bytes(&operation,sizeof(operation)) || (!operation.high && !operation.low)) { fail(error,"Cannot allocate a recovery operation identity."); return NULL; }
    WorkbookJournalSession *s=dx_alloc(sizeof(*s)); memset(s,0,sizeof(*s)); s->root=dx_strdup(root); s->options=options; s->operation=operation; return s;
}
bool workbook_journal_session_needs_checkpoint(const WorkbookJournalSession *s,const Workbook *b) { return !s || !s->active.queue || s->version_gap || !matches(&s->expected,b); }
static size_t generation_bytes(Generation *g) {
    return g->queue && !workbook_journal_queue_state(g->queue).prepared ? g->checkpoint->format_bytes : 0;
}
bool workbook_journal_session_lineage(WorkbookJournalSession *s,JournalSessionLineage lineage,char **error) {
    const char *path=lineage.original_path;
    if (!s || s->active.queue || s->retired.queue || (!lineage.document.high && !lineage.document.low) || (!lineage.parent.high && !lineage.parent.low) || !path || strlen(path)>65536 ||
        (*path && path[0]!='/' && path[0]!='\\' && !(strlen(path)>2 && path[1]==':' && (path[2]=='/' || path[2]=='\\')))) return fail(error,"Choose a recovered document/parent and informational original locator before editing.");
    char *owned=dx_strdup(path); free(s->inherited_original); s->inherited_original=owned; s->inherited_document=lineage.document; s->inherited_parent=lineage.parent; return true;
}
void workbook_journal_session_poll(WorkbookJournalSession *s) {
    if (!s || !s->active.queue || !s->retired.queue) return;
    JournalQueueState current=workbook_journal_queue_state(s->active.queue),old=workbook_journal_queue_state(s->retired.queue);
    /* The new complete checkpoint contains all previous accepted edits before
       its base. Retire only after publication and the old writer's exit. */
    if (current.prepared && !current.opening && !current.failed && old.stopped) generation_close(&s->retired,true,NULL,NULL);
}
static bool begin_generation(WorkbookJournalSession *s,const Workbook *b,const JournalCheckpointVersions *versions,char **error) {
    if ((s->inherited_document.high || s->inherited_document.low) && (s->inherited_document.high!=b->id.high || s->inherited_document.low!=b->id.low)) return fail(error,"The recovered lineage belongs to a different workbook. Your draft is kept.");
    workbook_journal_session_poll(s);
    if (s->retired.queue) return fail(error,"The previous backup is still finishing. Your draft is kept; try again shortly.");
    WorkbookFormatUsage usage; FormatBudget budget;
    FormatBudgetStatus status=workbook_format_usage(b,NULL,0,&usage);
    if (status!=FORMAT_BUDGET_OK) return fail(error,workbook_format_budget_message(status));
    status=workbook_format_preflight(&usage,&usage,&(FormatTransientUsage){.adopted_staging=usage.current,.io_snapshots=generation_bytes(&s->active)},&budget);
    if (status!=FORMAT_BUDGET_OK) return fail(error,workbook_format_budget_message(status));
    if (!dx_make_directories(s->root)) return fail(error,"Cannot create the local backup folder. Your draft is kept.");
    Checkpoint *c=dx_alloc(sizeof(*c)); memset(c,0,sizeof(*c)); c->format_bytes=usage.current;
    if (versions) {
        c->versioned=true; c->binding=*versions; c->version_parent=dx_strdup(versions->parent);
        c->binding.parent=c->version_parent;
        c->versions=workbook_version_core_set_retain(versions->base);
        if (versions->base && !c->versions) { checkpoint_release(c); return fail(error,"Cannot retain Saved versions for local backup."); }
    }
    bool identified=false;
    for (unsigned attempt=0;attempt<8 && !identified;attempt++) {
        if (!dx_random_bytes(&c->origin.generation,sizeof(c->origin.generation))) break;
        identified=c->origin.generation.high || c->origin.generation.low;
    }
    if (!identified) { checkpoint_release(c); return fail(error,"Cannot allocate a recovery generation identity. Your draft is kept."); }
    c->origin.parent=s->active.checkpoint ? s->active.checkpoint->origin.generation : s->inherited_parent;
    if (b->path && strlen(b->path)>65536) { checkpoint_release(c); return fail(error,"The original workbook path exceeds the recovery metadata limit."); }
    c->origin.original_path=dx_strdup(b->path && *b->path ? b->path : s->inherited_original ? s->inherited_original : "");
    c->directory=dx_private_directory(s->root);
    if (!c->directory) { checkpoint_release(c); return fail(error,"Cannot create a private local backup. Your draft is kept."); }
    c->checkpoint=dx_printf("%s/checkpoint.dxls",c->directory); c->journal=dx_printf("%s/journal.duckdb",c->directory);
    c->lease_path=dx_printf("%s/lease.lock",c->directory);
    if (dx_file_lease_acquire(c->lease_path,true,&c->lease,error)!=DX_FILE_LEASE_ACQUIRED) { remove_owned(c); checkpoint_release(c); return false; }
    c->snapshot=workbook_snapshot(b);
    if (!c->snapshot) { remove_owned(c); checkpoint_release(c); return fail(error,"Cannot prepare this workbook for local backup. Your draft is kept."); }
    JournalQueueOptions options=s->options; options.setup=(JournalQueueSetup){checkpoint_prepare,checkpoint_release,c};
#ifdef __APPLE__
    options.origin=checkpoint_origin;
#else
    options.origin=NULL;
#endif
    WorkbookJournalQueue *queue=workbook_journal_queue_start(c->journal,b->id,b->command_revision,b->command_revision,options,error);
    if (!queue) { remove_owned(c); checkpoint_release(c); return false; }
    if (s->active.queue) {
        workbook_journal_queue_request_stop(s->active.queue,true,NULL); s->retired=s->active;
    }
    s->active=(Generation){queue,c}; s->version_gap=false; capture(&s->expected,b); return true;
}
bool workbook_journal_session_capture_versions(WorkbookJournalSession *s,const Workbook *b,JournalCheckpointVersions v,char **error) {
    const WorkbookVersionIndex *index=workbook_version_core_set_index(v.base);
    if (!s || !b || !workbook_identity_valid(b) || !v.parent || !*v.parent || !v.reserved_bytes ||
        !v.owner.session || v.owner.mode!=VERSION_WORKING_HEAD || v.owner.document_high!=b->id.high || v.owner.document_low!=b->id.low ||
        (v.base && (!index || index->document_high!=b->id.high || index->document_low!=b->id.low)))
        return fail(error,"Choose a matching working owner and complete Saved store for local backup.");
    const Checkpoint *c=s->active.checkpoint;
    if (c && c->versioned && c->versions==v.base && c->binding.owner.session==v.owner.session && !workbook_journal_session_needs_checkpoint(s,b)) return true;
    s->version_gap=true; return begin_generation(s,b,&v,error);
}
CellEditResult workbook_journal_session_apply(WorkbookJournalSession *s,Workbook *b,const CellEditCommand *command,CellEditPolicy policy,char **error) {
    CellEditResult result=workbook_check_cell_edit(b,command,policy,error);
    if (result.status!=CELL_EDIT_READY) return result;
    result.status=CELL_EDIT_LIMIT;
    if (!s) { fail(error,"Local backup is unavailable. Your draft is kept."); return result; }
    if (workbook_journal_session_needs_checkpoint(s,b)) {
        if (s->active.checkpoint && s->active.checkpoint->versioned) {
            fail(error,"Capture the current complete Saved store before the next versioned recovery generation."); return result;
        }
        if (!begin_generation(s,b,NULL,error)) return result;
    }
    if (s->operation.high==UINT64_MAX && s->operation.low==UINT64_MAX) { fail(error,"Recovery operation identity space is exhausted."); return result; }
    const Worksheet *sheet=NULL; for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==command->target.sheet_id) sheet=b->sheets+i;
    const char *before=cells_get(&sheet->cells,command->target.row,command->target.column);
    JournalCellEdit edit={.operation=s->operation,.previous_revision=b->command_revision,.revision=b->command_revision+1,.sheet=sheet->id,
        .row=command->target.row,.column=command->target.column,.before=before,.before_bytes=strlen(before),.after=command->text,.after_bytes=command->bytes};
    JournalReservation *reservation=NULL;
    if (!workbook_journal_queue_reserve(s->active.queue,&edit,&reservation,error)) return result;
    if (!++s->operation.low) s->operation.high++;
    result=workbook_apply_cell_edit(b,command,policy,error);
    if (result.status==CELL_EDIT_APPLIED) { workbook_journal_queue_publish(reservation); capture(&s->expected,b); }
    else workbook_journal_queue_cancel(reservation);
    return result;
}
JournalSessionState workbook_journal_session_state(WorkbookJournalSession *s,const Workbook *b) {
    JournalSessionState state={0}; if (!s) return state;
    state.active=s->active.queue!=NULL; state.covered=state.active && !s->version_gap && matches(&s->expected,b);
    if (state.active) { state.queue=workbook_journal_queue_state(s->active.queue); state.generation=s->active.checkpoint->origin.generation; state.parent=s->active.checkpoint->origin.parent; }
    state.checkpoint_format_bytes=generation_bytes(&s->active)+generation_bytes(&s->retired); return state;
}
bool workbook_journal_session_retry(WorkbookJournalSession *s,char **error) {
    return s && s->active.queue ? workbook_journal_queue_retry(s->active.queue,error) : fail(error,"There is no pending local backup to retry.");
}
bool workbook_journal_session_close(WorkbookJournalSession *s,bool discard,char **error) {
    if (!s) return true;
    bool published=false;
    bool active=generation_close(&s->active,discard,&published,error),retired=generation_close(&s->retired,discard || published,NULL,error);
    free(s->inherited_original); free(s->root); free(s); return active && retired;
}
bool workbook_journal_session_owns_path(const WorkbookJournalSession *s,const char *path) {
    if (!s || !path || !*path) return false;
    const Generation *generations[]={&s->active,&s->retired};
    for (size_t i=0;i<2;i++) if (generations[i]->queue) {
        const Checkpoint *c=generations[i]->checkpoint;
        if (!strcmp(path,c->checkpoint) || !strcmp(path,c->journal) || dx_same_file(path,c->checkpoint) || dx_same_file(path,c->journal)) return true;
        char *ancestor=dx_strdup(path);
        for (;;) {
            char *last=NULL; for (char *p=ancestor;*p;p++) if (*p=='/' || *p=='\\') last=p;
            if (!last || last==ancestor) break;
            *last=0;
            if (dx_same_file(ancestor,c->directory)) { free(ancestor); return true; }
        }
        free(ancestor);
    }
    return false;
}
const char *workbook_journal_session_checkpoint(const WorkbookJournalSession *s) { return s && s->active.queue ? s->active.checkpoint->checkpoint : NULL; }
const char *workbook_journal_session_journal(const WorkbookJournalSession *s) { return s && s->active.queue ? s->active.checkpoint->journal : NULL; }
