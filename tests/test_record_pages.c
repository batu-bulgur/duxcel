#define _DARWIN_C_SOURCE
#include "data/database.h"
#include "platform/platform.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
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
static void write_csv(Database *db,const char *path,const char *relation) {
    char *quoted=dx_quote(path,'\''),*sql=dx_printf("COPY (%s) TO %s (FORMAT CSV,HEADER true)",relation,quoted);
    command(db,sql); free(sql); free(quoted);
}
static Page fetch(Database *db,Analysis *a,uint64_t offset) {
    Page page; char *error=NULL;
    if (!database_fetch_table_window(db,a,offset,-1,&page,&error)) { fprintf(stderr,"Fetch: %s\n",error); exit(1); }
    free(error); return page;
}
static void check_file_identity(const Page *page,const char *path) {
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    SourceFileIdentity current; SourceDigest digest; char error[DX_SOURCE_DIGEST_ERROR_BYTES];
    CHECK(page->records.file_identity_valid && source_file_identity(path,&current,error)==SOURCE_DIGEST_OK);
    CHECK(source_file_identity_same(&page->records.file_identity,&current));
    CHECK(source_digest_file(path,NULL,NULL,&digest,error)==SOURCE_DIGEST_OK && source_file_identity_same(&page->records.file_identity,&digest.identity));
#else
    (void)path; CHECK(!page->records.file_identity_valid);
#endif
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
    CHECK(database_copy_page(db,a,NULL,256,12,0,page.column_count,&copy,&error));
    CHECK(copy.row_count==12 && copy.column_count==page.column_count && !copy.records.ids && !copy.records.source_key);
    for (size_t i=0;i<12*page.column_count;i++) CHECK(page.cells[i].is_null==copy.cells[i].is_null && (page.cells[i].is_null || !strcmp(page.cells[i].text,copy.cells[i].text)));
    for (size_t i=0;i<12;i++) dx_append(&expected,"%s%s",i ? "\n" : "",page.cells[i*page.column_count].text);
    CHECK(database_copy(db,a,256,12,0,1,&tsv,&error) && !strcmp(tsv,expected)); free(tsv); free(expected);
    int64_t sum=0,minimum=INT64_MAX,maximum=0;
    for (size_t i=0;i<12;i++) { int64_t n=strtoll(page.cells[i*page.column_count].text,NULL,10); sum+=n; if (n<minimum) minimum=n; if (n>maximum) maximum=n; }
    SelectionStats stats={0}; CHECK(database_statistics(db,a,NULL,256,12,0,1,&stats,&error));
    expected=dx_printf("%" PRId64,sum); CHECK(stats.sum && !strcmp(stats.sum,expected) && stats.cells==12 && stats.numeric==12); free(expected);
    expected=dx_printf("%" PRId64,minimum); CHECK(stats.minimum && !strcmp(stats.minimum,expected)); free(expected);
    expected=dx_printf("%" PRId64,maximum); CHECK(stats.maximum && !strcmp(stats.maximum,expected)); free(expected); statistics_free(&stats);
    char *exported=dx_printf("%s/export.parquet",dir); uint64_t rows=0;
    CHECK(database_export_parquet(db,a,exported,&rows,&error));
    Analysis other={.kind=SOURCE_PARQUET,.path=exported}; Page exported_page=fetch(db,&other,256);
    CHECK(rows>=256+12 && exported_page.column_count==page.column_count && exported_page.row_count>=12);
    for (size_t i=0;i<12*page.column_count;i++) CHECK(page.cells[i].is_null==exported_page.cells[i].is_null && (page.cells[i].is_null || !strcmp(page.cells[i].text,exported_page.cells[i].text)));
    page_free(&exported_page); CHECK(unlink(exported)==0); analysis_free(&other);
    page_free(&page); page_free(&copy); free(error);
}
static void test_fallbacks(Database *db,const char *path,const char *dir) {
    char *error=NULL; Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup(path)};
    CHECK(analysis_query_set(&a,"SELECT * FROM input",&error)); Page page=fetch(db,&a,0);
    CHECK(page.column_count==8 && !page.records.ids && !page.records.source_key && !page.records.file_identity_valid); page_free(&page);
    CHECK(analysis_query_set(&a,"",&error)); a.unique_rows=true; page=fetch(db,&a,0);
    CHECK(page.column_count==8 && !page.records.ids && !page.records.source_key && !page.records.file_identity_valid); page_free(&page);
    a.unique_rows=false; a.group_column=dx_strdup("tie key"); a.measure_column=dx_strdup("Money"); a.aggregate=AGG_SUM; page=fetch(db,&a,0);
    CHECK(page.column_count==2 && !page.records.ids && !page.records.source_key && !page.records.file_identity_valid); page_free(&page); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")}; page=fetch(db,&a,0);
    CHECK(page.column_count==6 && page.records.ids && page.records.count==page.row_count && page.records.ids[0]==1); page_free(&page); analysis_free(&a);
    char *collision=dx_printf("%s/collision.parquet",dir); write_parquet(db,collision,"SELECT 17 AS FiLe_RoW_NuMbEr,42 AS value");
    a=(Analysis){.kind=SOURCE_PARQUET,.path=collision}; page=fetch(db,&a,0);
    CHECK(page.column_count==2 && !page.records.ids && !page.records.file_identity_valid && !strcmp(page.cells[0].text,"17")); page_free(&page); CHECK(unlink(collision)==0); analysis_free(&a);
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
    check_file_identity(&page,path); /* Inline observation does not consume the optional ID/scope budget. */
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
    CHECK(!page_records_changed(&first,&bare) && page_records_changed(&first,&hint)==first.records.file_identity_valid);
    hint.records.stamp=second.records.stamp; CHECK(page_records_changed(&first,&hint));
    page_free(&first); page_free(&second); CHECK(unlink(path)==0); analysis_free(&source);
}
typedef struct { Database *db; const char *path; uint64_t queries; bool changed; } SourceTouch;
static bool touch_during_query(void *context) {
    SourceTouch *touch=context;
    if (!touch->changed && touch->db->query_count>touch->queries) {
        struct stat file; CHECK(stat(touch->path,&file)==0);
        struct timeval times[2]={{file.st_atime,0},{file.st_mtime+1,0}};
        CHECK(utimes(touch->path,times)==0); touch->changed=true;
    }
    return false; /* Mutate the private fixture without canceling execution. */
}
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
typedef struct { Database *db; const char *path; uint64_t queries; unsigned phase; bool changed; } IdentityTouch;
static bool touch_file_identity(void *context) {
    IdentityTouch *touch=context;
    if (!touch->changed && touch->db->query_count>=touch->queries+touch->phase) {
        struct stat file; CHECK(lstat(touch->path,&file)==0);
        uint64_t stamp=dx_file_stamp(touch->path);
        CHECK(chmod(touch->path,(file.st_mode&0100) ? 0600 : 0700)==0);
        CHECK(dx_file_stamp(touch->path)==stamp); /* Deliberately invisible to the old stamp. */
        touch->changed=true;
    }
    return false;
}
#endif
static void test_exact_file_observation(Database *db,const char *dir,SourceKind kind) {
    char *path=dx_printf("%s/exact-identity.%s",dir,kind==SOURCE_CSV ? "csv" : "parquet"),*error=NULL;
    if (kind==SOURCE_CSV) write_csv(db,path,"SELECT i::BIGINT AS id,'original' AS value FROM range(700) t(i)");
    else write_parquet(db,path,"SELECT i::BIGINT AS id,'original' AS value FROM range(700) t(i)");
    Analysis source={.kind=kind,.path=path}; Page page=fetch(db,&source,0); check_file_identity(&page,path);
    SourceFileIdentity retained=page.records.file_identity; page_free(&page);
    CHECK(!page.records.file_identity_valid && !page.records.file_identity.inode);
    CHECK(database_fetch_table(db,&source,256,-1,&page,&error) && page.total==700 && !page.count_pending); check_file_identity(&page,path); page_free(&page);
    uint64_t known_counts=db->count_queries;
    CHECK(database_fetch_table_window(db,&source,512,700,&page,&error) && page.total==700 && !page.count_pending && db->count_queries==known_counts); check_file_identity(&page,path); page_free(&page);
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    Page displayed=fetch(db,&source,0); uint64_t stable_stamp=displayed.records.stamp;
    struct stat stable; CHECK(lstat(path,&stable)==0 && chmod(path,(stable.st_mode&0100) ? 0600 : 0700)==0 && dx_file_stamp(path)==stable_stamp);
    Page changed=fetch(db,&source,256); check_file_identity(&changed,path);
    CHECK(displayed.records.file_identity_valid && changed.records.file_identity_valid && displayed.records.stamp==changed.records.stamp && !strcmp(displayed.records.source_key,changed.records.source_key));
    CHECK(page_records_changed(&displayed,&changed) && page_records_changed(&changed,&displayed));
    Page recovered=fetch(db,&source,512); CHECK(!page_records_changed(&changed,&recovered) && page_records_changed(&displayed,&recovered));
    CHECK(displayed.records.ids[0]==1 && changed.records.ids[0]==257 && recovered.records.ids[0]==513);
    Page unavailable=recovered; unavailable.records.file_identity_valid=false;
    CHECK(page_records_changed(&recovered,&unavailable)); /* Exact observation must not silently disappear. */
    unavailable.records.ids=NULL; unavailable.records.count=0; unavailable.records.source_key=NULL;
    CHECK(page_records_changed(&recovered,&unavailable)); /* Near-budget stamp-only provenance also fails closed. */
    unavailable.records.stamp=0; CHECK(!page_records_changed(&recovered,&unavailable));
    page_free(&displayed); page_free(&changed); page_free(&recovered); /* unavailable is a borrowed view. */
    for (unsigned phase=0;phase<3;phase++) {
        CHECK(database_refresh(db,&error));
        SourceFileIdentity before,after; CHECK(source_file_identity(path,&before,NULL)==SOURCE_DIGEST_OK);
        uint64_t stamp=dx_file_stamp(path),counts=db->count_queries;
        IdentityTouch touch={db,path,db->query_count,phase,false}; db->cancelled=touch_file_identity; db->cancel_context=&touch;
        bool ok=database_fetch_table(db,&source,0,-1,&page,&error);
        db->cancelled=NULL; db->cancel_context=NULL;
        CHECK(!ok && touch.changed && db->count_queries==counts+1 && db->query_count==touch.queries+2);
        CHECK(error && strstr(error,"exact file identity") && !page.records.file_identity_valid);
        CHECK(source_file_identity(path,&after,NULL)==SOURCE_DIGEST_OK && !source_file_identity_same(&before,&after) && dx_file_stamp(path)==stamp);
        free(error); error=NULL; page_free(&page);
        Page refreshed=fetch(db,&source,0); check_file_identity(&refreshed,path);
        CHECK(!source_file_identity_same(&retained,&refreshed.records.file_identity)); page_free(&refreshed);
    }
    char *alias=dx_printf("%s/identity-link.%s",dir,kind==SOURCE_CSV ? "csv" : "parquet"); CHECK(symlink(path,alias)==0);
    Analysis linked={.kind=kind,.path=alias}; page=fetch(db,&linked,0);
    CHECK(page.row_count==256 && page.records.ids && !page.records.file_identity_valid); /* Ordinary read is preserved. */
    page_free(&page); CHECK(unlink(alias)==0); analysis_free(&linked);
#else
    (void)retained;
#endif
    CHECK(unlink(path)==0); analysis_free(&source); free(error);
}
static void check_destination(const char *path) {
    char text[32]={0}; FILE *file=fopen(path,"rb"); CHECK(file && fread(text,1,sizeof(text)-1,file)==20 && fgetc(file)==EOF && !fclose(file));
    CHECK(!strcmp(text,"Existing destination"));
}
static void test_bound_consumers(Database *db,const char *dir,SourceKind kind) {
    char *path=dx_printf("%s/bound.%s",dir,kind==SOURCE_CSV ? "csv" : "parquet"),*output=dx_printf("%s/bound-output.parquet",dir),*error=NULL;
    if (kind==SOURCE_CSV) write_csv(db,path,"SELECT i::BIGINT AS id,'original' AS value FROM range(700) t(i)");
    else write_parquet(db,path,"SELECT i::BIGINT AS id,'original' AS value FROM range(700) t(i)");
    Analysis source={.kind=kind,.path=path}; Page displayed=fetch(db,&source,0),copy={0}; SelectionStats stats={0};
    uint64_t expected=displayed.records.stamp; CHECK(expected && displayed.records.source_key);
    CHECK(database_source_matches(&source,expected,&error));
    CHECK(database_copy_page_bound(db,&source,NULL,0,2,0,1,expected,&copy,&error));
    CHECK(database_statistics_bound(db,&source,NULL,0,2,0,1,expected,&stats,&error) && !strcmp(stats.sum,"1"));
    PivotDefinition pivot={0}; SelectionStats pivot_stats={0};
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"value",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"id",AGG_MAX,&error));
    CHECK(database_statistics_bound(db,&source,&pivot,0,64,1,1,expected,&pivot_stats,&error) && !strcmp(pivot_stats.sum,"699"));
    char *pivot_key=dx_strdup(db->pivot_key);
    Cell *keep_cells=copy.cells; char *keep_sum=stats.sum,*tsv=dx_strdup("Keep clipboard"),*keep_tsv=tsv;
    FILE *file=fopen(output,"wb"); CHECK(file && fwrite("Existing destination",1,20,file)==20 && !fclose(file));
    if (kind==SOURCE_CSV) write_csv(db,path,"SELECT (i+100)::BIGINT AS id,'changed source and extent' AS value FROM range(17) t(i)");
    else write_parquet(db,path,"SELECT (i+100)::BIGINT AS id,'changed source and extent' AS value FROM range(17) t(i)");
    CHECK(dx_file_stamp(path)!=expected); uint64_t queries=db->query_count,rows=999;
    CHECK(!database_copy_bound(db,&source,0,2,0,1,expected,&tsv,&error)); free(error); error=NULL;
    CHECK(!database_copy_page_bound(db,&source,NULL,0,2,0,1,expected,&copy,&error)); free(error); error=NULL;
    CHECK(!database_statistics_bound(db,&source,NULL,0,2,0,1,expected,&stats,&error)); free(error); error=NULL;
    CHECK(!database_export_parquet_bound(db,&source,output,expected,&rows,&error) && !rows); free(error); error=NULL;
    StatisticsRequest request={.count=1,.cells=2}; request.ranges[0]=(StatisticsRange){.rows=2,.columns=1,.expected_source_stamp=expected};
    CHECK(!database_statistics_ranges(db,&source,&request,&stats,&error)); free(error); error=NULL;
    CHECK(db->query_count==queries && copy.cells==keep_cells && stats.sum==keep_sum && tsv==keep_tsv);
    CHECK(!strcmp(copy.cells[0].text,"0") && !strcmp(stats.sum,"1") && !strcmp(tsv,"Keep clipboard")); check_destination(output);
    /* Fetching a new raw Page does not explicitly clear the materialized pivot.
       Its source-stamped cache key must rebuild rather than reuse old values. */
    Page current=fetch(db,&source,0); CHECK(current.records.stamp!=expected);
    CHECK(database_statistics_bound(db,&source,&pivot,0,64,1,1,current.records.stamp,&pivot_stats,&error) && !strcmp(pivot_stats.sum,"116") && strcmp(pivot_key,db->pivot_key));
    free(pivot_key); Page pivot_copy={0};
    CHECK(database_copy_page_bound(db,&source,&pivot,0,1,0,2,current.records.stamp,&pivot_copy,&error) && !strcmp(pivot_copy.cells[0].text,"changed source and extent") && !strcmp(pivot_copy.cells[1].text,"116"));
    char *pivot_output=dx_printf("%s/bound-pivot.parquet",dir); CHECK(database_export_pivot_parquet_bound(db,&source,&pivot,pivot_output,current.records.stamp,&rows,&error) && rows==1);
    Analysis exported={.kind=SOURCE_PARQUET,.path=pivot_output}; Page pivot_export=fetch(db,&exported,0); CHECK(pivot_export.row_count==1 && !strcmp(pivot_export.cells[1].text,"116"));
    page_free(&pivot_export); CHECK(unlink(pivot_output)==0); analysis_free(&exported); page_free(&pivot_copy); page_free(&current); pivot_free(&pivot); statistics_free(&pivot_stats);
    CHECK(database_refresh(db,&error)); Page refreshed=fetch(db,&source,0);
    CHECK(refreshed.records.stamp!=expected && refreshed.row_count==17 && !strcmp(refreshed.cells[0].text,"100")); expected=refreshed.records.stamp;
    CHECK(database_copy_bound(db,&source,0,2,0,1,expected,&tsv,&error) && !strcmp(tsv,"100\n101")); free(keep_tsv); free(tsv); tsv=keep_tsv=dx_strdup("Keep clipboard");
    CHECK(database_statistics_bound(db,&source,NULL,0,2,0,1,expected,&stats,&error) && !strcmp(stats.sum,"201")); keep_sum=stats.sum;
    /* Change the observed version only after an engine execution has begun.
       Every output remains staged until its final source check succeeds. */
    for (unsigned operation=0;operation<5;operation++) {
        expected=dx_file_stamp(path); SourceTouch touch={db,path,db->query_count,false}; db->cancelled=touch_during_query; db->cancel_context=&touch;
        bool ok=false;
        if (operation==0) ok=database_copy_bound(db,&source,0,2,0,1,expected,&tsv,&error);
        else if (operation==1) ok=database_copy_page_bound(db,&source,NULL,0,2,0,1,expected,&copy,&error);
        else if (operation==2) ok=database_statistics_bound(db,&source,NULL,0,2,0,1,expected,&stats,&error);
        else if (operation==3) { request.ranges[0].expected_source_stamp=expected; ok=database_statistics_ranges(db,&source,&request,&stats,&error); }
        else ok=database_export_parquet_bound(db,&source,output,expected,&rows,&error);
        db->cancelled=NULL; db->cancel_context=NULL;
        CHECK(!ok && touch.changed && db->query_count>touch.queries && error && strstr(error,"source changed")); free(error); error=NULL;
        CHECK(copy.cells==keep_cells && stats.sum==keep_sum && tsv==keep_tsv && !strcmp(stats.sum,"201") && !strcmp(tsv,"Keep clipboard")); check_destination(output);
    }
    CHECK(database_export_parquet_bound(db,&source,output,dx_file_stamp(path),&rows,&error) && rows==17);
    exported=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup(output)}; Page raw_export=fetch(db,&exported,0); CHECK(raw_export.column_count==2 && raw_export.row_count==17 && !strcmp(raw_export.cells[0].text,"100")); page_free(&raw_export); analysis_free(&exported);
    /* Unbound SQL has no raw-record version claim and retains its valid path. */
    CHECK(analysis_query_set(&source,"SELECT * FROM input",&error)); queries=db->query_count;
    CHECK(!database_copy_bound(db,&source,0,2,0,1,dx_file_stamp(path),&tsv,&error) && db->query_count==queries); free(error); error=NULL;
    CHECK(database_copy_bound(db,&source,0,2,0,1,0,&tsv,&error) && !strcmp(tsv,"100\n101")); free(keep_tsv); free(tsv);
    CHECK(database_source_matches(NULL,0,&error));
    page_free(&displayed); page_free(&refreshed); page_free(&copy); statistics_free(&stats);
    CHECK(unlink(output)==0); free(output); CHECK(unlink(path)==0); analysis_free(&source); free(error);
}
/* The reference relation numbers the complete raw reader before predicates.
   Its schema/value comparison also checks that the optional ID projection did
   not change CSV inference or the demo's exact DECIMAL/DATE/NULL values. */
