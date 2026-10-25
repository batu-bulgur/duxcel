#include "column_width_command.h"
#include <stdlib.h>
#include <string.h>
struct ColumnWidthCommand {
    ColumnWidthCommandContext owner;
    float *before_widths,*widths;
    char **before_columns,**columns;
    size_t before_count,count;
    uint64_t revision;
    ColumnWidthState canonical;
    ColumnWidthHistory *journal;
    bool owns_journal;
    ColumnWidthPlan *operation;
};
static ColumnWidthStatus shape(ColumnWidthCommandContext c,size_t *sparse) {
    if (!c.view || !c.history || !c.revision || c.view->column_count>DX_COLUMN_WIDTH_COLUMNS ||
        (c.view->column_count && (!c.view->widths || !c.view->columns))) return COLUMN_WIDTH_INVALID;
    *sparse=0;
    for (size_t i=0;i<c.view->column_count;i++) {
        uint32_t bits=column_width_bits(c.view->widths[i]);
        if (!c.view->columns[i] || bits<column_width_bits(52) || bits>column_width_bits(1200)) return COLUMN_WIDTH_INVALID;
        *sparse+=bits!=DX_COLUMN_WIDTH_DEFAULT_BITS;
    }
    return COLUMN_WIDTH_OK;
}
static bool matches(SheetView *v,ColumnWidthView after) {
    size_t at=0;
    for (size_t i=0;i<v->column_count;i++) {
        uint32_t bits=column_width_bits(v->widths[i]); if (bits==DX_COLUMN_WIDTH_DEFAULT_BITS) continue;
        if (at==after.count || after.entries[at].column!=i || after.entries[at].bits!=bits) return false; at++;
    }
    return at==after.count;
}
ColumnWidthStatus column_width_command_measure(ColumnWidthCommandContext c,ColumnWidthView after,size_t *owned) {
    size_t sparse=0; ColumnWidthStatus status=shape(c,&sparse); if (status) return status;
    if (!owned || !column_width_valid(after)) return COLUMN_WIDTH_INVALID;
    if (matches(c.view,after)) return COLUMN_WIDTH_NOOP;
    if (*c.revision==UINT64_MAX) return COLUMN_WIDTH_LIMIT;
    size_t count=c.view->column_count; if (after.count && count<=after.entries[after.count-1].column) count=after.entries[after.count-1].column+1;
    /* Fixed virtual bounds make each product/sum finite on supported hosts.
       Include the opaque journal plan conservatively through a full journal
       allocation (larger than the operation wrapper), plus both pair copies. */
    *owned=sizeof(ColumnWidthCommand)+2*column_width_history_allocation()+
        2*DX_COLUMN_WIDTH_MAX_BYTES+3*DX_COLUMN_WIDTH_COLUMNS*sizeof(ColumnWidthEntry)+
        count*(sizeof(float)+sizeof(char *))+(count-c.view->column_count);
    return COLUMN_WIDTH_OK;
}
static void dense(ColumnWidthCommand *p,ColumnWidthView target) {
    p->count=p->before_count; if (target.count && p->count<=target.entries[target.count-1].column) p->count=target.entries[target.count-1].column+1;
    p->widths=dx_alloc(p->count*sizeof(*p->widths)); p->columns=dx_alloc(p->count*sizeof(*p->columns));
    for (size_t i=0;i<p->count;i++) { p->widths[i]=96; p->columns[i]=i<p->before_count ? p->before_columns[i] : dx_strdup(""); }
    for (size_t i=0;i<target.count;i++) p->widths[target.entries[i].column]=column_width_value(target.entries[i].bits);
}
static ColumnWidthCommand *start(ColumnWidthCommandContext c) {
    ColumnWidthCommand *p=dx_alloc(sizeof(*p)); *p=(ColumnWidthCommand){.owner=c,.before_widths=c.view->widths,.before_columns=c.view->columns,
        .before_count=c.view->column_count,.revision=*c.revision,.journal=*c.history};
    return p;
}
static ColumnWidthContext journal_context(ColumnWidthCommand *p) { return (ColumnWidthContext){&p->canonical,&p->revision,p->journal,SIZE_MAX}; }
ColumnWidthStatus column_width_command_prepare(ColumnWidthCommandContext c,ColumnWidthView after,ColumnWidthCommand **out) {
    if (!out || *out) return COLUMN_WIDTH_INVALID;
    size_t owned=0; ColumnWidthStatus status=column_width_command_measure(c,after,&owned); if (status) return status;
    if (owned>c.reserved) return COLUMN_WIDTH_LIMIT;
    ColumnWidthCommand *p=start(c);
    if (!p->journal) { status=column_width_history_new((ColumnWidthHistoryOptions){DX_COLUMN_WIDTH_HISTORY_ENTRIES,DX_COLUMN_WIDTH_HISTORY_BYTES},column_width_history_allocation(),&p->journal); p->owns_journal=!status; }
    if (!status) status=column_width_capture(c.view->widths,c.view->column_count,SIZE_MAX,&p->canonical);
    if (!status) status=column_width_history_prepare(journal_context(p),after,&p->operation);
    if (!status) dense(p,after);
    if (status) { column_width_command_cancel(&p); return status; }
    *out=p; return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_command_replay(ColumnWidthCommandContext c,bool redo,ColumnWidthCommand **out) {
    size_t sparse=0; if (!out || *out) return COLUMN_WIDTH_INVALID;
    ColumnWidthStatus status=shape(c,&sparse); if (status) return status;
    if (!*c.history) return COLUMN_WIDTH_EMPTY;
    /* Maximum dense target admits replay without allocating merely to measure. */
    size_t bound=sizeof(ColumnWidthCommand)+2*column_width_history_allocation()+2*DX_COLUMN_WIDTH_MAX_BYTES+
        3*DX_COLUMN_WIDTH_COLUMNS*sizeof(ColumnWidthEntry)+DX_COLUMN_WIDTH_COLUMNS*(sizeof(float)+sizeof(char *)+1);
    if (bound>c.reserved || *c.revision==UINT64_MAX) return COLUMN_WIDTH_LIMIT;
    ColumnWidthCommand *p=start(c); status=column_width_capture(c.view->widths,c.view->column_count,SIZE_MAX,&p->canonical);
    if (!status) status=column_width_history_replay(journal_context(p),redo,&p->operation);
    if (!status) dense(p,column_width_plan_target(p->operation));
    if (status) { column_width_command_cancel(&p); return status; }
    *out=p; return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_command_check(ColumnWidthCommandContext c,const ColumnWidthCommand *p) {
    size_t sparse=0; if (!p || shape(c,&sparse)) return COLUMN_WIDTH_INVALID;
    if (c.view!=p->owner.view || c.history!=p->owner.history || c.revision!=p->owner.revision || *c.revision!=p->revision ||
        *c.history!=(p->owns_journal ? NULL : p->journal) || c.view->widths!=p->before_widths || c.view->columns!=p->before_columns ||
        c.view->column_count!=p->before_count || !matches(c.view,column_width_view(&p->canonical))) return COLUMN_WIDTH_STALE;
    for (size_t i=0;i<p->before_count;i++) if (c.view->columns[i]!=p->columns[i]) return COLUMN_WIDTH_STALE;
    ColumnWidthHistoryInfo next; return column_width_plan_history(p->operation,&next);
}
ColumnWidthStatus column_width_command_history(const ColumnWidthCommand *p,ColumnWidthHistoryInfo *out) {
    if (!p) return COLUMN_WIDTH_INVALID;
    ColumnWidthStatus status=column_width_command_check(p->owner,p); return status ? status : column_width_plan_history(p->operation,out);
}
ColumnWidthStatus column_width_command_retain_undo(ColumnWidthCommand *p,size_t keep) {
    if (!p) return COLUMN_WIDTH_INVALID;
    ColumnWidthStatus status=column_width_command_check(p->owner,p); return status ? status : column_width_plan_retain_undo(p->operation,keep);
}
ColumnWidthPair column_width_command_pair(const ColumnWidthCommand *p) { return p ? (ColumnWidthPair){column_width_view(&p->canonical),column_width_plan_target(p->operation)} : (ColumnWidthPair){0}; }
const ColumnWidthPacket *column_width_command_packet(const ColumnWidthCommand *p) { return p ? column_width_plan_packet(p->operation) : NULL; }
ColumnWidthStatus column_width_command_adopt(ColumnWidthCommandContext c,ColumnWidthCommand **out) {
    if (!out || !*out) return COLUMN_WIDTH_INVALID; ColumnWidthCommand *p=*out;
    ColumnWidthStatus status=column_width_command_check(c,p); if (status) return status;
    status=column_width_history_commit(journal_context(p),&p->operation); if (status) return status;
    /* Closed synchronous adoption: no allocation, callback or query remains. */
    free(c.view->widths); free(c.view->columns); c.view->widths=p->widths; c.view->columns=p->columns; c.view->column_count=p->count;
    p->widths=NULL; p->columns=NULL; p->count=0; *c.revision=p->revision;
    if (p->owns_journal) { *c.history=p->journal; p->owns_journal=false; }
    column_width_command_cancel(out); return COLUMN_WIDTH_OK;
}
void column_width_command_cancel(ColumnWidthCommand **owned) {
    if (!owned || !*owned) return; ColumnWidthCommand *p=*owned;
    column_width_plan_free(&p->operation); column_width_free(&p->canonical);
    if (p->columns) for (size_t i=p->before_count;i<p->count;i++) free(p->columns[i]);
    free(p->columns); free(p->widths); if (p->owns_journal) column_width_history_free(&p->journal); free(p); *owned=NULL;
}
