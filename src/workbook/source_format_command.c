#include "source_format_command.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static const Analysis *owner_source(const Worksheet *s,const Analysis *primary,uint64_t owner) {
    if (!owner) return primary;
    for (size_t i=0;i<s->objects.count;i++) if (s->objects.items[i].id==owner && s->objects.items[i].kind==OBJECT_TABLE) return &s->objects.items[i].source;
    return NULL;
}
static bool eligible(const Analysis *a) { return a && (a->kind==SOURCE_CSV || a->kind==SOURCE_PARQUET); }
bool workbook_prepare_source_format(SourceFormatCommandContext *c,size_t index,uint64_t object,SourceFormatCommandTarget *out,char **error) {
    if (!c || !c->primary || !c->request_epoch || !out || !workbook_identity_valid(c->book) || index>=c->book->count)
        return fail(error,"Choose a valid source formatting owner.");
    Worksheet *s=c->book->sheets+index;
    if (s->objects.count>DX_MAX_OBJECTS || (s->objects.count && !s->objects.items) || s->source_versions.count>DX_SOURCE_VERSIONS || (s->source_versions.count && !s->source_versions.items))
        return fail(error,"Invalid bounded source formatting ownership.");
    if (!c->ready || c->read_only || !eligible(owner_source(s,c->primary,object))) return fail(error,"Wait for an editable, verified raw source.");
    CellEditTarget owner={0}; if (!workbook_prepare_cell_edit(c->book,index,0,0,&owner,error)) return false;
    memset(out,0,sizeof(*out)); out->owner=owner; out->object=object; out->authority=c->primary==&s->analysis ? NULL : c->primary;
    out->request_epoch=*c->request_epoch; out->format_revision=s->formats.revision; out->history_last=s->history_last;
    memcpy(&out->primary,c->primary,sizeof(out->primary)); memcpy(&out->cached,&s->analysis,sizeof(out->cached));
    memcpy(&out->objects,&s->objects,sizeof(out->objects)); memcpy(&out->sources,&s->source_versions,sizeof(out->sources));
    if (s->objects.count) memcpy(out->items,s->objects.items,s->objects.count*sizeof(*out->items));
    if (s->source_versions.count) memcpy(out->versions,s->source_versions.items,s->source_versions.count*sizeof(*out->versions));
    return true;
}
static bool format_safe(FormatSnapshotView f) {
    if (f.source_aware) return true;
    if (f.scope_count || f.count>DX_FORMAT_PATCHES || (f.count && !f.items)) return false;
    for (size_t i=0;i<f.count;i++) if (f.items[i].selector!=FORMAT_SPATIAL || f.items[i].range.r0 || f.items[i].range.r1!=DX_SHEET_ROWS-1) return false;
    return true;
}
static bool history_safe(const FormatStore *f) {
    if (f->undo_count>DX_FORMAT_HISTORY || f->redo_count>DX_FORMAT_HISTORY || !format_safe(format_delta_store_view(f))) return false;
    for (size_t i=0;i<f->undo_count;i++) if (!format_safe(format_delta_snapshot_view(f->undo+i))) return false;
    for (size_t i=0;i<f->redo_count;i++) if (!format_safe(format_delta_snapshot_view(f->redo+i))) return false;
    return true;
}
static bool independent_format(const FormatStore *f,const FormatSnapshot *p,char **error) {
    for (size_t i=0;i<1+f->undo_count+f->redo_count;i++) {
        FormatSnapshot current={f->items,f->count,f->scopes,f->scope_count,f->source_aware};
        const FormatSnapshot *owned=i==0 ? &current : i-1<f->undo_count ? f->undo+i-1 : f->redo+i-1-f->undo_count;
        if ((p->items && p->items==owned->items) || (p->scopes && p->scopes==owned->scopes)) return fail(error,"Source formatting requires an independent owned snapshot.");
    }
    return true;
}
static bool row_equal(const SourceVersion *a,const SourceVersion *b) {
    return a->owner==b->owner && a->version==b->version && a->kind==b->kind && !strcmp(a->path,b->path) &&
        a->raw.bytes==b->raw.bytes && (!a->raw.bytes || !memcmp(a->raw.data,b->raw.data,a->raw.bytes));
}
static bool ledger(const Worksheet *s,const SourceFormatCommandTarget *t,const SourceVersions *next,char **error) {
    const SourceVersions *old=&s->source_versions;
    if (!source_versions_valid(old,s->objects.next_id,error) || !source_versions_valid(next,s->objects.next_id,error)) return false;
    if (old->high_water==UINT64_MAX || next->high_water!=old->high_water+1 || next->count!=old->count+1 || next->items==old->items)
        return fail(error,"Source formatting requires one newly issued immutable version.");
    for (size_t i=0;i<old->count;i++) if (!row_equal(old->items+i,next->items+i)) return fail(error,"Source formatting cannot rewrite retained source versions.");
    for (size_t i=0;i<next->count;i++) for (size_t j=0;j<old->count;j++)
        if (next->items[i].path==old->items[j].path || (next->items[i].raw.data && next->items[i].raw.data==old->items[j].raw.data))
            return fail(error,"Prepare an independent owned source ledger before adoption.");
    const SourceVersion *entry=next->items+old->count;
    if (entry->version!=next->high_water || entry->owner!=t->object || !entry->raw.bytes) return fail(error,"The new formatting source version is incomplete or belongs to another owner.");
    return true;
}
static bool scratch(const Worksheet *s,const Analysis *primary,const SourceFormatCommandPlan *p,size_t *peak,char **error) {
    if (s->undo_count>DX_HISTORY || s->redo_count>DX_HISTORY || s->objects.undo_count>DX_HISTORY || s->objects.redo_count>DX_HISTORY ||
        s->formats.undo_count>DX_FORMAT_HISTORY || s->formats.redo_count>DX_FORMAT_HISTORY) return fail(error,"Invalid bounded composite history.");
    const Analysis *a[]={primary,&s->analysis,&p->primary};
    for (size_t i=0;i<3+s->undo_count+s->redo_count;i++) {
        const Analysis *value=i<3 ? a[i] : i-3<s->undo_count ? s->undo+i-3 : s->redo+i-3-s->undo_count;
        AnalysisAssetMeasure m={0}; if (analysis_asset_measure(s->id,0,value,s->objects.next_id,&m)!=ANALYSIS_ASSET_OK) return fail(error,"Invalid bounded retained primary definition.");
        if (m.validation_scratch>*peak) *peak=m.validation_scratch;
    }
    ObjectSnapshot current={s->objects.items,s->objects.count};
    for (size_t i=0;i<2+s->objects.undo_count+s->objects.redo_count;i++) {
        const ObjectSnapshot *value=i==0 ? &current : i==1 ? &p->objects : i-2<s->objects.undo_count ? s->objects.undo+i-2 : s->objects.redo+i-2-s->objects.undo_count;
        ObjectsAssetInfo m={0}; if (objects_asset_measure(s->id,value,s->objects.next_id,&m)!=OBJECTS_ASSET_OK) return fail(error,"Invalid bounded retained object definition.");
        if (m.measure.validation_scratch>*peak) *peak=m.measure.validation_scratch;
    }
    size_t scopes=s->formats.scope_count>p->formats.scope_count ? s->formats.scope_count : p->formats.scope_count;
    for (size_t i=0;i<s->formats.undo_count+s->formats.redo_count;i++) {
        size_t n=i<s->formats.undo_count ? s->formats.undo[i].scope_count : s->formats.redo[i-s->formats.undo_count].scope_count;
        if (n>scopes) scopes=n;
    }
    if (scopes>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS) return fail(error,"Invalid bounded retained format scopes.");
    if (scopes) {
        size_t n=analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true)+scopes;
        if (scopes>DX_FORMAT_GUARDS && scopes*sizeof(const FormatScope *)>n) n=scopes*sizeof(const FormatScope *);
        if (n>*peak) *peak=n;
    }
    return true;
}
bool workbook_source_format_scratch(const Worksheet *s,const Analysis *primary,const SourceFormatCommandPlan *p,size_t *out,char **error) {
    if (!s || !primary || !p || !out) return fail(error,"Choose complete source formatting plans before measuring scratch.");
    size_t peak=0; if (!scratch(s,primary,p,&peak,error)) return false; *out=peak; return true;
}
static DefinitionEventView view(const Worksheet *s,const SourceFormatCommandContext *c,const SourceFormatCommandTarget *t,const SourceFormatCommandPlan *p,const ObjectSnapshot *before) {
    DefinitionEventView v={.sheet=s->id,.object_high_water=s->objects.next_id,.effects=t->object ? 12 : 9,
        .before_formats=format_delta_store_view(&s->formats),.after_formats=format_delta_snapshot_view(&p->formats),.sources=&p->sources};
    if (t->object) { v.before.objects=before; v.after.objects=&p->objects; }
    else { v.before.primary=c->primary; v.after.primary=&p->primary; }
    return v;
}
SourceFormatCommandResult workbook_check_source_format_owner(SourceFormatCommandContext *c,const SourceFormatCommandTarget *t,char **error) {
    SourceFormatCommandResult r={.status=SOURCE_FORMAT_COMMAND_INVALID};
    if (!c || !c->primary || !c->request_epoch || !t || !workbook_identity_valid(c->book) || !t->owner.document || !t->owner.sheet || t->owner.row || t->owner.column) {
        fail(error,"Invalid composite source formatting command."); return r;
    }
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); r.before_revision=r.after_revision=b->command_revision; r.status=SOURCE_FORMAT_COMMAND_STALE;
    if (!s || b->id.high!=t->owner.workbook.high || b->id.low!=t->owner.workbook.low || b->edit_session_id!=t->owner.document || s->edit_session_id!=t->owner.sheet ||
        b->command_revision!=t->owner.expected_document_revision || s->cells.revision!=t->owner.expected_revision || s->formats.revision!=t->format_revision || s->history_last!=t->history_last ||
        c->primary!=(t->authority ? t->authority : &s->analysis) || *c->request_epoch!=t->request_epoch || memcmp(c->primary,&t->primary,sizeof(t->primary)) || memcmp(&s->analysis,&t->cached,sizeof(t->cached)) ||
        memcmp(&s->objects,&t->objects,sizeof(t->objects)) || memcmp(&s->source_versions,&t->sources,sizeof(t->sources)) ||
        s->objects.count>DX_MAX_OBJECTS || s->source_versions.count>DX_SOURCE_VERSIONS ||
        (s->objects.count && (!s->objects.items || memcmp(s->objects.items,t->items,s->objects.count*sizeof(*t->items)))) ||
        (s->source_versions.count && (!s->source_versions.items || memcmp(s->source_versions.items,t->versions,s->source_versions.count*sizeof(*t->versions))))) {
        fail(error,"The source formatting owner changed; prepare the command again."); return r;
    }
    r.status=SOURCE_FORMAT_COMMAND_PROTECTED;
    if (!c->ready || c->read_only || !eligible(owner_source(s,c->primary,t->object))) { fail(error,"The source formatting owner is unavailable."); return r; }
    r.status=SOURCE_FORMAT_COMMAND_READY; return r;
}
static SourceFormatCommandResult check(SourceFormatCommandContext *c,const SourceFormatCommandTarget *t,SourceFormatCommandPlan *p,char **error) {
    if (!p) { fail(error,"Prepare complete source formatting plans."); return (SourceFormatCommandResult){.status=SOURCE_FORMAT_COMMAND_INVALID}; }
    SourceFormatCommandResult r=workbook_check_source_format_owner(c,t,error); if (r.status!=SOURCE_FORMAT_COMMAND_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id);
    if (!history_safe(&s->formats)) { r.status=SOURCE_FORMAT_COMMAND_PROTECTED; fail(error,"Legacy finite formatting requires its verified baseline before source binding."); return r; }
    if (owner_source(s,c->primary,t->object)->source_version) { r.status=SOURCE_FORMAT_COMMAND_PROTECTED; fail(error,"First binding requires an unbound source."); return r; }
    r.status=SOURCE_FORMAT_COMMAND_LIMIT;
    if (b->command_revision==UINT64_MAX || s->formats.revision==UINT64_MAX || s->source_versions.high_water==UINT64_MAX || s->source_versions.count==DX_SOURCE_VERSIONS || !worksheet_history_order_available()) { fail(error,"Source formatting revision space is exhausted."); return r; }
    r.status=SOURCE_FORMAT_COMMAND_INVALID;
    size_t validation=0;
    if (!independent_format(&s->formats,&p->formats,error)) return r;
    if (!workbook_source_format_scratch(s,c->primary,p,&validation,error)) return r;
    if (validation>c->validation_reserved) { r.status=SOURCE_FORMAT_COMMAND_LIMIT; fail(error,"Composite validation exceeds the admitted scratch budget."); return r; }
    if (!ledger(s,t,&p->sources,error)) return r;
    for (size_t i=0;i<s->formats.undo_count+s->formats.redo_count;i++) {
        const FormatSnapshot *state=i<s->formats.undo_count ? s->formats.undo+i : s->formats.redo+i-s->formats.undo_count;
        FormatStore retained={.items=state->items,.count=state->count,.scopes=state->scopes,.scope_count=state->scope_count,.source_aware=state->source_aware};
        if (!format_valid(&retained,error)) return r;
    }
    const Analysis *old=owner_source(s,c->primary,t->object),*next=NULL;
    ObjectSnapshot before={s->objects.items,s->objects.count};
    if (!t->object) {
        if (p->objects.items || p->objects.count) { fail(error,"A primary formatting command cannot replace objects."); return r; }
        next=&p->primary;
    } else {
        Analysis empty={0};
        if (!definition_analysis_equal(&p->primary,&empty) || p->objects.count!=before.count || !p->objects.items || p->objects.items==before.items) { fail(error,"Prepare an independent complete Table formatting graph."); return r; }
        SheetObject projected[DX_MAX_OBJECTS]; memcpy(projected,p->objects.items,p->objects.count*sizeof(*projected));
        for (size_t i=0;i<p->objects.count;i++) if (projected[i].id==t->object && projected[i].kind==OBJECT_TABLE) { next=&p->objects.items[i].source; projected[i].source.source_version=old->source_version; }
        for (size_t i=0;i<p->objects.count;i++) if (projected[i].revision!=before.items[i].revision || projected[i].row!=before.items[i].row ||
            projected[i].col!=before.items[i].col || projected[i].rows!=before.items[i].rows || projected[i].columns!=before.items[i].columns) {
            fail(error,"Source binding cannot change object placement, extents or query revisions."); return r;
        }
        ObjectSnapshot unchanged={projected,p->objects.count};
        if (!next || !definition_objects_equal(&before,&unchanged)) { fail(error,"Source formatting cannot change other object definitions."); return r; }
    }
    Analysis unchanged=*next; unchanged.source_version=old->source_version;
    if (next->source_version!=p->sources.high_water || !definition_analysis_equal(old,&unchanged)) { fail(error,"Source formatting may only bind its unchanged source definition."); return r; }
    DefinitionEventView event=view(s,c,t,p,&before); DefinitionEventInfo info={0};
    DefinitionEventStatus measured=definition_event_measure(event,c->validation_reserved,&info);
    if (measured!=DEFINITION_EVENT_OK) { r.status=measured==DEFINITION_EVENT_LIMIT ? SOURCE_FORMAT_COMMAND_LIMIT : SOURCE_FORMAT_COMMAND_INVALID; fail(error,"The complete source formatting event does not fit its admitted closed profile."); return r; }
    FormatDependencySelection used={0};
    if (format_dependencies_select(s->id,(FormatSnapshotView){0},event.after_formats,&p->sources,s->objects.next_id,&used)!=FORMAT_DEPENDENCIES_OK || !used.rows[p->sources.count-1]) {
        fail(error,"The format plan must reference its newly bound source version."); return r;
    }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID) { fail(error,"Worksheet history is incomplete; no source formatting was applied."); return r; }
    bool changed=false;
    if (!t->object && (!worksheet_analysis_preflight(s,c->primary,&p->primary,&changed,error) || !changed)) return r;
    if (t->object) {
        if (s->objects.undo_count>=DX_HISTORY) { r.status=SOURCE_FORMAT_COMMAND_LIMIT; fail(error,"The complete Table binding history needs room before adoption."); return r; }
        size_t undo_bytes=0,redo_bytes=0;
        for (size_t i=0;i<1+s->objects.undo_count+s->objects.redo_count;i++) {
            const ObjectSnapshot *state=i==0 ? &before : i-1<s->objects.undo_count ? s->objects.undo+i-1 : s->objects.redo+i-1-s->objects.undo_count;
            ObjectsAssetInfo m={0}; ObjectStore retained={.items=state->items,.count=state->count,.next_id=s->objects.next_id};
            if (objects_asset_measure(s->id,state,s->objects.next_id,&m)!=OBJECTS_ASSET_OK || !objects_valid(&retained,error)) return r;
            size_t *bytes=i<=s->objects.undo_count ? &undo_bytes : &redo_bytes;
            if (m.measure.owned>DX_OBJECT_HISTORY_BYTES-*bytes) { r.status=SOURCE_FORMAT_COMMAND_LIMIT; fail(error,"Complete Table binding history exceeds its owned bound."); return r; }
            *bytes+=m.measure.owned;
        }
    }
    FormatStore projected=s->formats; projected.items=p->formats.items; projected.count=p->formats.count; projected.scopes=p->formats.scopes;
    projected.scope_count=p->formats.scope_count; projected.source_aware=p->formats.source_aware; projected.redo_count=0;
    if (projected.undo_count==DX_FORMAT_HISTORY) { memmove(projected.undo,projected.undo+1,(DX_FORMAT_HISTORY-1)*sizeof(*projected.undo)); projected.undo_count--; }
    projected.undo[projected.undo_count++]=(FormatSnapshot){s->formats.items,s->formats.count,s->formats.scopes,s->formats.scope_count,s->formats.source_aware};
    if (!format_valid(&projected,error)) { r.status=SOURCE_FORMAT_COMMAND_LIMIT; return r; }
    r.status=SOURCE_FORMAT_COMMAND_READY; return r;
}
SourceFormatCommandResult workbook_apply_source_format_recorded(SourceFormatCommandContext *c,const SourceFormatCommandTarget *t,SourceFormatCommandPlan *p,const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    SourceFormatCommandResult r=check(c,t,p,error); if (r.status!=SOURCE_FORMAT_COMMAND_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); void *ticket=NULL;
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=SOURCE_FORMAT_COMMAND_INVALID; fail(error,"Composite event recording is incomplete."); return r; }
        ObjectSnapshot before={s->objects.items,s->objects.count};
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,.expected_revision=b->command_revision,.revision=b->command_revision+1,
            .view=view(s,c,t,p,&before),.validation_reserved=c->validation_reserved};
        SourceFormatCommandPlan descriptors; memcpy(&descriptors,p,sizeof(descriptors));
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? SOURCE_FORMAT_COMMAND_INVALID : SOURCE_FORMAT_COMMAND_LIMIT; if (ticket) recorder->cancel(recorder->context,ticket); if (!error || !*error) fail(error,"Composite event recording is busy; no formatting was applied."); return r; }
        if (memcmp(&descriptors,p,sizeof(descriptors))) { r.status=SOURCE_FORMAT_COMMAND_STALE; fail(error,"The source formatting plan changed during reservation."); recorder->cancel(recorder->context,ticket); return r; }
        r=check(c,t,p,error); if (r.status!=SOURCE_FORMAT_COMMAND_READY) { recorder->cancel(recorder->context,ticket); return r; }
        s=resolve(b,t->owner.sheet_id);
    }
    source_versions_free(&s->source_versions); s->source_versions=p->sources; p->sources=(SourceVersions){0};
    if (t->object) objects_adopt_source_version(&s->objects,&p->objects); else worksheet_analysis_adopt(s,c->primary,&p->primary);
    format_commit(&s->formats,&p->formats);
    worksheet_record(s,t->object ? EDIT_OBJECTS_FORMAT : EDIT_ANALYSIS_FORMAT); b->command_revision++; b->dirty=true; workbook_history_note_edit(b);
    r.status=SOURCE_FORMAT_COMMAND_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
}
void source_format_command_plan_free(SourceFormatCommandPlan *p) {
    if (p) { source_versions_free(&p->sources); analysis_free(&p->primary); objects_snapshot_free(&p->objects); format_snapshot_free(&p->formats); }
}
static SourceFormatCommandResult bound_check(SourceFormatCommandContext *c,const SourceFormatCommandTarget *t,FormatSnapshot *p,char **error) {
    if (!p) { fail(error,"Prepare an independent source format plan."); return (SourceFormatCommandResult){.status=SOURCE_FORMAT_COMMAND_INVALID}; }
    SourceFormatCommandResult r=workbook_check_source_format_owner(c,t,error); if (r.status!=SOURCE_FORMAT_COMMAND_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); const Analysis *a=owner_source(s,c->primary,t->object);
    r.status=SOURCE_FORMAT_COMMAND_PROTECTED;
    if (!history_safe(&s->formats)) { fail(error,"Legacy finite formatting requires its verified baseline before source formatting."); return r; }
    if (!a->source_version) { fail(error,"Source formatting needs its complete first-binding owner."); return r; }
    r.status=SOURCE_FORMAT_COMMAND_INVALID;
    SourceFormatCommandPlan candidate={.formats=*p}; size_t validation=0;
    if (!independent_format(&s->formats,p,error)) return r;
    if (!workbook_source_format_scratch(s,c->primary,&candidate,&validation,error)) return r;
    if (validation>c->validation_reserved) { r.status=SOURCE_FORMAT_COMMAND_LIMIT; fail(error,"Source format validation exceeds its admitted scratch budget."); return r; }
    if (!source_versions_valid(&s->source_versions,s->objects.next_id,error)) return r;
    const SourceVersion *entry=source_versions_find(&s->source_versions,a->source_version);
    if (!entry || entry->owner!=t->object || entry->kind!=a->kind || !a->path || strcmp(entry->path,a->path) || !entry->raw.bytes) {
        fail(error,"The current source binding has no complete immutable metadata."); return r;
    }
    FormatSnapshotView before=format_delta_store_view(&s->formats),after=format_delta_snapshot_view(p); size_t bytes=0;
    FormatDeltaStatus delta=format_delta_encoded_size(s->id,before,after,&bytes);
    FormatDependencySelection deps={0};
    if ((delta!=FORMAT_DELTA_OK && delta!=FORMAT_DELTA_NOOP) ||
        format_dependencies_select(s->id,before,after,&s->source_versions,s->objects.next_id,&deps)!=FORMAT_DEPENDENCIES_OK || !deps.rows[(size_t)(entry-s->source_versions.items)]) {
        fail(error,"The source format delta and immutable dependencies are incomplete."); return r;
    }
    if (delta==FORMAT_DELTA_NOOP) { r.status=SOURCE_FORMAT_COMMAND_NOOP; return r; }
    r.status=SOURCE_FORMAT_COMMAND_LIMIT;
    if (b->command_revision==UINT64_MAX || s->formats.revision==UINT64_MAX || !worksheet_history_order_available()) { fail(error,"Source format revision space is exhausted."); return r; }
    r.status=SOURCE_FORMAT_COMMAND_INVALID;
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID) { fail(error,"Source formatting history is incomplete."); return r; }
    for (size_t i=0;i<s->formats.undo_count+s->formats.redo_count;i++) {
        const FormatSnapshot *state=i<s->formats.undo_count ? s->formats.undo+i : s->formats.redo+i-s->formats.undo_count;
        FormatStore retained={.items=state->items,.count=state->count,.scopes=state->scopes,.scope_count=state->scope_count,.source_aware=state->source_aware};
        if (!format_valid(&retained,error)) return r;
    }
    FormatStore projected=s->formats; projected.items=p->items; projected.count=p->count; projected.scopes=p->scopes; projected.scope_count=p->scope_count; projected.source_aware=p->source_aware; projected.redo_count=0;
    if (projected.undo_count==DX_FORMAT_HISTORY) { memmove(projected.undo,projected.undo+1,(DX_FORMAT_HISTORY-1)*sizeof(*projected.undo)); projected.undo_count--; }
    projected.undo[projected.undo_count++]=(FormatSnapshot){s->formats.items,s->formats.count,s->formats.scopes,s->formats.scope_count,s->formats.source_aware};
    if (!format_valid(&projected,error)) { r.status=SOURCE_FORMAT_COMMAND_LIMIT; return r; }
    r.status=SOURCE_FORMAT_COMMAND_READY; return r;
}
SourceFormatCommandResult workbook_apply_bound_source_format_recorded(SourceFormatCommandContext *c,const SourceFormatCommandTarget *t,FormatSnapshot *p,const WorkbookHistoryFormatRecorder *recorder,char **error) {
    SourceFormatCommandResult r=bound_check(c,t,p,error);
    if (r.status==SOURCE_FORMAT_COMMAND_NOOP) { format_snapshot_free(p); return r; }
    if (r.status!=SOURCE_FORMAT_COMMAND_READY) return r;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); void *ticket=NULL;
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=SOURCE_FORMAT_COMMAND_INVALID; fail(error,"Source format recording is incomplete."); return r; }
        WorkbookHistoryFormatRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,.sheet=s->id,.expected_revision=b->command_revision,.revision=b->command_revision+1,.effects=8,
            .before=format_delta_store_view(&s->formats),.after=format_delta_snapshot_view(p),.sources=&s->source_versions,.object_high_water=s->objects.next_id};
        FormatSnapshot descriptors; memcpy(&descriptors,p,sizeof(descriptors));
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? SOURCE_FORMAT_COMMAND_INVALID : SOURCE_FORMAT_COMMAND_LIMIT; if (ticket) recorder->cancel(recorder->context,ticket); if (!error || !*error) fail(error,"Source format recording is busy; the plan is kept."); return r; }
        if (memcmp(&descriptors,p,sizeof(descriptors))) { r.status=SOURCE_FORMAT_COMMAND_STALE; fail(error,"The source format plan changed during reservation."); recorder->cancel(recorder->context,ticket); return r; }
        r=bound_check(c,t,p,error);
        if (r.status!=SOURCE_FORMAT_COMMAND_READY) { recorder->cancel(recorder->context,ticket); if (r.status==SOURCE_FORMAT_COMMAND_NOOP) format_snapshot_free(p); return r; }
        s=resolve(b,t->owner.sheet_id);
    }
    format_commit(&s->formats,p); worksheet_record(s,EDIT_FORMAT); b->command_revision++; b->dirty=true; workbook_history_note_edit(b);
    r.status=SOURCE_FORMAT_COMMAND_APPLIED; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
}
