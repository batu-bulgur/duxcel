#include "workbook/history_event_slot.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL event slot:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void binding(bool forked) {
    WorkbookVersionOwner owner={10,20,30,VERSION_WORKING_HEAD};
    WorkbookHistoryEventInfo frame={.owner=owner,.edit_session=40,.action=13,.transition=21,.revision=9,.previous_position=4,.position=5,.lineage=forked ? 8 : 4,.parent_lineage=4,.forked=forked};
    WorkbookHistorySegment segments[2]={{4,0,forked ? 4 : 5},{8,4,5}}; WorkbookHistoryEventPosition event={21,frame.lineage,5};
    WorkbookHistoryPath path={.document_high=10,.document_low=20,.segments=segments,.segment_count=forked ? 2 : 1,.events=&event,.event_count=1,.lineage_high_water=frame.lineage,.operation_high_water=21,.available=4,.cursor=4,.tail=5};
    WorkbookHistoryEventSlot slot={&path,0,owner,40,&frame},changed;
    CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_OK);
    changed=slot; changed.path=NULL; CHECK(workbook_history_event_slot_check(changed)==HISTORY_EVENT_SLOT_INVALID);
    changed=slot; changed.event=NULL; CHECK(workbook_history_event_slot_check(changed)==HISTORY_EVENT_SLOT_INVALID);
    changed=slot; changed.edit_session=0; CHECK(workbook_history_event_slot_check(changed)==HISTORY_EVENT_SLOT_INVALID);
    changed=slot; changed.owner.session=0; CHECK(workbook_history_event_slot_check(changed)==HISTORY_EVENT_SLOT_INVALID);
    changed=slot; changed.owner.mode=VERSION_HISTORICAL_VIEW; CHECK(workbook_history_event_slot_check(changed)==HISTORY_EVENT_SLOT_INVALID);
    changed=slot; changed.index=SIZE_MAX; CHECK(workbook_history_event_slot_check(changed)==HISTORY_EVENT_SLOT_ASSOCIATION);
    changed=slot; changed.owner.session++; CHECK(workbook_history_event_slot_check(changed)==HISTORY_EVENT_SLOT_ASSOCIATION);
    changed=slot; changed.edit_session++; CHECK(workbook_history_event_slot_check(changed)==HISTORY_EVENT_SLOT_ASSOCIATION);
    frame.owner.document_high++; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION); frame.owner=owner;
    frame.owner.document_low++; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION); frame.owner=owner;
    event.operation=frame.action; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION); event.operation=21;
    frame.action=UINT64_MAX; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_OK); frame.action=13;
    frame.previous_position=UINT64_MAX; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION); frame.previous_position=4;
    frame.position++; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION); frame.position=5;
    frame.lineage++; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION); frame.lineage=event.lineage;
    path.document_low++; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION); path.document_low--;
    path.cursor=6; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_PATH); path.cursor=4;
    if (forked) {
        frame.parent_lineage=3; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION); frame.parent_lineage=4;
        frame.forked=false; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION); frame.forked=true;
        WorkbookHistoryPath opaque=path; opaque.origin=4; opaque.segments=segments+1; opaque.segment_count=1; changed=slot; changed.path=&opaque;
        CHECK(workbook_history_event_slot_check(changed)==HISTORY_EVENT_SLOT_ASSOCIATION);
    } else {
        frame.forked=true; frame.parent_lineage=3; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION); frame.forked=false; frame.parent_lineage=4;
    }
    WorkbookHistorySegment last={frame.lineage,UINT64_MAX-1,UINT64_MAX}; WorkbookHistoryEventPosition end={UINT64_MAX,frame.lineage,UINT64_MAX};
    path.origin=path.available=path.cursor=UINT64_MAX-1; path.tail=UINT64_MAX; path.segments=&last; path.segment_count=1; path.events=&end; path.operation_high_water=UINT64_MAX;
    frame.forked=false; frame.parent_lineage=frame.lineage; frame.previous_position=UINT64_MAX-1; frame.position=frame.transition=UINT64_MAX;
    CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_OK);
    frame.previous_position=UINT64_MAX; CHECK(workbook_history_event_slot_check(slot)==HISTORY_EVENT_SLOT_ASSOCIATION);
}
int main(void) { binding(false); binding(true); printf("PASS: %u shared assigned event-slot checks\n",checks); return 0; }
