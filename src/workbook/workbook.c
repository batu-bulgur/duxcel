#include "workbook.h"
#include "../platform/platform.h"
#ifndef DUXCEL_BROWSER_HOST
#include <duckdb.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
static void error_set(char **error,const char *text) { free(*error); *error=dx_strdup(text ? text : "Workspace operation failed"); }
bool workbook_is_path(const char *path) {
    const char *suffixes[]={".dxls",".duxcel"}; size_t n=strlen(path);
    for (size_t i=0;i<2;i++) {
        size_t length=strlen(suffixes[i]); if (n<length) continue;
        bool same=true;
        for (size_t c=0;c<length;c++) {
            char ch=path[n-length+c]; if (ch>='A' && ch<='Z') ch=(char)(ch-'A'+'a');
            if (ch!=suffixes[i][c]) { same=false; break; }
        }
        if (same) return true;
    }
    return false;
}
void view_free(SheetView *v) {
    for (size_t i=0;i<v->column_count;i++) free(v->columns[i]);
    free(v->columns); free(v->widths); memset(v,0,sizeof(*v));
}
void workbook_init(Workbook *b) { memset(b,0,sizeof(*b)); b->user_scale=1; }
static void action_push(EditKind *actions,ColumnEdit *columns,size_t *count,EditKind kind,ColumnEdit column) {
    if (*count==DX_HISTORY) { memmove(actions,actions+1,(DX_HISTORY-1)*sizeof(*actions)); memmove(columns,columns+1,(DX_HISTORY-1)*sizeof(*columns)); --*count; }
    columns[*count]=column; actions[(*count)++]=kind;
}
unsigned worksheet_edit_effects(EditKind kind) { static const unsigned effects[]={1,2,4,6,8,10,12,14}; return (unsigned)kind<sizeof(effects)/sizeof(*effects) ? effects[kind] : 0; }
static EditKind range_edit(bool cells,bool objects,bool formats) {
    return formats ? cells ? objects ? EDIT_RANGE_FORMAT : EDIT_CELLS_FORMAT : objects ? EDIT_OBJECTS_FORMAT : EDIT_FORMAT : cells ? objects ? EDIT_RANGE : EDIT_CELLS : EDIT_OBJECTS;
}
static void action_prune(Worksheet *s,bool redo) {
    ColumnEdit *columns=redo ? s->redo_columns : s->undo_columns;
    EditKind *actions=redo ? s->redo_actions : s->undo_actions; size_t *n=redo ? &s->redo_actions_count : &s->undo_actions_count;
    size_t available[]={redo ? s->redo_count : s->undo_count,redo ? s->cells.redo_count : s->cells.undo_count,redo ? s->objects.redo_count : s->objects.undo_count,redo ? s->formats.redo_count : s->formats.undo_count};
    size_t required[4]={0};
    for (size_t i=0;i<*n;i++) for (int k=0;k<4;k++) required[k]+=!!(worksheet_edit_effects(actions[i]) & (1u<<k));
    /* A composite edit must stay aligned with both snapshot stacks. Discard a
       chronological prefix, then release orphaned snapshots in every stack. */
    size_t drop=0;
    while (drop<*n && (required[0]>available[0] || required[1]>available[1] || required[2]>available[2] || required[3]>available[3])) {
        for (int k=0;k<4;k++) required[k]-=!!(worksheet_edit_effects(actions[drop]) & (1u<<k)); drop++;
    }
    memmove(actions,actions+drop,(*n-drop)*sizeof(*actions)); memmove(columns,columns+drop,(*n-drop)*sizeof(*columns)); memset(columns+*n-drop,0,drop*sizeof(*columns)); *n-=drop;
    Analysis *stack=redo ? s->redo : s->undo; size_t *count=redo ? &s->redo_count : &s->undo_count;
    drop=*count-required[0]; for (size_t i=0;i<drop;i++) analysis_free(&stack[i]);
    memmove(stack,stack+drop,required[0]*sizeof(*stack)); memset(stack+required[0],0,drop*sizeof(*stack)); *count=required[0];
    cells_trim_history(&s->cells,redo,required[1]); objects_trim_history(&s->objects,redo,required[2]);
    format_trim_history(&s->formats,redo,required[3]);
}
void worksheet_record(Worksheet *s,EditKind kind) {
    action_push(s->undo_actions,s->undo_columns,&s->undo_actions_count,kind,(ColumnEdit){0}); s->redo_actions_count=0; memset(s->redo_columns,0,sizeof(s->redo_columns)); action_prune(s,false); action_prune(s,true);
}
void worksheet_transfer_history(Worksheet *s,bool redo) {
    size_t *from=redo ? &s->redo_actions_count : &s->undo_actions_count,*to=redo ? &s->undo_actions_count : &s->redo_actions_count;
    EditKind *a=redo ? s->redo_actions : s->undo_actions,*b=redo ? s->undo_actions : s->redo_actions;
    ColumnEdit *ca=redo ? s->redo_columns : s->undo_columns,*cb=redo ? s->undo_columns : s->redo_columns;
    if (*from) { --*from; action_push(b,cb,to,a[*from],ca[*from]); ca[*from]=(ColumnEdit){0}; }
    action_prune(s,false); action_prune(s,true);
}
static bool range_fail(char **error,const char *message) { if (error) error_set(error,message); return false; }
static bool range_overlap(SheetRange a,SheetRange b) { return a.r0<=b.r1 && b.r0<=a.r1 && a.c0<=b.c1 && b.c0<=a.c1; }
static bool range_contains(SheetRange a,SheetRange b) { return a.r0<=b.r0 && a.r1>=b.r1 && a.c0<=b.c0 && a.c1>=b.c1; }
static SheetRange object_range(const SheetObject *o) { return (SheetRange){o->row,o->row+o->rows-1,o->col,o->col+o->columns-1}; }
static bool range_cell(SheetRange r,const SheetCell *c) { return c->row>=r.r0 && c->row<=r.r1 && c->col>=r.c0 && c->col<=r.c1; }
bool worksheet_range_fits(const Worksheet *s,SheetRange r,int64_t dr,int dc,uint64_t rows,size_t columns,char **error) {
    if (r.r0>r.r1 || r.c0>r.c1 || r.r1>=DX_SHEET_ROWS || r.c1>=16384 || dr < -(int64_t)DX_SHEET_ROWS || dr>(int64_t)DX_SHEET_ROWS || dc < -16384 || dc>16384)
        return range_fail(error,"Move stays inside the worksheet.");
    if ((dr<0 && r.r0<(uint64_t)-dr) || (dc<0 && r.c0<(size_t)-dc) || (int64_t)r.r1+dr>=(int64_t)DX_SHEET_ROWS || (int64_t)r.c1+dc>=16384)
        return range_fail(error,"Move stays inside the worksheet.");
    SheetRange to={(uint64_t)((int64_t)r.r0+dr),(uint64_t)((int64_t)r.r1+dr),(size_t)((int)r.c0+dc),(size_t)((int)r.c1+dc)};
    if (columns && rows && ((r.r0<rows && r.c0<columns) || (to.r0<rows && to.c0<columns)))
        return range_fail(error,"Source data is read-only. Move a range beside the source table.");
    for (size_t i=0;i<s->objects.count;i++) {
        SheetRange object=object_range(&s->objects.items[i]);
        if (range_overlap(r,object)) { if (!range_contains(r,object)) return range_fail(error,"Select the entire table, pivot or chart before moving it."); }
        else if (range_overlap(to,object)) return range_fail(error,"Destination overlaps another table, pivot or chart. Choose an empty range.");
    }
    if (dr || dc) for (size_t i=0;i<s->cells.count;i++) if (range_cell(to,&s->cells.cells[i]) && !range_cell(r,&s->cells.cells[i]))
        return range_fail(error,"Destination contains data. Choose an empty range.");
    return true;
}
bool worksheet_move_range(Worksheet *s,SheetRange r,int64_t dr,int dc,uint64_t rows,size_t columns,char **error) {
    if (!worksheet_range_fits(s,r,dr,dc,rows,columns,error)) return false;
    if (!dr && !dc) return true;
    uint32_t mask=0;
    for (size_t i=0;i<s->objects.count;i++) if (range_contains(r,object_range(&s->objects.items[i]))) mask|=UINT32_C(1)<<i;
    uint64_t revision=s->cells.revision;
    FormatSnapshot formats={0}; if (!format_plan_move(&s->formats,(FormatRange){r.r0,r.r1,r.c0,r.c1},dr,dc,&formats,error)) return false;
    if (!cells_move(&s->cells,r.r0,r.r1,r.c0,r.c1,dr,dc,error)) { format_snapshot_free(&formats); return false; }
    objects_translate_mask(&s->objects,mask,dr,dc);
    bool cells_changed=revision!=s->cells.revision,format_changed=format_commit(&s->formats,&formats);
    if (cells_changed || mask || format_changed) worksheet_record(s,range_edit(cells_changed,mask!=0,format_changed));
    return true;
}
bool worksheet_column(Worksheet *s,size_t col,bool insert,size_t source_columns,char **error) {
    if (col>=16384) return range_fail(error,"Column is outside the worksheet.");
    if (col<source_columns) return range_fail(error,"Imported columns are read-only. Insert or delete worksheet columns beside the source.");
    uint32_t mask=0;
    for (size_t i=0;i<s->objects.count;i++) {
        const SheetObject *o=&s->objects.items[i];
        if ((insert && col>o->col && col<o->col+o->columns) || (!insert && col>=o->col && col<o->col+o->columns))
            return range_fail(error,"This column intersects a pivot or chart. Choose a column outside its boundary.");
        if (insert && o->col>=col && o->col+o->columns>=16384)
            return range_fail(error,"Insert would move a pivot or chart beyond the last column.");
        if (insert ? o->col>=col : o->col>col) mask|=UINT32_C(1)<<i;
    }
    FormatSnapshot formats={0}; if (!format_plan_axis(&s->formats,false,col,1,insert,&formats,error)) return false;
    if (!cells_column(&s->cells,col,insert,error)) { format_snapshot_free(&formats); return false; }
    objects_translate_mask(&s->objects,mask,0,insert ? 1 : -1);
    bool format_changed=format_commit(&s->formats,&formats);
    worksheet_record(s,range_edit(true,mask!=0,format_changed)); s->undo_columns[s->undo_actions_count-1]=(ColumnEdit){.active=true,.insert=insert,.column=col,.width=96}; return true;
}
bool worksheet_row(Worksheet *s,uint64_t row,bool insert,uint64_t source_rows,char **error) {
    return worksheet_rows(s,row,1,insert,source_rows,error);
}
bool worksheet_rows(Worksheet *s,uint64_t row,uint64_t count,bool insert,uint64_t source_rows,char **error) {
    if (row>=DX_SHEET_ROWS || !count || count>DX_SHEET_ROWS-row) return range_fail(error,"Rows are outside the worksheet.");
    if (row<source_rows) return range_fail(error,"Imported rows are read-only. Insert or delete worksheet rows below the source.");
    uint32_t mask=0;
    for (size_t i=0;i<s->objects.count;i++) {
        const SheetObject *o=&s->objects.items[i];
        if ((insert && row>o->row && row<o->row+o->rows) || (!insert && row<o->row+o->rows && o->row<row+count))
            return range_fail(error,"These rows intersect a pivot or chart. Choose rows outside its boundary.");
        if (insert && o->row>=row && o->row+o->rows>DX_SHEET_ROWS-count)
            return range_fail(error,"Insert would move a pivot or chart beyond the last row.");
        if (insert ? o->row>=row : o->row>=row+count) mask|=UINT32_C(1)<<i;
    }
    uint64_t revision=s->cells.revision;
    FormatSnapshot formats={0}; if (!format_plan_axis(&s->formats,true,row,count,insert,&formats,error)) return false;
    if (!cells_rows(&s->cells,row,count,insert,error)) { format_snapshot_free(&formats); return false; }
    objects_translate_mask(&s->objects,mask,insert ? (int64_t)count : -(int64_t)count,0);
    bool changed=revision!=s->cells.revision,format_changed=format_commit(&s->formats,&formats);
    if (changed || mask || format_changed) worksheet_record(s,range_edit(changed,mask!=0,format_changed));
    return true;
}
void workbook_free(Workbook *b) {
    for (size_t i=0;i<b->count;i++) {
        Worksheet *s=&b->sheets[i]; free(s->name); analysis_free(&s->analysis); view_free(&s->view); cells_free(&s->cells); objects_free(&s->objects); format_free(&s->formats); source_versions_free(&s->source_versions);
        for (size_t j=0;j<s->undo_count;j++) analysis_free(&s->undo[j]);
        for (size_t j=0;j<s->redo_count;j++) analysis_free(&s->redo[j]);
    }
    free(b->sheets); free(b->path); workbook_init(b);
}
bool workbook_add(Workbook *b,const char *name,const Analysis *a,char **error) {
    if (b->count>=DX_MAX_SHEETS) { error_set(error,"A workspace supports up to 64 worksheets."); return false; }
    char *owned_name=dx_strdup(name); Analysis owned_analysis=analysis_copy(a);
    b->sheets=dx_realloc(b->sheets,(b->count+1)*sizeof(*b->sheets));
    Worksheet *s=&b->sheets[b->count]; memset(s,0,sizeof(*s)); s->name=owned_name; s->analysis=owned_analysis;
    b->active=b->count++; b->dirty=true; return true;
}
static void view_copy(SheetView *to,const SheetView *from) {
    *to=*from;
    to->columns=dx_alloc(from->column_count*sizeof(char *)); to->widths=dx_alloc(from->column_count*sizeof(float));
    for (size_t c=0;c<from->column_count;c++) { to->columns[c]=dx_strdup(from->columns[c]); to->widths[c]=from->widths[c]; }
}
static bool same_name(const char *a,const char *b) {
    while (*a && *b) {
        unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++;
        if (x>='A' && x<='Z') x+=32; if (y>='A' && y<='Z') y+=32;
        if (x!=y) return false;
    }
    return *a==*b;
}
bool workbook_rename(Workbook *b,size_t index,const char *name,char **error) {
    if (index>=b->count || !name) return range_fail(error,"Choose a worksheet to rename.");
    while (*name && isspace((unsigned char)*name)) name++;
    size_t length=strlen(name); while (length && isspace((unsigned char)name[length-1])) length--;
    if (!length || length>4096) return range_fail(error,"Worksheet names must contain 1–4096 bytes of text.");
    for (size_t i=0;i<length;i++) if ((unsigned char)name[i]<32 || name[i]==127) return range_fail(error,"Worksheet names cannot contain control characters.");
    char *owned=dx_printf("%.*s",(int)length,name);
    for (size_t i=0;i<b->count;i++) if (i!=index && same_name(owned,b->sheets[i].name)) { free(owned); return range_fail(error,"Another worksheet already has that name."); }
    if (strcmp(owned,b->sheets[index].name)) { free(b->sheets[index].name); b->sheets[index].name=owned; b->dirty=true; }
    else free(owned);
    return true;
}
bool workbook_reorder(Workbook *b,size_t from,size_t to,char **error) {
    if (from>=b->count || to>=b->count) return range_fail(error,"Choose an existing worksheet position.");
    if (from==to) return true;
    Worksheet moved=b->sheets[from];
    if (from<to) memmove(b->sheets+from,b->sheets+from+1,(to-from)*sizeof(Worksheet));
    else memmove(b->sheets+to+1,b->sheets+to,(from-to)*sizeof(Worksheet));
    b->sheets[to]=moved;
    if (b->active==from) b->active=to;
    else if (from<b->active && b->active<=to) b->active--;
    else if (to<=b->active && b->active<from) b->active++;
    b->dirty=true; return true;
}
bool workbook_duplicate(Workbook *b,size_t index,char **error) {
    if (index>=b->count) return range_fail(error,"Choose a worksheet to duplicate.");
    if (b->count>=DX_MAX_SHEETS) return range_fail(error,"A workspace supports up to 64 worksheets.");
    SourceVersions versions={0};
    if (!source_versions_valid(&b->sheets[index].source_versions,b->sheets[index].objects.next_id,error) || !source_versions_copy(&versions,&b->sheets[index].source_versions,error)) return false;
    char *name=NULL;
    for (unsigned suffix=2;;suffix++) {
        const char *base=b->sheets[index].name; size_t bytes=strlen(base);
        if (bytes>4080) { bytes=4080; while (bytes && ((unsigned char)base[bytes]&0xc0)==0x80) bytes--; }
        free(name); name=dx_printf("%.*s (%u)",(int)bytes,base,suffix); bool taken=false;
        for (size_t i=0;i<b->count;i++) if (same_name(name,b->sheets[i].name)) taken=true;
        if (!taken) break;
    }
    /* add may realloc the sheet array: never retain a pointer into it. */
    if (!workbook_add(b,name,&b->sheets[index].analysis,error)) { free(name); source_versions_free(&versions); return false; } free(name);
    Worksheet *to=&b->sheets[b->active],*from=&b->sheets[index];
    view_copy(&to->view,&from->view); cells_copy(&to->cells,&from->cells); objects_copy(&to->objects,&from->objects);
    format_copy(&to->formats,&from->formats);
    to->source_versions=versions;
    return workbook_reorder(b,b->active,index+1,error);
}
size_t workbook_visible_count(const Workbook *b) {
    size_t count=0; for (size_t i=0;i<b->count;i++) count+=!b->sheets[i].hidden; return count;
}
size_t workbook_visible_sheet(const Workbook *b,size_t position) {
    for (size_t i=0;i<b->count;i++) if (!b->sheets[i].hidden && !position--) return i;
    return b->count;
}
bool workbook_hide(Workbook *b,size_t index,bool hidden,char **error) {
    if (index>=b->count) return range_fail(error,"Choose a worksheet to show or hide.");
    if (b->sheets[index].hidden==hidden) return true;
    if (hidden && workbook_visible_count(b)<=1) return range_fail(error,"Keep at least one worksheet visible.");
    b->sheets[index].hidden=hidden;
    if (hidden && index==b->active) {
        for (size_t n=1;n<b->count;n++) { size_t next=(index+n)%b->count; if (!b->sheets[next].hidden) { b->active=next; break; } }
    }
    b->dirty=true; return true;
}
Workbook *workbook_snapshot(const Workbook *b) {
    if (!b || b->count>DX_MAX_SHEETS || (b->count && !b->sheets)) return NULL;
    Workbook *out=dx_alloc(sizeof(*out)); workbook_init(out); out->user_scale=b->user_scale; out->reduced_motion=b->reduced_motion;
    out->path=b->path ? dx_strdup(b->path) : NULL;
    for (size_t i=0;i<b->count;i++) {
        char *error=NULL; SourceVersions versions={0};
        if (!source_versions_valid(&b->sheets[i].source_versions,b->sheets[i].objects.next_id,&error) || !source_versions_copy(&versions,&b->sheets[i].source_versions,&error)) { free(error); workbook_free(out); free(out); return NULL; }
        if (!workbook_add(out,b->sheets[i].name,&b->sheets[i].analysis,&error)) { free(error); source_versions_free(&versions); workbook_free(out); free(out); return NULL; } free(error);
        out->sheets[i].hidden=b->sheets[i].hidden;
        view_copy(&out->sheets[i].view,&b->sheets[i].view);
        cells_copy(&out->sheets[i].cells,&b->sheets[i].cells);
        objects_copy(&out->sheets[i].objects,&b->sheets[i].objects);
        format_copy(&out->sheets[i].formats,&b->sheets[i].formats);
        out->sheets[i].source_versions=versions;
    }
    out->active=b->active; out->dirty=b->dirty; return out;
}
static bool absolute(const char *path) {
    return path[0]=='/' || path[0]=='\\' || (path[0] && path[1]==':');
}
static char *directory(const char *path) {
    const char *base=dx_basename(path); return base==path ? dx_strdup(".") : dx_printf("%.*s",(int)(base-path),path);
}
char *workbook_target_path(const char *path) {
    char *existing=dx_absolute_path(path); if (existing) return existing;
    char *dir=directory(path),*resolved=dx_absolute_path(dir); free(dir);
    if (!resolved) return NULL;
    char *out=dx_printf("%s/%s",resolved,dx_basename(path)); free(resolved); return out;
}
char *workbook_source_path(const char *workspace,const char *ref) {
    if (absolute(ref)) return dx_strdup(ref);
    char *dir=directory(workspace); char *out=dx_printf("%s/%s",dir,ref); free(dir);
    char *resolved=dx_absolute_path(out); if (resolved) { free(out); return resolved; } return out;
}
char *workbook_relative_path(const char *workspace,const char *source) {
    if (!source || !*source) return dx_strdup("");
    char *dir=directory(workspace); size_t common=0;
    for (size_t i=0;dir[i] && source[i] && dir[i]==source[i];i++) if (dir[i]=='/' || dir[i]=='\\') common=i+1;
    if (!common) { free(dir); return dx_strdup(source); }
    char *out=dx_strdup("");
    for (size_t i=common;dir[i];i++) if (dir[i]=='/' || dir[i]=='\\') dx_append(&out,"../");
    size_t n=strlen(dir); if (n && dir[n-1]!='/' && dir[n-1]!='\\') dx_append(&out,"../");
    dx_append(&out,"%s",source+common); free(dir); return out;
}
/* The shared model and path helpers above need no engine. Native persistence
   stays intact; the browser renderer supplies an explicit unavailable adapter. */
