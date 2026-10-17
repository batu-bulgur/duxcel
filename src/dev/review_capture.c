#include "review_capture.h"
#ifndef DUXCEL_BROWSER_HOST
#include "review_store.h"
#endif
#include "review_input.h"
#include "review_build_info.h"
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef DUXCEL_BROWSER_HOST
#ifdef _WIN32
#include <process.h>
#define REVIEW_PID _getpid
#else
#include <unistd.h>
#define REVIEW_PID getpid
#endif
#endif
static void copy_text(char *out,size_t cap,const char *text) { snprintf(out,cap,"%s",text ? text : ""); }
static void release_pixels(DxReviewCapture *r) {
    if (r->texture.id) UnloadTexture(r->texture);
    if (r->image.data) UnloadImage(r->image);
    r->texture=(Texture2D){0}; r->image=(Image){0};
}
DxReviewCapture *dx_review_capture_create(void) {
    DxReviewCapture *r=calloc(1,sizeof(*r)); if (!r) return NULL;
    r->registry=calloc(1,sizeof(*r->registry)); if (!r->registry) { free(r); return NULL; }
    dx_mutex_init(&r->mutex);
#ifdef DUXCEL_BROWSER_HOST
    if (!dx_review_browser_create(&r->browser,r->frame.session_id,sizeof(r->frame.session_id),r->frame.binary_sha256,sizeof(r->frame.binary_sha256))) { dx_mutex_destroy(&r->mutex); free(r->registry); free(r); return NULL; }
#else
    snprintf(r->frame.session_id,sizeof(r->frame.session_id),"S-%llu-%ld",(unsigned long long)time(NULL),(long)REVIEW_PID());
#endif
    copy_text(r->frame.project_id,sizeof(r->frame.project_id),DX_REVIEW_BUILD_PROJECT);
    copy_text(r->frame.source_fingerprint,sizeof(r->frame.source_fingerprint),DX_REVIEW_BUILD_SOURCE);
    copy_text(r->frame.assets_fingerprint,sizeof(r->frame.assets_fingerprint),DX_REVIEW_BUILD_ASSETS);
    copy_text(r->frame.build_flags,sizeof(r->frame.build_flags),DX_REVIEW_BUILD_FLAGS);
#ifndef DUXCEL_BROWSER_HOST
    char path[4096]; snprintf(path,sizeof(path),"%sduxcel-review.sha256",GetApplicationDirectory());
    FILE *f=fopen(path,"rb");
    if (f) { char hash[66]={0}; if (fread(hash,1,65,f)==65 && hash[64]=='\n') { hash[64]=0; copy_text(r->frame.binary_sha256,sizeof(r->frame.binary_sha256),hash); } fclose(f); }
    dx_review_store_default_root(r->frame.project_id,r->root,sizeof(r->root),r->error,sizeof(r->error));
#endif
    return r;
}
void dx_review_capture_event(DxReviewCapture *r,uint64_t frame,const char *kind,uint64_t detail) {
    if (!r || r->phase!=DX_REPORT_IDLE) return;
    unsigned at=(r->trace_head+r->trace_count)%DX_REVIEW_EVENTS;
    snprintf(r->trace[at],sizeof(r->trace[at]),"frame=%llu %s=%llu",(unsigned long long)frame,kind,(unsigned long long)detail);
    if (r->trace_count<DX_REVIEW_EVENTS) r->trace_count++; else r->trace_head=(r->trace_head+1)%DX_REVIEW_EVENTS;
}
bool dx_review_capture_request(DxReviewCapture *r) {
    if (!r || r->phase!=DX_REPORT_IDLE) return false;
    r->previous_keyboard_focus=ui_focus_visible(); r->phase=DX_REPORT_ARMED; r->mode=DX_REVIEW_PICK_CONTROL;
    r->last_report_id[0]=0; r->target=(DxReviewTarget){0}; r->keyboard_pick=false; r->pick_index=-1; r->focus=0; r->error[0]=0;
    snprintf(r->report_id,sizeof(r->report_id),"DXR-%s-%u",r->frame.session_id,++r->serial);
    input_set(&r->note,""); return true;
}
void dx_review_capture_begin(DxReviewCapture *r,const DxReviewFrame *state) {
    DxReviewFrame frame=r->frame;
    frame.frame_id=state->frame_id; frame.timestamp_ns=state->timestamp_ns;
    frame.window_width=state->window_width; frame.window_height=state->window_height;
    frame.framebuffer_width=state->framebuffer_width; frame.framebuffer_height=state->framebuffer_height;
    frame.dpi_x=state->dpi_x; frame.dpi_y=state->dpi_y; frame.ui_scale=state->ui_scale; frame.grid_zoom=state->grid_zoom;
    copy_text(frame.document_id,sizeof(frame.document_id),state->document_id);
    copy_text(frame.state_text,sizeof(frame.state_text),state->state_text);
    copy_text(frame.fonts,sizeof(frame.fonts),state->fonts);
    size_t used=0;
    for (unsigned i=0;i<r->trace_count;i++) {
        const char *line=r->trace[(r->trace_head+i)%DX_REVIEW_EVENTS]; size_t n=strlen(line);
        if (used+n+2>=sizeof(frame.trace_text)) { frame.truncated=true; break; }
        memcpy(frame.trace_text+used,line,n); used+=n; frame.trace_text[used++]='\n';
    }
    frame.trace_text[used]=0; r->frame=frame;
    DxReviewStatus status=dx_review_begin(r->registry,&frame,true);
    if (status!=DX_REVIEW_OK) { copy_text(r->error,sizeof(r->error),dx_review_status_text(status)); r->phase=DX_REPORT_ERROR; }
#ifdef DUXCEL_BROWSER_HOST
    else if (!dx_review_browser_begin(&r->browser,&frame,r->report_id)) { copy_text(r->error,sizeof(r->error),r->browser.error); r->phase=DX_REPORT_ERROR; }
#endif
}
bool dx_review_capture_pixels(DxReviewCapture *r) {
    if (r->phase!=DX_REPORT_ARMED) return false;
    double begin=dx_millis();
    DxReviewStatus status=dx_review_seal(r->registry);
    if (status!=DX_REVIEW_OK) { copy_text(r->error,sizeof(r->error),dx_review_status_text(status)); r->phase=DX_REPORT_ERROR; return false; }
    release_pixels(r); rlDrawRenderBatchActive();
    if (!getenv("DUXCEL_REVIEW_TEST_FAIL_CAPTURE") && GetRenderWidth()>0 && GetRenderHeight()>0 && (size_t)GetRenderWidth()*GetRenderHeight()<=33554432u) r->image=LoadImageFromScreen();
    if (!r->image.data || r->image.width<1 || r->image.height<1 || (size_t)r->image.width*r->image.height>33554432u) {
        release_pixels(r); copy_text(r->error,sizeof(r->error),"Cannot capture this frame. Retry or cancel; the worksheet is unchanged."); r->phase=DX_REPORT_ERROR; return false;
    }
    r->texture=LoadTextureFromImage(r->image);
    if (!r->texture.id) { release_pixels(r); copy_text(r->error,sizeof(r->error),"Cannot display the captured frame. Retry or cancel."); r->phase=DX_REPORT_ERROR; return false; }
    SetTextureFilter(r->texture,TEXTURE_FILTER_BILINEAR);
    r->error[0]=0; r->phase=DX_REPORT_PICK; r->capture_ms=dx_millis()-begin;
#ifdef DUXCEL_BROWSER_HOST
    printf("Browser review captured: %s frame=%llu widgets=%zu\n",r->report_id,(unsigned long long)r->frame.frame_id,r->registry->count);
    printf("Browser review state: %s zoom=%.6f ui_scale=%.6f logical=%d,%d framebuffer=%d,%d\n",r->frame.state_text,r->frame.grid_zoom,r->frame.ui_scale,r->frame.window_width,r->frame.window_height,r->frame.framebuffer_width,r->frame.framebuffer_height);
#endif
    return true;
}
#ifndef DUXCEL_BROWSER_HOST
static void *write_report(void *context) {
    DxReviewCapture *r=context; int bytes=0;
    unsigned char *png=ExportImageToMemory(r->image,".png",&bytes); char error[512]={0};
    bool ok=png && bytes>0 && dx_review_store_write(r->root,r->report_id,r->json,png,(size_t)bytes,error,sizeof(error));
    if (png) MemFree(png);
    if (!ok && !error[0]) copy_text(error,sizeof(error),"PNG encoding failed; your note is retained.");
    dx_lock(&r->mutex); r->writer_ok=ok; copy_text(r->writer_error,sizeof(r->writer_error),error); r->writer_done=true; dx_unlock(&r->mutex);
    return NULL;
}
#endif
bool dx_review_capture_save(DxReviewCapture *r) {
    if (!r || r->writer_started || !r->image.data || !r->target.found || !r->note.length || r->note.length>=DX_REVIEW_NOTE_BYTES) return false;
#ifndef DUXCEL_BROWSER_HOST
    if (!dx_review_store_default_root(r->frame.project_id,r->root,sizeof(r->root),r->error,sizeof(r->error))) { r->phase=DX_REPORT_ERROR; return false; }
#endif
    DxReviewStatus status; size_t length=0;
    free(r->json); r->json=dx_review_json_alloc(r->registry,&r->target,r->report_id,r->note.text,&length,&status);
    if (!r->json) { copy_text(r->error,sizeof(r->error),dx_review_status_text(status)); r->phase=DX_REPORT_ERROR; return false; }
    r->writer_done=r->writer_ok=false; r->writer_error[0]=0; r->writer_begin=dx_millis();
#ifdef DUXCEL_BROWSER_HOST
    if (r->image.format!=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 || !dx_review_browser_save(&r->browser,r->report_id,r->frame.frame_id,r->json,length,r->image.data,r->image.width,r->image.height)) {
        copy_text(r->error,sizeof(r->error),r->browser.error[0] ? r->browser.error : "Unsupported captured pixel format."); r->phase=DX_REPORT_ERROR; return false;
    }
#else
    r->writer_started=dx_thread_start(&r->writer,write_report,r);
    if (!r->writer_started) { copy_text(r->error,sizeof(r->error),"Cannot start the report writer. Your note is retained."); r->phase=DX_REPORT_ERROR; return false; }
#endif
    r->phase=DX_REPORT_WRITING; return true;
}
static void poll_writer(DxReviewCapture *r) {
#ifdef DUXCEL_BROWSER_HOST
    if (!r->browser.done) return;
    r->browser.done=false; bool ok=r->browser.ok;
    r->write_ms=dx_millis()-r->writer_begin;
    r->phase=ok ? DX_REPORT_SAVED : DX_REPORT_ERROR; copy_text(r->error,sizeof(r->error),r->browser.error);
    if (ok) { copy_text(r->last_report_id,sizeof(r->last_report_id),r->report_id); printf("Browser review prepared: %s\n",r->report_id); }
#else
    if (!r->writer_started) return;
    dx_lock(&r->mutex); bool done=r->writer_done,ok=r->writer_ok;
    char error[512]; copy_text(error,sizeof(error),r->writer_error); dx_unlock(&r->mutex);
    if (!done) return;
    dx_thread_join(r->writer); r->writer_started=false; r->write_ms=dx_millis()-r->writer_begin;
    r->phase=ok ? DX_REPORT_SAVED : DX_REPORT_ERROR; copy_text(r->error,sizeof(r->error),error);
    if (ok) { copy_text(r->last_report_id,sizeof(r->last_report_id),r->report_id); printf("Native review saved: %s/%s\n",r->root,r->report_id); }
#endif
}
void dx_review_capture_cancel(DxReviewCapture *r) {
    if (!r) return;
#ifdef DUXCEL_BROWSER_HOST
    dx_review_browser_cancel(&r->browser);
#else
    if (r->writer_started) { dx_thread_join(r->writer); r->writer_started=false; }
#endif
    release_pixels(r); free(r->json); r->json=NULL; r->phase=DX_REPORT_IDLE; r->pending=false;
    dx_review_input_quarantine(false); ui_review_discard_input(); ui_review_restore_focus(r->previous_keyboard_focus);
}
void dx_review_capture_destroy(DxReviewCapture *r) {
    if (!r) return;
    dx_review_capture_cancel(r);
#ifdef DUXCEL_BROWSER_HOST
    dx_review_browser_destroy(&r->browser);
#endif
    input_free(&r->note); text_editor_free(&r->editor); free(r->registry); dx_mutex_destroy(&r->mutex); free(r);
}
static Rectangle fit_image(DxReviewCapture *r) {
    float x=0,y=0,w=GetScreenWidth(),h=GetScreenHeight(),aspect=(float)r->image.width/r->image.height;
    if (w/h>aspect) { float next=h*aspect; x=(w-next)*.5f; w=next; }
    else { float next=w/aspect; y=(h-next)*.5f; h=next; }
    return (Rectangle){x,y,w,h};
}
static Vector2 frame_point(DxReviewCapture *r,Rectangle display,Vector2 point) {
    return (Vector2){(point.x-display.x)*r->frame.window_width/display.width,(point.y-display.y)*r->frame.window_height/display.height};
}
bool dx_review_capture_draw(DxReviewCapture *r,UIContext *u) {
    poll_writer(r); u->pointer_blocked=false; float s=u->scale,w=GetScreenWidth(),h=GetScreenHeight();
    ClearBackground((Color){31,41,52,255}); Rectangle display={0};
    if (r->texture.id) { display=fit_image(r); DrawTexturePro(r->texture,(Rectangle){0,0,r->texture.width,r->texture.height},display,(Vector2){0},0,WHITE); }
    if (r->phase!=DX_REPORT_WRITING && ui_pressed(KEY_ESCAPE)) { dx_review_capture_cancel(r); return true; }
    if (r->phase==DX_REPORT_PICK) {
        if (ui_pressed(KEY_SPACE)) r->toolbar_top=!r->toolbar_top;
        Rectangle bar={8*s,r->toolbar_top ? 8*s : h-52*s,w-16*s,44*s}; DrawRectangleRec(bar,(Color){238,244,248,255}); DrawRectangleLinesEx(bar,s,UI_BORDER);
        ui_label(u,"Pick · drag region · Space moves bar",(Rectangle){bar.x+10*s,bar.y,bar.width-250*s,bar.height},12,UI_TEXT,false,UI_MEDIUM);
        if (ui_button(u,(Rectangle){w-228*s,bar.y+7*s,95*s,30*s},r->mode==DX_REVIEW_PICK_CONTAINER ? "Containers" : "Controls",false,true)) { r->mode=r->mode==DX_REVIEW_PICK_CONTAINER ? DX_REVIEW_PICK_CONTROL : DX_REVIEW_PICK_CONTAINER; r->target=(DxReviewTarget){0}; r->keyboard_pick=false; r->pick_index=-1; }
        if (ui_button(u,(Rectangle){w-124*s,bar.y+7*s,100*s,30*s},"Cancel",false,true)) { dx_review_capture_cancel(r); return true; }
        Vector2 point=frame_point(r,display,GetMousePosition()); DxReviewTarget hover;
        bool hovered=dx_review_pick(r->registry,point.x,point.y,r->mode,&hover);
        if (r->keyboard_pick && r->target.found) { hover=r->target; hovered=true; }
        if (hovered) {
            Rectangle b={hover.bounds.x,hover.bounds.y,hover.bounds.width,hover.bounds.height}; b.x=display.x+b.x*display.width/r->frame.window_width; b.y=display.y+b.y*display.height/r->frame.window_height;
            b.width*=display.width/r->frame.window_width; b.height*=display.height/r->frame.window_height;
            DrawRectangleLinesEx(b,2*s,UI_FOCUS); if (CheckCollisionPointRec(GetMousePosition(),display)) u->cursor=MOUSE_CURSOR_CROSSHAIR;
        }
        DxPointerGesture gesture; Rectangle pick_area=display;
        if (r->toolbar_top) { float bottom=pick_area.y+pick_area.height; pick_area.y=fmaxf(pick_area.y,bar.y+bar.height); pick_area.height=fmaxf(0,bottom-pick_area.y); }
        else pick_area.height=fmaxf(0,fminf(pick_area.height,bar.y-pick_area.y));
        if (ui_take_left_gesture_from(pick_area,&gesture)) {
            Vector2 first=frame_point(r,display,(Vector2){gesture.x0,gesture.y0}),last=frame_point(r,display,(Vector2){gesture.x1,gesture.y1});
            if (fabsf(last.x-first.x)+fabsf(last.y-first.y)>6) dx_review_region(r->registry,(DxReviewRect){fminf(first.x,last.x),fminf(first.y,last.y),fmaxf(1,fabsf(last.x-first.x)),fmaxf(1,fabsf(last.y-first.y))},&r->target);
            else dx_review_pick(r->registry,first.x,first.y,r->mode,&r->target);
            r->keyboard_pick=false; r->pick_point=first;
        } else {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(),pick_area)) { r->drag_start=point; r->dragging=true; }
            if (r->dragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                Vector2 first=r->drag_start; r->keyboard_pick=false; r->pick_point=first; r->dragging=false;
                if (fabsf(point.x-first.x)+fabsf(point.y-first.y)>6) dx_review_region(r->registry,(DxReviewRect){fminf(first.x,point.x),fminf(first.y,point.y),fmaxf(1,fabsf(point.x-first.x)),fmaxf(1,fabsf(point.y-first.y))},&r->target);
                else dx_review_pick(r->registry,first.x,first.y,r->mode,&r->target);
            }
        }
        if (ui_pressed(KEY_TAB) && r->registry->count) {
            if (r->pick_index<0 && ui_key_shift(KEY_TAB)) r->pick_index=0;
            for (size_t i=0;i<r->registry->count;i++) {
                r->pick_index=(r->pick_index+(int)r->registry->count+(ui_key_shift(KEY_TAB) ? -1 : 1))%(int)r->registry->count;
                DxReviewWidget *widget=&r->registry->widgets[r->pick_index];
                float left=fmaxf(0,fmaxf(widget->bounds.x,widget->clip.x)),right=fminf(r->frame.window_width,fminf(widget->bounds.x+widget->bounds.width,widget->clip.x+widget->clip.width)); float x=(left+right)*.5f;
                float top=fmaxf(0,fmaxf(widget->bounds.y,widget->clip.y)),bottom=fminf(r->frame.window_height,fminf(widget->bounds.y+widget->bounds.height,widget->clip.y+widget->clip.height)); float y=(top+bottom)*.5f;
                DxReviewTarget candidate;
                if (dx_review_pick(r->registry,x,y,r->mode,&candidate) && !strcmp(candidate.widget.id,widget->id)) { r->keyboard_pick=true; r->target=candidate; r->pick_point=(Vector2){x,y}; break; }
            }
        }
        if (r->target.found && (!r->keyboard_pick || ui_pressed(KEY_ENTER))) {
            r->phase=DX_REPORT_NOTE; r->focus=0;
            printf("Review picked: %s\n",r->target.region ? "region" : r->target.widget.id);
        }
        return false;
    }
    float width=fminf(510*s,w-24*s),height=fminf(348*s,h-24*s),x=(w-width)*.5f,y=(h-height)*.5f;
    Rectangle panel={x,y,width,height}; DrawRectangle(0,0,(int)w,(int)h,(Color){20,31,46,90});
    DrawRectangleRounded(panel,.025f,4,UI_SURFACE); DrawRectangleLinesEx(panel,s,UI_BORDER);
