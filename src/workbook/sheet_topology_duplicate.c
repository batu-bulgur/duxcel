#include "sheet_topology_duplicate.h"
#include "definition_baseline.h"
#include "history.h"
#include "lifecycle.h"
#include "history_sheet_topology_internal.h"
#include <stdlib.h>
#include <string.h>
struct SheetTopologyDuplicatePlan {
    Workbook *book;
    const uint64_t *epoch_owner;
    uint64_t epoch,sheet,added,document_session,sheet_session,revision,branch,order,cells_revision,format_revision,width_revision;
    SheetTopologyDuplicateMeasure usage;
    SheetLifecycleEntry before_rows[DX_MAX_SHEETS],after_rows[DX_MAX_SHEETS];
    SheetLifecycleState before,after;
    SheetPresenceAsset *presence;
    SheetTopologyPacket packet;
    char *name;
    WorkbookHistoryTopologyTicket *ticket;
};
static size_t locate(const Workbook *book,uint64_t id) { for (size_t i=0;i<book->count;i++) if (book->sheets[i].id==id) return i; return book->count; }
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool add(size_t *value,size_t extra) { if (*value>SIZE_MAX-extra) return false; *value+=extra; return true; }
static size_t catalog_bytes(SheetLifecycleState state) { size_t n=16*state.count; for (size_t i=0;i<state.count;i++) n+=strlen(state.sheets[i].name); return n; }
static SheetTopologyDuplicateStatus measure(SheetTopologyDuplicateContext c,uint64_t id,SheetTopologyDuplicateMeasure *out,SheetPresenceWireInfo *wire) {
    if (!out || !wire || !c.book || !c.epoch || !c.primary || !workbook_identity_valid(c.book) || !c.book->count || c.book->active>=c.book->count || c.primary_sheet!=id) return SHEET_DUPLICATE_INVALID;
    if (!c.ready || c.read_only) return SHEET_DUPLICATE_PROTECTED;
    size_t index=locate(c.book,id); if (index==c.book->count || !c.book->edit_session_id || !c.book->sheets[index].edit_session_id) return SHEET_DUPLICATE_INVALID;
    const Worksheet *sheet=c.book->sheets+index;
    if (c.book->count>=DX_MAX_SHEETS || c.book->sheet_high_water==UINT64_MAX) return SHEET_DUPLICATE_LIMIT;
    if (c.book->command_revision==UINT64_MAX || !worksheet_history_order_available()) return SHEET_DUPLICATE_LIMIT;
    if (workbook_history_peek(c.book,false,NULL).status==WORKBOOK_HISTORY_INVALID) return SHEET_DUPLICATE_INVALID;
    ObjectSnapshot objects={sheet->objects.items,sheet->objects.count}; DefinitionBaselineStatus baseline=definition_baseline_status((DefinitionDependencyView){c.primary,&objects});
    if (baseline!=DEFINITION_BASELINE_READY) return baseline==DEFINITION_BASELINE_UNRESOLVED ? SHEET_DUPLICATE_PROTECTED : SHEET_DUPLICATE_INVALID;
    SheetLifecycleEntry entries[DX_MAX_SHEETS]; SheetLifecycleState before;
    if (sheet_lifecycle_from_workbook(c.book,entries,&before)!=SHEET_LIFECYCLE_OK) return SHEET_DUPLICATE_INVALID;
    SheetPresenceView view={c.book->id,c.book->sheet_high_water,sheet,c.primary}; SheetPresenceStatus status=sheet_presence_wire_view_measure(view,wire);
    if (status!=SHEET_PRESENCE_OK) return status==SHEET_PRESENCE_LIMIT ? SHEET_DUPLICATE_LIMIT : SHEET_DUPLICATE_INVALID;
    size_t names=0; for (size_t i=0;i<before.count;i++) names+=strlen(before.sheets[i].name)+1;
    /* Name normalization/suffix search is admitted before it allocates. The
       maximum valid new name bounds both the copied asset and ADD row. */
    size_t extra=4096-strlen(sheet->name); wire->bytes+=extra; wire->owned+=extra;
    size_t catalog=catalog_bytes(before),bytes=DX_SHEET_TOPOLOGY_HEADER_BYTES+2*catalog+16+4096+wire->bytes;
    SheetTopologyDuplicateMeasure usage={.owned=sizeof(SheetTopologyDuplicatePlan)};
    if (!add(&usage.owned,4097) || !add(&usage.owned,names) || !add(&usage.owned,wire->owned) || !add(&usage.owned,bytes)) return SHEET_DUPLICATE_LIMIT;
    usage.staging=usage.owned;
    if (!add(&usage.staging,8194)) return SHEET_DUPLICATE_LIMIT;
    if (!add(&usage.staging,bytes) || !add(&usage.staging,wire->owned) || !add(&usage.staging,wire->validation_scratch)) return SHEET_DUPLICATE_LIMIT;
    usage.reservation=workbook_history_topology_owned_bytes(bytes);
    if (usage.reservation==SIZE_MAX) return SHEET_DUPLICATE_LIMIT;
    if (!add(&usage.reservation,wire->owned) || !add(&usage.reservation,wire->validation_scratch)) return SHEET_DUPLICATE_LIMIT;
    *out=usage; return SHEET_DUPLICATE_READY;
}
SheetTopologyDuplicateStatus sheet_topology_duplicate_measure(SheetTopologyDuplicateContext context,uint64_t sheet,SheetTopologyDuplicateMeasure *out) {
    SheetTopologyDuplicateMeasure usage; SheetPresenceWireInfo wire; SheetTopologyDuplicateStatus status=measure(context,sheet,&usage,&wire); if (status==SHEET_DUPLICATE_READY && out) *out=usage; return out ? status : SHEET_DUPLICATE_INVALID;
}
SheetTopologyDuplicateStatus sheet_topology_duplicate_prepare(SheetTopologyDuplicateContext c,uint64_t id,SheetTopologyDuplicatePlan **out,char **error) {
    if (!out || *out) return SHEET_DUPLICATE_INVALID;
    SheetTopologyDuplicateMeasure usage; SheetPresenceWireInfo wire; SheetTopologyDuplicateStatus status=measure(c,id,&usage,&wire);
    if (status!=SHEET_DUPLICATE_READY) { fail(error,"Choose a resolved sheet with genuine command sessions and available topology capacity."); return status; }
    if (usage.staging>c.reserved) { fail(error,"The complete sheet duplicate preparation budget is unavailable."); return SHEET_DUPLICATE_LIMIT; }
    size_t index=locate(c.book,id); const Worksheet *sheet=c.book->sheets+index;
    SheetTopologyDuplicatePlan *p=dx_alloc(sizeof(*p)); *p=(SheetTopologyDuplicatePlan){.book=c.book,.epoch_owner=c.epoch,.epoch=*c.epoch,.sheet=id,.added=c.book->sheet_high_water+1,.document_session=c.book->edit_session_id,.sheet_session=sheet->edit_session_id,
        .revision=c.book->command_revision,.branch=c.book->history_branch,.order=worksheet_history_high_water(),.cells_revision=sheet->cells.revision,.format_revision=sheet->formats.revision,.width_revision=sheet->width_revision,.usage=usage};
    if (sheet_lifecycle_from_workbook(c.book,p->before_rows,&p->before)!=SHEET_LIFECYCLE_OK) { sheet_topology_duplicate_free(&p); return SHEET_DUPLICATE_INVALID; }
    for (size_t i=0;i<p->before.count;i++) p->before_rows[i].name=dx_strdup(p->before_rows[i].name);
    p->name=workbook_unique_sheet_name(c.book,sheet->name,error);
    if (!p->name) { sheet_topology_duplicate_free(&p); return SHEET_DUPLICATE_INVALID; }
    p->after=p->before; p->after.count++; p->after.sheets=p->after_rows;
    p->after.high_water=p->added; p->after.active=p->added;
    for (size_t i=0,j=0;i<p->before.count;i++) {
        p->after_rows[j++]=p->before_rows[i];
        if (i==index) p->after_rows[j++]=(SheetLifecycleEntry){p->added,p->name,false};
    }
    Worksheet projected=*sheet; projected.id=p->added; projected.name=p->name; projected.hidden=false;
    SheetPresenceStatus captured=sheet_presence_capture((SheetPresenceView){c.book->id,p->added,&projected,c.primary},wire.owned+wire.validation_scratch,&p->presence,error);
    SheetPresencePacket presence={0};
    if (captured==SHEET_PRESENCE_OK) captured=sheet_presence_encode(p->presence,wire.bytes+wire.validation_scratch,&presence);
    if (captured!=SHEET_PRESENCE_OK) { sheet_topology_duplicate_free(&p); return captured==SHEET_PRESENCE_LIMIT ? SHEET_DUPLICATE_LIMIT : SHEET_DUPLICATE_INVALID; }
    SheetTopologyView event={SHEET_TOPOLOGY_ADD,p->added,p->before,p->after,&presence}; SheetTopologyInfo event_info;
    SheetTopologyStatus prepared=sheet_topology_measure(event,&event_info);
    if (!prepared) prepared=sheet_topology_capture(event,event_info.bytes+event_info.validation_scratch,&p->packet);
    sheet_presence_packet_free(&presence);
    if (prepared!=SHEET_TOPOLOGY_OK) { sheet_topology_duplicate_free(&p); return prepared==SHEET_TOPOLOGY_LIMIT ? SHEET_DUPLICATE_LIMIT : SHEET_DUPLICATE_INVALID; }
    status=sheet_topology_duplicate_check(c,p); if (status!=SHEET_DUPLICATE_READY) { sheet_topology_duplicate_free(&p); return status; }
    *out=p; return SHEET_DUPLICATE_READY;
}
SheetTopologyDuplicateStatus sheet_topology_duplicate_check(SheetTopologyDuplicateContext c,const SheetTopologyDuplicatePlan *p) {
    if (!p || !c.book || !c.epoch || !c.primary || !workbook_identity_valid(c.book)) return SHEET_DUPLICATE_INVALID;
    if (!c.ready || c.read_only) return SHEET_DUPLICATE_PROTECTED;
    if (c.book!=p->book || c.epoch!=p->epoch_owner || *c.epoch!=p->epoch || c.primary_sheet!=p->sheet || c.book->id.high!=p->before.document.high || c.book->id.low!=p->before.document.low ||
        c.book->edit_session_id!=p->document_session || c.book->command_revision!=p->revision || c.book->history_branch!=p->branch || worksheet_history_high_water()!=p->order || c.book->sheet_high_water!=p->before.high_water || c.book->count!=p->before.count) return SHEET_DUPLICATE_STALE;
    if (workbook_history_peek(c.book,false,NULL).status==WORKBOOK_HISTORY_INVALID) return SHEET_DUPLICATE_INVALID;
    size_t index=locate(c.book,p->sheet); if (index==c.book->count) return SHEET_DUPLICATE_STALE; const Worksheet *sheet=c.book->sheets+index;
    if (sheet->edit_session_id!=p->sheet_session || sheet->cells.revision!=p->cells_revision || sheet->formats.revision!=p->format_revision || sheet->width_revision!=p->width_revision) return SHEET_DUPLICATE_STALE;
    SheetLifecycleEntry rows[DX_MAX_SHEETS]; SheetLifecycleState current;
    if (sheet_lifecycle_from_workbook(c.book,rows,&current)!=SHEET_LIFECYCLE_OK) return SHEET_DUPLICATE_INVALID;
    for (size_t i=0;i<current.count;i++) if (rows[i].id!=p->before_rows[i].id || rows[i].hidden!=p->before_rows[i].hidden || strcmp(rows[i].name,p->before_rows[i].name)) return SHEET_DUPLICATE_STALE;
    Worksheet projected=*sheet; projected.id=p->added; projected.name=p->name; projected.hidden=false;
    bool matches=false; SheetPresenceStatus status=sheet_presence_matches(p->presence,(SheetPresenceView){c.book->id,p->added,&projected,c.primary},&matches);
    if (status!=SHEET_PRESENCE_OK) return SHEET_DUPLICATE_INVALID; return matches ? SHEET_DUPLICATE_READY : SHEET_DUPLICATE_STALE;
}
SheetTopologyDuplicateStatus sheet_topology_duplicate_reserve(SheetTopologyDuplicateContext context,SheetTopologyDuplicatePlan *plan,WorkbookHistoryStream *stream,size_t reserved,char **error) {
    if (!plan) return SHEET_DUPLICATE_INVALID; SheetTopologyDuplicateStatus status=sheet_topology_duplicate_check(context,plan); if (status!=SHEET_DUPLICATE_READY) return status;
    if (plan->ticket) return SHEET_DUPLICATE_BUSY;
    WorkbookHistoryTopologyRecord record={plan->before.document,plan->document_session,plan->revision,plan->revision+1,&plan->packet};
    if (!workbook_history_topology_reserve(stream,&record,reserved,&plan->ticket,error)) return SHEET_DUPLICATE_LIMIT;
    status=sheet_topology_duplicate_check(context,plan); if (status!=SHEET_DUPLICATE_READY) sheet_topology_duplicate_cancel(plan); return status;
}
void sheet_topology_duplicate_cancel(SheetTopologyDuplicatePlan *plan) { if (plan) workbook_history_topology_cancel(&plan->ticket); }
void sheet_topology_duplicate_free(SheetTopologyDuplicatePlan **owned) {
    if (!owned || !*owned) return; SheetTopologyDuplicatePlan *p=*owned; sheet_topology_duplicate_cancel(p);
    for (size_t i=0;i<p->before.count;i++) free((void *)p->before_rows[i].name);
    free(p->name); sheet_presence_free(&p->presence); sheet_topology_packet_free(&p->packet); free(p); *owned=NULL;
}
const SheetTopologyPacket *sheet_topology_duplicate_packet(const SheetTopologyDuplicatePlan *plan) { return plan ? &plan->packet : NULL; }
SheetPresenceView sheet_topology_duplicate_presence(const SheetTopologyDuplicatePlan *plan) { return sheet_presence_view(plan ? plan->presence : NULL); }
SheetTopologyDuplicateMeasure sheet_topology_duplicate_usage(const SheetTopologyDuplicatePlan *plan) { return plan ? plan->usage : (SheetTopologyDuplicateMeasure){0}; }
bool sheet_topology_duplicate_reserved(const SheetTopologyDuplicatePlan *plan) { return plan && plan->ticket; }
