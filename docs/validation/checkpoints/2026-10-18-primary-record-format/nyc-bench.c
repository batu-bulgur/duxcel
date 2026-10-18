#define _DARWIN_C_SOURCE
#include "data/database.h"
#include "platform/platform.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>

static void need(bool ok,const char *why,const char *error) {
    if (!ok) { fprintf(stderr,"FAIL: %s: %s\n",why,error ? error : ""); exit(1); }
}
static void hex(char out[65],const unsigned char in[32]) {
    static const char digits[]="0123456789abcdef";
    for (size_t i=0;i<32;i++) { out[i*2]=digits[in[i]>>4]; out[i*2+1]=digits[in[i]&15]; } out[64]=0;
}
static void identity(const SourceFileIdentity *p) {
    printf("{\"device\":\"%" PRIu64 "\",\"inode\":\"%" PRIu64 "\",\"size\":\"%" PRIu64 "\",\"links\":\"%" PRIu64 "\",\"mode\":%u,\"uid\":%u,\"gid\":%u,\"flags\":%u,\"generation\":%u,\"mtime_s\":\"%" PRId64 "\",\"mtime_ns\":%u,\"ctime_s\":\"%" PRId64 "\",\"ctime_ns\":%u,\"birth_s\":\"%" PRId64 "\",\"birth_ns\":%u}",p->device,p->inode,p->size,p->links,p->mode,p->uid,p->gid,p->flags,p->generation,p->modified_seconds,p->modified_nanoseconds,p->changed_seconds,p->changed_nanoseconds,p->created_seconds,p->created_nanoseconds);
}
static void digest(const char *path,unsigned pass,SourceDigest *out) {
    char error[DX_SOURCE_DIGEST_ERROR_BYTES]={0},sha[65]; double start=dx_millis();
    SourceDigestStatus status=source_digest_file(path,NULL,NULL,out,error); double ms=dx_millis()-start;
    need(status==SOURCE_DIGEST_OK,"complete native SDK digest",error); hex(sha,out->sha256);
    printf("{\"phase\":\"digest\",\"pass\":%u,\"bytes\":%" PRIu64 ",\"ms\":%.6f,\"sha256\":\"%s\",\"stamp\":\"%" PRIu64 "\",\"identity\":",pass,out->bytes,ms,sha,out->observed_stamp); identity(&out->identity); puts("}"); fflush(stdout);
}
int main(int argc,char **argv) {
    need(argc==2,"one read-only source path",NULL); SourceDigest proof={0},after={0}; digest(argv[1],0,&proof);
    Database db={0}; char *error=NULL; double opened=dx_millis(); need(database_open(&db,&error),"private normal Database",error); double open_ms=dx_millis()-opened;
    duckdb_result setup={0}; need(duckdb_query(db.connection,"SET memory_limit='384MiB'",&setup)==DuckDBSuccess,"stricter private memory limit",duckdb_result_error(&setup)); duckdb_destroy_result(&setup);
    need(duckdb_query(db.connection,"SELECT current_setting('memory_limit'),current_setting('threads'),current_setting('preserve_insertion_order')",&setup)==DuckDBSuccess,"actual settings",duckdb_result_error(&setup));
    char *limit=duckdb_value_varchar(&setup,0,0),*threads=duckdb_value_varchar(&setup,1,0),*order=duckdb_value_varchar(&setup,2,0);
    printf("{\"phase\":\"open\",\"ms\":%.6f,\"engine\":\"%s\",\"memory_limit\":\"%s\",\"threads\":\"%s\",\"preserve_insertion_order\":\"%s\",\"private_temp\":\"%s\",\"setup_direct_sql\":2}\n",open_ms,duckdb_library_version(),limit,threads,order,db.temp_directory);
    duckdb_free(limit); duckdb_free(threads); duckdb_free(order); duckdb_destroy_result(&setup);
    const char *names[]={"unsorted_first_20k","unsorted_offset_1000000_20k","date_filter_amount_desc_20k"};
    for (unsigned pass=0;pass<2;pass++) for (unsigned item=0;item<3;item++) {
        Analysis source={.kind=SOURCE_PARQUET,.path=dx_strdup(argv[1])}; RecordSelection records={0}; uint64_t first=item==1 ? UINT64_C(1000000) : 0;
        if (item==2) {
            char *dates[]={"2025-01-15 00:00:00","2025-01-15 23:59:59.999999"}; char *json=analysis_values_json(dates,2);
            need(analysis_filter_set(&source,"tpep_pickup_datetime",FILTER_BETWEEN,json,&error),"actual date condition",error); free(json);
            need(analysis_sort_set(&source,"total_amount",true,false,&error),"actual descending sort",error);
        }
        uint64_t sql=db.query_count,counts=db.count_queries; double start=dx_millis();
        need(database_collect_records(&db,&source,first,20000,&proof.identity,proof.observed_stamp,&records,&error),names[item],error); double ms=dx_millis()-start;
        need(records.count==20000 && records.bytes==160000 && records.ids && source_file_identity_same(&records.file_identity,&proof.identity) && records.stamp==proof.observed_stamp,"bounded exact result",error);
        uint64_t hash=UINT64_C(14695981039346656037);
        for (size_t i=0;i<records.count;i++) {
            need(records.ids[i] && (!i || records.ids[i]>records.ids[i-1]),"sorted unique positive IDs",NULL);
            if (item<2) need(records.ids[i]==first+i+1,"original unsorted ordinals",NULL);
            for (unsigned b=0;b<8;b++) hash=(hash^((records.ids[i]>>(8*b))&255))*UINT64_C(1099511628211);
        }
        struct rusage usage={0}; need(getrusage(RUSAGE_SELF,&usage)==0,"process highwater",NULL);
        printf("{\"phase\":\"collect\",\"case\":\"%s\",\"pass\":%u,\"ms\":%.6f,\"offset\":%" PRIu64 ",\"requested\":20000,\"ids\":%zu,\"vector_bytes\":%zu,\"min_id\":\"%" PRIu64 "\",\"max_id\":\"%" PRIu64 "\",\"membership_fnv64\":\"%016" PRIx64 "\",\"sql_executions\":%" PRIu64 ",\"count_executions\":%" PRIu64 ",\"process_highwater_bytes\":%ld}\n",names[item],pass,ms,first,records.count,records.bytes,records.ids[0],records.ids[records.count-1],hash,db.query_count-sql,db.count_queries-counts,usage.ru_maxrss); fflush(stdout);
        record_selection_free(&records); analysis_free(&source);
    }
    printf("{\"phase\":\"total\",\"collector_sql_executions\":%" PRIu64 ",\"collector_count_executions\":%" PRIu64 ",\"business_pages_requested\":0}\n",db.query_count,db.count_queries);
    database_close(&db); free(error); digest(argv[1],1,&after);
    need(proof.bytes==after.bytes && proof.observed_stamp==after.observed_stamp && source_file_identity_same(&proof.identity,&after.identity) && !memcmp(proof.sha256,after.sha256,32),"source unchanged through benchmark",NULL);
    puts("{\"phase\":\"complete\",\"source_unchanged\":true}"); return 0;
}
