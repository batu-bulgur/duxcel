#include "marks.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool field_valid(const MarkField *f) { return f->name && *f->name && strlen(f->name)<=4096 && (unsigned)f->aggregate<=AGG_MAX && (unsigned)f->grain<=PIVOT_DATE_MONTH && (!f->measure || !f->grain); }
static bool same(const MarkField *a,const MarkField *b) { return a->measure==b->measure && a->grain==b->grain && (!a->measure || a->aggregate==b->aggregate) && !strcmp(a->name,b->name); }
const char *marks_label(MarkRole role) { static const char *names[]={"Color","Size","Label","Detail","Tooltip","Shape"}; return (unsigned)role<DX_MARK_ROLES ? names[role] : "Marks"; }
void marks_free(MarkDefinition *m) { for (int r=0;r<DX_MARK_ROLES;r++) for (size_t i=0;i<m->counts[r];i++) free(m->fields[r][i].name); memset(m,0,sizeof(*m)); }
MarkDefinition marks_copy(const MarkDefinition *m) { MarkDefinition copy=*m; for (int r=0;r<DX_MARK_ROLES;r++) for (size_t i=0;i<m->counts[r];i++) copy.fields[r][i].name=dx_strdup(m->fields[r][i].name); return copy; }
bool marks_valid(const MarkDefinition *m,char **error) {
    for (int r=0;r<DX_MARK_ROLES;r++) {
        if (m->counts[r]>((r==MARKS_SIZE || r==MARKS_SHAPE) ? 1 : DX_MARK_FIELDS)) return fail(error,"Size and Shape accept one field; other Marks targets accept up to eight.");
        for (size_t i=0;i<m->counts[r];i++) {
            if (!field_valid(&m->fields[r][i])) return fail(error,"Invalid Marks field assignment.");
            for (size_t j=0;j<i;j++) if (same(&m->fields[r][i],&m->fields[r][j])) return fail(error,"Duplicate Marks field assignment.");
        }
    }
    return true;
}
size_t marks_text_bytes(const MarkDefinition *m) { if (!marks_valid(m,NULL)) return SIZE_MAX; size_t bytes=0; for (int r=0;r<DX_MARK_ROLES;r++) for (size_t i=0;i<m->counts[r];i++) bytes+=strlen(m->fields[r][i].name)+1; return bytes; }
bool marks_equal(const MarkDefinition *a,const MarkDefinition *b) { if (!marks_valid(a,NULL)||!marks_valid(b,NULL)) return false; for (int r=0;r<DX_MARK_ROLES;r++) { if (a->counts[r]!=b->counts[r]) return false; for (size_t i=0;i<a->counts[r];i++) if (!same(&a->fields[r][i],&b->fields[r][i])) return false; } return true; }
bool marks_assign(MarkDefinition *m,MarkRole role,const MarkField *field,char **error) {
    if ((unsigned)role>=DX_MARK_ROLES || !field_valid(field) || !marks_valid(m,error)) return fail(error,"Choose a valid Marks field.");
    for (size_t i=0;i<m->counts[role];i++) if (same(&m->fields[role][i],field)) return true;
    if (role!=MARKS_SIZE && role!=MARKS_SHAPE && m->counts[role]==DX_MARK_FIELDS) return fail(error,"Use up to eight fields in this Marks target.");
    MarkField copy=*field; copy.name=dx_strdup(field->name);
    if (role==MARKS_SIZE || role==MARKS_SHAPE) { if (m->counts[role]) free(m->fields[role][0].name); m->counts[role]=1; m->fields[role][0]=copy; }
    else m->fields[role][m->counts[role]++]=copy;
    return true;
}
void marks_remove(MarkDefinition *m,MarkRole role,size_t index) { if ((unsigned)role>=DX_MARK_ROLES || index>=m->counts[role]) return; free(m->fields[role][index].name); memmove(m->fields[role]+index,m->fields[role]+index+1,(--m->counts[role]-index)*sizeof(MarkField)); memset(m->fields[role]+m->counts[role],0,sizeof(MarkField)); }
bool marks_move(MarkDefinition *m,MarkRole role,size_t from,size_t to) { if ((unsigned)role>=DX_MARK_ROLES || from>=m->counts[role] || to>=m->counts[role]) return false; MarkField field=m->fields[role][from]; if (from<to) memmove(m->fields[role]+from,m->fields[role]+from+1,(to-from)*sizeof(field)); else memmove(m->fields[role]+to+1,m->fields[role]+to,(from-to)*sizeof(field)); m->fields[role][to]=field; return true; }
