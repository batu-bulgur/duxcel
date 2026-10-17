#include "distinct_page.h"
#include <stdlib.h>
#include <string.h>

static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static bool text_size(const char *text,size_t limit,size_t *size) {
    if (!text) return false;
    size_t length=0; while (length<=limit && text[length]) length++;
    if (length>limit) return false;
    if (size) *size=length; return true;
}
static bool utf8(const char *text,size_t size) {
    const unsigned char *bytes=(const unsigned char *)text;
    for (size_t at=0;at<size;) {
        unsigned char first=bytes[at++]; if (first<0x80) continue;
        unsigned count; uint32_t value,minimum;
        if (first>=0xc2 && first<=0xdf) { count=1; value=first&0x1f; minimum=0x80; }
        else if (first>=0xe0 && first<=0xef) { count=2; value=first&0x0f; minimum=0x800; }
        else if (first>=0xf0 && first<=0xf4) { count=3; value=first&7; minimum=0x10000; }
        else return false;
        if (count>size-at) return false;
        while (count--) { unsigned char next=bytes[at++]; if ((next&0xc0)!=0x80) return false; value=(value<<6)|(next&0x3f); }
        if (value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return false;
    }
    return true;
}
static bool add_bytes(size_t *bytes,size_t count) {
    if (count>DX_DISTINCT_PAGE_BYTES-*bytes) return false;
    *bytes+=count; return true;
}
bool distinct_page_from_page(DistinctPage *out,const Page *values,uint64_t exact_total,char **error) {
    static const char *headers[]={"value","label","upper_bound"};
    if (!out || !values || values->column_count!=3 || values->row_count>DX_DISTINCT_ROWS || !values->columns ||
        (values->row_count && !values->cells)) return fail(error,"Expected a three-column available-value page of up to 128 rows.");
    if (values->row_count>UINT64_MAX-values->offset || (values->row_count && exact_total<values->offset+values->row_count))
        return fail(error,"Available-value count is inconsistent with the page. Refresh its search.");
    for (size_t i=0;i<3;i++) if (values->columns[i].type!=DUCKDB_TYPE_VARCHAR || !text_size(values->columns[i].name,16,NULL) || strcmp(values->columns[i].name,headers[i]))
        return fail(error,"Unexpected available-value page columns.");
    size_t lengths[DX_DISTINCT_ROWS][3],bytes=values->row_count*sizeof(DistinctValue);
    for (size_t row=0;row<values->row_count;row++) {
        const Cell *cells=&values->cells[row*3];
        for (size_t col=0;col<3;col++) {
            lengths[row][col]=0;
            if (cells[col].is_null ? cells[col].text!=NULL : !text_size(cells[col].text,DX_DISTINCT_PAGE_BYTES,&lengths[row][col]) || !utf8(cells[col].text,lengths[row][col]))
                return fail(error,"Invalid available-value NULL marker or UTF-8 text.");
        }
        if (cells[0].is_null) {
            if (!cells[1].is_null || !cells[2].is_null) return fail(error,"Invalid SQL NULL available-value row.");
            if (!add_bytes(&bytes,5)) return fail(error,"Available values exceed the 4 MiB page limit. Narrow the search.");
        } else {
            if (cells[1].is_null) return fail(error,"A non-NULL available value needs a label.");
            size_t value=lengths[row][0],label=lengths[row][1],upper=lengths[row][2];
            if (!add_bytes(&bytes,value+1) || !add_bytes(&bytes,label+1) ||
                (!cells[2].is_null && !add_bytes(&bytes,upper+1)) || !add_bytes(&bytes,value+1) ||
                (!cells[2].is_null && !add_bytes(&bytes,upper+1))) return fail(error,"Available values exceed the 4 MiB page limit. Narrow the search.");
        }
    }
    DistinctPage page={.count=values->row_count,.bytes=bytes,.offset=values->offset,.total=exact_total};
    if (page.count) page.values=dx_alloc(page.count*sizeof(*page.values));
    for (size_t row=0;row<page.count;row++) {
        const Cell *cells=&values->cells[row*3]; DistinctValue *value=&page.values[row]; value->is_null=cells[0].is_null;
        value->label=dx_strdup(value->is_null ? "NULL" : cells[1].text);
        if (!value->is_null) {
            value->value=dx_strdup(cells[0].text); value->upper=cells[2].is_null ? NULL : dx_strdup(cells[2].text);
            value->key=value->upper ? dx_printf("%s\t%s",value->value,value->upper) : dx_strdup(value->value);
        }
    }
    distinct_free(out); *out=page; return true;
}
