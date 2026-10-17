#include "database.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

static void fail(char **error, const char *s) { free(*error); *error = dx_strdup(s ? s : "DuckDB query failed"); }
static void schema_cache_clear(Database *db) {
    free(db->query_validation_key); db->query_validation_key=NULL;
    for (size_t i=0;i<2;i++) {
        if (db->schema_statement[i]) duckdb_destroy_prepare(&db->schema_statement[i]);
        free(db->schema_key[i]); db->schema_key[i]=NULL;
    }
}
static void page_cache_clear(Database *db) {
    if (db->page_statement) duckdb_destroy_prepare(&db->page_statement);
    free(db->page_key); db->page_key=NULL;
}
static void column_free(Column *c) {
    free(c->name); free(c->type_name); free(c->caption); free(c->detail);
    for (size_t i=0;i<c->category_count;i++) free(c->categories[i]);
    free(c->categories);
}
static void pivot_cache_clear(Database *db) {
    db->pivot_totals_valid=false;
    free(db->pivot_key); free(db->pivot_sql); db->pivot_key=db->pivot_sql=NULL;
    for (size_t i=0;i<db->pivot_columns;i++) column_free(&db->pivot_headers[i]);
    free(db->pivot_headers); db->pivot_headers=NULL; db->pivot_columns=0;
}
static void relations_free(Relation *relations,size_t count) {
    for (size_t i=0;i<count;i++) { free(relations[i].schema); free(relations[i].name); free(relations[i].label); }
    free(relations);
}
static bool cancelled(Database *db,char **error) {
    if (!db->cancelled || !db->cancelled(db->cancel_context)) return false;
    fail(error,"Query cancelled"); return true;
}
bool database_open(Database *db, char **error) {
    memset(db, 0, sizeof(*db));
    duckdb_config config = NULL;
    if (duckdb_create_config(&config) != DuckDBSuccess) { fail(error, "Cannot create DuckDB configuration"); return false; }
    db->temp_directory=dx_temp_directory();
    if (!db->temp_directory) { duckdb_destroy_config(&config); fail(error,"Cannot create writable temporary storage for queries."); return false; }
    /* Four concurrent hash partitions exhaust this budget on the full NYC
       DISTINCT result. Two threads retain spill headroom without raising RAM. */
    if (duckdb_set_config(config,"memory_limit","512MB")!=DuckDBSuccess || duckdb_set_config(config,"threads","2")!=DuckDBSuccess || duckdb_set_config(config,"temp_directory",db->temp_directory)!=DuckDBSuccess) {
        duckdb_destroy_config(&config); database_close(db); fail(error,"Cannot configure query memory or temporary storage."); return false;
    }
    char *message = NULL;
    duckdb_state state = duckdb_open_ext(NULL, &db->db, config, &message);
    duckdb_destroy_config(&config);
    if (state != DuckDBSuccess) { fail(error, message); duckdb_free(message); database_close(db); return false; }
    if (duckdb_connect(db->db, &db->connection) != DuckDBSuccess) {
        fail(error, "Cannot connect to DuckDB"); database_close(db); return false;
    }
    return true;
}
void database_close(Database *db) {
    schema_cache_clear(db); page_cache_clear(db);
    if (db->connection) duckdb_disconnect(&db->connection);
    if (db->db) duckdb_close(&db->db);
    dx_remove_temp_directory(db->temp_directory); free(db->temp_directory);
    relations_free(db->relations,db->relation_count);
    free(db->attached_path); pivot_cache_clear(db); memset(db, 0, sizeof(*db));
}
bool database_execute(Database *db, const char *sql, duckdb_result *r, char **error) {
    memset(r, 0, sizeof(*r));
    db->query_count++;
    if (duckdb_query(db->connection, sql, r) == DuckDBSuccess) return true;
    fail(error, duckdb_result_error(r)); duckdb_destroy_result(r); return false;
}
bool database_refresh(Database *db,char **error) {
    schema_cache_clear(db); page_cache_clear(db); pivot_cache_clear(db);
    if (db->attached_path) {
        duckdb_result result={0}; if (!database_execute(db,"DETACH source_db",&result,error)) return false;
        duckdb_destroy_result(&result); free(db->attached_path); db->attached_path=NULL;
        relations_free(db->relations,db->relation_count); db->relations=NULL; db->relation_count=0;
    }
    return true;
}
static bool bind_filter(const Analysis *a,duckdb_prepared_statement stmt,char **error) {
    idx_t parameter=0;
    if (a->filter_column && (a->filter_op < FILTER_NULL || a->filter_op>=FILTER_IN) &&
        duckdb_bind_parameter_index(stmt,&parameter,"__duxcel_filter")==DuckDBSuccess && parameter &&
        duckdb_bind_varchar(stmt,parameter,a->filter_value ? a->filter_value : "")!=DuckDBSuccess) {
        fail(error,"Could not bind filter value"); return false;
    }
    return true;
}
static bool prepare_analysis(Database *db,const Analysis *a,const char *sql,duckdb_prepared_statement *stmt,char **error) {
    if (cancelled(db,error)) return false;
    if (duckdb_prepare(db->connection,sql,stmt)!=DuckDBSuccess) {
        fail(error,duckdb_prepare_error(*stmt)); duckdb_destroy_prepare(stmt); return false;
    }
    if (!bind_filter(a,*stmt,error)) { duckdb_destroy_prepare(stmt); return false; }
    return true;
}
static bool execute_statement(Database *db,duckdb_prepared_statement stmt,duckdb_result *r,char **error) {
    duckdb_pending_result pending=NULL; memset(r,0,sizeof(*r));
    bool ok=false;
    if (cancelled(db,error)) goto done;
    db->query_count++;
    if (duckdb_pending_prepared(stmt,&pending)!=DuckDBSuccess) { fail(error,pending ? duckdb_pending_error(pending) : "Cannot start the prepared query"); goto done; }
    for (;;) {
        if (cancelled(db,error)) { duckdb_interrupt(db->connection); goto done; }
        duckdb_pending_state state=duckdb_pending_execute_task(pending);
        if (state==DUCKDB_PENDING_ERROR) { fail(error,duckdb_pending_error(pending)); goto done; }
        if (state==DUCKDB_PENDING_RESULT_READY) break;
        if (state==DUCKDB_PENDING_NO_TASKS_AVAILABLE) dx_sleep_ms(1);
    }
    if (cancelled(db,error)) goto done;
    ok=duckdb_execute_pending(pending,r)==DuckDBSuccess;
    if (!ok) { fail(error,duckdb_result_error(r)); duckdb_destroy_result(r); }
done:
    if (pending) duckdb_destroy_pending(&pending);
    return ok;
}
static bool execute_analysis(Database *db,const Analysis *a,const char *sql,duckdb_result *r,char **error) {
    duckdb_prepared_statement stmt=NULL; memset(r,0,sizeof(*r));
    if (!prepare_analysis(db,a,sql,&stmt,error)) return false;
    bool ok=execute_statement(db,stmt,r,error); duckdb_destroy_prepare(&stmt); return ok;
}
static void query_error_position(const Analysis *a,char **error) {
    if (!a->query || !error || !*error) return;
    char *marker=NULL,*next=*error;
    while ((next=strstr(next,"\nLINE "))!=NULL) { marker=next+1; next+=6; }
    if (!marker && !strncmp(*error,"LINE ",5)) marker=*error;
    if (!marker) return;
    char *end=NULL; unsigned long line=strtoul(marker+5,&end,10); if (!end || *end!=':') return;
    Analysis raw=*a; raw.query=NULL; raw.unique_rows=false; raw.filter_column=raw.group_column=raw.measure_column=raw.sort_column=NULL; raw.filter_count=raw.sort_count=0;
    char *sql=analysis_sql(&raw,false); unsigned long prefix=4+(a->unique_rows ? 1 : 0); for (const char *p=sql;*p;p++) if (*p=='\n') prefix++; free(sql);
    if (line<=prefix) return;
    char *message=dx_printf("%.*sLINE %lu%s",(int)(marker-*error),*error,line-prefix,end); free(*error); *error=message;
}
static bool validate_query(Database *db,const Analysis *a,bool force,char **error) {
    if (!a->query && !force) return true;
    if (cancelled(db,error)) return false;
    Analysis shape=*a; shape.filter_column=NULL; shape.filter_count=0; shape.group_column=shape.measure_column=shape.sort_column=NULL; shape.sort_count=0;
    char *sql=analysis_sql(&shape,false),*key=dx_printf("%s\n%" PRIu64,sql,dx_file_stamp(a->path));
    if (db->query_validation_key && !strcmp(db->query_validation_key,key)) { free(sql); free(key); return true; }
    bool ok=true; db->relation_prepares++;
    if (a->query) {
        duckdb_extracted_statements statements=NULL;
        idx_t count=duckdb_extract_statements(db->connection,a->query,&statements);
        if (count!=1) { const char *message=duckdb_extract_statements_error(statements); fail(error,count || !message || !*message ? "Use one SELECT query, optionally with WITH. Multiple statements are not allowed." : message); ok=false; }
        duckdb_destroy_extracted(&statements);
    }
    duckdb_prepared_statement statement=NULL;
    if (ok && duckdb_prepare(db->connection,sql,&statement)!=DuckDBSuccess) { fail(error,duckdb_prepare_error(statement)); query_error_position(a,error); ok=false; }
    if (ok && (duckdb_prepared_statement_type(statement)!=DUCKDB_STATEMENT_TYPE_SELECT || duckdb_nparams(statement))) { fail(error,"Use one SELECT query with no unbound parameters."); ok=false; }
    if (statement) duckdb_destroy_prepare(&statement);
    if (ok && cancelled(db,error)) ok=false;
    if (ok) { free(db->query_validation_key); db->query_validation_key=key; key=NULL; }
    free(sql); free(key); return ok;
}
static bool source_prepare(Database *db, Analysis *a, Page *p, char **error) {
    if (!analysis_query_valid(a,error) || !analysis_filters_valid(a,error) || !analysis_sorts_valid(a,error)) return false;
    if (db->attached_path && (a->kind != SOURCE_DATABASE || strcmp(db->attached_path, a->path))) {
        schema_cache_clear(db); page_cache_clear(db); duckdb_result r;
        if (!database_execute(db, "DETACH source_db", &r, error)) return false;
        duckdb_destroy_result(&r); free(db->attached_path); db->attached_path = NULL;
        relations_free(db->relations,db->relation_count); db->relations=NULL; db->relation_count=0;
    }
    if (a->kind != SOURCE_DATABASE) return validate_query(db,a,false,error);
    if (!db->attached_path) {
        char *path = dx_quote(a->path, '\'');
        char *sql = dx_printf("ATTACH %s AS source_db (READ_ONLY)", path);
        duckdb_result r; bool ok = database_execute(db, sql, &r, error);
        free(sql); free(path); if (!ok) return false;
        duckdb_destroy_result(&r); db->attached_path = dx_strdup(a->path);
    }
    if (!db->relations) {
        duckdb_result r;
        if (!database_execute(db, "SELECT table_schema, table_name FROM information_schema.tables "
        "WHERE table_catalog = 'source_db' ORDER BY table_schema, table_name", &r, error)) return false;
        db->relation_count=(size_t)duckdb_row_count(&r);
        db->relations=dx_alloc(db->relation_count*sizeof(*db->relations));
        for (size_t i=0;i<db->relation_count;i++) {
            char *schema=duckdb_value_varchar(&r,0,i),*name=duckdb_value_varchar(&r,1,i);
            db->relations[i]=(Relation){dx_strdup(schema),dx_strdup(name),dx_printf("%s.%s",schema,name)};
            duckdb_free(schema); duckdb_free(name);
        }
        duckdb_destroy_result(&r);
    }
    if (!db->relation_count) { fail(error,"This DuckDB database has no tables or views."); return false; }
    if (p) {
        p->relation_count=db->relation_count; p->relations=dx_alloc(p->relation_count*sizeof(*p->relations));
        for (size_t i=0;i<p->relation_count;i++) p->relations[i]=(Relation){dx_strdup(db->relations[i].schema),dx_strdup(db->relations[i].name),dx_strdup(db->relations[i].label)};
    }
    if (!a->table) {
        free(a->schema); a->schema=dx_strdup(db->relations[0].schema); a->table=dx_strdup(db->relations[0].name);
    }
    return validate_query(db,a,false,error);
}
bool database_validate_relation(Database *db,Analysis *a,char **error) {
    if (a->kind==SOURCE_NONE) { fail(error,"Open a data source before editing its query."); return false; }
    return source_prepare(db,a,NULL,error) && validate_query(db,a,true,error);
}
static const char *type_name(duckdb_type t) {
    switch (t) {
    case DUCKDB_TYPE_BOOLEAN: return "BOOLEAN";
    case DUCKDB_TYPE_TINYINT: return "TINYINT";
    case DUCKDB_TYPE_SMALLINT: return "SMALLINT";
    case DUCKDB_TYPE_INTEGER: return "INTEGER";
    case DUCKDB_TYPE_BIGINT: return "BIGINT";
    case DUCKDB_TYPE_UTINYINT: return "UTINYINT";
    case DUCKDB_TYPE_USMALLINT: return "USMALLINT";
    case DUCKDB_TYPE_UINTEGER: return "UINTEGER";
    case DUCKDB_TYPE_UBIGINT: return "UBIGINT";
    case DUCKDB_TYPE_HUGEINT: return "HUGEINT";
    case DUCKDB_TYPE_UHUGEINT: return "UHUGEINT";
    case DUCKDB_TYPE_FLOAT: return "FLOAT";
    case DUCKDB_TYPE_DOUBLE: return "DOUBLE";
    case DUCKDB_TYPE_DECIMAL: return "DECIMAL";
    case DUCKDB_TYPE_DATE: return "DATE";
    case DUCKDB_TYPE_TIME: return "TIME";
    case DUCKDB_TYPE_TIMESTAMP: return "TIMESTAMP";
    case DUCKDB_TYPE_TIMESTAMP_S: return "TIMESTAMP_S";
    case DUCKDB_TYPE_TIMESTAMP_MS: return "TIMESTAMP_MS";
    case DUCKDB_TYPE_TIMESTAMP_NS: return "TIMESTAMP_NS";
    case DUCKDB_TYPE_TIMESTAMP_TZ: return "TIMESTAMPTZ";
    case DUCKDB_TYPE_TIME_TZ: return "TIMETZ";
    case DUCKDB_TYPE_INTERVAL: return "INTERVAL";
    case DUCKDB_TYPE_VARCHAR: return "VARCHAR";
    case DUCKDB_TYPE_BLOB: return "BLOB";
    case DUCKDB_TYPE_ENUM: return "ENUM";
    case DUCKDB_TYPE_LIST: return "LIST";
    case DUCKDB_TYPE_STRUCT: return "STRUCT";
    case DUCKDB_TYPE_MAP: return "MAP";
    case DUCKDB_TYPE_ARRAY: return "ARRAY";
    case DUCKDB_TYPE_UNION: return "UNION";
    case DUCKDB_TYPE_UUID: return "UUID";
    case DUCKDB_TYPE_SQLNULL: return "NULL";
    default: return "OTHER";
    }
}
static bool numeric_type(duckdb_type t) {
    return (t >= DUCKDB_TYPE_TINYINT && t <= DUCKDB_TYPE_DOUBLE) || t == DUCKDB_TYPE_DECIMAL ||
        t == DUCKDB_TYPE_HUGEINT || t == DUCKDB_TYPE_UHUGEINT;
}
static NumberType number_type(duckdb_logical_type type) {
    switch (duckdb_get_type_id(type)) {
    case DUCKDB_TYPE_TINYINT: return (NumberType){NUM_I8,0,0};
    case DUCKDB_TYPE_SMALLINT: return (NumberType){NUM_I16,0,0};
    case DUCKDB_TYPE_INTEGER: return (NumberType){NUM_I32,0,0};
    case DUCKDB_TYPE_BIGINT: return (NumberType){NUM_I64,0,0};
    case DUCKDB_TYPE_HUGEINT: return (NumberType){NUM_I128,0,0};
    case DUCKDB_TYPE_UTINYINT: return (NumberType){NUM_U8,0,0};
    case DUCKDB_TYPE_USMALLINT: return (NumberType){NUM_U16,0,0};
    case DUCKDB_TYPE_UINTEGER: return (NumberType){NUM_U32,0,0};
    case DUCKDB_TYPE_UBIGINT: return (NumberType){NUM_U64,0,0};
    case DUCKDB_TYPE_UHUGEINT: return (NumberType){NUM_U128,0,0};
    case DUCKDB_TYPE_DECIMAL: return (NumberType){NUM_DECIMAL,duckdb_decimal_width(type),duckdb_decimal_scale(type)};
    case DUCKDB_TYPE_FLOAT: case DUCKDB_TYPE_DOUBLE: return (NumberType){NUM_FLOAT,0,0};
    default: return (NumberType){0};
    }
}
static bool identifier_equal(const char *a,const char *b) {
    while (*a && *b) { unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++; if (x>='A' && x<='Z') x+=32; if (y>='A' && y<='Z') y+=32; if (x!=y) return false; }
    return *a==*b;
}
static duckdb_prepared_statement source_schema(Database *db,Analysis *a,bool output,char **error) {
    Analysis shape=*a; shape.filter_column=NULL; shape.filter_count=0; shape.sort_column=NULL; shape.sort_count=0;
    if (!output) { shape.group_column=NULL; shape.measure_column=NULL; }
    char *sql=analysis_sql(&shape,false),*key=dx_printf("%s\n%" PRIu64,sql,dx_file_stamp(a->path));
    size_t slot=output && a->group_column ? 1 : 0;
    if (db->schema_statement[slot] && db->schema_key[slot] && !strcmp(key,db->schema_key[slot])) { free(sql); free(key); return db->schema_statement[slot]; }
    if (db->schema_statement[slot]) duckdb_destroy_prepare(&db->schema_statement[slot]);
    free(db->schema_key[slot]); db->schema_key[slot]=NULL;
    bool ok=prepare_analysis(db,&shape,sql,&db->schema_statement[slot],error); free(sql);
    if (!ok) { free(key); return NULL; }
    db->schema_key[slot]=key; return db->schema_statement[slot];
}
static NumberType schema_number(duckdb_prepared_statement statement,const char *column) {
    size_t count=(size_t)duckdb_prepared_statement_column_count(statement);
    for (size_t i=0;i<count;i++) {
        const char *name=duckdb_prepared_statement_column_name(statement,i); bool match=name && identifier_equal(name,column); duckdb_free((void *)name);
        if (match) { duckdb_logical_type type=duckdb_prepared_statement_column_logical_type(statement,i); NumberType result=number_type(type); duckdb_destroy_logical_type(&type); return result; }
    }
    return (NumberType){0};
}
static bool comparison(FilterOp op) { return (op>=FILTER_EQUAL && op<=FILTER_LESS) || op==FILTER_BETWEEN || op==FILTER_COMPOSITE; }
static bool resolve_analysis_types(Database *db,Analysis *a,char **error) {
    bool needed=a->filter_column && comparison(a->filter_op);
    for (size_t i=0;i<a->filter_count;i++) needed|=comparison(a->filters[i].op);
    if (!needed) return true;
    duckdb_prepared_statement schema=source_schema(db,a,false,error); if (!schema) return false;
    if (a->filter_column && comparison(a->filter_op)) a->filter_number=schema_number(schema,a->filter_column);
    for (size_t i=0;i<a->filter_count;i++) if (comparison(a->filters[i].op)) a->filters[i].number=schema_number(schema,a->filters[i].column);
    return true;
}
static bool resolve_pivot_types(Database *db,Analysis *a,PivotDefinition *pivot,char **error) {
    bool needed=false; for (size_t i=0;i<pivot->counts[PIVOT_FILTERS];i++) needed|=comparison(pivot->fields[PIVOT_FILTERS][i].filter);
    for (int role=0;role<2;role++) for (size_t i=0;i<pivot->counts[role];i++) needed|=pivot->fields[role][i].grain!=PIVOT_DATE_EXACT;
    if (!needed) return true;
    duckdb_prepared_statement schema=source_schema(db,a,true,error); if (!schema) return false;
    for (size_t i=0;i<pivot->counts[PIVOT_FILTERS];i++) {
        PivotField *field=&pivot->fields[PIVOT_FILTERS][i]; if (comparison(field->filter)) field->number=schema_number(schema,field->name);
    }
    for (int role=0;role<2;role++) for (size_t i=0;i<pivot->counts[role];i++) {
        PivotField *field=&pivot->fields[role][i]; if (!field->grain) continue;
        bool temporal=false;
        for (idx_t c=0;c<duckdb_prepared_statement_column_count(schema);c++) {
            const char *name=duckdb_prepared_statement_column_name(schema,c); bool match=name && identifier_equal(name,field->name); duckdb_free((void *)name); if (!match) continue;
            duckdb_type type=duckdb_prepared_statement_column_type(schema,c);
            temporal=type==DUCKDB_TYPE_DATE || type==DUCKDB_TYPE_TIMESTAMP || type==DUCKDB_TYPE_TIMESTAMP_S || type==DUCKDB_TYPE_TIMESTAMP_MS || type==DUCKDB_TYPE_TIMESTAMP_NS || type==DUCKDB_TYPE_TIMESTAMP_TZ;
        }
        if (!temporal) { fail(error,"Date grains require a DATE or TIMESTAMP field. Cast the source field explicitly first."); return false; }
    }
    return true;
}
static bool result_page(duckdb_result *r,Page *p,size_t limit,char **error) {
    p->column_count = (size_t)duckdb_column_count(r); p->row_count = (size_t)duckdb_row_count(r);
    size_t bytes=p->column_count*sizeof(*p->columns)+p->column_count*p->row_count*sizeof(*p->cells);
    if (p->column_count>16384 || bytes>limit) { p->column_count=p->row_count=0; fail(error,"Result page is too wide. Select fewer columns in SQL."); return false; }
    p->columns = dx_alloc(p->column_count * sizeof(*p->columns));
    p->cells = dx_alloc(p->column_count * p->row_count * sizeof(*p->cells));
    p->bytes = p->column_count * sizeof(*p->columns) + p->column_count * p->row_count * sizeof(*p->cells);
    for (size_t c = 0; c < p->column_count; c++) {
        Column *col = &p->columns[c]; col->name = dx_strdup(duckdb_column_name(r, c));
        col->type = duckdb_column_type(r,c); col->numeric=numeric_type(col->type);
        duckdb_logical_type logical=duckdb_column_logical_type(r,c); col->number_type=number_type(logical); duckdb_destroy_logical_type(&logical);
        if (col->type == DUCKDB_TYPE_DECIMAL) {
            duckdb_logical_type type = duckdb_column_logical_type(r, c);
            col->type_name = dx_printf("DECIMAL(%u,%u)", duckdb_decimal_width(type), duckdb_decimal_scale(type));
            duckdb_destroy_logical_type(&type);
        } else col->type_name = dx_strdup(type_name(col->type));
        p->bytes+=strlen(col->name)+strlen(col->type_name)+2;
        if (p->bytes>limit) { fail(error,"Column metadata exceeds the page memory limit."); return false; }
        for (size_t row = 0; row < p->row_count; row++) {
            Cell *cell = &p->cells[row * p->column_count + c];
            cell->is_null = duckdb_value_is_null(r, c, row);
            if (!cell->is_null) {
                cell->text = duckdb_value_varchar(r, c, row);
                if (!cell->text) { fail(error,"DuckDB could not convert a cell for display"); return false; }
                if (col->numeric) cell->number = duckdb_value_double(r, c, row);
                p->bytes += strlen(cell->text) + 1;
                if (p->bytes>limit) { fail(error,"Result page exceeds its memory limit. Choose fewer fields or shorter values."); return false; }
            }
        }
    }
    return true;
}
static bool execute_page(Database *db,Analysis *a,uint64_t offset,duckdb_result *result,char **error) {
    char *base=analysis_sql(a,true),*key=dx_printf("%s\n%" PRIu64,base,dx_file_stamp(a->path));
    bool same=db->page_statement && db->page_key && !strcmp(key,db->page_key);
    if (!same) {
        char *sql=dx_printf("SELECT * FROM (%s) AS page LIMIT $__duxcel_page_limit OFFSET $__duxcel_page_offset",base);
        duckdb_prepared_statement stmt=NULL; db->page_prepares++;
        Analysis unbound={0}; bool prepared=prepare_analysis(db,&unbound,sql,&stmt,error); free(sql);
        if (!prepared) { free(base); free(key); return false; }
        page_cache_clear(db); db->page_statement=stmt; db->page_key=key; key=NULL;
    }
    free(base); free(key);
    idx_t limit=0,start=0; duckdb_prepared_statement stmt=db->page_statement;
    if (duckdb_clear_bindings(stmt)!=DuckDBSuccess) { fail(error,"Could not reset the prepared page bindings"); page_cache_clear(db); return false; }
    if (!bind_filter(a,stmt,error)) return false;
    if (duckdb_bind_parameter_index(stmt,&limit,"__duxcel_page_limit")!=DuckDBSuccess ||
        duckdb_bind_parameter_index(stmt,&start,"__duxcel_page_offset")!=DuckDBSuccess ||
        duckdb_bind_uint64(stmt,limit,DX_PAGE_ROWS)!=DuckDBSuccess || duckdb_bind_uint64(stmt,start,offset)!=DuckDBSuccess) {
        fail(error,"Could not bind the requested page"); return false;
    }
    return execute_statement(db,stmt,result,error);
}
static bool fetch_window(Database *db,Analysis *a,uint64_t offset,int64_t known_total,Page *p,bool exact,size_t budget,size_t columns,char **error) {
    double start = dx_millis(); memset(p, 0, sizeof(*p)); p->offset = offset;
    if (!source_prepare(db,a,p,error) || !resolve_analysis_types(db,a,error)) return false;
    duckdb_result r={0};
    bool ok=execute_page(db,a,offset,&r,error);
    if (!ok) return false;
    if (columns && duckdb_column_count(&r)>columns) { duckdb_destroy_result(&r); fail(error,"Worksheet tables support up to 256 columns. Select fewer fields before importing this table."); return false; }
    ok=result_page(&r,p,budget,error); if (!ok) { duckdb_destroy_result(&r); return false; }
    duckdb_destroy_result(&r);
    for (size_t c=0;c<p->column_count;c++) for (size_t i=0;i<a->sort_count+(a->sort_column ? 1 : 0);i++) {
        AnalysisSort sort=analysis_sort_at(a,i);
        if (identifier_equal(sort.column,p->columns[c].name)) { p->columns[c].sort_priority=(unsigned char)(i+1); p->columns[c].sort_descending=sort.descending; break; }
    }
    if (cancelled(db,error)) return false;
    if (known_total >= 0) p->total = (uint64_t)known_total;
    else if (!exact) {
        p->count_pending=p->row_count==DX_PAGE_ROWS || (offset>0 && p->row_count==0);
        p->total=p->row_count || !offset ? offset+p->row_count : 0;
    } else if (!database_count(db,a,&p->total,error)) return false;
    p->query_ms = dx_millis() - start; return true;
}
bool database_fetch(Database *db,Analysis *a,uint64_t offset,int64_t total,Page *p,char **error) { return fetch_window(db,a,offset,total,p,true,16*1024*1024,0,error); }
bool database_fetch_window(Database *db,Analysis *a,uint64_t offset,int64_t total,Page *p,char **error) { return fetch_window(db,a,offset,total,p,false,16*1024*1024,0,error); }
bool database_fetch_table(Database *db,Analysis *a,uint64_t offset,int64_t total,Page *p,char **error) { return fetch_window(db,a,offset,total,p,true,4*1024*1024,256,error); }
bool database_fetch_table_window(Database *db,Analysis *a,uint64_t offset,int64_t total,Page *p,char **error) { return fetch_window(db,a,offset,total,p,false,4*1024*1024,256,error); }
bool database_count(Database *db,Analysis *a,uint64_t *total,char **error) {
    bool ok=source_prepare(db,a,NULL,error); if (!ok || !resolve_analysis_types(db,a,error)) return false;
    Analysis count=*a; count.sort_column=NULL; count.sort_count=0;
    char *base=analysis_sql(&count,true),*sql=dx_printf("SELECT count(*) FROM (%s) AS counted",base); free(base);
    duckdb_result r={0}; db->count_queries++; ok=execute_analysis(db,a,sql,&r,error); free(sql);
    if (ok) { *total=duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r); } return ok;
}
typedef struct { char *sql; Column *headers; size_t count,bytes; bool empty_input; } PivotPlan;
typedef struct { char **values,*list; size_t count; } PivotDomain;
static void pivot_plan_free(PivotPlan *p) {
    for (size_t i=0;i<p->count;i++) column_free(&p->headers[i]);
    free(p->headers); free(p->sql); memset(p,0,sizeof(*p));
}
static bool pivot_plan(Database *db,Analysis *source,const PivotDefinition *definition,PivotPlan *plan,char **error) {
    PivotDomain domains[DX_PIVOT_FIELDS]={0}; char *lists[DX_PIVOT_FIELDS]={0},*input=pivot_input_sql(source,definition); duckdb_result r={0};
    size_t product=1,keys=definition->counts[PIVOT_ROWS],measures=definition->counts[PIVOT_VALUES],domain_bytes=0; bool ok=false;
    for (size_t field=0;field<definition->counts[PIVOT_COLUMNS];field++) {
        if (cancelled(db,error)) goto done;
        char *key=pivot_field_key(definition,PIVOT_COLUMNS,field),*col=dx_quote(key,'"'); free(key);
        char *sql=dx_printf("SELECT DISTINCT %s FROM (%s) pivot_domains ORDER BY %s NULLS FIRST LIMIT 129",col,input,col); free(col);
        bool queried=database_execute(db,sql,&r,error); free(sql); if (!queried) goto done;
        size_t count=(size_t)duckdb_row_count(&r); if (!count) { count=1; plan->empty_input=true; }
        if (count>(128-keys)/(product*measures)) { fail(error,"Pivot exceeds 128 result columns. Filter the column fields or use fewer measures."); goto done; }
        PivotDomain *d=&domains[field]; d->count=count; d->values=dx_alloc(count*sizeof(char *)); product*=count;
        for (size_t i=0;i<duckdb_row_count(&r);i++) if (!duckdb_value_is_null(&r,0,i)) {
            char *text=duckdb_value_varchar(&r,0,i); if (!text) { fail(error,"Cannot display a pivot column value."); goto done; }
            size_t bytes=strlen(text)+1; domain_bytes+=bytes;
            if (domain_bytes>4*1024*1024) { duckdb_free(text); fail(error,"Pivot column values exceed 4 MiB. Use shorter or fewer categories."); goto done; }
            d->values[i]=dx_strdup(text); duckdb_free(text);
        }
        duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
        d->list=dx_strdup("");
        for (size_t i=0;i<count;i++) {
            /* Ordinal SQL aliases never collide with literal values such as "NULL". */
            char *value=d->values[i] ? dx_quote(d->values[i],'\'') : dx_strdup("NULL");
            char *alias=dx_printf("\"category_%zu\"",i);
            dx_append(&d->list,"%s%s AS %s",i ? "," : "",value,alias); free(value); free(alias);
            if (strlen(d->list)>4*1024*1024) { fail(error,"Pivot column definition exceeds 4 MiB."); goto done; }
        }
        lists[field]=d->list;
    }
    plan->sql=pivot_sql_domains(source,definition,lists); plan->bytes=strlen(plan->sql)+1;
    if (plan->bytes>4*1024*1024) { fail(error,"Pivot SQL exceeds 4 MiB. Narrow the filter selection."); goto done; }
    plan->count=product*measures; plan->headers=dx_alloc(plan->count*sizeof(Column)); plan->bytes+=plan->count*sizeof(Column);
    for (size_t group=0;group<product;group++) {
        size_t positions[DX_PIVOT_FIELDS]={0},remaining=group;
        for (int f=(int)definition->counts[PIVOT_COLUMNS]-1;f>=0;f--) { positions[f]=remaining%domains[f].count; remaining/=domains[f].count; }
        char *label=dx_strdup("");
        for (size_t f=0;f<definition->counts[PIVOT_COLUMNS];f++) { PivotDomain *d=&domains[f]; const char *raw=d->values[positions[f]]; char display[64]; pivot_date_label(definition->fields[PIVOT_COLUMNS][f].grain,raw,display); dx_append(&label,"%s%s",f ? " · " : "",raw ? definition->fields[PIVOT_COLUMNS][f].grain ? display : raw : "NULL"); }
        for (size_t measure=0;measure<measures;measure++) {
            const PivotField *field=&definition->fields[PIVOT_VALUES][measure]; char *name=aggregate_measure_label(field->aggregate,field->name);
            bool has_columns=definition->counts[PIVOT_COLUMNS]!=0 && !plan->empty_input; size_t at=group*measures+measure;
            plan->bytes+=strlen(has_columns ? label : name)+1+(has_columns && measures>1 ? strlen(name)+1 : 0);
            if (plan->bytes>4*1024*1024) { free(name); free(label); fail(error,"Pivot headers exceed 4 MiB. Use shorter or fewer categories."); goto done; }
            Column *header=&plan->headers[at];
            header->caption=dx_strdup(has_columns ? label : name); if (has_columns && measures>1) header->detail=dx_strdup(name); free(name);
            header->category_count=has_columns ? definition->counts[PIVOT_COLUMNS] : 0;
            for (size_t f=0;f<header->category_count;f++) header->category_grains[f]=definition->fields[PIVOT_COLUMNS][f].grain;
            header->categories=dx_alloc(header->category_count*sizeof(char *)); plan->bytes+=header->category_count*sizeof(char *);
            for (size_t f=0;f<header->category_count;f++) if (domains[f].values[positions[f]]) {
                header->categories[f]=dx_strdup(domains[f].values[positions[f]]); plan->bytes+=strlen(header->categories[f])+1;
            }
            if (plan->bytes>4*1024*1024) { free(label); fail(error,"Pivot headers exceed 4 MiB. Use shorter or fewer categories."); goto done; }
        }
        free(label);
    }
    ok=true;
 done:
    duckdb_destroy_result(&r); free(input);
    for (size_t f=0;f<DX_PIVOT_FIELDS;f++) { PivotDomain *d=&domains[f]; for (size_t i=0;i<d->count;i++) free(d->values[i]); free(d->values); free(d->list); }
    if (!ok) pivot_plan_free(plan); return ok;
}
static bool pivot_materialize(Database *db,Analysis *source,const PivotDefinition *definition,bool force,char **error) {
    if (!pivot_valid(definition,error)) return false;
    if (!definition->counts[PIVOT_VALUES]) { fail(error,"Add a field to Values to build the pivot."); return false; }
    bool ok=source_prepare(db,source,NULL,error); if (!ok || !resolve_analysis_types(db,source,error)) return false;
    PivotDefinition typed=*definition; if (!resolve_pivot_types(db,source,&typed,error)) return false; definition=&typed;
    char *base=pivot_sql(source,definition); if (strlen(base)>4*1024*1024) { free(base); fail(error,"Pivot SQL exceeds 4 MiB. Narrow the filter selection."); return false; }
    char *key=dx_printf("%d:%s\n%s",source->kind,source->path ? source->path : "",base); duckdb_result r={0};
    if (!db->pivot_configured) {
        if (!database_execute(db,"SET pivot_limit=128",&r,error)) { free(base); free(key); return false; }
        duckdb_destroy_result(&r); db->pivot_configured=true;
    }
    if (force || !db->pivot_key || strcmp(db->pivot_key,key)) {
        pivot_cache_clear(db); PivotPlan plan={0};
        if (!pivot_plan(db,source,definition,&plan,error)) { free(base); free(key); return false; }
        char *sql=dx_printf("CREATE OR REPLACE TEMP TABLE _duxcel_pivot_result AS %s",plan.sql);
        ok=!cancelled(db,error) && database_execute(db,sql,&r,error); free(sql);
        if (ok) { duckdb_destroy_result(&r); db->pivot_key=key; key=NULL; db->pivot_sql=plan.sql; db->pivot_headers=plan.headers; db->pivot_columns=plan.count; memset(&plan,0,sizeof(plan)); }
        pivot_plan_free(&plan);
        if (!ok) { free(base); free(key); return false; }
    }
    free(base); free(key); return !cancelled(db,error);
}
bool database_pivot(Database *db,Analysis *source,const PivotDefinition *definition,uint64_t offset,int64_t known_total,Page *p,char **error) {
    double start=dx_millis(); memset(p,0,sizeof(*p)); p->offset=offset;
    if (!pivot_materialize(db,source,definition,known_total<0,error)) return false;
    duckdb_result r={0}; bool ok;
    char *sql=dx_printf("SELECT * FROM _duxcel_pivot_result LIMIT %d OFFSET %" PRIu64,DX_PAGE_ROWS,offset);
    Analysis literal={0}; ok=execute_analysis(db,&literal,sql,&r,error); free(sql); if (!ok) return false;
    if (duckdb_column_count(&r)>128) { duckdb_destroy_result(&r); fail(error,"Pivot exceeds 128 result columns. Filter the column fields or use fewer measures."); return false; }
    ok=result_page(&r,p,4*1024*1024,error); duckdb_destroy_result(&r); if (!ok) return false;
    size_t keys=definition->counts[PIVOT_ROWS];
    if (p->column_count!=keys+db->pivot_columns) { fail(error,"Pivot schema changed. Refresh the pivot."); return false; }
    p->sql=dx_strdup(db->pivot_sql); p->bytes+=strlen(p->sql)+1;
    for (size_t c=0;c<keys;c++) if (definition->fields[PIVOT_ROWS][c].grain) {
        Column *column=&p->columns[c]; char *label=pivot_field_label(&definition->fields[PIVOT_ROWS][c]);
        p->bytes-=strlen(column->name)+1; free(column->name); column->name=label; p->bytes+=strlen(label)+1;
        column->date_grain=definition->fields[PIVOT_ROWS][c].grain;
    }
    for (size_t c=keys;c<p->column_count;c++) {
        Column *col=&p->columns[c],*header=&db->pivot_headers[c-keys];
        col->caption=dx_strdup(header->caption); if (header->detail) col->detail=dx_strdup(header->detail);
        col->category_count=header->category_count; col->categories=dx_alloc(col->category_count*sizeof(char *));
        memcpy(col->category_grains,header->category_grains,sizeof(col->category_grains));
        p->bytes+=strlen(col->caption)+1+(col->detail ? strlen(col->detail)+1 : 0)+col->category_count*sizeof(char *);
        for (size_t f=0;f<col->category_count;f++) if (header->categories[f]) {
            col->categories[f]=dx_strdup(header->categories[f]); p->bytes+=strlen(col->categories[f])+1;
        }
    }
    if (p->bytes>4*1024*1024) { fail(error,"Pivot page exceeds its memory limit. Use fewer fields or shorter values."); return false; }
    if (known_total>=0) p->total=(uint64_t)known_total;
    else {
        db->count_queries++;
        if (!database_execute(db,"SELECT count(*) FROM _duxcel_pivot_result",&r,error)) return false;
        p->total=duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r);
    }
    p->query_ms=dx_millis()-start; return !cancelled(db,error);
}
bool database_pivot_totals(Database *db,Analysis *source,const PivotDefinition *definition,Page *page,char **error) {
    if (page->totals) return true;
    if (!page->column_count || !pivot_materialize(db,source,definition,false,error)) return false;
    PivotDefinition typed=*definition; if (!resolve_pivot_types(db,source,&typed,error)) return false;
    size_t rows=typed.counts[PIVOT_ROWS],columns=typed.counts[PIVOT_COLUMNS],measures=typed.counts[PIVOT_VALUES];
    uint64_t row_mask=(UINT64_C(1)<<columns)-1,column_mask=((UINT64_C(1)<<rows)-1)<<columns,grand_mask=(UINT64_C(1)<<(rows+columns))-1;
    if (page->column_count!=rows+db->pivot_columns) { fail(error,"Pivot totals schema changed. Refresh the view."); return false; }
    double start=dx_millis(); duckdb_result result={0}; bool ok=false;
    if (!db->pivot_totals_valid) {
        char *base=pivot_totals_sql(source,&typed),*sql=dx_printf("CREATE OR REPLACE TEMP TABLE _duxcel_pivot_totals AS %s",base); free(base);
        bool queried=strlen(sql)<=4*1024*1024 && !cancelled(db,error) && database_execute(db,sql,&result,error); free(sql);
        if (!queried) { if (!*error) fail(error,"Pivot totals SQL exceeds its memory limit."); duckdb_destroy_result(&result); return false; }
        duckdb_destroy_result(&result); memset(&result,0,sizeof(result)); db->pivot_totals_valid=true;
    }
    PivotTotals *totals=dx_alloc(sizeof(*totals));
    char *sql=dx_strdup("SELECT ");
    for (size_t m=0;m<measures;m++) dx_append(&sql,"%sm%zu",m ? "," : "",m);
    dx_append(&sql," FROM _duxcel_pivot_totals WHERE mask=%" PRIu64,row_mask);
    if (rows) { dx_append(&sql," ORDER BY "); for (size_t d=0;d<rows;d++) dx_append(&sql,"%sd%zu NULLS FIRST",d ? "," : "",d); }
    dx_append(&sql," LIMIT %d OFFSET %" PRIu64,DX_PAGE_ROWS,page->offset);
    bool queried=!cancelled(db,error) && database_execute(db,sql,&result,error); free(sql);
    if (!queried || !result_page(&result,&totals->rows,4*1024*1024,error)) goto done;
    duckdb_destroy_result(&result); memset(&result,0,sizeof(result));
    if (totals->rows.row_count!=page->row_count || totals->rows.column_count!=measures) { fail(error,"Pivot totals no longer match the result page. Refresh the view."); goto done; }
    totals->rows.offset=page->offset; totals->rows.total=page->total;
    totals->has_columns=columns && db->pivot_headers[0].category_count;
    size_t category_values=totals->has_columns ? db->pivot_columns : 0;
    sql=dx_strdup("SELECT ");
    for (size_t c=0;c<category_values+measures;c++) {
        size_t measure=c<category_values ? c%measures : c-category_values;
        bool count=typed.fields[PIVOT_VALUES][measure].aggregate==AGG_COUNT || typed.fields[PIVOT_VALUES][measure].aggregate==AGG_DISTINCT;
        dx_append(&sql,"%s%smax(m%zu) FILTER (WHERE mask=%" PRIu64,c ? "," : "",count ? "coalesce(" : "",measure,c<category_values ? column_mask : grand_mask);
        if (c<category_values) {
            const Column *header=&db->pivot_headers[c];
            for (size_t d=0;d<columns;d++) {
                char *literal=header->categories[d] ? dx_quote(header->categories[d],'\'') : dx_strdup("NULL");
                dx_append(&sql," AND d%zu IS NOT DISTINCT FROM %s",rows+d,literal); free(literal);
            }
        }
        dx_append(&sql,")%s AS total_%zu",count ? ",0)" : "",c);
        if (strlen(sql)>4*1024*1024) { free(sql); fail(error,"Pivot totals definition exceeds its memory limit."); goto done; }
    }
    dx_append(&sql," FROM _duxcel_pivot_totals");
    queried=!cancelled(db,error) && database_execute(db,sql,&result,error); free(sql);
    if (!queried || !result_page(&result,&totals->columns,4*1024*1024,error)) goto done;
    if (totals->columns.row_count!=1 || totals->columns.column_count!=category_values+measures || page->bytes+totals->rows.bytes+totals->columns.bytes+sizeof(*totals)>4*1024*1024) {
        fail(error,"Pivot totals exceed the page memory limit. Use fewer fields or shorter values."); goto done;
    }
    if (cancelled(db,error)) goto done;
    page->totals=totals; page->bytes+=totals->rows.bytes+totals->columns.bytes+sizeof(*totals); page->query_ms+=dx_millis()-start; totals=NULL; ok=true;
 done:
    duckdb_destroy_result(&result);
    if (totals) { page_free(&totals->rows); page_free(&totals->columns); free(totals); }
    return ok;
}
void page_free(Page *p) {
    if (p->totals) { page_free(&p->totals->rows); page_free(&p->totals->columns); free(p->totals); }
    for (size_t i = 0; i < p->column_count; i++) column_free(&p->columns[i]);
    for (size_t i = 0; i < p->column_count * p->row_count; i++) duckdb_free(p->cells[i].text);
    relations_free(p->relations,p->relation_count);
    free(p->columns); free(p->cells); free(p->sql); memset(p, 0, sizeof(*p));
}
void distinct_free(DistinctPage *p) {
    for (size_t i=0;i<p->count;i++) { free(p->values[i].label); free(p->values[i].value); free(p->values[i].upper); free(p->values[i].key); }
    free(p->values); memset(p,0,sizeof(*p));
}
bool database_pivot_distinct(Database *db,Analysis *a,const PivotDefinition *context,const char *column,const char *search,int level,
    const char *start,const char *end,uint64_t offset,int64_t known_total,DistinctPage *p,char **error) {
    memset(p,0,sizeof(*p)); p->offset=offset;
    if (level < -1 || level>4) { fail(error,"Invalid date hierarchy level"); return false; }
    if (!source_prepare(db,a,NULL,error) || !resolve_analysis_types(db,a,error)) return false;
    PivotDefinition typed={0}; if (context) { typed=*context; if (!resolve_pivot_types(db,a,&typed,error)) return false; context=&typed; }
    Analysis source=*a; if (!context) { source.group_column=NULL; source.measure_column=NULL; } source.sort_column=NULL; source.sort_count=0;
    if (!context) {
        if (source.filter_column && !strcmp(source.filter_column,column)) source.filter_column=NULL;
        source.filter_count=0; for (size_t i=0;i<a->filter_count;i++) if (strcmp(a->filters[i].column,column)) source.filters[source.filter_count++]=a->filters[i];
    }
    char *base=analysis_sql(&source,true),*col=dx_quote(column,'"'),*find=dx_quote(search ? search : "",'\'');
    if (context) for (size_t i=0;i<context->counts[PIVOT_FILTERS];i++) {
        const PivotField *f=&context->fields[PIVOT_FILTERS][i]; if (!strcmp(f->name,column)) continue;
        char *condition=analysis_filter_typed(f->name,f->filter,f->value,NULL,f->number),*next=dx_printf("SELECT * FROM (%s) pivot_available WHERE %s",base,condition);
        free(base); free(condition); base=next;
    }
    char *where=dx_printf("contains(COALESCE(CAST(%s AS VARCHAR), 'NULL'), %s)",col,find);
    if (start && *start && end && *end) {
        char *lo=dx_quote(start,'\''),*hi=dx_quote(end,'\'');
        dx_append(&where," AND %s >= cast_to_type(%s,%s) AND %s < cast_to_type(%s,%s)",col,lo,col,col,hi,col); free(lo); free(hi);
    }
    const char *units[]={"year","month","day","hour","minute"};
    const char *formats[]={"%Y","%B","%d","%H:00","%H:%M"};
    char *value=level<0 ? dx_printf("CAST(%s AS VARCHAR)",col) : dx_printf("CAST(date_trunc('%s',%s) AS VARCHAR)",units[level],col);
    char *label=level<0 ? dx_strdup(value) : dx_printf("strftime(date_trunc('%s',%s),'%s')",units[level],col,formats[level]);
    char *upper=level<0 ? dx_strdup("NULL::VARCHAR") : dx_printf("CAST(date_trunc('%s',%s)+INTERVAL '1 %s' AS VARCHAR)",units[level],col,units[level]);
    char *relation=dx_printf("SELECT DISTINCT %s AS value,%s AS label,%s AS upper_bound FROM (%s) available WHERE %s",value,label,upper,base,where);
    char *sql=dx_printf("SELECT * FROM (%s) distinct_values ORDER BY value NULLS FIRST LIMIT %d OFFSET %" PRIu64,relation,DX_DISTINCT_ROWS,offset);
    duckdb_result r={0}; bool ok=execute_analysis(db,&source,sql,&r,error); free(sql);
    if (ok) {
        p->count=(size_t)duckdb_row_count(&r); p->bytes=p->count*sizeof(*p->values); p->values=dx_alloc(p->bytes);
        for (size_t i=0;i<p->count;i++) {
            DistinctValue *v=&p->values[i]; v->is_null=duckdb_value_is_null(&r,0,i);
            char *key=duckdb_value_varchar(&r,0,i),*text=duckdb_value_varchar(&r,1,i),*hi=duckdb_value_varchar(&r,2,i);
            v->value=v->is_null ? NULL : dx_strdup(key); v->label=dx_strdup(v->is_null ? "NULL" : text); v->upper=hi ? dx_strdup(hi) : NULL;
            v->key=v->is_null ? NULL : v->upper ? dx_printf("%s\t%s",v->value,v->upper) : dx_strdup(v->value);
            p->bytes+=strlen(v->label)+1+(v->value ? strlen(v->value)+1 : 0)+(v->upper ? strlen(v->upper)+1 : 0)+(v->key ? strlen(v->key)+1 : 0);
            duckdb_free(key); duckdb_free(text); duckdb_free(hi);
            if (p->bytes>4*1024*1024) { fail(error,"Available values exceed the 4 MiB page limit. Narrow the search."); ok=false; break; }
        }
        duckdb_destroy_result(&r);
        if (ok && known_total>=0) p->total=(uint64_t)known_total;
        else if (ok) {
            sql=dx_printf("SELECT count(*) FROM (%s) counted",relation);
            ok=execute_analysis(db,&source,sql,&r,error); free(sql);
            if (ok) { p->total=duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r); }
        }
    }
    free(base); free(col); free(find); free(where); free(value); free(label); free(upper); free(relation);
    if (!ok) distinct_free(p); return ok;
}
bool database_distinct(Database *db,Analysis *a,const char *column,const char *search,int level,const char *start,const char *end,uint64_t offset,int64_t known_total,DistinctPage *p,char **error) {
    return database_pivot_distinct(db,a,NULL,column,search,level,start,end,offset,known_total,p,error);
}
static bool output_reserve(char **buffer,size_t *capacity,size_t required) {
    if (required>8*1024*1024+1) return false;
    if (required>*capacity) {
        size_t next=*capacity ? *capacity*2 : 1024;
        if (next<required) next=required;
        if (next>8*1024*1024+1) next=8*1024*1024+1;
        *buffer=dx_realloc(*buffer,next); *capacity=next;
    }
    return true;
}
static bool copy_result(Database *db,Analysis *a,const char *relation,uint64_t first,uint64_t rows,size_t col,size_t cols,duckdb_result *result,char **error) {
    if (!cols || cols>DX_COPY_CELLS || rows>DX_COPY_CELLS/cols) {
        fail(error,"Copy up to 20,000 cells at a time. Select a smaller range."); return false;
    }
    char *base=dx_strdup(relation); duckdb_prepared_statement schema=NULL; bool borrowed=a->kind!=SOURCE_NONE;
    /* JSON checklist predicates can defer DuckDB's prepared output metadata.
       Filters/sorts do not change source columns; use the cached output shape. */
    if (borrowed) schema=source_schema(db,a,true,error);
    else if (!prepare_analysis(db,a,base,&schema,error)) { free(base); return false; }
    if (!schema) { free(base); return false; }
    size_t count=(size_t)duckdb_prepared_statement_column_count(schema);
    if (col>=count || cols>count-col) {
        free(base); if (!borrowed) duckdb_destroy_prepare(&schema); fail(error,"Selection no longer matches this relation."); return false;
    }
    char *projection=dx_strdup("");
    for (size_t c=col;c<col+cols;c++) {
        const char *name=duckdb_prepared_statement_column_name(schema,c);
        char *quoted=dx_quote(name,'"'); dx_append(&projection,"%s%s",c==col ? "" : ", ",quoted);
        free(quoted); duckdb_free((void *)name);
    }
    if (!borrowed) duckdb_destroy_prepare(&schema);
    char *sql=dx_printf("SELECT %s FROM (\n%s\n) AS copied LIMIT %" PRIu64 " OFFSET %" PRIu64,projection,base,rows,first);
    free(projection); free(base);
    bool ok=execute_analysis(db,a,sql,result,error); free(sql); return ok;
}

