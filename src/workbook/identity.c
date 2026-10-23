#include "workbook.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <string.h>

static bool fail(char **error) {
    if (error) { free(*error); *error=dx_strdup("Invalid or exhausted workbook/worksheet identity metadata."); }
    return false;
}
bool workbook_identity_valid(const Workbook *book) {
    if (!book || book->count>DX_MAX_SHEETS || (book->count && !book->sheets)) return false;
    if (!book->id.high && !book->id.low) return !book->count && !book->sheet_high_water;
    for (size_t i=0;i<book->count;i++) {
        uint64_t id=book->sheets[i].id;
        if (!id || id>book->sheet_high_water) return false;
        for (size_t j=0;j<i;j++) if (book->sheets[j].id==id) return false;
    }
    return true;
}
bool workbook_identity_next(const Workbook *book,WorkbookId *document,uint64_t *sheet,char **error) {
    if (!document || !sheet || !workbook_identity_valid(book) || book->sheet_high_water==UINT64_MAX) return fail(error);
    WorkbookId id=book->id;
    if (!id.high && !id.low) {
        if (!dx_random_bytes(&id,sizeof(id)) || (!id.high && !id.low)) return fail(error);
    }
    *document=id; *sheet=book->sheet_high_water+1; return true;
}
bool workbook_identity_restore(Workbook *book,WorkbookId document,uint64_t high_water,const uint64_t *sheets,size_t count,char **error) {
    if (!book || !sheets || !count || count>DX_MAX_SHEETS || count!=book->count || !book->sheets || (!document.high && !document.low)) return fail(error);
    /* Validate a bounded independent candidate before touching the live model.
       Adapters must reject SQL NULL/wrong types before passing unsigned values. */
    for (size_t i=0;i<count;i++) {
        if (!sheets[i] || sheets[i]>high_water) return fail(error);
        for (size_t j=0;j<i;j++) if (sheets[j]==sheets[i]) return fail(error);
    }
    book->id=document; book->sheet_high_water=high_water;
    for (size_t i=0;i<count;i++) book->sheets[i].id=sheets[i];
    return true;
}
