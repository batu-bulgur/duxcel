#include "number.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#define NUMBER_TEXT_LIMIT 4096
typedef struct { char digits[NUMBER_TEXT_LIMIT+1]; size_t length; int point; bool negative; } Number;
static bool zero(const Number *n) { return n->length==1 && n->digits[0]=='0'; }
static bool parse(Number *n,const char *text) {
    n->length=0; n->point=0; n->negative=false; if (!text || strlen(text)>NUMBER_TEXT_LIMIT) return false;
    const unsigned char *p=(const unsigned char *)text;
    while (isspace(*p)) p++;
    if (*p=='-' || *p=='+') n->negative=*p++=='-';
    bool dot=false,any=false;
    while (isdigit(*p) || (*p=='.' && !dot)) {
        if (*p=='.') dot=true;
        else { n->digits[n->length++]=(char)*p; any=true; if (!dot) n->point++; }
        p++;
    }
    if (!any) return false;
    if (*p=='e' || *p=='E') {
        p++; bool negative=false; if (*p=='-' || *p=='+') negative=*p++=='-';
        if (!isdigit(*p)) return false;
        int exponent=0;
        while (isdigit(*p)) { if (exponent<1000000) exponent=exponent*10+(*p-'0'); p++; }
        if (exponent>1000000) exponent=1000000;
        n->point+=negative ? -exponent : exponent;
    }
    while (isspace(*p)) p++;
    if (*p) return false;
    size_t leading=0; while (leading<n->length && n->digits[leading]=='0') leading++;
    if (leading==n->length) { n->digits[0]='0'; n->digits[1]=0; n->length=1; n->point=1; n->negative=false; return true; }
    n->point-=(int)leading; n->length-=leading; memmove(n->digits,n->digits+leading,n->length);
    while (n->length>1 && n->digits[n->length-1]=='0') n->length--;
    n->digits[n->length]=0; return true;
}
static int compare(const Number *a,const Number *b) {
    if (a->negative!=b->negative) return a->negative ? -1 : 1;
    int result=0;
    if (zero(a) || zero(b)) result=zero(a) ? zero(b) ? 0 : -1 : 1;
    else if (a->point!=b->point) result=a->point<b->point ? -1 : 1;
    else {
        size_t length=a->length>b->length ? a->length : b->length;
        for (size_t i=0;i<length;i++) {
            char x=i<a->length ? a->digits[i] : '0',y=i<b->length ? b->digits[i] : '0';
            if (x!=y) { result=x<y ? -1 : 1; break; }
        }
    }
    return a->negative ? -result : result;
}
static bool bounds(NumberType t,Number *min,Number *max) {
    static const char *lows[]={"0","-128","-32768","-2147483648","-9223372036854775808","-170141183460469231731687303715884105728","0","0","0","0","0"};
    static const char *highs[]={"0","127","32767","2147483647","9223372036854775807","170141183460469231731687303715884105727","255","65535","4294967295","18446744073709551615","340282366920938463463374607431768211455"};
    if (t.kind>=NUM_I8 && t.kind<=NUM_U128) return parse(min,lows[t.kind]) && parse(max,highs[t.kind]);
    if (t.kind!=NUM_DECIMAL || !t.precision || t.precision>38 || t.scale>t.precision) return false;
    memset(max,0,sizeof(*max)); max->length=t.precision; max->point=t.precision-t.scale; memset(max->digits,'9',max->length);
    *min=*max; min->negative=true; return true;
}
static void increment(char digits[48]) {
    size_t length=strlen(digits),i=length;
    while (i && digits[i-1]=='9') digits[--i]='0';
    if (i) digits[i-1]++; else { memmove(digits+1,digits,length+1); digits[0]='1'; }
}
bool number_display(const char *raw,unsigned decimals,bool accounting,char output[512]) {
    Number value; if (decimals>4 || !parse(&value,raw) || value.point>350) return false;
    char digits[384]; int kept=value.point+(int)decimals; size_t length=kept>0 ? (size_t)kept : 1;
    for (size_t i=0;i<length;i++) digits[i]=kept>0 && i<value.length ? value.digits[i] : '0'; digits[length]=0;
    if ((kept==0 && value.digits[0]>='5') || (kept>0 && (size_t)kept<value.length && value.digits[kept]>='5')) increment(digits);
    length=strlen(digits);
    if (length<=decimals) { size_t padding=decimals+1-length; memmove(digits+padding,digits,length+1); memset(digits,'0',padding); length+=padding; }
    size_t integer=length-decimals,at=0;
    if (value.negative) { if (accounting) output[at++]='('; else { memcpy(output+at,"\xe2\x88\x92",3); at+=3; } }
    for (size_t i=0;i<integer;i++) { output[at++]=digits[i]; if (i+1<integer && (integer-i-1)%3==0) output[at++]=','; }
    if (decimals) { output[at++]='.'; memcpy(output+at,digits+integer,decimals); at+=decimals; }
    if (value.negative && accounting) output[at++]=')'; output[at]=0; return true;
}
char *number_filter_sql(NumberType type,const char *column,NumberComparison op,const char *text,const char *value) {
    if (type.kind==NUM_NONE || (unsigned)op>NUMBER_LESS) return NULL;
    const char *operation=op==NUMBER_GREATER ? ">" : op==NUMBER_LESS ? "<" : "=";
    if (type.kind==NUM_FLOAT) return dx_printf("CAST(%s AS DOUBLE) %s CAST(%s AS DOUBLE)",column,operation,value);
    Number input,min,max;
    if (!bounds(type,&min,&max)) return NULL;
    if (!parse(&input,text)) return dx_strdup("error('Enter a number using digits, a decimal point and an optional exponent.')");
    int low=compare(&input,&min),high=compare(&input,&max);
    if (low<0 || high>0) {
        bool all=(low<0 && op==NUMBER_GREATER) || (high>0 && op==NUMBER_LESS);
        return all ? dx_printf("%s IS NOT NULL",column) : dx_strdup("false");
    }
    int scale=type.kind==NUM_DECIMAL ? type.scale : 0,cut=input.point+scale;
    bool discarded=false;
    for (size_t i=cut>0 ? (size_t)cut : 0;i<input.length;i++) if (input.digits[i]!='0') { discarded=true; break; }
    if (op==NUMBER_EQUAL && discarded) return dx_strdup("false");
    char units[48]={0}; size_t count=cut>0 ? (size_t)cut : 1;
    /* A value inside its native bounds has at most 39 scaled integer digits. */
    if (count>39) return dx_strdup("error('Numeric filter exceeds the column precision.')");
    for (size_t i=0;i<count;i++) units[i]=cut>0 && i<input.length ? input.digits[i] : '0';
    if (discarded && ((input.negative && op==NUMBER_GREATER) || (!input.negative && op==NUMBER_LESS))) increment(units);
    size_t length=strlen(units); bool nonzero=false; for (size_t i=0;i<length;i++) nonzero|=units[i]!='0';
    char canonical[96]={0}; size_t at=0;
    if (input.negative && nonzero) canonical[at++]='-';
    if (!scale) { memcpy(canonical+at,units,length); at+=length; }
    else if (length<=(size_t)scale) {
        canonical[at++]='0'; canonical[at++]='.';
        for (size_t i=length;i<(size_t)scale;i++) canonical[at++]='0';
        memcpy(canonical+at,units,length); at+=length;
    } else {
        size_t whole=length-(size_t)scale; memcpy(canonical+at,units,whole); at+=whole; canonical[at++]='.';
        memcpy(canonical+at,units+whole,(size_t)scale); at+=(size_t)scale;
    }
    canonical[at]=0;
    return dx_printf("%s %s cast_to_type('%s',%s)",column,operation,canonical,column);
}
