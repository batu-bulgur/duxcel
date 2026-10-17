#ifndef DUXCEL_FILTERLABEL_H
#define DUXCEL_FILTERLABEL_H
#include "../transforms/analysis.h"
/* Owned presentation text for a view-filter pill. Build on definition changes,
   then retain it with the owning object ID/revision; never parse filter JSON in
   a paint loop. No pointers into Analysis/PivotDefinition survive this call.
   The bounded label is not an input to SQL, clipboard or persistence. */
typedef struct {
    char text[512];
    size_t value_count; /* Total selected/excluded keys, not just visible text. */
    bool valid,truncated;
    bool has_null,excluded; /* Mentions SQL NULL; list uses excluded keys. */
} UIFilterLabel;
void ui_filter_label_set(UIFilterLabel *out,const char *field,FilterOp op,const char *value);
#endif
