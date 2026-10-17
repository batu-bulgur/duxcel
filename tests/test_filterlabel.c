#include "ui/filterlabel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL filterlabel:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static bool utf8(const char *text) {
    const unsigned char *p=(const unsigned char *)text;
    while (*p) {
        unsigned n=*p<128 ? 0 : *p>=0xc2 && *p<=0xdf ? 1 : *p>=0xe0 && *p<=0xef ? 2 : *p>=0xf0 && *p<=0xf4 ? 3 : 9;
        if (n>3 || (*p==0xe0 && p[1]<0xa0) || (*p==0xed && p[1]>=0xa0) || (*p==0xf0 && p[1]<0x90) || (*p==0xf4 && p[1]>=0x90)) return false;
        p++; for (unsigned i=0;i<n;i++,p++) if (!*p || (*p&0xc0)!=0x80) return false;
    }
    return true;
}
static UIFilterLabel label(const char *field,FilterOp op,const char *raw,const char *expected) {
    UIFilterLabel out; memset(&out,0xa5,sizeof(out)); ui_filter_label_set(&out,field,op,raw);
    CHECK(out.valid); CHECK(!out.truncated); CHECK(!strcmp(out.text,expected)); CHECK(utf8(out.text));
    return out;
}
static void test_reference_and_owned_values(void) {
    UIFilterLabel out=label("Status",FILTER_IN,"[\"Posted\"]","Status: Posted"); CHECK(out.value_count==1 && !out.has_null && !out.excluded);
    out=label("City",FILTER_IN,"[\"Ankara\",\"Bursa\",\"İzmir\"]","City: Ankara, Bursa, İzmir"); CHECK(out.value_count==3);
    label("Counterparty",FILTER_CONTAINS,"Northwind","Counterparty: Northwind");
    label("Currency",FILTER_EQUAL,"USD","Currency: USD");
    label("Amount",FILTER_GREATER,"123456789012345678901234567890.12","Amount: > 123456789012345678901234567890.12");
    label("Amount",FILTER_LESS,"-0.00000000000000000001","Amount: < -0.00000000000000000001");
    label("Amount",FILTER_BETWEEN,"[\"-12345678901234567890.12\",\"99999999999999999999.99\"]","Amount: Between -12345678901234567890.12 and 99999999999999999999.99");
    out=label("Status",FILTER_NOT_IN,"[\"Pending\",\"Void\"]","Status: Except Pending, Void"); CHECK(out.excluded && out.value_count==2);
    out=label("Status",FILTER_NOT_IN,"[]","Status: All values"); CHECK(out.excluded && !out.value_count);
    out=label("Status",FILTER_IN,"[]","Status: No values"); CHECK(!out.excluded && !out.value_count);
    char field[32]="Status",raw[64]="[\"Posted\"]"; ui_filter_label_set(&out,field,FILTER_IN,raw);
    memset(field,'x',strlen(field)); memset(raw,'x',strlen(raw)); CHECK(!strcmp(out.text,"Status: Posted"));
    ui_filter_label_set(&out,"Original",FILTER_EQUAL,"value"); ui_filter_label_set(&out,"Alias",FILTER_EQUAL,out.text); CHECK(!strcmp(out.text,"Alias: Original: value"));
    ui_filter_label_set(&out,out.text,FILTER_EQUAL,"last"); CHECK(!strcmp(out.text,"Alias: Original: value: last"));
    label(NULL,FILTER_EQUAL,"x","Field: x"); label("",FILTER_EQUAL,"x","Field: x");
}
static void test_typed_values(void) {
    UIFilterLabel out=label("Text",FILTER_IN,"[null,\"\",\"NULL\",\"\\\"\\\"\"]","Text: NULL, \"\", \"NULL\", \"\\\"\\\"\"");
    CHECK(out.has_null && out.value_count==4);
    out=label("Text",FILTER_IN,"[\"NULL\"]","Text: \"NULL\""); CHECK(!out.has_null);
    out=label("Text",FILTER_NULL,NULL,"Text: NULL"); CHECK(out.has_null);
    out=label("Text",FILTER_NOT_NULL,"unused","Text: Not NULL"); CHECK(out.has_null && !out.excluded);
    label("Text",FILTER_EQUAL,"","Text: \"\""); label("Text",FILTER_EQUAL,NULL,"Text: \"\"");
    label("Text",FILTER_IN,"[\"a, b\",\"a\\nb\",\"\\\\path\",\" trailing \"]","Text: \"a, b\", \"a\\nb\", \"\\\\path\", \" trailing \"");
    label("Text\ncolumn",FILTER_EQUAL,"a\t\r\001\177","Text column: \"a\\t\\r\\u0001\\u007f\"");
    label("Emoji",FILTER_IN,"[\"\\uD83D\\uDE00\",\"\\u011f\"]","Emoji: 😀, ğ");
}
static void test_date_hierarchy(void) {
    UIFilterLabel out=label("Date",FILTER_DATE_IN,"[\"2026-01-01 00:00:00\\t2027-01-01 00:00:00\"]","Date: 2026"); CHECK(out.value_count==1);
    label("Date",FILTER_DATE_IN,"[\"2026-01-01\\t2026-02-01\",\"2026-12-01\\t2027-01-01\"]","Date: Jan 2026, Dec 2026");
    label("Date",FILTER_DATE_IN,"[\"2024-02-29\\t2024-03-01\",\"2026-12-31\\t2027-01-01\"]","Date: 2024-02-29, 2026-12-31");
    label("Stamp",FILTER_DATE_IN,"[\"2026-12-31 23:00:00\\t2027-01-01 00:00:00\"]","Stamp: 2026-12-31 23:00");
    label("Stamp",FILTER_DATE_IN,"[\"2026-12-31T23:59:00.000000\\t2027-01-01T00:00:00.000000\"]","Stamp: 2026-12-31 23:59");
    label("Stamp",FILTER_DATE_IN,"[\"2026-01-02 14:37:00\\t2026-01-02 14:38:00\"]","Stamp: 2026-01-02 14:37");
    out=label("Date",FILTER_DATE_NOT_IN,"[null,\"2026-01-01\\t2026-02-01\"]","Date: Except NULL, Jan 2026"); CHECK(out.has_null && out.excluded && out.value_count==2);
    label("Date",FILTER_DATE_NOT_IN,"[]","Date: All values");
    label("Date",FILTER_DATE_IN,"[\"2026-01-01\\t2026-01-04\"]","Date: [2026-01-01, 2026-01-04)");
    /* An invalid date must not be promoted to a misleading hierarchy unit. */
    label("Date",FILTER_DATE_IN,"[\"2026-02-29\\t2026-03-01\"]","Date: [2026-02-29, 2026-03-01)");
    label("Stamp",FILTER_DATE_IN,"[\"2026-01-01 00:00:00+00\\t2026-02-01 00:00:00+00\"]","Stamp: [2026-01-01 00:00:00+00, 2026-02-01 00:00:00+00)");
}
static void test_composite(void) {
    char *payload=analysis_filter_pack(FILTER_IN,"[\"Posted\",\"Pending\"]",FILTER_CONTAINS,"Post"); CHECK(payload);
    UIFilterLabel out=label("Status",FILTER_COMPOSITE,payload,"Status: Posted, Pending AND Contains Post"); CHECK(out.value_count==2 && !out.excluded);
    memset(payload,'x',strlen(payload)); free(payload); CHECK(!strcmp(out.text,"Status: Posted, Pending AND Contains Post"));
    payload=analysis_filter_pack(FILTER_NOT_IN,"[null,\"NULL\"]",FILTER_BETWEEN,"[\"A\",\"Z\"]"); CHECK(payload);
    out=label("Text",FILTER_COMPOSITE,payload,"Text: Except NULL, \"NULL\" AND Between A and Z"); CHECK(out.has_null && out.excluded && out.value_count==2); free(payload);
    payload=analysis_filter_pack(FILTER_DATE_IN,"[\"2026-01-01\\t2027-01-01\"]",FILTER_GREATER,"2026-01-12"); CHECK(payload);
    label("Date",FILTER_COMPOSITE,payload,"Date: 2026 AND > 2026-01-12"); free(payload);
}
static void test_bounds_and_invalid(void) {
    UIFilterLabel out;
    const char *invalid[]={"", "[", "[null,]", "[1]", "[\"\\u0000\"]", "[\"\\ud800\"]", "[\"x\"] trailing"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) { ui_filter_label_set(&out,"Field",FILTER_IN,invalid[i]); CHECK(!out.valid && !out.has_null && !out.value_count && !strcmp(out.text,"Field: Invalid filter")); }
    ui_filter_label_set(&out,"Field",FILTER_IN,NULL); CHECK(!out.valid);
    ui_filter_label_set(&out,"Field",(FilterOp)-1,"x"); CHECK(!out.valid);
    ui_filter_label_set(&out,"Field",(FilterOp)(FILTER_OP_MAX+1),"x"); CHECK(!out.valid);
    const char *bad_dates[]={"[\"2026-01-01\"]","[\"\\t2026-01-01\"]","[\"2026-01-01\\t\"]","[\"a\\tb\\tc\"]"};
    for (size_t i=0;i<sizeof(bad_dates)/sizeof(*bad_dates);i++) { ui_filter_label_set(&out,"Date",FILTER_DATE_IN,bad_dates[i]); CHECK(!out.valid); }
    const char *bad_between[]={"[]","[null,\"b\"]","[\"a\"]","[\"a\",\"b\",\"c\"]"};
    for (size_t i=0;i<sizeof(bad_between)/sizeof(*bad_between);i++) { ui_filter_label_set(&out,"Amount",FILTER_BETWEEN,bad_between[i]); CHECK(!out.valid); }
    ui_filter_label_set(&out,"Field",FILTER_COMPOSITE,"[]"); CHECK(!out.valid);
    ui_filter_label_set(NULL,"Field",FILTER_IN,NULL);
    char name[4096],raw[4096];
    for (size_t padding=0;padding<16;padding++) {
        memset(name,'n',padding); memset(raw,'x',padding); size_t n=padding;
        while (n+9<sizeof(raw)) { memcpy(name+n,"ğ€😀",9); memcpy(raw+n,"ğ€😀",9); n+=9; } name[n]=raw[n]=0;
        ui_filter_label_set(&out,name,FILTER_CONTAINS,raw);
        CHECK(out.valid && out.truncated && strlen(out.text)<sizeof(out.text) && utf8(out.text));
        CHECK(strstr(out.text,"…: ") && !strcmp(out.text+strlen(out.text)-3,"…"));
        CHECK(!out.value_count && !out.has_null);
    }
    char malformed[]={'X',(char)0xff,(char)0xc0,(char)0x80,0}; ui_filter_label_set(&out,malformed,FILTER_EQUAL,malformed); CHECK(out.valid && utf8(out.text) && strstr(out.text,"�"));
    size_t limit=8*1024*1024; char *large=dx_alloc(limit+2); memset(large,'x',limit); large[limit]=0;
    ui_filter_label_set(&out,"Large",FILTER_CONTAINS,large); CHECK(out.valid && out.truncated && utf8(out.text));
    large[limit]='x'; ui_filter_label_set(&out,"Large",FILTER_CONTAINS,large); CHECK(!out.valid); large[limit]=0;
    /* A legitimate view-filter payload can exceed the source-filter limit. */
    large[5*1024*1024]=0; char *large_key[]={large}; char *large_json=analysis_values_json(large_key,1);
    CHECK(strlen(large_json)>4*1024*1024 && strlen(large_json)<8*1024*1024);
    ui_filter_label_set(&out,"Large view",FILTER_IN,large_json); CHECK(out.valid && out.truncated && out.value_count==1 && utf8(out.text));
    Analysis source={0}; char *error=NULL;
    CHECK(!analysis_filter_set(&source,"Large source",FILTER_IN,large_json,&error)); CHECK(error && !source.filter_column);
    analysis_free(&source); free(error); free(large_json); free(large);
    char *keys[4097]; for (size_t i=0;i<4097;i++) keys[i]=i==4095 ? NULL : "x";
    char *json=analysis_values_json(keys,4096); ui_filter_label_set(&out,"Many",FILTER_IN,json); CHECK(out.valid && out.truncated && out.value_count==4096 && out.has_null && utf8(out.text)); free(json);
    json=analysis_values_json(keys,4097); ui_filter_label_set(&out,"Many",FILTER_IN,json); CHECK(!out.valid); free(json);
    char *long_text=dx_alloc(2001); memset(long_text,'x',2000);
    char *single[]={long_text}; char *list=analysis_values_json(single,1),*composite=analysis_filter_pack(FILTER_IN,list,FILTER_GREATER,long_text); CHECK(composite);
    ui_filter_label_set(&out,"Long",FILTER_COMPOSITE,composite); CHECK(out.valid && out.truncated && strstr(out.text,"… AND > ") && utf8(out.text));
    free(long_text); free(list); free(composite);
}
int main(void) {
    test_reference_and_owned_values(); test_typed_values(); test_date_hierarchy(); test_composite(); test_bounds_and_invalid();
    printf("PASS: %u filter-label checks (reference values/terms, exact numbers, typed NULL/empty/literal values, hierarchical date bounds, composite predicates, owned UTF-8 prefixes and payload limits)\n",checks);
    return 0;
}
