#include "definition_wire.h"
#include <string.h>
typedef char DefinitionTagsStable[(FILTER_CONTAINS==0 && FILTER_EQUAL==1 && FILTER_GREATER==2 && FILTER_LESS==3 && FILTER_NULL==4 && FILTER_NOT_NULL==5 &&
    FILTER_IN==6 && FILTER_NOT_IN==7 && FILTER_DATE_IN==8 && FILTER_DATE_NOT_IN==9 && FILTER_BETWEEN==10 && FILTER_COMPOSITE==11 &&
    AGG_SUM==0 && AGG_COUNT==1 && AGG_DISTINCT==2 && AGG_AVG==3 && AGG_MIN==4 && AGG_MAX==5) ? 1 : -1];
static bool utf8(const unsigned char *s,size_t n) {
    for (size_t i=0;i<n;) {
        unsigned c=s[i++],v,min; size_t tail;
        if (!c) return false; if (c<128) continue;
        if (c>=0xc2 && c<=0xdf) { v=c&31; min=0x80; tail=1; }
        else if (c>=0xe0 && c<=0xef) { v=c&15; min=0x800; tail=2; }
        else if (c>=0xf0 && c<=0xf4) { v=c&7; min=0x10000; tail=3; } else return false;
        if (tail>n-i) return false;
        while (tail--) { unsigned b=s[i++]; if ((b&0xc0)!=0x80) return false; v=(v<<6)|(b&63); }
        if (v<min || v>0x10ffff || (v>=0xd800 && v<=0xdfff)) return false;
    }
    return true;
}
bool definition_wire_text_measure(const char *s,DefinitionWireMeasure *m,size_t limit,size_t *length) {
    *length=0;
    if (m->owned>limit || m->bytes>SIZE_MAX-4) return false;
    m->bytes+=4; if (!s) return true;
    size_t room=limit-m->owned,n=0; while (n<room && s[n]) n++;
    if (n==room || !utf8((const unsigned char *)s,n) || n>SIZE_MAX-m->bytes) return false;
    m->owned+=n+1; m->bytes+=n; *length=n; return true;
}
void definition_wire_put(DefinitionWireWriter *w,uint64_t v,unsigned n) { for (unsigned i=0;i<n;i++) { *w->p++=(unsigned char)(v&255u); v>>=8; } }
void definition_wire_text_write(DefinitionWireWriter *w,const char *s) {
    size_t n=s ? strlen(s) : 0; definition_wire_put(w,s ? n : UINT32_MAX,4);
    if (n) { memcpy(w->p,s,n); w->p+=n; }
}
static const unsigned char *span(DefinitionWireReader *r,size_t n) {
    if (!r->ok || r->at>r->bytes || n>r->bytes-r->at) { r->ok=false; return NULL; }
    const unsigned char *p=r->data+r->at; r->at+=n; return p;
}
uint64_t definition_wire_get(DefinitionWireReader *r,unsigned n) { const unsigned char *p=span(r,n); uint64_t v=0; if (p) for (unsigned i=0;i<n;i++) v|=(uint64_t)p[i]<<(8*i); return v; }
bool definition_wire_text_read(DefinitionWireReader *r,char **out,size_t *length) {
    size_t n=(size_t)definition_wire_get(r,4); *length=0;
    if (!r->ok || n==UINT32_MAX) return false;
    if (r->owned>=r->owned_limit || n>r->owned_limit-r->owned-1) { r->ok=false; return false; }
    const unsigned char *p=span(r,n);
    if (!p || !utf8(p,n)) { r->ok=false; return false; }
    r->owned+=n+1; *length=n;
    if (out) { *out=dx_alloc(n+1); if (n) memcpy(*out,p,n); (*out)[n]=0; }
    return true;
}
