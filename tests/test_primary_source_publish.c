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
    const SheetPresenceAsset *borrowed=primary_source_publish_presence(plan); CHECK(borrowed && sheet_presence_view(borrowed).sheet->id==info.prospective_sheet);
    PrimarySourcePublication out={0}; CHECK(primary_source_publish_apply(c,&plan,info.coexisting-1,&out,NULL)==PRIMARY_PUBLISH_LIMIT && plan && !out.sheet);
    CHECK(!memcmp(&b,&before,sizeof(b)) && primary_source_publish_check(c,plan,NULL)==PRIMARY_PUBLISH_READY && primary_source_publish_presence(plan)==borrowed);
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
static void prospective_presence_lifetime(void) {
    Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=12,epoch=4;
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceDraft *draft=resolved(c); PrimarySourcePublishPlan *plan=NULL;
    CHECK(!primary_source_publish_presence(NULL));
    CHECK(primary_source_publish_prepare(c,&draft,SIZE_MAX,&plan,NULL)==PRIMARY_PUBLISH_READY);
    const SheetPresenceAsset *borrowed=primary_source_publish_presence(plan); CHECK(borrowed);
    SheetPresenceView view=sheet_presence_view(borrowed); SheetPresenceMeasure info={0};
    CHECK(view.sheet->id==b.sheet_high_water+1 && view.primary->path!=app.path && !strcmp(view.primary->table,"Sales"));
    CHECK(sheet_presence_measure(view,&info)==SHEET_PRESENCE_OK);
    SheetPresenceAsset *independent=NULL;
    CHECK(sheet_presence_capture(view,info.staging-1,&independent,NULL)==SHEET_PRESENCE_LIMIT && !independent && primary_source_publish_presence(plan)==borrowed);
    CHECK(sheet_presence_capture(view,info.staging,&independent,NULL)==SHEET_PRESENCE_OK);
    CHECK(sheet_presence_view(independent).primary->path!=view.primary->path && sheet_presence_view(independent).sheet->name!=view.sheet->name);
    request++; CHECK(primary_source_publish_check(c,plan,NULL)==PRIMARY_PUBLISH_STALE);
    CHECK(primary_source_publish_presence(plan)==borrowed && !strcmp(sheet_presence_view(borrowed).primary->table,"Sales"));
    PrimarySourcePublication out={0}; CHECK(primary_source_publish_apply(c,&plan,SIZE_MAX,&out,NULL)==PRIMARY_PUBLISH_STALE && !out.sheet && b.count==1);
    primary_source_publish_cancel(&plan); analysis_free(&app); workbook_free(&b);
    view=sheet_presence_view(independent);
    CHECK(view.sheet->id==2 && !strcmp(view.sheet->name,"Current (2)") && !strcmp(view.primary->path,"/next.duckdb") && !strcmp(view.primary->table,"Sales"));
    sheet_presence_free(&independent); CHECK(!independent);
}
static void occupied_output_preservation(void) {
    Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=12,epoch=4;
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceDraft *draft=resolved(c); PrimarySourcePublishPlan *plan=NULL;
    CHECK(primary_source_publish_prepare(c,&draft,SIZE_MAX,&plan,NULL)==PRIMARY_PUBLISH_READY);
    Workbook before=b; Worksheet old=b.sheets[0];
    for (unsigned mode=0;mode<4;mode++) {
        PrimarySourcePublication out={0};
        if (mode==0) out.candidate.source.query=dx_strdup("SELECT occupied FROM input");
        if (mode==1) out.candidate.source.schema=dx_strdup("occupied");
        if (mode==2) out.candidate.source.filter_value=dx_strdup("occupied");
        if (mode==3) out.candidate.source.kind=SOURCE_DEMO;
        PrimarySourcePublication occupied=out;
        CHECK(primary_source_publish_apply(c,&plan,SIZE_MAX,&out,NULL)==PRIMARY_PUBLISH_INVALID);
        CHECK(!memcmp(&out,&occupied,sizeof(out)) && !memcmp(&b,&before,sizeof(b)) && !memcmp(b.sheets,&old,sizeof(old)));
        CHECK(primary_source_publish_check(c,plan,NULL)==PRIMARY_PUBLISH_READY && primary_source_publish_presence(plan));
        primary_source_publication_free(&out);
    }
    PrimarySourcePublication out={0}; CHECK(primary_source_publish_apply(c,&plan,SIZE_MAX,&out,NULL)==PRIMARY_PUBLISH_ADMITTED && b.count==2);
    primary_source_publication_free(&out); analysis_free(&app); workbook_free(&b);
}
static void query_revision_exhaustion(void) {
    Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=12,epoch=UINT64_MAX;
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceDraft *draft=resolved(c);
    PrimarySourcePublishInfo info={0},unchanged=info; PrimarySourcePublishPlan *plan=NULL; Workbook before=b;
    CHECK(primary_source_publish_measure(c,draft,&info,NULL)==PRIMARY_PUBLISH_LIMIT && !memcmp(&info,&unchanged,sizeof(info)));
    CHECK(primary_source_publish_prepare(c,&draft,SIZE_MAX,&plan,NULL)==PRIMARY_PUBLISH_LIMIT && draft && !plan && !memcmp(&before,&b,sizeof(b)));
    primary_source_draft_free(&draft); epoch--; draft=resolved(c);
    CHECK(primary_source_publish_prepare(c,&draft,SIZE_MAX,&plan,NULL)==PRIMARY_PUBLISH_READY);
    info=primary_source_publish_usage(plan); CHECK(info.primary_epoch==UINT64_MAX && info.coexisting && !primary_source_publish_usage(NULL).coexisting);
    PrimarySourcePublication out={0}; CHECK(primary_source_publish_apply(c,&plan,SIZE_MAX,&out,NULL)==PRIMARY_PUBLISH_ADMITTED);
    CHECK(out.primary_epoch==UINT64_MAX && epoch==UINT64_MAX-1 && b.count==2);
    primary_source_publication_free(&out); analysis_free(&app); workbook_free(&b);
}
static SheetSourceView *initial_view(Column columns[3]) {
    SheetSourceViewSpec spec={columns,3,{.row=800,.col=9000,.anchor_row=801,.anchor_col=9001,.end_row=802,.end_col=9002,.top=780,.horizontal=11.5f,.fraction=.25,.canvas_zoom=1.5f,.selection=2}};
    SheetSourceViewInfo info; SheetSourceView *view=NULL;
    CHECK(sheet_source_view_measure(spec,&info)==SHEET_SOURCE_VIEW_OK);
    CHECK(sheet_source_view_build(spec,info.owned,&view)==SHEET_SOURCE_VIEW_OK); return view;
}
static void initial_view_adoption(void) {
    Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=12,epoch=4;
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceDraft *draft=resolved(c);
    Column columns[]={{.name=dx_strdup("City"),.type=DUCKDB_TYPE_VARCHAR},{.name=dx_strdup("When"),.type=DUCKDB_TYPE_TIMESTAMP},{.name=dx_strdup("Amount"),.type=DUCKDB_TYPE_DECIMAL}};
    SheetSourceView *view=initial_view(columns); PrimarySourceInitialSchema schema={request,epoch,primary_source_draft_source(draft),columns,3,NULL};
    PrimarySourcePublishInfo plain,info; CHECK(primary_source_publish_measure(c,draft,&plain,NULL)==PRIMARY_PUBLISH_READY);
    CHECK(primary_source_publish_measure_initial(c,draft,view,schema,&info,NULL)==PRIMARY_PUBLISH_READY);
    CHECK(info.initial_view==sheet_source_view_owned(view) && info.presence>plain.presence && info.coexisting>plain.coexisting+info.initial_view);
    Workbook before=b; PrimarySourcePublishPlan *plan=NULL;
    CHECK(primary_source_publish_prepare_initial(c,&draft,&view,schema,info.coexisting-1,&plan,NULL)==PRIMARY_PUBLISH_LIMIT && draft && view && !plan && !memcmp(&b,&before,sizeof(b)));
    CHECK(primary_source_publish_prepare_initial(c,&draft,&view,schema,info.coexisting,&plan,NULL)==PRIMARY_PUBLISH_READY && !draft && !view && !memcmp(&b,&before,sizeof(b)));
    const SheetView *after=primary_source_publish_initial_view(plan); SheetPresenceView proposed=sheet_presence_view(primary_source_publish_presence(plan));
    CHECK(after && after->column_count==52 && after->columns!=proposed.sheet->view.columns && after->widths!=proposed.sheet->view.widths);
    CHECK(after->widths[0]==128 && after->widths[1]==190 && after->widths[51]==96 && proposed.sheet->view.col==9000 && proposed.sheet->view.canvas_zoom==1.5f);
    for (size_t i=0;i<3;i++) free(columns[i].name);
    PrimarySourcePublication out={0}; CHECK(primary_source_publish_apply(c,&plan,info.coexisting,&out,NULL)==PRIMARY_PUBLISH_ADMITTED && !plan);
    Worksheet *created=b.sheets+b.active; bool matches=false;
    CHECK(created->view.column_count==52 && created->view.widths[1]==190 && created->view.col==9000 && created->view.fraction==.25 && created->view.canvas_zoom==1.5f);
    CHECK(sheet_presence_matches(out.presence,(SheetPresenceView){b.id,b.sheet_high_water,created,&out.candidate.source},&matches)==SHEET_PRESENCE_OK && matches);
    CHECK(b.command_revision==before.command_revision+1 && out.primary_epoch==epoch+1);
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && b.count==1);
    CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED && b.count==2);
    created=b.sheets+b.active; CHECK(created->view.widths[1]==190 && created->view.col==9000 && !strcmp(created->view.columns[2],"Amount"));
    CHECK(sheet_presence_matches(out.presence,(SheetPresenceView){b.id,b.sheet_high_water,created,&created->analysis},&matches)==SHEET_PRESENCE_OK && matches);
    primary_source_publication_free(&out); analysis_free(&app); workbook_free(&b);
}
static void initial_schema_fences(void) {
    for (unsigned mode=0;mode<10;mode++) {
        Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=12,epoch=4;
        PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceDraft *draft=resolved(c);
        Column columns[]={{.name="City",.type=DUCKDB_TYPE_VARCHAR},{.name="When",.type=DUCKDB_TYPE_TIMESTAMP},{.name="Amount",.type=DUCKDB_TYPE_DECIMAL}};
        SheetSourceView *view=initial_view(columns); Analysis actual=*primary_source_draft_source(draft);
        PrimarySourceInitialSchema schema={request,epoch,&actual,columns,3,NULL};
        if (mode==0) schema.request++;
        if (mode==1) schema.primary_epoch++;
        if (mode==2) schema.error="actual query failed";
        if (mode==3) actual.path="/different.duckdb";
        if (mode==4) columns[0].name="Different";
        if (mode==5) columns[1].type=DUCKDB_TYPE_VARCHAR;
        if (mode==6) schema.count=2;
        if (mode==7) schema.count=0;
        if (mode==8) columns[1].name=NULL;
        if (mode==9) actual.filter_number=(NumberType){NUM_DECIMAL,39,0};
        PrimarySourcePublishStatus expected=mode==2 ? PRIMARY_PUBLISH_FAILED : mode>=7 ? PRIMARY_PUBLISH_INVALID : PRIMARY_PUBLISH_STALE;
        PrimarySourcePublishInfo info={0},unchanged=info; Workbook before=b; PrimarySourcePublishPlan *plan=NULL;
        CHECK(primary_source_publish_measure_initial(c,draft,view,schema,&info,NULL)==expected && !memcmp(&info,&unchanged,sizeof(info)));
        CHECK(primary_source_publish_prepare_initial(c,&draft,&view,schema,SIZE_MAX,&plan,NULL)==expected && draft && view && !plan && !memcmp(&before,&b,sizeof(b)));
        sheet_source_view_free(&view); primary_source_draft_free(&draft); analysis_free(&app); workbook_free(&b);
    }
    CHECK(!primary_source_publish_initial_view(NULL));
}
int main(void) { publication(); stale_and_cancel(); assets_and_unresolved(); file_and_demo_publication(); prospective_presence_lifetime(); occupied_output_preservation(); query_revision_exhaustion(); initial_view_adoption(); initial_schema_fences(); printf("PASS: %u primary publication checks\n",checks); return 0; }
