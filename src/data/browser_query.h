#ifndef DUXCEL_BROWSER_QUERY_H
#define DUXCEL_BROWSER_QUERY_H
#include "page.h"

#define DX_BROWSER_SQL_BYTES (1024u*1024u)

/* These pure C helpers own returned SQL and schema strings with free(). No
   engine calls or host paths are resolved here. SQL is generated from the real
   Analysis/Pivot definitions, with inline escaped filters, for the official
   browser Worker. NONE and attached native databases are unavailable in W2. */
char *browser_relation_sql(const Analysis *analysis,const PivotDefinition *pivot,char **error);
char *browser_describe_sql(const char *relation,char **error);
char *browser_count_sql(const char *relation,char **error);
/* Six-column DESCRIBE reply -> owned zero-row schema. Standard column headers,
   names/types, <=256 rows and unique case-insensitive names are validated.
   Unknown SQL types retain their exact type_name with VARCHAR/NUM_NONE fallback.
   out must be zero-initialized or owned; failure leaves it unchanged. */
bool browser_schema_from_describe(Page *out,const Page *describe,char **error);
/* Canonical text projection keeps SQL NULL distinct and preserves exact raw
   numbers/temporal text. The original typed schema accompanies its wire reply.
   The full fixed 256-row interval must fit uint64_t. All SQL is <=1 MiB. */
char *browser_page_sql(const char *relation,const Page *schema,uint64_t offset,char **error);
#endif