#ifndef DUXCEL_BROWSER_HOST
static bool exec(duckdb_connection c,const char *sql,char **error) {
    duckdb_result r; bool ok=duckdb_query(c,sql,&r)==DuckDBSuccess;
    if (!ok) error_set(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return ok;
}
static bool prepare(duckdb_connection c,const char *sql,duckdb_prepared_statement *stmt,char **error) {
    if (duckdb_prepare(c,sql,stmt)==DuckDBSuccess) return true;
    error_set(error,duckdb_prepare_error(*stmt)); duckdb_destroy_prepare(stmt); return false;
}
static bool run(duckdb_prepared_statement stmt,char **error) {
    duckdb_result r; bool ok=duckdb_execute_prepared(stmt,&r)==DuckDBSuccess;
    if (!ok) error_set(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return ok;
}
static void bind_text(duckdb_prepared_statement p,idx_t i,const char *s) {
    if (s) duckdb_bind_varchar(p,i,s); else duckdb_bind_null(p,i);
}
static bool metadata_types(duckdb_connection c,const char *sql,const duckdb_type *types,size_t count,char **error) {
    duckdb_prepared_statement statement=NULL;
    if (!prepare(c,sql,&statement,error)) return false;
    bool ok=duckdb_prepared_statement_column_count(statement)==count;
    for (size_t i=0;ok && i<count;i++) ok=duckdb_prepared_statement_column_type(statement,i)==types[i];
    duckdb_destroy_prepare(&statement);
    if (!ok) error_set(error,"Workbook metadata has incompatible column types. Save to a new .dxls file.");
    return ok;
}
/* v13 first preserves owner allocation lifetimes. Its format package is
   deliberately spatial-only: opaque scopes and guard rows remain unsupported. */
static bool package_shape(duckdb_connection c,const char *table,const char *columns,const duckdb_type *types,size_t count,char **error) {
    duckdb_result r={0};
    char *sql=dx_printf("SELECT (SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='%s' AND table_type='BASE TABLE'),(SELECT count(*) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='%s')",table,table);
    bool queried=duckdb_query(c,sql,&r)==DuckDBSuccess; free(sql);
    if (!queried) { error_set(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return false; }
    bool valid=duckdb_value_uint64(&r,0,0)==1 && duckdb_value_uint64(&r,1,0)==count; duckdb_destroy_result(&r);
    if (!valid) { error_set(error,"Incomplete or incompatible workspace owner/format metadata."); return false; }
    sql=dx_printf("SELECT %s FROM _duxcel_workspace.%s LIMIT 0",columns,table); valid=metadata_types(c,sql,types,count,error); free(sql); return valid;
}
static bool owner_package_rows_supported(duckdb_connection c,char **error) {
    duckdb_result r={0};
    const char *sql="SELECT EXISTS(SELECT 1 FROM _duxcel_workspace.object_counters WHERE sheet_id IS NULL OR high_water IS NULL) OR "
        "EXISTS(SELECT sheet_id FROM _duxcel_workspace.object_counters GROUP BY sheet_id HAVING count(*)<>1) OR "
        "EXISTS((SELECT sheet_id FROM _duxcel_workspace.object_counters EXCEPT SELECT id FROM _duxcel_workspace.sheets) UNION ALL (SELECT id FROM _duxcel_workspace.sheets EXCEPT SELECT sheet_id FROM _duxcel_workspace.object_counters)) OR "
        "EXISTS(SELECT 1 FROM _duxcel_workspace.objects o LEFT JOIN _duxcel_workspace.object_counters c ON o.sheet_id=c.sheet_id WHERE c.high_water IS NULL OR o.id IS NULL OR o.id>c.high_water) OR "
        "EXISTS((SELECT sheet_id,position FROM _duxcel_workspace.formats EXCEPT ALL SELECT sheet_id,position FROM _duxcel_workspace.format_selectors) UNION ALL (SELECT sheet_id,position FROM _duxcel_workspace.format_selectors EXCEPT ALL SELECT sheet_id,position FROM _duxcel_workspace.formats)) OR "
        "EXISTS(SELECT 1 FROM _duxcel_workspace.formats f LEFT JOIN _duxcel_workspace.sheets s ON f.sheet_id=s.id WHERE s.id IS NULL OR f.position IS NULL OR f.position<0) OR "
        "EXISTS(SELECT sheet_id FROM _duxcel_workspace.formats GROUP BY sheet_id HAVING count(*)>512 OR min(position)<>0 OR max(position)<>count(*)-1 OR count(DISTINCT position)<>count(*))";
    if (duckdb_query(c,sql,&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return false; }
    bool invalid=duckdb_value_boolean(&r,0,0); duckdb_destroy_result(&r);
    if (invalid) error_set(error,"Invalid workspace v13 owner counters or spatial selector metadata."); return !invalid;
}
static bool source_package_shape(duckdb_connection c,char **error) {
    static const struct { const char *table,*columns,*types; } tables[]={
        {"meta","version,active_sheet,user_scale","I U D"},
        {"sheets","id,name,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending,active_row,active_col,anchor_row,anchor_col,end_row,end_col,top_row,horizontal,fraction,selection,canvas_zoom","I T I T T T T T I T T I T B U U U U U U U D D I D"},
        {"columns","sheet_id,column_id,name,width","I I T D"},
        {"cells","sheet_id,row_id,column_id,text","I U I T"},
        {"filters","sheet_id,position,name,op,value","I I T I T"},
        {"sorts","sheet_id,position,name,descending","I I T B"},
        {"queries","sheet_id,object_id,query","I U T"},
        {"source_options","sheet_id,object_id,unique_rows","I U B"},
        {"table_sources","sheet_id,object_id,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending","I U I T T T T T I T T I T B"},
        {"table_filters","sheet_id,object_id,position,name,op,value","I U I T I T"},
        {"table_sorts","sheet_id,object_id,position,name,descending","I U I T B"},
        {"objects","sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure","I U T U I U I I I U I"},
        {"pivot_fields","sheet_id,object_id,role,position,name,aggregate,filter_op,value,grain","I U I I T I I T I"},
        {"marks","sheet_id,object_id,role,position,name,is_measure,aggregate,grain","I U I I T B I I"},
        {"preferences","name,value","T B"},
        {"formats","sheet_id,position,r0,r1,c0,c1,mask,flags,alignment,color,font_size,numeric_format,decimals","I I U U I I I I I I I I I"},
        {"object_counters","sheet_id,high_water","I U"},
        {"format_scopes","sheet_id,scope_id,owner,encoding,definition","I I U I X"},
        {"format_selectors","sheet_id,position,kind","I I I"},
        {"format_guards","sheet_id,position,guard_position,scope_id","I I I I"},
        {"source_version_counters","sheet_id,high_water","I U"},
        {"source_versions","sheet_id,version,owner,source_kind,source_reference,encoding,definition","I U U I T I X"},
        {"source_bindings","sheet_id,owner,version","I U U"},
        {"format_records","sheet_id,position,member_position,record_id","I I I U"},
        {"format_fields","sheet_id,position,field_position,source_ordinal","I I I I"}
    };
    duckdb_result r={0};
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace'",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return false; }
    bool exact=duckdb_value_uint64(&r,0,0)==sizeof(tables)/sizeof(*tables); duckdb_destroy_result(&r);
    if (!exact) return range_fail(error,"Workspace v14 requires exactly its 25 supported metadata tables.");
    for (size_t i=0;i<sizeof(tables)/sizeof(*tables);i++) {
        duckdb_type types[25]; size_t count=0;
        for (const char *p=tables[i].types;*p;p++) if (*p!=' ') types[count++]=*p=='I' ? DUCKDB_TYPE_INTEGER : *p=='U' ? DUCKDB_TYPE_UBIGINT : *p=='D' ? DUCKDB_TYPE_DOUBLE : *p=='B' ? DUCKDB_TYPE_BOOLEAN : *p=='X' ? DUCKDB_TYPE_BLOB : DUCKDB_TYPE_VARCHAR;
        if (!package_shape(c,tables[i].table,tables[i].columns,types,count,error)) return false;
    }
    return owner_package_rows_supported(c,error);
}
static bool format_metadata_supported(duckdb_connection c,int workspace_version,char **error) {
    duckdb_result r={0}; bool ok=false;
    if (workspace_version==14) return source_package_shape(c,error);
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name IN ('source_version_counters','source_versions','source_bindings','format_records','format_fields')",&r)!=DuckDBSuccess) goto sql_error;
    bool future=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (future) { error_set(error,"Source-version metadata requires workspace v14; an older version cannot load or overwrite it."); return false; }
    if (workspace_version<=12) {
        if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name IN ('object_counters','format_scopes','format_selectors','format_guards')",&r)!=DuckDBSuccess) goto sql_error;
        bool extended=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
        if (extended) goto unsupported;
    } else {
        const duckdb_type meta_types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_DOUBLE};
        if (!package_shape(c,"meta","version,active_sheet,user_scale",meta_types,3,error)) return false;
        const duckdb_type object_types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER};
        if (!package_shape(c,"objects","sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure",object_types,11,error)) return false;
        const duckdb_type counter_types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT},scope_types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_BLOB};
        const duckdb_type selector_types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER},guard_types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER};
        if (!package_shape(c,"object_counters","sheet_id,high_water",counter_types,2,error) || !package_shape(c,"format_scopes","sheet_id,scope_id,owner,encoding,definition",scope_types,5,error) ||
            !package_shape(c,"format_selectors","sheet_id,position,kind",selector_types,3,error) || !package_shape(c,"format_guards","sheet_id,position,guard_position,scope_id",guard_types,4,error)) return false;
        if (duckdb_query(c,"SELECT EXISTS(SELECT 1 FROM _duxcel_workspace.format_scopes) OR EXISTS(SELECT 1 FROM _duxcel_workspace.format_guards) OR EXISTS(SELECT 1 FROM _duxcel_workspace.format_selectors WHERE kind IS NULL OR kind<>0)",&r)!=DuckDBSuccess) goto sql_error;
        bool scoped=duckdb_value_boolean(&r,0,0); duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
        if (scoped) { error_set(error,"Scoped cell formatting payloads are not supported by this owner-only workspace v13 codec."); return false; }
    }
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='formats'",&r)!=DuckDBSuccess) goto sql_error;
    uint64_t columns=duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!columns && workspace_version<=12) return true;
    if (columns!=13) goto unsupported;
    const duckdb_type types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER};
    const char *names="sheet_id,position,r0,r1,c0,c1,mask,flags,alignment,color,font_size,numeric_format,decimals";
    ok=workspace_version<=12 ? metadata_types(c,"SELECT sheet_id,position,r0,r1,c0,c1,mask,flags,alignment,color,font_size,numeric_format,decimals FROM _duxcel_workspace.formats LIMIT 0",types,13,error) : package_shape(c,"formats",names,types,13,error) && owner_package_rows_supported(c,error); goto done;
