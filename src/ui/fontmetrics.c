#include "fontmetrics.h"
#include <string.h>
static unsigned u16(const unsigned char *data) { return (unsigned)data[0]*256+data[1]; }
static uint32_t u32(const unsigned char *data) { return ((uint32_t)data[0]<<24)|((uint32_t)data[1]<<16)|((uint32_t)data[2]<<8)|data[3]; }
static int i16(const unsigned char *data) { unsigned value=u16(data); return value>=32768 ? (int)value-65536 : (int)value; }
static bool table(const unsigned char *data,size_t size,const char *tag,size_t *offset,size_t *length) {
    unsigned count=u16(data+4);
    for (unsigned i=0;i<count;i++) {
        size_t row=12+(size_t)i*16;
        if (!memcmp(data+row,tag,4)) { *offset=u32(data+row+8); *length=u32(data+row+12); return *offset<=size && *length<=size-*offset; }
    }
    return false;
}
bool ui_font_metrics_init(UIFontMetrics *out,const unsigned char *data,size_t size) {
    if (!out) return false; memset(out,0,sizeof(*out));
    if (!data || size<12 || (u32(data)!=0x00010000 && memcmp(data,"OTTO",4)) || u16(data+4)>(size-12)/16) return false;
    size_t head,hh,hm,mp,cm,head_len,hh_len,hm_len,mp_len,cm_len;
    if (!table(data,size,"head",&head,&head_len) || head_len<54 || !table(data,size,"hhea",&hh,&hh_len) || hh_len<36 || !table(data,size,"hmtx",&hm,&hm_len) || !table(data,size,"maxp",&mp,&mp_len) || mp_len<6 || !table(data,size,"cmap",&cm,&cm_len) || cm_len<4) return false;
    unsigned units=u16(data+head+18),metrics=u16(data+hh+34),glyphs=u16(data+mp+4),records=u16(data+cm+2);
    int ascent=i16(data+hh+4),descent=i16(data+hh+6);
    if (units<16 || units>16384 || !metrics || metrics>glyphs || hm_len<(size_t)metrics*4 || ascent<=descent || records>(cm_len-4)/8) return false;
    UIFontMetrics result={.data=data,.size=size,.hmtx=hm,.units=units,.metrics=metrics,.glyphs=glyphs,.ascent=ascent,.descent=descent,.height_per_em=(float)(ascent-descent)/units};
    if (result.height_per_em<.25f || result.height_per_em>4) return false;
    for (unsigned i=0;i<records;i++) {
        size_t row=cm+4+(size_t)i*8; unsigned platform=u16(data+row),encoding=u16(data+row+2); size_t relative=u32(data+row+4);
        if ((platform!=0 && (platform!=3 || (encoding!=1 && encoding!=10))) || relative>cm_len-2) continue;
        size_t start=cm+relative; unsigned format=u16(data+start);
        if (format==4 && cm_len-relative>=16) {
            size_t length=u16(data+start+2); unsigned count=u16(data+start+6)/2;
            if (length<=cm_len-relative && length>=16 && count && u16(data+start+6)%2==0 && count<=(length-16)/8 && !result.cmap4) { result.cmap4=start; result.cmap4_end=start+length; }
        } else if (format==12 && cm_len-relative>=16) {
            size_t length=u32(data+start+4),count=u32(data+start+12);
            if (length<=cm_len-relative && length>=16 && count<=(length-16)/12 && !result.cmap12) { result.cmap12=start; result.cmap12_end=start+length; }
        }
    }
    result.valid=result.cmap4 || result.cmap12; if (!result.valid) return false; *out=result; return true;
}
unsigned ui_font_glyph(const UIFontMetrics *m,unsigned cp) {
    if (!m || !m->valid || cp>0x10ffff) return 0; const unsigned char *data=m->data;
    if (m->cmap12) {
        size_t base=m->cmap12+16,low=0,high=u32(data+m->cmap12+12);
        while (low<high) { size_t middle=low+(high-low)/2; if (u32(data+base+middle*12+4)<cp) low=middle+1; else high=middle; }
        size_t row=base+low*12;
        if (low<u32(data+m->cmap12+12) && cp>=u32(data+row)) { uint64_t glyph=(uint64_t)u32(data+row+8)+cp-u32(data+row); return glyph<m->glyphs ? (unsigned)glyph : 0; }
    }
    if (cp>65535 || !m->cmap4) return 0;
    unsigned count=u16(data+m->cmap4+6)/2; size_t ends=m->cmap4+14,starts=ends+(size_t)count*2+2,deltas=starts+(size_t)count*2,ranges=deltas+(size_t)count*2,low=0,high=count;
    while (low<high) { size_t middle=low+(high-low)/2; if (u16(data+ends+middle*2)<cp) low=middle+1; else high=middle; }
    if (low==count || cp<u16(data+starts+low*2)) return 0;
    unsigned range=u16(data+ranges+low*2),glyph=cp;
    if (range) { size_t at=ranges+low*2+range+(cp-u16(data+starts+low*2))*2; if (at>m->cmap4_end-2) return 0; glyph=u16(data+at); if (!glyph) return 0; }
    glyph=(glyph+i16(data+deltas+low*2))&65535; return glyph<m->glyphs ? glyph : 0;
}
float ui_font_advance_em(const UIFontMetrics *m,unsigned cp) {
    unsigned glyph=ui_font_glyph(m,cp); if (!glyph) return 0;
    if (glyph>=m->metrics) glyph=m->metrics-1; return (float)u16(m->data+m->hmtx+(size_t)glyph*4)/m->units;
}
