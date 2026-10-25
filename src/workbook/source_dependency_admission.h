#ifndef DUXCEL_SOURCE_DEPENDENCY_ADMISSION_H
#define DUXCEL_SOURCE_DEPENDENCY_ADMISSION_H
#include "source_versions_admission.h"
#include "definition_dependencies.h"
#include "format_dependencies.h"
typedef struct { uint64_t sheet,object_high_water; const SourceVersions *versions; } SourceDependencyLive;
typedef struct { SourceSubsetAdmission sources; size_t unbound_sources,opaque_scopes; } SourceDependencyAdmission;
typedef enum { SOURCE_DEPENDENCY_EXACT,SOURCE_DEPENDENCY_REFINED,SOURCE_DEPENDENCY_UNAVAILABLE,
    SOURCE_DEPENDENCY_CONFLICT,SOURCE_DEPENDENCY_INVALID,SOURCE_DEPENDENCY_LIMIT,SOURCE_DEPENDENCY_UNSUPPORTED } SourceDependencyStatus;
/* Borrowed current decoded canonical definitions plus their decoded dependency
   metadata. Recompute exact selected owner/version/kind/locator associations and
   observation counts against CAPTURED rows first, then compare those rows with
   the immutable current ledger. Every captured row must be referenced: extra
   live rows are allowed; extra captured rows are not this closed dependency set.
   Full definition asset decode/grammar is a caller prerequisite, not repeated
   through a synthetic event. Source0 remains unbound/inactive, and compatible
   live completion never promotes a captured pending observation to readiness.
   Expected live sheet is explicit/nonzero. No second owner, IO, merge, issuance,
   rebind, replay or activation. INVALID/LIMIT/UNSUPPORTED preserve out; other
   outcomes report metadata compatibility/inactive observations only. */
SourceDependencyStatus source_definition_dependencies_admit(DefinitionDependencyView before,DefinitionDependencyView after,
    const DefinitionDependencies *captured,SourceDependencyLive live,SourceDependencyAdmission *out);
/* Allocation-free structural measure for the EXISTING format selector's serial
   scope marker, format-validation and maximum filter JSON validation scratch.
   Admit scratch before calling format admission; inputs stay immutable through
   both. The existing capture/selection APIs and their canonical semantics are
   unchanged. Failure preserves scratch. */
SourceDependencyStatus source_format_dependencies_scratch(FormatSnapshotView before,FormatSnapshotView after,size_t *scratch);
/* Recompute exact record/guard RAW closure against captured metadata, including
   fields and opaque non-record observations, before read-only live comparison.
   Captured record scopes require COMPLETE exact RAW; a current pending row is
   unavailable, not permission to weaken that captured proof. Opaque observations
   stay inactive and cannot authorize record scopes. */
SourceDependencyStatus source_format_dependencies_admit(FormatSnapshotView before,FormatSnapshotView after,
    const FormatDependencies *captured,SourceDependencyLive live,size_t reserved,SourceDependencyAdmission *out);
#endif
