#ifndef DUXCEL_WORKBOOK_SAVE_FENCE_H
#define DUXCEL_WORKBOOK_SAVE_FENCE_H
#include "workbook.h"
/* Compare current registered model state with its immutable Save snapshot.
   Caller also fences chronological action order. History, paths, dirty flags,
   session fences and derived query types are not persisted content. No IO,
   allocation or hashing; borrowed inputs must remain stable during the call. */
bool workbook_save_matches(const Workbook *current,const Workbook *snapshot);
#endif
