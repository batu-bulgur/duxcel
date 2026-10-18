#include "workbook_codec_read.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *name,*reference; Analysis analysis; SheetView view; size_t object_offset,object_count,column_capacity; } HeaderSheet;
typedef struct { Analysis analysis; char *reference; } HeaderTableSource;
typedef struct { size_t sheet; WorkbookCodecObjectView value; char *query; bool unique_rows; HeaderTableSource *source; PivotDefinition *pivot; MarkDefinition *marks; } HeaderObject;
typedef struct { HeaderObject *owner; void *definition; size_t original[DX_MARK_ROLES]; } DefinitionChange;
struct WorkbookCodecHeader {
    WorkbookCodecKey key;
    unsigned version;
    size_t count,active,bytes;
    uint64_t hidden;
    float user_scale;
    bool sorts_ready,preferences_ready,reduced_motion,objects_ready,queries_ready,source_options_ready,table_sources_ready,object_table_present,pivot_marks_ready,filters_ready,columns_ready;
    PivotDefinition empty_pivot;
    MarkDefinition empty_marks;
    HeaderObject *objects;
    size_t object_count;
    HeaderSheet sheets[DX_MAX_SHEETS];
};
struct WorkbookCodecRead {
    WorkbookCodecKey key;
    WorkbookCodecHeader *header;
    WorkbookCodecRequest request;
    WorkbookCodecResolve resolve;
    void *context;
    size_t limit,bytes;
    unsigned version;
    WorkbookCodecSheetsLayout layout;
    uint64_t preference_seen;
    size_t query_sheet;
    uint64_t query_owner;
    uint64_t source_options_seen[DX_MAX_SHEETS];
    size_t source_options_sheet;
    uint64_t source_options_owner;
    size_t table_next,table_sheet,table_raw[DX_MAX_SHEETS];
    uint64_t table_owner;
    size_t definition_sheet,definition_role,definition_raw[2][DX_MAX_SHEETS];
    uint64_t definition_owner;
    bool preference_motion_seen;
    WorkbookCodecReadOptions options;
    size_t filter_sheet,filter_raw[DX_MAX_SHEETS];
    size_t columns_sheet;
};
static const char meta_sql[]="SELECT version,active_sheet,user_scale FROM _duxcel_workspace.meta LIMIT 2";
static const char sheets25_sql[]="SELECT " DX_WORKBOOK_SHEET_COLUMNS ",canvas_zoom AS canvas_zoom FROM _duxcel_workspace.sheets ORDER BY id LIMIT 65";
static const char sheets24_sql[]="SELECT " DX_WORKBOOK_SHEET_COLUMNS ",1::DOUBLE AS canvas_zoom FROM _duxcel_workspace.sheets ORDER BY id LIMIT 65";
static const char columns_sql[]="SELECT sheet_id,column_id,name,width FROM _duxcel_workspace.columns ORDER BY sheet_id,column_id LIMIT 1048577";
static const char filters_sql[]="SELECT sheet_id,position,name,op,value FROM _duxcel_workspace.filters ORDER BY sheet_id,position LIMIT 961";
static const char sorts_sql[]="SELECT sheet_id,position,name,descending FROM _duxcel_workspace.sorts ORDER BY sheet_id,position LIMIT 449";
static const char preferences_sql[]="SELECT name,value FROM _duxcel_workspace.preferences WHERE name='reduce_motion' OR starts_with(name,'sheet_hidden_') ORDER BY name LIMIT 66";
static const char preferences14_sql[]="SELECT name,value FROM _duxcel_workspace.preferences WHERE name='reduce_motion' OR starts_with(name,'sheet_hidden_') OR name IS NULL OR value IS NULL OR contains(name,chr(0)) ORDER BY name LIMIT 66";
static const char objects_sql[]="SELECT sheet_id,id,name,row_id,column_id,height,width,kind,mark,source_id,chart_measure FROM _duxcel_workspace.objects ORDER BY sheet_id,id LIMIT 2049";
static const char queries_sql[]="SELECT sheet_id,object_id,CASE WHEN query IS NULL OR octet_length(encode(query))>65536 OR length(trim(query))=0 OR contains(query,chr(0)) THEN NULL::VARCHAR ELSE query END AS query FROM _duxcel_workspace.queries ORDER BY sheet_id,object_id LIMIT 2113";
static const char source_options_sql[]="SELECT sheet_id,object_id,unique_rows FROM _duxcel_workspace.source_options ORDER BY sheet_id,object_id LIMIT 2113";
static const char table_sources_sql[]="SELECT sheet_id,object_id,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending FROM _duxcel_workspace.table_sources ORDER BY sheet_id,object_id LIMIT 2049";
static const char table_filters_sql[]="SELECT sheet_id,object_id,position,name,op,value FROM _duxcel_workspace.table_filters ORDER BY sheet_id,object_id,position LIMIT 30721";
static const char table_sorts_sql[]="SELECT sheet_id,object_id,position,name,descending FROM _duxcel_workspace.table_sorts ORDER BY sheet_id,object_id,position LIMIT 14337";
static const char pivot_fields_sql[]="SELECT sheet_id,object_id,role,position,name,aggregate,filter_op,value,grain FROM _duxcel_workspace.pivot_fields ORDER BY sheet_id,object_id,role,position LIMIT 65537";
static const char marks_sql[]="SELECT sheet_id,object_id,role,position,name,is_measure,aggregate,grain FROM _duxcel_workspace.marks ORDER BY sheet_id,object_id,role,position LIMIT 98305";
#define COLUMN_LIMIT 16384u
#define COLUMN_ROWS (DX_MAX_SHEETS*COLUMN_LIMIT)
#define FILTER_ROWS (DX_MAX_SHEETS*(DX_ANALYSIS_FILTERS-1u))
#define SORT_ROWS (DX_MAX_SHEETS*(DX_ANALYSIS_SORTS-1u))
#define OBJECT_ROWS (DX_MAX_SHEETS*DX_MAX_OBJECTS)
#define QUERY_ROWS (DX_MAX_SHEETS*(DX_MAX_OBJECTS+1u))
#define TABLE_FILTER_ROWS (OBJECT_ROWS*(DX_ANALYSIS_FILTERS-1u))
#define TABLE_SORT_ROWS (OBJECT_ROWS*(DX_ANALYSIS_SORTS-1u))
#define TABLE_RAW_BYTES (8u*1024u*1024u)
#define TABLE_FILTER_BYTES (4u*1024u*1024u)
#define PIVOT_ROWS_LIMIT (OBJECT_ROWS*4u*DX_PIVOT_FIELDS)
#define MARK_ROWS_LIMIT (OBJECT_ROWS*DX_MARK_ROLES*DX_MARK_FIELDS)
static bool request_family(WorkbookCodecFamily family) {
    return family==WORKBOOK_CODEC_META || family==WORKBOOK_CODEC_SHEETS || family==WORKBOOK_CODEC_SORTS || family==WORKBOOK_CODEC_PREFERENCES || family==WORKBOOK_CODEC_OBJECTS || family==WORKBOOK_CODEC_QUERIES || family==WORKBOOK_CODEC_SOURCE_OPTIONS || family==WORKBOOK_CODEC_TABLE_SOURCES || family==WORKBOOK_CODEC_TABLE_FILTERS || family==WORKBOOK_CODEC_TABLE_SORTS || family==WORKBOOK_CODEC_PIVOT_FIELDS || family==WORKBOOK_CODEC_MARKS || family==WORKBOOK_CODEC_FILTERS || family==WORKBOOK_CODEC_COLUMNS;
}
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static bool key_valid(WorkbookCodecKey key) { return key.document_epoch && key.operation_ticket && key.snapshot_generation; }
static bool same_key(WorkbookCodecKey a,WorkbookCodecKey b) {
    return a.document_epoch==b.document_epoch && a.operation_ticket==b.operation_ticket && a.snapshot_generation==b.snapshot_generation;
}
static bool room(size_t owned,size_t retained,size_t extra,size_t limit) {
    return owned<=limit && retained<=limit-owned && extra<=limit-owned-retained;
}
static bool utf8(const char *text,size_t bytes) {
    for (size_t i=0;i<bytes;) {
        unsigned char ch=(unsigned char)text[i++]; if (!ch) return false; if (ch<0x80) continue;
        unsigned count; uint32_t value,minimum;
        if (ch>=0xc2 && ch<=0xdf) { count=1; value=ch&31; minimum=0x80; }
        else if (ch>=0xe0 && ch<=0xef) { count=2; value=ch&15; minimum=0x800; }
        else if (ch>=0xf0 && ch<=0xf4) { count=3; value=ch&7; minimum=0x10000; }
        else return false;
        if (count>bytes-i) return false;
        while (count--) { unsigned char next=(unsigned char)text[i++]; if ((next&0xc0)!=0x80) return false; value=(value<<6)|(next&63); }
        if (value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return false;
    }
    return true;
}
static WorkbookCodecAtomKind kind(char type) {
    return type=='I' ? WORKBOOK_CODEC_I32 : type=='U' ? WORKBOOK_CODEC_U64 : type=='D' ? WORKBOOK_CODEC_DOUBLE : type=='B' ? WORKBOOK_CODEC_BOOLEAN : WORKBOOK_CODEC_TEXT;
}
static bool shape(const WorkbookCodecBatch *batch,const WorkbookCodecTable *table,bool full_primary_filters,char **error) {
    if (!batch || batch->row_count>DX_WORKBOOK_CODEC_READ_ROWS || batch->column_count>DX_WORKBOOK_CODEC_COLUMNS || !batch->columns ||
        (batch->row_count && !batch->cells) || (!batch->row_count && !batch->end)) return fail(error,"Invalid bounded metadata batch.");
    size_t column=0; const char *types=table->types;
    for (const char *p=table->columns;*p;column++) {
        if (column>=batch->column_count) return fail(error,"Metadata columns do not match the named projection.");
        size_t n=strcspn(p,","); while (*types==' ') types++;
        const WorkbookCodecColumn *actual=&batch->columns[column];
        size_t actual_bytes=0; if (actual->name) while (actual_bytes<=n && actual->name[actual_bytes]) actual_bytes++;
        if (!actual->name || actual_bytes!=n || memcmp(actual->name,p,n) || actual->kind!=kind(*types))
            return fail(error,"Metadata names or SQL types do not match the named projection.");
        for (size_t row=0;row<batch->row_count;row++) {
            const WorkbookCodecAtom *atom=&batch->cells[row*batch->column_count+column];
            if (atom->kind==WORKBOOK_CODEC_NULL) {
                if (!workbook_codec_nullable(table,p,n)) return fail(error,"Required metadata value is SQL NULL.");
            } else if (atom->kind!=actual->kind) return fail(error,"Metadata atom does not match its exact SQL type.");
            else if (atom->kind==WORKBOOK_CODEC_TEXT) {
                size_t maximum=(!strcmp(table->table,"sheets") && column==1) || ((!strcmp(table->table,"sorts") || !strcmp(table->table,"objects")) && column==2) ? 4096u : DX_WORKBOOK_CODEC_READ_TEXT;
                if (full_primary_filters && !strcmp(table->table,"sheets") && (column==6 || column==7)) maximum=column==6 ? 4096u : TABLE_FILTER_BYTES;
                else if (!strcmp(table->table,"filters")) maximum=column==2 ? 4096u : TABLE_FILTER_BYTES;
                else if (!strcmp(table->table,"table_sources")) maximum=column==7 ? TABLE_FILTER_BYTES : column==3 ? DX_WORKBOOK_CODEC_READ_TEXT : 4096u;
                else if (!strcmp(table->table,"table_filters")) maximum=column==5 ? TABLE_FILTER_BYTES : 4096u;
                else if (!strcmp(table->table,"table_sorts") || !strcmp(table->table,"marks")) maximum=4096u;
                else if (!strcmp(table->table,"pivot_fields")) maximum=column==7 ? TABLE_RAW_BYTES : 4096u;
                else if (!strcmp(table->table,"columns")) maximum=SIZE_MAX-1u;
                /* Uncapped column names are scanned only after the complete
                   prospective batch passes its owned-budget admission. */
                if (atom->value.text.bytes>maximum || (!atom->value.text.data && atom->value.text.bytes) ||
                    (strcmp(table->table,"columns") && !utf8(atom->value.text.data,atom->value.text.bytes))) return fail(error,"Metadata text is oversized, contains NUL or is not valid UTF-8.");
            }
        }
        types++; p+=n; if (*p) p++;
    }
    return column==batch->column_count || fail(error,"Metadata has trailing projected columns.");
}
static void sheet_free(HeaderSheet *sheet) {
    free(sheet->name); free(sheet->reference); analysis_free(&sheet->analysis);
    for (size_t i=0;i<sheet->view.column_count;i++) free(sheet->view.columns[i]);
    free(sheet->view.columns); free(sheet->view.widths); *sheet=(HeaderSheet){0};
}
static void table_source_free(HeaderTableSource *source) {
    if (source) { analysis_free(&source->analysis); free(source->reference); free(source); }
}
void workbook_codec_header_free(WorkbookCodecHeader *header) {
    if (!header) return;
    for (size_t i=0;i<header->count;i++) sheet_free(header->sheets+i);
    for (size_t i=0;i<header->object_count;i++) { free((char *)header->objects[i].value.name); free(header->objects[i].query); table_source_free(header->objects[i].source);
        if (header->objects[i].pivot) { pivot_free(header->objects[i].pivot); free(header->objects[i].pivot); }
        if (header->objects[i].marks) { marks_free(header->objects[i].marks); free(header->objects[i].marks); } }
    free(header->objects);
    free(header);
}
void workbook_codec_read_free(WorkbookCodecRead *read) {
    if (read) { workbook_codec_header_free(read->header); free(read); }
}
bool workbook_codec_read_begin_with_options(WorkbookCodecRead **out,WorkbookCodecKey key,unsigned version,
    WorkbookCodecSheetsLayout layout,WorkbookCodecReadOptions options,size_t limit,WorkbookCodecResolve resolve,void *context,char **error) {
    if (!out || !key_valid(key) || version<12 || version>14 || (layout!=WORKBOOK_CODEC_SHEETS_24 && layout!=WORKBOOK_CODEC_SHEETS_25) ||
        (version==14 && layout!=WORKBOOK_CODEC_SHEETS_25)) return fail(error,"Invalid metadata operation key, version or sheets layout.");
    if (!room(sizeof(WorkbookCodecRead),0,sizeof(WorkbookCodecHeader),limit)) return fail(error,"Metadata cursor exceeds its owned-buffer budget.");
    WorkbookCodecRead *next=dx_alloc(sizeof(*next)); next->header=dx_alloc(sizeof(*next->header));
    next->key=key; next->version=version; next->layout=layout; next->limit=limit; next->resolve=resolve; next->context=context; next->options=options;
    next->bytes=sizeof(*next)+sizeof(*next->header); next->header->bytes=sizeof(*next->header); next->header->key=key; next->header->version=version;
    next->request=(WorkbookCodecRequest){WORKBOOK_CODEC_META,meta_sql,0,3,3,2};
    workbook_codec_read_free(*out); *out=next; return true;
}
bool workbook_codec_read_begin(WorkbookCodecRead **out,WorkbookCodecKey key,unsigned version,
    WorkbookCodecSheetsLayout layout,size_t limit,WorkbookCodecResolve resolve,void *context,char **error) {
    return workbook_codec_read_begin_with_options(out,key,version,layout,(WorkbookCodecReadOptions){0},limit,resolve,context,error);
}
const WorkbookCodecRequest *workbook_codec_read_request(const WorkbookCodecRead *read) {
    return read && request_family(read->request.family) ? &read->request : NULL;
}
bool workbook_codec_read_add_columns(WorkbookCodecRead *read,bool present,char **error) {
    if (!read || !read->header || read->request.family!=WORKBOOK_CODEC_HEADER_READY || read->header->columns_ready)
        return fail(error,"Column metadata requires a complete unclaimed header and may be armed once.");
    if (!present) return fail(error,"The workbook requires column metadata.");
    read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_COLUMNS,columns_sql,0,4,4,COLUMN_ROWS+1u}; return true;
}
bool workbook_codec_read_add_sorts(WorkbookCodecRead *read,bool present,char **error) {
    if (!read || !read->header || read->request.family!=WORKBOOK_CODEC_HEADER_READY || read->header->sorts_ready)
        return fail(error,"Sort metadata requires a complete unclaimed header and may be armed once.");
    if (!present && read->version==14) return fail(error,"The v14 workbook requires sort metadata.");
    for (size_t i=0;i<read->header->count;i++) if (!analysis_sorts_valid(&read->header->sheets[i].analysis,error)) return false;
    if (present) read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_SORTS,sorts_sql,0,4,4,SORT_ROWS+1u};
    else read->header->sorts_ready=true;
    return true;
}
bool workbook_codec_read_add_preferences(WorkbookCodecRead *read,bool present,char **error) {
    if (!read || !read->header || read->request.family!=WORKBOOK_CODEC_HEADER_READY || read->header->preferences_ready)
        return fail(error,"Preferences require a complete unclaimed header and may be armed once.");
    if (!present && read->version==14) return fail(error,"The v14 workbook requires preferences.");
    if (present) read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_PREFERENCES,read->version==14 ? preferences14_sql : preferences_sql,0,2,2,DX_MAX_SHEETS+2u};
    else read->header->preferences_ready=true;
    return true;
}
bool workbook_codec_read_add_objects(WorkbookCodecRead *read,bool present,char **error) {
    if (!read || !read->header || read->request.family!=WORKBOOK_CODEC_HEADER_READY || read->header->objects_ready)
        return fail(error,"Object descriptors require a complete unclaimed header and may be armed once.");
    if (!present && read->version>=13) return fail(error,read->version==13 ? "The v13 workbook requires object descriptors." : "The v14 workbook requires object descriptors.");
    read->header->object_table_present=present;
    if (present) read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_OBJECTS,objects_sql,0,11,11,OBJECT_ROWS+1u};
    else read->header->objects_ready=true;
    return true;
}
bool workbook_codec_read_add_queries(WorkbookCodecRead *read,bool present,char **error) {
    if (!read || !read->header || read->request.family!=WORKBOOK_CODEC_HEADER_READY ||
        !read->header->objects_ready || read->header->queries_ready)
        return fail(error,"Query definitions require complete unclaimed object descriptors and may be armed once.");
    if (!present) return fail(error,"The workbook requires query metadata.");
    read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_QUERIES,queries_sql,0,3,3,QUERY_ROWS+1u};
    return true;
}
bool workbook_codec_read_add_source_options(WorkbookCodecRead *read,bool present,char **error) {
    if (!read || !read->header || read->request.family!=WORKBOOK_CODEC_HEADER_READY ||
        !read->header->objects_ready || read->header->source_options_ready)
        return fail(error,"Source options require complete unclaimed object descriptors and may be armed once.");
    if (!present) return fail(error,"The workbook requires source options metadata.");
    read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_SOURCE_OPTIONS,source_options_sql,0,3,3,QUERY_ROWS+1u};
    return true;
}
bool workbook_codec_read_add_table_sources(WorkbookCodecRead *read,WorkbookCodecTableSourcePresence present,char **error) {
    if (!read || !read->header || read->request.family!=WORKBOOK_CODEC_HEADER_READY || !read->header->objects_ready ||
        !read->header->queries_ready || !read->header->source_options_ready || read->header->table_sources_ready)
        return fail(error,"Table sources require complete unclaimed descriptors, queries and options and may be armed once.");
    if (read->version==14 && (!present.sources || !present.filters || !present.sorts)) return fail(error,"The v14 workbook requires the complete Table source package.");
    if (!present.sources) {
        for (size_t i=0;i<read->header->object_count;i++) if (read->header->objects[i].value.kind==OBJECT_TABLE)
            return fail(error,"A Table descriptor requires its source definition.");
        read->header->table_sources_ready=true; return true;
    }
    if (!present.filters || !present.sorts) return fail(error,"Present Table sources require filter and sort metadata tables.");
    read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_TABLE_SOURCES,table_sources_sql,0,14,14,OBJECT_ROWS+1u};
    return true;
}
bool workbook_codec_read_add_pivot_marks(WorkbookCodecRead *read,WorkbookCodecPivotMarksPresence present,char **error) {
    if (!read || !read->header || read->request.family!=WORKBOOK_CODEC_HEADER_READY || !read->header->table_sources_ready || read->header->pivot_marks_ready)
        return fail(error,"Pivot/Marks require the complete unclaimed Table metadata chain and may be armed once.");
    if (!read->header->object_table_present) { read->header->pivot_marks_ready=true; return true; }
    if (!present.fields || !present.marks) return fail(error,"An objects table requires both Pivot fields and Marks metadata.");
    read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_PIVOT_FIELDS,pivot_fields_sql,0,9,9,PIVOT_ROWS_LIMIT+1u}; return true;
}
static char **query_target(WorkbookCodecHeader *header,size_t sheet,uint64_t owner) {
    if (!header || sheet>=header->count) return NULL;
    HeaderSheet *s=header->sheets+sheet;
    if (!owner) return &s->analysis.query;
    for (size_t i=0;i<s->object_count;i++) {
        HeaderObject *object=header->objects+s->object_offset+i;
        if (object->value.id==owner && object->value.kind==OBJECT_TABLE) return &object->query;
    }
    return NULL;
}
static bool copy_text(const WorkbookCodecAtom *atom,char **out,size_t *staged,size_t owned,size_t retained,size_t limit,char **error) {
    if (atom->kind==WORKBOOK_CODEC_NULL) { *out=NULL; return true; }
    size_t bytes=atom->value.text.bytes+1;
    if (*staged>SIZE_MAX-bytes || !room(owned,retained,*staged+bytes,limit)) return fail(error,"Metadata text exceeds its owned-buffer budget.");
    *out=dx_alloc(bytes); if (bytes>1) memcpy(*out,atom->value.text.data,bytes-1); (*out)[bytes-1]=0; *staged+=bytes; return true;
}
static bool sheet_row(WorkbookCodecRead *read,const WorkbookCodecAtom *cells,size_t id,HeaderSheet *out,size_t *staged,size_t retained,char **error) {
    if (cells[0].value.i32<0 || (size_t)cells[0].value.i32!=id || cells[2].value.i32<SOURCE_NONE || cells[2].value.i32>SOURCE_DEMO ||
        cells[8].value.i32<0 || cells[8].value.i32>FILTER_OP_MAX || cells[11].value.i32<0 || cells[11].value.i32>AGG_MAX)
        return fail(error,"Invalid ordered worksheet source metadata.");
    uint64_t rows[]={cells[14].value.u64,cells[16].value.u64,cells[18].value.u64,cells[20].value.u64};
    uint64_t columns[]={cells[15].value.u64,cells[17].value.u64,cells[19].value.u64};
    for (size_t i=0;i<sizeof(rows)/sizeof(*rows);i++) if (rows[i]>=DX_SHEET_ROWS) return fail(error,"Invalid worksheet viewport row.");
    for (size_t i=0;i<sizeof(columns)/sizeof(*columns);i++) if (columns[i]>=16384) return fail(error,"Invalid worksheet viewport column.");
    double horizontal_value=cells[21].value.real,canvas_value=cells[24].value.real;
    double fraction=cells[22].value.real;
    if (!isfinite(horizontal_value) || horizontal_value<0 || horizontal_value>FLT_MAX || !isfinite(fraction) || fraction<0 || fraction>=1 ||
        cells[23].value.i32<0 || cells[23].value.i32>3 || !isfinite(canvas_value) || canvas_value<.5 || canvas_value>2.5 ||
        (read->layout==WORKBOOK_CODEC_SHEETS_24 && cells[24].value.real!=1)) return fail(error,"Invalid worksheet viewport or legacy canvas default.");
    float horizontal=(float)horizontal_value,canvas=(float)canvas_value;
    Analysis *a=&out->analysis; a->kind=(SourceKind)cells[2].value.i32; a->filter_op=(FilterOp)cells[8].value.i32;
    a->aggregate=(Aggregate)cells[11].value.i32; a->descending=cells[13].value.boolean;
    out->view=(SheetView){.row=rows[0],.col=(size_t)columns[0],.anchor_row=rows[1],.anchor_col=(size_t)columns[1],.end_row=rows[2],.end_col=(size_t)columns[2],
        .top=rows[3],.horizontal=horizontal,.fraction=fraction,.selection=(unsigned)cells[23].value.i32,.canvas_zoom=canvas};
    char **texts[]={&out->name,&out->reference,&a->schema,&a->table,&a->filter_column,&a->filter_value,&a->group_column,&a->measure_column,&a->sort_column};
    const size_t fields[]={1,3,4,5,6,7,9,10,12};
    for (size_t i=0;i<sizeof(fields)/sizeof(*fields);i++) if (!copy_text(cells+fields[i],texts[i],staged,read->bytes,retained,read->limit,error)) return false;
    if (a->kind==SOURCE_NONE || a->kind==SOURCE_DEMO) {
        WorkbookCodecAtom empty={.kind=WORKBOOK_CODEC_TEXT,.value.text={"",0}};
        return copy_text(&empty,&a->path,staged,read->bytes,retained,read->limit,error);
    }
    if (!read->resolve || !room(read->bytes,retained,*staged,read->limit)) return fail(error,"This source reference needs a bounded locator resolver.");
    size_t maximum=read->limit-read->bytes-retained-*staged;
    if (maximum>DX_WORKBOOK_CODEC_READ_TEXT+1u) maximum=DX_WORKBOOK_CODEC_READ_TEXT+1u;
    if (!maximum) return fail(error,"Resolved locator exceeds its owned-buffer budget.");
    WorkbookCodecOwnedText resolved={0}; bool ok=read->resolve(read->context,a->kind,out->reference,maximum,&resolved);
    if (!ok || !resolved.data || resolved.bytes>=maximum || resolved.data[resolved.bytes] || !utf8(resolved.data,resolved.bytes)) {
        free(resolved.data); return fail(error,"Source locator resolver returned an invalid or oversized owned locator.");
    }
    a->path=resolved.data; *staged+=resolved.bytes+1; return true;
}
typedef struct { size_t sheet; AnalysisSort sort; } SortRow;
static bool columns_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,size_t retained,char **error) {
    if (read->request.first_row>COLUMN_ROWS || batch->row_count>COLUMN_ROWS-read->request.first_row)
        return fail(error,"Too many column presentation rows.");
    /* Fixed64-sheet POD scratch owns no allocations. Only touched sheets may
       allocate; all new arrays and names coexist with every old array until
       the complete batch has been validated and admitted. */
    size_t counts[DX_MAX_SHEETS]={0},capacities[DX_MAX_SHEETS]={0};
    char **arrays[DX_MAX_SHEETS]={0}; float *widths[DX_MAX_SHEETS]={0};
    WorkbookCodecHeader *header=read->header; size_t last=read->columns_sheet,string_bytes=0;
    for (size_t i=0;i<header->count;i++) counts[i]=header->sheets[i].view.column_count;
    for (size_t row=0;row<batch->row_count;row++) {
        const WorkbookCodecAtom *cells=batch->cells+row*4;
        int32_t sheet=cells[0].value.i32,index=cells[1].value.i32; double width=cells[3].value.real;
        if (sheet<0 || (size_t)sheet>=header->count || (size_t)sheet<last || index<0 ||
            (size_t)index!=counts[sheet] || counts[sheet]>=COLUMN_LIMIT || !isfinite(width) || width<52 || width>1200)
            return fail(error,"Invalid ordered column presentation metadata.");
        size_t bytes=cells[2].value.text.bytes;
        if (bytes==SIZE_MAX || string_bytes>SIZE_MAX-bytes-1u)
            return fail(error,"Column name allocation size overflows.");
        string_bytes+=bytes+1u; counts[sheet]++; last=(size_t)sheet;
    }
    size_t stage_bytes=batch->row_count*sizeof(char *),extra=stage_bytes,grown=0,retired=0;
    if (extra>SIZE_MAX-string_bytes) return fail(error,"Column metadata staging size overflows.");
    extra+=string_bytes;
    for (size_t i=0;i<header->count;i++) {
        size_t old=header->sheets[i].column_capacity;
        if (counts[i]<=old) continue;
        size_t capacity=old ? old : 16u;
        while (capacity<counts[i]) capacity*=2u; /* count<=16384, capped power of two */
        if (capacity>COLUMN_LIMIT || capacity>SIZE_MAX/(sizeof(char *)+sizeof(float)))
            return fail(error,"Column array allocation size overflows.");
        size_t bytes=capacity*(sizeof(char *)+sizeof(float));
        if (extra>SIZE_MAX-bytes) return fail(error,"Column metadata allocation size overflows.");
        capacities[i]=capacity; extra+=bytes; grown+=bytes; retired+=old*(sizeof(char *)+sizeof(float));
    }
    if (!room(read->bytes,retained,extra,read->limit)) return fail(error,"Column metadata staging exceeds its owned-buffer budget.");
    for (size_t row=0;row<batch->row_count;row++) {
        WorkbookCodecText name=batch->cells[row*4+2].value.text;
        if (!utf8(name.data,name.bytes)) return fail(error,"Column name contains NUL or is not valid UTF-8.");
    }
    char **names=stage_bytes ? dx_alloc(stage_bytes) : NULL;
    for (size_t i=0;i<header->count;i++) if (capacities[i]) {
        const SheetView *view=&header->sheets[i].view;
        arrays[i]=dx_alloc(capacities[i]*sizeof(char *)); widths[i]=dx_alloc(capacities[i]*sizeof(float));
        if (view->column_count) {
            memcpy(arrays[i],view->columns,view->column_count*sizeof(char *));
            memcpy(widths[i],view->widths,view->column_count*sizeof(float));
        }
    }
    for (size_t row=0;row<batch->row_count;row++) {
        WorkbookCodecText text=batch->cells[row*4+2].value.text; names[row]=dx_alloc(text.bytes+1u);
        if (text.bytes) memcpy(names[row],text.data,text.bytes);
    }
    /* All following operations are infallible publication. Old string pointers
       are moved verbatim; only superseded pointer/float arrays are retired. */
    for (size_t i=0;i<header->count;i++) if (capacities[i]) {
        SheetView *view=&header->sheets[i].view;
        free(view->columns); free(view->widths); view->columns=arrays[i]; view->widths=widths[i];
        header->sheets[i].column_capacity=capacities[i];
    }
    for (size_t row=0;row<batch->row_count;row++) {
        const WorkbookCodecAtom *cells=batch->cells+row*4; SheetView *view=&header->sheets[cells[0].value.i32].view;
        size_t index=view->column_count++; view->columns[index]=names[row]; view->widths[index]=(float)cells[3].value.real;
    }
    free(names); size_t added=string_bytes+grown-retired;
    read->bytes+=added; header->bytes+=added; read->columns_sheet=last; read->request.first_row+=batch->row_count;
    if (batch->end) { header->columns_ready=true; read->request.family=WORKBOOK_CODEC_HEADER_READY; }
    return true;
}
static bool sorts_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,size_t retained,char **error) {
    if (read->request.first_row>SORT_ROWS || batch->row_count>SORT_ROWS-read->request.first_row)
        return fail(error,"Too many secondary sort metadata rows.");
    size_t array_bytes=batch->row_count*sizeof(SortRow);
    if (!room(read->bytes,retained,array_bytes,read->limit)) return fail(error,"Sort metadata batch staging exceeds its owned-buffer budget.");
    SortRow *next=array_bytes ? dx_alloc(array_bytes) : NULL; size_t staged=array_bytes,done=0;
    bool previous=read->request.first_row!=0; size_t last_sheet=0,current_sheet=SIZE_MAX;
    if (previous) for (size_t i=0;i<read->header->count;i++) if (read->header->sheets[i].analysis.sort_count) last_sheet=i;
    /* The prospective Analysis borrows old and staged strings. Never free it:
       only the staging rows own their newly copied names until publication. */
    Analysis prospective={0};
    for (;done<batch->row_count;done++) {
        const WorkbookCodecAtom *row=batch->cells+done*4;
        int32_t sheet=row[0].value.i32,position=row[1].value.i32;
        if (sheet<0 || (size_t)sheet>=read->header->count || position<0 || position>=DX_ANALYSIS_SORTS-1 ||
            (previous && (size_t)sheet<last_sheet)) {
            fail(error,"Invalid ordered secondary sort metadata."); goto rejected;
        }
        if ((size_t)sheet!=current_sheet) { current_sheet=(size_t)sheet; prospective=read->header->sheets[current_sheet].analysis; }
        if (!prospective.sort_column || (size_t)position!=prospective.sort_count) {
            fail(error,"Secondary sort positions must follow an existing primary sort without gaps."); goto rejected;
        }
        next[done].sheet=(size_t)sheet; next[done].sort.descending=row[3].value.boolean;
        if (!copy_text(row+2,&next[done].sort.column,&staged,read->bytes,retained,read->limit,error)) goto rejected;
        prospective.sorts[prospective.sort_count++]=next[done].sort;
        /* At most8 names of4096 bytes also preserves the native32768-byte
           name limit. Existing ASCII-insensitive duplicate semantics apply. */
        if (!analysis_sorts_valid(&prospective,error)) goto rejected;
        previous=true; last_sheet=(size_t)sheet;
    }
    for (size_t i=0;i<done;i++) {
        Analysis *analysis=&read->header->sheets[next[i].sheet].analysis;
        analysis->sorts[analysis->sort_count++]=next[i].sort;
    }
    free(next); staged-=array_bytes;
    read->bytes+=staged; read->header->bytes+=staged; read->request.first_row+=done;
    if (batch->end) { read->header->sorts_ready=true; read->request.family=WORKBOOK_CODEC_HEADER_READY; }
    return true;
rejected:
    for (size_t i=0;i<=done && i<batch->row_count;i++) free(next[i].sort.column);
    free(next); return false;
}
static bool preferences_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,char **error) {
    if (read->request.first_row>DX_MAX_SHEETS+1u || batch->row_count>DX_MAX_SHEETS+1u-read->request.first_row)
        return fail(error,"Too many supported preference rows.");
    uint64_t seen=read->preference_seen,hidden=read->header->hidden;
    bool motion_seen=read->preference_motion_seen,motion=read->header->reduced_motion;
    for (size_t i=0;i<batch->row_count;i++) {
        const WorkbookCodecAtom *row=batch->cells+i*2; WorkbookCodecText name=row[0].value.text;
        if (name.bytes==sizeof("reduce_motion")-1u && !memcmp(name.data,"reduce_motion",name.bytes)) {
            if (motion_seen) return fail(error,"Duplicate reduce_motion preference.");
            motion_seen=true; motion=row[1].value.boolean; continue;
        }
        size_t prefix=sizeof("sheet_hidden_")-1u;
        if (name.bytes<=prefix || name.bytes>prefix+2u || memcmp(name.data,"sheet_hidden_",prefix))
            return fail(error,"Preference name does not match the supported named projection.");
        if (name.bytes>prefix+1u && name.data[prefix]=='0') return fail(error,"Visibility index is not canonical decimal.");
        size_t sheet=0;
        for (size_t j=prefix;j<name.bytes;j++) {
            unsigned char digit=(unsigned char)name.data[j];
            if (digit<'0' || digit>'9') return fail(error,"Visibility index is not canonical decimal.");
            sheet=sheet*10u+(digit-'0'); /* At most2 digits; bounded before arithmetic. */
        }
        if (sheet>=read->header->count || sheet>=DX_MAX_SHEETS) return fail(error,"Visibility preference references a missing worksheet.");
        uint64_t bit=UINT64_C(1)<<sheet;
        if (seen&bit) return fail(error,"Duplicate worksheet visibility preference.");
        seen|=bit;
        if (row[1].value.boolean) hidden|=bit; else hidden&=~bit;
    }
    if (hidden&(UINT64_C(1)<<read->header->active)) return fail(error,"The active worksheet must be visible.");
    read->preference_seen=seen; read->preference_motion_seen=motion_seen;
    read->header->hidden=hidden; read->header->reduced_motion=motion; read->request.first_row+=batch->row_count;
    if (batch->end) { read->header->preferences_ready=true; read->request.family=WORKBOOK_CODEC_HEADER_READY; }
    return true;
}
static bool object_sources(const HeaderObject *objects,size_t count,char **error) {
    for (size_t i=0;i<count;i++) if (objects[i].value.source_id) {
        bool found=false;
        for (size_t j=i;j>0 && objects[j-1].sheet==objects[i].sheet;j--) {
            const WorkbookCodecObjectView *candidate=&objects[j-1].value;
            if (candidate->id==objects[i].value.source_id && candidate->kind==OBJECT_TABLE) { found=true; break; }
        }
        for (size_t j=i+1;!found && j<count && objects[j].sheet==objects[i].sheet;j++) {
            const WorkbookCodecObjectView *candidate=&objects[j].value;
            if (candidate->id==objects[i].value.source_id && candidate->kind==OBJECT_TABLE) found=true;
        }
        if (!found) return fail(error,"An object source must reference a Table descriptor on the same worksheet.");
    }
    return true;
}
static bool objects_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,size_t retained,char **error) {
    WorkbookCodecHeader *header=read->header; size_t old_count=header->object_count;
    if (old_count>OBJECT_ROWS || batch->row_count>OBJECT_ROWS-old_count)
        return fail(error,"Too many object descriptors.");
    size_t count=old_count+batch->row_count,array_bytes=count*sizeof(HeaderObject),old_array_bytes=old_count*sizeof(HeaderObject);
    /* Existing names are transferred only after the full prospective batch
       validates. Both arrays and all newly copied names coexist until then. */
    if (!room(read->bytes,retained,array_bytes,read->limit)) return fail(error,"Object descriptor staging exceeds its owned-buffer budget.");
    HeaderObject *next=array_bytes ? dx_alloc(array_bytes) : NULL; size_t staged=array_bytes;
    if (old_count) memcpy(next,header->objects,old_array_bytes);
    for (size_t i=old_count;i<count;i++) {
        const WorkbookCodecAtom *row=batch->cells+(i-old_count)*11;
        int32_t sheet=row[0].value.i32,column=row[4].value.i32,width=row[6].value.i32;
        int32_t kind=row[7].value.i32,mark=row[8].value.i32,measure=row[10].value.i32;
        uint64_t id=row[1].value.u64,r=row[3].value.u64,height=row[5].value.u64,source=row[9].value.u64;
        if (sheet<0 || (size_t)sheet>=header->count || !id ||
            (i && ((size_t)sheet<next[i-1].sheet || ((size_t)sheet==next[i-1].sheet && id<=next[i-1].value.id)))) {
            fail(error,"Object descriptor IDs must be nonzero and strictly ordered within ordered worksheets."); goto rejected;
        }
        if (!row[2].value.text.bytes || column<0 || width<=0 || width>16384 || column>16384-width || !height || height>DX_SHEET_ROWS || r>DX_SHEET_ROWS-height ||
            kind<OBJECT_PIVOT || kind>OBJECT_TABLE || mark<MARK_BAR || mark>MARK_SCATTER || (kind!=OBJECT_CHART && mark!=MARK_BAR) ||
            measure<0 || measure>=DX_PIVOT_FIELDS || (kind==OBJECT_TABLE && (source || measure))) {
            fail(error,"Invalid object descriptor name, geometry, kind, mark or presentation index."); goto rejected;
        }
        size_t same_sheet=0;
        for (size_t j=i;j>0 && next[j-1].sheet==(size_t)sheet;j--) {
            const WorkbookCodecObjectView *prior=&next[j-1].value; same_sheet++;
            if (r<prior->row+prior->height && prior->row<r+height && (size_t)column<prior->column+prior->width && prior->column<(size_t)column+(size_t)width) {
                fail(error,"Object descriptors overlap on a worksheet."); goto rejected;
            }
        }
        if (same_sheet>=DX_MAX_OBJECTS) { fail(error,"Too many object descriptors on one worksheet."); goto rejected; }
        next[i].sheet=(size_t)sheet;
        next[i].value=(WorkbookCodecObjectView){.id=id,.row=r,.height=height,.source_id=source,.column=(size_t)column,.width=(size_t)width,
            .kind=(ObjectKind)kind,.mark=(ChartMark)mark,.chart_measure=(unsigned)measure};
        char *name=NULL;
        if (!copy_text(row+2,&name,&staged,read->bytes,retained,read->limit,error)) goto rejected;
        next[i].value.name=name;
    }
    if (batch->end && !object_sources(next,count,error)) goto rejected;
    free(header->objects); header->objects=next; header->object_count=count;
    size_t added=staged-old_array_bytes; read->bytes+=added; header->bytes+=added;
    for (size_t sheet=0,offset=0;sheet<header->count;sheet++) {
        header->sheets[sheet].object_offset=offset;
        while (offset<count && next[offset].sheet==sheet) offset++;
        header->sheets[sheet].object_count=offset-header->sheets[sheet].object_offset;
    }
    read->request.first_row=count;
    if (batch->end) { header->objects_ready=true; read->request.family=WORKBOOK_CODEC_HEADER_READY; }
    return true;
rejected:
    for (size_t i=old_count;i<count;i++) free((char *)next[i].value.name);
    free(next); return false;
}
typedef struct { char **target,*text; } QueryChange;
static bool queries_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,size_t retained,char **error) {
    if (read->request.first_row>QUERY_ROWS || batch->row_count>QUERY_ROWS-read->request.first_row)
        return fail(error,"Too many query definitions.");
    size_t array_bytes=batch->row_count*sizeof(QueryChange);
    if (!room(read->bytes,retained,array_bytes,read->limit)) return fail(error,"Query definition staging exceeds its owned-buffer budget.");
    QueryChange *next=array_bytes ? dx_alloc(array_bytes) : NULL; size_t staged=array_bytes,done=0;
    size_t last_sheet=read->query_sheet; uint64_t last_owner=read->query_owner;
    bool previous=read->request.first_row!=0;
    for (;done<batch->row_count;done++) {
        const WorkbookCodecAtom *row=batch->cells+done*3; int32_t sheet=row[0].value.i32; uint64_t owner=row[1].value.u64;
        if (sheet<0 || (size_t)sheet>=read->header->count ||
            (previous && ((size_t)sheet<last_sheet || ((size_t)sheet==last_sheet && owner<=last_owner)))) {
            fail(error,"Query owners must be strictly ordered within ordered worksheets."); goto rejected;
        }
        next[done].target=query_target(read->header,(size_t)sheet,owner);
        if (!next[done].target || (!owner && read->header->sheets[sheet].analysis.kind==SOURCE_NONE) ||
            *next[done].target || !row[2].value.text.bytes) {
            fail(error,"Query definition requires an existing source owner and nonempty unique text."); goto rejected;
        }
        if (!copy_text(row+2,&next[done].text,&staged,read->bytes,retained,read->limit,error)) goto rejected;
        last_sheet=(size_t)sheet; last_owner=owner; previous=true;
    }
    for (size_t i=0;i<done;i++) *next[i].target=next[i].text;
    free(next); staged-=array_bytes;
    read->bytes+=staged; read->header->bytes+=staged; read->request.first_row+=done;
    read->query_sheet=last_sheet; read->query_owner=last_owner;
    if (batch->end) { read->header->queries_ready=true; read->request.family=WORKBOOK_CODEC_HEADER_READY; }
    return true;
rejected:
    for (size_t i=0;i<=done && i<batch->row_count;i++) free(next[i].text);
    free(next); return false;
}
static bool *source_option_target(WorkbookCodecHeader *header,size_t sheet,uint64_t owner,size_t *index) {
    HeaderSheet *s=header->sheets+sheet;
    if (!owner) { *index=0; return &s->analysis.unique_rows; }
    for (size_t i=0;i<s->object_count;i++) {
        HeaderObject *object=header->objects+s->object_offset+i;
        if (object->value.id==owner && object->value.kind==OBJECT_TABLE) { *index=i+1; return &object->unique_rows; }
    }
    return NULL;
}
static bool source_options_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,char **error) {
    if (read->request.first_row>QUERY_ROWS || batch->row_count>QUERY_ROWS-read->request.first_row)
        return fail(error,"Too many source options rows.");
    /* Fixed512-byte stack POD scratch; no input strings or new owned buffers.
       The caller-owned input and entire reader were admitted before dispatch.
       Atoms remain borrowed and unchanged through both passes of this call. */
    uint64_t seen[DX_MAX_SHEETS]; memcpy(seen,read->source_options_seen,sizeof(seen));
    bool previous=read->request.first_row!=0; size_t last_sheet=read->source_options_sheet; uint64_t last_owner=read->source_options_owner;
    for (size_t row=0;row<batch->row_count;row++) {
        const WorkbookCodecAtom *cells=batch->cells+row*3; int32_t sheet=cells[0].value.i32; uint64_t owner=cells[1].value.u64;
        if (sheet<0 || (size_t)sheet>=read->header->count ||
            (previous && ((size_t)sheet<last_sheet || ((size_t)sheet==last_sheet && owner<=last_owner)))) {
            return fail(error,"Source option owners must be strictly ordered within ordered worksheets.");
        }
        size_t index=0; bool *target=source_option_target(read->header,(size_t)sheet,owner,&index),value=cells[2].value.boolean;
        if (!target || index>DX_MAX_OBJECTS || (!owner && value && read->header->sheets[sheet].analysis.kind==SOURCE_NONE)) {
            return fail(error,"Source option requires an existing source owner; a blank primary cannot remove duplicates.");
        }
        uint64_t bit=UINT64_C(1)<<index;
        if (seen[sheet]&bit) return fail(error,"Duplicate source option owner.");
        seen[sheet]|=bit;
        previous=true; last_sheet=(size_t)sheet; last_owner=owner;
    }
    for (size_t row=0;row<batch->row_count;row++) {
        const WorkbookCodecAtom *cells=batch->cells+row*3; size_t index=0;
        *source_option_target(read->header,(size_t)cells[0].value.i32,cells[1].value.u64,&index)=cells[2].value.boolean;
    }
    memcpy(read->source_options_seen,seen,sizeof(seen));
    read->source_options_sheet=last_sheet; read->source_options_owner=last_owner; read->request.first_row+=batch->row_count;
    if (batch->end) { read->header->source_options_ready=true; read->request.family=WORKBOOK_CODEC_HEADER_READY; }
    return true;
}
static HeaderObject *table_owner(WorkbookCodecHeader *header,size_t sheet,uint64_t owner) {
    if (sheet>=header->count || !owner) return NULL;
    HeaderSheet *s=header->sheets+sheet;
    for (size_t i=0;i<s->object_count;i++) {
        HeaderObject *object=header->objects+s->object_offset+i;
        if (object->value.id==owner && object->value.kind==OBJECT_TABLE) return object;
    }
    return NULL;
}
static bool table_reserve(WorkbookCodecRead *read,size_t retained,size_t staged,size_t extra,char **error) {
    return extra<=SIZE_MAX-staged && room(read->bytes,retained,staged+extra,read->limit) ? true : fail(error,"Table metadata staging or validation scratch exceeds its owned-buffer budget.");
}
static bool table_raw_add(size_t raw[DX_MAX_SHEETS],size_t sheet,const WorkbookCodecAtom *atom,char **error) {
    size_t bytes=atom->kind==WORKBOOK_CODEC_NULL ? 0 : atom->value.text.bytes;
    if (raw[sheet]>TABLE_RAW_BYTES || bytes>TABLE_RAW_BYTES-raw[sheet]) return fail(error,"Combined raw Table source text exceeds 8 MiB on one worksheet.");
    raw[sheet]+=bytes; return true;
}
static bool table_filter_scratch(FilterOp op,const char *text,size_t *out,char **error) {
    *out=0;
    if (!text || (op!=FILTER_BETWEEN && op!=FILTER_COMPOSITE)) return true;
    size_t n=strlen(text),pointers=6144u*sizeof(char *);
    /* analysis_parse_values: input scratchN+1, copied/temporary strings<=N+4097,
       and old2048+new4096 pointer arrays during growth. Composite retains only
       four outer strings<=N+4 plus16 pointers while one nested parser runs;
       its condition grammar permits BETWEEN, never recursive COMPOSITE.
       This is logical buffer admission, excluding allocator/RSS/diagnostics. */
    if (pointers>SIZE_MAX-4098u || n>(SIZE_MAX-4098u-pointers)/2u) return fail(error,"Table filter validation scratch overflow.");
    size_t bytes=2u*n+4098u+pointers;
    if (op==FILTER_COMPOSITE) {
        size_t extra=4u+16u*sizeof(char *);
        if (n>SIZE_MAX-extra || bytes>SIZE_MAX-n-extra) return fail(error,"Table composite validation scratch overflow.");
        bytes+=n+extra;
    }
    *out=bytes; return true;
}
static bool primary_filter_scratch(const Analysis *a,size_t *maximum,char **error) {
    for (size_t i=0;i<a->filter_count+(a->filter_column ? 1u : 0u);i++) {
        const char *value=i ? a->filters[i-1].value : a->filter_value; FilterOp op=i ? a->filters[i-1].op : a->filter_op; size_t scratch=0;
        if (!table_filter_scratch(op,value,&scratch,error)) return false;
        if (scratch>*maximum) *maximum=scratch;
    }
    return true;
}
bool workbook_codec_read_add_filters(WorkbookCodecRead *read,bool present,size_t retained,char **error) {
    if (!read || !read->header || read->request.family!=WORKBOOK_CODEC_HEADER_READY || read->header->filters_ready)
        return fail(error,"Filters require a complete unclaimed Header and may be armed once.");
    if (!present && read->version==14) return fail(error,"The v14 workbook requires secondary filter metadata.");
    size_t scratch=0;
    if (!table_reserve(read,retained,0,0,error)) return false;
    for (size_t i=0;i<read->header->count;i++) if (!primary_filter_scratch(&read->header->sheets[i].analysis,&scratch,error)) return false;
    /* Reserve the maximum across all primaries before the first native parser
       call, even for the absent12/13 default. There are no new owned buffers. */
    if (!table_reserve(read,retained,0,scratch,error)) return false;
    for (size_t i=0;i<read->header->count;i++) if (!analysis_filters_valid(&read->header->sheets[i].analysis,error)) return false;
    if (present) read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_FILTERS,filters_sql,0,5,5,FILTER_ROWS+1u};
    else read->header->filters_ready=true;
    return true;
}
typedef struct { size_t sheet; AnalysisFilter filter; } PrimaryFilterChange;
static bool filters_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,size_t retained,char **error) {
    if (read->request.first_row>FILTER_ROWS || batch->row_count>FILTER_ROWS-read->request.first_row) return fail(error,"Too many secondary filter metadata rows.");
    size_t array_bytes=batch->row_count*sizeof(PrimaryFilterChange);
    if (!table_reserve(read,retained,0,array_bytes,error)) return false;
    PrimaryFilterChange *next=array_bytes ? dx_alloc(array_bytes) : NULL; size_t staged=array_bytes,current_sheet=SIZE_MAX,last_sheet=read->filter_sheet,raw[DX_MAX_SHEETS];
    memcpy(raw,read->filter_raw,sizeof(raw)); bool previous=read->request.first_row!=0; Analysis prospective={0};
    for (size_t row=0;row<batch->row_count;row++) {
        const WorkbookCodecAtom *cells=batch->cells+row*5; int32_t sheet=cells[0].value.i32,position=cells[1].value.i32,op=cells[3].value.i32;
        if (sheet<0 || (size_t)sheet>=read->header->count || (previous && (size_t)sheet<last_sheet)) { fail(error,"Secondary filter worksheets must be ordered."); goto rejected; }
        if ((size_t)sheet!=current_sheet) { current_sheet=(size_t)sheet; prospective=read->header->sheets[sheet].analysis; }
        if (!prospective.filter_column || position<0 || position>=DX_ANALYSIS_FILTERS-1 || (size_t)position!=prospective.filter_count || op<0 || op>FILTER_OP_MAX) { fail(error,"Secondary filters require a primary and contiguous positions."); goto rejected; }
        size_t value_bytes=cells[4].kind==WORKBOOK_CODEC_NULL ? 0 : cells[4].value.text.bytes;
        if (cells[2].value.text.bytes>TABLE_RAW_BYTES-raw[sheet] || value_bytes>TABLE_RAW_BYTES-raw[sheet]-cells[2].value.text.bytes) { fail(error,"Raw secondary filter text exceeds 8 MiB on one worksheet."); goto rejected; }
        raw[sheet]+=cells[2].value.text.bytes+value_bytes; next[row].sheet=(size_t)sheet; next[row].filter.op=(FilterOp)op;
        if (!copy_text(cells+2,&next[row].filter.column,&staged,read->bytes,retained,read->limit,error) || !copy_text(cells+4,&next[row].filter.value,&staged,read->bytes,retained,read->limit,error)) goto rejected;
        prospective.filters[prospective.filter_count++]=next[row].filter; size_t scratch=0;
        if (!primary_filter_scratch(&prospective,&scratch,error) || !table_reserve(read,retained,staged,scratch,error) || !analysis_filters_valid(&prospective,error)) goto rejected;
        previous=true; last_sheet=(size_t)sheet;
    }
    for (size_t row=0;row<batch->row_count;row++) { Analysis *a=&read->header->sheets[next[row].sheet].analysis; a->filters[a->filter_count++]=next[row].filter; }
    free(next); staged-=array_bytes; read->bytes+=staged; read->header->bytes+=staged; memcpy(read->filter_raw,raw,sizeof(raw)); read->filter_sheet=last_sheet; read->request.first_row+=batch->row_count;
    if (batch->end) { read->header->filters_ready=true; read->request.family=WORKBOOK_CODEC_HEADER_READY; }
    return true;
rejected:
    for (size_t row=0;row<batch->row_count;row++) { free(next[row].filter.column); free(next[row].filter.value); }
    free(next); return false;
}
static bool table_analysis_valid(WorkbookCodecRead *read,const HeaderObject *owner,const HeaderTableSource *source,const Analysis *analysis,size_t staged,size_t retained,char **error) {
    Analysis view=*analysis; view.path=source->reference; view.query=owner->query; view.unique_rows=owner->unique_rows;
    if (!analysis_query_valid(&view,error) || !analysis_sorts_valid(&view,error)) return false;
    size_t scratch=0;
    for (size_t i=0;i<view.filter_count+(view.filter_column ? 1u : 0u);i++) {
        const char *value=i ? view.filters[i-1].value : view.filter_value; FilterOp op=i ? view.filters[i-1].op : view.filter_op; size_t next=0;
        if (!table_filter_scratch(op,value,&next,error)) return false;
        if (next>scratch) scratch=next; /* Existing validator frees each parse before the next. */
    }
    return table_reserve(read,retained,staged,scratch,error) && analysis_filters_valid(&view,error);
}
static bool table_locator(WorkbookCodecRead *read,HeaderTableSource *source,const WorkbookCodecAtom *atom,size_t *staged,size_t retained,char **error) {
    Analysis *a=&source->analysis;
    if (a->kind==SOURCE_DEMO) return copy_text(atom,&a->path,staged,read->bytes,retained,read->limit,error);
    if (!read->resolve) return fail(error,"Table reference needs a bounded locator resolver.");
    if (!table_reserve(read,retained,*staged,1,error)) return false;
    size_t maximum=read->limit-read->bytes-retained-*staged;
    if (maximum>DX_WORKBOOK_CODEC_READ_TEXT+1u) maximum=DX_WORKBOOK_CODEC_READ_TEXT+1u;
    WorkbookCodecOwnedText resolved={0}; bool ok=read->resolve(read->context,a->kind,source->reference,maximum,&resolved);
    if (!ok || !resolved.data || resolved.bytes>=maximum || resolved.data[resolved.bytes] || !utf8(resolved.data,resolved.bytes)) {
        free(resolved.data); return fail(error,"Table locator resolver returned invalid or oversized owned storage.");
    }
    a->path=resolved.data; *staged+=resolved.bytes+1u; return true;
}
typedef struct { HeaderObject *owner; HeaderTableSource *source; } TableBaseChange;
static bool table_sources_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,size_t retained,char **error) {
    if (read->request.first_row>OBJECT_ROWS || batch->row_count>OBJECT_ROWS-read->request.first_row) return fail(error,"Too many Table source rows.");
    size_t array_bytes=batch->row_count*sizeof(TableBaseChange);
    if (!table_reserve(read,retained,0,array_bytes,error)) return false;
    TableBaseChange *next=array_bytes ? dx_alloc(array_bytes) : NULL; size_t staged=array_bytes,index=read->table_next;
    size_t raw[DX_MAX_SHEETS]; memcpy(raw,read->table_raw,sizeof(raw));
    for (size_t row=0;row<batch->row_count;row++) {
        const WorkbookCodecAtom *cells=batch->cells+row*14;
        while (index<read->header->object_count && read->header->objects[index].value.kind!=OBJECT_TABLE) index++;
        HeaderObject *owner=index<read->header->object_count ? read->header->objects+index : NULL;
        int32_t sheet=cells[0].value.i32,kind=cells[2].value.i32,op=cells[8].value.i32,aggregate=cells[11].value.i32;
        if (!owner || sheet<0 || owner->sheet!=(size_t)sheet || owner->value.id!=cells[1].value.u64 || owner->source) { fail(error,"Table sources must match every ordered Table descriptor exactly once."); goto rejected; }
        if (kind<SOURCE_CSV || kind>SOURCE_DEMO || op<0 || op>FILTER_OP_MAX || aggregate<0 || aggregate>AGG_MAX || (kind!=SOURCE_DEMO && !cells[3].value.text.bytes)) { fail(error,"Invalid Table source kind, reference or operation."); goto rejected; }
        const size_t fields[]={3,4,5,6,7,9,10,12};
        for (size_t f=0;f<sizeof(fields)/sizeof(*fields);f++) if (!table_raw_add(raw,(size_t)sheet,cells+fields[f],error)) goto rejected;
        if (!table_reserve(read,retained,staged,sizeof(HeaderTableSource),error)) goto rejected;
        next[row].owner=owner; next[row].source=dx_alloc(sizeof(HeaderTableSource)); staged+=sizeof(HeaderTableSource);
        HeaderTableSource *source=next[row].source; Analysis *a=&source->analysis; a->kind=(SourceKind)kind; a->filter_op=(FilterOp)op; a->aggregate=(Aggregate)aggregate; a->descending=cells[13].value.boolean;
        char **texts[]={&source->reference,&a->schema,&a->table,&a->filter_column,&a->filter_value,&a->group_column,&a->measure_column,&a->sort_column};
        for (size_t f=0;f<sizeof(fields)/sizeof(*fields);f++) if (!copy_text(cells+fields[f],texts[f],&staged,read->bytes,retained,read->limit,error)) goto rejected;
        if (!table_locator(read,source,cells+3,&staged,retained,error) || !table_analysis_valid(read,owner,source,a,staged,retained,error)) goto rejected;
        index++;
    }
    if (batch->end) {
        while (index<read->header->object_count && read->header->objects[index].value.kind!=OBJECT_TABLE) index++;
        if (index!=read->header->object_count) { fail(error,"A Table descriptor is missing its source definition."); goto rejected; }
    }
    for (size_t row=0;row<batch->row_count;row++) next[row].owner->source=next[row].source;
    free(next); staged-=array_bytes; read->bytes+=staged; read->header->bytes+=staged; read->table_next=index; memcpy(read->table_raw,raw,sizeof(raw)); read->request.first_row+=batch->row_count;
    if (batch->end) read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_TABLE_FILTERS,table_filters_sql,0,6,6,TABLE_FILTER_ROWS+1u};
    return true;
