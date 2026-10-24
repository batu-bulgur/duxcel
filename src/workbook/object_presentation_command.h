#ifndef DUXCEL_OBJECT_PRESENTATION_COMMAND_H
#define DUXCEL_OBJECT_PRESENTATION_COMMAND_H
#include "object_fence.h"
#include "history_definition.h"
typedef enum { OBJECT_PRESENTATION_MARK,OBJECT_PRESENTATION_MEASURE,OBJECT_PRESENTATION_BINDINGS } ObjectPresentationOperation;
typedef struct { ObjectPresentationOperation operation; size_t value; const MarkDefinition *marks; } ObjectPresentationCommand;
typedef struct { size_t owned,validation_scratch; } ObjectPresentationPlanInfo;
typedef enum { OBJECT_PRESENTATION_APPLIED,OBJECT_PRESENTATION_NOOP,OBJECT_PRESENTATION_READY,OBJECT_PRESENTATION_STALE,
    OBJECT_PRESENTATION_INVALID,OBJECT_PRESENTATION_PROTECTED,OBJECT_PRESENTATION_LIMIT } ObjectPresentationStatus;
typedef struct { ObjectPresentationStatus status; uint64_t object,before_revision,after_revision; } ObjectPresentationResult;
/* Allocation-free complete-graph measurement. No-op/failure leaves info intact.
   Scatter/schema availability belongs to App, not this Page-independent owner. */
ObjectPresentationStatus objects_presentation_measure(const ObjectStore *store,uint64_t object,ObjectPresentationCommand command,
    ObjectPresentationPlanInfo *info,char **error);
/* Borrowed command and full runtime fence; one owned after-graph, one action.
   Reserve the optional immutable event before eviction and recheck after reserve.
   plan_reserved covers after-graph + max before/after parser scratch. Live model,
   history and event validation reservations are separate. No query/source/
   allocator/layout changes. Measure changes alone advance the title counter; bindings advance Marks.
   Binding text stays borrowed and is deep-copied once in the after-graph.
   No IO or retained-history App activation; App owns availability and caches. */
ObjectPresentationResult workbook_apply_object_presentation_recorded(ObjectCommandContext *context,const ObjectCommandTarget *target,
    ObjectPresentationCommand command,const WorkbookHistoryDefinitionRecorder *recorder,char **error);
#endif
