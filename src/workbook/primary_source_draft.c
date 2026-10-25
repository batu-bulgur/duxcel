#include "primary_source_draft.h"
#include "analysis_asset.h"
#include "definition_state.h"
#include <stdlib.h>
#include <string.h>
struct PrimarySourceDraft {
    const Workbook *book;
    const Analysis *authority;
    WorkbookId document;
    uint64_t session,revision,branch,high,request,primary_epoch;
    size_t count,active;
    const Worksheet *sheets;
    struct { uint64_t id,session,history,cells,formats; const char *name; bool hidden; } topology[DX_MAX_SHEETS];
    Analysis before_primary,before_cached,proposal,resolved;
    char *name;
    bool complete;
};
static PrimarySourceDraftStatus fail(PrimarySourceDraftStatus status,char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return status;
}
static bool add(size_t *value,size_t extra) { if (*value>SIZE_MAX-extra) return false; *value+=extra; return true; }
static PrimarySourceDraftStatus context(PrimarySourceDraftContext c,char **error) {
    if (!c.book || !c.primary || !c.request || !*c.request || !c.primary_epoch || !workbook_identity_valid(c.book) || !c.book->count || c.book->active>=c.book->count ||
        !c.book->edit_session_id || !c.book->sheets[c.book->active].edit_session_id) return fail(PRIMARY_SOURCE_INVALID,error,"Choose the actual initialized document and source probe request.");
    if (!c.ready || c.read_only) return fail(PRIMARY_SOURCE_PROTECTED,error,"Wait for an editable source chooser owner.");
    return PRIMARY_SOURCE_READY;
}
static bool numbers(const Analysis *a) {
    NumberType types[DX_ANALYSIS_FILTERS]; types[0]=a->filter_number;
    for (size_t i=1;i<DX_ANALYSIS_FILTERS;i++) types[i]=a->filters[i-1].number;
    for (size_t i=0;i<DX_ANALYSIS_FILTERS;i++) {
        NumberType n=types[i]; if ((unsigned)n.kind>NUM_FLOAT || (n.kind==NUM_DECIMAL ? !n.precision || n.precision>38 || n.scale>n.precision : n.precision || n.scale)) return false;
    }
    return true;
}
static bool room(size_t reserved,PrimarySourceDraftInfo info) { return add(&info.owned,info.validation_scratch) && add(&info.owned,info.naming_scratch) && reserved>=info.owned; }
PrimarySourceDraftStatus primary_source_draft_measure(PrimarySourceDraftContext c,const Analysis *proposal,const char *name,PrimarySourceDraftInfo *out,char **error) {
    PrimarySourceDraftStatus status=context(c,error); if (status!=PRIMARY_SOURCE_READY) return status;
    if (!proposal || !name || !out || proposal->kind==SOURCE_NONE || (unsigned)proposal->kind>SOURCE_DEMO || proposal->source_version ||
        (proposal->kind==SOURCE_DEMO ? proposal->path && *proposal->path : !proposal->path || !*proposal->path) ||
        proposal->table || (proposal->kind!=SOURCE_DATABASE ? proposal->schema!=NULL : proposal->schema && !*proposal->schema))
        return fail(PRIMARY_SOURCE_INVALID,error,"Choose a bounded unbound file/demo source or pending DATABASE default relation.");
    size_t length=0; while (length<=4096 && name[length]) length++;
    if (!length || length>4096) return fail(PRIMARY_SOURCE_INVALID,error,"Choose a bounded proposed worksheet name.");
    if (c.book->count==DX_MAX_SHEETS || c.book->sheet_high_water==UINT64_MAX) return fail(PRIMARY_SOURCE_LIMIT,error,"The document has no remaining sheet identity or count capacity.");
    AnalysisAssetMeasure measure={0};
    if (!numbers(proposal) || analysis_asset_measure(1,0,proposal,0,&measure)!=ANALYSIS_ASSET_OK) return fail(PRIMARY_SOURCE_INVALID,error,"Invalid bounded source proposal.");
    PrimarySourceDraftInfo info={sizeof(PrimarySourceDraft),measure.validation_scratch,2u*4097u};
    if (!add(&info.owned,measure.owned-sizeof(Analysis)) || !add(&info.owned,4097)) return fail(PRIMARY_SOURCE_LIMIT,error,"Source proposal ownership accounting overflowed.");
    *out=info; return PRIMARY_SOURCE_READY;
}
PrimarySourceDraftStatus primary_source_draft_begin(PrimarySourceDraftContext c,const Analysis *proposal,const char *name,size_t reserved,PrimarySourceDraft **out,char **error) {
    if (!out || *out) return fail(PRIMARY_SOURCE_INVALID,error,"Choose an empty pending primary draft.");
    PrimarySourceDraftInfo info={0}; PrimarySourceDraftStatus status=primary_source_draft_measure(c,proposal,name,&info,error); if (status!=PRIMARY_SOURCE_READY) return status;
    if (!room(reserved,info)) return fail(PRIMARY_SOURCE_LIMIT,error,"The pending source/name/parser exceeds its admitted budget.");
    if (!analysis_filters_valid(proposal,error)) return PRIMARY_SOURCE_INVALID;
    char *unique=workbook_unique_sheet_name(c.book,name,error); if (!unique) return PRIMARY_SOURCE_INVALID;
    PrimarySourceDraft *draft=dx_alloc(sizeof(*draft));
    *draft=(PrimarySourceDraft){.book=c.book,.authority=c.primary,.document=c.book->id,.session=c.book->edit_session_id,.revision=c.book->command_revision,
        .branch=c.book->history_branch,.high=c.book->sheet_high_water,.request=*c.request,.primary_epoch=*c.primary_epoch,.count=c.book->count,.active=c.book->active,
        .sheets=c.book->sheets,.before_primary=*c.primary,.before_cached=c.book->sheets[c.book->active].analysis,.proposal=analysis_copy(proposal),.name=unique};
    for (size_t i=0;i<c.book->count;i++) {
        draft->topology[i].id=c.book->sheets[i].id; draft->topology[i].session=c.book->sheets[i].edit_session_id;
        draft->topology[i].history=c.book->sheets[i].history_last; draft->topology[i].cells=c.book->sheets[i].cells.revision; draft->topology[i].formats=c.book->sheets[i].formats.revision;
        draft->topology[i].name=c.book->sheets[i].name; draft->topology[i].hidden=c.book->sheets[i].hidden;
    }
    *out=draft; return PRIMARY_SOURCE_READY;
}
PrimarySourceDraftStatus primary_source_draft_check(PrimarySourceDraftContext c,const PrimarySourceDraft *draft,char **error) {
    PrimarySourceDraftStatus status=context(c,error); if (status!=PRIMARY_SOURCE_READY) return status;
    if (!draft) return fail(PRIMARY_SOURCE_INVALID,error,"Choose a pending primary source owner.");
    const Workbook *book=c.book;
    if (book!=draft->book || c.primary!=draft->authority || book->id.high!=draft->document.high || book->id.low!=draft->document.low ||
        book->edit_session_id!=draft->session || book->command_revision!=draft->revision || book->history_branch!=draft->branch || book->sheet_high_water!=draft->high ||
        book->count!=draft->count || book->active!=draft->active || book->sheets!=draft->sheets || *c.request!=draft->request || *c.primary_epoch!=draft->primary_epoch)
        return fail(PRIMARY_SOURCE_STALE,error,"The document, topology or probe request changed before source publication.");
    for (size_t i=0;i<book->count;i++) if (book->sheets[i].id!=draft->topology[i].id || book->sheets[i].edit_session_id!=draft->topology[i].session ||
        book->sheets[i].name!=draft->topology[i].name || book->sheets[i].hidden!=draft->topology[i].hidden || book->sheets[i].history_last!=draft->topology[i].history ||
        book->sheets[i].cells.revision!=draft->topology[i].cells || book->sheets[i].formats.revision!=draft->topology[i].formats)
        return fail(PRIMARY_SOURCE_STALE,error,"A worksheet owner changed while resolving the new source.");
    if (memcmp(c.primary,&draft->before_primary,sizeof(Analysis)) || memcmp(&book->sheets[book->active].analysis,&draft->before_cached,sizeof(Analysis)))
        return fail(PRIMARY_SOURCE_STALE,error,"Current primary/cache ownership changed while resolving the new source.");
    return PRIMARY_SOURCE_READY;
}
PrimarySourceDraftStatus primary_source_draft_reply_measure(PrimarySourceDraftContext c,const PrimarySourceDraft *draft,PrimarySourceDraftReply reply,PrimarySourceDraftInfo *out,char **error) {
    PrimarySourceDraftStatus status=primary_source_draft_check(c,draft,error); if (status!=PRIMARY_SOURCE_READY) return status;
    if (draft->complete) return fail(PRIMARY_SOURCE_PROTECTED,error,"The resolved candidate is immutable until taken or cancelled.");
    if (!out || !reply.analysis) return fail(PRIMARY_SOURCE_INVALID,error,"Choose the actual owned source probe reply.");
    if (reply.request!=draft->request) return fail(PRIMARY_SOURCE_STALE,error,"The source completion belongs to another probe request.");
    if (reply.error) return fail(PRIMARY_SOURCE_FAILED,error,"The source probe failed; no document or candidate was published.");
    if (reply.analysis->kind!=draft->proposal.kind) return fail(PRIMARY_SOURCE_STALE,error,"The source probe changed the proposed source kind.");
    if (!numbers(reply.analysis) || definition_baseline_status((DefinitionDependencyView){reply.analysis,NULL})!=DEFINITION_BASELINE_READY)
        return fail(PRIMARY_SOURCE_INVALID,error,"The source probe did not close a valid source definition.");
    Analysis unchanged=*reply.analysis;
    if (draft->proposal.kind==SOURCE_DATABASE) {
        if (draft->proposal.schema && strcmp(draft->proposal.schema,reply.analysis->schema)) return fail(PRIMARY_SOURCE_STALE,error,"The source completion changed an explicitly selected schema.");
        unchanged.schema=draft->proposal.schema; unchanged.table=NULL;
    }
    if (!definition_analysis_equal(&unchanged,&draft->proposal)) return fail(PRIMARY_SOURCE_STALE,error,"The source completion changed authored proposal fields.");
    AnalysisAssetMeasure before={0},after={0};
    if (analysis_asset_measure(1,0,&draft->proposal,0,&before)!=ANALYSIS_ASSET_OK || analysis_asset_measure(1,0,reply.analysis,0,&after)!=ANALYSIS_ASSET_OK)
        return fail(PRIMARY_SOURCE_INVALID,error,"Invalid complete source proposal grammar shape.");
    *out=(PrimarySourceDraftInfo){after.owned-sizeof(Analysis),before.validation_scratch>after.validation_scratch ? before.validation_scratch : after.validation_scratch,0};
    return PRIMARY_SOURCE_READY;
}
PrimarySourceDraftStatus primary_source_draft_resolve(PrimarySourceDraftContext c,PrimarySourceDraft *draft,PrimarySourceDraftReply reply,size_t reserved,char **error) {
    PrimarySourceDraftInfo info={0}; PrimarySourceDraftStatus status=primary_source_draft_reply_measure(c,draft,reply,&info,error); if (status!=PRIMARY_SOURCE_READY) return status;
    if (!room(reserved,info)) return fail(PRIMARY_SOURCE_LIMIT,error,"Resolved source ownership/parser exceeds its admitted budget.");
    if (!analysis_filters_valid(&draft->proposal,error) || !analysis_filters_valid(reply.analysis,error)) return PRIMARY_SOURCE_INVALID;
    Analysis candidate=analysis_copy(reply.analysis);
    status=primary_source_draft_check(c,draft,error); if (status!=PRIMARY_SOURCE_READY) { analysis_free(&candidate); return status; }
    draft->resolved=candidate; draft->complete=true; return PRIMARY_SOURCE_RESOLVED;
}
const Analysis *primary_source_draft_source(const PrimarySourceDraft *draft) { return draft && draft->complete ? &draft->resolved : NULL; }
const char *primary_source_draft_name(const PrimarySourceDraft *draft) { return draft ? draft->name : NULL; }
size_t primary_source_draft_owned(const PrimarySourceDraft *draft) {
    if (!draft) return 0; size_t bytes=sizeof(*draft);
    return add(&bytes,analysis_text_bytes(&draft->proposal)) && add(&bytes,analysis_text_bytes(&draft->resolved)) && add(&bytes,strlen(draft->name)+1) ? bytes : SIZE_MAX;
}
PrimarySourceDraftStatus primary_source_draft_take(PrimarySourceDraftContext c,PrimarySourceDraft **draft,PrimarySourceCandidate *out,char **error) {
    if (!draft || !*draft || !out || out->name || memcmp(&out->source,&(Analysis){0},sizeof(Analysis))) return fail(PRIMARY_SOURCE_INVALID,error,"Choose an empty independent source candidate output.");
    PrimarySourceDraftStatus status=primary_source_draft_check(c,*draft,error); if (status!=PRIMARY_SOURCE_READY) return status;
    if (!(*draft)->complete) return fail(PRIMARY_SOURCE_PROTECTED,error,"Resolve the source proposal before transferring ownership.");
    out->source=(*draft)->resolved; out->name=(*draft)->name; (*draft)->resolved=(Analysis){0}; (*draft)->name=NULL;
    primary_source_draft_free(draft); return PRIMARY_SOURCE_TAKEN;
}
void primary_source_candidate_free(PrimarySourceCandidate *candidate) { if (candidate) { analysis_free(&candidate->source); free(candidate->name); *candidate=(PrimarySourceCandidate){0}; } }
void primary_source_draft_free(PrimarySourceDraft **draft) { if (draft && *draft) { analysis_free(&(*draft)->proposal); analysis_free(&(*draft)->resolved); free((*draft)->name); free(*draft); *draft=NULL; } }
