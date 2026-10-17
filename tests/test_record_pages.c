#define _DARWIN_C_SOURCE
#include "data/database.h"
#include "platform/platform.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned checks;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL record pages:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
static duckdb_result query(Database *db,const char *sql) {
    duckdb_result result={0}; char *error=NULL;
    if (!database_execute(db,sql,&result,&error)) { fprintf(stderr,"DuckDB: %s\nSQL: %s\n",error,sql); exit(1); }
    free(error); return result;
}
static void command(Database *db,const char *sql) { duckdb_result result=query(db,sql); duckdb_destroy_result(&result); }
static void write_parquet(Database *db,const char *path,const char *relation) {
    char *quoted=dx_quote(path,'\''),*sql=dx_printf("COPY (%s) TO %s (FORMAT PARQUET)",relation,quoted);
    command(db,sql); free(sql); free(quoted);
}
static Page fetch(Database *db,Analysis *a,uint64_t offset) {
    Page page; char *error=NULL;
    if (!database_fetch_table_window(db,a,offset,-1,&page,&error)) { fprintf(stderr,"Fetch: %s\n",error); exit(1); }
    free(error); return page;
}
static void check_page(Database *db,Analysis *a,uint64_t offset,const char *where,const char *order) {
    Page page=fetch(db,a,offset); char *quoted=dx_quote(a->path,'\'');
    char *sql=dx_printf("SELECT * EXCLUDE(file_row_number),file_row_number::UBIGINT+1 AS expected_record FROM read_parquet(%s,file_row_number=true) %s %s LIMIT 256 OFFSET %" PRIu64,quoted,where,order,offset);
    duckdb_result expected=query(db,sql); free(sql); free(quoted);
    CHECK(page.offset==offset && page.column_count==8 && page.row_count==duckdb_row_count(&expected));
    CHECK(page.release_cell_text==duckdb_free && page.records.count==page.row_count && page.records.source_key && *page.records.source_key);
    CHECK(page.records.stamp==dx_file_stamp(a->path));
    for (size_t col=0;col<page.column_count;col++) {
        CHECK(!strcmp(page.columns[col].name,duckdb_column_name(&expected,col)));
        CHECK(page.columns[col].type==duckdb_column_type(&expected,col));
    }
    for (size_t row=0;row<page.row_count;row++) {
        uint64_t id=0; CHECK(page_record_id(&page,offset+row,&id));
        CHECK(id==duckdb_value_uint64(&expected,8,row));
        for (size_t col=0;col<page.column_count;col++) {
            Cell *cell=&page.cells[row*page.column_count+col]; CHECK(cell->is_null==duckdb_value_is_null(&expected,col,row));
            char *text=cell->is_null ? NULL : duckdb_value_varchar(&expected,col,row);
            CHECK(text ? cell->text && !strcmp(text,cell->text) : !cell->text); duckdb_free(text);
        }
    }
    uint64_t keep=999; CHECK(!page_record_id(&page,offset+page.row_count,&keep) && keep==999);
    if (offset) CHECK(!page_record_id(&page,offset-1,&keep) && keep==999);
    duckdb_destroy_result(&expected); page_free(&page);
    CHECK(!page.records.ids && !page.records.source_key && !page.records.count && !page.records.stamp);
}
static void check_consumers(Database *db,Analysis *a,const char *dir) {
    Page page=fetch(db,a,256),copy={0}; char *error=NULL,*tsv=NULL,*expected=dx_strdup("");
    CHECK(page.row_count>=12 && page.records.count==page.row_count);
    CHECK(database_copy_page(db,a,NULL,256,12,0,8,&copy,&error));
    CHECK(copy.row_count==12 && copy.column_count==8 && !copy.records.ids && !copy.records.source_key);
    for (size_t i=0;i<12*8;i++) CHECK(page.cells[i].is_null==copy.cells[i].is_null && (page.cells[i].is_null || !strcmp(page.cells[i].text,copy.cells[i].text)));
    for (size_t i=0;i<12;i++) dx_append(&expected,"%s%s",i ? "\n" : "",page.cells[i*8].text);
    CHECK(database_copy(db,a,256,12,0,1,&tsv,&error) && !strcmp(tsv,expected)); free(tsv); free(expected);
    int64_t sum=0,minimum=INT64_MAX,maximum=0;
    for (size_t i=0;i<12;i++) { int64_t n=strtoll(page.cells[i*8].text,NULL,10); sum+=n; if (n<minimum) minimum=n; if (n>maximum) maximum=n; }
    SelectionStats stats={0}; CHECK(database_statistics(db,a,NULL,256,12,0,1,&stats,&error));
    expected=dx_printf("%" PRId64,sum); CHECK(stats.sum && !strcmp(stats.sum,expected) && stats.cells==12 && stats.numeric==12); free(expected);
    expected=dx_printf("%" PRId64,minimum); CHECK(stats.minimum && !strcmp(stats.minimum,expected)); free(expected);
    expected=dx_printf("%" PRId64,maximum); CHECK(stats.maximum && !strcmp(stats.maximum,expected)); free(expected); statistics_free(&stats);
    char *exported=dx_printf("%s/export.parquet",dir); uint64_t rows=0;
    CHECK(database_export_parquet(db,a,exported,&rows,&error));
    Analysis other={.kind=SOURCE_PARQUET,.path=exported}; Page exported_page=fetch(db,&other,256);
    CHECK(rows>=256+12 && exported_page.column_count==8 && exported_page.row_count>=12);
    for (size_t i=0;i<12*8;i++) CHECK(page.cells[i].is_null==exported_page.cells[i].is_null && (page.cells[i].is_null || !strcmp(page.cells[i].text,exported_page.cells[i].text)));
    page_free(&exported_page); CHECK(unlink(exported)==0); analysis_free(&other);
    page_free(&page); page_free(&copy); free(error);
}
static void test_fallbacks(Database *db,const char *path,const char *dir) {
    char *error=NULL; Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup(path)};
    CHECK(analysis_query_set(&a,"SELECT * FROM input",&error)); Page page=fetch(db,&a,0);
    CHECK(page.column_count==8 && !page.records.ids && !page.records.source_key); page_free(&page);
    CHECK(analysis_query_set(&a,"",&error)); a.unique_rows=true; page=fetch(db,&a,0);
    CHECK(page.column_count==8 && !page.records.ids && !page.records.source_key); page_free(&page);
    a.unique_rows=false; a.group_column=dx_strdup("tie key"); a.measure_column=dx_strdup("Money"); a.aggregate=AGG_SUM; page=fetch(db,&a,0);
    CHECK(page.column_count==2 && !page.records.ids && !page.records.source_key); page_free(&page); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")}; page=fetch(db,&a,0);
    CHECK(page.column_count==6 && !page.records.ids && !page.records.source_key); page_free(&page); analysis_free(&a);
    char *collision=dx_printf("%s/collision.parquet",dir); write_parquet(db,collision,"SELECT 17 AS FiLe_RoW_NuMbEr,42 AS value");
    a=(Analysis){.kind=SOURCE_PARQUET,.path=collision}; page=fetch(db,&a,0);
    CHECK(page.column_count==2 && !page.records.ids && !strcmp(page.cells[0].text,"17")); page_free(&page); CHECK(unlink(collision)==0); analysis_free(&a);
    char *wide=dx_printf("%s/wide.parquet",dir),*sql=dx_strdup("SELECT 1 AS c0");
    for (size_t i=1;i<256;i++) dx_append(&sql,",%zu AS c%zu",i,i); write_parquet(db,wide,sql); free(sql);
    a=(Analysis){.kind=SOURCE_PARQUET,.path=wide}; page=fetch(db,&a,0);
    CHECK(page.column_count==256 && page.records.count==1 && page.records.ids[0]==1); page_free(&page);
    write_parquet(db,wide,"SELECT range AS x FROM range(0)"); page=fetch(db,&a,0);
    CHECK(page.column_count==1 && !page.row_count && page.records.source_key && !page.records.ids && !page.records.count); page_free(&page);
    CHECK(unlink(wide)==0); analysis_free(&a); free(error);
}
static void test_optional_budget(Database *db,const char *dir) {
    const size_t budget=4*1024*1024,fixed=sizeof(Column)+256*sizeof(Cell)+sizeof("payload")+sizeof("VARCHAR");
    size_t length=(budget-1000-fixed)/256-1;
    char *path=dx_printf("%s/budget.parquet",dir),*sql=dx_printf("SELECT repeat('x',%zu)::VARCHAR AS payload FROM range(256)",length);
    write_parquet(db,path,sql); free(sql); Analysis source={.kind=SOURCE_PARQUET,.path=path}; Page page=fetch(db,&source,0);
    CHECK(page.row_count==256 && page.column_count==1 && strlen(page.cells[255].text)==length);
    CHECK(page.bytes<=budget && budget-page.bytes<1500 && !page.records.ids && !page.records.source_key);
    CHECK(db->page_record_alias!=NULL); /* Hidden column was read but omitted, not exposed. */
    page_free(&page); CHECK(unlink(path)==0); analysis_free(&source);
}
static void test_observed_versions(Database *db,const char *dir) {
    char *path=dx_printf("%s/replaced.parquet",dir); write_parquet(db,path,"SELECT i AS id FROM range(700) t(i)");
    Analysis source={.kind=SOURCE_PARQUET,.path=path}; Page first=fetch(db,&source,0),second=fetch(db,&source,256);
    CHECK(!page_records_changed(&first,&second)); page_free(&second);
    write_parquet(db,path,"SELECT i AS id, 'new schema' AS version FROM range(17) t(i)"); second=fetch(db,&source,0);
    CHECK(page_records_changed(&first,&second) && first.row_count==256 && second.row_count==17 && second.column_count==2);
    CHECK(first.records.ids[255]==256 && second.records.ids[16]==17);
    CHECK(!page_records_changed(NULL,&second) && !page_records_changed(&first,NULL));
    Page bare={0},hint={0}; hint.records.stamp=first.records.stamp;
    CHECK(!page_records_changed(&first,&bare) && !page_records_changed(&first,&hint));
    hint.records.stamp=second.records.stamp; CHECK(page_records_changed(&first,&hint));
    page_free(&first); page_free(&second); CHECK(unlink(path)==0); analysis_free(&source);
}
/* Opt-in local measurement against the existing public NYC Parquet. Each
   variant/case gets a fresh private engine session; "cold" is not OS-cache
   cold. This measures complete Page conversion, not just stand-alone SQL. */
