#include "workbook.h"
#ifdef DUXCEL_BROWSER_HOST
#include <stdlib.h>

/* W1 compiles the real workbook model without a native DuckDB connection.
   Browser persistence requires its own asynchronous engine/file adapter. */
static bool unavailable(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); }
    return false;
}
bool workbook_save(const Workbook *book,const char *path,char **error) {
    (void)book; (void)path;
    return unavailable(error,"Workbook saving is not available in the browser renderer proof.");
}
bool workbook_load(Workbook *book,const char *path,char **error) {
    (void)book; (void)path;
    return unavailable(error,"Workbook loading is not available in the browser renderer proof.");
}
#endif
