#include "dev/review_capture_layout.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL review capture:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void inside(Rectangle r,Rectangle bounds) {
    CHECK(isfinite(r.x) && isfinite(r.y) && isfinite(r.width) && isfinite(r.height));
    CHECK(r.width>0 && r.height>0);
    CHECK(r.x>=bounds.x-.001f && r.y>=bounds.y-.001f);
    CHECK(r.x+r.width<=bounds.x+bounds.width+.001f && r.y+r.height<=bounds.y+bounds.height+.001f);
}
int main(void) {
    const float widths[]={1,120,240,316,320,420,640,900,1440},heights[]={1,80,153,200,240,320,600,900},scales[]={.8f,1,1.5f,2};
    for (size_t i=0;i<sizeof(widths)/sizeof(*widths);i++) for (size_t j=0;j<sizeof(heights)/sizeof(*heights);j++) for (size_t k=0;k<sizeof(scales)/sizeof(*scales);k++) for (int top=0;top<2;top++) {
        float w=widths[i],h=heights[j]; DxReviewCaptureLayout l=dx_review_capture_layout(w,h,scales[k],top);
        Rectangle screen={0,0,w,h}; inside(l.panel,screen);
        const Rectangle elements[]={l.title,l.target,l.note,l.status,l.cancel,l.pick,l.save,l.done,l.saved_id,l.saved_detail};
        for (size_t r=0;r<sizeof(elements)/sizeof(*elements);r++) inside(elements[r],l.panel);
        inside(l.picker_bar,screen); inside(l.picker_label,l.picker_bar); inside(l.picker_mode,l.picker_bar); inside(l.picker_cancel,l.picker_bar);
        CHECK(l.scale>0 && l.scale<=scales[k]);
        CHECK(l.note.height>=37*l.scale-.001f);
        CHECK(l.title.y+l.title.height<=l.target.y+.001f && l.target.y+l.target.height<=l.note.y+.001f);
        CHECK(l.note.y+l.note.height<=l.status.y+.001f && l.status.y+l.status.height<=l.cancel.y+.001f);
        CHECK(l.cancel.x+l.cancel.width<l.pick.x && l.pick.x+l.pick.width<l.save.x);
        CHECK(l.picker_label.y+l.picker_label.height<=l.picker_mode.y+.001f || l.picker_label.x+l.picker_label.width<=l.picker_mode.x+.001f);
        CHECK(l.saved_id.y+l.saved_id.height<=l.saved_detail.y && l.saved_detail.y+l.saved_detail.height<l.done.y);
    }
    DxReviewCaptureLayout short_window=dx_review_capture_layout(640,200,1.5f,false);
    CHECK(short_window.compact && short_window.note.height>48 && short_window.note.height<49);
    CHECK(short_window.scale<1.5f && short_window.scale>1.3f);
    DxReviewCaptureLayout regular=dx_review_capture_layout(1440,900,1,false);
    CHECK(!regular.compact && regular.scale==1 && regular.panel.width==510 && regular.panel.height==348);
    CHECK(regular.note.height==185 && regular.save.width==100 && regular.pick.width==92 && regular.cancel.width==84);
    DxReviewCaptureLayout invalid=dx_review_capture_layout(NAN,-1,INFINITY,false);
    inside(invalid.panel,(Rectangle){0,0,1,1});
    printf("PASS: %u bounded review composer/picker layout checks\n",checks); return 0;
}
