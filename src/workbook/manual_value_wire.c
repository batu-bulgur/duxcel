#include "manual_value_wire.h"
#include <stdlib.h>
#include <string.h>
static unsigned number_tag(NumberKind kind) {
    switch (kind) {
    case NUM_NONE: return 0;
    case NUM_I8: return 1; case NUM_I16: return 2; case NUM_I32: return 3; case NUM_I64: return 4; case NUM_I128: return 5;
    case NUM_U8: return 6; case NUM_U16: return 7; case NUM_U32: return 8; case NUM_U64: return 9; case NUM_U128: return 10;
    case NUM_DECIMAL: return 11;
    default: return 255;
    }
}
static bool number_kind(unsigned tag,NumberKind *out) {
    switch (tag) {
    case 0: *out=NUM_NONE; break;
    case 1: *out=NUM_I8; break; case 2: *out=NUM_I16; break; case 3: *out=NUM_I32; break; case 4: *out=NUM_I64; break; case 5: *out=NUM_I128; break;
    case 6: *out=NUM_U8; break; case 7: *out=NUM_U16; break; case 8: *out=NUM_U32; break; case 9: *out=NUM_U64; break; case 10: *out=NUM_U128; break;
    case 11: *out=NUM_DECIMAL; break;
    default: return false;
    }
    return true;
}
static ManualValueWireStatus status(ManualValueStatus value) {
    return value==MANUAL_VALUE_OK ? MANUAL_VALUE_WIRE_OK : value==MANUAL_VALUE_LIMIT ? MANUAL_VALUE_WIRE_LIMIT : MANUAL_VALUE_WIRE_INVALID;
}
ManualValueWireStatus manual_value_wire_measure(ManualValueInput input,ManualValueWireInfo *info) {
    if (!info) return MANUAL_VALUE_WIRE_INVALID;
    ManualValueInfo value; ManualValueWireStatus result=status(manual_value_measure(input,&value)); if (result!=MANUAL_VALUE_WIRE_OK) return result;
    *info=(ManualValueWireInfo){DX_MANUAL_VALUE_WIRE_HEADER_BYTES+value.raw_bytes,value.owned,value.raw_bytes,value.canonical_bytes,input.present,input.kind,input.number};
    return MANUAL_VALUE_WIRE_OK;
}
static ManualValueWireStatus preflight(const void *data,size_t bytes,ManualValueInput *input,ManualValueWireInfo *info) {
    if (!data || !info || bytes<DX_MANUAL_VALUE_WIRE_HEADER_BYTES) return MANUAL_VALUE_WIRE_INVALID;
    if (bytes>DX_MANUAL_VALUE_WIRE_MAX_BYTES) return MANUAL_VALUE_WIRE_LIMIT;
    const unsigned char *p=data;
    if (memcmp(p,"DXMV",4) || p[4]!=DX_MANUAL_VALUE_WIRE_VERSION || p[5]>1 || p[6]>2 || p[7]>11) return MANUAL_VALUE_WIRE_INVALID;
    for (size_t i=10;i<16;i++) if (p[i]) return MANUAL_VALUE_WIRE_INVALID;
    uint64_t raw=0; for (unsigned i=0;i<8;i++) raw|=(uint64_t)p[16+i]<<(8*i);
    if (raw>DX_MANUAL_VALUE_TEXT_BYTES || raw>SIZE_MAX) return MANUAL_VALUE_WIRE_LIMIT;
    if (raw!=bytes-DX_MANUAL_VALUE_WIRE_HEADER_BYTES) return MANUAL_VALUE_WIRE_INVALID;
    NumberKind number; if (!number_kind(p[7],&number)) return MANUAL_VALUE_WIRE_INVALID;
    ManualValueInput next={p[5]!=0,(ManualValueKind)p[6],{number,p[8],p[9]},
        p[6]==0 ? NULL : (const char *)p+DX_MANUAL_VALUE_WIRE_HEADER_BYTES,(size_t)raw};
    ManualValueWireInfo measured; ManualValueWireStatus result=manual_value_wire_measure(next,&measured); if (result!=MANUAL_VALUE_WIRE_OK) return result;
    if (input) *input=next; *info=measured; return MANUAL_VALUE_WIRE_OK;
}
ManualValueWireStatus manual_value_wire_preflight(const void *data,size_t bytes,ManualValueWireInfo *info) {
    return preflight(data,bytes,NULL,info);
}
ManualValueWireStatus manual_value_wire_capture(ManualValueInput input,size_t reserved,ManualValuePacket *out) {
    if (!out || (out->bytes && !out->data) || (!out->bytes && out->data)) return MANUAL_VALUE_WIRE_INVALID;
    ManualValueWireInfo info; ManualValueWireStatus result=manual_value_wire_measure(input,&info); if (result!=MANUAL_VALUE_WIRE_OK) return result;
    if (out->bytes>SIZE_MAX-info.bytes || out->bytes+info.bytes>reserved) return MANUAL_VALUE_WIRE_LIMIT;
    unsigned char *next=dx_alloc(info.bytes); memcpy(next,"DXMV",4);
    next[4]=DX_MANUAL_VALUE_WIRE_VERSION; next[5]=input.present ? 1 : 0; next[6]=(unsigned char)input.kind;
    next[7]=(unsigned char)number_tag(input.number.kind); next[8]=input.number.precision; next[9]=input.number.scale;
    uint64_t bytes=input.raw_bytes; for (unsigned i=0;i<8;i++) next[16+i]=(unsigned char)(bytes>>(8*i));
    if (input.raw_bytes) memcpy(next+DX_MANUAL_VALUE_WIRE_HEADER_BYTES,input.raw,input.raw_bytes);
    manual_value_packet_free(out); *out=(ManualValuePacket){next,info.bytes}; return MANUAL_VALUE_WIRE_OK;
}
ManualValueWireStatus manual_value_wire_decode(const void *data,size_t bytes,size_t reserved,ManualValue **out) {
    if (!out) return MANUAL_VALUE_WIRE_INVALID;
    ManualValueInput input; ManualValueWireInfo info; ManualValueWireStatus result=preflight(data,bytes,&input,&info); if (result!=MANUAL_VALUE_WIRE_OK) return result;
    size_t prior=manual_value_view(*out).owned;
    if (prior>SIZE_MAX-info.value_owned || prior+info.value_owned>reserved) return MANUAL_VALUE_WIRE_LIMIT;
    return status(manual_value_build(input,info.value_owned,out));
}
void manual_value_packet_free(ManualValuePacket *packet) {
    if (packet) { free(packet->data); *packet=(ManualValuePacket){0}; }
}
