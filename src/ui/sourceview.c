#include "sourceview.h"
bool ui_source_provenance(const Analysis *source,SourceViewState state,SourceProvenanceView *out) {
    if (!source || !out || !source->path || !*source->path ||
        (source->kind!=SOURCE_CSV && source->kind!=SOURCE_PARQUET && source->kind!=SOURCE_DATABASE) ||
        (unsigned)state>SOURCE_VIEW_UNAVAILABLE) return false;
    *out=(SourceProvenanceView){.definition=source,.kind=source->kind,.path=source->path,
        .schema=source->schema,.table=source->table,.query=source->query,
        .state=state,.external=true,.read_only=true};
    return true;
}
const char *ui_source_kind_label(SourceKind kind) {
    switch (kind) {
    case SOURCE_CSV: return "Delimited text";
    case SOURCE_PARQUET: return "Parquet";
    case SOURCE_DATABASE: return "DuckDB";
    default: return "No external connection";
    }
}
const char *ui_source_state_label(SourceViewState state) {
    switch (state) {
    case SOURCE_VIEW_LOADING: return "Loading";
    case SOURCE_VIEW_READY: return "Ready";
    case SOURCE_VIEW_UNAVAILABLE: return "Unavailable";
    default: return "Unavailable";
    }
}
