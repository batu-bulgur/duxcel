// Review-only platform loader. The C registry, picker and composer are shared
// with native review; this module supplies browser lifetime/export transport.
import createDuxcel from './duxcel.mjs';
import {createEngineHost} from './engine_host.mjs';
import {createReviewHost} from './review_host.mjs';
import {installCanvasInput} from './canvas_input.mjs';
const canvas=document.getElementById('canvas');
canvas.addEventListener('contextmenu',event=>event.preventDefault());
const disposeInput=installCanvasInput(canvas);
try {
    if(!canvas.getContext('webgl',{alpha:false,antialias:false,premultipliedAlpha:false}))throw new Error('WebGL is unavailable.');
    const [vendor,build]=await Promise.all([fetch('./vendor.json').then(r=>r.json()),fetch('./build.json').then(r=>r.json())]);
    const dxEngine=createEngineHost({moduleURL:new URL(vendor.worker.moduleURL,location.href).href,workerURL:new URL(vendor.worker.workerURL,location.href).href});
    const dxReview=createReviewHost({canvas,build});
    await createDuxcel({canvas,dxEngine,dxReview,print:message=>console.log(message),printErr:message=>console.error(message)});
    canvas.focus({preventScroll:true});
} catch(error) {
    disposeInput();
    console.error(error);const message=document.getElementById('error');message.textContent=String(error);message.style.display='block';
}
