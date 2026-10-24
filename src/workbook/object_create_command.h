#ifndef DUXCEL_OBJECT_CREATE_COMMAND_H
#define DUXCEL_OBJECT_CREATE_COMMAND_H
#include "object_fence.h"
#include "history_definition.h"
/* Admission borrows the actual prospective format history plus incoming object
   history charge. No mutation/reentry; false preserves every input graph. */
typedef bool (*ObjectCreateAdmission)(void *context,const FormatStore *prospective,size_t adopted,size_t scratch,size_t retained,char **error);
typedef struct {
    ObjectCommandContext command;
    const Analysis *primary;
    const uint64_t *primary_epoch,*source_rows;
    const size_t *source_columns;
    const bool *source_count_pending;
    bool manual_formats;
    ObjectCreateAdmission admit;
    void *admission_context;
} ObjectCreateContext;
typedef struct {
    ObjectCommandTarget owner;
    const Analysis *authority;
    Analysis primary;
    uint64_t primary_epoch,source_rows;
    size_t source_columns;
    bool source_count_pending,manual_formats;
    FormatStore formats;
    FormatPatch patches[DX_FORMAT_PATCHES];
} ObjectCreateTarget;
typedef struct {
    ObjectKind kind;
    uint64_t row,source_id;
    size_t column;
    const char *name;
    const PivotDefinition *pivot;
    const Analysis *source;
} ObjectCreateCommand;
typedef struct { size_t owned,validation_scratch,primary_owned,proposal_owned; } ObjectCreatePlanInfo;
typedef enum { OBJECT_CREATE_APPLIED,OBJECT_CREATE_READY,OBJECT_CREATE_STALE,
    OBJECT_CREATE_INVALID,OBJECT_CREATE_PROTECTED,OBJECT_CREATE_LIMIT } ObjectCreateStatus;
typedef struct { ObjectCreateStatus status; uint64_t object,before_revision,after_revision; } ObjectCreateResult;
/* Borrow immutable model/proposal ownership. Capture sheet-level owner, live
   primary authority/epoch and source extent; pending count protects its lane. */
bool workbook_prepare_object_create(ObjectCreateContext *context,size_t sheet,ObjectCreateTarget *out,char **error);
/* Allocation-free complete graph/geometry and primary shape projection. Default
   Pivot/Table extent is 5x4, Chart 12x6. Tables and selected analytical sources
   must have resolved relation identity. Failure preserves info. Caller staging
   and borrowed primary footprint are separate App accounting obligations. */
ObjectCreateStatus objects_create_measure(const ObjectStore *store,ObjectCreateCommand command,const Analysis *primary,
    const CellStore *cells,uint64_t source_rows,size_t source_columns,ObjectCreatePlanInfo *info,char **error);
/* Context-aware complete reservation, including a mode-only Format constituent
   for a first manual-only Table import. Whole source-free history eligibility is
   proved after scratch admission and before cloning/reserving/eviction. */
ObjectCreateStatus workbook_object_create_measure(const ObjectCreateContext *context,size_t sheet,ObjectCreateCommand command,ObjectCreatePlanInfo *info,char **error);
/* Validate complete before/after grammar, source union and geometry BEFORE any
   history eviction; reserve optional DXDE, recheck all owners, transfer once.
   New identity advances only at adoption and remains outside Undo. Creation is
   always an action: cancelled chooser/default worker enrichment are separate
   owners, never fabricated no-op events. App owns schema readiness, caches/jobs,
   native admission and source-version composites. Manual-mode migration is an
   exact Objects+Format action; immutable pre-import histories are preserved. No IO or retained App writer. */
ObjectCreateResult workbook_apply_object_create_recorded(ObjectCreateContext *context,const ObjectCreateTarget *target,
    ObjectCreateCommand command,const WorkbookHistoryDefinitionRecorder *recorder,char **error);
#endif
