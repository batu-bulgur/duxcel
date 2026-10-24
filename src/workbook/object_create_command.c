#include "object_create_command.h"
#include "definition_baseline.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
typedef char CreateTargetStackBound[(sizeof(ObjectCreateTarget)<1024u*1024u) ? 1 : -1];
static bool manual_shape(const FormatStore *f) {
    return f && !f->source_aware && !f->scope_count && f->count<=DX_FORMAT_PATCHES && (!f->count || f->items) &&
        f->undo_count<=DX_FORMAT_HISTORY && f->redo_count<=DX_FORMAT_HISTORY && format_owned_bytes(f,false)<=DX_FORMAT_CURRENT_BYTES;
}
static bool context_valid(const ObjectCreateContext *c) { return c && c->primary && c->primary_epoch && c->source_rows && c->source_columns && c->source_count_pending; }
bool workbook_prepare_object_create(ObjectCreateContext *c,size_t sheet,ObjectCreateTarget *out,char **error) {
    if (!context_valid(c) || !out) return fail(error,"Choose a valid object creation owner.");
    ObjectCreateTarget next={0};
    if (!workbook_prepare_object_creation_fence(&c->command,sheet,&next.owner,error)) return false;
    next.authority=c->primary==&c->command.book->sheets[sheet].analysis ? NULL : c->primary;
    memcpy(&next.primary,c->primary,sizeof(next.primary)); next.primary_epoch=*c->primary_epoch;
    next.source_rows=*c->source_rows; next.source_columns=*c->source_columns; next.source_count_pending=*c->source_count_pending;
    next.manual_formats=c->manual_formats;
    if (c->manual_formats) {
        const FormatStore *f=&c->command.book->sheets[sheet].formats;
        if (!manual_shape(f)) return fail(error,"Choose bounded legacy manual formats before the first Table import.");
        memcpy(&next.formats,f,sizeof(*f)); if (f->count) memcpy(next.patches,f->items,f->count*sizeof(*f->items));
    }
    *out=next; return true;
}
static ObjectCreateStatus project(const ObjectStore *s,ObjectCreateCommand command,const Analysis *primary,const CellStore *cells,
    uint64_t source_rows,size_t source_columns,SheetObject *items,ObjectCreatePlanInfo *out,char **error) {
    if (!s || !out || !primary || !cells || s->count>DX_MAX_OBJECTS || (s->count && !s->items) || (unsigned)command.kind>OBJECT_TABLE ||
        !command.name || !*command.name || strlen(command.name)>DX_OBJECT_NAME_BYTES ||
        (command.kind==OBJECT_TABLE ? (!command.source || command.pivot || command.source_id) : command.source!=NULL)) {
        fail(error,"Invalid object creation proposal."); return OBJECT_CREATE_INVALID;
    }
    if (s->count==DX_MAX_OBJECTS || s->next_id==UINT64_MAX) { fail(error,"This worksheet has exhausted its object count or identities."); return OBJECT_CREATE_LIMIT; }
    ObjectSnapshot before={s->items,s->count};
    if (definition_baseline_status((DefinitionDependencyView){primary,&before})!=DEFINITION_BASELINE_READY) {
        fail(error,"Resolve the current definition origin before creating an object."); return OBJECT_CREATE_PROTECTED;
    }
    ObjectsAssetInfo b={0},a={0}; AnalysisAssetMeasure p={0},proposal={0};
    if (objects_asset_measure(1,&before,s->next_id,&b)!=OBJECTS_ASSET_OK || analysis_asset_measure(1,0,primary,s->next_id,&p)!=ANALYSIS_ASSET_OK) {
        fail(error,"Invalid bounded object or primary definitions."); return OBJECT_CREATE_INVALID;
    }
    const Analysis *chosen=command.kind==OBJECT_TABLE ? command.source : primary;
    if (command.source_id) {
        size_t i=0; while (i<s->count && s->items[i].id!=command.source_id) i++;
        if (i==s->count || s->items[i].kind!=OBJECT_TABLE) { fail(error,"The source Table is missing."); return OBJECT_CREATE_INVALID; }
        chosen=&s->items[i].source;
    }
    if (chosen->kind==SOURCE_NONE || definition_baseline_status((DefinitionDependencyView){chosen,NULL})!=DEFINITION_BASELINE_READY) {
        fail(error,"Choose a source with a resolved relation before creating this object."); return OBJECT_CREATE_PROTECTED;
    }
    uint64_t rows=command.kind==OBJECT_CHART ? 12 : 5; size_t columns=command.kind==OBJECT_CHART ? 6 : 4;
    if (!objects_fits(s,-1,command.row,command.column,rows,columns,cells,source_rows,source_columns,error)) return OBJECT_CREATE_INVALID;
    if (s->count) memcpy(items,s->items,s->count*sizeof(*items));
    items[s->count]=(SheetObject){.id=s->next_id+1,.row=command.row,.rows=rows,.col=command.column,.columns=columns,
        .revision=1,.name=(char *)command.name,.kind=command.kind,.source_id=command.source_id};
    if (command.pivot) items[s->count].pivot=*command.pivot;
    if (command.source) items[s->count].source=*command.source;
    ObjectSnapshot after={items,s->count+1};
    if (objects_asset_measure(1,&after,s->next_id+1,&a)!=OBJECTS_ASSET_OK) { fail(error,"Invalid complete object creation definitions."); return OBJECT_CREATE_INVALID; }
    size_t staged=strlen(command.name)+1;
    if (command.source) {
        if (analysis_asset_measure(1,s->next_id+1,command.source,s->next_id+1,&proposal)!=ANALYSIS_ASSET_OK) return OBJECT_CREATE_INVALID;
        staged+=proposal.owned;
    }
    if (command.pivot) { PivotAssetMeasure v={0}; if (pivot_asset_measure(1,s->next_id+1,command.pivot,s->next_id+1,&v)!=PIVOT_ASSET_OK) return OBJECT_CREATE_INVALID; staged+=v.owned; }
    size_t scratch=a.measure.validation_scratch>b.measure.validation_scratch ? a.measure.validation_scratch : b.measure.validation_scratch;
    *out=(ObjectCreatePlanInfo){a.measure.owned,scratch>p.validation_scratch ? scratch : p.validation_scratch,p.owned,staged}; return OBJECT_CREATE_READY;
}
ObjectCreateStatus objects_create_measure(const ObjectStore *s,ObjectCreateCommand command,const Analysis *primary,const CellStore *cells,
    uint64_t source_rows,size_t source_columns,ObjectCreatePlanInfo *out,char **error) {
    if (!out) return OBJECT_CREATE_INVALID;
    SheetObject items[DX_MAX_OBJECTS]; ObjectCreatePlanInfo info={0};
    ObjectCreateStatus status=project(s,command,primary,cells,source_rows,source_columns,items,&info,error); if (status==OBJECT_CREATE_READY) *out=info; return status;
}
ObjectCreateStatus workbook_object_create_measure(const ObjectCreateContext *c,size_t sheet,ObjectCreateCommand command,ObjectCreatePlanInfo *out,char **error) {
    if (!context_valid(c) || !c->command.book || sheet>=c->command.book->count || !out) { fail(error,"Invalid creation measurement owner."); return OBJECT_CREATE_INVALID; }
    const Worksheet *s=c->command.book->sheets+sheet; ObjectCreatePlanInfo info={0};
    ObjectCreateStatus status=objects_create_measure(&s->objects,command,c->primary,&s->cells,*c->source_count_pending ? UINT64_MAX : *c->source_rows,*c->source_columns,&info,error);
    if (status!=OBJECT_CREATE_READY) return status;
    if (c->manual_formats) {
        if (command.kind!=OBJECT_TABLE || !manual_shape(&s->formats)) { fail(error,"Manual format migration requires a first Table import."); return OBJECT_CREATE_INVALID; }
        size_t bytes=format_owned_bytes(&s->formats,false);
        if (bytes>SIZE_MAX-info.owned) { fail(error,"Creation format reservation overflow."); return OBJECT_CREATE_LIMIT; }
        info.owned+=bytes;
        if (info.validation_scratch<DX_FORMAT_VALIDATION_BYTES) info.validation_scratch=DX_FORMAT_VALIDATION_BYTES;
    }
    *out=info; return status;
}
static ObjectCreateResult check(ObjectCreateContext *c,const ObjectCreateTarget *t,char **error) {
    if (!context_valid(c) || !t) { fail(error,"Invalid creation context."); return (ObjectCreateResult){.status=OBJECT_CREATE_INVALID}; }
    ObjectFenceResult f=workbook_check_object_creation_fence(&c->command,&t->owner,error);
    ObjectCreateResult r={.status=f.status==OBJECT_FENCE_READY ? OBJECT_CREATE_READY : f.status==OBJECT_FENCE_STALE ? OBJECT_CREATE_STALE :
        f.status==OBJECT_FENCE_PROTECTED ? OBJECT_CREATE_PROTECTED : OBJECT_CREATE_INVALID,.before_revision=f.revision,.after_revision=f.revision};
    Worksheet *s=r.status==OBJECT_CREATE_READY ? resolve(c->command.book,t->owner.owner.sheet_id) : NULL;
    if (s && (c->manual_formats!=t->manual_formats || c->primary!=(t->authority ? t->authority : &s->analysis) || *c->primary_epoch!=t->primary_epoch || memcmp(c->primary,&t->primary,sizeof(t->primary)) ||
        *c->source_rows!=t->source_rows || *c->source_columns!=t->source_columns || *c->source_count_pending!=t->source_count_pending)) {
        r.status=OBJECT_CREATE_STALE; fail(error,"The primary source or extent changed; prepare creation again.");
    }
    if (r.status==OBJECT_CREATE_READY && c->manual_formats && (!manual_shape(&s->formats) || memcmp(&s->formats,&t->formats,sizeof(t->formats)) ||
        (s->formats.count && memcmp(s->formats.items,t->patches,s->formats.count*sizeof(*t->patches))))) {
        r.status=OBJECT_CREATE_STALE; fail(error,"The manual format owner changed; prepare creation again.");
    }
    return r;
}
ObjectCreateResult workbook_apply_object_create_recorded(ObjectCreateContext *c,const ObjectCreateTarget *t,ObjectCreateCommand command,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error) {
    ObjectCreateResult r=check(c,t,error); if (r.status!=OBJECT_CREATE_READY) return r;
    Workbook *b=c->command.book; Worksheet *s=resolve(b,t->owner.owner.sheet_id);
    SheetObject items[DX_MAX_OBJECTS]; ObjectCreatePlanInfo info={0};
    r.status=project(&s->objects,command,c->primary,&s->cells,*c->source_count_pending ? UINT64_MAX : *c->source_rows,*c->source_columns,items,&info,error);
    if (r.status!=OBJECT_CREATE_READY) return r;
    if (r.status==OBJECT_CREATE_READY && c->manual_formats) {
        r.status=workbook_object_create_measure(c,(size_t)(s-b->sheets),command,&info,error); if (r.status!=OBJECT_CREATE_READY) return r;
    }
    if (b->command_revision==UINT64_MAX || !worksheet_history_order_available() || info.owned>c->command.plan_reserved || info.validation_scratch>c->command.plan_reserved-info.owned) {
        r.status=OBJECT_CREATE_LIMIT; fail(error,"Creation revision space or admitted planning budget is exhausted."); return r;
    }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID || !source_versions_valid(&s->source_versions,s->objects.next_id,error)) {
        r.status=OBJECT_CREATE_INVALID; if (!error || !*error) fail(error,"Object history or source ownership is incomplete."); return r;
    }
    if (c->manual_formats && (s->formats.revision==UINT64_MAX || !worksheet_manual_format_history_eligible(s,c->primary,error))) {
        r.status=s->formats.revision==UINT64_MAX ? OBJECT_CREATE_LIMIT : OBJECT_CREATE_PROTECTED;
        if (!error || !*error) fail(error,"The complete pre-import manual format history is unavailable."); return r;
    }
    DefinitionDependencySelection root={0};
    if (!analysis_query_valid(c->primary,error) || !analysis_filters_valid(c->primary,error) || !analysis_sorts_valid(c->primary,error) ||
        definition_dependencies_select(s->id,(DefinitionDependencyView){c->primary,NULL},(DefinitionDependencyView){c->primary,NULL},&s->source_versions,s->objects.next_id,&root)!=DEFINITION_DEPENDENCIES_OK) {
        r.status=OBJECT_CREATE_INVALID; if (!error || !*error) fail(error,"The unchanged primary source dependencies are inconsistent."); return r;
    }
    uint64_t high=s->objects.next_id+1;
    ObjectStore candidate={.items=items,.count=s->objects.count+1,.next_id=high},copy={0};
    if (!objects_valid(&s->objects,error) || !objects_valid(&candidate,error)) { r.status=OBJECT_CREATE_INVALID; return r; }
    ObjectSnapshot before={s->objects.items,s->objects.count},projected={items,candidate.count}; DefinitionDependencySelection dependencies={0}; void *ticket=NULL;
    if (definition_dependencies_select(s->id,(DefinitionDependencyView){NULL,&before},(DefinitionDependencyView){NULL,&projected},&s->source_versions,high,&dependencies)!=DEFINITION_DEPENDENCIES_OK) {
        r.status=OBJECT_CREATE_INVALID; fail(error,"The complete object source dependencies are inconsistent."); return r;
    }
    objects_copy(&copy,&candidate); ObjectSnapshot after={copy.items,copy.count}; copy.items=NULL; copy.count=0; objects_free(&copy);
    FormatSnapshot formats={0};
    DefinitionEventView view={.sheet=s->id,.object_high_water=high,.effects=c->manual_formats ? 12 : 4,
        .before={NULL,&before},.after={NULL,&after},.sources=&s->source_versions};
    if (c->manual_formats) {
        FormatStore copied={0}; format_copy(&copied,&s->formats);
        formats=(FormatSnapshot){copied.items,copied.count,copied.scopes,copied.scope_count,true}; copied.items=NULL; copied.scopes=NULL; copied.count=copied.scope_count=0; format_free(&copied);
        view.before_formats=format_delta_store_view(&s->formats); view.after_formats=format_delta_snapshot_view(&formats);
        DefinitionEventInfo event={0}; DefinitionEventStatus status=definition_event_measure(view,c->command.validation_reserved,&event);
        if (status!=DEFINITION_EVENT_OK) { r.status=status==DEFINITION_EVENT_LIMIT ? OBJECT_CREATE_LIMIT : OBJECT_CREATE_INVALID; fail(error,"The complete creation and manual format event is invalid."); goto cancelled; }
    }
    if (c->admit) {
        FormatStore prospective=s->formats;
        if (c->manual_formats) {
            prospective.items=formats.items; prospective.count=formats.count; prospective.scopes=formats.scopes; prospective.scope_count=formats.scope_count; prospective.source_aware=true; prospective.redo_count=0;
            if (prospective.undo_count==DX_FORMAT_HISTORY) { memmove(prospective.undo,prospective.undo+1,(DX_FORMAT_HISTORY-1)*sizeof(*prospective.undo)); prospective.undo_count--; }
            prospective.undo[prospective.undo_count++]=(FormatSnapshot){s->formats.items,s->formats.count,s->formats.scopes,s->formats.scope_count,s->formats.source_aware}; prospective.revision++;
        }
        if (!c->admit(c->admission_context,&prospective,info.owned,info.validation_scratch,c->manual_formats ? objects_snapshot_owned_bytes(&before) : 0,error)) {
            r.status=OBJECT_CREATE_LIMIT; if (!error || !*error) fail(error,"Creation memory admission was rejected."); goto cancelled;
        }
        r=check(c,t,error); if (r.status!=OBJECT_CREATE_READY) goto cancelled; s=resolve(b,t->owner.owner.sheet_id);
    }
    view.sources=&s->source_versions;
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=OBJECT_CREATE_INVALID; fail(error,"Object event recording is incomplete."); goto cancelled; }
        WorkbookHistoryDefinitionRecord record={.workbook=b->id,.edit_session=b->edit_session_id,.sheet_session=s->edit_session_id,
            .expected_revision=b->command_revision,.revision=b->command_revision+1,.validation_reserved=c->command.validation_reserved,
            .view=view};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? OBJECT_CREATE_INVALID : OBJECT_CREATE_LIMIT; if (!error || !*error) fail(error,"Object event recording is busy; no change was applied."); goto cancelled; }
        r=check(c,t,error); if (r.status!=OBJECT_CREATE_READY) goto cancelled;
        s=resolve(b,t->owner.owner.sheet_id);
    }
    if (c->manual_formats) format_commit(&s->formats,&formats);
    objects_adopt_creation(&s->objects,&after); worksheet_record(s,c->manual_formats ? EDIT_OBJECTS_FORMAT : EDIT_OBJECTS);
    b->command_revision++; b->dirty=true; workbook_history_note_edit(b); r.status=OBJECT_CREATE_APPLIED; r.object=high; r.after_revision=b->command_revision;
    if (recorder) recorder->publish(recorder->context,ticket);
    return r;
cancelled:
    if (ticket) recorder->cancel(recorder->context,ticket);
    objects_snapshot_free(&after); format_snapshot_free(&formats); return r;
}
