#include "filterlabel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *text; size_t capacity,length; bool truncated; } Label;
static void bytes(Label *out,const char *text,size_t length) {
    if (out->truncated) return;
    if (length>=out->capacity-out->length) { out->truncated=true; return; }
    memcpy(out->text+out->length,text,length); out->length+=length; out->text[out->length]=0;
}
/* Invalid input bytes become one replacement glyph; a clipped prefix always
   ends on a codepoint boundary. This also keeps imported field names safe. */
static size_t utf8_length(const unsigned char *p) {
    if (*p<0x80) return 1;
    size_t count=*p>=0xc2 && *p<=0xdf ? 2 : *p>=0xe0 && *p<=0xef ? 3 : *p>=0xf0 && *p<=0xf4 ? 4 : 0;
    if (!count) return 0;
    for (size_t i=1;i<count;i++) if (!p[i] || (p[i]&0xc0)!=0x80) return 0;
    if ((*p==0xe0 && p[1]<0xa0) || (*p==0xed && p[1]>=0xa0) || (*p==0xf0 && p[1]<0x90) || (*p==0xf4 && p[1]>=0x90)) return 0;
    return count;
}
static void text(Label *out,const char *value) {
    if (!value) return;
    for (const unsigned char *p=(const unsigned char *)value;*p && !out->truncated;) {
        size_t count=utf8_length(p);
        if (!count) { bytes(out,"�",3); p++; }
        else { if (*p<32 || *p==127) bytes(out," ",1); else bytes(out,(const char *)p,count); p+=count; }
    }
}
static void finish(Label *out) {
    if (!out->truncated) return;
    while (out->length>out->capacity-4) out->length=dx_utf8_prev(out->text,out->length);
    memcpy(out->text+out->length,"…",4); out->length+=3;
}
static void value(Label *out,const char *raw,bool *has_null) {
    if (!raw) { text(out,"NULL"); *has_null=true; return; }
    size_t length=strlen(raw);
    bool quote=!length || !strcmp(raw,"NULL") || raw[0]==' ' || raw[length-1]==' ';
    for (const unsigned char *p=(const unsigned char *)raw;*p && !quote;p++) quote=*p<32 || *p==127 || *p=='"' || *p=='\\' || *p==',';
    if (!quote) { text(out,raw); return; }
    text(out,"\"");
    for (const unsigned char *p=(const unsigned char *)raw;*p && !out->truncated;) {
        char escape[8];
        if (*p=='"' || *p=='\\') { escape[0]='\\'; escape[1]=(char)*p++; escape[2]=0; text(out,escape); }
        else if (*p<32 || *p==127) {
            if (*p=='\n') text(out,"\\n"); else if (*p=='\r') text(out,"\\r"); else if (*p=='\t') text(out,"\\t");
            else { snprintf(escape,sizeof(escape),"\\u%04x",*p); text(out,escape); } p++;
        } else { size_t count=utf8_length(p); if (!count) { text(out,"�"); p++; } else { bytes(out,(const char *)p,count); p+=count; } }
    }
    text(out,"\"");
}
static void values_free(char **values,size_t count) { for (size_t i=0;i<count;i++) free(values[i]); free(values); }

