#ifndef DX_STEP_VIEW_H
#define DX_STEP_VIEW_H
#include "../transforms/analysis.h"
#define DX_UI_STEPS (3+DX_ANALYSIS_FILTERS+DX_ANALYSIS_SORTS)
typedef enum { UI_STEP_SOURCE,UI_STEP_UNIQUE,UI_STEP_FILTER,UI_STEP_GROUP,UI_STEP_SORT } UIStepKind;
typedef struct { UIStepKind kind; size_t index; const char *column; char title[320],detail[512]; } UIStep;
/* Bounded presentation snapshot. The column pointer is borrowed from source;
   text is inline. Call on a definition change, not on each rendered frame. */
size_t ui_source_steps(const Analysis *source,size_t fields,UIStep steps[DX_UI_STEPS]);
/* Optional presentation filename, borrowed only during this call. NULL keeps
   the native basename fallback; supplied text is not parsed as a path. Titles
   and details keep their existing bounded, single-line rendering. This does
   not validate a filename or alter source identity, SQL or owned definitions. */
size_t ui_source_steps_named(const Analysis *source,size_t fields,const char *display_filename,UIStep steps[DX_UI_STEPS]);
/* Owned preview definition through the selected step. Failure preserves out;
   successful replacement never mutates the live source or its history. */
bool ui_step_source(const Analysis *source,size_t index,Analysis *out);
#endif
