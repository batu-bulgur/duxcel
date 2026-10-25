#include "sheet_metadata_replay.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
struct SheetMetadataReplayPlan {
    Workbook *book;
    const uint64_t *epoch_owner;
    uint64_t epoch,session,revision,branch,issued,high_water,target_session;
    WorkbookLifecycleMetadata selection;
    const void *packet_data;
    size_t packet_bytes;
    SheetLifecycle view;
    SheetLifecyclePacket packet;
    bool redo;
};
static size_t locate(const Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return i; return b->count; }
static SheetMetadataReplayStatus selected(SheetMetadataReplayContext c,bool redo,WorkbookLifecycleMetadata *out) {
    if (!c.book || !c.epoch || !workbook_identity_valid(c.book) || !c.book->edit_session_id || !c.book->count || c.book->active>=c.book->count) return SHEET_METADATA_REPLAY_INVALID;
    if (!c.ready || c.read_only) return SHEET_METADATA_REPLAY_PROTECTED;
    WorkbookHistoryResult history=workbook_history_peek(c.book,redo,NULL);
    if (history.status==WORKBOOK_HISTORY_LIMIT) return SHEET_METADATA_REPLAY_LIMIT;
    if (history.status==WORKBOOK_HISTORY_INVALID) return SHEET_METADATA_REPLAY_INVALID;
    if (history.status!=WORKBOOK_HISTORY_READY || !history.lifecycle || !workbook_lifecycle_metadata(c.book,redo,out) || !out->packet) return SHEET_METADATA_REPLAY_UNSUPPORTED;
    if (!c.book->sheets[locate(c.book,out->sheet)].edit_session_id) return SHEET_METADATA_REPLAY_INVALID;
    uint64_t latest=workbook_history_latest_record(c.book);
    return c.book->history_branch==latest ? SHEET_METADATA_REPLAY_READY : SHEET_METADATA_REPLAY_STALE;
}
SheetMetadataReplayStatus sheet_metadata_replay_measure(SheetMetadataReplayContext c,bool redo,SheetMetadataReplayMeasure *out) {
    if (!out) return SHEET_METADATA_REPLAY_INVALID; WorkbookLifecycleMetadata entry;
    SheetMetadataReplayStatus status=selected(c,redo,&entry); if (status) return status;
    SheetLifecycleInfo info;
    if (workbook_sheet_metadata_preflight(entry.packet,entry.sheet,entry.operation,&info)!=SHEET_PAYLOAD_OK) return SHEET_METADATA_REPLAY_INVALID;
    size_t extra=info.owned-sizeof(SheetLifecycle); if (extra>SIZE_MAX-info.bytes || extra+info.bytes>SIZE_MAX-sizeof(SheetMetadataReplayPlan)) return SHEET_METADATA_REPLAY_LIMIT;
    extra+=info.bytes;
    *out=(SheetMetadataReplayMeasure){sizeof(SheetMetadataReplayPlan)+extra,sizeof(SheetMetadataReplayPlan)+extra}; return SHEET_METADATA_REPLAY_READY;
}
SheetMetadataReplayStatus sheet_metadata_replay_check(SheetMetadataReplayContext c,const SheetMetadataReplayPlan *p) {
    if (!p) return SHEET_METADATA_REPLAY_INVALID; WorkbookLifecycleMetadata current;
    SheetMetadataReplayStatus status=selected(c,p->redo,&current); if (status) return status==SHEET_METADATA_REPLAY_UNSUPPORTED ? SHEET_METADATA_REPLAY_STALE : status;
    if (c.book!=p->book || c.epoch!=p->epoch_owner || *c.epoch!=p->epoch || c.book->edit_session_id!=p->session || c.book->command_revision!=p->revision ||
        c.book->history_branch!=p->branch || worksheet_history_high_water()!=p->issued || c.book->sheet_high_water!=p->high_water ||
        current.owner!=p->selection.owner || current.order!=p->selection.order || current.sheet!=p->selection.sheet || current.operation!=p->selection.operation ||
        current.active!=p->selection.active || current.packet!=p->selection.packet || current.packet->data!=p->packet_data || current.packet->bytes!=p->packet_bytes ||
        c.book->sheets[locate(c.book,current.sheet)].edit_session_id!=p->target_session) return SHEET_METADATA_REPLAY_STALE;
    SheetLifecycleInfo packet;
    if (workbook_sheet_metadata_preflight(current.packet,current.sheet,current.operation,&packet)!=SHEET_PAYLOAD_OK) return SHEET_METADATA_REPLAY_INVALID;
    if (memcmp(current.packet->data,p->packet.data,p->packet.bytes)) return SHEET_METADATA_REPLAY_STALE;
    SheetLifecycleEntry rows[DX_MAX_SHEETS]; SheetLifecycleState live,target;
    if (sheet_lifecycle_from_workbook(c.book,rows,&live)!=SHEET_LIFECYCLE_OK) return SHEET_METADATA_REPLAY_INVALID;
    return sheet_lifecycle_admit(p->view,live,p->redo,&target)==SHEET_LIFECYCLE_OK ? SHEET_METADATA_REPLAY_READY : SHEET_METADATA_REPLAY_STALE;
}
SheetMetadataReplayStatus sheet_metadata_replay_prepare(SheetMetadataReplayContext c,bool redo,SheetMetadataReplayPlan **out) {
    if (!out || *out) return SHEET_METADATA_REPLAY_INVALID; SheetMetadataReplayMeasure usage;
    SheetMetadataReplayStatus status=sheet_metadata_replay_measure(c,redo,&usage); if (status) return status;
    if (c.reserved<usage.staging) return SHEET_METADATA_REPLAY_LIMIT;
    WorkbookLifecycleMetadata entry; status=selected(c,redo,&entry); if (status) return status;
    SheetMetadataReplayPlan *p=dx_alloc(sizeof(*p)); *p=(SheetMetadataReplayPlan){.book=c.book,.epoch_owner=c.epoch,.epoch=*c.epoch,.session=c.book->edit_session_id,
        .revision=c.book->command_revision,.branch=c.book->history_branch,.issued=worksheet_history_high_water(),.high_water=c.book->sheet_high_water,
        .target_session=c.book->sheets[locate(c.book,entry.sheet)].edit_session_id,.selection=entry,.packet_data=entry.packet->data,.packet_bytes=entry.packet->bytes,.redo=redo};
    p->packet=(SheetLifecyclePacket){dx_alloc(entry.packet->bytes),entry.packet->bytes}; memcpy(p->packet.data,entry.packet->data,entry.packet->bytes);
    if (sheet_lifecycle_decode(p->packet.data,p->packet.bytes,usage.staging-sizeof(*p)-p->packet.bytes+sizeof(SheetLifecycle),&p->view)!=SHEET_LIFECYCLE_OK) { sheet_metadata_replay_free(&p); return SHEET_METADATA_REPLAY_INVALID; }
    status=sheet_metadata_replay_check(c,p); if (status) { sheet_metadata_replay_free(&p); return status; }
    *out=p; return SHEET_METADATA_REPLAY_READY;
}
static uint64_t active_after(SheetLifecycleState target,uint64_t active,uint64_t fallback) {
    for (size_t i=0;i<target.count;i++) if (target.sheets[i].id==active && !target.sheets[i].hidden) return active;
    for (size_t i=0;i<target.count;i++) if (target.sheets[i].id==fallback && !target.sheets[i].hidden) return fallback;
    for (size_t i=0;i<target.count;i++) if (!target.sheets[i].hidden) return target.sheets[i].id;
    return 0;
}
SheetMetadataReplayStatus sheet_metadata_replay_apply(SheetMetadataReplayContext c,SheetMetadataReplayPlan **owned,char **error) {
    if (!owned || !*owned) return SHEET_METADATA_REPLAY_INVALID; SheetMetadataReplayPlan *p=*owned;
    SheetMetadataReplayStatus status=sheet_metadata_replay_check(c,p); if (status) return status;
    SheetLifecycleState target=p->redo ? p->view.after : p->view.before;
    uint64_t active=active_after(target,c.book->sheets[c.book->active].id,p->selection.active);
    WorkbookHistoryResult applied=workbook_history_step(c.book,p->redo,error);
    if (applied.status!=WORKBOOK_HISTORY_APPLIED) return applied.status==WORKBOOK_HISTORY_LIMIT ? SHEET_METADATA_REPLAY_LIMIT : SHEET_METADATA_REPLAY_INVALID;
    /* No allocation, source/data adoption, OS calls or callbacks intervene.
       Original packet bytes stay in the one genuine lifecycle event owner. */
    WorkbookLifecycleMetadata inverse; SheetLifecycleEntry rows[DX_MAX_SHEETS]; SheetLifecycleState live,unused;
    if (c.book->edit_session_id!=p->session || c.book->command_revision!=p->revision+1 || c.book->history_branch!=p->branch || c.book->sheet_high_water!=p->high_water ||
        worksheet_history_high_water()!=p->issued || c.book->sheets[c.book->active].id!=active ||
        !workbook_lifecycle_metadata(c.book,!p->redo,&inverse) || inverse.owner!=p->selection.owner || inverse.sheet!=p->selection.sheet || inverse.order!=p->selection.order || inverse.operation!=p->selection.operation ||
        !inverse.packet || inverse.packet->data!=p->packet_data || inverse.packet->bytes!=p->packet_bytes || memcmp(inverse.packet->data,p->packet.data,p->packet.bytes) ||
        sheet_lifecycle_from_workbook(c.book,rows,&live)!=SHEET_LIFECYCLE_OK || sheet_lifecycle_admit(p->view,live,!p->redo,&unused)!=SHEET_LIFECYCLE_OK) abort();
    sheet_metadata_replay_free(owned); return SHEET_METADATA_REPLAY_APPLIED;
}
void sheet_metadata_replay_free(SheetMetadataReplayPlan **owned) {
    if (!owned || !*owned) return; SheetMetadataReplayPlan *p=*owned; sheet_lifecycle_free(&p->view); sheet_lifecycle_packet_free(&p->packet); free(p); *owned=NULL;
}
