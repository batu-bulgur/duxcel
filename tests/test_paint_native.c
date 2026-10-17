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
int main(void) {
    SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_VSYNC_HINT); InitWindow(640,640,"Duxcel — native paint validation"); SetTargetFPS(60);
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
        rlDrawRenderBatchActive(); Image image=LoadImageFromScreen(); CHECK(IsImageValid(image));
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
    printf("PASS: %d native paint checks (actual group/nested alpha, DPI clips, sharp Plex text, state restoration and retained allocation)\n",checks); return 0;
}
