/* Duxcel adapter for the pinned raylib 6/Emscripten GLFW web platform.
 * Included only in the private copied dependency, never the SDK or native lib.
 * CSS owns logical layout. C owns framebuffer/projection/clip synchronization.
 * GLFW's HiDPI switch is deliberately unused: it overwrites CSS sizes and
 * disables its CSS mouse transform. Keep its logical/framebuffer records exact.
 */
EM_JS(void, dx_web_install_surface, (void), {
    // The pinned GLFW updater otherwise deletes host inline CSS sizing and
    // resets backing to 1x on initialization/DPR notifications. Preserve CSS;
    // our bounded surface supplies the canvas attributes instead.
    GLFW.updateCanvasDimensions=Browser.updateCanvasDimensions=function(canvas) {
        const s=Module.dxSurface.current;
        if (!s) return;
        canvas.widthNative=s.logicalWidth; canvas.heightNative=s.logicalHeight;
        if (canvas.width!==s.framebufferWidth) canvas.width=s.framebufferWidth;
        if (canvas.height!==s.framebufferHeight) canvas.height=s.framebufferHeight;
    };
});
EM_JS(int, dx_web_read_surface, (int *out, int force), {
    const surface=Module.dxSurface?.current;
    if (!surface || (!force && !Module.dxSurfacePending)) return 0;
    Module.dxSurfacePending=false;
    const canvas=Module.canvas;
    const w=surface.logicalWidth,h=surface.logicalHeight;
    const fw=surface.framebufferWidth,fh=surface.framebufferHeight;
    if (canvas.width!==fw) canvas.width=fw;
    if (canvas.height!==fh) canvas.height=fh;
    canvas.widthNative=w; canvas.heightNative=h;
    if (GLFW.active) {
        GLFW.active.width=w; GLFW.active.height=h;
        GLFW.active.framebufferWidth=fw; GLFW.active.framebufferHeight=fh;
    }
    // Pinned GLFW.calculateMouseCoords uses active.width/height against the
    // transformed CSS rect, so mouse and wheel remain in logical coordinates.
    HEAP32[out>>2]=w; HEAP32[(out>>2)+1]=h;
    HEAP32[(out>>2)+2]=fw; HEAP32[(out>>2)+3]=fh;
    return 1;
});

static void DxWebApplySurface(bool force, bool viewport)
{
    int size[4];
    if (!dx_web_read_surface(size, force)) return;
    CORE.Window.screen.width=size[0]; CORE.Window.screen.height=size[1];
    CORE.Window.render.width=size[2]; CORE.Window.render.height=size[3];
    CORE.Window.currentFbo=CORE.Window.render;
    CORE.Window.renderOffset=(Point){0,0};
    CORE.Window.screenScale=MatrixScale((float)size[2]/size[0],(float)size[3]/size[1],1.0f);
    CORE.Window.resizedLastFrame=true;
    if (viewport) {
        rlSetFramebufferWidth(size[2]); rlSetFramebufferHeight(size[3]);
        SetupViewport(size[2],size[3]);
    }
}

EM_JS(void, dx_web_read_wheel, (float *out), {
    const frame=Module.dxWheelFrame||(Module.dxWheelFrame={});
    Module.dxWheel.drain(frame);
    const i=out>>2;
    HEAPF32[i]=frame.panX; HEAPF32[i+1]=frame.panY;
    HEAPF32[i+2]=frame.zoomDelta; HEAPF32[i+3]=frame.x;
    HEAPF32[i+4]=frame.y; HEAPF32[i+5]=frame.events;
});
static float dxWebWheel[6];
/* One snapshot per browser frame, also while a modal/review frame owns input.
 * There is no queued momentum replay when a blocked controller becomes live. */
void dx_web_begin_frame(void)
{
    DxWebApplySurface(false,true);
    dx_web_read_wheel(dxWebWheel);
    CORE.Input.Mouse.currentWheelMove=(Vector2){-dxWebWheel[0]/52.0f,-dxWebWheel[1]/72.0f};
    if (dxWebWheel[5]) CORE.Input.Mouse.currentPosition=(Vector2){dxWebWheel[3],dxWebWheel[4]};
}
bool dx_web_pixel_pan(float *x,float *y)
{
    *x=dxWebWheel[0]; *y=dxWebWheel[1]; return dxWebWheel[5]!=0;
}
float dx_web_take_magnification(void)
{
    float value=dxWebWheel[2]*-.002f; dxWebWheel[2]=0;
    return fmaxf(-.5f,fminf(.5f,value));
}
