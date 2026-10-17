#ifndef DX_STEP_PREVIEW_H
#define DX_STEP_PREVIEW_H
#include "../ui/preview.h"
#include "../ui/stepview.h"
#include "../data/worker.h"
typedef enum { PREVIEW_STATS_IDLE,PREVIEW_STATS_WAITING,PREVIEW_STATS_LOADING,PREVIEW_STATS_READY,PREVIEW_STATS_FAILED,PREVIEW_STATS_UNAVAILABLE } PreviewStatsState;
typedef struct {
    Analysis source;
    Page page,spare;
    Grid grid;
    char *title,*error;
    char **width_names;
    float *width_values;
    size_t width_count;
    FormatStore formats;
    SelectionStats statistics;
    GridRegion stats_bounds;
    char *stats_error;
    uint64_t stats_request;
    double stats_changed;
    PreviewStatsState stats_state;
    uint64_t request,source_id,source_revision;
    size_t step;
    bool active,loading,initialized;
} StepPreview;
void step_preview_free(StepPreview *preview);
/* Build first, then replace an existing preview. The live source is borrowed
   only during this call; jobs and retained definitions own separate copies. */
bool step_preview_start(StepPreview *preview,const Analysis *source,size_t step,
    uint64_t source_id,uint64_t source_revision,const Grid *presentation);
/* Copy widths and whole-column styles by bound source fields. Row/range
   formatting cannot safely follow a historical filter/sort prefix. */
void step_preview_presentation(StepPreview *preview,const Page *fields,const Grid *grid,const FormatStore *formats,size_t first_column);
QueryJob *step_preview_job(StepPreview *preview,uint64_t request,bool force);
/* Consumes the page/error only for the matching preview request; the caller
   still frees the reply. Unrelated and superseded replies remain untouched. */
bool step_preview_reply(StepPreview *preview,QueryReply *reply);
/* Selection identity excludes viewport/zoom/column width. Returns true when
   a pending statistics job belongs to a superseded selection. */
bool step_preview_watch_statistics(StepPreview *preview,bool requested);
QueryJob *step_preview_statistics_job(StepPreview *preview,uint64_t request);
bool step_preview_statistics_reply(StepPreview *preview,QueryReply *reply);
#endif
