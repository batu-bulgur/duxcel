#ifndef DUXCEL_CELLS_H
#define DUXCEL_CELLS_H
#include "../common.h"
#define DX_MAX_EDIT_CELLS 20000
#define DX_CELL_BYTES (4*1024*1024)
typedef struct { uint64_t row; size_t col; char *text; } SheetCell;
typedef struct { SheetCell *cells; size_t count,bytes; } CellSnapshot;
typedef struct {
    SheetCell *cells;
    size_t count,capacity,bytes;
    uint64_t revision;
    CellSnapshot undo[32],redo[32];
    size_t undo_count,redo_count;
} CellStore;
void cells_free(CellStore *store);
void cells_copy(CellStore *destination,const CellStore *source); /* excludes history */
const char *cells_get(const CellStore *store,uint64_t row,size_t col);
bool cells_set(CellStore *store,uint64_t row,size_t col,const char *text,char **error);
/* Atomic TSV paste; quoted tabs/newlines round-trip through cells_tsv. One undo state. */
bool cells_paste(CellStore *store,uint64_t row,size_t col,const char *tsv,size_t *rows,size_t *columns,char **error);
typedef bool (*CellPasteGuard)(void *context,uint64_t row,size_t column,size_t rows,size_t columns,char **error);
bool cells_paste_checked(CellStore *store,uint64_t row,size_t col,const char *tsv,size_t *rows,size_t *columns,CellPasteGuard guard,void *context,char **error);
bool cells_clear(CellStore *store,uint64_t r0,uint64_t r1,size_t c0,size_t c1);
bool cells_move(CellStore *store,uint64_t r0,uint64_t r1,size_t c0,size_t c1,int64_t dr,int dc,char **error);
bool cells_column(CellStore *store,size_t col,bool insert,char **error);
bool cells_row(CellStore *store,uint64_t row,bool insert,char **error);
bool cells_rows(CellStore *store,uint64_t row,uint64_t count,bool insert,char **error);
bool cells_history(CellStore *store,bool redo);
void cells_trim_history(CellStore *store,bool redo,size_t keep);
void cells_region(const CellStore *store,uint64_t row,size_t col,uint64_t *r0,uint64_t *r1,size_t *c0,size_t *c1,bool whole);
char *cells_tsv(const CellStore *store,uint64_t r0,uint64_t r1,size_t c0,size_t c1,char **error);
#endif
