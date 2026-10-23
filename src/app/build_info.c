#include "build_info.h"
#ifndef DX_BUILD_REVISION
#define DX_BUILD_REVISION "Unavailable (source archive)"
#define DX_BUILD_SUBJECT "Revision information was not supplied by the build."
#define DX_BUILD_AUTHOR "Unavailable"
#define DX_BUILD_COMMIT_DATE "Unavailable"
#define DX_BUILD_TIME __DATE__ " " __TIME__
#define DX_BUILD_LOG "Change history is unavailable in this source archive."
#define DX_BUILD_SOURCE "Unavailable"
#define DX_BUILD_KNOWN 0
#define DX_BUILD_DIRTY 0
#endif

const BuildInfo *build_info(void) {
    static const BuildInfo info={"Development",DX_BUILD_REVISION,DX_BUILD_SUBJECT,
        DX_BUILD_AUTHOR,DX_BUILD_COMMIT_DATE,DX_BUILD_TIME,DX_BUILD_LOG,DX_BUILD_SOURCE,DX_BUILD_KNOWN,DX_BUILD_DIRTY};
    return &info;
}
