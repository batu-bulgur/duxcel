#include "review_browser.h"
#include <emscripten/emscripten.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static DxReviewBrowser *owner;
static unsigned sequence;
EM_JS(int, host_create, (char *session,size_t session_size,char *binary,size_t binary_size), {
    const host=Module['dxReview'];
    if(!host || host.closed || !new RegExp("^S-[A-Za-z0-9-]+$").test(host.sessionId) || host.sessionId.length>=session_size || !new RegExp("^[a-f0-9]{64}$").test(host.binarySHA256) || binary_size<65)return 0;
    stringToUTF8(host.sessionId,session,session_size);stringToUTF8(host.binarySHA256,binary,binary_size);return 1;
})
EM_JS(int, host_request, (void), {return Module['dxReview']?.takeRequest() ? 1 : 0;})
EM_JS(int, host_begin, (const char *report,const char *frame,unsigned width,unsigned height,char *error,size_t error_size), {
    try{Module['dxReview'].beginFrame({reportId:UTF8ToString(report),frameId:UTF8ToString(frame),logicalWidth:width,logicalHeight:height});return 1;}
    catch(failure){stringToUTF8(String(failure),error,error_size);return 0;}
})
EM_JS(void, host_export, (unsigned cookie,unsigned ticket,const char *report,const char *frame,const char *json,size_t json_size,const void *rgba,int width,int height), {
    cookie=cookie>>>0;ticket=ticket>>>0;
    const complete=(ok,error)=>{
        const message=_malloc(512);
        if(message)stringToUTF8(String(error==null ? '' : error),message,512);
        _dx_browser_review_result(cookie,ticket,Boolean(ok),message);if(message)_free(message);
    };
    try{
        Module['dxReview'].exportReport({reportId:UTF8ToString(report),frameId:UTF8ToString(frame),json:HEAPU8.subarray(json,json+json_size),rgba:HEAPU8.subarray(rgba,rgba+width*height*4),width,height,complete});
    }catch(failure){queueMicrotask(()=>complete(false,String(failure)));}
})
EM_JS(void, host_cancel, (void), {Module['dxReview']?.cancel();})
EM_JS(void, host_destroy, (void), {Module['dxReview']?.dispose();})

EMSCRIPTEN_KEEPALIVE void dx_browser_review_result(unsigned cookie,unsigned ticket,int ok,const char *error) {
    if (!owner || owner->cookie!=cookie || owner->ticket!=ticket || !owner->pending) return;
    owner->pending=false; owner->done=true; owner->ok=ok!=0;
    snprintf(owner->error,sizeof(owner->error),"%s",error && *error ? error : ok ? "" : "Could not prepare the report download. Your note is retained.");
}
bool dx_review_browser_create(DxReviewBrowser *browser,char *session,size_t session_size,char *binary,size_t binary_size) {
    if (owner || sequence==UINT32_MAX || !host_create(session,session_size,binary,binary_size)) return false;
    *browser=(DxReviewBrowser){.cookie=++sequence}; owner=browser; return true;
}
bool dx_review_browser_take_request(void) { return owner && host_request(); }
bool dx_review_browser_begin(DxReviewBrowser *browser,const DxReviewFrame *frame,const char *report_id) {
    char id[24]; snprintf(id,sizeof(id),"%" PRIu64,frame->frame_id);
    return owner==browser && host_begin(report_id,id,frame->window_width,frame->window_height,browser->error,sizeof(browser->error));
}
bool dx_review_browser_save(DxReviewBrowser *browser,const char *report_id,uint64_t frame_id,const char *json,size_t json_size,const void *rgba,int width,int height) {
    if (owner!=browser || browser->pending || sequence==UINT32_MAX || !json || !json_size || json_size>DX_REVIEW_JSON_LIMIT || !rgba || width<1 || height<1 || (uint64_t)width*(uint64_t)height>33554432u) {
        snprintf(browser->error,sizeof(browser->error),"Invalid or unavailable report export. Your note is retained."); return false;
    }
    char id[24]; snprintf(id,sizeof(id),"%" PRIu64,frame_id);
    browser->ticket=++sequence; browser->done=browser->ok=false; browser->pending=true; browser->error[0]=0;
    host_export(browser->cookie,browser->ticket,report_id,id,json,json_size,rgba,width,height); return true;
}
void dx_review_browser_cancel(DxReviewBrowser *browser) {
    if (owner!=browser) return;
    browser->pending=browser->done=false; browser->ticket=0; host_cancel();
}
void dx_review_browser_destroy(DxReviewBrowser *browser) {
    if (owner!=browser) return;
    owner=NULL; browser->pending=browser->done=false; browser->ticket=0; host_destroy();
}
