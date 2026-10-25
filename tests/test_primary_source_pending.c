#include "workbook/primary_source_pending.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL pending:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void stages(unsigned stop,bool stale,bool publish) {
    Workbook b; workbook_init(&b); Analysis old={.kind=SOURCE_CSV,.path="/old.csv"};
    CHECK(workbook_add(&b,"Old",&old,NULL) && workbook_prepare_command_session(&b,0,NULL)); b.dirty=false;
    Analysis app=analysis_copy(&b.sheets[0].analysis),proposal={.kind=SOURCE_DATABASE,.path="/new.duckdb"};
    uint64_t request=8,epoch=4,order=worksheet_history_high_water();
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; Workbook before=b; Worksheet sheet=b.sheets[0];
    PrimarySourcePending *p=NULL; PrimarySourcePendingUsage u={0};
    CHECK(primary_source_pending_begin_measure(c,&proposal,"New",&u,NULL)==PRIMARY_TOPOLOGY_READY && !u.resident && u.required);
    CHECK(primary_source_pending_begin(c,&proposal,"New",u.required-1,&p,NULL)==PRIMARY_TOPOLOGY_LIMIT && !p);
    CHECK(primary_source_pending_begin(c,&proposal,"New",u.required,&p,NULL)==PRIMARY_TOPOLOGY_READY);
    CHECK(primary_source_pending_stage(p)==PRIMARY_PENDING_DRAFT && primary_source_pending_owned(p)>0);
    if (stop>0) {
        proposal.schema="main"; proposal.table="Sales"; PrimarySourceDraftReply reply={request,&proposal,NULL};
        CHECK(primary_source_pending_reply_measure(c,p,reply,&u,NULL)==PRIMARY_TOPOLOGY_READY && u.resident==primary_source_pending_owned(p) && u.required>u.resident);
        size_t resident=u.resident;
        CHECK(primary_source_pending_resolve(c,p,reply,u.required-1,NULL)==PRIMARY_TOPOLOGY_LIMIT && primary_source_pending_owned(p)==resident && primary_source_pending_stage(p)==PRIMARY_PENDING_DRAFT);
        CHECK(primary_source_pending_resolve(c,p,reply,u.required,NULL)==PRIMARY_TOPOLOGY_READY && primary_source_pending_stage(p)==PRIMARY_PENDING_RESOLVED);
        CHECK(primary_source_pending_resolve(c,p,reply,SIZE_MAX,NULL)==PRIMARY_TOPOLOGY_INVALID);
    }
    if (stop>1) {
        Column column={.name="Time",.type=DUCKDB_TYPE_TIMESTAMP}; SheetSourceView *view=NULL;
        CHECK(sheet_source_view_build((SheetSourceViewSpec){&column,1,{.canvas_zoom=1}},SIZE_MAX,&view)==SHEET_SOURCE_VIEW_OK);
        PrimarySourceInitialSchema schema={request,epoch,&proposal,&column,1,NULL};
        CHECK(primary_source_pending_initial_measure(c,p,view,schema,&u,NULL)==PRIMARY_TOPOLOGY_READY && u.resident==primary_source_pending_owned(p));
        size_t resident=u.resident;
        CHECK(primary_source_pending_prepare_initial(c,p,&view,schema,u.required-1,NULL)==PRIMARY_TOPOLOGY_LIMIT && view && primary_source_pending_owned(p)==resident && primary_source_pending_stage(p)==PRIMARY_PENDING_RESOLVED);
        CHECK(primary_source_pending_prepare_initial(c,p,&view,schema,u.required,NULL)==PRIMARY_TOPOLOGY_READY && !view && primary_source_pending_stage(p)==PRIMARY_PENDING_SOURCE);
    }
    if (stop>2) {
        CHECK(primary_source_pending_topology_measure(c,p,&u,NULL)==PRIMARY_TOPOLOGY_READY && u.resident==primary_source_pending_owned(p));
        size_t resident=u.resident;
        CHECK(primary_source_pending_prepare_topology(c,p,u.required-1,NULL)==PRIMARY_TOPOLOGY_LIMIT && primary_source_pending_owned(p)==resident && primary_source_pending_stage(p)==PRIMARY_PENDING_SOURCE);
        CHECK(primary_source_pending_prepare_topology(c,p,u.required,NULL)==PRIMARY_TOPOLOGY_READY && primary_source_pending_stage(p)==PRIMARY_PENDING_TOPOLOGY);
    }
    CHECK(primary_source_pending_check(c,p,NULL)==PRIMARY_TOPOLOGY_READY);
    CHECK(!memcmp(&b,&before,sizeof(b)) && !memcmp(b.sheets,&sheet,sizeof(sheet)) && worksheet_history_high_water()==order);
    if (stale) {
        request++; CHECK(primary_source_pending_check(c,p,NULL)==PRIMARY_TOPOLOGY_STALE);
        /* Free after all borrowed authorities die: disposal owns only values. */
        analysis_free(&app); workbook_free(&b); primary_source_pending_free(&p); CHECK(!p); return;
    }
    if (publish) {
        PrimarySourcePublication out={0}; CHECK(primary_source_pending_topology_measure(c,p,&u,NULL)==PRIMARY_TOPOLOGY_READY);
        CHECK(primary_source_pending_apply(c,&p,NULL,u.required-1,&out,NULL)==PRIMARY_TOPOLOGY_LIMIT && p && !out.sheet);
        CHECK(!memcmp(&b,&before,sizeof(b)) && worksheet_history_high_water()==order);
        CHECK(primary_source_pending_apply(c,&p,NULL,u.required,&out,NULL)==PRIMARY_TOPOLOGY_APPLIED && !p);
        CHECK(b.count==2 && b.command_revision==before.command_revision+1 && worksheet_history_high_water()==order+1 && out.sheet==2 && b.sheets[1].view.widths[0]==190);
        primary_source_publication_free(&out);
    }
    primary_source_pending_free(&p); CHECK(!p && primary_source_pending_stage(p)==PRIMARY_PENDING_EMPTY && !primary_source_pending_owned(p));
    analysis_free(&app); workbook_free(&b);
}
int main(void) {
    for (unsigned stage=0;stage<4;stage++) { stages(stage,false,false); stages(stage,true,false); }
    stages(3,false,true); primary_source_pending_free(NULL);
    CHECK(primary_source_pending_check((PrimarySourceDraftContext){0},NULL,NULL)==PRIMARY_TOPOLOGY_INVALID);
    printf("PASS: %u pending primary ownership checks\n",checks); return 0;
}
