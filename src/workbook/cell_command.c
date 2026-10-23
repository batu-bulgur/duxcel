#include "cell_command.h"
#include <stdlib.h>
#include <string.h>

static uint64_t issued_identity;
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); }
    return false;
}
bool workbook_prepare_cell_edit(Workbook *book,size_t index,uint64_t row,size_t col,CellEditTarget *out,char **error) {
    if (!book || !out || book->count>DX_MAX_SHEETS || !book->sheets || index>=book->count || row>=DX_SHEET_ROWS || col>=16384)
        return fail(error,"Choose a valid manual cell before editing.");
    Worksheet *sheet=&book->sheets[index];
    unsigned needed=!book->edit_session_id + (unsigned)!sheet->edit_session_id;
    if (issued_identity>UINT64_MAX-needed) return fail(error,"Cell editing identity space is exhausted.");
    if (!book->edit_session_id) book->edit_session_id=++issued_identity;
    if (!sheet->edit_session_id) sheet->edit_session_id=++issued_identity;
    *out=(CellEditTarget){book->edit_session_id,sheet->edit_session_id,sheet->cells.revision,row,col};
    return true;
}
CellEditResult workbook_apply_cell_edit(Workbook *book,const CellEditCommand *command,CellEditPolicy policy,char **error) {
    CellEditResult result={CELL_EDIT_INVALID,0,0};
    if (!book || !command || command->version!=CELL_EDIT_COMMAND_VERSION || book->count>DX_MAX_SHEETS ||
        (book->count && !book->sheets) || command->target.row>=DX_SHEET_ROWS || command->target.column>=16384 ||
        (!command->text && command->bytes) || command->bytes>65536 ||
        (command->bytes && memchr(command->text,0,command->bytes))) {
        fail(error,"Invalid bounded cell edit command."); return result;
    }
    const CellEditTarget *target=&command->target;
    if (!target->document || !target->sheet || target->document!=book->edit_session_id) {
        result.status=CELL_EDIT_STALE; fail(error,"The document changed. Cancel and reopen the cell editor."); return result;
    }
    Worksheet *sheet=NULL;
    for (size_t i=0;i<book->count;i++) if (book->sheets[i].edit_session_id==target->sheet) {
        if (sheet) { fail(error,"Ambiguous cell edit target."); return result; }
        sheet=&book->sheets[i];
    }
    if (!sheet) { result.status=CELL_EDIT_STALE; fail(error,"The worksheet changed. Cancel and reopen the cell editor."); return result; }
    result.before_revision=result.after_revision=sheet->cells.revision;
    if (target->expected_revision!=sheet->cells.revision) {
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
    if (sheet->cells.revision==UINT64_MAX) { fail(error,"Cell editing revision space is exhausted."); return result; }
    char *owned=dx_alloc(command->bytes+1);
    if (command->bytes) memcpy(owned,command->text,command->bytes);
    owned[command->bytes]=0;
    bool accepted=cells_set(&sheet->cells,target->row,target->column,owned,&failure); free(owned);
    if (!accepted) { fail(error,failure ? failure : "Cell edit could not be applied."); free(failure); return result; }
    worksheet_record(sheet,EDIT_CELLS); book->dirty=true;
    result.status=CELL_EDIT_APPLIED; result.after_revision=sheet->cells.revision;
    return result;
}