static bool copy_query(Database *db,Analysis *a,const char *relation,uint64_t first,uint64_t rows,size_t col,size_t cols,bool header,char *const *headers,char **tsv,char **error) {
    if ((!rows && !header) || !cols || cols>DX_COPY_CELLS || rows+(header ? 1 : 0)>DX_COPY_CELLS/cols) {
        fail(error,"Copy up to 20,000 cells at a time. Select a smaller range."); return false;
    }
    duckdb_result r={0}; bool ok=true;
    /* LIMIT 0 still obtains column names for a header-only legacy copy. */
    if (!copy_result(db,a,relation,first,rows,col,cols,&r,error)) return false;
    char *text=NULL; size_t capacity=0,bytes=0;
    idx_t total=duckdb_row_count(&r)+(header ? 1 : 0);
    for (idx_t row=0;row<total && ok;row++) {
        if (cancelled(db,error)) { ok=false; break; }
        for (size_t c=0;c<cols;c++) {
            bool name=header && row==0; idx_t data_row=row-(header ? 1 : 0);
            char *value=name || duckdb_value_is_null(&r,c,data_row) ? NULL : duckdb_value_varchar(&r,c,data_row);
            const char *s=name ? headers ? headers[col+c] : duckdb_column_name(&r,c) : value ? value : ""; size_t n=strlen(s),quotes=0; bool escape=!n && (name || value);
            for (size_t i=0;i<n;i++) { if (s[i]=='"') quotes++; if (s[i]=='\t'||s[i]=='\r'||s[i]=='\n'||s[i]=='"') escape=true; }
            size_t needed=n+quotes+(escape ? 2 : 0)+(c ? 1 : 0);
            if (needed>8*1024*1024 || !output_reserve(&text,&capacity,bytes+needed+2)) { duckdb_free(value); ok=false; break; }
            if (c) text[bytes++]='\t';
            if (escape) text[bytes++]='"';
            for (size_t i=0;i<n;i++) { if (s[i]=='"') text[bytes++]='"'; text[bytes++]=s[i]; }
            if (escape) text[bytes++]='"'; duckdb_free(value);
        }
        if (ok && row+1<total) text[bytes++]='\n';
    }
    duckdb_destroy_result(&r);
    if (!ok) { free(text); if (!*error) fail(error,"Clipboard output exceeds 8 MiB. Select a smaller range."); return false; }
    if (!text) text=dx_alloc(1); text[bytes]=0; *tsv=text; return true;
}
bool database_copy(Database *db,Analysis *a,uint64_t first,uint64_t rows,size_t col,size_t cols,char **tsv,char **error) {
    if (!source_prepare(db,a,NULL,error) || !resolve_analysis_types(db,a,error)) return false;
    char *base=analysis_sql(a,true); bool ok=copy_query(db,a,base,first,rows,col,cols,false,NULL,tsv,error); free(base); return ok;
}
void statistics_free(SelectionStats *s) {
    free(s->sum); free(s->average); free(s->minimum); free(s->maximum); memset(s,0,sizeof(*s));
}
/* Representation only: the engine has already summed/compared BIGNUM units at
   scale 38. Insert the decimal point without rounding through a C double. */
