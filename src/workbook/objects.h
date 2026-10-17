#ifndef DUXCEL_OBJECTS_H
#define DUXCEL_OBJECTS_H
#include "../transforms/pivot.h"
#include "../transforms/marks.h"
#include "cells.h"
typedef enum { OBJECT_PIVOT,OBJECT_CHART,OBJECT_TABLE } ObjectKind;
typedef enum { MARK_BAR,MARK_LINE,MARK_DOT,MARK_SCATTER } ChartMark;
typedef struct {
    uint64_t id,row,rows,revision;
    size_t col,columns;
    char *name;
    ObjectKind kind;
    ChartMark mark;
    unsigned char chart_measure; /* Presentation index; never a query revision. */
    PivotDefinition pivot;
    Analysis source; /* owned by a table; analytical objects bind through source_id */
    MarkDefinition marks; /* owned presentation bindings, retained across representation changes */
    uint64_t source_id; /* 0 = worksheet primary source, otherwise a table ID */
} SheetObject;
typedef struct { SheetObject *items; size_t count; } ObjectSnapshot;
typedef struct {
    SheetObject *items; size_t count; uint64_t next_id,title_revision,marks_revision;
    ObjectSnapshot undo[32],redo[32]; size_t undo_count,redo_count;
} ObjectStore;
typedef struct { uint64_t r0,r1; size_t c0,c1; } SheetRange;
SheetRange objects_region(const ObjectStore *objects,const CellStore *cells,uint64_t source_rows,size_t source_columns,uint64_t row,size_t column,bool whole);
void objects_free(ObjectStore *store);
void objects_copy(ObjectStore *to,const ObjectStore *from);
bool objects_valid(const ObjectStore *store,char **error);
int objects_add(ObjectStore *store,uint64_t row,size_t col,const char *name,char **error);
int objects_add_chart(ObjectStore *store,uint64_t row,size_t col,const char *name,const PivotDefinition *definition,char **error);
int objects_add_table(ObjectStore *store,uint64_t row,size_t col,const char *name,const Analysis *source,char **error);
const SheetObject *objects_source(const ObjectStore *store,const SheetObject *object);
bool objects_bind_source(ObjectStore *store,size_t index,uint64_t source_id,char **error);
bool objects_set_source(ObjectStore *store,size_t index,const Analysis *source,char **error);
/* Pin a worker-resolved default relation without a new edit or query revision. */
bool objects_resolve_source(ObjectStore *store,size_t index,const Analysis *resolved,char **error);
bool objects_update(ObjectStore *store,size_t index,const PivotDefinition *definition,char **error);
/* Presentation-only change: retain query revision and cached result. */
bool objects_mark(ObjectStore *store,size_t index,ChartMark mark,char **error);
bool objects_chart_measure(ObjectStore *store,size_t index,size_t measure,char **error);
bool objects_marks(ObjectStore *store,size_t index,const MarkDefinition *marks,char **error);
/* Switch representation of the same bound result, without a query revision.
   Caller supplies the cached matrix extent or desired chart extent. */
bool objects_representation(ObjectStore *store,size_t index,ObjectKind kind,ChartMark mark,
    uint64_t rows,size_t columns,const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error);
bool objects_rename(ObjectStore *store,size_t index,const char *name,char **error);
bool objects_remove(ObjectStore *store,size_t index);
bool objects_history(ObjectStore *store,bool redo);
void objects_trim_history(ObjectStore *store,bool redo,size_t keep);
/* Caller validates the complete destination first. Preserves result revisions. */
void objects_translate_mask(ObjectStore *store,uint32_t mask,int64_t dr,int dc);
int objects_hit(const ObjectStore *store,uint64_t row,size_t col);
bool objects_fits(const ObjectStore *store,int ignore,uint64_t row,size_t col,uint64_t rows,size_t columns,
    const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error);
bool objects_move(ObjectStore *store,size_t index,uint64_t row,size_t col,const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error);
/* Chart presentation only: 8–4096 rows, 3–256 columns; preserve query revision. */
bool objects_resize_chart(ObjectStore *store,size_t index,uint64_t rows,size_t columns,const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error);
#endif
