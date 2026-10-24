#ifndef DUXCEL_DEFINITION_BASELINE_H
#define DUXCEL_DEFINITION_BASELINE_H
#include "definition_dependencies.h"
typedef enum { DEFINITION_BASELINE_READY,DEFINITION_BASELINE_UNRESOLVED,DEFINITION_BASELINE_INVALID } DefinitionBaselineStatus;
/* Trusted, bounded definition view; omitted families are allowed. Pure and
   allocation-free. A DATABASE default relation must be resolved before retained
   origin/event admission. NULL table is unresolved; empty identifiers are not
   defaults. This is not full asset/source/schema validation or worker readiness.
   NumberType caches and computed object extents do not affect this boundary.
   Resolve legacy/new-source baselines BEFORE sealing an origin; never rewrite
   previously admitted immutable assets or invent a user action for enrichment. */
DefinitionBaselineStatus definition_baseline_status(DefinitionDependencyView view);
#endif
