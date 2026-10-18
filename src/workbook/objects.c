#include "objects.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
static bool fail(char **error,const char *text) { if (error) { free(*error); *error=dx_strdup(text); } return false; }
void objects_snapshot_free(ObjectSnapshot *s) {
    for (size_t i=0;i<s->count;i++) { free(s->items[i].name); pivot_free(&s->items[i].pivot); analysis_free(&s->items[i].source); marks_free(&s->items[i].marks); }
    free(s->items); memset(s,0,sizeof(*s));
}
static ObjectSnapshot snapshot(const ObjectStore *s) {
    ObjectSnapshot out={.count=s->count}; out.items=dx_alloc(out.count*sizeof(SheetObject));
    for (size_t i=0;i<s->count;i++) { out.items[i]=s->items[i]; out.items[i].name=dx_strdup(s->items[i].name); out.items[i].pivot=pivot_copy(&s->items[i].pivot); out.items[i].source=analysis_copy(&s->items[i].source); out.items[i].marks=marks_copy(&s->items[i].marks); }
    return out;
}
static bool source_valid(const Analysis *a,char **error) {
    if (!a || (unsigned)a->kind<SOURCE_CSV || (unsigned)a->kind>SOURCE_DEMO ||
        (a->kind!=SOURCE_DEMO && (!a->path || !*a->path)) || (a->path && strlen(a->path)>65536) ||
        (a->schema && strlen(a->schema)>4096) || (a->table && strlen(a->table)>4096) ||
        (a->group_column && strlen(a->group_column)>4096) || (a->measure_column && strlen(a->measure_column)>4096) ||
        (unsigned)a->filter_op>FILTER_OP_MAX || (unsigned)a->aggregate>AGG_MAX)
        return fail(error,"Choose a valid table source.");
    return analysis_query_valid(a,error) && analysis_filters_valid(a,error) && analysis_sorts_valid(a,error);
}
const SheetObject *objects_source(const ObjectStore *s,const SheetObject *o) {
    if (!o) return NULL;
    if (o->kind==OBJECT_TABLE) return o;
    for (size_t i=0;o->source_id && i<s->count;i++) if (s->items[i].id==o->source_id && s->items[i].kind==OBJECT_TABLE) return &s->items[i];
    return NULL;
}
static bool add_text_bytes(size_t *bytes,const char *text) {
    if (!text) return true;
    size_t n=strlen(text);
    if (n==SIZE_MAX || *bytes>SIZE_MAX-(n+1)) return false;
    *bytes+=n+1; return true;
}
size_t objects_snapshot_owned_bytes(const ObjectSnapshot *s) {
    if (!s || s->count>DX_MAX_OBJECTS || (s->count && !s->items)) return SIZE_MAX;
    size_t n=s->count*sizeof(SheetObject);
    for (size_t i=0;i<s->count;i++) {
        const SheetObject *o=&s->items[i]; const Analysis *a=&o->source;
        if (!o->name || !add_text_bytes(&n,o->name) || a->filter_count>=DX_ANALYSIS_FILTERS || a->sort_count>=DX_ANALYSIS_SORTS) return SIZE_MAX;
        const char *texts[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
        for (size_t j=0;j<sizeof(texts)/sizeof(*texts);j++) if (!add_text_bytes(&n,texts[j])) return SIZE_MAX;
        for (size_t j=0;j<a->filter_count;j++) if (!a->filters[j].column || !add_text_bytes(&n,a->filters[j].column) || !add_text_bytes(&n,a->filters[j].value)) return SIZE_MAX;
        for (size_t j=0;j<a->sort_count;j++) if (!a->sorts[j].column || !add_text_bytes(&n,a->sorts[j].column)) return SIZE_MAX;
        const PivotDefinition *p=&o->pivot;
        for (int r=0;r<4;r++) {
            if (p->counts[r]>DX_PIVOT_FIELDS) return SIZE_MAX;
            for (size_t j=0;j<p->counts[r];j++) if (!p->fields[r][j].name || !add_text_bytes(&n,p->fields[r][j].name) || !add_text_bytes(&n,p->fields[r][j].value)) return SIZE_MAX;
        }
        for (int r=0;r<DX_MARK_ROLES;r++) {
            if (o->marks.counts[r]>DX_MARK_FIELDS) return SIZE_MAX;
            for (size_t j=0;j<o->marks.counts[r];j++) if (!o->marks.fields[r][j].name || !add_text_bytes(&n,o->marks.fields[r][j].name)) return SIZE_MAX;
        }
    }
    return n;
}
static size_t bytes(const ObjectSnapshot *s) { return objects_snapshot_owned_bytes(s); }
bool objects_valid(const ObjectStore *s,char **error) {
    if (s->count>DX_MAX_OBJECTS || (s->count && !s->items)) return fail(error,"Too many sheet objects.");
    for (size_t i=0;i<s->count;i++) {
        const SheetObject *o=&s->items[i];
        for (size_t j=0;j<i;j++) if (s->items[j].id==o->id) return fail(error,"Duplicate sheet object ID.");
        if (!o->id || o->id>s->next_id) return fail(error,"Invalid sheet object ID allocation high-water mark.");
        if (!o->name || !*o->name || strlen(o->name)>4096 || (unsigned)o->kind>OBJECT_TABLE || (unsigned)o->mark>MARK_SCATTER || (o->kind!=OBJECT_CHART && o->mark!=MARK_BAR) || (o->chart_measure && o->chart_measure>=o->pivot.counts[PIVOT_VALUES]) || !pivot_valid(&o->pivot,error) || !marks_valid(&o->marks,error)) return fail(error,"Invalid sheet object definition.");
        if (o->kind==OBJECT_TABLE) {
            if (marks_text_bytes(&o->marks) || o->source_id || !source_valid(&o->source,error)) return fail(error,"Invalid table source definition.");
            for (int r=0;r<4;r++) if (o->pivot.counts[r]) return fail(error,"A table cannot contain pivot shelves.");
        } else if (o->source.kind!=SOURCE_NONE || o->source.unique_rows || analysis_text_bytes(&o->source) || (o->source_id && !objects_source(s,o))) return fail(error,"Missing or invalid source table binding.");
    }
    ObjectSnapshot current={s->items,s->count};
    return bytes(&current)<=DX_OBJECT_CURRENT_BYTES || fail(error,"Sheet object definitions exceed 8 MiB.");
}
static void clear(ObjectSnapshot *stack,size_t *count) { while (*count) objects_snapshot_free(&stack[--*count]); }
static void push(ObjectSnapshot *stack,size_t *count,ObjectSnapshot state) {
    size_t size=bytes(&state); for (size_t i=0;i<*count;i++) size+=bytes(&stack[i]);
    while (*count && (*count==32 || size>DX_OBJECT_HISTORY_BYTES)) { size-=bytes(&stack[0]); objects_snapshot_free(&stack[0]); memmove(stack,stack+1,(--*count)*sizeof(*stack)); }
    stack[(*count)++]=state;
}
static void remember(ObjectStore *s) { clear(s->redo,&s->redo_count); push(s->undo,&s->undo_count,snapshot(s)); }
void objects_free(ObjectStore *s) {
    ObjectSnapshot data={s->items,s->count}; objects_snapshot_free(&data); clear(s->undo,&s->undo_count); clear(s->redo,&s->redo_count); memset(s,0,sizeof(*s));
}
void objects_copy(ObjectStore *to,const ObjectStore *from) {
    uint64_t next=from->next_id,titles=from->title_revision,marks=from->marks_revision; ObjectSnapshot copy=snapshot(from);
    objects_free(to); to->items=copy.items; to->count=copy.count; to->next_id=next; to->title_revision=titles; to->marks_revision=marks;
}
int objects_add(ObjectStore *s,uint64_t row,size_t col,const char *name,char **error) {
    if (s->next_id==UINT64_MAX) { fail(error,"This worksheet has exhausted its object IDs."); return -1; }
    if (s->count==DX_MAX_OBJECTS || row>DX_SHEET_ROWS-5 || col>16384-4 || !name || !*name || strlen(name)>4096) { fail(error,"Choose a destination inside the sheet; use up to 32 objects per sheet."); return -1; }
    remember(s); s->items=dx_realloc(s->items,(s->count+1)*sizeof(SheetObject));
    s->items[s->count]=(SheetObject){.id=++s->next_id,.row=row,.col=col,.rows=5,.columns=4,.revision=1,.name=dx_strdup(name)};
    return (int)s->count++;
}
int objects_add_chart(ObjectStore *s,uint64_t row,size_t col,const char *name,const PivotDefinition *definition,char **error) {
    if (row>DX_SHEET_ROWS-12 || col>16384-6) { if (!error || !*error) fail(error,"Choose space for a chart inside the sheet."); return -1; }
    char *ignored_error=NULL;
    bool valid=pivot_valid(definition,error ? error : &ignored_error); free(ignored_error);
    if (!valid) return -1;
    SheetObject candidate={.name=(char *)name,.pivot=*definition}; ObjectSnapshot current={s->items,s->count},add={&candidate,1};
    if (!name || bytes(&current)+bytes(&add)>DX_OBJECT_CURRENT_BYTES) { fail(error,"Sheet object definitions exceed 8 MiB."); return -1; }
    int index=objects_add(s,row,col,name,error); if (index<0) return index;
    SheetObject *o=&s->items[index]; o->kind=OBJECT_CHART; o->rows=12; o->columns=6; o->pivot=pivot_copy(definition); return index;
}
int objects_add_table(ObjectStore *s,uint64_t row,size_t col,const char *name,const Analysis *source,char **error) {
    if (!source_valid(source,error)) return -1;
    SheetObject candidate={.name=(char *)name,.source=*source}; ObjectSnapshot current={s->items,s->count},add={&candidate,1};
    if (!name || bytes(&current)+bytes(&add)>DX_OBJECT_CURRENT_BYTES) { fail(error,"Sheet object definitions exceed 8 MiB."); return -1; }
    Analysis copy=analysis_copy(source); char *owned=dx_strdup(name);
    int index=objects_add(s,row,col,owned,error); free(owned);
    if (index<0) { analysis_free(&copy); return index; }
    s->items[index].kind=OBJECT_TABLE; s->items[index].source=copy; return index;
}
bool objects_bind_source(ObjectStore *s,size_t index,uint64_t id,char **error) {
    if (index>=s->count || s->items[index].kind==OBJECT_TABLE) return fail(error,"Choose a pivot or chart to bind to a source.");
    SheetObject candidate=s->items[index]; candidate.source_id=id;
    if (id && !objects_source(s,&candidate)) return fail(error,"The source table is missing.");
    if (s->items[index].source_id==id) return true;
    remember(s); s->items[index].source_id=id; s->items[index].revision++; return true;
}
bool objects_set_source(ObjectStore *s,size_t index,const Analysis *source,char **error) {
    if (index>=s->count || s->items[index].kind!=OBJECT_TABLE || !source_valid(source,error)) return fail(error,"Choose a valid table source.");
    const Analysis *old=&s->items[index].source;
    bool same_result=analysis_same_result(old,source),same=same_result;
    for (size_t i=0;same && i<old->filter_count+(old->filter_column ? 1 : 0);i++) {
        const char *a=i ? old->filters[i-1].column : old->filter_column,*b=i ? source->filters[i-1].column : source->filter_column;
        same=a && b && !strcmp(a,b);
    }
    if (same) return true;
    ObjectSnapshot current={s->items,s->count};
    if (bytes(&current)-analysis_text_bytes(old)+analysis_text_bytes(source)>DX_OBJECT_CURRENT_BYTES) return fail(error,"Sheet object definitions exceed 8 MiB.");
    Analysis copy=analysis_copy(source); remember(s); analysis_free(&s->items[index].source); s->items[index].source=copy;
    if (!same_result) s->items[index].revision++; return true;
}
static bool plan_snapshot_valid(const ObjectSnapshot *state,uint64_t next_id,char **error) {
    if (objects_snapshot_owned_bytes(state)==SIZE_MAX) return fail(error,"Invalid owned sheet object snapshot.");
    ObjectStore model={.items=state->items,.count=state->count,.next_id=next_id};
    return objects_valid(&model,error);
}
static bool plan_history_valid(const ObjectSnapshot *states,size_t count,uint64_t next_id,size_t *owned,char **error) {
    *owned=0;
    for (size_t i=0;i<count;i++) {
        if (!plan_snapshot_valid(&states[i],next_id,error)) return false;
        size_t n=objects_snapshot_owned_bytes(&states[i]);
        if (n>DX_OBJECT_HISTORY_BYTES-*owned) return fail(error,"Sheet object history exceeds 8 MiB.");
        *owned+=n;
    }
    return true;
}
bool objects_plan_source_version(const ObjectStore *s,uint64_t owner,uint64_t expected,uint64_t next,ObjectSnapshot *out,bool *changed,char **error) {
    if (!s || !out || !changed || !owner || !next) return fail(error,"Choose an issued source version and source table.");
    if (s->undo_count>32 || s->redo_count>32) return fail(error,"Invalid sheet object history.");
    ObjectSnapshot current={s->items,s->count}; size_t undo_bytes=0,redo_bytes=0;
    if (!plan_snapshot_valid(&current,s->next_id,error) ||
        !plan_history_valid(s->undo,s->undo_count,s->next_id,&undo_bytes,error) ||
        !plan_history_valid(s->redo,s->redo_count,s->next_id,&redo_bytes,error)) return false;
    size_t index=0; while (index<s->count && s->items[index].id!=owner) index++;
    if (index==s->count || s->items[index].kind!=OBJECT_TABLE) return fail(error,"The source table is missing.");
    if (s->items[index].source.source_version!=expected) return fail(error,"The source table binding changed before formatting.");
    if (next==expected) { *changed=false; return true; }
    if (s->undo_count==32 || objects_snapshot_owned_bytes(&current)>DX_OBJECT_HISTORY_BYTES-undo_bytes)
        return fail(error,"The source binding cannot fit sheet object history without pruning another action.");
    ObjectSnapshot planned=snapshot(s); planned.items[index].source.source_version=next;
    objects_snapshot_free(out); *out=planned; *changed=true; return true;
}
void objects_adopt_source_version(ObjectStore *s,ObjectSnapshot *owned) {
    assert(s && owned && owned->items && s->undo_count<32);
    clear(s->redo,&s->redo_count);
    s->undo[s->undo_count++]=(ObjectSnapshot){s->items,s->count};
    s->items=owned->items; s->count=owned->count; *owned=(ObjectSnapshot){0};
}
bool objects_resolve_source(ObjectStore *s,size_t index,const Analysis *resolved,char **error) {
    if (index>=s->count || s->items[index].kind!=OBJECT_TABLE || !resolved) return fail(error,"Choose a valid source table.");
    Analysis *source=&s->items[index].source;
    if (source->kind!=SOURCE_DATABASE || resolved->kind!=SOURCE_DATABASE || !source->path || !resolved->path || strcmp(source->path,resolved->path)) return fail(error,"The resolved table belongs to a different source.");
    if (source->table) return true;
    if (!resolved->schema || !*resolved->schema || !resolved->table || !*resolved->table) return fail(error,"The source has no resolved table or view.");
    Analysis candidate=*source; candidate.schema=resolved->schema; candidate.table=resolved->table;
    if (!source_valid(&candidate,error)) return false;
    ObjectSnapshot current={s->items,s->count};
    if (bytes(&current)-analysis_text_bytes(source)+analysis_text_bytes(&candidate)>DX_OBJECT_CURRENT_BYTES) return fail(error,"Sheet object definitions exceed 8 MiB.");
    char *schema=dx_strdup(candidate.schema),*table=dx_strdup(candidate.table);
    free(source->schema); source->schema=schema; source->table=table; return true;
}
bool objects_update(ObjectStore *s,size_t index,const PivotDefinition *definition,char **error) {
    if (index>=s->count || s->items[index].kind==OBJECT_TABLE || !pivot_valid(definition,error)) return false;
    const PivotDefinition *old=&s->items[index].pivot; bool same=true;
    for (int r=0;r<4;r++) {
        if (old->counts[r]!=definition->counts[r]) { same=false; break; }
        for (size_t i=0;i<old->counts[r];i++) {
            const PivotField *a=&old->fields[r][i],*b=&definition->fields[r][i];
            if (strcmp(a->name,b->name) || a->aggregate!=b->aggregate || a->grain!=b->grain || a->filter!=b->filter || (a->value && b->value ? strcmp(a->value,b->value)!=0 : a->value!=b->value)) same=false;
        }
    }
    if (same) return true;
    SheetObject candidate=s->items[index]; candidate.pivot=*definition;
    ObjectSnapshot current={s->items,s->count},before={&s->items[index],1},after={&candidate,1};
    if (bytes(&current)-bytes(&before)+bytes(&after)>DX_OBJECT_CURRENT_BYTES) return fail(error,"Pivot definitions are limited to 8 MiB per sheet. Narrow the value selection.");
    size_t selected=s->items[index].chart_measure;
    if (selected<old->counts[PIVOT_VALUES]) {
        const PivotField *value=&old->fields[PIVOT_VALUES][selected]; bool found=false;
        for (size_t i=0;i<definition->counts[PIVOT_VALUES];i++) if (!strcmp(value->name,definition->fields[PIVOT_VALUES][i].name) && value->aggregate==definition->fields[PIVOT_VALUES][i].aggregate) { selected=i; found=true; break; }
        /* An aggregate edit changes the identity but keeps the same measure
           slot. A removed assignment must not select an unrelated field. */
        if (!found) selected=selected<definition->counts[PIVOT_VALUES] && !strcmp(value->name,definition->fields[PIVOT_VALUES][selected].name) ? selected : 0;
    } else selected=0;
    PivotDefinition copy=pivot_copy(definition); remember(s); pivot_free(&s->items[index].pivot); s->items[index].pivot=copy; s->items[index].chart_measure=(unsigned char)selected; s->items[index].revision++; return true;
}
bool objects_mark(ObjectStore *s,size_t index,ChartMark mark,char **error) {
    if (index>=s->count || s->items[index].kind!=OBJECT_CHART || (unsigned)mark>MARK_SCATTER) return fail(error,"Choose a valid chart mark.");
    if (s->items[index].mark==mark) return true;
    remember(s); s->items[index].mark=mark; return true;
}
bool objects_marks(ObjectStore *s,size_t index,const MarkDefinition *marks,char **error) {
    if (index>=s->count || s->items[index].kind==OBJECT_TABLE || !marks_valid(marks,error)) return fail(error,"Choose an analysis view for Marks.");
    if (marks_equal(&s->items[index].marks,marks)) return true;
    ObjectSnapshot current={s->items,s->count};
    if (bytes(&current)-marks_text_bytes(&s->items[index].marks)+marks_text_bytes(marks)>DX_OBJECT_CURRENT_BYTES) return fail(error,"Sheet object definitions exceed 8 MiB.");
    MarkDefinition copy=marks_copy(marks); remember(s); marks_free(&s->items[index].marks); s->items[index].marks=copy; s->marks_revision++; return true;
}
bool objects_chart_measure(ObjectStore *s,size_t index,size_t measure,char **error) {
    if (index>=s->count || s->items[index].kind==OBJECT_TABLE || measure>=s->items[index].pivot.counts[PIVOT_VALUES]) return fail(error,"Choose a measure assigned to this view.");
    if (s->items[index].chart_measure==measure) return true;
    remember(s); s->items[index].chart_measure=(unsigned char)measure; s->title_revision++; return true;
}
bool objects_representation(ObjectStore *s,size_t index,ObjectKind kind,ChartMark mark,uint64_t rows,size_t columns,
    const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error) {
    if (index>=s->count || s->items[index].kind==OBJECT_TABLE || (kind!=OBJECT_PIVOT && kind!=OBJECT_CHART) ||
        (unsigned)mark>MARK_SCATTER || (kind==OBJECT_PIVOT && mark!=MARK_BAR)) return fail(error,"Choose a matrix or chart representation.");
    if ((kind==OBJECT_CHART && (rows<8 || rows>4096 || columns<3 || columns>256)) || !rows || !columns)
        return fail(error,"The representation does not fit a valid worksheet extent.");
    SheetObject *o=&s->items[index];
    if (o->kind==kind && o->mark==mark) return true;
    if (!objects_fits(s,(int)index,o->row,o->col,rows,columns,cells,source_rows,source_columns,error)) return false;
    remember(s); o->kind=kind; o->mark=mark; o->rows=rows; o->columns=columns; return true;
}
static bool names_equal(const char *a,const char *b) {
    while (*a && *b) { unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++; if (x>='A' && x<='Z') x+=32; if (y>='A' && y<='Z') y+=32; if (x!=y) return false; }
    return *a==*b;
}
bool objects_rename(ObjectStore *s,size_t index,const char *name,char **error) {
    if (index>=s->count || !name) return fail(error,"Choose a pivot or chart to rename.");
    while (isspace((unsigned char)*name)) name++;
    size_t n=strlen(name); while (n && isspace((unsigned char)name[n-1])) n--;
    if (!n || n>4096) return fail(error,"Names must contain 1–4096 bytes of text.");
    for (size_t i=0;i<n;i++) if ((unsigned char)name[i]<32 || name[i]==127) return fail(error,"Names cannot contain control characters.");
    char *owned=dx_alloc(n+1); memcpy(owned,name,n);
    for (size_t i=0;i<s->count;i++) if (i!=index && names_equal(owned,s->items[i].name)) { free(owned); return fail(error,"Another pivot or chart already has that name."); }
    if (!strcmp(owned,s->items[index].name)) { free(owned); return true; }
    ObjectSnapshot current={s->items,s->count};
    if (bytes(&current)-strlen(s->items[index].name)+n>DX_OBJECT_CURRENT_BYTES) { free(owned); return fail(error,"Sheet object definitions exceed 8 MiB."); }
    remember(s); free(s->items[index].name); s->items[index].name=owned; s->title_revision++; return true;
}
bool objects_remove(ObjectStore *s,size_t index) {
    if (index>=s->count) return false;
    for (size_t i=0;i<s->count;i++) if (s->items[i].source_id==s->items[index].id) return false;
    remember(s); free(s->items[index].name); pivot_free(&s->items[index].pivot); analysis_free(&s->items[index].source); marks_free(&s->items[index].marks);
    memmove(s->items+index,s->items+index+1,(--s->count-index)*sizeof(SheetObject)); return true;
}
bool objects_history(ObjectStore *s,bool redo) {
    ObjectSnapshot *from=redo ? s->redo : s->undo,*to=redo ? s->undo : s->redo;
    size_t *fc=redo ? &s->redo_count : &s->undo_count,*tc=redo ? &s->undo_count : &s->redo_count;
    if (!*fc) return false;
    ObjectSnapshot state=from[*fc-1]; bool names=s->count!=state.count,marks=names;
    for (size_t i=0;!names && i<s->count;i++) names=s->items[i].id!=state.items[i].id || strcmp(s->items[i].name,state.items[i].name);
    for (size_t i=0;!marks && i<s->count;i++) marks=s->items[i].id!=state.items[i].id || !marks_equal(&s->items[i].marks,&state.items[i].marks);
    push(to,tc,(ObjectSnapshot){s->items,s->count}); --*fc; s->items=state.items; s->count=state.count;
    if (names) s->title_revision++; if (marks) s->marks_revision++; return true;
}
void objects_trim_history(ObjectStore *s,bool redo,size_t keep) {
    ObjectSnapshot *stack=redo ? s->redo : s->undo; size_t *count=redo ? &s->redo_count : &s->undo_count;
    if (keep>=*count) return;
    size_t drop=*count-keep; for (size_t i=0;i<drop;i++) objects_snapshot_free(&stack[i]);
    memmove(stack,stack+drop,keep*sizeof(*stack)); memset(stack+keep,0,drop*sizeof(*stack)); *count=keep;
}
void objects_translate_mask(ObjectStore *s,uint32_t mask,int64_t dr,int dc) {
    if (!mask || (!dr && !dc)) return;
    remember(s);
    for (size_t i=0;i<s->count;i++) if (mask & (UINT32_C(1)<<i)) {
        s->items[i].row=(uint64_t)((int64_t)s->items[i].row+dr); s->items[i].col=(size_t)((int)s->items[i].col+dc);
    }
}
int objects_hit(const ObjectStore *s,uint64_t row,size_t col) {
    for (size_t i=0;i<s->count;i++) { const SheetObject *o=&s->items[i]; if (row>=o->row && row-o->row<o->rows && col>=o->col && col-o->col<o->columns) return (int)i; }
    return -1;
}
SheetRange objects_region(const ObjectStore *s,const CellStore *cells,uint64_t source_rows,size_t source_columns,uint64_t row,size_t col,bool whole) {
    SheetRange range={row,row,col,col}; int hit=objects_hit(s,row,col);
    if (!whole && hit>=0) { const SheetObject *o=&s->items[hit]; return (SheetRange){o->row,o->row+o->rows-1,o->col,o->col+o->columns-1}; }
    if (!whole && row<source_rows && col<source_columns) return (SheetRange){0,source_rows-1,0,source_columns-1};
    cells_region(cells,row,col,&range.r0,&range.r1,&range.c0,&range.c1,whole);
    if (!whole) return range;
    bool populated=cells->count!=0;
    if (source_rows && source_columns) { if (!populated) range=(SheetRange){0,source_rows-1,0,source_columns-1}; else { range.r0=0; range.c0=0; if (range.r1<source_rows-1) range.r1=source_rows-1; if (range.c1<source_columns-1) range.c1=source_columns-1; } populated=true; }
    for (size_t i=0;i<s->count;i++) {
        const SheetObject *o=&s->items[i]; SheetRange add={o->row,o->row+o->rows-1,o->col,o->col+o->columns-1};
        if (!populated) { range=add; populated=true; continue; }
        if (add.r0<range.r0) range.r0=add.r0; if (add.r1>range.r1) range.r1=add.r1;
        if (add.c0<range.c0) range.c0=add.c0; if (add.c1>range.c1) range.c1=add.c1;
    }
    return range;
}
static bool overlap(uint64_t r,size_t c,uint64_t h,size_t w,const SheetObject *b) { return r<b->row+b->rows && b->row<r+h && c<b->col+b->columns && b->col<c+w; }
bool objects_fits(const ObjectStore *s,int ignore,uint64_t row,size_t col,uint64_t rows,size_t columns,const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error) {
    if (!rows || !columns || rows>DX_SHEET_ROWS || columns>16384 || row>DX_SHEET_ROWS-rows || col>16384-columns) return fail(error,"The object would extend beyond the worksheet.");
    if (row<source_rows && col<source_columns) return fail(error,"That range contains source data. Choose an empty range beside it.");
    for (size_t i=0;i<s->count;i++) if ((int)i!=ignore && overlap(row,col,rows,columns,&s->items[i])) return fail(error,"That range overlaps another sheet object. Move it to an empty range.");
    for (size_t i=0;cells && i<cells->count;i++) { const SheetCell *cell=&cells->cells[i]; if (cell->row>=row && cell->row-row<rows && cell->col>=col && cell->col-col<columns) return fail(error,"That range contains manual cells. Choose an empty destination."); }
    return true;
}
bool objects_move(ObjectStore *s,size_t index,uint64_t row,size_t col,const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error) {
    if (index>=s->count) return false; SheetObject *o=&s->items[index];
    if (!objects_fits(s,(int)index,row,col,o->rows,o->columns,cells,source_rows,source_columns,error)) return false;
    if (o->row==row && o->col==col) return true;
    remember(s); o->row=row; o->col=col; return true;
}
bool objects_resize_chart(ObjectStore *s,size_t index,uint64_t rows,size_t columns,const CellStore *cells,uint64_t source_rows,size_t source_columns,char **error) {
    if (index>=s->count || s->items[index].kind!=OBJECT_CHART) return fail(error,"Choose a chart to resize.");
    if (rows<8 || rows>4096 || columns<3 || columns>256) return fail(error,"Use 8–4096 rows and 3–256 columns for a chart.");
    SheetObject *o=&s->items[index];
    if (!objects_fits(s,(int)index,o->row,o->col,rows,columns,cells,source_rows,source_columns,error)) return false;
    if (o->rows==rows && o->columns==columns) return true;
    remember(s); o->rows=rows; o->columns=columns; return true;
}
