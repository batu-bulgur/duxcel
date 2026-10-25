#ifndef DX_BROWSE_PREVIEW_H
#define DX_BROWSE_PREVIEW_H
#include "source_versions_codec.h"
#include "range_command.h" /* Existing borrowed actual-result resolver types. */
#define DX_BROWSE_PREVIEW_ROWS 256u
#define DX_BROWSE_PREVIEW_COLUMNS 256u
#define DX_BROWSE_PREVIEW_TEXT_BYTES (4u*1024u*1024u)
#define DX_BROWSE_PREVIEW_OWNED_BYTES (8u*1024u*1024u)
typedef struct BrowsePreview BrowsePreview;
typedef struct {
    const WorkbookId *document;
    const uint64_t *document_session,*sheet,*sheet_session,*object_high_water,*metadata_clock;
    const uint64_t *request_epoch,*query_epoch,*source_epoch;
    uint64_t owner; /* 0 primary, otherwise actual Table ID, never a Pivot/Chart. */
    const Analysis *definition;
    const void *result_owner;
    const SourceVersions *sources;
    const RangeResultContext *results;
    size_t other_owned; /* Caller accounts borrowed live inputs and other owners. */
} BrowsePreviewContext;
typedef struct {
    BrowsePreviewContext context;
    WorkbookId document;
    uint64_t document_session,sheet,sheet_session,object_high_water,metadata_clock,request,query_epoch,source_epoch;
    RangeResultContext resolver;
    RangeResultSnapshot result;
    SourceVersions sources;
    SourceVersion binding;
    bool bound;
} BrowsePreviewTarget;
typedef struct {
    WorkbookId document;
    uint64_t sheet,owner,object_high_water,source_version,source_high_water;
    uint64_t metadata_clock,request,query_epoch,source_epoch; /* Captured runtime observations, not reusable execution permission. */
    uint64_t offset,total;
    size_t rows,columns,records,definition_bytes,source_bytes,owned,validation_scratch,coexistence;
    bool count_pending,binding_pending;
} BrowsePreviewInfo;
typedef enum { BROWSE_PREVIEW_OK,BROWSE_PREVIEW_UNAVAILABLE,BROWSE_PREVIEW_STALE,
    BROWSE_PREVIEW_INVALID,BROWSE_PREVIEW_UNSUPPORTED,BROWSE_PREVIEW_LIMIT } BrowsePreviewStatus;
/* Pure optional browse cache, not retained inputs, a queryable dataset, saved
   analytical outputs or a persistent source/format verification. The resolver
   must return the actual selected owner's current ready INTERVAL. Unlike MOVE,
   this permits a pending count and preserves its observed lower bound. Never
   weaken the existing complete-result MOVE resolver to produce this proof.
   No IO, callbacks other than the read-only/non-reentrant resolver, model edits,
   session assignment, identity issuance, schema or persistence installation.
   Context/current owner descriptors remain valid during each synchronous call;
   text, arrays and packets are immutable between preparation and capture.
   Captured pointer representations are fences, never old owners to dereference. */
BrowsePreviewStatus browse_preview_prepare(const BrowsePreviewContext *context,BrowsePreviewTarget *out);
/* Full new+prior+target+scratch+other coexistence is reported before allocation.
   Existing preview may be NULL. Failure preserves info/output and all models. */
BrowsePreviewStatus browse_preview_measure(const BrowsePreviewContext *context,const BrowsePreviewTarget *target,
    const BrowsePreview *prior,BrowsePreviewInfo *out);
BrowsePreviewStatus browse_preview_capture(const BrowsePreviewContext *context,const BrowsePreviewTarget *target,
    size_t reserved,BrowsePreview **out);
void browse_preview_free(BrowsePreview *preview);
BrowsePreviewInfo browse_preview_info(const BrowsePreview *preview);
/* Borrowed immutable views, valid only while preview remains owned. The Page
   owns independent C-allocator cell text, preserves display-only number bits,
   and cannot serve as current-query readiness or activate record formats.
   Runtime SQL/catalog/timing and analytical totals/Other are not browse assets.
   DXAN bytes are inert canonical provenance, not another executable Analysis. */
const Page *browse_preview_page(const BrowsePreview *preview);
const void *browse_preview_definition(const BrowsePreview *preview,size_t *bytes);
/* Inert selected current DXSV metadata closure, including allocator observation. */
const void *browse_preview_sources(const BrowsePreview *preview,size_t *bytes);
#endif
