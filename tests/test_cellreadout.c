#include "ui/cellreadout.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL readout:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static bool utf8(const char *text) {
    const unsigned char *p=(const unsigned char *)text;
    while (*p) {
        unsigned count=*p<128 ? 0 : *p>=0xc2 && *p<=0xdf ? 1 : *p>=0xe0 && *p<=0xef ? 2 : *p>=0xf0 && *p<=0xf4 ? 3 : 9;
        if (count>3) return false;
        p++; for (unsigned i=0;i<count;i++,p++) if ((*p&0xc0)!=0x80) return false;
    }
    return true;
}
int main(void) {
    UICellReadout out={0}; char raw[4096]="-123456789012345678901234567890.12",type[100]="DECIMAL(38,2)";
    Column column={.type_name=type,.type=DUCKDB_TYPE_DECIMAL,.numeric=true}; Cell cell={.text=raw,.number=-1.2345678901234568e29};
    ui_cell_readout_set(&out,&column,&cell);
    CHECK(out.valid && !out.is_null && out.numeric && !out.truncated && out.type==DUCKDB_TYPE_DECIMAL);
    CHECK(!strcmp(out.text,raw) && !strcmp(out.type_name,type) && out.number==cell.number);
    raw[0]='X'; type[0]='X'; CHECK(out.text[0]=='-' && out.type_name[0]=='D');
    cell=(Cell){.text="NULL",.is_null=true}; ui_cell_readout_set(&out,&column,&cell); CHECK(out.valid && out.is_null && !*out.text);
    column=(Column){.type_name="VARCHAR",.type=DUCKDB_TYPE_VARCHAR};
    cell=(Cell){.text="NULL"}; ui_cell_readout_set(&out,&column,&cell); CHECK(out.valid && !out.is_null && !strcmp(out.text,"NULL") && !out.numeric);
    cell.text=""; ui_cell_readout_set(&out,&column,&cell); CHECK(out.valid && !out.is_null && !*out.text);
    cell.text="\"\""; ui_cell_readout_set(&out,&column,&cell); CHECK(out.valid && !out.is_null && !strcmp(out.text,"\"\""));
    cell.text=out.text; column.type_name=out.type_name; ui_cell_readout_set(&out,&column,&cell); CHECK(!strcmp(out.text,"\"\"") && !strcmp(out.type_name,"VARCHAR"));
    column.type_name="VARCHAR";
    for (size_t padding=0;padding<40;padding++) {
        memset(raw,'x',padding); size_t n=padding;
        while (n+9<sizeof(raw)) { memcpy(raw+n,"ğ€😀",9); n+=9; } raw[n]=0;
        cell.text=raw; ui_cell_readout_set(&out,&column,&cell);
        CHECK(out.valid && out.truncated && !out.is_null && strlen(out.text)<sizeof(out.text));
        CHECK(utf8(out.text) && !strcmp(out.text+strlen(out.text)-3,"…"));
        CHECK(!memcmp(out.text,raw,strlen(out.text)-3));
    }
    memset(type,'T',sizeof(type)-1); type[sizeof(type)-1]=0; column.type_name=type; cell.text="value"; ui_cell_readout_set(&out,&column,&cell);
    CHECK(strlen(out.type_name)<sizeof(out.type_name) && !strcmp(out.type_name+strlen(out.type_name)-3,"…"));
    column=(Column){.type_name="DOUBLE",.type=DUCKDB_TYPE_DOUBLE,.numeric=true}; cell=(Cell){.text="NaN",.number=NAN}; ui_cell_readout_set(&out,&column,&cell);
    CHECK(out.valid && out.numeric && out.type==DUCKDB_TYPE_DOUBLE && isnan(out.number) && !strcmp(out.text,"NaN"));
    cell.text=NULL; ui_cell_readout_set(&out,&column,&cell); CHECK(!out.valid && !*out.text && !*out.type_name);
    ui_cell_readout_set(&out,NULL,&cell); CHECK(!out.valid); ui_cell_readout_set(&out,&column,NULL); CHECK(!out.valid);
    printf("PASS: %u active-value snapshot checks (owned prefixes, exact decimals, UTF-8 bounds, NULL/empty/literal distinctions, non-finite values and alias safety)\n",checks);
    return 0;
}
