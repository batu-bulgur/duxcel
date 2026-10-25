#ifndef DUXCEL_PRIMARY_SOURCE_PENDING_H
#define DUXCEL_PRIMARY_SOURCE_PENDING_H
#include "primary_source_topology.h"
typedef struct PrimarySourcePending PrimarySourcePending;
typedef enum { PRIMARY_PENDING_EMPTY,PRIMARY_PENDING_DRAFT,PRIMARY_PENDING_RESOLVED,
    PRIMARY_PENDING_SOURCE,PRIMARY_PENDING_TOPOLOGY } PrimarySourcePendingStage;
typedef struct { size_t resident,required; } PrimarySourcePendingUsage;
/* Owns one unpublished stage. No Worker, reply/Page, request allocation, UI or
   origin registry ownership. All required grants include this wrapper; resident
   is the already charged live ownership, not a future CREATE/parser peak.
   Pressure preserves the stage. A FAILED producer can be disposed but cannot
   be assumed retryable. Actual publication alone consumes the entire owner. */
PrimarySourceTopologyStatus primary_source_pending_begin_measure(PrimarySourceDraftContext c,const Analysis *source,
    const char *name,PrimarySourcePendingUsage *usage,char **error);
PrimarySourceTopologyStatus primary_source_pending_begin(PrimarySourceDraftContext c,const Analysis *source,
    const char *name,size_t reserved,PrimarySourcePending **out,char **error);
PrimarySourceTopologyStatus primary_source_pending_check(PrimarySourceDraftContext c,const PrimarySourcePending *pending,char **error);
PrimarySourceTopologyStatus primary_source_pending_reply_measure(PrimarySourceDraftContext c,const PrimarySourcePending *pending,
    PrimarySourceDraftReply reply,PrimarySourcePendingUsage *usage,char **error);
PrimarySourceTopologyStatus primary_source_pending_resolve(PrimarySourceDraftContext c,PrimarySourcePending *pending,
    PrimarySourceDraftReply reply,size_t reserved,char **error);
PrimarySourceTopologyStatus primary_source_pending_initial_measure(PrimarySourceDraftContext c,const PrimarySourcePending *pending,
    const SheetSourceView *initial,PrimarySourceInitialSchema schema,PrimarySourcePendingUsage *usage,char **error);
PrimarySourceTopologyStatus primary_source_pending_prepare_initial(PrimarySourceDraftContext c,PrimarySourcePending *pending,
    SheetSourceView **initial,PrimarySourceInitialSchema schema,size_t reserved,char **error);
PrimarySourceTopologyStatus primary_source_pending_topology_measure(PrimarySourceDraftContext c,const PrimarySourcePending *pending,
    PrimarySourcePendingUsage *usage,char **error);
PrimarySourceTopologyStatus primary_source_pending_prepare_topology(PrimarySourceDraftContext c,PrimarySourcePending *pending,
    size_t reserved,char **error);
PrimarySourceTopologyStatus primary_source_pending_apply(PrimarySourceDraftContext c,PrimarySourcePending **pending,
    WorkbookHistoryStream *stream,size_t reserved,PrimarySourcePublication *out,char **error);
PrimarySourcePendingStage primary_source_pending_stage(const PrimarySourcePending *pending);
size_t primary_source_pending_owned(const PrimarySourcePending *pending);
/* Cancellation is terminal and reads no borrowed model/request/source storage. */
void primary_source_pending_free(PrimarySourcePending **pending);
#endif
