// Browser lifetime and device translation only. All widgets remain shared C.
import {createCanvasSurface,CANVAS_SURFACE_LIMITS} from './canvas_surface.mjs';
import {installCanvasWheel} from './canvas_wheel.mjs';
import {createBrowserClipboard} from './clipboard.mjs';
import {createBrowserDownload} from './download.mjs';
export function createBrowserPlatform(canvas,gl){
    const viewport=gl.getParameter(gl.MAX_VIEWPORT_DIMS);
    const maxDimension=Math.min(CANVAS_SURFACE_LIMITS.maxDimension,
        gl.getParameter(gl.MAX_RENDERBUFFER_SIZE),viewport[0],viewport[1]);
    // Pass this object itself to createDuxcel: its Module owns the pending flag.
    const platform={canvas,dxSurfacePending:true,dxWheel:installCanvasWheel(canvas)};
    platform.dxClipboard=createBrowserClipboard({writeText:text=>{
        const clipboard=globalThis.navigator?.clipboard;
        if(typeof clipboard?.writeText!=='function')throw new Error('Clipboard writing is unavailable in this browser or context.');
        return clipboard.writeText(text);
    }});
    platform.dxDownload=createBrowserDownload();
    platform.dxSurface=createCanvasSurface({canvas,limits:{...CANVAS_SURFACE_LIMITS,maxDimension},
        onResize:()=>{platform.dxSurfacePending=true;}});
    const pagehide=event=>{if(!event.persisted)platform.disposePlatform();};
    platform.disposePlatform=()=>{
        removeEventListener('pagehide',pagehide);
        platform.dxWheel.dispose();platform.dxSurface.dispose();platform.dxClipboard.dispose();platform.dxDownload.dispose();
    };
    addEventListener('pagehide',pagehide);
    return platform;
}
