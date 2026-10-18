#define _POSIX_C_SOURCE 200809L
#include <duckdb.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static double ms(void){struct timespec t;CHECK(!clock_gettime(CLOCK_MONOTONIC,&t));return t.tv_sec*1000.0+t.tv_nsec/1e6;}
static char *read_file(const char *directory,const char *name){char path[4096];CHECK(snprintf(path,sizeof(path),"%s/%s",directory,name)>0);FILE *f=fopen(path,"rb");CHECK(f&&!fseek(f,0,SEEK_END));long n=ftell(f);CHECK(n>0&&n<32768&&!fseek(f,0,SEEK_SET));char *s=malloc((size_t)n+1);CHECK(s&&fread(s,1,(size_t)n,f)==(size_t)n&&!fclose(f));s[n]=0;return s;}
static void exec(duckdb_connection c,const char *sql){duckdb_result r={0};if(duckdb_query(c,sql,&r)!=DuckDBSuccess){fprintf(stderr,"setup error: %s\nSQL:%s\n",duckdb_result_error(&r),sql);exit(1);}duckdb_destroy_result(&r);}
static bool boolean(duckdb_connection c,const char *sql,double *elapsed){duckdb_result r={0};double start=ms();duckdb_state state=duckdb_query(c,sql,&r);double end=ms();if(state!=DuckDBSuccess){fprintf(stderr,"query error: %s\n",duckdb_result_error(&r));exit(1);}CHECK(duckdb_row_count(&r)==1&&duckdb_column_count(&r)==1&&duckdb_column_type(&r,0)==DUCKDB_TYPE_BOOLEAN&&!duckdb_value_is_null(&r,0,0));bool value=duckdb_value_boolean(&r,0,0);duckdb_destroy_result(&r);if(elapsed)*elapsed=end-start;return value;}
static const char ddl[]="CREATE SCHEMA _duxcel_workspace;"
 "CREATE TABLE _duxcel_workspace.source_versions(sheet_id INTEGER,source_reference VARCHAR,definition BLOB);"
 "CREATE TABLE _duxcel_workspace.sheets(path VARCHAR);CREATE TABLE _duxcel_workspace.table_sources(path VARCHAR);"
 "CREATE TABLE _duxcel_workspace.format_scopes(sheet_id INTEGER,definition BLOB);"
 "CREATE TABLE _duxcel_workspace.format_guards(sheet_id INTEGER,position INTEGER);"
 "CREATE TABLE _duxcel_workspace.format_records(sheet_id INTEGER,position INTEGER);"
 "CREATE TABLE _duxcel_workspace.format_fields(sheet_id INTEGER,position INTEGER);";
static const char reset[]="DELETE FROM _duxcel_workspace.source_versions;DELETE FROM _duxcel_workspace.sheets;DELETE FROM _duxcel_workspace.table_sources;DELETE FROM _duxcel_workspace.format_scopes;DELETE FROM _duxcel_workspace.format_guards;DELETE FROM _duxcel_workspace.format_records;DELETE FROM _duxcel_workspace.format_fields;"
 "INSERT INTO _duxcel_workspace.source_versions VALUES(0,'source.csv',encode('raw'));"
 "INSERT INTO _duxcel_workspace.sheets VALUES('source.csv');INSERT INTO _duxcel_workspace.table_sources VALUES('table.csv');"
 "INSERT INTO _duxcel_workspace.format_scopes VALUES(0,encode('result'));INSERT INTO _duxcel_workspace.format_guards VALUES(0,0);"
 "INSERT INTO _duxcel_workspace.format_records VALUES(0,0);INSERT INTO _duxcel_workspace.format_fields VALUES(0,0);";
