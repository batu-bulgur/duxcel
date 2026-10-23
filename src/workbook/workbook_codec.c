#include "workbook_codec.h"
#include <string.h>

static const WorkbookCodecTable tables[]={
        {"workbook_identity","high,low,sheet_high_water,command_revision","U U U U","",15},
        {"sheet_identity","sheet_id,stable_id","I U","",15},
        {"meta","version,active_sheet,user_scale","I U D","",12},
        {"sheets",DX_WORKBOOK_SHEET_COLUMNS ",canvas_zoom","I T I T T T T T I T T I T B U U U U U U U D D I D","source_schema,source_table,filter_column,filter_value,group_column,measure_column,sort_column",12},
        {"columns","sheet_id,column_id,name,width","I I T D","",12},
        {"cells","sheet_id,row_id,column_id,text","I U I T","",12},
        {"filters","sheet_id,position,name,op,value","I I T I T","value",12},
        {"sorts","sheet_id,position,name,descending","I I T B","",12},
        {"queries","sheet_id,object_id,query","I U T","",12},
        {"source_options","sheet_id,object_id,unique_rows","I U B","",12},
        {"table_sources","sheet_id,object_id,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending","I U I T T T T T I T T I T B","source_schema,source_table,filter_column,filter_value,group_column,measure_column,sort_column",12},
        {"table_filters","sheet_id,object_id,position,name,op,value","I U I T I T","value",12},
        {"table_sorts","sheet_id,object_id,position,name,descending","I U I T B","",12},
        {"objects","sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure","I U T U I U I I I U I","",12},
        {"pivot_fields","sheet_id,object_id,role,position,name,aggregate,filter_op,value,grain","I U I I T I I T I","value",12},
        {"marks","sheet_id,object_id,role,position,name,is_measure,aggregate,grain","I U I I T B I I","",12},
        {"preferences","name,value","T B","",12},
        {"formats","sheet_id,position,r0,r1,c0,c1,mask,flags,alignment,color,font_size,numeric_format,decimals","I I U U I I I I I I I I I","",12},
        {"object_counters","sheet_id,high_water","I U","",13},
        {"format_scopes","sheet_id,scope_id,owner,encoding,definition","I I U I X","",13},
        {"format_selectors","sheet_id,position,kind","I I I","",13},
        {"format_state","sheet_id,source_aware","I B","",14},
        {"format_guards","sheet_id,position,guard_position,scope_id","I I I I","",13},
        {"source_version_counters","sheet_id,high_water","I U","",14},
        {"source_versions","sheet_id,version,owner,source_kind,source_reference,encoding,definition","I U U I T I X","definition",14},
        {"source_bindings","sheet_id,owner,version","I U U","",14},
        {"format_records","sheet_id,position,member_position,record_id","I I I U","",14},
        {"format_fields","sheet_id,position,field_position,source_ordinal","I I I I","",14}
};
static bool package_valid(unsigned package) { return package>=12 && package<=DX_WORKSPACE_VERSION; }
size_t workbook_codec_table_count(unsigned package) {
    if (!package_valid(package)) return 0;
    size_t count=0;
    for (size_t i=0;i<sizeof(tables)/sizeof(*tables);i++) count+=tables[i].min_package<=package;
    return count;
}
const WorkbookCodecTable *workbook_codec_table_at(unsigned package,size_t index) {
    if (!package_valid(package)) return NULL;
    for (size_t i=0;i<sizeof(tables)/sizeof(*tables);i++) if (tables[i].min_package<=package) {
        if (!index) return tables+i;
        index--;
    }
    return NULL;
}
const WorkbookCodecTable *workbook_codec_table_find(const char *name) {
    if (name) for (size_t i=0;i<sizeof(tables)/sizeof(*tables);i++) if (!strcmp(tables[i].table,name)) return tables+i;
    return NULL;
}
bool workbook_codec_nullable(const WorkbookCodecTable *table,const char *column,size_t bytes) {
    if (!table || !column || !bytes) return false;
    for (const char *p=table->nullable_values;*p;) {
        size_t n=strcspn(p,","); if (n==bytes && !memcmp(p,column,n)) return true;
        p+=n; if (*p) p++;
    }
    return false;
}
bool workbook_codec_reserved_before(const char *name,unsigned version) {
    const WorkbookCodecTable *table=workbook_codec_table_find(name);
    return version>=1 && version<=DX_WORKSPACE_VERSION && table && table->min_package>=13 && table->min_package>version;
}
/* Exact native representation predicate; legitimate retained source bindings
   also have their durable ledger entry. This does not issue a source version. */
static bool source_version_state(const Worksheet *sheet) {
    const SourceVersions *versions=&sheet->source_versions; const ObjectStore *objects=&sheet->objects;
    bool bound=sheet->analysis.source_version || versions->high_water || versions->count || versions->capacity || versions->bytes || versions->items;
    if (sheet->undo_count>DX_HISTORY || sheet->redo_count>DX_HISTORY || objects->undo_count>DX_HISTORY || objects->redo_count>DX_HISTORY) return true;
    for (size_t i=0;i<sheet->undo_count;i++) bound|=sheet->undo[i].source_version!=0;
    for (size_t i=0;i<sheet->redo_count;i++) bound|=sheet->redo[i].source_version!=0;
    for (size_t i=0;i<objects->count;i++) bound|=objects->items[i].source.source_version!=0;
    for (size_t history=0;history<2;history++) {
        const ObjectSnapshot *stack=history ? objects->redo : objects->undo; size_t count=history ? objects->redo_count : objects->undo_count;
        for (size_t i=0;i<count;i++) {
            if (stack[i].count>DX_MAX_OBJECTS || (stack[i].count && !stack[i].items)) return true;
            for (size_t j=0;j<stack[i].count;j++) bound|=stack[i].items[j].source.source_version!=0;
        }
    }
    return bound || sheet->formats.scope_count || sheet->formats.source_aware;
}
unsigned workbook_codec_content_version(const Workbook *book) {
    if (!book || !book->count || book->count>DX_MAX_SHEETS || !book->sheets) return 0;
    for (size_t i=0;i<book->count;i++) {
        const ObjectStore *objects=&book->sheets[i].objects;
        if (objects->count>DX_MAX_OBJECTS || (objects->count && !objects->items)) return 0;
    }
    unsigned minimum=12;
    for (size_t i=0;i<book->count;i++) {
        const Worksheet *sheet=book->sheets+i; const ObjectStore *objects=&sheet->objects;
        if (source_version_state(sheet)) return 14;
        uint64_t live=0;
        for (size_t j=0;j<objects->count;j++) if (objects->items[j].id>live) live=objects->items[j].id;
        if (objects->next_id>live) minimum=13;
    }
    return minimum;
}
unsigned workbook_codec_min_write_version(const Workbook *book) {
    return workbook_codec_content_version(book) ? 15 : 0;
}
bool workbook_codec_select_write_version(unsigned minimum,int existing,unsigned *out) {
    if (!out || minimum<12 || minimum>DX_WORKSPACE_VERSION || existing<0 || existing>DX_WORKSPACE_VERSION) return false;
    *out=minimum>(unsigned)existing ? minimum : (unsigned)existing; return true;
}
