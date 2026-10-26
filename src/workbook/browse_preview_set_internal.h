#ifndef DX_BROWSE_PREVIEW_SET_INTERNAL_H
#define DX_BROWSE_PREVIEW_SET_INTERNAL_H
#include "browse_preview_set.h"
#include "../platform/platform.h"
typedef struct { BrowsePreviewManifestEntry manifest; BrowsePreview *asset; } BrowsePreviewSetEntry;
struct BrowsePreviewSet { BrowsePreviewSetInfo info; BrowsePreviewSetEntry *entries; size_t references; DxMutex ownership; };
#endif
