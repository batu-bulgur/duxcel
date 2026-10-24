#include "schema.h"
#include <string.h>
static const char *type_name(char code) {
    switch (code) {
    case 'I': return "INTEGER"; case 'U': return "UBIGINT"; case 'D': return "DOUBLE";
    case 'B': return "BOOLEAN"; case 'T': return "VARCHAR"; case 'X': return "BLOB";
    default: return NULL;
    }
}
static bool exact(WorkbookSchemaText text,const char *expected) { return text.data && text.bytes==strlen(expected) && !memcmp(text.data,expected,text.bytes); }
static unsigned char fold(unsigned char c) { return c>='A' && c<='Z' ? (unsigned char)(c+('a'-'A')) : c; }
static bool column_equal(WorkbookSchemaText text,const char *expected,size_t bytes) {
    if (!text.data || text.bytes!=bytes) return false;
    const unsigned char *data=text.data;
    for (size_t i=0;i<bytes;i++) if (fold(data[i])!=(unsigned char)expected[i]) return false;
    return true;
}
static bool reject(WorkbookSchemaProof *p,WorkbookSchemaError error) { if (p && !p->error) p->error=error; return false; }
bool workbook_schema_begin(WorkbookSchemaProof *out,unsigned package) {
    size_t count=workbook_codec_table_count(package);
    if (!out || !count || count>DX_WORKBOOK_SCHEMA_TABLES) return false;
    WorkbookSchemaProof next={.package=package,.tables=count};
    for (size_t i=0;i<count;i++) {
        const WorkbookCodecTable *table=workbook_codec_table_at(package,i); size_t columns=1,types=0;
        for (const char *p=table->columns;*p;p++) if (*p==',') columns++;
        for (const char *p=table->types;*p;p++) if (*p!=' ') { if (!type_name(*p)) return false; types++; }
        if (!columns || columns>64 || types!=columns) return false;
        next.expected[i]=columns==64 ? UINT64_MAX : (UINT64_C(1)<<columns)-1; next.expected_rows+=columns;
    }
    *out=next; return true;
}
bool workbook_schema_offer(WorkbookSchemaProof *p,WorkbookSchemaText table,WorkbookSchemaText kind,WorkbookSchemaText column,WorkbookSchemaText type) {
    if (!p || p->error) return false;
    if (!p->tables || p->tables>DX_WORKBOOK_SCHEMA_TABLES || p->tables!=workbook_codec_table_count(p->package) || p->rows>=p->expected_rows || !exact(kind,"BASE TABLE")) return reject(p,WORKBOOK_SCHEMA_STRUCTURE);
    size_t index=0;
    while (index<p->tables && !exact(table,workbook_codec_table_at(p->package,index)->table)) index++;
    if (index==p->tables) return reject(p,WORKBOOK_SCHEMA_STRUCTURE);
    const WorkbookCodecTable *descriptor=workbook_codec_table_at(p->package,index);
    size_t field=0; const char *name=descriptor->columns,*types=descriptor->types;
    for (;;) {
        size_t bytes=strcspn(name,",");
        if (column_equal(column,name,bytes)) break;
        if (!name[bytes]) return reject(p,WORKBOOK_SCHEMA_STRUCTURE);
        name+=bytes+1; field++;
    }
    if (field>=64 || (p->seen[index]&(UINT64_C(1)<<field))) return reject(p,WORKBOOK_SCHEMA_STRUCTURE);
    for (size_t i=0;i<field;i++) { if (!*types) return reject(p,WORKBOOK_SCHEMA_STRUCTURE); types++; while (*types==' ') types++; }
    const char *expected=type_name(*types);
    /* DuckDB exposes JSON as VARCHAR through the C type API used by the
       C metadata API. JSON remains the same physical string type, independently
       of file-version compatibility. */
    if (!expected || (!exact(type,expected) && !(*types=='T' && exact(type,"JSON")))) return reject(p,WORKBOOK_SCHEMA_COLUMN_TYPES);
    p->seen[index]|=UINT64_C(1)<<field; p->rows++; return true;
}
bool workbook_schema_complete(const WorkbookSchemaProof *p) {
    if (!p || p->error || !p->tables || p->tables>DX_WORKBOOK_SCHEMA_TABLES || p->tables!=workbook_codec_table_count(p->package) || p->rows!=p->expected_rows) return false;
    for (size_t i=0;i<p->tables;i++) if (p->seen[i]!=p->expected[i]) return false;
    return true;
}