rejected:
    for (size_t row=0;row<batch->row_count;row++) table_source_free(next[row].source);
    free(next); return false;
}
typedef struct { HeaderObject *owner; AnalysisFilter filter; } TableFilterChange;
static bool table_filters_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,size_t retained,char **error) {
    if (read->request.first_row>TABLE_FILTER_ROWS || batch->row_count>TABLE_FILTER_ROWS-read->request.first_row) return fail(error,"Too many Table filter rows.");
    size_t array_bytes=batch->row_count*sizeof(TableFilterChange);
    if (!table_reserve(read,retained,0,array_bytes,error)) return false;
    TableFilterChange *next=array_bytes ? dx_alloc(array_bytes) : NULL; size_t staged=array_bytes,raw[DX_MAX_SHEETS]; memcpy(raw,read->table_raw,sizeof(raw));
    size_t last_sheet=read->table_sheet; uint64_t last_owner=read->table_owner; bool previous=read->request.first_row!=0; HeaderObject *current=NULL; Analysis prospective={0};
    for (size_t row=0;row<batch->row_count;row++) {
        const WorkbookCodecAtom *cells=batch->cells+row*6; int32_t sheet=cells[0].value.i32,position=cells[2].value.i32,op=cells[4].value.i32; uint64_t id=cells[1].value.u64;
        if (sheet<0 || (size_t)sheet>=read->header->count || (previous && ((size_t)sheet<last_sheet || ((size_t)sheet==last_sheet && id<last_owner)))) { fail(error,"Table filter owners are not ordered."); goto rejected; }
        HeaderObject *owner=table_owner(read->header,(size_t)sheet,id);
        if (!owner || !owner->source) { fail(error,"Table filter owner is missing or not a Table."); goto rejected; }
        if (owner!=current) { current=owner; prospective=owner->source->analysis; }
        if (!prospective.filter_column || position<0 || position>=DX_ANALYSIS_FILTERS-1 || (size_t)position!=prospective.filter_count || op<0 || op>FILTER_OP_MAX) { fail(error,"Table secondary filters require a primary and contiguous positions."); goto rejected; }
        if (!table_raw_add(raw,(size_t)sheet,cells+3,error) || !table_raw_add(raw,(size_t)sheet,cells+5,error)) goto rejected;
        next[row].owner=owner; next[row].filter.op=(FilterOp)op;
        if (!copy_text(cells+3,&next[row].filter.column,&staged,read->bytes,retained,read->limit,error) || !copy_text(cells+5,&next[row].filter.value,&staged,read->bytes,retained,read->limit,error)) goto rejected;
        prospective.filters[prospective.filter_count++]=next[row].filter;
        if (!table_analysis_valid(read,owner,owner->source,&prospective,staged,retained,error)) goto rejected;
        previous=true; last_sheet=(size_t)sheet; last_owner=id;
    }
    for (size_t row=0;row<batch->row_count;row++) { Analysis *a=&next[row].owner->source->analysis; a->filters[a->filter_count++]=next[row].filter; }
    free(next); staged-=array_bytes; read->bytes+=staged; read->header->bytes+=staged; memcpy(read->table_raw,raw,sizeof(raw)); read->table_sheet=last_sheet; read->table_owner=last_owner; read->request.first_row+=batch->row_count;
    if (batch->end) read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_TABLE_SORTS,table_sorts_sql,0,5,5,TABLE_SORT_ROWS+1u};
    return true;
