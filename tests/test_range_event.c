#include "workbook/range_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL range event:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void compound(void) {
    ColumnWidthEntry before[]={{0,0},{16383,0}},after[]={{1,0}};
    before[0].bits=column_width_bits(100); before[1].bits=column_width_bits(1200); after[0].bits=column_width_bits(100);
    CellDelta cell={.sheet=7,.row=4,.column=2,.before="NULL",.after="9007199254740993.0123456789",.before_bytes=4,.after_bytes=25};
    cell.after_bytes=strlen(cell.after);
    FormatStore store={0}; FormatSnapshot style={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_plan_spatial(&store,(FormatRange){4,4,2,2},FORMAT_BOLD,bold,&style,NULL));
    const unsigned effects[]={2,8,10,16,18,24,26};
    for (size_t k=0;k<sizeof(effects)/sizeof(*effects);k++) {
        unsigned effect=effects[k];
        RangeEventView v={.sheet=7,.effects=effect};
        if (effect&2) { v.cells=&cell; v.count=1; }
        if (effect&8) { v.before_formats=format_delta_store_view(&store); v.after_formats=format_delta_snapshot_view(&style); }
        if (effect&16) v.widths=(ColumnWidthPair){{before,2},{after,1}};
        RangeEventInfo measured={0},info={0}; CHECK(range_event_measure(v,SIZE_MAX,&measured)==RANGE_EVENT_OK);
        unsigned char *data=dx_alloc(measured.bytes+1); memset(data,0x77,measured.bytes+1); size_t written=91;
        CHECK(range_event_encode(v,SIZE_MAX,data,measured.bytes,&written)==RANGE_EVENT_OK && written==measured.bytes && data[written]==0x77 && data[4]==DX_RANGE_EVENT_ENCODING);
        CHECK(range_event_preflight(data,written,&info)==RANGE_EVENT_OK && info.effects==effect && info.owned<=measured.owned && info.width_bytes==measured.width_bytes);
        RangeEvent event={0}; CHECK(range_event_decode(data,written,info.owned+info.validation_scratch-1,&event)==RANGE_EVENT_LIMIT && !event.data);
        CHECK(range_event_decode(data,written,info.owned+info.validation_scratch,&event)==RANGE_EVENT_OK);
        if (effect&16) {
            CHECK(event.widths.before.count==2 && event.widths.after.count==1 && event.widths.before.entries!=before && event.widths.after.entries!=after);
            CHECK(event.widths.before.entries[1].column==16383 && event.widths.before.entries[1].bits==column_width_bits(1200));
            ColumnWidthPair inverse=column_width_inverse(column_width_delta_view(&event.widths));
            CHECK(column_width_equal(inverse.after,(ColumnWidthView){before,2}) && column_width_equal(inverse.before,(ColumnWidthView){after,1}));
        }
        if (effect&2) CHECK(event.cells[0].before!=cell.before && !memcmp(event.cells[0].after,cell.after,cell.after_bytes));
        if (effect&8) CHECK(event.formats.after.items!=style.items && event.formats.after.count==1);
        RangeEvent held=event; RangeEventInfo sentinel=info;
        for (size_t n=0;n<written;n++) CHECK(range_event_decode(data,n,SIZE_MAX,&event)!=RANGE_EVENT_OK && !memcmp(&held,&event,sizeof(held)));
        data[written]=0; CHECK(range_event_preflight(data,written+1,&info)==RANGE_EVENT_INVALID && !memcmp(&sentinel,&info,sizeof(info)));
        data[4]=1; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_UNSUPPORTED && !memcmp(&held,&event,sizeof(held))); data[4]=DX_RANGE_EVENT_ENCODING;
        if (effect&16) {
            size_t at=written-measured.width_bytes;
            data[at+4]=2; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_UNSUPPORTED && !memcmp(&held,&event,sizeof(held))); data[at+4]=1;
            data[at+16]=1; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(held))); data[at+16]=0;
            data[at+20]=0xff; data[at+21]=0xff;
            CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(held)));
            /* Restore the exact known-good packet before testing input lifetime. */
            CHECK(range_event_encode(v,SIZE_MAX,data,written,&written)==RANGE_EVENT_OK);
        }
        memset(data,0,written); free(data);
        if (effect&16) CHECK(event.widths.before.entries[1].bits==column_width_bits(1200));
        range_event_free(&event);
    }
    RangeEventView v={.sheet=7,.effects=16,.widths={{before,2},{after,1}}}; RangeEventInfo info={0};
    CHECK(range_event_measure(v,0,&info)==RANGE_EVENT_OK); size_t bytes=info.bytes,written=17; unsigned char *data=dx_alloc(bytes); memset(data,0x66,bytes);
    CHECK(range_event_encode(v,0,data,bytes,(size_t *)data)==RANGE_EVENT_INVALID && data[0]==0x66);
    CHECK(range_event_encode(v,0,before,bytes,&written)==RANGE_EVENT_INVALID && before[0].bits==column_width_bits(100) && written==17);
    CHECK(range_event_encode(v,0,data,bytes,(size_t *)before)==RANGE_EVENT_INVALID && before[0].bits==column_width_bits(100));
    v.widths.after=v.widths.before; CHECK(range_event_measure(v,0,&info)==RANGE_EVENT_INVALID);
    v.widths.after=(ColumnWidthView){after,1}; v.effects=2; CHECK(range_event_measure(v,0,&info)==RANGE_EVENT_INVALID);
    v.effects=17; CHECK(range_event_measure(v,0,&info)==RANGE_EVENT_INVALID);
    free(data); format_snapshot_free(&style); format_free(&store);
}
static size_t u32(const unsigned char *p) { return (size_t)p[0]|((size_t)p[1]<<8)|((size_t)p[2]<<16)|((size_t)p[3]<<24); }
static void objects_compound(void) {
    ObjectStore objects={0}; CHECK(objects_add(&objects,2,2,"İstanbul template",NULL)==0);
    ObjectSnapshot before={objects.items,objects.count}; SheetObject moved=objects.items[0]; moved.row=20; moved.col=12; ObjectSnapshot after={&moved,1};
    SourceVersions sources={.high_water=17}; FormatStore formats={0}; FormatSnapshot styling={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_plan_spatial(&formats,(FormatRange){2,2,2,2},FORMAT_BOLD,bold,&styling,NULL));
    CellDelta cell={.sheet=9,.row=1,.column=1,.before="NULL",.before_bytes=4,.after="Exact",.after_bytes=5}; ColumnWidthEntry width={3,column_width_bits(120)};
    const unsigned effects[]={4,6,12,14,20,22,28,30};
    for (size_t k=0;k<sizeof(effects)/sizeof(*effects);k++) {
        unsigned effect=effects[k]; RangeEventView view={.sheet=9,.effects=effect,.definitions={.sheet=9,.object_high_water=objects.next_id,.effects=effect&12u,.before={NULL,&before},.after={NULL,&after},.sources=&sources}};
        if (effect&2) { view.cells=&cell; view.count=1; }
        if (effect&8) { view.definitions.before_formats=format_delta_store_view(&formats); view.definitions.after_formats=format_delta_snapshot_view(&styling); }
        if (effect&16) view.widths=(ColumnWidthPair){{0},{&width,1}};
        RangeEventInfo measured={0},info={0}; CHECK(range_event_measure(view,SIZE_MAX,&measured)==RANGE_EVENT_OK && measured.definition_bytes && !measured.format_bytes);
        unsigned char *data=dx_alloc(measured.bytes+1); size_t written=71; memset(data,0x77,measured.bytes+1);
        CHECK(range_event_encode(view,SIZE_MAX,data,measured.bytes,&written)==RANGE_EVENT_OK && data[written]==0x77);
        CHECK(range_event_preflight(data,written,&info)==RANGE_EVENT_OK && info.effects==effect && info.owned<=measured.owned);
        RangeEvent event={0}; CHECK(range_event_decode(data,written,info.owned+info.validation_scratch-1,&event)==RANGE_EVENT_LIMIT && !event.data);
        CHECK(range_event_decode(data,written,info.owned+info.validation_scratch,&event)==RANGE_EVENT_OK && event.definitions.effects==(effect&12u) && event.definitions.before_objects.objects.count==1);
        CHECK(event.definitions.before_objects.objects.items[0].name!=objects.items[0].name && event.definitions.after_objects.objects.items[0].row==20 && event.definitions.sources.versions.high_water==17 && !event.definitions.sources.versions.count);
        if (effect&8) CHECK(event.definitions.formats.after.count==1 && !event.formats.after.count);
        RangeEvent held=event;
        for (size_t n=0;n<written;n++) CHECK(range_event_decode(data,n,SIZE_MAX,&event)!=RANGE_EVENT_OK && !memcmp(&held,&event,sizeof(held)));
        data[4]=2; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_UNSUPPORTED && !memcmp(&held,&event,sizeof(held))); data[4]=DX_RANGE_EVENT_ENCODING;
        size_t nested=written-measured.definition_bytes;
        data[nested+8]=8; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(held))); data[nested+8]=9;
        data[36]=1; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(held))); data[36]=0;
        size_t old_bytes=u32(data+nested+36),new_bytes=u32(data+nested+40); CHECK(old_bytes==new_bytes);
        memcpy(data+nested+DX_DEFINITION_EVENT_HEADER_BYTES+old_bytes,data+nested+DX_DEFINITION_EVENT_HEADER_BYTES,old_bytes);
        CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(held)));
        CHECK(range_event_encode(view,SIZE_MAX,data,written,&written)==RANGE_EVENT_OK);
        CHECK(range_event_encode(view,SIZE_MAX,objects.items,measured.bytes,&written)==RANGE_EVENT_INVALID && objects.items[0].row==2);
        CHECK(range_event_encode(view,SIZE_MAX,data,measured.bytes,(size_t *)objects.items[0].name)==RANGE_EVENT_INVALID && !strcmp(objects.items[0].name,"İstanbul template"));
        free(data); range_event_free(&event);
        RangeEventView invalid=view; invalid.definitions.before.primary=&(Analysis){0}; CHECK(range_event_measure(invalid,SIZE_MAX,&info)==RANGE_EVENT_UNSUPPORTED);
        moved.source_id=1; CHECK(range_event_measure(view,SIZE_MAX,&info)==RANGE_EVENT_UNSUPPORTED); moved.source_id=0;
        moved.kind=OBJECT_TABLE; CHECK(range_event_measure(view,SIZE_MAX,&info)==RANGE_EVENT_UNSUPPORTED); moved.kind=OBJECT_PIVOT;
    }
    format_snapshot_free(&styling); format_free(&formats); objects_free(&objects);
}
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
    data[4]=1; CHECK(range_event_decode(data,bytes,SIZE_MAX,&event)==RANGE_EVENT_UNSUPPORTED); data[4]=DX_RANGE_EVENT_ENCODING;
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
    compound(); objects_compound(); range_event_free(&event); free(data); format_snapshot_free(&after); format_free(&formats); printf("PASS: %u complete range event checks\n",checks); return 0;
}
