#include "workbook_codec.h"
#include <string.h>

static const WorkbookCodecTable tables[]={
        {"workbook_identity","high,low,sheet_high_water,command_revision","U U U U",""},
        {"sheet_identity","sheet_id,stable_id","I U",""},
        {"meta","version,active_sheet,user_scale","I U D",""},
        {"sheets",DX_WORKBOOK_SHEET_COLUMNS ",canvas_zoom","I T I T T T T T I T T I T B U U U U U U U D D I D","source_schema,source_table,filter_column,filter_value,group_column,measure_column,sort_column"},
        {"columns","sheet_id,column_id,name,width","I I T D",""},
        {"cells","sheet_id,row_id,column_id,text","I U I T",""},
        {"filters","sheet_id,position,name,op,value","I I T I T","value"},
        {"sorts","sheet_id,position,name,descending","I I T B",""},
        {"queries","sheet_id,object_id,query","I U T",""},
        {"source_options","sheet_id,object_id,unique_rows","I U B",""},
        {"table_sources","sheet_id,object_id,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending","I U I T T T T T I T T I T B","source_schema,source_table,filter_column,filter_value,group_column,measure_column,sort_column"},
        {"table_filters","sheet_id,object_id,position,name,op,value","I U I T I T","value"},
        {"table_sorts","sheet_id,object_id,position,name,descending","I U I T B",""},
        {"objects","sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure","I U T U I U I I I U I",""},
        {"pivot_fields","sheet_id,object_id,role,position,name,aggregate,filter_op,value,grain","I U I I T I I T I","value"},
        {"marks","sheet_id,object_id,role,position,name,is_measure,aggregate,grain","I U I I T B I I",""},
        {"preferences","name,value","T B",""},
        {"formats","sheet_id,position,r0,r1,c0,c1,mask,flags,alignment,color,font_size,numeric_format,decimals","I I U U I I I I I I I I I",""},
        {"object_counters","sheet_id,high_water","I U",""},
        {"format_scopes","sheet_id,scope_id,owner,encoding,definition","I I U I X",""},
        {"format_selectors","sheet_id,position,kind","I I I",""},
        {"format_state","sheet_id,source_aware","I B",""},
        {"format_guards","sheet_id,position,guard_position,scope_id","I I I I",""},
        {"source_version_counters","sheet_id,high_water","I U",""},
        {"source_versions","sheet_id,version,owner,source_kind,source_reference,encoding,definition","I U U I T I X","definition"},
        {"source_bindings","sheet_id,owner,version","I U U",""},
        {"format_records","sheet_id,position,member_position,record_id","I I I U",""},
        {"format_fields","sheet_id,position,field_position,source_ordinal","I I I I",""}
};
size_t workbook_codec_table_count(unsigned package) { return package==DX_WORKSPACE_VERSION ? sizeof(tables)/sizeof(*tables) : 0; }
const WorkbookCodecTable *workbook_codec_table_at(unsigned package,size_t index) {
    return index<workbook_codec_table_count(package) ? tables+index : NULL;
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
unsigned workbook_codec_min_write_version(const Workbook *book) {
    if (!book || !book->count || book->count>DX_MAX_SHEETS || !book->sheets) return 0;
    for (size_t i=0;i<book->count;i++) {
        const ObjectStore *objects=&book->sheets[i].objects;
        if (objects->count>DX_MAX_OBJECTS || (objects->count && !objects->items)) return 0;
    }
    return DX_WORKSPACE_VERSION;
}
bool workbook_codec_select_write_version(unsigned minimum,int existing,unsigned *out) {
    if (!out || minimum!=DX_WORKSPACE_VERSION || (existing && existing!=DX_WORKSPACE_VERSION)) return false;
    *out=DX_WORKSPACE_VERSION; return true;
}
