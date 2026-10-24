#include "workbook/workbook_codec.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL workbook codec:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static size_t names(const char *list) {
    size_t count=0;
    for (const char *p=list;*p;) { count++; p+=strcspn(p,","); if (*p) p++; }
    return count;
}
static bool member(const char *list,const char *name,size_t bytes) {
    for (const char *p=list;*p;) {
        size_t n=strcspn(p,","); if (n==bytes && !memcmp(p,name,n)) return true;
        p+=n; if (*p) p++;
    }
    return false;
}
static void test_inventory(void) {
    static const char *const ordered[]={"workbook_identity","sheet_identity","meta","sheets","columns","cells","filters","sorts","queries","source_options",
        "table_sources","table_filters","table_sorts","objects","pivot_fields","marks","preferences","formats",
        "object_counters","format_scopes","format_selectors","format_state","format_guards","source_version_counters",
        "source_versions","source_bindings","format_records","format_fields"};
    CHECK(workbook_codec_table_count(15)==28);
    for (unsigned version=0;version<15;version++) CHECK(!workbook_codec_table_count(version) && !workbook_codec_table_at(version,0));
    CHECK(!workbook_codec_table_count(16) && !workbook_codec_table_count(UINT_MAX) && !workbook_codec_table_at(15,28) && !workbook_codec_table_at(15,SIZE_MAX));
    CHECK(!workbook_codec_table_find(NULL) && !workbook_codec_table_find("") && !workbook_codec_table_find("custom_note") && !workbook_codec_table_find("META"));
    size_t maximum=0;
    for (size_t i=0;i<sizeof(ordered)/sizeof(*ordered);i++) {
        const WorkbookCodecTable *table=workbook_codec_table_at(15,i);
        CHECK(table && !strcmp(table->table,ordered[i]) && table==workbook_codec_table_find(ordered[i]));
        CHECK(table->columns && *table->columns && table->types && *table->types && table->nullable_values);
        size_t columns=names(table->columns),types=0;
        for (const char *p=table->types;*p;p++) if (*p!=' ') { CHECK(strchr("IUDBTX",*p)); types++; }
        CHECK(columns==types && columns<=DX_WORKBOOK_CODEC_COLUMNS); if (columns>maximum) maximum=columns;
        for (const char *p=table->columns;*p;) {
            size_t n=strcspn(p,","); CHECK(n && n<64);
            for (const char *q=table->columns;q<p;) { size_t m=strcspn(q,","); CHECK(m!=n || memcmp(p,q,n)); q+=m; if (*q) q++; }
            bool nullable=member(table->nullable_values,p,n); CHECK(workbook_codec_nullable(table,p,n)==nullable);
            p+=n; if (*p) p++;
        }
        for (const char *p=table->nullable_values;*p;) {
            size_t n=strcspn(p,","); CHECK(member(table->columns,p,n)); p+=n; if (*p) p++;
        }
        for (size_t j=0;j<i;j++) CHECK(strcmp(table->table,workbook_codec_table_at(15,j)->table));
    }
    CHECK(maximum==DX_WORKBOOK_CODEC_COLUMNS);
    CHECK(workbook_codec_table_count(15)==28);
    CHECK(!strcmp(workbook_codec_table_find("workbook_identity")->types,"U U U U"));
    CHECK(!strcmp(workbook_codec_table_find("sheet_identity")->types,"I U"));
    const WorkbookCodecTable *sheets=workbook_codec_table_find("sheets"),*versions=workbook_codec_table_find("source_versions");
    CHECK(!strcmp(sheets->columns,DX_WORKBOOK_SHEET_COLUMNS ",canvas_zoom"));
    CHECK(!strcmp(workbook_codec_table_find("meta")->types,"I U D"));
    CHECK(!strcmp(workbook_codec_table_find("format_scopes")->types,"I I U I X"));
    CHECK(!strcmp(workbook_codec_table_find("format_state")->columns,"sheet_id,source_aware"));
    CHECK(!strcmp(versions->nullable_values,"definition"));
    CHECK(workbook_codec_nullable(versions,"definition",10) && !workbook_codec_nullable(versions,"encoding",8));
    CHECK(workbook_codec_nullable(sheets,"filter_value",12) && !workbook_codec_nullable(sheets,"path",4));
    CHECK(!workbook_codec_nullable(sheets,"filter",6) && !workbook_codec_nullable(sheets,"filter_value_extra",18));
    CHECK(!workbook_codec_nullable(NULL,"value",5) && !workbook_codec_nullable(sheets,NULL,1) && !workbook_codec_nullable(sheets,"",0));
}
static void test_selection(void) {
    unsigned out=777;
    CHECK(workbook_codec_select_write_version(15,0,&out) && out==15);
    CHECK(workbook_codec_select_write_version(15,15,&out) && out==15);
    for (unsigned version=0;version<15;version++) {
        out=777; CHECK(!workbook_codec_select_write_version(version,0,&out) && out==777);
        if (version) CHECK(!workbook_codec_select_write_version(15,(int)version,&out) && out==777);
    }
    out=777; CHECK(!workbook_codec_select_write_version(15,-1,&out) && out==777);
    CHECK(!workbook_codec_select_write_version(15,16,&out) && out==777);
    CHECK(!workbook_codec_select_write_version(15,INT_MAX,&out) && out==777);
    CHECK(!workbook_codec_select_write_version(15,0,NULL));
}
static void test_model_policy(void) {
    Worksheet sheets[2]={0}; Workbook book={.sheets=sheets,.count=2};
    Workbook before=book; CHECK(workbook_codec_min_write_version(&book)==15 && !memcmp(&before,&book,sizeof(book)));
    SheetObject object={.id=UINT64_MAX}; sheets[1].objects=(ObjectStore){.items=&object,.count=1,.next_id=UINT64_MAX};
    sheets[0].source_versions.high_water=UINT64_MAX; sheets[0].formats.source_aware=true;
    CHECK(workbook_codec_min_write_version(&book)==15);
    sheets[1].objects.count=0; CHECK(workbook_codec_min_write_version(&book)==15);
    CHECK(!workbook_codec_min_write_version(NULL)); Workbook invalid={0}; CHECK(!workbook_codec_min_write_version(&invalid));
    invalid.count=1; CHECK(!workbook_codec_min_write_version(&invalid)); invalid.sheets=sheets; invalid.count=DX_MAX_SHEETS+1; CHECK(!workbook_codec_min_write_version(&invalid));
    invalid.count=1; sheets[0].objects.count=DX_MAX_OBJECTS+1; CHECK(!workbook_codec_min_write_version(&invalid));
    sheets[0].objects.count=1; CHECK(!workbook_codec_min_write_version(&invalid));
}
int main(void) {
    test_inventory(); test_selection(); test_model_policy();
    printf("PASS: workbook codec %u checks (current15 inventory, strict write selection and unchanged model)\n",checks); return 0;
}
