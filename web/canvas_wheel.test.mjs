import test from 'node:test';
import assert from 'node:assert/strict';
import {installCanvasWheel} from './canvas_wheel.mjs';

function setup(options={}){
    const listeners=new Map();
    let rect={left:20,top:30,width:800,height:600,...options.rect};
    const canvas={clientWidth:800,clientHeight:600,width:1600,height:1200,...options.canvas,
        getBoundingClientRect(){return rect;},
        addEventListener(type,listener,options){const entries=listeners.get(type)??[];entries.push({listener,options});listeners.set(type,entries);},
        removeEventListener(type,listener){listeners.set(type,(listeners.get(type)??[]).filter(entry=>entry.listener!==listener));}
    };
    function dispatch(overrides={},type='wheel'){
        const event={target:canvas,deltaX:0,deltaY:0,deltaMode:0,clientX:120,clientY:230,ctrlKey:false,shiftKey:false,
            defaultPrevented:false,preventions:0,stopped:false,
            preventDefault(){this.defaultPrevented=true;this.preventions++;},
            stopImmediatePropagation(){this.stopped=true;},...overrides};
        for(const {listener} of listeners.get(type)??[]){listener(event);if(event.stopped)break;}
        return event;
    }
    return {canvas,listeners,dispatch,setRect(value){rect=value;}};
}
const near=(actual,expected)=>assert.ok(Math.abs(actual-expected)<1e-10,`${actual} != ${expected}`);

test('pixel fractions and simultaneous axes accumulate without native detents',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};
    dispatch({deltaX:0.125,deltaY:0.0625});dispatch({deltaX:0.25,deltaY:0.1875});
    assert.equal(wheel.drain(out),out);
    assert.deepEqual(out,{panX:.375,panY:.25,zoomDelta:0,x:100,y:200,events:2});
    wheel.drain(out);assert.deepEqual(out,{panX:0,panY:0,zoomDelta:0,x:100,y:200,events:0});
});

test('small momentum-like tails retain their sum and are never rounded up',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};let total=0;
    for(let n=1;n<=256;n++){const delta=1/(n*n);total+=delta;dispatch({deltaY:delta});}
    wheel.drain(out);near(out.panY,total);assert.equal(out.panX,0);assert.equal(out.events,256);
    assert.ok(out.panY<2); // Old minimum detent routing would produce 256.
});

test('opposite directions cancel accurately within one frame',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};
    dispatch({deltaX:12.5,deltaY:-25.125});dispatch({deltaX:-12.5,deltaY:25.125});
    wheel.drain(out);assert.equal(out.panX,0);assert.equal(out.panY,0);assert.equal(out.events,2);
});

test('line and page delta modes use consistent CSS-pixel axes',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};
    dispatch({deltaX:1.5,deltaY:-2.25,deltaMode:1});wheel.drain(out);
    assert.equal(out.panX,24);assert.equal(out.panY,-36);
    dispatch({deltaX:-.5,deltaY:.25,deltaMode:2});wheel.drain(out);
    assert.equal(out.panX,-400);assert.equal(out.panY,150);
});

test('Shift maps a vertical-only wheel to horizontal, but preserves a real diagonal',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};
    dispatch({deltaY:-12.25,shiftKey:true});wheel.drain(out);
    assert.equal(out.panX,-12.25);assert.equal(out.panY,0);
    dispatch({deltaX:.001,deltaY:12.25,shiftKey:true});wheel.drain(out);
    assert.equal(out.panX,.001);assert.equal(out.panY,12.25);
});

test('Ctrl-wheel pinch is separate from pan and does not Shift-remap',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};
    dispatch({deltaX:3,deltaY:-4.125,ctrlKey:true,shiftKey:true});wheel.drain(out);
    assert.equal(out.panX,0);assert.equal(out.panY,0);assert.equal(out.zoomDelta,-4.125);
    dispatch({deltaY:10,ctrlKey:true});dispatch({deltaY:3});wheel.drain(out);
    assert.equal(out.panY,3);assert.equal(out.zoomDelta,10);
});

