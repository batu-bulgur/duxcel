#ifndef DUXCEL_VERSION_TAG_H
#define DUXCEL_VERSION_TAG_H
#include "version_index.h"
typedef struct {
    /* User Save component; the complete tag publication also uses the explicit
       next generation below. Never publish a tag using save alone. */
    WorkbookVersionPlan save;
    uint64_t next_generation;
    size_t label_bytes;
    char label[DX_VERSION_LABEL_BYTES+1];
} WorkbookVersionTagPlan;
/* Pure current-head Create Tag planning. The native adapter must independently
   prove a complete immutable root. One atomic user publication updates Latest
   Saved and adds a unique immutable label; same-state labels share the root.
   Input/output are unchanged on failure. No rename/delete/eviction or IO. */
WorkbookVersionStatus workbook_version_tag_prepare(const WorkbookVersionIndex *index,WorkbookVersionOwner owner,
    const WorkbookVersionRoot *root,WorkbookVersionText label,WorkbookVersionTagPlan *out);
/* Current owner/index fence plus canonical tag plan validation. The plan owns
   its UTF-8 label and survives the original UI buffer or borrowed index. */
bool workbook_version_tag_can_publish(const WorkbookVersionIndex *index,WorkbookVersionOwner owner,
    const WorkbookVersionTagPlan *plan);
#endif
