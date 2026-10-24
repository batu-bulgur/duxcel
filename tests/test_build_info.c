#include "app/build_info.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    /* Direct C compilation deliberately receives no generated Git header. */
    const BuildInfo *info=build_info();
    assert(info && info==build_info());
    assert(!info->revision_known && !info->dirty);
    assert(strstr(info->revision,"source archive"));
    assert(*info->version && *info->subject && *info->author && *info->commit_date);
    assert(info->lane && !*info->lane && info->checkout && !*info->checkout);
    assert(*info->built_at && strstr(info->changelog,"unavailable"));
    puts("PASS: source-archive build metadata without Git or generated input");
    return 0;
}
