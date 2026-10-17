#ifndef DUXCEL_REVIEW_CAPTURE_H
#define DUXCEL_REVIEW_CAPTURE_H
#include "review_registry.h"
#include "../ui/ui.h"
#include "../ui/textview.h"
#ifdef DUXCEL_BROWSER_HOST
#include "review_browser.h"
#endif
/* Native renderer/composer adapter. Core records remain platform-independent. */
typedef enum { DX_REPORT_IDLE,DX_REPORT_ARMED,DX_REPORT_PICK,DX_REPORT_NOTE,DX_REPORT_WRITING,DX_REPORT_SAVED,DX_REPORT_ERROR } DxReportPhase;
typedef struct {
    DxReviewRegistry *registry;
    DxReportPhase phase;
    DxReviewTarget target;
    DxReviewFrame frame;
    Image image;
    Texture2D texture;
    TextInput note;
    TextEditor editor;
    DxReviewPick mode;
    int focus,pick_index;
    Vector2 pick_point,drag_start; bool dragging,pending,keyboard_pick,toolbar_top;
    bool previous_keyboard_focus,writer_started,writer_done,writer_ok;
    DxThread writer;
    DxMutex mutex;
#ifdef DUXCEL_BROWSER_HOST
    DxReviewBrowser browser;
#endif
    char *json;
    char root[4096],report_id[96],last_report_id[96],error[512],writer_error[512];
    char trace[DX_REVIEW_EVENTS][96];
    unsigned trace_head,trace_count,serial,document_serial;
    double capture_ms,write_ms,writer_begin;
} DxReviewCapture;
DxReviewCapture *dx_review_capture_create(void);
void dx_review_capture_destroy(DxReviewCapture *capture);
void dx_review_capture_event(DxReviewCapture *capture,uint64_t frame,const char *kind,uint64_t detail);
bool dx_review_capture_request(DxReviewCapture *capture);
void dx_review_capture_begin(DxReviewCapture *capture,const DxReviewFrame *state);
bool dx_review_capture_pixels(DxReviewCapture *capture);
/* Returns true if the overlay closed; normal dispatch resumes next frame. */
bool dx_review_capture_draw(DxReviewCapture *capture,UIContext *ui);
void dx_review_capture_cancel(DxReviewCapture *capture);
bool dx_review_capture_save(DxReviewCapture *capture);
#endif
