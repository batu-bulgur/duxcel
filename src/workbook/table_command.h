#ifndef DUXCEL_TABLE_COMMAND_H
#define DUXCEL_TABLE_COMMAND_H
#include "cell_command.h"
#include "object_source_plan.h"
#include "history_definition.h"
typedef struct {
    Workbook *book;
    const uint64_t *request_epoch;
    size_t plan_reserved,validation_reserved;
    bool ready,read_only; /* Owned definition readiness, independent of old Page success. */
} TableCommandContext;
typedef struct {
    CellEditTarget owner;
    uint64_t object,request_epoch,format_revision,history_last;
    ObjectStore objects;
    SourceVersions sources;
    SheetObject items[DX_MAX_OBJECTS];
    SourceVersion versions[DX_SOURCE_VERSIONS];
} TableTransformTarget;
typedef enum { TABLE_TRANSFORM_APPLIED,TABLE_TRANSFORM_NOOP,TABLE_TRANSFORM_READY,
    TABLE_TRANSFORM_STALE,TABLE_TRANSFORM_INVALID,TABLE_TRANSFORM_PROTECTED,TABLE_TRANSFORM_LIMIT } TableTransformStatus;
typedef struct {
    TableTransformStatus status;
    uint64_t owner,before_revision,after_revision;
    bool result_changed;
} TableTransformResult;
/* Same-connection Table definitions only. Runtime fences borrow native pointer/
   scalar representations, never serialized or dereferenced as old ownership.
   They cover all object layouts/definitions/caches and source ledger headers,
   including non-user extent/default-relation completion. Owned strings/packets
   in trusted stores are immutable; callbacks cannot mutate/reenter the model.
   Source acquisition/rebinding/format composites retain their separate owner. */
bool workbook_prepare_table_transform(TableCommandContext *context,size_t sheet,uint64_t object,
    TableTransformTarget *out,char **error);
/* Borrowed proposal; rejection and success leave its ownership with the caller.
   Build one independent plan under plan_reserved (graph+parser scratch), close
   source dependencies, reserve the complete before/after event BEFORE eviction,
   recheck, then move the graph and publish one action/document clock/dirty branch.
   validation_reserved is separate remaining scratch for event preparation, after
   model/event/global accounting. App owns request/cache invalidation and focus.
   NULL recorder is session-only; no durable receipt or App history activation. */
TableTransformResult workbook_apply_table_transform_recorded(TableCommandContext *context,
    const TableTransformTarget *target,const Analysis *proposal,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error);
#endif
