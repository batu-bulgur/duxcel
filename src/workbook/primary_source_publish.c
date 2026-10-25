#include "primary_source_publish.h"
#include "definition_state.h"
#include <stdlib.h>
#include <string.h>
struct PrimarySourcePublishPlan {
    PrimarySourceDraft *draft;
    PrimarySourceCandidate candidate;
    SheetPresenceAsset *presence;
    SheetSourceView *initial;
    SheetView staged_view;
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
static PrimarySourcePublishStatus measure(PrimarySourceDraftContext c,const PrimarySourceDraft *draft,const SheetSourceView *initial,const PrimarySourceInitialSchema *schema,PrimarySourcePublishInfo *out,char **error) {
    PrimarySourceDraftStatus ds=primary_source_draft_check(c,draft,error); if (ds!=PRIMARY_SOURCE_READY) return draft_status(ds);
    const Analysis *source=primary_source_draft_source(draft);
    if (!out || !source) return fail(PRIMARY_PUBLISH_PROTECTED,error,"Resolve the actual source probe before preparing publication.");
    if (initial || schema) {
        if (!initial || !schema || !schema->analysis) return fail(PRIMARY_PUBLISH_INVALID,error,"Choose the complete actual-probe initial view proof.");
        if (schema->request!=*c.request || schema->primary_epoch!=*c.primary_epoch) return fail(PRIMARY_PUBLISH_STALE,error,"The initial source schema belongs to an earlier request/revision.");
        if (schema->error) return fail(PRIMARY_PUBLISH_FAILED,error,"The source schema probe failed before creation.");
        if (!definition_analysis_equal(schema->analysis,source)) return fail(PRIMARY_PUBLISH_STALE,error,"The initial source schema changed its resolved authored source.");
        Worksheet actual=prospective(c,draft); actual.analysis=*schema->analysis; DefinitionOriginSealInfo shape;
        if (definition_origin_asset_shape((DefinitionOriginAssetView){&actual,&actual.analysis},&shape,error)!=DEFINITION_ORIGIN_READY)
            return fail(PRIMARY_PUBLISH_INVALID,error,"Invalid materialized actual-probe source metadata.");
        SheetView preferences=*sheet_source_view_get(initial); preferences.columns=NULL; preferences.widths=NULL; preferences.column_count=0;
        bool matches=false; SheetSourceViewStatus status=sheet_source_view_matches(initial,(SheetSourceViewSpec){schema->columns,schema->count,preferences},&matches);
        if (status!=SHEET_SOURCE_VIEW_OK) return fail(PRIMARY_PUBLISH_INVALID,error,"Invalid bounded actual-probe column schema.");
        if (!matches) return fail(PRIMARY_PUBLISH_STALE,error,"The initial view does not match the actual-probe column names/types.");
    }
    if (*c.primary_epoch==UINT64_MAX) return fail(PRIMARY_PUBLISH_LIMIT,error,"Source query revision space is exhausted before publication.");
    size_t creation=workbook_lifecycle_create_staging(c.book,primary_source_draft_name(draft),source);
    if (creation==SIZE_MAX) return fail(PRIMARY_PUBLISH_LIMIT,error,"The real worksheet creation allocation cannot be admitted.");
    WorkbookHistoryResult history=workbook_history_peek(c.book,false,error);
    if (history.status==WORKBOOK_HISTORY_INVALID) return PRIMARY_PUBLISH_INVALID;
    Worksheet sheet=prospective(c,draft); if (initial) sheet.view=*sheet_source_view_get(initial); SheetPresenceMeasure presence={0}; DefinitionOriginSealInfo seal={0};
    if (sheet_presence_measure((SheetPresenceView){c.book->id,sheet.id,&sheet,&sheet.analysis},&presence)!=SHEET_PRESENCE_OK ||
        definition_origin_asset_shape((DefinitionOriginAssetView){&sheet,&sheet.analysis},&seal,error)!=DEFINITION_ORIGIN_READY)
        return fail(PRIMARY_PUBLISH_INVALID,error,"The complete prospective source sheet is invalid.");
    PrimarySourcePublishInfo info={.live=c.book->count*sizeof(Worksheet),.draft=primary_source_draft_owned(draft),
        .presence=presence.staging,.creation=creation,.origin=definition_origin_owner_bytes(),.scratch=seal.validation_scratch,.initial_view=sheet_source_view_owned(initial),.prospective_sheet=sheet.id,.primary_epoch=*c.primary_epoch+1};
    for (size_t i=0;i<c.book->count;i++) if (!add(&info.live,analysis_text_bytes(&c.book->sheets[i].analysis))) return PRIMARY_PUBLISH_LIMIT;
    if (!add(&info.live,sizeof(Analysis)) || !add(&info.live,analysis_text_bytes(c.primary)) || !add(&info.origin,seal.asset_bytes)) return PRIMARY_PUBLISH_LIMIT;
    info.coexisting=sizeof(PrimarySourcePublishPlan);
    if (!add(&info.coexisting,info.live) || !add(&info.coexisting,info.draft) || !add(&info.coexisting,info.presence) ||
        !add(&info.coexisting,info.creation) || !add(&info.coexisting,info.origin) || !add(&info.coexisting,info.scratch) || !add(&info.coexisting,info.initial_view))
        return fail(PRIMARY_PUBLISH_LIMIT,error,"Source publication ownership accounting overflowed.");
    *out=info; return PRIMARY_PUBLISH_READY;
}
static PrimarySourcePublishStatus prepare(PrimarySourceDraftContext c,PrimarySourceDraft **draft,SheetSourceView **initial,const PrimarySourceInitialSchema *schema,size_t reserved,PrimarySourcePublishPlan **out,char **error) {
    if (!draft || !out || *out) return fail(PRIMARY_PUBLISH_INVALID,error,"Choose empty publication plan ownership.");
    PrimarySourcePublishInfo info={0}; PrimarySourcePublishStatus status=measure(c,*draft,initial ? *initial : NULL,schema,&info,error); if (status!=PRIMARY_PUBLISH_READY) return status;
    if (reserved<info.coexisting) return fail(PRIMARY_PUBLISH_LIMIT,error,"Reserve the complete publication coexistence before validation or cloning.");
    Worksheet sheet=prospective(c,*draft); if (initial) sheet.view=*sheet_source_view_get(*initial); DefinitionOriginSealInfo seal={0};
    if (definition_origin_asset_measure((DefinitionOriginAssetView){&sheet,&sheet.analysis},info.scratch,&seal,error)!=DEFINITION_ORIGIN_READY) return PRIMARY_PUBLISH_INVALID;
    SheetPresenceAsset *presence=NULL;
    if (sheet_presence_capture((SheetPresenceView){c.book->id,sheet.id,&sheet,&sheet.analysis},info.presence,&presence,error)!=SHEET_PRESENCE_OK) return PRIMARY_PUBLISH_INVALID;
    PrimarySourceDraftStatus ds=primary_source_draft_check(c,*draft,error);
    if (ds!=PRIMARY_SOURCE_READY) { sheet_presence_free(&presence); return draft_status(ds); }
    PrimarySourcePublishPlan *plan=dx_alloc(sizeof(*plan)); *plan=(PrimarySourcePublishPlan){.draft=*draft,.presence=presence,.initial=initial ? *initial : NULL,.info=info,.seal=seal};
    *draft=NULL; if (initial) *initial=NULL; *out=plan; return PRIMARY_PUBLISH_READY;
}
PrimarySourcePublishStatus primary_source_publish_measure(PrimarySourceDraftContext c,const PrimarySourceDraft *draft,PrimarySourcePublishInfo *out,char **error) { return measure(c,draft,NULL,NULL,out,error); }
PrimarySourcePublishStatus primary_source_publish_prepare(PrimarySourceDraftContext c,PrimarySourceDraft **draft,size_t reserved,PrimarySourcePublishPlan **out,char **error) { return prepare(c,draft,NULL,NULL,reserved,out,error); }
PrimarySourcePublishStatus primary_source_publish_measure_initial(PrimarySourceDraftContext c,const PrimarySourceDraft *draft,const SheetSourceView *initial,PrimarySourceInitialSchema schema,PrimarySourcePublishInfo *out,char **error) {
    if (!initial) return PRIMARY_PUBLISH_INVALID;
    return measure(c,draft,initial,&schema,out,error);
}
PrimarySourcePublishStatus primary_source_publish_prepare_initial(PrimarySourceDraftContext c,PrimarySourceDraft **draft,SheetSourceView **initial,PrimarySourceInitialSchema schema,size_t reserved,PrimarySourcePublishPlan **out,char **error) {
    if (!initial || !*initial) return PRIMARY_PUBLISH_INVALID;
    return prepare(c,draft,initial,&schema,reserved,out,error);
}
const SheetView *primary_source_publish_initial_view(const PrimarySourcePublishPlan *plan) {
    return !plan ? NULL : plan->initial ? sheet_source_view_get(plan->initial) : plan->staged_view.columns ? &plan->staged_view : NULL;
}
PrimarySourcePublishInfo primary_source_publish_usage(const PrimarySourcePublishPlan *plan) { return plan ? plan->info : (PrimarySourcePublishInfo){0}; }
const SheetPresenceAsset *primary_source_publish_presence(const PrimarySourcePublishPlan *plan) {
    return plan && plan->draft ? plan->presence : NULL;
}
PrimarySourcePublishStatus primary_source_publish_check(PrimarySourceDraftContext c,const PrimarySourcePublishPlan *plan,char **error) {
    if (!plan || !plan->draft) return fail(PRIMARY_PUBLISH_INVALID,error,"The unpublished creation plan has no live source lease.");
    return draft_status(primary_source_draft_check(c,plan->draft,error));
}
PrimarySourcePublishStatus primary_source_publish_apply(PrimarySourceDraftContext c,PrimarySourcePublishPlan **owned,size_t reserved,PrimarySourcePublication *out,char **error) {
    if (!owned || !*owned || !out || out->sheet || out->primary_epoch || out->candidate.name || memcmp(&out->candidate.source,&(Analysis){0},sizeof(Analysis)) || out->presence || out->origin)
        return fail(PRIMARY_PUBLISH_INVALID,error,"Choose independent empty publication result ownership.");
    PrimarySourcePublishPlan *plan=*owned; PrimarySourcePublishStatus status=primary_source_publish_check(c,plan,error); if (status!=PRIMARY_PUBLISH_READY) return status;
    if (reserved<plan->info.coexisting) return fail(PRIMARY_PUBLISH_LIMIT,error,"The admitted creation coexistence must remain reserved through publication.");
    if (primary_source_draft_take(c,&plan->draft,&plan->candidate,error)!=PRIMARY_SOURCE_TAKEN) return PRIMARY_PUBLISH_STALE;
    /* Move into owned staging BEFORE the real producer. A rejected CREATE
       remains unpublished and cancellation still owns every initial-view byte. */
    if (plan->initial && sheet_source_view_take(&plan->initial,&plan->staged_view)!=SHEET_SOURCE_VIEW_OK) return PRIMARY_PUBLISH_FAILED;
    Workbook *book=(Workbook *)c.book; bool changed=false;
    if (!workbook_sheet_command(book,(WorkbookSheetCommand){.operation=SHEET_CREATE,.name=plan->candidate.name,.analysis=&plan->candidate.source},&changed,error) || !changed)
        return PRIMARY_PUBLISH_FAILED;
    Worksheet *sheet=book->sheets+book->active;
    /* The actual workbook_add producer zero-initializes its new SheetView.
       This is a nonallocating part of the SAME creation boundary, before any
       origin result/topology adoption proof can be returned to its caller. */
    sheet->view=plan->staged_view; plan->staged_view=(SheetView){0};
    PrimarySourcePublication result={.sheet=sheet->id,.primary_epoch=plan->info.primary_epoch,.candidate=plan->candidate,.presence=plan->presence};
    plan->candidate=(PrimarySourceCandidate){0}; plan->presence=NULL;
    DefinitionOriginContext origin={book,&sheet->analysis,c.request,c.primary_epoch,plan->seal.validation_scratch,true,false};
    result.origin_status=definition_origin_begin(&origin,book->active,definition_origin_owner_bytes(),&result.origin,error);
    if (result.origin_status==DEFINITION_ORIGIN_READY) result.origin_status=definition_origin_seal(&origin,result.origin,plan->seal.asset_bytes,error);
    status=result.origin_status==DEFINITION_ORIGIN_APPLIED ? PRIMARY_PUBLISH_ADMITTED : PRIMARY_PUBLISH_PENDING;
    *out=result; primary_source_publish_cancel(owned); return status;
}
void primary_source_publish_cancel(PrimarySourcePublishPlan **owned) {
    if (owned && *owned) { PrimarySourcePublishPlan *plan=*owned; primary_source_draft_free(&plan->draft); primary_source_candidate_free(&plan->candidate); sheet_source_view_free(&plan->initial); view_free(&plan->staged_view); sheet_presence_free(&plan->presence); free(plan); *owned=NULL; }
}
void primary_source_publication_free(PrimarySourcePublication *out) {
    if (out) { primary_source_candidate_free(&out->candidate); sheet_presence_free(&out->presence); definition_origin_free(&out->origin); *out=(PrimarySourcePublication){0}; }
}
