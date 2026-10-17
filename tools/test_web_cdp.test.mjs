import test from 'node:test';
import assert from 'node:assert/strict';
import {createCDP} from './test_web_cdp.mjs';

class Socket {
    readyState=1; listeners=new Map(); sent=[];
    addEventListener(name,listener) { const listeners=this.listeners.get(name)??[]; listeners.push(listener); this.listeners.set(name,listeners); }
    emit(name,event) { for (const listener of this.listeners.get(name)??[]) listener(event); }
    send(text) { if (this.sendError) throw this.sendError; this.sent.push(JSON.parse(text)); }
    reply(id,result) { this.emit('message',{data:JSON.stringify({id,result})}); }
}
function setup() {
    const socket=new Socket(),events=[],timers=new Map(); let time=1000,timerId=0;
    const cdp=createCDP(socket,events,{now:()=>time,setTimer:(callback,delay)=>{const id=++timerId;timers.set(id,{callback,delay});return id;},clearTimer:id=>timers.delete(id)});
    return {socket,events,timers,cdp,timeout(id,elapsed){time+=elapsed;const timer=timers.get(id);assert.ok(timer);timer.callback();}};
}

test('requests retain identity, phase and timing; replies clear timers and events remain separate',async()=>{
    const {socket,events,timers,cdp}=setup();
    const first=cdp.send('Runtime.evaluate',{},15000,'ArrowUp frames_after_keyDown');
    const second=cdp.send('Page.captureScreenshot');
    assert.deepEqual(cdp.pending(),[
        {id:1,method:'Runtime.evaluate',started_ms:1000,timeout_ms:15000,context:'ArrowUp frames_after_keyDown'},
        {id:2,method:'Page.captureScreenshot',started_ms:1000,timeout_ms:15000,context:''}
    ]);
    const detached=cdp.pending();detached[0].method='modified';assert.equal(cdp.pending()[0].method,'Runtime.evaluate');
    socket.emit('message',{data:JSON.stringify({method:'Debugger.paused',params:{callFrames:[{functionName:'app_frame'}]}})});
    socket.reply(2,{data:'png'});assert.deepEqual(await second,{data:'png'});
    socket.reply(1,{result:{value:true}});assert.deepEqual(await first,{result:{value:true}});
    assert.equal(events[0].method,'Debugger.paused');assert.equal(timers.size,0);assert.deepEqual(cdp.pending(),[]);
});

