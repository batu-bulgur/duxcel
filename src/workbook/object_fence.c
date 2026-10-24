#include "object_fence.h"
#include <stdlib.h>
#include <string.h>
typedef char ObjectFenceStackBound[(sizeof(ObjectCommandTarget)<512u*1024u) ? 1 : -1];
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) {
    for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i;
    return NULL;
}
static size_t object_index(const ObjectStore *s,uint64_t id) {
    size_t i=0; while (i<s->count && s->items[i].id!=id) i++; return i;
}
static bool prepare(ObjectCommandContext *c,size_t index,uint64_t object,bool creation,ObjectCommandTarget *out,char **error) {
    if (!c || !c->request_epoch || !out || (!creation && !object) || !workbook_identity_valid(c->book) || index>=c->book->count)
        return fail(error,"Choose a valid object before editing it.");
    Worksheet *s=c->book->sheets+index;
    if (s->objects.count>DX_MAX_OBJECTS || (s->objects.count && !s->objects.items) || s->source_versions.count>DX_SOURCE_VERSIONS ||
        (s->source_versions.count && !s->source_versions.items)) return fail(error,"Invalid bounded object ownership.");
    if (!creation && object_index(&s->objects,object)==s->objects.count) return fail(error,"The object is missing.");
    if (!c->ready || c->read_only) return fail(error,"This object is currently read-only or unavailable.");
    CellEditTarget owner={0}; if (!workbook_prepare_cell_edit(c->book,index,0,0,&owner,error)) return false;
    memset(out,0,sizeof(*out)); out->owner=owner; out->object=object; out->request_epoch=*c->request_epoch;
    out->format_revision=s->formats.revision; out->history_last=s->history_last;
    memcpy(&out->objects,&s->objects,sizeof(s->objects)); memcpy(&out->sources,&s->source_versions,sizeof(s->source_versions));
    if (s->objects.count) memcpy(out->items,s->objects.items,s->objects.count*sizeof(*out->items));
    if (s->source_versions.count) memcpy(out->versions,s->source_versions.items,s->source_versions.count*sizeof(*out->versions));
    return true;
}
static ObjectFenceResult check(ObjectCommandContext *c,const ObjectCommandTarget *t,bool creation,char **error) {
    ObjectFenceResult r={.status=OBJECT_FENCE_INVALID};
    if (!c || !c->request_epoch || !t || (creation ? t->object!=0 : !t->object) || !t->owner.document || !t->owner.sheet || t->owner.row || t->owner.column || !workbook_identity_valid(c->book)) {
        fail(error,"Invalid object command."); return r;
    }
    Workbook *b=c->book; Worksheet *s=resolve(b,t->owner.sheet_id); r.owner=t->object; r.revision=b->command_revision; r.status=OBJECT_FENCE_STALE;
    if (!s || b->id.high!=t->owner.workbook.high || b->id.low!=t->owner.workbook.low || b->edit_session_id!=t->owner.document || s->edit_session_id!=t->owner.sheet ||
        b->command_revision!=t->owner.expected_document_revision || s->cells.revision!=t->owner.expected_revision || s->formats.revision!=t->format_revision ||
        s->history_last!=t->history_last || *c->request_epoch!=t->request_epoch || memcmp(&s->objects,&t->objects,sizeof(s->objects)) ||
        memcmp(&s->source_versions,&t->sources,sizeof(s->source_versions))) {
        fail(error,"The object owner changed; prepare the edit again."); return r;
    }
    if (s->objects.count>DX_MAX_OBJECTS || s->source_versions.count>DX_SOURCE_VERSIONS ||
        (s->objects.count && (!s->objects.items || memcmp(s->objects.items,t->items,s->objects.count*sizeof(*t->items)))) ||
        (s->source_versions.count && (!s->source_versions.items || memcmp(s->source_versions.items,t->versions,s->source_versions.count*sizeof(*t->versions))))) {
        fail(error,"Object layout, definitions or source metadata changed; prepare again."); return r;
    }
    if (!creation && object_index(&s->objects,t->object)==s->objects.count) { fail(error,"The object changed."); return r; }
    if (!c->ready || c->read_only) { r.status=OBJECT_FENCE_PROTECTED; fail(error,"This object is currently read-only or unavailable."); return r; }
    r.status=OBJECT_FENCE_READY; return r;
}

bool workbook_prepare_object_fence(ObjectCommandContext *c,size_t sheet,uint64_t object,ObjectCommandTarget *out,char **error) { return prepare(c,sheet,object,false,out,error); }
ObjectFenceResult workbook_check_object_fence(ObjectCommandContext *c,const ObjectCommandTarget *t,char **error) { return check(c,t,false,error); }
bool workbook_prepare_object_creation_fence(ObjectCommandContext *c,size_t sheet,ObjectCommandTarget *out,char **error) { return prepare(c,sheet,0,true,out,error); }
ObjectFenceResult workbook_check_object_creation_fence(ObjectCommandContext *c,const ObjectCommandTarget *t,char **error) { return check(c,t,true,error); }
