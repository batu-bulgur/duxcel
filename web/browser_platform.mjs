// Browser lifetime and device translation only. All widgets remain shared C.
import {createCanvasSurface,CANVAS_SURFACE_LIMITS} from './canvas_surface.mjs';
import {installCanvasWheel} from './canvas_wheel.mjs';
export function createBrowserPlatform(canvas,gl){
    const viewport=gl.getParameter(gl.MAX_VIEWPORT_DIMS);
    const maxDimension=Math.min(CANVAS_SURFACE_LIMITS.maxDimension,
        gl.getParameter(gl.MAX_RENDERBUFFER_SIZE),viewport[0],viewport[1]);
    // Pass this object itself to createDuxcel: its Module owns the pending flag.
    const platform={canvas,dxSurfacePending:true,dxWheel:installCanvasWheel(canvas)};
    platform.dxSurface=createCanvasSurface({canvas,limits:{...CANVAS_SURFACE_LIMITS,maxDimension},
        onResize:()=>{platform.dxSurfacePending=true;}});
    const pagehide=event=>{if(!event.persisted)platform.disposePlatform();};
    platform.disposePlatform=()=>{
        removeEventListener('pagehide',pagehide);
        platform.dxWheel.dispose();platform.dxSurface.dispose();
    };
    addEventListener('pagehide',pagehide);
    return platform;
}