unsupported: error_set(error,workspace_version<=12 ? "Scoped or extended cell formatting metadata requires a newer workspace format; this workbook cannot be loaded or overwritten by workspace v12." : "Extended or incompatible formatting metadata is unsupported by the owner-only workspace v13 codec."); goto done;
sql_error: error_set(error,duckdb_result_error(&r));
done: duckdb_destroy_result(&r); return ok;
}
static bool save_table_sources(duckdb_connection c,const Workbook *b,const char *target,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.table_sources(sheet_id INTEGER,object_id UBIGINT,kind INTEGER,path VARCHAR,source_schema VARCHAR,source_table VARCHAR,filter_column VARCHAR,filter_value VARCHAR,filter_op INTEGER,group_column VARCHAR,measure_column VARCHAR,aggregate INTEGER,sort_column VARCHAR,descending BOOLEAN,PRIMARY KEY(sheet_id,object_id));"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.table_filters(sheet_id INTEGER,object_id UBIGINT,position INTEGER,name VARCHAR,op INTEGER,value VARCHAR,PRIMARY KEY(sheet_id,object_id,position));"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.table_sorts(sheet_id INTEGER,object_id UBIGINT,position INTEGER,name VARCHAR,descending BOOLEAN,PRIMARY KEY(sheet_id,object_id,position));"
        "DELETE FROM _duxcel_workspace.table_sources; DELETE FROM _duxcel_workspace.table_filters; DELETE FROM _duxcel_workspace.table_sorts",error)) return false;
    const duckdb_type source_types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_BOOLEAN};
    const duckdb_type filter_types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_VARCHAR};
    const duckdb_type sort_types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_BOOLEAN};
    if (!metadata_types(c,"SELECT sheet_id,object_id,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending FROM _duxcel_workspace.table_sources LIMIT 0",source_types,14,error) ||
        !metadata_types(c,"SELECT sheet_id,object_id,position,name,op,value FROM _duxcel_workspace.table_filters LIMIT 0",filter_types,6,error) ||
        !metadata_types(c,"SELECT sheet_id,object_id,position,name,descending FROM _duxcel_workspace.table_sorts LIMIT 0",sort_types,5,error)) return false;
    duckdb_prepared_statement source=NULL,filter=NULL,sort=NULL; bool ok=false;
    if (!prepare(c,"INSERT INTO _duxcel_workspace.table_sources(sheet_id,object_id,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)",&source,error) ||
        !prepare(c,"INSERT INTO _duxcel_workspace.table_filters(sheet_id,object_id,position,name,op,value) VALUES (?,?,?,?,?,?)",&filter,error) ||
        !prepare(c,"INSERT INTO _duxcel_workspace.table_sorts(sheet_id,object_id,position,name,descending) VALUES (?,?,?,?,?)",&sort,error)) goto done;
    for (size_t s=0;s<b->count;s++) for (size_t i=0;i<b->sheets[s].objects.count;i++) {
        const SheetObject *o=&b->sheets[s].objects.items[i]; if (o->kind!=OBJECT_TABLE) continue;
        const Analysis *a=&o->source; char *ref=workbook_relative_path(target,a->path);
        duckdb_bind_uint64(source,1,s); duckdb_bind_uint64(source,2,o->id); duckdb_bind_int32(source,3,a->kind); bind_text(source,4,ref); free(ref);
        bind_text(source,5,a->schema); bind_text(source,6,a->table); bind_text(source,7,a->filter_column); bind_text(source,8,a->filter_value);
        duckdb_bind_int32(source,9,a->filter_op); bind_text(source,10,a->group_column); bind_text(source,11,a->measure_column); duckdb_bind_int32(source,12,a->aggregate);
        bind_text(source,13,a->sort_column); duckdb_bind_boolean(source,14,a->descending);
        if (!run(source,error)) goto done;
        for (size_t j=0;j<a->filter_count;j++) {
            const AnalysisFilter *f=&a->filters[j]; duckdb_bind_uint64(filter,1,s); duckdb_bind_uint64(filter,2,o->id); duckdb_bind_uint64(filter,3,j);
            bind_text(filter,4,f->column); duckdb_bind_int32(filter,5,f->op); bind_text(filter,6,f->value); if (!run(filter,error)) goto done;
        }
        for (size_t j=0;j<a->sort_count;j++) {
            duckdb_bind_uint64(sort,1,s); duckdb_bind_uint64(sort,2,o->id); duckdb_bind_uint64(sort,3,j);
            bind_text(sort,4,a->sorts[j].column); duckdb_bind_boolean(sort,5,a->sorts[j].descending); if (!run(sort,error)) goto done;
        }
    }
    ok=true;
done:
    if (source) duckdb_destroy_prepare(&source); if (filter) duckdb_destroy_prepare(&filter); if (sort) duckdb_destroy_prepare(&sort); return ok;
}
static char *text_value(duckdb_result *r,idx_t c,idx_t row);
static bool save_marks(duckdb_connection c,const Workbook *b,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.marks(sheet_id INTEGER,object_id UBIGINT,role INTEGER,position INTEGER,name VARCHAR,is_measure BOOLEAN,aggregate INTEGER,grain INTEGER,PRIMARY KEY(sheet_id,object_id,role,position)); DELETE FROM _duxcel_workspace.marks",error)) return false;
    const duckdb_type types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_BOOLEAN,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER};
    if (!metadata_types(c,"SELECT sheet_id,object_id,role,position,name,is_measure,aggregate,grain FROM _duxcel_workspace.marks LIMIT 0",types,8,error)) return false;
    duckdb_prepared_statement statement=NULL; bool ok=false;
    if (!prepare(c,"INSERT INTO _duxcel_workspace.marks VALUES (?,?,?,?,?,?,?,?)",&statement,error)) goto done;
    for (size_t si=0;si<b->count;si++) for (size_t oi=0;oi<b->sheets[si].objects.count;oi++) {
        const SheetObject *object=&b->sheets[si].objects.items[oi];
        for (int role=0;role<DX_MARK_ROLES;role++) for (size_t fi=0;fi<object->marks.counts[role];fi++) {
            const MarkField *field=&object->marks.fields[role][fi];
            duckdb_bind_uint64(statement,1,si); duckdb_bind_uint64(statement,2,object->id); duckdb_bind_int32(statement,3,role); duckdb_bind_uint64(statement,4,fi);
            bind_text(statement,5,field->name); duckdb_bind_boolean(statement,6,field->measure); duckdb_bind_int32(statement,7,field->aggregate); duckdb_bind_int32(statement,8,field->grain);
            if (!run(statement,error)) goto done;
        }
    }
    ok=true;
