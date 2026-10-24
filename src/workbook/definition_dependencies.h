#ifndef DUXCEL_DEFINITION_DEPENDENCIES_H
#define DUXCEL_DEFINITION_DEPENDENCIES_H
#include "objects_asset.h"
#include "source_versions_codec.h"
typedef struct { const Analysis *primary; const ObjectSnapshot *objects; } DefinitionDependencyView;
typedef struct { SourceVersionsAsset sources; size_t unbound_sources; /* Observations across both states, not inferred unique identities. */ } DefinitionDependencies;
typedef enum {
    DEFINITION_DEPENDENCIES_OK,DEFINITION_DEPENDENCIES_INVALID,DEFINITION_DEPENDENCIES_MISSING,
    DEFINITION_DEPENDENCIES_CONFLICT,DEFINITION_DEPENDENCIES_LIMIT,DEFINITION_DEPENDENCIES_UNSUPPORTED
} DefinitionDependenciesStatus;
/* Close primary/Table version bindings across both definition states. A changed
   constituent is supplied as a complete pair; NULL/NULL omits it, while one NULL
   rejects. Trusted prevalidated graphs only: bounded model measure is repeated,
   but full grammar/query/Object semantics still belong to canonical capture.
   Nonzero CSV/Parquet versions require exact immutable owner/kind/locator rows;
   complete or pending metadata survives. Version0 is explicitly unbound, never
   an invented identity or activation proof. Other bound source kinds reject.
   Selected rows are copied/deduplicated in ledger order, with captured high-water
   outside Undo. reserved covers only new copied ledger ownership; graph validation
   and outer event/global budgets remain separate. out is zero/independently owned,
   not the input ledger. All failures preserve every input and the existing out.
   No FS/SQL, source materialization, schema rebinding, live adoption or publication. */
DefinitionDependenciesStatus definition_dependencies_build(uint64_t sheet,DefinitionDependencyView before,DefinitionDependencyView after,
    const SourceVersions *ledger,uint64_t object_high_water,size_t reserved,DefinitionDependencies *out);
void definition_dependencies_free(DefinitionDependencies *dependencies);
#endif
