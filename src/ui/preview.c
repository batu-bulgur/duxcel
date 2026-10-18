#include "preview.h"
#include <math.h>
#include <stdio.h>
#ifdef DUXCEL_DEV_REVIEW
#include <string.h>
static void preview_review_record(UIPreviewReviewFrame *frame,UIContext *ui,const char *key,const char *role,const char *text,Rectangle bounds,bool enabled,bool container,const char *file,int line) {
    if (!frame || !frame->armed) return;
    Rectangle clip=ui->clipping ? ui->clip : (Rectangle){0,0,GetScreenWidth(),GetScreenHeight()};
    Rectangle shown=GetCollisionRec(bounds,clip); if (shown.width<=0 || shown.height<=0) return;
    if (frame->count>=UI_PREVIEW_REVIEW_TARGETS) { frame->overflow=true; return; }
    UIPreviewReviewTarget *target=&frame->targets[frame->count++]; *target=(UIPreviewReviewTarget){.bounds=bounds,.clip=clip,.enabled=enabled,.container=container};
    snprintf(target->key,sizeof(target->key),"%s",key); snprintf(target->role,sizeof(target->role),"%s",role);
    size_t length=0; while (text && text[length] && length<sizeof(target->text)-1) length++;
    target->truncated=text && text[length];
    if (target->truncated) while (length && ((unsigned char)text[length]&0xc0)==0x80) length--;
    if (length) memcpy(target->text,text,length); target->text[length]=0;
    snprintf(target->source_file,sizeof(target->source_file),"%s",file); target->source_line=(uint32_t)line;
}
#define PREVIEW_REVIEW_PARAM ,UIPreviewReviewFrame *review
#define PREVIEW_RECORD(ui,...) preview_review_record(review,ui,__VA_ARGS__,__FILE__,__LINE__)
#else
#define PREVIEW_REVIEW_PARAM
#define PREVIEW_RECORD(...) ((void)0)
#endif
UIPreviewLayout ui_preview_layout(Rectangle canvas,float scale) {
    UIPreviewLayout out={0};
    if (!isfinite(scale) || scale<=0 || !isfinite(canvas.x) || !isfinite(canvas.y) || !isfinite(canvas.width) || !isfinite(canvas.height) || canvas.width<160*scale || canvas.height<60*scale) return out;
    out.banner=(Rectangle){canvas.x,canvas.y,canvas.width,29*scale};
    float width=fminf(142*scale,canvas.width-12*scale);
    out.return_button=(Rectangle){canvas.x+canvas.width-width-6*scale,canvas.y+3*scale,width,23*scale};
    out.context=(Rectangle){canvas.x,canvas.y+29*scale,canvas.width,28*scale};
    out.grid=(Rectangle){canvas.x,canvas.y+57*scale,canvas.width,canvas.height-57*scale}; out.valid=true; return out;
}
static bool preview_draw(UIContext *u,Grid *grid,const Page *page,Rectangle canvas,size_t step,const char *title,bool enabled,bool grid_enabled,bool loading,const char *error PREVIEW_REVIEW_PARAM) {
#ifdef DUXCEL_DEV_REVIEW
    if (review && review->armed) { review->count=0; review->overflow=false; }
#endif
    float scale=u->scale; UIPreviewLayout layout=ui_preview_layout(canvas,scale); if (!layout.valid) return false;
    DrawRectangleRec(layout.banner,(Color){248,241,217,255}); DrawRectangleRec((Rectangle){canvas.x,layout.banner.y+layout.banner.height-scale,canvas.width,scale},(Color){220,208,169,255});
    PREVIEW_RECORD(u,"panel","group","Historical step preview",canvas,true,true);
    PREVIEW_RECORD(u,"banner","group","Historical step preview",layout.banner,true,true);
    ui_carbon(u,CARBON_VIEW,(Rectangle){canvas.x+10*scale,canvas.y+7*scale,14*scale,14*scale},(Color){107,90,33,255});
    char label[64]; snprintf(label,sizeof(label),"Preview · Step %zu",step+1);
    float label_width=ui_measure_weight(u,label,11,UI_MEDIUM);
    Rectangle heading={canvas.x+32*scale,canvas.y,label_width+scale,29*scale};
    float available=fmaxf(0,layout.return_button.x-heading.x-heading.width-16*scale);
    if (available<30*scale) heading.width=fmaxf(0,layout.return_button.x-heading.x-8*scale);
    ui_label(u,label,heading,11,(Color){58,56,39,255},false,UI_MEDIUM);
    PREVIEW_RECORD(u,"heading","text",label,heading,true,false);
    if (available>=30*scale) {
        Rectangle title_bounds={heading.x+heading.width+8*scale,canvas.y,available,29*scale};
        ui_label(u,title ? title : "",title_bounds,11,(Color){58,56,39,255},false,UI_REGULAR);
        PREVIEW_RECORD(u,"title","text",title ? title : "",title_bounds,true,false);
    }
    bool hover=enabled && CheckCollisionPointRec(GetMousePosition(),layout.return_button); DxPointerGesture gesture;
    bool close=enabled && ((hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || (ui_take_gesture_from(layout.return_button,&gesture) && !gesture.right));
    Color link={107,90,33,255}; if (hover) u->cursor=MOUSE_CURSOR_POINTING_HAND;
    float link_width=ui_measure(u,"Return to current result",11);
    Rectangle link_box={canvas.x+canvas.width-10*scale-link_width,layout.return_button.y,link_width,layout.return_button.height};
    ui_label(u,"Return to current result",link_box,11,link,false,UI_REGULAR);
    PREVIEW_RECORD(u,"return","button","Return to current result",layout.return_button,review ? review->return_enabled : enabled,false);
    DrawRectangleRec((Rectangle){link_box.x,link_box.y+17*scale,link_box.width,scale},link);
    if (error || !page->column_count) {
        const char *message=error ? error : loading ? "Loading step preview…" : "No result at this step";
        Rectangle status_bounds={layout.grid.x+12*scale,layout.grid.y+12*scale,layout.grid.width-24*scale,40*scale};
        DrawRectangleRec(layout.grid,WHITE); ui_text_clip(u,message,status_bounds,12,UI_MUTED,false);
        PREVIEW_RECORD(u,"status","text",message,status_bounds,true,false); return close;
    }
    grid->source_header=true; grid->allow_move=false; grid->chrome_scale=scale; grid->lock_structure=loading;
    grid->digit_width=u->digit_width;
    u->scale=scale*grid->zoom; grid_update(grid,u,page,layout.grid,grid_enabled);
    DxKeyModifiers event;
    while (ui_take_navigation_key(&event)) {
        if (!grid_enabled || grid->dragging || grid->resizing>=0 || grid->context_requested || grid->filter_requested || grid->fit_requested) continue;
        GridInput input={.mouse={-1000000,-1000000},.key=event.key,.separate_keyboard=true,
            .key_shift=(event.modifiers&DX_MOD_SHIFT)!=0,.key_alt=(event.modifiers&DX_MOD_ALT)!=0};
#ifdef DUXCEL_BROWSER_HOST
        input.key_shortcut=(event.modifiers&(DX_MOD_CONTROL|DX_MOD_SUPER))!=0;
#elif defined(__APPLE__)
        input.key_shortcut=(event.modifiers&DX_MOD_SUPER)!=0;
#else
        input.key_shortcut=(event.modifiers&DX_MOD_CONTROL)!=0;
#endif
        grid_handle_input(grid,u,page,&input);
    }
    grid_draw(grid,u,page,loading); u->scale=scale;
    return close;
}
bool ui_preview_draw(UIContext *u,Grid *grid,const Page *page,Rectangle canvas,size_t step,const char *title,bool enabled,bool grid_enabled,bool loading,const char *error) {
    return preview_draw(u,grid,page,canvas,step,title,enabled,grid_enabled,loading,error
#ifdef DUXCEL_DEV_REVIEW
        ,NULL
#endif
    );
}
#ifdef DUXCEL_DEV_REVIEW
bool ui_preview_draw_review(UIContext *u,Grid *grid,const Page *page,Rectangle canvas,size_t step,const char *title,bool enabled,bool grid_enabled,bool loading,const char *error,UIPreviewReviewFrame *review) {
    return preview_draw(u,grid,page,canvas,step,title,enabled,grid_enabled,loading,error,review);
}
#endif
