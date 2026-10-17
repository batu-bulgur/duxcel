#ifndef DUXCEL_REVIEW_BROWSER_H
#define DUXCEL_REVIEW_BROWSER_H
#include "review_registry.h"
/* Review-only single-threaded host transport. No borrowed pixels/JSON survive
   export submission; the host copies them before returning to C. */
typedef struct {
    unsigned cookie,ticket;
    bool pending,done,ok;
    char error[512];
} DxReviewBrowser;
bool dx_review_browser_create(DxReviewBrowser *browser,char *session,size_t session_size,char *binary,size_t binary_size);
bool dx_review_browser_take_request(void);
bool dx_review_browser_begin(DxReviewBrowser *browser,const DxReviewFrame *frame,const char *report_id);
bool dx_review_browser_save(DxReviewBrowser *browser,const char *report_id,uint64_t frame_id,const char *json,size_t json_size,const void *rgba,int width,int height);
void dx_review_browser_cancel(DxReviewBrowser *browser);
void dx_review_browser_destroy(DxReviewBrowser *browser);
#endif