static char *statistics_exact(duckdb_result *r,idx_t column) {
    if (duckdb_value_is_null(r,column,0)) return NULL;
    char *raw=duckdb_value_varchar(r,column,0); const char *digits=raw+(*raw=='-');
    size_t length=strlen(digits),sign=digits!=raw,whole=length>38 ? length-38 : 0;
    char *text=dx_alloc(sign+(whole ? length+1 : 40)+1); size_t at=0;
    if (sign) text[at++]='-';
    if (whole) { memcpy(text+at,digits,whole); at+=whole; text[at++]='.'; memcpy(text+at,digits+whole,38); at+=38; }
    else { text[at++]='0'; text[at++]='.'; memset(text+at,'0',38-length); at+=38-length; memcpy(text+at,digits,length); at+=length; }
    while (at && text[at-1]=='0') at--; if (at && text[at-1]=='.') at--; text[at]=0;
    if (!strcmp(text,"-0")) { text[0]='0'; text[1]=0; }
    duckdb_free(raw); return text;
}
static char *statistics_float(duckdb_result *r,idx_t column,bool *overflow) {
    if (duckdb_value_is_null(r,column,0)) return NULL;
    double value=duckdb_value_double(r,column,0); if (!isfinite(value)) { if (overflow) *overflow=true; return NULL; }
    return dx_printf("%.15g",value);
}
static char *statistics_cells_query(Database *db,Analysis *a,const PivotDefinition *pivot,uint64_t first,uint64_t rows,size_t col,size_t cols,char **error) {
    if (!cols || cols>256 || first>DX_SHEET_ROWS || rows>DX_SHEET_ROWS-first) { fail(error,"Selection statistics support up to 256 columns inside the worksheet."); return NULL; }
    if (pivot) { if (!pivot_materialize(db,a,pivot,false,error)) return NULL; }
    else if (!source_prepare(db,a,NULL,error) || !resolve_analysis_types(db,a,error)) return NULL;
    Analysis literal={0}; const Analysis *parameters=pivot ? &literal : a;
    char *base=pivot ? dx_strdup("SELECT * FROM _duxcel_pivot_result") : analysis_sql(a,true);
    duckdb_prepared_statement schema=NULL; char *projection=NULL,*values=NULL,*sql=NULL; bool borrowed=!pivot && a->kind!=SOURCE_NONE;
    if (strlen(base)>4*1024*1024) { fail(error,"Selection query exceeds 4 MiB."); goto done; }
    if (borrowed) schema=source_schema(db,a,true,error);
    else if (!prepare_analysis(db,parameters,base,&schema,error)) goto done;
    if (!schema) goto done;
    size_t count=(size_t)duckdb_prepared_statement_column_count(schema);
    if (col>=count || cols>count-col) { fail(error,"Selection no longer matches this relation."); goto done; }
    projection=dx_strdup(""); values=dx_strdup("");
    for (size_t i=0;i<cols;i++) {
        if (cancelled(db,error)) goto done;
        const char *name=duckdb_prepared_statement_column_name(schema,col+i); char *quoted=dx_quote(name,'"'); duckdb_free((void *)name);
        duckdb_logical_type type=duckdb_prepared_statement_column_logical_type(schema,col+i); NumberType number=number_type(type); duckdb_destroy_logical_type(&type);
        dx_append(&projection,"%s%s AS c%zu",i ? "," : "",quoted,i); free(quoted);
        bool numeric=number.kind!=NUM_NONE,floating=number.kind==NUM_FLOAT;
        char *units=numeric && !floating ? dx_printf("CAST(replace(CAST(c%zu AS VARCHAR),'.','') || repeat('0',%u) AS BIGNUM)",i,38-(number.kind==NUM_DECIMAL ? number.scale : 0)) : dx_strdup("NULL::BIGNUM");
        char *value=numeric ? dx_printf("CAST(c%zu AS DOUBLE)",i) : dx_strdup("NULL::DOUBLE");
        char *text=floating ? dx_printf("CASE WHEN c%zu=0 THEN '0' ELSE CAST(c%zu AS VARCHAR) END",i,i) : dx_printf("CAST(c%zu AS VARCHAR)",i);
        dx_append(&values,"%sSELECT c%zu IS NOT NULL AS present,%s AS numeric,typeof(c%zu) AS type,%s AS text,%s AS exact,%s AS value,%s AS floating FROM selected",i ? " UNION ALL " : "",i,numeric ? "true" : "false",i,text,units,value,floating ? "true" : "false");
        free(units); free(value); free(text);
        if (strlen(projection)+strlen(values)>2*1024*1024) { fail(error,"Selection statistics query exceeds 2 MiB."); goto done; }
    }
    sql=dx_printf("WITH selected AS MATERIALIZED (SELECT %s FROM (%s) AS source LIMIT %" PRIu64 " OFFSET %" PRIu64 ") %s",projection,base,rows,first,values);
done:
    if (schema && !borrowed) duckdb_destroy_prepare(&schema);
    free(base); free(projection); free(values); return sql;
}
static bool statistics_aggregate(Database *db,const Analysis *parameters,const char *cells,SelectionStats *stats,char **error) {
    SelectionStats out={0}; duckdb_result r={0}; bool ok=false;
    char *sql=dx_printf("WITH cells AS (%s) "
        "SELECT count(*),count(*) FILTER (WHERE present),count(*) FILTER (WHERE numeric AND present),count(DISTINCT (type,text)) FILTER (WHERE present),"
        "CAST(sum(exact) AS VARCHAR),CAST(min(exact) AS VARCHAR),CAST(max(exact) AS VARCHAR),sum(value),avg(value),min(value),max(value),"
        "coalesce(bool_or(floating AND present),false),coalesce(bool_or(numeric AND present AND NOT isfinite(value)),false) FROM cells",cells);
    if (!execute_analysis(db,parameters,sql,&r,error)) goto done;
    out.cells=duckdb_value_uint64(&r,0,0); out.filled=duckdb_value_uint64(&r,1,0); out.numeric=duckdb_value_uint64(&r,2,0); out.distinct=duckdb_value_uint64(&r,3,0);
    out.approximate=duckdb_value_boolean(&r,11,0); out.non_finite=duckdb_value_boolean(&r,12,0);
    if (!out.non_finite) {
        out.sum=out.approximate ? statistics_float(&r,7,&out.sum_overflow) : statistics_exact(&r,4);
        out.minimum=out.approximate ? statistics_float(&r,9,NULL) : statistics_exact(&r,5);
        out.maximum=out.approximate ? statistics_float(&r,10,NULL) : statistics_exact(&r,6);
        out.average=statistics_float(&r,8,&out.average_overflow);
    }
    statistics_free(stats); *stats=out; memset(&out,0,sizeof(out)); ok=true;
done:
    statistics_free(&out); duckdb_destroy_result(&r); free(sql); return ok;
}
bool database_statistics(Database *db,Analysis *a,const PivotDefinition *pivot,uint64_t first,uint64_t rows,size_t col,size_t cols,SelectionStats *stats,char **error) {
    double started=dx_millis(); char *cells=statistics_cells_query(db,a,pivot,first,rows,col,cols,error); if (!cells) return false;
    Analysis literal={0}; bool ok=statistics_aggregate(db,pivot ? &literal : a,cells,stats,error); free(cells);
    if (ok) stats->query_ms=dx_millis()-started; return ok;
}
size_t source_snapshot_bind(SourceSnapshot *snapshot,uint64_t id,const Analysis *source,char **error) {
    if (snapshot->count>DX_MAX_OBJECTS || snapshot->bytes>8*1024*1024 || !id || !source || source->kind<SOURCE_CSV || source->kind>SOURCE_DEMO || !analysis_query_valid(source,error) || !analysis_filters_valid(source,error) || !analysis_sorts_valid(source,error)) { fail(error,"Invalid table source snapshot."); return SIZE_MAX; }
    for (size_t i=0;i<snapshot->count;i++) if (snapshot->ids[i]==id) {
        if (analysis_same_result(&snapshot->items[i],source)) return i+1;
        fail(error,"The table source changed while capturing this selection."); return SIZE_MAX;
    }
    size_t bytes=analysis_text_bytes(source);
    if (snapshot->count>=DX_MAX_OBJECTS || bytes>8*1024*1024-snapshot->bytes) { fail(error,"Selection table sources exceed the 8 MiB snapshot limit."); return SIZE_MAX; }
    snapshot->items[snapshot->count]=analysis_copy(source); snapshot->ids[snapshot->count]=id; snapshot->bytes+=bytes;
    return ++snapshot->count;
}
void source_snapshot_free(SourceSnapshot *snapshot) {
    for (size_t i=0;i<snapshot->count && i<DX_MAX_OBJECTS;i++) analysis_free(&snapshot->items[i]); memset(snapshot,0,sizeof(*snapshot));
}
void statistics_request_free(StatisticsRequest *request) {
    if (!request) return;
    for (size_t i=0;i<request->count && i<DX_MAX_OBJECTS+1;i++) pivot_free(&request->ranges[i].pivot);
    source_snapshot_free(&request->sources);
    for (size_t i=0;i<request->text_count;i++) free(request->texts[i]);
    free(request->texts); free(request);
}
bool statistics_request_text(StatisticsRequest *request,const char *text,char **error) {
    if (!text) return true;
    size_t bytes=strlen(text)+1;
    if (request->text_count>=32768 || bytes>8*1024*1024-request->text_bytes) { fail(error,"Selection labels exceed the 8 MiB statistics snapshot limit."); return false; }
    if (request->text_count==request->text_capacity) { request->text_capacity=request->text_capacity ? request->text_capacity*2 : 32; request->texts=dx_realloc(request->texts,request->text_capacity*sizeof(*request->texts)); }
    request->texts[request->text_count++]=dx_strdup(text); request->text_bytes+=bytes; return true;
}
bool database_statistics_ranges(Database *db,Analysis *a,StatisticsRequest *request,SelectionStats *stats,char **error) {
    double started=dx_millis(); Analysis literal={0}; SelectionStats out={0}; duckdb_result r={0}; bool ok=false;
    if (!request || request->count>DX_MAX_OBJECTS+1 || request->sources.count>DX_MAX_OBJECTS || request->sources.bytes>8*1024*1024 || request->text_count>32768 || request->text_bytes>8*1024*1024 || request->cells>DX_SHEET_ROWS*16384) { fail(error,"Invalid worksheet statistics snapshot."); return false; }
    if (cancelled(db,error) || !database_execute(db,"CREATE OR REPLACE TEMP TABLE _duxcel_selection_cells(present BOOLEAN,numeric BOOLEAN,type VARCHAR,text VARCHAR,exact BIGNUM,value DOUBLE,floating BOOLEAN)",&r,error)) return false;
    duckdb_destroy_result(&r);
    for (size_t i=0;i<request->count;i++) {
        StatisticsRange *range=&request->ranges[i];
        if (range->source>request->sources.count) { fail(error,"Missing selection table source."); goto done; }
        Analysis *source=range->source ? &request->sources.items[range->source-1] : a;
        char *cells=statistics_cells_query(db,source,range->is_pivot ? &range->pivot : NULL,range->first,range->rows,range->column,range->columns,error); if (!cells) goto done;
        char *sql=dx_printf("INSERT INTO _duxcel_selection_cells SELECT * FROM (%s) selected_cells WHERE present",cells); free(cells);
        bool inserted=execute_analysis(db,range->is_pivot ? &literal : source,sql,&r,error); free(sql); duckdb_destroy_result(&r); if (!inserted) goto done;
    }
    for (size_t i=0;i<request->text_count;) {
        char *values=dx_strdup(""); size_t stop=i+256<request->text_count ? i+256 : request->text_count;
        for (;i<stop;i++) { char *quoted=dx_quote(request->texts[i],'\''); dx_append(&values,"%s(%s)",*values ? "," : "",quoted); free(quoted); }
        char *sql=dx_printf("INSERT INTO _duxcel_selection_cells SELECT true,false,'VARCHAR',v,NULL::BIGNUM,NULL::DOUBLE,false FROM (VALUES %s) labels(v)",values); free(values);
        bool inserted=execute_analysis(db,&literal,sql,&r,error); free(sql); duckdb_destroy_result(&r); if (!inserted) goto done;
    }
    if (!statistics_aggregate(db,&literal,"SELECT * FROM _duxcel_selection_cells",&out,error)) goto done;
    out.cells=request->cells; out.query_ms=dx_millis()-started; statistics_free(stats); *stats=out; memset(&out,0,sizeof(out)); ok=true;
done:
    /* Clear large staging data on success, error and cancellation. A following
       request also replaces this table before reuse. Never mask the real error. */
    { char *ignored=NULL; database_execute(db,"DROP TABLE IF EXISTS _duxcel_selection_cells",&r,&ignored); free(ignored); duckdb_destroy_result(&r); }
    statistics_free(&out); return ok;
}
bool database_copy_page(Database *db,Analysis *a,const PivotDefinition *pivot,uint64_t first,size_t rows,size_t col,size_t cols,Page *page,char **error) {
    memset(page,0,sizeof(*page));
    if (!rows || !cols || cols>DX_COPY_CELLS || rows>DX_COPY_CELLS/cols) { fail(error,"Copy up to 20,000 cells at a time. Select a smaller range."); return false; }
    if (pivot) { if (!pivot_materialize(db,a,pivot,false,error)) return false; }
    else if (!source_prepare(db,a,NULL,error) || !resolve_analysis_types(db,a,error)) return false;
    char *base=pivot ? dx_strdup("SELECT * FROM _duxcel_pivot_result") : analysis_sql(a,true);
    Analysis literal={0}; duckdb_result result={0};
    bool ok=copy_result(db,pivot ? &literal : a,base,first,rows,col,cols,&result,error); free(base);
    if (ok) { ok=result_page(&result,page,8*1024*1024,error); page->offset=first; page->total=first+page->row_count; }
    duckdb_destroy_result(&result); return ok;
}
bool database_pivot_copy(Database *db,Analysis *a,const PivotDefinition *definition,uint64_t first,uint64_t rows,size_t col,size_t cols,bool header,char **tsv,char **error) {
    if (!pivot_materialize(db,a,definition,false,error)) return false;
    size_t keys=definition->counts[PIVOT_ROWS],count=keys+db->pivot_columns;
    char **headers=dx_alloc(count*sizeof(char *));
    for (size_t i=0;i<keys;i++) headers[i]=pivot_field_label(&definition->fields[PIVOT_ROWS][i]);
    for (size_t i=0;i<db->pivot_columns;i++) {
        const PivotField *measure=&definition->fields[PIVOT_VALUES][i%definition->counts[PIVOT_VALUES]];
        char *label=aggregate_measure_label(measure->aggregate,measure->name);
        headers[keys+i]=definition->counts[PIVOT_COLUMNS] ? dx_printf("%s_%s",db->pivot_headers[i].caption,label) : dx_strdup(db->pivot_headers[i].caption); free(label);
    }
    Analysis literal={0}; bool ok=copy_query(db,&literal,"SELECT * FROM _duxcel_pivot_result",first,rows,col,cols,header,headers,tsv,error);
    for (size_t i=0;i<count;i++) free(headers[i]); free(headers); return ok;
}
/* DuckDB 1.5 exports HUGEINT/UHUGEINT as DOUBLE, including nested fields. */
static bool parquet_loses_integer(duckdb_logical_type type) {
    duckdb_type id=duckdb_get_type_id(type);
    if (id==DUCKDB_TYPE_HUGEINT || id==DUCKDB_TYPE_UHUGEINT) return true;
    size_t count=id==DUCKDB_TYPE_STRUCT ? duckdb_struct_type_child_count(type) :
        id==DUCKDB_TYPE_UNION ? duckdb_union_type_member_count(type) : id==DUCKDB_TYPE_MAP ? 2 :
        id==DUCKDB_TYPE_LIST || id==DUCKDB_TYPE_ARRAY ? 1 : 0;
    for (size_t i=0;i<count;i++) {
        duckdb_logical_type child=id==DUCKDB_TYPE_STRUCT ? duckdb_struct_type_child_type(type,i) :
            id==DUCKDB_TYPE_UNION ? duckdb_union_type_member_type(type,i) :
            id==DUCKDB_TYPE_MAP ? i ? duckdb_map_type_value_type(type) : duckdb_map_type_key_type(type) :
            id==DUCKDB_TYPE_LIST ? duckdb_list_type_child_type(type) : duckdb_array_type_child_type(type);
        bool loses=parquet_loses_integer(child); duckdb_destroy_logical_type(&child);
        if (loses) return true;
    }
    return false;
}
static bool parquet_schema_safe(duckdb_prepared_statement schema,char **error) {
    for (idx_t i=0;i<duckdb_prepared_statement_column_count(schema);i++) {
        duckdb_logical_type type=duckdb_prepared_statement_column_logical_type(schema,i);
        bool loses=parquet_loses_integer(type); duckdb_destroy_logical_type(&type);
        if (loses) {
            const char *name=duckdb_prepared_statement_column_name(schema,i);
            char *message=dx_printf("Column \"%s\" contains a 128-bit integer. This DuckDB version would lose precision in Parquet. Convert it explicitly to DECIMAL or VARCHAR before exporting.",name ? name : "?");
            duckdb_free((void *)name); fail(error,message); free(message); return false;
        }
    }
    return true;
}
static char *pivot_export_sql(Database *db,const PivotDefinition *pivot,duckdb_prepared_statement schema,char **error) {
    size_t count=(size_t)duckdb_prepared_statement_column_count(schema),keys=pivot->counts[PIVOT_ROWS];
    if (count!=keys+db->pivot_columns || count>128) { fail(error,"Pivot schema changed. Refresh the pivot before exporting."); return NULL; }
    char *names[128]={0},*sql=dx_strdup("SELECT ");
    for (size_t i=0;i<count;i++) {
        const char *original=duckdb_prepared_statement_column_name(schema,i); char *label=NULL;
        if (i<keys) label=dx_strdup(original);
        else {
            Column *header=&db->pivot_headers[i-keys];
            const PivotField *measure=&pivot->fields[PIVOT_VALUES][(i-keys)%pivot->counts[PIVOT_VALUES]];
            label=dx_strdup("");
            for (size_t f=0;f<header->category_count;f++) {
                char *value=header->categories[f] ? dx_quote(header->categories[f],'\'') : dx_strdup("NULL");
                dx_append(&label,"%s%s=%s",f ? " · " : "",pivot->fields[PIVOT_COLUMNS][f].name,value); free(value);
            }
            char *aggregate=aggregate_measure_label(measure->aggregate,measure->name);
            dx_append(&label,"%s%s",header->category_count ? " · " : "",aggregate); free(aggregate);
        }
        char *name=dx_strdup(label);
        for (size_t suffix=2;;suffix++) {
            bool duplicate=false; for (size_t j=0;j<i;j++) if (identifier_equal(names[j],name)) duplicate=true;
            if (!duplicate) break;
            free(name); name=dx_printf("%s · %zu",label,suffix);
        }
        free(label); names[i]=name;
        char *from=dx_quote(original,'"'),*to=dx_quote(name,'"'); duckdb_free((void *)original);
        dx_append(&sql,"%s%s AS %s",i ? "," : "",from,to); free(from); free(to);
        if (strlen(sql)>4*1024*1024) { fail(error,"Pivot export headers exceed 4 MiB. Use shorter or fewer categories."); free(sql); sql=NULL; break; }
    }
    for (size_t i=0;i<count;i++) free(names[i]);
    if (sql) dx_append(&sql," FROM _duxcel_pivot_result"); return sql;
}
static bool export_parquet(Database *db,Analysis *a,const PivotDefinition *pivot,const char *path,uint64_t *rows,char **error) {
    if (rows) *rows=0;
    if (a->kind==SOURCE_NONE || !path || !*path) { fail(error,"Choose a data table and a Parquet destination."); return false; }
    if (a->kind!=SOURCE_DEMO && dx_same_file(a->path,path)) { fail(error,"Choose a different destination. The source file cannot be replaced by its export."); return false; }
    duckdb_prepared_statement schema=NULL; bool ok; char *base=NULL;
    if (pivot) {
        if (!pivot_materialize(db,a,pivot,false,error)) return false;
        ok=duckdb_prepare(db->connection,"SELECT * FROM _duxcel_pivot_result LIMIT 0",&schema)==DuckDBSuccess;
        if (!ok) fail(error,duckdb_prepare_error(schema));
    } else {
        if (!source_prepare(db,a,NULL,error) || !resolve_analysis_types(db,a,error)) return false;
        schema=source_schema(db,a,true,error); ok=schema!=NULL;
    }
    if (ok) ok=parquet_schema_safe(schema,error);
    if (ok && pivot) { base=pivot_export_sql(db,pivot,schema,error); ok=base!=NULL; }
    if (pivot) duckdb_destroy_prepare(&schema);
    if (!ok || cancelled(db,error)) { free(base); return false; }
    char *temporary=dx_temp_sibling(path);
    if (!temporary) { free(base); fail(error,"Cannot create the export in that directory. Choose a writable destination."); return false; }
    if (!base) base=analysis_sql(a,true);
    char *quoted=dx_quote(temporary,'\''),*sql=dx_printf("COPY (%s) TO %s (FORMAT PARQUET)",base,quoted);
    free(base); free(quoted); duckdb_result r={0};
    Analysis literal={0}; ok=execute_analysis(db,pivot ? &literal : a,sql,&r,error); free(sql);
    uint64_t count=ok && duckdb_row_count(&r) ? duckdb_value_uint64(&r,0,0) : 0; duckdb_destroy_result(&r);
    if (ok) ok=!cancelled(db,error);
    if (ok && a->kind!=SOURCE_DEMO && dx_same_file(a->path,path)) { fail(error,"The destination now refers to the source file. Choose another destination."); ok=false; }
    if (ok && !dx_replace_file(temporary,path)) { fail(error,"Could not finish the export. The existing destination has been preserved."); ok=false; }
    if (!ok) remove(temporary); else if (rows) *rows=count;
    free(temporary); return ok;
}
bool database_export_parquet(Database *db,Analysis *a,const char *path,uint64_t *rows,char **error) {
    return export_parquet(db,a,NULL,path,rows,error);
}
bool database_export_pivot_parquet(Database *db,Analysis *a,const PivotDefinition *pivot,const char *path,uint64_t *rows,char **error) {
    if (!pivot) { if (rows) *rows=0; fail(error,"Choose a pivot to export."); return false; }
    return export_parquet(db,a,pivot,path,rows,error);
}
