#ifndef DUXCEL_SHEET_HISTORY_APPLICABILITY_H
#define DUXCEL_SHEET_HISTORY_APPLICABILITY_H
#include "sheet_history_payload.h"
typedef struct WorkbookSheetApplicability WorkbookSheetApplicability;
typedef struct { size_t owned,prior_owned,validation_scratch,staging; } WorkbookSheetApplicabilityMeasure;
typedef struct { SheetLifecycleState target; SheetPresenceRefinement presence; } WorkbookSheetApplicabilityResult;
typedef enum { SHEET_APPLICABILITY_OK,SHEET_APPLICABILITY_INVALID,SHEET_APPLICABILITY_MISMATCH,SHEET_APPLICABILITY_LIMIT } WorkbookSheetApplicabilityStatus;
/* Read-only authored applicability for ONE already decoded family5 payload.
   Payload/frame/path association remains the slot owner's proof, not inferred
   here. Payload stays borrowed and immutable until this owner is freed; topology
   owns one independent decoded DXSP for exact authored/refinement comparison.
   Measure includes prior-owner coexistence and one validation scratch grant.
   Inputs/live graphs/payload owner are separate charges. Failure preserves out.
   No traversal ordering, authority lookup, fake session/action, mutation, source
   readiness/activation, IO or publication. Missing families are not admitted.
   A caller providing actual presence still proves its true live/detached owner
   and source-cache handoff externally. Same-key immutable enrichment is checked
   explicitly; captured pending observations remain inactive, never readiness. */
WorkbookSheetApplicabilityStatus workbook_sheet_applicability_measure(const WorkbookSheetPayload *payload,const WorkbookSheetApplicability *prior,WorkbookSheetApplicabilityMeasure *out);
WorkbookSheetApplicabilityStatus workbook_sheet_applicability_prepare(const WorkbookSheetPayload *payload,size_t reserved,WorkbookSheetApplicability **out);
/* Current catalog must be fully readable. Actual presence is required only for
   topology, including the caller's genuine detached owner on the absent side.
   Its document/high-water must equal the current catalog; captured bounds are
   obligations and never replace newer current counters. Success target borrows
   payload arrays/names and preserves visible active navigation and high-water.
   All inputs and output storage are disjoint; invalid/mismatch preserves out. */
WorkbookSheetApplicabilityStatus workbook_sheet_applicability_check(const WorkbookSheetApplicability *plan,SheetLifecycleState current,const SheetPresenceView *actual,bool redo,WorkbookSheetApplicabilityResult *out);
/* Borrow the existing closed topology asset for scalar observation/reporting.
   NULL for metadata/NULL plan; no clone, lease, readiness or transfer authority.
   It remains immutable and readable only until plan replacement/free. The caller
   reserves observation scratch separately while this plan remains resident. */
const SheetPresenceAsset *workbook_sheet_applicability_presence(const WorkbookSheetApplicability *plan);
void workbook_sheet_applicability_free(WorkbookSheetApplicability **plan);
#endif
