#ifndef DUXCEL_OBJECT_IMPORT_DRAFT_H
#define DUXCEL_OBJECT_IMPORT_DRAFT_H
#include "object_create_command.h"
typedef struct {
    ObjectCreateTarget target;
    Analysis source;
    uint64_t request,row;
    size_t column;
    bool active,manual_formats;
    char name[64];
} ObjectImportDraft;
/* A pending DATABASE chooser owns its unresolved proposal and freezes the
   creation owner AFTER the caller assigned its probe request. No object/action,
   allocator or dirty state is published. Output must be zero or independently
   owned. App separately admits probe/definition/result coexistence. */
bool object_import_draft_begin(ObjectCreateContext *context,size_t sheet,uint64_t request,uint64_t row,size_t column,
    const Analysis *source,bool manual_formats,ObjectImportDraft *out,char **error);
/* Match only the captured request/destination and exact source proposal plus
   resolved schema/table enrichment. Other authored attributes remain strict.
   Returned command borrows reply and draft until immediate transaction return;
   workbook_apply_object_create must still recheck its captured full target.
   Failure preserves out/draft. No generic definition-equality rule is weakened. */
bool object_import_draft_command(const ObjectImportDraft *draft,uint64_t request,uint64_t row,size_t column,
    const Analysis *reply,ObjectCreateCommand *out,char **error);
void object_import_draft_free(ObjectImportDraft *draft);
#endif
