#include "manual_find.h"
#include <string.h>
static bool text_valid(const char *text,size_t bytes) {
    ManualValueInfo info;
    return manual_value_measure((ManualValueInput){.present=true,.kind=MANUAL_VALUE_TEXT,.raw=text,.raw_bytes=bytes},&info)==MANUAL_VALUE_OK;
}
static int compare(uint64_t row,size_t column,uint64_t other_row,size_t other_column) {
    return row!=other_row ? (row<other_row ? -1 : 1) : column!=other_column ? (column<other_column ? -1 : 1) : 0;
}
static bool contains(const char *text,size_t bytes,const char *query,size_t query_bytes,const uint16_t *prefix) {
    size_t matched=0;
    for (size_t i=0;i<bytes;i++) {
        while (matched && text[i]!=query[matched]) matched=prefix[matched-1];
        if (text[i]==query[matched] && ++matched==query_bytes) return true;
    }
    return false;
}
GridFindStatus grid_manual_find(const GridFindInventory *o,uint64_t row,size_t column,const char *query,size_t bytes,
    GridFindDirection direction,GridFindMode mode,GridFindResult *out) {
    if (!o || !out || !query || !bytes || row>=DX_SHEET_ROWS || column>=GRID_BOUNDARY_COLUMNS || (unsigned)direction>GRID_FIND_PREVIOUS ||
        (unsigned)mode>GRID_FIND_WHOLE_CELL || (o->count && !o->read)) return GRID_FIND_INVALID;
    if (bytes>GRID_FIND_QUERY_BYTES || o->count>o->grant || o->grant>GRID_BOUNDARY_CELLS_MAX || o->text_grant>GRID_FIND_TEXT_BYTES) return GRID_FIND_LIMIT;
    if (!text_valid(query,bytes)) return GRID_FIND_INVALID;
    uint16_t prefix[GRID_FIND_QUERY_BYTES]; prefix[0]=0;
    for (size_t i=1,matched=0;i<bytes;i++) {
        while (matched && query[i]!=query[matched]) matched=prefix[matched-1];
        if (query[i]==query[matched]) matched++;
        prefix[i]=(uint16_t)matched;
    }
    GridFindCell previous={0}; GridFindResult first={0},last={0},chosen={0}; size_t total=0,text_bytes=0; bool found=false;
    for (size_t i=0;i<o->count;i++) {
        GridFindCell cell={0}; if (!o->read(o->context,i,&cell)) return GRID_FIND_UNAVAILABLE;
        if (cell.row>=DX_SHEET_ROWS || cell.column>=GRID_BOUNDARY_COLUMNS || (i && compare(cell.row,cell.column,previous.row,previous.column)<=0) ||
            (!cell.present && cell.searchable) || (!cell.searchable && (cell.text || cell.bytes)) || (cell.bytes && !cell.text)) return GRID_FIND_INVALID;
        previous=cell;
        if (cell.bytes>o->text_grant-text_bytes) return GRID_FIND_LIMIT;
        text_bytes+=cell.bytes;
        if (!cell.searchable) continue;
        if (!text_valid(cell.text,cell.bytes)) return GRID_FIND_INVALID;
        bool match=mode==GRID_FIND_WHOLE_CELL ? cell.bytes==bytes && !memcmp(cell.text,query,bytes) : contains(cell.text,cell.bytes,query,bytes,prefix);
        if (!match) continue;
        GridFindResult next={cell.row,cell.column,0,false}; if (!total) first=next; last=next; total++;
        int relation=compare(cell.row,cell.column,row,column);
        if (direction==GRID_FIND_NEXT ? relation>0 && !found : relation<0) { chosen=next; found=true; }
    }
    if (!total) return GRID_FIND_NOT_FOUND;
    if (!found) { chosen=direction==GRID_FIND_NEXT ? first : last; chosen.wrapped=true; }
    chosen.matches=total; *out=chosen; return GRID_FIND_FOUND;
}
