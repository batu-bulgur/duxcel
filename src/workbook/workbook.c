#include "workbook.h"
#include "workbook_codec.h"
#include "history.h"
#include "lifecycle.h"
#include "../platform/platform.h"
#ifndef DUXCEL_BROWSER_HOST
#include <duckdb.h>
#include "save.h"
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
static void history_lost(Worksheet *s,bool redo,uint64_t order) {
    if (redo) {
        if (order && (!s->history_ceiling || order<s->history_ceiling)) s->history_ceiling=order;
    } else if (order>s->history_floor) s->history_floor=order;
}
static void action_push(Worksheet *s,EditKind *actions,ColumnEdit *columns,uint64_t *orders,size_t *count,EditKind kind,ColumnEdit column,uint64_t order) {
    if (*count==DX_HISTORY) { history_lost(s,orders==s->redo_order,orders[0]); memmove(orders,orders+1,(DX_HISTORY-1)*sizeof(*orders)); memmove(actions,actions+1,(DX_HISTORY-1)*sizeof(*actions)); memmove(columns,columns+1,(DX_HISTORY-1)*sizeof(*columns)); --*count; }
    orders[*count]=order; columns[*count]=column; actions[(*count)++]=kind;
}
unsigned worksheet_edit_effects(EditKind kind) { static const unsigned effects[]={1,2,4,6,8,10,12,14,9}; return (unsigned)kind<sizeof(effects)/sizeof(*effects) ? effects[kind] : 0; }
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
    uint64_t *orders=redo ? s->redo_order : s->undo_order;
    for (size_t i=0;i<drop;i++) history_lost(s,redo,orders[i]);
    memmove(orders,orders+drop,(*n-drop)*sizeof(*orders)); memset(orders+*n-drop,0,drop*sizeof(*orders));
    memmove(actions,actions+drop,(*n-drop)*sizeof(*actions)); memmove(columns,columns+drop,(*n-drop)*sizeof(*columns)); memset(columns+*n-drop,0,drop*sizeof(*columns)); *n-=drop;
    Analysis *stack=redo ? s->redo : s->undo; size_t *count=redo ? &s->redo_count : &s->undo_count;
    drop=*count-required[0]; for (size_t i=0;i<drop;i++) analysis_free(&stack[i]);
    memmove(stack,stack+drop,required[0]*sizeof(*stack)); memset(stack+required[0],0,drop*sizeof(*stack)); *count=required[0];
    cells_trim_history(&s->cells,redo,required[1]); objects_trim_history(&s->objects,redo,required[2]);
    format_trim_history(&s->formats,redo,required[3]);
}
void worksheet_record(Worksheet *s,EditKind kind) {
    s->history_last=worksheet_history_issue_order();
    action_push(s,s->undo_actions,s->undo_columns,s->undo_order,&s->undo_actions_count,kind,(ColumnEdit){0},s->history_last); s->redo_actions_count=0; memset(s->redo_columns,0,sizeof(s->redo_columns)); action_prune(s,false); action_prune(s,true);
}
void worksheet_transfer_history(Worksheet *s,bool redo) {
    size_t *from=redo ? &s->redo_actions_count : &s->undo_actions_count,*to=redo ? &s->undo_actions_count : &s->redo_actions_count;
    EditKind *a=redo ? s->redo_actions : s->undo_actions,*b=redo ? s->undo_actions : s->redo_actions;
    ColumnEdit *ca=redo ? s->redo_columns : s->undo_columns,*cb=redo ? s->undo_columns : s->redo_columns;
    uint64_t *oa=redo ? s->redo_order : s->undo_order,*ob=redo ? s->undo_order : s->redo_order;
    if (*from) { --*from; action_push(s,b,cb,ob,to,a[*from],ca[*from],oa[*from]); ca[*from]=(ColumnEdit){0}; oa[*from]=0; }
    action_prune(s,false); action_prune(s,true);
}
static bool range_fail(char **error,const char *message) { if (error) error_set(error,message); return false; }
#define ANALYSIS_OWNED_TEXTS (9+2*(DX_ANALYSIS_FILTERS-1)+(DX_ANALYSIS_SORTS-1))
typedef struct { uintptr_t first,last; } AnalysisOwnedRange;
typedef struct {
    const Worksheet *sheet; const Analysis *live,*next;
    AnalysisOwnedRange ranges[ANALYSIS_OWNED_TEXTS*(2*DX_HISTORY+3)]; size_t count;
} AnalysisOwnership;
static bool owned_overlap(uintptr_t a,size_t an,uintptr_t b,size_t bn) {
    return an && bn && (a<=b ? b-a<an : a-b<bn);
}
static bool analysis_location(const Worksheet *s,const Analysis *a,bool live,char **error) {
    if (!a || (owned_overlap((uintptr_t)s,sizeof(*s),(uintptr_t)a,sizeof(*a)) && (!live || a!=&s->analysis))) return range_fail(error,"Use independent owned Analysis locations outside worksheet history.");
    /* A table's source belongs to ObjectStore history, not primary Analysis
       history. Reject known owned object locations without guessing capacities. */
    const ObjectStore *objects=&s->objects;
    if (objects->count>DX_MAX_OBJECTS || (objects->count && !objects->items)) return range_fail(error,"Invalid object ownership in Analysis history.");
    if (owned_overlap((uintptr_t)objects->items,objects->count*sizeof(*objects->items),(uintptr_t)a,sizeof(*a))) return range_fail(error,"A table source cannot be adopted into primary Analysis history.");
    for (size_t h=0;h<2;h++) {
        const ObjectSnapshot *states=h ? objects->redo : objects->undo; size_t count=h ? objects->redo_count : objects->undo_count;
        for (size_t i=0;i<count;i++) {
            if (states[i].count>DX_MAX_OBJECTS || (states[i].count && !states[i].items)) return range_fail(error,"Invalid historical object ownership.");
            if (owned_overlap((uintptr_t)states[i].items,states[i].count*sizeof(*states[i].items),(uintptr_t)a,sizeof(*a))) return range_fail(error,"An object history source is not a prepared Analysis.");
        }
    }
    return true;
}
static bool analysis_text_owned(AnalysisOwnership *owned,const char *text,size_t *bytes,char **error) {
    if (!text) return true;
    size_t room=DX_ANALYSIS_HISTORY_BYTES-*bytes,n=0;
    while (n<room && text[n]) n++;
    if (n==room) return range_fail(error,"An Analysis exceeds its 8 MiB owned limit.");
    size_t length=n+1; uintptr_t begin=(uintptr_t)text;
    if (begin>UINTPTR_MAX-length || owned->count>=sizeof(owned->ranges)/sizeof(*owned->ranges) ||
        owned_overlap(begin,length,(uintptr_t)owned->sheet,sizeof(*owned->sheet)) ||
        owned_overlap(begin,length,(uintptr_t)owned->live,sizeof(*owned->live)) ||
        (owned->next && owned_overlap(begin,length,(uintptr_t)owned->next,sizeof(*owned->next)))) return range_fail(error,"Analysis text must own storage separate from its model locations.");
    owned->ranges[owned->count++]=(AnalysisOwnedRange){begin,begin+length}; *bytes+=length; return true;
}
static bool analysis_owned_storage(AnalysisOwnership *owned,const Analysis *a,size_t *out,char **error) {
    if (a->filter_count>=DX_ANALYSIS_FILTERS || a->sort_count>=DX_ANALYSIS_SORTS || (unsigned)a->kind>SOURCE_DEMO || (unsigned)a->filter_op>FILTER_OP_MAX || (unsigned)a->aggregate>AGG_MAX) return range_fail(error,"Invalid bounded Analysis definition.");
    size_t bytes=sizeof(*a); const char *texts[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
    for (size_t i=0;i<sizeof(texts)/sizeof(*texts);i++) if (!analysis_text_owned(owned,texts[i],&bytes,error)) return false;
    for (size_t i=0;i<DX_ANALYSIS_FILTERS-1;i++) {
        if (i>=a->filter_count) { if (a->filters[i].column || a->filters[i].value) return range_fail(error,"Unused Analysis filters cannot retain owned text."); }
        else if (!analysis_text_owned(owned,a->filters[i].column,&bytes,error) || !analysis_text_owned(owned,a->filters[i].value,&bytes,error)) return false;
    }
    for (size_t i=0;i<DX_ANALYSIS_SORTS-1;i++) {
        if (i>=a->sort_count) { if (a->sorts[i].column) return range_fail(error,"Unused Analysis sorts cannot retain owned text."); }
        else if (!analysis_text_owned(owned,a->sorts[i].column,&bytes,error)) return false;
    }
    *out=bytes; return true;
}
static bool analysis_owned_valid(AnalysisOwnership *owned,const Analysis *a,size_t *out,char **error) {
    return analysis_owned_storage(owned,a,out,error) && analysis_query_valid(a,error) && analysis_filters_valid(a,error) && analysis_sorts_valid(a,error);
}
static int analysis_range_compare(const void *a,const void *b) {
    const AnalysisOwnedRange *x=a,*y=b; return x->first<y->first ? -1 : x->first>y->first;
}
static bool analysis_argument_room(const AnalysisOwnership *owned,const Analysis *a,char **error) {
    for (size_t i=0;i<owned->count;i++) if (owned_overlap((uintptr_t)a,sizeof(*a),owned->ranges[i].first,owned->ranges[i].last-owned->ranges[i].first)) return range_fail(error,"An Analysis argument overlaps retained text ownership.");
    return true;
}
static bool analysis_journal_valid(const Worksheet *s,AnalysisOwnership *owned,char **error) {
    if (!s || s->undo_count>DX_HISTORY || s->redo_count>DX_HISTORY || s->undo_actions_count>DX_HISTORY || s->redo_actions_count>DX_HISTORY || s->cells.undo_count>DX_HISTORY || s->cells.redo_count>DX_HISTORY || s->objects.undo_count>DX_HISTORY || s->objects.redo_count>DX_HISTORY || s->formats.undo_count>DX_HISTORY || s->formats.redo_count>DX_HISTORY) return range_fail(error,"Invalid bounded worksheet history counts.");
    if (!analysis_location(s,owned->live,true,error) || (owned->next && (!analysis_location(s,owned->next,false,error) || owned_overlap((uintptr_t)owned->live,sizeof(*owned->live),(uintptr_t)owned->next,sizeof(*owned->next))))) return range_fail(error,"Prepared Analysis and live ownership must be independent.");
    for (size_t h=0;h<2;h++) {
        const EditKind *actions=h ? s->redo_actions : s->undo_actions; size_t count=h ? s->redo_actions_count : s->undo_actions_count,required[4]={0};
        size_t available[]={h ? s->redo_count : s->undo_count,h ? s->cells.redo_count : s->cells.undo_count,h ? s->objects.redo_count : s->objects.undo_count,h ? s->formats.redo_count : s->formats.undo_count};
        for (size_t i=0;i<count;i++) { unsigned effects=worksheet_edit_effects(actions[i]); if (!effects) return range_fail(error,"Unknown worksheet history action."); for (size_t k=0;k<4;k++) required[k]+=!!(effects&(1u<<k)); }
        for (size_t k=0;k<4;k++) if (required[k]!=available[k]) return range_fail(error,"Worksheet journal and owned histories are not aligned.");
        const Analysis *states=h ? s->redo : s->undo; size_t bytes=0;
        for (size_t i=0;i<available[0];i++) { size_t entry=0; if (!analysis_owned_valid(owned,states+i,&entry,error)) return false; if (entry>DX_ANALYSIS_HISTORY_BYTES-bytes) return range_fail(error,"Analysis history exceeds its 8 MiB owned limit."); bytes+=entry; }
    }
    size_t ignored=0;
    if (owned->live!=&s->analysis && !analysis_owned_valid(owned,&s->analysis,&ignored,error)) return false;
    if (!analysis_argument_room(owned,owned->live,error) || !analysis_owned_valid(owned,owned->live,&ignored,error)) return false;
    if (owned->next && (!analysis_argument_room(owned,owned->next,error) || !analysis_owned_valid(owned,owned->next,&ignored,error))) return false;
    qsort(owned->ranges,owned->count,sizeof(*owned->ranges),analysis_range_compare);
    for (size_t i=1;i<owned->count;i++) if (owned->ranges[i-1].last>owned->ranges[i].first) return range_fail(error,"Analysis histories and prepared values must not share owned text.");
    return true;
}
static bool analysis_text_same(const char *a,const char *b) { return (!a || !b) ? a==b : !strcmp(a,b); }
static bool analysis_number_same(NumberType a,NumberType b) { return a.kind==b.kind && a.precision==b.precision && a.scale==b.scale; }
static bool analysis_owned_same(const Analysis *a,const Analysis *b) {
    if (a->kind!=b->kind || a->source_version!=b->source_version || a->unique_rows!=b->unique_rows || a->filter_op!=b->filter_op || a->aggregate!=b->aggregate || a->descending!=b->descending || a->filter_count!=b->filter_count || a->sort_count!=b->sort_count || !analysis_number_same(a->filter_number,b->filter_number)) return false;
    const char *x[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column},*y[]={b->path,b->schema,b->table,b->query,b->filter_column,b->filter_value,b->group_column,b->measure_column,b->sort_column};
    for (size_t i=0;i<sizeof(x)/sizeof(*x);i++) if (!analysis_text_same(x[i],y[i])) return false;
    for (size_t i=0;i<DX_ANALYSIS_FILTERS-1;i++) if (a->filters[i].op!=b->filters[i].op || !analysis_number_same(a->filters[i].number,b->filters[i].number) || !analysis_text_same(a->filters[i].column,b->filters[i].column) || !analysis_text_same(a->filters[i].value,b->filters[i].value)) return false;
    for (size_t i=0;i<DX_ANALYSIS_SORTS-1;i++) if (a->sorts[i].descending!=b->sorts[i].descending || !analysis_text_same(a->sorts[i].column,b->sorts[i].column)) return false;
    return true;
}
bool worksheet_analysis_preflight(const Worksheet *s,Analysis *live,Analysis *next,bool *changed,char **error) {
    AnalysisOwnership owned={.sheet=s,.live=live,.next=next};
    if (!next) return range_fail(error,"Prepare an independently owned next Analysis.");
    if (!analysis_journal_valid(s,&owned,error)) return false;
    if (changed) *changed=!analysis_owned_same(live,next); return true;
}
static void analysis_owned_push(Analysis *stack,size_t *count,Analysis *state) {
    size_t bytes=sizeof(*state)+analysis_text_bytes(state); for (size_t i=0;i<*count;i++) bytes+=sizeof(*stack)+analysis_text_bytes(stack+i);
    while (*count && (*count==DX_HISTORY || bytes>DX_ANALYSIS_HISTORY_BYTES)) {
        bytes-=sizeof(*stack)+analysis_text_bytes(stack); analysis_free(stack); memmove(stack,stack+1,(--*count)*sizeof(*stack)); memset(stack+*count,0,sizeof(*stack));
    }
    stack[(*count)++]=*state; *state=(Analysis){0};
}
void worksheet_analysis_adopt(Worksheet *s,Analysis *live,Analysis *next) {
    analysis_owned_push(s->undo,&s->undo_count,live);
    while (s->redo_count) analysis_free(s->redo+--s->redo_count);
    *live=*next; *next=(Analysis){0};
}
bool worksheet_analysis_history_preflight(const Worksheet *s,Analysis *live,bool redo,char **error) {
    AnalysisOwnership owned={.sheet=s,.live=live};
    if (!analysis_journal_valid(s,&owned,error)) return false;
    size_t count=redo ? s->redo_actions_count : s->undo_actions_count; const EditKind *actions=redo ? s->redo_actions : s->undo_actions;
    return (count && (worksheet_edit_effects(actions[count-1])&1)) || range_fail(error,"The next worksheet action has no Analysis state.");
}
void worksheet_analysis_history_adopt(Worksheet *s,Analysis *live,bool redo) {
    Analysis *from=redo ? s->redo : s->undo,*to=redo ? s->undo : s->redo; size_t *fc=redo ? &s->redo_count : &s->undo_count,*tc=redo ? &s->undo_count : &s->redo_count;
    Analysis restored=from[--*fc]; from[*fc]=(Analysis){0}; analysis_owned_push(to,tc,live); *live=restored;
}
static bool format_budget_add(size_t *value,size_t extra) {
    if (*value>SIZE_MAX-extra) return false; *value+=extra; return true;
}
static FormatBudgetStatus format_budget_analysis_history(const Worksheet *sheet,size_t *out) {
    bool composite=false;
    for (size_t h=0;h<2;h++) {
        const EditKind *actions=h ? sheet->redo_actions : sheet->undo_actions; size_t count=h ? sheet->redo_actions_count : sheet->undo_actions_count;
        for (size_t i=0;i<count && i<DX_HISTORY;i++) composite|=actions[i]==EDIT_ANALYSIS_FORMAT;
    }
    /* Ordinary Analysis ownership is baseline data. Only the new composite
       journal opts into this extra retained charge and ownership validation. */
    if (!composite) { *out=0; return FORMAT_BUDGET_OK; }
    if (sheet->undo_count>DX_HISTORY || sheet->redo_count>DX_HISTORY || sheet->undo_actions_count>DX_HISTORY || sheet->redo_actions_count>DX_HISTORY) return FORMAT_BUDGET_MODEL;
    AnalysisOwnership owned={.sheet=sheet,.live=&sheet->analysis}; size_t charge=0,ignored=0;
    if (!analysis_owned_storage(&owned,&sheet->analysis,&ignored,NULL)) return FORMAT_BUDGET_MODEL;
    for (size_t h=0;h<2;h++) {
        const EditKind *actions=h ? sheet->redo_actions : sheet->undo_actions; size_t count=h ? sheet->redo_actions_count : sheet->undo_actions_count;
        const Analysis *states=h ? sheet->redo : sheet->undo; size_t available=h ? sheet->redo_count : sheet->undo_count,index=0,bytes=0;
        for (size_t i=0;i<count;i++) {
            unsigned effects=worksheet_edit_effects(actions[i]); if (!effects) return FORMAT_BUDGET_MODEL;
            if (!(effects&1)) continue;
            if (index>=available) return FORMAT_BUDGET_MODEL;
            size_t entry=0; if (!analysis_owned_storage(&owned,states+index++,&entry,NULL) || entry>DX_ANALYSIS_HISTORY_BYTES-bytes) return FORMAT_BUDGET_MODEL;
            bytes+=entry;
            if (actions[i]==EDIT_ANALYSIS_FORMAT && !format_budget_add(&charge,entry)) return FORMAT_BUDGET_OVERFLOW;
        }
        if (index!=available) return FORMAT_BUDGET_MODEL;
    }
    qsort(owned.ranges,owned.count,sizeof(*owned.ranges),analysis_range_compare);
    for (size_t i=1;i<owned.count;i++) if (owned.ranges[i-1].last>owned.ranges[i].first) return FORMAT_BUDGET_MODEL;
    *out=charge; return FORMAT_BUDGET_OK;
}
static bool format_budget_object_bytes(const ObjectSnapshot *state,size_t *logical,size_t *reserved) {
    size_t bytes=objects_snapshot_owned_bytes(state); if (bytes==SIZE_MAX) return false;
    /* The shared helper validates shape before this checked subtraction. Its
       documented max32 reservation includes slack, not extra logical fields. */
    size_t slack=state->items ? (DX_MAX_OBJECTS-state->count)*sizeof(SheetObject) : 0;
    if (bytes<slack) return false;
    *logical=bytes-slack; *reserved=bytes; return true;
}
static FormatBudgetStatus format_budget_object_history(const Worksheet *sheet,const ObjectStore *objects,size_t *out) {
    bool composite=false;
    for (size_t h=0;h<2;h++) {
        const EditKind *actions=h ? sheet->redo_actions : sheet->undo_actions; size_t count=h ? sheet->redo_actions_count : sheet->undo_actions_count;
        for (size_t i=0;i<count && i<DX_HISTORY;i++) composite|=(worksheet_edit_effects(actions[i])&12)==12;
    }
    if (!composite) { *out=0; return FORMAT_BUDGET_OK; }
    if (objects->undo_count>DX_HISTORY || objects->redo_count>DX_HISTORY || sheet->undo_actions_count>DX_HISTORY || sheet->redo_actions_count>DX_HISTORY) return FORMAT_BUDGET_MODEL;
    ObjectSnapshot current={objects->items,objects->count}; size_t logical=0,reserved=0;
    if (!format_budget_object_bytes(&current,&logical,&reserved) || logical>DX_OBJECT_CURRENT_BYTES) return FORMAT_BUDGET_MODEL;
    size_t charge=0;
    for (size_t h=0;h<2;h++) {
        const EditKind *actions=h ? sheet->redo_actions : sheet->undo_actions; size_t count=h ? sheet->redo_actions_count : sheet->undo_actions_count;
        const ObjectSnapshot *states=h ? objects->redo : objects->undo; size_t available=h ? objects->redo_count : objects->undo_count,index=0,bytes=0;
        for (size_t i=0;i<count;i++) {
            unsigned effects=worksheet_edit_effects(actions[i]); if (!effects) return FORMAT_BUDGET_MODEL;
            if (!(effects&4)) continue;
            if (index>=available) return FORMAT_BUDGET_MODEL;
            if (!format_budget_object_bytes(states+index++,&logical,&reserved) || logical>DX_OBJECT_HISTORY_BYTES-bytes) return FORMAT_BUDGET_MODEL;
            bytes+=logical;
            if ((effects&12)==12 && !format_budget_add(&charge,reserved)) return FORMAT_BUDGET_OVERFLOW;
        }
        if (index!=available) return FORMAT_BUDGET_MODEL;
    }
    *out=charge; return FORMAT_BUDGET_OK;
}
static bool format_budget_model_valid(const FormatStore *store) {
    if (!format_valid(store,NULL)) return false;
    for (size_t h=0;h<2;h++) {
        const FormatSnapshot *states=h ? store->redo : store->undo; size_t count=h ? store->redo_count : store->undo_count;
        for (size_t i=0;i<count;i++) {
            const FormatSnapshot *state=states+i;
            FormatStore view={.items=state->items,.count=state->count,.scopes=state->scopes,.scope_count=state->scope_count,.source_aware=state->source_aware};
            if (!format_valid(&view,NULL)) return false;
        }
    }
    return true;
}
static bool legacy_cell_shape(const SheetCell *cells,size_t count,size_t bytes) {
    return count<=DX_MAX_EDIT_CELLS && bytes<=DX_CELL_BYTES && (!count || cells);
}
bool worksheet_legacy_format_history_eligible(const Worksheet *s,char **error) {
    if (!s || s->undo_actions_count>DX_HISTORY || s->redo_actions_count>DX_HISTORY ||
        s->cells.undo_count>DX_HISTORY || s->cells.redo_count>DX_HISTORY ||
        s->formats.undo_count>DX_FORMAT_HISTORY || s->formats.redo_count>DX_FORMAT_HISTORY)
        return range_fail(error,"Legacy formatting migration needs bounded worksheet history; reopen the unchanged workbook.");
    if (s->undo_count || s->redo_count || s->objects.undo_count || s->objects.redo_count)
        return range_fail(error,"Legacy formatting migration cannot bind retained Analysis or object history; reopen the unchanged workbook.");
    if (s->cells.count>s->cells.capacity || !legacy_cell_shape(s->cells.cells,s->cells.count,s->cells.bytes))
        return range_fail(error,"Legacy formatting migration found an invalid editable-cell array shape.");
    for (size_t h=0;h<2;h++) {
        const EditKind *actions=h ? s->redo_actions : s->undo_actions; size_t count=h ? s->redo_actions_count : s->undo_actions_count,cells=0,formats=0;
        for (size_t i=0;i<count;i++) {
            if (actions[i]!=EDIT_CELLS && actions[i]!=EDIT_FORMAT && actions[i]!=EDIT_CELLS_FORMAT)
                return range_fail(error,"Legacy formatting migration supports only retained Cell and Format actions; reopen the unchanged workbook.");
            cells+=actions[i]!=EDIT_FORMAT; formats+=actions[i]!=EDIT_CELLS;
        }
        size_t available_cells=h ? s->cells.redo_count : s->cells.undo_count,available_formats=h ? s->formats.redo_count : s->formats.undo_count;
        if (cells!=available_cells || formats!=available_formats)
            return range_fail(error,"Legacy formatting migration needs aligned Cell and Format Undo/Redo histories; reopen the unchanged workbook.");
        const CellSnapshot *states=h ? s->cells.redo : s->cells.undo;
        for (size_t i=0;i<available_cells;i++) if (!legacy_cell_shape(states[i].cells,states[i].count,states[i].bytes))
            return range_fail(error,"Legacy formatting migration found an invalid retained editable-cell array shape.");
    }
    if (!format_budget_model_valid(&s->formats))
        return range_fail(error,"Legacy formatting migration found invalid current or retained cell formats.");
    return true;
}
static bool manual_object_state_valid(const ObjectSnapshot *state,uint64_t next_id,size_t *logical,char **error) {
    size_t reserved=0;
    if (!format_budget_object_bytes(state,logical,&reserved)) return range_fail(error,"Manual formatting eligibility found an invalid owned object state.");
    for (size_t i=0;i<state->count;i++) if (state->items[i].kind==OBJECT_TABLE)
        return range_fail(error,"Existing or retained Tables need verified source formatting; their legacy formats cannot become manual-only.");
    ObjectStore view={.items=state->items,.count=state->count,.next_id=next_id};
    /* Pivot's existing validator requires a diagnostic location on failure. */
    char *ignored=NULL; bool valid=objects_valid(&view,error ? error : &ignored); free(ignored); return valid;
}
bool worksheet_manual_format_history_eligible(const Worksheet *s,const Analysis *live,char **error) {
    if (!s || !live) return range_fail(error,"Choose the worksheet and its live Analysis before importing the first Table.");
    AnalysisOwnership owned={.sheet=s,.live=live};
    if (!analysis_journal_valid(s,&owned,error)) return false;
    if (live->kind!=SOURCE_NONE || s->analysis.kind!=SOURCE_NONE)
        return range_fail(error,"A current data source needs verified formatting; its legacy formats cannot become manual-only.");
    for (size_t h=0;h<2;h++) {
        const Analysis *states=h ? s->redo : s->undo; size_t count=h ? s->redo_count : s->undo_count;
        for (size_t i=0;i<count;i++) if (states[i].kind!=SOURCE_NONE)
            return range_fail(error,"Retained data-source history needs verified formatting; its legacy formats cannot become manual-only.");
    }
    if (s->cells.count>s->cells.capacity || !legacy_cell_shape(s->cells.cells,s->cells.count,s->cells.bytes))
        return range_fail(error,"Manual formatting eligibility found an invalid editable-cell array shape.");
    ObjectSnapshot current={s->objects.items,s->objects.count}; size_t logical=0;
    if (!manual_object_state_valid(&current,s->objects.next_id,&logical,error)) return false;
    for (size_t h=0;h<2;h++) {
        const CellSnapshot *cells=h ? s->cells.redo : s->cells.undo; size_t cell_count=h ? s->cells.redo_count : s->cells.undo_count;
        for (size_t i=0;i<cell_count;i++) if (!legacy_cell_shape(cells[i].cells,cells[i].count,cells[i].bytes))
            return range_fail(error,"Manual formatting eligibility found an invalid retained editable-cell array shape.");
        const ObjectSnapshot *states=h ? s->objects.redo : s->objects.undo; size_t count=h ? s->objects.redo_count : s->objects.undo_count,bytes=0;
        for (size_t i=0;i<count;i++) {
            if (!manual_object_state_valid(states+i,s->objects.next_id,&logical,error)) return false;
            if (logical>DX_OBJECT_HISTORY_BYTES-bytes) return range_fail(error,"Manual formatting eligibility exceeds the existing object-history limit.");
            bytes+=logical;
        }
    }
    if (!format_budget_model_valid(&s->formats)) return range_fail(error,"Manual formatting eligibility found invalid current or retained cell formats.");
    bool legacy=!s->formats.source_aware;
    for (size_t i=0;i<s->formats.undo_count;i++) legacy|=!s->formats.undo[i].source_aware;
    for (size_t i=0;i<s->formats.redo_count;i++) legacy|=!s->formats.redo[i].source_aware;
    if (legacy && s->formats.revision==UINT64_MAX) return range_fail(error,"Manual formatting awareness cannot advance the exhausted format revision.");
    return true;
}
FormatBudgetStatus workbook_format_usage(const Workbook *book,const WorkbookFormatReplacement *replacements,size_t count,WorkbookFormatUsage *out) {
    if (!book || !out || book->count>DX_MAX_SHEETS || (book->count && !book->sheets) || count>DX_MAX_SHEETS || (count && !replacements)) return FORMAT_BUDGET_INPUT;
    const FormatStore *formats[DX_MAX_SHEETS]={0}; const SourceVersions *versions[DX_MAX_SHEETS]={0}; const ObjectStore *objects[DX_MAX_SHEETS]={0}; bool replaced[DX_MAX_SHEETS]={0};
    for (size_t i=0;i<book->count;i++) { formats[i]=&book->sheets[i].formats; versions[i]=&book->sheets[i].source_versions; objects[i]=&book->sheets[i].objects; }
    for (size_t i=0;i<count;i++) {
        size_t sheet=replacements[i].sheet; if (sheet>=book->count || replaced[sheet]) return FORMAT_BUDGET_INPUT; replaced[sheet]=true;
        if (replacements[i].formats) formats[sheet]=replacements[i].formats;
        if (replacements[i].versions) versions[sheet]=replacements[i].versions;
        if (replacements[i].objects) objects[sheet]=replacements[i].objects;
    }
    WorkbookFormatUsage usage={.sheet_count=book->count};
    for (size_t i=0;i<book->count;i++) {
        if (!format_budget_model_valid(formats[i]) || !source_versions_valid(versions[i],objects[i]->next_id,NULL)) return FORMAT_BUDGET_MODEL;
        size_t current=format_owned_bytes(formats[i],false),retained=format_owned_bytes(formats[i],true),analysis=0,object_history=0;
        FormatBudgetStatus status=format_budget_analysis_history(book->sheets+i,&analysis); if (status!=FORMAT_BUDGET_OK) return status;
        status=format_budget_object_history(book->sheets+i,objects[i],&object_history); if (status!=FORMAT_BUDGET_OK) return status;
        if (current==SIZE_MAX || retained==SIZE_MAX || !format_budget_add(&current,versions[i]->bytes) || !format_budget_add(&retained,versions[i]->bytes) || !format_budget_add(&retained,analysis) || !format_budget_add(&retained,object_history) || !format_budget_add(&usage.current,current) || !format_budget_add(&usage.retained,retained)) return FORMAT_BUDGET_OVERFLOW;
        if (current>usage.max_sheet_current) { usage.max_sheet_current=current; usage.max_sheet=i; }
    }
    size_t detached=0; FormatBudgetStatus status=workbook_lifecycle_format_usage(book,&detached);
    if (status!=FORMAT_BUDGET_OK) return status;
    if (!format_budget_add(&usage.retained,detached)) return FORMAT_BUDGET_OVERFLOW;
    *out=usage; return FORMAT_BUDGET_OK;
}
static bool format_usage_valid(const WorkbookFormatUsage *usage) {
    if (!usage || usage->sheet_count>DX_MAX_SHEETS || usage->current>usage->retained || usage->max_sheet_current>usage->current) return false;
    if (!usage->sheet_count) return !usage->current && !usage->retained && !usage->max_sheet_current && !usage->max_sheet;
    if (usage->max_sheet>=usage->sheet_count || (usage->current && (!usage->max_sheet_current || (usage->current-1)/usage->max_sheet_current>=usage->sheet_count))) return false;
    return true;
}
FormatBudgetStatus workbook_format_preflight(const WorkbookFormatUsage *live,const WorkbookFormatUsage *next,const FormatTransientUsage *transient,FormatBudget *out) {
    if (!out || !format_usage_valid(live) || !format_usage_valid(next)) return FORMAT_BUDGET_INPUT;
    if (next->max_sheet_current>DX_FORMAT_CURRENT_BYTES) return FORMAT_BUDGET_SHEET;
    if (next->current>DX_WORKBOOK_FORMAT_CURRENT_BYTES) return FORMAT_BUDGET_CURRENT;
    if (next->retained>DX_WORKBOOK_FORMAT_RETAINED_BYTES) return FORMAT_BUDGET_RETAINED;
    FormatTransientUsage empty={0}; if (!transient) transient=&empty;
    size_t other=0; const size_t bytes[]={transient->io_snapshots,transient->other_staging,transient->collector_owned,transient->page_metadata_owned,transient->pending_reserved};
    for (size_t i=0;i<sizeof(bytes)/sizeof(*bytes);i++) if (!format_budget_add(&other,bytes[i])) return FORMAT_BUDGET_OVERFLOW;
    FormatBudget budget={.before=live->retained,.after=next->retained};
    if (!format_budget_add(&budget.before,transient->adopted_staging) || !format_budget_add(&budget.before,other) || !format_budget_add(&budget.after,other)) return FORMAT_BUDGET_OVERFLOW;
    budget.peak=budget.before>budget.after ? budget.before : budget.after;
    if (budget.peak>DX_APP_FORMAT_ADDITIONAL_BYTES) return FORMAT_BUDGET_APP;
    *out=budget; return FORMAT_BUDGET_OK;
}
const char *workbook_format_budget_message(FormatBudgetStatus status) {
    static const char *messages[]={"Formatting storage is within its explicit limits.","Invalid formatting budget input.","Invalid owned format or source-version model.","Formatting byte accounting overflowed.","Current formats and source versions exceed 1 MiB in one worksheet.","Current formats and source versions exceed 8 MiB in the workbook.","Retained formatting and source versions exceed 32 MiB in the workbook.","Owned or reserved additional formatting data exceeds 64 MiB."};
    return (unsigned)status<sizeof(messages)/sizeof(*messages) ? messages[status] : "Unknown formatting budget status.";
}
static bool range_overlap(SheetRange a,SheetRange b) { return a.r0<=b.r1 && b.r0<=a.r1 && a.c0<=b.c1 && b.c0<=a.c1; }
static bool range_contains(SheetRange a,SheetRange b) { return a.r0<=b.r0 && a.r1>=b.r1 && a.c0<=b.c0 && a.c1>=b.c1; }
static SheetRange object_range(const SheetObject *o) { return (SheetRange){o->row,o->row+o->rows-1,o->col,o->col+o->columns-1}; }
static bool range_cell(SheetRange r,const SheetCell *c) { return c->row>=r.r0 && c->row<=r.r1 && c->col>=r.c0 && c->col<=r.c1; }
static bool range_fits(const Worksheet *s,SheetRange r,int64_t dr,int dc,uint64_t rows,size_t columns,bool replace,char **error) {
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
    if (!replace && (dr || dc)) for (size_t i=0;i<s->cells.count;i++) if (range_cell(to,&s->cells.cells[i]) && !range_cell(r,&s->cells.cells[i]))
        return range_fail(error,"Destination contains data. Choose an empty range.");
    return true;
}
bool worksheet_range_fits(const Worksheet *s,SheetRange r,int64_t dr,int dc,uint64_t rows,size_t columns,char **error) {
    return range_fits(s,r,dr,dc,rows,columns,false,error);
}
SheetMoveStatus worksheet_check_move(const Worksheet *s,SheetRange r,int64_t dr,int dc,uint64_t rows,size_t columns,char **error) {
    if (!range_fits(s,r,dr,dc,rows,columns,true,error)) return SHEET_MOVE_INVALID;
    if (range_fits(s,r,dr,dc,rows,columns,false,error)) return SHEET_MOVE_READY;
    for (size_t i=0;i<s->objects.count;i++) if (range_overlap(r,object_range(&s->objects.items[i]))) return SHEET_MOVE_INVALID;
    return SHEET_MOVE_NEEDS_REPLACE;
}
static bool move_range(Worksheet *s,SheetRange r,int64_t dr,int dc,uint64_t rows,size_t columns,bool replace,char **error) {
    if (!range_fits(s,r,dr,dc,rows,columns,replace,error)) return false;
    if (replace) for (size_t i=0;i<s->objects.count;i++) if (range_overlap(r,object_range(&s->objects.items[i])))
        return range_fail(error,"Replacement is available only for manual cell ranges. Move analytical objects to an empty range.");
    if (!dr && !dc) return true;
    uint32_t mask=0;
    for (size_t i=0;i<s->objects.count;i++) if (range_contains(r,object_range(&s->objects.items[i]))) mask|=UINT32_C(1)<<i;
    uint64_t revision=s->cells.revision;
    FormatSnapshot formats={0}; if (!format_plan_move(&s->formats,(FormatRange){r.r0,r.r1,r.c0,r.c1},dr,dc,&formats,error)) return false;
    bool moved=replace ? cells_move_replace(&s->cells,r.r0,r.r1,r.c0,r.c1,dr,dc,error) : cells_move(&s->cells,r.r0,r.r1,r.c0,r.c1,dr,dc,error);
    if (!moved) { format_snapshot_free(&formats); return false; }
    objects_translate_mask(&s->objects,mask,dr,dc);
    bool cells_changed=revision!=s->cells.revision,format_changed=format_commit(&s->formats,&formats);
    if (cells_changed || mask || format_changed) worksheet_record(s,range_edit(cells_changed,mask!=0,format_changed));
    return true;
}
bool worksheet_move_range(Worksheet *s,SheetRange r,int64_t dr,int dc,uint64_t rows,size_t columns,char **error) {
    return move_range(s,r,dr,dc,rows,columns,false,error);
}
bool worksheet_replace_range(Worksheet *s,SheetRange r,int64_t dr,int dc,uint64_t rows,size_t columns,char **error) {
    return move_range(s,r,dr,dc,rows,columns,true,error);
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
void worksheet_free(Worksheet *s) {
    free(s->name); analysis_free(&s->analysis); view_free(&s->view); cells_free(&s->cells); objects_free(&s->objects); format_free(&s->formats); source_versions_free(&s->source_versions);
    for (size_t j=0;j<s->undo_count;j++) analysis_free(&s->undo[j]);
    for (size_t j=0;j<s->redo_count;j++) analysis_free(&s->redo[j]);
    memset(s,0,sizeof(*s));
}
void workbook_free(Workbook *b) {
    workbook_lifecycle_free(b);
    for (size_t i=0;i<b->count;i++) worksheet_free(b->sheets+i);
    free(b->sheets); free(b->path); workbook_init(b);
}
bool workbook_add(Workbook *b,const char *name,const Analysis *a,char **error) {
    if (b->count>=DX_MAX_SHEETS) { error_set(error,"A workspace supports up to 64 worksheets."); return false; }
    WorkbookId document; uint64_t identity;
    if (!workbook_identity_next(b,&document,&identity,error)) return false;
    char *owned_name=dx_strdup(name); Analysis owned_analysis=analysis_copy(a);
    b->sheets=dx_realloc(b->sheets,(b->count+1)*sizeof(*b->sheets));
    Worksheet *s=&b->sheets[b->count]; memset(s,0,sizeof(*s)); s->name=owned_name; s->analysis=owned_analysis; s->id=identity;
    b->id=document; b->sheet_high_water=identity;
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
char *workbook_prepare_sheet_name(const Workbook *b,size_t index,const char *name,char **error) {
    if (index>b->count || !name) { range_fail(error,"Choose a worksheet name."); return NULL; }
    while (*name && isspace((unsigned char)*name)) name++;
    size_t length=strlen(name); while (length && isspace((unsigned char)name[length-1])) length--;
    if (!length || length>4096) { range_fail(error,"Worksheet names must contain 1–4096 bytes of text."); return NULL; }
    for (size_t i=0;i<length;i++) if ((unsigned char)name[i]<32 || name[i]==127) { range_fail(error,"Worksheet names cannot contain control characters."); return NULL; }
    char *owned=dx_printf("%.*s",(int)length,name);
    for (size_t i=0;i<b->count;i++) if (i!=index && same_name(owned,b->sheets[i].name)) { free(owned); range_fail(error,"Another worksheet already has that name."); return NULL; }
    return owned;
}
char *workbook_unique_sheet_name(const Workbook *b,const char *base,char **error) {
    /* Normalize independently of existing names before searching a bounded
       suffix space. Invalid names fail rather than looping on validation. */
    char *normalized=workbook_prepare_sheet_name(&(Workbook){0},0,base,error);
    if (!normalized) return NULL;
    char *name=dx_strdup(normalized);
    for (size_t suffix=1;suffix<=DX_MAX_SHEETS+1;suffix++) {
        bool taken=false; for (size_t i=0;i<b->count;i++) if (same_name(name,b->sheets[i].name)) taken=true;
        if (!taken) { free(normalized); return name; }
        size_t bytes=strlen(normalized);
        if (bytes>4080) { bytes=4080; while (bytes && ((unsigned char)normalized[bytes]&0xc0)==0x80) bytes--; }
        free(name); name=dx_printf("%.*s (%zu)",(int)bytes,normalized,suffix+1);
    }
    free(normalized); free(name); range_fail(error,"No available worksheet name."); return NULL;
}
bool workbook_rename(Workbook *b,size_t index,const char *name,char **error) {
    if (index>=b->count) return range_fail(error,"Choose a worksheet to rename.");
    char *owned=workbook_prepare_sheet_name(b,index,name,error); if (!owned) return false;
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
    char *name=workbook_unique_sheet_name(b,b->sheets[index].name,error);
    if (!name) { source_versions_free(&versions); return false; }
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
    if (!workbook_identity_valid(b)) return NULL;
    Workbook *out=dx_alloc(sizeof(*out)); workbook_init(out); out->user_scale=b->user_scale; out->reduced_motion=b->reduced_motion;
    out->path=b->path ? dx_strdup(b->path) : NULL; out->id=b->id;
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
    for (size_t i=0;i<b->count;i++) out->sheets[i].id=b->sheets[i].id;
    out->sheet_high_water=b->sheet_high_water; out->command_revision=b->command_revision;
    out->active=b->active; out->dirty=b->dirty; return out;
}
static bool absolute(const char *path) {
    return path[0]=='/' || path[0]=='\\' || (path[0] && path[1]==':');
}
static char *directory(const char *path) {
    const char *base=dx_basename(path); return base==path ? dx_strdup(".") : dx_printf("%.*s",(int)(base-path),path);
}
static char *directory_join(const char *dir,const char *name) {
    size_t n=strlen(dir);
    bool separated=n && (dir[n-1]=='/' || dir[n-1]=='\\');
    return dx_printf("%s%s%s",dir,separated ? "" : "/",name);
}
char *workbook_target_path(const char *path) {
    char *existing=dx_absolute_path(path); if (existing) return existing;
    char *dir=directory(path),*resolved=dx_absolute_path(dir); free(dir);
    if (!resolved) return NULL;
    char *out=directory_join(resolved,dx_basename(path)); free(resolved); return out;
}
char *workbook_source_path(const char *workspace,const char *ref) {
    if (absolute(ref)) return dx_strdup(ref);
    char *dir=directory(workspace); char *out=directory_join(dir,ref); free(dir);
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
static bool source_package_inventory(duckdb_connection c,unsigned package,bool complete,char **error) {
    char *sql=dx_strdup("SELECT count(*),count(*) FILTER(WHERE table_type<>'BASE TABLE' OR table_name NOT IN (");
    for (size_t i=0;i<workbook_codec_table_count(package);i++) dx_append(&sql,"%s'%s'",i ? "," : "",workbook_codec_table_at(package,i)->table);
    dx_append(&sql,")) FROM information_schema.tables WHERE table_schema='_duxcel_workspace'");
    duckdb_result r={0}; bool queried=duckdb_query(c,sql,&r)==DuckDBSuccess; free(sql);
    if (!queried) { error_set(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return false; }
    uint64_t count=duckdb_value_uint64(&r,0,0),unknown=duckdb_value_uint64(&r,1,0); duckdb_destroy_result(&r);
    bool valid=!unknown && (complete ? count==workbook_codec_table_count(package) : count<=workbook_codec_table_count(package));
    return valid || range_fail(error,"Workspace requires its complete known base metadata tables; unknown or view metadata cannot be preserved.");
}
/* One required-column inventory serves both ordinary v14 loading and archived
   overwrite preflight. Optional predicates/schema text and pending raw proofs
   retain SQL NULL; required zero values never arise from NULL coercion. */
static bool source_package_required(duckdb_connection c,unsigned package,char **error) {
    char *sql=dx_strdup("SELECT ");
    for (size_t i=0;i<workbook_codec_table_count(package);i++) {
        const WorkbookCodecTable *table=workbook_codec_table_at(package,i);
        dx_append(&sql,"%sEXISTS(SELECT 1 FROM _duxcel_workspace.%s WHERE ",i ? " OR " : "",table->table);
        bool any=false; const char *type=table->types;
        for (const char *p=table->columns;*p;) {
            size_t n=strcspn(p,","); while (*type==' ') type++;
            if (!workbook_codec_nullable(table,p,n)) { dx_append(&sql,"%s%.*s IS NULL",any ? " OR " : "",(int)n,p); any=true; }
            if (*type=='T') { dx_append(&sql,"%scontains(%.*s,chr(0))",any ? " OR " : "",(int)n,p); any=true; }
            type++; p+=n; if (*p) p++;
        }
        dx_append(&sql,")");
    }
    duckdb_result r={0}; bool queried=duckdb_query(c,sql,&r)==DuckDBSuccess; free(sql);
    if (!queried) { error_set(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return false; }
    bool valid=!duckdb_value_boolean(&r,0,0); duckdb_destroy_result(&r);
    return valid || range_fail(error,"Workspace v14 metadata contains a required SQL NULL or embedded NUL text.");
}
static size_t codec_types(const WorkbookCodecTable *table,duckdb_type types[DX_WORKBOOK_CODEC_COLUMNS]) {
    size_t count=0;
    for (const char *p=table->types;*p;p++) if (*p!=' ') types[count++]=*p=='I' ? DUCKDB_TYPE_INTEGER : *p=='U' ? DUCKDB_TYPE_UBIGINT : *p=='D' ? DUCKDB_TYPE_DOUBLE : *p=='B' ? DUCKDB_TYPE_BOOLEAN : *p=='X' ? DUCKDB_TYPE_BLOB : DUCKDB_TYPE_VARCHAR;
    return count;
}
static bool codec_package_shape(duckdb_connection c,const char *name,char **error) {
    const WorkbookCodecTable *table=workbook_codec_table_find(name);
    duckdb_type types[DX_WORKBOOK_CODEC_COLUMNS]; size_t count=codec_types(table,types);
    return package_shape(c,table->table,table->columns,types,count,error);
}
static bool source_package_shape(duckdb_connection c,unsigned package,char **error) {
    if (!source_package_inventory(c,package,true,error)) return false;
    for (size_t i=0;i<workbook_codec_table_count(package);i++) {
        const WorkbookCodecTable *table=workbook_codec_table_at(package,i);
        if (!codec_package_shape(c,table->table,error)) return false;
    }
    return source_package_required(c,package,error) && owner_package_rows_supported(c,error);
}
static bool format_metadata_supported(duckdb_connection c,int workspace_version,char **error) {
    duckdb_result r={0}; bool ok=false;
    if (workspace_version>=14) return source_package_shape(c,(unsigned)workspace_version,error);
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name IN ('workbook_identity','sheet_identity')",&r)!=DuckDBSuccess) goto sql_error;
    bool identities=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (identities) return range_fail(error,"Persistent identity metadata requires workspace v15.");
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name IN ('source_version_counters','source_versions','source_bindings','format_records','format_fields','format_state')",&r)!=DuckDBSuccess) goto sql_error;
    bool future=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (future) { error_set(error,"Source-version metadata requires workspace v14; an older version cannot load or overwrite it."); return false; }
    if (workspace_version<=12) {
        if (duckdb_query(c,"SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace' AND table_name IN ('object_counters','format_scopes','format_selectors','format_guards')",&r)!=DuckDBSuccess) goto sql_error;
        bool extended=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
        if (extended) goto unsupported;
    } else {
        if (!codec_package_shape(c,"meta",error) || !codec_package_shape(c,"objects",error) ||
            !codec_package_shape(c,"object_counters",error) || !codec_package_shape(c,"format_scopes",error) ||
            !codec_package_shape(c,"format_selectors",error) || !codec_package_shape(c,"format_guards",error)) return false;
        if (duckdb_query(c,"SELECT EXISTS(SELECT 1 FROM _duxcel_workspace.format_scopes) OR EXISTS(SELECT 1 FROM _duxcel_workspace.format_guards) OR EXISTS(SELECT 1 FROM _duxcel_workspace.format_selectors WHERE kind IS NULL OR kind<>0)",&r)!=DuckDBSuccess) goto sql_error;
        bool scoped=duckdb_value_boolean(&r,0,0); duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
        if (scoped) { error_set(error,"Scoped cell formatting payloads are not supported by this owner-only workspace v13 codec."); return false; }
    }
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='formats'",&r)!=DuckDBSuccess) goto sql_error;
    uint64_t columns=duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!columns && workspace_version<=12) return true;
    if (columns!=13) goto unsupported;
    duckdb_type types[DX_WORKBOOK_CODEC_COLUMNS]; codec_types(workbook_codec_table_find("formats"),types);
    ok=workspace_version<=12 ? metadata_types(c,"SELECT sheet_id,position,r0,r1,c0,c1,mask,flags,alignment,color,font_size,numeric_format,decimals FROM _duxcel_workspace.formats LIMIT 0",types,13,error) : codec_package_shape(c,"formats",error) && owner_package_rows_supported(c,error); goto done;
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
    if (!prepare(c,"INSERT INTO _duxcel_workspace.marks(sheet_id,object_id,role,position,name,is_measure,aggregate,grain) VALUES (?,?,?,?,?,?,?,?)",&statement,error)) goto done;
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
static bool save_owner_package(duckdb_connection c,const Workbook *book,bool scoped,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.object_counters(sheet_id INTEGER PRIMARY KEY,high_water UBIGINT);"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.format_scopes(sheet_id INTEGER,scope_id INTEGER,owner UBIGINT,encoding INTEGER,definition BLOB,PRIMARY KEY(sheet_id,scope_id));"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.format_selectors(sheet_id INTEGER,position INTEGER,kind INTEGER,PRIMARY KEY(sheet_id,position));"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.format_guards(sheet_id INTEGER,position INTEGER,guard_position INTEGER,scope_id INTEGER,PRIMARY KEY(sheet_id,position,guard_position));"
        "DELETE FROM _duxcel_workspace.object_counters; DELETE FROM _duxcel_workspace.format_selectors; DELETE FROM _duxcel_workspace.format_scopes; DELETE FROM _duxcel_workspace.format_guards",error)) return false;
    duckdb_prepared_statement counter=NULL,selector=NULL; bool ok=prepare(c,"INSERT INTO _duxcel_workspace.object_counters(sheet_id,high_water) VALUES (?,?)",&counter,error) && prepare(c,"INSERT INTO _duxcel_workspace.format_selectors(sheet_id,position,kind) VALUES (?,?,?)",&selector,error);
    for (size_t i=0;ok && i<book->count;i++) {
        duckdb_bind_int32(counter,1,(int32_t)i); duckdb_bind_uint64(counter,2,book->sheets[i].objects.next_id); ok=run(counter,error);
        for (size_t f=0;ok && f<book->sheets[i].formats.count;f++) { duckdb_bind_int32(selector,1,(int32_t)i); duckdb_bind_int32(selector,2,(int32_t)f); duckdb_bind_int32(selector,3,scoped ? book->sheets[i].formats.items[f].selector : FORMAT_SPATIAL); ok=run(selector,error); }
    }
    if (counter) duckdb_destroy_prepare(&counter); if (selector) duckdb_destroy_prepare(&selector); return ok;
}
static bool save_source_package(duckdb_connection c,const Workbook *book,const char *target,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.source_version_counters(sheet_id INTEGER PRIMARY KEY,high_water UBIGINT);"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.source_versions(sheet_id INTEGER,version UBIGINT,owner UBIGINT,source_kind INTEGER,source_reference VARCHAR,encoding INTEGER,definition BLOB,PRIMARY KEY(sheet_id,version));"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.source_bindings(sheet_id INTEGER,owner UBIGINT,version UBIGINT,PRIMARY KEY(sheet_id,owner));"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.format_records(sheet_id INTEGER,position INTEGER,member_position INTEGER,record_id UBIGINT,PRIMARY KEY(sheet_id,position,member_position));"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.format_fields(sheet_id INTEGER,position INTEGER,field_position INTEGER,source_ordinal INTEGER,PRIMARY KEY(sheet_id,position,field_position));"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.format_state(sheet_id INTEGER PRIMARY KEY,source_aware BOOLEAN);"
        "DELETE FROM _duxcel_workspace.source_version_counters; DELETE FROM _duxcel_workspace.source_versions; DELETE FROM _duxcel_workspace.source_bindings; DELETE FROM _duxcel_workspace.format_records; DELETE FROM _duxcel_workspace.format_fields; DELETE FROM _duxcel_workspace.format_state",error)) return false;
    const char *sql[]={"INSERT INTO _duxcel_workspace.source_version_counters(sheet_id,high_water) VALUES (?,?)","INSERT INTO _duxcel_workspace.source_versions(sheet_id,version,owner,source_kind,source_reference,encoding,definition) VALUES (?,?,?,?,?,?,?)","INSERT INTO _duxcel_workspace.source_bindings(sheet_id,owner,version) VALUES (?,?,?)","INSERT INTO _duxcel_workspace.format_scopes(sheet_id,scope_id,owner,encoding,definition) VALUES (?,?,?,?,?)","INSERT INTO _duxcel_workspace.format_guards(sheet_id,position,guard_position,scope_id) VALUES (?,?,?,?)","INSERT INTO _duxcel_workspace.format_records(sheet_id,position,member_position,record_id) VALUES (?,?,?,?)","INSERT INTO _duxcel_workspace.format_fields(sheet_id,position,field_position,source_ordinal) VALUES (?,?,?,?)","INSERT INTO _duxcel_workspace.format_state(sheet_id,source_aware) VALUES (?,?)"};
    duckdb_prepared_statement statements[8]={0}; bool ok=false;
    for (size_t i=0;i<8;i++) if (!prepare(c,sql[i],statements+i,error)) goto done;
    for (size_t i=0;i<book->count;i++) {
        const Worksheet *sheet=book->sheets+i; duckdb_prepared_statement counter=statements[0],version=statements[1],binding=statements[2],scope=statements[3];
        duckdb_bind_int32(counter,1,(int32_t)i); duckdb_bind_uint64(counter,2,sheet->source_versions.high_water); if (!run(counter,error)) goto done;
        duckdb_bind_int32(statements[7],1,(int32_t)i); duckdb_bind_boolean(statements[7],2,sheet->formats.source_aware); if (!run(statements[7],error)) goto done;
        for (size_t j=0;j<sheet->source_versions.count;j++) {
            const SourceVersion *entry=sheet->source_versions.items+j; char *ref=workbook_relative_path(target,entry->path);
            duckdb_bind_int32(version,1,(int32_t)i); duckdb_bind_uint64(version,2,entry->version); duckdb_bind_uint64(version,3,entry->owner); duckdb_bind_int32(version,4,entry->kind); bind_text(version,5,ref); free(ref);
            duckdb_bind_int32(version,6,entry->raw.bytes ? 1 : 0);
            if (entry->raw.bytes) duckdb_bind_blob(version,7,entry->raw.data,entry->raw.bytes); else duckdb_bind_null(version,7);
            if (!run(version,error)) goto done;
        }
        for (size_t j=0;j<=sheet->objects.count;j++) {
            if (j && sheet->objects.items[j-1].kind!=OBJECT_TABLE) continue;
            uint64_t owner=j ? sheet->objects.items[j-1].id : 0,id=j ? sheet->objects.items[j-1].source.source_version : sheet->analysis.source_version;
            duckdb_bind_int32(binding,1,(int32_t)i); duckdb_bind_uint64(binding,2,owner); duckdb_bind_uint64(binding,3,id); if (!run(binding,error)) goto done;
        }
        for (size_t j=0;j<sheet->formats.scope_count;j++) {
            const FormatScope *entry=sheet->formats.scopes+j;
            duckdb_bind_int32(scope,1,(int32_t)i); duckdb_bind_int32(scope,2,(int32_t)j); duckdb_bind_uint64(scope,3,entry->owner); duckdb_bind_int32(scope,4,1); duckdb_bind_blob(scope,5,entry->definition,entry->bytes); if (!run(scope,error)) goto done;
        }
        for (size_t j=0;j<sheet->formats.count;j++) {
            const FormatPatch *patch=sheet->formats.items+j;
            for (size_t g=0;g<patch->guard_count;g++) { duckdb_prepared_statement st=statements[4]; duckdb_bind_int32(st,1,(int32_t)i); duckdb_bind_int32(st,2,(int32_t)j); duckdb_bind_int32(st,3,(int32_t)g); duckdb_bind_int32(st,4,(int32_t)patch->guard_ids[g]); if (!run(st,error)) goto done; }
            for (size_t m=0;m<patch->record_count;m++) { duckdb_prepared_statement st=statements[5]; duckdb_bind_int32(st,1,(int32_t)i); duckdb_bind_int32(st,2,(int32_t)j); duckdb_bind_int32(st,3,(int32_t)m); duckdb_bind_uint64(st,4,patch->record_ids[m]); if (!run(st,error)) goto done; }
            for (size_t f=0;f<patch->field_count;f++) { duckdb_prepared_statement st=statements[6]; duckdb_bind_int32(st,1,(int32_t)i); duckdb_bind_int32(st,2,(int32_t)j); duckdb_bind_int32(st,3,(int32_t)f); duckdb_bind_int32(st,4,(int32_t)patch->fields[f]); if (!run(st,error)) goto done; }
        }
    }
    ok=true;
done: for (size_t i=0;i<8;i++) if (statements[i]) duckdb_destroy_prepare(statements+i); return ok;
}
static bool save_filters(duckdb_connection c,const Workbook *b,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.filters(sheet_id INTEGER,position INTEGER,name VARCHAR,op INTEGER,value VARCHAR,PRIMARY KEY(sheet_id,position)); DELETE FROM _duxcel_workspace.filters",error)) return false;
    duckdb_prepared_statement stmt=NULL; if (!prepare(c,"INSERT INTO _duxcel_workspace.filters(sheet_id,position,name,op,value) VALUES (?,?,?,?,?)",&stmt,error)) return false;
    bool ok=true;
    for (size_t i=0;i<b->count && ok;i++) for (size_t j=0;j<b->sheets[i].analysis.filter_count;j++) {
        const AnalysisFilter *f=&b->sheets[i].analysis.filters[j]; duckdb_bind_uint64(stmt,1,i); duckdb_bind_uint64(stmt,2,j); bind_text(stmt,3,f->column); duckdb_bind_int32(stmt,4,f->op); bind_text(stmt,5,f->value);
        if (!run(stmt,error)) { ok=false; break; }
    }
    duckdb_destroy_prepare(&stmt); return ok;
}
static bool save_sorts(duckdb_connection c,const Workbook *b,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.sorts(sheet_id INTEGER,position INTEGER,name VARCHAR,descending BOOLEAN,PRIMARY KEY(sheet_id,position)); DELETE FROM _duxcel_workspace.sorts",error)) return false;
    duckdb_prepared_statement stmt=NULL; if (!prepare(c,"INSERT INTO _duxcel_workspace.sorts(sheet_id,position,name,descending) VALUES (?,?,?,?)",&stmt,error)) return false;
    bool ok=true;
    for (size_t i=0;i<b->count && ok;i++) for (size_t j=0;j<b->sheets[i].analysis.sort_count;j++) {
        const AnalysisSort *sort=&b->sheets[i].analysis.sorts[j]; duckdb_bind_uint64(stmt,1,i); duckdb_bind_uint64(stmt,2,j); bind_text(stmt,3,sort->column); duckdb_bind_boolean(stmt,4,sort->descending);
        if (!run(stmt,error)) { ok=false; break; }
    }
    duckdb_destroy_prepare(&stmt); return ok;
}
static bool save_queries(duckdb_connection c,const Workbook *b,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.queries(sheet_id INTEGER,object_id UBIGINT,query VARCHAR,PRIMARY KEY(sheet_id,object_id)); DELETE FROM _duxcel_workspace.queries",error)) return false;
    duckdb_prepared_statement stmt=NULL; if (!prepare(c,"INSERT INTO _duxcel_workspace.queries(sheet_id,object_id,query) VALUES (?,?,?)",&stmt,error)) return false;
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
    duckdb_prepared_statement stmt=NULL; if (!prepare(c,"INSERT INTO _duxcel_workspace.source_options(sheet_id,object_id,unique_rows) VALUES (?,?,true)",&stmt,error)) return false;
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
static const SourceVersion *source_entry(const Worksheet *sheet,uint64_t version) {
    for (size_t i=0;i<sheet->source_versions.count;i++) if (sheet->source_versions.items[i].version==version) return sheet->source_versions.items+i;
    return NULL;
}
static bool source_binding_valid(const Worksheet *sheet,uint64_t owner,const Analysis *analysis,char **error) {
    if (!analysis->source_version) return true;
    const SourceVersion *entry=source_entry(sheet,analysis->source_version);
    return (entry && entry->owner==owner && entry->kind==analysis->kind && analysis->path && !strcmp(entry->path,analysis->path)) || range_fail(error,"Workspace v14 has a dangling or incompatible source-version binding.");
}
static bool source_model_valid(const Workbook *book,char **error) {
    size_t workbook_bytes=0;
    for (size_t s=0;s<book->count;s++) {
        const Worksheet *sheet=book->sheets+s; const ObjectStore *objects=&sheet->objects; const FormatStore *formats=&sheet->formats;
        if (!source_versions_valid(&sheet->source_versions,objects->next_id,error) || !format_valid(formats,error) || !source_binding_valid(sheet,0,&sheet->analysis,error)) return false;
        if (sheet->undo_count>DX_HISTORY || sheet->redo_count>DX_HISTORY || objects->undo_count>DX_HISTORY || objects->redo_count>DX_HISTORY) return range_fail(error,"Invalid workspace v14 source history bounds.");
        for (size_t i=0;i<sheet->undo_count;i++) if (!source_binding_valid(sheet,0,sheet->undo+i,error)) return false;
        for (size_t i=0;i<sheet->redo_count;i++) if (!source_binding_valid(sheet,0,sheet->redo+i,error)) return false;
        for (size_t i=0;i<objects->count;i++) {
            const SheetObject *object=objects->items+i;
            if (object->kind==OBJECT_TABLE ? !source_binding_valid(sheet,object->id,&object->source,error) : object->source.source_version!=0) return range_fail(error,"Invalid workspace v14 object source binding.");
        }
        for (size_t h=0;h<2;h++) {
            const ObjectSnapshot *stack=h ? objects->redo : objects->undo; size_t count=h ? objects->redo_count : objects->undo_count;
            for (size_t i=0;i<count;i++) {
                if (stack[i].count>DX_MAX_OBJECTS || (stack[i].count && !stack[i].items)) return range_fail(error,"Invalid workspace v14 object history bounds.");
                for (size_t j=0;j<stack[i].count;j++) { const SheetObject *o=stack[i].items+j; if (o->kind==OBJECT_TABLE ? !source_binding_valid(sheet,o->id,&o->source,error) : o->source.source_version!=0) return range_fail(error,"Invalid workspace v14 historical source binding."); }
            }
        }
        for (size_t i=0;i<sheet->source_versions.count;i++) for (size_t j=0;j<objects->count;j++) if (sheet->source_versions.items[i].owner==objects->items[j].id && objects->items[j].kind!=OBJECT_TABLE) return range_fail(error,"A raw workspace v14 source version cannot belong to an analytical object.");
        bool used[DX_FORMAT_PATCHES]={0}; SourceScopeInfo infos[DX_FORMAT_PATCHES];
        if (formats->scope_count>DX_FORMAT_PATCHES) return range_fail(error,"Workspace v14 has too many supported source scopes.");
        for (size_t i=0;i<formats->scope_count;i++) {
            const FormatScope *scope=formats->scopes+i;
            if (!source_scope_validate(scope->definition,scope->bytes,infos+i,error)) return false;
            const SourceVersion *entry=source_entry(sheet,infos[i].version);
            if (scope->owner!=infos[i].owner || !entry || !entry->raw.bytes || entry->owner!=scope->owner || entry->kind!=infos[i].source_kind || entry->raw.bytes!=infos[i].raw_bytes || memcmp(entry->raw.data,infos[i].raw,entry->raw.bytes)) return range_fail(error,"Workspace v14 source scope differs from its complete typed ledger proof.");
        }
        for (size_t i=0;i<formats->count;i++) {
            const FormatPatch *patch=formats->items+i;
            if (patch->selector==FORMAT_SPATIAL) continue;
            if (patch->selector!=FORMAT_GUARDED && patch->guard_count!=1) return range_fail(error,"Workspace v14 result/record selectors require one exact source scope.");
            for (size_t g=0;g<patch->guard_count;g++) {
                size_t guard=patch->guard_ids[g]; used[guard]=true;
                if (infos[guard].kind!=(patch->selector==FORMAT_RECORDS ? SOURCE_SCOPE_RAW : SOURCE_SCOPE_RESULT)) return range_fail(error,"Workspace v14 selector and canonical packet kinds disagree.");
                for (size_t h=0;h<g;h++) if (infos[patch->guard_ids[h]].owner==infos[guard].owner) return range_fail(error,"Workspace v14 guarded formatting requires one exact result per owner.");
                for (size_t j=0;j<patch->field_count;j++) if (patch->fields[j]>=infos[guard].field_count) return range_fail(error,"Workspace v14 record formatting refers to an unknown original field.");
            }
        }
        for (size_t i=0;i<formats->scope_count;i++) if (!used[i]) return range_fail(error,"Workspace v14 contains an unreferenced format scope.");
        size_t formats_bytes=format_owned_bytes(formats,false),ledger_bytes=sheet->source_versions.bytes;
        if (formats_bytes>DX_FORMAT_CURRENT_BYTES || ledger_bytes>DX_FORMAT_CURRENT_BYTES-formats_bytes || workbook_bytes>8u*1024u*1024u-formats_bytes-ledger_bytes) return range_fail(error,"Workspace v14 current source formats exceed the 1 MiB sheet or 8 MiB workbook owned limit.");
        workbook_bytes+=formats_bytes+ledger_bytes;
    }
    return true;
}
/* Missing sources cannot be realpath-normalized. Relative Save As references may
   therefore restore to longer locator strings. Validate that exact next owned
   representation as well as the live model before writing an archive. */
static bool source_target_reference(const char *target,const char *path,size_t *bytes,char **error) {
    char *ref=workbook_relative_path(target,path),*restored=workbook_source_path(target,ref);
    bool valid=ref && strlen(ref)<=DX_SOURCE_SCOPE_TEXT_BYTES && restored && strlen(restored)<=DX_SOURCE_SCOPE_TEXT_BYTES;
    if (valid) *bytes=strlen(restored)+1;
    free(ref); free(restored);
    return valid || range_fail(error,"Workspace v14 Save As source reference exceeds its bounded restored locator.");
}
static bool source_target_valid(const Workbook *book,const char *target,char **error) {
    size_t total=0;
    for (size_t s=0;s<book->count;s++) {
        const Worksheet *sheet=book->sheets+s; size_t ignored=0,bytes=sheet->source_versions.count*sizeof(SourceVersion);
        if (!source_target_reference(target,sheet->analysis.path,&ignored,error)) return false;
        for (size_t i=0;i<sheet->objects.count;i++) if (sheet->objects.items[i].kind==OBJECT_TABLE && !source_target_reference(target,sheet->objects.items[i].source.path,&ignored,error)) return false;
        for (size_t i=0;i<sheet->source_versions.count;i++) {
            const SourceVersion *entry=sheet->source_versions.items+i; size_t path_bytes=0;
            if (!source_target_reference(target,entry->path,&path_bytes,error)) return false;
            if (bytes>DX_FORMAT_CURRENT_BYTES-path_bytes || entry->raw.bytes>DX_FORMAT_CURRENT_BYTES-bytes-path_bytes) return range_fail(error,"Workspace v14 Save As ledger exceeds its restored owned budget.");
            bytes+=path_bytes+entry->raw.bytes;
        }
        size_t formats=format_owned_bytes(&sheet->formats,false);
        if (formats>DX_FORMAT_CURRENT_BYTES || bytes>DX_FORMAT_CURRENT_BYTES-formats || total>8u*1024u*1024u-formats-bytes) return range_fail(error,"Workspace v14 Save As source formats exceed their restored owned budget.");
        total+=formats+bytes;
    }
    return true;
}
/* Ordinal sheet_id remains the archive join key. Stable IDs are an explicit
   bijection, never inferred from a display name or a physical table position. */
static bool save_identity_package(duckdb_connection c,const Workbook *book,char **error) {
    if (!exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.workbook_identity(high UBIGINT,low UBIGINT,sheet_high_water UBIGINT,command_revision UBIGINT);"
        "CREATE TABLE IF NOT EXISTS _duxcel_workspace.sheet_identity(sheet_id INTEGER,stable_id UBIGINT);"
        "DELETE FROM _duxcel_workspace.workbook_identity; DELETE FROM _duxcel_workspace.sheet_identity",error)) return false;
    duckdb_prepared_statement document=NULL,sheet=NULL; bool ok=false;
    if (!prepare(c,"INSERT INTO _duxcel_workspace.workbook_identity VALUES (?,?,?,?)",&document,error) ||
        !prepare(c,"INSERT INTO _duxcel_workspace.sheet_identity VALUES (?,?)",&sheet,error)) goto done;
    duckdb_bind_uint64(document,1,book->id.high); duckdb_bind_uint64(document,2,book->id.low); duckdb_bind_uint64(document,3,book->sheet_high_water); duckdb_bind_uint64(document,4,book->command_revision);
    if (!run(document,error)) goto done;
    for (size_t i=0;i<book->count;i++) {
        duckdb_bind_int32(sheet,1,(int32_t)i); duckdb_bind_uint64(sheet,2,book->sheets[i].id);
        if (!run(sheet,error)) goto done;
    }
    ok=true;
 done:
    if (document) duckdb_destroy_prepare(&document); if (sheet) duckdb_destroy_prepare(&sheet); return ok;
}
static bool load_identity_package(duckdb_connection c,Workbook *book,char **error) {
    duckdb_result r={0}; WorkbookId document={0}; uint64_t high_water=0,revision=0,ids[DX_MAX_SHEETS]={0}; bool ok=false;
    if (duckdb_query(c,"SELECT high,low,sheet_high_water,command_revision FROM _duxcel_workspace.workbook_identity LIMIT 2",&r)!=DuckDBSuccess) goto done;
    if (duckdb_row_count(&r)!=1) goto done;
    document=(WorkbookId){duckdb_value_uint64(&r,0,0),duckdb_value_uint64(&r,1,0)}; high_water=duckdb_value_uint64(&r,2,0); revision=duckdb_value_uint64(&r,3,0);
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,stable_id FROM _duxcel_workspace.sheet_identity ORDER BY sheet_id LIMIT 65",&r)!=DuckDBSuccess) goto done;
    if (duckdb_row_count(&r)!=book->count) goto done;
    for (size_t i=0;i<book->count;i++) {
        if (duckdb_value_int32(&r,0,i)!=(int32_t)i) goto done;
        ids[i]=duckdb_value_uint64(&r,1,i);
    }
    ok=workbook_identity_restore(book,document,high_water,ids,book->count,error);
    if (ok) book->command_revision=revision;
 done:
    duckdb_destroy_result(&r);
    return ok || range_fail(error,"Invalid workbook/worksheet identity rows; the current document was preserved.");
}
static bool source_archive_valid(duckdb_connection c,const char *target,int package,char **error);
typedef struct {
    WorkbookSaveControl control;
    WorkbookSaveProgress progress;
    bool cancelled;
} SaveTracker;
static bool save_progress(SaveTracker *tracker,WorkbookSavePhase phase,bool advance,char **error) {
    tracker->progress.phase=phase;
    if (advance) tracker->progress.completed++;
    if (!tracker->control.update || tracker->control.update(tracker->control.context,tracker->progress) || phase==SAVE_PHASE_COMPLETE) return true;
    tracker->cancelled=true; error_set(error,"Save cancelled. No new workbook state was published."); return false;
}
WorkbookSaveResult workbook_save_controlled(const Workbook *b,const char *path,WorkbookSaveControl control,char **error) {
    if (!b || !path || !*path || (b->count && !b->sheets)) { if (error) error_set(error,"No valid workbook or destination to save."); return WORKBOOK_SAVE_FAILED; }
    if (!b->count || b->count>DX_MAX_SHEETS || b->active>=b->count) { error_set(error,"No valid worksheet to save."); return WORKBOOK_SAVE_FAILED; }
    if (b->sheets[b->active].hidden) { error_set(error,"The active worksheet must be visible."); return WORKBOOK_SAVE_FAILED; }
    if (!isfinite(b->user_scale) || b->user_scale<.8 || b->user_scale>1.5) { error_set(error,"Invalid workspace zoom."); return WORKBOOK_SAVE_FAILED; }
    SaveTracker tracker={.control=control,.progress={.total=8+b->count}};
    for (size_t i=0;i<b->count;i++) {
        if (b->sheets[i].view.column_count>16384 || b->sheets[i].cells.count>DX_MAX_EDIT_CELLS) {
            error_set(error,"Worksheet contents exceed the supported Save bounds."); return WORKBOOK_SAVE_FAILED;
        }
        tracker.progress.total+=b->sheets[i].view.column_count+b->sheets[i].cells.count;
    }
    if (!save_progress(&tracker,SAVE_PHASE_VALIDATE,false,error)) return WORKBOOK_SAVE_CANCELLED;
    for (size_t i=0;i<b->count;i++) {
        const Worksheet *s=&b->sheets[i]; const Analysis *a=&s->analysis; const SheetView *v=&s->view;
        if (!analysis_query_valid(a,error) || !analysis_filters_valid(a,error) || !analysis_sorts_valid(a,error)) return WORKBOOK_SAVE_FAILED;
        if (!s->name || strlen(s->name)>4096 || (a->path && strlen(a->path)>65536) ||
            a->kind<SOURCE_NONE || a->kind>SOURCE_DEMO || a->filter_op>FILTER_OP_MAX || a->aggregate>AGG_MAX ||
            !isfinite(v->horizontal) || v->horizontal<0 || !isfinite(v->fraction) || v->fraction<0 || v->fraction>=1 ||
            (v->canvas_zoom && (!isfinite(v->canvas_zoom)||v->canvas_zoom<.5f||v->canvas_zoom>2.5f)) ||
            v->row>=DX_SHEET_ROWS || v->anchor_row>=DX_SHEET_ROWS || v->end_row>=DX_SHEET_ROWS || v->top>=DX_SHEET_ROWS || v->col>=16384 || v->anchor_col>=16384 || v->end_col>=16384 || v->selection>3 || v->column_count>16384 || (v->column_count && (!v->columns || !v->widths))) {
            error_set(error,"Invalid worksheet definition or presentation."); return WORKBOOK_SAVE_FAILED;
        }
        for (size_t c=0;c<v->column_count;c++) if (!v->columns[c] || !isfinite(v->widths[c]) || v->widths[c]<52 || v->widths[c]>1200) {
            error_set(error,"Invalid column presentation metadata."); return WORKBOOK_SAVE_FAILED;
        }
        if (!objects_valid(&s->objects,error) || !format_valid(&s->formats,error)) return WORKBOOK_SAVE_FAILED;
        for (size_t j=0;j<s->objects.count;j++) {
            const SheetObject *o=&s->objects.items[j];
            if (!o->id || !o->name || (unsigned)o->kind>OBJECT_TABLE || strlen(o->name)>4096 || !pivot_valid(&o->pivot,error) ||
                !objects_fits(&s->objects,(int)j,o->row,o->col,o->rows,o->columns,&s->cells,0,0,error)) { if (!*error) error_set(error,"Invalid sheet object."); return WORKBOOK_SAVE_FAILED; }
        }
    }
    unsigned minimum=workbook_codec_min_write_version(b); bool source_metadata=workbook_codec_content_version(b)==14;
    if (!workbook_identity_valid(b)) return range_fail(error,"Invalid workbook/worksheet identities; the destination was not changed.");
    if (source_metadata && !source_model_valid(b,error)) {
        char *detail=*error; *error=dx_printf("Unsupported workspace v14 source metadata (v12/v13 cannot preserve it): %s",detail ? detail : "invalid binding"); free(detail); return WORKBOOK_SAVE_FAILED;
    }
    if (!save_progress(&tracker,SAVE_PHASE_VALIDATE,true,error)) return WORKBOOK_SAVE_CANCELLED;
    char *target=workbook_target_path(path); if (!target) { error_set(error,"Workspace directory does not exist."); return WORKBOOK_SAVE_FAILED; }
    for (size_t i=0;i<b->count;i++) {
        const Worksheet *s=&b->sheets[i];
        if (s->analysis.path && *s->analysis.path && dx_same_file(target,s->analysis.path)) { free(target); error_set(error,"Choose a workbook destination other than a data source."); return WORKBOOK_SAVE_FAILED; }
        for (size_t j=0;j<s->objects.count;j++) if (s->objects.items[j].kind==OBJECT_TABLE && dx_same_file(target,s->objects.items[j].source.path)) { free(target); error_set(error,"Choose a workbook destination other than a table source."); return WORKBOOK_SAVE_FAILED; }
    }
    FILE *f=fopen(target,"rb"); bool existing=f!=NULL; if (f) fclose(f);
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_prepared_statement sheet=NULL,column=NULL,meta=NULL,cell=NULL;
    bool ok=false,transaction=false; char *temporary=NULL; int existing_version=0; unsigned write_version=minimum;
    if (!open_metadata(target,false,&db,&c,&temporary,error)) goto done;
    if (!version(c,existing,&existing_version,error)) goto done;
    if (!workbook_codec_select_write_version(minimum,existing_version,&write_version)) { error_set(error,"Unsupported or invalid Duxcel workspace version."); goto done; }
    if (existing_version>=14 && !source_archive_valid(c,target,existing_version,error)) goto done;
    if (write_version>=14 && (!source_package_inventory(c,write_version,false,error) || !source_model_valid(b,error) || !source_target_valid(b,target,error))) goto done;
    if (!save_progress(&tracker,SAVE_PHASE_PREPARE,true,error)) goto done;
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
    if (!prepare(c,"INSERT INTO _duxcel_workspace.meta(version,active_sheet,user_scale) VALUES (?,?,?)",&meta,error)) goto done;
    duckdb_bind_int32(meta,1,write_version); duckdb_bind_uint64(meta,2,b->active); duckdb_bind_double(meta,3,b->user_scale);
    if (!run(meta,error) || !prepare(c,"INSERT INTO _duxcel_workspace.sheets(" DX_WORKBOOK_SHEET_COLUMNS ",canvas_zoom) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",&sheet,error) ||
        !prepare(c,"INSERT INTO _duxcel_workspace.columns(sheet_id,column_id,name,width) VALUES (?,?,?,?)",&column,error) ||
        !prepare(c,"INSERT INTO _duxcel_workspace.cells(sheet_id,row_id,column_id,text) VALUES (?,?,?,?)",&cell,error)) goto done;
    if (!save_progress(&tracker,SAVE_PHASE_WRITE,true,error)) goto done;
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
        if (!run(sheet,error) || !save_progress(&tracker,SAVE_PHASE_WRITE,true,error)) goto done;
        for (size_t j=0;j<v->column_count;j++) {
            duckdb_bind_uint64(column,1,i); duckdb_bind_uint64(column,2,j); bind_text(column,3,v->columns[j]); duckdb_bind_double(column,4,v->widths[j]);
            if (!run(column,error) || !save_progress(&tracker,SAVE_PHASE_WRITE,true,error)) goto done;
        }
        for (size_t j=0;j<s->cells.count;j++) {
            const SheetCell *value=&s->cells.cells[j];
            duckdb_bind_uint64(cell,1,i); duckdb_bind_uint64(cell,2,value->row); duckdb_bind_uint64(cell,3,value->col); bind_text(cell,4,value->text);
            if (!run(cell,error) || !save_progress(&tracker,SAVE_PHASE_WRITE,true,error)) goto done;
        }
    }
    if (!save_objects(c,b,target,error) || !save_marks(c,b,error) || !save_progress(&tracker,SAVE_PHASE_WRITE,true,error)) goto done;
    if (!save_formats(c,b,error) || (write_version>=13 && !save_owner_package(c,b,write_version>=14,error)) || (write_version>=14 && !save_source_package(c,b,target,error)) || !save_progress(&tracker,SAVE_PHASE_WRITE,true,error)) goto done;
    if (!save_filters(c,b,error) || !save_sorts(c,b,error) || !save_queries(c,b,error) || !save_source_options(c,b,error) ||
        !exec(c,"CREATE TABLE IF NOT EXISTS _duxcel_workspace.preferences(name VARCHAR PRIMARY KEY,value BOOLEAN); DELETE FROM _duxcel_workspace.preferences WHERE name='reduce_motion' OR starts_with(name,'sheet_hidden_')",error) ||
        !exec(c,b->reduced_motion ? "INSERT INTO _duxcel_workspace.preferences(name,value) VALUES ('reduce_motion',true)" : "INSERT INTO _duxcel_workspace.preferences(name,value) VALUES ('reduce_motion',false)",error)) goto done;
    for (size_t i=0;i<b->count;i++) if (b->sheets[i].hidden) {
        char sql[128]; snprintf(sql,sizeof(sql),"INSERT INTO _duxcel_workspace.preferences(name,value) VALUES ('sheet_hidden_%zu',true)",i);
        if (!exec(c,sql,error)) goto done;
    }
    if (!save_progress(&tracker,SAVE_PHASE_WRITE,true,error)) goto done;
    if (!save_identity_package(c,b,error) || !source_package_shape(c,write_version,error)) goto done;
    if (!save_progress(&tracker,SAVE_PHASE_WRITE,true,error) || !save_progress(&tracker,SAVE_PHASE_PUBLISH,false,error)) goto done;
    if (!exec(c,"COMMIT",error)) goto done;
    transaction=false; ok=true; save_progress(&tracker,SAVE_PHASE_COMPLETE,true,error);
 done:
    if (transaction) { char *ignored=NULL; exec(c,"ROLLBACK",&ignored); free(ignored); }
    if (sheet) duckdb_destroy_prepare(&sheet); if (column) duckdb_destroy_prepare(&column); if (meta) duckdb_destroy_prepare(&meta);
    if (cell) duckdb_destroy_prepare(&cell);
    if (c) duckdb_disconnect(&c); if (db) duckdb_close(&db);
    dx_remove_temp_directory(temporary); free(temporary);
    if (!ok && !existing) remove(target);
    free(target); return ok ? WORKBOOK_SAVE_COMPLETE : tracker.cancelled ? WORKBOOK_SAVE_CANCELLED : WORKBOOK_SAVE_FAILED;
}
bool workbook_save(const Workbook *b,const char *path,char **error) {
    return workbook_save_controlled(b,path,(WorkbookSaveControl){0},error)==WORKBOOK_SAVE_COMPLETE;
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
/* Bound restored heap state before fetching binary/text/membership payloads.
   DuckDB result storage, one <=256 KiB packet conversion buffer and bounded
   <=64 KiB locator conversions are separate staging,
   not a claim about allocator/process peak or future App history reservations. */
static bool source_package_budget(duckdb_connection c,const Workbook *book,char **error) {
    char *sql=dx_printf("SELECT sheet_id,CASE WHEN sum(bytes)<=1048576 THEN sum(bytes)::UBIGINT ELSE 1048577::UBIGINT END FROM ("
        "SELECT sheet_id,count(*)::HUGEINT*%zu bytes FROM _duxcel_workspace.formats GROUP BY sheet_id UNION ALL "
        "SELECT sheet_id,count(*)::HUGEINT*%zu+sum(coalesce(octet_length(definition),0)+1) FROM _duxcel_workspace.format_scopes GROUP BY sheet_id UNION ALL "
        "SELECT sheet_id,count(*)::HUGEINT*%zu FROM _duxcel_workspace.format_guards GROUP BY sheet_id UNION ALL "
        "SELECT sheet_id,count(*)::HUGEINT*%zu FROM _duxcel_workspace.format_records GROUP BY sheet_id UNION ALL "
        "SELECT sheet_id,count(*)::HUGEINT*%zu FROM _duxcel_workspace.format_fields GROUP BY sheet_id UNION ALL "
        "SELECT sheet_id,count(*)::HUGEINT*%zu+sum(coalesce(octet_length(encode(source_reference)),0)+1+coalesce(octet_length(definition),0)) FROM _duxcel_workspace.source_versions GROUP BY sheet_id"
        ") owned GROUP BY sheet_id LIMIT 65",sizeof(FormatPatch),sizeof(FormatScope),sizeof(size_t),sizeof(uint64_t),sizeof(size_t),sizeof(SourceVersion));
    duckdb_result r={0}; bool queried=duckdb_query(c,sql,&r)==DuckDBSuccess; free(sql);
    if (!queried) { error_set(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return false; }
    size_t total=0; bool valid=duckdb_row_count(&r)<=book->count;
    for (idx_t i=0;valid && i<duckdb_row_count(&r);i++) {
        int sheet=duckdb_value_int32(&r,0,i); uint64_t bytes=duckdb_value_uint64(&r,1,i);
        valid=!duckdb_value_is_null(&r,0,i) && sheet>=0 && (size_t)sheet<book->count && bytes<=DX_FORMAT_CURRENT_BYTES && total<=8u*1024u*1024u-bytes;
        if (valid) total+=(size_t)bytes;
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!valid) return range_fail(error,"Workspace v14 restored formats exceed their sheet/workbook owned budget.");

    /* Independent scalar checks avoid a nine-way join-order search. EXISTS
       always yields a non-NULL Boolean; bool_or keeps the exact same limits. */
    const char *limits="SELECT bool_or(invalid) FROM ("
        "SELECT EXISTS(SELECT sheet_id FROM _duxcel_workspace.source_versions GROUP BY sheet_id HAVING count(*)>1024) AS invalid"
        " UNION ALL SELECT EXISTS(SELECT 1 FROM _duxcel_workspace.source_versions WHERE source_reference IS NULL OR octet_length(encode(source_reference))>65536 OR contains(source_reference,chr(0)) OR octet_length(definition)>=262144) AS invalid"
        " UNION ALL SELECT EXISTS(SELECT 1 FROM _duxcel_workspace.sheets WHERE path IS NULL OR octet_length(encode(path))>65536 OR contains(path,chr(0))) AS invalid"
        " UNION ALL SELECT EXISTS(SELECT 1 FROM _duxcel_workspace.table_sources WHERE path IS NULL OR octet_length(encode(path))>65536 OR contains(path,chr(0))) AS invalid"
        " UNION ALL SELECT EXISTS(SELECT sheet_id FROM _duxcel_workspace.format_scopes GROUP BY sheet_id HAVING count(*)>512) AS invalid"
        " UNION ALL SELECT EXISTS(SELECT 1 FROM _duxcel_workspace.format_scopes WHERE definition IS NULL OR octet_length(definition)>=262144) AS invalid"
        " UNION ALL SELECT EXISTS(SELECT sheet_id,position FROM _duxcel_workspace.format_guards GROUP BY sheet_id,position HAVING count(*)>33) AS invalid"
        " UNION ALL SELECT EXISTS(SELECT sheet_id,position FROM _duxcel_workspace.format_records GROUP BY sheet_id,position HAVING count(*)>20000) AS invalid"
        " UNION ALL SELECT EXISTS(SELECT sheet_id,position FROM _duxcel_workspace.format_fields GROUP BY sheet_id,position HAVING count(*)>16384) AS invalid"
        ") checks";
    if (duckdb_query(c,limits,&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return false; }
    valid=!duckdb_value_boolean(&r,0,0); duckdb_destroy_result(&r);
    return valid || range_fail(error,"Workspace v14 metadata exceeds bounded counts/text or contains an embedded NUL.");
}
static bool load_owner_package(duckdb_connection c,Workbook *book,bool scoped,char **error) {
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
        int kind=duckdb_value_int32(&r,2,i);
        if (duckdb_value_is_null(&r,0,i) || duckdb_value_is_null(&r,1,i) || duckdb_value_is_null(&r,2,i) || sheet<0 || (size_t)sheet>=book->count || position<0 || (size_t)position!=counts[sheet]++ || (size_t)position>=book->sheets[sheet].formats.count || (scoped ? kind<FORMAT_SPATIAL || kind>FORMAT_RECORDS : kind!=FORMAT_SPATIAL)) goto invalid;
        book->sheets[sheet].formats.items[position].selector=(FormatSelector)kind;
    }
    ok=true; goto done;
invalid: error_set(error,scoped ? "Invalid workspace v14 owner counters or supported selector metadata." : "Invalid workspace v13 owner counters or spatial selector metadata."); goto done;
sql_error: error_set(error,duckdb_result_error(&r));
done: duckdb_destroy_result(&r); return ok;
}
static bool source_restored_room(const Workbook *book,size_t sheet,size_t extra,char **error) {
    size_t total=0;
    for (size_t i=0;i<book->count;i++) {
        size_t formats=format_owned_bytes(&book->sheets[i].formats,false),ledger=book->sheets[i].source_versions.bytes,add=i==sheet ? extra : 0;
        if (formats>DX_FORMAT_CURRENT_BYTES || ledger>DX_FORMAT_CURRENT_BYTES-formats || add>DX_FORMAT_CURRENT_BYTES-formats-ledger || total>8u*1024u*1024u-formats-ledger-add) return range_fail(error,"Workspace v14 restored formats exceed the owned memory budget.");
        total+=formats+ledger+add;
    }
    return true;
}
static char *source_reference_value(duckdb_result *r,idx_t column,idx_t row,const char *target) {
    if (duckdb_value_is_null(r,column,row)) return NULL;
    duckdb_string value=duckdb_value_string(r,column,row); char *path=NULL;
    if (value.data && value.size && value.size<=DX_SOURCE_SCOPE_TEXT_BYTES && !memchr(value.data,0,(size_t)value.size)) {
        char *ref=dx_alloc((size_t)value.size+1); memcpy(ref,value.data,(size_t)value.size); path=workbook_source_path(target,ref); free(ref);
    }
    duckdb_free(value.data); return path;
}
static bool load_source_members(duckdb_connection c,Workbook *book,bool fields,char **error) {
    duckdb_result r={0}; size_t counts[DX_MAX_SHEETS][DX_FORMAT_PATCHES]={{0}}; bool ok=false;
    const char *sql=fields ? "SELECT sheet_id,position,field_position,source_ordinal FROM _duxcel_workspace.format_fields ORDER BY sheet_id,position,field_position LIMIT 1048577" : "SELECT sheet_id,position,member_position,record_id FROM _duxcel_workspace.format_records ORDER BY sheet_id,position,member_position LIMIT 1048577";
    if (duckdb_query(c,sql,&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)>1048576) goto invalid;
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        for (idx_t col=0;col<4;col++) if (duckdb_value_is_null(&r,col,i)) goto invalid;
        int sheet=duckdb_value_int32(&r,0,i),position=duckdb_value_int32(&r,1,i),member=duckdb_value_int32(&r,2,i);
        if (sheet<0 || (size_t)sheet>=book->count || position<0 || (size_t)position>=book->sheets[sheet].formats.count || book->sheets[sheet].formats.items[position].selector!=FORMAT_RECORDS || member<0 || (size_t)member!=counts[sheet][position] || counts[sheet][position]>=(fields ? DX_FORMAT_FIELDS : DX_FORMAT_RECORDS)) goto invalid;
        counts[sheet][position]++;
    }
    for (size_t s=0;s<book->count;s++) for (size_t p=0;p<book->sheets[s].formats.count;p++) if (counts[s][p]) {
        FormatPatch *patch=book->sheets[s].formats.items+p; size_t bytes=counts[s][p]*(fields ? sizeof(size_t) : sizeof(uint64_t));
        if (!source_restored_room(book,s,bytes,error)) goto done;
        if (fields) { patch->fields=dx_alloc(bytes); patch->field_count=counts[s][p]; } else { patch->record_ids=dx_alloc(bytes); patch->record_count=counts[s][p]; }
    }
    memset(counts,0,sizeof(counts));
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        size_t s=(size_t)duckdb_value_int32(&r,0,i),p=(size_t)duckdb_value_int32(&r,1,i),at=counts[s][p]++; FormatPatch *patch=book->sheets[s].formats.items+p;
        if (fields) { int value=duckdb_value_int32(&r,3,i); if (value<0 || value>=16384) goto invalid; patch->fields[at]=(size_t)value; }
        else { uint64_t value=duckdb_value_uint64(&r,3,i); if (!value) goto invalid; patch->record_ids[at]=value; }
    }
    ok=true; goto done;
invalid: error_set(error,"Invalid workspace v14 record/field membership metadata."); goto done;
sql_error: error_set(error,duckdb_result_error(&r));
done: duckdb_destroy_result(&r); return ok;
}
static bool load_source_package(duckdb_connection c,Workbook *book,const char *target,char **error) {
    duckdb_result r={0}; bool ok=false;
    if (!source_package_budget(c,book,error)) return false;
    if (duckdb_query(c,"SELECT sheet_id,source_aware FROM _duxcel_workspace.format_state ORDER BY sheet_id LIMIT 65",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)!=book->count) goto invalid;
    for (size_t i=0;i<book->count;i++) {
        if (duckdb_value_is_null(&r,0,i) || duckdb_value_is_null(&r,1,i) || duckdb_value_int32(&r,0,i)!=(int32_t)i) goto invalid;
        book->sheets[i].formats.source_aware=duckdb_value_boolean(&r,1,i);
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,high_water FROM _duxcel_workspace.source_version_counters ORDER BY sheet_id LIMIT 65",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)!=book->count) goto invalid;
    for (size_t i=0;i<book->count;i++) {
        if (duckdb_value_is_null(&r,0,i) || duckdb_value_is_null(&r,1,i) || duckdb_value_int32(&r,0,i)!=(int32_t)i) goto invalid;
        book->sheets[i].source_versions.high_water=duckdb_value_uint64(&r,1,i);
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,version,owner,source_kind,source_reference,encoding,definition FROM _duxcel_workspace.source_versions ORDER BY sheet_id,version LIMIT 65537",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)>DX_MAX_SHEETS*DX_SOURCE_VERSIONS) goto invalid;
    size_t counts[DX_MAX_SHEETS]={0};
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        for (idx_t col=0;col<6;col++) if (duckdb_value_is_null(&r,col,i)) goto invalid;
        int sheet=duckdb_value_int32(&r,0,i); if (sheet<0 || (size_t)sheet>=book->count || ++counts[sheet]>DX_SOURCE_VERSIONS) goto invalid;
    }
    for (size_t i=0;i<book->count;i++) if (counts[i]) {
        SourceVersions *versions=&book->sheets[i].source_versions; versions->capacity=counts[i]; versions->bytes=counts[i]*sizeof(SourceVersion);
        if (!source_restored_room(book,i,0,error)) goto done;
        versions->items=dx_alloc(versions->bytes);
    }
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        size_t s=(size_t)duckdb_value_int32(&r,0,i); SourceVersions *versions=&book->sheets[s].source_versions;
        uint64_t version=duckdb_value_uint64(&r,1,i),owner=duckdb_value_uint64(&r,2,i); int kind=duckdb_value_int32(&r,3,i),encoding=duckdb_value_int32(&r,5,i);
        bool pending=duckdb_value_is_null(&r,6,i);
        if (!version || version>versions->high_water || (versions->count && version<=versions->items[versions->count-1].version) || (kind!=SOURCE_CSV && kind!=SOURCE_PARQUET) || (pending ? encoding!=0 : encoding!=1)) goto invalid;
        char *path=source_reference_value(&r,4,i,target); if (!path) goto invalid;
        size_t bytes=strlen(path)+1;
        if (bytes>DX_SOURCE_SCOPE_TEXT_BYTES+1) { free(path); goto invalid; }
        if (!source_restored_room(book,s,bytes,error)) { free(path); goto done; }
        SourceVersion *entry=versions->items+versions->count++; *entry=(SourceVersion){.owner=owner,.version=version,.kind=(SourceKind)kind,.path=path}; versions->bytes+=bytes;
        if (!pending) {
            duckdb_blob blob=duckdb_value_blob(&r,6,i); SourceScopeInfo info;
            bool valid=source_scope_validate(blob.data,(size_t)blob.size,&info,error) && info.kind==SOURCE_SCOPE_RAW && info.owner==owner && info.version==version && info.source_kind==(SourceKind)kind;
            if (!valid) { duckdb_free(blob.data); if (!*error) error_set(error,"Invalid workspace v14 canonical source proof."); goto done; }
            if (!source_restored_room(book,s,(size_t)blob.size,error)) { duckdb_free(blob.data); goto done; }
            entry->raw=(SourcePacket){dx_alloc((size_t)blob.size),(size_t)blob.size}; memcpy(entry->raw.data,blob.data,(size_t)blob.size); versions->bytes+=(size_t)blob.size; duckdb_free(blob.data);
        }
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,owner,version FROM _duxcel_workspace.source_bindings ORDER BY sheet_id,owner LIMIT 2113",&r)!=DuckDBSuccess) goto sql_error;
    size_t expected=book->count; for (size_t s=0;s<book->count;s++) for (size_t j=0;j<book->sheets[s].objects.count;j++) expected+=book->sheets[s].objects.items[j].kind==OBJECT_TABLE;
    if (duckdb_row_count(&r)!=expected) goto invalid;
    bool primary[DX_MAX_SHEETS]={0},tables[DX_MAX_SHEETS][DX_MAX_OBJECTS]={{0}};
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        for (idx_t col=0;col<3;col++) if (duckdb_value_is_null(&r,col,i)) goto invalid;
        int s=duckdb_value_int32(&r,0,i); uint64_t owner=duckdb_value_uint64(&r,1,i),version=duckdb_value_uint64(&r,2,i);
        if (s<0 || (size_t)s>=book->count) goto invalid; Worksheet *sheet=book->sheets+s;
        if (!owner) { if (primary[s]) goto invalid; primary[s]=true; sheet->analysis.source_version=version; }
        else {
            size_t j=0; while (j<sheet->objects.count && sheet->objects.items[j].id!=owner) j++;
            if (j==sheet->objects.count || sheet->objects.items[j].kind!=OBJECT_TABLE || tables[s][j]) goto invalid;
            tables[s][j]=true; sheet->objects.items[j].source.source_version=version;
        }
    }
    for (size_t s=0;s<book->count;s++) { if (!primary[s]) goto invalid; for (size_t j=0;j<book->sheets[s].objects.count;j++) if (book->sheets[s].objects.items[j].kind==OBJECT_TABLE && !tables[s][j]) goto invalid; }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (duckdb_query(c,"SELECT sheet_id,scope_id,owner,encoding,definition FROM _duxcel_workspace.format_scopes ORDER BY sheet_id,scope_id LIMIT 32769",&r)!=DuckDBSuccess) goto sql_error;
    if (duckdb_row_count(&r)>DX_MAX_SHEETS*DX_FORMAT_PATCHES) goto invalid; memset(counts,0,sizeof(counts));
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        for (idx_t col=0;col<5;col++) if (duckdb_value_is_null(&r,col,i)) goto invalid;
        int s=duckdb_value_int32(&r,0,i),id=duckdb_value_int32(&r,1,i);
        if (s<0 || (size_t)s>=book->count || id<0 || (size_t)id!=counts[s]++ || counts[s]>DX_FORMAT_PATCHES || duckdb_value_int32(&r,3,i)!=1) goto invalid;
    }
    for (size_t s=0;s<book->count;s++) if (counts[s]) {
        FormatStore *formats=&book->sheets[s].formats;
        if (!source_restored_room(book,s,counts[s]*(sizeof(FormatScope)+1),error)) goto done;
        formats->scopes=dx_alloc(counts[s]*sizeof(FormatScope)); formats->scope_count=counts[s];
    }
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        size_t s=(size_t)duckdb_value_int32(&r,0,i),id=(size_t)duckdb_value_int32(&r,1,i); duckdb_blob blob=duckdb_value_blob(&r,4,i); SourceScopeInfo info;
        if (!source_scope_validate(blob.data,(size_t)blob.size,&info,error) || !source_restored_room(book,s,(size_t)blob.size,error)) { duckdb_free(blob.data); goto done; }
        FormatScope *scope=book->sheets[s].formats.scopes+id; scope->owner=duckdb_value_uint64(&r,2,i); scope->bytes=(size_t)blob.size; scope->definition=dx_alloc(scope->bytes+1); memcpy(scope->definition,blob.data,scope->bytes); duckdb_free(blob.data);
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    const size_t guard_limit=(size_t)DX_MAX_SHEETS*DX_FORMAT_PATCHES*DX_FORMAT_GUARDS;
    char *guard_sql=dx_printf("SELECT sheet_id,position,guard_position,scope_id FROM _duxcel_workspace.format_guards ORDER BY sheet_id,position,guard_position LIMIT %zu",guard_limit+1);
    bool guards_queried=duckdb_query(c,guard_sql,&r)==DuckDBSuccess; free(guard_sql); if (!guards_queried) goto sql_error;
    if (duckdb_row_count(&r)>guard_limit) goto invalid;
    /* Count the complete bounded vectors before allocation. The staged patches
       already account for those slots when checking restored path/format room. */
    int previous_sheet=-1,previous_patch=-1; size_t ids[DX_FORMAT_GUARDS]={0};
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        for (idx_t col=0;col<4;col++) if (duckdb_value_is_null(&r,col,i)) goto invalid;
        int s=duckdb_value_int32(&r,0,i),p=duckdb_value_int32(&r,1,i),g=duckdb_value_int32(&r,2,i),id=duckdb_value_int32(&r,3,i);
        if (s<0 || (size_t)s>=book->count || p<0 || (size_t)p>=book->sheets[s].formats.count || g<0 || id<0 || (size_t)id>=book->sheets[s].formats.scope_count) goto invalid;
        FormatPatch *patch=book->sheets[s].formats.items+p;
        if (patch->selector==FORMAT_SPATIAL || patch->guard_count>=DX_FORMAT_GUARDS || (size_t)g!=patch->guard_count || (patch->selector!=FORMAT_GUARDED && patch->guard_count)) goto invalid;
        if (s!=previous_sheet || p!=previous_patch) { previous_sheet=s; previous_patch=p; memset(ids,0,sizeof(ids)); }
        for (size_t j=0;j<patch->guard_count;j++) if (ids[j]==(size_t)id) goto invalid;
        ids[patch->guard_count++]=(size_t)id;
    }
    if (!source_restored_room(book,0,0,error)) goto done;
    for (size_t s=0;s<book->count;s++) for (size_t p=0;p<book->sheets[s].formats.count;p++) {
        FormatPatch *patch=book->sheets[s].formats.items+p;
        if (patch->guard_count) patch->guard_ids=dx_alloc(patch->guard_count*sizeof(*patch->guard_ids));
    }
    for (idx_t i=0;i<duckdb_row_count(&r);i++) {
        size_t s=(size_t)duckdb_value_int32(&r,0,i),p=(size_t)duckdb_value_int32(&r,1,i),g=(size_t)duckdb_value_int32(&r,2,i);
        book->sheets[s].formats.items[p].guard_ids[g]=(size_t)duckdb_value_int32(&r,3,i);
    }
    duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!load_source_members(c,book,false,error) || !load_source_members(c,book,true,error)) goto done;
    ok=source_model_valid(book,error); goto done;
