#include "version_read.h"
#include "version_codec.h"
#include "version_sql.h"
#include "../common.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
struct WorkbookVersionMetadata { WorkbookVersionIndex index; WorkbookVersionRoot *roots; WorkbookVersionTag *tags; };
typedef struct { duckdb_connection connection; WorkbookVersionReadControl control; bool stopped; } Read;
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message ? message : "Version metadata admission failed."); } return false; }
static bool stopped(Read *r,char **error) {
    if (!r->stopped && r->control.cancelled) r->stopped=r->control.cancelled(r->control.context);
    if (r->stopped) fail(error,"Version metadata admission was cancelled."); return r->stopped;
}
static bool query(Read *r,const char *sql,duckdb_result *out,char **error) {
    if (stopped(r,error)) return false;
    if (duckdb_query(r->connection,sql,out)==DuckDBSuccess) return !stopped(r,error);
    return fail(error,duckdb_result_error(out));
}
static bool text_equal(const char *a,const char *b,size_t bytes) {
    if (!a || strlen(a)!=bytes) return false;
    for (size_t i=0;i<bytes;i++) if (tolower((unsigned char)a[i])!=tolower((unsigned char)b[i])) return false;
    return true;
}
static const char *type_name(char type) {
    switch (type) { case 'I': return "INTEGER"; case 'U': return "UBIGINT"; case 'B': return "BOOLEAN"; case 'T': return "VARCHAR"; case 'X': return "BLOB"; default: return NULL; }
}
static bool shape(Read *read,char **error) {
    duckdb_result r={0}; uint64_t seen[DX_VERSION_CODEC_TABLES]={0}; bool ok=false;
    const char *sql="SELECT CASE WHEN length(t.table_name)<=64 AND NOT contains(t.table_name,chr(0)) THEN t.table_name END,t.table_type,CASE WHEN length(c.column_name)<=64 AND NOT contains(c.column_name,chr(0)) THEN c.column_name END,CASE WHEN length(c.data_type)<=64 THEN c.data_type END FROM information_schema.tables t LEFT JOIN information_schema.columns c ON t.table_catalog=c.table_catalog AND t.table_schema=c.table_schema AND t.table_name=c.table_name WHERE t.table_catalog=current_database() AND t.table_schema='_duxcel_versions' LIMIT 19";
    if (!query(read,sql,&r,error)) goto done;
    if (duckdb_row_count(&r)!=DX_VERSION_CODEC_FIELDS) goto invalid;
    for (idx_t row=0;row<duckdb_row_count(&r);row++) {
        char *name=duckdb_value_varchar(&r,0,row),*kind=duckdb_value_varchar(&r,1,row),*column=duckdb_value_varchar(&r,2,row),*type=duckdb_value_varchar(&r,3,row);
        bool valid=false;
        for (size_t i=0;name && kind && column && type && i<DX_VERSION_CODEC_TABLES;i++) {
            const WorkbookVersionCodecTable *table=workbook_version_codec_table(i);
            if (strcmp(name,table->name) || strcmp(kind,"BASE TABLE")) continue;
            const char *field=table->columns,*code=table->types; size_t ordinal=0;
            for (;;) {
                size_t bytes=strcspn(field,","); while (*code==' ') code++;
                const char *expected=type_name(*code);
                if (text_equal(column,field,bytes)) {
                    valid=expected && !strcmp(type,expected) && !(seen[i]&(UINT64_C(1)<<ordinal));
                    if (valid) seen[i]|=UINT64_C(1)<<ordinal; break;
                }
                if (!field[bytes] || !*code) break; field+=bytes+1; code++; ordinal++;
            }
        }
        duckdb_free(name); duckdb_free(kind); duckdb_free(column); duckdb_free(type);
        if (!valid) goto invalid;
    }
    for (size_t i=0;i<DX_VERSION_CODEC_TABLES;i++) {
        const char *columns=workbook_version_codec_table(i)->columns; size_t fields=1;
        for (const char *p=columns;*p;p++) fields+=*p==',';
        if (seen[i]!=(UINT64_C(1)<<fields)-1) goto invalid;
    }
    ok=true; goto done;
invalid:
    fail(error,"Invalid or unsupported version metadata table/column types.");
done:
    duckdb_destroy_result(&r); return ok;
}
static bool preflight(Read *read,const char *sql,size_t limit,bool exact,char **error) {
    duckdb_result r={0}; bool ok=query(read,sql,&r,error);
    if (ok) ok=duckdb_row_count(&r)==1 && !duckdb_value_is_null(&r,0,0) && !duckdb_value_is_null(&r,1,0) &&
        (exact ? duckdb_value_uint64(&r,0,0)==limit : duckdb_value_uint64(&r,0,0)<=limit) && !duckdb_value_boolean(&r,1,0);
    duckdb_destroy_result(&r);
    return ok || (read->stopped ? false : fail(error,"Version metadata exceeds its bounds or contains invalid NULL/value rows."));
}
static bool key(duckdb_result *r,idx_t column,idx_t row,WorkbookVersionKey *out,char **error) {
    duckdb_blob blob=duckdb_value_blob(r,column,row); bool ok=!duckdb_value_is_null(r,column,row) && blob.data && blob.size==DX_VERSION_KEY_BYTES;
    if (ok) memcpy(out->bytes,blob.data,DX_VERSION_KEY_BYTES); duckdb_free(blob.data);
    return ok || fail(error,"Invalid version payload identity width.");
}
const WorkbookVersionIndex *workbook_version_metadata_index(const WorkbookVersionMetadata *m) { return m ? &m->index : NULL; }
void workbook_version_metadata_free(WorkbookVersionMetadata *m) {
    if (!m) return;
    for (size_t i=0;i<m->index.tag_count;i++) free((char *)m->tags[i].label.data);
    free(m->roots); free(m->tags); free(m);
}
WorkbookVersionReadResult workbook_version_metadata_read(duckdb_connection c,uint64_t reserved,
    WorkbookVersionReadControl control,WorkbookVersionMetadata **out,char **error) {
    if (!c || !out || *out) { fail(error,"Choose a version transaction and an empty metadata result."); return VERSION_METADATA_FAILED; }
    Read read={.connection=c,.control=control}; WorkbookVersionMetadata *m=NULL; duckdb_result r={0}; bool ok=false;
    if (stopped(&read,error) || !workbook_version_sql_transaction(c,error) || !shape(&read,error)) goto done;
    if (!preflight(&read,"SELECT count(*),coalesce(bool_or(codec IS NULL OR codec<>1 OR document_high IS NULL OR document_low IS NULL OR generation IS NULL OR saved_generation IS NULL OR saved_present IS NULL OR (saved_present AND (saved_key IS NULL OR octet_length(saved_key)<>32)) OR (NOT saved_present AND saved_key IS NOT NULL)),false) FROM (SELECT * FROM _duxcel_versions.meta LIMIT 2)",1,true,error)) goto done;
    char *sql=dx_printf("SELECT count(*),coalesce(bool_or(position IS NULL OR position<0 OR key IS NULL OR octet_length(key)<>32 OR bytes IS NULL OR bytes=0 OR encoding IS NULL OR encoding<>15),false) FROM (SELECT * FROM _duxcel_versions.roots LIMIT %u)",DX_VERSION_ROOTS+1);
    bool bounded=preflight(&read,sql,DX_VERSION_ROOTS,false,error); free(sql); if (!bounded) goto done;
    sql=dx_printf("SELECT count(*),coalesce(bool_or(position IS NULL OR position<0 OR label IS NULL OR octet_length(encode(label))=0 OR octet_length(encode(label))>%u OR contains(label,chr(0)) OR root IS NULL OR octet_length(root)<>32 OR created IS NULL),false) FROM (SELECT * FROM _duxcel_versions.tags LIMIT %u)",DX_VERSION_LABEL_BYTES,DX_VERSION_TAGS+1);
    bounded=preflight(&read,sql,DX_VERSION_TAGS,false,error); free(sql); if (!bounded) goto done;
    m=dx_alloc(sizeof(*m));
    if (!query(&read,"SELECT codec,document_high,document_low,generation,saved_generation,saved_present,saved_key FROM _duxcel_versions.meta",&r,error)) goto done;
    m->index.document_high=duckdb_value_uint64(&r,1,0); m->index.document_low=duckdb_value_uint64(&r,2,0);
    m->index.generation=duckdb_value_uint64(&r,3,0); m->index.saved_generation=duckdb_value_uint64(&r,4,0);
    m->index.saved.present=duckdb_value_boolean(&r,5,0);
    if (m->index.saved.present && !key(&r,6,0,&m->index.saved.key,error)) goto done;
    duckdb_destroy_result(&r);
    if (!query(&read,"SELECT position,key,bytes,encoding FROM _duxcel_versions.roots ORDER BY position",&r,error)) goto done;
    m->index.root_count=(size_t)duckdb_row_count(&r); m->roots=dx_alloc(m->index.root_count*sizeof(*m->roots)); m->index.roots=m->roots;
    uint64_t bytes=0;
    for (size_t i=0;i<m->index.root_count;i++) {
        if (duckdb_value_int32(&r,0,i)!=(int32_t)i || !key(&r,1,i,&m->roots[i].key,error)) goto invalid;
        m->roots[i].bytes=duckdb_value_uint64(&r,2,i); m->roots[i].encoding=(unsigned)duckdb_value_int32(&r,3,i);
        if (m->roots[i].bytes>reserved-bytes) goto invalid; bytes+=m->roots[i].bytes;
    }
    duckdb_destroy_result(&r);
    if (!query(&read,"SELECT position,label,root,created FROM _duxcel_versions.tags ORDER BY position",&r,error)) goto done;
    m->index.tag_count=(size_t)duckdb_row_count(&r); m->tags=dx_alloc(m->index.tag_count*sizeof(*m->tags)); m->index.tags=m->tags;
    for (size_t i=0;i<m->index.tag_count;i++) {
        if (duckdb_value_int32(&r,0,i)!=(int32_t)i || !key(&r,2,i,&m->tags[i].root,error)) goto invalid;
        char *label=duckdb_value_varchar(&r,1,i); if (!label) goto invalid;
        size_t length=strlen(label); m->tags[i].label=(WorkbookVersionText){dx_strdup(label),length}; duckdb_free(label);
        m->tags[i].created=duckdb_value_uint64(&r,3,i);
    }
    if (workbook_version_index_check(&m->index)!=VERSION_OK) goto invalid;
    if (stopped(&read,error)) goto done;
    ok=true; goto done;
invalid:
    fail(error,"Invalid version index order, references, labels, generations or reserved bytes.");
done:
    duckdb_destroy_result(&r);
    if (ok) { *out=m; return VERSION_METADATA_READY; }
    workbook_version_metadata_free(m); return read.stopped ? VERSION_METADATA_CANCELLED : VERSION_METADATA_FAILED;
}