test('owned wheel prevents browser scrolling and later GLFW listeners exactly once',()=>{
    const {canvas,dispatch,listeners}=setup(),wheel=installCanvasWheel(canvas);let legacyCalls=0;
    canvas.addEventListener('wheel',()=>legacyCalls++,true);
    assert.deepEqual(listeners.get('wheel')[0].options,{capture:true,passive:false});
    const event=dispatch({deltaY:1});assert.equal(event.preventions,1);assert.equal(event.stopped,true);assert.equal(legacyCalls,0);
    const prevented=dispatch({deltaY:1,defaultPrevented:true});assert.equal(prevented.preventions,0);assert.equal(prevented.stopped,true);
    wheel.dispose();
});

test('wheel ownership is exact canvas target and is independent of keyboard focus',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};let downstream=0;
    canvas.addEventListener('wheel',()=>downstream++);
    const outside=dispatch({target:{tagName:'INPUT'},deltaY:99});
    assert.equal(outside.defaultPrevented,false);assert.equal(outside.stopped,false);assert.equal(downstream,1);
    wheel.drain(out);assert.equal(out.events,0);
    // Hover scrolling must work before a canvas click/focus.
    canvas.ownerDocument={activeElement:{tagName:'INPUT'}};
    assert.equal(dispatch({deltaY:1}).defaultPrevented,true);wheel.drain(out);assert.equal(out.panY,1);
});

test('deprecated duplicate mousewheel events are consumed without double movement',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};let downstream=0;
    canvas.addEventListener('mousewheel',()=>downstream++);
    dispatch({deltaY:6.25});const duplicate=dispatch({wheelDeltaY:120},'mousewheel');
    wheel.drain(out);assert.equal(out.panY,6.25);assert.equal(out.events,1);
    assert.equal(duplicate.defaultPrevented,true);assert.equal(downstream,0);
    const outside=dispatch({target:{},wheelDeltaY:120},'mousewheel');
    assert.equal(outside.defaultPrevented,false);assert.equal(downstream,1);
});

test('DPR framebuffer dimensions do not scale pan distances or pointer coordinates',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};
    for(const dpr of [1,1.25,2,3]){
        canvas.width=800*dpr;canvas.height=600*dpr;
        dispatch({deltaX:3.25,deltaY:4.5});wheel.drain(out);
        assert.equal(out.panX,3.25);assert.equal(out.panY,4.5);assert.equal(out.x,100);assert.equal(out.y,200);
    }
});

test('inset and CSS-transformed canvases use their actual local layout units',()=>{
    const {canvas,dispatch}=setup({rect:{left:50,top:100,width:400,height:150}}),wheel=installCanvasWheel(canvas),out={};
    dispatch({clientX:250,clientY:175,deltaX:3,deltaY:5});wheel.drain(out);
    assert.deepEqual(out,{panX:6,panY:20,zoomDelta:0,x:400,y:300,events:1});
    dispatch({deltaX:0,deltaY:5,shiftKey:true});wheel.drain(out);
    assert.equal(out.panX,10);assert.equal(out.panY,0);
    dispatch({deltaY:.5,deltaMode:2,shiftKey:true});wheel.drain(out);
    assert.equal(out.panX,400);assert.equal(out.panY,0);
    dispatch({deltaX:1,deltaY:1,deltaMode:2});wheel.drain(out);
    assert.equal(out.panX,800);assert.equal(out.panY,600);
});

test('resize geometry is read at delivery, not frozen at installation',()=>{
    const {canvas,dispatch,setRect}=setup(),wheel=installCanvasWheel(canvas),out={};
    canvas.clientWidth=400;canvas.clientHeight=300;setRect({left:0,top:0,width:400,height:300});
    dispatch({deltaX:.5,deltaY:.5,deltaMode:2,clientX:200,clientY:150});wheel.drain(out);
    assert.equal(out.panX,200);assert.equal(out.panY,150);assert.equal(out.x,200);assert.equal(out.y,150);
});

