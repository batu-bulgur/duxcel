#include "manual_value.h"
#include <stdlib.h>
#include <string.h>
struct ManualValue {
    ManualValueKind kind;
    NumberType number;
    size_t raw_bytes,canonical_bytes,owned;
    char data[];
};
static bool plain(NumberType n) { return n.kind==NUM_NONE && !n.precision && !n.scale; }
static bool utf8(const unsigned char *s,size_t n) {
    for (size_t i=0;i<n;) {
        unsigned c=s[i++],value,minimum,need;
        if (c && c<128) continue;
        if (c>=0xc2 && c<=0xdf) { value=c&31; minimum=128; need=1; }
        else if (c>=0xe0 && c<=0xef) { value=c&15; minimum=2048; need=2; }
        else if (c>=0xf0 && c<=0xf4) { value=c&7; minimum=65536; need=3; }
        else return false;
        if (need>n-i) return false;
        while (need--) { unsigned next=s[i++]; if ((next&0xc0)!=0x80) return false; value=(value<<6)|(next&63); }
        if (value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return false;
    }
    return true;
}
static bool digit(unsigned char c) { return c>='0' && c<='9'; }
static const char *integer_bound(NumberKind kind,bool negative) {
    switch (kind) {
    case NUM_I8: return negative ? "128" : "127";
    case NUM_I16: return negative ? "32768" : "32767";
    case NUM_I32: return negative ? "2147483648" : "2147483647";
    case NUM_I64: return negative ? "9223372036854775808" : "9223372036854775807";
    case NUM_I128: return negative ? "170141183460469231731687303715884105728" : "170141183460469231731687303715884105727";
    case NUM_U8: return "255";
    case NUM_U16: return "65535";
    case NUM_U32: return "4294967295";
    case NUM_U64: return "18446744073709551615";
    case NUM_U128: return "340282366920938463463374607431768211455";
    default: return NULL;
    }
}
static ManualValueStatus numeric(ManualValueInput in,char out[DX_MANUAL_VALUE_CANONICAL_BYTES+1],size_t *bytes) {
    NumberType type=in.number;
    if (type.kind==NUM_FLOAT) return MANUAL_VALUE_UNSUPPORTED;
    bool decimal=type.kind==NUM_DECIMAL;
    if (decimal ? !type.precision || type.precision>38 || type.scale>type.precision :
        !integer_bound(type.kind,false) || type.precision || type.scale) return MANUAL_VALUE_INVALID;
    if (!in.raw || !in.raw_bytes) return MANUAL_VALUE_INVALID;
    if (in.raw_bytes>DX_MANUAL_VALUE_NUMBER_BYTES) return MANUAL_VALUE_LIMIT;
    size_t at=0; bool negative=false;
    if (in.raw[at]=='+' || in.raw[at]=='-') negative=in.raw[at++]=='-';
    if (negative && type.kind>=NUM_U8 && type.kind<=NUM_U128) return MANUAL_VALUE_INVALID;
    size_t first=at; while (at<in.raw_bytes && digit((unsigned char)in.raw[at])) at++;
    if (at==first) return MANUAL_VALUE_INVALID;
    size_t end=at,fraction=at;
    if (at<in.raw_bytes && in.raw[at]=='.' && decimal) {
        fraction=++at; while (at<in.raw_bytes && digit((unsigned char)in.raw[at])) at++;
        if (at==fraction) return MANUAL_VALUE_INVALID;
    }
    if (at!=in.raw_bytes) return MANUAL_VALUE_INVALID;
    size_t fraction_bytes=at-fraction;
    while (first<end && in.raw[first]=='0') first++;
    size_t integer_bytes=end-first;
    bool nonzero=integer_bytes!=0;
    for (size_t i=fraction;i<at;i++) if (in.raw[i]!='0') nonzero=true;
    if (decimal) {
        if (fraction_bytes>type.scale || integer_bytes>(size_t)(type.precision-type.scale)) return MANUAL_VALUE_RANGE;
    } else {
        const char *bound=integer_bound(type.kind,negative); size_t length=strlen(bound);
        if (integer_bytes>length || (integer_bytes==length && memcmp(in.raw+first,bound,length)>0)) return MANUAL_VALUE_RANGE;
    }
    size_t n=0;
    if (negative && nonzero) out[n++]='-';
    if (integer_bytes) { memcpy(out+n,in.raw+first,integer_bytes); n+=integer_bytes; } else out[n++]='0';
    if (decimal && type.scale) {
        out[n++]='.';
        if (fraction_bytes) { memcpy(out+n,in.raw+fraction,fraction_bytes); n+=fraction_bytes; }
        for (size_t i=fraction_bytes;i<type.scale;i++) out[n++]='0';
    }
    out[n]=0; *bytes=n; return MANUAL_VALUE_OK;
}
static ManualValueStatus inspect(ManualValueInput input,ManualValueInfo *info,char canonical[DX_MANUAL_VALUE_CANONICAL_BYTES+1]) {
    ManualValueInfo next={0};
    if (!input.present || input.kind==MANUAL_VALUE_SQL_NULL) {
        if (input.kind!=MANUAL_VALUE_SQL_NULL || !plain(input.number) || input.raw || input.raw_bytes) return MANUAL_VALUE_INVALID;
        if (input.present) next.owned=sizeof(ManualValue);
    } else if (input.kind==MANUAL_VALUE_TEXT) {
        if (!plain(input.number) || (input.raw_bytes && !input.raw)) return MANUAL_VALUE_INVALID;
        if (input.raw_bytes>DX_MANUAL_VALUE_TEXT_BYTES) return MANUAL_VALUE_LIMIT;
        if (!utf8((const unsigned char *)input.raw,input.raw_bytes)) return MANUAL_VALUE_INVALID;
        next.raw_bytes=next.canonical_bytes=input.raw_bytes; next.owned=sizeof(ManualValue)+input.raw_bytes+1;
    } else if (input.kind==MANUAL_VALUE_NUMBER) {
        ManualValueStatus status=numeric(input,canonical,&next.canonical_bytes); if (status!=MANUAL_VALUE_OK) return status;
        next.raw_bytes=input.raw_bytes; next.owned=sizeof(ManualValue)+input.raw_bytes+1+next.canonical_bytes+1;
    } else return MANUAL_VALUE_INVALID;
    *info=next; return MANUAL_VALUE_OK;
}
ManualValueStatus manual_value_measure(ManualValueInput input,ManualValueInfo *info) {
    if (!info) return MANUAL_VALUE_INVALID;
    char canonical[DX_MANUAL_VALUE_CANONICAL_BYTES+1]; return inspect(input,info,canonical);
}
ManualValueStatus manual_value_build(ManualValueInput input,size_t reserved,ManualValue **out) {
    if (!out) return MANUAL_VALUE_INVALID;
    ManualValueInfo info; char canonical[DX_MANUAL_VALUE_CANONICAL_BYTES+1];
    ManualValueStatus status=inspect(input,&info,canonical); if (status!=MANUAL_VALUE_OK) return status;
    if (info.owned>reserved) return MANUAL_VALUE_LIMIT;
    ManualValue *next=info.owned ? dx_alloc(info.owned) : NULL;
    if (next) {
        next->kind=input.kind; next->number=input.number; next->raw_bytes=info.raw_bytes; next->canonical_bytes=info.canonical_bytes; next->owned=info.owned;
        if (input.kind!=MANUAL_VALUE_SQL_NULL) {
            if (input.raw_bytes) memcpy(next->data,input.raw,input.raw_bytes);
            next->data[input.raw_bytes]=0;
            if (input.kind==MANUAL_VALUE_NUMBER) memcpy(next->data+input.raw_bytes+1,canonical,info.canonical_bytes+1);
        }
    }
    manual_value_free(out); *out=next; return MANUAL_VALUE_OK;
}
ManualValueStatus manual_value_clone(const ManualValue *source,size_t reserved,ManualValue **out) {
    if (!out) return MANUAL_VALUE_INVALID;
    size_t owned=source ? source->owned : 0; if (owned>reserved) return MANUAL_VALUE_LIMIT;
    ManualValue *next=owned ? dx_alloc(owned) : NULL; if (next) memcpy(next,source,owned);
    manual_value_free(out); *out=next; return MANUAL_VALUE_OK;
}
ManualValueView manual_value_view(const ManualValue *value) {
    if (!value) return (ManualValueView){0};
    const char *raw=value->kind==MANUAL_VALUE_SQL_NULL ? NULL : value->data;
    return (ManualValueView){true,value->kind,value->number,raw,value->kind==MANUAL_VALUE_NUMBER ? raw+value->raw_bytes+1 : raw,
        value->raw_bytes,value->canonical_bytes,value->owned};
}
bool manual_value_equal(const ManualValue *a,const ManualValue *b) {
    if (!a || !b) return a==b;
    ManualValueView x=manual_value_view(a),y=manual_value_view(b);
    return x.kind==y.kind && x.number.kind==y.number.kind && x.number.precision==y.number.precision && x.number.scale==y.number.scale &&
        x.raw_bytes==y.raw_bytes && x.canonical_bytes==y.canonical_bytes &&
        (!x.raw_bytes || !memcmp(x.raw,y.raw,x.raw_bytes)) && (!x.canonical_bytes || !memcmp(x.canonical,y.canonical,x.canonical_bytes));
}
void manual_value_free(ManualValue **value) { if (value) { free(*value); *value=NULL; } }
