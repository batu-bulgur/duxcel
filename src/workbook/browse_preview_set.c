#include "browse_preview_set_internal.h"
#include "browse_preview_codec.h"
#include <stdlib.h>
static bool add(size_t *total,size_t bytes) { if (*total>SIZE_MAX-bytes) return false; *total+=bytes; return true; }
BrowsePreviewSetInfo browse_preview_set_info(const BrowsePreviewSet *set) { return set ? set->info : (BrowsePreviewSetInfo){0}; }
const BrowsePreviewManifestEntry *browse_preview_set_manifest(const BrowsePreviewSet *set,size_t position) { return set && position<set->info.count ? &set->entries[position].manifest : NULL; }
const BrowsePreview *browse_preview_set_asset(const BrowsePreviewSet *set,size_t position) { return set && position<set->info.count ? set->entries[position].asset : NULL; }
BrowsePreviewSet *browse_preview_set_retain(const BrowsePreviewSet *borrowed) {
    if (!borrowed) return NULL; BrowsePreviewSet *set=(BrowsePreviewSet *)borrowed;
    dx_lock(&set->ownership); bool valid=set->references && set->references<SIZE_MAX;
    if (valid) set->references++; dx_unlock(&set->ownership); return valid ? set : NULL;
}
void browse_preview_set_free(BrowsePreviewSet *set) {
    if (!set) return;
    dx_lock(&set->ownership); bool last=--set->references==0; dx_unlock(&set->ownership);
    if (!last) return;
    for (size_t i=0;i<set->info.count;i++) browse_preview_free(set->entries[i].asset);
    free(set->entries); dx_mutex_destroy(&set->ownership); free(set);
}
static void discard_staging(BrowsePreviewSet *set) {
    /* No input reference has moved yet: release wrapper storage only. */
    free(set->entries); dx_mutex_destroy(&set->ownership); free(set);
}
BrowsePreviewSetStatus browse_preview_set_measure(const Workbook *actual,BrowsePreview *const *assets,size_t count,
    const BrowsePreviewSet *prior,size_t retained_limit,size_t other_owned,BrowsePreviewSetInfo *out) {
    if (!out || !actual || !workbook_identity_valid(actual) || (!actual->id.high && !actual->id.low)) return BROWSE_PREVIEW_SET_INVALID;
    if (count>DX_BROWSE_PREVIEW_SET_ENTRIES) return BROWSE_PREVIEW_SET_LIMIT;
    if (count && !assets) return BROWSE_PREVIEW_SET_INVALID;
    BrowsePreviewSetInfo info={.document=actual->id,.snapshot_revision=actual->command_revision,.scope=BROWSE_PREVIEW_AUXILIARY_CURRENT_ARTIFACT,.count=count,.capacity=count,.owned=sizeof(BrowsePreviewSet)};
    if (count>SIZE_MAX/sizeof(BrowsePreviewSetEntry) || !add(&info.owned,count*sizeof(BrowsePreviewSetEntry))) return BROWSE_PREVIEW_SET_LIMIT;
    for (size_t i=0;i<count;i++) {
        if (!assets[i]) return BROWSE_PREVIEW_SET_INVALID;
        BrowsePreviewInfo asset=browse_preview_info(assets[i]);
        for (size_t j=0;j<i;j++) {
            BrowsePreviewInfo earlier=browse_preview_info(assets[j]);
            if (assets[i]==assets[j] || (asset.sheet==earlier.sheet && asset.owner==earlier.owner)) return BROWSE_PREVIEW_SET_INVALID;
        }
        if (!add(&info.assets_owned,asset.owned) || !add(&info.owned,asset.owned)) return BROWSE_PREVIEW_SET_LIMIT;
    }
    if (info.owned>retained_limit) return BROWSE_PREVIEW_SET_LIMIT;
    for (size_t i=0;i<count;i++) {
        size_t admission=0; BrowsePreviewStatus status=browse_preview_workbook_measure(assets[i],actual,0,&admission);
        if (status!=BROWSE_PREVIEW_OK) return status==BROWSE_PREVIEW_LIMIT ? BROWSE_PREVIEW_SET_LIMIT : BROWSE_PREVIEW_SET_INVALID;
        size_t asset_owned=browse_preview_info(assets[i]).owned;
        if (admission<asset_owned) return BROWSE_PREVIEW_SET_INVALID;
        if (admission-asset_owned>info.validation_scratch) info.validation_scratch=admission-asset_owned;
        BrowsePreviewWireInfo wire={0}; status=browse_preview_encode_measure(assets[i],NULL,&wire);
        if (status!=BROWSE_PREVIEW_OK) return status==BROWSE_PREVIEW_LIMIT ? BROWSE_PREVIEW_SET_LIMIT : status==BROWSE_PREVIEW_UNSUPPORTED ? BROWSE_PREVIEW_SET_UNSUPPORTED : BROWSE_PREVIEW_SET_INVALID;
    }
    info.coexistence=info.owned;
    if (!add(&info.coexistence,other_owned) || !add(&info.coexistence,prior ? prior->info.owned : 0) || !add(&info.coexistence,info.validation_scratch)) return BROWSE_PREVIEW_SET_LIMIT;
    *out=info; return BROWSE_PREVIEW_SET_OK;
}
static bool before(const BrowsePreviewSetEntry *a,const BrowsePreviewSetEntry *b) {
    return a->manifest.sheet<b->manifest.sheet || (a->manifest.sheet==b->manifest.sheet && a->manifest.owner<b->manifest.owner);
}
static void sort(BrowsePreviewSetEntry *entries,size_t count) {
    /* Bounded in-place ordering: no libc sort scratch escapes admission. */
    for (size_t i=1;i<count;i++) { BrowsePreviewSetEntry item=entries[i]; size_t at=i; while (at && before(&item,entries+at-1)) { entries[at]=entries[at-1]; at--; } entries[at]=item; }
}
BrowsePreviewSetStatus browse_preview_set_take(const Workbook *actual,BrowsePreview **assets,size_t count,size_t retained_limit,size_t other_owned,size_t reserved,BrowsePreviewSetControl control,BrowsePreviewSet **out) {
    if (!out) return BROWSE_PREVIEW_SET_INVALID;
    BrowsePreviewSetInfo info={0}; BrowsePreviewSetStatus status=browse_preview_set_measure(actual,assets,count,*out,retained_limit,other_owned,&info);
    if (status!=BROWSE_PREVIEW_SET_OK) return status;
    if (reserved<info.coexistence) return BROWSE_PREVIEW_SET_LIMIT;
    BrowsePreviewSet *next=dx_alloc(sizeof(*next)); next->info=info; next->references=1; dx_mutex_init(&next->ownership);
    next->entries=count ? dx_alloc(count*sizeof(*next->entries)) : NULL;
    for (size_t i=0;i<count;i++) {
        if (control.cancelled && control.cancelled(control.context,i)) { discard_staging(next); return BROWSE_PREVIEW_SET_CANCELLED; }
        BrowsePreviewWorkbookAdmission admission={0};
        size_t validation=browse_preview_info(assets[i]).owned+info.validation_scratch;
        SourceDependencyStatus associated=browse_preview_workbook_admit(assets[i],actual,0,validation,&admission);
        if (associated!=SOURCE_DEPENDENCY_EXACT && associated!=SOURCE_DEPENDENCY_REFINED) {
            discard_staging(next);
            return associated==SOURCE_DEPENDENCY_LIMIT ? BROWSE_PREVIEW_SET_LIMIT : associated==SOURCE_DEPENDENCY_INVALID ? BROWSE_PREVIEW_SET_INVALID :
                associated==SOURCE_DEPENDENCY_UNSUPPORTED ? BROWSE_PREVIEW_SET_UNSUPPORTED : BROWSE_PREVIEW_SET_ASSOCIATION;
        }
        BrowsePreviewWireInfo wire={0}; if (browse_preview_encode_measure(assets[i],NULL,&wire)!=BROWSE_PREVIEW_OK) { discard_staging(next); return BROWSE_PREVIEW_SET_INVALID; }
        BrowsePreviewInfo asset=browse_preview_info(assets[i]);
        next->entries[i]=(BrowsePreviewSetEntry){.asset=assets[i],.manifest={.sheet=asset.sheet,.owner=asset.owner,.source_version=asset.source_version,.encoded_bytes=wire.bytes,.owned_bytes=asset.owned,.admission=admission}};
    }
    sort(next->entries,count);
    if (control.cancelled && control.cancelled(control.context,count)) { discard_staging(next); return BROWSE_PREVIEW_SET_CANCELLED; }
    /* All fallible work is complete; these caller-owned refs now move once. */
    for (size_t i=0;i<count;i++) assets[i]=NULL;
    BrowsePreviewSet *old=*out; *out=next; browse_preview_set_free(old); return BROWSE_PREVIEW_SET_OK;
}
