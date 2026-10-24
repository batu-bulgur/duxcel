#ifndef DUXCEL_WORKBOOK_CODEC_H
#define DUXCEL_WORKBOOK_CODEC_H
#include "workbook.h"

/* Exact current core15 metadata, embedded inside the native v16 container. */
#define DX_WORKBOOK_CODEC_COLUMNS 25u
#define DX_WORKBOOK_SHEET_COLUMNS "id,name,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending,active_row,active_col,anchor_row,anchor_col,end_row,end_col,top_row,horizontal,fraction,selection"
typedef struct {
    const char *table,*columns,*types,*nullable_values;
} WorkbookCodecTable;
/* Borrowed immutable descriptors, in the existing native package order. Type
   codes: I INTEGER, U UBIGINT, D DOUBLE, B BOOLEAN, T VARCHAR, X BLOB.
   nullable_values lists supported SQL NULL row values for current core checks;
   it says nothing about physical NOT NULL constraints. Unknown package/index
   or names return NULL/0. No allocation, SQL, I/O or model mutation. */
size_t workbook_codec_table_count(unsigned package);
const WorkbookCodecTable *workbook_codec_table_at(unsigned package,size_t index);
const WorkbookCodecTable *workbook_codec_table_find(const char *name);
bool workbook_codec_nullable(const WorkbookCodecTable *table,const char *column,size_t bytes);
/* Current core writer only. Bounded top-level/current object shapes are checked
   without traversing histories or inferring historical content families. Full
   identity/model/source admission remains with the native Save owner. */
unsigned workbook_codec_min_write_version(const Workbook *book);
/* existing=0 denotes a new destination; existing15 denotes the current core.
   Other versions reject without changing out. Native container16 is separate. */
bool workbook_codec_select_write_version(unsigned minimum,int existing,unsigned *out);
#endif
