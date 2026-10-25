#ifndef DX_SOURCE_VIEW_H
#define DX_SOURCE_VIEW_H
#include "../transforms/analysis.h"
/* Runtime availability belongs to the selected actual result owner. In
   particular, UNAVAILABLE does not diagnose an offline file, and READY does
   not promise retained rows or availability after reopening a workbook. */
typedef enum { SOURCE_VIEW_LOADING, SOURCE_VIEW_READY, SOURCE_VIEW_UNAVAILABLE } SourceViewState;
typedef struct {
    const Analysis *definition;
    SourceKind kind;
    const char *path,*schema,*table,*query;
    SourceViewState state;
    bool external,read_only;
} SourceProvenanceView;
/* Allocation-free presentation projection of an external connection. All
   pointers are borrowed until the definition changes; consume synchronously.
   No filesystem probe, source validation, identity issuance or model mutation.
   Blank/generated sources and malformed presentation input preserve out. */
bool ui_source_provenance(const Analysis *source,SourceViewState state,SourceProvenanceView *out);
const char *ui_source_kind_label(SourceKind kind);
const char *ui_source_state_label(SourceViewState state);
#endif
