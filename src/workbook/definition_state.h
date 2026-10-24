#ifndef DUXCEL_DEFINITION_STATE_H
#define DUXCEL_DEFINITION_STATE_H
#include "objects.h"
/* Retained-event intent/preimage projection for trusted, prevalidated borrowed
   graphs. Pure/no allocation/grammar parsing/IO or mutation. Counts/pointers are
   bounded defensively; this is not model/archive/ownership/source validation.
   Nullable texts and visible field order are exact, even when SQL results or
   marks_equal would be equivalent. Schema-derived NumberType caches are ignored.
   All stored Marks attributes, including latent dimension aggregates, remain.
   Object order, IDs, placements, source links and definitions are authored;
   query epochs are not. Chart width/height are authored; Pivot/Table width/height
   are computed result extents and excluded. Checkpoint assets still preserve
   captured extents; replay must pair/invalidate results before using them.
   Analysis schema/table remain STRICT: default-relation completion requires an
   explicit baseline/resolution owner, never a silent equality waiver. */
bool definition_analysis_equal(const Analysis *left,const Analysis *right);
bool definition_pivot_equal(const PivotDefinition *left,const PivotDefinition *right);
bool definition_marks_equal(const MarkDefinition *left,const MarkDefinition *right);
bool definition_objects_equal(const ObjectSnapshot *left,const ObjectSnapshot *right);
#endif
