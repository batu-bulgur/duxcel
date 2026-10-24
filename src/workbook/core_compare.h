#ifndef DUXCEL_CORE_COMPARE_H
#define DUXCEL_CORE_COMPARE_H
#include "core_payload.h"
typedef enum { CORE_COMPARE_VERIFY,CORE_COMPARE_ROWS,CORE_COMPARE_RECHECK } WorkbookCoreComparePhase;
typedef struct {
    /* Synchronous bounded callback on the worker; true cancels. table is the
       current codec descriptor ordinal during ROWS, zero during file checks.
       Called between hash blocks, tables and DuckDB pending execution tasks.
       Never mutate/release either payload, reenter SQL or retain helper data. */
    bool (*cancelled)(void *context,WorkbookCoreComparePhase phase,size_t table);
    void *context;
} WorkbookCoreCompareControl;
/* Compare every persisted v15 metadata table/column as typed SQL multisets in
   both directions, including identities, clocks, presentation and definitions.
   Ordering is represented by stored ordinal fields, never physical row order.
   Source query strings are compared as data, never executed. Session history,
   external bytes and outputs absent from the core codec are outside this proof.
   Borrows two sealed leaves for the whole native worker call. Both are verified
   before/after read-only SQL, with bounded engine memory/private spill. equal is
   changed only on READY. Physical byte hashes, query text or command clocks
   alone are never substituted for complete core-state equality. */
WorkbookCorePayloadResult workbook_core_payload_equal(const WorkbookCorePayload *left,
    const WorkbookCorePayload *right,WorkbookCoreCompareControl control,bool *equal,char **error);
#endif
