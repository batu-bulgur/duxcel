#include "workbook/objects.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL objects:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
static void clear_error(char **error) { CHECK(*error && **error); free(*error); *error=NULL; }
static void text_equal(const char *a,const char *b,bool independent) {
    CHECK(a && b ? !strcmp(a,b) : a==b); if (a && independent) CHECK(a!=b);
}
/* Compare the complete scalar definitions as well as each owned logical string.
   The only permitted scalar difference is the planned Table version. */
static void same_snapshot(ObjectSnapshot a,ObjectSnapshot b,size_t changed_index,uint64_t version,bool independent) {
    CHECK(a.count==b.count);
    for (size_t i=0;i<a.count;i++) {
        const SheetObject *x=&a.items[i]; SheetObject y=b.items[i];
#define TEXT(member) do { text_equal(x->member,y.member,independent); y.member=x->member; } while (0)
        TEXT(name); TEXT(source.path); TEXT(source.schema); TEXT(source.table); TEXT(source.query);
        TEXT(source.filter_column); TEXT(source.filter_value); TEXT(source.group_column); TEXT(source.measure_column); TEXT(source.sort_column);
        CHECK(x->source.filter_count==y.source.filter_count && x->source.sort_count==y.source.sort_count);
        for (size_t j=0;j<x->source.filter_count;j++) { TEXT(source.filters[j].column); TEXT(source.filters[j].value); }
        for (size_t j=0;j<x->source.sort_count;j++) TEXT(source.sorts[j].column);
        for (int r=0;r<4;r++) { CHECK(x->pivot.counts[r]==y.pivot.counts[r]); for (size_t j=0;j<x->pivot.counts[r];j++) { TEXT(pivot.fields[r][j].name); TEXT(pivot.fields[r][j].value); } }
        for (int r=0;r<DX_MARK_ROLES;r++) { CHECK(x->marks.counts[r]==y.marks.counts[r]); for (size_t j=0;j<x->marks.counts[r];j++) TEXT(marks.fields[r][j].name); }
#undef TEXT
        if (i==changed_index) { CHECK(y.source.source_version==version); y.source.source_version=x->source.source_version; }
        CHECK(!memcmp(x,&y,sizeof(y)));
    }
}
static ObjectSnapshot current(const ObjectStore *s) { return (ObjectSnapshot){s->items,s->count}; }
static size_t current_bytes(const ObjectStore *s) { ObjectSnapshot state=current(s); return objects_snapshot_owned_bytes(&state); }
static void unchanged(const ObjectStore *s,const ObjectStore *before,const SheetObject *items) {
    CHECK(!memcmp(s,before,sizeof(*s))); CHECK(!memcmp(s->items,items,s->count*sizeof(*items)));
}
static ObjectStore fixture(void) {
    ObjectStore s={.next_id=UINT64_C(9007199254740992)}; char *error=NULL;
    Analysis a={.kind=SOURCE_PARQUET,.path="/private/source.parquet",.source_version=11,.schema="main",.table="source"};
    CHECK(analysis_query_set(&a,"SELECT City, Amount, Note FROM input",&error));
    CHECK(analysis_filter_set(&a,"City",FILTER_EQUAL,"Ankara",&error));
    CHECK(analysis_filter_set(&a,"Note",FILTER_NOT_NULL,NULL,&error));
    CHECK(analysis_sort_set(&a,"Amount",true,false,&error)); CHECK(analysis_sort_set(&a,"City",false,true,&error));
    CHECK(objects_add_table(&s,8,10,"Raw Table",&a,&error)==0);
    CHECK(objects_add_table(&s,40,10,"Independent Table",&a,&error)==1);
    /* Only the query/filter/sort additions were owned by this local Analysis. */
    a.path=a.schema=a.table=NULL; analysis_free(&a);
    CHECK(objects_add(&s,8,30,"Pivot",&error)==2);
    PivotDefinition p={0}; CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(pivot_assign(&p,PIVOT_FILTERS,"Note",AGG_COUNT,&error));
    CHECK(objects_update(&s,2,&p,&error)); pivot_free(&p);
    CHECK(objects_bind_source(&s,2,s.items[0].id,&error));
    MarkDefinition marks={0}; MarkField label={.name="City"}; CHECK(marks_assign(&marks,MARKS_LABEL,&label,&error));
    CHECK(objects_marks(&s,2,&marks,&error)); marks_free(&marks);
    CHECK(objects_rename(&s,2,"Analysis view",&error));
    CHECK(objects_history(&s,false)); /* Keep real redo data throughout planning. */
    CHECK(s.redo_count==1 && s.undo_count && objects_valid(&s,&error) && !error);
    return s;
}
static size_t manual_bytes(ObjectSnapshot state) {
    size_t bytes=state.items ? DX_MAX_OBJECTS*sizeof(SheetObject) : 0;
    for (size_t i=0;i<state.count;i++) {
        const SheetObject *o=&state.items[i]; bytes+=strlen(o->name)+1+analysis_text_bytes(&o->source)+marks_text_bytes(&o->marks);
        for (int r=0;r<4;r++) for (size_t j=0;j<o->pivot.counts[r];j++) bytes+=strlen(o->pivot.fields[r][j].name)+1+(o->pivot.fields[r][j].value ? strlen(o->pivot.fields[r][j].value)+1 : 0);
    }
    return bytes;
}
static void binding_plan(void) {
    ObjectStore s=fixture(),before=s,prior={0}; ObjectSnapshot out={0}; char *error=NULL; bool changed=false;
    SheetObject original[DX_MAX_OBJECTS]; memcpy(original,s.items,s.count*sizeof(*original));
    Analysis unchanged_source=s.items[0].source; unchanged_source.source_version=99;
    CHECK(objects_set_source(&s,0,&unchanged_source,&error)); unchanged(&s,&before,original);
    objects_copy(&prior,&s); out=(ObjectSnapshot){prior.items,prior.count}; prior.items=NULL; prior.count=0; objects_free(&prior);
    SheetObject *previous=out.items; uint64_t owner=s.items[0].id;
    CHECK(owner==UINT64_C(9007199254740993));
    CHECK(objects_snapshot_owned_bytes(&out)==manual_bytes(out));
    CHECK(objects_plan_source_version(&s,owner,11,12,&out,&changed,&error) && changed && !error);
    unchanged(&s,&before,original); CHECK(out.items!=previous && out.items!=s.items);
    same_snapshot(current(&s),out,0,12,true); CHECK(out.items[1].source.source_version==11);
    CHECK(objects_snapshot_owned_bytes(&out)==current_bytes(&s)); /* Scalar binding owns no bytes. */
    char saved=out.items[0].source.path[0]; s.items[0].source.path[0]='!'; CHECK(out.items[0].source.path[0]==saved); s.items[0].source.path[0]=saved;
    /* Cancel releases the independent plan without changing live or redo state. */
    objects_snapshot_free(&out); CHECK(!out.items && !out.count); unchanged(&s,&before,original);
    CHECK(objects_plan_source_version(&s,owner,11,12,&out,&changed,&error));
    SheetObject *owned=out.items,*old=s.items; size_t undo=s.undo_count;
    uint64_t next_id=s.next_id,titles=s.title_revision,marks=s.marks_revision,revision=s.items[0].revision;
    objects_adopt_source_version(&s,&out);
    CHECK(!out.items && !out.count && s.items==owned && s.undo_count==undo+1 && !s.redo_count);
    CHECK(s.undo[undo].items==old && s.undo[undo].count==s.count && s.items[0].source.source_version==12);
    CHECK(s.next_id==next_id && s.title_revision==titles && s.marks_revision==marks && s.items[0].revision==revision);
    CHECK(objects_valid(&s,&error)); CHECK(objects_history(&s,false) && s.items==old && s.items[0].source.source_version==11);
    CHECK(s.next_id==next_id && s.title_revision==titles && s.marks_revision==marks && s.items[0].revision==revision);
    CHECK(objects_history(&s,true) && s.items==owned && s.items[0].source.source_version==12);
    CHECK(s.next_id==next_id && s.title_revision==titles && s.marks_revision==marks && s.items[0].revision==revision);
    CHECK(objects_history(&s,false)); /* Same-version/failure must preserve redo. */
    before=s; memcpy(original,s.items,s.count*sizeof(*original));
    CHECK(objects_plan_source_version(&s,owner,11,13,&out,&changed,&error)); previous=out.items;
    changed=true; CHECK(objects_plan_source_version(&s,owner,11,11,&out,&changed,&error) && !changed && out.items==previous);
    unchanged(&s,&before,original);
    const uint64_t owners[]={0,owner+100,s.items[2].id,owner,owner,owner};
    const uint64_t expected[]={11,11,0,12,11,11},versions[]={12,12,12,13,0,13};
    for (size_t i=0;i<sizeof(owners)/sizeof(*owners);i++) {
        if (i==5) { /* A stale async request after its actual owner was deleted. */
            ObjectStore deleted={0}; objects_copy(&deleted,&s); CHECK(objects_remove(&deleted,2)); CHECK(objects_remove(&deleted,0));
            ObjectStore held=deleted; changed=false; CHECK(!objects_plan_source_version(&deleted,owner,11,13,&out,&changed,&error));
            CHECK(!changed && out.items==previous && !memcmp(&deleted,&held,sizeof(held))); clear_error(&error); objects_free(&deleted); continue;
        }
        changed=i%2!=0; bool sentinel=changed; CHECK(!objects_plan_source_version(&s,owners[i],expected[i],versions[i],&out,&changed,&error));
        CHECK(changed==sentinel && out.items==previous); unchanged(&s,&before,original); clear_error(&error);
    }
    objects_snapshot_free(&out); objects_free(&s);
}
static void history_limits(void) {
    ObjectStore s={0}; Analysis a={.kind=SOURCE_CSV,.path="/private/data.csv"}; char *error=NULL; ObjectSnapshot out={0}; bool changed=false;
    CHECK(objects_add_table(&s,0,0,"Table",&a,&error)==0); objects_trim_history(&s,false,0);
    for (size_t i=0;i<32;i++) CHECK(objects_move(&s,0,i+1,0,NULL,0,0,&error));
    CHECK(s.undo_count==32); ObjectStore before=s; SheetObject original=*s.items;
    CHECK(!objects_plan_source_version(&s,s.items[0].id,0,1,&out,&changed,&error));
    CHECK(!changed && !out.items && !out.count && strstr(error,"without pruning")); unchanged(&s,&before,&original); clear_error(&error);
    /* A no-op needs no new history slot, including at the count limit. */
    s.items[0].source.source_version=1; before=s; original=*s.items; changed=true;
    CHECK(objects_plan_source_version(&s,s.items[0].id,1,1,&out,&changed,&error) && !changed); unchanged(&s,&before,&original);
    objects_free(&s);
    /* Exactly8MiB of proposed undo is admitted; one byte more fails atomically.
       Large valid CONTAINS payloads avoid artificial invalid model state. */
    size_t per=DX_OBJECT_HISTORY_BYTES/2,base=DX_MAX_OBJECTS*sizeof(SheetObject)+strlen("Table")+1+strlen(a.path)+1+strlen("City")+1;
    char *value=dx_alloc(per-base); memset(value,'x',per-base-1);
    a.filter_column="City"; a.filter_value=value; a.filter_op=FILTER_CONTAINS;
    CHECK(objects_add_table(&s,0,0,"Table",&a,&error)==0); free(value); objects_trim_history(&s,false,0);
    CHECK(current_bytes(&s)==per);
    CHECK(objects_move(&s,0,1,0,NULL,0,0,&error) && s.undo_count==1);
    CHECK(objects_plan_source_version(&s,s.items[0].id,0,1,&out,&changed,&error) && changed);
    objects_snapshot_free(&out); before=s; original=*s.items;
    char *old=s.items[0].source.filter_value; size_t n=strlen(old); char *larger=dx_alloc(n+2); memcpy(larger,old,n); larger[n]='x';
    free(old); s.items[0].source.filter_value=larger; before=s; original=*s.items; changed=false;
    CHECK(current_bytes(&s)==per+1);
    CHECK(!objects_plan_source_version(&s,s.items[0].id,0,1,&out,&changed,&error) && !changed && !out.items && strstr(error,"without pruning"));
    unchanged(&s,&before,&original); clear_error(&error);
    /* Ordinary history retains both logical snapshots; the new conservative
       reservation must not replace its legacy count-based accounting policy. */
    CHECK(objects_move(&s,0,2,0,NULL,0,0,&error) && s.undo_count==2);
    CHECK(objects_snapshot_owned_bytes(&s.undo[0])+objects_snapshot_owned_bytes(&s.undo[1])>DX_OBJECT_HISTORY_BYTES);
    s.items[0].source.source_version=1; before=s; original=*s.items; changed=true;
    CHECK(objects_plan_source_version(&s,s.items[0].id,1,1,&out,&changed,&error) && !changed && !out.items);
    unchanged(&s,&before,&original); objects_free(&s);
}
static void malformed_and_full_id(void) {
    ObjectStore s={.next_id=UINT64_MAX-1}; Analysis a={.kind=SOURCE_CSV,.path="/private/full.csv"}; char *error=NULL; ObjectSnapshot out={0}; bool changed=false;
    CHECK(objects_add_table(&s,0,0,"Last Table",&a,&error)==0 && s.items[0].id==UINT64_MAX);
    CHECK(objects_plan_source_version(&s,UINT64_MAX,0,UINT64_MAX,&out,&changed,&error) && changed);
    objects_adopt_source_version(&s,&out); CHECK(s.items[0].source.source_version==UINT64_MAX && s.next_id==UINT64_MAX);
    CHECK(objects_history(&s,false) && !s.items[0].source.source_version && s.next_id==UINT64_MAX);
    ObjectStore before=s; SheetObject original=*s.items;
    s.redo_count=33; changed=true;
    CHECK(!objects_plan_source_version(&s,UINT64_MAX,0,1,&out,&changed,&error) && changed && !out.items); clear_error(&error); s=before;
    s.redo[0].items[0].source.filter_count=DX_ANALYSIS_FILTERS;
    CHECK(!objects_plan_source_version(&s,UINT64_MAX,0,1,&out,&changed,&error) && changed && !out.items); clear_error(&error);
    s.redo[0].items[0].source.filter_count=0; unchanged(&s,&before,&original);
    ObjectSnapshot bad={.count=SIZE_MAX}; CHECK(objects_snapshot_owned_bytes(&bad)==SIZE_MAX);
    bad=(ObjectSnapshot){.count=1}; CHECK(objects_snapshot_owned_bytes(&bad)==SIZE_MAX);
    SheetObject item={.name="x"}; bad=(ObjectSnapshot){&item,1}; CHECK(objects_snapshot_owned_bytes(&bad)==DX_MAX_OBJECTS*sizeof(item)+2);
    item.pivot.counts[0]=DX_PIVOT_FIELDS+1; CHECK(objects_snapshot_owned_bytes(&bad)==SIZE_MAX); item.pivot.counts[0]=1;
    CHECK(objects_snapshot_owned_bytes(&bad)==SIZE_MAX); item.pivot.counts[0]=0;
    item.marks.counts[0]=DX_MARK_FIELDS+1; CHECK(objects_snapshot_owned_bytes(&bad)==SIZE_MAX); item.marks.counts[0]=1;
    CHECK(objects_snapshot_owned_bytes(&bad)==SIZE_MAX); item.marks.counts[0]=0;
    item.source.sort_count=DX_ANALYSIS_SORTS; CHECK(objects_snapshot_owned_bytes(&bad)==SIZE_MAX); item.source.sort_count=1;
    CHECK(objects_snapshot_owned_bytes(&bad)==SIZE_MAX); item.source.sort_count=0;
    item.source.filter_count=DX_ANALYSIS_FILTERS; CHECK(objects_snapshot_owned_bytes(&bad)==SIZE_MAX); item.source.filter_count=1;
    CHECK(objects_snapshot_owned_bytes(&bad)==SIZE_MAX); item.source.filter_count=0;
    CHECK(objects_snapshot_owned_bytes(NULL)==SIZE_MAX); bad=(ObjectSnapshot){0}; CHECK(!objects_snapshot_owned_bytes(&bad));
    objects_snapshot_free(&out); CHECK(!out.items && !out.count); objects_free(&s);
}
static void retained_arrays_and_inactive_slots(void) {
    ObjectStore s={0}; Analysis a={.kind=SOURCE_CSV,.path="/private/tail.csv",.source_version=1}; char *error=NULL;
    ObjectSnapshot out={0}; bool changed=false; size_t array=DX_MAX_OBJECTS*sizeof(SheetObject);
    CHECK(objects_add_table(&s,0,0,"Kept",&a,&error)==0);
    CHECK(objects_add(&s,10,10,"Deleted",&error)==1); CHECK(objects_add(&s,20,10,"Last",&error)==2);
    SheetObject *retained=s.items; size_t full=current_bytes(&s);
    CHECK(objects_remove(&s,1) && s.items==retained && s.count==2);
    CHECK(current_bytes(&s)==full-strlen("Deleted")-1);
    CHECK(objects_remove(&s,1) && s.items==retained && s.count==1);
    CHECK(current_bytes(&s)==array+strlen("Kept")+1+strlen(a.path)+1);
    CHECK(objects_plan_source_version(&s,s.items[0].id,1,2,&out,&changed,&error) && changed);
    CHECK(objects_snapshot_owned_bytes(&out)==current_bytes(&s)); objects_snapshot_free(&out);
    CHECK(objects_remove(&s,0) && s.items==retained && !s.count && current_bytes(&s)==array);
    CHECK(objects_history(&s,false) && s.count==1); /* Empty retained array is now in redo. */
    CHECK(s.redo_count==1 && !s.redo[0].count && s.redo[0].items==retained && objects_snapshot_owned_bytes(&s.redo[0])==array);
    CHECK(objects_plan_source_version(&s,s.items[0].id,1,2,&out,&changed,&error)); objects_snapshot_free(&out);
    ObjectStore before=s; SheetObject original=*s.items;
    /* The ordinary model still accepts inactive fixed slots. Source-binding
       planning rejects them without parsing, cloning or adopting the pointer. */
    char **inactive[]={&s.items[0].source.filters[14].column,&s.items[0].source.filters[14].value,
        &s.items[0].source.sorts[6].column,&s.items[0].pivot.fields[PIVOT_FILTERS][7].name,
        &s.items[0].pivot.fields[PIVOT_FILTERS][7].value,&s.items[0].marks.fields[MARKS_LABEL][7].name};
    for (size_t i=0;i<sizeof(inactive)/sizeof(*inactive);i++) {
        *inactive[i]=dx_strdup("inactive owned text"); CHECK(objects_valid(&s,&error) && !error);
        CHECK(current_bytes(&s)==SIZE_MAX); changed=i%2!=0; bool prior=changed;
        CHECK(!objects_plan_source_version(&s,s.items[0].id,1,2,&out,&changed,&error) && changed==prior && !out.items && !out.count);
        clear_error(&error); free(*inactive[i]); *inactive[i]=NULL; unchanged(&s,&before,&original);
    }
    /* Analysis free always owns its nine top-level strings, even a filter value
       with no active column: the reservation must charge it rather than omit it. */
    size_t normal=current_bytes(&s); s.items[0].source.filter_value=dx_strdup("inactive top-level value");
    CHECK(current_bytes(&s)==normal+strlen("inactive top-level value")+1);
    CHECK(objects_plan_source_version(&s,s.items[0].id,1,2,&out,&changed,&error));
    CHECK(out.items[0].source.filter_value!=s.items[0].source.filter_value && !strcmp(out.items[0].source.filter_value,"inactive top-level value"));
    objects_snapshot_free(&out); free(s.items[0].source.filter_value); s.items[0].source.filter_value=NULL;
    /* Reject noncanonical history as well as the live model before cloning. */
    s.undo[s.undo_count-1].items[0].marks.fields[MARKS_COLOR][7].name=dx_strdup("hidden history ownership"); changed=true;
    CHECK(!objects_plan_source_version(&s,s.items[0].id,1,2,&out,&changed,&error) && changed && !out.items); clear_error(&error);
    free(s.undo[s.undo_count-1].items[0].marks.fields[MARKS_COLOR][7].name); s.undo[s.undo_count-1].items[0].marks.fields[MARKS_COLOR][7].name=NULL;
    objects_free(&s);
}
int main(void) { binding_plan(); history_limits(); malformed_and_full_id(); retained_arrays_and_inactive_slots(); printf("objects: %u checks passed\n",checks); return 0; }
