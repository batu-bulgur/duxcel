#include "workbook/definition_origin.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL definition origin:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(bool table,bool route,bool filter) {
    Workbook b; workbook_init(&b); Analysis a={.kind=SOURCE_DATABASE,.path="/source.duckdb"},empty={0};
    if (filter) CHECK(analysis_filter_set(&a,"Id",FILTER_BETWEEN,"[\"1\",\"2\"]",NULL));
    CHECK(workbook_add(&b,"Loaded",table ? &empty : &a,NULL));
    if (table) {
        CHECK(objects_add_table(&b.sheets[0].objects,10,2,"Loaded table",&a,NULL)==0); worksheet_record(b.sheets,EDIT_OBJECTS);
        if (route) { CHECK(objects_add(&b.sheets[0].objects,40,2,"View",NULL)==1); worksheet_record(b.sheets,EDIT_OBJECTS); CHECK(objects_bind_source(&b.sheets[0].objects,1,b.sheets[0].objects.items[0].id,NULL)); worksheet_record(b.sheets,EDIT_OBJECTS); }
    }
    if (filter) analysis_filters_clear(&a);
    workbook_history_note_edit(&b); b.dirty=false; CellEditTarget target; CHECK(workbook_prepare_cell_edit(&b,0,0,0,&target,NULL)); return b;
}
static DefinitionOrigin *begin(DefinitionOriginContext *c) {
    DefinitionOrigin *p=NULL; CHECK(definition_origin_begin(c,0,definition_origin_owner_bytes(),&p,NULL)==DEFINITION_ORIGIN_READY && p);
    CHECK(definition_origin_state(p).phase==DEFINITION_ORIGIN_OPEN && definition_origin_state(p).epoch==1 && !definition_origin_state(p).asset_bytes && !definition_origin_assets(p).primary); return p;
}
static DefinitionOriginReply reply(DefinitionOriginContext *c,bool table,bool route,Analysis *resolved) {
    Worksheet *s=c->book->sheets; const Analysis *old=table ? &s->objects.items[0].source : c->primary;
    *resolved=analysis_copy(old); free(resolved->schema); resolved->schema=dx_strdup("main"); resolved->table=dx_strdup("alpha");
    return (DefinitionOriginReply){*c->request_epoch,table ? s->objects.items[0].id : 0,table ? s->objects.items[route ? 1 : 0].id : 0,
        table ? s->objects.items[route ? 1 : 0].revision : *c->primary_epoch,resolved};
}
static void basics(bool table,bool route,bool external) {
    Workbook b=book(table,route,false); Worksheet *s=b.sheets; Analysis live=analysis_copy(&s->analysis); uint64_t request=10,primary_epoch=20;
    DefinitionOriginContext c={&b,external ? &live : &s->analysis,&request,&primary_epoch,SIZE_MAX,true,false}; DefinitionOrigin *p=begin(&c);
    Workbook *saved=workbook_snapshot(&b); CHECK(saved); Workbook before=b; Worksheet sheet=*s; Analysis resolved={0}; DefinitionOriginReply r=reply(&c,table,route,&resolved);
    CHECK(definition_origin_seal(&c,p,SIZE_MAX,NULL)==DEFINITION_ORIGIN_UNRESOLVED && !definition_origin_assets(p).primary);
    DefinitionOriginCompletionInfo info={0}; CHECK(definition_origin_completion_measure(&c,p,&r,&info,NULL)==DEFINITION_ORIGIN_READY && info.owned==11 && info.owner==r.owner);
    CHECK(definition_origin_complete(&c,p,&r,info.owned-1,NULL)==DEFINITION_ORIGIN_LIMIT && !b.dirty);
    CHECK(definition_origin_complete(&c,p,&r,info.owned,NULL)==DEFINITION_ORIGIN_APPLIED && b.dirty && definition_origin_state(p).epoch==2);
    const Analysis *actual=table ? &s->objects.items[0].source : c.primary;
    CHECK(!strcmp(actual->schema,"main") && !strcmp(actual->table,"alpha") && b.command_revision==before.command_revision && b.history_branch==before.history_branch &&
        s->history_last==sheet.history_last && s->undo_actions_count==sheet.undo_actions_count && s->redo_actions_count==sheet.redo_actions_count &&
        s->objects.next_id==sheet.objects.next_id && request==10 && primary_epoch==20);
    if (table) CHECK(s->objects.items[0].revision==1 && !saved->sheets[0].objects.items[0].source.table);
    else CHECK(!saved->sheets[0].analysis.table);
    if (external && !table) CHECK(!s->analysis.table);
    CHECK(definition_origin_seal(&c,p,0,NULL)==DEFINITION_ORIGIN_LIMIT && definition_origin_state(p).phase==DEFINITION_ORIGIN_OPEN);
    bool dirty=b.dirty; CHECK(definition_origin_seal(&c,p,SIZE_MAX,NULL)==DEFINITION_ORIGIN_APPLIED && b.dirty==dirty && definition_origin_state(p).phase==DEFINITION_ORIGIN_ADMITTED);
    DefinitionOriginAssets assets=definition_origin_assets(p); DefinitionOriginState state=definition_origin_state(p);
    CHECK(assets.primary && assets.objects && assets.sources && !assets.formats && !assets.format_bytes && state.asset_bytes==assets.primary_bytes+assets.object_bytes+assets.source_bytes);
    unsigned char *immutable=dx_alloc(state.asset_bytes); memcpy(immutable,assets.primary,state.asset_bytes);
    CHECK(definition_origin_complete(&c,p,&r,SIZE_MAX,NULL)==DEFINITION_ORIGIN_PROTECTED && definition_origin_seal(&c,p,SIZE_MAX,NULL)==DEFINITION_ORIGIN_PROTECTED);
    AnalysisAsset a={0}; ObjectsAsset o={0}; SourceVersionsAsset sources={0};
    CHECK(analysis_asset_decode(assets.primary,assets.primary_bytes,assets.object_high_water,SIZE_MAX,&a)==ANALYSIS_ASSET_OK);
    CHECK(objects_asset_decode(assets.objects,assets.object_bytes,SIZE_MAX,&o)==OBJECTS_ASSET_OK && source_versions_asset_decode(assets.sources,assets.source_bytes,assets.object_high_water,SIZE_MAX,&sources)==SOURCE_VERSIONS_CODEC_OK);
    CHECK(table ? !strcmp(o.objects.items[0].source.table,"alpha") : !strcmp(a.definition.table,"alpha"));
    analysis_asset_free(&a); objects_asset_free(&o); source_versions_asset_free(&sources);
    analysis_free(&resolved); analysis_free(&live); workbook_free(&b); workbook_free(saved); free(saved);
    CHECK(!memcmp(immutable,assets.primary,state.asset_bytes) && definition_origin_state(p).asset_bytes==state.asset_bytes); free(immutable); definition_origin_free(&p); CHECK(!p);
}
static void reject(DefinitionOriginContext *c,DefinitionOrigin *p,const DefinitionOriginReply *r,DefinitionOriginStatus expected) {
    Workbook before=*c->book; Worksheet sheet=c->book->sheets[0]; DefinitionOriginState state=definition_origin_state(p); DefinitionOriginCompletionInfo info={7,8,9},old=info; char *error=NULL;
    CHECK(definition_origin_completion_measure(c,p,r,&info,&error)==expected && error && !memcmp(&info,&old,sizeof(info))); free(error); error=NULL;
    CHECK(definition_origin_complete(c,p,r,SIZE_MAX,&error)==expected && error && !memcmp(&before,c->book,sizeof(before)) && !memcmp(&sheet,c->book->sheets,sizeof(sheet)));
    DefinitionOriginState after=definition_origin_state(p); CHECK(state.epoch==after.epoch && state.phase==after.phase && state.asset_bytes==after.asset_bytes); free(error);
}
static void fences(bool table) {
    for (unsigned mode=0;mode<9;mode++) {
        Workbook b=book(table,false,false); uint64_t request=10,epoch=20; DefinitionOriginContext c={&b,&b.sheets[0].analysis,&request,&epoch,SIZE_MAX,true,false};
        DefinitionOrigin *p=begin(&c); Analysis resolved={0}; DefinitionOriginReply r=reply(&c,table,false,&resolved);
        if (mode==0) r.request++;
        if (mode==1) r.revision++;
        if (mode==2) request++;
        if (mode==3) epoch++;
        if (mode==4) b.command_revision++;
        if (mode==5) c.ready=false;
        if (mode==6) c.read_only=true;
        if (mode==7) b.sheets[0].formats.revision++;
        if (mode==8) resolved.unique_rows=true;
        reject(&c,p,&r,mode==5 || mode==6 ? DEFINITION_ORIGIN_PROTECTED : DEFINITION_ORIGIN_STALE);
        analysis_free(&resolved); definition_origin_free(&p); workbook_free(&b);
    }
}
static void validation_and_formats(void) {
    Workbook b=book(false,false,true); uint64_t request=1,epoch=1; DefinitionOriginContext c={&b,&b.sheets[0].analysis,&request,&epoch,SIZE_MAX,true,false};
    DefinitionOrigin *p=begin(&c); Analysis resolved={0}; DefinitionOriginReply r=reply(&c,false,false,&resolved); DefinitionOriginCompletionInfo info={0};
    CHECK(definition_origin_completion_measure(&c,p,&r,&info,NULL)==DEFINITION_ORIGIN_READY && info.validation_scratch>0);
    c.validation_reserved=0; CHECK(definition_origin_complete(&c,p,&r,SIZE_MAX,NULL)==DEFINITION_ORIGIN_LIMIT && !b.sheets[0].analysis.table);
    c.validation_reserved=info.validation_scratch; CHECK(definition_origin_complete(&c,p,&r,SIZE_MAX,NULL)==DEFINITION_ORIGIN_APPLIED);
    c.validation_reserved=0; CHECK(definition_origin_seal(&c,p,SIZE_MAX,NULL)==DEFINITION_ORIGIN_LIMIT && !definition_origin_state(p).asset_bytes);
    c.validation_reserved=SIZE_MAX; CHECK(definition_origin_seal(&c,p,SIZE_MAX,NULL)==DEFINITION_ORIGIN_APPLIED);
    definition_origin_free(&p); analysis_free(&resolved); workbook_free(&b);
    b=book(false,false,false); free(b.sheets[0].analysis.path); b.sheets[0].analysis.path=dx_strdup("/source.csv"); b.sheets[0].analysis.kind=SOURCE_CSV;
    CellFormat style=format_default(); style.flags=FORMAT_BOLD; CHECK(format_apply(&b.sheets[0].formats,(FormatRange){2,2,0,0},FORMAT_BOLD,style,NULL)); worksheet_record(b.sheets,EDIT_FORMAT); workbook_history_note_edit(&b);
    c=(DefinitionOriginContext){&b,&b.sheets[0].analysis,&request,&epoch,SIZE_MAX,true,false}; p=begin(&c);
    CHECK(definition_origin_seal(&c,p,SIZE_MAX,NULL)==DEFINITION_ORIGIN_UNRESOLVED); definition_origin_free(&p);
    b.sheets[0].formats.source_aware=true; p=begin(&c); CHECK(definition_origin_seal(&c,p,SIZE_MAX,NULL)==DEFINITION_ORIGIN_APPLIED);
    DefinitionOriginAssets assets=definition_origin_assets(p); FormatDelta formats={0}; CHECK(assets.format_bytes && format_delta_decode(assets.formats,assets.format_bytes,SIZE_MAX,&formats)==FORMAT_DELTA_OK && formats.after.source_aware && formats.after.count==1);
    format_delta_free(&formats); definition_origin_free(&p); workbook_free(&b);
}
int main(void) {
    basics(false,false,false); basics(false,false,true); basics(true,false,false); basics(true,true,false); fences(false); fences(true); validation_and_formats();
    definition_origin_free(NULL); printf("PASS: %u immutable working definition origin checks\n",checks); return 0;
}
