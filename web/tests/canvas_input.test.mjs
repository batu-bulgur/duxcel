import test from 'node:test';
import assert from 'node:assert/strict';
import {installCanvasInput} from '../canvas_input.mjs';

function setup(){
    const listeners=new Map(),document={activeElement:null};
    const canvas={ownerDocument:document,
        addEventListener(type,listener,options){const entries=listeners.get(type)??[];entries.push({listener,options});listeners.set(type,entries);},
        removeEventListener(type,listener){listeners.set(type,(listeners.get(type)??[]).filter(entry=>entry.listener!==listener));}
    };
    document.activeElement=canvas;
    function dispatch(key,overrides={}){
        const event={key,target:canvas,isComposing:false,keyCode:0,ctrlKey:false,altKey:false,metaKey:false,shiftKey:false,defaultPrevented:false,preventions:0,
            preventDefault(){this.defaultPrevented=true;this.preventions++;},
            stopPropagation(){assert.fail('GLFW must still receive the event');},
            stopImmediatePropagation(){assert.fail('GLFW must still receive the event');},...overrides};
        for(const {listener} of listeners.get('keydown')??[])listener(event);
        return event;
    }
    return {canvas,document,listeners,dispatch};
}

test('focused canvas claims only the eight navigation keys and still delivers them to GLFW',()=>{
    const {canvas,listeners,dispatch}=setup();const dispose=installCanvasInput(canvas);let glfwEvents=0;
    canvas.addEventListener('keydown',()=>glfwEvents++);
    assert.deepEqual(listeners.get('keydown')[0].options,{capture:false,passive:false});
    for(const key of ['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Home','End','PageUp','PageDown']){
        const event=dispatch(key);assert.equal(event.defaultPrevented,true,key);assert.equal(event.preventions,1,key);
    }
    assert.equal(glfwEvents,8);dispose();
});

test('Shift navigation remains claimed; printable keys, Space and unrelated controls retain defaults',()=>{
    const {canvas,dispatch}=setup();installCanvasInput(canvas);
    for(const key of ['ArrowUp','Home','End','PageDown'])assert.equal(dispatch(key,{shiftKey:true}).defaultPrevented,true);
    for(const key of ['a','9','İ',' ','Spacebar','Enter','Tab','Escape','Backspace','F5','Unidentified','arrowup'])assert.equal(dispatch(key).defaultPrevented,false,key);
});

test('other targets and an unfocused canvas retain their defaults',()=>{
    const {canvas,document,dispatch}=setup();installCanvasInput(canvas);
    const input={tagName:'INPUT'};
    assert.equal(dispatch('ArrowUp',{target:input}).defaultPrevented,false);
    document.activeElement=input;assert.equal(dispatch('ArrowUp').defaultPrevented,false);
    document.activeElement=null;assert.equal(dispatch('Home').defaultPrevented,false);
    document.activeElement=canvas;assert.equal(dispatch('ArrowUp').defaultPrevented,true);
});

test('composition and browser modifier shortcuts retain defaults',()=>{
    const {canvas,dispatch}=setup();installCanvasInput(canvas);
    assert.equal(dispatch('ArrowUp',{isComposing:true}).defaultPrevented,false);
    assert.equal(dispatch('ArrowUp',{keyCode:229}).defaultPrevented,false);
    for(const modifier of ['ctrlKey','altKey','metaKey'])for(const key of ['ArrowLeft','Home','PageUp','r','l'])assert.equal(dispatch(key,{[modifier]:true}).defaultPrevented,false,modifier+' '+key);
});

test('an existing default prevention remains intact without duplicate prevention',()=>{
    const {canvas,dispatch}=setup();installCanvasInput(canvas);
    const event=dispatch('ArrowUp',{defaultPrevented:true});assert.equal(event.defaultPrevented,true);assert.equal(event.preventions,0);
});

test('bare Alt belongs to C keytips, with composition and OS chords preserved',()=>{
    const {canvas,document,dispatch}=setup();installCanvasInput(canvas);
    assert.equal(dispatch('Alt',{altKey:true}).preventions,1);
    assert.equal(dispatch('Alt',{shiftKey:true,altKey:true}).preventions,1);
    for(const modifiers of [{ctrlKey:true},{metaKey:true},{isComposing:true},{keyCode:229}])
        assert.equal(dispatch('Alt',modifiers).defaultPrevented,false);
    for(const key of ['AltGraph','å','v'])assert.equal(dispatch(key,{altKey:true}).defaultPrevented,false);
    assert.equal(dispatch('Alt',{defaultPrevented:true}).preventions,0);
    document.activeElement={};assert.equal(dispatch('Alt').defaultPrevented,false);
});

test('repeated installs share one listener; disposal and reinstall cannot remove another installation',()=>{
    const {canvas,listeners,dispatch}=setup();
    const first=installCanvasInput(canvas),second=installCanvasInput(canvas);assert.equal(first,second);assert.equal(listeners.get('keydown').length,1);
    assert.equal(dispatch('ArrowDown').preventions,1);second();assert.equal(listeners.get('keydown').length,0);
    first();assert.equal(dispatch('ArrowDown').defaultPrevented,false);
    const next=installCanvasInput(canvas);assert.notEqual(next,first);first();assert.equal(dispatch('ArrowDown').defaultPrevented,true);
    next();next();assert.equal(listeners.get('keydown').length,0);
});

test('independent canvases use their own document focus and lifecycle',()=>{
    const a=setup(),b=setup();const disposeA=installCanvasInput(a.canvas),disposeB=installCanvasInput(b.canvas);
    disposeA();assert.equal(a.dispatch('End').defaultPrevented,false);assert.equal(b.dispatch('End').defaultPrevented,true);disposeB();
});

test('primary G belongs to the C Go To handler while GLFW still receives it',()=>{
    const {canvas,dispatch}=setup();installCanvasInput(canvas);let delivered=0;
    canvas.addEventListener('keydown',()=>delivered++);
    for(const modifiers of [{ctrlKey:true},{metaKey:true},{ctrlKey:true,metaKey:true}])for(const key of ['g','G']){
        const event=dispatch(key,{...modifiers,repeat:true});assert.equal(event.defaultPrevented,true);assert.equal(event.preventions,1);
    }
    assert.equal(delivered,6);assert.equal(dispatch('g',{ctrlKey:true,defaultPrevented:true}).preventions,0);
});

test('Go To default ownership does not escape canvas focus or claim other G chords',()=>{
    const {canvas,document,dispatch}=setup();installCanvasInput(canvas);
    for(const fields of [{ctrlKey:true,shiftKey:true},{metaKey:true,altKey:true},{ctrlKey:true,altKey:true},{ctrlKey:true,isComposing:true},{metaKey:true,keyCode:229},{}])assert.equal(dispatch('g',fields).defaultPrevented,false);
    const input={tagName:'INPUT'};
    assert.equal(dispatch('g',{ctrlKey:true,target:input}).defaultPrevented,false);
    document.activeElement=input;assert.equal(dispatch('g',{ctrlKey:true}).defaultPrevented,false);
    document.activeElement=canvas;assert.equal(dispatch('F5',{ctrlKey:true}).defaultPrevented,false);assert.equal(dispatch('F12').defaultPrevented,false);
});