done:
    if (statement) duckdb_destroy_prepare(&statement); return ok;
}
static bool load_marks(duckdb_connection c,Workbook *b,char **error) {
    duckdb_result result={0}; bool ok=false;
    if (duckdb_query(c,"SELECT EXISTS(SELECT 1 FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='marks'),version>=12 FROM _duxcel_workspace.meta",&result)!=DuckDBSuccess) goto sql_error;
    bool present=duckdb_value_boolean(&result,0,0),required=duckdb_value_boolean(&result,1,0); duckdb_destroy_result(&result);
    if (!present) return !required || (error_set(error,"Missing Marks assignments in this workbook."),false);
    const duckdb_type types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_BOOLEAN,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER};
    if (!metadata_types(c,"SELECT sheet_id,object_id,role,position,name,is_measure,aggregate,grain FROM _duxcel_workspace.marks LIMIT 0",types,8,error)) goto done;
    if (duckdb_query(c,"SELECT sheet_id FROM _duxcel_workspace.marks GROUP BY sheet_id HAVING sum(octet_length(encode(coalesce(name,''))))>8388608 LIMIT 1",&result)!=DuckDBSuccess) goto sql_error;
    bool oversized=duckdb_row_count(&result)>0; duckdb_destroy_result(&result); if (oversized) goto invalid;
    if (duckdb_query(c,"SELECT sheet_id,object_id,role,position,name,is_measure,aggregate,grain FROM _duxcel_workspace.marks ORDER BY sheet_id,object_id,role,position LIMIT 196609",&result)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&result)>DX_MAX_SHEETS*DX_MAX_OBJECTS*DX_MARK_ROLES*DX_MARK_FIELDS) goto invalid;
    for (idx_t i=0;i<duckdb_row_count(&result);i++) {
        for (idx_t column=0;column<8;column++) if (duckdb_value_is_null(&result,column,i)) goto invalid;
        int64_t si=duckdb_value_int64(&result,0,i),role=duckdb_value_int64(&result,2,i),position=duckdb_value_int64(&result,3,i),aggregate=duckdb_value_int64(&result,6,i),grain=duckdb_value_int64(&result,7,i);
        uint64_t id=duckdb_value_uint64(&result,1,i);
        if (si<0 || (uint64_t)si>=b->count || role<0 || role>=DX_MARK_ROLES || position<0 || position>=DX_MARK_FIELDS || aggregate<0 || aggregate>AGG_MAX || grain<0 || grain>PIVOT_DATE_MONTH) goto invalid;
        ObjectStore *store=&b->sheets[si].objects; SheetObject *object=NULL;
        for (size_t j=0;j<store->count;j++) if (store->items[j].id==id) object=&store->items[j];
        if (!object || object->kind==OBJECT_TABLE || object->marks.counts[role]!=(size_t)position) goto invalid;
        MarkField *field=&object->marks.fields[role][object->marks.counts[role]++];
        *field=(MarkField){.name=text_value(&result,4,i),.measure=duckdb_value_boolean(&result,5,i),.aggregate=(Aggregate)aggregate,.grain=(PivotDateGrain)grain};
        if (!marks_valid(&object->marks,error)) goto done;
    }
    ok=true; goto done;
sql_error:
    error_set(error,duckdb_result_error(&result)); goto done;
invalid:
    error_set(error,"Invalid Marks assignment metadata.");
done:
    duckdb_destroy_result(&result); return ok;
}
static bool save_objects(duckdb_connection c,const Workbook *b,const char *target,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.objects(sheet_id INTEGER,id UBIGINT,name VARCHAR,row_id UBIGINT,column_id INTEGER,height UBIGINT,width INTEGER,PRIMARY KEY(sheet_id,id));"
        "ALTER TABLE _duxcel_workspace.objects ADD COLUMN IF NOT EXISTS kind INTEGER DEFAULT 0;"
        "ALTER TABLE _duxcel_workspace.objects ADD COLUMN IF NOT EXISTS mark INTEGER DEFAULT 0;"
        "ALTER TABLE _duxcel_workspace.objects ADD COLUMN IF NOT EXISTS source_id UBIGINT DEFAULT 0;"
        "ALTER TABLE _duxcel_workspace.objects ADD COLUMN IF NOT EXISTS chart_measure INTEGER DEFAULT 0;"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.pivot_fields(sheet_id INTEGER,object_id UBIGINT,role INTEGER,position INTEGER,name VARCHAR,aggregate INTEGER,filter_op INTEGER,value VARCHAR,PRIMARY KEY(sheet_id,object_id,role,position));"
        "ALTER TABLE _duxcel_workspace.pivot_fields ADD COLUMN IF NOT EXISTS grain INTEGER DEFAULT 0;"
        "DELETE FROM _duxcel_workspace.objects; DELETE FROM _duxcel_workspace.pivot_fields;",error)) return false;
    const duckdb_type types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER};
    if (!metadata_types(c,"SELECT sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure FROM _duxcel_workspace.objects LIMIT 0",types,11,error)) return false;
    duckdb_prepared_statement object=NULL,field=NULL; bool ok=false;
    if (!prepare(c,"INSERT INTO _duxcel_workspace.objects(sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure) VALUES (?,?,?,?,?,?,?,?,?,?,?)",&object,error) || !prepare(c,"INSERT INTO _duxcel_workspace.pivot_fields(sheet_id,object_id,role,position,name,aggregate,filter_op,value,grain) VALUES (?,?,?,?,?,?,?,?,?)",&field,error)) goto done;
    for (size_t s=0;s<b->count;s++) for (size_t i=0;i<b->sheets[s].objects.count;i++) {
        const SheetObject *o=&b->sheets[s].objects.items[i];
        duckdb_bind_uint64(object,1,s); duckdb_bind_uint64(object,2,o->id); bind_text(object,3,o->name);
        duckdb_bind_uint64(object,4,o->row); duckdb_bind_uint64(object,5,o->col); duckdb_bind_uint64(object,6,o->rows); duckdb_bind_uint64(object,7,o->columns);
        duckdb_bind_int32(object,8,o->kind);
        duckdb_bind_int32(object,9,o->mark);
        duckdb_bind_uint64(object,10,o->source_id);
        duckdb_bind_int32(object,11,o->chart_measure);
        if (!run(object,error)) goto done;
        for (int r=0;r<4;r++) for (size_t j=0;j<o->pivot.counts[r];j++) {
            const PivotField *f=&o->pivot.fields[r][j];
            duckdb_bind_uint64(field,1,s); duckdb_bind_uint64(field,2,o->id); duckdb_bind_int32(field,3,r); duckdb_bind_uint64(field,4,j); bind_text(field,5,f->name);
            duckdb_bind_int32(field,6,f->aggregate); duckdb_bind_int32(field,7,f->filter); bind_text(field,8,f->value);
            duckdb_bind_int32(field,9,f->grain);
            if (!run(field,error)) goto done;
        }
    }
    ok=save_table_sources(c,b,target,error);
 done:
    if (object) duckdb_destroy_prepare(&object); if (field) duckdb_destroy_prepare(&field); return ok;
}
static bool save_formats(duckdb_connection c,const Workbook *b,char **error) {
    const char *columns="sheet_id,position,r0,r1,c0,c1,mask,flags,alignment,color,font_size,numeric_format,decimals";
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.formats(sheet_id INTEGER,position INTEGER,r0 UBIGINT,r1 UBIGINT,c0 INTEGER,c1 INTEGER,mask INTEGER,flags INTEGER,alignment INTEGER,color INTEGER,font_size INTEGER,numeric_format INTEGER,decimals INTEGER,PRIMARY KEY(sheet_id,position))",error)) return false;
    const duckdb_type types[]={DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_UBIGINT,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER,DUCKDB_TYPE_INTEGER};
    char *sql=dx_printf("SELECT %s FROM _duxcel_workspace.formats LIMIT 0",columns); bool valid=metadata_types(c,sql,types,13,error); free(sql);
    if (!valid || !exec(c,"DELETE FROM _duxcel_workspace.formats",error)) return false;
    duckdb_prepared_statement stmt=NULL; sql=dx_printf("INSERT INTO _duxcel_workspace.formats(%s) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",columns);
    bool ok=prepare(c,sql,&stmt,error); free(sql); if (!ok) return false;
    for (size_t s=0;ok && s<b->count;s++) for (size_t i=0;ok && i<b->sheets[s].formats.count;i++) {
        const FormatPatch *p=&b->sheets[s].formats.items[i]; CellFormat f=p->value;
        duckdb_bind_int32(stmt,1,(int32_t)s); duckdb_bind_int32(stmt,2,(int32_t)i); duckdb_bind_uint64(stmt,3,p->range.r0); duckdb_bind_uint64(stmt,4,p->range.r1);
        duckdb_bind_int32(stmt,5,(int32_t)p->range.c0); duckdb_bind_int32(stmt,6,(int32_t)p->range.c1); duckdb_bind_int32(stmt,7,(int32_t)p->mask);
        duckdb_bind_int32(stmt,8,f.flags); duckdb_bind_int32(stmt,9,f.align); duckdb_bind_int32(stmt,10,f.color); duckdb_bind_int32(stmt,11,f.size); duckdb_bind_int32(stmt,12,f.number); duckdb_bind_int32(stmt,13,f.decimals);
        ok=run(stmt,error);
    }
    duckdb_destroy_prepare(&stmt); return ok;
}
static bool save_owner_package(duckdb_connection c,const Workbook *book,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.object_counters(sheet_id INTEGER PRIMARY KEY,high_water UBIGINT);"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.format_scopes(sheet_id INTEGER,scope_id INTEGER,owner UBIGINT,encoding INTEGER,definition BLOB,PRIMARY KEY(sheet_id,scope_id));"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.format_selectors(sheet_id INTEGER,position INTEGER,kind INTEGER,PRIMARY KEY(sheet_id,position));"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.format_guards(sheet_id INTEGER,position INTEGER,guard_position INTEGER,scope_id INTEGER,PRIMARY KEY(sheet_id,position,guard_position));"
        "DELETE FROM _duxcel_workspace.object_counters; DELETE FROM _duxcel_workspace.format_selectors",error)) return false;
    duckdb_prepared_statement counter=NULL,selector=NULL; bool ok=prepare(c,"INSERT INTO _duxcel_workspace.object_counters(sheet_id,high_water) VALUES (?,?)",&counter,error) && prepare(c,"INSERT INTO _duxcel_workspace.format_selectors(sheet_id,position,kind) VALUES (?,?,0)",&selector,error);
    for (size_t i=0;ok && i<book->count;i++) {
        duckdb_bind_int32(counter,1,(int32_t)i); duckdb_bind_uint64(counter,2,book->sheets[i].objects.next_id); ok=run(counter,error);
        for (size_t f=0;ok && f<book->sheets[i].formats.count;f++) { duckdb_bind_int32(selector,1,(int32_t)i); duckdb_bind_int32(selector,2,(int32_t)f); ok=run(selector,error); }
    }
    if (counter) duckdb_destroy_prepare(&counter); if (selector) duckdb_destroy_prepare(&selector); return ok;
}
static bool save_filters(duckdb_connection c,const Workbook *b,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.filters(sheet_id INTEGER,position INTEGER,name VARCHAR,op INTEGER,value VARCHAR,PRIMARY KEY(sheet_id,position)); DELETE FROM _duxcel_workspace.filters",error)) return false;
    duckdb_prepared_statement stmt=NULL; if (!prepare(c,"INSERT INTO _duxcel_workspace.filters VALUES (?,?,?,?,?)",&stmt,error)) return false;
    bool ok=true;
    for (size_t i=0;i<b->count && ok;i++) for (size_t j=0;j<b->sheets[i].analysis.filter_count;j++) {
        const AnalysisFilter *f=&b->sheets[i].analysis.filters[j]; duckdb_bind_uint64(stmt,1,i); duckdb_bind_uint64(stmt,2,j); bind_text(stmt,3,f->column); duckdb_bind_int32(stmt,4,f->op); bind_text(stmt,5,f->value);
        if (!run(stmt,error)) { ok=false; break; }
    }
    duckdb_destroy_prepare(&stmt); return ok;
}
static bool save_sorts(duckdb_connection c,const Workbook *b,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.sorts(sheet_id INTEGER,position INTEGER,name VARCHAR,descending BOOLEAN,PRIMARY KEY(sheet_id,position)); DELETE FROM _duxcel_workspace.sorts",error)) return false;
    duckdb_prepared_statement stmt=NULL; if (!prepare(c,"INSERT INTO _duxcel_workspace.sorts VALUES (?,?,?,?)",&stmt,error)) return false;
    bool ok=true;
    for (size_t i=0;i<b->count && ok;i++) for (size_t j=0;j<b->sheets[i].analysis.sort_count;j++) {
        const AnalysisSort *sort=&b->sheets[i].analysis.sorts[j]; duckdb_bind_uint64(stmt,1,i); duckdb_bind_uint64(stmt,2,j); bind_text(stmt,3,sort->column); duckdb_bind_boolean(stmt,4,sort->descending);
        if (!run(stmt,error)) { ok=false; break; }
    }
    duckdb_destroy_prepare(&stmt); return ok;
}
static bool save_queries(duckdb_connection c,const Workbook *b,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.queries(sheet_id INTEGER,object_id UBIGINT,query VARCHAR,PRIMARY KEY(sheet_id,object_id)); DELETE FROM _duxcel_workspace.queries",error)) return false;
    duckdb_prepared_statement stmt=NULL; if (!prepare(c,"INSERT INTO _duxcel_workspace.queries VALUES (?,?,?)",&stmt,error)) return false;
    bool ok=true;
    for (size_t i=0;i<b->count && ok;i++) {
        const Worksheet *s=&b->sheets[i];
        for (size_t j=0;j<=s->objects.count && ok;j++) {
            const SheetObject *o=j ? &s->objects.items[j-1] : NULL;
            const Analysis *a=o ? &o->source : &s->analysis;
            if (!a->query) continue;
            duckdb_bind_uint64(stmt,1,i); duckdb_bind_uint64(stmt,2,o ? o->id : 0); bind_text(stmt,3,a->query); ok=run(stmt,error);
        }
    }
    duckdb_destroy_prepare(&stmt); return ok;
}
static bool save_source_options(duckdb_connection c,const Workbook *b,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.source_options(sheet_id INTEGER,object_id UBIGINT,unique_rows BOOLEAN,PRIMARY KEY(sheet_id,object_id)); DELETE FROM _duxcel_workspace.source_options",error)) return false;
    duckdb_prepared_statement stmt=NULL; if (!prepare(c,"INSERT INTO _duxcel_workspace.source_options VALUES (?,?,true)",&stmt,error)) return false;
    bool ok=true;
    for (size_t i=0;i<b->count && ok;i++) {
        const Worksheet *s=&b->sheets[i];
        for (size_t j=0;j<=s->objects.count && ok;j++) {
            const SheetObject *o=j ? &s->objects.items[j-1] : NULL;
            const Analysis *a=o ? &o->source : &s->analysis;
            if (!a->unique_rows) continue;
            duckdb_bind_uint64(stmt,1,i); duckdb_bind_uint64(stmt,2,o ? o->id : 0); ok=run(stmt,error);
        }
    }
    duckdb_destroy_prepare(&stmt); return ok;
}
static bool open_metadata(const char *path,bool read_only,duckdb_database *db,duckdb_connection *c,char **temporary,char **error) {
    duckdb_config config=NULL; char *message=NULL;
    if (duckdb_create_config(&config)!=DuckDBSuccess) { error_set(error,"Could not configure workspace database"); return false; }
    *temporary=dx_temp_directory();
    if (!*temporary) { duckdb_destroy_config(&config); error_set(error,"Cannot create writable temporary storage for this workbook."); return false; }
    if ((read_only && duckdb_set_config(config,"access_mode","READ_ONLY")!=DuckDBSuccess) || duckdb_set_config(config,"memory_limit","256MB")!=DuckDBSuccess || duckdb_set_config(config,"threads","2")!=DuckDBSuccess || duckdb_set_config(config,"temp_directory",*temporary)!=DuckDBSuccess) {
        duckdb_destroy_config(&config); error_set(error,"Could not configure workspace temporary storage"); return false;
    }
    duckdb_state state=duckdb_open_ext(path,db,config,&message); duckdb_destroy_config(&config);
    if (state!=DuckDBSuccess) { error_set(error,message); duckdb_free(message); return false; }
    if (duckdb_connect(*db,c)!=DuckDBSuccess) { error_set(error,"Could not connect to workspace"); duckdb_close(db); return false; }
    return true;
}
static bool version(duckdb_connection c,bool existing,int *found,char **error) {
    if (!existing) { if (found) *found=0; return true; }
    duckdb_result r;
    if (duckdb_query(c,"SELECT version FROM _duxcel_workspace.meta",&r)!=DuckDBSuccess) {
        error_set(error,"This is not a dxls workbook. Choose a new .dxls file; existing data will not be overwritten."); duckdb_destroy_result(&r); return false;
    }
    int v=duckdb_value_int32(&r,0,0);
    bool ok=duckdb_row_count(&r)==1 && !duckdb_value_is_null(&r,0,0) && v>=1 && v<=DX_WORKSPACE_VERSION && (v<=12 || duckdb_column_type(&r,0)==DUCKDB_TYPE_INTEGER);
    duckdb_destroy_result(&r); if (!ok) error_set(error,"Unsupported or invalid Duxcel workspace version.");
    if (ok) ok=format_metadata_supported(c,v,error); if (ok && found) *found=v; return ok;
}
/* v14 is not implemented. Reject state that v12/v13 cannot represent before
   resolving/opening a destination. History is not serialized; a dangling bound
   history is still unsupported rather than silently treated as legacy input. */
