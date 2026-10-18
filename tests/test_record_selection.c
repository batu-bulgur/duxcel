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
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL record selection:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void clear_error(char **error) { CHECK(*error && **error); free(*error); *error=NULL; }
static void command(Database *db,const char *sql) {
    duckdb_result result={0}; char *error=NULL;
    if (!database_execute(db,sql,&result,&error)) { fprintf(stderr,"DuckDB: %s\n",error); exit(1); }
    duckdb_destroy_result(&result); CHECK(!error);
}
static void write_source(Database *db,const char *path,SourceKind kind,size_t rows) {
    char *quoted=dx_quote(path,'\''),*sql=dx_printf("COPY (SELECT i//2 AS duplicate_pair, (i//2)%%7 AS tie, CASE WHEN (i//2)%%3=0 THEN NULL WHEN (i//2)%%3=1 THEN '' ELSE 'NULL' END AS text_value, 9007199254740993::BIGINT AS exact_integer, 12345678901234567890.1250::DECIMAL(38,4) AS money, repeat('x',128) AS payload FROM range(%zu) t(i)) TO %s (FORMAT %s%s)",rows,quoted,kind==SOURCE_CSV ? "CSV" : "PARQUET",kind==SOURCE_CSV ? ",HEADER true" : "");
    command(db,sql); free(quoted); free(sql);
}
static SourceFileIdentity observation(const char *path) {
    SourceFileIdentity identity={0}; CHECK(source_file_identity(path,&identity,NULL)==SOURCE_DIGEST_OK); return identity;
}
static void preserved_failure(Database *db,Analysis *a,uint64_t first,uint64_t rows,const SourceFileIdentity *identity,uint64_t stamp,RecordSelection *out) {
    RecordSelection before=*out; uint64_t saved=out->count ? out->ids[0] : 0; char *error=NULL;
    CHECK(!database_collect_records(db,a,first,rows,identity,stamp,out,&error)); clear_error(&error);
    CHECK(!memcmp(&before,out,sizeof(before)) && (!out->count || out->ids[0]==saved));
}
static void collect(Database *db,Analysis *a,uint64_t first,uint64_t rows,const SourceFileIdentity *identity,RecordSelection *out) {
    uint64_t queries=db->query_count,counts=db->count_queries; char *error=NULL;
    CHECK(database_collect_records(db,a,first,rows,identity,dx_file_stamp(a->path),out,&error) && !error);
    CHECK(db->query_count==queries+1 && db->count_queries==counts);
    CHECK(out->count<=DX_RECORD_SELECTION_ROWS && out->bytes==out->count*sizeof(uint64_t) && (!out->count ? !out->ids : out->ids!=NULL));
    CHECK(source_file_identity_same(&out->file_identity,identity) && out->stamp==dx_file_stamp(a->path));
    for (size_t i=0;i<out->count;i++) CHECK(out->ids[i] && (!i || out->ids[i-1]<out->ids[i]));
}
typedef struct { Database *db; uint64_t queries; bool cancel,changed; const char *path; } Interruption;
static bool interrupt_query(void *context) {
    Interruption *state=context;
    if (state->db->query_count<=state->queries) return false;
    if (state->path && !state->changed) {
        struct stat file; CHECK(stat(state->path,&file)==0); CHECK(chmod(state->path,(file.st_mode&0777)^S_IXUSR)==0); state->changed=true;
    }
    return state->cancel;
}
static void test_reader(Database *db,const char *path,SourceKind kind) {
    Analysis a={.kind=kind,.path=dx_strdup(path)}; SourceFileIdentity identity=observation(path); RecordSelection selected={0}; char *error=NULL;
    collect(db,&a,240,350,&identity,&selected); CHECK(selected.count==350);
    for (size_t i=0;i<selected.count;i++) CHECK(selected.ids[i]==241+i);
    /* Loaded Pages and the collector share deterministic tied ordering, while
       membership is returned in original-ID order across several page spans. */
    CHECK(analysis_sort_set(&a,"tie",true,false,&error)); CHECK(analysis_filter_set(&a,"duplicate_pair",FILTER_LESS,"260",&error));
    Page page={0}; CHECK(database_fetch_table_window(db,&a,240,-1,&page,&error));
    collect(db,&a,240,350,&identity,&selected); uint64_t expected[600]; size_t count=0;
    for (int tie=6;tie>=0;tie--) for (uint64_t i=0;i<600;i++) if ((i/2)%7==(uint64_t)tie && i/2<260) expected[count++]=i+1;
    size_t end=count<590 ? count : 590,member_count=end>240 ? end-240 : 0;
    CHECK(selected.count==member_count && page.row_count<=selected.count);
    for (size_t i=0;i<selected.count;i++) {
        bool present=false; for (size_t j=240;j<end;j++) present|=selected.ids[i]==expected[j]; CHECK(present);
    }
    for (size_t i=0;i<page.row_count;i++) { uint64_t id=0; CHECK(page_record_id(&page,page.offset+i,&id)); bool found=false; for (size_t j=0;j<selected.count;j++) found|=selected.ids[j]==id; CHECK(found); }
    page_free(&page); analysis_filters_clear(&a); analysis_sorts_clear(&a);
    collect(db,&a,599,20,&identity,&selected); CHECK(selected.count==1 && selected.ids[0]==600);
    collect(db,&a,700,20,&identity,&selected); CHECK(!selected.count && !selected.bytes && !selected.ids);
    collect(db,&a,0,4,&identity,&selected); CHECK(selected.count==4); /* All physical duplicates retain distinct ordinals. */
    uint64_t queries=db->query_count;
    preserved_failure(db,&a,0,0,&identity,dx_file_stamp(path),&selected);
    preserved_failure(db,&a,DX_SHEET_ROWS-1,2,&identity,dx_file_stamp(path),&selected);
    preserved_failure(db,&a,0,1,NULL,dx_file_stamp(path),&selected);
    preserved_failure(db,&a,0,1,&identity,0,&selected); CHECK(db->query_count==queries);
    a.query=dx_strdup("SELECT * FROM input"); preserved_failure(db,&a,0,1,&identity,dx_file_stamp(path),&selected); free(a.query); a.query=NULL;
    a.unique_rows=true; preserved_failure(db,&a,0,1,&identity,dx_file_stamp(path),&selected); a.unique_rows=false;
    a.group_column=dx_strdup("tie"); preserved_failure(db,&a,0,1,&identity,dx_file_stamp(path),&selected); free(a.group_column); a.group_column=NULL;
    a.kind=SOURCE_DEMO; preserved_failure(db,&a,0,1,&identity,dx_file_stamp(path),&selected); a.kind=kind;
    char *actual=a.path; a.path=dx_strdup("/tmp/*.parquet"); preserved_failure(db,&a,0,1,&identity,dx_file_stamp(path),&selected); free(a.path); a.path=actual;
    CHECK(db->query_count==queries);
    SourceFileIdentity changed=identity; changed.flags^=1;
    preserved_failure(db,&a,0,1,&changed,dx_file_stamp(path),&selected); CHECK(db->query_count==queries);
    char *link=dx_printf("%s.link",path); CHECK(symlink(path,link)==0); actual=a.path; a.path=link;
    preserved_failure(db,&a,0,1,&identity,dx_file_stamp(path),&selected); CHECK(db->query_count==queries);
    a.path=actual; CHECK(unlink(link)==0); free(link);
    Interruption interruption={.db=db,.queries=queries,.cancel=true}; db->cancelled=interrupt_query; db->cancel_context=&interruption;
    preserved_failure(db,&a,0,500,&identity,dx_file_stamp(path),&selected); CHECK(db->query_count==queries+1);
    db->cancelled=NULL; db->cancel_context=NULL; collect(db,&a,0,350,&identity,&selected); CHECK(selected.count==350);
    /* A metadata-only change keeps the old cache stamp, but exact before/after
       observation must reject it and preserve the old owned membership. */
    uint64_t stamp=dx_file_stamp(path); interruption=(Interruption){.db=db,.queries=db->query_count,.path=path}; db->cancelled=interrupt_query; db->cancel_context=&interruption;
    preserved_failure(db,&a,0,500,&identity,stamp,&selected); CHECK(interruption.changed && dx_file_stamp(path)==stamp);
    db->cancelled=NULL; db->cancel_context=NULL; SourceFileIdentity refreshed=observation(path); CHECK(!source_file_identity_same(&identity,&refreshed));
    queries=db->query_count; preserved_failure(db,&a,0,500,&identity,stamp,&selected); CHECK(db->query_count==queries);
    collect(db,&a,0,500,&refreshed,&selected); CHECK(selected.count==500);
    record_selection_free(&selected); CHECK(!selected.ids && !selected.count && !selected.bytes && !selected.stamp && !selected.file_identity.inode); record_selection_free(&selected);
    analysis_free(&a); free(error);
}
static void test_limit(Database *db,const char *path,SourceKind kind) {
    write_source(db,path,kind,DX_RECORD_SELECTION_ROWS+1); Analysis source={.kind=kind,.path=dx_strdup(path)}; SourceFileIdentity identity=observation(path); RecordSelection selected={0};
    collect(db,&source,0,DX_RECORD_SELECTION_ROWS,&identity,&selected); CHECK(selected.count==20000 && selected.ids[19999]==20000);
    uint64_t queries=db->query_count,counts=db->count_queries;
    preserved_failure(db,&source,0,DX_RECORD_SELECTION_ROWS+1,&identity,dx_file_stamp(path),&selected);
    CHECK(db->query_count==queries+1 && db->count_queries==counts && selected.count==20000);
    collect(db,&source,1,DX_RECORD_SELECTION_ROWS+1,&identity,&selected); CHECK(selected.count==20000 && selected.ids[0]==2 && selected.ids[19999]==20001);
    record_selection_free(&selected); analysis_free(&source);
}
static void test_reader_collision(Database *db,const char *directory) {
    char *path=dx_printf("%s/collision.parquet",directory),*quoted=dx_quote(path,'\''),*sql=dx_printf("COPY (SELECT 42 AS FiLe_RoW_NuMbEr,17 AS value) TO %s (FORMAT PARQUET)",quoted);
    command(db,sql); free(sql); free(quoted); Analysis source={.kind=SOURCE_PARQUET,.path=path}; SourceFileIdentity identity=observation(path);
    RecordSelection selected={.ids=dx_alloc(sizeof(uint64_t)),.count=1,.bytes=sizeof(uint64_t)}; selected.ids[0]=UINT64_MAX;
    uint64_t queries=db->query_count; preserved_failure(db,&source,0,1,&identity,dx_file_stamp(path),&selected);
    CHECK(db->query_count==queries && selected.ids[0]==UINT64_MAX); record_selection_free(&selected); CHECK(unlink(path)==0); analysis_free(&source);
}
int main(void) {
    char directory[]="/tmp/duxcel-record-selection-XXXXXX"; CHECK(mkdtemp(directory)); Database db={0}; char *error=NULL; CHECK(database_open(&db,&error));
    const SourceKind kinds[]={SOURCE_CSV,SOURCE_PARQUET};
    for (size_t i=0;i<2;i++) {
        char *path=dx_printf("%s/source.%s",directory,i ? "parquet" : "csv"); write_source(&db,path,kinds[i],600); test_reader(&db,path,kinds[i]); CHECK(unlink(path)==0);
        test_limit(&db,path,kinds[i]); CHECK(unlink(path)==0); free(path);
    }
    test_reader_collision(&db,directory); database_close(&db); CHECK(rmdir(directory)==0); free(error); printf("PASS: %u finite raw-record selection checks\n",checks); return 0;
}
