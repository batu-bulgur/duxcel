#include "workbook_codec_read.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *name,*reference; Analysis analysis; SheetView view; } HeaderSheet;
struct WorkbookCodecHeader {
    WorkbookCodecKey key;
    unsigned version;
    size_t count,active,bytes;
    float user_scale;
    bool sorts_ready;
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
};
static const char meta_sql[]="SELECT version,active_sheet,user_scale FROM _duxcel_workspace.meta LIMIT 2";
static const char sheets25_sql[]="SELECT " DX_WORKBOOK_SHEET_COLUMNS ",canvas_zoom AS canvas_zoom FROM _duxcel_workspace.sheets ORDER BY id LIMIT 65";
static const char sheets24_sql[]="SELECT " DX_WORKBOOK_SHEET_COLUMNS ",1::DOUBLE AS canvas_zoom FROM _duxcel_workspace.sheets ORDER BY id LIMIT 65";
static const char sorts_sql[]="SELECT sheet_id,position,name,descending FROM _duxcel_workspace.sorts ORDER BY sheet_id,position LIMIT 449";
#define SORT_ROWS (DX_MAX_SHEETS*(DX_ANALYSIS_SORTS-1u))
static bool request_family(WorkbookCodecFamily family) {
    return family==WORKBOOK_CODEC_META || family==WORKBOOK_CODEC_SHEETS || family==WORKBOOK_CODEC_SORTS;
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
static bool shape(const WorkbookCodecBatch *batch,const WorkbookCodecTable *table,char **error) {
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
                size_t maximum=(!strcmp(table->table,"sheets") && column==1) || (!strcmp(table->table,"sorts") && column==2) ? 4096u : DX_WORKBOOK_CODEC_READ_TEXT;
                if (atom->value.text.bytes>maximum || (!atom->value.text.data && atom->value.text.bytes) ||
                    !utf8(atom->value.text.data,atom->value.text.bytes)) return fail(error,"Metadata text is oversized, contains NUL or is not valid UTF-8.");
            }
        }
        types++; p+=n; if (*p) p++;
    }
    return column==batch->column_count || fail(error,"Metadata has trailing projected columns.");
}
static void sheet_free(HeaderSheet *sheet) {
    free(sheet->name); free(sheet->reference); analysis_free(&sheet->analysis); *sheet=(HeaderSheet){0};
}
void workbook_codec_header_free(WorkbookCodecHeader *header) {
    if (!header) return;
    for (size_t i=0;i<header->count;i++) sheet_free(header->sheets+i);
    free(header);
}
void workbook_codec_read_free(WorkbookCodecRead *read) {
    if (read) { workbook_codec_header_free(read->header); free(read); }
}
bool workbook_codec_read_begin(WorkbookCodecRead **out,WorkbookCodecKey key,unsigned version,
    WorkbookCodecSheetsLayout layout,size_t limit,WorkbookCodecResolve resolve,void *context,char **error) {
    if (!out || !key_valid(key) || version<12 || version>14 || (layout!=WORKBOOK_CODEC_SHEETS_24 && layout!=WORKBOOK_CODEC_SHEETS_25) ||
        (version==14 && layout!=WORKBOOK_CODEC_SHEETS_25)) return fail(error,"Invalid metadata operation key, version or sheets layout.");
    if (!room(sizeof(WorkbookCodecRead),0,sizeof(WorkbookCodecHeader),limit)) return fail(error,"Metadata cursor exceeds its owned-buffer budget.");
    WorkbookCodecRead *next=dx_alloc(sizeof(*next)); next->header=dx_alloc(sizeof(*next->header));
    next->key=key; next->version=version; next->layout=layout; next->limit=limit; next->resolve=resolve; next->context=context;
    next->bytes=sizeof(*next)+sizeof(*next->header); next->header->bytes=sizeof(*next->header); next->header->key=key; next->header->version=version;
    next->request=(WorkbookCodecRequest){WORKBOOK_CODEC_META,meta_sql,0,3,3,2};
    workbook_codec_read_free(*out); *out=next; return true;
}
const WorkbookCodecRequest *workbook_codec_read_request(const WorkbookCodecRead *read) {
    return read && request_family(read->request.family) ? &read->request : NULL;
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
WorkbookCodecReadStatus workbook_codec_read_feed(WorkbookCodecRead *read,WorkbookCodecKey key,
    WorkbookCodecFamily family,const WorkbookCodecBatch *batch,size_t retained,char **error) {
    if (!read || !same_key(read->key,key)) return WORKBOOK_CODEC_READ_STALE;
    if (!read->header || !request_family(read->request.family) || family!=read->request.family || !batch || batch->first_row!=read->request.first_row) {
        fail(error,"Metadata family or batch order does not match its cursor."); return WORKBOOK_CODEC_READ_ERROR;
    }
    const WorkbookCodecTable *table=workbook_codec_table_find(family==WORKBOOK_CODEC_META ? "meta" : family==WORKBOOK_CODEC_SHEETS ? "sheets" : "sorts");
    if (!shape(batch,table,error)) return WORKBOOK_CODEC_READ_ERROR;
    if (!room(read->bytes,retained,0,read->limit)) { fail(error,"Metadata input exceeds its owned-buffer budget."); return WORKBOOK_CODEC_READ_ERROR; }
    if (family==WORKBOOK_CODEC_SORTS) return sorts_feed(read,batch,retained,error) ? WORKBOOK_CODEC_READ_ACCEPTED : WORKBOOK_CODEC_READ_ERROR;
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
const char *workbook_codec_header_name(const WorkbookCodecHeader *header,size_t sheet) { return header && sheet<header->count ? header->sheets[sheet].name : NULL; }
const char *workbook_codec_header_reference(const WorkbookCodecHeader *header,size_t sheet) { return header && sheet<header->count ? header->sheets[sheet].reference : NULL; }
const Analysis *workbook_codec_header_analysis(const WorkbookCodecHeader *header,size_t sheet) { return header && sheet<header->count ? &header->sheets[sheet].analysis : NULL; }
const SheetView *workbook_codec_header_view(const WorkbookCodecHeader *header,size_t sheet) { return header && sheet<header->count ? &header->sheets[sheet].view : NULL; }
