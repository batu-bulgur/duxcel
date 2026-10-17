#ifndef DUXCEL_REVIEW_CAPTURE_LAYOUT_H
#define DUXCEL_REVIEW_CAPTURE_LAYOUT_H
#include <raylib.h>
#include <math.h>
#include <stdbool.h>

/* Review-only presentation geometry. The captured frame retains its original
   interface scale even when a small overlay needs a compact presentation. */
typedef struct {
    float scale;
    bool compact,stacked_picker;
    Rectangle panel,title,target,note,status,cancel,pick,save,done,saved_id,saved_detail;
    Rectangle picker_bar,picker_label,picker_mode,picker_cancel;
} DxReviewCaptureLayout;

static inline DxReviewCaptureLayout dx_review_capture_layout(float width,float height,float scale,bool toolbar_top) {
    width=isfinite(width) && width>0 ? width : 1;
    height=isfinite(height) && height>0 ? height : 1;
    scale=isfinite(scale) && scale>0 ? scale : 1;
    /* 37 units leave one complete text-editor line plus its 16-unit padding.
       All three labeled actions and target/status rows remain available. */
    float s=fminf(scale,fminf(width/316,height/153));
    bool compact=width<400*s || height<300*s;
    float outer=(compact ? 4 : 12)*s,pad=(compact ? 8 : 16)*s;
    float w=fminf(510*s,width-2*outer),h=fminf(348*s,height-2*outer);
    float x=(width-w)*.5f,y=(height-h)*.5f;
    DxReviewCaptureLayout l={.scale=s,.compact=compact,.panel={x,y,w,h}};
    float left=x+pad,content=w-2*pad;
    l.title=(Rectangle){left,y+8*s,content,(compact ? 20 : 30)*s};
    l.target=(Rectangle){left,y+(compact ? 28 : 43)*s,content,(compact ? 16 : 26)*s};
    l.note=(Rectangle){left,y+(compact ? 48 : 78)*s,content,h-(compact ? 108 : 163)*s};
    l.status=(Rectangle){left,y+h-(compact ? 56 : 80)*s,content,(compact ? 16 : 29)*s};
    float buttons=y+h-(compact ? 36 : 45)*s,bh=(compact ? 28 : 30)*s;
    l.cancel=(Rectangle){left,buttons,84*s,bh};
    l.pick=(Rectangle){left+92*s,buttons,92*s,bh};
    l.save=(Rectangle){x+w-pad-100*s,buttons,100*s,bh};
    l.done=l.save; if (!compact) l.done.y-=s;
    l.saved_id=(Rectangle){left,compact ? l.note.y : y+92*s,content,(compact ? 20 : 42)*s};
    l.saved_detail=(Rectangle){left,compact ? l.saved_id.y+l.saved_id.height+4*s : y+140*s,content,compact ? l.status.y+l.status.height-l.saved_id.y-l.saved_id.height-4*s : 40*s};
    l.stacked_picker=width<480*s;
    float bar_height=(l.stacked_picker ? 72 : 44)*s;
    l.picker_bar=(Rectangle){8*s,toolbar_top ? 8*s : height-8*s-bar_height,width-16*s,bar_height};
    float py=l.picker_bar.y+(l.stacked_picker ? 35 : 7)*s;
    l.picker_mode=(Rectangle){width-228*s,py,95*s,30*s};
    l.picker_cancel=(Rectangle){width-124*s,py,100*s,30*s};
    l.picker_label=(Rectangle){l.picker_bar.x+10*s,l.picker_bar.y,l.picker_bar.width-(l.stacked_picker ? 20 : 250)*s,(l.stacked_picker ? 30 : 44)*s};
    return l;
}
#endif
