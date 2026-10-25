#include "workbook/history_event_frame.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL frame:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    WorkbookHistoryEventInfo valid={.owner={1,2,3,VERSION_WORKING_HEAD},.edit_session=4,.revision=5,.action=9,.transition=11,.previous_position=7,.position=8};
    CHECK(workbook_history_event_frame_valid(&valid) && !workbook_history_event_frame_valid(NULL));
    for (unsigned i=0;i<11;i++) {
        WorkbookHistoryEventInfo e=valid;
        switch (i) {
        case 0: e.owner.document_high=e.owner.document_low=0; break;
        case 1: e.owner.session=0; break;
        case 2: e.owner.mode=VERSION_HISTORICAL_VIEW; break;
        case 3: e.edit_session=0; break;
        case 4: e.revision=0; break;
        case 5: e.action=0; break;
        case 6: e.transition=0; break;
        case 7: e.previous_position=UINT64_MAX; e.position=0; break;
        case 8: e.position++; break;
        case 9: e.parent_lineage=1; break;
        case 10: e.forked=true; break;
        }
        CHECK(!workbook_history_event_frame_valid(&e));
    }
    valid.forked=true; valid.lineage=1; valid.parent_lineage=0; CHECK(workbook_history_event_frame_valid(&valid));
    valid.parent_lineage=1; CHECK(!workbook_history_event_frame_valid(&valid));
    valid.forked=false; CHECK(workbook_history_event_frame_valid(&valid));
    valid.owner.document_high=0; valid.revision=valid.action=valid.transition=valid.position=UINT64_MAX; valid.previous_position=UINT64_MAX-1;
    CHECK(workbook_history_event_frame_valid(&valid));
    /* Profiles/dependencies are deliberately proved by the typed dispatcher. */
    valid.family=99; valid.encoding=99; valid.dependency_encoding=99; valid.dependency_bytes=99; valid.inactive_scopes=99;
    CHECK(workbook_history_event_frame_valid(&valid));
    printf("PASS: %u shared history frame clock checks\n",checks); return 0;
}
