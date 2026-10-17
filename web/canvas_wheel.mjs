// Browser event units/ownership only. C owns hit testing, scrolling and zoom.
// Install before Emscripten creates GLFW's canvas capture listeners: legacy
// GLFW quantizes tiny vertical wheel deltas and uses unlike horizontal units.
const installations=new WeakMap();
const limit=1_000_000, maxEvents=65_535, linePixels=16;
const bounded=value=>Math.max(-limit,Math.min(limit,value));

/** Returns one installation per canvas. drain(into) fills a reusable object:
 * {panX, panY, zoomDelta, x, y, events}. Deltas and pointer coordinates use
 * canvas layout CSS pixels (not framebuffer pixels), including CSS transforms.
 * Positive pan means right/down; positive zoomDelta means zoom out. Ctrl-wheel
 * is browser pinch/zoom and never contributes to pan. C chooses zoom policy.
 *
 * drain clears accumulated deltas/count. reset drops all pending input;
 * setEnabled(false) additionally ignores future deltas while still claiming
 * owned events, useful during a frozen review frame or modal transition.
 * The returned object and caller's drain output are reused; no event queue,
 * synthesized momentum, per-event arrays or spreadsheet geometry live here.
 * Safari's nonstandard gesture events are not converted without physical QA.
 */
export function installCanvasWheel(canvas){
    if(!canvas?.addEventListener||!canvas?.removeEventListener||!canvas?.getBoundingClientRect)
        throw new TypeError('A canvas event target with bounds is required.');
    const installed=installations.get(canvas);if(installed)return installed;
    let panX=0,panY=0,zoomDelta=0,x=0,y=0,events=0,enabled=true,disposed=false;
    const reset=()=>{panX=panY=zoomDelta=x=y=events=0;};
    const claim=event=>{
        if(!event.defaultPrevented)event.preventDefault();
        // GLFW listens on this same target in capture phase. Plain
        // stopPropagation would still deliver to its same-target listener.
        event.stopImmediatePropagation();
    };
    const wheel=event=>{
        if(disposed||event.target!==canvas)return;
        const rect=canvas.getBoundingClientRect();
        if(!(rect.width>0&&rect.height>0))return;
        claim(event);
        if(!enabled)return;
        const width=Number(canvas.clientWidth),height=Number(canvas.clientHeight);
        const sx=width>0?width/rect.width:1,sy=height>0?height/rect.height:1;
        let dx=Number(event.deltaX),dy=Number(event.deltaY);
        if(!Number.isFinite(dx)||!Number.isFinite(dy))return;
        // Trackpads already deliver both axes. Shift remaps a one-axis mouse
        // before page/coordinate transforms, preserving real diagonals.
        if(!event.ctrlKey&&event.shiftKey&&!dx){dx=dy;dy=0;}
        if(event.deltaMode===1){dx*=linePixels;dy*=linePixels;}
        else if(event.deltaMode===2){dx*=rect.width;dy*=rect.height;}
        else if(event.deltaMode!==0&&event.deltaMode!==undefined)return;
        dx=bounded(dx*sx);dy=bounded(dy*sy);
        if(!dx&&!dy)return;
        if(event.ctrlKey)zoomDelta=bounded(zoomDelta+dy);
        else{
            panX=bounded(panX+dx);panY=bounded(panY+dy);
        }
        x=Number.isFinite(event.clientX)?(event.clientX-rect.left)*sx:0;
        y=Number.isFinite(event.clientY)?(event.clientY-rect.top)*sy:0;
        events=Math.min(events+1,maxEvents);
    };
    // All supported browsers emit `wheel`. Suppress the deprecated additional
    // event over our canvas so GLFW cannot process that same movement twice.
    const legacy=event=>{if(!disposed&&event.target===canvas)claim(event);};
    const api={
        drain(into){
            if(!into||typeof into!=='object')throw new TypeError('A reusable wheel output object is required.');
            into.panX=panX;into.panY=panY;into.zoomDelta=zoomDelta;
            into.x=x;into.y=y;into.events=events;
            panX=panY=zoomDelta=events=0;return into;
        },
        reset,
        setEnabled(value){enabled=Boolean(value);reset();},
        dispose(){
            if(disposed)return;disposed=true;reset();
            canvas.removeEventListener('wheel',wheel,true);
            canvas.removeEventListener('mousewheel',legacy,true);
            if(installations.get(canvas)===api)installations.delete(canvas);
        }
    };
    canvas.addEventListener('wheel',wheel,{capture:true,passive:false});
    canvas.addEventListener('mousewheel',legacy,{capture:true,passive:false});
    installations.set(canvas,api);return api;
}
