#ifndef DUXCEL_SOURCE_VERSIONS_ADMISSION_H
#define DUXCEL_SOURCE_VERSIONS_ADMISSION_H
#include "source_versions_codec.h"
typedef enum { SOURCE_SUBSET_EXACT,SOURCE_SUBSET_REFINED,SOURCE_SUBSET_UNAVAILABLE,
    SOURCE_SUBSET_CONFLICT,SOURCE_SUBSET_INVALID } SourceSubsetStatus;
typedef struct {
    size_t exact,refined,unavailable,conflicting,captured_pending;
    bool allocator_unavailable;
} SourceSubsetAdmission;
/* Read-only compatibility of a decoded DXSV subset with one current immutable
   sheet ledger. Validates both with existing canonical RAW/ledger rules before
   comparison. Captured rows are bounded by their captured allocator; live owners
   use the actual current object high-water. Extra live rows and higher monotonic
   allocator values are allowed; captured counters are NEVER assigned to live.
   Matching ID/owner/kind/locator with pending captured RAW may be refined by a
   complete live RAW. This reports metadata compatibility ONLY: captured_pending
   stays nonzero, no packet is copied/merged and no observation, Page, file,
   activation, rebind or writer readiness follows. Complete captured RAW requires
   exact bytes; missing live completion is unavailable, different complete RAW
   conflicts. A missing ledger, row, owner capacity or allocator is unavailable.
   Conflicts dominate unavailable, which dominates refined, then exact.
   sheet mismatch conflicts. Malformed inputs preserve out; all other outcomes
   fill the bounded summary. Inputs are immutable/readable during the call.
   Existing bounded RAW-validator stack workspace is reused serially for both
   graphs; no heap reservation or normalization is introduced.
   No allocation, IO, ownership transfer, ID issuance or live mutation. */
SourceSubsetStatus source_versions_subset_admit(const SourceVersionsAsset *captured,
    uint64_t live_sheet,const SourceVersions *live,uint64_t object_high_water,SourceSubsetAdmission *out);
#endif
