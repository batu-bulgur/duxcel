#include "ui.h"
#include <rlgl.h>
#ifdef DUXCEL_BROWSER_HOST
#include <GLES2/gl2.h>
#endif
#include <math.h>
#include <string.h>
static Rectangle paint_intersection(Rectangle a,Rectangle b) {
    float x=fmaxf(a.x,b.x),y=fmaxf(a.y,b.y);
    return (Rectangle){x,y,fmaxf(0,fminf(a.x+a.width,b.x+b.width)-x),fmaxf(0,fminf(a.y+a.height,b.y+b.height)-y)};
}
void ui_paint_apply_clip(UIContext *ui) {
    if (!IsWindowReady()) return;
    rlDrawRenderBatchActive();
    bool clip=ui->scissor_depth || ui->paint_depth;
    if (!clip) { rlDisableScissorTest(); return; }
    Rectangle area=ui->scissor_depth ? ui->scissor_rects[ui->scissor_depth-1] : ui->paint_frames[ui->paint_depth-1].layout.bounds;
    int origin_x=0,origin_y=0;
    if (ui->paint_depth) {
        UIPaintLayout *layer=&ui->paint_frames[ui->paint_depth-1].layout;
        area=paint_intersection(area,layer->bounds); origin_x=layer->x; origin_y=layer->y;
    }
    if (area.width<=0 || area.height<=0) { rlEnableScissorTest(); rlScissor(0,0,0,0); return; }
    float dx=ui->render_scale.x,dy=ui->render_scale.y;
    int left=(int)floorf(area.x*dx)-origin_x,top=(int)floorf(area.y*dy)-origin_y;
    int right=(int)ceilf((area.x+area.width)*dx)-origin_x,bottom=(int)ceilf((area.y+area.height)*dy)-origin_y;
    int height=ui->paint_depth ? rlGetFramebufferHeight() : GetRenderHeight();
    rlEnableScissorTest(); rlScissor(left,height-bottom,right-left,bottom-top);
}
static void paint_restore(const UIPaintFrame *frame) {
    rlEnableFramebuffer(frame->framebuffer);
    rlViewport(0,0,frame->viewport_width,frame->viewport_height);
    rlSetFramebufferWidth(frame->framebuffer_width); rlSetFramebufferHeight(frame->framebuffer_height);
    rlSetMatrixProjection(frame->projection); rlSetMatrixModelview(frame->modelview);
}
static void paint_unload(UIPaintBuffer *buffer) {
    if (buffer->target.texture.id) rlUnloadTexture(buffer->target.texture.id);
    if (buffer->target.id) {
#ifdef DUXCEL_BROWSER_HOST
        /* These caches own only the color texture released above, with no
           depth attachment. raylib's generic unload queries an absent depth
           object's name, which WebGL1 rejects. Delete the known FBO directly
           without rebinding or introspecting unrelated framebuffer state. */
        GLuint framebuffer=buffer->target.id; glDeleteFramebuffers(1,&framebuffer);
#else
        rlUnloadFramebuffer(buffer->target.id);
#endif
    }
    memset(buffer,0,sizeof(*buffer));
}
static bool paint_allocate(UIPaintBuffer *buffer,UIPaintLayout layout) {
    RenderTexture2D target={0}; target.id=rlLoadFramebuffer();
    if (!target.id) return false;
    target.texture=(Texture2D){.id=rlLoadTexture(NULL,layout.capacity_width,layout.capacity_height,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,1),.width=layout.capacity_width,.height=layout.capacity_height,.mipmaps=1,.format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    bool complete=false;
    if (target.texture.id) {
        rlEnableFramebuffer(target.id);
        rlFramebufferAttach(target.id,target.texture.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);
        complete=rlFramebufferComplete(target.id);
    }
    if (!complete) { UIPaintBuffer failed={.target=target}; paint_unload(&failed); return false; }
    SetTextureFilter(target.texture,TEXTURE_FILTER_POINT); SetTextureWrap(target.texture,TEXTURE_WRAP_CLAMP);
    *buffer=(UIPaintBuffer){.target=target,.bytes=layout.bytes}; return true;
}
static bool paint_begin(UIContext *ui,unsigned slot,Rectangle bounds,Vector2 reserve,float opacity,bool capture) {
    if (!ui || !isfinite(opacity) || (!capture && opacity>=1) || !IsWindowReady() || ui->paint_depth>=UI_PAINT_DEPTH) return false;
    opacity=fmaxf(0,opacity);
    bounds=paint_intersection(bounds,(Rectangle){0,0,GetScreenWidth(),GetScreenHeight()});
    if (ui->clipping) bounds=paint_intersection(bounds,ui->clip);
    if (slot>=UI_PAINT_BUFFERS) return false;
    unsigned index=ui->paint_depth; UIPaintBuffer *buffer=&ui->paint_buffers[slot];
    UIPaintLayout layout=ui_paint_layout(bounds,ui->render_scale,reserve,(Vector2){buffer->target.texture.width,buffer->target.texture.height});
    if (!layout.valid || ui->paint_bytes-buffer->bytes+layout.bytes>UI_PAINT_TOTAL_BYTES) { ui->paint_fallbacks++; return false; }
    UIPaintFrame frame={.layout=layout,.buffer_index=slot,.projection=rlGetMatrixProjection(),.modelview=rlGetMatrixModelview(),.saved_clip=ui->clip,.framebuffer=rlGetActiveFramebuffer(),.scissor_depth=ui->scissor_depth,.framebuffer_width=rlGetFramebufferWidth(),.framebuffer_height=rlGetFramebufferHeight(),.saved_clipping=ui->clipping,.opacity=opacity};
    frame.viewport_width=frame.framebuffer ? frame.framebuffer_width : GetRenderWidth();
    frame.viewport_height=frame.framebuffer ? frame.framebuffer_height : GetRenderHeight();
    rlDrawRenderBatchActive(); rlDisableScissorTest();
    if (!buffer->target.id || buffer->target.texture.width!=layout.capacity_width || buffer->target.texture.height!=layout.capacity_height) {
        ui->paint_bytes-=buffer->bytes; paint_unload(buffer);
        if (!paint_allocate(buffer,layout)) { paint_restore(&frame); ui_paint_apply_clip(ui); ui->paint_fallbacks++; return false; }
        ui->paint_bytes+=buffer->bytes; ui->paint_allocations++;
    }
    ui->paint_frames[index]=frame; ui->paint_depth++;
    if (ui->paint_depth>ui->paint_peak_depth) ui->paint_peak_depth=ui->paint_depth;
    rlEnableFramebuffer(buffer->target.id); rlViewport(0,0,layout.capacity_width,layout.capacity_height);
    rlSetFramebufferWidth(layout.capacity_width); rlSetFramebufferHeight(layout.capacity_height);
    Matrix projection={.m0=2.0f/layout.capacity_width,.m5=-2.0f/layout.capacity_height,.m10=-2,.m12=-1,.m13=1,.m14=-1,.m15=1};
    Matrix modelview={.m0=ui->render_scale.x,.m5=ui->render_scale.y,.m10=1,.m12=-(float)layout.x,.m13=-(float)layout.y,.m15=1};
    rlSetMatrixProjection(projection); rlSetMatrixModelview(modelview);
    rlClearColor(0,0,0,0); rlClearScreenBuffers();
    /* Store premultiplied color with correct coverage, including overlapping
       translucent glyphs. A single later composition applies group opacity. */
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA,RL_ONE_MINUS_SRC_ALPHA,RL_ONE,RL_ONE_MINUS_SRC_ALPHA,RL_FUNC_ADD,RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
    ui->clip=ui->clipping ? paint_intersection(ui->clip,layout.bounds) : layout.bounds; ui->clipping=true;
    ui_paint_apply_clip(ui); return true;
}
bool ui_paint_begin(UIContext *ui,Rectangle bounds,Vector2 reserve,float opacity) {
    return paint_begin(ui,ui ? ui->paint_depth : 0,bounds,reserve,opacity,false);
}
bool ui_paint_cache_begin(UIContext *ui,unsigned slot,Rectangle bounds,Vector2 reserve) {
    if (slot<UI_PAINT_DEPTH) return false;
    return paint_begin(ui,slot,bounds,reserve,1,true);
}
UIPaintImage ui_paint_cache_end(UIContext *ui) {
    if (!ui || !ui->paint_depth) return (UIPaintImage){0};
    rlDrawRenderBatchActive();
    UIPaintFrame frame=ui->paint_frames[--ui->paint_depth];
    ui->clip=frame.saved_clip; ui->clipping=frame.saved_clipping;
    paint_restore(&frame); ui_paint_apply_clip(ui);
    rlSetBlendMode(ui->paint_depth ? RL_BLEND_CUSTOM_SEPARATE : RL_BLEND_ALPHA);
    return (UIPaintImage){.layout=frame.layout,.buffer_index=frame.buffer_index,.texture=ui->paint_buffers[frame.buffer_index].target.texture.id};
}
void ui_paint_image(UIContext *ui,UIPaintImage image,float opacity,Vector2 offset) {
    if (!ui || !image.layout.valid || image.buffer_index>=UI_PAINT_BUFFERS || !isfinite(opacity) || opacity<=0 || !isfinite(offset.x) || !isfinite(offset.y)) return;
    UIPaintBuffer *buffer=&ui->paint_buffers[image.buffer_index];
    if (!image.texture || image.texture!=buffer->target.texture.id) return;
    unsigned char alpha=(unsigned char)lroundf(fminf(1,opacity)*255);
    Rectangle source={0,image.layout.capacity_height-image.layout.height,image.layout.width,-image.layout.height};
    Rectangle destination=image.layout.bounds; destination.x+=offset.x; destination.y+=offset.y;
    rlSetBlendMode(RL_BLEND_ALPHA_PREMULTIPLY);
    DrawTexturePro(buffer->target.texture,source,destination,(Vector2){0},0,(Color){alpha,alpha,alpha,alpha});
    rlSetBlendMode(ui->paint_depth ? RL_BLEND_CUSTOM_SEPARATE : RL_BLEND_ALPHA);
}
void ui_paint_end(UIContext *ui) {
    if (!ui || !ui->paint_depth) return;
    float opacity=ui->paint_frames[ui->paint_depth-1].opacity;
    UIPaintImage image=ui_paint_cache_end(ui); ui_paint_image(ui,image,opacity,(Vector2){0});
}
void ui_paint_destroy(UIContext *ui) {
    if (!ui) return;
    if (ui->paint_depth) {
        rlDrawRenderBatchActive(); UIPaintFrame first=ui->paint_frames[0]; paint_restore(&first);
        ui->clip=first.saved_clip; ui->clipping=first.saved_clipping; ui->paint_depth=0;
        rlSetBlendMode(RL_BLEND_ALPHA); ui_paint_apply_clip(ui);
    }
    for (int i=0;i<UI_PAINT_BUFFERS;i++) paint_unload(&ui->paint_buffers[i]); ui->paint_bytes=0;
}
