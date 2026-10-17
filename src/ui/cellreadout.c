#include "cellreadout.h"
#include <string.h>
static bool prefix(char *out,size_t capacity,const char *text) {
    size_t n=0;
    if (!text) { *out=0; return false; }
    while (n<capacity-1 && text[n]) n++;
    bool truncated=text[n]!=0;
    if (truncated) {
        n=capacity-4;
        while (n && ((unsigned char)text[n]&0xc0)==0x80) n--;
    }
    memcpy(out,text,n);
    if (truncated) memcpy(out+n,"…",4); else out[n]=0;
    return truncated;
}
void ui_cell_readout_set(UICellReadout *out,const Column *column,const Cell *cell) {
    UICellReadout next={0};
    if (column && cell && (cell->is_null || cell->text)) {
        next.valid=true; next.is_null=cell->is_null; next.numeric=column->numeric;
        next.number=cell->number; next.type=column->type;
        next.truncated=prefix(next.text,sizeof(next.text),cell->is_null ? NULL : cell->text);
        prefix(next.type_name,sizeof(next.type_name),column->type_name);
    }
    *out=next;
}
