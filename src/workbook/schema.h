#ifndef DUXCEL_WORKBOOK_SCHEMA_H
#define DUXCEL_WORKBOOK_SCHEMA_H
#include "workbook_codec.h"
#define DX_WORKBOOK_SCHEMA_TABLES 64u
typedef struct { const void *data; size_t bytes; } WorkbookSchemaText;
typedef enum { WORKBOOK_SCHEMA_OK,WORKBOOK_SCHEMA_STRUCTURE,WORKBOOK_SCHEMA_COLUMN_TYPES } WorkbookSchemaError;
typedef struct {
    unsigned package; size_t tables,rows,expected_rows;
    uint64_t seen[DX_WORKBOOK_SCHEMA_TABLES],expected[DX_WORKBOOK_SCHEMA_TABLES];
    WorkbookSchemaError error;
} WorkbookSchemaProof;
/* Pure current-package shape proof. Borrowed metadata spans are consumed only
   during offer; there is no allocation, SQL, model or row-value validation.
   Table names stay exact as in the package inventory. Column identifiers use
   DuckDB's ASCII case folding and may arrive in any physical order. A bad or
   duplicate field poisons the proof; required row values remain a separate gate. */
bool workbook_schema_begin(WorkbookSchemaProof *out,unsigned package);
bool workbook_schema_offer(WorkbookSchemaProof *proof,WorkbookSchemaText table,WorkbookSchemaText kind,WorkbookSchemaText column,WorkbookSchemaText type);
bool workbook_schema_complete(const WorkbookSchemaProof *proof);
#endif