test('reset and modal disable discard queued movement without replaying after enable',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};
    dispatch({deltaY:5});wheel.reset();wheel.drain(out);assert.equal(out.events,0);assert.equal(out.panY,0);
    dispatch({deltaY:10});wheel.setEnabled(false);
    assert.equal(dispatch({deltaY:99,ctrlKey:true}).defaultPrevented,true);
    wheel.drain(out);assert.equal(out.panY,0);assert.equal(out.zoomDelta,0);assert.equal(out.events,0);
    wheel.setEnabled(true);dispatch({deltaY:.125});wheel.drain(out);assert.equal(out.panY,.125);assert.equal(out.events,1);
});

test('malformed or unknown delta modes cannot poison subsequent valid input',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};
    for(const invalid of [{deltaX:NaN,deltaY:5},{deltaX:2,deltaY:Infinity},{deltaX:-Infinity},{deltaY:1,deltaMode:3}])
        assert.equal(dispatch(invalid).defaultPrevented,true);
    wheel.drain(out);assert.equal(out.events,0);assert.equal(out.panX,0);assert.equal(out.panY,0);
    dispatch({deltaY:.25});wheel.drain(out);assert.equal(out.panY,.25);
});

test('zero delta does not create pending work; zero-size canvas preserves outside defaults',()=>{
    const {canvas,dispatch,setRect}=setup(),wheel=installCanvasWheel(canvas),out={};
    dispatch();wheel.drain(out);assert.equal(out.events,0);
    setRect({left:0,top:0,width:0,height:300});assert.equal(dispatch({deltaY:1}).defaultPrevented,false);
    setRect({left:0,top:0,width:400,height:0});assert.equal(dispatch({deltaY:1}).defaultPrevented,false);
    wheel.drain(out);assert.equal(out.events,0);
});

test('bounded accumulation and event count cannot overflow from event flooding',()=>{
    const {canvas,dispatch}=setup(),wheel=installCanvasWheel(canvas),out={};
    dispatch({deltaX:Number.MAX_VALUE,deltaY:-Number.MAX_VALUE});wheel.drain(out);
    assert.equal(out.panX,1_000_000);assert.equal(out.panY,-1_000_000);
    for(let n=0;n<65_540;n++)dispatch({deltaY:1});wheel.drain(out);
    assert.equal(out.events,65_535);assert.equal(out.panY,65_540);
});

test('install/dispose are idempotent and stale disposal cannot remove a new installation',()=>{
    const {canvas,listeners,dispatch}=setup(),first=installCanvasWheel(canvas);
    assert.equal(installCanvasWheel(canvas),first);assert.equal(listeners.get('wheel').length,1);
    dispatch({deltaY:5});first.dispose();first.dispose();assert.equal(listeners.get('wheel').length,0);
    assert.equal(dispatch({deltaY:1}).defaultPrevented,false);const out={};first.drain(out);assert.equal(out.events,0);
    const second=installCanvasWheel(canvas);assert.notEqual(first,second);first.dispose();
    assert.equal(dispatch({deltaY:.5}).defaultPrevented,true);second.drain(out);assert.equal(out.panY,.5);second.dispose();
});

test('independent canvases have independent buffers and disposal',()=>{
    const a=setup(),b=setup(),wa=installCanvasWheel(a.canvas),wb=installCanvasWheel(b.canvas),out={};
    a.dispatch({deltaY:5});b.dispatch({deltaX:2});wa.dispose();wb.drain(out);
    assert.equal(out.panX,2);assert.equal(out.panY,0);assert.equal(out.events,1);wb.dispose();
});

test('invalid installation/output contracts fail early',()=>{
    for(const invalid of [null,{}, {addEventListener(){},removeEventListener(){}}])assert.throws(()=>installCanvasWheel(invalid),TypeError);
    const {canvas}=setup(),wheel=installCanvasWheel(canvas);
    for(const invalid of [undefined,null,1,'text'])assert.throws(()=>wheel.drain(invalid),TypeError);
});