rejected:
    for (size_t row=0;row<batch->row_count;row++) { free(next[row].filter.column); free(next[row].filter.value); }
    free(next); return false;
}
typedef struct { HeaderObject *owner; AnalysisSort sort; } TableSortChange;
static bool table_known_add(size_t *bytes,size_t extra,char **error) {
    if (*bytes>DX_OBJECT_CURRENT_BYTES || extra>DX_OBJECT_CURRENT_BYTES-*bytes)
        return fail(error,"Known Table/object definitions exceed the native 8 MiB current limit.");
    *bytes+=extra; return true;
}
static bool table_known_text(size_t *bytes,const char *text,char **error) {
    return !text || table_known_add(bytes,strlen(text)+1u,error);
}
static bool object_known_current(const WorkbookCodecHeader *header,const TableSortChange *pending,size_t count,const DefinitionChange *definitions,size_t definition_count,bool marks_phase,char **error) {
    /* Same native ObjectSnapshot byte terms for the data already decoded.
       Unavailable locator lengths and uncompleted Pivot/Marks payloads are unknown;
       this is a lower bound, not completed ObjectStore/model validation. Raw
       references have their separate native raw-text and reader-owned budgets. */
    for (size_t sheet=0;sheet<header->count;sheet++) {
        const HeaderSheet *s=header->sheets+sheet; size_t bytes=s->object_count*sizeof(SheetObject);
        for (size_t i=0;i<s->object_count;i++) {
            const HeaderObject *owner=header->objects+s->object_offset+i;
            if (!table_known_text(&bytes,owner->value.name,error)) return false;
            const PivotDefinition *pivot=owner->pivot; const MarkDefinition *marks=owner->marks;
            for (size_t j=0;j<definition_count;j++) if (definitions[j].owner==owner) {
                if (marks_phase) marks=definitions[j].definition; else pivot=definitions[j].definition;
            }
            if (pivot) for (size_t r=0;r<4;r++) for (size_t j=0;j<pivot->counts[r];j++)
                if (!table_known_text(&bytes,pivot->fields[r][j].name,error) || !table_known_text(&bytes,pivot->fields[r][j].value,error)) return false;
            if (marks) for (size_t r=0;r<DX_MARK_ROLES;r++) for (size_t j=0;j<marks->counts[r];j++)
                if (!table_known_text(&bytes,marks->fields[r][j].name,error)) return false;
            if (owner->value.kind!=OBJECT_TABLE) continue;
            const Analysis *a=&owner->source->analysis;
            const char *texts[]={a->path,a->schema,a->table,a->query ? a->query : owner->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
            for (size_t j=0;j<sizeof(texts)/sizeof(*texts);j++) if (!table_known_text(&bytes,texts[j],error)) return false;
            for (size_t j=0;j<a->filter_count;j++) if (!table_known_text(&bytes,a->filters[j].column,error) || !table_known_text(&bytes,a->filters[j].value,error)) return false;
            for (size_t j=0;j<a->sort_count;j++) if (!table_known_text(&bytes,a->sorts[j].column,error)) return false;
        }
        for (size_t i=0;i<count;i++) if (pending[i].owner->sheet==sheet && !table_known_text(&bytes,pending[i].sort.column,error)) return false;
    }
    return true;
}
static bool table_sorts_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,size_t retained,char **error) {
    if (read->request.first_row>TABLE_SORT_ROWS || batch->row_count>TABLE_SORT_ROWS-read->request.first_row) return fail(error,"Too many Table sort rows.");
    size_t array_bytes=batch->row_count*sizeof(TableSortChange);
    if (!table_reserve(read,retained,0,array_bytes,error)) return false;
    TableSortChange *next=array_bytes ? dx_alloc(array_bytes) : NULL; size_t staged=array_bytes,raw[DX_MAX_SHEETS]; memcpy(raw,read->table_raw,sizeof(raw));
    size_t last_sheet=read->table_sheet; uint64_t last_owner=read->table_owner; bool previous=read->request.first_row!=0; HeaderObject *current=NULL; Analysis prospective={0};
    for (size_t row=0;row<batch->row_count;row++) {
        const WorkbookCodecAtom *cells=batch->cells+row*5; int32_t sheet=cells[0].value.i32,position=cells[2].value.i32; uint64_t id=cells[1].value.u64;
        if (sheet<0 || (size_t)sheet>=read->header->count || (previous && ((size_t)sheet<last_sheet || ((size_t)sheet==last_sheet && id<last_owner)))) { fail(error,"Table sort owners are not ordered."); goto rejected; }
        HeaderObject *owner=table_owner(read->header,(size_t)sheet,id);
        if (!owner || !owner->source) { fail(error,"Table sort owner is missing or not a Table."); goto rejected; }
        if (owner!=current) { current=owner; prospective=owner->source->analysis; }
        if (!prospective.sort_column || position<0 || position>=DX_ANALYSIS_SORTS-1 || (size_t)position!=prospective.sort_count) { fail(error,"Table secondary sorts require a primary and contiguous positions."); goto rejected; }
        if (!table_raw_add(raw,(size_t)sheet,cells+3,error)) goto rejected;
        next[row].owner=owner; next[row].sort.descending=cells[4].value.boolean;
        if (!copy_text(cells+3,&next[row].sort.column,&staged,read->bytes,retained,read->limit,error)) goto rejected;
        prospective.sorts[prospective.sort_count++]=next[row].sort;
        if (!table_analysis_valid(read,owner,owner->source,&prospective,staged,retained,error)) goto rejected;
        previous=true; last_sheet=(size_t)sheet; last_owner=id;
    }
    if (batch->end && !object_known_current(read->header,next,batch->row_count,NULL,0,false,error)) goto rejected;
    for (size_t row=0;row<batch->row_count;row++) { Analysis *a=&next[row].owner->source->analysis; a->sorts[a->sort_count++]=next[row].sort; }
    free(next); staged-=array_bytes; read->bytes+=staged; read->header->bytes+=staged; memcpy(read->table_raw,raw,sizeof(raw)); read->table_sheet=last_sheet; read->table_owner=last_owner; read->request.first_row+=batch->row_count;
    if (batch->end) {
        /* Every fallible operation is over. Move each already-owned query once;
           no copy/reallocation changes its borrowed text identity. */
        for (size_t i=0;i<read->header->object_count;i++) {
            HeaderObject *owner=read->header->objects+i;
            if (owner->value.kind!=OBJECT_TABLE) continue;
            owner->source->analysis.query=owner->query; owner->query=NULL;
            owner->source->analysis.unique_rows=owner->unique_rows;
        }
        read->header->table_sources_ready=true; read->request.family=WORKBOOK_CODEC_HEADER_READY;
    }
    return true;
rejected:
    for (size_t row=0;row<batch->row_count;row++) free(next[row].sort.column);
    free(next); return false;
}
static HeaderObject *analytical_owner(WorkbookCodecHeader *header,size_t sheet,uint64_t id) {
    if (sheet>=header->count || !id) return NULL;
    HeaderSheet *s=header->sheets+sheet;
    for (size_t i=0;i<s->object_count;i++) { HeaderObject *o=header->objects+s->object_offset+i; if (o->value.id==id && o->value.kind!=OBJECT_TABLE) return o; }
    return NULL;
}
static void definition_discard(DefinitionChange *change,bool marks_phase) {
    if (marks_phase) {
        MarkDefinition *m=change->definition;
        for (size_t r=0;r<DX_MARK_ROLES;r++) for (size_t i=change->original[r];i<m->counts[r];i++) free(m->fields[r][i].name);
    } else {
        PivotDefinition *p=change->definition;
        for (size_t r=0;r<4;r++) for (size_t i=change->original[r];i<p->counts[r];i++) { free(p->fields[r][i].name); free(p->fields[r][i].value); }
    }
    free(change->definition);
}
static bool definition_pivot_valid(WorkbookCodecRead *read,const PivotDefinition *p,size_t staged,size_t retained,char **error) {
    size_t scratch=0;
    for (size_t r=0;r<4;r++) for (size_t i=0;i<p->counts[r];i++) {
        size_t next=0; if (!table_filter_scratch(p->fields[r][i].filter,p->fields[r][i].value,&next,error)) return false;
        if (next>scratch) scratch=next;
    }
    if (!table_reserve(read,retained,staged,scratch,error)) return false;
    /* pivot_valid's legacy failure helper requires an actual diagnostic slot. */
    char *discard=NULL; bool valid=pivot_valid(p,error ? error : &discard); free(discard); return valid;
}
static bool definition_arity(const WorkbookCodecHeader *header,const DefinitionChange *next,size_t count,char **error) {
    for (size_t i=0;i<header->object_count;i++) {
        const HeaderObject *owner=header->objects+i; const PivotDefinition *p=owner->pivot;
        for (size_t j=0;j<count;j++) if (next[j].owner==owner) p=next[j].definition;
        if (owner->value.chart_measure && (!p || owner->value.chart_measure>=p->counts[PIVOT_VALUES]))
            return fail(error,"Object presentation index does not fit its complete Values shelf.");
    }
    return true;
}
static bool definitions_feed(WorkbookCodecRead *read,const WorkbookCodecBatch *batch,size_t retained,bool marks_phase,char **error) {
    size_t limit=marks_phase ? MARK_ROWS_LIMIT : PIVOT_ROWS_LIMIT;
    if (read->request.first_row>limit || batch->row_count>limit-read->request.first_row) return fail(error,"Too many Pivot/Marks metadata rows.");
    size_t array_bytes=batch->row_count*sizeof(DefinitionChange);
    if (!table_reserve(read,retained,0,array_bytes,error)) return false;
    DefinitionChange *next=array_bytes ? dx_alloc(array_bytes) : NULL; size_t staged=array_bytes,count=0,retired=0;
    size_t raw[DX_MAX_SHEETS]; memcpy(raw,read->definition_raw[marks_phase ? 1 : 0],sizeof(raw));
    size_t last_sheet=read->definition_sheet,last_role=read->definition_role; uint64_t last_owner=read->definition_owner; bool previous=read->request.first_row!=0;
    for (size_t row=0;row<batch->row_count;row++) {
        const WorkbookCodecAtom *cells=batch->cells+row*(marks_phase ? 8 : 9);
        int32_t sheet=cells[0].value.i32,role=cells[2].value.i32,position=cells[3].value.i32,aggregate=cells[marks_phase ? 6 : 5].value.i32,grain=cells[marks_phase ? 7 : 8].value.i32;
        uint64_t id=cells[1].value.u64;
        if (sheet<0 || (size_t)sheet>=read->header->count || role<0 || role>=(marks_phase ? DX_MARK_ROLES : 4) || position<0 || position>=(marks_phase ? DX_MARK_FIELDS : DX_PIVOT_FIELDS) || aggregate<0 || aggregate>AGG_MAX || grain<0 || grain>PIVOT_DATE_MONTH)
            { fail(error,"Invalid Pivot/Marks owner, role, position or operation."); goto rejected; }
        if (previous && ((size_t)sheet<last_sheet || ((size_t)sheet==last_sheet && (id<last_owner || (id==last_owner && (size_t)role<last_role)))))
            { fail(error,"Pivot/Marks owners and roles must be strictly ordered."); goto rejected; }
        HeaderObject *owner=analytical_owner(read->header,(size_t)sheet,id);
        if (!owner) { fail(error,"Pivot/Marks requires an exact non-Table object owner."); goto rejected; }
        if (cells[4].value.text.bytes>TABLE_RAW_BYTES-raw[sheet]) { fail(error,"Raw Pivot/Marks text exceeds 8 MiB on one worksheet."); goto rejected; }
        raw[sheet]+=cells[4].value.text.bytes;
        if (!marks_phase && cells[7].kind!=WORKBOOK_CODEC_NULL) {
            if (cells[7].value.text.bytes>TABLE_RAW_BYTES-raw[sheet]) { fail(error,"Raw Pivot/Marks text exceeds 8 MiB on one worksheet."); goto rejected; }
            raw[sheet]+=cells[7].value.text.bytes;
        }
        if (!count || next[count-1].owner!=owner) {
            size_t bytes=marks_phase ? sizeof(MarkDefinition) : sizeof(PivotDefinition);
            if (!table_reserve(read,retained,staged,bytes,error)) goto rejected;
            DefinitionChange *change=next+count++; change->owner=owner; change->definition=dx_alloc(bytes); staged+=bytes;
            if (marks_phase && owner->marks) { memcpy(change->definition,owner->marks,bytes); memcpy(change->original,owner->marks->counts,sizeof(owner->marks->counts)); retired+=bytes; }
            if (!marks_phase && owner->pivot) { memcpy(change->definition,owner->pivot,bytes); memcpy(change->original,owner->pivot->counts,sizeof(owner->pivot->counts)); retired+=bytes; }
        }
        DefinitionChange *change=next+count-1;
        if (marks_phase) {
            MarkDefinition *m=change->definition;
            if ((size_t)position!=m->counts[role]) { fail(error,"Marks positions must be contiguous and unique."); goto rejected; }
            MarkField *field=&m->fields[role][m->counts[role]++];
            *field=(MarkField){.measure=cells[5].value.boolean,.aggregate=(Aggregate)aggregate,.grain=(PivotDateGrain)grain};
            if (!copy_text(cells+4,&field->name,&staged,read->bytes,retained,read->limit,error) || !marks_valid(m,error)) goto rejected;
        } else {
            PivotDefinition *p=change->definition; int32_t op=cells[6].value.i32;
            if ((size_t)position!=p->counts[role] || op<0 || op>FILTER_OP_MAX) { fail(error,"Pivot positions must be contiguous and operations valid."); goto rejected; }
            PivotField *field=&p->fields[role][p->counts[role]++];
            *field=(PivotField){.aggregate=(Aggregate)aggregate,.filter=(FilterOp)op,.grain=(PivotDateGrain)grain};
            if (!copy_text(cells+4,&field->name,&staged,read->bytes,retained,read->limit,error) || !copy_text(cells+7,&field->value,&staged,read->bytes,retained,read->limit,error) || !definition_pivot_valid(read,p,staged,retained,error)) goto rejected;
        }
        previous=true; last_sheet=(size_t)sheet; last_owner=id; last_role=(size_t)role;
    }
    if (batch->end && ((!marks_phase && !definition_arity(read->header,next,count,error)) || !object_known_current(read->header,NULL,0,next,count,marks_phase,error))) goto rejected;
    for (size_t i=0;i<count;i++) {
        if (marks_phase) { free(next[i].owner->marks); next[i].owner->marks=next[i].definition; }
        else { free(next[i].owner->pivot); next[i].owner->pivot=next[i].definition; }
    }
    free(next); size_t added=staged-array_bytes-retired; read->bytes+=added; read->header->bytes+=added;
    memcpy(read->definition_raw[marks_phase ? 1 : 0],raw,sizeof(raw)); read->definition_sheet=last_sheet; read->definition_owner=last_owner; read->definition_role=last_role; read->request.first_row+=batch->row_count;
    if (batch->end) {
        if (marks_phase) { read->header->pivot_marks_ready=true; read->request.family=WORKBOOK_CODEC_HEADER_READY; }
        else { read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_MARKS,marks_sql,0,8,8,MARK_ROWS_LIMIT+1u}; read->definition_sheet=read->definition_role=0; read->definition_owner=0; }
    }
    return true;
