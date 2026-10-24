#ifndef DUXCEL_SOURCE_FORMAT_COMMAND_H
#define DUXCEL_SOURCE_FORMAT_COMMAND_H
#include "cell_command.h"
#include "history_definition.h"
#include "history_format.h"
typedef struct {
    Workbook *book;
    Analysis *primary; /* Actual authority; an independent App cache is allowed. */
    const uint64_t *request_epoch;
    size_t validation_reserved;
    bool ready,read_only;
} SourceFormatCommandContext;
typedef struct {
    CellEditTarget owner;
    const Analysis *authority;
    uint64_t object,request_epoch,format_revision,history_last;
    Analysis primary,cached;
    ObjectStore objects;
    SourceVersions sources;
    SheetObject items[DX_MAX_OBJECTS];
    SourceVersion versions[DX_SOURCE_VERSIONS];
} SourceFormatCommandTarget;
typedef struct {
    SourceVersions sources;
    Analysis primary; /* object0 only; otherwise empty. */
    ObjectSnapshot objects; /* Existing Table owner only; otherwise empty. */
    FormatSnapshot formats;
} SourceFormatCommandPlan;
typedef enum { SOURCE_FORMAT_COMMAND_APPLIED,SOURCE_FORMAT_COMMAND_NOOP,SOURCE_FORMAT_COMMAND_READY,SOURCE_FORMAT_COMMAND_STALE,
    SOURCE_FORMAT_COMMAND_INVALID,SOURCE_FORMAT_COMMAND_PROTECTED,SOURCE_FORMAT_COMMAND_LIMIT } SourceFormatCommandStatus;
typedef struct { SourceFormatCommandStatus status; uint64_t before_revision,after_revision; } SourceFormatCommandResult;
/* Verified native raw owner: object0 primary or an existing Table.
   Runtime borrowed fences cover request/document/sheet/cells/formats/history,
   full object descriptors and immutable source rows. Text/packets are trusted
   immutable ownership; these native snapshots never become retained assets.
   Preparation assigns session IDs only. Actual Page/content/schema/collector
   verification, origin completion and all transient budgets stay with the App. */
bool workbook_prepare_source_format(SourceFormatCommandContext *context,size_t sheet,uint64_t object,
    SourceFormatCommandTarget *out,char **error);
/* Independently owned complete plans from existing source/format/definition
   planners. A plan appends exactly one complete version at highwater+1, binds
   exactly its previously unbound owner, and changes formatting using that RAW
   dependency. Prior ledger rows and every other definition remain immutable.
   Legacy current/Undo/Redo formats must be broad-only or already explicitly
   source-aware; finite legacy data is never relabeled as manual. No pending
   completion/replacement/Refresh/manual migration is hidden here.
   Closed event9/12 validation is reused. Reserve before any constituent adoption
   or history eviction; recheck fences and plan descriptors after reservation;
   adopt ledger/binding/formats plus ONE action/clock/dirty/branch, then publish.
   Rejection preserves the plan. APPLIED consumes it. Callbacks obey the
   single-owner nonreentrant recorder contract. No IO or retained App activation.
   Model/plan/event/job/global coexistence must be admitted by the outer owner. */
/* Allocation-free retained/current/candidate grammar scratch measurement. Admit
   this peak through the App governor before semantic validation; outputs remain
   unchanged on failure. Existing independent plan ownership is separate. */
bool workbook_source_format_scratch(const Worksheet *sheet,const Analysis *primary,
    const SourceFormatCommandPlan *plan,size_t *out,char **error);
SourceFormatCommandResult workbook_apply_source_format_recorded(SourceFormatCommandContext *context,
    const SourceFormatCommandTarget *target,SourceFormatCommandPlan *owned,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error);
/* Already-bound finite source formatting uses the SAME captured owner fences
   and immutable live ledger. Only formats change: reserve a closed effect8
   FormatDelta/source union, recheck, then adopt one format/action/clock/branch.
   NOOP consumes the independent plan with no reserve, clock, dirty or Redo
   change, including exhausted allocation counters. No new source issuance,
   pending completion, definition mutation or App retained writer activation. */
SourceFormatCommandResult workbook_apply_bound_source_format_recorded(SourceFormatCommandContext *context,
    const SourceFormatCommandTarget *target,FormatSnapshot *owned,
    const WorkbookHistoryFormatRecorder *recorder,char **error);
void source_format_command_plan_free(SourceFormatCommandPlan *plan);
#endif
