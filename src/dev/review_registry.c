#include "review_registry.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t codepoint(const unsigned char *p,size_t available) {
    if (!available) return 0;
    if (*p<0x80) return 1;
    size_t n=*p>=0xc2 && *p<=0xdf ? 2 : *p>=0xe0 && *p<=0xef ? 3 : *p>=0xf0 && *p<=0xf4 ? 4 : 0;
    if (!n || n>available) return 0;
    for (size_t i=1;i<n;i++) if ((p[i]&0xc0)!=0x80) return 0;
    if ((*p==0xe0 && p[1]<0xa0) || (*p==0xed && p[1]>=0xa0) || (*p==0xf0 && p[1]<0x90) || (*p==0xf4 && p[1]>=0x90)) return 0;
    return n;
}
/* Copy at most source_capacity input bytes. Caller strings may use SIZE_MAX;
   fixed records always provide their actual capacity. No unbounded strlen. */
static bool owned(char *out,size_t capacity,const char *input,size_t source_capacity) {
    if (!input) { *out=0; return false; }
    size_t i=0,n=0; bool clipped=false;
    while (i<source_capacity && input[i]) {
        size_t available=source_capacity-i,k=0;
        while (k<4 && k<available && input[i+k]) k++;
        size_t count=codepoint((const unsigned char *)input+i,k);
        const char *part=count ? input+i : "�"; size_t bytes=count ? count : 3;
        if (n+bytes>=capacity) { clipped=true; break; }
        memcpy(out+n,part,bytes); n+=bytes; i+=count ? count : 1;
    }
    if (i==source_capacity) clipped=true;
    out[n]=0;
    if (clipped && capacity>=4) {
        while (n>capacity-4) { n--; while (n && ((unsigned char)out[n]&0xc0)==0x80) n--; }
        memcpy(out+n,"…",4);
    }
    return clipped;
}
static bool semantic_text(const char *text,size_t capacity) {
    if (!text) return true;
    size_t i=0;
    while (i<capacity && text[i]) {
        if ((unsigned char)text[i]<32 || text[i]==127) return false;
        size_t available=0; while (available<4 && available<capacity-i && text[i+available]) available++;
        size_t n=codepoint((const unsigned char *)text+i,available); if (!n) return false; i+=n;
    }
    return i<capacity;
}
static bool identifier(const char *text,size_t capacity) {
    size_t n=0;
    for (;n<capacity && text[n];n++) {
        unsigned char c=(unsigned char)text[n];
        if (!((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-')) return false;
    }
    return n>0 && n<capacity && n<=95;
}
static bool rectangle(DxReviewRect r) {
    return isfinite(r.x) && isfinite(r.y) && isfinite(r.width) && isfinite(r.height) && r.width>=0 && r.height>=0 && isfinite(r.x+r.width) && isfinite(r.y+r.height);
}
static DxReviewRect intersect(DxReviewRect a,DxReviewRect b) {
    float left=fmaxf(a.x,b.x),top=fmaxf(a.y,b.y),right=fminf(a.x+a.width,b.x+b.width),bottom=fminf(a.y+a.height,b.y+b.height);
    return (DxReviewRect){left,top,fmaxf(0,right-left),fmaxf(0,bottom-top)};
}
static DxReviewRect visible(const DxReviewRegistry *r,const DxReviewWidget *w) {
    return intersect(intersect(w->bounds,w->clip),(DxReviewRect){0,0,(float)r->frame.window_width,(float)r->frame.window_height});
}
static bool contains(DxReviewRect r,float x,float y) { return r.width>0 && r.height>0 && x>=r.x && x<r.x+r.width && y>=r.y && y<r.y+r.height; }
static bool transform(DxReviewTransform t) { return isfinite(t.a) && isfinite(t.b) && isfinite(t.c) && isfinite(t.d) && isfinite(t.tx) && isfinite(t.ty); }
static void increment(size_t *count) { if (*count<SIZE_MAX) (*count)++; }
#define COPY(record,field) next.truncated|=owned(next.field,sizeof(next.field),(record)->field,sizeof((record)->field))
DxReviewStatus dx_review_begin(DxReviewRegistry *r,const DxReviewFrame *frame,bool armed) {
    if (!r) return DX_REVIEW_INVALID;
    r->armed=false; r->sealed=false; r->count=r->duplicate_count=r->overflow_count=r->invalid_count=0; r->truncated=false;
    if (!armed) return DX_REVIEW_NOT_ARMED;
    if (!frame || !frame->frame_id || !identifier(frame->session_id,sizeof(frame->session_id)) || !identifier(frame->project_id,sizeof(frame->project_id)) ||
        !frame->window_width || !frame->window_height || !frame->framebuffer_width || !frame->framebuffer_height ||
        !isfinite(frame->dpi_x) || frame->dpi_x<=0 || !isfinite(frame->dpi_y) || frame->dpi_y<=0 ||
        !isfinite(frame->ui_scale) || frame->ui_scale<=0 || !isfinite(frame->grid_zoom) || frame->grid_zoom<=0) return DX_REVIEW_INVALID;
    DxReviewFrame next=*frame;
    COPY(frame,session_id); COPY(frame,project_id); COPY(frame,document_id); COPY(frame,source_fingerprint); COPY(frame,binary_sha256);
    COPY(frame,build_flags); COPY(frame,assets_fingerprint); COPY(frame,fonts); COPY(frame,state_text); COPY(frame,trace_text);
    r->frame=next; r->truncated=next.truncated; r->armed=true; return DX_REVIEW_OK;
}
void dx_review_widget_init(DxReviewWidget *w,const char *id,const char *action_id,const char *path,const char *role,const char *label) {
    if (!w) return;
    DxReviewWidget next={0}; next.enabled=next.input_eligible=next.pickable=true; next.transform=(DxReviewTransform){1,0,0,1,0,0};
    next.identity_truncated=!semantic_text(id,sizeof(next.id)) || !semantic_text(action_id,sizeof(next.action_id)) || !semantic_text(path,sizeof(next.path));
    next.identity_truncated|=owned(next.id,sizeof(next.id),id,SIZE_MAX);
    next.identity_truncated|=owned(next.action_id,sizeof(next.action_id),action_id,SIZE_MAX);
    next.identity_truncated|=owned(next.path,sizeof(next.path),path,SIZE_MAX);
    next.truncated=owned(next.role,sizeof(next.role),role,SIZE_MAX); next.truncated|=owned(next.label,sizeof(next.label),label,SIZE_MAX);
    next.truncated|=next.identity_truncated; *w=next;
}
void dx_review_value_set(DxReviewValue *out,const char *type,bool is_null,const char *raw) {
    if (!out) return;
    DxReviewValue next={0}; next.present=is_null || raw!=NULL; next.is_null=is_null;
    next.truncated=owned(next.type,sizeof(next.type),type,SIZE_MAX);
    if (!is_null) next.truncated|=owned(next.exact_text,sizeof(next.exact_text),raw,SIZE_MAX);
    *out=next;
}
DxReviewStatus dx_review_add(DxReviewRegistry *r,const DxReviewWidget *widget) {
    if (!r || !r->armed) return DX_REVIEW_NOT_ARMED;
    if (r->sealed) return DX_REVIEW_SEALED;
    if (!widget || !rectangle(widget->bounds) || !rectangle(widget->clip) || !transform(widget->transform)) { increment(&r->invalid_count); return DX_REVIEW_INVALID; }
    DxReviewWidget next=*widget;
    next.identity_truncated|=!semantic_text(widget->id,sizeof(widget->id)) || !semantic_text(widget->action_id,sizeof(widget->action_id)) ||
        !semantic_text(widget->path,sizeof(widget->path)) || !semantic_text(widget->parent_id,sizeof(widget->parent_id));
    next.identity_truncated|=owned(next.id,sizeof(next.id),widget->id,sizeof(widget->id));
    next.identity_truncated|=owned(next.action_id,sizeof(next.action_id),widget->action_id,sizeof(widget->action_id));
    next.identity_truncated|=owned(next.path,sizeof(next.path),widget->path,sizeof(widget->path));
    next.identity_truncated|=owned(next.parent_id,sizeof(next.parent_id),widget->parent_id,sizeof(widget->parent_id));
    COPY(widget,role); COPY(widget,label); COPY(widget,source_file);
    next.identity_truncated|=owned(next.data.kind,sizeof(next.data.kind),widget->data.kind,sizeof(widget->data.kind));
    next.identity_truncated|=owned(next.data.document_id,sizeof(next.data.document_id),widget->data.document_id,sizeof(widget->data.document_id));
    next.truncated|=owned(next.data.field,sizeof(next.data.field),widget->data.field,sizeof(widget->data.field));
    bool value_truncated=owned(next.value.type,sizeof(next.value.type),widget->value.type,sizeof(widget->value.type));
    value_truncated|=owned(next.value.exact_text,sizeof(next.value.exact_text),widget->value.exact_text,sizeof(widget->value.exact_text));
    next.value.truncated|=value_truncated; next.truncated|=value_truncated || next.value.truncated;
    if (next.value.is_null) next.value.exact_text[0]=0;
    if (!*next.id || !*next.path || next.identity_truncated || (next.data.present && !*next.data.kind) || !strcmp(next.id,next.parent_id)) { increment(&r->invalid_count); return DX_REVIEW_INVALID; }
    for (size_t i=0;i<r->count;i++) if (!strcmp(r->widgets[i].id,next.id)) { increment(&r->duplicate_count); return DX_REVIEW_DUPLICATE; }
    if (r->count==DX_REVIEW_WIDGETS) { increment(&r->overflow_count); return DX_REVIEW_CAPACITY; }
    r->widgets[r->count++]=next; r->truncated|=next.truncated; return DX_REVIEW_OK;
}
#undef COPY
DxReviewStatus dx_review_seal(DxReviewRegistry *r) {
    if (!r || !r->armed) return DX_REVIEW_NOT_ARMED;
    if (r->sealed) return DX_REVIEW_SEALED;
    r->sealed=true; return DX_REVIEW_OK;
}
const DxReviewWidget *dx_review_find(const DxReviewRegistry *r,const char *id) {
    if (!r || !r->armed || !id) return NULL;
    for (size_t i=0;i<r->count;i++) if (!strcmp(r->widgets[i].id,id)) return &r->widgets[i];
    return NULL;
}
static void target(const DxReviewRegistry *r,const DxReviewWidget *w,DxReviewTarget *out) {
    DxReviewTarget next={0}; next.found=true; next.frame_id=r->frame.frame_id; next.widget=*w; next.bounds=visible(r,w); *out=next;
}
bool dx_review_pick(const DxReviewRegistry *r,float x,float y,DxReviewPick mode,DxReviewTarget *out) {
    if (!out) return false; *out=(DxReviewTarget){0};
    if (!r || !r->armed || !r->sealed || !isfinite(x) || !isfinite(y) || (unsigned)mode>DX_REVIEW_PICK_CONTAINER) return false;
    int32_t barrier=INT32_MIN; const DxReviewWidget *best=NULL;
    for (size_t i=0;i<r->count;i++) { const DxReviewWidget *w=&r->widgets[i]; if (w->pickable && w->container && contains(visible(r,w),x,y) && w->layer>barrier) barrier=w->layer; }
    for (size_t i=0;i<r->count;i++) {
        const DxReviewWidget *w=&r->widgets[i];
        if (!w->pickable || !contains(visible(r,w),x,y) || (mode==DX_REVIEW_PICK_CONTAINER && !w->container) || (mode==DX_REVIEW_PICK_CONTROL && (w->container || w->layer<barrier))) continue;
        if (!best || w->layer>best->layer || (w->layer==best->layer && (mode!=DX_REVIEW_PICK_ANY || !w->container || best->container))) best=w;
    }
    if (!best) return false; target(r,best,out); return true;
}
bool dx_review_parent(const DxReviewRegistry *r,const DxReviewTarget *child,DxReviewTarget *out) {
    if (!out) return false;
    DxReviewTarget input=child ? *child : (DxReviewTarget){0}; *out=(DxReviewTarget){0};
    if (!r || !r->armed || !r->sealed || !input.found || input.region || input.frame_id!=r->frame.frame_id || !*input.widget.parent_id) return false;
    const DxReviewWidget *w=dx_review_find(r,input.widget.parent_id); if (!w) return false; target(r,w,out); return true;
}
bool dx_review_region(const DxReviewRegistry *r,DxReviewRect region,DxReviewTarget *out) {
    if (!out) return false; *out=(DxReviewTarget){0};
    if (!r || !r->armed || !r->sealed || !rectangle(region)) return false;
    region=intersect(region,(DxReviewRect){0,0,(float)r->frame.window_width,(float)r->frame.window_height});
    if (region.width<=0 || region.height<=0) return false;
    out->found=out->region=true; out->frame_id=r->frame.frame_id; out->bounds=region; return true;
}
bool dx_review_target_stale(const DxReviewTarget *t,const DxReviewDataIdentity *now) {
    if (!t || !t->found) return true;
    const DxReviewDataIdentity *old=&t->widget.data;
    if (t->region || !old->present) return false;
    return !now || !now->present || strcmp(old->document_id,now->document_id) || old->sheet_id!=now->sheet_id || old->source_id!=now->source_id || old->object_id!=now->object_id || old->schema_generation!=now->schema_generation || old->query_generation!=now->query_generation;
}

typedef struct { char *buffer; size_t capacity,length; DxReviewStatus status; } Json;
static void emit(Json *j,const char *text,size_t length) {
    if (j->status!=DX_REVIEW_OK) return;
    if (length>DX_REVIEW_JSON_LIMIT-j->length) { j->status=DX_REVIEW_JSON_TOO_LARGE; return; }
    if (j->buffer && (j->length>=j->capacity || length>=j->capacity-j->length)) { j->status=DX_REVIEW_BUFFER_TOO_SMALL; return; }
    if (j->buffer && length) memcpy(j->buffer+j->length,text,length);
    j->length+=length;
}
static void literal(Json *j,const char *text) { emit(j,text,strlen(text)); }
static void string(Json *j,const char *text) {
    literal(j,"\"");
    for (const unsigned char *p=(const unsigned char *)(text ? text : "");*p && j->status==DX_REVIEW_OK;) {
        if (*p=='"' || *p=='\\') { char escaped[]={'\\',(char)*p++,0}; emit(j,escaped,2); }
        else if (*p<32 || *p==127) { char escaped[7]; snprintf(escaped,sizeof(escaped),"\\u%04x",*p++); emit(j,escaped,6); }
        else { size_t n=0; while (n<4 && p[n]) n++; size_t count=codepoint(p,n); if (count) { emit(j,(const char *)p,count); p+=count; } else { literal(j,"�"); p++; } }
    }
    literal(j,"\"");
}
static void key(Json *j,const char *name) { string(j,name); literal(j,":"); }
static void u64(Json *j,uint64_t value) { char raw[32]; snprintf(raw,sizeof(raw),"%" PRIu64,value); string(j,raw); }
static void integer(Json *j,int64_t value) { char raw[32]; int n=snprintf(raw,sizeof(raw),"%" PRId64,value); emit(j,raw,(size_t)n); }
static void number(Json *j,double value) {
    if (!isfinite(value)) { j->status=DX_REVIEW_INVALID; return; }
    char raw[48]; int n=snprintf(raw,sizeof(raw),"%.9g",value);
    if (n<=0 || (size_t)n>=sizeof(raw)) { j->status=DX_REVIEW_INVALID; return; }
    for (int i=0;i<n;i++) if (raw[i]==',') raw[i]='.';
    emit(j,raw,(size_t)n);
}
static void boolean(Json *j,bool value) { literal(j,value ? "true" : "false"); }
static void rect_json(Json *j,DxReviewRect r) {
    literal(j,"{"); key(j,"x"); number(j,r.x); literal(j,","); key(j,"y"); number(j,r.y);
    literal(j,","); key(j,"width"); number(j,r.width); literal(j,","); key(j,"height"); number(j,r.height); literal(j,"}");
}
#define STR(field,name) key(j,name); string(j,(field))
#define BOOL(field,name) key(j,name); boolean(j,(field))
#define U64(field,name) key(j,name); u64(j,(field))
static void data_json(Json *j,const DxReviewDataIdentity *d) {
    if (!d->present) { literal(j,"null"); return; }
    literal(j,"{"); STR(d->kind,"kind"); literal(j,","); STR(d->document_id,"document_id");
    literal(j,","); U64(d->sheet_id,"sheet_id"); literal(j,","); U64(d->source_id,"source_id"); literal(j,","); U64(d->object_id,"object_id");
    literal(j,","); U64(d->schema_generation,"schema_generation"); literal(j,","); U64(d->query_generation,"query_generation");
    literal(j,","); BOOL(d->has_cell,"has_cell"); literal(j,","); U64(d->row,"row"); literal(j,","); U64(d->column,"column"); literal(j,","); STR(d->field,"field"); literal(j,"}");
}
static void value_json(Json *j,const DxReviewValue *v) {
    if (!v->present) { literal(j,"null"); return; }
    literal(j,"{"); STR(v->type,"sql_type"); literal(j,","); BOOL(v->is_null,"is_null"); literal(j,","); key(j,"exact_text");
    if (v->is_null) literal(j,"null"); else string(j,v->exact_text);
    literal(j,","); BOOL(v->truncated,"truncated"); literal(j,"}");
}
static void widget_json(Json *j,const DxReviewRegistry *r,const DxReviewWidget *w) {
    literal(j,"{"); STR(w->id,"id"); literal(j,","); STR(w->action_id,"action_id"); literal(j,","); STR(w->path,"presentation_path");
    literal(j,","); STR(w->parent_id,"parent_id"); literal(j,","); STR(w->role,"role"); literal(j,","); STR(w->label,"label");
    literal(j,","); key(j,"bounds"); rect_json(j,w->bounds); literal(j,","); key(j,"clip"); rect_json(j,w->clip);
    literal(j,","); key(j,"visible_bounds"); rect_json(j,visible(r,w)); literal(j,","); key(j,"transform"); literal(j,"[");
    number(j,w->transform.a); literal(j,","); number(j,w->transform.b); literal(j,","); number(j,w->transform.c); literal(j,","); number(j,w->transform.d); literal(j,","); number(j,w->transform.tx); literal(j,","); number(j,w->transform.ty); literal(j,"]");
    literal(j,","); key(j,"layer"); integer(j,w->layer); literal(j,","); BOOL(w->enabled,"enabled"); literal(j,","); BOOL(w->input_eligible,"input_eligible");
    literal(j,","); BOOL(w->checked,"checked"); literal(j,","); BOOL(w->indeterminate,"indeterminate"); literal(j,","); BOOL(w->expanded,"expanded"); literal(j,","); BOOL(w->focused,"focused");
    literal(j,","); BOOL(w->container,"container"); literal(j,","); BOOL(w->pickable,"pickable"); literal(j,","); BOOL(w->truncated,"truncated");
    literal(j,","); key(j,"source"); literal(j,"{"); STR(w->source_file,"file"); literal(j,","); key(j,"line"); integer(j,w->source_line); literal(j,"}");
    literal(j,","); key(j,"data"); data_json(j,&w->data); literal(j,","); key(j,"value"); value_json(j,&w->value); literal(j,"}");
}
static void target_json(Json *j,const DxReviewRegistry *r,const DxReviewTarget *t) {
    if (!t || !t->found) { literal(j,"null"); return; }
    literal(j,"{"); STR(t->region ? "region" : "widget","kind"); literal(j,","); U64(t->frame_id,"frame_id"); literal(j,","); key(j,"bounds"); rect_json(j,t->bounds);
    literal(j,","); key(j,"widget"); if (t->region) literal(j,"null"); else widget_json(j,r,&t->widget); literal(j,"}");
}
static bool events_json(Json *j,const char *trace) {
    literal(j,"["); size_t count=0; const char *p=trace;
    while (*p && j->status==DX_REVIEW_OK) {
        const char *start=p; while (*p && *p!='\n') p++;
        size_t bytes=(size_t)(p-start); if (*p=='\n') p++;
        if (!bytes) continue;
        if (count==DX_REVIEW_EVENTS) { literal(j,"]"); return true; }
        char line[sizeof(((DxReviewFrame *)0)->trace_text)]; memcpy(line,start,bytes); line[bytes]=0;
        if (count++) literal(j,","); literal(j,"{\"kind\":\"trace\",\"text\":"); string(j,line); literal(j,"}");
    }
    literal(j,"]"); return false;
}
static void frame_json(Json *j,const DxReviewFrame *f) {
    literal(j,"{"); U64(f->frame_id,"frame_id"); literal(j,","); U64(f->timestamp_ns,"timestamp_ns"); literal(j,","); STR(f->project_id,"project_id");
    literal(j,","); STR(f->document_id,"document_id"); literal(j,","); STR(f->source_fingerprint,"source_fingerprint"); literal(j,","); STR(f->binary_sha256,"binary_sha256");
    literal(j,","); STR(f->build_flags,"build_flags"); literal(j,","); STR(f->assets_fingerprint,"assets_fingerprint"); literal(j,","); STR(f->fonts,"fonts");
    literal(j,","); key(j,"window"); literal(j,"{"); key(j,"width"); integer(j,f->window_width); literal(j,","); key(j,"height"); integer(j,f->window_height); literal(j,"}");
    literal(j,","); key(j,"framebuffer"); literal(j,"{"); key(j,"width"); integer(j,f->framebuffer_width); literal(j,","); key(j,"height"); integer(j,f->framebuffer_height); literal(j,"}");
    literal(j,","); key(j,"dpi"); literal(j,"{"); key(j,"x"); number(j,f->dpi_x); literal(j,","); key(j,"y"); number(j,f->dpi_y); literal(j,"}");
    literal(j,","); key(j,"ui_scale"); number(j,f->ui_scale); literal(j,","); key(j,"grid_zoom"); number(j,f->grid_zoom);
    literal(j,","); STR(f->state_text,"state_text"); literal(j,","); BOOL(f->truncated,"truncated"); literal(j,"}");
}
static DxReviewStatus serialize(Json *j,const DxReviewRegistry *r,const DxReviewTarget *t,const char *id,const char *note) {
    if (!r || !r->armed) return DX_REVIEW_NOT_ARMED;
    if (!r->sealed) return DX_REVIEW_NOT_SEALED;
    if (!identifier(id ? id : "",96) || (t && t->found && (t->frame_id!=r->frame.frame_id || !rectangle(t->bounds)))) return DX_REVIEW_INVALID;
    char note_text[DX_REVIEW_NOTE_BYTES]; bool note_truncated=owned(note_text,sizeof(note_text),note,SIZE_MAX);
    literal(j,"{\"schema\":\"duxcel.native-review\",\"version\":1,"); STR(id,"report_id"); literal(j,","); STR(r->frame.session_id,"session_id"); literal(j,","); STR(r->frame.project_id,"project_id");
    literal(j,","); key(j,"frame"); frame_json(j,&r->frame); literal(j,","); key(j,"target"); target_json(j,r,t); literal(j,","); key(j,"widgets"); literal(j,"[");
    for (size_t i=0;i<r->count && j->status==DX_REVIEW_OK;i++) { if (i) literal(j,","); widget_json(j,r,&r->widgets[i]); } literal(j,"]");
    literal(j,","); key(j,"events"); bool events_truncated=events_json(j,r->frame.trace_text);
    literal(j,","); STR(note_text,"note"); literal(j,","); key(j,"diagnostics"); literal(j,"{"); U64(r->count,"widgets"); literal(j,","); U64(r->duplicate_count,"duplicates");
    literal(j,","); U64(r->overflow_count,"overflow"); literal(j,","); U64(r->invalid_count,"invalid"); literal(j,","); BOOL(r->truncated,"truncated");
    literal(j,","); BOOL(events_truncated,"events_truncated"); literal(j,","); BOOL(note_truncated,"note_truncated"); literal(j,"}}");
    if (j->buffer && j->status==DX_REVIEW_OK) j->buffer[j->length]=0;
    return j->status;
}
#undef STR
#undef BOOL
#undef U64
DxReviewStatus dx_review_json(const DxReviewRegistry *r,const DxReviewTarget *t,const char *id,const char *note,char *buffer,size_t capacity,size_t *required) {
    if (required) *required=0; if (buffer && capacity) *buffer=0;
    Json count={0}; DxReviewStatus status=serialize(&count,r,t,id,note); if (status!=DX_REVIEW_OK) return status;
    if (required) *required=count.length+1;
    if (!buffer || capacity<count.length+1) return DX_REVIEW_BUFFER_TOO_SMALL;
    Json output={buffer,capacity,0,DX_REVIEW_OK}; status=serialize(&output,r,t,id,note);
    if (status!=DX_REVIEW_OK) *buffer=0; return status;
}
char *dx_review_json_alloc(const DxReviewRegistry *r,const DxReviewTarget *t,const char *id,const char *note,size_t *length,DxReviewStatus *result) {
    if (length) *length=0;
    Json count={0}; DxReviewStatus status=serialize(&count,r,t,id,note); char *out=NULL;
    if (status==DX_REVIEW_OK) {
        out=malloc(count.length+1);
        if (!out) status=DX_REVIEW_NO_MEMORY;
        else { Json output={out,count.length+1,0,DX_REVIEW_OK}; status=serialize(&output,r,t,id,note); if (status==DX_REVIEW_OK && length) *length=output.length; }
    }
    if (status!=DX_REVIEW_OK) { free(out); out=NULL; }
    if (result) *result=status; return out;
}
const char *dx_review_status_text(DxReviewStatus status) {
    static const char *const names[]={"ok","not armed","sealed","invalid record","duplicate widget ID","widget capacity reached","not sealed","JSON exceeds 4 MiB","buffer too small","out of memory"};
    return (unsigned)status<sizeof(names)/sizeof(*names) ? names[status] : "unknown review status";
}
