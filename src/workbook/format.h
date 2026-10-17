#ifndef DUXCEL_FORMAT_H
#define DUXCEL_FORMAT_H
#include "../common.h"
#define DX_FORMAT_PATCHES 512
#define DX_FORMAT_HISTORY 32
typedef enum { FORMAT_AUTO,FORMAT_LEFT,FORMAT_CENTER,FORMAT_RIGHT } FormatAlignment;
typedef enum { FORMAT_GENERAL,FORMAT_ACCOUNTING,FORMAT_NUMBER } FormatNumber;
enum { FORMAT_BOLD=1,FORMAT_ITALIC=2,FORMAT_UNDERLINE=4,FORMAT_ALIGN=8,
    FORMAT_COLOR=16,FORMAT_SIZE=32,FORMAT_NUMERIC=64,FORMAT_DECIMALS=128,FORMAT_ALL=255 };
typedef struct {
    unsigned char flags,align,color,size,number,decimals;
    unsigned char specified; /* Derived by format_at; not stored as a property. */
} CellFormat;
typedef struct { uint64_t r0,r1; size_t c0,c1; } FormatRange;
typedef struct { FormatRange range; unsigned mask; CellFormat value; } FormatPatch;
typedef struct { FormatPatch *items; size_t count; } FormatSnapshot;
typedef struct {
    FormatPatch *items; size_t count; uint64_t revision;
    FormatSnapshot undo[DX_FORMAT_HISTORY],redo[DX_FORMAT_HISTORY]; size_t undo_count,redo_count;
} FormatStore;
CellFormat format_default(void);
CellFormat format_at(const FormatStore *store,uint64_t row,size_t column);
bool format_valid(const FormatStore *store,char **error);
bool format_apply(FormatStore *store,FormatRange range,unsigned mask,CellFormat value,char **error);
void format_free(FormatStore *store);
void format_copy(FormatStore *to,const FormatStore *from);
bool format_history(FormatStore *store,bool redo);
void format_trim_history(FormatStore *store,bool redo,size_t keep);
/* Plan before editing cells/objects, then atomically commit the owned snapshot.
   Unchanged plans are freed without adding an independent history entry. */
bool format_plan_move(const FormatStore *store,FormatRange range,int64_t dr,int dc,FormatSnapshot *out,char **error);
bool format_plan_axis(const FormatStore *store,bool rows,uint64_t at,uint64_t count,bool insert,FormatSnapshot *out,char **error);
bool format_commit(FormatStore *store,FormatSnapshot *owned);
void format_snapshot_free(FormatSnapshot *snapshot);
#endif
