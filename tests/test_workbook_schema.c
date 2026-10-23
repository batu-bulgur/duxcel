#include "workbook/schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL schema:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static WorkbookSchemaText text(const char *s) { return (WorkbookSchemaText){s,s ? strlen(s) : 0}; }
static const char *type(char code) {
    switch (code) { case 'I': return "INTEGER"; case 'U': return "UBIGINT"; case 'D': return "DOUBLE"; case 'B': return "BOOLEAN"; case 'X': return "BLOB"; default: return "VARCHAR"; }
}
static void complete_package(unsigned package,bool reversed,bool upper) {
    WorkbookSchemaProof p={0}; CHECK(workbook_schema_begin(&p,package) && !workbook_schema_complete(&p)); size_t rows=0;
    for (size_t n=0;n<p.tables;n++) {
        const WorkbookCodecTable *t=workbook_codec_table_at(package,reversed ? p.tables-1-n : n);
        char names[64][64]; char types[64]; size_t count=0,at=0;
        for (const char *c=t->types;*c;c++) if (*c!=' ') types[at++]=*c;
        for (const char *c=t->columns;*c;) {
            size_t bytes=strcspn(c,","); CHECK(bytes<sizeof(names[0]) && count<64);
            memcpy(names[count],c,bytes); names[count][bytes]=0;
            if (upper) for (size_t i=0;i<bytes;i++) if (names[count][i]>='a' && names[count][i]<='z') names[count][i]+='A'-'a';
            count++; c+=bytes; if (*c) c++;
        }
        CHECK(count==at);
        for (size_t i=0;i<count;i++) {
            size_t field=reversed ? count-1-i : i;
            CHECK(workbook_schema_offer(&p,text(t->table),text("BASE TABLE"),text(names[field]),text(type(types[field])))); rows++;
        }
        memset(names,0,sizeof(names)); /* Offered spans are never retained. */
    }
    CHECK(p.rows==rows && rows==p.expected_rows && workbook_schema_complete(&p));
    CHECK(!workbook_schema_offer(&p,text("meta"),text("BASE TABLE"),text("version"),text("INTEGER")) && !workbook_schema_complete(&p));
}
static bool offer(WorkbookSchemaProof *p,const char *table,const char *kind,const char *column,const char *value) { return workbook_schema_offer(p,text(table),text(kind),text(column),text(value)); }
int main(void) {
    for (unsigned version=12;version<=15;version++) { complete_package(version,false,false); complete_package(version,true,true); }
    WorkbookSchemaProof p,unchanged; memset(&p,0x5a,sizeof(p)); unchanged=p;
    CHECK(!workbook_schema_begin(&p,0) && !memcmp(&p,&unchanged,sizeof(p))); CHECK(!workbook_schema_begin(&p,16) && !memcmp(&p,&unchanged,sizeof(p)));
    CHECK(!workbook_schema_begin(NULL,15) && !workbook_schema_complete(NULL));
    const char *bad[][4]={{"META","BASE TABLE","version","INTEGER"},{"custom_note","BASE TABLE","version","INTEGER"},
        {"meta","VIEW","version","INTEGER"},{"meta","BASE TABLE","unknown","INTEGER"},{"meta","BASE TABLE","version","UBIGINT"},
        {"meta","BASE TABLE","version","VARCHAR"},{"meta","BASE TABLE","version",NULL},{"meta",NULL,"version","INTEGER"},
        {"meta","BASE TABLE",NULL,"INTEGER"},{NULL,"BASE TABLE","version","INTEGER"},{"meta","BASE TABLE","version","INTEGER[]"},
        {"meta","BASE TABLE","version","ENUM('INTEGER')"},{"meta","BASE TABLE","version","INTEGER "}};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        CHECK(workbook_schema_begin(&p,15)); CHECK(!offer(&p,bad[i][0],bad[i][1],bad[i][2],bad[i][3]) && p.error && !workbook_schema_complete(&p));
        CHECK(!offer(&p,"meta","BASE TABLE","version","INTEGER"));
    }
    CHECK(workbook_schema_begin(&p,15) && offer(&p,"meta","BASE TABLE","version","INTEGER"));
    CHECK(!offer(&p,"meta","BASE TABLE","VERSION","INTEGER") && p.error==WORKBOOK_SCHEMA_STRUCTURE);
    CHECK(workbook_schema_begin(&p,15)); const char nul[]="version\0hidden";
    CHECK(!workbook_schema_offer(&p,text("meta"),text("BASE TABLE"),(WorkbookSchemaText){nul,sizeof(nul)-1},text("INTEGER")));
    CHECK(workbook_schema_begin(&p,15));
    CHECK(!workbook_schema_offer(&p,text("meta"),text("BASE TABLE"),(WorkbookSchemaText){"version",SIZE_MAX},text("INTEGER")));
    CHECK(workbook_schema_begin(&p,15) && !offer(&p,"meta","BASE TABLE","version","DOUBLE") && p.error==WORKBOOK_SCHEMA_COLUMN_TYPES);
    CHECK(workbook_schema_begin(&p,14) && !offer(&p,"workbook_identity","BASE TABLE","high","UBIGINT"));
    CHECK(workbook_schema_begin(&p,15) && offer(&p,"cells","BASE TABLE","text","JSON"));
    CHECK(workbook_schema_begin(&p,15) && !offer(&p,"meta","BASE TABLE","version","JSON") && p.error==WORKBOOK_SCHEMA_COLUMN_TYPES);
    CHECK(workbook_schema_begin(&p,15) && !workbook_schema_complete(&p)); p=(WorkbookSchemaProof){0}; CHECK(!workbook_schema_complete(&p));
    printf("PASS: %u bounded workbook schema proof checks\n",checks); return 0;
}
