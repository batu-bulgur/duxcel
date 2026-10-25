#include "primary_source_pending.h"
#include <stdlib.h>
struct PrimarySourcePending {
    PrimarySourceDraft *draft;
    PrimarySourcePublishPlan *source;
    PrimarySourceTopologyPlan *topology;
};
static PrimarySourceTopologyStatus draft_status(PrimarySourceDraftStatus s) {
    switch (s) {
    case PRIMARY_SOURCE_READY: case PRIMARY_SOURCE_RESOLVED: return PRIMARY_TOPOLOGY_READY;
    case PRIMARY_SOURCE_STALE: return PRIMARY_TOPOLOGY_STALE;
    case PRIMARY_SOURCE_PROTECTED: return PRIMARY_TOPOLOGY_PROTECTED;
    case PRIMARY_SOURCE_LIMIT: return PRIMARY_TOPOLOGY_LIMIT;
    case PRIMARY_SOURCE_FAILED: return PRIMARY_TOPOLOGY_FAILED;
    default: return PRIMARY_TOPOLOGY_INVALID;
    }
}
static PrimarySourceTopologyStatus publish_status(PrimarySourcePublishStatus s) {
    switch (s) {
    case PRIMARY_PUBLISH_READY: return PRIMARY_TOPOLOGY_READY;
    case PRIMARY_PUBLISH_STALE: return PRIMARY_TOPOLOGY_STALE;
    case PRIMARY_PUBLISH_PROTECTED: return PRIMARY_TOPOLOGY_PROTECTED;
    case PRIMARY_PUBLISH_LIMIT: return PRIMARY_TOPOLOGY_LIMIT;
    case PRIMARY_PUBLISH_FAILED: return PRIMARY_TOPOLOGY_FAILED;
    default: return PRIMARY_TOPOLOGY_INVALID;
    }
}
static bool sum(size_t *n,size_t extra) { if (extra==SIZE_MAX || *n>SIZE_MAX-extra) return false; *n+=extra; return true; }
PrimarySourcePendingStage primary_source_pending_stage(const PrimarySourcePending *p) {
    return !p ? PRIMARY_PENDING_EMPTY : p->topology ? PRIMARY_PENDING_TOPOLOGY : p->source ? PRIMARY_PENDING_SOURCE :
        p->draft ? (primary_source_draft_source(p->draft) ? PRIMARY_PENDING_RESOLVED : PRIMARY_PENDING_DRAFT) : PRIMARY_PENDING_EMPTY;
}
size_t primary_source_pending_owned(const PrimarySourcePending *p) {
    if (!p) return 0;
    size_t n=sizeof(*p);
    if (!sum(&n,primary_source_draft_owned(p->draft)) || !sum(&n,primary_source_publish_owned(p->source)) || !sum(&n,primary_source_topology_owned(p->topology))) return SIZE_MAX;
    return n;
}
static PrimarySourceTopologyStatus usage(const PrimarySourcePending *p,size_t required,PrimarySourcePendingUsage *out) {
    if (!out) return PRIMARY_TOPOLOGY_INVALID;
    if (!sum(&required,sizeof(PrimarySourcePending))) return PRIMARY_TOPOLOGY_LIMIT;
    size_t resident=primary_source_pending_owned(p); if (resident==SIZE_MAX) return PRIMARY_TOPOLOGY_LIMIT;
    *out=(PrimarySourcePendingUsage){resident,required}; return PRIMARY_TOPOLOGY_READY;
}
PrimarySourceTopologyStatus primary_source_pending_begin_measure(PrimarySourceDraftContext c,const Analysis *source,const char *name,PrimarySourcePendingUsage *out,char **error) {
    PrimarySourceDraftInfo info; PrimarySourceTopologyStatus s=draft_status(primary_source_draft_measure(c,source,name,&info,error));
    if (s!=PRIMARY_TOPOLOGY_READY) return s;
    size_t n=info.owned; if (!sum(&n,info.validation_scratch) || !sum(&n,info.naming_scratch)) return PRIMARY_TOPOLOGY_LIMIT;
    return usage(NULL,n,out);
}
PrimarySourceTopologyStatus primary_source_pending_begin(PrimarySourceDraftContext c,const Analysis *source,const char *name,size_t reserved,PrimarySourcePending **out,char **error) {
    if (!out || *out) return PRIMARY_TOPOLOGY_INVALID;
    PrimarySourcePendingUsage info; PrimarySourceTopologyStatus s=primary_source_pending_begin_measure(c,source,name,&info,error);
    if (s!=PRIMARY_TOPOLOGY_READY) return s;
    if (reserved<info.required) return PRIMARY_TOPOLOGY_LIMIT;
    PrimarySourceDraft *draft=NULL; s=draft_status(primary_source_draft_begin(c,source,name,reserved-sizeof(PrimarySourcePending),&draft,error));
    if (s!=PRIMARY_TOPOLOGY_READY) return s;
    PrimarySourcePending *p=dx_alloc(sizeof(*p)); *p=(PrimarySourcePending){.draft=draft}; *out=p; return PRIMARY_TOPOLOGY_READY;
}
PrimarySourceTopologyStatus primary_source_pending_check(PrimarySourceDraftContext c,const PrimarySourcePending *p,char **error) {
    if (!p) return PRIMARY_TOPOLOGY_INVALID;
    if (p->topology) return primary_source_topology_check(c,p->topology,error);
    if (p->source) return publish_status(primary_source_publish_check(c,p->source,error));
    return draft_status(primary_source_draft_check(c,p->draft,error));
}
PrimarySourceTopologyStatus primary_source_pending_reply_measure(PrimarySourceDraftContext c,const PrimarySourcePending *p,PrimarySourceDraftReply reply,PrimarySourcePendingUsage *out,char **error) {
    if (primary_source_pending_stage(p)!=PRIMARY_PENDING_DRAFT) return PRIMARY_TOPOLOGY_INVALID;
    PrimarySourceDraftInfo info; PrimarySourceTopologyStatus s=draft_status(primary_source_draft_reply_measure(c,p->draft,reply,&info,error));
    if (s!=PRIMARY_TOPOLOGY_READY) return s;
    size_t n=primary_source_draft_owned(p->draft); if (!sum(&n,info.owned) || !sum(&n,info.validation_scratch)) return PRIMARY_TOPOLOGY_LIMIT;
    return usage(p,n,out);
}
PrimarySourceTopologyStatus primary_source_pending_resolve(PrimarySourceDraftContext c,PrimarySourcePending *p,PrimarySourceDraftReply reply,size_t reserved,char **error) {
    PrimarySourcePendingUsage info; PrimarySourceTopologyStatus s=primary_source_pending_reply_measure(c,p,reply,&info,error);
    if (s!=PRIMARY_TOPOLOGY_READY) return s;
    if (reserved<info.required) return PRIMARY_TOPOLOGY_LIMIT;
    return draft_status(primary_source_draft_resolve(c,p->draft,reply,reserved-info.resident,error));
}
PrimarySourceTopologyStatus primary_source_pending_initial_measure(PrimarySourceDraftContext c,const PrimarySourcePending *p,const SheetSourceView *initial,PrimarySourceInitialSchema schema,PrimarySourcePendingUsage *out,char **error) {
    if (primary_source_pending_stage(p)!=PRIMARY_PENDING_RESOLVED) return PRIMARY_TOPOLOGY_INVALID;
    PrimarySourcePublishInfo info; PrimarySourceTopologyStatus s=publish_status(primary_source_publish_measure_initial(c,p->draft,initial,schema,&info,error));
    return s==PRIMARY_TOPOLOGY_READY ? usage(p,info.coexisting,out) : s;
}
PrimarySourceTopologyStatus primary_source_pending_prepare_initial(PrimarySourceDraftContext c,PrimarySourcePending *p,SheetSourceView **initial,PrimarySourceInitialSchema schema,size_t reserved,char **error) {
    if (!initial) return PRIMARY_TOPOLOGY_INVALID;
    PrimarySourcePendingUsage info; PrimarySourceTopologyStatus s=primary_source_pending_initial_measure(c,p,*initial,schema,&info,error);
    if (s!=PRIMARY_TOPOLOGY_READY) return s;
    if (reserved<info.required) return PRIMARY_TOPOLOGY_LIMIT;
    return publish_status(primary_source_publish_prepare_initial(c,&p->draft,initial,schema,reserved-sizeof(*p),&p->source,error));
}
PrimarySourceTopologyStatus primary_source_pending_topology_measure(PrimarySourceDraftContext c,const PrimarySourcePending *p,PrimarySourcePendingUsage *out,char **error) {
    if (primary_source_pending_stage(p)==PRIMARY_PENDING_TOPOLOGY) {
        PrimarySourceTopologyStatus s=primary_source_topology_check(c,p->topology,error);
        return s==PRIMARY_TOPOLOGY_READY ? usage(p,primary_source_topology_usage(p->topology).total,out) : s;
    }
    if (primary_source_pending_stage(p)!=PRIMARY_PENDING_SOURCE) return PRIMARY_TOPOLOGY_INVALID;
    PrimarySourceTopologyUsage info; PrimarySourceTopologyStatus s=primary_source_topology_measure(c,p->source,&info,error);
    return s==PRIMARY_TOPOLOGY_READY ? usage(p,info.total,out) : s;
}
PrimarySourceTopologyStatus primary_source_pending_prepare_topology(PrimarySourceDraftContext c,PrimarySourcePending *p,size_t reserved,char **error) {
    if (primary_source_pending_stage(p)!=PRIMARY_PENDING_SOURCE) return PRIMARY_TOPOLOGY_INVALID;
    PrimarySourcePendingUsage info; PrimarySourceTopologyStatus s=primary_source_pending_topology_measure(c,p,&info,error);
    if (s!=PRIMARY_TOPOLOGY_READY) return s;
    if (reserved<info.required) return PRIMARY_TOPOLOGY_LIMIT;
    return primary_source_topology_prepare(c,&p->source,reserved-sizeof(*p),&p->topology,error);
}
PrimarySourceTopologyStatus primary_source_pending_apply(PrimarySourceDraftContext c,PrimarySourcePending **owned,WorkbookHistoryStream *stream,size_t reserved,PrimarySourcePublication *out,char **error) {
    if (!owned || primary_source_pending_stage(*owned)!=PRIMARY_PENDING_TOPOLOGY) return PRIMARY_TOPOLOGY_INVALID;
    PrimarySourcePending *p=*owned; PrimarySourcePendingUsage info; PrimarySourceTopologyStatus s=primary_source_pending_topology_measure(c,p,&info,error);
    if (s!=PRIMARY_TOPOLOGY_READY) return s;
    if (reserved<info.required) return PRIMARY_TOPOLOGY_LIMIT;
    s=primary_source_topology_apply(c,&p->topology,stream,reserved-sizeof(*p),out,error);
    if (s==PRIMARY_TOPOLOGY_APPLIED || s==PRIMARY_TOPOLOGY_APPLIED_PENDING) primary_source_pending_free(owned);
    return s;
}
void primary_source_pending_free(PrimarySourcePending **owned) {
    if (owned && *owned) { PrimarySourcePending *p=*owned; primary_source_topology_free(&p->topology); primary_source_publish_cancel(&p->source); primary_source_draft_free(&p->draft); free(p); *owned=NULL; }
}
