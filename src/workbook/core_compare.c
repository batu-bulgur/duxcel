#include "core_compare.h"
#include "workbook_codec.h"
#include "../platform/platform.h"
#include <duckdb.h>
#include <stdlib.h>
#include <string.h>
typedef struct { WorkbookCoreCompareControl control; WorkbookCoreComparePhase phase; size_t table; bool stopped; } Compare;
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message ? message : "Core payload comparison failed."); } return false; }
static bool stopped(Compare *c,char **error) {
    if (!c->stopped && c->control.cancelled) c->stopped=c->control.cancelled(c->control.context,c->phase,c->table);
    if (c->stopped) fail(error,"Core payload comparison was cancelled.");
    return c->stopped;
}
static bool hash_cancel(void *context,uint64_t bytes) { (void)bytes; return stopped(context,NULL); }
static WorkbookCorePayloadResult verify(const WorkbookCorePayload *p,Compare *c,char **error) {
    if (stopped(c,error)) return CORE_PAYLOAD_CANCELLED;
    return workbook_core_payload_verify(p,hash_cancel,c,error);
}
static bool query(duckdb_connection connection,const char *sql,Compare *c,duckdb_result *out,char **error) {
    duckdb_prepared_statement statement=NULL; duckdb_pending_result pending=NULL; bool ok=false;
    if (stopped(c,error)) goto done;
    if (duckdb_prepare(connection,sql,&statement)!=DuckDBSuccess) { fail(error,duckdb_prepare_error(statement)); goto done; }
    if (duckdb_pending_prepared(statement,&pending)!=DuckDBSuccess) { fail(error,pending ? duckdb_pending_error(pending) : "Cannot prepare core comparison."); goto done; }
    for (;;) {
        if (stopped(c,error)) { duckdb_interrupt(connection); goto done; }
        duckdb_pending_state state=duckdb_pending_execute_task(pending);
        if (state==DUCKDB_PENDING_ERROR) { fail(error,duckdb_pending_error(pending)); goto done; }
        if (state==DUCKDB_PENDING_RESULT_READY) break;
        if (state==DUCKDB_PENDING_NO_TASKS_AVAILABLE) dx_sleep_ms(1);
    }
    if (stopped(c,error)) goto done;
    ok=duckdb_execute_pending(pending,out)==DuckDBSuccess;
    if (!ok) fail(error,duckdb_result_error(out));
done:
    if (pending) duckdb_destroy_pending(&pending);
    if (statement) duckdb_destroy_prepare(&statement);
    return ok;
}
WorkbookCorePayloadResult workbook_core_payload_equal(const WorkbookCorePayload *left,const WorkbookCorePayload *right,
    WorkbookCoreCompareControl control,bool *equal,char **error) {
    const WorkbookCorePayloadInfo *a=workbook_core_payload_info(left),*b=workbook_core_payload_info(right);
    if (!a || !b || !equal || a->encoding!=15 || b->encoding!=15) { fail(error,"Choose two live supported core payloads and an equality result."); return CORE_PAYLOAD_FAILED; }
    Compare compare={.control=control,.phase=CORE_COMPARE_VERIFY};
    WorkbookCorePayloadResult status=verify(left,&compare,error); if (status!=CORE_PAYLOAD_READY) return status;
    status=verify(right,&compare,error); if (status!=CORE_PAYLOAD_READY) return status;
    bool same=a->document.high==b->document.high && a->document.low==b->document.low;
    duckdb_config config=NULL; duckdb_database db=NULL; duckdb_connection connection=NULL;
    char *temporary=NULL,*message=NULL; duckdb_result result={0}; status=CORE_PAYLOAD_FAILED;
    if (same && left!=right) {
        temporary=dx_temp_directory();
        if (!temporary || duckdb_create_config(&config)!=DuckDBSuccess ||
            duckdb_set_config(config,"access_mode","READ_ONLY")!=DuckDBSuccess ||
            duckdb_set_config(config,"memory_limit","64MB")!=DuckDBSuccess ||
            duckdb_set_config(config,"threads","1")!=DuckDBSuccess ||
            duckdb_set_config(config,"temp_directory",temporary)!=DuckDBSuccess) { fail(error,"Cannot configure bounded core comparison."); goto done; }
        if (duckdb_open_ext(workbook_core_payload_path(left),&db,config,&message)!=DuckDBSuccess) { fail(error,message); goto done; }
        if (duckdb_connect(db,&connection)!=DuckDBSuccess) { fail(error,"Cannot connect to the sealed core payload."); goto done; }
        compare.phase=CORE_COMPARE_ROWS;
        char *quoted=dx_quote(workbook_core_payload_path(right),'\'');
        char *sql=dx_printf("ATTACH %s AS dx_other (READ_ONLY)",quoted); free(quoted);
        bool ok=query(connection,sql,&compare,&result,error); free(sql); duckdb_destroy_result(&result); if (!ok) goto done;
        size_t count=workbook_codec_table_count(15);
        for (size_t i=0;i<count;i++) {
            compare.table=i; const WorkbookCodecTable *table=workbook_codec_table_at(15,i);
            sql=dx_printf("SELECT NOT EXISTS ((SELECT %s FROM _duxcel_workspace.%s EXCEPT ALL SELECT %s FROM dx_other._duxcel_workspace.%s) UNION ALL (SELECT %s FROM dx_other._duxcel_workspace.%s EXCEPT ALL SELECT %s FROM _duxcel_workspace.%s))",table->columns,table->table,table->columns,table->table,table->columns,table->table,table->columns,table->table);
            ok=query(connection,sql,&compare,&result,error); free(sql);
            if (ok && (duckdb_row_count(&result)!=1 || duckdb_column_count(&result)!=1 || duckdb_value_is_null(&result,0,0))) ok=fail(error,"Invalid core comparison result.");
            if (ok) same &= duckdb_value_boolean(&result,0,0);
            duckdb_destroy_result(&result); if (!ok) goto done;
        }
    }
    status=CORE_PAYLOAD_READY;
done:
    duckdb_free(message); duckdb_destroy_result(&result);
    if (connection) duckdb_disconnect(&connection); if (db) duckdb_close(&db); if (config) duckdb_destroy_config(&config);
    dx_remove_temp_directory(temporary); free(temporary);
    if (compare.stopped) return CORE_PAYLOAD_CANCELLED;
    if (status!=CORE_PAYLOAD_READY) return status;
    compare.phase=CORE_COMPARE_RECHECK; compare.table=0;
    status=verify(left,&compare,error); if (status!=CORE_PAYLOAD_READY) return status;
    status=verify(right,&compare,error); if (status!=CORE_PAYLOAD_READY) return status;
    *equal=same; return CORE_PAYLOAD_READY;
}
