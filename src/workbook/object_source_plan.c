#include "object_source_plan.h"
#include "definition_state.h"
#include "definition_baseline.h"
#include <string.h>
static bool text(const char *a,const char *b) { return a && b ? !strcmp(a,b) : a==b; }
static ObjectSourcePlanStatus project(const ObjectStore *s,uint64_t owner,const Analysis *source,
    SheetObject *items,ObjectSourcePlanInfo *info) {
    if (!s || !source || !info || !owner || s->count>DX_MAX_OBJECTS || !s->count || !s->items) return OBJECT_SOURCE_PLAN_INVALID;
    ObjectSnapshot before={s->items,s->count}; ObjectsAssetInfo b={0},a={0};
    if (objects_asset_measure(1,&before,s->next_id,&b)!=OBJECTS_ASSET_OK) return OBJECT_SOURCE_PLAN_INVALID;
    size_t index=0; while (index<s->count && s->items[index].id!=owner) index++;
    if (index==s->count || s->items[index].kind!=OBJECT_TABLE) return OBJECT_SOURCE_PLAN_INVALID;
    const Analysis *old=&s->items[index].source;
    if (old->kind!=source->kind || old->source_version!=source->source_version || !text(old->path,source->path) || !text(old->schema,source->schema) || !text(old->table,source->table) ||
        definition_baseline_status((DefinitionDependencyView){old,NULL})!=DEFINITION_BASELINE_READY) return OBJECT_SOURCE_PLAN_PROTECTED;
    memcpy(items,s->items,s->count*sizeof(*items)); items[index].source=*source;
    ObjectSnapshot after={items,s->count};
    if (objects_asset_measure(1,&after,s->next_id,&a)!=OBJECTS_ASSET_OK) return OBJECT_SOURCE_PLAN_INVALID;
    if (definition_analysis_equal(old,source)) return OBJECT_SOURCE_PLAN_NOOP;
    bool changed=!analysis_same_result(old,source); uint64_t revision=s->items[index].revision;
    if (changed && revision==UINT64_MAX) return OBJECT_SOURCE_PLAN_LIMIT;
    items[index].revision=revision+(changed ? 1 : 0);
    *info=(ObjectSourcePlanInfo){a.measure.owned,a.measure.validation_scratch>b.measure.validation_scratch ? a.measure.validation_scratch : b.measure.validation_scratch,
        owner,revision,items[index].revision,changed};
    return OBJECT_SOURCE_PLAN_OK;
}
ObjectSourcePlanStatus objects_source_transform_measure(const ObjectStore *s,uint64_t owner,const Analysis *source,ObjectSourcePlanInfo *out) {
    if (!out) return OBJECT_SOURCE_PLAN_INVALID;
    SheetObject items[DX_MAX_OBJECTS]; ObjectSourcePlanInfo info={0}; ObjectSourcePlanStatus status=project(s,owner,source,items,&info);
    if (status==OBJECT_SOURCE_PLAN_OK) *out=info;
    return status;
}
ObjectSourcePlanStatus objects_plan_source_transform(const ObjectStore *s,uint64_t owner,const Analysis *source,size_t reserved,ObjectSnapshot *out,ObjectSourcePlanInfo *info) {
    if (!out || !info) return OBJECT_SOURCE_PLAN_INVALID;
    SheetObject items[DX_MAX_OBJECTS]; ObjectSourcePlanInfo next={0}; ObjectSourcePlanStatus status=project(s,owner,source,items,&next);
    if (status!=OBJECT_SOURCE_PLAN_OK) return status;
    if (next.owned>reserved || next.validation_scratch>reserved-next.owned) return OBJECT_SOURCE_PLAN_LIMIT;
    /* Never serialize or adopt this borrowed projection. Full semantics precede
       the one independent current-graph copy, without cloning any Undo/Redo. */
    ObjectStore candidate={.items=items,.count=s->count,.next_id=s->next_id},copy={0};
    if (!objects_valid(s,NULL) || !objects_valid(&candidate,NULL)) return OBJECT_SOURCE_PLAN_INVALID;
    objects_copy(&copy,&candidate);
    objects_snapshot_free(out); *out=(ObjectSnapshot){copy.items,copy.count}; *info=next;
    copy.items=NULL; copy.count=0; objects_free(&copy);
    return OBJECT_SOURCE_PLAN_OK;
}
