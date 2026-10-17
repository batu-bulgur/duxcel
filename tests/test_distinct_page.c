#include "data/distinct_page.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL distinct page:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
static Page values_page(size_t rows) {
    const char *headers[]={"value","label","upper_bound"}; Page page={.column_count=3,.row_count=rows};
    page.columns=dx_alloc(3*sizeof(Column)); if (rows) page.cells=dx_alloc(rows*3*sizeof(Cell));
    for (size_t i=0;i<3;i++) page.columns[i]=(Column){.name=dx_strdup(headers[i]),.type_name=dx_strdup("VARCHAR"),.type=DUCKDB_TYPE_VARCHAR};
    for (size_t i=0;i<rows*3;i++) page.cells[i].is_null=true; return page;
}
static void cell(Page *page,size_t row,size_t column,const char *text) {
    Cell *cell=&page->cells[row*3+column]; free(cell->text); cell->text=text ? dx_strdup(text) : NULL; cell->is_null=text==NULL;
}
static DistinctPage retained(void) {
    DistinctPage page={.count=1,.bytes=99,.offset=128,.total=999}; page.values=dx_alloc(sizeof(*page.values));
    page.values[0]=(DistinctValue){.label=dx_strdup("Retained"),.value=dx_strdup("Keep"),.key=dx_strdup("Keep")}; return page;
}
static void reject(DistinctPage *out,const Page *page,uint64_t total) {
    DistinctPage before=*out; char *error=dx_strdup("Previous error");
    CHECK(!distinct_page_from_page(out,page,total,&error)); CHECK(error && *error && strcmp(error,"Previous error"));
    CHECK(!memcmp(&before,out,sizeof(before)) && !strcmp(out->values[0].label,"Retained") && !strcmp(out->values[0].key,"Keep")); free(error);
}
static size_t bytes(const DistinctPage *page) {
    size_t total=page->count*sizeof(DistinctValue);
    for (size_t i=0;i<page->count;i++) { const DistinctValue *v=&page->values[i]; total+=strlen(v->label)+1; if (v->value) total+=strlen(v->value)+1; if (v->upper) total+=strlen(v->upper)+1; if (v->key) total+=strlen(v->key)+1; }
    return total;
}
static void test_owned_values(void) {
    Page page=values_page(7); page.offset=128;
    cell(&page,1,0,""); cell(&page,1,1,""); cell(&page,2,0,"NULL"); cell(&page,2,1,"NULL");
    cell(&page,3,0,"2026-01-01"); cell(&page,3,1,"January"); cell(&page,3,2,"2026-02-01");
    cell(&page,4,0,"literal\ttab"); cell(&page,4,1,"literal\ttab");
    cell(&page,5,0,"\xc4\xb0stanbul \xf0\x9f\xa6\x86"); cell(&page,5,1,"\xc4\xb0stanbul \xf0\x9f\xa6\x86");
    cell(&page,6,0,"zero-length upper"); cell(&page,6,1,"Upper"); cell(&page,6,2,"");
    DistinctPage out=retained(); char *error=NULL;
    CHECK(distinct_page_from_page(&out,&page,200,&error)); CHECK(!error && out.offset==128 && out.total==200 && out.count==7 && out.bytes==bytes(&out));
    CHECK(out.values[0].is_null && !out.values[0].value && !out.values[0].key && !out.values[0].upper && !strcmp(out.values[0].label,"NULL"));
    CHECK(!out.values[1].is_null && out.values[1].value && !*out.values[1].value && out.values[1].key && !*out.values[1].key && !*out.values[1].label);
    CHECK(!out.values[2].is_null && !strcmp(out.values[2].value,"NULL") && !strcmp(out.values[2].key,"NULL"));
    CHECK(!strcmp(out.values[3].value,"2026-01-01") && !strcmp(out.values[3].label,"January") && !strcmp(out.values[3].key,"2026-01-01\t2026-02-01"));
    CHECK(!strcmp(out.values[4].key,"literal\ttab") && out.values[4].upper==NULL);
    CHECK(out.values[6].upper && !*out.values[6].upper && !strcmp(out.values[6].key,"zero-length upper\t"));
    CHECK(out.values[5].value!=page.cells[15].text && out.values[5].label!=page.cells[16].text);
    memset(page.cells[15].text,'x',strlen(page.cells[15].text)); page_free(&page);
    CHECK(!strcmp(out.values[5].value,"\xc4\xb0stanbul \xf0\x9f\xa6\x86")); distinct_free(&out); distinct_free(&out); CHECK(!out.values && !out.count && !out.bytes);
}
static void test_shape_and_counts(void) {
    DistinctPage out=retained(); Page page=values_page(1),bad=page;
    reject(&out,NULL,1); bad.column_count=2; reject(&out,&bad,1); bad=page; bad.row_count=SIZE_MAX; reject(&out,&bad,1);
    bad=page; bad.columns=NULL; reject(&out,&bad,1); bad=page; bad.cells=NULL; reject(&out,&bad,1);
    char *name=page.columns[1].name; page.columns[1].name="wrong"; reject(&out,&page,1); page.columns[1].name=name;
    page.columns[1].type=DUCKDB_TYPE_BIGINT; reject(&out,&page,1); page.columns[1].type=DUCKDB_TYPE_VARCHAR;
    page.offset=UINT64_MAX; reject(&out,&page,UINT64_MAX); page.offset=10; reject(&out,&page,10);
    cell(&page,0,1,"invalid NULL label"); reject(&out,&page,11); cell(&page,0,1,NULL);
    cell(&page,0,2,"invalid NULL upper"); reject(&out,&page,11); cell(&page,0,2,NULL);
    cell(&page,0,0,"Value"); reject(&out,&page,11); cell(&page,0,1,"Label");
    page.cells[0].is_null=true; reject(&out,&page,11); page.cells[0].is_null=false;
    CHECK(!distinct_page_from_page(NULL,&page,11,NULL)); CHECK(!distinct_page_from_page(&out,NULL,11,NULL));
    page_free(&page); page=values_page(129); reject(&out,&page,129); page_free(&page);
    page=values_page(0); page.offset=UINT64_MAX; CHECK(distinct_page_from_page(&out,&page,0,NULL));
    CHECK(!out.count && !out.values && !out.bytes && out.offset==UINT64_MAX && !out.total); distinct_free(&out); page_free(&page);
    page=values_page(128); page.offset=UINT64_MAX-128; CHECK(distinct_page_from_page(&out,&page,UINT64_MAX,NULL));
    CHECK(out.count==128 && out.values[127].is_null && out.bytes==bytes(&out)); distinct_free(&out); page_free(&page);
}
static void test_utf8_and_budget(void) {
    const char *invalid[]={"\x80","\xc0\x80","\xc2","\xe0\x80\x80","\xed\xa0\x80","\xf0\x80\x80\x80","\xf4\x90\x80\x80","\xf5\x80\x80\x80","\xe2\x82","\xff"};
    DistinctPage out=retained(); Page page=values_page(1); cell(&page,0,0,"Value"); cell(&page,0,1,"Label");
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) for (size_t c=0;c<3;c++) {
        char *old=page.cells[c].text; bool old_null=page.cells[c].is_null; page.cells[c].text=(char *)invalid[i]; page.cells[c].is_null=false;
        reject(&out,&page,1); page.cells[c].text=old; page.cells[c].is_null=old_null;
    }
    char *large=dx_alloc(DX_DISTINCT_PAGE_BYTES+2); memset(large,'x',DX_DISTINCT_PAGE_BYTES+1);
    cell(&page,0,0,large); reject(&out,&page,1);
    /* Output duplicates value into its key, so the budget includes both copies. */
    size_t exact=(DX_DISTINCT_PAGE_BYTES-sizeof(DistinctValue)-3)/2;
    large[exact]=0; cell(&page,0,0,large); cell(&page,0,1,"");
    CHECK(distinct_page_from_page(&out,&page,1,NULL)); CHECK(out.bytes==bytes(&out) && out.bytes<=DX_DISTINCT_PAGE_BYTES);
    distinct_free(&out); out=retained(); large[exact]='x'; large[exact+1]=0; cell(&page,0,0,large); reject(&out,&page,1);
    free(large); page_free(&page); distinct_free(&out);
}
int main(void) { test_owned_values(); test_shape_and_counts(); test_utf8_and_budget(); printf("PASS: %u distinct page checks\n",checks); return 0; }