rejected:
    for (size_t i=0;i<count;i++) definition_discard(next+i,marks_phase);
    free(next); return false;
}

WorkbookCodecReadStatus workbook_codec_read_feed(WorkbookCodecRead *read,WorkbookCodecKey key,
    WorkbookCodecFamily family,const WorkbookCodecBatch *batch,size_t retained,char **error) {
    if (!read || !same_key(read->key,key)) return WORKBOOK_CODEC_READ_STALE;
    if (!read->header || !request_family(read->request.family) || family!=read->request.family || !batch || batch->first_row!=read->request.first_row) {
        fail(error,"Metadata family or batch order does not match its cursor."); return WORKBOOK_CODEC_READ_ERROR;
    }
    const WorkbookCodecTable *table=workbook_codec_table_find(family==WORKBOOK_CODEC_META ? "meta" : family==WORKBOOK_CODEC_SHEETS ? "sheets" : family==WORKBOOK_CODEC_SORTS ? "sorts" : family==WORKBOOK_CODEC_OBJECTS ? "objects" : family==WORKBOOK_CODEC_QUERIES ? "queries" : family==WORKBOOK_CODEC_SOURCE_OPTIONS ? "source_options" : family==WORKBOOK_CODEC_TABLE_SOURCES ? "table_sources" : family==WORKBOOK_CODEC_TABLE_FILTERS ? "table_filters" : family==WORKBOOK_CODEC_TABLE_SORTS ? "table_sorts" : family==WORKBOOK_CODEC_PIVOT_FIELDS ? "pivot_fields" : family==WORKBOOK_CODEC_MARKS ? "marks" : family==WORKBOOK_CODEC_FILTERS ? "filters" : family==WORKBOOK_CODEC_COLUMNS ? "columns" : "preferences");
    if (!shape(batch,table,read->options.full_primary_filters,error)) return WORKBOOK_CODEC_READ_ERROR;
    if (!room(read->bytes,retained,0,read->limit)) { fail(error,"Metadata input exceeds its owned-buffer budget."); return WORKBOOK_CODEC_READ_ERROR; }
    if (family==WORKBOOK_CODEC_PREFERENCES) return preferences_feed(read,batch,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_COLUMNS) return columns_feed(read,batch,retained,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_FILTERS) return filters_feed(read,batch,retained,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_SORTS) return sorts_feed(read,batch,retained,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_OBJECTS) return objects_feed(read,batch,retained,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_QUERIES) return queries_feed(read,batch,retained,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_SOURCE_OPTIONS) return source_options_feed(read,batch,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_TABLE_SOURCES) return table_sources_feed(read,batch,retained,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_TABLE_FILTERS) return table_filters_feed(read,batch,retained,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_TABLE_SORTS) return table_sorts_feed(read,batch,retained,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_PIVOT_FIELDS || family==WORKBOOK_CODEC_MARKS) return definitions_feed(read,batch,retained,family==WORKBOOK_CODEC_MARKS,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
    if (family==WORKBOOK_CODEC_META) {
        if (batch->first_row || batch->row_count!=1 || !batch->end || batch->cells[0].value.i32!=(int32_t)read->version ||
            batch->cells[1].value.u64>=DX_MAX_SHEETS || !isfinite(batch->cells[2].value.real) || batch->cells[2].value.real<.8 || batch->cells[2].value.real>1.5) {
            fail(error,"Invalid complete workbook meta row."); return WORKBOOK_CODEC_READ_ERROR;
        }
        read->header->active=(size_t)batch->cells[1].value.u64; read->header->user_scale=(float)batch->cells[2].value.real;
        read->request=(WorkbookCodecRequest){WORKBOOK_CODEC_SHEETS,read->layout==WORKBOOK_CODEC_SHEETS_24 ? sheets24_sql : sheets25_sql,0,25,(size_t)read->layout,65};
        return WORKBOOK_CODEC_READ_ACCEPTED;
    }
    size_t count=read->header->count;
    if (batch->row_count>DX_MAX_SHEETS-count || (batch->end && (!count && !batch->row_count)) ||
        (batch->end && read->header->active>=count+batch->row_count)) {
        fail(error,"Too many worksheets, no sheets or invalid active-sheet identity."); return WORKBOOK_CODEC_READ_ERROR;
    }
    size_t array_bytes=batch->row_count*sizeof(HeaderSheet);
    if (!room(read->bytes,retained,array_bytes,read->limit)) { fail(error,"Metadata batch staging exceeds its owned-buffer budget."); return WORKBOOK_CODEC_READ_ERROR; }
    HeaderSheet *next=array_bytes ? dx_alloc(array_bytes) : NULL; size_t staged=array_bytes,done=0;
    for (;done<batch->row_count;done++) if (!sheet_row(read,batch->cells+done*25,count+done,next+done,&staged,retained,error)) {
        for (size_t i=0;i<=done;i++) sheet_free(next+i); free(next); return WORKBOOK_CODEC_READ_ERROR;
    }
    for (size_t i=0;i<done;i++) read->header->sheets[count+i]=next[i];
    free(next); staged-=array_bytes;
    read->header->count+=done; read->header->bytes+=staged; read->bytes+=staged; read->request.first_row+=done;
    if (batch->end) read->request.family=WORKBOOK_CODEC_HEADER_READY;
    return WORKBOOK_CODEC_READ_ACCEPTED;
}
bool workbook_codec_read_finish(WorkbookCodecRead *read,WorkbookCodecHeader **out,char **error) {
    if (!read || !out || !read->header || read->request.family!=WORKBOOK_CODEC_HEADER_READY) return fail(error,"Metadata header family is not complete or was already claimed.");
    workbook_codec_header_free(*out); *out=read->header; read->header=NULL; read->bytes=sizeof(*read); read->request.family=WORKBOOK_CODEC_HEADER_CLAIMED; return true;
}
size_t workbook_codec_read_owned_bytes(const WorkbookCodecRead *read) { return read ? read->bytes : 0; }
size_t workbook_codec_header_owned_bytes(const WorkbookCodecHeader *header) { return header ? header->bytes : 0; }
const WorkbookCodecKey *workbook_codec_header_key(const WorkbookCodecHeader *header) { return header ? &header->key : NULL; }
unsigned workbook_codec_header_version(const WorkbookCodecHeader *header) { return header ? header->version : 0; }
size_t workbook_codec_header_sheet_count(const WorkbookCodecHeader *header) { return header ? header->count : 0; }
size_t workbook_codec_header_active_sheet(const WorkbookCodecHeader *header) { return header ? header->active : SIZE_MAX; }
float workbook_codec_header_user_scale(const WorkbookCodecHeader *header) { return header ? header->user_scale : 0; }
bool workbook_codec_header_sorts_ready(const WorkbookCodecHeader *header) { return header && header->sorts_ready; }
bool workbook_codec_header_preferences_ready(const WorkbookCodecHeader *header) { return header && header->preferences_ready; }
bool workbook_codec_header_objects_ready(const WorkbookCodecHeader *header) { return header && header->objects_ready; }
bool workbook_codec_header_queries_ready(const WorkbookCodecHeader *header) { return header && header->queries_ready; }
bool workbook_codec_header_source_options_ready(const WorkbookCodecHeader *header) { return header && header->source_options_ready; }
bool workbook_codec_header_unique_rows(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,bool *out) {
    if (!out || !header || !header->source_options_ready || sheet>=header->count) return false;
    const HeaderSheet *s=header->sheets+sheet;
    if (!owner) { *out=s->analysis.unique_rows; return true; }
    for (size_t i=0;i<s->object_count;i++) {
        const HeaderObject *object=header->objects+s->object_offset+i;
        if (object->value.id==owner && object->value.kind==OBJECT_TABLE) { *out=header->table_sources_ready && object->source ? object->source->analysis.unique_rows : object->unique_rows; return true; }
    }
    return false;
}
bool workbook_codec_header_query(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,const char **out) {
    if (!out || !header || !header->queries_ready || sheet>=header->count) return false;
    const HeaderSheet *s=header->sheets+sheet;
    if (!owner) { *out=s->analysis.query; return true; }
    for (size_t i=0;i<s->object_count;i++) {
        const HeaderObject *object=header->objects+s->object_offset+i;
        if (object->value.id==owner && object->value.kind==OBJECT_TABLE) { *out=header->table_sources_ready && object->source ? object->source->analysis.query : object->query; return true; }
    }
    return false;
}
size_t workbook_codec_header_object_count(const WorkbookCodecHeader *header,size_t sheet) {
    return header && header->objects_ready && sheet<header->count ? header->sheets[sheet].object_count : 0;
}
bool workbook_codec_header_object_at(const WorkbookCodecHeader *header,size_t sheet,size_t index,WorkbookCodecObjectView *out) {
    if (!out || !header || !header->objects_ready || sheet>=header->count || index>=header->sheets[sheet].object_count) return false;
    *out=header->objects[header->sheets[sheet].object_offset+index].value; return true;
}
bool workbook_codec_header_reduced_motion(const WorkbookCodecHeader *header) { return header && header->reduced_motion; }
bool workbook_codec_header_sheet_hidden(const WorkbookCodecHeader *header,size_t sheet) { return header && sheet<header->count && (header->hidden&(UINT64_C(1)<<sheet))!=0; }
const char *workbook_codec_header_name(const WorkbookCodecHeader *header,size_t sheet) { return header && sheet<header->count ? header->sheets[sheet].name : NULL; }
const char *workbook_codec_header_reference(const WorkbookCodecHeader *header,size_t sheet) { return header && sheet<header->count ? header->sheets[sheet].reference : NULL; }
const Analysis *workbook_codec_header_analysis(const WorkbookCodecHeader *header,size_t sheet) { return header && sheet<header->count ? &header->sheets[sheet].analysis : NULL; }
const SheetView *workbook_codec_header_view(const WorkbookCodecHeader *header,size_t sheet) { return header && sheet<header->count ? &header->sheets[sheet].view : NULL; }

bool workbook_codec_header_table_sources_ready(const WorkbookCodecHeader *header) { return header && header->table_sources_ready; }
bool workbook_codec_header_table_source(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,WorkbookCodecTableSourceView *out) {
    if (!out || !header || !header->table_sources_ready || sheet>=header->count || !owner) return false;
    const HeaderSheet *s=header->sheets+sheet;
    for (size_t i=0;i<s->object_count;i++) {
        const HeaderObject *object=header->objects+s->object_offset+i;
        if (object->value.id==owner && object->value.kind==OBJECT_TABLE && object->source) {
            const Analysis *a=&object->source->analysis;
            *out=(WorkbookCodecTableSourceView){a,object->source->reference,a->kind==SOURCE_DEMO || (a->path && *a->path)}; return true;
        }
    }
    return false;
}

bool workbook_codec_header_pivot_marks_ready(const WorkbookCodecHeader *header) { return header && header->pivot_marks_ready; }
bool workbook_codec_header_pivot_definition(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,const PivotDefinition **out) {
    if (!out || !header || !header->pivot_marks_ready) return false;
    HeaderObject *object=analytical_owner((WorkbookCodecHeader *)header,sheet,owner);
    if (!object) return false;
    *out=object->pivot ? object->pivot : &header->empty_pivot; return true;
}
bool workbook_codec_header_marks_definition(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,const MarkDefinition **out) {
    if (!out || !header || !header->pivot_marks_ready) return false;
    HeaderObject *object=analytical_owner((WorkbookCodecHeader *)header,sheet,owner);
    if (!object) return false;
    *out=object->marks ? object->marks : &header->empty_marks; return true;
}

bool workbook_codec_header_filters_ready(const WorkbookCodecHeader *header) { return header && header->filters_ready; }

bool workbook_codec_header_columns_ready(const WorkbookCodecHeader *header) { return header && header->columns_ready; }
