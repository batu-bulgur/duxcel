#ifndef DUXCEL_OBJECT_BINDING_COMMAND_H
#define DUXCEL_OBJECT_BINDING_COMMAND_H
#include "object_fence.h"
#include "history_definition.h"
typedef struct { ObjectCommandContext command; const Analysis *primary; const uint64_t *primary_epoch; } ObjectBindingContext;
typedef struct { ObjectCommandTarget object; const Analysis *authority; Analysis primary; uint64_t primary_epoch; } ObjectBindingTarget;
typedef struct { size_t owned,validation_scratch,primary_owned; } ObjectBindingPlanInfo;
typedef enum { OBJECT_BINDING_APPLIED,OBJECT_BINDING_NOOP,OBJECT_BINDING_READY,OBJECT_BINDING_STALE,
    OBJECT_BINDING_INVALID,OBJECT_BINDING_PROTECTED,OBJECT_BINDING_LIMIT } ObjectBindingStatus;
typedef struct { ObjectBindingStatus status; uint64_t object,before_revision,after_revision; } ObjectBindingResult;
/* Immediate single-owner fence, including the actual primary authority. Strings
   and packets remain trusted immutable ownership, never serialized native state. */
bool workbook_prepare_object_binding(ObjectBindingContext *context,size_t sheet,uint64_t object,ObjectBindingTarget *out,char **error);
/* Allocation-free complete graph/primary shape measurement. source0 means the
   unchanged primary; nonzero must identify an existing Table. No-op/failure
   preserves info. primary_owned measures the borrowed live authority for outer
   accounting, not an additional controller allocation. Field/schema compatibility stays with App/worker feedback. */
ObjectBindingStatus objects_binding_measure(const ObjectStore *store,uint64_t object,uint64_t source,const Analysis *primary,ObjectBindingPlanInfo *info,char **error);
/* One owned graph, one query/document revision and action, preserving object
   identity/layout/name/Marks/measure/allocator and independent source definitions.
   Validate primary grammar/dependencies within scratch reservation. The unchanged
   primary and its retained data remain parent/root obligations, not a falsely
   changed event family. Reserve complete object assets/source union before
   eviction; recheck both owners and publish once. No IO or retained App writer. */
ObjectBindingResult workbook_apply_object_binding_recorded(ObjectBindingContext *context,const ObjectBindingTarget *target,
    uint64_t source,const WorkbookHistoryDefinitionRecorder *recorder,char **error);
#endif
