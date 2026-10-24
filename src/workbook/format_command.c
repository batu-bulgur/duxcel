#include "format_command.h"
#include "history.h"
#include <stdlib.h>
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static bool eligible(const Worksheet *s) {
    return s->analysis.kind==SOURCE_NONE && !s->objects.count && !s->formats.source_aware && !s->formats.scope_count;
}
bool workbook_prepare_manual_format(Workbook *book,size_t index,ManualFormatTarget *out,char **error) {
    if (!out || !workbook_identity_valid(book) || index>=book->count) return fail(error,"Choose a valid worksheet before formatting.");
    Worksheet *s=book->sheets+index;
    if (!eligible(s)) return fail(error,"Source and analytical formatting require their complete command owner.");
    CellEditTarget owner={0};
    if (!workbook_prepare_cell_edit(book,index,0,0,&owner,error)) return false;
    *out=(ManualFormatTarget){owner,s->formats.revision}; return true;
}
static Worksheet *resolve(Workbook *book,uint64_t id) {
    for (size_t i=0;i<book->count;i++) if (book->sheets[i].id==id) return book->sheets+i;
    return NULL;
}
static ManualFormatResult check(Workbook *book,const ManualFormatTarget *target,const FormatSnapshot *next,char **error) {
    ManualFormatResult r={.status=MANUAL_FORMAT_INVALID};
    if (!target || !next || !workbook_identity_valid(book) || !target->owner.document || !target->owner.sheet ||
        target->owner.row || target->owner.column) { fail(error,"Invalid manual format command."); return r; }
    const CellEditTarget *t=&target->owner; Worksheet *s=resolve(book,t->sheet_id);
    r.before_document_revision=r.after_document_revision=book->command_revision;
    if (!s || s->edit_session_id!=t->sheet || book->edit_session_id!=t->document || book->id.high!=t->workbook.high || book->id.low!=t->workbook.low) {
        r.status=MANUAL_FORMAT_STALE; fail(error,"The formatting owner changed; the prepared edit is kept."); return r;
    }
    r.before_revision=r.after_revision=s->formats.revision;
    if (s->formats.revision!=target->expected_format_revision || s->cells.revision!=t->expected_revision || book->command_revision!=t->expected_document_revision) {
        r.status=MANUAL_FORMAT_STALE; fail(error,"The worksheet changed; prepare formatting again."); return r;
    }
    if (!eligible(s) || next->source_aware || next->scope_count) {
        r.status=MANUAL_FORMAT_PROTECTED; fail(error,"Source and analytical formatting require their complete command owner."); return r;
    }
    FormatStore candidate={.items=next->items,.count=next->count,.scopes=next->scopes,.scope_count=next->scope_count,.source_aware=next->source_aware};
    if (!format_valid(&s->formats,error) || !format_valid(&candidate,error) || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) return r;
    if (format_snapshot_matches(&s->formats,next)) { r.status=MANUAL_FORMAT_NOOP; return r; }
    if (s->formats.revision==UINT64_MAX || book->command_revision==UINT64_MAX || !worksheet_history_order_available()) {
        r.status=MANUAL_FORMAT_LIMIT; fail(error,"Formatting revision space is exhausted."); return r;
    }
    if (workbook_history_peek(book,false,NULL).status==WORKBOOK_HISTORY_INVALID) { fail(error,"Worksheet history is incomplete; no formatting was applied."); return r; }
    /* Borrowed projection: format_commit keeps at most32 previous states,
       releases Redo and adopts the independent candidate without allocation. */
    FormatStore projected=s->formats;
    projected.items=next->items; projected.count=next->count; projected.scopes=next->scopes; projected.scope_count=next->scope_count; projected.source_aware=next->source_aware;
    projected.redo_count=0;
    size_t start=projected.undo_count==DX_FORMAT_HISTORY ? 1 : 0;
    if (start) for (size_t i=1;i<projected.undo_count;i++) projected.undo[i-1]=projected.undo[i];
    projected.undo_count-=start;
    projected.undo[projected.undo_count++]=(FormatSnapshot){s->formats.items,s->formats.count,s->formats.scopes,s->formats.scope_count,s->formats.source_aware};
    if (!format_valid(&projected,error)) { r.status=MANUAL_FORMAT_LIMIT; return r; }
    r.status=MANUAL_FORMAT_READY; return r;
}
ManualFormatResult workbook_apply_manual_format_recorded(Workbook *book,const ManualFormatTarget *target,FormatSnapshot *owned,
    const WorkbookHistoryFormatRecorder *recorder,char **error) {
    ManualFormatResult r=check(book,target,owned,error);
    if (r.status==MANUAL_FORMAT_NOOP) { format_snapshot_free(owned); return r; }
    if (r.status!=MANUAL_FORMAT_READY) return r;
    Worksheet *s=resolve(book,target->owner.sheet_id); void *ticket=NULL;
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=MANUAL_FORMAT_INVALID; fail(error,"Format recording is incomplete; the prepared edit is kept."); return r; }
        WorkbookHistoryFormatRecord record={.workbook=book->id,.edit_session=book->edit_session_id,.sheet_session=s->edit_session_id,.sheet=s->id,
            .expected_revision=book->command_revision,.revision=book->command_revision+1,.effects=8,
            .before=format_delta_store_view(&s->formats),.after=format_delta_snapshot_view(owned),.sources=&s->source_versions,.object_high_water=s->objects.next_id};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) {
            bool invalid=reserved || ticket!=NULL; if (ticket) recorder->cancel(recorder->context,ticket);
            r.status=invalid ? MANUAL_FORMAT_INVALID : MANUAL_FORMAT_LIMIT;
            if (!error || !*error) fail(error,"Format recording is busy; the prepared edit is kept."); return r;
        }
        r=check(book,target,owned,error);
        if (r.status!=MANUAL_FORMAT_READY) {
            recorder->cancel(recorder->context,ticket);
            if (r.status==MANUAL_FORMAT_NOOP) format_snapshot_free(owned);
            return r;
        }
        s=resolve(book,target->owner.sheet_id);
    }
    if (!format_commit(&s->formats,owned)) {
        if (recorder) recorder->cancel(recorder->context,ticket);
        r.status=MANUAL_FORMAT_NOOP; return r;
    }
    worksheet_record(s,EDIT_FORMAT); book->command_revision++; book->dirty=true; workbook_history_note_edit(book);
    r.status=MANUAL_FORMAT_APPLIED; r.after_revision=s->formats.revision; r.after_document_revision=book->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
}
