// Platform loader only. All workbook controls and rendering are shared C99.
import createDuxcel from './duxcel.mjs';
import {createEngineHost} from './engine_host.mjs';
const canvas=document.getElementById('canvas');
canvas.addEventListener('contextmenu',event=>event.preventDefault());
try {
    // The workbook is opaque. Browser compositing of a straight-alpha GL
    // framebuffer would brighten every antialiased glyph a second time.
    if(!canvas.getContext('webgl',{alpha:false,antialias:false,premultipliedAlpha:false}))
        throw new Error('WebGL is unavailable.');
    const vendor=await (await fetch('./vendor.json')).json();
    const dxEngine=createEngineHost({moduleURL:new URL(vendor.worker.moduleURL,location.href).href,workerURL:new URL(vendor.worker.workerURL,location.href).href});
    await createDuxcel({canvas,dxEngine,print:message=>console.log(message),printErr:message=>console.error(message)});
    canvas.focus({preventScroll:true});
} catch (error) {
    console.error(error);
    const message=document.getElementById('error'); message.textContent=String(error); message.style.display='block';
}
