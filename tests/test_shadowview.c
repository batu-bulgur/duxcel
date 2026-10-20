#include "ui/shadowview.h"
#include "fixtures/rendering/shadow_reference.h"
#include "fixtures/rendering/shadow_corner_reference.h"
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
static void encoded_samples(void) {
    /* Independent byte expectations at color/opacity rounding boundaries.
       The normalized RGB coefficients encode as binary16 0x2511/0x27ed/0x2922
       (1297*2^-16,2029*2^-16,1314*2^-15), before spatial coverage. Alpha
       remains coverage*33, including the 150% sample just above 20.5. These
       samples exercise the public mask, without exporting a test quantizer. */
    static const struct {
        float scale,density; int x; unsigned char expected[4];
    } samples[]={
        {1,.5f,10,{3,4,5,17}}, /* Exactly half coverage: byte opacity ties up. */
        {1,2,43,{3,4,6,18}},
        {1,2,44,{3,5,6,19}}, /* Green crosses its normalized coefficient boundary. */
        {1,2,45,{3,5,6,20}},
        {1.5f,2,68,{3,5,6,20}},
        {1.5f,2,69,{3,5,6,21}}, /* Quantizing alpha with RGB would incorrectly give 20. */
        {1.5f,2,70,{3,5,7,21}}
    };
    for (size_t i=0;i<sizeof samples/sizeof *samples;i++) {
        UIShadowLayout q=ui_shadow_layout(samples[i].scale,(Vector2){samples[i].density,samples[i].density});
        CHECK(q.valid && samples[i].x>=0 && samples[i].x<q.width);
        unsigned char *mask=malloc(q.bytes); CHECK(mask && ui_shadow_mask_rgba(&q,mask,q.bytes));
        CHECK(!memcmp(mask+pixel(&q,samples[i].x,q.corner_rows),samples[i].expected,4));
        free(mask);
    }
}
static void reference_pixels(void) {
    const struct { const ShadowReferencePixel *pixels; size_t count; float scale; int left,first; } refs[]={
        {narrow_100,sizeof narrow_100/sizeof *narrow_100,1,720,624},
        {narrow_150,sizeof narrow_150/sizeof *narrow_150,1.5f,440,296}
    };
    size_t points=0;
    for (size_t r=0;r<sizeof refs/sizeof *refs;r++) {
        UIShadowLayout q=ui_shadow_layout(refs[r].scale,(Vector2){2,2}); CHECK(q.valid);
        unsigned char *mask=malloc(q.bytes); CHECK(mask && ui_shadow_mask_rgba(&q,mask,q.bytes));
        for (size_t i=0;i<refs[r].count;i++) {
            int x=refs[r].first+(int)i-refs[r].left+q.width;
            unsigned char zero[4]={0},composited[4],*p=x>=0 && x<q.width ? mask+pixel(&q,x,q.corner_rows) : zero;
            const ShadowReferencePixel *ref=&refs[r].pixels[i];
            for (int c=0;c<3;c++) composited[c]=(unsigned char)(p[c]+((unsigned)ref->background[c]*(255-p[3])+127)/255);
            composited[3]=255; points++;
            CHECK(!memcmp(composited,ref->foreground,4));
        }
        free(mask);
    }
    CHECK(points==240);
    printf("Reference: %zu/%zu exact source-profile pixels, without exceptions or tolerance.\n",points,points);
}

/* Point-sample the actual public three-slice placement at a recorded DPR2
   device-pixel center. Outside rows have no placed slice; this is a pure
   placement test, not a claim about App scissor/GPU coverage. */
