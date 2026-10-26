#include "core_compare_internal.h"
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
/* The outer artifact stores relative locators; sealed cores store resolved
   absolute locators. Compare all three fields separately, in bounded row batches,
   using the SAME existing resolver and admitted model as the real archive read. */
static const char *locator_expected(const Workbook *book,unsigned role,uint64_t sid,uint64_t owner,SourceKind *kind) {
    if (sid>=book->count) return NULL; const Worksheet *sheet=book->sheets+sid;
    if (!role) { *kind=sheet->analysis.kind; return sheet->analysis.path ? sheet->analysis.path : ""; }
    if (role==1) {
        for (size_t i=0;i<sheet->objects.count;i++) if (sheet->objects.items[i].id==owner && sheet->objects.items[i].kind==OBJECT_TABLE) {
            const Analysis *a=&sheet->objects.items[i].source; *kind=a->kind; return a->path ? a->path : "";
        }
    } else for (size_t i=0;i<sheet->source_versions.count;i++) if (sheet->source_versions.items[i].version==owner) {
        *kind=sheet->source_versions.items[i].kind; return sheet->source_versions.items[i].path;
    }
    return NULL;
}
static bool compare_locators(duckdb_connection connection,Compare *compare,const char *archive,const Workbook *book,bool *equal,char **error) {
    static const char *tables[]={"sheets","table_sources","source_versions"};
    static const char *keys[]={"l.id,0","l.sheet_id,l.object_id","l.sheet_id,l.version"};
    static const char *joins[]={"l.id=r.id","l.sheet_id=r.sheet_id AND l.object_id=r.object_id","l.sheet_id=r.sheet_id AND l.version=r.version"};
    static const char *orders[]={"l.id","l.sheet_id,l.object_id","l.sheet_id,l.version"};
    static const char *kinds[]={"kind","kind","source_kind"};
    static const char *paths[]={"path","path","source_reference"};
    const size_t caps[]={DX_MAX_SHEETS,DX_MAX_SHEETS*DX_MAX_OBJECTS,DX_MAX_SHEETS*DX_SOURCE_VERSIONS};
    bool same=true;
    for (unsigned role=0;role<3;role++) for (size_t offset=0;;offset+=64) {
        duckdb_result result={0};
        char *sql=dx_printf("SELECT %s,l.%s,CASE WHEN octet_length(encode(l.%s))<=65536 AND NOT contains(l.%s,chr(0)) THEN l.%s ELSE NULL END,CASE WHEN octet_length(encode(r.%s))<=65536 AND NOT contains(r.%s,chr(0)) THEN r.%s ELSE NULL END FROM _duxcel_workspace.%s l JOIN dx_other._duxcel_workspace.%s r ON %s ORDER BY %s LIMIT 64 OFFSET %zu",keys[role],kinds[role],paths[role],paths[role],paths[role],paths[role],paths[role],paths[role],tables[role],tables[role],joins[role],orders[role],offset);
        bool ok=query(connection,sql,compare,&result,error); free(sql);
        size_t count=(size_t)duckdb_row_count(&result);
        if (ok && (offset>caps[role] || count>caps[role]-offset)) ok=fail(error,"Archive locator comparison exceeds the admitted row bound.");
        for (size_t i=0;ok && i<count;i++) {
            if (stopped(compare,error)) { ok=false; break; }
            if (duckdb_value_is_null(&result,3,i) || duckdb_value_is_null(&result,4,i)) { ok=fail(error,"Invalid bounded archive locator."); break; }
            uint64_t sid=duckdb_value_uint64(&result,0,i),owner=duckdb_value_uint64(&result,1,i);
            SourceKind kind=(SourceKind)duckdb_value_int32(&result,2,i),expected_kind=SOURCE_NONE;
            const char *expected=locator_expected(book,role,sid,owner,&expected_kind);
            char *left=duckdb_value_varchar(&result,3,i),*right=duckdb_value_varchar(&result,4,i);
            /* Empty stays empty. Demo/None use their existing literal semantics;
               ledger references always use the strict source-package resolver. */
            char *resolved=left && *left && (role==2 || (kind!=SOURCE_NONE && kind!=SOURCE_DEMO)) ? workbook_source_path(archive,left) : dx_strdup(left ? left : "");
            bool reference_equal=left && right && resolved && !strcmp(resolved,right);
            if (!reference_equal && left && right && *right && (role==2 || (kind!=SOURCE_NONE && kind!=SOURCE_DEMO))) {
                /* Missing sources cannot be realpath-normalized by the loader.
                   Prove the writer's EXACT declared reference conversion rather
                   than invent lexical normalization across possible symlinks. */
                char *relative=workbook_relative_path(archive,right);
                char *restored=relative ? workbook_source_path(archive,relative) : NULL;
                reference_equal=relative && restored && !strcmp(left,relative) && !strcmp(resolved,restored);
                free(restored); free(relative);
            }
            if (!left || !right || !resolved || !expected || kind!=expected_kind || strcmp(resolved,expected) || !reference_equal) same=false;
            free(resolved); duckdb_free(left); duckdb_free(right);
        }
        duckdb_destroy_result(&result); if (!ok) return false;
        if (count<64) break;
    }
    *equal=same; return true;
}
static char *comparison_columns(const WorkbookCodecTable *table,bool container) {
    const char *locator=container ? (!strcmp(table->table,"sheets") || !strcmp(table->table,"table_sources") ? "path" : !strcmp(table->table,"source_versions") ? "source_reference" : NULL) : NULL;
    char *columns=dx_strdup("");
    for (const char *p=table->columns;*p;) {
        size_t n=strcspn(p,","); if (*columns) dx_append(&columns,",");
        if (locator && strlen(locator)==n && !memcmp(p,locator,n)) dx_append(&columns,"'' AS %s",locator);
        else dx_append(&columns,"%.*s",(int)n,p);
        p+=n; if (*p) p++;
    }
    return columns;
}
/* Both native entry points compare the same admitted current codec rows. */
static bool compare_tables(duckdb_connection connection,Compare *compare,bool container,bool *equal,char **error) {
    duckdb_result result={0}; bool same=true;
    /* Native optional origin metadata is outside the required shared codec
       inventory. Missing and empty both mean no origin; admitted rows must
       still participate in complete-core semantic reuse. */
    if (!query(connection,"SELECT EXISTS(SELECT 1 FROM information_schema.tables WHERE table_catalog=current_database() AND table_schema='_duxcel_workspace' AND table_name='export_origin'),EXISTS(SELECT 1 FROM information_schema.tables WHERE table_catalog='dx_other' AND table_schema='_duxcel_workspace' AND table_name='export_origin')",compare,&result,error)) goto done;
    bool left_origin=duckdb_value_boolean(&result,0,0),right_origin=duckdb_value_boolean(&result,1,0);
    duckdb_destroy_result(&result);
    if (left_origin || right_origin) {
        const char *origin_sql=left_origin && right_origin ?
        "SELECT NOT EXISTS ((SELECT * FROM _duxcel_workspace.export_origin EXCEPT ALL SELECT * FROM dx_other._duxcel_workspace.export_origin) UNION ALL (SELECT * FROM dx_other._duxcel_workspace.export_origin EXCEPT ALL SELECT * FROM _duxcel_workspace.export_origin))" :
        left_origin ? "SELECT NOT EXISTS(SELECT 1 FROM _duxcel_workspace.export_origin)" : "SELECT NOT EXISTS(SELECT 1 FROM dx_other._duxcel_workspace.export_origin)";
        if (!query(connection,origin_sql,compare,&result,error)) goto done;
        same &= duckdb_value_boolean(&result,0,0); duckdb_destroy_result(&result);
    }
    size_t count=workbook_codec_table_count(15);
    for (size_t i=0;i<count;i++) {
        compare->table=i; const WorkbookCodecTable *table=workbook_codec_table_at(15,i);
        /* The outer current native container advertises 16 in its head meta;
           its embedded current core is 15. Normalize only that framing field,
           never authored rows, identities, command clocks or presentation. */
        /* Locator values are proved separately above; a neutral projection on
           BOTH sides keeps every other typed column/multiplicity exact. */
        char *right_columns=comparison_columns(table,container);
        const char *left_columns=container && !strcmp(table->table,"meta") ? "15 AS version,active_sheet,user_scale" : right_columns;
        char *sql=dx_printf("SELECT NOT EXISTS ((SELECT %s FROM _duxcel_workspace.%s EXCEPT ALL SELECT %s FROM dx_other._duxcel_workspace.%s) UNION ALL (SELECT %s FROM dx_other._duxcel_workspace.%s EXCEPT ALL SELECT %s FROM _duxcel_workspace.%s))",left_columns,table->table,right_columns,table->table,right_columns,table->table,left_columns,table->table);
        free(right_columns);
        bool ok=query(connection,sql,compare,&result,error); free(sql);
        if (ok && (duckdb_row_count(&result)!=1 || duckdb_column_count(&result)!=1 || duckdb_value_is_null(&result,0,0))) ok=fail(error,"Invalid core comparison result.");
        if (ok) same &= duckdb_value_boolean(&result,0,0);
        duckdb_destroy_result(&result); if (!ok) goto done;
    }
    duckdb_destroy_result(&result); *equal=same; return true;
done:
    duckdb_destroy_result(&result); return false;
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
        if (!compare_tables(connection,&compare,false,&same,error)) goto done;
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

/* Internal borrowed-transaction comparison. The archive scope owns attachment
   cleanup by ending and closing its unpublished read transaction/connection. */
WorkbookCorePayloadResult workbook_core_payload_equal_transaction(duckdb_connection connection,
    const WorkbookCorePayload *right,const char *archive,const Workbook *book,WorkbookCoreCompareControl control,bool *equal,char **error) {
    const WorkbookCorePayloadInfo *info=workbook_core_payload_info(right);
    if (!connection || !info || info->encoding!=15 || !archive || !*archive || !book || !equal) { fail(error,"Choose an admitted current transaction and sealed leaf."); return CORE_PAYLOAD_FAILED; }
    Compare compare={.control=control,.phase=CORE_COMPARE_VERIFY};
    WorkbookCorePayloadResult status=verify(right,&compare,error);
    if (status!=CORE_PAYLOAD_READY) return status;
    duckdb_result result={0}; bool same=false;
    char *quoted=dx_quote(workbook_core_payload_path(right),'\'');
    char *sql=dx_printf("ATTACH %s AS dx_other (READ_ONLY)",quoted); free(quoted);
    bool ok=query(connection,sql,&compare,&result,error); free(sql); duckdb_destroy_result(&result);
    if (!ok) return compare.stopped ? CORE_PAYLOAD_CANCELLED : CORE_PAYLOAD_FAILED;
    compare.phase=CORE_COMPARE_ROWS;
    ok=compare_tables(connection,&compare,true,&same,error);
    bool locators=false;
    if (ok) ok=compare_locators(connection,&compare,archive,book,&locators,error);
    same &= locators;
    /* DuckDB cannot DETACH after reading an attached database inside the active
       transaction. Keep this read-only attachment pinned until archive_read
       ends/closes its owned transaction. Never COMMIT/ROLLBACK a borrowed one.
       Failure/cancellation poisons that entire unpublished archive admission. */
    if (compare.stopped) return CORE_PAYLOAD_CANCELLED;
    if (!ok) return CORE_PAYLOAD_FAILED;
    compare.phase=CORE_COMPARE_RECHECK; compare.table=0;
    status=verify(right,&compare,error); if (status!=CORE_PAYLOAD_READY) return status;
    *equal=same; return CORE_PAYLOAD_READY;
}