typedef struct { int year,month,day,hour,minute,second; } Date;
static bool digits(const char *text,size_t count,int *out) {
    int value=0; for (size_t i=0;i<count;i++) { if (text[i]<'0' || text[i]>'9') return false; value=value*10+text[i]-'0'; } *out=value; return true;
}
static int days(int year,int month) {
    static const int count[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return count[month-1]+(month==2 && (!(year%400) || (year%100 && !(year%4))));
}
static bool date(const char *raw,size_t length,Date *out) {
    Date d={0};
    if (length<10 || raw[4]!='-' || raw[7]!='-' || !digits(raw,4,&d.year) || !digits(raw+5,2,&d.month) || !digits(raw+8,2,&d.day) || d.month<1 || d.month>12 || d.day<1 || d.day>days(d.year,d.month)) return false;
    if (length>10) {
        if (length<19 || (raw[10]!=' ' && raw[10]!='T') || raw[13]!=':' || raw[16]!=':' || !digits(raw+11,2,&d.hour) || !digits(raw+14,2,&d.minute) || !digits(raw+17,2,&d.second) || d.hour>23 || d.minute>59 || d.second>59) return false;
        if (length>19) { if (raw[19]!='.' || length==20) return false; for (size_t i=20;i<length;i++) if (raw[i]!='0') return false; }
    }
    *out=d; return true;
}
static bool same_date(Date a,Date b) { return a.year==b.year && a.month==b.month && a.day==b.day && a.hour==b.hour && a.minute==b.minute && a.second==b.second; }
static Date next_day(Date d) { if (++d.day>days(d.year,d.month)) { d.day=1; if (++d.month>12) { d.month=1; d.year++; } } return d; }
static Date next_hour(Date d) { if (++d.hour==24) { d.hour=0; d=next_day(d); } return d; }
static Date next_minute(Date d) { if (++d.minute==60) { d.minute=0; d=next_hour(d); } return d; }
/* Date checklist keys are real half-open bounds separated by a tab. Recognize
   only exact hierarchy units; arbitrary ranges retain both bounds explicitly. */
static bool date_value(Label *out,const char *raw,bool *has_null) {
    if (!raw) { value(out,NULL,has_null); return true; }
    const char *tab=strchr(raw,'\t');
    if (!tab || tab==raw || !tab[1] || strchr(tab+1,'\t')) return false;
    Date lo,hi; char display[96]; size_t first=(size_t)(tab-raw);
    bool midnight=false;
    if (date(raw,first,&lo) && date(tab+1,strlen(tab+1),&hi)) {
        midnight=!lo.hour && !lo.minute && !lo.second;
        Date next=lo; next.year++;
        if (midnight && lo.month==1 && lo.day==1 && same_date(next,hi)) snprintf(display,sizeof(display),"%04d",lo.year);
        else {
            next=lo; if (++next.month==13) { next.month=1; next.year++; }
            if (midnight && lo.day==1 && same_date(next,hi)) {
                static const char *months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
                snprintf(display,sizeof(display),"%s %04d",months[lo.month-1],lo.year);
            } else if (midnight && same_date(next_day(lo),hi)) snprintf(display,sizeof(display),"%04d-%02d-%02d",lo.year,lo.month,lo.day);
            else if (!lo.minute && !lo.second && same_date(next_hour(lo),hi)) snprintf(display,sizeof(display),"%04d-%02d-%02d %02d:00",lo.year,lo.month,lo.day,lo.hour);
            else if (!lo.second && same_date(next_minute(lo),hi)) snprintf(display,sizeof(display),"%04d-%02d-%02d %02d:%02d",lo.year,lo.month,lo.day,lo.hour,lo.minute);
            else *display=0;
        }
        if (*display) { text(out,display); return true; }
    }
    text(out,"[");
    for (size_t i=0;i<first && !out->truncated;) { size_t count=utf8_length((const unsigned char *)raw+i); if (!count) { text(out,"�"); i++; } else { if ((unsigned char)raw[i]<32 || raw[i]==127) bytes(out," ",1); else bytes(out,raw+i,count); i+=count; } }
    text(out,", "); text(out,tab+1); text(out,")"); return true;
}
static bool list(Label *out,FilterOp op,const char *raw,UIFilterLabel *meta) {
    char **values=NULL; size_t count=0;
    if (!analysis_parse_values(raw,&values,&count)) return false;
    meta->value_count=count; meta->excluded=op==FILTER_NOT_IN || op==FILTER_DATE_NOT_IN;
    if (!count) text(out,meta->excluded ? "All values" : "No values");
    else {
        if (meta->excluded) text(out,"Except ");
        for (size_t i=0;i<count;i++) {
            if (i) text(out,", ");
            if (op==FILTER_DATE_IN || op==FILTER_DATE_NOT_IN) { if (!date_value(out,values[i],&meta->has_null)) { values_free(values,count); return false; } }
            else value(out,values[i],&meta->has_null);
        }
    }
    values_free(values,count); return true;
}
static bool condition(Label *out,FilterOp op,const char *raw,UIFilterLabel *meta) {
    if (op>=FILTER_IN && op<=FILTER_DATE_NOT_IN) return list(out,op,raw,meta);
    if (op==FILTER_NULL || op==FILTER_NOT_NULL) { text(out,op==FILTER_NULL ? "NULL" : "Not NULL"); meta->has_null=true; return true; }
    if (op==FILTER_GREATER || op==FILTER_LESS) text(out,op==FILTER_GREATER ? "> " : "< ");
    else if (op==FILTER_BETWEEN) {
        char **bounds=NULL; size_t count=0;
        if (!analysis_parse_values(raw,&bounds,&count)) return false;
        bool valid=count==2 && bounds[0] && bounds[1];
        if (valid) { text(out,"Between "); value(out,bounds[0],&meta->has_null); text(out," and "); value(out,bounds[1],&meta->has_null); }
        values_free(bounds,count); return valid;
    } else if (op!=FILTER_EQUAL && op!=FILTER_CONTAINS) return false;
    /* The reference pill shows the term for these two simple conditions. */
    value(out,raw ? raw : "",&meta->has_null); return true;
}
void ui_filter_label_set(UIFilterLabel *out,const char *field,FilterOp op,const char *raw) {
    if (!out) return;
    UIFilterLabel next={0}; char name[161]={0},criteria[348]={0};
    Label prefix={name,sizeof(name),0,false},body={criteria,sizeof(criteria),0,false};
    text(&prefix,field && *field ? field : "Field"); finish(&prefix);
    /* Match the persisted per-filter bound before calling shared parsers. */
    size_t length=0; if (raw) while (length<=4*1024*1024 && raw[length]) length++;
    next.valid=(unsigned)op<=FILTER_OP_MAX && length<=4*1024*1024;
    if (next.valid && op==FILTER_COMPOSITE) {
        FilterOp list_op,condition_op; char *list_raw=NULL,*condition_raw=NULL;
        next.valid=analysis_filter_unpack(raw,&list_op,&list_raw,&condition_op,&condition_raw);
        if (next.valid) {
            char selected[171]={0},term[171]={0}; Label a={selected,sizeof(selected),0,false},b={term,sizeof(term),0,false};
            if (condition_op==FILTER_CONTAINS) text(&b,"Contains ");
            else if (condition_op==FILTER_EQUAL) text(&b,"= ");
            next.valid=list(&a,list_op,list_raw,&next) && condition(&b,condition_op,condition_raw,&next);
            finish(&a); finish(&b); text(&body,selected); text(&body," AND "); text(&body,term); next.truncated=a.truncated || b.truncated;
        }
        free(list_raw); free(condition_raw);
    } else if (next.valid) next.valid=condition(&body,op,raw,&next);
    if (!next.valid) { memset(&next,0,sizeof(next)); memset(criteria,0,sizeof(criteria)); body=(Label){criteria,sizeof(criteria),0,false}; text(&body,"Invalid filter"); }
    finish(&body);
    Label result={next.text,sizeof(next.text),0,false}; text(&result,name); text(&result,": "); text(&result,criteria); finish(&result);
    next.truncated=next.truncated || prefix.truncated || body.truncated || result.truncated;
    *out=next;
}
