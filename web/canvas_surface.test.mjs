import test from 'node:test';
import assert from 'node:assert/strict';
import {CANVAS_SURFACE_LIMITS,computeCanvasSurface,createCanvasSurface} from './canvas_surface.mjs';

function target(){
    const listeners=new Map();
    return {listeners,addEventListener(type,fn){const all=listeners.get(type)??new Set();all.add(fn);listeners.set(type,all);},
        removeEventListener(type,fn){listeners.get(type)?.delete(fn);},
        emit(type){for(const fn of [...listeners.get(type)??[]])fn();}};
}
function setup({width=1200,height=800,ratio=2,legacyMedia=false}={}){
    const viewport={...target(),devicePixelRatio:ratio,visualViewport:{...target(),scale:1}},media=[];
    viewport.matchMedia=query=>{
        const eventTarget=target();eventTarget.query=query;media.push(eventTarget);
        if(legacyMedia){eventTarget.addListener=fn=>eventTarget.listeners.set('change',new Set([fn]));eventTarget.removeListener=fn=>eventTarget.listeners.get('change')?.delete(fn);delete eventTarget.addEventListener;delete eventTarget.removeEventListener;}
        return eventTarget;
    };
    const canvas={clientWidth:width,clientHeight:height,width:300,height:150,style:{width:'80vw'},getBoundingClientRect(){throw new Error('Transformed/inset bounding rect is not the logical surface.');}};
    const observers=[];
    class ResizeObserver{
        constructor(fn){this.fn=fn;this.disconnected=false;observers.push(this);}
        observe(value){this.target=value;}
        disconnect(){this.disconnected=true;}
        emit(width,height,target=canvas){this.fn([{target,contentRect:{width,height}}]);}
    }
    const calls=[];
    const surface=createCanvasSurface({canvas,onResize:value=>calls.push(value),window:viewport,ResizeObserver});
    return {viewport,media,canvas,observers,calls,surface};
}

test('DPR1/2 and fractional density preserve CSS viewport, with actual rounded scale',()=>{
    for(const ratio of [1,1.25,1.5,1.75,2,2.625]){
        const result=computeCanvasSurface(1001,701,ratio);
        assert.equal(result.logicalWidth,1001);assert.equal(result.logicalHeight,701);
        assert.equal(result.framebufferWidth,Math.floor(1001*ratio));assert.equal(result.framebufferHeight,Math.floor(701*ratio));
        assert.equal(result.scaleX,result.framebufferWidth/1001);assert.equal(result.scaleY,result.framebufferHeight/701);
    }
});

test('invalid/invisible dimensions are deferred; finite invalid DPR safely becomes1',()=>{
    for(const width of [0,-1,NaN,Infinity,-Infinity,0x80000000])assert.equal(computeCanvasSurface(width,100),null);
    for(const height of [0,-1,NaN,Infinity])assert.equal(computeCanvasSurface(100,height),null);
    for(const ratio of [0,-1,NaN,Infinity,undefined])assert.equal(computeCanvasSurface(100,100,ratio).devicePixelRatio,1);
    const tiny=computeCanvasSurface(.1,.1,2);assert.equal(tiny.logicalWidth,1);assert.equal(tiny.framebufferWidth,2);
});

test('large/hostile viewports bound backing dimension and color pixels without shrinking logical viewport',()=>{
    for(const [width,height,ratio] of [[3840,2160,2],[16384,16384,4],[100000,50,3],[50,100000,2],[2147483647,2147483647,4],[1200,800,1000]]){
        const result=computeCanvasSurface(width,height,ratio);
        assert.equal(result.logicalWidth,width);assert.equal(result.logicalHeight,height);
        assert.ok(result.framebufferWidth<=CANVAS_SURFACE_LIMITS.maxDimension);
        assert.ok(result.framebufferHeight<=CANVAS_SURFACE_LIMITS.maxDimension);
        assert.ok(result.framebufferWidth*result.framebufferHeight<=CANVAS_SURFACE_LIMITS.maxPixels);
        assert.ok(result.scaleX<=CANVAS_SURFACE_LIMITS.maxScale);
    }
    const result=computeCanvasSurface(1000,1000,4,{maxDimension:1024,maxPixels:65536,maxScale:3});
    assert.deepEqual([result.framebufferWidth,result.framebufferHeight],[256,256]);
    for(const [width,height] of [[100000,1],[1,100000],[100000,100000]]){
        const tinyBudget=computeCanvasSurface(width,height,4,{maxPixels:1});
        assert.equal(tinyBudget.framebufferWidth*tinyBudget.framebufferHeight,1);
    }
    for(const limits of [{maxPixels:0},{maxDimension:NaN},{maxScale:Infinity},{maxPixels:1.5}])assert.throws(()=>computeCanvasSurface(10,10,2,limits),RangeError);
});