static bool source_versions_legacy_saveable(const Worksheet *sheet,char **error) {
    const SourceVersions *versions=&sheet->source_versions; const ObjectStore *objects=&sheet->objects;
    bool bound=sheet->analysis.source_version || versions->high_water || versions->count || versions->capacity || versions->bytes || versions->items;
    if (sheet->undo_count>DX_HISTORY || sheet->redo_count>DX_HISTORY || objects->undo_count>DX_HISTORY || objects->redo_count>DX_HISTORY) return range_fail(error,"Invalid source-binding history metadata.");
    for (size_t i=0;i<sheet->undo_count;i++) bound|=sheet->undo[i].source_version!=0;
    for (size_t i=0;i<sheet->redo_count;i++) bound|=sheet->redo[i].source_version!=0;
    for (size_t i=0;i<objects->count;i++) bound|=objects->items[i].source.source_version!=0;
    for (size_t history=0;history<2;history++) {
        const ObjectSnapshot *stack=history ? objects->redo : objects->undo; size_t count=history ? objects->redo_count : objects->undo_count;
        for (size_t i=0;i<count;i++) {
            if (stack[i].count>DX_MAX_OBJECTS || (stack[i].count && !stack[i].items)) return range_fail(error,"Invalid source-binding object history metadata.");
            for (size_t j=0;j<stack[i].count;j++) bound|=stack[i].items[j].source.source_version!=0;
        }
    }
    return !bound || range_fail(error,"Source-version bindings require workspace v14; saving them through workspace v12/v13 is unsupported.");
}
bool workbook_save(const Workbook *b,const char *path,char **error) {
    if (!b || !path || !*path || (b->count && !b->sheets)) { if (error) error_set(error,"No valid workbook or destination to save."); return false; }
    if (!b->count || b->count>DX_MAX_SHEETS || b->active>=b->count) { error_set(error,"No valid worksheet to save."); return false; }
    if (b->sheets[b->active].hidden) { error_set(error,"The active worksheet must be visible."); return false; }
    if (!isfinite(b->user_scale) || b->user_scale<.8 || b->user_scale>1.5) { error_set(error,"Invalid workspace zoom."); return false; }
    for (size_t i=0;i<b->count;i++) {
        const Worksheet *s=&b->sheets[i]; const Analysis *a=&s->analysis; const SheetView *v=&s->view;
        if (!analysis_query_valid(a,error) || !analysis_filters_valid(a,error) || !analysis_sorts_valid(a,error)) return false;
        if (!s->name || strlen(s->name)>4096 || (a->path && strlen(a->path)>65536) ||
            a->kind<SOURCE_NONE || a->kind>SOURCE_DEMO || a->filter_op>FILTER_OP_MAX || a->aggregate>AGG_MAX ||
            !isfinite(v->horizontal) || v->horizontal<0 || !isfinite(v->fraction) || v->fraction<0 || v->fraction>=1 ||
            (v->canvas_zoom && (!isfinite(v->canvas_zoom)||v->canvas_zoom<.5f||v->canvas_zoom>2.5f)) ||
            v->row>=DX_SHEET_ROWS || v->anchor_row>=DX_SHEET_ROWS || v->end_row>=DX_SHEET_ROWS || v->top>=DX_SHEET_ROWS || v->col>=16384 || v->anchor_col>=16384 || v->end_col>=16384 || v->selection>3 || v->column_count>16384 || (v->column_count && (!v->columns || !v->widths))) {
            error_set(error,"Invalid worksheet definition or presentation."); return false;
        }
        for (size_t c=0;c<v->column_count;c++) if (!v->columns[c] || !isfinite(v->widths[c]) || v->widths[c]<52 || v->widths[c]>1200) {
            error_set(error,"Invalid column presentation metadata."); return false;
        }
        if (!objects_valid(&s->objects,error) || !source_versions_legacy_saveable(s,error) || !format_valid(&s->formats,error) || !format_persistable(&s->formats,error)) return false;
        for (size_t j=0;j<s->objects.count;j++) {
            const SheetObject *o=&s->objects.items[j];
            if (!o->id || !o->name || (unsigned)o->kind>OBJECT_TABLE || strlen(o->name)>4096 || !pivot_valid(&o->pivot,error) ||
                !objects_fits(&s->objects,(int)j,o->row,o->col,o->rows,o->columns,&s->cells,0,0,error)) { if (!*error) error_set(error,"Invalid sheet object."); return false; }
        }
    }
    char *target=workbook_target_path(path); if (!target) { error_set(error,"Workspace directory does not exist."); return false; }
    for (size_t i=0;i<b->count;i++) {
        const Worksheet *s=&b->sheets[i];
        if (s->analysis.path && *s->analysis.path && dx_same_file(target,s->analysis.path)) { free(target); error_set(error,"Choose a workbook destination other than a data source."); return false; }
        for (size_t j=0;j<s->objects.count;j++) if (s->objects.items[j].kind==OBJECT_TABLE && dx_same_file(target,s->objects.items[j].source.path)) { free(target); error_set(error,"Choose a workbook destination other than a table source."); return false; }
    }
    FILE *f=fopen(target,"rb"); bool existing=f!=NULL; if (f) fclose(f);
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_prepared_statement sheet=NULL,column=NULL,meta=NULL,cell=NULL;
    bool ok=false,transaction=false; char *temporary=NULL; int existing_version=0,write_version=12;
    for (size_t i=0;i<b->count;i++) {
        const ObjectStore *objects=&b->sheets[i].objects; uint64_t live=0;
        for (size_t o=0;o<objects->count;o++) if (objects->items[o].id>live) live=objects->items[o].id;
        if (objects->next_id>live) write_version=13;
    }
    if (!open_metadata(target,false,&db,&c,&temporary,error)) goto done;
    if (!version(c,existing,&existing_version,error)) goto done;
    if (existing_version==13) write_version=13;
    if (!exec(c,"BEGIN TRANSACTION",error)) goto done; transaction=true;
    if (!exec(c,"CREATE SCHEMA IF NOT EXISTS _duxcel_workspace; "
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.meta(version INTEGER,active_sheet UBIGINT,user_scale DOUBLE);"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.sheets("
        "id INTEGER PRIMARY KEY,name VARCHAR,kind INTEGER,path VARCHAR,source_schema VARCHAR,source_table VARCHAR,"
        "filter_column VARCHAR,filter_value VARCHAR,filter_op INTEGER,group_column VARCHAR,measure_column VARCHAR,aggregate INTEGER,sort_column VARCHAR,descending BOOLEAN,"
        "active_row UBIGINT,active_col UBIGINT,anchor_row UBIGINT,anchor_col UBIGINT,end_row UBIGINT,end_col UBIGINT,top_row UBIGINT,horizontal DOUBLE,fraction DOUBLE,selection INTEGER);"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.columns(sheet_id INTEGER,column_id INTEGER,name VARCHAR,width DOUBLE,PRIMARY KEY(sheet_id,column_id));"
        "ALTER TABLE _duxcel_workspace.sheets ADD COLUMN IF NOT EXISTS canvas_zoom DOUBLE DEFAULT 1;"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.cells(sheet_id INTEGER,row_id UBIGINT,column_id INTEGER,text VARCHAR,PRIMARY KEY(sheet_id,row_id,column_id));"
        "DELETE FROM _duxcel_workspace.cells;"
        "DELETE FROM _duxcel_workspace.meta; DELETE FROM _duxcel_workspace.sheets; DELETE FROM _duxcel_workspace.columns",error)) goto done;
    if (!prepare(c,"INSERT INTO _duxcel_workspace.meta VALUES (?,?,?)",&meta,error)) goto done;
    duckdb_bind_int32(meta,1,write_version); duckdb_bind_uint64(meta,2,b->active); duckdb_bind_double(meta,3,b->user_scale);
    if (!run(meta,error) || !prepare(c,"INSERT INTO _duxcel_workspace.sheets VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",&sheet,error) ||
        !prepare(c,"INSERT INTO _duxcel_workspace.columns VALUES (?,?,?,?)",&column,error) ||
        !prepare(c,"INSERT INTO _duxcel_workspace.cells VALUES (?,?,?,?)",&cell,error)) goto done;
    for (size_t i=0;i<b->count;i++) {
        const Worksheet *s=&b->sheets[i]; const Analysis *a=&s->analysis; const SheetView *v=&s->view;
        char *ref=workbook_relative_path(target,a->path);
        duckdb_bind_uint64(sheet,1,i); bind_text(sheet,2,s->name); duckdb_bind_int32(sheet,3,a->kind); bind_text(sheet,4,ref); free(ref);
        bind_text(sheet,5,a->schema); bind_text(sheet,6,a->table); bind_text(sheet,7,a->filter_column); bind_text(sheet,8,a->filter_value);
        duckdb_bind_int32(sheet,9,a->filter_op); bind_text(sheet,10,a->group_column); bind_text(sheet,11,a->measure_column); duckdb_bind_int32(sheet,12,a->aggregate);
        bind_text(sheet,13,a->sort_column); duckdb_bind_boolean(sheet,14,a->descending);
        duckdb_bind_uint64(sheet,15,v->row); duckdb_bind_uint64(sheet,16,v->col); duckdb_bind_uint64(sheet,17,v->anchor_row); duckdb_bind_uint64(sheet,18,v->anchor_col);
        duckdb_bind_uint64(sheet,19,v->end_row); duckdb_bind_uint64(sheet,20,v->end_col); duckdb_bind_uint64(sheet,21,v->top);
        duckdb_bind_double(sheet,22,v->horizontal); duckdb_bind_double(sheet,23,v->fraction); duckdb_bind_uint32(sheet,24,v->selection);
        duckdb_bind_double(sheet,25,v->canvas_zoom ? v->canvas_zoom : 1);
        if (!run(sheet,error)) goto done;
        for (size_t j=0;j<v->column_count;j++) {
            duckdb_bind_uint64(column,1,i); duckdb_bind_uint64(column,2,j); bind_text(column,3,v->columns[j]); duckdb_bind_double(column,4,v->widths[j]);
            if (!run(column,error)) goto done;
        }
        for (size_t j=0;j<s->cells.count;j++) {
            const SheetCell *value=&s->cells.cells[j];
            duckdb_bind_uint64(cell,1,i); duckdb_bind_uint64(cell,2,value->row); duckdb_bind_uint64(cell,3,value->col); bind_text(cell,4,value->text);
            if (!run(cell,error)) goto done;
        }
    }
    if (!save_objects(c,b,target,error) || !save_marks(c,b,error) || !save_formats(c,b,error) || (write_version==13 && !save_owner_package(c,b,error)) || !save_filters(c,b,error) || !save_sorts(c,b,error) || !save_queries(c,b,error) || !save_source_options(c,b,error) ||
        !exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.preferences(name VARCHAR PRIMARY KEY,value BOOLEAN); DELETE FROM _duxcel_workspace.preferences WHERE name='reduce_motion' OR starts_with(name,'sheet_hidden_')",error) ||
        !exec(c,b->reduced_motion ? "INSERT INTO _duxcel_workspace.preferences VALUES ('reduce_motion',true)" : "INSERT INTO _duxcel_workspace.preferences VALUES ('reduce_motion',false)",error)) goto done;
    for (size_t i=0;i<b->count;i++) if (b->sheets[i].hidden) {
        char sql[128]; snprintf(sql,sizeof(sql),"INSERT INTO _duxcel_workspace.preferences VALUES ('sheet_hidden_%zu',true)",i);
        if (!exec(c,sql,error)) goto done;
    }
    if (!exec(c,"COMMIT",error)) goto done;
    transaction=false; ok=true;
 done:
    if (transaction) { char *ignored=NULL; exec(c,"ROLLBACK",&ignored); free(ignored); }
    if (sheet) duckdb_destroy_prepare(&sheet); if (column) duckdb_destroy_prepare(&column); if (meta) duckdb_destroy_prepare(&meta);
    if (cell) duckdb_destroy_prepare(&cell);
    if (c) duckdb_disconnect(&c); if (db) duckdb_close(&db);
    dx_remove_temp_directory(temporary); free(temporary);
    if (!ok && !existing) remove(target);
    free(target); return ok;
}
static char *text_value(duckdb_result *r,idx_t c,idx_t row) {
    if (duckdb_value_is_null(r,c,row)) return NULL;
    char *s=duckdb_value_varchar(r,c,row); char *out=dx_strdup(s); duckdb_free(s); return out;
}
static bool load_sorts(duckdb_connection c,Workbook *b,char **error) {
    duckdb_result r={0}; bool ok=false;
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='sorts'",&r)!=DuckDBSuccess) goto sql_error;
    bool exists=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (exists) {
        if (duckdb_query(c,"SELECT sheet_id FROM _duxcel_workspace.sorts GROUP BY sheet_id HAVING sum(octet_length(encode(coalesce(name,''))))>32768 LIMIT 1",&r)!=DuckDBSuccess) goto sql_error;
        if (duckdb_row_count(&r)) goto invalid; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
        if (duckdb_query(c,"SELECT sheet_id,position,name,descending FROM _duxcel_workspace.sorts ORDER BY sheet_id,position LIMIT 449",&r)!=DuckDBSuccess) goto sql_error;
        if (duckdb_row_count(&r)>448) goto invalid;
        for (idx_t row=0;row<duckdb_row_count(&r);row++) {
            uint64_t sheet=duckdb_value_uint64(&r,0,row),position=duckdb_value_uint64(&r,1,row);
            if (duckdb_value_is_null(&r,0,row) || duckdb_value_is_null(&r,1,row) || duckdb_value_is_null(&r,2,row) || duckdb_value_is_null(&r,3,row) || sheet>=b->count || position>=DX_ANALYSIS_SORTS-1) goto invalid;
            Analysis *a=&b->sheets[sheet].analysis; char *name=text_value(&r,2,row);
            bool valid=a->sort_column && position==a->sort_count && analysis_sort_set(a,name,duckdb_value_boolean(&r,3,row),true,error) && a->sort_count==position+1;
            free(name); if (!valid) goto invalid;
        }
    }
    for (size_t i=0;i<b->count;i++) if (!analysis_sorts_valid(&b->sheets[i].analysis,error)) goto done;
    ok=true; goto done;
invalid: error_set(error,"Invalid sort order metadata."); goto done;
sql_error: error_set(error,duckdb_result_error(&r));
done: duckdb_destroy_result(&r); return ok;
}
static bool load_filters(duckdb_connection c,Workbook *b,char **error) {
    duckdb_result r={0}; bool ok=false;
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='filters'",&r)!=DuckDBSuccess) goto sql_error;
    bool exists=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (exists) {
        if (duckdb_query(c,"SELECT sheet_id FROM _duxcel_workspace.filters GROUP BY sheet_id HAVING sum(octet_length(encode(coalesce(name,'')))+octet_length(encode(coalesce(value,''))))>8388608 LIMIT 1",&r)!=DuckDBSuccess) goto sql_error;
        bool large=duckdb_row_count(&r)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r)); if (large) goto invalid;
        if (duckdb_query(c,"SELECT sheet_id,position,name,op,value FROM _duxcel_workspace.filters ORDER BY sheet_id,position LIMIT 961",&r)!=DuckDBSuccess) goto sql_error;
        if (duckdb_row_count(&r)>DX_MAX_SHEETS*(DX_ANALYSIS_FILTERS-1)) goto invalid;
        for (idx_t i=0;i<duckdb_row_count(&r);i++) {
            uint64_t sheet=duckdb_value_uint64(&r,0,i),position=duckdb_value_uint64(&r,1,i); int op=duckdb_value_int32(&r,3,i);
            if (sheet>=b->count || position>=DX_ANALYSIS_FILTERS-1 || op<0 || op>FILTER_OP_MAX || duckdb_value_is_null(&r,2,i)) goto invalid;
            Analysis *a=&b->sheets[sheet].analysis; char *name=text_value(&r,2,i),*value=text_value(&r,4,i);
            bool valid=a->filter_column && position==a->filter_count && !analysis_filter_get(a,name,NULL,NULL) && analysis_filter_set(a,name,(FilterOp)op,value,error);
            free(name); free(value); if (!valid) goto invalid;
        }
    }
    for (size_t i=0;i<b->count;i++) if (!analysis_filters_valid(&b->sheets[i].analysis,error)) goto done;
    ok=true; goto done;
 invalid: error_set(error,"Invalid column filter metadata."); goto done;
 sql_error: error_set(error,duckdb_result_error(&r));
 done: duckdb_destroy_result(&r); return ok;
}
static SheetObject *table_object(Workbook *b,uint64_t sheet,uint64_t id) {
    if (sheet>=b->count) return NULL;
    ObjectStore *store=&b->sheets[sheet].objects;
    for (size_t i=0;i<store->count;i++) if (store->items[i].id==id && store->items[i].kind==OBJECT_TABLE) return &store->items[i];
    return NULL;
}
static bool load_queries(duckdb_connection c,Workbook *b,char **error) {
    duckdb_result r={0}; bool ok=false;
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='queries'",&r)!=DuckDBSuccess) goto sql_error;
    bool exists=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!exists) {
        if (duckdb_query(c,"SELECT version FROM _duxcel_workspace.meta",&r)!=DuckDBSuccess) goto sql_error;
        if (duckdb_value_int32(&r,0,0)>=7) goto invalid;
        ok=true; goto done;
    }
    if (duckdb_query(c,"SELECT 1 FROM _duxcel_workspace.queries WHERE query IS NULL OR octet_length(encode(query))>65536 OR length(trim(query))=0 OR contains(query,chr(0)) LIMIT 1",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)) goto invalid; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,object_id,query FROM _duxcel_workspace.queries ORDER BY sheet_id,object_id LIMIT 2113",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)>DX_MAX_SHEETS*(DX_MAX_OBJECTS+1) || duckdb_column_type(&r,0)!=DUCKDB_TYPE_INTEGER || duckdb_column_type(&r,1)!=DUCKDB_TYPE_UBIGINT || duckdb_column_type(&r,2)!=DUCKDB_TYPE_VARCHAR) goto invalid;
    for (idx_t row=0;row<duckdb_row_count(&r);row++) {
        for (idx_t col=0;col<3;col++) if (duckdb_value_is_null(&r,col,row)) goto invalid;
        int32_t sheet_id=duckdb_value_int32(&r,0,row); uint64_t id=duckdb_value_uint64(&r,1,row);
        if (sheet_id<0 || (size_t)sheet_id>=b->count) goto invalid; size_t sheet=(size_t)sheet_id;
        SheetObject *o=id ? table_object(b,sheet,id) : NULL; if (id && !o) goto invalid;
        Analysis *a=o ? &o->source : &b->sheets[sheet].analysis; if (a->query) goto invalid;
        duckdb_string text=duckdb_value_string(&r,2,row);
        if (!text.data || strlen(text.data)!=text.size) { duckdb_free(text.data); goto invalid; }
        a->query=dx_strdup(text.data); duckdb_free(text.data);
        if (!analysis_query_valid(a,error)) goto invalid;
    }
    for (size_t i=0;i<b->count;i++) if (!objects_valid(&b->sheets[i].objects,error)) goto done;
    ok=true; goto done;
