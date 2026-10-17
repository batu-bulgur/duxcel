#ifndef DUXCEL_CLIPBOARD_H
#define DUXCEL_CLIPBOARD_H
#include "../common.h"
#include "cells.h"
#include "objects.h"
#define DX_CLIPBOARD_CELLS 20000
#define DX_CLIPBOARD_BYTES (8*1024*1024)
/* NULL pointer = missing value. Owned "" = genuine zero-length text. */
/* capacity is the number of owned pointer slots, independent of logical shape.
   Shrinking rows does not relinquish those slots or their cleanup ownership. */
typedef struct { char **values; size_t rows,columns,bytes,capacity; } CellClipboard;
bool clipboard_init(CellClipboard *copy,size_t rows,size_t columns,char **error);
bool clipboard_set(CellClipboard *copy,size_t row,size_t column,const char *value,char **error);
char *clipboard_tsv(const CellClipboard *copy,char **error);
void clipboard_free(CellClipboard *copy);
/* Resolve whole-row/column sentinels against used content in the selected lanes. */
bool clipboard_bounds(const CellStore *cells,const ObjectStore *objects,uint64_t source_rows,size_t source_columns,
    uint64_t *first_row,uint64_t *last_row,size_t *first_column,size_t *last_column,char **error);
#endif
