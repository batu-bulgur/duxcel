#include "journal.h"
#include <duckdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
struct WorkbookJournal {
    duckdb_database db;
    duckdb_connection connection;
    WorkbookId document;
    uint64_t base,head;
    bool failed;
};
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message ? message : "Journal operation failed."); } return false;
}
static bool query(WorkbookJournal *j,const char *sql,duckdb_result *r,char **error) {
    memset(r,0,sizeof(*r));
    if (duckdb_query(j->connection,sql,r)==DuckDBSuccess) return true;
    fail(error,duckdb_result_error(r)); duckdb_destroy_result(r); return false;
}
static bool execute(WorkbookJournal *j,const char *sql,char **error) {
    duckdb_result r; if (!query(j,sql,&r,error)) return false; duckdb_destroy_result(&r); return true;
}
static bool prepare(WorkbookJournal *j,const char *sql,duckdb_prepared_statement *s,char **error) {
    if (duckdb_prepare(j->connection,sql,s)==DuckDBSuccess) return true;
    fail(error,duckdb_prepare_error(*s)); duckdb_destroy_prepare(s); return false;
}
static bool run(duckdb_prepared_statement s,duckdb_result *r,char **error) {
    memset(r,0,sizeof(*r));
    if (duckdb_execute_prepared(s,r)==DuckDBSuccess) return true;
    fail(error,duckdb_result_error(r)); duckdb_destroy_result(r); return false;
}
static bool span(const char *text,size_t bytes) { return bytes<=65536 && (text || !bytes) && (!bytes || !memchr(text,0,bytes)); }
bool workbook_journal_edit_valid(const JournalCellEdit *e) {
    return e && (e->operation.high || e->operation.low) && e->sheet && e->row<DX_SHEET_ROWS && e->column<16384 &&
        e->previous_revision<UINT64_MAX && e->revision==e->previous_revision+1 && span(e->before,e->before_bytes) && span(e->after,e->after_bytes) &&
        (e->before_bytes!=e->after_bytes || (e->before_bytes && memcmp(e->before,e->after,e->before_bytes)));
}
static bool shape(duckdb_result *r,const duckdb_type *types,size_t columns) {
    if (duckdb_column_count(r)!=columns) return false;
    for (size_t c=0;c<columns;c++) if (duckdb_column_type(r,c)!=types[c]) return false;
    for (idx_t row=0;row<duckdb_row_count(r);row++) for (size_t c=0;c<columns;c++) if (duckdb_value_is_null(r,c,row)) return false;
    return true;
}
static bool meta(WorkbookJournal *j,char **error) {
    duckdb_result r;
    if (!query(j,"SELECT version,document_high,document_low,base_revision,head_revision FROM duxcel_journal_meta",&r,error)) return false;
    static const duckdb_type types[]={DUCKDB_TYPE_UINTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_UBIGINT};
    bool ok=duckdb_row_count(&r)==1 && shape(&r,types,5) && duckdb_value_uint32(&r,0,0)==1 &&
        duckdb_value_uint64(&r,1,0)==j->document.high && duckdb_value_uint64(&r,2,0)==j->document.low && duckdb_value_uint64(&r,3,0)==j->base;
    uint64_t head=ok ? duckdb_value_uint64(&r,4,0) : 0; duckdb_destroy_result(&r);
    if (!ok || head<j->base) return fail(error,"Journal version, document or checkpoint lineage does not match.");
    j->head=head; return true;
}
void workbook_journal_close(WorkbookJournal *j) {
    if (!j) return;
    if (j->connection) duckdb_disconnect(&j->connection);
    if (j->db) duckdb_close(&j->db);
    free(j);
}
static bool cell_shape(duckdb_result *r);
static void discard_failed_creation(const char *path,bool created) {
    if (!created) return;
    /* No journal handle/receipt escaped this failed open. Remove only a path
       proven absent before this single owner's attempt, never an existing file. */
    remove(path); char *wal=dx_printf("%s.wal",path); remove(wal); free(wal);
}
bool workbook_journal_open(WorkbookJournal **out,const char *path,WorkbookId document,uint64_t base,char **error) {
    if (!out || *out || !path || !*path || !strncmp(path,":memory:",8) || (!document.high && !document.low)) return fail(error,"Choose a private journal path and document identity.");
    struct stat existing; bool created=stat(path,&existing)<0 && errno==ENOENT;
    WorkbookJournal *j=dx_alloc(sizeof(*j)); memset(j,0,sizeof(*j)); j->document=document; j->base=base;
    duckdb_config config=NULL; char *engine_error=NULL;
    bool ok=duckdb_create_config(&config)==DuckDBSuccess && duckdb_set_config(config,"threads","1")==DuckDBSuccess &&
        duckdb_set_config(config,"memory_limit","64MB")==DuckDBSuccess && duckdb_open_ext(path,&j->db,config,&engine_error)==DuckDBSuccess;
    duckdb_destroy_config(&config);
    if (!ok) { fail(error,engine_error ? engine_error : "Cannot open the journal engine."); duckdb_free(engine_error); workbook_journal_close(j); discard_failed_creation(path,created); return false; }
    duckdb_free(engine_error);
    if (duckdb_connect(j->db,&j->connection)!=DuckDBSuccess) { workbook_journal_close(j); discard_failed_creation(path,created); return fail(error,"Cannot connect the journal writer."); }
    duckdb_result r;
    if (!query(j,"SELECT count(*) FROM duckdb_tables() WHERE NOT internal",&r,error)) goto failed;
    uint64_t tables=duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r);
    if (!tables) {
        if (!execute(j,"BEGIN TRANSACTION",error)) goto failed;
        if (!execute(j,"CREATE TABLE duxcel_journal_meta(singleton BOOLEAN PRIMARY KEY CHECK(singleton),version UINTEGER NOT NULL,document_high UBIGINT NOT NULL,document_low UBIGINT NOT NULL,base_revision UBIGINT NOT NULL,head_revision UBIGINT NOT NULL);"
            "CREATE TABLE duxcel_journal_cells(operation_high UBIGINT NOT NULL,operation_low UBIGINT NOT NULL,previous_revision UBIGINT NOT NULL,revision UBIGINT UNIQUE NOT NULL,sheet UBIGINT NOT NULL,row_index UBIGINT NOT NULL,column_index UINTEGER NOT NULL,before_value BLOB NOT NULL,after_value BLOB NOT NULL,PRIMARY KEY(operation_high,operation_low))",error)) goto rollback;
        duckdb_prepared_statement s=NULL;
        if (!prepare(j,"INSERT INTO duxcel_journal_meta VALUES (true,1,?,?,?,?)",&s,error)) goto rollback;
        ok=duckdb_bind_uint64(s,1,document.high)==DuckDBSuccess && duckdb_bind_uint64(s,2,document.low)==DuckDBSuccess &&
            duckdb_bind_uint64(s,3,base)==DuckDBSuccess && duckdb_bind_uint64(s,4,base)==DuckDBSuccess;
        if (!ok) { duckdb_destroy_prepare(&s); fail(error,"Cannot bind journal lineage."); goto rollback; }
        ok=run(s,&r,error); duckdb_destroy_prepare(&s); if (!ok) goto rollback; duckdb_destroy_result(&r);
        if (!execute(j,"COMMIT",error)) goto rollback;
    } else if (tables!=2) { fail(error,"This file is not a private Duxcel journal."); goto failed; }
    if (!meta(j,error)) goto failed;
    if (!query(j,"SELECT * FROM duxcel_journal_cells LIMIT 0",&r,error)) goto failed;
    ok=cell_shape(&r); duckdb_destroy_result(&r);
    if (!ok) { fail(error,"Journal cell schema does not match this version."); goto failed; }
    if (!query(j,"SELECT count(*),min(revision),max(revision),count(DISTINCT revision),count(DISTINCT (operation_high,operation_low)) FROM duxcel_journal_cells",&r,error)) goto failed;
    uint64_t count=duckdb_value_uint64(&r,0,0);
    ok=count==j->head-j->base && count==duckdb_value_uint64(&r,3,0) && count==duckdb_value_uint64(&r,4,0) && (!count ||
        (duckdb_value_uint64(&r,1,0)==j->base+1 && duckdb_value_uint64(&r,2,0)==j->head));
    duckdb_destroy_result(&r);
    if (!ok) { fail(error,"Journal revisions are incomplete."); goto failed; }
    *out=j; return true;
