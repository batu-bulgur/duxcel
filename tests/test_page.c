#include "data/page.h"
#ifdef DUXCEL_PAGE_ENGINE_TEST
#include "data/database.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,releases_a,releases_b;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL page:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
typedef struct { uint64_t marker; char text[]; } AllocationA;
typedef struct { uint64_t padding[3],marker; char text[]; } AllocationB;
static char *text_a(const char *text) {
    AllocationA *allocation=dx_alloc(sizeof(*allocation)+strlen(text)+1);
    allocation->marker=UINT64_C(0x3174e980cc27ad61); strcpy(allocation->text,text); return allocation->text;
}
static char *text_b(const char *text) {
    AllocationB *allocation=dx_alloc(sizeof(*allocation)+strlen(text)+1);
    allocation->marker=UINT64_C(0x51b48d2039e6fa72); strcpy(allocation->text,text); return allocation->text;
}
static void release_a(void *text) {
    CHECK(text!=NULL);
    AllocationA *allocation=(AllocationA *)((char *)text-sizeof(*allocation));
    CHECK(allocation->marker==UINT64_C(0x3174e980cc27ad61));
    releases_a++; free(allocation);
}
static void release_b(void *text) {
    CHECK(text!=NULL);
    AllocationB *allocation=(AllocationB *)((char *)text-sizeof(*allocation));
    CHECK(allocation->marker==UINT64_C(0x51b48d2039e6fa72));
    releases_b++; free(allocation);
}
static Page owned_page(size_t rows,void (*release)(void *)) {
    Page page={0}; page.column_count=1; page.row_count=rows; page.total=rows;
    page.columns=dx_alloc(sizeof(*page.columns)); page.cells=dx_alloc(rows*sizeof(*page.cells));
    page.columns[0]=(Column){.name=dx_strdup("Owned value"),.type_name=dx_strdup("VARCHAR"),.type=DUCKDB_TYPE_VARCHAR};
    page.release_cell_text=release; return page;
}
static void test_moved_and_nested_ownership(void) {
    const char *exact="-12345678901234567890.123456789012345678";
    Page page=owned_page(5,release_a);
    page.cells[0].is_null=true;
    page.cells[1].text=text_a(""); page.cells[2].text=text_a("NULL");
    page.cells[3].text=text_a(exact); page.cells[4].text=text_a("9007199254740993");
    page.columns[0].caption=dx_strdup("Category"); page.columns[0].detail=dx_strdup("Measure");
    page.columns[0].category_count=3; page.columns[0].categories=dx_alloc(3*sizeof(char *));
    page.columns[0].categories[1]=dx_strdup(""); page.columns[0].categories[2]=dx_strdup("NULL");
    page.relation_count=1; page.relations=dx_alloc(sizeof(*page.relations));
    page.relations[0]=(Relation){dx_strdup("main"),dx_strdup("source"),dx_strdup("main.source")};
    page.sql=dx_strdup("SELECT exact_value FROM source");
    page.totals=dx_alloc(sizeof(*page.totals));
    page.totals->rows=owned_page(1,release_b); page.totals->rows.cells[0].text=text_b(exact);
    page.totals->columns=owned_page(1,NULL); page.totals->columns.cells[0].text=dx_strdup("18446744073709551615");
    page.totals->has_columns=true;
    page.records.file_identity=(SourceFileIdentity){.device=1,.inode=UINT64_MAX,.size=9007199254740993,.changed_seconds=17}; page.records.file_identity_valid=true;
    page.totals->rows.records.file_identity=(SourceFileIdentity){.device=2,.inode=3}; page.totals->rows.records.file_identity_valid=true;
    CHECK(page.cells[0].is_null && !page.cells[0].text);
    CHECK(!page.cells[1].is_null && page.cells[1].text && !*page.cells[1].text);
    CHECK(!page.cells[2].is_null && !strcmp(page.cells[2].text,"NULL"));
    CHECK(!strcmp(page.cells[3].text,exact) && !strcmp(page.cells[4].text,"9007199254740993"));
    CHECK(!page.columns[0].categories[0] && !*page.columns[0].categories[1] && !strcmp(page.columns[0].categories[2],"NULL"));
    Page moved=page; page=(Page){0}; page_free(&page);
    CHECK(!releases_a && !releases_b && moved.release_cell_text==release_a);
    CHECK(moved.records.file_identity_valid && moved.records.file_identity.inode==UINT64_MAX && moved.records.file_identity.size==UINT64_C(9007199254740993) && moved.records.file_identity.changed_seconds==17);
    CHECK(moved.totals->rows.records.file_identity_valid && moved.totals->rows.records.file_identity.device==2 && moved.totals->rows.records.file_identity.inode==3);
    page_free(&moved);
    CHECK(releases_a==4 && releases_b==1);
    CHECK(!moved.cells && !moved.columns && !moved.totals && !moved.release_cell_text && !moved.row_count && !moved.column_count);
    CHECK(!moved.records.file_identity_valid && !moved.records.file_identity.inode && !moved.records.file_identity.size);
    page_free(&moved); CHECK(releases_a==4 && releases_b==1);
}
static void test_default_and_partial_pages(void) {
    Page page=owned_page(2,NULL); page.cells[0].text=dx_strdup(""); page.cells[1].text=dx_strdup("NULL");
    page_free(&page); CHECK(!page.cells && !page.release_cell_text);
    page=owned_page(3,release_a); page.cells[0].text=text_a("Partially filled");
    page_free(&page); CHECK(releases_a==5);
    page=(Page){.release_cell_text=release_b}; page_free(&page); CHECK(releases_b==1 && !page.release_cell_text);
}
static void test_record_publication_identity(void) {
    Page previous={0},next={0},bare={.offset=UINT64_MAX,.row_count=DX_PAGE_ROWS,.column_count=16384};
    previous.records.file_identity_valid=next.records.file_identity_valid=true;
    memset(&previous.records.file_identity,0xa5,sizeof(SourceFileIdentity)); memset(&next.records.file_identity,0x5a,sizeof(SourceFileIdentity));
#define IDENTITY_FIELDS(X) X(device) X(inode) X(size) X(links) X(mode) X(uid) X(gid) X(flags) X(generation) X(modified_seconds) X(modified_nanoseconds) X(changed_seconds) X(changed_nanoseconds) X(created_seconds) X(created_nanoseconds)
#define ZERO_FIELD(field) previous.records.file_identity.field=next.records.file_identity.field=0;
    IDENTITY_FIELDS(ZERO_FIELD)
#undef ZERO_FIELD
    CHECK(!page_records_changed(&previous,&next)); /* Scalar equality, different padding. */
#define CHANGE_FIELD(field) do { next.records.file_identity.field++; CHECK(page_records_changed(&previous,&next) && page_records_changed(&next,&previous)); next.records.file_identity.field--; } while (0);
    IDENTITY_FIELDS(CHANGE_FIELD)
#undef CHANGE_FIELD
#undef IDENTITY_FIELDS
    CHECK(!page_records_changed(NULL,&next) && !page_records_changed(&previous,NULL));
    CHECK(!page_records_changed(&previous,&bare));
    next.records.file_identity_valid=false;
    CHECK(!page_records_changed(&previous,&next)); /* No remaining raw provenance. */
    next.records.stamp=1; CHECK(page_records_changed(&previous,&next)); next.records.stamp=0;
    next.records.source_key="raw-source"; CHECK(page_records_changed(&previous,&next)); next.records.source_key=NULL;
    uint64_t id=1; next.records.ids=&id; CHECK(page_records_changed(&previous,&next)); next.records.ids=NULL;
    next.records.count=1; CHECK(page_records_changed(&previous,&next)); next.records.count=0;
    previous.records.file_identity_valid=false;
    previous.records.source_key=next.records.source_key="demo-record-v1"; previous.records.stamp=next.records.stamp=0;
    CHECK(!page_records_changed(&previous,&next));
    next.records.file_identity_valid=true; CHECK(!page_records_changed(&previous,&next)); /* Newly available metadata is not a reported change. */
    next.records.file_identity_valid=false; next.records.source_key="different-reader"; CHECK(page_records_changed(&previous,&next));
}
static void test_distinct_and_statistics(void) {
    DistinctPage page={.count=3,.offset=128,.total=1000}; page.values=dx_alloc(page.count*sizeof(*page.values));
    page.values[0]=(DistinctValue){.label=dx_strdup("NULL"),.is_null=true};
    page.values[1]=(DistinctValue){dx_strdup(""),dx_strdup(""),NULL,dx_strdup(""),false};
    page.values[2]=(DistinctValue){dx_strdup("NULL"),dx_strdup("NULL"),dx_strdup("Upper"),dx_strdup("NULL"),false};
    CHECK(page.values[0].is_null && !page.values[0].key);
    CHECK(!page.values[1].is_null && page.values[1].key && !*page.values[1].key);
    CHECK(!page.values[2].is_null && !strcmp(page.values[2].key,"NULL"));
    distinct_free(&page); CHECK(!page.values && !page.count && !page.offset && !page.total); distinct_free(&page);
    SelectionStats statistics={.sum=dx_strdup("18446744073709551615"),.average=dx_strdup("0.125"),.minimum=dx_strdup("-0.125"),.maximum=dx_strdup("18446744073709551615"),.numeric=2};
    CHECK(!strcmp(statistics.sum,"18446744073709551615"));
    statistics_free(&statistics); CHECK(!statistics.sum && !statistics.average && !statistics.minimum && !statistics.maximum && !statistics.numeric); statistics_free(&statistics);
}
static void test_request_ownership(void) {
    char *error=NULL; Analysis source={.kind=SOURCE_PARQUET,.path=dx_strdup("logical-source.parquet")};
    CHECK(analysis_filter_set(&source,"Amount",FILTER_EQUAL,"9007199254740993",&error));
    StatisticsRequest *request=dx_alloc(sizeof(*request));
    CHECK(source_snapshot_bind(&request->sources,9,&source,&error)==1);
    CHECK(request->sources.items[0].path!=source.path && request->sources.items[0].filter_value!=source.filter_value);
    CHECK(source_snapshot_bind(&request->sources,9,&source,&error)==1 && request->sources.count==1);
    analysis_free(&source);
    CHECK(!strcmp(request->sources.items[0].path,"logical-source.parquet") && !strcmp(request->sources.items[0].filter_value,"9007199254740993"));
    request->count=1; CHECK(pivot_assign(&request->ranges[0].pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(statistics_request_text(request,NULL,&error) && !request->text_count);
    char *temporary=dx_strdup(""); CHECK(statistics_request_text(request,temporary,&error)); free(temporary);
    CHECK(statistics_request_text(request,"NULL",&error) && request->text_count==2);
    CHECK(!*request->texts[0] && !strcmp(request->texts[1],"NULL"));
    statistics_request_free(request); statistics_request_free(NULL); free(error);
}
static void test_rejected_request_cleanup(void) {
    StatisticsRequest *request=dx_alloc(sizeof(*request)); request->text_count=request->text_capacity=SIZE_MAX;
    statistics_request_free(request); CHECK(true); /* No pointer array exists. */
    for (size_t test=0;test<3;test++) {
        size_t count=test==2 ? 32768 : 2;
        request=dx_alloc(sizeof(*request)); request->texts=dx_alloc(count*sizeof(char *));
        for (size_t i=0;i<count;i++) request->texts[i]=dx_strdup(i%2 ? "NULL" : "");
        request->text_capacity=test==1 || test==2 ? SIZE_MAX : count;
        request->text_count=test==1 ? count : SIZE_MAX;
        request->count=SIZE_MAX; request->sources.count=SIZE_MAX;
        for (size_t i=0;i<DX_MAX_OBJECTS+1;i++) CHECK(pivot_assign(&request->ranges[i].pivot,PIVOT_VALUES,"Owned rejected measure",AGG_SUM,NULL));
        for (size_t i=0;i<DX_MAX_OBJECTS;i++) request->sources.items[i]=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("owned-rejected-source.csv")};
        statistics_request_free(request); CHECK(true); /* ASan verifies every loop stays within its owned bound. */
    }
    request=dx_alloc(sizeof(*request)); request->texts=dx_alloc(sizeof(char *)); request->text_count=SIZE_MAX;
    statistics_request_free(request); CHECK(true); /* Zero capacity owns no text entries. */
}
static void test_rejected_analysis_cleanup(void) {
    Analysis source={.filter_count=SIZE_MAX,.sort_count=SIZE_MAX,.kind=SOURCE_DEMO,.query=dx_strdup("SELECT 1"),.filter_column=dx_strdup("Primary filter"),.filter_value=dx_strdup("NULL"),.sort_column=dx_strdup("Primary sort")};
    for (size_t i=0;i<sizeof(source.filters)/sizeof(*source.filters);i++) source.filters[i]=(AnalysisFilter){.column=dx_strdup("Rejected filter"),.value=dx_strdup("")};
    for (size_t i=0;i<sizeof(source.sorts)/sizeof(*source.sorts);i++) source.sorts[i]=(AnalysisSort){.column=dx_strdup("Rejected sort")};
    analysis_free(&source); Analysis zero={0}; CHECK(!memcmp(&source,&zero,sizeof(zero))); analysis_free(&source); CHECK(!memcmp(&source,&zero,sizeof(zero)));
}
static void test_rejected_source_cleanup(void) {
    SourceSnapshot snapshot={.count=SIZE_MAX};
    for (size_t i=0;i<DX_MAX_OBJECTS;i++) {
        Analysis *source=&snapshot.items[i];
        source->kind=SOURCE_DEMO; source->path=dx_strdup("Owned rejected source"); source->query=dx_strdup("SELECT 1");
        source->filter_column=dx_strdup("Primary filter"); source->filter_value=dx_strdup("NULL"); source->sort_column=dx_strdup("Primary sort");
        source->filter_count=source->sort_count=SIZE_MAX;
        for (size_t f=0;f<sizeof(source->filters)/sizeof(*source->filters);f++) source->filters[f]=(AnalysisFilter){.column=dx_strdup("Nested owned filter"),.value=dx_strdup("")};
        for (size_t n=0;n<sizeof(source->sorts)/sizeof(*source->sorts);n++) source->sorts[n]=(AnalysisSort){.column=dx_strdup("Nested owned sort")};
    }
    source_snapshot_free(&snapshot); SourceSnapshot zero={0}; CHECK(!memcmp(&snapshot,&zero,sizeof(zero))); source_snapshot_free(&snapshot); CHECK(!memcmp(&snapshot,&zero,sizeof(zero)));
}

#ifdef DUXCEL_PAGE_ENGINE_TEST
static void test_native_allocator(void) {
    char *error=NULL; Database database={0}; Page page={0};
    Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/null-text.csv")};
    CHECK(database_open(&database,&error));
    CHECK(analysis_query_set(&source,"SELECT NULL::VARCHAR AS missing, ''::VARCHAR AS empty, 'NULL' AS literal, 170141183460469231731687303715884105727::HUGEINT AS wide, -12345678901234567890.123456789012345678::DECIMAL(38,18) AS amount, 'A' AS city FROM input LIMIT 1",&error));
    CHECK(database_fetch_table(&database,&source,0,-1,&page,&error));
    CHECK(page.release_cell_text==duckdb_free && page.row_count==1 && page.column_count==6);
    CHECK(page.cells[0].is_null && !page.cells[0].text);
    CHECK(!page.cells[1].is_null && page.cells[1].text && !*page.cells[1].text);
    CHECK(!page.cells[2].is_null && !strcmp(page.cells[2].text,"NULL"));
    CHECK(!strcmp(page.cells[3].text,"170141183460469231731687303715884105727"));
    CHECK(page.columns[4].type==DUCKDB_TYPE_DECIMAL && !strcmp(page.columns[4].type_name,"DECIMAL(38,18)"));
    CHECK(!strcmp(page.cells[4].text,"-12345678901234567890.123456789012345678"));
    Page moved=page; page=(Page){0};
    PivotDefinition pivot={0};
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"city",AGG_SUM,&error));
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"amount",AGG_SUM,&error));
    CHECK(database_pivot(&database,&source,&pivot,0,-1,&page,&error));
    CHECK(database_pivot_totals(&database,&source,&pivot,&page,&error));
    CHECK(page.release_cell_text==duckdb_free && page.totals && page.totals->rows.release_cell_text==duckdb_free && page.totals->columns.release_cell_text==duckdb_free);
    database_close(&database); page_free(&moved); page_free(&page); pivot_free(&pivot); analysis_free(&source); free(error);
}
#endif
int main(void) {
    test_moved_and_nested_ownership(); test_default_and_partial_pages(); test_record_publication_identity(); test_distinct_and_statistics(); test_request_ownership(); test_rejected_request_cleanup(); test_rejected_source_cleanup(); test_rejected_analysis_cleanup();
#ifdef DUXCEL_PAGE_ENGINE_TEST
    test_native_allocator();
#endif
    printf("PASS: %u page ownership checks\n",checks); return 0;
}
