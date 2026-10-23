#ifndef DUXCEL_WORKBOOK_CODEC_H
#define DUXCEL_WORKBOOK_CODEC_H
#include "workbook.h"

/* Current writer packages only. These are NOT historical introduction versions
   or a strict schema/nullability contract for legacy v1-v13 readers. */
#define DX_WORKBOOK_CODEC_COLUMNS 25u
#define DX_WORKBOOK_SHEET_COLUMNS "id,name,kind,path,source_schema,source_table,filter_column,filter_value,filter_op,group_column,measure_column,aggregate,sort_column,descending,active_row,active_col,anchor_row,anchor_col,end_row,end_col,top_row,horizontal,fraction,selection"
typedef struct {
    const char *table,*columns,*types,*nullable_values;
    unsigned min_package; /* 12 ordinary, 13 owner-only, 14 source-aware, 15 durable identities. */
} WorkbookCodecTable;
/* Borrowed immutable descriptors, in the existing native package order. Type
   codes: I INTEGER, U UBIGINT, D DOUBLE, B BOOLEAN, T VARCHAR, X BLOB.
   nullable_values lists supported SQL NULL row values for strict v14 checks;
   it says nothing about physical NOT NULL constraints. Unknown package/index
   or names return NULL/0. No allocation, SQL, I/O or model mutation. */
size_t workbook_codec_table_count(unsigned package);
const WorkbookCodecTable *workbook_codec_table_at(unsigned package,size_t index);
const WorkbookCodecTable *workbook_codec_table_find(const char *name);
bool workbook_codec_nullable(const WorkbookCodecTable *table,const char *column,size_t bytes);
/* True only for known v13-v15 names a legacy version must not silently lose.
   Unknown names stay unknown: this is not the strict v14 inventory validator. */
bool workbook_codec_reserved_before(const char *name,unsigned version);

/* Representation policy for an existing bounded, prevalidated owned model.
   Does not validate packets, bindings, histories or source provenance, and does
   not serialize Undo/Redo. Retained source bindings still require v14; malformed
   history bounds conservatively select v14 for native validation to reject.
   Invalid top-level/current-array shapes return 0 without dereferencing them. */
unsigned workbook_codec_content_version(const Workbook *book); /* Legacy 12/13/14 content families. */
/* Current writer always includes durable identity metadata (v15); invalid
   top-level/current-array shapes return0. Identity validity is checked by Save. */
unsigned workbook_codec_min_write_version(const Workbook *book);
/* existing=0 denotes a new destination; 1..15 denotes a validated archive.
   Never downgrades an archive. Invalid inputs leave out unchanged. */
bool workbook_codec_select_write_version(unsigned minimum,int existing,unsigned *out);
#endif
