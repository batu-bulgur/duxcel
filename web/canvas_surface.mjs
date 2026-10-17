// Browser surface sizing only. C/raylib owns layout, input coordinates and the
// synchronized GL framebuffer change; this module never changes canvas CSS or
// its backing attributes. No animation-frame polling is needed.
export const CANVAS_SURFACE_LIMITS=Object.freeze({maxDimension:8192,maxPixels:16*1024*1024,maxScale:4});
const positive=value=>Number.isFinite(value)&&value>0;
const pixelRatio=value=>positive(value)?value:1;

/** Preserve the logical viewport while bounding the backing allocation. The
 * pixel limit describes color-buffer pixels, not total browser/GPU memory. */
export function computeCanvasSurface(width,height,ratio=1,limits=CANVAS_SURFACE_LIMITS){
    if(!positive(width)||!positive(height)||width>0x7fffffff||height>0x7fffffff)return null;
    const logicalWidth=Math.max(1,Math.round(width)),logicalHeight=Math.max(1,Math.round(height));
    const maxDimension=limits.maxDimension??CANVAS_SURFACE_LIMITS.maxDimension;
    const maxPixels=limits.maxPixels??CANVAS_SURFACE_LIMITS.maxPixels;
    const maxScale=limits.maxScale??CANVAS_SURFACE_LIMITS.maxScale;
    if(!Number.isSafeInteger(maxDimension)||maxDimension<1||!Number.isSafeInteger(maxPixels)||maxPixels<1||!positive(maxScale))
        throw new RangeError('Surface limits must be positive finite dimensions, pixels and scale.');
    const devicePixelRatio=pixelRatio(ratio);
    const scale=Math.min(devicePixelRatio,maxScale,maxDimension/logicalWidth,maxDimension/logicalHeight,Math.sqrt(maxPixels/(logicalWidth*logicalHeight)),maxPixels/logicalWidth,maxPixels/logicalHeight);
    // Floor avoids crossing either budget due to independent dimension rounding.
    const framebufferWidth=Math.max(1,Math.floor(logicalWidth*scale));
    const framebufferHeight=Math.max(1,Math.floor(logicalHeight*scale));
    return {logicalWidth,logicalHeight,framebufferWidth,framebufferHeight,devicePixelRatio,
        scaleX:framebufferWidth/logicalWidth,scaleY:framebufferHeight/logicalHeight};
}

/** The host creates this after the C runtime is ready. onResize receives one
 * snapshot and synchronously applies logicalWidth/Height plus framebufferWidth/
 * Height in C, so GL projection/scissors and canvas backing cannot diverge.
 * ResizeObserver reports CSS content dimensions; offsets and CSS transforms
 * must not enter this logical viewport. An invisible/zero-size canvas retains
 * its previous valid surface until visible again.
 *
 * window, ResizeObserver and devicePixelRatio are injectable for headless tests.
 * resize() is also available for platform/fullscreen integration. dispose()
 * removes every observer/listener and is idempotent. */
export function createCanvasSurface({canvas,onResize,window:viewport=globalThis.window,
    ResizeObserver:Observer=globalThis.ResizeObserver,devicePixelRatio=()=>viewport?.devicePixelRatio??1,
    limits=CANVAS_SURFACE_LIMITS}={}){
    if(!canvas||typeof onResize!=='function')throw new TypeError('A canvas and resize callback are required.');
    if(typeof devicePixelRatio!=='function')throw new TypeError('devicePixelRatio must be a function.');
    let current=null,disposed=false,observer=null,resolution=null,watchedRatio=null;
    const readRatio=()=>pixelRatio(devicePixelRatio());
    function removeResolution(){
        if(resolution?.removeEventListener)resolution.removeEventListener('change',resolutionChanged);
        else resolution?.removeListener?.(resolutionChanged);
        resolution=null;watchedRatio=null;
    }
    function watchResolution(ratio){
        if(ratio===watchedRatio)return;
        removeResolution();
        if(!viewport?.matchMedia)return;
        resolution=viewport.matchMedia(`(resolution: ${ratio}dppx)`);watchedRatio=ratio;
        if(resolution.addEventListener)resolution.addEventListener('change',resolutionChanged);
        else resolution.addListener?.(resolutionChanged);
    }
    function resize(size){
        if(disposed)return false;
        const ratio=readRatio();watchResolution(ratio);
        // clientWidth/Height remain CSS-space values even when the canvas has a
        // translated/scaled bounding rect or a DPR-sized backing allocation.
        const next=computeCanvasSurface(size?.width??canvas.clientWidth,size?.height??canvas.clientHeight,ratio,limits);
        if(!next)return false;
        if(current&&Object.keys(next).every(key=>next[key]===current[key]))return false;
        onResize(next); // Keep prior state on failure so a retry is possible.
        current=next;
        return true;
    }
    function resolutionChanged(){
        if(disposed)return;
        removeResolution();resize();
    }
    const viewportChanged=()=>resize();
    if(typeof Observer==='function'){
        observer=new Observer(entries=>{
            if(disposed)return;
            for(const entry of entries)if(entry.target===canvas){resize(entry.contentRect);break;}
        });
        observer.observe(canvas);
    }
    viewport?.addEventListener?.('resize',viewportChanged);
    // Browser page zoom normally updates DPR; visualViewport is an additional
    // notification source. Its pinch scale is deliberately not UI/grid scale.
    viewport?.visualViewport?.addEventListener?.('resize',viewportChanged);
    try{resize();}catch(error){dispose();throw error;}
    function dispose(){
        if(disposed)return;
        disposed=true;observer?.disconnect();removeResolution();
        viewport?.removeEventListener?.('resize',viewportChanged);
        viewport?.visualViewport?.removeEventListener?.('resize',viewportChanged);
    }
    return {resize:()=>resize(),dispose,get current(){return current;}};
}
