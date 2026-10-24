#include "definition_baseline.h"
static DefinitionBaselineStatus source(const Analysis *a) {
    if ((unsigned)a->kind>SOURCE_DEMO) return DEFINITION_BASELINE_INVALID;
    if (a->kind!=SOURCE_DATABASE) return DEFINITION_BASELINE_READY;
    if (!a->path || !*a->path) return DEFINITION_BASELINE_INVALID;
    if (!a->table) return DEFINITION_BASELINE_UNRESOLVED;
    return *a->table && a->schema && *a->schema ? DEFINITION_BASELINE_READY : DEFINITION_BASELINE_INVALID;
}
DefinitionBaselineStatus definition_baseline_status(DefinitionDependencyView view) {
    DefinitionBaselineStatus status=view.primary ? source(view.primary) : DEFINITION_BASELINE_READY;
    if (status==DEFINITION_BASELINE_INVALID || !view.objects) return status;
    if (view.objects->count>DX_MAX_OBJECTS || (view.objects->count && !view.objects->items)) return DEFINITION_BASELINE_INVALID;
    for (size_t i=0;i<view.objects->count;i++) {
        const SheetObject *o=view.objects->items+i;
        if ((unsigned)o->kind>OBJECT_TABLE) return DEFINITION_BASELINE_INVALID;
        if (o->kind!=OBJECT_TABLE) continue;
        DefinitionBaselineStatus next=source(&o->source);
        if (next==DEFINITION_BASELINE_INVALID) return next;
        if (next==DEFINITION_BASELINE_UNRESOLVED) status=next;
    }
    return status;
}
