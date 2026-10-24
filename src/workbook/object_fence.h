#ifndef DUXCEL_OBJECT_FENCE_H
#define DUXCEL_OBJECT_FENCE_H
#include "cell_command.h"
typedef struct {
    Workbook *book;
    const uint64_t *request_epoch;
    size_t plan_reserved,validation_reserved;
    bool ready,read_only;
} ObjectCommandContext;
typedef struct {
    CellEditTarget owner;
    uint64_t object,request_epoch,format_revision,history_last;
    ObjectStore objects;
    SourceVersions sources;
    SheetObject items[DX_MAX_OBJECTS];
    SourceVersion versions[DX_SOURCE_VERSIONS];
} ObjectCommandTarget;
typedef enum { OBJECT_FENCE_READY,OBJECT_FENCE_STALE,OBJECT_FENCE_INVALID,OBJECT_FENCE_PROTECTED } ObjectFenceStatus;
typedef struct { ObjectFenceStatus status; uint64_t owner,revision; } ObjectFenceResult;
/* Single-owner trusted model. Bounded native pointer/scalar snapshots freeze
   identity, history, source descriptors and all object layout/definition/cache
   fields. They are runtime fences, never serialized or old dereferenceable
   ownership. Store strings/packets are immutable; callbacks cannot reenter.
   Preparation lazily assigns session IDs; all other model/history stays intact.
   Source/execution readiness and semantic ownership belong to each command. */
bool workbook_prepare_object_fence(ObjectCommandContext *context,size_t sheet,uint64_t object,ObjectCommandTarget *out,char **error);
ObjectFenceResult workbook_check_object_fence(ObjectCommandContext *context,const ObjectCommandTarget *target,char **error);
#endif
