#include "sheet_topology_remove.h"
#include "definition_baseline.h"
#include "history.h"
#include "lifecycle.h"
#include "history_sheet_topology_internal.h"
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
    if (!add(&usage.staging,bytes) || !add(&usage.staging,wire->owned) || !add(&usage.staging,wire->validation_scratch)) return SHEET_REMOVE_LIMIT;
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
static SheetTopologyRemoveStatus refresh_navigation(SheetTopologyRemoveContext c,SheetTopologyRemovePlan *p) {
    uint64_t active=c.book->sheets[c.book->active].id; if (active==p->before.active) return SHEET_REMOVE_READY;
    if (c.reserved<p->usage.staging) return SHEET_REMOVE_LIMIT;
    SheetTopologyInfo info;
    if (sheet_topology_preflight(p->packet.data,p->packet.bytes,&info)!=SHEET_TOPOLOGY_OK) return SHEET_REMOVE_INVALID;
    SheetLifecycleState before=p->before,after=p->after; before.active=active; after.active=active;
    if (active==p->sheet) for (size_t i=0;i<after.count;i++) if (!after.sheets[i].hidden) { after.active=after.sheets[i].id; break; }
    SheetPresencePacket presence={(unsigned char *)p->packet.data+p->packet.bytes-info.presence_bytes,info.presence_bytes};
    SheetTopologyView event={SHEET_TOPOLOGY_REMOVE,p->sheet,before,after,&presence};
    /* Only unpublished navigation fallback descriptors change. An older owned
       reservation is cancelled before replacement; no stream clock advances. */
    sheet_topology_remove_cancel(p);
    SheetTopologyStatus status=sheet_topology_capture(event,info.bytes+info.validation_scratch,&p->packet);
    if (status!=SHEET_TOPOLOGY_OK) return status==SHEET_TOPOLOGY_LIMIT ? SHEET_REMOVE_LIMIT : SHEET_REMOVE_INVALID;
    p->before.active=before.active; p->after.active=after.active; return SHEET_REMOVE_READY;
}
static bool after_matches(const SheetTopologyRemovePlan *p) {
    const Workbook *b=p->book;
    if (b->id.high!=p->before.document.high || b->id.low!=p->before.document.low || b->edit_session_id!=p->document_session || b->command_revision!=p->revision+1 ||
        b->count!=p->after.count || b->sheet_high_water!=p->after.high_water || locate(b,p->sheet)!=b->count || b->active>=b->count || b->sheets[b->active].hidden || b->sheets[b->active].id!=p->after.active ||
        b->history_branch!=p->order+1 || workbook_lifecycle_latest(b)!=p->order+1 || worksheet_history_high_water()!=p->order+1) return false;
    for (size_t i=0;i<b->count;i++) if (b->sheets[i].id!=p->after_rows[i].id || b->sheets[i].hidden!=p->after_rows[i].hidden || strcmp(b->sheets[i].name,p->after_rows[i].name)) return false;
    WorkbookHistoryResult history=workbook_history_peek(b,false,NULL);
    return (history.status==WORKBOOK_HISTORY_READY || history.status==WORKBOOK_HISTORY_LIMIT) && history.lifecycle && history.sheet_id==p->sheet && history.order==p->order+1;
}
static bool cache_coherent(const SheetTopologyRemovePlan *p) {
    size_t index=locate(p->book,p->sheet); if (index==p->book->count) return false;
    const Worksheet *sheet=p->book->sheets+index; bool matches=false;
    return sheet_presence_matches(p->presence,(SheetPresenceView){p->book->id,p->book->sheet_high_water,sheet,&sheet->analysis},&matches)==SHEET_PRESENCE_OK && matches;
}
SheetTopologyRemoveStatus sheet_topology_remove_apply(SheetTopologyRemoveContext context,SheetTopologyRemovePlan **owned,WorkbookHistoryStream *stream,size_t reserved,char **error) {
    if (!owned || !*owned) return SHEET_REMOVE_INVALID; SheetTopologyRemovePlan *p=*owned;
    SheetTopologyRemoveStatus status=sheet_topology_remove_check(context,p);
    if (status!=SHEET_REMOVE_READY) { sheet_topology_remove_cancel(p); return status; }
    if (!cache_coherent(p)) { sheet_topology_remove_cancel(p); fail(error,"Materialize the actual primary through its ownership handoff before removing the sheet."); return SHEET_REMOVE_PROTECTED; }
    size_t index=locate(context.book,p->sheet),adoption=workbook_lifecycle_remove_staging(context.book,index);
    if (adoption==SIZE_MAX || adoption>context.adoption_reserved) { sheet_topology_remove_cancel(p); fail(error,"The real worksheet removal allocation is not admitted."); return SHEET_REMOVE_LIMIT; }
    if (p->ticket && !workbook_history_topology_belongs(p->ticket,stream)) { sheet_topology_remove_cancel(p); return SHEET_REMOVE_INVALID; }
    status=refresh_navigation(context,p); if (status!=SHEET_REMOVE_READY) { sheet_topology_remove_cancel(p); return status; }
    if (stream && !p->ticket) { status=sheet_topology_remove_reserve(context,p,stream,reserved,error); if (status!=SHEET_REMOVE_READY) return status; }
    status=sheet_topology_remove_check(context,p);
    if (status!=SHEET_REMOVE_READY || !cache_coherent(p)) { sheet_topology_remove_cancel(p); return status==SHEET_REMOVE_READY ? SHEET_REMOVE_PROTECTED : status; }
    bool changed=false;
    if (!workbook_sheet_command(context.book,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=index},&changed,error)) { sheet_topology_remove_cancel(p); return SHEET_REMOVE_INVALID; }
    /* No callbacks, queries, OS calls, validation allocation or fallible work
       intervene between closed producer adoption and this release-build proof. */
    if (!changed || !after_matches(p)) abort();
    if (p->ticket) workbook_history_topology_publish_adopted(&p->ticket,context.book);
    sheet_topology_remove_free(owned); return SHEET_REMOVE_APPLIED;
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
