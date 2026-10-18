#include "ui/shadowview.h"
#include "shadow_reference.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { fprintf(stderr,"FAIL shadowview:%d: %s\n",__LINE__,#condition); exit(1); } } while (0)
static bool same(Rectangle a,Rectangle b) {
    return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height;
}
static size_t pixel(const UIShadowLayout *q,int x,int y) {
    return ((size_t)y*(size_t)q->atlas_width+(size_t)x)*4;
}
static void reference_geometry(void) {
    /* Recorded wrapper edges from the same frozen captures as the samples. */
    static const struct {
        float scale; Rectangle wrapper,top,body,bottom; int width,corner,table; size_t bytes;
    } cases[]={
        {1,{360,93,280,605},{334,93,26,21},{334,114,26,563},{334,677,26,21},52,42,256,17680},
        {1.5f,{220,139,420,468},{181,139,39,31.5f},{181,170.5f,39,405},{181,575.5f,39,31.5f},78,63,256,39624}
    };
    for (size_t i=0;i<sizeof cases/sizeof *cases;i++) {
        UIShadowLayout q=ui_shadow_layout(cases[i].scale,(Vector2){2,2});
        CHECK(q.valid && q.width==cases[i].width && q.corner_rows==cases[i].corner);
        CHECK(q.table_width_x==cases[i].table && q.table_width_y==cases[i].table);
        CHECK(q.bytes==cases[i].bytes);
        UIShadowPlacement p=ui_shadow_placement(&q,cases[i].wrapper);
        CHECK(p.valid); CHECK(same(p.top.bounds,cases[i].top)); CHECK(same(p.body.bounds,cases[i].body)); CHECK(same(p.bottom.bounds,cases[i].bottom));
        CHECK(same(p.top.source,q.top)); CHECK(same(p.body.source,q.body)); CHECK(same(p.bottom.source,q.bottom));
        Rectangle shifted=cases[i].wrapper; shifted.x+=137.25f; shifted.y-=12.5f; shifted.height+=225.5f; shifted.width+=100;
        UIShadowPlacement moved=ui_shadow_placement(&q,shifted);
        CHECK(moved.valid && same(moved.top.source,p.top.source) && same(moved.body.source,p.body.source) && same(moved.bottom.source,p.bottom.source));
        CHECK(moved.top.bounds.x==p.top.bounds.x+137.25f && moved.top.bounds.y==p.top.bounds.y-12.5f);
        CHECK(moved.body.bounds.height==p.body.bounds.height+225.5f && moved.bottom.bounds.y==p.bottom.bounds.y-12.5f+225.5f);
        /* No new mask key/data for moving, scrolling, height or width changes. */
        CHECK(q.bytes==cases[i].bytes && q.width==cases[i].width);
    }
}
static void density_masks(void) {
    const float scales[]={.8f,1,1.5f};
    const Vector2 densities[]={{.5f,.5f},{1,1},{1.5f,1.5f},{2,2},{3,3},{4,4},{8,8},{1,2},{2,1},{1.5f,2.5f}};
    for (size_t si=0;si<sizeof scales/sizeof *scales;si++) for (size_t di=0;di<sizeof densities/sizeof *densities;di++) {
        UIShadowLayout q=ui_shadow_layout(scales[si],densities[di]); CHECK(q.valid && q.bytes<=UI_SHADOW_MASK_LIMIT);
        CHECK(q.atlas_width==q.width && q.atlas_height==2*q.corner_rows+1);
        CHECK(q.bytes==(size_t)q.atlas_width*(size_t)q.atlas_height*4);
        unsigned char *memory=malloc(q.bytes+32),*copy=malloc(q.bytes); CHECK(memory && copy);
        memset(memory,0xa5,q.bytes+32);
        CHECK(ui_shadow_mask_rgba(&q,memory+16,q.bytes)); memcpy(copy,memory+16,q.bytes);
        for (int guard=0;guard<16;guard++) CHECK(memory[guard]==0xa5 && memory[16+q.bytes+guard]==0xa5);
        CHECK(ui_shadow_mask_rgba(&q,memory+16,q.bytes) && !memcmp(copy,memory+16,q.bytes));
        CHECK(!ui_shadow_mask_rgba(&q,memory+16,q.bytes-1) && !memcmp(copy,memory+16,q.bytes));
        UIShadowLayout edited=q; edited.bytes++; CHECK(!ui_shadow_mask_rgba(&edited,memory+16,q.bytes+1));
        edited=q; edited.bottom.x++; CHECK(!ui_shadow_mask_rgba(&edited,memory+16,q.bytes));
        edited=q; edited.table_width_x*=2; CHECK(!ui_shadow_mask_rgba(&edited,memory+16,q.bytes));
        CHECK(!memcmp(copy,memory+16,q.bytes));
        for (int y=0;y<q.atlas_height;y++) for (int x=0;x<q.width;x++) {
            size_t at=pixel(&q,x,y),mirror=pixel(&q,x,q.atlas_height-1-y),body=pixel(&q,x,q.corner_rows);
            CHECK(!memcmp(memory+16+at,memory+16+mirror,4));
            CHECK(memory[16+at]<=memory[16+at+3] && memory[16+at+1]<=memory[16+at+3] && memory[16+at+2]<=memory[16+at+3]);
            CHECK(memory[16+at+3]<=memory[16+body+3] && memory[16+at+3]<=33);
            if (y>0 && y<q.corner_rows) CHECK(memory[16+at+3]>=memory[16+pixel(&q,x,y-1)+3]);
            if (x>0) CHECK(memory[16+at+3]>=memory[16+pixel(&q,x-1,y)+3]);
        }
        CHECK(memory[16+pixel(&q,q.width-1,0)+3]<memory[16+pixel(&q,q.width-1,q.corner_rows)+3]);
        UIShadowPlacement p=ui_shadow_placement(&q,(Rectangle){113.25f,47.5f,280*q.scale,600*q.scale}); CHECK(p.valid);
        CHECK(p.top.bounds.x+p.top.bounds.width==113.25f && p.top.bounds.y==47.5f);
        CHECK(fabsf(p.bottom.bounds.y+p.bottom.bounds.height-(47.5f+600*q.scale))<.001f);
        CHECK(p.top.bounds.y+p.top.bounds.height==p.body.bounds.y);
        CHECK(fabsf(p.body.bounds.y+p.body.bounds.height-p.bottom.bounds.y)<=4*FLT_EPSILON*fabsf(p.bottom.bounds.y));
        free(copy); free(memory);
    }
}
static void invalid_inputs(void) {
    const float invalid[]={0,-1,NAN,INFINITY,-INFINITY,FLT_MAX};
    for (size_t i=0;i<sizeof invalid/sizeof *invalid;i++) {
        CHECK(!ui_shadow_layout(invalid[i],(Vector2){2,2}).valid);
        CHECK(!ui_shadow_layout(1,(Vector2){invalid[i],2}).valid);
        CHECK(!ui_shadow_layout(1,(Vector2){2,invalid[i]}).valid);
    }
    CHECK(!ui_shadow_layout(8,(Vector2){8,8}).valid); /* Fixed byte cap. */
    CHECK(!ui_shadow_layout(1,(Vector2){FLT_MIN,2}).valid);
    CHECK(!ui_shadow_layout(1,(Vector2){2,FLT_MIN}).valid);
    CHECK(!ui_shadow_mask_rgba(NULL,NULL,0));
    UIShadowLayout q=ui_shadow_layout(1,(Vector2){2,2}); unsigned char guard[16]; memset(guard,0x5a,sizeof guard);
    CHECK(!ui_shadow_mask_rgba(&q,guard,sizeof guard));
    for (size_t i=0;i<sizeof guard;i++) CHECK(guard[i]==0x5a);
    CHECK(!ui_shadow_mask_rgba(&q,NULL,q.bytes));
    CHECK(!ui_shadow_placement(NULL,(Rectangle){0,0,280,600}).valid);
    const Rectangle bad[]={{NAN,0,280,600},{0,INFINITY,280,600},{0,0,-1,600},{0,0,280,-1},{0,0,40,600},{0,0,280,41},{0,0,280,NAN},{0,0,INFINITY,600}};
    for (size_t i=0;i<sizeof bad/sizeof *bad;i++) CHECK(!ui_shadow_placement(&q,bad[i]).valid);
    UIShadowPlacement exact=ui_shadow_placement(&q,(Rectangle){0,0,42,42}); CHECK(exact.valid && exact.body.bounds.height==0);
    UIShadowLayout fractional=ui_shadow_layout(1,(Vector2){1.5f,1.5f});
    /* Float placement cannot represent two adjacent corners at this extreme
       origin without overlap; fail closed rather than publish negative body. */
    CHECK(!ui_shadow_placement(&fractional,(Rectangle){0,-32768,280,2*(fractional.corner_rows/fractional.density.y)}).valid);
}
static void reference_pixels(void) {
    const struct { const ShadowReferencePixel *pixels; size_t count; float scale; int left,first; } refs[]={
        {narrow_100,sizeof narrow_100/sizeof *narrow_100,1,720,624},
        {narrow_150,sizeof narrow_150/sizeof *narrow_150,1.5f,440,296}
    };
    size_t matches=0,differences=0,points=0;
    for (size_t r=0;r<sizeof refs/sizeof *refs;r++) {
        UIShadowLayout q=ui_shadow_layout(refs[r].scale,(Vector2){2,2}); CHECK(q.valid);
        unsigned char *mask=malloc(q.bytes); CHECK(mask && ui_shadow_mask_rgba(&q,mask,q.bytes));
        for (size_t i=0;i<refs[r].count;i++) {
            int x=refs[r].first+(int)i-refs[r].left+q.width;
            unsigned char zero[4]={0},composited[4],*p=x>=0 && x<q.width ? mask+pixel(&q,x,q.corner_rows) : zero;
            const ShadowReferencePixel *ref=&refs[r].pixels[i];
            for (int c=0;c<3;c++) composited[c]=(unsigned char)(p[c]+((unsigned)ref->background[c]*(255-p[3])+127)/255);
            composited[3]=255; points++;
            if (!memcmp(composited,ref->foreground,4)) { matches++; continue; }
            differences++;
            /* Preserve the actual disagreement, not a fitted correction or
               tolerance: source-table double/RGBA8 model has green240 here;
               the recorded GPU has green241. GPU interpolation/blending at
               this threshold is a separate integration acceptance requirement. */
            CHECK(r==0 && i==88 && refs[r].first+(int)i==712);
            CHECK(!memcmp(composited,(unsigned char[]){239,240,242,255},4));
            CHECK(!memcmp(ref->foreground,(unsigned char[]){239,241,242,255},4));
        }
        free(mask);
    }
    CHECK(points==240 && matches==239 && differences==1);
    printf("Reference: %zu/%zu exact source-profile pixels; one explicit GPU rounding disagreement retained (no tolerance).\n",matches,points);
}
int main(void) {
    reference_geometry(); density_masks(); invalid_inputs(); reference_pixels();
    printf("PASS: %u pure shadowview checks; no GPU/render/OS parity claim.\n",checks); return 0;
}
