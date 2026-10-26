#ifndef DUXCEL_CORE_COMPARE_INTERNAL_H
#define DUXCEL_CORE_COMPARE_INTERNAL_H
#include "core_compare.h"
/* Archive scope only: the caller has strictly admitted current core tables and
   owns a read-only transaction. Complete row comparison shares the standalone
   comparator; no model decoder or authority is introduced. On any failure the
   archive owner ends/closes this unpublished connection and its read-only
   attachment on BOTH success and failure. Do not retry on this connection. */
WorkbookCorePayloadResult workbook_core_payload_equal_transaction(duckdb_connection connection,
    const WorkbookCorePayload *right,const char *archive,const Workbook *book,
    WorkbookCoreCompareControl control,bool *equal,char **error);
#endif