invalid: error_set(error,"Invalid source query metadata."); goto done;
sql_error: error_set(error,duckdb_result_error(&r));
done: duckdb_destroy_result(&r); return ok;
}
static bool load_source_options(duckdb_connection c,Workbook *b,char **error) {
    duckdb_result r={0}; bool ok=false,seen[DX_MAX_SHEETS][DX_MAX_OBJECTS+1]={{0}};
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='source_options'",&r)!=DuckDBSuccess) goto sql_error;
    bool exists=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!exists) {
        if (duckdb_query(c,"SELECT version FROM _duxcel_workspace.meta",&r)!=DuckDBSuccess) goto sql_error;
        if (duckdb_value_int32(&r,0,0)>=8) goto invalid;
        ok=true; goto done;
    }
    if (duckdb_query(c,"SELECT sheet_id,object_id,unique_rows FROM _duxcel_workspace.source_options ORDER BY sheet_id,object_id LIMIT 2113",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)>DX_MAX_SHEETS*(DX_MAX_OBJECTS+1) || duckdb_column_type(&r,0)!=DUCKDB_TYPE_INTEGER || duckdb_column_type(&r,1)!=DUCKDB_TYPE_UBIGINT || duckdb_column_type(&r,2)!=DUCKDB_TYPE_BOOLEAN) goto invalid;
    for (idx_t row=0;row<duckdb_row_count(&r);row++) {
        for (idx_t col=0;col<3;col++) if (duckdb_value_is_null(&r,col,row)) goto invalid;
        int32_t sheet=duckdb_value_int32(&r,0,row); uint64_t id=duckdb_value_uint64(&r,1,row); size_t index=0;
        if (sheet<0 || (size_t)sheet>=b->count) goto invalid;
        SheetObject *o=id ? table_object(b,(size_t)sheet,id) : NULL; if (id && !o) goto invalid;
        if (o) index=(size_t)(o-b->sheets[sheet].objects.items)+1;
        if (seen[sheet][index]) goto invalid; seen[sheet][index]=true;
        Analysis *a=o ? &o->source : &b->sheets[sheet].analysis; a->unique_rows=duckdb_value_boolean(&r,2,row);
        if (!analysis_query_valid(a,error)) goto invalid;
    }
    ok=true; goto done;
