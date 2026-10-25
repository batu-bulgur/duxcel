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
/* Identity/readiness fence only, with no grammar or model publication. The
   producer separately admits legacy baseline vs new user-format policy. */
SourceFormatCommandResult workbook_check_source_format_owner(SourceFormatCommandContext *context,
    const SourceFormatCommandTarget *target,char **error);
/* Independently owned complete plans from existing source/format/definition
   planners. A plan appends exactly one complete version at highwater+1, binds
   exactly its previously unbound owner, and changes formatting using that RAW
   dependency. Prior ledger rows and every other definition remain immutable.
   A source-unaware current store and its source-unaware retained states must
   be broad-only. An already-source-aware current store keeps older finite
   manual snapshots in their original mode (for example before real Table
   import); all retained snapshots are validated without relabeling. No pending
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
/* Worksheet-wide spatial fallback in an ALREADY source-aware store. The same
   exact borrowed owner/request/definition/ledger fences apply, without selecting
   a raw source owner or issuing/rebinding any source version. Existing raw-only
   preparation and first/bound record commands retain their eligibility rules.
   The independent plan comes from format_plan_spatial; its current source-aware
   mode and all existing record/RESULT/GUARDED scope semantics remain intact.
   App admission still proves manual-space versus imported-result eligibility.
   A complete effect8 format/source union is reserved before mutation, then
   rechecked and adopted once. Historical snapshots are validated but never
   relabeled. False-to-true manual-first ownership is explicitly unsupported.
   NOOP consumes only the candidate and preserves Redo/clocks at exhaustion;
   every failure preserves the candidate. No retained App writer activation. */
bool workbook_prepare_source_aware_spatial_format(SourceFormatCommandContext *context,size_t sheet,
    SourceFormatCommandTarget *out,char **error);
SourceFormatCommandResult workbook_apply_source_aware_spatial_format_recorded(SourceFormatCommandContext *context,
    const SourceFormatCommandTarget *target,FormatSnapshot *owned,
    const WorkbookHistoryFormatRecorder *recorder,char **error);
/* Current manual-first spatial transition on an imported worksheet. Every
   source-unaware current/retained state must contain broad defaults only;
   already-source-aware retained states keep their exact finite/RAW/RESULT/
   GUARDED semantics, including legitimate Redo. App proves the edited range
   manual before calling. The independent spatial candidate retains false mode
   until atomic publication; ONLY the new current state becomes source-aware.
   No historical flag, immutable ledger row or source binding is rewritten.
   Unsupported ownership profiles return PROTECTED; failure keeps the candidate
   and all before-mode flags unchanged. One effect8 event owns the transition. */
bool workbook_prepare_manual_source_format(SourceFormatCommandContext *context,size_t sheet,
    SourceFormatCommandTarget *out,char **error);
SourceFormatCommandResult workbook_apply_manual_source_format_recorded(SourceFormatCommandContext *context,
    const SourceFormatCommandTarget *target,FormatSnapshot *owned,
    const WorkbookHistoryFormatRecorder *recorder,char **error);
void source_format_command_plan_free(SourceFormatCommandPlan *plan);
#endif