test('initial sizing uses CSS dimensions and never changes backing attributes or CSS',()=>{
    const {canvas,calls,surface,observers}=setup();
    assert.equal(calls.length,1);assert.equal(surface.current,calls[0]);
    assert.deepEqual([surface.current.logicalWidth,surface.current.logicalHeight,surface.current.framebufferWidth,surface.current.framebufferHeight],[1200,800,2400,1600]);
    assert.deepEqual([canvas.width,canvas.height,canvas.style.width],[300,150,'80vw']);
    assert.equal(observers[0].target,canvas);assert.equal(surface.resize(),false);assert.equal(calls.length,1);surface.dispose();
});

test('ResizeObserver handles actual canvas CSS resizing/insets independently of window and backing',()=>{
    const {canvas,observers,calls,surface}=setup();
    observers[0].emit(999.6,650.4);
    assert.deepEqual([surface.current.logicalWidth,surface.current.logicalHeight,surface.current.framebufferWidth,surface.current.framebufferHeight],[1000,650,2000,1300]);
    observers[0].emit(1000,650);assert.equal(calls.length,2);
    observers[0].emit(500,400,{});assert.equal(calls.length,2);
    canvas.clientWidth=0;canvas.clientHeight=0;observers[0].emit(0,0);assert.equal(calls.length,2);assert.equal(surface.resize(),false);
    canvas.clientWidth=500;canvas.clientHeight=350;observers[0].emit(500,350);assert.equal(calls.length,3);surface.dispose();
});

test('DPR change re-arms resolution watcher and browser zoom resizes without scaling C layout',()=>{
    const {viewport,media,calls,surface}=setup();
    assert.equal(media[0].query,'(resolution: 2dppx)');viewport.devicePixelRatio=1.25;media[0].emit('change');
    assert.equal(calls.length,2);assert.equal(media[0].listeners.get('change').size,0);assert.equal(media[1].query,'(resolution: 1.25dppx)');
    assert.deepEqual([surface.current.logicalWidth,surface.current.framebufferWidth],[1200,1500]);
    viewport.devicePixelRatio=3;viewport.emit('resize');assert.equal(calls.length,3);assert.equal(media.length,3);
    viewport.visualViewport.scale=2;viewport.visualViewport.emit('resize');assert.equal(calls.length,3,'pinch scale does not become UI scale');
    viewport.devicePixelRatio=2;viewport.visualViewport.emit('resize');assert.equal(calls.length,4);surface.dispose();
});

test('legacy resolution listeners and observer-less event/explicit fallback work',()=>{
    const {viewport,media,surface}=setup({legacyMedia:true});viewport.devicePixelRatio=1;media[0].emit('change');
    assert.equal(surface.current.devicePixelRatio,1);surface.dispose();assert.equal(media.at(-1).listeners.get('change').size,0);
    const canvas={clientWidth:500,clientHeight:200},calls=[];
    const fallback=createCanvasSurface({canvas,onResize:value=>calls.push(value),window:viewport,ResizeObserver:null});
    canvas.clientWidth=600;viewport.emit('resize');assert.equal(fallback.current.logicalWidth,600);
    canvas.clientHeight=400;assert.equal(fallback.resize(),true);assert.equal(calls.length,3);fallback.dispose();
});

test('disposal removes all listeners and deferred observer callbacks cannot publish',()=>{
    const {surface,viewport,media,observers,calls}=setup();
    surface.dispose();surface.dispose();assert.equal(observers[0].disconnected,true);
    assert.equal(viewport.listeners.get('resize').size,0);assert.equal(viewport.visualViewport.listeners.get('resize').size,0);assert.equal(media[0].listeners.get('change').size,0);
    observers[0].emit(400,300);viewport.emit('resize');media[0].emit('change');assert.equal(surface.resize(),false);assert.equal(calls.length,1);
});

test('callback failure cleans initial installation; later failures retain prior state for retry',()=>{
    const viewport={...target(),visualViewport:target()},canvas={clientWidth:100,clientHeight:100};
    assert.throws(()=>createCanvasSurface({canvas,window:viewport,ResizeObserver:null,onResize(){throw new Error('C unavailable');}}),/C unavailable/);
    assert.equal(viewport.listeners.get('resize').size,0);assert.equal(viewport.visualViewport.listeners.get('resize').size,0);
    let fail=false,calls=0;
    const surface=createCanvasSurface({canvas,window:viewport,ResizeObserver:null,onResize(){if(fail)throw new Error('apply failed');calls++;}});
    fail=true;canvas.clientWidth=200;assert.throws(()=>surface.resize(),/apply failed/);assert.equal(surface.current.logicalWidth,100);
    fail=false;assert.equal(surface.resize(),true);assert.equal(surface.current.logicalWidth,200);assert.equal(calls,2);surface.dispose();
});

test('invalid host contracts fail before installing observers/listeners',()=>{
    assert.throws(()=>createCanvasSurface(),TypeError);assert.throws(()=>createCanvasSurface({canvas:{}}),TypeError);
    assert.throws(()=>createCanvasSurface({canvas:{},onResize(){},devicePixelRatio:2}),TypeError);
});
