#include "workbook/range_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL range event:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    CellDelta cells[]={{1,1,1,"NULL","",4,0},{1,10,10,"","12345678901234567890.123456789",0,30}};
    cells[1].after_bytes=strlen(cells[1].after); RangeEventView view={.sheet=1,.effects=2,.cells=cells,.count=2}; RangeEventInfo info={0};
    CHECK(range_event_measure(view,0,&info)==RANGE_EVENT_OK && info.count==2); size_t bytes=info.bytes,written=99;
    unsigned char *data=dx_alloc(bytes+2); memset(data,0x55,bytes+2);
    CHECK(range_event_encode(view,0,data,bytes-1,&written)==RANGE_EVENT_LIMIT && written==99 && data[0]==0x55);
    CHECK(range_event_encode(view,0,data,bytes,&written)==RANGE_EVENT_OK && written==bytes && data[bytes]==0x55);
    RangeEvent event={0}; CHECK(range_event_preflight(data,bytes,&info)==RANGE_EVENT_OK);
    CHECK(range_event_decode(data,bytes,info.owned-1,&event)==RANGE_EVENT_LIMIT && !event.data);
    CHECK(range_event_decode(data,bytes,info.owned,&event)==RANGE_EVENT_OK && event.data!=data && event.cells[0].before_bytes==4 && !event.cells[0].after_bytes && !memcmp(event.cells[0].before,"NULL",4));
    CellDelta inverse=cell_delta_inverse(event.cells[1]); CHECK(inverse.before_bytes==cells[1].after_bytes && !inverse.after_bytes);
    RangeEvent before=event;
    for (size_t n=0;n<bytes;n++) CHECK(range_event_decode(data,n,SIZE_MAX,&event)!=RANGE_EVENT_OK && !memcmp(&before,&event,sizeof(event)));
    data[6]=1; CHECK(range_event_decode(data,bytes,SIZE_MAX,&event)==RANGE_EVENT_INVALID); data[6]=0;
    data[4]=2; CHECK(range_event_decode(data,bytes,SIZE_MAX,&event)==RANGE_EVENT_UNSUPPORTED); data[4]=1;
    CellDelta duplicate[2]={cells[0],cells[0]}; RangeEventView invalid=view; invalid.cells=duplicate;
    CHECK(range_event_measure(invalid,0,&info)==RANGE_EVENT_INVALID); invalid=view; invalid.count=0; CHECK(range_event_measure(invalid,0,&info)==RANGE_EVENT_INVALID);
    invalid=view; invalid.effects=10; CHECK(range_event_measure(invalid,0,&info)==RANGE_EVENT_LIMIT);
    FormatStore formats={0}; FormatSnapshot after={0}; CellFormat style=format_default(); style.flags=1;
    CHECK(format_plan_spatial(&formats,(FormatRange){1,1,1,1},FORMAT_BOLD,style,&after,NULL)); view.effects=10; view.before_formats=format_delta_store_view(&formats); view.after_formats=format_delta_snapshot_view(&after);
    CHECK(range_event_measure(view,DX_FORMAT_VALIDATION_BYTES,&info)==RANGE_EVENT_OK); free(data); data=dx_alloc(info.bytes); bytes=info.bytes;
    CHECK(range_event_encode(view,DX_FORMAT_VALIDATION_BYTES,data,bytes,&written)==RANGE_EVENT_OK && range_event_preflight(data,bytes,&info)==RANGE_EVENT_OK);
    CHECK(range_event_decode(data,bytes,info.owned+info.validation_scratch,&event)==RANGE_EVENT_OK && event.effects==10 && event.formats.after.count==1 && !event.formats.before.count);
    CHECK(event.formats.after.items!=after.items); view.effects=8; view.cells=NULL; view.count=0;
    CHECK(range_event_measure(view,DX_FORMAT_VALIDATION_BYTES,&info)==RANGE_EVENT_OK); view.after_formats=view.before_formats; CHECK(range_event_measure(view,DX_FORMAT_VALIDATION_BYTES,&info)==RANGE_EVENT_INVALID);
    view.after_formats=format_delta_snapshot_view(&after); view.after_formats.source_aware=true; CHECK(range_event_measure(view,SIZE_MAX,&info)==RANGE_EVENT_UNSUPPORTED);
    range_event_free(&event); free(data); format_snapshot_free(&after); format_free(&formats); printf("PASS: %u complete range event checks\n",checks); return 0;
}