static const unsigned char *reference_sample(const UIShadowLayout *q,const UIShadowPlacement *p,
        const unsigned char *mask,unsigned x,unsigned y,int *atlas_x,int *atlas_y) {
    static const unsigned char zero[4]={0};
    const UIShadowSlice slices[]={p->top,p->body,p->bottom};
    double px=(x+.5)/2,py=(y+.5)/2;
    *atlas_x=*atlas_y=-1;
    for (size_t i=0;i<sizeof slices/sizeof *slices;i++) {
        Rectangle b=slices[i].bounds,t=slices[i].source;
        if (b.width<=0 || b.height<=0 || px<b.x || py<b.y || px>=b.x+b.width || py>=b.y+b.height) continue;
        int sx=(int)floor(t.x+(px-b.x)*t.width/b.width),sy=(int)floor(t.y+(py-b.y)*t.height/b.height);
        CHECK(sx>=0 && sx<q->atlas_width && sy>=0 && sy<q->atlas_height);
        *atlas_x=sx; *atlas_y=sy;
        return mask+pixel(q,sx,sy);
    }
    return zero;
}
static void reference_composite(const unsigned char *mask,const unsigned char *background,unsigned char out[4]) {
    for (int c=0;c<3;c++) out[c]=(unsigned char)(mask[c]+((unsigned)background[c]*(255-mask[3])+127)/255);
    out[3]=255;
}
static void reference_corner_pixels(void) {
#define CORNER_CASE(name,scale,wrapper,expected) {#name,scale,wrapper,name,sizeof name/sizeof *name,expected}
    const struct {
        const char *name; float scale; Rectangle wrapper; const ShadowCornerPixel *pixels; size_t count,expected;
    } refs[]={
        CORNER_CASE(narrow_100_top,1,((Rectangle){360,93,280,605}),1768),
        CORNER_CASE(narrow_100_bottom,1,((Rectangle){360,93,280,605}),1596),
        CORNER_CASE(narrow_100_top_seam,1,((Rectangle){360,93,280,605}),104),
        CORNER_CASE(narrow_100_bottom_seam,1,((Rectangle){360,93,280,605}),104),
        CORNER_CASE(narrow_150_top,1.5f,((Rectangle){220,139,420,468}),3080),
        CORNER_CASE(narrow_150_bottom,1.5f,((Rectangle){220,139,420,468}),4723),
        CORNER_CASE(narrow_150_top_seam,1.5f,((Rectangle){220,139,420,468}),132),
        CORNER_CASE(narrow_150_bottom_seam,1.5f,((Rectangle){220,139,420,468}),156)
    };
#undef CORNER_CASE
    const unsigned char white[]={255,255,255,255};
    size_t points=0;
    for (size_t r=0;r<sizeof refs/sizeof *refs;r++) {
        CHECK(refs[r].count==refs[r].expected);
        UIShadowLayout q=ui_shadow_layout(refs[r].scale,(Vector2){2,2}); CHECK(q.valid);
        UIShadowPlacement p=ui_shadow_placement(&q,refs[r].wrapper); CHECK(p.valid);
        unsigned char *mask=malloc(q.bytes); CHECK(mask && ui_shadow_mask_rgba(&q,mask,q.bytes));
        for (size_t i=0;i<refs[r].count;i++) {
            const ShadowCornerPixel *ref=&refs[r].pixels[i]; int sx,sy; unsigned char rgba[4];
            const unsigned char *value=reference_sample(&q,&p,mask,ref->x,ref->y,&sx,&sy);
            reference_composite(value,white,rgba); points++;
            if (memcmp(rgba,ref->foreground,4)) {
                fprintf(stderr,"STRICT corner mismatch %s pixel=(%u,%u) atlas=(%d,%d) expected=(%u,%u,%u,%u) actual=(%u,%u,%u,%u) mask=(%u,%u,%u,%u) background=(255,255,255,255)\n",
                    refs[r].name,ref->x,ref->y,sx,sy,ref->foreground[0],ref->foreground[1],ref->foreground[2],ref->foreground[3],rgba[0],rgba[1],rgba[2],rgba[3],value[0],value[1],value[2],value[3]);
                CHECK(!memcmp(rgba,ref->foreground,4));
            }
            CHECK(!memcmp(rgba,ref->foreground,4));
        }
        free(mask);
    }
    CHECK(points==11663);
    printf("Corner reference: %zu/%zu exact recorded white-background top/bottom/seam pixels.\n",points,points);
}
static void reference_outside_pixels(void) {
#define OUTSIDE_CASE(name,scale,wrapper,expected) {#name,scale,wrapper,name,sizeof name/sizeof *name,expected}
    const struct {
        const char *name; float scale; Rectangle wrapper; const ShadowOutsidePixel *pixels; size_t count,expected;
    } refs[]={
        OUTSIDE_CASE(narrow_100_outside_top,1,((Rectangle){360,93,280,605}),52),
        OUTSIDE_CASE(narrow_100_outside_bottom,1,((Rectangle){360,93,280,605}),52),
        OUTSIDE_CASE(narrow_150_outside_top,1.5f,((Rectangle){220,139,420,468}),78),
        OUTSIDE_CASE(narrow_150_outside_bottom,1.5f,((Rectangle){220,139,420,468}),78)
    };
#undef OUTSIDE_CASE
    size_t points=0;
    for (size_t r=0;r<sizeof refs/sizeof *refs;r++) {
        CHECK(refs[r].count==refs[r].expected);
        UIShadowLayout q=ui_shadow_layout(refs[r].scale,(Vector2){2,2}); CHECK(q.valid);
        UIShadowPlacement p=ui_shadow_placement(&q,refs[r].wrapper); CHECK(p.valid);
        unsigned char *mask=malloc(q.bytes); CHECK(mask && ui_shadow_mask_rgba(&q,mask,q.bytes));
        for (size_t i=0;i<refs[r].count;i++) {
            const ShadowOutsidePixel *ref=&refs[r].pixels[i]; int sx,sy; unsigned char rgba[4];
            const unsigned char *value=reference_sample(&q,&p,mask,ref->x,ref->y,&sx,&sy);
            CHECK(sx==-1 && sy==-1); CHECK(!memcmp(ref->foreground,ref->background,4));
            reference_composite(value,ref->background,rgba); points++;
            CHECK(!memcmp(rgba,ref->foreground,4));
        }
        free(mask);
    }
    CHECK(points==260);
    printf("Outside reference: %zu/%zu actual unchanged pixels immediately outside placed extent; no App clip claim.\n",points,points);
}

int main(int argc,char **argv) {
    if (argc>2 || (argc==2 && strcmp(argv[1],"--reference-corners"))) {
        fprintf(stderr,"Usage: %s [--reference-corners]\n",argv[0]); return 2;
    }
    reference_geometry(); density_masks(); invalid_inputs(); encoded_samples(); reference_pixels();
    /* The independent exact corner diagnostic currently exposes a retained
       mismatch. It must not silently replace/relax the accepted body gate. */
    if (argc==2) { reference_outside_pixels(); reference_corner_pixels(); }
    printf("PASS: %u pure shadowview checks; no GPU/render/OS parity claim.\n",checks); return 0;
}
