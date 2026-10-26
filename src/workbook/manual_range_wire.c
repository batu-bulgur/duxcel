#include "manual_range_wire.h"
#include <stdlib.h>
#include <string.h>
struct ManualRange { size_t rows,columns,count,owned; ManualValue *values[]; };
static bool add(size_t *total,size_t bytes) { if (*total>SIZE_MAX-bytes) return false; *total+=bytes; return true; }
static ManualRangeStatus shape(size_t rows,size_t columns,size_t count) {
    if (!rows || !columns) return MANUAL_RANGE_INVALID;
    if (columns>16384 || rows>DX_CLIPBOARD_CELLS/columns) return MANUAL_RANGE_LIMIT;
    return rows*columns==count ? MANUAL_RANGE_OK : MANUAL_RANGE_INVALID;
}
static ManualValueInput input(const ManualValue *value) {
    ManualValueView v=manual_value_view(value); return (ManualValueInput){v.present,v.kind,v.number,v.raw,v.raw_bytes};
}
static ManualRangeStatus nested(ManualValueWireStatus status) {
    return status==MANUAL_VALUE_WIRE_OK ? MANUAL_RANGE_OK : status==MANUAL_VALUE_WIRE_LIMIT ? MANUAL_RANGE_LIMIT : MANUAL_RANGE_INVALID;
}
static uint64_t get(const unsigned char *p,unsigned bytes) { uint64_t value=0; for (unsigned i=0;i<bytes;i++) value|=(uint64_t)p[i]<<(8*i); return value; }
static void put(unsigned char *p,uint64_t value,unsigned bytes) { for (unsigned i=0;i<bytes;i++) p[i]=(unsigned char)(value>>(8*i)); }
ManualRangeStatus manual_range_measure(ManualRangeView view,ManualRangeInfo *info) {
    if (!info || !view.values) return MANUAL_RANGE_INVALID;
    ManualRangeStatus status=shape(view.rows,view.columns,view.count); if (status!=MANUAL_RANGE_OK) return status;
    ManualRangeInfo next={view.rows,view.columns,view.count,DX_MANUAL_RANGE_HEADER_BYTES,sizeof(ManualRange)+view.count*sizeof(ManualValue *),0};
    for (size_t i=0;i<view.count;i++) {
        ManualValueWireInfo value; status=nested(manual_value_wire_measure(input(view.values[i]),&value)); if (status!=MANUAL_RANGE_OK) return status;
        if (!add(&next.bytes,4) || !add(&next.bytes,value.bytes) || next.bytes>DX_CLIPBOARD_BYTES || !add(&next.owned,value.value_owned)) return MANUAL_RANGE_LIMIT;
    }
    *info=next; return MANUAL_RANGE_OK;
}
static ManualRangeStatus header(const void *data,size_t bytes,ManualRangeInfo *info) {
    if (!data || bytes<DX_MANUAL_RANGE_HEADER_BYTES) return MANUAL_RANGE_INVALID;
    if (bytes>DX_CLIPBOARD_BYTES) return MANUAL_RANGE_LIMIT;
    const unsigned char *p=data;
    if (memcmp(p,"DXMR",4) || p[4]!=DX_MANUAL_RANGE_VERSION) return MANUAL_RANGE_INVALID;
    for (size_t i=5;i<8;i++) if (p[i]) return MANUAL_RANGE_INVALID;
    for (size_t i=20;i<24;i++) if (p[i]) return MANUAL_RANGE_INVALID;
    size_t rows=(size_t)get(p+8,4),columns=(size_t)get(p+12,4),count=(size_t)get(p+16,4);
    ManualRangeStatus status=shape(rows,columns,count); if (status!=MANUAL_RANGE_OK) return status;
    if (get(p+24,8)!=bytes-DX_MANUAL_RANGE_HEADER_BYTES) return MANUAL_RANGE_INVALID;
    *info=(ManualRangeInfo){rows,columns,count,bytes,sizeof(ManualRange)+count*sizeof(ManualValue *),0}; return MANUAL_RANGE_OK;
}
static ManualRangeStatus frame(const unsigned char *data,size_t bytes,size_t at,size_t *length) {
    if (at>bytes || bytes-at<4) return MANUAL_RANGE_INVALID;
    size_t n=(size_t)get(data+at,4);
    if (n>bytes-at-4) return MANUAL_RANGE_INVALID;
    *length=n; return MANUAL_RANGE_OK;
}
ManualRangeStatus manual_range_preflight(const void *data,size_t bytes,ManualRangeInfo *info) {
    if (!info) return MANUAL_RANGE_INVALID;
    ManualRangeInfo next; ManualRangeStatus status=header(data,bytes,&next); if (status!=MANUAL_RANGE_OK) return status;
    const unsigned char *p=data; size_t at=DX_MANUAL_RANGE_HEADER_BYTES;
    for (size_t i=0;i<next.count;i++) {
        size_t n; status=frame(p,bytes,at,&n); if (status!=MANUAL_RANGE_OK) return status;
        ManualValueWireInfo value; status=nested(manual_value_wire_preflight(p+at+4,n,&value)); if (status!=MANUAL_RANGE_OK) return status;
        if (!add(&next.owned,value.value_owned)) return MANUAL_RANGE_LIMIT;
        at+=4+n;
    }
    if (at!=bytes) return MANUAL_RANGE_INVALID;
    *info=next; return MANUAL_RANGE_OK;
}
ManualRangeStatus manual_range_capture(ManualRangeView view,size_t reserved,ManualRangePacket *out) {
    if (!out || (out->bytes && !out->data) || (!out->bytes && out->data)) return MANUAL_RANGE_INVALID;
    ManualRangeInfo info; ManualRangeStatus status=manual_range_measure(view,&info); if (status!=MANUAL_RANGE_OK) return status;
    if (out->bytes>SIZE_MAX-info.bytes || out->bytes+info.bytes>reserved) return MANUAL_RANGE_LIMIT;
    unsigned char *next=dx_alloc(info.bytes); memcpy(next,"DXMR",4); next[4]=DX_MANUAL_RANGE_VERSION;
    put(next+8,info.rows,4); put(next+12,info.columns,4); put(next+16,info.count,4); put(next+24,info.bytes-DX_MANUAL_RANGE_HEADER_BYTES,8);
    size_t at=DX_MANUAL_RANGE_HEADER_BYTES;
    for (size_t i=0;i<view.count;i++) {
        ManualValueInput in=input(view.values[i]); ManualValueWireInfo value;
        status=nested(manual_value_wire_measure(in,&value)); if (status!=MANUAL_RANGE_OK) { free(next); return status; }
        if (at>info.bytes || info.bytes-at<4 || value.bytes>info.bytes-at-4) { free(next); return MANUAL_RANGE_INVALID; }
        put(next+at,value.bytes,4); size_t written=0;
        status=nested(manual_value_wire_encode(in,next+at+4,value.bytes,&written));
        if (status!=MANUAL_RANGE_OK || written!=value.bytes) { free(next); return status==MANUAL_RANGE_OK ? MANUAL_RANGE_INVALID : status; }
        at+=4+written;
    }
    if (at!=info.bytes) { free(next); return MANUAL_RANGE_INVALID; }
    manual_range_packet_free(out); *out=(ManualRangePacket){next,info.bytes}; return MANUAL_RANGE_OK;
}
ManualRangeStatus manual_range_decode(const void *data,size_t bytes,size_t reserved,ManualRange **out) {
    if (!out) return MANUAL_RANGE_INVALID;
    ManualRangeInfo info; ManualRangeStatus status=manual_range_preflight(data,bytes,&info); if (status!=MANUAL_RANGE_OK) return status;
    size_t prior=manual_range_owned(*out); if (prior>SIZE_MAX-info.owned || prior+info.owned>reserved) return MANUAL_RANGE_LIMIT;
    ManualRange *next=dx_alloc(sizeof(*next)+info.count*sizeof(ManualValue *));
    next->rows=info.rows; next->columns=info.columns; next->count=info.count; next->owned=info.owned;
    const unsigned char *p=data; size_t at=DX_MANUAL_RANGE_HEADER_BYTES;
    for (size_t i=0;i<info.count;i++) {
        size_t n; status=frame(p,bytes,at,&n); if (status!=MANUAL_RANGE_OK) { manual_range_free(&next); return status; }
        ManualValueWireInfo value; status=nested(manual_value_wire_preflight(p+at+4,n,&value));
        if (status==MANUAL_RANGE_OK) status=nested(manual_value_wire_decode(p+at+4,n,value.value_owned,next->values+i));
        if (status!=MANUAL_RANGE_OK) { manual_range_free(&next); return status; }
        at+=4+n;
    }
    if (at!=bytes) { manual_range_free(&next); return MANUAL_RANGE_INVALID; }
    manual_range_free(out); *out=next; return MANUAL_RANGE_OK;
}
ManualRangeView manual_range_view(const ManualRange *range) {
    return range ? (ManualRangeView){range->rows,range->columns,range->count,(const ManualValue *const *)range->values} : (ManualRangeView){0};
}
size_t manual_range_owned(const ManualRange *range) { return range ? range->owned : 0; }
bool manual_range_at(const ManualRange *range,size_t row,size_t column,const ManualValue **out) {
    if (!range || !out || row>=range->rows || column>=range->columns) return false;
    *out=range->values[row*range->columns+column]; return true;
}
void manual_range_free(ManualRange **range) {
    if (!range || !*range) return;
    for (size_t i=0;i<(*range)->count;i++) manual_value_free((*range)->values+i);
    free(*range); *range=NULL;
}
void manual_range_packet_free(ManualRangePacket *packet) { if (packet) { free(packet->data); *packet=(ManualRangePacket){0}; } }
static const char *public_text(const void *context,size_t index) {
    const ManualRangeView *range=context; ManualValueView value=manual_value_view(range->values[index]);
    return !value.present || value.kind==MANUAL_VALUE_SQL_NULL ? NULL : value.canonical;
}
ManualRangeStatus manual_range_tsv_measure(ManualRangeView view,size_t *owned) {
    if (!owned) return MANUAL_RANGE_INVALID;
    ManualRangeInfo info; ManualRangeStatus status=manual_range_measure(view,&info); if (status!=MANUAL_RANGE_OK) return status;
    size_t bytes=0;
    if (!clipboard_tsv_measure_view((ClipboardTextView){view.rows,view.columns,public_text,&view},&bytes,NULL)) return MANUAL_RANGE_LIMIT;
    *owned=bytes+1; return MANUAL_RANGE_OK;
}
ManualRangeStatus manual_range_tsv(ManualRangeView view,size_t reserved,char **out) {
    if (!out) return MANUAL_RANGE_INVALID;
    size_t bytes=0; ManualRangeStatus status=manual_range_tsv_measure(view,&bytes); if (status!=MANUAL_RANGE_OK) return status;
    size_t prior=*out ? strlen(*out)+1 : 0; if (prior>SIZE_MAX-bytes || prior+bytes>reserved) return MANUAL_RANGE_LIMIT;
    char *next=dx_alloc(bytes); size_t written=0;
    if (!clipboard_tsv_encode_view((ClipboardTextView){view.rows,view.columns,public_text,&view},next,bytes,&written,NULL) || written!=bytes-1) { free(next); return MANUAL_RANGE_INVALID; }
    free(*out); *out=next; return MANUAL_RANGE_OK;
}
