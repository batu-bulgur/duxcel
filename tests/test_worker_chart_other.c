#include "data/worker.h"
#include "ui/chartencoding.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static uint64_t sequence;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL chart Other worker:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const char *weighted="SELECT * FROM (VALUES ('A',100,100,'a'),('B',90,100,'b'),('C',80,100,'c'),('D',2,0,'x'),('E',1,10,'x'),('E',1,10,'y'),('E',1,10,'z')) t(r,rank,v,customer)";
static QueryJob *request(bool other) {
    QueryJob *j=dx_alloc(sizeof(*j)); j->id=++sequence; j->kind=JOB_PIVOT;
    j->object_id=UINT64_C(9007199254740993); j->revision=UINT64_C(9007199254740995);
    j->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(weighted)};
    j->known_total=-1; j->include_totals=true; j->include_chart_other=other;
    char *error=NULL;
    CHECK(pivot_assign(&j->pivot,PIVOT_ROWS,"r",AGG_SUM,&error));
    CHECK(pivot_assign(&j->pivot,PIVOT_VALUES,"rank",AGG_SUM,&error));
    CHECK(pivot_assign(&j->pivot,PIVOT_VALUES,"v",AGG_AVG,&error));
    CHECK(pivot_assign(&j->pivot,PIVOT_VALUES,"customer",AGG_DISTINCT,&error));
    CHECK(!error); return j;
}
static QueryReply *wait_reply(QueryWorker *w) {
    double deadline=dx_millis()+10000;
    while (dx_millis()<deadline) { QueryReply *r=worker_poll(w); if (r) return r; dx_sleep_ms(1); }
    CHECK(!"reply timeout"); return NULL;
}
static void wait_idle(QueryWorker *w) {
    double deadline=dx_millis()+10000;
    while (dx_millis()<deadline) {
        dx_lock(&w->mutex); bool idle=!w->running && !w->pending; dx_unlock(&w->mutex);
        if (idle) return; dx_sleep_ms(1);
    }
    CHECK(!"worker timeout");
}
static void result(QueryReply *r,uint64_t id,bool other) {
    CHECK(r && r->id==id && !r->error && r->kind==JOB_PIVOT);
    CHECK(r->object_id==UINT64_C(9007199254740993) && r->revision==UINT64_C(9007199254740995));
    CHECK(r->page.row_count==5 && r->page.column_count==4 && r->page.total==5 && r->page.totals);
    CHECK(r->page.other_status==(other ? PAGE_OTHER_READY : PAGE_OTHER_NONE));
    CHECK((r->page.other!=NULL)==other);
    if (other) {
        const Page *values=page_other_values(&r->page,1); CHECK(values && values==page_other_values(&r->page,2));
        CHECK(values->column_count==3 && values->row_count==1);
        CHECK(!values->cells[1].is_null && fabs(values->cells[1].number-7.5)<1e-12);
        CHECK(!values->cells[2].is_null && values->cells[2].number==3 && !strcmp(values->cells[2].text,"3"));
    }
}
int main(void) {
    QueryWorker w; CHECK(worker_start(&w));
    /* Invalid opt-in payloads cannot open the engine or perform another job. */
    QueryJob *j=request(true); j->kind=JOB_PAGE; uint64_t id=j->id;
    worker_submit(&w,j); QueryReply *r=wait_reply(&w);
    CHECK(r->id==id && r->error && !r->page.cells && !r->page.other); reply_free(r); wait_idle(&w);
    CHECK(!w.db.connection && !w.db.query_count);
    j=request(true); j->is_table=true; id=j->id; worker_submit(&w,j); r=wait_reply(&w);
    CHECK(r->id==id && r->error && !r->page.cells && !r->page.other); reply_free(r); wait_idle(&w);
    CHECK(!w.db.connection && !w.db.query_count);
    j=request(false); id=j->id; worker_submit(&w,j); r=wait_reply(&w); result(r,id,false); reply_free(r);
    j=request(true); PivotDefinition d=pivot_copy(&j->pivot); id=j->id;
    worker_submit(&w,j); r=wait_reply(&w); result(r,id,true); wait_idle(&w);
    uint64_t sql=w.db.query_count,jobs=w.jobs_started;
    for (unsigned i=0;i<60;i++) {
        UIChartView view; size_t measure=i%3; CHECK(ui_chart_view(&r->page,&d,measure,&view));
        CHECK(view.other && view.other_values && view.series==4 && !view.non_additive);
        CHECK(view.values[3][0]==(measure==0 ? 5 : measure==1 ? 7.5 : 3));
        UIEncodingValue value=ui_encoding_value(&r->page,&d,&view,(UIEncodingBinding){UI_BIND_MEASURE,2},3,0);
        CHECK(value.available && value.numeric && !value.is_null && !value.approximate && value.raw && !strcmp(value.raw,"3"));
    }
    CHECK(w.db.query_count==sql && w.jobs_started==jobs && !worker_poll(&w)); reply_free(r);
    /* A superseded Other request must never publish as the latest job. The
       older request can be pending or active when it is superseded. */
    j=request(true); worker_submit(&w,j); j=request(true); id=j->id; worker_submit(&w,j);
    r=wait_reply(&w); result(r,id,true); wait_idle(&w);
    worker_stop(&w); CHECK(!w.started);
    /* Polled pages/typed strings remain owned after connection shutdown. */
    result(r,id,true); reply_free(r); pivot_free(&d);
    CHECK(!w.reply && !w.pending);
    printf("PASS: %u chart Other worker checks (owned replies, query-free Plot/Marks, supersession and shutdown; no GUI)\n",checks);
    return 0;
}
