#include "object_import_draft.h"
#include "definition_baseline.h"
#include "definition_state.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
void object_import_draft_free(ObjectImportDraft *draft) { if (draft) { analysis_free(&draft->source); *draft=(ObjectImportDraft){0}; } }
bool object_import_draft_begin(ObjectCreateContext *c,size_t sheet,uint64_t request,uint64_t row,size_t column,
    const Analysis *source,bool manual_formats,ObjectImportDraft *out,char **error) {
    if (!c || !c->command.request_epoch || !request || *c->command.request_epoch!=request || !source || !out || source->kind!=SOURCE_DATABASE ||
        source->table || source->schema || !source->path || !*source->path || row>DX_SHEET_ROWS-5 || column>16384-4)
        return fail(error,"Choose an unresolved database source and a valid import destination.");
    AnalysisAssetMeasure m={0};
    if (analysis_asset_measure(1,0,source,0,&m)!=ANALYSIS_ASSET_OK || m.validation_scratch || !analysis_filters_valid(source,error))
        return fail(error,"The pending database source is invalid or needs unsupported parser staging.");
    ObjectImportDraft next={.request=request,.row=row,.column=column,.active=true,.manual_formats=manual_formats};
    if (!workbook_prepare_object_create(c,sheet,&next.target,error)) return false;
    if (next.target.owner.objects.next_id==UINT64_MAX || next.target.owner.objects.count==DX_MAX_OBJECTS)
        return fail(error,"This worksheet has exhausted its object count or identities.");
    snprintf(next.name,sizeof(next.name),"Table %" PRIu64,next.target.owner.objects.next_id+1);
    next.source=analysis_copy(source); object_import_draft_free(out); *out=next; return true;
}
bool object_import_draft_command(const ObjectImportDraft *draft,uint64_t request,uint64_t row,size_t column,const Analysis *reply,ObjectCreateCommand *out,char **error) {
    if (!draft || !draft->active || !out || !reply || request!=draft->request || row!=draft->row || column!=draft->column)
        return fail(error,"The database import request or destination changed; choose it again.");
    if (reply->kind!=SOURCE_DATABASE || definition_baseline_status((DefinitionDependencyView){reply,NULL})!=DEFINITION_BASELINE_READY)
        return fail(error,"The database did not return a resolved source relation.");
    Analysis projected=*reply; projected.schema=draft->source.schema; projected.table=draft->source.table;
    if (!definition_analysis_equal(&projected,&draft->source)) return fail(error,"The database completion does not match its captured source proposal.");
    *out=(ObjectCreateCommand){.kind=OBJECT_TABLE,.row=row,.column=column,.name=draft->name,.source=reply}; return true;
}
