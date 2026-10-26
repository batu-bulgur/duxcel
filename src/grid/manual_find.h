#ifndef DUXCEL_GRID_MANUAL_FIND_H
#define DUXCEL_GRID_MANUAL_FIND_H
#include "data_boundary.h"
#include "../workbook/manual_value.h"
#define GRID_FIND_QUERY_BYTES 4096u
#define GRID_FIND_TEXT_BYTES DX_MANUAL_VALUE_TEXT_BYTES
typedef struct {
    uint64_t row; size_t column;
    bool present,searchable;
    const char *text; size_t bytes;
} GridFindCell;
typedef struct {
    /* Complete ordered sparse inventory, never a viewport. Synchronous,
       read-only/non-reentrant callback; owner and all spans stay pinned and
       immutable for the call. Searchable means explicit authored text (also
       numeric raw text), never a formatted display or inferred NULL spelling.
       Missing and SQL NULL have no text; present empty TEXT is searchable.
       No value/type/text pointers survive the call. */
    bool (*read)(const void *context,size_t index,GridFindCell *out);
    const void *context;
    size_t count,grant,text_grant;
} GridFindInventory;
typedef enum { GRID_FIND_NEXT,GRID_FIND_PREVIOUS } GridFindDirection;
typedef enum { GRID_FIND_CONTAINS,GRID_FIND_WHOLE_CELL } GridFindMode;
typedef struct { uint64_t row; size_t column,matches; bool wrapped; } GridFindResult;
typedef enum { GRID_FIND_FOUND,GRID_FIND_NOT_FOUND,GRID_FIND_INVALID,GRID_FIND_LIMIT,GRID_FIND_UNAVAILABLE } GridFindStatus;
/* Nonempty literal, case-sensitive UTF-8 query. Validate the entire inventory
   before publishing a result. Skip active, then wrap in row-major order; a sole
   active match returns the same coordinate with wrapped=true. Every non-result
   preserves out, including a late malformed/unavailable entry after a match.
   No allocation, query, model mutation, selection/history or locale authority. */
GridFindStatus grid_manual_find(const GridFindInventory *inventory,uint64_t row,size_t column,
    const char *query,size_t bytes,GridFindDirection direction,GridFindMode mode,GridFindResult *out);
#endif
