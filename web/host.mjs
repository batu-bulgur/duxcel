// Platform loader only. All workbook controls and rendering are shared C99.
import createDuxcel from './duxcel.mjs';
import {createEngineHost} from './engine_host.mjs';
import {installCanvasInput} from './canvas_input.mjs';
import {createBrowserPlatform} from './browser_platform.mjs';
const canvas=document.getElementById('canvas');
canvas.addEventListener('contextmenu',event=>event.preventDefault());
const disposeInput=installCanvasInput(canvas);
let platform;
try {
    // The workbook is opaque. Browser compositing of a straight-alpha GL
    // framebuffer would brighten every antialiased glyph a second time.
    const gl=canvas.getContext('webgl',{alpha:false,antialias:false,premultipliedAlpha:false});
    if(!gl)throw new Error('WebGL is unavailable.');
    platform=createBrowserPlatform(canvas,gl);
    const vendor=await (await fetch('./vendor.json')).json();
    const dxEngine=createEngineHost({moduleURL:new URL(vendor.worker.moduleURL,location.href).href,workerURL:new URL(vendor.worker.workerURL,location.href).href});
    await createDuxcel(Object.assign(platform,{dxEngine,print:message=>console.log(message),printErr:message=>console.error(message)}));
    canvas.focus({preventScroll:true});
} catch (error) {
    disposeInput();
    platform?.disposePlatform();
    console.error(error);
    const message=document.getElementById('error'); message.textContent=String(error); message.style.display='block';
}
