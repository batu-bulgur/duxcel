#include "clipboard.h"
#include <stdlib.h>
#include <string.h>
static bool failed(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
bool clipboard_bounds(const CellStore *cells,const ObjectStore *objects,uint64_t rows,size_t columns,uint64_t *r0,uint64_t *r1,size_t *c0,size_t *c1,char **error) {
    bool whole_rows=*c1==SIZE_MAX,whole_columns=*r1==UINT64_MAX;
    if (!whole_rows && !whole_columns) return true;
    if (whole_columns && columns>*c0 && rows==UINT64_MAX) return failed(error,"Wait for the row count before copying whole data columns, or select a bounded range.");
    uint64_t last_row=*r0; size_t last_column=*c0;
    if (columns>*c0 && rows && rows-1>last_row) last_row=rows-1;
    if (rows>*r0 && columns && columns-1>last_column) last_column=columns-1;
    for (size_t i=0;i<cells->count;i++) {
        const SheetCell *cell=&cells->cells[i];
        if (cell->col>=*c0 && cell->col<=*c1 && cell->row>last_row) last_row=cell->row;
        if (cell->row>=*r0 && cell->row<=*r1 && cell->col>last_column) last_column=cell->col;
    }
    for (size_t i=0;i<objects->count;i++) {
        const SheetObject *object=&objects->items[i];
        if (object->col<=*c1 && object->col+object->columns>*c0 && object->row+object->rows-1>last_row) last_row=object->row+object->rows-1;
        if (object->row<=*r1 && object->row+object->rows>*r0 && object->col+object->columns-1>last_column) last_column=object->col+object->columns-1;
    }
    if (whole_columns) *r1=last_row;
    if (whole_rows) *c1=last_column;
    return true;
}
void clipboard_free(CellClipboard *copy) {
    if (!copy) return;
    for (size_t i=0;copy->values && i<copy->capacity && i<DX_CLIPBOARD_CELLS;i++) free(copy->values[i]);
    free(copy->values); memset(copy,0,sizeof(*copy));
}
static bool shape_valid(const CellClipboard *copy) {
    return copy && copy->values && copy->capacity && copy->capacity<=DX_CLIPBOARD_CELLS && copy->rows && copy->columns &&
        copy->columns<=DX_CLIPBOARD_CELLS && copy->rows<=DX_CLIPBOARD_CELLS/copy->columns && copy->rows*copy->columns<=copy->capacity;
}
bool clipboard_init(CellClipboard *copy,size_t rows,size_t columns,char **error) {
    memset(copy,0,sizeof(*copy));
    if (!rows || !columns || columns>DX_CLIPBOARD_CELLS || rows>DX_CLIPBOARD_CELLS/columns)
        return failed(error,"Copy up to 20,000 cells at a time. Select a smaller range.");
    copy->rows=rows; copy->columns=columns; copy->capacity=rows*columns; copy->values=dx_alloc(copy->capacity*sizeof(*copy->values)); return true;
}
bool clipboard_set(CellClipboard *copy,size_t row,size_t column,const char *value,char **error) {
    if (!shape_valid(copy) || row>=copy->rows || column>=copy->columns) return failed(error,"The copied range changed. Select it again.");
    size_t index=row*copy->columns+column,old=copy->values[index] ? strlen(copy->values[index])+1 : 0,next=value ? strlen(value)+1 : 0;
    if (old>copy->bytes || next>DX_CLIPBOARD_BYTES || copy->bytes-old>DX_CLIPBOARD_BYTES-next)
        return failed(error,"Clipboard output exceeds 8 MiB. Select a smaller range.");
    char *owned=value ? dx_strdup(value) : NULL; free(copy->values[index]); copy->values[index]=owned; copy->bytes=copy->bytes-old+next; return true;
}
static bool text_shape(ClipboardTextView view) {
    return view.read && view.rows && view.columns && view.columns<=DX_CLIPBOARD_CELLS && view.rows<=DX_CLIPBOARD_CELLS/view.columns;
}
static bool text_bytes(size_t *size,size_t added) {
    if (added>DX_CLIPBOARD_BYTES || *size>DX_CLIPBOARD_BYTES-added) return false;
    *size+=added; return true;
}
bool clipboard_tsv_measure_view(ClipboardTextView view,size_t *bytes,char **error) {
    if (!bytes || !text_shape(view)) return failed(error,"Select a valid borrowed range to copy.");
    size_t count=view.rows*view.columns,size=count-1;
    for (size_t i=0;i<count;i++) {
        const char *s=view.read(view.context,i); if (!s) continue;
        bool quote=!*s;
        for (const char *p=s;*p;p++) {
            if (!text_bytes(&size,*p=='"' ? 2 : 1)) return failed(error,"Clipboard output exceeds 8 MiB. Select a smaller range.");
            if (strchr("\t\r\n\"",*p)) quote=true;
        }
        if (quote && !text_bytes(&size,2)) return failed(error,"Clipboard output exceeds 8 MiB. Select a smaller range.");
    }
    *bytes=size; return true;
}
static void text_encode(ClipboardTextView view,char *out) {
    for (size_t row=0;row<view.rows;row++) for (size_t col=0;col<view.columns;col++) {
        if (row || col) *out++=col ? '\t' : '\n';
        const char *s=view.read(view.context,row*view.columns+col); if (!s) continue;
        bool quote=!*s || strpbrk(s,"\t\r\n\"");
        if (quote) *out++='"';
        for (;*s;s++) { if (*s=='"') *out++='"'; *out++=*s; }
        if (quote) *out++='"';
    }
    *out=0;
}
static bool text_overlap(const void *a,size_t an,const void *b,size_t bn) {
    if (!an || !bn) return false;
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<an : x-y<bn;
}
bool clipboard_tsv_encode_view(ClipboardTextView view,char *arena,size_t capacity,size_t *written,char **error) {
    if (!arena || !written) return failed(error,"Choose a complete clipboard output arena.");
    if (text_overlap(arena,capacity,written,sizeof(*written))) return failed(error,"Clipboard output and length storage must be independent.");
    size_t bytes=0; if (!clipboard_tsv_measure_view(view,&bytes,error)) return false;
    if (capacity<=bytes) return failed(error,"The clipboard output arena is too small.");
    /* Reader/context traversal metadata remains immutable by caller contract.
       Check ALL returned spans before text_encode writes even the first field. */
    size_t count=view.rows*view.columns;
    for (size_t i=0;i<count;i++) {
        const char *text=view.read(view.context,i); if (!text) continue;
        size_t span=strlen(text)+1;
        if (text_overlap(arena,capacity,text,span) || text_overlap(written,sizeof(*written),text,span))
            return failed(error,"Clipboard output must not overlap borrowed text.");
    }
    text_encode(view,arena); *written=bytes; return true;
}
static const char *clipboard_text(const void *context,size_t index) { return ((const CellClipboard *)context)->values[index]; }
char *clipboard_tsv(const CellClipboard *copy,char **error) {
    if (!shape_valid(copy)) { failed(error,"Select a valid owned range to copy."); return NULL; }
    ClipboardTextView view={copy->rows,copy->columns,clipboard_text,copy}; size_t bytes=0;
    if (!clipboard_tsv_measure_view(view,&bytes,error)) return NULL;
    char *text=dx_alloc(bytes+1); text_encode(view,text); return text;
}
