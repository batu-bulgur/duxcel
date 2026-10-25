/* Read-only actual result associations; no owned copies or source activation. */
static bool app_range_result(void *context,uint64_t id,RangeResultProof *out) {
    App *a=context; Worksheet *s=active_sheet(a); if (!s || !out) return false;
    bool quiet=!a->busy && !a->io_busy && !a->query_loading && !a->refresh_plans && !a->step_preview.active;
    if (!id) {
        *out=(RangeResultProof){.object=0,.request=a->request_id,.query_epoch=a->analysis_revision,.source_epoch=a->analysis_revision,
            .owner=&a->page,.definition=&a->analysis,.page=&a->page,.ready=quiet && !a->page_stale && !a->fetch_failed && !a->page.count_pending && (a->analysis.kind==SOURCE_NONE || a->page.column_count),.failed=a->fetch_failed};
        return true;
    }
    for (size_t i=0;i<s->objects.count;i++) {
        const SheetObject *o=s->objects.items+i; if (o->id!=id) continue;
        if (o->kind!=OBJECT_TABLE && (o->kind!=OBJECT_PIVOT || !o->pivot.counts[PIVOT_VALUES])) return false;
        const ObjectView *v=a->objects+i;
        *out=(RangeResultProof){.object=v->id,.request=a->request_id,.query_epoch=v->revision,.source_epoch=v->source_revision,
            .owner=v,.definition=object_source(a,o),.page=&v->page,.ready=quiet && object_ready(a,i),.failed=v->failed};
        return true;
    }
    return false;
}