static void open_database(duckdb_database *db,duckdb_connection *c,const char *temp){duckdb_config config=NULL;char *error=NULL;CHECK(duckdb_create_config(&config)==DuckDBSuccess);CHECK(duckdb_set_config(config,"memory_limit","256MB")==DuckDBSuccess&&duckdb_set_config(config,"threads","2")==DuckDBSuccess&&duckdb_set_config(config,"temp_directory",temp)==DuckDBSuccess);duckdb_state state=duckdb_open_ext(NULL,db,config,&error);if(state!=DuckDBSuccess)fprintf(stderr,"open:%s\n",error);CHECK(state==DuckDBSuccess);duckdb_free(error);duckdb_destroy_config(&config);CHECK(duckdb_connect(*db,c)==DuckDBSuccess);exec(*c,ddl);exec(*c,reset);}
typedef struct{const char *name,*sql;bool expected;} Case;
static void error_case(duckdb_connection c,const char *sql,const char *name){duckdb_result r={0};CHECK(duckdb_query(c,sql,&r)==DuckDBError);CHECK(duckdb_result_error(&r)&&*duckdb_result_error(&r));printf("{\"kind\":\"expected_error\",\"case\":\"%s\"}\n",name);duckdb_destroy_result(&r);}
int main(int argc,char **argv){CHECK(argc==3);char *old=read_file(argv[1],"old.sql"),*candidate=read_file(argv[1],"union.sql"),*predicates[9];for(size_t i=0;i<9;i++){char name[40];snprintf(name,sizeof(name),"predicate-%zu.sql",i);predicates[i]=read_file(argv[1],name);}
 duckdb_database db[2]={0};duckdb_connection c[2]={0};char temp[2][4096];for(size_t i=0;i<2;i++){CHECK(snprintf(temp[i],sizeof(temp[i]),"%s/%zu",argv[2],i)>0);open_database(&db[i],&c[i],temp[i]);}
 printf("{\"kind\":\"config\",\"sdk\":\"%s\",\"memory_limit\":\"256MB\",\"threads\":2,\"schema\":\"private_in_memory_7tables\"}\n",duckdb_library_version());
 const char *queries[]={old,candidate};const size_t order[]={0,1,1,0};const char *variant[]={"old_or","union_bool_or"};
 for(size_t i=0;i<4;i++){size_t k=order[i];double elapsed=0;CHECK(!boolean(c[k],queries[k],&elapsed));printf("{\"kind\":\"timing\",\"variant\":\"%s\",\"temperature\":\"%s\",\"milliseconds\":%.6f,\"invalid\":false}\n",variant[k],i<2?"cold":"warm",elapsed);fflush(stdout);}
 const Case cases[]={
 {"populated benign",NULL,false},
 {"source_versions1024","DELETE FROM _duxcel_workspace.source_versions;INSERT INTO _duxcel_workspace.source_versions SELECT 0,'source.csv',encode('raw') FROM range(1024)",false},
 {"source_versions1025","DELETE FROM _duxcel_workspace.source_versions;INSERT INTO _duxcel_workspace.source_versions SELECT 0,'source.csv',encode('raw') FROM range(1025)",true},
 {"source_reference65536","UPDATE _duxcel_workspace.source_versions SET source_reference=repeat('x',65536)",false},
 {"source_reference65537","UPDATE _duxcel_workspace.source_versions SET source_reference=repeat('x',65537)",true},
 {"source_referenceNULL","UPDATE _duxcel_workspace.source_versions SET source_reference=NULL",true},
 {"source_referenceNUL","UPDATE _duxcel_workspace.source_versions SET source_reference='a'||chr(0)||'b'",true},
 {"raw_definition262143","UPDATE _duxcel_workspace.source_versions SET definition=encode(repeat('x',262143))",false},
 {"raw_definition262144","UPDATE _duxcel_workspace.source_versions SET definition=encode(repeat('x',262144))",true},
 {"pending_raw_definitionNULL","UPDATE _duxcel_workspace.source_versions SET definition=NULL",false},
 {"primary_path65536","UPDATE _duxcel_workspace.sheets SET path=repeat('x',65536)",false},
 {"primary_path65537","UPDATE _duxcel_workspace.sheets SET path=repeat('x',65537)",true},
 {"primary_pathNULL","UPDATE _duxcel_workspace.sheets SET path=NULL",true},
 {"primary_pathNUL","UPDATE _duxcel_workspace.sheets SET path='a'||chr(0)||'b'",true},
 {"table_path65536","UPDATE _duxcel_workspace.table_sources SET path=repeat('x',65536)",false},
 {"table_path65537","UPDATE _duxcel_workspace.table_sources SET path=repeat('x',65537)",true},
 {"table_pathNULL","UPDATE _duxcel_workspace.table_sources SET path=NULL",true},
 {"table_pathNUL","UPDATE _duxcel_workspace.table_sources SET path='a'||chr(0)||'b'",true},
 {"scopes512","DELETE FROM _duxcel_workspace.format_scopes;INSERT INTO _duxcel_workspace.format_scopes SELECT 0,encode('result') FROM range(512)",false},
 {"scopes513","DELETE FROM _duxcel_workspace.format_scopes;INSERT INTO _duxcel_workspace.format_scopes SELECT 0,encode('result') FROM range(513)",true},
 {"scope_definition262143","UPDATE _duxcel_workspace.format_scopes SET definition=encode(repeat('x',262143))",false},
 {"scope_definition262144","UPDATE _duxcel_workspace.format_scopes SET definition=encode(repeat('x',262144))",true},
 {"scope_definitionNULL","UPDATE _duxcel_workspace.format_scopes SET definition=NULL",true},
 {"guards33","DELETE FROM _duxcel_workspace.format_guards;INSERT INTO _duxcel_workspace.format_guards SELECT 0,0 FROM range(33)",false},
 {"guards34","DELETE FROM _duxcel_workspace.format_guards;INSERT INTO _duxcel_workspace.format_guards SELECT 0,0 FROM range(34)",true},
 {"records20000","DELETE FROM _duxcel_workspace.format_records;INSERT INTO _duxcel_workspace.format_records SELECT 0,0 FROM range(20000)",false},
 {"records20001","DELETE FROM _duxcel_workspace.format_records;INSERT INTO _duxcel_workspace.format_records SELECT 0,0 FROM range(20001)",true},
 {"fields16384","DELETE FROM _duxcel_workspace.format_fields;INSERT INTO _duxcel_workspace.format_fields SELECT 0,0 FROM range(16384)",false},
 {"fields16385","DELETE FROM _duxcel_workspace.format_fields;INSERT INTO _duxcel_workspace.format_fields SELECT 0,0 FROM range(16385)",true},
 {"empty","DELETE FROM _duxcel_workspace.source_versions;DELETE FROM _duxcel_workspace.sheets;DELETE FROM _duxcel_workspace.table_sources;DELETE FROM _duxcel_workspace.format_scopes;DELETE FROM _duxcel_workspace.format_guards;DELETE FROM _duxcel_workspace.format_records;DELETE FROM _duxcel_workspace.format_fields",false}};
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);i++){exec(c[1],reset);if(cases[i].sql)exec(c[1],cases[i].sql);bool expected_or=false;for(size_t j=0;j<9;j++)expected_or|=boolean(c[1],predicates[j],NULL);bool actual=boolean(c[1],candidate,NULL);CHECK(actual==cases[i].expected&&actual==expected_or);printf("{\"kind\":\"correctness\",\"case\":\"%s\",\"invalid\":%s,\"unchanged_predicates\":9}\n",cases[i].name,actual?"true":"false");}
 exec(c[1],"DROP TABLE _duxcel_workspace.format_fields");error_case(c[1],old,"old_missing_table");error_case(c[1],candidate,"union_missing_table");
 exec(c[1],"CREATE TABLE _duxcel_workspace.format_fields(sheet_id INTEGER,position INTEGER);ALTER TABLE _duxcel_workspace.source_versions DROP COLUMN definition;ALTER TABLE _duxcel_workspace.source_versions ADD COLUMN definition INTEGER");error_case(c[1],old,"old_incompatible_blob_type");error_case(c[1],candidate,"union_incompatible_blob_type");
 for(size_t i=0;i<2;i++){duckdb_disconnect(&c[i]);duckdb_close(&db[i]);}for(size_t i=0;i<9;i++)free(predicates[i]);free(old);free(candidate);printf("{\"kind\":\"passed\",\"checks\":%u,\"cases\":%zu,\"timed_queries\":4,\"predicate_order_unchanged\":true}\n",checks,sizeof(cases)/sizeof(*cases));return 0;}
