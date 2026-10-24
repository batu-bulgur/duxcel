#ifndef DUXCEL_ANALYSIS_COMMAND_H
#define DUXCEL_ANALYSIS_COMMAND_H
#include "cell_command.h"
#include "history_definition.h"
typedef struct {
    Workbook *book;
    Analysis *live; /* Authoritative primary; may be an independent App cache. */
    const uint64_t *source_epoch; /* Borrowed runtime owner, rechecked after reserve. */
    size_t validation_reserved; /* Admitted parser scratch, excluding owned models. */
    bool ready,read_only;
} AnalysisCommandContext;
typedef struct {
    CellEditTarget owner;
    const Analysis *external_authority; /* NULL selects resolved sheet.analysis. */
    uint64_t source_epoch,format_revision,history_last,source_high_water;
} AnalysisTransformTarget;
typedef enum { ANALYSIS_TRANSFORM_APPLIED,ANALYSIS_TRANSFORM_NOOP,ANALYSIS_TRANSFORM_READY,
    ANALYSIS_TRANSFORM_STALE,ANALYSIS_TRANSFORM_INVALID,ANALYSIS_TRANSFORM_PROTECTED,ANALYSIS_TRANSFORM_LIMIT } AnalysisTransformStatus;
typedef struct { AnalysisTransformStatus status; uint64_t before_revision,after_revision; } AnalysisTransformResult;
/* Same-connection primary transforms only: keep kind/path/schema/table and
   source_version exact. Source acquisition/replacement/binding and composites
   have separate owners. DATABASE defaults must already be resolved.
   Trusted owned model; targets bind durable/session/cell/format/history/source
   fences and the actual primary authority, not a stale worksheet save cache. */
bool workbook_prepare_analysis_transform(AnalysisCommandContext *context,size_t sheet,AnalysisTransformTarget *out,char **error);
/* Candidate is independently owned; rejection preserves it, NOOP/APPLIED consume
   it. Authored no-op ignores derived NumberType caches and preserves global Redo.
   Reserve exact before/after before eviction, recheck all fences, move ownership,
   record one EDIT_ANALYSIS/document revision/dirty/branch, then publish once.
   The App owns verified field/schema readiness, runtime epoch advancement, query
   invalidation/jobs, focus and global transient budgets. Callbacks are single-owner
   and nonreentrant as in history_definition.h. No retained App activation or IO. */
AnalysisTransformResult workbook_apply_analysis_transform_recorded(AnalysisCommandContext *context,
    const AnalysisTransformTarget *target,Analysis *owned,const WorkbookHistoryDefinitionRecorder *recorder,char **error);
#endif
