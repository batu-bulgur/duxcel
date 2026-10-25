#include "workbook/primary_source_publish.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL primary publication:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis a={.kind=SOURCE_CSV,.path="/current.csv"};
    CHECK(workbook_add(&b,"Current",&a,NULL)); CHECK(workbook_prepare_command_session(&b,0,NULL)); b.dirty=false; return b;
}
static PrimarySourceDraft *resolved(PrimarySourceDraftContext c) {
    Analysis proposal={.kind=SOURCE_DATABASE,.path="/next.duckdb",.schema="main"}; PrimarySourceDraft *draft=NULL;
    CHECK(primary_source_draft_begin(c,&proposal," Current ",SIZE_MAX,&draft,NULL)==PRIMARY_SOURCE_READY);
    Analysis reply=proposal; reply.table="Sales";
    CHECK(primary_source_draft_resolve(c,draft,(PrimarySourceDraftReply){*c.request,&reply,NULL},SIZE_MAX,NULL)==PRIMARY_SOURCE_RESOLVED);
    return draft;
}
static void publication(void) {
    Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=12,epoch=4;
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceDraft *draft=resolved(c);
    PrimarySourcePublishInfo info={0}; CHECK(primary_source_publish_measure(c,draft,&info,NULL)==PRIMARY_PUBLISH_READY);
    CHECK(info.prospective_sheet==2 && info.live>=sizeof(Worksheet)+sizeof(Analysis) && info.creation && info.origin && info.presence);
    CHECK(info.coexisting>info.live+info.draft+info.creation+info.origin+info.presence);
    Workbook before=b; Worksheet old=b.sheets[0]; PrimarySourcePublishPlan *plan=NULL;
    CHECK(primary_source_publish_prepare(c,&draft,info.coexisting-1,&plan,NULL)==PRIMARY_PUBLISH_LIMIT && draft && !plan);
    CHECK(!memcmp(&b,&before,sizeof(b)) && !memcmp(b.sheets,&old,sizeof(old)));
    CHECK(primary_source_publish_prepare(c,&draft,info.coexisting,&plan,NULL)==PRIMARY_PUBLISH_READY && !draft && plan);
    CHECK(!memcmp(&b,&before,sizeof(b)) && !memcmp(b.sheets,&old,sizeof(old)) && !b.lifecycle);
    PrimarySourcePublication out={0}; CHECK(primary_source_publish_apply(c,&plan,info.coexisting-1,&out,NULL)==PRIMARY_PUBLISH_LIMIT && plan && !out.sheet);
    CHECK(!memcmp(&b,&before,sizeof(b)) && primary_source_publish_check(c,plan,NULL)==PRIMARY_PUBLISH_READY);
    CHECK(primary_source_publish_apply(c,&plan,info.coexisting,&out,NULL)==PRIMARY_PUBLISH_ADMITTED && !plan);
    CHECK(b.count==2 && b.active==1 && b.sheet_high_water==2 && b.command_revision==before.command_revision+1 && b.dirty && out.sheet==2);
    CHECK(b.sheets[0].id==old.id && b.sheets[0].analysis.path==old.analysis.path && !strcmp(app.path,"/current.csv"));
    CHECK(!strcmp(out.candidate.name,"Current (2)") && !strcmp(b.sheets[1].name,out.candidate.name));
    CHECK(b.sheets[1].analysis.path!=out.candidate.source.path && !strcmp(b.sheets[1].analysis.table,"Sales"));
    SheetPresenceView presence=sheet_presence_view(out.presence);
    CHECK(presence.sheet->id==2 && presence.sheet_high_water==2 && presence.primary->path!=out.candidate.source.path && presence.primary->path!=b.sheets[1].analysis.path);
    CHECK(definition_origin_state(out.origin).phase==DEFINITION_ORIGIN_ADMITTED && out.origin_status==DEFINITION_ORIGIN_APPLIED);
    DefinitionOriginAssets assets=definition_origin_assets(out.origin); CHECK(assets.primary && assets.primary_bytes && assets.objects && assets.sources && !assets.formats);
    unsigned char *frozen=malloc(assets.primary_bytes); memcpy(frozen,assets.primary,assets.primary_bytes);
    WorkbookHistoryResult undo=workbook_history_step(&b,false,NULL); CHECK(undo.status==WORKBOOK_HISTORY_APPLIED && undo.lifecycle && b.count==1 && b.sheet_high_water==2);
    WorkbookHistoryResult redo=workbook_history_step(&b,true,NULL); CHECK(redo.status==WORKBOOK_HISTORY_APPLIED && redo.lifecycle && b.count==2 && b.sheets[b.active].id==out.sheet);
    CHECK(!memcmp(frozen,definition_origin_assets(out.origin).primary,assets.primary_bytes)); free(frozen);
    primary_source_publication_free(&out); CHECK(!out.sheet && !out.origin && !out.presence && b.count==2);
    analysis_free(&app); workbook_free(&b);
}
static void stale_and_cancel(void) {
    for (unsigned mode=0;mode<9;mode++) {
        Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=12,epoch=4;
        PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceDraft *draft=resolved(c); PrimarySourcePublishPlan *plan=NULL;
        CHECK(primary_source_publish_prepare(c,&draft,SIZE_MAX,&plan,NULL)==PRIMARY_PUBLISH_READY);
        if (mode==0) request++;
        if (mode==1) epoch++;
        if (mode==2) b.command_revision++;
        if (mode==3) b.history_branch++;
        if (mode==4) b.sheet_high_water++;
        if (mode==5) b.sheets[0].cells.revision++;
        if (mode==6) { char *p=app.path; app.path=dx_strdup(p); free(p); }
        if (mode==7) c.ready=false;
        if (mode==8) c.read_only=true;
        Workbook before=b; PrimarySourcePublication out={0};
        PrimarySourcePublishStatus expected=mode>=7 ? PRIMARY_PUBLISH_PROTECTED : PRIMARY_PUBLISH_STALE;
        CHECK(primary_source_publish_check(c,plan,NULL)==expected);
        CHECK(primary_source_publish_apply(c,&plan,SIZE_MAX,&out,NULL)==expected && plan && !out.sheet && !memcmp(&b,&before,sizeof(b)));
        primary_source_publish_cancel(&plan); CHECK(!plan && !memcmp(&b,&before,sizeof(b)));
        analysis_free(&app); workbook_free(&b);
    }
}
static void assets_and_unresolved(void) {
    Workbook b=book(); uint64_t request=12,epoch=4; PrimarySourceDraftContext c={&b,&b.sheets[0].analysis,&request,&epoch,true,false};
    Analysis proposal={.kind=SOURCE_DATABASE,.path="/next.duckdb"}; PrimarySourceDraft *draft=NULL;
    CHECK(primary_source_draft_begin(c,&proposal,"Next",SIZE_MAX,&draft,NULL)==PRIMARY_SOURCE_READY);
    PrimarySourcePublishInfo info={1,2,3,4,5,6,7,8},old=info;
    CHECK(primary_source_publish_measure(c,draft,&info,NULL)==PRIMARY_PUBLISH_PROTECTED && !memcmp(&old,&info,sizeof(info)));
    primary_source_draft_free(&draft);
    DefinitionOriginAssetView view={b.sheets,&b.sheets[0].analysis}; DefinitionOriginSealInfo shape={0},measure={0};
    CHECK(definition_origin_asset_shape(view,&shape,NULL)==DEFINITION_ORIGIN_READY && shape.asset_bytes);
    CHECK(definition_origin_asset_measure(view,shape.validation_scratch,&measure,NULL)==DEFINITION_ORIGIN_READY && !memcmp(&shape,&measure,sizeof(shape)));
    b.sheets[0].analysis.filter_number=(NumberType){NUM_DECIMAL,39,0};
    CHECK(definition_origin_asset_shape(view,&measure,NULL)==DEFINITION_ORIGIN_INVALID);
    b.sheets[0].analysis.filter_number=(NumberType){0}; workbook_free(&b);
}
static void file_and_demo_publication(void) {
    const SourceKind kinds[]={SOURCE_CSV,SOURCE_PARQUET,SOURCE_DEMO};
    for (size_t k=0;k<3;k++) {
        Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=12,epoch=4;
        PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false};
        Analysis source={.kind=kinds[k],.path=k==0 ? "/new.csv" : k==1 ? "/new.parquet" : ""};
        PrimarySourceDraft *draft=NULL; PrimarySourcePublishPlan *plan=NULL; PrimarySourcePublication out={0};
        CHECK(primary_source_draft_begin(c,&source,"File",SIZE_MAX,&draft,NULL)==PRIMARY_SOURCE_READY);
        CHECK(primary_source_draft_resolve(c,draft,(PrimarySourceDraftReply){request,&source,NULL},SIZE_MAX,NULL)==PRIMARY_SOURCE_RESOLVED);
        CHECK(primary_source_publish_prepare(c,&draft,SIZE_MAX,&plan,NULL)==PRIMARY_PUBLISH_READY && !draft && b.count==1);
        CHECK(primary_source_publish_apply(c,&plan,SIZE_MAX,&out,NULL)==PRIMARY_PUBLISH_ADMITTED && !plan);
        CHECK(b.count==2 && b.sheets[1].analysis.kind==kinds[k] && out.candidate.source.kind==kinds[k]);
        CHECK(!strcmp(out.candidate.source.path,source.path) && out.candidate.source.path!=source.path && !strcmp(app.path,"/current.csv"));
        CHECK(definition_origin_assets(out.origin).primary && sheet_presence_view(out.presence).sheet->id==b.sheets[1].id);
        primary_source_publication_free(&out); analysis_free(&app); workbook_free(&b);
    }
}
int main(void) { publication(); stale_and_cancel(); assets_and_unresolved(); file_and_demo_publication(); printf("PASS: %u primary publication checks\n",checks); return 0; }
