#include "primary_source_topology.h"
#include <stdlib.h>
#include <string.h>
struct PrimarySourceTopologyPlan {
    PrimarySourcePublishPlan *source;
    PrimarySourceTopologyUsage usage;
    SheetLifecycleEntry before_rows[DX_MAX_SHEETS],after_rows[DX_MAX_SHEETS];
    SheetLifecycleState before,after;
    SheetTopologyPacket packet;
    WorkbookHistoryTopologyTicket *ticket;
    WorkbookHistoryStream *stream;
    WorkbookHistoryStreamOrigin stream_before;
};
static PrimarySourceTopologyStatus fail(PrimarySourceTopologyStatus status,char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return status;
}
static bool add(size_t *sum,size_t bytes) { if (bytes==SIZE_MAX || *sum>SIZE_MAX-bytes) return false; *sum+=bytes; return true; }
static PrimarySourceTopologyStatus source_status(PrimarySourcePublishStatus status) {
    return status==PRIMARY_PUBLISH_READY ? PRIMARY_TOPOLOGY_READY : status==PRIMARY_PUBLISH_STALE ? PRIMARY_TOPOLOGY_STALE :
        status==PRIMARY_PUBLISH_PROTECTED ? PRIMARY_TOPOLOGY_PROTECTED : status==PRIMARY_PUBLISH_LIMIT ? PRIMARY_TOPOLOGY_LIMIT : PRIMARY_TOPOLOGY_INVALID;
}
static bool same_stream(WorkbookHistoryStreamOrigin a,WorkbookHistoryStreamOrigin b) {
    return a.owner.document_high==b.owner.document_high && a.owner.document_low==b.owner.document_low && a.owner.session==b.owner.session && a.owner.mode==b.owner.mode &&
        a.edit_session==b.edit_session && a.command_revision==b.command_revision && a.origin==b.origin && a.available==b.available && a.cursor==b.cursor && a.tail==b.tail &&
        a.lineage==b.lineage && a.action_high_water==b.action_high_water && a.transition_high_water==b.transition_high_water && a.lineage_high_water==b.lineage_high_water;
}
static PrimarySourceTopologyStatus measure(PrimarySourceDraftContext c,const PrimarySourcePublishPlan *source,PrimarySourceTopologyUsage *out,
    SheetPresenceWireInfo *wire,char **error) {
    PrimarySourcePublishStatus checked=primary_source_publish_check(c,source,error); if (checked!=PRIMARY_PUBLISH_READY) return source_status(checked);
    if (!out || !wire) return PRIMARY_TOPOLOGY_INVALID;
    const SheetPresenceAsset *presence=primary_source_publish_presence(source); SheetPresenceView view=sheet_presence_view(presence);
    PrimarySourcePublishInfo source_usage=primary_source_publish_usage(source);
    if (!presence || !view.sheet || view.sheet->id!=source_usage.prospective_sheet || view.sheet->id!=c.book->sheet_high_water+1 ||
        view.document.high!=c.book->id.high || view.document.low!=c.book->id.low || view.sheet_high_water!=view.sheet->id || view.sheet->hidden || view.sheet->edit_session_id)
        return fail(PRIMARY_TOPOLOGY_INVALID,error,"Choose the independent proposed source sheet without a fabricated runtime session.");
    SheetLifecycleEntry rows[DX_MAX_SHEETS],after_rows[DX_MAX_SHEETS]; SheetLifecycleState before,after;
    if (sheet_lifecycle_from_workbook(c.book,rows,&before)!=SHEET_LIFECYCLE_OK || before.count>=DX_MAX_SHEETS) return PRIMARY_TOPOLOGY_INVALID;
    memcpy(after_rows,rows,before.count*sizeof(*rows)); after_rows[before.count]=(SheetLifecycleEntry){view.sheet->id,view.sheet->name,false};
    after=before; after.sheets=after_rows; after.count++; after.active=view.sheet->id; after.high_water=view.sheet->id;
    SheetLifecycleInfo a,b;
    if (sheet_lifecycle_state_measure(before,&a)!=SHEET_LIFECYCLE_OK || sheet_lifecycle_state_measure(after,&b)!=SHEET_LIFECYCLE_OK ||
        sheet_presence_wire_measure(presence,wire)!=SHEET_PRESENCE_OK) return PRIMARY_TOPOLOGY_INVALID;
    /* Catalog byte counts come from the actual DXSL measure, not a private
       native layout. DXTP's public encoding header and DXSP supply the rest. */
    size_t event=DX_SHEET_TOPOLOGY_HEADER_BYTES;
    if (!add(&event,a.bytes) || !add(&event,b.bytes) || !add(&event,wire->bytes) || event>DX_SHEET_TOPOLOGY_MAX_BYTES)
        return PRIMARY_TOPOLOGY_LIMIT;
    size_t validation=wire->owned; if (!add(&validation,wire->validation_scratch)) return PRIMARY_TOPOLOGY_LIMIT;
    PrimarySourceTopologyUsage usage={.source=source_usage.coexisting,.owned=sizeof(PrimarySourceTopologyPlan),.event_bytes=event,.prospective_sheet=view.sheet->id};
    for (size_t i=0;i<before.count;i++) if (!add(&usage.owned,strlen(rows[i].name)+1)) return PRIMARY_TOPOLOGY_LIMIT;
    if (!add(&usage.owned,event)) return PRIMARY_TOPOLOGY_LIMIT;
    usage.preparation=usage.owned;
    if (!add(&usage.preparation,wire->bytes) || !add(&usage.preparation,validation)) return PRIMARY_TOPOLOGY_LIMIT;
    usage.reservation=workbook_history_topology_owned_bytes(event);
    if (!add(&usage.reservation,validation)) return PRIMARY_TOPOLOGY_LIMIT;
    usage.total=usage.source;
    if (!add(&usage.total,usage.preparation) || !add(&usage.total,usage.reservation)) return PRIMARY_TOPOLOGY_LIMIT;
    *out=usage; return PRIMARY_TOPOLOGY_READY;
}
PrimarySourceTopologyStatus primary_source_topology_measure(PrimarySourceDraftContext c,const PrimarySourcePublishPlan *source,PrimarySourceTopologyUsage *out,char **error) {
    SheetPresenceWireInfo wire; return measure(c,source,out,&wire,error);
}
PrimarySourceTopologyStatus primary_source_topology_prepare(PrimarySourceDraftContext c,PrimarySourcePublishPlan **source,size_t reserved,PrimarySourceTopologyPlan **out,char **error) {
    if (!source || !out || *out) return PRIMARY_TOPOLOGY_INVALID;
    PrimarySourceTopologyUsage usage; SheetPresenceWireInfo wire; PrimarySourceTopologyStatus status=measure(c,*source,&usage,&wire,error); if (status!=PRIMARY_TOPOLOGY_READY) return status;
    if (reserved<usage.total) return fail(PRIMARY_TOPOLOGY_LIMIT,error,"Reserve the complete source/topology/catalog/stream coexistence before preparation.");
    PrimarySourceTopologyPlan *plan=dx_alloc(sizeof(*plan)); *plan=(PrimarySourceTopologyPlan){.usage=usage};
    if (sheet_lifecycle_from_workbook(c.book,plan->before_rows,&plan->before)!=SHEET_LIFECYCLE_OK) { free(plan); return PRIMARY_TOPOLOGY_INVALID; }
    for (size_t i=0;i<plan->before.count;i++) plan->before_rows[i].name=dx_strdup(plan->before_rows[i].name);
    plan->after=plan->before; plan->after.sheets=plan->after_rows; plan->after.count++; plan->after.active=usage.prospective_sheet; plan->after.high_water=usage.prospective_sheet;
    memcpy(plan->after_rows,plan->before_rows,plan->before.count*sizeof(*plan->before_rows));
    const SheetPresenceAsset *presence=primary_source_publish_presence(*source); SheetPresenceView view=sheet_presence_view(presence);
    plan->after_rows[plan->before.count]=(SheetLifecycleEntry){view.sheet->id,view.sheet->name,false};
    SheetPresencePacket encoded={0}; SheetTopologyInfo event_info;
    bool ok=sheet_presence_encode(presence,wire.bytes+wire.validation_scratch,&encoded)==SHEET_PRESENCE_OK;
    SheetTopologyView event={SHEET_TOPOLOGY_ADD,view.sheet->id,plan->before,plan->after,&encoded};
    if (ok) ok=sheet_topology_measure(event,&event_info)==SHEET_TOPOLOGY_OK && event_info.bytes==usage.event_bytes &&
        event_info.bytes<=usage.preparation && event_info.validation_scratch<=usage.preparation-event_info.bytes;
    if (ok) ok=sheet_topology_capture(event,event_info.bytes+event_info.validation_scratch,&plan->packet)==SHEET_TOPOLOGY_OK;
    sheet_presence_packet_free(&encoded);
    PrimarySourcePublishStatus checked=ok ? primary_source_publish_check(c,*source,error) : PRIMARY_PUBLISH_INVALID;
    if (checked!=PRIMARY_PUBLISH_READY) { primary_source_topology_free(&plan); return source_status(checked); }
    plan->source=*source; *source=NULL; *out=plan; return PRIMARY_TOPOLOGY_READY;
}
PrimarySourceTopologyStatus primary_source_topology_check(PrimarySourceDraftContext c,const PrimarySourceTopologyPlan *plan,char **error) {
    if (!plan) return PRIMARY_TOPOLOGY_INVALID;
    PrimarySourcePublishStatus checked=primary_source_publish_check(c,plan->source,error); if (checked!=PRIMARY_PUBLISH_READY) return source_status(checked);
    SheetLifecycleEntry rows[DX_MAX_SHEETS]; SheetLifecycleState current;
    if (sheet_lifecycle_from_workbook(c.book,rows,&current)!=SHEET_LIFECYCLE_OK) return PRIMARY_TOPOLOGY_INVALID;
    if (current.count!=plan->before.count || current.high_water!=plan->before.high_water || current.active!=plan->before.active) return PRIMARY_TOPOLOGY_STALE;
    for (size_t i=0;i<current.count;i++) if (rows[i].id!=plan->before_rows[i].id || rows[i].hidden!=plan->before_rows[i].hidden || strcmp(rows[i].name,plan->before_rows[i].name)) return PRIMARY_TOPOLOGY_STALE;
    if (plan->ticket && !same_stream(workbook_history_stream_state(plan->stream).current,plan->stream_before)) return PRIMARY_TOPOLOGY_STALE;
    return PRIMARY_TOPOLOGY_READY;
}
PrimarySourceTopologyStatus primary_source_topology_reserve(PrimarySourceDraftContext c,PrimarySourceTopologyPlan *plan,WorkbookHistoryStream *stream,size_t reserved,char **error) {
    PrimarySourceTopologyStatus status=primary_source_topology_check(c,plan,error); if (status!=PRIMARY_TOPOLOGY_READY) return status;
    if (plan->ticket) return PRIMARY_TOPOLOGY_BUSY;
    if (!stream || reserved<plan->usage.total) return fail(stream ? PRIMARY_TOPOLOGY_LIMIT : PRIMARY_TOPOLOGY_INVALID,error,"Reserve complete source/topology coexistence before the history ticket.");
    WorkbookHistoryStreamState before=workbook_history_stream_state(stream);
    if (before.current.owner.mode!=VERSION_WORKING_HEAD || before.current.owner.document_high!=c.book->id.high || before.current.owner.document_low!=c.book->id.low ||
        before.current.edit_session!=c.book->edit_session_id || before.current.command_revision!=c.book->command_revision) return PRIMARY_TOPOLOGY_STALE;
    if (before.reserved) return PRIMARY_TOPOLOGY_BUSY;
    WorkbookHistoryTopologyRecord record={c.book->id,c.book->edit_session_id,c.book->command_revision,c.book->command_revision+1,&plan->packet};
    WorkbookHistoryTopologyUsage actual;
    if (!workbook_history_topology_measure(&plan->packet,&actual) || actual.staging>plan->usage.reservation) return PRIMARY_TOPOLOGY_LIMIT;
    WorkbookHistoryTopologyTicket *ticket=NULL;
    if (!workbook_history_topology_reserve(stream,&record,plan->usage.reservation,&ticket,error)) return PRIMARY_TOPOLOGY_LIMIT;
    status=primary_source_topology_check(c,plan,error);
    if (status!=PRIMARY_TOPOLOGY_READY || !same_stream(workbook_history_stream_state(stream).current,before.current)) {
        workbook_history_topology_cancel(&ticket); return status==PRIMARY_TOPOLOGY_READY ? PRIMARY_TOPOLOGY_STALE : status;
    }
    plan->ticket=ticket; plan->stream=stream; plan->stream_before=before.current; return PRIMARY_TOPOLOGY_READY;
}
void primary_source_topology_cancel(PrimarySourceTopologyPlan *plan) { if (plan) { workbook_history_topology_cancel(&plan->ticket); plan->stream=NULL; } }
void primary_source_topology_free(PrimarySourceTopologyPlan **owned) {
    if (owned && *owned) { PrimarySourceTopologyPlan *plan=*owned; primary_source_topology_cancel(plan); primary_source_publish_cancel(&plan->source);
        for (size_t i=0;i<plan->before.count;i++) free((char *)plan->before_rows[i].name);
        sheet_topology_packet_free(&plan->packet); free(plan); *owned=NULL; }
}
const SheetTopologyPacket *primary_source_topology_packet(const PrimarySourceTopologyPlan *plan) { return plan ? &plan->packet : NULL; }
const SheetPresenceAsset *primary_source_topology_presence(const PrimarySourceTopologyPlan *plan) { return plan ? primary_source_publish_presence(plan->source) : NULL; }
PrimarySourceTopologyUsage primary_source_topology_usage(const PrimarySourceTopologyPlan *plan) { return plan ? plan->usage : (PrimarySourceTopologyUsage){0}; }
bool primary_source_topology_reserved(const PrimarySourceTopologyPlan *plan) { return plan && plan->ticket; }
