#ifndef DUXCEL_HISTORY_NATIVE_CAPTURE_H
#define DUXCEL_HISTORY_NATIVE_CAPTURE_H
#include "history_profile.h"
#include "version_restore.h"
typedef struct WorkbookHistoryNativeCapture WorkbookHistoryNativeCapture;
typedef struct { WorkbookVersionOwner owner; uint64_t revision; } WorkbookHistoryCapturedPosition;
typedef struct {
    uint64_t physical_bytes; /* Peak new/prior sealed head and retained root extents. */
    size_t additional_bytes; /* Peak new/prior wrappers; existing input graphs stay charged. */
} WorkbookHistoryNativeCaps;
typedef struct { uint64_t head_bytes,root_bytes,prior_physical_bytes,physical_bytes; size_t additional_bytes; } WorkbookHistoryNativeMeasure;
typedef enum { HISTORY_NATIVE_READY,HISTORY_NATIVE_INVALID,HISTORY_NATIVE_ASSOCIATION,
    HISTORY_NATIVE_LIMIT,HISTORY_NATIVE_FAILED,HISTORY_NATIVE_CANCELLED,HISTORY_NATIVE_UNSUPPORTED } WorkbookHistoryNativeStatus;
typedef struct {
    /* True cancels. root==SIZE_MAX denotes captured head, otherwise actual
       CoreSet ordinal. Synchronous worker callback, no mutation/reentry or
       borrowed pointer retention; all owners stay unchanged until READY. */
    bool (*cancelled)(void *context,size_t root,uint64_t bytes);
    void *context;
} WorkbookHistoryNativeControl;
/* Native current-profile handoff, NOT an archive writer or live adoption.
   profile is the actual admitted typed candidate; roots is an actual opaque
   CoreSet admitted by the existing read/take authority (exact chunks/hash/core
   model closure); head is an actual sealed captured/restored current15 owner.
   Match complete index metadata by durable identity, every root descriptor,
   document and the head's actual persisted command revision. Reverify actual
   lease/file/hash bytes before consuming any owner. The eventual byte consumer
   must reverify again: leases do not exclude arbitrary external file mutation.
   No duplicate DB decoder,
   caller coherence flag, root clone, source query or synthetic Undo.
   READY moves all three inputs into one immutable frozen-R capsule. The runtime
   owner fence is in-process authority only, never a serialized durable key.
   Live R+1 can advance independently; publication still needs its separate live
   owner/index fence. A prospective native16/core15 policy does not certify an
   outer file that has not been written; writer/schema activation is excluded.
   Input residency and existing disk reservations remain charged by their real
   owners. additional_bytes bounds the new/prior wrapper coexistence; no
   input payload/model is copied. physical_bytes bounds complete head+root
   extent including prior-output coexistence, conservatively counting shared
   references twice. Independently retained CoreSet references may be shared
   between old/new capsules; exclusive head/profile owners may not. Existing output
   coexists throughout checks. Failure/cancel preserves inputs/output; previous
   output teardown occurs only after all fallible work. Worker-only disposal may
   retire private files, using existing nonrecursive owner cleanup policy. */
WorkbookHistoryNativeStatus workbook_history_native_measure(const WorkbookHistoryProfile *profile,
    const WorkbookVersionCoreSet *roots,const WorkbookCorePayload *head,WorkbookHistoryCapturedPosition captured,
    WorkbookHistoryNativeCaps caps,const WorkbookHistoryNativeCapture *prior,WorkbookHistoryNativeMeasure *out);
WorkbookHistoryNativeStatus workbook_history_native_take(WorkbookHistoryProfile **profile,
    WorkbookVersionCoreSet **roots,WorkbookCorePayload **head,WorkbookHistoryCapturedPosition captured,
    WorkbookHistoryNativeCaps caps,WorkbookHistoryNativeControl control,WorkbookHistoryNativeCapture **out,char **error);
const WorkbookHistoryProfile *workbook_history_native_profile(const WorkbookHistoryNativeCapture *capture);
const WorkbookVersionCoreSet *workbook_history_native_roots(const WorkbookHistoryNativeCapture *capture);
const WorkbookCorePayload *workbook_history_native_head(const WorkbookHistoryNativeCapture *capture);
const WorkbookHistoryCapturedPosition *workbook_history_native_position(const WorkbookHistoryNativeCapture *capture);
void workbook_history_native_free(WorkbookHistoryNativeCapture **capture);
#endif