static void check_window_page(Database *db,Analysis *a,uint64_t offset,const char *where,const char *order) {
    Page page=fetch(db,a,offset); char *raw=analysis_source_sql(a,false);
    char *sql=dx_printf("SELECT * FROM (SELECT *,row_number() OVER ()::UBIGINT AS expected_record FROM (%s) raw) numbered %s %s LIMIT 256 OFFSET %" PRIu64,raw,where,order,offset);
    duckdb_result expected=query(db,sql); free(sql); free(raw);
    CHECK(page.offset==offset && page.column_count+1==duckdb_column_count(&expected) && page.row_count==duckdb_row_count(&expected));
    CHECK(page.records.source_key && page.records.count==page.row_count && page.records.stamp==dx_file_stamp(a->path));
    CHECK(!strncmp(page.records.source_key,a->kind==SOURCE_CSV ? "csv-record-v1\n" : "demo-record-v1\n",a->kind==SOURCE_CSV ? 14 : 15));
    for (size_t col=0;col<page.column_count;col++) {
        CHECK(!strcmp(page.columns[col].name,duckdb_column_name(&expected,col)));
        CHECK(page.columns[col].type==duckdb_column_type(&expected,col));
        for (size_t row=0;row<page.row_count;row++) {
            Cell *cell=&page.cells[row*page.column_count+col]; CHECK(cell->is_null==duckdb_value_is_null(&expected,col,row));
            char *text=cell->is_null ? NULL : duckdb_value_varchar(&expected,col,row);
            CHECK(text ? cell->text && !strcmp(text,cell->text) : !cell->text); duckdb_free(text);
        }
    }
    for (size_t row=0;row<page.row_count;row++) { uint64_t id=0; CHECK(page_record_id(&page,offset+row,&id) && id==duckdb_value_uint64(&expected,page.column_count,row)); }
    duckdb_destroy_result(&expected); page_free(&page);
}
static void check_window_fallbacks(Database *db,Analysis *source) {
    Analysis fallback=analysis_copy(source); char *error=NULL;
    CHECK(analysis_query_set(&fallback,"SELECT * FROM input",&error)); Page page=fetch(db,&fallback,0);
    CHECK(page.column_count && !page.records.ids && !page.records.source_key && !page.records.file_identity_valid); page_free(&page);
    CHECK(analysis_query_set(&fallback,"",&error)); fallback.unique_rows=true; page=fetch(db,&fallback,0);
    CHECK(page.column_count && !page.records.ids && !page.records.source_key && !page.records.file_identity_valid); page_free(&page);
    fallback.unique_rows=false; fallback.group_column=dx_strdup(source->kind==SOURCE_CSV ? "tie key" : "City");
    fallback.measure_column=dx_strdup(source->kind==SOURCE_CSV ? "Money" : "Amount"); fallback.aggregate=AGG_SUM; page=fetch(db,&fallback,0);
    CHECK(page.column_count==2 && !page.records.ids && !page.records.source_key && !page.records.file_identity_valid); page_free(&page); analysis_free(&fallback); free(error);
}
static void test_csv_pages(Database *db,const char *dir) {
    char *path=dx_printf("%s/raw 'quoted.csv",dir); write_csv(db,path,"SELECT * FROM original");
    Analysis source={.kind=SOURCE_CSV,.path=path}; Page retained=fetch(db,&source,0); char *error=NULL;
    CHECK(retained.column_count==8 && retained.records.count==256 && retained.records.ids[0]==1 && retained.records.ids[255]==256);
    CHECK(retained.columns[0].type==DUCKDB_TYPE_BIGINT && !strcmp(retained.cells[0].text,"9007199254740993"));
    CHECK(retained.cells[3].is_null && !retained.cells[2*8+3].is_null && !*retained.cells[2*8+3].text && !strcmp(retained.cells[4*8+3].text,"NULL"));
    /* Two physically identical rows keep different original ordinals. */
    CHECK(!strcmp(retained.cells[0].text,retained.cells[8].text) && retained.records.ids[0]!=retained.records.ids[1]);
    uint64_t prepares=db->page_prepares,queries=db->query_count; Page next=fetch(db,&source,256);
    CHECK(db->page_prepares==prepares && db->query_count==queries+1 && !page_records_changed(&retained,&next)); page_free(&next);
    for (uint64_t offset=0;offset<=768;offset+=256) check_window_page(db,&source,offset,"","");
    CHECK(analysis_sort_set(&source,"tie key",false,false,&error));
    check_window_page(db,&source,0,"","ORDER BY \"tie key\" ASC,expected_record ASC");
    check_window_page(db,&source,256,"","ORDER BY \"tie key\" ASC,expected_record ASC");
    CHECK(analysis_sort_set(&source,"Money",true,true,&error));
    check_window_page(db,&source,0,"","ORDER BY \"tie key\" ASC,Money DESC,expected_record ASC"); check_consumers(db,&source,dir);
    CHECK(analysis_filter_set(&source,"id",FILTER_GREATER,"9007199254741040",&error));
    check_window_page(db,&source,0,"WHERE id>9007199254741040","ORDER BY \"tie key\" ASC,Money DESC,expected_record ASC");
    next=fetch(db,&source,0); CHECK(!strcmp(retained.records.source_key,next.records.source_key)); page_free(&next);
    analysis_filters_clear(&source); analysis_sorts_clear(&source); check_window_fallbacks(db,&source);
    char *glob=dx_printf("%s/*.csv",dir); Analysis multiple={.kind=SOURCE_CSV,.path=glob}; next=fetch(db,&multiple,0);
    CHECK(next.column_count==8 && !next.records.ids && !next.records.source_key); page_free(&next); analysis_free(&multiple);
    /* A pattern may itself name an existing regular file, but the reader still
       expands it. Its file stamp cannot stand for the expanded input. */
    char *pattern=dx_printf("%s/pattern[1].csv",dir),*matched=dx_printf("%s/pattern1.csv",dir);
    write_csv(db,pattern,"SELECT * FROM original"); write_csv(db,matched,"SELECT * FROM original");
    multiple=(Analysis){.kind=SOURCE_CSV,.path=pattern}; next=fetch(db,&multiple,0);
    CHECK(next.column_count==8 && !next.records.ids && !next.records.source_key); page_free(&next);
    CHECK(unlink(pattern)==0 && unlink(matched)==0); analysis_free(&multiple); free(matched);
    write_csv(db,path,"SELECT i AS id,'new' AS version FROM range(17) t(i)"); next=fetch(db,&source,0);
    CHECK(page_records_changed(&retained,&next) && next.row_count==17 && next.column_count==2 && next.records.ids[16]==17);
    CHECK(retained.records.ids[255]==256 && retained.column_count==8); page_free(&retained); page_free(&next);
    CHECK(unlink(path)==0); analysis_free(&source); free(error);
}
static void test_demo_pages(Database *db,const char *dir) {
    Analysis source={.kind=SOURCE_DEMO}; char *error=NULL; Page retained=fetch(db,&source,0);
    CHECK(retained.column_count==6 && retained.columns[1].type==DUCKDB_TYPE_DATE && retained.columns[3].type==DUCKDB_TYPE_DECIMAL);
    CHECK(!retained.records.file_identity_valid);
    CHECK(!strcmp(retained.columns[3].type_name,"DECIMAL(12,2)") && retained.cells[5].is_null && retained.records.ids[0]==1);
    for (uint64_t offset=0;offset<=512;offset+=256) check_window_page(db,&source,offset,"","");
    CHECK(analysis_sort_set(&source,"City",false,false,&error));
    check_window_page(db,&source,0,"","ORDER BY City ASC,expected_record ASC");
    check_window_page(db,&source,256,"","ORDER BY City ASC,expected_record ASC"); check_consumers(db,&source,dir);
    CHECK(analysis_filter_set(&source,"Transaction ID",FILTER_GREATER,"600.5",&error));
    check_window_page(db,&source,0,"WHERE \"Transaction ID\">600","ORDER BY City ASC,expected_record ASC");
    Page filtered=fetch(db,&source,0); CHECK(!strcmp(retained.records.source_key,filtered.records.source_key) && filtered.records.ids[0]>601);
    page_free(&filtered); analysis_filters_clear(&source); analysis_sorts_clear(&source); check_window_fallbacks(db,&source);
    CHECK(database_refresh(db,&error)); filtered=fetch(db,&source,512);
    CHECK(!page_records_changed(&retained,&filtered) && !strcmp(retained.records.source_key,filtered.records.source_key) && filtered.records.ids[0]==513);
    page_free(&filtered); page_free(&retained); analysis_free(&source); free(error);
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
static int performance_windows(void) {
    char dir[]="/tmp/duxcel-record-windows-XXXXXX"; CHECK(mkdtemp(dir)!=NULL);
    char *path=dx_printf("%s/demo.csv",dir),*error=NULL; Database setup={0}; CHECK(database_open(&setup,&error));
    Analysis demo={.kind=SOURCE_DEMO}; char *raw=analysis_source_sql(&demo,false),*limited=dx_printf("SELECT * FROM (%s) raw LIMIT 200000",raw);
    write_csv(&setup,path,limited); free(limited); free(raw); database_close(&setup);
    struct stat info; CHECK(stat(path,&info)==0);
    printf("{\"engine\":\"%s\",\"warm_samples\":3,\"csv_rows\":200000,\"csv_bytes\":%" PRIu64 ",\"cases\":[",duckdb_library_version(),(uint64_t)info.st_size);
    const char *cases[]={"first","filter","deep","sort"}; bool comma=false;
    for (int kind=0;kind<2;kind++) for (size_t c=0;c<4;c++) for (int identified=0;identified<2;identified++) {
        Database db={0}; CHECK(database_open(&db,&error)); Analysis source={.kind=kind ? SOURCE_DEMO : SOURCE_CSV,.path=kind ? NULL : dx_strdup(path)};
        if (!identified) CHECK(analysis_query_set(&source,"SELECT * FROM input",&error));
        if (c==1) CHECK(analysis_filter_set(&source,"Amount",FILTER_GREATER,"750",&error));
        if (c==3) CHECK(analysis_sort_set(&source,"City",false,false,&error));
        double cold=0,samples[3]; size_t bytes=0,metadata=0; uint64_t offset=c==2 ? 150000 : 0;
        for (int run=0;run<4;run++) {
            Page page=fetch(&db,&source,offset); CHECK(page.column_count==6 && page.row_count==256);
            CHECK(identified ? page.records.count==page.row_count : !page.records.source_key);
            if (run) samples[run-1]=page.query_ms; else cold=page.query_ms;
            bytes=page.bytes; metadata=page.records.count*sizeof(uint64_t)+(page.records.source_key ? strlen(page.records.source_key)+1 : 0); page_free(&page);
        }
        qsort(samples,3,sizeof(*samples),compare_ms);
        printf("%s{\"source\":\"%s\",\"case\":\"%s\",\"records\":%s,\"session_first_ms\":%.3f,\"warm_median_ms\":%.3f,\"page_bytes\":%zu,\"metadata_bytes\":%zu,\"page_prepares\":%" PRIu64 ",\"queries\":%" PRIu64 "}",comma ? "," : "",kind ? "demo" : "csv",cases[c],identified ? "true" : "false",cold,samples[1],bytes,metadata,db.page_prepares,db.query_count);
        comma=true; database_close(&db); analysis_free(&source);
    }
    puts("]}"); CHECK(unlink(path)==0 && rmdir(dir)==0); free(path); free(error); return 0;
}
int main(int argc,char **argv) {
    if (argc==3 && !strcmp(argv[1],"--perf")) return performance(argv[2]);
    if (argc==2 && !strcmp(argv[1],"--perf-windows")) return performance_windows();
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
    test_fallbacks(&db,path,dir); test_optional_budget(&db,dir); test_observed_versions(&db,dir); test_csv_pages(&db,dir); test_demo_pages(&db,dir);
    test_bound_consumers(&db,dir,SOURCE_PARQUET); test_bound_consumers(&db,dir,SOURCE_CSV);
    test_exact_file_observation(&db,dir,SOURCE_PARQUET); test_exact_file_observation(&db,dir,SOURCE_CSV);
    CHECK(database_refresh(&db,&error)); CHECK(retained.records.ids[255]==256 && !strcmp(retained.records.source_key,scope));
    Page moved=retained; retained=(Page){0}; database_close(&db); page_free(&retained); page_free(&moved); free(scope);
    CHECK(unlink(path)==0); analysis_free(&source); CHECK(rmdir(dir)==0); free(error);
    printf("PASS: %u native record-page checks\n",checks); return 0;
}