invalid: error_set(error,"Invalid source options metadata."); goto done;
sql_error: error_set(error,duckdb_result_error(&r));
done: duckdb_destroy_result(&r); return ok;
}
static bool load_table_sources(duckdb_connection c,Workbook *b,const char *target,char **error) {
    duckdb_result r={0}; bool ok=false;
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='table_sources'",&r)!=DuckDBSuccess) goto sql_error;
    bool exists=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r)); if (!exists) return true;
    if (duckdb_query(c,"SELECT sheet_id FROM ("
        "SELECT sheet_id,sum(octet_length(encode(coalesce(path,'')))+octet_length(encode(coalesce(source_schema,'')))+octet_length(encode(coalesce(source_table,'')))+octet_length(encode(coalesce(filter_column,'')))+octet_length(encode(coalesce(filter_value,'')))+octet_length(encode(coalesce(group_column,'')))+octet_length(encode(coalesce(measure_column,'')))+octet_length(encode(coalesce(sort_column,'')))) AS bytes FROM _duxcel_workspace.table_sources GROUP BY sheet_id UNION ALL "
        "SELECT sheet_id,sum(octet_length(encode(coalesce(name,'')))+octet_length(encode(coalesce(value,'')))) FROM _duxcel_workspace.table_filters GROUP BY sheet_id UNION ALL "
        "SELECT sheet_id,sum(octet_length(encode(coalesce(name,'')))) FROM _duxcel_workspace.table_sorts GROUP BY sheet_id) GROUP BY sheet_id HAVING sum(bytes)>8388608 LIMIT 1",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)) goto invalid; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,object_id,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending FROM _duxcel_workspace.table_sources ORDER BY sheet_id,object_id LIMIT 2049",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)>DX_MAX_SHEETS*DX_MAX_OBJECTS || duckdb_column_type(&r,0)!=DUCKDB_TYPE_INTEGER || duckdb_column_type(&r,1)!=DUCKDB_TYPE_UBIGINT || duckdb_column_type(&r,2)!=DUCKDB_TYPE_INTEGER || duckdb_column_type(&r,8)!=DUCKDB_TYPE_INTEGER || duckdb_column_type(&r,11)!=DUCKDB_TYPE_INTEGER || duckdb_column_type(&r,13)!=DUCKDB_TYPE_BOOLEAN) goto invalid;
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        SheetObject *o=table_object(b,duckdb_value_uint64(&r,0,i),duckdb_value_uint64(&r,1,i));
        int kind=duckdb_value_int32(&r,2,i),op=duckdb_value_int32(&r,8,i),agg=duckdb_value_int32(&r,11,i);
        if (!o || o->source.kind!=SOURCE_NONE || kind<SOURCE_CSV || kind>SOURCE_DEMO || op<0 || op>FILTER_OP_MAX || agg<0 || agg>AGG_MAX) goto invalid;
        const idx_t required[]={0,1,2,3,8,11,13}; for (size_t n=0;n<sizeof(required)/sizeof(*required);n++) if (duckdb_value_is_null(&r,required[n],i)) goto invalid;
        char *ref=text_value(&r,3,i); if (strlen(ref)>65536 || (kind!=SOURCE_DEMO && !*ref)) { free(ref); goto invalid; }
        Analysis *a=&o->source; a->kind=(SourceKind)kind; a->path=kind==SOURCE_DEMO ? dx_strdup(ref) : workbook_source_path(target,ref); free(ref);
        a->schema=text_value(&r,4,i); a->table=text_value(&r,5,i); a->filter_column=text_value(&r,6,i); a->filter_value=text_value(&r,7,i); a->filter_op=(FilterOp)op;
        a->group_column=text_value(&r,9,i); a->measure_column=text_value(&r,10,i); a->aggregate=(Aggregate)agg; a->sort_column=text_value(&r,12,i); a->descending=duckdb_value_boolean(&r,13,i);
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,object_id,position,name,op,value FROM _duxcel_workspace.table_filters ORDER BY sheet_id,object_id,position LIMIT 30721",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)>DX_MAX_SHEETS*DX_MAX_OBJECTS*(DX_ANALYSIS_FILTERS-1) || duckdb_column_type(&r,0)!=DUCKDB_TYPE_INTEGER || duckdb_column_type(&r,1)!=DUCKDB_TYPE_UBIGINT || duckdb_column_type(&r,2)!=DUCKDB_TYPE_INTEGER || duckdb_column_type(&r,4)!=DUCKDB_TYPE_INTEGER) goto invalid;
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        SheetObject *o=table_object(b,duckdb_value_uint64(&r,0,i),duckdb_value_uint64(&r,1,i)); uint64_t position=duckdb_value_uint64(&r,2,i); int op=duckdb_value_int32(&r,4,i);
        for (idx_t n=0;n<5;n++) if (duckdb_value_is_null(&r,n,i)) goto invalid;
        if (!o || !o->source.filter_column || position!=o->source.filter_count || position>=DX_ANALYSIS_FILTERS-1 || op<0 || op>FILTER_OP_MAX) goto invalid;
        char *name=text_value(&r,3,i),*value=text_value(&r,5,i);
        bool valid=!analysis_filter_get(&o->source,name,NULL,NULL) && analysis_filter_set(&o->source,name,(FilterOp)op,value,error); free(name); free(value); if (!valid) goto invalid;
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,object_id,position,name,descending FROM _duxcel_workspace.table_sorts ORDER BY sheet_id,object_id,position LIMIT 14337",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)>DX_MAX_SHEETS*DX_MAX_OBJECTS*(DX_ANALYSIS_SORTS-1) || duckdb_column_type(&r,0)!=DUCKDB_TYPE_INTEGER || duckdb_column_type(&r,1)!=DUCKDB_TYPE_UBIGINT || duckdb_column_type(&r,2)!=DUCKDB_TYPE_INTEGER || duckdb_column_type(&r,4)!=DUCKDB_TYPE_BOOLEAN) goto invalid;
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        SheetObject *o=table_object(b,duckdb_value_uint64(&r,0,i),duckdb_value_uint64(&r,1,i)); uint64_t position=duckdb_value_uint64(&r,2,i);
        for (idx_t n=0;n<5;n++) if (duckdb_value_is_null(&r,n,i)) goto invalid;
        if (!o || !o->source.sort_column || position!=o->source.sort_count || position>=DX_ANALYSIS_SORTS-1) goto invalid;
        char *name=text_value(&r,3,i); size_t before=o->source.sort_count;
        bool valid=analysis_sort_set(&o->source,name,duckdb_value_boolean(&r,4,i),true,error) && o->source.sort_count==before+1; free(name); if (!valid) goto invalid;
    }
    ok=true; goto done;
invalid: error_set(error,"Invalid table source metadata."); goto done;
sql_error: error_set(error,duckdb_result_error(&r));
done: duckdb_destroy_result(&r); return ok;
}
static bool load_formats(duckdb_connection c,Workbook *b,char **error) {
    duckdb_result r={0}; bool ok=false;
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='formats'",&r)!=DuckDBSuccess) goto sql_error;
    bool exists=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!exists) {
        if (duckdb_query(c,"SELECT version>=10 FROM _duxcel_workspace.meta",&r)!=DuckDBSuccess) goto sql_error;
        bool required=duckdb_value_boolean(&r,0,0); duckdb_destroy_result(&r); if (!required) return true; goto invalid;
    }
    if (duckdb_query(c,"SELECT sheet_id,position,r0,r1,c0,c1,mask,flags,alignment,color,font_size,numeric_format,decimals FROM _duxcel_workspace.formats ORDER BY sheet_id,position LIMIT 32769",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_column_count(&r)!=13 || duckdb_row_count(&r)>DX_MAX_SHEETS*DX_FORMAT_PATCHES) goto invalid;
    for (idx_t col=0;col<13;col++) if (duckdb_column_type(&r,col)!=(col==2 || col==3 ? DUCKDB_TYPE_UBIGINT : DUCKDB_TYPE_INTEGER)) goto invalid;
    size_t counts[DX_MAX_SHEETS]={0};
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        int sheet=duckdb_value_int32(&r,0,i); if (sheet<0 || (size_t)sheet>=b->count || ++counts[sheet]>DX_FORMAT_PATCHES) goto invalid;
        for (idx_t col=0;col<13;col++) if (duckdb_value_is_null(&r,col,i)) goto invalid;
    }
    for (size_t i=0;i<b->count;i++) if (counts[i]) b->sheets[i].formats.items=dx_alloc(counts[i]*sizeof(FormatPatch));
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        int sheet=duckdb_value_int32(&r,0,i),position=duckdb_value_int32(&r,1,i),c0=duckdb_value_int32(&r,4,i),c1=duckdb_value_int32(&r,5,i),mask=duckdb_value_int32(&r,6,i);
        int flags=duckdb_value_int32(&r,7,i),align=duckdb_value_int32(&r,8,i),color=duckdb_value_int32(&r,9,i),size=duckdb_value_int32(&r,10,i),number=duckdb_value_int32(&r,11,i),decimals=duckdb_value_int32(&r,12,i);
        FormatStore *s=&b->sheets[sheet].formats;
        if (position<0 || (size_t)position!=s->count || c0<0 || c1<0 || c1>=16384 || mask<=0 || mask>FORMAT_ALL || flags<0 || flags>7 || align<0 || align>FORMAT_RIGHT || color<0 || color>3 || size<11 || size>16 || number<0 || number>FORMAT_NUMBER || decimals<0 || decimals>4) goto invalid;
        s->items[s->count++]=(FormatPatch){.range={duckdb_value_uint64(&r,2,i),duckdb_value_uint64(&r,3,i),(size_t)c0,(size_t)c1},.mask=(unsigned)mask,
            .value={.flags=(unsigned char)flags,.align=(unsigned char)align,.color=(unsigned char)color,.size=(unsigned char)size,.number=(unsigned char)number,.decimals=(unsigned char)decimals}};
    }
    for (size_t i=0;i<b->count;i++) if (!format_valid(&b->sheets[i].formats,error)) goto done;
    ok=true; goto done;