static int compare_ms(const void *a,const void *b) { double x=*(const double *)a,y=*(const double *)b; return (x>y)-(x<y); }
static int performance(const char *path) {
    const char *cases[]={"first","filter","deep","filtered_deep","sort","filtered_sort"};
    printf("{\"engine\":\"%s\",\"warm_samples\":5,\"cases\":[",duckdb_library_version());
    for (size_t c=0;c<6;c++) for (int identified=0;identified<2;identified++) {
        Database db={0}; char *error=NULL; CHECK(database_open(&db,&error)); Analysis source={.kind=SOURCE_PARQUET,.path=dx_strdup(path)};
        if (!identified) CHECK(analysis_query_set(&source,"SELECT * FROM input",&error));
        if (c==1 || c==3 || c==5) CHECK(analysis_filter_set(&source,"fare_amount",FILTER_GREATER,"25",&error));
        if (c>=4) CHECK(analysis_sort_set(&source,"fare_amount",true,false,&error));
        uint64_t offset=c==2 ? 3000000 : c==3 ? 100000 : 0; double samples[5],cold=0; size_t bytes=0,metadata=0;
        for (int run=0;run<6;run++) {
            Page page=fetch(&db,&source,offset); CHECK(page.column_count==20 && page.row_count==256);
            CHECK(identified ? page.records.count==page.row_count : !page.records.source_key);
            if (run) samples[run-1]=page.query_ms; else cold=page.query_ms;
            bytes=page.bytes; metadata=page.records.count*sizeof(uint64_t)+(page.records.source_key ? strlen(page.records.source_key)+1 : 0); page_free(&page);
        }
        qsort(samples,5,sizeof(*samples),compare_ms);
        printf("%s{\"case\":\"%s\",\"records\":%s,\"session_first_ms\":%.3f,\"warm_median_ms\":%.3f,\"page_bytes\":%zu,\"metadata_bytes\":%zu,\"page_prepares\":%" PRIu64 ",\"queries\":%" PRIu64 "}",c || identified ? "," : "",cases[c],identified ? "true" : "false",cold,samples[2],bytes,metadata,db.page_prepares,db.query_count);
        database_close(&db); analysis_free(&source); free(error);
    }
    puts("]}"); return 0;
}
int main(int argc,char **argv) {
    if (argc==3 && !strcmp(argv[1],"--perf")) return performance(argv[2]);
    CHECK(argc==1);
    char dir[]="/tmp/duxcel-record-pages-XXXXXX"; CHECK(mkdtemp(dir)!=NULL);
    char *error=NULL; Database db={0}; CHECK(database_open(&db,&error));
    duckdb_result setting=query(&db,"SELECT current_setting('preserve_insertion_order')"); CHECK(duckdb_value_boolean(&setting,0,0)); duckdb_destroy_result(&setting);
    command(&db,"CREATE TABLE original AS SELECT (9007199254740993::BIGINT+i//2)::BIGINT AS id,((i//2)%3)::INTEGER AS \"tie key\",'9999999999999999999999999999.1234567890'::DECIMAL(38,10) AS \"exact DECIMAL\",CASE (i//2)%4 WHEN 0 THEN NULL WHEN 1 THEN '' WHEN 2 THEN 'NULL' ELSE 'İstanbul' END AS nullable,((100+(i//2)%5)/100.0)::DECIMAL(4,2) AS Money,'business' AS __duxcel_record_id,7 AS __DUXCEL_RECORD_ID_1,'odd''quoted' AS \"line\nname\" FROM range(700) t(i)");
    char *path=dx_printf("%s/raw 'quoted.parquet",dir); write_parquet(&db,path,"SELECT * FROM original");
    Analysis source={.kind=SOURCE_PARQUET,.path=path}; Page retained=fetch(&db,&source,0);
    CHECK(retained.column_count==8 && retained.records.count==256 && retained.records.ids[0]==1 && retained.records.ids[255]==256);
    char *scope=dx_strdup(retained.records.source_key); uint64_t prepares=db.page_prepares,queries=db.query_count;
    Page second=fetch(&db,&source,256); CHECK(db.page_prepares==prepares && db.query_count==queries+1); page_free(&second);
    for (uint64_t offset=0;offset<=768;offset+=256) check_page(&db,&source,offset,"","");
    CHECK(analysis_sort_set(&source,"tie key",false,false,&error));
    check_page(&db,&source,0,"","ORDER BY \"tie key\" ASC,expected_record ASC"); check_page(&db,&source,256,"","ORDER BY \"tie key\" ASC,expected_record ASC");
    CHECK(analysis_sort_set(&source,"Money",true,true,&error));
    check_page(&db,&source,0,"","ORDER BY \"tie key\" ASC,Money DESC,expected_record ASC"); check_consumers(&db,&source,dir);
    CHECK(analysis_filter_set(&source,"id",FILTER_GREATER,"9007199254741040",&error));
    check_page(&db,&source,0,"WHERE id>9007199254741040","ORDER BY \"tie key\" ASC,Money DESC,expected_record ASC");
    Page filtered=fetch(&db,&source,0); CHECK(!strcmp(scope,filtered.records.source_key) && strcmp(scope,db.page_key)); page_free(&filtered);
    DistinctPage distinct={0}; CHECK(database_distinct(&db,&source,"nullable","",-1,NULL,NULL,0,-1,&distinct,&error)); CHECK(distinct.total==4); distinct_free(&distinct);
    CHECK(analysis_filter_set(&source,"id",FILTER_GREATER,"9223372036854775807",&error));
    check_page(&db,&source,0,"WHERE id>9223372036854775807","ORDER BY \"tie key\" ASC,Money DESC,expected_record ASC");
    test_fallbacks(&db,path,dir); test_optional_budget(&db,dir); test_observed_versions(&db,dir);
    CHECK(database_refresh(&db,&error)); CHECK(retained.records.ids[255]==256 && !strcmp(retained.records.source_key,scope));
    Page moved=retained; retained=(Page){0}; database_close(&db); page_free(&retained); page_free(&moved); free(scope);
    CHECK(unlink(path)==0); analysis_free(&source); CHECK(rmdir(dir)==0); free(error);
    printf("PASS: %u native record-page checks\n",checks); return 0;
}
