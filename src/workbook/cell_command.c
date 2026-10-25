#include "cell_command.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>

static uint64_t issued_identity;
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); }
    return false;
}
bool workbook_prepare_command_session(Workbook *book,size_t index,char **error) {
    if (!book || book->count>DX_MAX_SHEETS || !book->sheets || index>=book->count)
        return fail(error,"Choose an existing worksheet command owner.");
    if (!workbook_identity_valid(book)) return fail(error,"Invalid workbook/worksheet identities.");
    Worksheet *sheet=&book->sheets[index];
    unsigned needed=!book->edit_session_id + (unsigned)!sheet->edit_session_id;
    if (issued_identity>UINT64_MAX-needed) return fail(error,"Worksheet command identity space is exhausted.");
    if (!book->edit_session_id) book->edit_session_id=++issued_identity;
    if (!sheet->edit_session_id) sheet->edit_session_id=++issued_identity;
    return true;
}
bool workbook_prepare_cell_edit(Workbook *book,size_t index,uint64_t row,size_t col,CellEditTarget *out,char **error) {
    if (!book || !out || book->count>DX_MAX_SHEETS || !book->sheets || index>=book->count || row>=DX_SHEET_ROWS || col>=16384)
        return fail(error,"Choose a valid manual cell before editing.");
    if (!workbook_prepare_command_session(book,index,error)) return false;
    Worksheet *sheet=&book->sheets[index];
    *out=(CellEditTarget){.document=book->edit_session_id,.sheet=sheet->edit_session_id,
        .expected_revision=sheet->cells.revision,.row=row,.column=col,.workbook=book->id,.expected_document_revision=book->command_revision,.sheet_id=sheet->id};
    return true;
}
CellEditResult workbook_check_cell_edit(const Workbook *book,const CellEditCommand *command,CellEditPolicy policy,char **error) {
    CellEditResult result={.status=CELL_EDIT_INVALID};
    if (!book || !command || command->version!=CELL_EDIT_COMMAND_VERSION || book->count>DX_MAX_SHEETS ||
        (book->count && !book->sheets) || command->target.row>=DX_SHEET_ROWS || command->target.column>=16384 ||
        (!command->text && command->bytes) || command->bytes>65536 ||
        (command->bytes && memchr(command->text,0,command->bytes))) {
        fail(error,"Invalid bounded cell edit command."); return result;
    }
    const CellEditTarget *target=&command->target;
    if (!target->document || !target->sheet || !target->sheet_id || target->document!=book->edit_session_id ||
        target->workbook.high!=book->id.high || target->workbook.low!=book->id.low) {
        result.status=CELL_EDIT_STALE; fail(error,"The document changed. Cancel and reopen the cell editor."); return result;
    }
    const Worksheet *sheet=NULL;
    for (size_t i=0;i<book->count;i++) if (book->sheets[i].id==target->sheet_id) {
        if (sheet) { fail(error,"Ambiguous cell edit target."); return result; }
        sheet=&book->sheets[i];
    }
    if (!sheet || sheet->edit_session_id!=target->sheet) { result.status=CELL_EDIT_STALE; fail(error,"The worksheet changed. Cancel and reopen the cell editor."); return result; }
    result.before_revision=result.after_revision=sheet->cells.revision;
    result.before_document_revision=result.after_document_revision=book->command_revision;
    if (target->expected_revision!=sheet->cells.revision || target->expected_document_revision!=book->command_revision) {
        result.status=CELL_EDIT_STALE;
        fail(error,"Cell data changed while editing. Your draft is kept; press Escape and reopen the cell to use the latest data."); return result;
    }
    result.status=CELL_EDIT_PROTECTED;
    if (!policy.source_ready) { fail(error,"Wait for the source columns before editing this cell."); return result; }
    char *failure=NULL;
    if (!objects_fits(&sheet->objects,-1,target->row,target->column,1,1,NULL,policy.source_rows,policy.source_columns,&failure)) {
        fail(error,failure ? failure : "This cell is read-only."); free(failure); return result;
    }
    const char *before=cells_get(&sheet->cells,target->row,target->column);
    if (strlen(before)==command->bytes && (!command->bytes || !memcmp(before,command->text,command->bytes))) {
        result.status=CELL_EDIT_NOOP; return result;
    }
    result.status=CELL_EDIT_LIMIT;
    if (sheet->cells.revision==UINT64_MAX || book->command_revision==UINT64_MAX || !worksheet_history_order_available()) { fail(error,"Cell editing revision space is exhausted."); return result; }
    if (workbook_history_peek(book,false,NULL).status==WORKBOOK_HISTORY_INVALID) {
        result.status=CELL_EDIT_INVALID; fail(error,"Worksheet history is incomplete; no cell edit was applied."); return result;
    }
    result.status=CELL_EDIT_READY; return result;
}
static Worksheet *edit_sheet(Workbook *book,uint64_t id) {
    for (size_t i=0;i<book->count;i++) if (book->sheets[i].id==id) return book->sheets+i;
    return NULL;
}
CellEditResult workbook_apply_cell_edit_recorded(Workbook *book,const CellEditCommand *command,CellEditPolicy policy,const CellEditRecorder *recorder,char **error) {
    CellEditResult result=workbook_check_cell_edit(book,command,policy,error);
    if (result.status!=CELL_EDIT_READY) return result;
    const CellEditTarget *target=&command->target; Worksheet *sheet=edit_sheet(book,target->sheet_id);
    void *ticket=NULL;
    if (recorder) {
        result.status=CELL_EDIT_INVALID;
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { fail(error,"Cell recording is incomplete; your draft is kept."); return result; }
        const char *before=cells_get(&sheet->cells,target->row,target->column);
        CellEditRecord record={.target=*target,.revision=book->command_revision+1,
            .delta={target->sheet_id,target->row,target->column,before,command->text,strlen(before),command->bytes}};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) {
            bool invalid=reserved || ticket!=NULL;
            if (ticket) recorder->cancel(recorder->context,ticket);
            result.status=invalid ? CELL_EDIT_INVALID : CELL_EDIT_LIMIT;
            if (!error || !*error) fail(error,"Cell recording is busy; your draft is kept.");
            return result;
        }
        result=workbook_check_cell_edit(book,command,policy,error);
        if (result.status!=CELL_EDIT_READY) { recorder->cancel(recorder->context,ticket); return result; }
        sheet=edit_sheet(book,target->sheet_id);
    }
    char *failure=NULL;
    result.status=CELL_EDIT_LIMIT;
    char *owned=dx_alloc(command->bytes+1);
    if (command->bytes) memcpy(owned,command->text,command->bytes);
    owned[command->bytes]=0;
    bool accepted=cells_set(&sheet->cells,target->row,target->column,owned,&failure); free(owned);
    if (!accepted) {
        if (recorder) recorder->cancel(recorder->context,ticket);
        fail(error,failure ? failure : "Cell edit could not be applied."); free(failure); return result;
    }
    worksheet_record(sheet,EDIT_CELLS); workbook_history_note_edit(book); book->dirty=true; book->command_revision++;
    result.status=CELL_EDIT_APPLIED; result.after_revision=sheet->cells.revision; result.after_document_revision=book->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return result;
}
CellEditResult workbook_apply_cell_edit(Workbook *book,const CellEditCommand *command,CellEditPolicy policy,char **error) {
    return workbook_apply_cell_edit_recorded(book,command,policy,NULL,error);
}
