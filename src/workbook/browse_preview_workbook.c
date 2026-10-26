#include "browse_preview_workbook.h"
static SourceDependencyStatus resolve(const BrowsePreview *preview,const Workbook *actual,size_t other_owned,BrowsePreviewLive *live) {
    if (!preview || !actual || !workbook_identity_valid(actual) || (!actual->id.high && !actual->id.low)) return SOURCE_DEPENDENCY_INVALID;
    BrowsePreviewInfo info=browse_preview_info(preview);
    if (info.document.high!=actual->id.high || info.document.low!=actual->id.low) return SOURCE_DEPENDENCY_CONFLICT;
    const Worksheet *sheet=NULL;
    for (size_t i=0;i<actual->count;i++) if (actual->sheets[i].id==info.sheet) sheet=actual->sheets+i;
    if (!sheet) return SOURCE_DEPENDENCY_UNAVAILABLE;
    const ObjectStore *objects=&sheet->objects;
    if (objects->count>DX_MAX_OBJECTS || (objects->count && !objects->items)) return SOURCE_DEPENDENCY_INVALID;
    const SheetObject *table=NULL;
    for (size_t i=0;i<objects->count;i++) {
        if (!objects->items[i].id || objects->items[i].id>objects->next_id) return SOURCE_DEPENDENCY_INVALID;
        for (size_t j=0;j<i;j++) if (objects->items[j].id==objects->items[i].id) return SOURCE_DEPENDENCY_INVALID;
        if (objects->items[i].id==info.owner) table=objects->items+i;
    }
    if (info.owner && !table) return SOURCE_DEPENDENCY_UNAVAILABLE;
    if (table && table->kind!=OBJECT_TABLE) return SOURCE_DEPENDENCY_CONFLICT;
    *live=(BrowsePreviewLive){.document=actual->id,.sheet=sheet->id,.owner=info.owner,.object_high_water=objects->next_id,
        .definition=table ? &table->source : &sheet->analysis,.sources=&sheet->source_versions,.other_owned=other_owned,.table=table};
    return SOURCE_DEPENDENCY_EXACT;
}
BrowsePreviewStatus browse_preview_workbook_measure(const BrowsePreview *preview,const Workbook *actual,size_t other_owned,size_t *coexistence) {
    if (!coexistence) return BROWSE_PREVIEW_INVALID;
    BrowsePreviewLive live={0}; SourceDependencyStatus status=resolve(preview,actual,other_owned,&live);
    if (status==SOURCE_DEPENDENCY_INVALID) return BROWSE_PREVIEW_INVALID;
    if (status==SOURCE_DEPENDENCY_EXACT) return browse_preview_admission_measure(preview,live,coexistence);
    size_t owned=browse_preview_info(preview).owned; if (owned>SIZE_MAX-other_owned) return BROWSE_PREVIEW_LIMIT;
    *coexistence=owned+other_owned; return BROWSE_PREVIEW_OK;
}
SourceDependencyStatus browse_preview_workbook_admit(const BrowsePreview *preview,const Workbook *actual,size_t other_owned,size_t reserved,BrowsePreviewWorkbookAdmission *out) {
    if (!out) return SOURCE_DEPENDENCY_INVALID;
    size_t coexistence=0; BrowsePreviewStatus measured=browse_preview_workbook_measure(preview,actual,other_owned,&coexistence);
    if (measured!=BROWSE_PREVIEW_OK) return measured==BROWSE_PREVIEW_LIMIT ? SOURCE_DEPENDENCY_LIMIT : SOURCE_DEPENDENCY_INVALID;
    if (reserved<coexistence) return SOURCE_DEPENDENCY_LIMIT;
    BrowsePreviewLive live={0}; SourceDependencyStatus status=resolve(preview,actual,other_owned,&live);
    BrowsePreviewWorkbookAdmission report={0};
    if (status==SOURCE_DEPENDENCY_EXACT) status=browse_preview_admit(preview,live,reserved,&report.dependencies);
    else {
        BrowsePreviewInfo info=browse_preview_info(preview);
        report.dependencies.unbound_sources=!info.source_version;
        report.dependencies.sources.captured_pending=info.binding_pending;
        if (status==SOURCE_DEPENDENCY_UNAVAILABLE) report.dependencies.sources.unavailable=info.source_version!=0;
    }
    if (status==SOURCE_DEPENDENCY_INVALID || status==SOURCE_DEPENDENCY_LIMIT || status==SOURCE_DEPENDENCY_UNSUPPORTED) return status;
    report.classification=status==SOURCE_DEPENDENCY_EXACT || status==SOURCE_DEPENDENCY_REFINED ? BROWSE_PREVIEW_CAPTURED_ONLY : BROWSE_PREVIEW_NOT_ASSOCIATED;
    *out=report; return status;
}
