#ifndef DUXCEL_SOURCE_FORMAT_BASELINE_H
#define DUXCEL_SOURCE_FORMAT_BASELINE_H
#include "source_format_command.h"
typedef struct SourceFormatBaselinePlan SourceFormatBaselinePlan;
typedef struct {
    SourceFormatCommandContext command;
    const uint64_t *origin_epoch; /* Working baseline admission epoch, not Saved. */
    bool origin_open; /* Explicit owner proof: no immutable working origin sealed. */
} SourceFormatBaselineContext;
typedef struct { size_t owned,validation_scratch; uint64_t version; size_t model_owned; } SourceFormatBaselineInfo;
typedef struct { const SourceVersions *sources; const FormatStore *formats; } SourceFormatBaselineView;
/* Read-only prospective replacement for the outer App governor. Borrowed only
   until the opaque plan is adopted/freed; no caller may edit these graphs. */
SourceFormatBaselineView source_format_baseline_view(const SourceFormatBaselinePlan *plan);
/* Conservative NEW plan-owned ceiling, including opaque preimage/whole current,
   Undo/Redo formats and full ledger. Parser scratch/input/live plans are separate.
   The App must admit this before any cloning; actual owned is returned on success. */
size_t source_format_baseline_reservation(void);
/* Verified loaded-primary compatibility enrichment only. The caller proves
   loaded origin, actual Page/file/schema/definition proof and outer IO budgets.
   Closed/changed working-origin admission rejects; Saved roots and independent
   recovery snapshots are not rewritten or inferred to be a mutable origin.
   Reuse existing whole-store guard and immutable ledger planners under a bounded
   reservation. All before/after current/Undo/Redo states remain owned/unaltered
   until complete closure. out must be NULL; failure preserves out/info/authored model. Preparation
   may lazily assign the same transient session IDs as other C commands.
   The resulting opaque plan cannot be retargeted or edited by a caller. */
SourceFormatCommandStatus workbook_prepare_source_format_baseline(SourceFormatBaselineContext *context,size_t sheet,
    const SourceVersion *verified,const SourcePacket *result,size_t reserved,
    SourceFormatBaselinePlan **out,SourceFormatBaselineInfo *info,char **error);
/* Recheck original model/readiness/origin epoch, then transfer all three
   constituents without allocation. Consumes only APPLIED plans; rejection
   retains an independent plan for cancellation. This is NOT a user action:
   preserve every action/order/Undo/Redo, cell/value, document command revision,
   history branch and Saved asset. Advance only format invalidation and issued
   source high-water, bind the primary, and mark the working model dirty.
   The App updates verified caches and invalidates its loaded-origin stamp after
   success. No recorder, IO, retained-history activation or origin sealing. */
SourceFormatCommandStatus workbook_apply_source_format_baseline(SourceFormatBaselineContext *context,
    SourceFormatBaselinePlan **owned,char **error);
void source_format_baseline_plan_free(SourceFormatBaselinePlan **owned);
#endif
