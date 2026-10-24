#include "format.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *text) { if (error) { free(*error); *error=dx_strdup(text); } return false; }
CellFormat format_default(void) { return (CellFormat){.size=13,.decimals=2}; }
static void merge(CellFormat *to,CellFormat value,unsigned mask) {
    to->flags=(unsigned char)((to->flags&~(mask&7))|(value.flags&(mask&7)));
    if (mask&FORMAT_ALIGN) to->align=value.align;
    if (mask&FORMAT_COLOR) to->color=value.color;
    if (mask&FORMAT_SIZE) to->size=value.size;
    if (mask&FORMAT_NUMERIC) to->number=value.number;
    if (mask&FORMAT_DECIMALS) to->decimals=value.decimals;
}
static bool same_value(CellFormat a,CellFormat b,unsigned mask) {
    CellFormat x=format_default(),y=x; merge(&x,a,mask); merge(&y,b,mask);
    return x.flags==y.flags && x.align==y.align && x.color==y.color && x.size==y.size && x.number==y.number && x.decimals==y.decimals;
}
static bool range_valid(FormatRange r) { return r.r0<=r.r1 && r.r1<DX_SHEET_ROWS && r.c0<=r.c1 && r.c1<16384; }
static bool overlap(FormatRange a,FormatRange b) { return a.r0<=b.r1 && b.r0<=a.r1 && a.c0<=b.c1 && b.c0<=a.c1; }
static bool contains(FormatRange a,FormatRange b) { return a.r0<=b.r0 && a.r1>=b.r1 && a.c0<=b.c0 && a.c1>=b.c1; }
static bool same_range(FormatRange a,FormatRange b) { return a.r0==b.r0 && a.r1==b.r1 && a.c0==b.c0 && a.c1==b.c1; }
static bool value_valid(CellFormat v) { return v.flags<=7 && v.align<=FORMAT_RIGHT && v.color<=3 && v.size>=11 && v.size<=16 && v.number<=FORMAT_NUMBER && v.decimals<=4; }
static bool whole_columns(FormatRange r) { return !r.r0 && r.r1==DX_SHEET_ROWS-1; }
static bool scope_same(FormatScope a,FormatScope b) { return a.owner==b.owner && a.bytes==b.bytes && !memcmp(a.definition,b.definition,a.bytes); }
static int scope_compare(const FormatScope *a,const FormatScope *b) {
    if (a->owner!=b->owner) return a->owner<b->owner ? -1 : 1;
    if (a->bytes!=b->bytes) return a->bytes<b->bytes ? -1 : 1;
    return memcmp(a->definition,b->definition,a->bytes);
}
static void scope_sift(const FormatScope **heap,size_t root,size_t count) {
    while (root<count/2) {
        size_t child=2*root+1;
        if (child+1<count && scope_compare(heap[child],heap[child+1])<0) child++;
        if (scope_compare(heap[root],heap[child])>=0) break;
        const FormatScope *swap=heap[root]; heap[root]=heap[child]; heap[child]=swap; root=child;
    }
}
/* Shapes have been checked before comparisons. Keep normal owner tables
   allocation-free; larger tables use one bounded pointer array and heapsort,
   with deterministic O(n log n) comparisons and exact binary equality. */