invalid: error_set(error,"Invalid cell formatting metadata."); goto done;
sql_error: error_set(error,duckdb_result_error(&r));
done: duckdb_destroy_result(&r); return ok;
}
static bool load_objects(duckdb_connection c,Workbook *b,const char *target,char **error) {
    duckdb_result r={0}; bool ok=false;
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='objects'",&r)!=DuckDBSuccess) goto sql_error;
    bool exists=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r)); if (!exists) return true;
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='objects' AND column_name='kind'",&r)!=DuckDBSuccess) goto sql_error;
    bool typed=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='objects' AND column_name='mark'",&r)!=DuckDBSuccess) goto sql_error;
    bool marked=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='objects' AND column_name='source_id'",&r)!=DuckDBSuccess) goto sql_error;
    bool bound=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='objects' AND column_name='chart_measure'",&r)!=DuckDBSuccess) goto sql_error;
    bool plotted=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    char *object_sql=dx_printf("SELECT sheet_id,id,name,row_id,column_id,height,width,%s,%s,%s,%s FROM _duxcel_workspace.objects ORDER BY sheet_id,id LIMIT 2049",typed ? "kind" : "0",marked ? "mark" : "0",bound ? "source_id" : "0::UBIGINT",plotted ? "chart_measure" : "0::INTEGER");
    bool queried=duckdb_query(c,object_sql,&r)==DuckDBSuccess; free(object_sql); if (!queried) goto sql_error;
    if ((typed && duckdb_column_type(&r,7)!=DUCKDB_TYPE_INTEGER) || (marked && duckdb_column_type(&r,8)!=DUCKDB_TYPE_INTEGER) || duckdb_column_type(&r,9)!=DUCKDB_TYPE_UBIGINT || duckdb_column_type(&r,10)!=DUCKDB_TYPE_INTEGER) goto invalid;
    if (duckdb_row_count(&r)>DX_MAX_SHEETS*DX_MAX_OBJECTS) goto invalid;
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        uint64_t sheet=duckdb_value_uint64(&r,0,i),id=duckdb_value_uint64(&r,1,i),row=duckdb_value_uint64(&r,3,i),col=duckdb_value_uint64(&r,4,i),height=duckdb_value_uint64(&r,5,i),width=duckdb_value_uint64(&r,6,i);
        int kind=duckdb_value_int32(&r,7,i),mark=duckdb_value_int32(&r,8,i),measure=duckdb_value_int32(&r,10,i);
        if (sheet>=b->count || !id || kind<0 || kind>OBJECT_TABLE || duckdb_value_is_null(&r,2,i) || duckdb_value_is_null(&r,7,i) || duckdb_value_is_null(&r,8,i) || duckdb_value_is_null(&r,9,i) || duckdb_value_is_null(&r,10,i) || measure<0 || measure>=DX_PIVOT_FIELDS || mark<0 || mark>MARK_SCATTER || (kind!=OBJECT_CHART && mark!=MARK_BAR) || col>=16384 || !height || !width) goto invalid;
        ObjectStore *store=&b->sheets[sheet].objects;
        if (store->count==DX_MAX_OBJECTS || (store->count && id<=store->items[store->count-1].id)) goto invalid;
        char *name=text_value(&r,2,i); if (strlen(name)>4096) { free(name); goto invalid; }
        store->items=dx_realloc(store->items,(store->count+1)*sizeof(SheetObject));
        store->items[store->count++]=(SheetObject){.id=id,.row=row,.col=(size_t)col,.rows=height,.columns=(size_t)width,.revision=1,.name=name,.kind=(ObjectKind)kind,.mark=(ChartMark)mark,.chart_measure=(unsigned char)measure,.source_id=duckdb_value_uint64(&r,9,i)}; store->next_id=id;
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id FROM _duxcel_workspace.pivot_fields GROUP BY sheet_id HAVING sum(octet_length(encode(coalesce(name,'')))+octet_length(encode(coalesce(value,'')))) > 8388608 LIMIT 1",&r)!=DuckDBSuccess) goto sql_error;
    bool oversized=duckdb_row_count(&r)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r)); if (oversized) goto invalid;
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='pivot_fields' AND column_name='grain'",&r)!=DuckDBSuccess) goto sql_error;
    bool grained=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!grained) {
        if (duckdb_query(c,"SELECT version>=9 FROM _duxcel_workspace.meta",&r)!=DuckDBSuccess) goto sql_error;
        bool required=duckdb_value_boolean(&r,0,0); duckdb_destroy_result(&r); memset(&r,0,sizeof(r)); if (required) goto invalid;
    }
    char *field_sql=dx_printf("SELECT sheet_id,object_id,role,position,name,aggregate,filter_op,value,%s FROM _duxcel_workspace.pivot_fields ORDER BY sheet_id,object_id,role,position LIMIT 65537",grained ? "grain" : "0::INTEGER");
    queried=duckdb_query(c,field_sql,&r)==DuckDBSuccess; free(field_sql); if (!queried) goto sql_error;
    if (duckdb_column_type(&r,8)!=DUCKDB_TYPE_INTEGER) goto invalid;
    if (duckdb_row_count(&r)>DX_MAX_SHEETS*DX_MAX_OBJECTS*4*DX_PIVOT_FIELDS) goto invalid;
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        uint64_t sheet=duckdb_value_uint64(&r,0,i),id=duckdb_value_uint64(&r,1,i),position=duckdb_value_uint64(&r,3,i);
        int role=duckdb_value_int32(&r,2,i),aggregate=duckdb_value_int32(&r,5,i),op=duckdb_value_int32(&r,6,i),grain=duckdb_value_int32(&r,8,i);
        if (sheet>=b->count || role<0 || role>3 || position>=DX_PIVOT_FIELDS || aggregate<0 || aggregate>AGG_MAX || op<0 || op>FILTER_OP_MAX || grain<0 || grain>PIVOT_DATE_MONTH || (role>=PIVOT_VALUES && grain) || duckdb_value_is_null(&r,4,i) || duckdb_value_is_null(&r,8,i)) goto invalid;
        ObjectStore *store=&b->sheets[sheet].objects; SheetObject *o=NULL;
        for (size_t j=0;j<store->count;j++) if (store->items[j].id==id) o=&store->items[j];
        if (!o || o->pivot.counts[role]!=position) goto invalid;
        PivotField *f=&o->pivot.fields[role][o->pivot.counts[role]++];
        *f=(PivotField){.name=text_value(&r,4,i),.aggregate=(Aggregate)aggregate,.filter=(FilterOp)op,.grain=(PivotDateGrain)grain,.value=text_value(&r,7,i)};
    }
    if (!load_table_sources(c,b,target,error) || !load_marks(c,b,error)) goto done;
    for (size_t i=0;i<b->count;i++) {
        Worksheet *s=&b->sheets[i];
        if (!objects_valid(&s->objects,error)) goto done;
        for (size_t j=0;j<s->objects.count;j++) {
            SheetObject *o=&s->objects.items[j];
            if (!pivot_valid(&o->pivot,error) || !objects_fits(&s->objects,(int)j,o->row,o->col,o->rows,o->columns,&s->cells,0,0,error)) goto done;
        }
    }
    ok=true; goto done;
 invalid: error_set(error,"Invalid sheet object metadata."); goto done;
 sql_error: error_set(error,duckdb_result_error(&r));
 done: duckdb_destroy_result(&r); return ok;
}
static bool load_owner_package(duckdb_connection c,Workbook *book,char **error) {
    duckdb_result r={0}; bool ok=false;
    if (duckdb_query(c,"SELECT sheet_id,high_water FROM _duxcel_workspace.object_counters ORDER BY sheet_id LIMIT 65",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)!=book->count) goto invalid;
    for (size_t i=0;i<book->count;i++) {
        if (duckdb_value_is_null(&r,0,i) || duckdb_value_is_null(&r,1,i) || duckdb_value_int32(&r,0,i)!=(int32_t)i || duckdb_value_uint64(&r,1,i)<book->sheets[i].objects.next_id) goto invalid;
        book->sheets[i].objects.next_id=duckdb_value_uint64(&r,1,i);
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,position,kind FROM _duxcel_workspace.format_selectors ORDER BY sheet_id,position LIMIT 32769",&r)!=DuckDBSuccess) goto sql_error;
    size_t expected=0,counts[DX_MAX_SHEETS]={0}; for (size_t i=0;i<book->count;i++) expected+=book->sheets[i].formats.count;
    if (duckdb_row_count(&r)!=expected) goto invalid;
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        int sheet=duckdb_value_int32(&r,0,i),position=duckdb_value_int32(&r,1,i);
        if (duckdb_value_is_null(&r,0,i) || duckdb_value_is_null(&r,1,i) || duckdb_value_is_null(&r,2,i) || sheet<0 || (size_t)sheet>=book->count || position<0 || (size_t)position!=counts[sheet]++ || (size_t)position>=book->sheets[sheet].formats.count || duckdb_value_int32(&r,2,i)!=FORMAT_SPATIAL) goto invalid;
    }
    ok=true; goto done;
invalid: error_set(error,"Invalid workspace v13 owner counters or spatial selector metadata."); goto done;
sql_error: error_set(error,duckdb_result_error(&r));
done: duckdb_destroy_result(&r); return ok;
}
bool workbook_load(Workbook *out,const char *path,char **error) {
    char *target=dx_absolute_path(path); if (!target) { error_set(error,"Workspace file is missing or inaccessible."); return false; }
    duckdb_database db=NULL; duckdb_connection c=NULL; Workbook book; workbook_init(&book); duckdb_result r={0}; bool ok=false; char *temporary=NULL; int workspace_version=0;
    if (!open_metadata(target,true,&db,&c,&temporary,error) || !version(c,true,&workspace_version,error)) goto done;
    if (duckdb_query(c,"SELECT active_sheet,user_scale FROM _duxcel_workspace.meta",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); goto done; }
    size_t active=(size_t)duckdb_value_uint64(&r,0,0); double zoom=duckdb_value_double(&r,1,0); duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!isfinite(zoom) || zoom<.8 || zoom>1.5) { error_set(error,"Invalid workspace zoom."); goto done; }
    if (duckdb_query(c,"SELECT * FROM _duxcel_workspace.sheets ORDER BY id LIMIT 65",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); goto done; }
    size_t n=(size_t)duckdb_row_count(&r);
    idx_t fields=duckdb_column_count(&r);
    if (!n || n>DX_MAX_SHEETS || active>=n || (fields!=24 && fields!=25)) { error_set(error,"Invalid worksheet metadata."); goto done; }
    for (size_t i=0;i<n;i++) {
        if (duckdb_value_uint64(&r,0,i)!=i) { error_set(error,"Invalid worksheet ordering."); goto done; }
        char *name=text_value(&r,1,i),*ref=text_value(&r,3,i); Analysis a={0};
        int kind=duckdb_value_int32(&r,2,i),op=duckdb_value_int32(&r,8,i),agg=duckdb_value_int32(&r,11,i);
        if (!name || !ref || strlen(name)>4096 || strlen(ref)>65536 || kind<SOURCE_NONE || kind>SOURCE_DEMO || op<0 || op>FILTER_OP_MAX || agg<0 || agg>AGG_MAX) {
            free(name); free(ref); error_set(error,"Invalid worksheet definition."); goto done;
        }
        a.kind=(SourceKind)kind; a.path=kind==SOURCE_DEMO || kind==SOURCE_NONE ? dx_strdup("") : workbook_source_path(target,ref); free(ref);
        a.schema=text_value(&r,4,i); a.table=text_value(&r,5,i); a.filter_column=text_value(&r,6,i); a.filter_value=text_value(&r,7,i); a.filter_op=(FilterOp)op;
        a.group_column=text_value(&r,9,i); a.measure_column=text_value(&r,10,i); a.aggregate=(Aggregate)agg; a.sort_column=text_value(&r,12,i); a.descending=duckdb_value_boolean(&r,13,i);
        workbook_add(&book,name,&a,error); free(name); analysis_free(&a);
        SheetView *v=&book.sheets[i].view;
        v->row=duckdb_value_uint64(&r,14,i); v->col=(size_t)duckdb_value_uint64(&r,15,i);
        v->anchor_row=duckdb_value_uint64(&r,16,i); v->anchor_col=(size_t)duckdb_value_uint64(&r,17,i);
        v->end_row=duckdb_value_uint64(&r,18,i); v->end_col=(size_t)duckdb_value_uint64(&r,19,i); v->top=duckdb_value_uint64(&r,20,i);
        v->horizontal=(float)duckdb_value_double(&r,21,i); v->fraction=duckdb_value_double(&r,22,i); v->selection=duckdb_value_uint32(&r,23,i);
        v->canvas_zoom=fields==25 ? (float)duckdb_value_double(&r,24,i) : 1;
        if (!isfinite(v->canvas_zoom)||v->canvas_zoom<.5f||v->canvas_zoom>2.5f) { error_set(error,"Invalid canvas zoom."); goto done; }
        if (!isfinite(v->horizontal)||v->horizontal<0||!isfinite(v->fraction)||v->fraction<0||v->fraction>=1||v->selection>3||v->row>=DX_SHEET_ROWS||v->anchor_row>=DX_SHEET_ROWS||v->end_row>=DX_SHEET_ROWS||v->top>=DX_SHEET_ROWS||v->col>=16384||v->anchor_col>=16384||v->end_col>=16384) { error_set(error,"Invalid worksheet viewport."); goto done; }
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,column_id,name,width FROM _duxcel_workspace.columns ORDER BY sheet_id,column_id LIMIT 1048577",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); goto done; }
    if (duckdb_row_count(&r)>1048576) { error_set(error,"Too many workspace columns."); goto done; }
    size_t column_counts[DX_MAX_SHEETS]={0};
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        size_t sheet=(size_t)duckdb_value_uint64(&r,0,i);
        if (sheet>=n || ++column_counts[sheet]>16384) { error_set(error,"Invalid workspace column count."); goto done; }
    }
    for (size_t i=0;i<n;i++) {
        book.sheets[i].view.columns=dx_alloc(column_counts[i]*sizeof(char *));
        book.sheets[i].view.widths=dx_alloc(column_counts[i]*sizeof(float));
    }
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        size_t sheet=(size_t)duckdb_value_uint64(&r,0,i),index=(size_t)duckdb_value_uint64(&r,1,i); double width=duckdb_value_double(&r,3,i);
        if (sheet>=n || index!=book.sheets[sheet].view.column_count || index>=16384 || !isfinite(width) || width<52 || width>1200 || duckdb_value_is_null(&r,2,i)) {
            error_set(error,"Invalid column presentation metadata."); goto done;
        }
        SheetView *v=&book.sheets[sheet].view; v->column_count++;
        v->columns[index]=text_value(&r,2,i); v->widths[index]=(float)width;
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='cells'",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); goto done; }
    bool has_cells=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (has_cells) {
        if (duckdb_query(c,"SELECT sheet_id,row_id,column_id,text FROM _duxcel_workspace.cells ORDER BY sheet_id,row_id,column_id LIMIT 1280001",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); goto done; }
        if (duckdb_row_count(&r)>1280000) { error_set(error,"Too many editable cells."); goto done; }
        size_t counts[DX_MAX_SHEETS]={0};
        for (idx_t i=0;i<duckdb_row_count(&r);i++) {
            size_t sheet=(size_t)duckdb_value_uint64(&r,0,i);
            if (sheet>=n || ++counts[sheet]>DX_MAX_EDIT_CELLS) { error_set(error,"Invalid editable cell count."); goto done; }
        }
        for (size_t i=0;i<n;i++) { book.sheets[i].cells.cells=dx_alloc(counts[i]*sizeof(SheetCell)); book.sheets[i].cells.capacity=counts[i]; }
        for (idx_t i=0;i<duckdb_row_count(&r);i++) {
            size_t sheet=(size_t)duckdb_value_uint64(&r,0,i),col=(size_t)duckdb_value_uint64(&r,2,i); uint64_t row=duckdb_value_uint64(&r,1,i);
            char *text=text_value(&r,3,i); CellStore *store=&book.sheets[sheet].cells;
            if (!text || strlen(text)>65536 || col>=16384 || row>=DX_SHEET_ROWS || store->bytes+strlen(text)+1>DX_CELL_BYTES) { free(text); error_set(error,"Invalid editable cell data."); goto done; }
            store->bytes+=strlen(text)+1; store->cells[store->count++]=(SheetCell){row,col,text};
        }
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!load_filters(c,&book,error) || !load_sorts(c,&book,error) || !load_objects(c,&book,target,error) || !load_formats(c,&book,error) || !load_queries(c,&book,error) || !load_source_options(c,&book,error) || (workspace_version==13 && !load_owner_package(c,&book,error))) goto done;
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name='preferences'",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); goto done; }
    bool preferences=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (preferences) {
        if (duckdb_query(c,"SELECT value FROM _duxcel_workspace.preferences WHERE name='reduce_motion' LIMIT 2",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); goto done; }
        if (duckdb_row_count(&r)>1 || duckdb_column_type(&r,0)!=DUCKDB_TYPE_BOOLEAN || (duckdb_row_count(&r) && duckdb_value_is_null(&r,0,0))) { error_set(error,"Invalid workspace preferences."); goto done; }
        if (duckdb_row_count(&r)) book.reduced_motion=duckdb_value_boolean(&r,0,0);
        duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
        if (duckdb_query(c,"SELECT name,value FROM _duxcel_workspace.preferences WHERE starts_with(name,'sheet_hidden_') LIMIT 65",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); goto done; }
        if (duckdb_row_count(&r)>DX_MAX_SHEETS || duckdb_column_type(&r,1)!=DUCKDB_TYPE_BOOLEAN) { error_set(error,"Invalid worksheet visibility preferences."); goto done; }
        bool seen[DX_MAX_SHEETS]={0};
        for (idx_t i=0;i<duckdb_row_count(&r);i++) {
            char *name=text_value(&r,0,i),tail; unsigned index=DX_MAX_SHEETS;
            bool valid=name && sscanf(name,"sheet_hidden_%u%c",&index,&tail)==1 && index<book.count && !seen[index] && !duckdb_value_is_null(&r,1,i);
            free(name); if (!valid) { error_set(error,"Invalid worksheet visibility preferences."); goto done; }
            seen[index]=true; book.sheets[index].hidden=duckdb_value_boolean(&r,1,i);
        }
        duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    }
    if (book.sheets[active].hidden) { error_set(error,"The active worksheet must be visible."); goto done; }
    book.active=active; book.user_scale=(float)zoom; book.path=target; target=NULL; book.dirty=false;
    workbook_free(out); *out=book; workbook_init(&book); ok=true;
 done:
    duckdb_destroy_result(&r); workbook_free(&book); if (c) duckdb_disconnect(&c); if (db) duckdb_close(&db); dx_remove_temp_directory(temporary); free(temporary); free(target); return ok;
}
#endif
