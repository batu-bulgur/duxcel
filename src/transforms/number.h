#ifndef DUXCEL_NUMBER_H
#define DUXCEL_NUMBER_H
#include "../common.h"
typedef enum { NUM_NONE,NUM_I8,NUM_I16,NUM_I32,NUM_I64,NUM_I128,NUM_U8,NUM_U16,NUM_U32,NUM_U64,NUM_U128,NUM_DECIMAL,NUM_FLOAT } NumberKind;
typedef struct { NumberKind kind; unsigned char precision,scale; } NumberType;
typedef enum { NUMBER_EQUAL,NUMBER_GREATER,NUMBER_LESS } NumberComparison;
/* Column/value_expression are already quoted SQL. Exact thresholds are quantized
   to the column's quantum, with directed rounding; no floating-point arithmetic. */
char *number_filter_sql(NumberType type,const char *column,NumberComparison op,const char *text,const char *value_expression);
/* Display-only decimal rounding/grouping from the original numeric text. Never
   round-trip DECIMAL/HUGEINT through double; callers retain raw copy/export. */
bool number_display(const char *raw,unsigned decimals,bool accounting,char output[512]);
#endif