static bool scopes_unique(const FormatScope *scopes,size_t count) {
    if (count<=DX_FORMAT_GUARDS) {
        for (size_t i=0;i<count;i++) for (size_t j=0;j<i;j++) if (scope_same(scopes[i],scopes[j])) return false;
        return true;
    }
    const FormatScope **ordered=dx_alloc(count*sizeof(*ordered));
    for (size_t i=0;i<count;i++) ordered[i]=&scopes[i];
    for (size_t start=count/2;start;start--) scope_sift(ordered,start-1,count);
    for (size_t end=count;end>1;) {
        const FormatScope *swap=ordered[0]; ordered[0]=ordered[--end]; ordered[end]=swap; scope_sift(ordered,0,end);
    }
    bool unique=true;
    for (size_t i=1;i<count;i++) if (!scope_compare(ordered[i-1],ordered[i])) { unique=false; break; }
    free(ordered); return unique;
}
size_t format_scope_token(const FormatStore *s,const FormatScopeView *scope) {
    if (!scope) return 0;
    if (!scope->definition || !scope->bytes || scope->bytes>=DX_FORMAT_SCOPE_BYTES) return SIZE_MAX;
    for (size_t i=0;i<s->scope_count;i++) if (scope_same(s->scopes[i],(FormatScope){scope->owner,(void *)scope->definition,scope->bytes})) return i+1;
    return SIZE_MAX;
}
static bool eligible(const FormatPatch *p,size_t token) {
    if (p->selector==FORMAT_RECORDS) return false;
    if (!token) return p->selector!=FORMAT_RESULT;
    if (p->selector==FORMAT_SPATIAL) return whole_columns(p->range);
    if (token==SIZE_MAX) return false;
    for (size_t i=0;i<p->guard_count;i++) if (p->guard_ids[i]==token-1) return true;
    return false;
}
static bool has_record(const FormatPatch *p,uint64_t id,size_t field) {
    size_t lo=0,hi=p->record_count;
    while (lo<hi) { size_t mid=lo+(hi-lo)/2; if (p->record_ids[mid]<id) lo=mid+1; else hi=mid; }
    if (lo==p->record_count || p->record_ids[lo]!=id) return false;
    lo=0; hi=p->field_count;
    while (lo<hi) { size_t mid=lo+(hi-lo)/2; if (p->fields[mid]<field) lo=mid+1; else hi=mid; }
    return lo<p->field_count && p->fields[lo]==field;
}
static CellFormat resolve(const FormatStore *s,uint64_t row,size_t column,size_t token,size_t record_token,bool record,uint64_t id,size_t field) {
    if (row>=DX_SHEET_ROWS || column>=DX_FORMAT_FIELDS) return format_default();
    CellFormat result=format_default(); unsigned needed=FORMAT_ALL;
    for (size_t i=s->count;i && needed;i--) {
        const FormatPatch *p=&s->items[i-1]; FormatRange r=p->range;
        bool matches=p->selector==FORMAT_RECORDS ? record && id && field<DX_FORMAT_FIELDS && record_token && record_token<=s->scope_count && p->guard_ids[0]==record_token-1 && has_record(p,id,field) :
            row>=r.r0 && row<=r.r1 && column>=r.c0 && column<=r.c1 && eligible(p,token);
        if (matches) { merge(&result,p->value,p->mask&needed); result.specified|=(unsigned char)(p->mask&needed); needed&=~p->mask; }
    }
    return result;
}
CellFormat format_resolve_token(const FormatStore *s,uint64_t row,size_t column,size_t token) { return resolve(s,row,column,token,0,false,0,0); }
CellFormat format_resolve_record_token(const FormatStore *s,uint64_t row,size_t column,size_t result_token,size_t record_token,uint64_t id,size_t field) {
    if (!result_token && (record_token || id)) result_token=SIZE_MAX;
    if (result_token && result_token<=s->scope_count && record_token && record_token<=s->scope_count && s->scopes[result_token-1].owner!=s->scopes[record_token-1].owner) result_token=record_token=SIZE_MAX;
    return resolve(s,row,column,result_token,record_token,true,id,field);
}
CellFormat format_resolve(const FormatStore *s,uint64_t row,size_t column,const FormatScopeView *scope) { return format_resolve_token(s,row,column,format_scope_token(s,scope)); }
CellFormat format_at(const FormatStore *s,uint64_t row,size_t column) { return format_resolve_token(s,row,column,0); }
static size_t add_bytes(size_t a,size_t b) { return a>SIZE_MAX-b ? SIZE_MAX : a+b; }
static size_t state_bytes(FormatSnapshot s) {
    if (s.count>DX_FORMAT_PATCHES || s.scope_count>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS || (s.count && !s.items) || (s.scope_count && !s.scopes)) return SIZE_MAX;
    size_t bytes=s.count*sizeof(*s.items)+s.scope_count*sizeof(*s.scopes);
    for (size_t i=0;i<s.count;i++) {
        if (s.items[i].guard_count>DX_FORMAT_GUARDS || s.items[i].record_count>DX_FORMAT_RECORDS || s.items[i].field_count>DX_FORMAT_FIELDS) return SIZE_MAX;
        bytes=add_bytes(bytes,s.items[i].guard_count*sizeof(size_t));
        bytes=add_bytes(bytes,s.items[i].record_count*sizeof(uint64_t));
        bytes=add_bytes(bytes,s.items[i].field_count*sizeof(size_t));
    }
    for (size_t i=0;i<s.scope_count;i++) bytes=add_bytes(bytes,add_bytes(s.scopes[i].bytes,1));
    return bytes;
}
static FormatSnapshot current(const FormatStore *s) { return (FormatSnapshot){.items=s->items,.count=s->count,.scopes=s->scopes,.scope_count=s->scope_count,.source_aware=s->source_aware}; }
size_t format_owned_bytes(const FormatStore *s,bool history) {
    size_t bytes=state_bytes(current(s));
    if (history) {
        if (s->undo_count>DX_FORMAT_HISTORY || s->redo_count>DX_FORMAT_HISTORY) return SIZE_MAX;
        for (size_t i=0;i<s->undo_count;i++) bytes=add_bytes(bytes,state_bytes(s->undo[i]));
        for (size_t i=0;i<s->redo_count;i++) bytes=add_bytes(bytes,state_bytes(s->redo[i]));
    }
    return bytes;
}
static bool membership_valid(const uint64_t *ids,size_t count,const size_t *fields,size_t field_count) {
    if (!count || count>DX_FORMAT_RECORDS || !ids || !field_count || field_count>DX_FORMAT_FIELDS || !fields) return false;
    for (size_t i=0;i<count;i++) if (!ids[i] || (i && ids[i-1]>=ids[i])) return false;
    for (size_t i=0;i<field_count;i++) if (fields[i]>=DX_FORMAT_FIELDS || (i && fields[i-1]>=fields[i])) return false;
    return true;
}
bool format_valid(const FormatStore *s,char **error) {
    if (s->count>DX_FORMAT_PATCHES || (s->count && !s->items) || s->scope_count>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS || (s->scope_count && !s->scopes)) return fail(error,"Cell formats exceed the workbook's bounded range limit.");
    for (size_t i=0;i<s->scope_count;i++) {
        const FormatScope *scope=&s->scopes[i];
        if (!scope->definition || !scope->bytes || scope->bytes>=DX_FORMAT_SCOPE_BYTES) return fail(error,"A cell format result definition exceeds its 256 KiB limit.");
    }
    if (!scopes_unique(s->scopes,s->scope_count)) return fail(error,"Duplicate cell format result definition.");
    for (size_t i=0;i<s->count;i++) {
        const FormatPatch *p=&s->items[i];
        if (!range_valid(p->range) || !p->mask || (p->mask&~FORMAT_ALL) || !value_valid(p->value)) return fail(error,"Invalid cell format range.");
        if (!s->source_aware && p->selector!=FORMAT_SPATIAL) return fail(error,"Scoped cell format selectors require source-aware snapshot state.");
        if (p->selector<FORMAT_SPATIAL || p->selector>FORMAT_RECORDS || p->guard_count>DX_FORMAT_GUARDS || (p->selector==FORMAT_SPATIAL ? p->guard_count!=0 || p->guard_ids!=NULL : !p->guard_count || !p->guard_ids)) return fail(error,"Invalid cell format result guards.");
        if (p->selector==FORMAT_RECORDS ? p->guard_count!=1 || !same_range(p->range,(FormatRange){0}) || !membership_valid(p->record_ids,p->record_count,p->fields,p->field_count) :
            p->record_ids!=NULL || p->record_count || p->fields!=NULL || p->field_count) return fail(error,"Invalid cell format record membership.");
        for (size_t g=0;g<p->guard_count;g++) if (p->guard_ids[g]>=s->scope_count || (g && p->guard_ids[g-1]>=p->guard_ids[g])) return fail(error,"Invalid cell format scope reference.");
    }
    if (state_bytes(current(s))>DX_FORMAT_CURRENT_BYTES || format_owned_bytes(s,true)>DX_FORMAT_HISTORY_BYTES) return fail(error,"Cell format storage exceeds its bounded memory limit.");
    return true;
}
static FormatPatch patch_copy(FormatPatch p) {
    if (p.guard_count) { size_t *ids=dx_alloc(p.guard_count*sizeof(*ids)); memcpy(ids,p.guard_ids,p.guard_count*sizeof(*ids)); p.guard_ids=ids; }
    if (p.record_count) { uint64_t *ids=dx_alloc(p.record_count*sizeof(*ids)); memcpy(ids,p.record_ids,p.record_count*sizeof(*ids)); p.record_ids=ids; }
    if (p.field_count) { size_t *fields=dx_alloc(p.field_count*sizeof(*fields)); memcpy(fields,p.fields,p.field_count*sizeof(*fields)); p.fields=fields; }
    return p;
}
static void patch_free(FormatPatch p) { free(p.guard_ids); free(p.record_ids); free(p.fields); }
static FormatSnapshot snapshot(const FormatStore *s) {
    FormatSnapshot copy={.count=s->count,.scope_count=s->scope_count,.source_aware=s->source_aware};
    if (s->count) { copy.items=dx_alloc(s->count*sizeof(*copy.items)); for (size_t i=0;i<s->count;i++) copy.items[i]=patch_copy(s->items[i]); }
    if (s->scope_count) {
        copy.scopes=dx_alloc(s->scope_count*sizeof(*copy.scopes));
        for (size_t i=0;i<s->scope_count;i++) { copy.scopes[i]=s->scopes[i]; copy.scopes[i].definition=dx_alloc(s->scopes[i].bytes+1); memcpy(copy.scopes[i].definition,s->scopes[i].definition,s->scopes[i].bytes); }
    }
    return copy;
}
void format_snapshot_free(FormatSnapshot *s) {
    for (size_t i=0;i<s->count;i++) patch_free(s->items[i]);
    for (size_t i=0;i<s->scope_count;i++) free(s->scopes[i].definition);
    free(s->items); free(s->scopes); *s=(FormatSnapshot){0};
}
static void clear(FormatSnapshot *stack,size_t *count) { while (*count) format_snapshot_free(&stack[--*count]); }
static void push(FormatSnapshot *stack,size_t *count,FormatSnapshot state) {
    if (*count==DX_FORMAT_HISTORY) { format_snapshot_free(&stack[0]); memmove(stack,stack+1,(--*count)*sizeof(*stack)); }
    stack[(*count)++]=state;
}
void format_free(FormatStore *s) { FormatSnapshot state=current(s); format_snapshot_free(&state); clear(s->undo,&s->undo_count); clear(s->redo,&s->redo_count); memset(s,0,sizeof(*s)); }
void format_copy(FormatStore *to,const FormatStore *from) {
    FormatSnapshot copy=snapshot(from); uint64_t revision=from->revision; format_free(to); to->items=copy.items; to->count=copy.count; to->scopes=copy.scopes; to->scope_count=copy.scope_count; to->source_aware=copy.source_aware; to->revision=revision;
}
static bool same_membership(FormatPatch a,FormatPatch b) {
    return a.record_count==b.record_count && a.field_count==b.field_count &&
        (!a.record_count || !memcmp(a.record_ids,b.record_ids,a.record_count*sizeof(uint64_t))) &&
        (!a.field_count || !memcmp(a.fields,b.fields,a.field_count*sizeof(size_t)));
}
static bool same_snapshot(const FormatStore *a,const FormatSnapshot *b) {
    if (a->source_aware!=b->source_aware || a->count!=b->count) return false;
    for (size_t i=0;i<a->count;i++) {
        const FormatPatch *x=&a->items[i],*y=&b->items[i];
        if (!same_range(x->range,y->range) || x->mask!=y->mask || !same_value(x->value,y->value,x->mask) || x->selector!=y->selector || x->guard_count!=y->guard_count || !same_membership(*x,*y)) return false;
        for (size_t g=0;g<x->guard_count;g++) {
            bool found=false; for (size_t h=0;h<y->guard_count;h++) found|=scope_same(a->scopes[x->guard_ids[g]],b->scopes[y->guard_ids[h]]);
            if (!found) return false;
        }
    }
    return true;
}
bool format_snapshot_matches(const FormatStore *s,const FormatSnapshot *snapshot) { return s && snapshot && same_snapshot(s,snapshot); }
bool format_commit(FormatStore *s,FormatSnapshot *owned) {
    if (same_snapshot(s,owned)) { format_snapshot_free(owned); return false; }
    clear(s->redo,&s->redo_count); push(s->undo,&s->undo_count,current(s));
    s->items=owned->items; s->count=owned->count; s->scopes=owned->scopes; s->scope_count=owned->scope_count; s->source_aware=owned->source_aware; *owned=(FormatSnapshot){0}; s->revision++; return true;
}
static bool same_selector(FormatPatch a,FormatPatch b) {
    return a.selector==b.selector && a.guard_count==b.guard_count && (!a.guard_count || !memcmp(a.guard_ids,b.guard_ids,a.guard_count*sizeof(size_t))) && same_membership(a,b);
}
static bool covers_membership(FormatPatch a,FormatPatch b) {
    size_t at=0;
    for (size_t i=0;i<b.record_count;i++) { while (at<a.record_count && a.record_ids[at]<b.record_ids[i]) at++; if (at==a.record_count || a.record_ids[at]!=b.record_ids[i]) return false; }
    at=0;
    for (size_t i=0;i<b.field_count;i++) { while (at<a.field_count && a.fields[at]<b.fields[i]) at++; if (at==a.field_count || a.fields[at]!=b.fields[i]) return false; }
    return true;
}
static bool covers_selector(FormatPatch a,FormatPatch b) {
    /* Physical ranges cannot prove coverage of original-record membership. */
    if (a.selector==FORMAT_RECORDS || b.selector==FORMAT_RECORDS) return a.selector==FORMAT_RECORDS && b.selector==FORMAT_RECORDS && a.guard_ids[0]==b.guard_ids[0] && covers_membership(a,b);
    if (a.selector==FORMAT_SPATIAL) return whole_columns(a.range) || (b.selector==FORMAT_SPATIAL && !whole_columns(b.range));
    if (b.selector!=FORMAT_RESULT && a.selector==FORMAT_RESULT) return false;
    if (b.selector==FORMAT_SPATIAL) return !whole_columns(b.range);
    for (size_t i=0;i<b.guard_count;i++) {
        bool found=false; for (size_t j=0;j<a.guard_count;j++) found|=a.guard_ids[j]==b.guard_ids[i];
        if (!found) return false;
    }
    return true;
}
static bool append(FormatSnapshot *s,FormatPatch patch,char **error) {
    /* Later edits of unrelated properties cannot change this exact selector's
       value. Stop at the first requested property, even on another selector or
       range: a newer result/default reset may require reasserting the edit. */
    for (size_t i=s->count;i;i--) if (s->items[i-1].mask&patch.mask) {
        FormatPatch last=s->items[i-1];
        if (same_range(last.range,patch.range) && same_selector(last,patch) &&
            (last.mask&patch.mask)==patch.mask && same_value(last.value,patch.value,patch.mask)) return true;
        break;
    }
    if (s->count && same_range(s->items[s->count-1].range,patch.range) && same_selector(s->items[s->count-1],patch)) {
        FormatPatch *last=&s->items[s->count-1]; merge(&last->value,patch.value,patch.mask); last->mask|=patch.mask; return true;
    }
    size_t at=0;
    for (size_t i=0;i<s->count;i++) {
        FormatPatch old=s->items[i]; if (contains(patch.range,old.range) && covers_selector(patch,old)) old.mask&=~patch.mask;
        if (old.mask) s->items[at++]=old; else patch_free(old);
    }
    s->count=at; if (at==DX_FORMAT_PATCHES) return fail(error,"Cell formats are limited to 512 range records per sheet. Format a containing range to consolidate them.");
    s->items=dx_realloc(s->items,(at+1)*sizeof(*s->items)); s->items[s->count++]=patch_copy(patch); return true;
}
static void compact(FormatSnapshot *s) {
    if (s->scope_count) {
        size_t *map=dx_alloc(s->scope_count*sizeof(*map)); for (size_t i=0;i<s->scope_count;i++) map[i]=SIZE_MAX;
        for (size_t i=0;i<s->count;i++) for (size_t g=0;g<s->items[i].guard_count;g++) map[s->items[i].guard_ids[g]]=0;
        size_t count=0;
        for (size_t i=0;i<s->scope_count;i++) {
            if (map[i]==SIZE_MAX) free(s->scopes[i].definition);
            else { map[i]=count; s->scopes[count++]=s->scopes[i]; }
        }
        for (size_t i=0;i<s->count;i++) for (size_t g=0;g<s->items[i].guard_count;g++) s->items[i].guard_ids[g]=map[s->items[i].guard_ids[g]];
        free(map); s->scope_count=count;
        if (count) s->scopes=dx_realloc(s->scopes,count*sizeof(*s->scopes)); else { free(s->scopes); s->scopes=NULL; }
    }
    if (s->count) s->items=dx_realloc(s->items,s->count*sizeof(*s->items)); else { free(s->items); s->items=NULL; }
}
static bool ready(const FormatStore *s,FormatSnapshot *next,char **error) {
    compact(next);
    if (state_bytes(*next)>DX_FORMAT_CURRENT_BYTES) return fail(error,"Cell formats are limited to 1 MiB per sheet. Use a whole-column format or a smaller result definition.");
    if (same_snapshot(s,next)) return true;
    size_t bytes=add_bytes(state_bytes(*next),state_bytes(current(s)));
    for (size_t i=s->undo_count==DX_FORMAT_HISTORY ? 1 : 0;i<s->undo_count;i++) bytes=add_bytes(bytes,state_bytes(s->undo[i]));
    if (bytes>DX_FORMAT_HISTORY_BYTES) return fail(error,"Cell format history exceeds its 32 MiB memory limit. No formatting or worksheet history was changed.");
    return true;
}
static bool scope_views_valid(const FormatScopeView *views,size_t count,char **error) {
    if (!count || count>DX_FORMAT_GUARDS || !views) return fail(error,"A cell format can bind at most 33 exact result owners.");
    for (size_t i=0;i<count;i++) {
        if (!views[i].definition || !views[i].bytes || views[i].bytes>=DX_FORMAT_SCOPE_BYTES) return fail(error,"A cell format result definition exceeds its 256 KiB limit.");
        for (size_t j=0;j<i;j++) if (views[i].owner==views[j].owner) return fail(error,"A cell format needs one exact result definition per owner.");
    }
    return true;
}
static bool scope_ids(FormatSnapshot *s,const FormatScopeView *views,size_t count,size_t ids[DX_FORMAT_GUARDS],char **error) {
    for (size_t i=0;i<count;i++) {
        FormatScope candidate={views[i].owner,(void *)views[i].definition,views[i].bytes}; size_t at=0;
        while (at<s->scope_count && !scope_same(s->scopes[at],candidate)) at++;
        if (at==s->scope_count) {
            size_t bytes=add_bytes(state_bytes(*s),sizeof(FormatScope)+candidate.bytes+1);
            if (bytes>DX_FORMAT_CURRENT_BYTES) return fail(error,"Cell formats are limited to 1 MiB per sheet. Use a whole-column format or a smaller result definition.");
            s->scopes=dx_realloc(s->scopes,(at+1)*sizeof(*s->scopes)); candidate.definition=dx_alloc(candidate.bytes+1); memcpy(candidate.definition,views[i].definition,candidate.bytes); s->scopes[s->scope_count++]=candidate;
        }
        ids[i]=at;
    }
    /* Stable order makes semantic-equivalent multi-owner actions a no-op. */
    for (size_t i=1;i<count;i++) { size_t value=ids[i],j=i; while (j && ids[j-1]>value) { ids[j]=ids[j-1]; j--; } ids[j]=value; }
    return true;
}
bool format_plan_scoped(const FormatStore *s,FormatRange range,unsigned mask,CellFormat value,const FormatScopeView *views,size_t count,bool manual,FormatSnapshot *out,char **error) {
    if (!s || !out) return fail(error,"Choose an independent cell format plan.");
    if (!range_valid(range) || !mask || (mask&~FORMAT_ALL) || !value_valid(value)) return fail(error,"Choose a valid cell format and worksheet range.");
    if (!scope_views_valid(views,count,error)) return false;
    FormatSnapshot next=snapshot(s); next.source_aware=true; size_t ids[DX_FORMAT_GUARDS];
    bool ok=scope_ids(&next,views,count,ids,error) && append(&next,(FormatPatch){.range=range,.mask=mask,.value=value,.selector=manual ? FORMAT_GUARDED : FORMAT_RESULT,.guard_ids=ids,.guard_count=count},error) && ready(s,&next,error);
    if (!ok) { format_snapshot_free(&next); return false; }
    format_snapshot_free(out); *out=next; return true;
}
bool format_plan_spatial(const FormatStore *s,FormatRange range,unsigned mask,CellFormat value,FormatSnapshot *out,char **error) {
    if (!s || !out) return fail(error,"Choose an independent cell format plan.");
    if (!range_valid(range) || !mask || (mask&~FORMAT_ALL) || !value_valid(value)) return fail(error,"Choose a valid cell format and worksheet range.");
    FormatSnapshot next=snapshot(s); bool ok=append(&next,(FormatPatch){.range=range,.mask=mask,.value=value},error) && ready(s,&next,error);
    if (!ok) { format_snapshot_free(&next); return false; }
    format_snapshot_free(out); *out=next; return true;
}
bool format_apply_scoped(FormatStore *s,FormatRange range,unsigned mask,CellFormat value,const FormatScopeView *views,size_t count,bool manual,char **error) {
    FormatSnapshot next={0};
    if (!format_plan_scoped(s,range,mask,value,views,count,manual,&next,error)) return false;
    format_commit(s,&next); return true;
}
bool format_apply(FormatStore *s,FormatRange range,unsigned mask,CellFormat value,char **error) {
    FormatSnapshot next={0};
    if (!format_plan_spatial(s,range,mask,value,&next,error)) return false;
    format_commit(s,&next); return true;
}
bool format_plan_records(const FormatStore *s,const FormatScopeView *scope,const uint64_t *records,size_t record_count,const size_t *fields,size_t field_count,unsigned mask,CellFormat value,FormatSnapshot *out,char **error) {
    if (!out) return fail(error,"Choose an owned output for the record-format plan.");
    if (!mask || (mask&~FORMAT_ALL) || !value_valid(value) || !membership_valid(records,record_count,fields,field_count)) return fail(error,"Choose a valid format and sorted record/field membership (at most 20,000 records and 16,384 fields).");
    if (!scope_views_valid(scope,1,error)) return false;
    FormatSnapshot next=snapshot(s); next.source_aware=true; size_t ids[DX_FORMAT_GUARDS];
    bool ok=scope_ids(&next,scope,1,ids,error) && append(&next,(FormatPatch){.mask=mask,.value=value,.selector=FORMAT_RECORDS,.guard_ids=ids,.guard_count=1,
        .record_ids=(uint64_t *)records,.record_count=record_count,.fields=(size_t *)fields,.field_count=field_count},error) && ready(s,&next,error);
    if (!ok) { format_snapshot_free(&next); return false; }
    format_snapshot_free(out); *out=next; return true;
}
bool format_apply_records(FormatStore *s,const FormatScopeView *scope,const uint64_t *records,size_t record_count,const size_t *fields,size_t field_count,unsigned mask,CellFormat value,char **error) {
    FormatSnapshot next={0};
    if (!format_plan_records(s,scope,records,record_count,fields,field_count,mask,value,&next,error)) return false;
    format_commit(s,&next); return true;
}
bool format_history(FormatStore *s,bool redo) {
    FormatSnapshot *from=redo ? s->redo : s->undo,*to=redo ? s->undo : s->redo; size_t *fc=redo ? &s->redo_count : &s->undo_count,*tc=redo ? &s->undo_count : &s->redo_count;
    if (!*fc) return false; push(to,tc,current(s)); FormatSnapshot state=from[--*fc]; from[*fc]=(FormatSnapshot){0}; s->items=state.items; s->count=state.count; s->scopes=state.scopes; s->scope_count=state.scope_count; s->source_aware=state.source_aware; s->revision++; return true;
}
void format_trim_history(FormatStore *s,bool redo,size_t keep) {
    FormatSnapshot *stack=redo ? s->redo : s->undo; size_t *count=redo ? &s->redo_count : &s->undo_count;
    if (keep>=*count) return; size_t drop=*count-keep; for (size_t i=0;i<drop;i++) format_snapshot_free(&stack[i]); memmove(stack,stack+drop,keep*sizeof(*stack)); memset(stack+keep,0,drop*sizeof(*stack)); *count=keep;
}
static bool move_resets_records(const FormatStore *s,FormatRange from,FormatRange to) {
    for (size_t i=0;i<s->count;i++) {
        const FormatPatch *p=&s->items[i];
        if ((p->selector!=FORMAT_RESULT && p->selector!=FORMAT_GUARDED) || (!overlap(p->range,from) && !overlap(p->range,to))) continue;
        for (size_t j=0;j<s->count;j++) if (s->items[j].selector==FORMAT_RECORDS) {
            uint64_t owner=s->scopes[s->items[j].guard_ids[0]].owner;
            for (size_t g=0;g<p->guard_count;g++) if (s->scopes[p->guard_ids[g]].owner==owner) return true;
        }
    }
    return false;
}
bool format_plan_move(const FormatStore *s,FormatRange range,int64_t dr,int dc,FormatSnapshot *out,char **error) {
    *out=(FormatSnapshot){0}; if (!range_valid(range) || dr<-(int64_t)DX_SHEET_ROWS || dr>(int64_t)DX_SHEET_ROWS || dc<-16384 || dc>16384 ||
        (dr<0 && range.r0<(uint64_t)-dr) || (dc<0 && range.c0<(size_t)-dc) || (int64_t)range.r1+dr>=(int64_t)DX_SHEET_ROWS || (int64_t)range.c1+dc>=16384) return fail(error,"Formatting moves stay inside the worksheet.");
    FormatRange destination={(uint64_t)((int64_t)range.r0+dr),(uint64_t)((int64_t)range.r1+dr),(size_t)((int64_t)range.c0+dc),(size_t)((int64_t)range.c1+dc)};
    if ((dr || dc) && move_resets_records(s,range,destination)) return fail(error,"Moving this range would change record-format precedence through positional result resets. No formatting was changed.");
    *out=snapshot(s); if (!dr && !dc) return true;
    bool relevant=false; for (size_t i=0;i<s->count;i++) if (s->items[i].selector!=FORMAT_RECORDS) relevant|=overlap(s->items[i].range,range) || overlap(s->items[i].range,destination);
    if (!relevant) return true;
    if (!append(out,(FormatPatch){.range=range,.mask=FORMAT_ALL,.value=format_default()},error) || !append(out,(FormatPatch){.range=destination,.mask=FORMAT_ALL,.value=format_default()},error)) goto failed;
    /* Clear the same imported eligibility as each original patch. A manual-only
       reset must not erase or expose an unrelated dormant result definition. */
    for (size_t i=0;i<s->count;i++) if (s->items[i].selector!=FORMAT_SPATIAL && s->items[i].selector!=FORMAT_RECORDS && (overlap(s->items[i].range,range) || overlap(s->items[i].range,destination))) {
        FormatPatch clear=s->items[i]; clear.mask=FORMAT_ALL; clear.value=format_default(); clear.range=range;
        if (!append(out,clear,error)) goto failed;
        clear.range=destination; if (!append(out,clear,error)) goto failed;
    }
    for (size_t i=0;i<s->count;i++) if (s->items[i].selector!=FORMAT_RECORDS && overlap(s->items[i].range,range)) {
        FormatPatch patch=s->items[i]; FormatRange r=patch.range;
        patch.range=(FormatRange){(r.r0>range.r0 ? r.r0 : range.r0)+dr,(r.r1<range.r1 ? r.r1 : range.r1)+dr,
            (size_t)((int64_t)(r.c0>range.c0 ? r.c0 : range.c0)+dc),(size_t)((int64_t)(r.c1<range.c1 ? r.c1 : range.c1)+dc)};
        if (!append(out,patch,error)) goto failed;
    }
    if (!ready(s,out,error)) goto failed;
    return true;
failed: format_snapshot_free(out); return false;
}
bool format_plan_axis(const FormatStore *s,bool rows,uint64_t at,uint64_t count,bool insert,FormatSnapshot *out,char **error) {
    *out=(FormatSnapshot){0}; uint64_t limit=rows ? DX_SHEET_ROWS : 16384;
    if (at>=limit || !count || count>limit-at) return fail(error,"Formatting changes stay inside the worksheet.");
    *out=snapshot(s); size_t n=0;
    for (size_t i=0;i<out->count;i++) {
        FormatPatch patch=out->items[i]; uint64_t lo=rows ? patch.range.r0 : patch.range.c0,hi=rows ? patch.range.r1 : patch.range.c1;
        if (patch.selector==FORMAT_RECORDS) { out->items[n++]=patch; continue; }
        /* Explicit spatial column/sheet defaults include future cells. Keeping
           their sentinel extent also keeps them eligible on imported targets. */
        if (patch.selector==FORMAT_SPATIAL && whole_columns(patch.range) && (rows || (!patch.range.c0 && patch.range.c1==16383))) { out->items[n++]=patch; continue; }
        if (insert) { if (lo>=at) lo+=count; if (hi>=at) hi+=count; if (lo>=limit) { patch_free(patch); continue; } if (hi>=limit) hi=limit-1; }
        else if (hi>=at) {
            if (lo>=at && hi<at+count) { patch_free(patch); continue; }
            if (lo>=at) lo=lo<at+count ? at : lo-count;
            hi=hi<at+count ? at-1 : hi-count;
        }
        if (rows) { patch.range.r0=lo; patch.range.r1=hi; } else { patch.range.c0=(size_t)lo; patch.range.c1=(size_t)hi; }
        out->items[n++]=patch;
    }
    out->count=n;
    if (!ready(s,out,error)) { format_snapshot_free(out); return false; }
    return true;
}
