#include "lifecycle.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
    WorkbookSheetOperation operation;
    uint64_t order,id,active;
    size_t position;
    bool hidden;
    char *name;
    Worksheet *detached;
} SheetEvent;
struct WorkbookLifecycle {
    SheetEvent undo[DX_HISTORY],redo[DX_HISTORY];
    size_t undo_count,redo_count;
    uint64_t latest,floor;
};
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static size_t locate(const Workbook *b,uint64_t id) {
    for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return i;
    return b->count;
}
static void event_free(SheetEvent *e) {
    free(e->name);
    if (e->detached) { worksheet_free(e->detached); free(e->detached); }
    memset(e,0,sizeof(*e));
}
void workbook_lifecycle_clear_redo(Workbook *b) {
    struct WorkbookLifecycle *h=b->lifecycle; if (!h) return;
    for (size_t i=0;i<h->redo_count;i++) event_free(h->redo+i);
    h->redo_count=0;
    for (size_t i=0;i<h->undo_count;i++) if (h->undo[i].detached) worksheet_history_clear_redo(h->undo[i].detached);
}
void workbook_lifecycle_free(Workbook *b) {
    if (!b->lifecycle) return;
    workbook_lifecycle_clear_redo(b);
    for (size_t i=0;i<b->lifecycle->undo_count;i++) event_free(b->lifecycle->undo+i);
    free(b->lifecycle); b->lifecycle=NULL;
}
uint64_t workbook_lifecycle_latest(const Workbook *b) { return b->lifecycle ? b->lifecycle->latest : 0; }
uint64_t workbook_lifecycle_floor(const Workbook *b) { return b->lifecycle ? b->lifecycle->floor : 0; }
static void push(struct WorkbookLifecycle *h,bool redo,SheetEvent event) {
    SheetEvent *stack=redo ? h->redo : h->undo;
    size_t *count=redo ? &h->redo_count : &h->undo_count;
    /* Total retained lifecycle events never exceeds DX_HISTORY: transfer does
       not allocate a new event, and new commands discard the Redo branch. */
    if (*count==DX_HISTORY) {
        if (stack[0].order>h->floor) h->floor=stack[0].order;
        event_free(stack); memmove(stack,stack+1,(DX_HISTORY-1)*sizeof(*stack)); --*count;
    }
    stack[(*count)++]=event;
}
bool workbook_lifecycle_peek(const Workbook *b,bool redo,WorkbookHistoryResult *out,char **error) {
    const struct WorkbookLifecycle *h=b->lifecycle; if (!h) return true;
    if (h->undo_count+h->redo_count>DX_HISTORY || !h->latest || h->floor>h->latest)
        return fail(error,"Invalid worksheet lifecycle history.");
    for (unsigned side=0;side<2;side++) {
        const SheetEvent *events=side ? h->redo : h->undo; size_t n=side ? h->redo_count : h->undo_count;
        for (size_t i=0;i<n;i++) if (!events[i].id || !events[i].order || events[i].order>h->latest ||
            (unsigned)events[i].operation>SHEET_SHOW ||
            (i && (side ? events[i].order>=events[i-1].order : events[i].order<=events[i-1].order)))
            return fail(error,"Incomplete worksheet lifecycle history.");
    }
    size_t count=redo ? h->redo_count : h->undo_count; if (!count) return true;
    const SheetEvent *e=(redo ? h->redo : h->undo)+count-1;
    *out=(WorkbookHistoryResult){.status=WORKBOOK_HISTORY_READY,.sheet=locate(b,e->id),.sheet_id=e->id,.order=e->order,.lifecycle=true};
    return true;
}
static Worksheet *remove_sheet(Workbook *b,size_t index) {
    Worksheet *detached=dx_alloc(sizeof(*detached)); *detached=b->sheets[index];
    uint64_t active=b->sheets[b->active].id;
    memmove(b->sheets+index,b->sheets+index+1,(b->count-index-1)*sizeof(*b->sheets));
    memset(b->sheets+--b->count,0,sizeof(*b->sheets));
    b->active=locate(b,active);
    if (b->active==b->count) b->active=workbook_visible_sheet(b,0);
    return detached;
}
static void publish(Workbook *b,SheetEvent event) {
    if (!b->lifecycle) { b->lifecycle=dx_alloc(sizeof(*b->lifecycle)); memset(b->lifecycle,0,sizeof(*b->lifecycle)); }
    event.order=worksheet_history_issue_order(); b->lifecycle->latest=event.order;
    /* Publish first so the branch stamp remains monotonic even if the sheet
       owning the newest cell edit has just been detached. */
    if (event.detached) worksheet_history_clear_redo(event.detached);
    workbook_history_note_edit(b); push(b->lifecycle,false,event);
    b->command_revision++; b->dirty=true;
}
bool workbook_sheet_command(Workbook *b,WorkbookSheetCommand c,bool *changed,char **error) {
    if (changed) *changed=false;
    if (!workbook_identity_valid(b) || !b->count || b->active>=b->count || (unsigned)c.operation>SHEET_SHOW ||
        (c.operation!=SHEET_CREATE && c.sheet>=b->count)) return fail(error,"Choose an existing worksheet document.");
    WorkbookHistoryResult history=workbook_history_peek(b,false,error);
    if (history.status==WORKBOOK_HISTORY_INVALID) return false;
    if (c.operation==SHEET_REORDER && c.destination>=b->count) return fail(error,"Choose an existing worksheet position.");
    if ((c.operation==SHEET_HIDE && b->sheets[c.sheet].hidden) || (c.operation==SHEET_SHOW && !b->sheets[c.sheet].hidden) ||
        (c.operation==SHEET_REORDER && c.sheet==c.destination)) return true;
    char *prepared=NULL;
    if (c.operation==SHEET_CREATE || c.operation==SHEET_RENAME) {
        prepared=workbook_prepare_sheet_name(b,c.operation==SHEET_CREATE ? b->count : c.sheet,c.name,error);
        if (!prepared) return false;
        if (c.operation==SHEET_RENAME && !strcmp(prepared,b->sheets[c.sheet].name)) { free(prepared); return true; }
    }
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available()) { free(prepared); return fail(error,"Worksheet command revision space is exhausted."); }
    SheetEvent event={.operation=c.operation,.position=c.sheet,.active=b->sheets[b->active].id};
    if (c.operation!=SHEET_CREATE) event.id=b->sheets[c.sheet].id;
    switch (c.operation) {
    case SHEET_CREATE:
        if (!c.analysis) { free(prepared); return fail(error,"A new worksheet needs a definition."); }
        if (!workbook_add(b,prepared,c.analysis,error)) { free(prepared); return false; }
        free(prepared);
        event.position=b->active; event.id=b->sheets[b->active].id; break;
    case SHEET_DUPLICATE:
        if (!workbook_duplicate(b,c.sheet,error)) return false;
        event.position=b->active; event.id=b->sheets[b->active].id; break;
    case SHEET_DELETE:
        if (!b->sheets[c.sheet].hidden && workbook_visible_count(b)<=1) return fail(error,"Keep at least one worksheet visible.");
        event.detached=remove_sheet(b,c.sheet); break;
    case SHEET_RENAME:
        event.name=b->sheets[c.sheet].name; b->sheets[c.sheet].name=prepared; break;
    case SHEET_REORDER:
        if (!workbook_reorder(b,c.sheet,c.destination,error)) return false; break;
    case SHEET_HIDE: case SHEET_SHOW:
        event.hidden=b->sheets[c.sheet].hidden;
        if (!workbook_hide(b,c.sheet,c.operation==SHEET_HIDE,error)) return false; break;
    }
    publish(b,event); if (changed) *changed=true; return true;
}
WorkbookHistoryResult workbook_lifecycle_step(Workbook *b,bool redo,char **error) {
    WorkbookHistoryResult result=workbook_history_peek(b,redo,error);
    if (result.status!=WORKBOOK_HISTORY_READY || !result.lifecycle) return result;
    struct WorkbookLifecycle *h=b->lifecycle;
    SheetEvent *e=(redo ? h->redo : h->undo)+(redo ? h->redo_count : h->undo_count)-1;
    size_t index=locate(b,e->id); bool presence=e->operation<=SHEET_DELETE;
    if ((e->operation==SHEET_REORDER && e->position>=b->count) ||
        ((e->operation==SHEET_HIDE || e->operation==SHEET_SHOW) && e->hidden && index<b->count && !b->sheets[index].hidden && workbook_visible_count(b)<=1) ||
        (!e->detached && index==b->count) || (e->detached && (index!=b->count || b->count==DX_MAX_SHEETS || e->position>b->count)) ||
        (presence && !e->detached && !b->sheets[index].hidden && workbook_visible_count(b)<=1)) {
        result.status=WORKBOOK_HISTORY_INVALID; fail(error,"Worksheet history no longer matches this document."); return result;
    }
    workbook_history_sync(b);
    uint64_t active=b->sheets[b->active].id;
    if (presence) {
        if (e->detached) {
            b->sheets=dx_realloc(b->sheets,(b->count+1)*sizeof(*b->sheets));
            memmove(b->sheets+e->position+1,b->sheets+e->position,(b->count-e->position)*sizeof(*b->sheets));
            b->sheets[e->position]=*e->detached; free(e->detached); e->detached=NULL; b->count++;
        } else { e->position=index; e->detached=remove_sheet(b,index); }
    } else if (e->operation==SHEET_RENAME) {
        char *name=b->sheets[index].name; b->sheets[index].name=e->name; e->name=name;
    } else if (e->operation==SHEET_REORDER) {
        workbook_reorder(b,index,e->position,NULL); e->position=index;
    } else {
        bool hidden=b->sheets[index].hidden; b->sheets[index].hidden=e->hidden; e->hidden=hidden;
    }
    size_t next=locate(b,e->active); e->active=active;
    b->active=next<b->count && !b->sheets[next].hidden ? next : workbook_visible_sheet(b,0);
    SheetEvent moved=*e; memset(e,0,sizeof(*e));
    if (redo) h->redo_count--; else h->undo_count--;
    push(h,!redo,moved); b->command_revision++; b->dirty=true;
    result.status=WORKBOOK_HISTORY_APPLIED; result.sheet=b->active; result.sheet_id=b->sheets[b->active].id;
    return result;
}