test('timeout records the expired request and pending snapshot; late response cannot settle another request',async()=>{
    const {socket,timers,cdp,timeout}=setup();
    const first=cdp.send('Runtime.evaluate',{},15000,'Enter frames_after_keyUp');
    const rejected=assert.rejects(first,/Timed out: Runtime.evaluate \(CDP #1 Enter frames_after_keyUp\)/);
    const second=cdp.send('Runtime.evaluate',{},2000,'failure visibility');
    timeout(1,15000);await rejected;
    assert.equal(cdp.failure.kind,'timeout');assert.equal(cdp.failure.time,16000);
    assert.equal(cdp.failure.request.started_ms,1000);assert.equal(cdp.failure.pending.length,2);
    assert.deepEqual(cdp.pending().map(item=>item.id),[2]);
    socket.reply(1,{result:{value:'late'}});assert.equal(timers.size,1);
    socket.reply(2,{result:{value:'visible'}});assert.deepEqual(await second,{result:{value:'visible'}});
});

for (const event of ['close','error']) test(`WebSocket ${event} promptly rejects every pending request and future sends`,async()=>{
    const {socket,timers,cdp}=setup();
    const first=cdp.send('Runtime.evaluate'),second=cdp.send('Input.dispatchKeyEvent');
    const rejected=Promise.all([assert.rejects(first,/CDP WebSocket/),assert.rejects(second,/CDP WebSocket/)]);
    socket.emit(event,{code:1006,reason:'renderer gone',message:'transport lost'});await rejected;
    assert.equal(cdp.failure.kind,'disconnected');assert.equal(cdp.failure.pending.length,2);
    assert.equal(timers.size,0);assert.deepEqual(cdp.pending(),[]);
    await assert.rejects(cdp.send('Debugger.pause'),/CDP WebSocket/);assert.equal(socket.sent.length,2);
    if(event==='close')assert.match(cdp.disconnected,/1006: renderer gone/);
    else assert.match(cdp.disconnected,/transport lost/);
});

test('closed readyState rejects without silently discarding a request',async()=>{
    const {socket,cdp,timers}=setup();socket.readyState=3;
    await assert.rejects(cdp.send('Runtime.evaluate'),/not open \(state 3\)/);
    assert.equal(socket.sent.length,0);assert.equal(timers.size,0);
});

test('protocol and synchronous send failures retain request metadata and clear pending timers',async()=>{
    const {socket,cdp,timers}=setup();
    const request=cdp.send('Debugger.pause',{},2000,'failure debugger');
    const rejected=assert.rejects(request,/CDP Debugger.pause:.*-32000/);
    socket.emit('message',{data:JSON.stringify({id:1,error:{code:-32000,message:'Not available'}})});await rejected;
    assert.equal(cdp.failure.kind,'protocol');assert.equal(cdp.failure.request.context,'failure debugger');
    socket.sendError=new Error('broken socket');await assert.rejects(cdp.send('Runtime.evaluate'),/send failed: broken socket/);
    assert.equal(cdp.failure.kind,'send');assert.equal(cdp.failure.request.id,2);assert.equal(timers.size,0);
});

test('cleanup releases raw modifiers and a timed-out keyDown without replaying input or waiting for frames',async()=>{
    const {socket,cdp,timeout}=setup();
    const shift=cdp.send('Input.dispatchKeyEvent',{type:'keyDown',key:'Shift',code:'ShiftLeft',windowsVirtualKeyCode:16,modifiers:8});socket.reply(1,{});await shift;
    const up=cdp.send('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowUp',code:'ArrowUp',windowsVirtualKeyCode:38,modifiers:8});
    const rejected=assert.rejects(up,/Timed out/);timeout(2,15000);await rejected;
    assert.deepEqual(cdp.heldKeys().map(item=>item.key),['Shift','ArrowUp']);
    const cleanup=cdp.releaseHeldKeys();
    assert.deepEqual(socket.sent.slice(2).map(item=>[item.params.type,item.params.key,item.params.modifiers]),[['keyUp','ArrowUp',0],['keyUp','Shift',0]]);
    assert.deepEqual(cdp.pending().map(item=>item.timeout_ms),[1000,1000]);
    socket.reply(3,{});socket.reply(4,{});
    assert.deepEqual(await cleanup,[{key:'ArrowUp',code:'ArrowUp',status:'released'},{key:'Shift',code:'ShiftLeft',status:'released'}]);
    assert.deepEqual(cdp.heldKeys(),[]);assert.deepEqual(await cdp.releaseHeldKeys(),[]);
});

test('acknowledged keyUp is not repeated; disconnected cleanup explicitly reports unreleased keys',async()=>{
    const {socket,cdp}=setup();
    const down=cdp.send('Input.dispatchKeyEvent',{type:'keyDown',key:'a',code:'KeyA'});socket.reply(1,{});await down;
    const up=cdp.send('Input.dispatchKeyEvent',{type:'keyUp',key:'a',code:'KeyA'});socket.reply(2,{});await up;
    assert.deepEqual(await cdp.releaseHeldKeys(),[]);
    const shift=cdp.send('Input.dispatchKeyEvent',{type:'keyDown',key:'Shift',code:'ShiftLeft'});socket.reply(3,{});await shift;
    socket.emit('close',{code:1006});const cleanup=await cdp.releaseHeldKeys();
    assert.equal(cleanup.length,1);assert.equal(cleanup[0].status,'failed');assert.match(cleanup[0].error,/closed/);
    assert.equal(socket.sent.length,3);
});
