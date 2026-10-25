#include "sheet_topology_remove.h"
#include "definition_baseline.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
struct SheetTopologyRemovePlan {
    Workbook *book;
    const uint64_t *epoch_owner;
    uint64_t epoch,sheet,document_session,sheet_session,revision,branch,order,cells_revision,format_revision,width_revision;
    SheetTopologyRemoveMeasure usage;
    SheetLifecycleEntry before_rows[DX_MAX_SHEETS],after_rows[DX_MAX_SHEETS];
    SheetLifecycleState before,after;
    SheetPresenceAsset *presence;
    SheetTopologyPacket packet;
    WorkbookHistoryTopologyTicket *ticket;
};
static size_t locate(const Workbook *book,uint64_t id) { for (size_t i=0;i<book->count;i++) if (book->sheets[i].id==id) return i; return book->count; }
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool add(size_t *value,size_t extra) { if (*value>SIZE_MAX-extra) return false; *value+=extra; return true; }
static size_t catalog_bytes(SheetLifecycleState state) { size_t n=16*state.count; for (size_t i=0;i<state.count;i++) n+=strlen(state.sheets[i].name); return n; }
static SheetTopologyRemoveStatus measure(SheetTopologyRemoveContext c,uint64_t id,SheetTopologyRemoveMeasure *out,SheetPresenceWireInfo *wire) {
    if (!out || !wire || !c.book || !c.epoch || !c.primary || !workbook_identity_valid(c.book) || !c.book->count || c.book->active>=c.book->count || c.primary_sheet!=id) return SHEET_REMOVE_INVALID;
    if (!c.ready || c.read_only) return SHEET_REMOVE_PROTECTED;
    size_t index=locate(c.book,id); if (index==c.book->count || !c.book->edit_session_id || !c.book->sheets[index].edit_session_id) return SHEET_REMOVE_INVALID;
    const Worksheet *sheet=c.book->sheets+index;
    if (!sheet->hidden && workbook_visible_count(c.book)<=1) return SHEET_REMOVE_PROTECTED;
    if (c.book->command_revision==UINT64_MAX || !worksheet_history_order_available()) return SHEET_REMOVE_LIMIT;
    if (workbook_history_peek(c.book,false,NULL).status==WORKBOOK_HISTORY_INVALID) return SHEET_REMOVE_INVALID;
    ObjectSnapshot objects={sheet->objects.items,sheet->objects.count}; DefinitionBaselineStatus baseline=definition_baseline_status((DefinitionDependencyView){c.primary,&objects});
    if (baseline!=DEFINITION_BASELINE_READY) return baseline==DEFINITION_BASELINE_UNRESOLVED ? SHEET_REMOVE_PROTECTED : SHEET_REMOVE_INVALID;
    SheetLifecycleEntry entries[DX_MAX_SHEETS]; SheetLifecycleState before;
    if (sheet_lifecycle_from_workbook(c.book,entries,&before)!=SHEET_LIFECYCLE_OK) return SHEET_REMOVE_INVALID;
    SheetPresenceView view={c.book->id,c.book->sheet_high_water,sheet,c.primary}; SheetPresenceStatus status=sheet_presence_wire_view_measure(view,wire);
    if (status!=SHEET_PRESENCE_OK) return status==SHEET_PRESENCE_LIMIT ? SHEET_REMOVE_LIMIT : SHEET_REMOVE_INVALID;
    size_t names=0; for (size_t i=0;i<before.count;i++) names+=strlen(before.sheets[i].name)+1;
    size_t one=16+strlen(sheet->name),catalog=catalog_bytes(before),bytes=DX_SHEET_TOPOLOGY_HEADER_BYTES+catalog+(catalog-one)+wire->bytes;
    SheetTopologyRemoveMeasure usage={.owned=sizeof(SheetTopologyRemovePlan)};
    if (!add(&usage.owned,names) || !add(&usage.owned,wire->owned) || !add(&usage.owned,bytes)) return SHEET_REMOVE_LIMIT;
    usage.staging=usage.owned;
    if (!add(&usage.staging,wire->bytes) || !add(&usage.staging,wire->owned) || !add(&usage.staging,wire->validation_scratch)) return SHEET_REMOVE_LIMIT;
    usage.reservation=workbook_history_topology_ticket_allocation();
    if (!add(&usage.reservation,wire->owned) || !add(&usage.reservation,wire->validation_scratch)) return SHEET_REMOVE_LIMIT;
    *out=usage; return SHEET_REMOVE_READY;
}
SheetTopologyRemoveStatus sheet_topology_remove_measure(SheetTopologyRemoveContext context,uint64_t sheet,SheetTopologyRemoveMeasure *out) {
    SheetTopologyRemoveMeasure usage; SheetPresenceWireInfo wire; SheetTopologyRemoveStatus status=measure(context,sheet,&usage,&wire); if (status==SHEET_REMOVE_READY && out) *out=usage; return out ? status : SHEET_REMOVE_INVALID;
}
SheetTopologyRemoveStatus sheet_topology_remove_prepare(SheetTopologyRemoveContext c,uint64_t id,SheetTopologyRemovePlan **out,char **error) {
    if (!out || *out) return SHEET_REMOVE_INVALID;
    SheetTopologyRemoveMeasure usage; SheetPresenceWireInfo wire; SheetTopologyRemoveStatus status=measure(c,id,&usage,&wire);
    if (status!=SHEET_REMOVE_READY) { fail(error,"Choose a resolved editable sheet with an independent visible fallback and genuine command sessions."); return status; }
    if (usage.staging>c.reserved) { fail(error,"The complete sheet removal preparation budget is unavailable."); return SHEET_REMOVE_LIMIT; }
    size_t index=locate(c.book,id); const Worksheet *sheet=c.book->sheets+index;
    SheetTopologyRemovePlan *p=dx_alloc(sizeof(*p)); *p=(SheetTopologyRemovePlan){.book=c.book,.epoch_owner=c.epoch,.epoch=*c.epoch,.sheet=id,.document_session=c.book->edit_session_id,.sheet_session=sheet->edit_session_id,
        .revision=c.book->command_revision,.branch=c.book->history_branch,.order=worksheet_history_high_water(),.cells_revision=sheet->cells.revision,.format_revision=sheet->formats.revision,.width_revision=sheet->width_revision,.usage=usage};
    if (sheet_lifecycle_from_workbook(c.book,p->before_rows,&p->before)!=SHEET_LIFECYCLE_OK) { sheet_topology_remove_free(&p); return SHEET_REMOVE_INVALID; }
    for (size_t i=0;i<p->before.count;i++) p->before_rows[i].name=dx_strdup(p->before_rows[i].name);
    p->after=p->before; p->after.count--; p->after.sheets=p->after_rows;
    for (size_t i=0,j=0;i<p->before.count;i++) if (i!=index) p->after_rows[j++]=p->before_rows[i];
    if (p->after.active==id) for (size_t i=0;i<p->after.count;i++) if (!p->after_rows[i].hidden) { p->after.active=p->after_rows[i].id; break; }
    SheetPresenceStatus captured=sheet_presence_capture((SheetPresenceView){c.book->id,c.book->sheet_high_water,sheet,c.primary},wire.owned+wire.validation_scratch,&p->presence,error);
    SheetPresencePacket presence={0};
    if (captured==SHEET_PRESENCE_OK) captured=sheet_presence_encode(p->presence,wire.bytes+wire.validation_scratch,&presence);
    if (captured!=SHEET_PRESENCE_OK) { sheet_topology_remove_free(&p); return captured==SHEET_PRESENCE_LIMIT ? SHEET_REMOVE_LIMIT : SHEET_REMOVE_INVALID; }
    SheetTopologyView event={SHEET_TOPOLOGY_REMOVE,id,p->before,p->after,&presence}; SheetTopologyInfo event_info;
    SheetTopologyStatus prepared=sheet_topology_measure(event,&event_info);
    if (!prepared) prepared=sheet_topology_capture(event,event_info.bytes+event_info.validation_scratch,&p->packet);
    sheet_presence_packet_free(&presence);
    if (prepared!=SHEET_TOPOLOGY_OK) { sheet_topology_remove_free(&p); return prepared==SHEET_TOPOLOGY_LIMIT ? SHEET_REMOVE_LIMIT : SHEET_REMOVE_INVALID; }
    status=sheet_topology_remove_check(c,p); if (status!=SHEET_REMOVE_READY) { sheet_topology_remove_free(&p); return status; }
    *out=p; return SHEET_REMOVE_READY;
}
SheetTopologyRemoveStatus sheet_topology_remove_check(SheetTopologyRemoveContext c,const SheetTopologyRemovePlan *p) {
    if (!p || !c.book || !c.epoch || !c.primary || !workbook_identity_valid(c.book)) return SHEET_REMOVE_INVALID;
    if (!c.ready || c.read_only) return SHEET_REMOVE_PROTECTED;
    if (c.book!=p->book || c.epoch!=p->epoch_owner || *c.epoch!=p->epoch || c.primary_sheet!=p->sheet || c.book->id.high!=p->before.document.high || c.book->id.low!=p->before.document.low ||
        c.book->edit_session_id!=p->document_session || c.book->command_revision!=p->revision || c.book->history_branch!=p->branch || worksheet_history_high_water()!=p->order || c.book->sheet_high_water!=p->before.high_water || c.book->count!=p->before.count) return SHEET_REMOVE_STALE;
    if (workbook_history_peek(c.book,false,NULL).status==WORKBOOK_HISTORY_INVALID) return SHEET_REMOVE_INVALID;
    size_t index=locate(c.book,p->sheet); if (index==c.book->count) return SHEET_REMOVE_STALE; const Worksheet *sheet=c.book->sheets+index;
    if (sheet->edit_session_id!=p->sheet_session || sheet->cells.revision!=p->cells_revision || sheet->formats.revision!=p->format_revision || sheet->width_revision!=p->width_revision) return SHEET_REMOVE_STALE;
    SheetLifecycleEntry rows[DX_MAX_SHEETS]; SheetLifecycleState current;
    if (sheet_lifecycle_from_workbook(c.book,rows,&current)!=SHEET_LIFECYCLE_OK) return SHEET_REMOVE_INVALID;
    for (size_t i=0;i<current.count;i++) if (rows[i].id!=p->before_rows[i].id || rows[i].hidden!=p->before_rows[i].hidden || strcmp(rows[i].name,p->before_rows[i].name)) return SHEET_REMOVE_STALE;
    bool matches=false; SheetPresenceStatus status=sheet_presence_matches(p->presence,(SheetPresenceView){c.book->id,c.book->sheet_high_water,sheet,c.primary},&matches);
    if (status!=SHEET_PRESENCE_OK) return SHEET_REMOVE_INVALID; return matches ? SHEET_REMOVE_READY : SHEET_REMOVE_STALE;
}
SheetTopologyRemoveStatus sheet_topology_remove_reserve(SheetTopologyRemoveContext context,SheetTopologyRemovePlan *plan,WorkbookHistoryStream *stream,size_t reserved,char **error) {
    if (!plan) return SHEET_REMOVE_INVALID; SheetTopologyRemoveStatus status=sheet_topology_remove_check(context,plan); if (status!=SHEET_REMOVE_READY) return status;
    if (plan->ticket) return SHEET_REMOVE_BUSY;
    WorkbookHistoryTopologyRecord record={plan->before.document,plan->document_session,plan->revision,plan->revision+1,&plan->packet};
    if (!workbook_history_topology_reserve(stream,&record,reserved,&plan->ticket,error)) return SHEET_REMOVE_LIMIT;
    status=sheet_topology_remove_check(context,plan); if (status!=SHEET_REMOVE_READY) sheet_topology_remove_cancel(plan); return status;
}
void sheet_topology_remove_cancel(SheetTopologyRemovePlan *plan) { if (plan) workbook_history_topology_cancel(&plan->ticket); }
void sheet_topology_remove_free(SheetTopologyRemovePlan **owned) {
    if (!owned || !*owned) return; SheetTopologyRemovePlan *p=*owned; sheet_topology_remove_cancel(p);
    for (size_t i=0;i<p->before.count;i++) free((void *)p->before_rows[i].name);
    sheet_presence_free(&p->presence); sheet_topology_packet_free(&p->packet); free(p); *owned=NULL;
}
const SheetTopologyPacket *sheet_topology_remove_packet(const SheetTopologyRemovePlan *plan) { return plan ? &plan->packet : NULL; }
SheetPresenceView sheet_topology_remove_presence(const SheetTopologyRemovePlan *plan) { return sheet_presence_view(plan ? plan->presence : NULL); }
SheetTopologyRemoveMeasure sheet_topology_remove_usage(const SheetTopologyRemovePlan *plan) { return plan ? plan->usage : (SheetTopologyRemoveMeasure){0}; }
bool sheet_topology_remove_reserved(const SheetTopologyRemovePlan *plan) { return plan && plan->ticket; }
