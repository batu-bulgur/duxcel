#ifndef DUXCEL_BUILD_INFO_H
#define DUXCEL_BUILD_INFO_H
#include <stdbool.h>

typedef struct {
    const char *version,*revision,*subject,*author,*commit_date,*built_at,*changelog,*source_id;
    bool revision_known,dirty;
    const char *lane,*checkout;
} BuildInfo;
/* Immutable compiled metadata. No Git executable or repository is required at
   runtime. Direct/source-archive compilation has an explicit unknown fallback. */
const BuildInfo *build_info(void);
#endif
