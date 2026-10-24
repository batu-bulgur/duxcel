#include "history_format.h"
#include "history_stream_internal.h"
#include <stdlib.h>
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static bool reserve_format(void *context,const WorkbookHistoryFormatRecord *r,void **ticket,char **error) {
    if (!context || !r || !ticket || *ticket || !r->sheet || !r->sheet_session || r->effects!=8)
        return fail(error,"Choose a complete format-only history record.");
    size_t bytes=0;
    if (format_delta_encoded_size(r->sheet,r->before,r->after,&bytes)!=FORMAT_DELTA_OK)
        return fail(error,"Choose a changed valid format payload before recording history.");
    FormatDependencies dependencies={0};
    FormatDependenciesStatus closed=format_dependencies_build(r->sheet,r->before,r->after,r->sources,
        r->object_high_water,DX_SOURCE_VERSIONS_BYTES,&dependencies);
    if (closed!=FORMAT_DEPENDENCIES_OK) return fail(error,"Format source dependencies are unavailable or inconsistent; the edit is kept.");
    size_t source_bytes=0;
    SourceVersionsCodecStatus measured=source_versions_asset_size(r->sheet,&dependencies.sources.versions,r->object_high_water,&source_bytes);
    WorkbookHistoryEditFence fence={r->workbook,r->edit_session,r->expected_revision,r->revision};
    bool ok=measured==SOURCE_VERSIONS_CODEC_OK && workbook_history_stream_reserve(context,fence,
        DX_HISTORY_FORMAT_FAMILY,DX_FORMAT_DELTA_ENCODING,bytes,DX_SOURCE_VERSIONS_ENCODING,source_bytes,
        dependencies.legacy_scopes,ticket,error);
    if (ok) {
        size_t written=0;
        ok=format_delta_encode(r->sheet,r->before,r->after,workbook_history_reservation_payload(*ticket),bytes,&written)==FORMAT_DELTA_OK && written==bytes;
        written=0;
        ok=ok && source_versions_asset_encode(r->sheet,&dependencies.sources.versions,r->object_high_water,
            workbook_history_reservation_dependencies(*ticket),source_bytes,&written)==SOURCE_VERSIONS_CODEC_OK && written==source_bytes;
        if (!ok) { workbook_history_stream_cancel(context,*ticket); *ticket=NULL; fail(error,"The format history payload changed before admission."); }
    }
    format_dependencies_free(&dependencies); return ok;
}
WorkbookHistoryFormatRecorder workbook_history_stream_format_recorder(WorkbookHistoryStream *stream) {
    return (WorkbookHistoryFormatRecorder){reserve_format,workbook_history_stream_publish,workbook_history_stream_cancel,stream};
}