rollback:
    execute(j,"ROLLBACK",NULL);
failed:
    workbook_journal_close(j); discard_failed_creation(path,created); return false;
}
static bool decode(duckdb_result *r,idx_t row,JournalCellEdit *e,char **error) {
    duckdb_blob before=duckdb_value_blob(r,7,row),after=duckdb_value_blob(r,8,row);
    *e=(JournalCellEdit){.operation={duckdb_value_uint64(r,0,row),duckdb_value_uint64(r,1,row)},
        .previous_revision=duckdb_value_uint64(r,2,row),.revision=duckdb_value_uint64(r,3,row),.sheet=duckdb_value_uint64(r,4,row),
        .row=duckdb_value_uint64(r,5,row),.column=duckdb_value_uint32(r,6,row),.before=before.data,.after=after.data,
        .before_bytes=(size_t)before.size,.after_bytes=(size_t)after.size};
    if (workbook_journal_edit_valid(e)) return true;
    duckdb_free(before.data); duckdb_free(after.data); e->before=e->after=NULL;
    return fail(error,"Invalid cell journal payload.");
}
static void decoded_free(JournalCellEdit *e) { duckdb_free((void *)e->before); duckdb_free((void *)e->after); }
static bool cell_shape(duckdb_result *r) {
    static const duckdb_type types[]={DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_UBIGINT,
        DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_UINTEGER,DUCKDB_TYPE_BLOB,DUCKDB_TYPE_BLOB};
    return shape(r,types,9);
}
static bool same(const JournalCellEdit *a,const JournalCellEdit *b) {
    return a->operation.high==b->operation.high && a->operation.low==b->operation.low && a->previous_revision==b->previous_revision &&
        a->revision==b->revision && a->sheet==b->sheet && a->row==b->row && a->column==b->column &&
        a->before_bytes==b->before_bytes && a->after_bytes==b->after_bytes &&
        (!a->before_bytes || !memcmp(a->before,b->before,a->before_bytes)) && (!a->after_bytes || !memcmp(a->after,b->after,a->after_bytes));
}
bool workbook_journal_append(WorkbookJournal *j,const JournalCellEdit *edits,size_t count,JournalControl control,JournalReceipt *receipt,char **error) {
    if (!j || j->failed || !edits || !count || count>DX_JOURNAL_BATCH) return fail(error,"Use an open journal and a bounded nonempty batch; reopen after engine failure.");
    size_t bytes=0;
    for (size_t i=0;i<count;i++) {
        if (!workbook_journal_edit_valid(edits+i) || (i && edits[i].previous_revision!=edits[i-1].revision)) return fail(error,"Cell journal edits must be valid and revision-ordered.");
        bytes+=sizeof(*edits)+edits[i].before_bytes+edits[i].after_bytes;
        if (bytes>DX_JOURNAL_BATCH_BYTES) return fail(error,"Journal batch exceeds its 4 MiB owned budget.");
    }
    if (!execute(j,"BEGIN TRANSACTION",error)) { j->failed=true; return false; }
    bool engine_failure=false; duckdb_prepared_statement find=NULL,insert=NULL,update=NULL; JournalReceipt next={0};
    if (!meta(j,error) || !prepare(j,"SELECT * FROM duxcel_journal_cells WHERE operation_high=? AND operation_low=?",&find,error) ||
        !prepare(j,"INSERT INTO duxcel_journal_cells VALUES (?,?,?,?,?,?,?,?,?)",&insert,error) ||
        !prepare(j,"UPDATE duxcel_journal_meta SET head_revision=?",&update,error)) { engine_failure=true; goto rollback; }
    next.durable_revision=j->head;
    for (size_t i=0;i<count;i++) {
        const JournalCellEdit *e=edits+i; duckdb_result r;
        if (duckdb_bind_uint64(find,1,e->operation.high)!=DuckDBSuccess || duckdb_bind_uint64(find,2,e->operation.low)!=DuckDBSuccess) {
            engine_failure=true; fail(error,"Cannot bind journal operation identity."); goto rollback;
        }
        if (!run(find,&r,error)) { engine_failure=true; goto rollback; }
        bool valid=cell_shape(&r); idx_t found=duckdb_row_count(&r);
        if (!valid || found>1) { duckdb_destroy_result(&r); fail(error,"Invalid operation identity in journal."); goto rollback; }
        if (found) {
            JournalCellEdit old={0}; bool decoded=decode(&r,0,&old,error); duckdb_destroy_result(&r);
            bool equal=decoded && same(&old,e); if (decoded) decoded_free(&old);
            if (!equal || next.appended) { fail(error,"Operation ID retry has changed payload or order."); goto rollback; }
            next.replayed++; continue;
        }
        duckdb_destroy_result(&r);
        if (e->previous_revision!=next.durable_revision) { fail(error,"Journal edit does not follow the durable revision."); goto rollback; }
        if (duckdb_bind_uint64(insert,1,e->operation.high)!=DuckDBSuccess || duckdb_bind_uint64(insert,2,e->operation.low)!=DuckDBSuccess ||
            duckdb_bind_uint64(insert,3,e->previous_revision)!=DuckDBSuccess || duckdb_bind_uint64(insert,4,e->revision)!=DuckDBSuccess ||
            duckdb_bind_uint64(insert,5,e->sheet)!=DuckDBSuccess || duckdb_bind_uint64(insert,6,e->row)!=DuckDBSuccess ||
            duckdb_bind_uint32(insert,7,(uint32_t)e->column)!=DuckDBSuccess ||
            duckdb_bind_blob(insert,8,e->before ? e->before : "",e->before_bytes)!=DuckDBSuccess ||
            duckdb_bind_blob(insert,9,e->after ? e->after : "",e->after_bytes)!=DuckDBSuccess) {
            engine_failure=true; fail(error,"Cannot bind journal cell payload."); goto rollback;
        }
        if (!run(insert,&r,error)) { engine_failure=true; goto rollback; } duckdb_destroy_result(&r);
        next.durable_revision=e->revision; next.appended++;
    }
    if (duckdb_bind_uint64(update,1,next.durable_revision)!=DuckDBSuccess) {
        engine_failure=true; fail(error,"Cannot bind journal revision."); goto rollback;
    }
    duckdb_result r;
    if (!run(update,&r,error)) { engine_failure=true; goto rollback; } duckdb_destroy_result(&r);
    if (control.before_commit && !control.before_commit(control.context)) { fail(error,"Journal commit cancelled."); goto rollback; }
    if (!execute(j,"COMMIT",error)) { engine_failure=true; goto rollback; }
    j->head=next.durable_revision; if (receipt) *receipt=next;
    duckdb_destroy_prepare(&find); duckdb_destroy_prepare(&insert); duckdb_destroy_prepare(&update);
    if (control.after_commit) control.after_commit(control.context);
    return true;
rollback:
    duckdb_destroy_prepare(&find); duckdb_destroy_prepare(&insert); duckdb_destroy_prepare(&update);
    if (!execute(j,"ROLLBACK",NULL)) engine_failure=true;
    if (engine_failure) j->failed=true;
    return false;
}
bool workbook_journal_read(WorkbookJournal *j,uint64_t after,size_t limit,JournalVisitor visitor,void *context,size_t *visited,char **error) {
    if (!j || j->failed || !visitor || !limit || limit>DX_JOURNAL_BATCH || after<j->base || after>j->head)
        return fail(error,"Choose a bounded journal interval within the checkpoint lineage.");
    duckdb_prepared_statement s=NULL; duckdb_result r;
    if (!prepare(j,"SELECT * FROM duxcel_journal_cells WHERE revision>? ORDER BY revision LIMIT ?",&s,error)) return false;
    if (limit>DX_JOURNAL_READ_BATCH) limit=DX_JOURNAL_READ_BATCH;
    if (duckdb_bind_uint64(s,1,after)!=DuckDBSuccess || duckdb_bind_uint64(s,2,limit)!=DuckDBSuccess) {
        duckdb_destroy_prepare(&s); return fail(error,"Cannot bind journal read interval.");
    }
    bool ok=run(s,&r,error); duckdb_destroy_prepare(&s); if (!ok) return false;
    if (!cell_shape(&r)) { duckdb_destroy_result(&r); return fail(error,"Invalid journal column types or NULL payload."); }
    size_t count=(size_t)duckdb_row_count(&r);
    for (size_t i=0;i<count;i++) {
        JournalCellEdit e={0};
        if (!decode(&r,i,&e,error)) { duckdb_destroy_result(&r); return false; }
        bool ordered=e.previous_revision==after && e.revision<=j->head;
        ok=ordered && visitor(context,&e,error); after=e.revision; decoded_free(&e);
        if (!ok) {
            duckdb_destroy_result(&r);
            if (!error || !*error) fail(error,ordered ? "Journal visitor stopped replay." : "Journal replay revisions are incomplete.");
            return false;
        }
    }
    duckdb_destroy_result(&r); if (visited) *visited=count; return true;
}
uint64_t workbook_journal_durable_revision(const WorkbookJournal *j) { return j ? j->head : 0; }
