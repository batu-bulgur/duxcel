#include "workbook/column_width_command.h"
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
static void exhausted_retention(void) {
    ColumnWidthHistory *h=NULL; CHECK(column_width_history_new((ColumnWidthHistoryOptions){32,4096},column_width_history_allocation(),&h)==COLUMN_WIDTH_OK);
    ColumnWidthState current={0},next={0}; uint64_t revision=10; ColumnWidthContext c={&current,&revision,h,SIZE_MAX};
    edit(c,0,100); edit(c,0,101); replay(c,false);
    ColumnWidthPlan *pending=NULL; CHECK(column_width_history_replay(c,true,&pending)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_test_generation(h,UINT64_MAX));
    CHECK(column_width_history_commit(c,&pending)==COLUMN_WIDTH_STALE && pending); column_width_plan_free(&pending);
    ColumnWidthHistoryInfo before=column_width_history_info(h); uint64_t old_revision=revision;
    CHECK(column_width_history_prepare(c,column_width_view(&current),&pending)==COLUMN_WIDTH_NOOP && same(before,column_width_history_info(h)));
    CHECK(column_width_resize(column_width_view(&current),0,102,SIZE_MAX,&next)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prepare(c,column_width_view(&next),&pending)==COLUMN_WIDTH_LIMIT && !pending && same(before,column_width_history_info(h)));
    CHECK(column_width_history_replay(c,true,&pending)==COLUMN_WIDTH_LIMIT && !pending);
    ColumnWidthPrune clear;
    CHECK(column_width_history_prune_prepare(h,1,0,&clear)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prune_commit(h,&clear)==COLUMN_WIDTH_OK && revision==old_revision && current.entries[0].bits==column_width_bits(100));
    CHECK(column_width_history_info(h).generation==UINT64_MAX && !column_width_history_info(h).redo && column_width_history_info(h).undo==1);
    CHECK(column_width_history_prune_commit(h,&clear)==COLUMN_WIDTH_STALE);
    CHECK(column_width_history_clear_redo(h)==COLUMN_WIDTH_NOOP);
    CHECK(column_width_history_trim(h,0,0)==COLUMN_WIDTH_OK && !column_width_history_info(h).owned_bytes && revision==old_revision && column_width_history_info(h).generation==UINT64_MAX);
    CHECK(!column_width_history_test_generation(h,0));
    column_width_free(&next); column_width_free(&current); column_width_history_free(&h);
}
static void authority(void) {
    SheetView v={.row=17,.canvas_zoom=1.5f,.column_count=2};
    v.widths=dx_alloc(2*sizeof(float)); v.widths[0]=96; v.widths[1]=120;
    v.columns=dx_alloc(2*sizeof(char *)); v.columns[0]=dx_strdup("City"); v.columns[1]=dx_strdup("Amount");
    char *city=v.columns[0],*amount=v.columns[1];
    ColumnWidthHistory *h=NULL; uint64_t revision=9;
    ColumnWidthCommandContext c={&v,&h,&revision,0}; ColumnWidthCommand *p=NULL,*q=NULL;
    ColumnWidthEntry entries[]={{1,column_width_bits(120)},{16383,column_width_bits(1200)}};
    ColumnWidthView after={entries,2}; size_t owned=123;
    CHECK(column_width_command_measure(c,after,&owned)==COLUMN_WIDTH_OK && owned);
    CHECK(column_width_command_prepare(c,after,&p)==COLUMN_WIDTH_LIMIT && !p && !h && revision==9 && v.column_count==2);
    c.reserved=owned; CHECK(column_width_command_prepare(c,after,&p)==COLUMN_WIDTH_OK && !h && v.column_count==2);
    CHECK(column_width_command_check(c,p)==COLUMN_WIDTH_OK && column_width_command_packet(p)->bytes==38);
    ColumnWidthHistoryInfo projected; CHECK(column_width_command_history(p,&projected)==COLUMN_WIDTH_OK && projected.undo==1);
    column_width_command_cancel(&p); CHECK(!p && !h && v.columns[0]==city && revision==9);
    CHECK(column_width_command_prepare(c,after,&p)==COLUMN_WIDTH_OK);
    CHECK(column_width_command_prepare(c,after,&q)==COLUMN_WIDTH_OK);
    v.row=88; v.canvas_zoom=2;
    CHECK(column_width_command_adopt(c,&p)==COLUMN_WIDTH_OK && !p && h && revision==10);
    CHECK(v.row==88 && v.canvas_zoom==2 && v.column_count==16384 && v.widths[16383]==1200 && v.widths[100]==96);
    CHECK(v.columns[0]==city && v.columns[1]==amount && v.columns[16383] && !*v.columns[16383]);
    CHECK(column_width_command_check(c,q)==COLUMN_WIDTH_STALE); column_width_command_cancel(&q);
    c.reserved=0; revision=UINT64_MAX;
    CHECK(column_width_command_prepare(c,after,&p)==COLUMN_WIDTH_NOOP && !p && column_width_history_info(h).undo==1);
    revision=10; c.reserved=SIZE_MAX;
    CHECK(column_width_command_replay(c,false,&p)==COLUMN_WIDTH_OK);
    v.widths[1]=121; CHECK(column_width_command_adopt(c,&p)==COLUMN_WIDTH_STALE && p && revision==10);
    v.widths[1]=120; CHECK(column_width_command_adopt(c,&p)==COLUMN_WIDTH_OK && revision==11 && v.widths[16383]==96 && v.column_count==16384);
    CHECK(v.columns[0]==city && v.columns[1]==amount && column_width_history_info(h).redo==1);
    CHECK(column_width_command_replay(c,true,&p)==COLUMN_WIDTH_OK);
    CHECK(column_width_command_adopt(c,&p)==COLUMN_WIDTH_OK && revision==12 && v.widths[16383]==1200);
    CHECK(column_width_command_replay(c,false,&p)==COLUMN_WIDTH_OK); revision++;
    CHECK(column_width_command_check(c,p)==COLUMN_WIDTH_STALE); column_width_command_cancel(&p);
    for (size_t i=0;i<v.column_count;i++) free(v.columns[i]); free(v.columns); free(v.widths); column_width_history_free(&h);
}
static void retention(void) {
    ColumnWidthHistory *h=NULL,*other=NULL;
    size_t allocation=column_width_history_allocation();
    CHECK(column_width_history_new((ColumnWidthHistoryOptions){32,4096},allocation,&h)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_new((ColumnWidthHistoryOptions){32,4096},allocation,&other)==COLUMN_WIDTH_OK);
    ColumnWidthState current={0},next={0}; uint64_t revision=200;
    ColumnWidthContext c={&current,&revision,h,SIZE_MAX};
    for (unsigned i=0;i<5;i++) edit(c,0,100+i);
    replay(c,false); replay(c,false);
    CHECK(column_width_at(column_width_view(&current),0)==column_width_bits(102));
    ColumnWidthHistoryInfo before=column_width_history_info(h);
    CHECK(before.undo==3 && before.redo==2 && before.owned_bytes==154);
    ColumnWidthPrune prune={0};
    CHECK(column_width_history_prune_prepare(h,2,1,&prune)==COLUMN_WIDTH_OK);
    CHECK(same(before,prune.before) && prune.released_bytes==58 && prune.retained_bytes==96 && prune.owner==h);
    CHECK(same(before,column_width_history_info(h)));
    ColumnWidthPlan *undo=NULL,*redo=NULL,*new_edit=NULL;
    CHECK(column_width_history_replay(c,false,&undo)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_replay(c,true,&redo)==COLUMN_WIDTH_OK);
    CHECK(column_width_resize(column_width_view(&current),0,110,SIZE_MAX,&next)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prepare(c,column_width_view(&next),&new_edit)==COLUMN_WIDTH_OK); column_width_free(&next);
    uint64_t prior_revision=revision; ColumnWidthState prior=current;
    CHECK(column_width_history_prune_commit(h,&prune)==COLUMN_WIDTH_OK);
    CHECK(revision==prior_revision && current.entries==prior.entries && current.count==prior.count && current.entries[0].bits==column_width_bits(102));
    ColumnWidthHistoryInfo after=column_width_history_info(h);
    CHECK(after.undo==2 && after.redo==1 && after.owned_bytes==96 && after.generation==before.generation+1);
    CHECK(column_width_history_commit(c,&undo)==COLUMN_WIDTH_STALE && undo);
    CHECK(column_width_history_commit(c,&redo)==COLUMN_WIDTH_STALE && redo);
    CHECK(column_width_history_commit(c,&new_edit)==COLUMN_WIDTH_STALE && new_edit);
    ColumnWidthHistoryInfo sentinel={55,66,77,88},inspected=sentinel;
    CHECK(column_width_plan_history(new_edit,&inspected)==COLUMN_WIDTH_STALE && same(inspected,sentinel));
    column_width_plan_free(&undo); column_width_plan_free(&redo); column_width_plan_free(&new_edit);
    CHECK(column_width_history_prune_commit(h,&prune)==COLUMN_WIDTH_STALE && same(after,column_width_history_info(h)));
    /* Trimming Redo drops the far future, retaining its immediate next step. */
    replay(c,true); CHECK(current.entries[0].bits==column_width_bits(103));
    replay(c,false); CHECK(current.entries[0].bits==column_width_bits(102));
    replay(c,false); CHECK(current.entries[0].bits==column_width_bits(101));
    replay(c,false); CHECK(current.entries[0].bits==column_width_bits(100));
    CHECK(column_width_history_replay(c,false,&undo)==COLUMN_WIDTH_EMPTY && !undo);
    prior_revision=revision; before=column_width_history_info(h);
    CHECK(column_width_history_clear_redo(h)==COLUMN_WIDTH_OK && revision==prior_revision && current.entries[0].bits==column_width_bits(100));
    after=column_width_history_info(h); CHECK(!after.undo && !after.redo && !after.owned_bytes && after.generation==before.generation+1);
    /* No-op/invalid retention leaves already prepared edits and their clocks. */
    CHECK(column_width_resize(column_width_view(&current),0,110,SIZE_MAX,&next)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prepare(c,column_width_view(&next),&new_edit)==COLUMN_WIDTH_OK); column_width_free(&next);
    ColumnWidthPrune untouched={0}; untouched.undo_keep=999; unsigned char bytes[sizeof(untouched)]; memcpy(bytes,&untouched,sizeof(bytes));
    before=column_width_history_info(h);
    CHECK(column_width_history_prune_prepare(h,0,0,&untouched)==COLUMN_WIDTH_NOOP && !memcmp(bytes,&untouched,sizeof(bytes)));
    CHECK(column_width_history_prune_prepare(h,1,0,&untouched)==COLUMN_WIDTH_INVALID && !memcmp(bytes,&untouched,sizeof(bytes)));
    CHECK(column_width_history_clear_redo(h)==COLUMN_WIDTH_NOOP && same(before,column_width_history_info(h)));
    CHECK(column_width_history_trim(h,1,0)==COLUMN_WIDTH_INVALID && same(before,column_width_history_info(h)));
    CHECK(column_width_history_commit(c,&new_edit)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prune_prepare(h,0,0,&prune)==COLUMN_WIDTH_OK);
    before=column_width_history_info(h); ColumnWidthHistoryInfo other_before=column_width_history_info(other);
    CHECK(column_width_history_prune_commit(other,&prune)==COLUMN_WIDTH_STALE && same(other_before,column_width_history_info(other)));
    ColumnWidthPrune changed=prune; changed.retained_bytes++;
    CHECK(column_width_history_prune_commit(h,&changed)==COLUMN_WIDTH_INVALID && same(before,column_width_history_info(h)));
    changed=prune; changed.released_bytes++;
    CHECK(column_width_history_prune_commit(h,&changed)==COLUMN_WIDTH_INVALID && same(before,column_width_history_info(h)));
    changed=prune; changed.undo_keep=2;
    CHECK(column_width_history_prune_commit(h,&changed)==COLUMN_WIDTH_INVALID && same(before,column_width_history_info(h)));
    replay(c,false); CHECK(column_width_history_prune_commit(h,&prune)==COLUMN_WIDTH_STALE);
    prior_revision=revision; CHECK(column_width_history_clear_redo(h)==COLUMN_WIDTH_OK && revision==prior_revision);
    /* Fold additional global-prefix pruning into the pending NEW width action,
       so no second retention operation can fail after model adoption. */
    edit(c,0,110); edit(c,0,111); edit(c,0,112);
    CHECK(column_width_resize(column_width_view(&current),0,113,SIZE_MAX,&next)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_prepare(c,column_width_view(&next),&new_edit)==COLUMN_WIDTH_OK); column_width_free(&next);
    before=column_width_history_info(h); prior_revision=revision;
    CHECK(column_width_plan_history(new_edit,&inspected)==COLUMN_WIDTH_OK && inspected.undo==4 && !inspected.redo && inspected.owned_bytes==128);
    revision++; inspected=sentinel;
    CHECK(column_width_plan_history(new_edit,&inspected)==COLUMN_WIDTH_STALE && same(inspected,sentinel));
    CHECK(column_width_plan_retain_undo(new_edit,2)==COLUMN_WIDTH_STALE && same(before,column_width_history_info(h)));
    revision--; current.entries[0].bits=column_width_bits(114); inspected=sentinel;
    CHECK(column_width_plan_history(new_edit,&inspected)==COLUMN_WIDTH_STALE && same(inspected,sentinel));
    CHECK(column_width_plan_retain_undo(new_edit,2)==COLUMN_WIDTH_STALE && same(before,column_width_history_info(h)));
    current.entries[0].bits=column_width_bits(112);
    CHECK(column_width_plan_retain_undo(new_edit,0)==COLUMN_WIDTH_INVALID);
    CHECK(column_width_plan_retain_undo(new_edit,5)==COLUMN_WIDTH_INVALID);
    CHECK(column_width_plan_retain_undo(new_edit,4)==COLUMN_WIDTH_NOOP && same(before,column_width_history_info(h)) && revision==prior_revision);
    CHECK(column_width_plan_retain_undo(new_edit,2)==COLUMN_WIDTH_OK && same(before,column_width_history_info(h)) && revision==prior_revision);
    CHECK(column_width_plan_history(new_edit,&inspected)==COLUMN_WIDTH_OK && inspected.undo==2 && !inspected.redo && inspected.owned_bytes==64 && inspected.generation==before.generation+1);
    CHECK(column_width_history_prune_prepare(h,2,0,&prune)==COLUMN_WIDTH_OK);
    CHECK(column_width_history_commit(c,&new_edit)==COLUMN_WIDTH_OK && same(inspected,column_width_history_info(h)));
    CHECK(column_width_history_prune_commit(h,&prune)==COLUMN_WIDTH_STALE);
    CHECK(column_width_history_replay(c,false,&undo)==COLUMN_WIDTH_OK);
    CHECK(column_width_plan_retain_undo(undo,1)==COLUMN_WIDTH_INVALID);
    CHECK(column_width_plan_history(undo,&inspected)==COLUMN_WIDTH_OK && inspected.undo==1 && inspected.redo==1 && inspected.owned_bytes==64);
    CHECK(column_width_history_commit(c,&undo)==COLUMN_WIDTH_OK && same(inspected,column_width_history_info(h)));
    replay(c,false); CHECK(current.entries[0].bits==column_width_bits(111));
    CHECK(column_width_history_replay(c,false,&undo)==COLUMN_WIDTH_EMPTY && !undo);
    prior_revision=revision; CHECK(column_width_history_trim(h,0,0)==COLUMN_WIDTH_OK && revision==prior_revision && !column_width_history_info(h).owned_bytes);
    CHECK(column_width_history_clear_redo(NULL)==COLUMN_WIDTH_INVALID);
    CHECK(column_width_history_prune_prepare(NULL,0,0,&prune)==COLUMN_WIDTH_INVALID);
    CHECK(column_width_history_prune_commit(h,NULL)==COLUMN_WIDTH_INVALID);
    column_width_free(&current); column_width_history_free(&h); column_width_history_free(&other);
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
    retention(); authority(); exhausted_retention();
    printf("PASS: %u bounded column width journal checks; exact inverse, monotonic revision and divergence guards\n",checks); return 0;
}
