#ifndef DUXCEL_VERSION_CODEC_H
#define DUXCEL_VERSION_CODEC_H
#include <stddef.h>
#define DX_VERSION_CODEC 1u
#define DX_VERSION_CODEC_TABLES 4u
#define DX_VERSION_CODEC_FIELDS 18u
typedef struct { const char *name,*columns,*types,*definition; } WorkbookVersionCodecTable;
/* Immutable shared metadata inventory. Type codes I/U/B/T/X have the same
   meanings as the core codec. Row NULL/value/closure rules remain admission
   work. No engine, allocation or runtime filesystem dependency. */
const WorkbookVersionCodecTable *workbook_version_codec_table(size_t position);
#endif
