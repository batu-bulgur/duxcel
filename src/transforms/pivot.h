#ifndef DUXCEL_PIVOT_H
#define DUXCEL_PIVOT_H
#include "analysis.h"
#define DX_PIVOT_FIELDS 8
#define DX_MAX_OBJECTS 32
typedef enum { PIVOT_ROWS, PIVOT_COLUMNS, PIVOT_VALUES, PIVOT_FILTERS } PivotRole;
typedef enum { PIVOT_DATE_EXACT,PIVOT_DATE_YEAR,PIVOT_DATE_QUARTER,PIVOT_DATE_MONTH } PivotDateGrain;
typedef struct { char *name,*value; Aggregate aggregate; FilterOp filter; NumberType number; PivotDateGrain grain; } PivotField;
typedef struct { PivotField fields[4][DX_PIVOT_FIELDS]; size_t counts[4]; } PivotDefinition;
void pivot_free(PivotDefinition *pivot);
PivotDefinition pivot_copy(const PivotDefinition *pivot);
bool pivot_assign(PivotDefinition *pivot,PivotRole role,const char *field,Aggregate aggregate,char **error);
bool pivot_assign_grain(PivotDefinition *pivot,PivotRole role,const char *field,Aggregate aggregate,PivotDateGrain grain,char **error);
bool pivot_set_grain(PivotDefinition *pivot,PivotRole role,size_t index,PivotDateGrain grain,char **error);
/* Owned SQL key/visible label. Internal date aliases avoid every selected raw
   column name; the original source names and typed filter values stay intact. */
char *pivot_field_key(const PivotDefinition *pivot,PivotRole role,size_t index);
char *pivot_field_label(const PivotField *field);
void pivot_date_label(PivotDateGrain grain,const char *raw,char output[64]);
void pivot_remove(PivotDefinition *pivot,PivotRole role,size_t index);
bool pivot_move(PivotDefinition *pivot,PivotRole from,size_t index,PivotRole to,size_t destination,char **error);
bool pivot_valid(const PivotDefinition *pivot,char **error);
char *pivot_sql(const Analysis *source,const PivotDefinition *pivot);
char *pivot_input_sql(const Analysis *source,const PivotDefinition *pivot);
/* Source-derived row/column/grand aggregates. Ordinal output names and a
   grouping mask distinguish rolled-up dimensions from actual SQL NULL keys. */
char *pivot_totals_sql(const Analysis *source,const PivotDefinition *pivot);
/* IN lists are SQL literals built internally from bounded, escaped DuckDB domains. */
char *pivot_sql_domains(const Analysis *source,const PivotDefinition *pivot,char *const *domains);
#endif
