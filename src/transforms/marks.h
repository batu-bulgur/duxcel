#ifndef DX_MARKS_H
#define DX_MARKS_H
#include "pivot.h"
typedef enum { MARKS_COLOR,MARKS_SIZE,MARKS_LABEL,MARKS_DETAIL,MARKS_TOOLTIP,MARKS_SHAPE,DX_MARK_ROLES } MarkRole;
#define DX_MARK_FIELDS 8
typedef struct { char *name; Aggregate aggregate; PivotDateGrain grain; bool measure; } MarkField;
typedef struct { MarkField fields[DX_MARK_ROLES][DX_MARK_FIELDS]; size_t counts[DX_MARK_ROLES]; } MarkDefinition;
void marks_free(MarkDefinition *marks);
MarkDefinition marks_copy(const MarkDefinition *marks);
bool marks_valid(const MarkDefinition *marks,char **error);
bool marks_equal(const MarkDefinition *a,const MarkDefinition *b);
size_t marks_text_bytes(const MarkDefinition *marks);
bool marks_assign(MarkDefinition *marks,MarkRole role,const MarkField *field,char **error);
void marks_remove(MarkDefinition *marks,MarkRole role,size_t index);
bool marks_move(MarkDefinition *marks,MarkRole role,size_t from,size_t to);
const char *marks_label(MarkRole role);
#endif
