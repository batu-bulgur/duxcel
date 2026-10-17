#ifndef DUXCEL_CELLREADOUT_H
#define DUXCEL_CELLREADOUT_H
#include "../data/database.h"
/* Presentation-only prefix of one active value. This owns every byte it uses;
   paging can release the original Cell/Column immediately. Copy/inspection
   and calculations continue to use the original data, never this prefix. */
typedef struct {
    char text[1024],type_name[64];
    double number;
    duckdb_type type;
    bool valid,is_null,numeric,truncated;
} UICellReadout;
void ui_cell_readout_set(UICellReadout *out,const Column *column,const Cell *cell);
#endif
