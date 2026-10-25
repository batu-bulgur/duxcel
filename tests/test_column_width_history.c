#include "workbook/column_width_history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL width history:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static bool same(ColumnWidthHistoryInfo a,ColumnWidthHistoryInfo b) { return a.undo==b.undo && a.redo==b.redo && a.owned_bytes==b.owned_bytes && a.generation==b.generation; }
static void edit(ColumnWidthContext c,size_t column,float width) {
    ColumnWidthState next={0}; ColumnWidthPlan *p=NULL;
    CHECK(column_width_resize(column_width_view(c.current),column,width,SIZE_MAX,&next)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prepare(c,column_width_view(&next),&p)==COLUMN_WIDTH_OK);
    CHECK(column_width_plan_target(p).entries!=next.entries);
    column_width_free(&next); CHECK(column_width_history_commit(c,&p)==COLUMN_WIDTH_OK && !p);
}
static void replay(ColumnWidthContext c,bool redo) {
    ColumnWidthPlan *p=NULL; uint64_t revision=*c.revision;
    CHECK(column_width_history_replay(c,redo,&p)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_commit(c,&p)==COLUMN_WIDTH_OK && !p && *c.revision==revision+1);
}
int main(void) {
    ColumnWidthHistory *h=NULL; size_t allocation=column_width_history_allocation();
    CHECK(column_width_history_new((ColumnWidthHistoryOptions){33,100},allocation,&h)==COLUMN_WIDTH_INVALID && !h);
    CHECK(column_width_history_new((ColumnWidthHistoryOptions){32,DX_COLUMN_WIDTH_HISTORY_BYTES},allocation-1,&h)==COLUMN_WIDTH_LIMIT && !h);
    CHECK(column_width_history_new((ColumnWidthHistoryOptions){32,DX_COLUMN_WIDTH_HISTORY_BYTES},allocation,&h)==COLUMN_WIDTH_OK);
    ColumnWidthState current={0},next={0}; uint64_t revision=7;
    ColumnWidthContext c={&current,&revision,h,0}; ColumnWidthPlan *p=NULL,*q=NULL;
    CHECK(column_width_history_prepare(c,column_width_view(&current),&p)==COLUMN_WIDTH_NOOP && !p);
    CHECK(column_width_resize(column_width_view(&current),51,120,SIZE_MAX,&next)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prepare(c,column_width_view(&next),&p)==COLUMN_WIDTH_LIMIT && !p && !current.count && revision==7);
    c.staging_reserved=SIZE_MAX; CHECK(column_width_history_prepare(c,column_width_view(&next),&p)==COLUMN_WIDTH_OK);
    size_t owned=column_width_plan_owned(p); ColumnWidthHistoryInfo before=column_width_history_info(h);
    CHECK(column_width_plan_packet(p)->bytes==26 && !before.undo && !before.owned_bytes);
    c.staging_reserved=owned-1; CHECK(column_width_history_prepare(c,column_width_view(&next),&q)==COLUMN_WIDTH_LIMIT && !q && same(before,column_width_history_info(h)));
    c.staging_reserved=owned; CHECK(column_width_history_prepare(c,column_width_view(&next),&q)==COLUMN_WIDTH_OK); column_width_plan_free(&q);
    column_width_free(&next); CHECK(column_width_history_commit(c,&p)==COLUMN_WIDTH_OK && revision==8 && current.count==1 && !p);
    CHECK(column_width_at(column_width_view(&current),51)==column_width_bits(120)); c.staging_reserved=SIZE_MAX;
    replay(c,false); CHECK(!current.count && revision==9 && column_width_history_info(h).redo==1);
    before=column_width_history_info(h); revision=UINT64_MAX; c.staging_reserved=0;
    CHECK(column_width_history_prepare(c,column_width_view(&current),&p)==COLUMN_WIDTH_NOOP && !p && same(before,column_width_history_info(h)) && revision==UINT64_MAX);
    CHECK(column_width_history_replay(c,true,&p)==COLUMN_WIDTH_LIMIT && !p); revision=9; c.staging_reserved=SIZE_MAX;
    replay(c,true); CHECK(revision==10 && column_width_at(column_width_view(&current),51)==column_width_bits(120));
    current.entries[0].bits=column_width_bits(121); before=column_width_history_info(h);
    CHECK(column_width_history_replay(c,false,&p)==COLUMN_WIDTH_STALE && !p && revision==10 && same(before,column_width_history_info(h)));
    CHECK(column_width_resize(column_width_view(&current),51,122,SIZE_MAX,&next)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prepare(c,column_width_view(&next),&p)==COLUMN_WIDTH_STALE && !p && same(before,column_width_history_info(h))); column_width_free(&next);
    current.entries[0].bits=column_width_bits(120);
    CHECK(column_width_history_replay(c,false,&p)==COLUMN_WIDTH_OK);
    uint64_t other_revision=revision; ColumnWidthContext other=c; other.revision=&other_revision;
    CHECK(column_width_history_commit(other,&p)==COLUMN_WIDTH_STALE && p && revision==10 && same(before,column_width_history_info(h)));
    revision++; CHECK(column_width_history_commit(c,&p)==COLUMN_WIDTH_STALE && p); revision--; column_width_plan_free(&p);
    CHECK(column_width_history_replay(c,false,&p)==COLUMN_WIDTH_OK && column_width_history_replay(c,false,&q)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_commit(c,&p)==COLUMN_WIDTH_OK && !p);
    CHECK(column_width_history_commit(c,&q)==COLUMN_WIDTH_STALE && q); column_width_plan_free(&q);
    edit(c,0,52); CHECK(column_width_history_info(h).redo==0); /* divergent branch discards Redo only on commit */
    /* Full before/after pair restores true truncated right edge exactly. */
    edit(c,16383,1200); ColumnWidthState original={0}; CHECK(column_width_copy(column_width_view(&current),SIZE_MAX,&original)==COLUMN_WIDTH_OK);
    CHECK(column_width_shift(column_width_view(&current),0,true,SIZE_MAX,&next)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prepare(c,column_width_view(&next),&p)==COLUMN_WIDTH_OK); column_width_free(&next);
    CHECK(column_width_history_commit(c,&p)==COLUMN_WIDTH_OK && column_width_at(column_width_view(&current),16383)==DX_COLUMN_WIDTH_DEFAULT_BITS);
    replay(c,false); CHECK(column_width_equal(column_width_view(&current),column_width_view(&original)) && column_width_at(column_width_view(&current),16383)==column_width_bits(1200));
    replay(c,true); column_width_free(&original); column_width_free(&current); column_width_history_free(&h);
    /* Entry and byte eviction are only prospective until complete preparation. */
    CHECK(column_width_history_new((ColumnWidthHistoryOptions){2,60},allocation,&h)==COLUMN_WIDTH_OK); revision=0; c=(ColumnWidthContext){&current,&revision,h,SIZE_MAX};
    edit(c,0,100); edit(c,0,101); before=column_width_history_info(h); CHECK(before.undo==2 && before.owned_bytes==58);
    CHECK(column_width_resize(column_width_view(&current),0,102,SIZE_MAX,&next)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prepare(c,column_width_view(&next),&p)==COLUMN_WIDTH_OK && same(before,column_width_history_info(h)) && revision==2);
    column_width_plan_free(&p); CHECK(same(before,column_width_history_info(h)) && current.entries[0].bits==column_width_bits(101));
    CHECK(column_width_history_prepare(c,column_width_view(&next),&p)==COLUMN_WIDTH_OK && column_width_history_commit(c,&p)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_info(h).undo==1 && column_width_history_info(h).owned_bytes==32 && revision==3);
    replay(c,false); CHECK(current.entries[0].bits==column_width_bits(101));
    CHECK(column_width_history_replay(c,false,&p)==COLUMN_WIDTH_EMPTY && !p); column_width_free(&next); column_width_free(&current); column_width_history_free(&h);
    CHECK(column_width_history_new((ColumnWidthHistoryOptions){1,26},allocation,&h)==COLUMN_WIDTH_OK); c=(ColumnWidthContext){&current,&revision,h,SIZE_MAX};
    edit(c,0,100); before=column_width_history_info(h);
    CHECK(column_width_resize(column_width_view(&current),0,101,SIZE_MAX,&next)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prepare(c,column_width_view(&next),&p)==COLUMN_WIDTH_LIMIT && !p && same(before,column_width_history_info(h)));
    column_width_free(&next); column_width_free(&current); column_width_history_free(&h);
    CHECK(column_width_history_new((ColumnWidthHistoryOptions){32,DX_COLUMN_WIDTH_HISTORY_BYTES},allocation,&h)==COLUMN_WIDTH_OK); c=(ColumnWidthContext){&current,&revision,h,SIZE_MAX};
    for (unsigned i=0;i<40;i++) edit(c,16383,52+i);
    CHECK(column_width_history_info(h).undo==32);
    for (unsigned i=0;i<32;i++) replay(c,false);
    CHECK(column_width_history_replay(c,false,&p)==COLUMN_WIDTH_EMPTY && !p && column_width_history_info(h).redo==32);
    for (unsigned i=0;i<32;i++) replay(c,true);
    CHECK(column_width_at(column_width_view(&current),16383)==column_width_bits(91));
    column_width_free(&current); column_width_history_free(&h); column_width_plan_free(&p); column_width_plan_free(NULL); column_width_history_free(NULL);
    printf("PASS: %u bounded column width journal checks; exact inverse, monotonic revision and divergence guards\n",checks); return 0;
}