invalid: error_set(error,"Invalid workspace v14 source-version/scope metadata."); goto done;
sql_error: error_set(error,duckdb_result_error(&r));
done: duckdb_destroy_result(&r); return ok;
}
/* The earliest valid files lack canvas_zoom. Select the stable named columns
   and supply its legacy default, independent of physical table column order. */
static bool load_sheet_rows(duckdb_connection c,duckdb_result *out,char **error) {
    duckdb_result r={0};
    if (duckdb_query(c,"SELECT count(*) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='sheets' AND column_name='canvas_zoom'",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return false; }
    bool canvas=duckdb_value_uint64(&r,0,0)!=0; duckdb_destroy_result(&r);
    char *sql=dx_printf("SELECT " DX_WORKBOOK_SHEET_COLUMNS ",%s AS canvas_zoom FROM _duxcel_workspace.sheets ORDER BY id LIMIT 65",canvas ? "canvas_zoom" : "1::DOUBLE");
    bool ok=duckdb_query(c,sql,out)==DuckDBSuccess; free(sql);
    if (!ok) error_set(error,duckdb_result_error(out)); return ok;
}
/* Shared with ordinary loading; archive preflight must not interpret source
   definitions differently or query the files that those definitions name. */
static bool load_sheet_analysis(duckdb_result *r,idx_t row,const char *target,Analysis *out,char **error) {
    char *ref=text_value(r,3,row); Analysis a={0};
    int kind=duckdb_value_int32(r,2,row),op=duckdb_value_int32(r,8,row),aggregate=duckdb_value_int32(r,11,row);
    if (!ref || strlen(ref)>65536 || kind<SOURCE_NONE || kind>SOURCE_DEMO || op<0 || op>FILTER_OP_MAX || aggregate<0 || aggregate>AGG_MAX) { free(ref); return range_fail(error,"Invalid worksheet source definition."); }
    a.kind=(SourceKind)kind; a.path=kind==SOURCE_DEMO || kind==SOURCE_NONE ? dx_strdup("") : workbook_source_path(target,ref); free(ref);
    a.schema=text_value(r,4,row); a.table=text_value(r,5,row); a.filter_column=text_value(r,6,row); a.filter_value=text_value(r,7,row); a.filter_op=(FilterOp)op;
    a.group_column=text_value(r,9,row); a.measure_column=text_value(r,10,row); a.aggregate=(Aggregate)aggregate; a.sort_column=text_value(r,12,row); a.descending=duckdb_value_boolean(r,13,row);
    *out=a; return true;
}
static bool read_workbook(duckdb_connection c,const char *target,int workspace_version,Workbook *out,char **error);
/* version() has already validated the exact v14 package on this connection.
   Use the ordinary staged reader so archived overwrite rejects the same invalid
   core/source state as Open, without a second connection or interpretation. */
static bool source_archive_valid(duckdb_connection c,const char *target,int package,char **error) {
    Workbook staged; workbook_init(&staged); bool ok=read_workbook(c,target,package,&staged,error); workbook_free(&staged); return ok;
}
static bool read_workbook(duckdb_connection c,const char *target,int workspace_version,Workbook *out,char **error) {
    Workbook book; workbook_init(&book); duckdb_result r={0}; bool ok=false;
    if (duckdb_query(c,"SELECT active_sheet,user_scale FROM _duxcel_workspace.meta",&r)!=DuckDBSuccess) { error_set(error,duckdb_result_error(&r)); goto done; }
    size_t active=(size_t)duckdb_value_uint64(&r,0,0); double zoom=duckdb_value_double(&r,1,0); duckdb_destroy_result(&r); memset(&r,0,sizeof(r));
    if (!isfinite(zoom) || zoom<.8 || zoom>1.5) { error_set(error,"Invalid workspace zoom."); goto done; }
    if (!load_sheet_rows(c,&r,error)) goto done;
    size_t n=(size_t)duckdb_row_count(&r);
    idx_t fields=duckdb_column_count(&r);
    if (!n || n>DX_MAX_SHEETS || active>=n || (fields!=24 && fields!=25)) { error_set(error,"Invalid worksheet metadata."); goto done; }
    for (size_t i=0;i<n;i++) {
        if (duckdb_value_uint64(&r,0,i)!=i) { error_set(error,"Invalid worksheet ordering."); goto done; }
        char *name=text_value(&r,1,i); Analysis a={0};
        if (!name || strlen(name)>4096 || !load_sheet_analysis(&r,i,target,&a,error)) { free(name); analysis_free(&a); if (!*error) error_set(error,"Invalid worksheet definition."); goto done; }
        bool added=workbook_add(&book,name,&a,error); free(name); analysis_free(&a);
        if (!added) goto done;
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
    if (workspace_version>=15 && !load_identity_package(c,&book,error)) goto done;
    if (workspace_version>=14 && !source_package_budget(c,&book,error)) goto done;
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
    if (!load_filters(c,&book,error) || !load_sorts(c,&book,error) || !load_objects(c,&book,target,error) || !load_formats(c,&book,error) || !load_queries(c,&book,error) || !load_source_options(c,&book,error) || (workspace_version>=13 && !load_owner_package(c,&book,workspace_version>=14,error)) || (workspace_version>=14 && !load_source_package(c,&book,target,error))) goto done;
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
    book.active=active; book.user_scale=(float)zoom; book.path=dx_strdup(target); book.dirty=false;
    workbook_free(out); *out=book; workbook_init(&book); ok=true;
 done:
    duckdb_destroy_result(&r); workbook_free(&book); return ok;
}
bool workbook_load(Workbook *out,const char *path,char **error) {
    char *target=dx_absolute_path(path); if (!target) { error_set(error,"Workspace file is missing or inaccessible."); return false; }
    duckdb_database db=NULL; duckdb_connection c=NULL; char *temporary=NULL; int workspace_version=0;
    bool ok=open_metadata(target,true,&db,&c,&temporary,error) && version(c,true,&workspace_version,error) && read_workbook(c,target,workspace_version,out,error);
    if (c) duckdb_disconnect(&c); if (db) duckdb_close(&db); dx_remove_temp_directory(temporary); free(temporary); free(target); return ok;
}
#endif
