#include "primary_source_publish.h"
#include <stdlib.h>
#include <string.h>
struct PrimarySourcePublishPlan {
    PrimarySourceDraft *draft;
    PrimarySourceCandidate candidate;
    SheetPresenceAsset *presence;
    PrimarySourcePublishInfo info;
    DefinitionOriginSealInfo seal;
};
static PrimarySourcePublishStatus fail(PrimarySourcePublishStatus status,char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return status;
}
static bool add(size_t *sum,size_t extra) { if (extra==SIZE_MAX || *sum>SIZE_MAX-extra) return false; *sum+=extra; return true; }
static PrimarySourcePublishStatus draft_status(PrimarySourceDraftStatus status) {
    return status==PRIMARY_SOURCE_READY ? PRIMARY_PUBLISH_READY : status==PRIMARY_SOURCE_STALE ? PRIMARY_PUBLISH_STALE :
        status==PRIMARY_SOURCE_PROTECTED ? PRIMARY_PUBLISH_PROTECTED : status==PRIMARY_SOURCE_LIMIT ? PRIMARY_PUBLISH_LIMIT : PRIMARY_PUBLISH_INVALID;
}
static Worksheet prospective(PrimarySourceDraftContext c,const PrimarySourceDraft *draft) {
    return (Worksheet){.id=c.book->sheet_high_water+1,.name=(char *)primary_source_draft_name(draft),.analysis=*primary_source_draft_source(draft)};
}
PrimarySourcePublishStatus primary_source_publish_measure(PrimarySourceDraftContext c,const PrimarySourceDraft *draft,PrimarySourcePublishInfo *out,char **error) {
    PrimarySourceDraftStatus ds=primary_source_draft_check(c,draft,error); if (ds!=PRIMARY_SOURCE_READY) return draft_status(ds);
    const Analysis *source=primary_source_draft_source(draft);
    if (!out || !source) return fail(PRIMARY_PUBLISH_PROTECTED,error,"Resolve the actual source probe before preparing publication.");
    size_t creation=workbook_lifecycle_create_staging(c.book,primary_source_draft_name(draft),source);
    if (creation==SIZE_MAX) return fail(PRIMARY_PUBLISH_LIMIT,error,"The real worksheet creation allocation cannot be admitted.");
    WorkbookHistoryResult history=workbook_history_peek(c.book,false,error);
    if (history.status==WORKBOOK_HISTORY_INVALID) return PRIMARY_PUBLISH_INVALID;
    Worksheet sheet=prospective(c,draft); SheetPresenceMeasure presence={0}; DefinitionOriginSealInfo seal={0};
    if (sheet_presence_measure((SheetPresenceView){c.book->id,sheet.id,&sheet,&sheet.analysis},&presence)!=SHEET_PRESENCE_OK ||
        definition_origin_asset_shape((DefinitionOriginAssetView){&sheet,&sheet.analysis},&seal,error)!=DEFINITION_ORIGIN_READY)
        return fail(PRIMARY_PUBLISH_INVALID,error,"The complete prospective source sheet is invalid.");
    PrimarySourcePublishInfo info={.live=c.book->count*sizeof(Worksheet),.draft=primary_source_draft_owned(draft),
        .presence=presence.staging,.creation=creation,.origin=definition_origin_owner_bytes(),.scratch=seal.validation_scratch,.prospective_sheet=sheet.id};
    for (size_t i=0;i<c.book->count;i++) if (!add(&info.live,analysis_text_bytes(&c.book->sheets[i].analysis))) return PRIMARY_PUBLISH_LIMIT;
    if (!add(&info.live,sizeof(Analysis)) || !add(&info.live,analysis_text_bytes(c.primary)) || !add(&info.origin,seal.asset_bytes)) return PRIMARY_PUBLISH_LIMIT;
    info.coexisting=sizeof(PrimarySourcePublishPlan);
    if (!add(&info.coexisting,info.live) || !add(&info.coexisting,info.draft) || !add(&info.coexisting,info.presence) ||
        !add(&info.coexisting,info.creation) || !add(&info.coexisting,info.origin) || !add(&info.coexisting,info.scratch))
        return fail(PRIMARY_PUBLISH_LIMIT,error,"Source publication ownership accounting overflowed.");
    *out=info; return PRIMARY_PUBLISH_READY;
}
PrimarySourcePublishStatus primary_source_publish_prepare(PrimarySourceDraftContext c,PrimarySourceDraft **draft,size_t reserved,PrimarySourcePublishPlan **out,char **error) {
    if (!draft || !out || *out) return fail(PRIMARY_PUBLISH_INVALID,error,"Choose empty publication plan ownership.");
    PrimarySourcePublishInfo info={0}; PrimarySourcePublishStatus status=primary_source_publish_measure(c,*draft,&info,error); if (status!=PRIMARY_PUBLISH_READY) return status;
    if (reserved<info.coexisting) return fail(PRIMARY_PUBLISH_LIMIT,error,"Reserve the complete publication coexistence before validation or cloning.");
    Worksheet sheet=prospective(c,*draft); DefinitionOriginSealInfo seal={0};
    if (definition_origin_asset_measure((DefinitionOriginAssetView){&sheet,&sheet.analysis},info.scratch,&seal,error)!=DEFINITION_ORIGIN_READY) return PRIMARY_PUBLISH_INVALID;
    SheetPresenceAsset *presence=NULL;
    if (sheet_presence_capture((SheetPresenceView){c.book->id,sheet.id,&sheet,&sheet.analysis},info.presence,&presence,error)!=SHEET_PRESENCE_OK) return PRIMARY_PUBLISH_INVALID;
    PrimarySourceDraftStatus ds=primary_source_draft_check(c,*draft,error);
    if (ds!=PRIMARY_SOURCE_READY) { sheet_presence_free(&presence); return draft_status(ds); }
    PrimarySourcePublishPlan *plan=dx_alloc(sizeof(*plan)); *plan=(PrimarySourcePublishPlan){.draft=*draft,.presence=presence,.info=info,.seal=seal};
    *draft=NULL; *out=plan; return PRIMARY_PUBLISH_READY;
}
const SheetPresenceAsset *primary_source_publish_presence(const PrimarySourcePublishPlan *plan) {
    return plan && plan->draft ? plan->presence : NULL;
}
PrimarySourcePublishStatus primary_source_publish_check(PrimarySourceDraftContext c,const PrimarySourcePublishPlan *plan,char **error) {
    if (!plan || !plan->draft) return fail(PRIMARY_PUBLISH_INVALID,error,"The unpublished creation plan has no live source lease.");
    return draft_status(primary_source_draft_check(c,plan->draft,error));
}
PrimarySourcePublishStatus primary_source_publish_apply(PrimarySourceDraftContext c,PrimarySourcePublishPlan **owned,size_t reserved,PrimarySourcePublication *out,char **error) {
    if (!owned || !*owned || !out || out->sheet || out->candidate.name || memcmp(&out->candidate.source,&(Analysis){0},sizeof(Analysis)) || out->presence || out->origin)
        return fail(PRIMARY_PUBLISH_INVALID,error,"Choose independent empty publication result ownership.");
    PrimarySourcePublishPlan *plan=*owned; PrimarySourcePublishStatus status=primary_source_publish_check(c,plan,error); if (status!=PRIMARY_PUBLISH_READY) return status;
    if (reserved<plan->info.coexisting) return fail(PRIMARY_PUBLISH_LIMIT,error,"The admitted creation coexistence must remain reserved through publication.");
    if (primary_source_draft_take(c,&plan->draft,&plan->candidate,error)!=PRIMARY_SOURCE_TAKEN) return PRIMARY_PUBLISH_STALE;
    Workbook *book=(Workbook *)c.book; bool changed=false;
    if (!workbook_sheet_command(book,(WorkbookSheetCommand){.operation=SHEET_CREATE,.name=plan->candidate.name,.analysis=&plan->candidate.source},&changed,error) || !changed)
        return PRIMARY_PUBLISH_FAILED;
    Worksheet *sheet=book->sheets+book->active;
    PrimarySourcePublication result={.sheet=sheet->id,.candidate=plan->candidate,.presence=plan->presence};
    plan->candidate=(PrimarySourceCandidate){0}; plan->presence=NULL;
    DefinitionOriginContext origin={book,&sheet->analysis,c.request,c.primary_epoch,plan->seal.validation_scratch,true,false};
    result.origin_status=definition_origin_begin(&origin,book->active,definition_origin_owner_bytes(),&result.origin,error);
    if (result.origin_status==DEFINITION_ORIGIN_READY) result.origin_status=definition_origin_seal(&origin,result.origin,plan->seal.asset_bytes,error);
    status=result.origin_status==DEFINITION_ORIGIN_APPLIED ? PRIMARY_PUBLISH_ADMITTED : PRIMARY_PUBLISH_PENDING;
    *out=result; primary_source_publish_cancel(owned); return status;
}
void primary_source_publish_cancel(PrimarySourcePublishPlan **owned) {
    if (owned && *owned) { PrimarySourcePublishPlan *plan=*owned; primary_source_draft_free(&plan->draft); primary_source_candidate_free(&plan->candidate); sheet_presence_free(&plan->presence); free(plan); *owned=NULL; }
}
void primary_source_publication_free(PrimarySourcePublication *out) {
    if (out) { primary_source_candidate_free(&out->candidate); sheet_presence_free(&out->presence); definition_origin_free(&out->origin); *out=(PrimarySourcePublication){0}; }
}
