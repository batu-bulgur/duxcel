#include "analysis_command.h"
#include "definition_baseline.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) {
    for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i;
    return NULL;
}
static bool same_text(const char *a,const char *b) { return a && b ? !strcmp(a,b) : a==b; }
static bool same_connection(const Analysis *a,const Analysis *b) {
    return a->kind==b->kind && a->source_version==b->source_version && same_text(a->path,b->path) && same_text(a->schema,b->schema) && same_text(a->table,b->table);
}
bool workbook_prepare_analysis_transform(AnalysisCommandContext *c,size_t index,AnalysisTransformTarget *out,char **error) {
    if (!c || !c->live || !c->source_epoch || !out || !workbook_identity_valid(c->book) || index>=c->book->count)
        return fail(error,"Choose a valid primary source before transforming it.");
    if (!c->ready || c->read_only || c->live->kind==SOURCE_NONE || definition_baseline_status((DefinitionDependencyView){c->live,NULL})!=DEFINITION_BASELINE_READY)
        return fail(error,"Wait for an editable, resolved primary source before transforming it.");
    CellEditTarget owner={0}; if (!workbook_prepare_cell_edit(c->book,index,0,0,&owner,error)) return false;
    Worksheet *s=c->book->sheets+index;
    *out=(AnalysisTransformTarget){owner,c->live==&s->analysis ? NULL : c->live,*c->source_epoch,s->formats.revision,s->history_last,s->source_versions.high_water}; return true;
}
static bool scratch(const Analysis *a,uint64_t sheet,uint64_t owners,size_t *peak,char **error) {
    AnalysisAssetMeasure m={0};
    if (analysis_asset_measure(sheet,0,a,owners,&m)!=ANALYSIS_ASSET_OK) return fail(error,"Invalid bounded primary source definition.");
    if (m.validation_scratch>*peak) *peak=m.validation_scratch;
    return true;
}
bool workbook_analysis_transform_scratch(const Worksheet *s,const Analysis *live,const Analysis *next,size_t *out,char **error) {
    if (!s || !live || !next || !out || s->undo_count>DX_HISTORY || s->redo_count>DX_HISTORY) return fail(error,"Invalid bounded primary source history.");
    size_t peak=0;
    if (!scratch(live,s->id,s->objects.next_id,&peak,error) || !scratch(next,s->id,s->objects.next_id,&peak,error) ||
        (live!=&s->analysis && !scratch(&s->analysis,s->id,s->objects.next_id,&peak,error))) return false;
    for (size_t i=0;i<s->undo_count;i++) if (!scratch(s->undo+i,s->id,s->objects.next_id,&peak,error)) return false;
    for (size_t i=0;i<s->redo_count;i++) if (!scratch(s->redo+i,s->id,s->objects.next_id,&peak,error)) return false;
    *out=peak; return true;
}
static AnalysisTransformResult check(AnalysisCommandContext *c,const AnalysisTransformTarget *t,Analysis *next,char **error) {
    AnalysisTransformResult r={.status=ANALYSIS_TRANSFORM_INVALID};
    if (!c || !c->live || !c->source_epoch || !t || !next || !workbook_identity_valid(c->book) || !t->owner.document || !t->owner.sheet || t->owner.row || t->owner.column) {
        fail(error,"Invalid primary source command."); return r;
    }
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); r.before_revision=r.after_revision=b->command_revision;
    r.status=ANALYSIS_TRANSFORM_STALE;
    if (!s || b->id.high!=t->owner.workbook.high || b->id.low!=t->owner.workbook.low || b->edit_session_id!=t->owner.document || s->edit_session_id!=t->owner.sheet ||
        c->live!=(t->external_authority ? t->external_authority : &s->analysis) || *c->source_epoch!=t->source_epoch || b->command_revision!=t->owner.expected_document_revision ||
        s->cells.revision!=t->owner.expected_revision || s->formats.revision!=t->format_revision || s->history_last!=t->history_last || s->source_versions.high_water!=t->source_high_water) {
        fail(error,"The primary source owner changed; prepare the transformation again."); return r;
    }
    r.status=ANALYSIS_TRANSFORM_PROTECTED;
    if (!c->ready || c->read_only || c->live->kind==SOURCE_NONE || !same_connection(c->live,next) ||
        definition_baseline_status((DefinitionDependencyView){c->live,NULL})!=DEFINITION_BASELINE_READY) {
        fail(error,"This transformation requires an editable, resolved source with the same connection."); return r;
    }
    r.status=ANALYSIS_TRANSFORM_INVALID;
    if (s->undo_count>DX_HISTORY || s->redo_count>DX_HISTORY || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) return r;
    /* Admit grammar scratch before the ownership validator parses ANY retained
       state. It validates one definition at a time; peaks do not accumulate. */
    size_t validation=0;
    if (!workbook_analysis_transform_scratch(s,c->live,next,&validation,error)) return r;
    if (validation>c->validation_reserved) { r.status=ANALYSIS_TRANSFORM_LIMIT; fail(error,"Primary source validation exceeds the admitted scratch budget."); return r; }
    bool changed=false;
    if (!worksheet_analysis_preflight(s,c->live,next,&changed,error)) return r;
    DefinitionDependencySelection deps={0};
    if (definition_dependencies_select(s->id,(DefinitionDependencyView){c->live,NULL},(DefinitionDependencyView){next,NULL},&s->source_versions,s->objects.next_id,&deps)!=DEFINITION_DEPENDENCIES_OK) {
        fail(error,"The primary source bindings are incomplete; the prepared edit is kept."); return r;
    }
    if (definition_analysis_equal(c->live,next)) { r.status=ANALYSIS_TRANSFORM_NOOP; return r; }
    r.status=ANALYSIS_TRANSFORM_LIMIT;
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available()) { fail(error,"Primary source revision space is exhausted."); return r; }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID) { r.status=ANALYSIS_TRANSFORM_INVALID; fail(error,"Worksheet history is incomplete; no transformation was applied."); return r; }
    r.status=ANALYSIS_TRANSFORM_READY; return r;
}
AnalysisTransformResult workbook_apply_analysis_transform_recorded(AnalysisCommandContext *c,const AnalysisTransformTarget *t,Analysis *owned,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    AnalysisTransformResult r=check(c,t,owned,error);
    if (r.status==ANALYSIS_TRANSFORM_NOOP) { analysis_free(owned); return r; }
    if (r.status!=ANALYSIS_TRANSFORM_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); void *ticket=NULL;
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=ANALYSIS_TRANSFORM_INVALID; fail(error,"Definition recording is incomplete; the prepared edit is kept."); return r; }
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,
            .expected_revision=b->command_revision,.revision=b->command_revision+1,.validation_reserved=c->validation_reserved,
            .view={.sheet=s->id,.object_high_water=s->objects.next_id,.effects=1,.before={c->live,NULL},.after={owned,NULL},.sources=&s->source_versions}};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) {
            bool invalid=reserved || ticket!=NULL; if (ticket) recorder->cancel(recorder->context,ticket);
            r.status=invalid ? ANALYSIS_TRANSFORM_INVALID : ANALYSIS_TRANSFORM_LIMIT;
            if (!error || !*error) fail(error,"Definition recording is busy; the prepared edit is kept."); return r;
        }
        r=check(c,t,owned,error);
        if (r.status!=ANALYSIS_TRANSFORM_READY) { recorder->cancel(recorder->context,ticket); if (r.status==ANALYSIS_TRANSFORM_NOOP) analysis_free(owned); return r; }
        s=resolve(b,t->owner.sheet_id);
    }
    worksheet_analysis_adopt(s,c->live,owned); worksheet_record(s,EDIT_ANALYSIS);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b);
    r.status=ANALYSIS_TRANSFORM_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
}
