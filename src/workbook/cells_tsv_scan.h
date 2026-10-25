#ifndef DUXCEL_CELLS_TSV_SCAN_H
#define DUXCEL_CELLS_TSV_SCAN_H
#include "cells.h"
/* Shared bounded TSV grammar. Fields borrow one reusable stack buffer only for
   the visitor call. Empty fields are present coordinates; ragged row holes are
   absent. One trailing line ending adds no row. Output shape changes only after
   complete success. Visitor side effects are caller-owned on later failure. */
typedef bool (*CellTSVVisitor)(void *context,uint64_t row,size_t column,const char *text,size_t bytes,char **error);
bool cells_tsv_scan(uint64_t row,size_t column,const char *text,CellTSVVisitor visitor,void *context,size_t *rows,size_t *columns,char **error);
#endif
