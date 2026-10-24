#ifndef DUXCEL_DEFINITION_EVENT_INTERNAL_H
#define DUXCEL_DEFINITION_EVENT_INTERNAL_H
#include "definition_event.h"
/* Borrowed prepared plan; never keep across model/ledger mutation or reentry. */
typedef struct {
    DefinitionEventView view;
    DefinitionEventInfo info;
    size_t lengths[6];
    bool selected[DX_SOURCE_VERSIONS];
} DefinitionEventPlan;
DefinitionEventStatus definition_event_prepare(DefinitionEventView view,size_t validation_reserved,DefinitionEventPlan *out);
/* Private disjoint UNPUBLISHED arena only. Caller cancels/discards any partial
   bytes on failure; written is preserved. No extra full packet allocation. */
DefinitionEventStatus definition_event_encode_unpublished(const DefinitionEventPlan *plan,size_t validation_reserved,
    void *arena,size_t capacity,size_t *written);
#endif
