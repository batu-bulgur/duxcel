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
    static const char *const ordered[]={"meta","sheets","columns","cells","filters","sorts","queries","source_options",
        "table_sources","table_filters","table_sorts","objects","pivot_fields","marks","preferences","formats",
        "object_counters","format_scopes","format_selectors","format_state","format_guards","source_version_counters",
        "source_versions","source_bindings","format_records","format_fields"};
    CHECK(workbook_codec_table_count(12)==16 && workbook_codec_table_count(13)==20 && workbook_codec_table_count(14)==26);
    CHECK(!workbook_codec_table_count(0) && !workbook_codec_table_count(1) && !workbook_codec_table_count(11) && !workbook_codec_table_count(15) && !workbook_codec_table_count(UINT_MAX));
    CHECK(!workbook_codec_table_at(0,0) && !workbook_codec_table_at(11,0) && !workbook_codec_table_at(15,0));
    CHECK(!workbook_codec_table_at(14,26) && !workbook_codec_table_at(12,16) && !workbook_codec_table_at(13,20) && !workbook_codec_table_at(14,SIZE_MAX));
    CHECK(!workbook_codec_table_find(NULL) && !workbook_codec_table_find("") && !workbook_codec_table_find("custom_note") && !workbook_codec_table_find("META"));
    size_t maximum=0;
    for (size_t i=0;i<sizeof(ordered)/sizeof(*ordered);i++) {
        const WorkbookCodecTable *table=workbook_codec_table_at(14,i);
        CHECK(table && !strcmp(table->table,ordered[i]) && table==workbook_codec_table_find(ordered[i]));
        CHECK(table->columns && *table->columns && table->types && *table->types && table->nullable_values);
        CHECK(table->min_package>=12 && table->min_package<=14);
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
        for (size_t j=0;j<i;j++) CHECK(strcmp(table->table,workbook_codec_table_at(14,j)->table));
        for (unsigned v=1;v<=14;v++) CHECK(workbook_codec_reserved_before(table->table,v)==(table->min_package>=13 && table->min_package>v));
    }
    CHECK(maximum==DX_WORKBOOK_CODEC_COLUMNS);
    for (unsigned package=12;package<=14;package++) {
        const WorkbookCodecTable *last=NULL;
        for (size_t i=0;i<workbook_codec_table_count(package);i++) {
            const WorkbookCodecTable *table=workbook_codec_table_at(package,i);
            CHECK(table && table->min_package<=package && (!last || table!=last)); last=table;
        }
    }
    const WorkbookCodecTable *sheets=workbook_codec_table_find("sheets"),*versions=workbook_codec_table_find("source_versions");
    CHECK(!strcmp(sheets->columns,DX_WORKBOOK_SHEET_COLUMNS ",canvas_zoom"));
    CHECK(!strcmp(workbook_codec_table_find("meta")->types,"I U D"));
    CHECK(!strcmp(workbook_codec_table_find("format_scopes")->types,"I I U I X"));
    CHECK(!strcmp(workbook_codec_table_find("format_state")->columns,"sheet_id,source_aware") && workbook_codec_table_find("format_state")->min_package==14);
    CHECK(!strcmp(versions->nullable_values,"definition"));
    CHECK(workbook_codec_nullable(versions,"definition",10) && !workbook_codec_nullable(versions,"encoding",8));
    CHECK(workbook_codec_nullable(sheets,"filter_value",12) && !workbook_codec_nullable(sheets,"path",4));
    CHECK(!workbook_codec_nullable(sheets,"filter",6) && !workbook_codec_nullable(sheets,"filter_value_extra",18));
    CHECK(!workbook_codec_nullable(NULL,"value",5) && !workbook_codec_nullable(sheets,NULL,1) && !workbook_codec_nullable(sheets,"",0));
    CHECK(!workbook_codec_reserved_before("custom_note",1) && !workbook_codec_reserved_before("format_state",0) && !workbook_codec_reserved_before("format_state",15));
    CHECK(!workbook_codec_reserved_before(NULL,1));
}
static void test_selection(void) {
    for (unsigned minimum=12;minimum<=14;minimum++) for (int existing=0;existing<=14;existing++) {
        unsigned out=999; CHECK(workbook_codec_select_write_version(minimum,existing,&out));
        CHECK(out==(minimum>(unsigned)existing ? minimum : (unsigned)existing));
    }
    const unsigned invalid[]={0,1,11,15,UINT_MAX};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        unsigned out=777; CHECK(!workbook_codec_select_write_version(invalid[i],0,&out) && out==777);
    }
    unsigned out=777; CHECK(!workbook_codec_select_write_version(12,-1,&out) && out==777);
    CHECK(!workbook_codec_select_write_version(12,15,&out) && out==777);
    CHECK(!workbook_codec_select_write_version(12,INT_MAX,&out) && out==777);
    CHECK(!workbook_codec_select_write_version(12,0,NULL));
}
static void minimum(const Workbook *book,unsigned expected) {
    Workbook before=*book; Worksheet *saved=malloc(book->count*sizeof(*saved)); CHECK(saved);
    memcpy(saved,book->sheets,book->count*sizeof(*saved));
    CHECK(workbook_codec_min_write_version(book)==expected);
    CHECK(!memcmp(book,&before,sizeof(before)) && !memcmp(book->sheets,saved,book->count*sizeof(*saved))); free(saved);
}
static void test_model_policy(void) {
    Worksheet *sheets=calloc(2,sizeof(*sheets)); CHECK(sheets);
    Workbook book={.sheets=sheets,.count=2}; minimum(&book,12);
    SheetObject objects[2]={{.id=UINT64_C(9007199254740993)},{.id=UINT64_C(7)}};
    sheets[1].objects=(ObjectStore){.items=objects,.count=2,.next_id=objects[0].id}; minimum(&book,12);
    sheets[1].objects.next_id++; minimum(&book,13);
    sheets[1].objects.next_id=UINT64_MAX; minimum(&book,13);
    objects[0].id=UINT64_MAX; minimum(&book,12); /* No conversion through double. */
    sheets[1].objects.count=0; minimum(&book,13); /* Retired final owner stays retired. */
    sheets[1].objects=(ObjectStore){0}; minimum(&book,12);
    sheets[0].formats.source_aware=true; minimum(&book,14); sheets[0].formats.source_aware=false;
    sheets[0].formats.scope_count=1; minimum(&book,14); sheets[0].formats.scope_count=0;
    SourceVersions *versions=&sheets[0].source_versions;
    versions->high_water=UINT64_MAX; minimum(&book,14); versions->high_water=0;
    versions->capacity=1; minimum(&book,14); versions->capacity=0;
    versions->bytes=sizeof(SourceVersion); minimum(&book,14); versions->bytes=0;
    SourceVersion entry={.owner=0,.version=UINT64_C(9007199254740994),.kind=SOURCE_CSV,.path="source.csv"};
    versions->items=&entry; minimum(&book,14);
    versions->count=versions->capacity=1; versions->high_water=entry.version; versions->bytes=sizeof(entry)+11; minimum(&book,14); /* pending */
    unsigned char packet[]={0,1,0,255}; entry.raw=(SourcePacket){packet,sizeof(packet)}; versions->bytes+=sizeof(packet); minimum(&book,14); /* complete storage, not proof validation */
    *versions=(SourceVersions){0};
    sheets[0].analysis.source_version=1; minimum(&book,14); sheets[0].analysis.source_version=0;
    sheets[0].undo_count=1; sheets[0].undo[0].source_version=1; minimum(&book,14); sheets[0].undo[0].source_version=0; minimum(&book,12);
    sheets[0].redo_count=1; sheets[0].redo[0].source_version=UINT64_MAX; minimum(&book,14); sheets[0].redo[0].source_version=0; minimum(&book,12);
    sheets[0].undo_count=sheets[0].redo_count=0;
    sheets[1].objects=(ObjectStore){.items=objects,.count=2,.next_id=UINT64_MAX}; objects[0].kind=OBJECT_TABLE;
    objects[0].source.source_version=UINT64_MAX; minimum(&book,14); objects[0].source.source_version=0; minimum(&book,12);
    SheetObject historical={.id=objects[0].id,.kind=OBJECT_TABLE};
    for (size_t redo=0;redo<2;redo++) {
        ObjectStore *store=&sheets[1].objects; ObjectSnapshot *stack=redo ? store->redo : store->undo;
        if (redo) store->redo_count=1; else store->undo_count=1;
        stack[0]=(ObjectSnapshot){&historical,1}; minimum(&book,12);
        historical.source.source_version=UINT64_MAX; minimum(&book,14); historical.source.source_version=0;
        stack[0].count=DX_MAX_OBJECTS+1; minimum(&book,14); stack[0].count=1;
        stack[0].items=NULL; minimum(&book,14); stack[0]=(ObjectSnapshot){0};
        if (redo) store->redo_count=0; else store->undo_count=0;
    }
    for (size_t kind=0;kind<4;kind++) {
        if (kind==0) sheets[0].undo_count=DX_HISTORY+1;
        if (kind==1) sheets[0].redo_count=SIZE_MAX;
        if (kind==2) sheets[0].objects.undo_count=DX_HISTORY+1;
        if (kind==3) sheets[0].objects.redo_count=SIZE_MAX;
        minimum(&book,14); sheets[0].undo_count=sheets[0].redo_count=sheets[0].objects.undo_count=sheets[0].objects.redo_count=0;
    }
    /* Category selection is not full validation: an unreferenced ledger or
       malformed historical packet still selects14 for the native validator. */
    sheets[0].analysis.source_version=3; sheets[1].objects.next_id=0; minimum(&book,14);
    free(sheets);
    Workbook invalid={0}; CHECK(!workbook_codec_min_write_version(NULL) && !workbook_codec_min_write_version(&invalid));
    invalid=(Workbook){.count=1}; CHECK(!workbook_codec_min_write_version(&invalid));
    Worksheet one={0}; invalid=(Workbook){.sheets=&one,.count=DX_MAX_SHEETS+1}; CHECK(!workbook_codec_min_write_version(&invalid));
    invalid.count=1; one.objects.count=DX_MAX_OBJECTS+1; CHECK(!workbook_codec_min_write_version(&invalid));
    one.objects.count=1; CHECK(!workbook_codec_min_write_version(&invalid));
}
int main(void) {
    test_inventory(); test_selection(); test_model_policy();
    printf("PASS: workbook codec %u checks (immutable inventory, legacy policy boundaries, exact version selection and unchanged model)\n",checks); return 0;
}
