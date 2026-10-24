#ifndef DUXCEL_FORMAT_DEPENDENCIES_H
#define DUXCEL_FORMAT_DEPENDENCIES_H
#include "format_delta_codec.h"
#include "source_versions_codec.h"
typedef struct {
    SourceVersionsAsset sources;
    size_t legacy_scopes; /* Referenced opaque non-record observations across both states. */
} FormatDependencies;
typedef enum {
    FORMAT_DEPENDENCIES_OK,FORMAT_DEPENDENCIES_INVALID,FORMAT_DEPENDENCIES_MISSING,
    FORMAT_DEPENDENCIES_CONFLICT,FORMAT_DEPENDENCIES_LIMIT,FORMAT_DEPENDENCIES_UNSUPPORTED
} FormatDependenciesStatus;
/* Close referenced source metadata for two valid format snapshots against the
   caller's immutable ledger/candidate. DXFS scopes must bind exact owner/version/
   kind/RAW bytes; record selectors require RAW packets and valid original fields.
   Missing/pending rows never invent identity or query a source. Unknown non-DXFS
   non-record legacy bytes are counted and retained by the format asset itself;
   this is no permission to activate them. Unknown record identity rejects.
   Unreferenced scope rows do not create dependencies. Source file/Page/content,
   document association and locator resolution remain outer owner obligations.
   reserved covers new copied ledger rows/locators/packets. Separately account
   bounded format-validation scratch and one scope-count-byte visitation buffer.
   out is zero/independently owned and cannot alias the input ledger. All failures
   preserve out and every input. Success owns the needed ordered subset with the
   captured high-water obligation, never a value to restore over live allocation.
   No IO, issuance, replay, live adoption or App profile activation. */
FormatDependenciesStatus format_dependencies_build(uint64_t sheet,FormatSnapshotView before,FormatSnapshotView after,
    const SourceVersions *ledger,uint64_t object_high_water,size_t reserved,FormatDependencies *out);
void format_dependencies_free(FormatDependencies *dependencies);
#endif
