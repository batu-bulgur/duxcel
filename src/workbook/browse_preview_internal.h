#ifndef DX_BROWSE_PREVIEW_INTERNAL_H
#define DX_BROWSE_PREVIEW_INTERNAL_H
#include "browse_preview.h"
#include "analysis_asset.h"
struct BrowsePreview { BrowsePreviewInfo info; Page page; void *definition,*sources; };
typedef struct {
    DefinitionAssetInfo definition;
    SourceVersionsAssetInfo sources;
    size_t owned,validation_scratch;
} BrowsePreviewMetadataInfo;
BrowsePreviewStatus browse_preview_column_status(const Column *column);
/* Private shared child closure. Temporary outputs must be zero-initialized;
   success transfers independent temporary owners, failure preserves both. */
BrowsePreviewStatus browse_preview_metadata_preflight(const BrowsePreview *preview,BrowsePreviewMetadataInfo *out);
BrowsePreviewStatus browse_preview_metadata_decode(const BrowsePreview *preview,const BrowsePreviewMetadataInfo *info,
    size_t reserved,AnalysisAsset *definition,SourceVersionsAsset *sources);
#endif
