#include "ui/ui.h"
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"native paint at %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Color sample(Image image,UIContext *ui,float x,float y) { return GetImageColor(image,(int)(x*ui->render_scale.x),(int)(y*ui->render_scale.y)); }
static void expect_color(Color actual,int red,int green,int blue) {
    CHECK(abs(actual.r-red)<=2 && abs(actual.g-green)<=2 && abs(actual.b-blue)<=2);
}
static int glyph_checks;
#define GLYPH_CHECK(x) do { glyph_checks++; if (!(x)) { fprintf(stderr,"native glyph at %d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { Rectangle bounds; int x,y,width,height; } GlyphTile;
typedef struct {
    Font fonts[7];
    GlyphTile tiles[6][6];
    int clip_left[6],clip_right[6];
    char text[6][5];
    uint64_t allocations;
    size_t paint_bytes;
} GlyphFrame;
static const char glyph_text[6][5]={"?","\xef\xbf\xbd","\xd8\xa7","\xe8\xa1\xa8","\xf0\x9f\xa6\x86","\xe8\xa1\xa8"};
static Font glyph_face(const UIContext *ui,int face) {
    return face==0 ? ui->body : face==1 ? ui->small : face==2 ? ui->medium : face==3 ? ui->emphasis : face==4 ? ui->italic : ui->bold_italic;
}
static bool exact_color(Color a,Color b) { return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a; }
static uint64_t glyph_hash(uint64_t hash,Color color) {
    const unsigned char bytes[]={color.r,color.g,color.b,color.a};
    for (size_t i=0;i<sizeof(bytes);i++) { hash^=bytes[i]; hash*=UINT64_C(1099511628211); }
    return hash;
}
static void draw_glyph_frame(UIContext *ui,GlyphFrame *frame) {
    memset(frame,0,sizeof(*frame)); memcpy(frame->text,glyph_text,sizeof(glyph_text));
    frame->allocations=ui->paint_allocations; frame->paint_bytes=ui->paint_bytes; frame->fonts[6]=ui->mono;
    GLYPH_CHECK(!ui->clipping && !ui->paint_depth && !ui->scissor_depth);
    float dx=ui->render_scale.x,dy=ui->render_scale.y;
    for (int face=0;face<6;face++) {
        Font font=frame->fonts[face]=glyph_face(ui,face); int missing=ui->fallback[face],question=GetGlyphIndex(font,'?');
        GLYPH_CHECK(IsTextureValid(font.texture) && missing>=0 && missing<font.glyphCount);
        GLYPH_CHECK(font.glyphs[missing].value==0xfffd && font.glyphs[question].value=='?' && missing!=question);
        GLYPH_CHECK(font.recs[missing].width>0 && font.recs[missing].height>0);
        UIFontWeight weight=face<2 ? UI_REGULAR : face==2 ? UI_MEDIUM : face==3 ? UI_SEMIBOLD : face==4 ? UI_ITALIC : UI_SEMIBOLD_ITALIC;
        float size=face==1 ? 11 : 13;
        int advance=(int)roundf(ui_measure_weight(ui,frame->text[1],size,weight)*dx);
        frame->clip_left[face]=advance/3; frame->clip_right[face]=advance*2/3;
        GLYPH_CHECK(frame->clip_left[face]>0 && frame->clip_right[face]>frame->clip_left[face]);
        for (int column=0;column<6;column++) {
            GlyphTile *tile=&frame->tiles[face][column];
            tile->x=(int)roundf((20+80*column)*dx); tile->y=(int)roundf((220+28*face)*dy);
            tile->width=(int)roundf(60*dx); tile->height=(int)roundf(28*dy);
            tile->bounds=(Rectangle){tile->x/dx,tile->y/dy,tile->width/dx,tile->height/dy};
            GLYPH_CHECK(advance<tile->width && tile->height>0);
            DrawRectangleRec(tile->bounds,WHITE);
            if (column==5) ui_set_clip(ui,(Rectangle){(tile->x+frame->clip_left[face])/dx,tile->y/dy,(frame->clip_right[face]-frame->clip_left[face])/dx,tile->height/dy});
            ui_label(ui,frame->text[column],tile->bounds,size,UI_TEXT,false,weight);
            if (column==5) ui_clear_clip(ui);
        }
    }
}
static void check_glyph_frame(Image image,UIContext *ui,const GlyphFrame *frame) {
    GLYPH_CHECK(!memcmp(frame->text,glyph_text,sizeof(glyph_text)));
    GLYPH_CHECK(ui->paint_allocations==frame->allocations && ui->paint_bytes==frame->paint_bytes);
    GLYPH_CHECK(!ui->clipping && !ui->paint_depth && !ui->scissor_depth);
    for (int face=0;face<7;face++) {
        Font current=face==6 ? ui->mono : glyph_face(ui,face),old=frame->fonts[face];
        GLYPH_CHECK(current.texture.id==old.texture.id && current.texture.width==old.texture.width && current.texture.height==old.texture.height && current.glyphs==old.glyphs && current.recs==old.recs && current.glyphCount==old.glyphCount && current.baseSize==old.baseSize);
    }
    for (int face=0;face<6;face++) {
        const GlyphTile *reference=&frame->tiles[face][1];
        GLYPH_CHECK(reference->x>=0 && reference->y>=0 && reference->y+reference->height<=image.height && frame->tiles[face][5].x+reference->width<=image.width);
        size_t ink=0,question_difference=0,mismatch[3]={0},clip_mismatch=0,clipped_ink=0,outside_ink=0;
        uint64_t hash=UINT64_C(14695981039346656037),question_hash=hash;
        for (int y=0;y<reference->height;y++) for (int x=0;x<reference->width;x++) {
            Color expected=GetImageColor(image,reference->x+x,reference->y+y);
            const GlyphTile *question=&frame->tiles[face][0]; Color actual=GetImageColor(image,question->x+x,question->y+y);
            ink+=!exact_color(expected,WHITE); question_difference+=!exact_color(expected,actual);
            hash=glyph_hash(hash,expected); question_hash=glyph_hash(question_hash,actual);
            for (int kind=0;kind<3;kind++) {
                const GlyphTile *tile=&frame->tiles[face][kind+2];
                mismatch[kind]+=!exact_color(expected,GetImageColor(image,tile->x+x,tile->y+y));
            }
            const GlyphTile *clipped=&frame->tiles[face][5]; Color cut=GetImageColor(image,clipped->x+x,clipped->y+y);
            bool inside=x>=frame->clip_left[face] && x<frame->clip_right[face];
            clip_mismatch+=!exact_color(cut,inside ? expected : WHITE);
            if (inside) clipped_ink+=!exact_color(cut,WHITE); else outside_ink+=!exact_color(cut,WHITE);
        }
        Font font=frame->fonts[face];
        printf("GPU glyph scale=%.3f density=%.3f,%.3f face=%d texture=%u atlas=%dx%d reference=%d,%d,%d,%d clip_x=%d..%d replacement_hash=%016llx question_hash=%016llx ink=%zu question_diff=%zu bmp_diff=%zu cjk_diff=%zu emoji_diff=%zu clip_diff=%zu clip_ink=%zu outside_ink=%zu\n",ui->scale,ui->render_scale.x,ui->render_scale.y,face,font.texture.id,font.texture.width,font.texture.height,reference->x,reference->y,reference->width,reference->height,frame->clip_left[face],frame->clip_right[face],(unsigned long long)hash,(unsigned long long)question_hash,ink,question_difference,mismatch[0],mismatch[1],mismatch[2],clip_mismatch,clipped_ink,outside_ink);
        GLYPH_CHECK(ink>0 && question_difference>0 && hash!=question_hash);
        GLYPH_CHECK(!mismatch[0] && !mismatch[1] && !mismatch[2]);
        GLYPH_CHECK(!clip_mismatch && clipped_ink>0 && !outside_ink);
    }
}
int main(void) {
    const char *visible=getenv("DUXCEL_TEST_VISIBLE"); bool hidden=!(visible && !strcmp(visible,"1"));
    SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_VSYNC_HINT|(hidden ? FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED|FLAG_WINDOW_ALWAYS_RUN : 0));
    InitWindow(640,640,"Duxcel — native paint validation"); SetTargetFPS(60);
    CHECK(IsWindowReady()); CHECK(!hidden || IsWindowHidden()); CHECK(!hidden || !IsWindowFocused());
    UIContext *ui=calloc(1,sizeof(*ui)); CHECK(ui);
    CHECK(ui_init(ui,"assets/fonts/IBMPlexSans-Regular.otf","assets/fonts/IBMPlexSans-Medium.otf","assets/fonts/IBMPlexSans-SemiBold.otf","assets/fonts/JetBrainsMono-Regular.ttf"));
    const float scales[]={.8f,1,1.5f};
    for (int scale=0;scale<3;scale++) {
        ui_set_scale(ui,scales[scale]); ui_update(ui); float s=ui->scale;
        BeginDrawing(); ClearBackground(WHITE);
        unsigned framebuffer=rlGetActiveFramebuffer(); int width=rlGetFramebufferWidth(),height=rlGetFramebufferHeight();
        Matrix projection=rlGetMatrixProjection(),modelview=rlGetMatrixModelview();
        Rectangle group={20,100,100*s,100*s};
        CHECK(ui_paint_begin(ui,group,(Vector2){100*s,100*s},.5f));
        DrawRectangleRec(group,RED); DrawRectangleRec((Rectangle){group.x+20*s,group.y+20*s,50*s,50*s},BLUE);
        ui_paint_end(ui);
        CHECK(!ui->paint_depth && !ui->clipping && !ui->scissor_depth);
        CHECK(rlGetActiveFramebuffer()==framebuffer && rlGetFramebufferWidth()==width && rlGetFramebufferHeight()==height);
        Matrix after=rlGetMatrixProjection(); CHECK(!memcmp(&projection,&after,sizeof(after))); after=rlGetMatrixModelview(); CHECK(!memcmp(&modelview,&after,sizeof(after)));
        Rectangle nested={220,100,100*s,100*s};
        CHECK(ui_paint_begin(ui,nested,(Vector2){100*s,100*s},.5f)); DrawRectangleRec(nested,RED);
        Rectangle child={nested.x+20*s,nested.y+20*s,50*s,50*s};
        CHECK(ui_paint_begin(ui,child,(Vector2){50*s,50*s},.5f)); DrawRectangleRec(child,BLUE); ui_paint_end(ui); ui_paint_end(ui);
        Rectangle clip={420,100,40*s,40*s}; CHECK(ui_scissor_begin(ui,clip));
        CHECK(ui_paint_begin(ui,(Rectangle){400,80,100*s,100*s},(Vector2){100*s,100*s},.5f)); DrawRectangleRec((Rectangle){400,80,120*s,120*s},RED); ui_paint_end(ui);
        ui_scissor_end(ui); DrawRectangleRec((Rectangle){570,100,20,20},GREEN);
        CHECK(ui_scissor_begin(ui,(Rectangle){10.25f,500.25f,0,20})); DrawRectangleRec((Rectangle){0,490,30,40},RED); ui_scissor_end(ui);
        TextInput editing={0}; input_set(&editing,"Amount café 123.45"); editing.select_all=false; editing.cursor=editing.anchor=0;
        Rectangle editor={200,400,260,30}; input_update(&editing);
        float caret_x=editor.x+8*s+ui_measure(ui,"Amount ",11)+ui_codepoint_width(ui,'c',11)*.2f;
        ui_note_pointer_gesture((DxPointerGesture){.x0=caret_x,.y0=editor.y+15,.x1=caret_x,.y1=editor.y+15});
        input_draw_sized(ui,&editing,editor,11,true); CHECK(editing.cursor==7 && editing.anchor==7); input_free(&editing);
        Rectangle baseline={8,8,180*s,40*s},copied={328,8,180*s,40*s};
        DrawRectangleRec(baseline,WHITE); ui_label(ui,"Amount 123.45 — NULL",baseline,13,UI_TEXT,false,UI_REGULAR);
        CHECK(ui_paint_begin(ui,copied,(Vector2){copied.width,copied.height},.9999f));
        DrawRectangleRec(copied,WHITE); ui_label(ui,"Amount 123.45 — NULL",copied,13,UI_TEXT,false,UI_REGULAR); ui_paint_end(ui);
        GlyphFrame glyph_frame; draw_glyph_frame(ui,&glyph_frame);
        rlDrawRenderBatchActive(); Image image=LoadImageFromScreen(); CHECK(IsImageValid(image));
        char glyph_capture[128]; snprintf(glyph_capture,sizeof(glyph_capture),"tests/results/native-authoring/native-glyph-%03d.png",(int)roundf(s*100));
        ExportImage(image,glyph_capture); /* Preserve exact pixels before a new glyph assertion can fail. */
        check_glyph_frame(image,ui,&glyph_frame);
        /* Raylib RED/BLUE are intentionally not full RGB primaries. */
        expect_color(sample(image,ui,25,105),(RED.r+255)/2,(RED.g+255)/2,(RED.b+255)/2);
        expect_color(sample(image,ui,20+40*s,100+40*s),(BLUE.r+255)/2,(BLUE.g+255)/2,(BLUE.b+255)/2);
        expect_color(sample(image,ui,220+40*s,100+40*s),(RED.r+BLUE.r+510)/4,(RED.g+BLUE.g+510)/4,(RED.b+BLUE.b+510)/4);
        expect_color(sample(image,ui,425,105),(RED.r+255)/2,(RED.g+255)/2,(RED.b+255)/2);
        expect_color(sample(image,ui,410,90),255,255,255); expect_color(sample(image,ui,575,105),GREEN.r,GREEN.g,GREEN.b);
        expect_color(sample(image,ui,10.5f,505),255,255,255);
        int px=(int)(baseline.x*ui->render_scale.x),py=(int)(baseline.y*ui->render_scale.y),offset=(int)(320*ui->render_scale.x);
        int columns=(int)(baseline.width*ui->render_scale.x),rows=(int)(baseline.height*ui->render_scale.y),largest=0;
        for (int y=0;y<rows;y++) for (int x=0;x<columns;x++) {
            Color a=GetImageColor(image,px+x,py+y),b=GetImageColor(image,px+x+offset,py+y);
            int difference=abs(a.r-b.r)+abs(a.g-b.g)+abs(a.b-b.b); if (difference>largest) largest=difference;
        }
        CHECK(largest<=6); CHECK(!ui->paint_depth && !ui->scissor_depth && !ui->clipping && ui->paint_bytes<=UI_PAINT_TOTAL_BYTES);
        if (scale==2) ExportImage(image,"tests/results/native-authoring/native-paint-gpu.png");
        UnloadImage(image); rlCheckErrors(); EndDrawing();
    }
    /* Keep the last complete popup while an action aborts its next picture. */
    BeginDrawing(); ClearBackground(WHITE);
    Rectangle popup={40,200,120,90};
    CHECK(ui_paint_cache_begin(ui,3,popup,(Vector2){120,90})); DrawRectangleRec(popup,RED);
    UIPaintImage front=ui_paint_cache_end(ui);
    CHECK(front.layout.valid && front.buffer_index==3 && !ui->paint_depth && rlGetActiveFramebuffer()==0);
    CHECK(ui_paint_cache_begin(ui,4,popup,(Vector2){120,90})); DrawRectangleRec(popup,BLUE);
    UIPaintImage partial=ui_paint_cache_end(ui); CHECK(partial.texture!=front.texture);
    ui_paint_image(ui,front,.5f,(Vector2){0,-3});
    rlDrawRenderBatchActive(); Image retained=LoadImageFromScreen();
    expect_color(sample(retained,ui,50,199),(RED.r+255)/2,(RED.g+255)/2,(RED.b+255)/2);
    expect_color(sample(retained,ui,50,289),255,255,255);
    CHECK(!ui->paint_depth && !ui->scissor_depth && !ui->clipping && ui->paint_bytes<=UI_PAINT_TOTAL_BYTES);
    UnloadImage(retained); rlCheckErrors(); EndDrawing();
    uint64_t allocations=ui->paint_allocations;
    for (int frame=0;frame<30;frame++) {
        BeginDrawing(); ClearBackground(WHITE);
        CHECK(ui_paint_begin(ui,(Rectangle){20,100,100,100},(Vector2){100,100},.5f)); DrawRectangle(20,100,100,100,RED); ui_paint_end(ui);
        EndDrawing();
    }
    CHECK(ui->paint_allocations==allocations && !ui->paint_fallbacks);
    ui_destroy(ui); CHECK(!ui->paint_bytes && !ui->paint_depth); free(ui); CloseWindow();
    printf("PASS: %d native GPU missing-glyph checks (exact same-frame translated pixels, all six faces, .8/1/1.5 scales; CJK/emoji remain unsupported)\n",glyph_checks);
    printf("PASS: %d native paint checks (actual group/nested alpha, DPI clips, sharp Plex text, state restoration and retained allocation)\n",checks); return 0;
}
