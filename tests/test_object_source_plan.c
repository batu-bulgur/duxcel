#include "workbook/object_source_plan.h"
#include "workbook/definition_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object source plan:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static ObjectStore fixture(void) {
    ObjectStore s={0}; Analysis source={.kind=SOURCE_CSV,.path="/source",.source_version=9};
    CHECK(analysis_query_set(&source,"SELECT City, Amount, Value FROM input",NULL));
    CHECK(analysis_filter_set(&source,"City",FILTER_EQUAL,"Ankara",NULL) && analysis_filter_set(&source,"Value",FILTER_EQUAL,"",NULL));
    CHECK(analysis_sort_set(&source,"Amount",true,false,NULL) && analysis_sort_set(&source,"City",false,true,NULL));
    CHECK(objects_add_table(&s,10,2,"Raw",&source,NULL)==0 && objects_add_table(&s,40,2,"Other",&source,NULL)==1);
    CHECK(objects_add(&s,10,20,"Pivot",NULL)==2 && objects_bind_source(&s,2,s.items[0].id,NULL));
    for (unsigned i=0;i<36;i++) { char name[32]; snprintf(name,sizeof(name),"Other %u",i); CHECK(objects_rename(&s,1,name,NULL)); }
    CHECK(objects_history(&s,false) && s.undo_count==31 && s.redo_count==1);
    source.path=NULL; analysis_free(&source); return s;
}
static void identical(const ObjectStore *s,const ObjectStore *before,const SheetObject *items) {
    CHECK(!memcmp(s,before,sizeof(*s)) && !memcmp(s->items,items,s->count*sizeof(*items)));
}
static void reject(const ObjectStore *s,uint64_t id,const Analysis *source,ObjectSourcePlanStatus expected,ObjectSnapshot *out) {
    ObjectSourcePlanInfo info={.owner=99,.owned=71},old=info; ObjectSnapshot previous=*out;
    CHECK(objects_plan_source_transform(s,id,source,SIZE_MAX,out,&info)==expected && !memcmp(&info,&old,sizeof(info)) && !memcmp(out,&previous,sizeof(*out)));
}
static void ownership_and_budget(void) {
    ObjectStore s=fixture(),before=s; SheetObject items[DX_MAX_OBJECTS]; memcpy(items,s.items,s.count*sizeof(*items));
    Analysis source=analysis_copy(&s.items[0].source); source.unique_rows=true; ObjectSnapshot out={0}; ObjectSourcePlanInfo info={0}; uint64_t id=s.items[0].id;
    CHECK(objects_source_transform_measure(&s,id,&source,&info)==OBJECT_SOURCE_PLAN_OK && info.owned && !info.validation_scratch && info.result_changed && info.before_revision==s.items[0].revision && info.after_revision==info.before_revision+1);
    CHECK(objects_plan_source_transform(&s,id,&source,info.owned,&out,&info)==OBJECT_SOURCE_PLAN_OK); identical(&s,&before,items);
    CHECK(out.count==s.count && out.items!=s.items && out.items[0].source.unique_rows && !s.items[0].source.unique_rows && out.items[0].revision==s.items[0].revision+1 && out.items[2].source_id==id);
    for (size_t i=0;i<s.count;i++) CHECK(out.items[i].name!=s.items[i].name && !strcmp(out.items[i].name,s.items[i].name));
    CHECK(out.items[0].source.query!=source.query && out.items[0].source.path!=source.path && out.items[0].source.filter_column!=source.filter_column &&
        out.items[0].source.filters[0].value && !*out.items[0].source.filters[0].value && out.items[1].source.query!=s.items[1].source.query && out.items[2].revision==s.items[2].revision);
    size_t exact=out.count*sizeof(*out.items);
    for (size_t i=0;i<out.count;i++) exact+=strlen(out.items[i].name)+1+analysis_text_bytes(&out.items[i].source);
    CHECK(info.owned==exact); /* No history arrays or unused current capacity. */
    ObjectSnapshot old=out; ObjectSourcePlanInfo previous=info;
    CHECK(objects_plan_source_transform(&s,id,&source,info.owned-1,&out,&info)==OBJECT_SOURCE_PLAN_LIMIT && out.items==old.items && !memcmp(&info,&previous,sizeof(info)));
    reject(&s,0,&source,OBJECT_SOURCE_PLAN_INVALID,&out); reject(&s,UINT64_MAX,&source,OBJECT_SOURCE_PLAN_INVALID,&out); reject(&s,s.items[2].id,&source,OBJECT_SOURCE_PLAN_INVALID,&out);
    source.source_version++; reject(&s,id,&source,OBJECT_SOURCE_PLAN_PROTECTED,&out); source.source_version--;
    source.path[0]='!'; reject(&s,id,&source,OBJECT_SOURCE_PLAN_PROTECTED,&out); source.path[0]='/';
    source.schema=dx_strdup(""); reject(&s,id,&source,OBJECT_SOURCE_PLAN_PROTECTED,&out); free(source.schema); source.schema=NULL;
    identical(&s,&before,items); analysis_free(&source); objects_free(&s);
    CHECK(out.items[0].source.unique_rows && !strcmp(out.items[0].source.query,"SELECT City, Amount, Value FROM input") && !strcmp(out.items[1].name,"Other 34"));
    objects_snapshot_free(&out);
}
static void noops_order_and_epochs(void) {
    ObjectStore s=fixture(); Analysis source=analysis_copy(&s.items[0].source); ObjectSnapshot out={0}; ObjectSourcePlanInfo info={.owner=99};
    source.filter_number=(NumberType){NUM_DECIMAL,38,4}; source.filters[0].number=(NumberType){NUM_I128,0,0};
    CHECK(objects_source_transform_measure(&s,s.items[0].id,&source,&info)==OBJECT_SOURCE_PLAN_NOOP && info.owner==99);
    reject(&s,s.items[0].id,&source,OBJECT_SOURCE_PLAN_NOOP,&out);
    CHECK(analysis_filter_move(&source,0,1)); s.items[0].revision=UINT64_MAX;
    CHECK(objects_plan_source_transform(&s,s.items[0].id,&source,SIZE_MAX,&out,&info)==OBJECT_SOURCE_PLAN_OK && !info.result_changed && out.items[0].revision==UINT64_MAX && !strcmp(out.items[0].source.filter_column,"Value"));
    CHECK(analysis_sort_move(&source,0,1)); reject(&s,s.items[0].id,&source,OBJECT_SOURCE_PLAN_LIMIT,&out);
    s.items[0].revision=17;
    CHECK(objects_plan_source_transform(&s,s.items[0].id,&source,SIZE_MAX,&out,&info)==OBJECT_SOURCE_PLAN_OK && info.result_changed && info.after_revision==18 && !strcmp(out.items[0].source.sort_column,"City"));
    analysis_free(&source); objects_snapshot_free(&out); objects_free(&s);
}
static void adoption_and_retention(void) {
    ObjectStore s=fixture(); uint64_t id=s.items[0].id,high=s.next_id,titles=s.title_revision,marks=s.marks_revision;
    Analysis source=analysis_copy(&s.items[0].source); source.unique_rows=true; ObjectSnapshot out={0}; ObjectSourcePlanInfo info={0};
    CHECK(objects_plan_source_transform(&s,id,&source,SIZE_MAX,&out,&info)==OBJECT_SOURCE_PLAN_OK);
    SheetObject *old=s.items,*next=out.items; objects_adopt_source_transform(&s,&out);
    CHECK(!out.items && !out.count && s.items==next && s.undo_count==32 && !s.redo_count && s.undo[31].items==old && s.items[0].source.unique_rows &&
        s.next_id==high && s.title_revision==titles && s.marks_revision==marks && s.items[0].revision==info.after_revision);
    CHECK(objects_history(&s,false) && s.items==old && !s.items[0].source.unique_rows && s.redo_count==1);
    CHECK(objects_history(&s,true) && s.items==next && s.items[0].source.unique_rows && !s.redo_count);
    analysis_free(&source);
    for (unsigned i=0;i<40;i++) {
        source=analysis_copy(&s.items[0].source); source.unique_rows=!source.unique_rows;
        CHECK(objects_plan_source_transform(&s,id,&source,SIZE_MAX,&out,&info)==OBJECT_SOURCE_PLAN_OK);
        objects_adopt_source_transform(&s,&out); CHECK(s.undo_count==32 && !s.redo_count && !out.items && s.items[0].source.unique_rows==source.unique_rows && s.next_id==high);
        analysis_free(&source);
    }
    /* Byte pressure can evict several old snapshots, independently from the
       action-count ceiling. The outer command must reserve before this move. */
    source=analysis_copy(&s.items[0].source); analysis_filters_clear(&source);
    source.filter_column=dx_strdup("Value"); source.filter_op=FILTER_CONTAINS; source.filter_value=dx_alloc(3u*1024u*1024u+1);
    memset(source.filter_value,'x',3u*1024u*1024u);
    source.filter_count=1; source.filters[0]=(AnalysisFilter){.column=dx_strdup("City"),.value=dx_alloc(3u*1024u*1024u+1),.op=FILTER_CONTAINS};
    memset(source.filters[0].value,'y',3u*1024u*1024u); /* Each predicate stays below4 MiB. */
    CHECK(objects_plan_source_transform(&s,id,&source,SIZE_MAX,&out,&info)==OBJECT_SOURCE_PLAN_OK); objects_adopt_source_transform(&s,&out); analysis_free(&source);
    for (unsigned i=0;i<2;i++) {
        source=analysis_copy(&s.items[0].source); source.unique_rows=!source.unique_rows;
        CHECK(objects_plan_source_transform(&s,id,&source,SIZE_MAX,&out,&info)==OBJECT_SOURCE_PLAN_OK); objects_adopt_source_transform(&s,&out); analysis_free(&source);
    }
    CHECK(s.undo_count==1 && s.next_id==high && s.title_revision==titles && s.marks_revision==marks && objects_valid(&s,NULL));
    CHECK(objects_history(&s,false) && s.redo_count==1 && strlen(s.items[0].source.filter_value)==3u*1024u*1024u && strlen(s.items[0].source.filters[0].value)==3u*1024u*1024u);
    objects_free(&s);
}
static void grammar_and_defaults(void) {
    ObjectStore s=fixture(); Analysis source=analysis_copy(&s.items[0].source); ObjectSnapshot out={0}; ObjectSourcePlanInfo info={0};
    CHECK(analysis_filter_set(&source,"Amount",FILTER_BETWEEN,"[\"1\",\"100\"]",NULL));
    CHECK(objects_source_transform_measure(&s,s.items[0].id,&source,&info)==OBJECT_SOURCE_PLAN_OK && info.validation_scratch>0);
    CHECK(objects_plan_source_transform(&s,s.items[0].id,&source,info.owned+info.validation_scratch-1,&out,&info)==OBJECT_SOURCE_PLAN_LIMIT && !out.items);
    CHECK(objects_plan_source_transform(&s,s.items[0].id,&source,info.owned+info.validation_scratch,&out,&info)==OBJECT_SOURCE_PLAN_OK);
    size_t index=source.filter_count-1; free(source.filters[index].value); source.filters[index].value=dx_strdup("[]");
    CHECK(objects_source_transform_measure(&s,s.items[0].id,&source,&info)==OBJECT_SOURCE_PLAN_OK); /* Shape is not grammar admission. */
    reject(&s,s.items[0].id,&source,OBJECT_SOURCE_PLAN_INVALID,&out); analysis_free(&source);
    s.items[0].source.kind=SOURCE_DATABASE; s.items[0].source.source_version=0;
    source=analysis_copy(&s.items[0].source); source.unique_rows=true; reject(&s,s.items[0].id,&source,OBJECT_SOURCE_PLAN_PROTECTED,&out);
    s.items[0].source.schema=dx_strdup("main"); s.items[0].source.table=dx_strdup("Sales"); analysis_free(&source); source=analysis_copy(&s.items[0].source); source.unique_rows=true;
    CHECK(objects_plan_source_transform(&s,s.items[0].id,&source,SIZE_MAX,&out,&info)==OBJECT_SOURCE_PLAN_OK && out.items[0].source.kind==SOURCE_DATABASE && !strcmp(out.items[0].source.table,"Sales"));
    analysis_free(&source); objects_snapshot_free(&out); objects_free(&s);
}
int main(void) { ownership_and_budget(); noops_order_and_epochs(); adoption_and_retention(); grammar_and_defaults(); printf("PASS: %u independent Table source plan checks\n",checks); return 0; }
