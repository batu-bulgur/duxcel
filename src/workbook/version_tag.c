#include "version_tag.h"
#include <string.h>
WorkbookVersionStatus workbook_version_tag_prepare(const WorkbookVersionIndex *index,WorkbookVersionOwner owner,
    const WorkbookVersionRoot *root,WorkbookVersionText label,WorkbookVersionTagPlan *out) {
    if (!out) return VERSION_INVALID_INDEX;
    WorkbookVersionTagPlan next={0};
    WorkbookVersionStatus status=workbook_version_prepare(index,owner,VERSION_USER_SAVE,root,&next.save);
    if (status) return status;
    status=workbook_version_label_check(label); if (status) return status;
    for (size_t i=0;i<index->tag_count;i++) if (index->tags[i].label.bytes==label.bytes && !memcmp(index->tags[i].label.data,label.data,label.bytes)) return VERSION_DUPLICATE_TAG;
    if (index->tag_count==DX_VERSION_TAGS) return VERSION_LIMIT;
    if (index->generation==UINT64_MAX) return VERSION_EXHAUSTED;
    next.next_generation=index->generation+1;
    next.label_bytes=label.bytes; memcpy(next.label,label.data,label.bytes); next.label[label.bytes]=0;
    *out=next; return VERSION_OK;
}
bool workbook_version_tag_can_publish(const WorkbookVersionIndex *index,WorkbookVersionOwner owner,const WorkbookVersionTagPlan *plan) {
    if (!plan || !workbook_version_can_publish(index,owner,&plan->save)) return false;
    WorkbookVersionTagPlan canonical;
    if (workbook_version_tag_prepare(index,owner,&plan->save.root,(WorkbookVersionText){plan->label,plan->label_bytes},&canonical)!=VERSION_OK) return false;
    return plan->next_generation==canonical.next_generation;
}