#ifdef DUXCEL_BROWSER_HOST
    const char *saved_title="UI issue saved",*saved_detail=r->browser.error[0] ? r->browser.error : "Report export prepared.";
#else
    const char *saved_title="UI issue saved",*saved_detail="Saved locally; an agent can read and reply later.";
#endif
    ui_label(u,r->phase==DX_REPORT_SAVED ? saved_title : "Report UI issue",(Rectangle){x+16*s,y+8*s,width-32*s,30*s},16,UI_TEXT,false,UI_MEDIUM);
    const char *target=r->target.region ? "Selected region" : r->target.found ? r->target.widget.id : "Capture unavailable";
    ui_label(u,target,(Rectangle){x+16*s,y+43*s,width-32*s,26*s},11,UI_MUTED,false,UI_REGULAR);
    if (r->phase==DX_REPORT_SAVED) {
        ui_label(u,r->report_id,(Rectangle){x+16*s,y+92*s,width-32*s,42*s},13,UI_ACCENT,false,UI_MEDIUM);
        ui_label(u,saved_detail,(Rectangle){x+16*s,y+140*s,width-32*s,40*s},12,UI_TEXT,false,UI_REGULAR);
        if (ui_button(u,(Rectangle){x+width-116*s,y+height-46*s,100*s,30*s},"Done",true,true) || ui_pressed(KEY_ENTER)) { dx_review_capture_cancel(r); return true; }
        return false;
    }
    Rectangle note={x+16*s,y+78*s,width-32*s,height-163*s}; r->note.multiline=true;
    if (ui_pressed(KEY_TAB)) r->focus=(r->focus+(ui_key_shift(KEY_TAB) ? 3 : 1))%4;
    if (CheckCollisionPointRec(ui_mouse_position(),note) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) r->focus=0;
    bool editable=r->phase==DX_REPORT_NOTE || r->phase==DX_REPORT_ERROR;
    if (!editable) dx_review_input_quarantine(true);
    bool activated=text_editor_draw(&r->editor,&r->note,u,note,r->focus==0 && editable);
    if (!editable) dx_review_input_quarantine(false);
    if (activated && editable) r->focus=0;
    ui_label(u,r->note.length>=DX_REVIEW_NOTE_BYTES ? "Note limit: 4095 UTF-8 bytes. Shorten the note to save." : r->error[0] ? r->error : r->phase==DX_REPORT_WRITING ? "Saving image and report…" : "Describe what happened and what you expected. ⌘Enter to save.",(Rectangle){x+16*s,y+height-80*s,width-32*s,29*s},11,r->error[0] ? MAROON : UI_MUTED,false,UI_REGULAR);
    bool available=r->phase!=DX_REPORT_WRITING;
    if (ui_focus_visible() && r->focus>0) { float bx=r->focus==1 ? x+width-116*s : r->focus==2 ? x+108*s : x+16*s; DrawRectangleLinesEx((Rectangle){bx-2*s,y+height-47*s,r->focus==3 ? 88*s : r->focus==2 ? 96*s : 104*s,34*s},s,UI_FOCUS); }
    if (ui_button(u,(Rectangle){x+16*s,y+height-45*s,84*s,30*s},"Cancel",false,available) || (available && r->focus==3 && ui_pressed(KEY_ENTER) && !ui_key_shortcut(KEY_ENTER))) { dx_review_capture_cancel(r); return true; }
    if (ui_button(u,(Rectangle){x+108*s,y+height-45*s,92*s,30*s},r->image.data ? "Pick again" : "Retry capture",false,available) || (available && r->focus==2 && ui_pressed(KEY_ENTER) && !ui_key_shortcut(KEY_ENTER))) { r->target=(DxReviewTarget){0}; r->phase=r->image.data ? DX_REPORT_PICK : DX_REPORT_ARMED; }
    if (ui_button(u,(Rectangle){x+width-116*s,y+height-45*s,100*s,30*s},r->phase==DX_REPORT_ERROR ? "Retry save" : "Save report",true,available && r->image.data && r->target.found && r->note.length>0 && r->note.length<DX_REVIEW_NOTE_BYTES) ||
        (available && ui_pressed(KEY_ENTER) && (ui_key_shortcut(KEY_ENTER) || r->focus==1))) dx_review_capture_save(r);
    return false;
}
