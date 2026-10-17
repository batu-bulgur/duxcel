#include "dev/review_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static int compare(const void *a,const void *b) { double x=*(const double *)a,y=*(const double *)b; return (x>y)-(x<y); }
int main(void) {
    DxReviewRegistry *r=calloc(1,sizeof(*r)); double samples[500]; size_t bytes=0;
    if (!r) return 1;
    DxReviewFrame f={.frame_id=1,.window_width=1280,.window_height=820,.framebuffer_width=2560,.framebuffer_height=1640,.dpi_x=2,.dpi_y=2,.ui_scale=1,.grid_zoom=1,.session_id="bench-session",.project_id="bench-project"};
    for (int pass=0;pass<500;pass++) {
        clock_t begin=clock(); f.frame_id++;
        if (dx_review_begin(r,&f,true)!=DX_REVIEW_OK) return 2;
        for (int i=0;i<80;i++) {
            char id[64]; snprintf(id,sizeof(id),"filter.value.%d",i);
            DxReviewWidget w; dx_review_widget_init(&w,id,"filter.toggle_value","popup/column-filter","checkbox","Native-like visible value");
            w.bounds=(DxReviewRect){10,(float)i*8,220,25}; w.clip=(DxReviewRect){0,0,1280,820};
            dx_review_value_set(&w.value,"DECIMAL(38,18)",false,"9007199254740993.123456789012345678");
            if (dx_review_add(r,&w)!=DX_REVIEW_OK) return 3;
        }
        if (dx_review_seal(r)!=DX_REVIEW_OK) return 4;
        DxReviewTarget target; dx_review_pick(r,50,50,DX_REVIEW_PICK_CONTROL,&target);
        DxReviewStatus status; char *json=dx_review_json_alloc(r,&target,"bench-report","CPU registry/serialization only",&bytes,&status);
        if (!json || status!=DX_REVIEW_OK) return 5;
        free(json); samples[pass]=(double)(clock()-begin)*1000/CLOCKS_PER_SEC;
    }
    qsort(samples,500,sizeof(*samples),compare);
    printf("{\"scope\":\"optimized C99 registry+JSON CPU microbenchmark; no renderer, database or frame-latency claim\",\"samples\":500,\"widgets\":80,\"registry_bytes\":%zu,\"json_bytes\":%zu,\"median_ms\":%.3f,\"p95_ms\":%.3f,\"max_ms\":%.3f}\n",sizeof(*r),bytes,samples[249],samples[474],samples[499]);
    free(r); return 0;
}
