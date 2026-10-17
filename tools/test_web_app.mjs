#!/usr/bin/env node
// Headless Chromium driver for the local shared C UI. Its Python/server parent
// owns the cross-worktree desktop lock. No user browser/profile is accessed.
import {spawn} from 'node:child_process';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {createHash} from 'node:crypto';
import {createCDP} from './test_web_cdp.mjs';
import {cdpKeyEvent} from './cdp_keyboard.mjs';
const url=process.argv[2]??'http://127.0.0.1:8767';
const output=path.resolve(process.argv[3]??'build/web/browser-qa/cdp');
const workerProbe=process.argv.includes('--worker-probe');
const workerDriver=process.argv.includes('--worker-driver');
const filters=process.argv.includes('--filters');
const busy=process.argv.includes('--busy');
const review=process.argv.includes('--review');
const accessOnly=process.argv.includes('--access-only');
const zoomOnly=process.argv.includes('--zoom-only');
const reviewControlsOnly=process.argv.includes('--review-controls-only');
const analysisOnly=process.argv.includes('--analysis-only');
const reviewOffline=process.argv.includes('--review-offline');
const canvasTransform=process.argv.includes('--canvas-transform');
const platform=process.argv.includes('--platform');
const altStress=process.argv.includes('--alt-stress');
const persistence=process.argv.includes('--persistence');
const sourceIndex=process.argv.indexOf('--source');const sourceFile=sourceIndex<0?null:process.argv[sourceIndex+1];
if(accessOnly&&(!review||['--analysis-only','--review-controls-only','--source','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline'].some(flag=>process.argv.includes(flag))))throw new Error('--access-only requires --review and excludes source/engine/platform/report-upload options');
if(zoomOnly&&(!review||['--analysis-only','--review-controls-only','--access-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline'].some(flag=>process.argv.includes(flag))))throw new Error('--zoom-only requires --review and excludes access/engine/platform/report-upload options');
if(reviewControlsOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||['--analysis-only','--access-only','--zoom-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--review-controls-only requires --review/transactions.csv and excludes access/zoom/engine/platform/report-upload options');
if(analysisOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||['--access-only','--zoom-only','--review-controls-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--analysis-only requires --review/transactions.csv and excludes other focused profiles or report-upload options');
await fs.mkdir(output,{recursive:true});
const profile=await fs.mkdtemp(path.join(os.tmpdir(),'duxcel-browser-'));
const browser=spawn(process.env.DUXCEL_CHROMIUM??'/Applications/Chromium.app/Contents/MacOS/Chromium',
 ['--headless','--disable-background-timer-throttling','--disable-renderer-backgrounding','--disable-backgrounding-occluded-windows','--remote-debugging-port=0','--user-data-dir='+profile,'--no-first-run','--no-default-browser-check','--no-proxy-server','about:blank'],{stdio:['ignore','ignore','pipe']});
await fs.writeFile(path.join(output,'browser-process.json'),JSON.stringify({pid:browser.pid,parent_pid:process.pid,started_ms:Date.now(),headless:true,profile:'owned temporary profile'},null,2));
const dprIndex=process.argv.indexOf('--dpr'),dpr=dprIndex<0?1:Number(process.argv[dprIndex+1]);
let sourceResult;
let filterResult;
let busyResult;
let reviewResult;
let platformResult;
let accessKeyResult;
let persistenceResult;
let zoomResult;
let reviewControlsResult;
let analysisResult;
let stderr='',ws,cdp,activePhase='';const events=[],steps=[];let checks=0;const started=Date.now();
function check(value,message){checks++;if(!value)throw new Error(message);}
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function send(method,params={},timeout=15000){
 if(!cdp)return Promise.reject(new Error('CDP WebSocket is not connected'));
 const context=activePhase||(method==='Runtime.evaluate'?params.expression.slice(0,160):method==='Input.dispatchKeyEvent'?params.type+' '+params.key:'');
 return cdp.send(method,params,timeout,context);
}
async function evaluate(expression,timeout){const result=await send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true},timeout);if(result.exceptionDetails)throw new Error(JSON.stringify(result.exceptionDetails));return result.result?.value;}
async function frames(count=2){await evaluate(`new Promise(resolve=>{let n=${count};function tick(){if(--n<=0)resolve(true);else requestAnimationFrame(tick)}requestAnimationFrame(tick)})`);}
async function key(key,code,virtual,text='',modifiers=0){
 const step={key,time:Date.now(),phases:[]};steps.push(step);
 async function phase(name,action){
  const entry={phase:name,time:Date.now()},previous=activePhase;step.phases.push(entry);activePhase=key+' '+name;
  try{await action();entry.completed_ms=Date.now();}catch(error){entry.error=String(error);throw error;}finally{activePhase=previous;}
 }
 await phase('keyDown',()=>send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown',key,code,virtual,{text,modifiers})));
 await phase('frames_after_keyDown',()=>frames());
 await phase('keyUp',()=>send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp',key,code,virtual,{modifiers})));
 await phase('frames_after_keyUp',()=>frames());
}
// Pipeline real protocol phases between animation frames. The listener records
// the frame actually receiving each trusted key; it does not fabricate input,
// pause the application or assume that quick transport implies one C poll.
async function fastKeys(sequence){
 await evaluate(`(()=>{const state={active:true,frame:0,events:[]};globalThis.duxcelFastKeys=state;state.listener=event=>{state.events.push({key:event.key,code:event.code,trusted:event.isTrusted,repeat:event.repeat,frame:state.frame});};window.addEventListener('keydown',state.listener);function tick(){state.frame++;if(state.active)requestAnimationFrame(tick);}requestAnimationFrame(tick);})()`);
 await frames(1);
 const entry={key:'fast '+sequence.map(item=>item[0]).join(' '),time:Date.now()};steps.push(entry);
 try{
  const pending=[];
  for(const [name,code,virtual] of sequence){
   pending.push(send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown',name,code,virtual)));
   pending.push(send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp',name,code,virtual)));
  }
  await Promise.all(pending);await frames(3);
  const delivered=await evaluate('(()=>{const state=globalThis.duxcelFastKeys;state.active=false;window.removeEventListener("keydown",state.listener);return state.events;})()');
  entry.delivered=delivered;entry.completed_ms=Date.now();
  check(delivered.length===sequence.length&&delivered.every((event,i)=>event.trusted&&!event.repeat&&event.code===sequence[i][1]),'Fast sequence did not retain actual trusted key order');
  check(new Set(delivered.map(event=>event.frame)).size===1,'Fast access-key regression did not land in one animation-frame interval');
  return delivered;
 }finally{
  await evaluate('(()=>{const state=globalThis.duxcelFastKeys;if(state){state.active=false;window.removeEventListener("keydown",state.listener);}return true;})()');
 }
}
async function canvasPoint(x,y){
 if(canvasTransform){const rect=await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height,w:innerWidth,h:innerHeight}})()');x=rect.x+x*rect.width/rect.w;y=rect.y+y*rect.height/rect.h;}
 return {x,y};
}
async function click(x,y){
 ({x,y}=await canvasPoint(x,y));
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',x,y});
 await send('Input.dispatchMouseEvent',{type:'mousePressed',x,y,button:'left',clickCount:1});await frames();
 await send('Input.dispatchMouseEvent',{type:'mouseReleased',x,y,button:'left',clickCount:1});await frames();
}
function replies(){return events.filter(e=>e.method==='Runtime.consoleAPICalled').flatMap(e=>e.params.args).map(a=>String(a.value??'')).filter(t=>t.startsWith('Browser worker reply: ')).map(t=>JSON.parse(t.slice('Browser worker reply: '.length)));}
function starts(){return events.filter(e=>e.method==='Runtime.consoleAPICalled').flatMap(e=>e.params.args).map(a=>String(a.value??'')).filter(t=>t.startsWith('Browser worker started: ')).map(t=>JSON.parse(t.slice('Browser worker started: '.length)));}
function consoleText(prefix){return events.filter(e=>e.method==='Runtime.consoleAPICalled').flatMap(e=>e.params.args).map(a=>String(a.value??'')).filter(t=>t.startsWith(prefix));}
async function waitConsole(prefix,after=0){for(const deadline=Date.now()+15000;Date.now()<deadline;){const values=consoleText(prefix);if(values.length>after)return values.at(-1);await pause(20);}throw new Error('Missing C event: '+prefix);}
async function beginReview(){
 const after=consoleText('Browser review captured: ').length;
 await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Control',code:'ControlLeft',windowsVirtualKeyCode:17,modifiers:2});
 await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Shift',code:'ShiftLeft',windowsVirtualKeyCode:16,modifiers:10});
 await key('.','Period',190,'',10);
 await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Shift',code:'ShiftLeft',windowsVirtualKeyCode:16,modifiers:2});
 await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Control',code:'ControlLeft',windowsVirtualKeyCode:17});
 const captured=await waitConsole('Browser review captured: ',after);await frames();return captured;
}
async function platformSnapshot(){
 await beginReview();
 const state=capturedState();
 await key('Escape','Escape',27);return state;
}
function capturedState(){
 const line=consoleText('Browser review state: ').at(-1);
 check(line,'Missing real C platform state');
 return Object.fromEntries([...line.matchAll(/([a-z_][a-z_0-9]*)=([^ ]+)/g)].map(match=>[match[1],match[2]]));
}
async function hintMotion(){
 await pause(180);await frames(3);
 const restingRed=await evaluate(`new Promise(resolve=>requestAnimationFrame(()=>{const c=document.querySelector('canvas'),g=c.getContext('webgl2')||c.getContext('webgl'),r=c.width/1280,p=new Uint8Array(4);g.readPixels(Math.floor(60*r),c.height-1-Math.floor(28.25*r),1,1,g.RGBA,g.UNSIGNED_BYTE,p);resolve(p[0]);}))`);
 const results=[];
 for(const name of ['show','hide']){
  await evaluate(`globalThis.duxcelHintSamples=new Promise(resolve=>{const c=document.querySelector('canvas'),gl=c.getContext('webgl2')||c.getContext('webgl'),ratio=c.width/1280,data=new Uint8Array(4),samples=[];let start;function tick(now){start??=now;gl.readPixels(Math.floor(60*ratio),c.height-1-Math.floor(28.25*ratio),1,1,gl.RGBA,gl.UNSIGNED_BYTE,data);samples.push({ms:now-start,rgb:Array.from(data.slice(0,3))});if(now-start>=260)resolve(samples);else requestAnimationFrame(tick);}requestAnimationFrame(tick);});true`);
  const show=name==='show',params=show?{key:'Alt',code:'AltLeft',windowsVirtualKeyCode:18,modifiers:1}:{key:'Escape',code:'Escape',windowsVirtualKeyCode:27,modifiers:0};
  await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown',params.key,params.code,params.windowsVirtualKeyCode,{modifiers:params.modifiers}));await frames(1);
  await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp',params.key,params.code,params.windowsVirtualKeyCode,{modifiers:params.modifiers}));
  const samples=await evaluate('globalThis.duxcelHintSamples');
  results.push({name,samples});await fs.writeFile(path.join(output,'keytip-motion.json'),JSON.stringify(results,null,2));
  check(samples.some(s=>s.rgb[0]>175&&s.rgb[0]<restingRed-3),'Keytips did not visibly fade '+name);
  const red=samples.at(-1).rgb[0];
  check(show?Math.abs(red-164)<=2:Math.abs(red-restingRed)<=2,'Keytips did not settle '+name);
  if(show)await capture('keytips-light');
 }
 await key('Alt','AltLeft',18);await key('h','KeyH',72);await key('Escape','Escape',27);await key('Escape','Escape',27);await pause(160);
 const dismissed=await platformSnapshot();check(Number(dismissed.keytips)===0,'Alt H Escape Escape left keytips visible');
 const cycles=altStress?8:0;
 for(let cycle=0;cycle<cycles;cycle++){
  await key('Alt','AltLeft',18);await key('h','KeyH',72);await key('Escape','Escape',27);await key('Escape','Escape',27);
  await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('9','Digit9',57);await key('Escape','Escape',27);
 }
 if(cycles){await click(40,42);await pause(160);const state=await platformSnapshot();check(state.active===dismissed.active&&state.extent===dismissed.extent&&Number(state.keytips)===0,'Repeated trusted Alt shortcuts mutated selection or leaked hints');}
 const altEvents=await evaluate('globalThis.duxcelTestAltEvents');
 check(altEvents.length>=3&&altEvents.every(e=>e.canvas&&e.prevented&&e.trusted),'Focused canvas Alt escaped to browser menu handling');
 return {results,dismissed,altEvents,cycles,scope:'Trusted CDP key events through focused canvas/GLFW/C, balanced modifiers and no malformed native key codes; actual GPU fade samples.'};
}
// Sample the shared C GL framebuffer in rAF, without review/hotkeys freezing
// or clearing the spring. The only full readback is the first stretched PNG.
async function transientPull(baseline){
 const [bx,by]=baseline.body.split(',').map(Number);
 const result=await evaluate(`new Promise(resolve=>{const canvas=document.querySelector('canvas'),r=canvas.getBoundingClientRect(),gl=canvas.getContext('webgl2')||canvas.getContext('webgl'),ratio=canvas.width/${Number(baseline.logical.split(',')[0])},samples=[];let began,previous,frame;
  const read=(x,y,w,h)=>{const data=new Uint8Array(w*h*4);gl.readPixels(x,canvas.height-y-h,w,h,gl.RGBA,gl.UNSIGNED_BYTE,data);return data;};
  const band=(data,reverse=false)=>{let n=0;for(let i=0;i<data.length;i+=4){const p=reverse?data.length-4-i:i;if(data[p]!==201||data[p+1]!==211||data[p+2]!==220)break;n++;}return n/ratio;};
  canvas.dispatchEvent(new WheelEvent('wheel',{bubbles:true,cancelable:true,clientX:r.x+r.width*.65,clientY:r.y+r.height*.65,deltaX:-160,deltaY:-160}));
  function sample(now){began??=now;const n=Math.ceil(85*ratio),x=Math.round((${bx}+1)*ratio),y=Math.round((${by}+1)*ratio),row=read(x,Math.round((${by}+91)*ratio),n,1),col=read(Math.round((${bx}+201)*ratio),y,1,n),ex=band(row),ey=band(col,true);samples.push({ms:now-began,x:ex,y:ey,gap:previous?now-previous:0,first:Array.from(row.slice(0,4)),vertical:Array.from(col.slice(-4))});previous=now;
   if(!frame&&ex>8&&ey>8){const at=(a,offset)=>Array.from(a.slice(offset*4,offset*4+3));frame={left:at(row,Math.round(ex*ratio)),top:at(col,n-1-Math.round(ey*ratio)),paper:at(row,Math.min(n-1,Math.round(ex*ratio)+8))};}
   if(now-began>=360)resolve({samples,frame,error:gl.getError()});else requestAnimationFrame(sample);
  }requestAnimationFrame(sample);})`);
 await fs.writeFile(path.join(output,'overscroll-samples.json'),JSON.stringify(result,null,2));
 check(result.error===0,'Overscroll GPU readback failed');
 check(result.frame,'Trackpad pull did not visibly stretch both axes');

 const {left,top,paper}=result.frame;
 check(left[0]<paper[0]-12&&top[0]<paper[0]-12,'Displaced A/row1 boundaries are missing');
 check(left[0]<181&&top[0]<181,'Displaced sheet edges do not contrast against the exposed backing');
 const samples=result.samples,initial=samples.find(s=>s.x>8&&s.y>8),returned=samples.find(s=>s.ms>=280&&s.ms<=340);
 check(returned&&returned.x<.5&&returned.y<.5,'Canvas did not rubber-band back within 300ms');
 const active=samples.filter(s=>s.ms>=initial.ms);
 check(active.every((s,i)=>!i||s.x<=active[i-1].x+1&&s.y<=active[i-1].y+1),'Rubber-band return jitters outward');
 // A separate pull supplies a peak PNG after the timing sample is complete:
 // synchronous PNG encoding must not perturb the return-time measurement.
 const png=await evaluate(`new Promise(resolve=>{const c=document.querySelector('canvas'),r=c.getBoundingClientRect();c.dispatchEvent(new WheelEvent('wheel',{bubbles:true,cancelable:true,clientX:r.x+r.width*.65,clientY:r.y+r.height*.65,deltaX:-160,deltaY:-160}));requestAnimationFrame(()=>resolve(c.toDataURL('image/png')));})`);
 await fs.writeFile(path.join(output,'overscroll-stretched.png'),Buffer.from(png.split(',')[1],'base64'));await pause(400);
 const final=await platformSnapshot();
 check(final.top===baseline.top&&final.horizontal===baseline.horizontal&&final.active===baseline.active,'Rubber-band return altered canonical position/selection');

 return {...result,final,scope:'Real C GPU frame samples over360ms, dark backing, displaced boundaries and monotone return; physical trackpad packet timing remains manual QA.'};
}
// Unlike the single burst above, hold an outward gesture across real C frames,
// then continue supplying declining momentum after the edge has started home.
// Sparse tails used to tug it back out or keep it suspended indefinitely.
async function momentumPull(baseline,alternating=false){
 const [bx,by]=baseline.body.split(',').map(Number);
 const tail=[{ms:0,d:10},{ms:17,d:8},{ms:50,d:6},{ms:67,d:4},...(alternating ?
  [{ms:117,d:.4},{ms:150,d:-.25},{ms:200,d:.3},{ms:234,d:-.15},{ms:284,d:.1},{ms:317,d:-.075},
   {ms:350,d:.04},{ms:400,d:-.02},{ms:434,d:.01},{ms:467,d:-.005},{ms:500,d:.005}] :
  [{ms:117,d:2},{ms:150,d:1},{ms:200,d:.5},{ms:234,d:.25},{ms:284,d:.1},{ms:317,d:.03},{ms:350,d:.01},{ms:400,d:.005}])];
 const result=await evaluate(`new Promise(resolve=>{const canvas=document.querySelector('canvas'),rect=canvas.getBoundingClientRect(),gl=canvas.getContext('webgl2')||canvas.getContext('webgl'),ratio=canvas.width/${Number(baseline.logical.split(',')[0])},samples=[],packets=[],tail=${JSON.stringify(tail)},n=Math.ceil(45*ratio),row=new Uint8Array(n*4),col=new Uint8Array(n*4);let began,previous,released,at=0;
  const send=(distance,phase,ms)=>{const event=new WheelEvent('wheel',{bubbles:true,cancelable:true,clientX:rect.x+rect.width*.65,clientY:rect.y+rect.height*.65,deltaX:-distance,deltaY:-distance});canvas.dispatchEvent(event);packets.push({ms,distance,phase,claimed:event.defaultPrevented});};
  const band=(data,reverse=false)=>{let length=0;for(let i=0;i<data.length;i+=4){const p=reverse?data.length-4-i:i;if(data[p]!==201||data[p+1]!==211||data[p+2]!==220)break;length++;}return length/ratio;};
  send(14,'pull',0);
  function sample(now){began??=now;const ms=now-began,dt=previous===undefined?1000/60:now-previous;previous=now;
   gl.readPixels(Math.round((${bx}+1)*ratio),canvas.height-1-Math.round((${by}+91)*ratio),n,1,gl.RGBA,gl.UNSIGNED_BYTE,row);
   gl.readPixels(Math.round((${bx}+201)*ratio),canvas.height-Math.round((${by}+1)*ratio)-n,1,n,gl.RGBA,gl.UNSIGNED_BYTE,col);
   samples.push({ms,x:band(row),y:band(col,true),gap:dt,phase:released===undefined?'pull':'tail'});
   if(ms<160) send(14*dt/(1000/60),'pull',ms);
   else{released??=ms;while(at<tail.length&&ms-released>=tail[at].ms){send(tail[at].d,'tail',ms);at++;}}
   if(released!==undefined&&ms-released>=650)resolve({samples,packets,released,ratio,error:gl.getError()});else requestAnimationFrame(sample);
  }requestAnimationFrame(sample);})`);
 await fs.writeFile(path.join(output,alternating?'overscroll-reversal-tail.json':'overscroll-momentum.json'),JSON.stringify(result,null,2));
 check(result.error===0,'Momentum GPU readback failed');
 check(result.packets.every(packet=>packet.claimed),'A continuous pull/tail packet escaped canvas ownership');
 check(result.packets.filter(packet=>packet.phase==='pull').length>=5&&result.packets.filter(packet=>packet.phase==='tail').length===tail.length,'Continuous pull or gappy momentum sequence was incomplete');
 const pull=result.samples.filter(sample=>sample.phase==='pull'),peak={x:Math.max(...pull.map(sample=>sample.x)),y:Math.max(...pull.map(sample=>sample.y))};
 const tolerance=1/result.ratio+.001;
 check(peak.x>24&&peak.y>24,'Continuous finger pull did not hold a visible edge cue');
 check(result.samples.every(sample=>sample.x<=32+tolerance&&sample.y<=32+tolerance),'Canvas overscroll exceeded the 32-point cap');
 check(pull.every((sample,i)=>!i||sample.x>=pull[i-1].x-tolerance&&sample.y>=pull[i-1].y-tolerance),'Canvas fought the continuous pull with inward jitter');
 const held=pull.filter(sample=>sample.ms>=120);
 check(held.length>=2&&held.every(sample=>sample.x>=peak.x-tolerance&&sample.y>=peak.y-tolerance),'Canvas did not hold its saturated cue while the finger kept pulling');
 const latched=result.samples.findIndex(sample=>sample.ms>=result.released&&sample.x<peak.x-1&&sample.y<peak.y-1);
 check(latched>=0,'Declining/gappy momentum never released the stretched canvas');
 const returning=result.samples.slice(latched),latch=returning[0].ms;
 check(latch-result.released<=110,'Canvas stayed suspended too long after the pull rate declined');
 check(returning.every((sample,i)=>!i||sample.x<=returning[i-1].x+tolerance&&sample.y<=returning[i-1].y+tolerance),'Late momentum re-extended or jittered the returning canvas');
 const settled=returning.find(sample=>sample.ms-latch>=230&&sample.ms-latch<=280);
 check(settled&&settled.x<.5&&settled.y<.5,'Canvas did not settle within about 250ms of momentum return latch');
 check(returning.filter(sample=>sample.ms-latch>=280).every(sample=>sample.x<.5&&sample.y<.5),'Gappy momentum packets restarted a settled edge');
 const final=await platformSnapshot();
 check(final.top===baseline.top&&final.fraction===baseline.fraction&&final.horizontal===baseline.horizontal&&final.active===baseline.active&&final.extent===baseline.extent,'Continuous pull/tails altered canonical position or selection');
 return {...result,latch,final,alternating,scope:'Actual shared-C GL pixels during a multi-frame pull and declining/gappy DOM wheel tail'+(alternating?' with tiny alternating reversals':'')+'; no review freeze or PNG encoding during timing. Physical trackpad feel remains separate.'};
}
async function pickAndCancel(id,x,y,{moveBar=false,captureName=null}={}){
 const captured=await beginReview();
 if(moveBar)await key(' ','Space',32,' ');
 if(captureName)await capture(captureName+'-picker');
 const after=consoleText('Review picked: ').length;await click(x,y);const picked=await waitConsole('Review picked: ',after);
 check(picked==='Review picked: '+id,'Frozen picker chose covered content instead of '+id+': '+picked);
 if(captureName)await capture(captureName+'-composer');
 await key('Escape','Escape',27);return{id,captured,picked,moved_bar:moveBar};
}
// Review instrumentation exposes only the actual armed C frame's bounded
// authoring geometry. The driver sends real input; it does not call controller
// functions, build a parallel menu model or serialize a cancelled report.
function authorState(state,kind){
 check(state.author_kind===kind&&state.author_parent==='author.'+kind,'Missing authoritative '+kind+' menu capture');
 check(state.author_overflow==='0','Authoring diagnostics overflowed their bounded visible-row capture');
 const rect=name=>{const result=state[name]?.split(',').map(Number);check(result?.length===4&&result.every(Number.isFinite)&&result[2]>0&&result[3]>0,'Invalid captured '+name);return result;};
 const panel=rect('author_panel'),heading=rect('author_heading'),[width,height]=state.logical.split(',').map(Number);
 check(panel[0]>=0&&panel[1]>=0&&panel[0]+panel[2]<=width+.01&&panel[1]+panel[3]<=height+.01,'Authoring panel escaped the real logical viewport');
 const count=Number(state.author_count);check(Number.isInteger(count)&&count>0&&count<=16,'Invalid visible authoring count');
 const rows=Array.from({length:count},(_,index)=>{
  const bounds=rect('author_row'+index),key=state['author_key'+index],id=state['author_id'+index],flags=state['author_flags'+index]?.split(',').map(Number);
  const plain=state.author_parent+'.action.'+key,stableId=id===plain||kind==='assign_field'&&key==='assign'&&new RegExp('^'+plain.replaceAll('.','\\.')+'\\.f[0-9a-f]{16}$').test(id);
  check(key&&stableId&&flags?.length===3&&flags.every(flag=>flag===0||flag===1),'Incomplete or unstable semantic authoring row '+index);
  check(bounds[0]>=panel[0]-.01&&bounds[1]>=panel[1]-.01&&bounds[0]+bounds[2]<=panel[0]+panel[2]+.01&&bounds[1]+bounds[3]<=panel[1]+panel[3]+.01,'Authoring row escaped its captured panel clip');
  return{index,key,id,bounds,enabled:!!flags[0],checked:!!flags[1],focused:!!flags[2]};
 });
 check(new Set(rows.map(row=>row.id)).size===count&&rows.filter(row=>row.focused).length<=1,'Authoring rows have duplicate identity or focus');
 return{state,panel,heading,rows};
}
const authorOwnedFields=['sheet','modal','active','anchor','extent','grid_selection','grid_cell','top','horizontal','fraction','manual_cells','sheet_count','dirty','request','source_revision','schema','page','spare','search_cursor','search_anchor','search_revision'];
function authorRetained(before,after,label){
 check(authorOwnedFields.every(field=>before[field]===after[field]),label+' changed the underlying modal, cells, selection or published source');
 check(before.author_kind===after.author_kind&&before.author_count===after.author_count,'Cancelled review lost its owning contextual menu');
 const count=Number(before.author_count);
 check(Array.from({length:count},(_,i)=>i).every(i=>before['author_id'+i]===after['author_id'+i]&&before['author_flags'+i]===after['author_flags'+i]),label+' changed contextual identity, enabled state or focus');
}
let authorPickerTop=false;
async function authorPick(kind,choose,name,{frozenName=null}={}){
 const jobs=starts().length,captured=await beginReview(),state=capturedState(),menu=authorState(state,kind),target=choose(menu);
 check(target&&target.id&&target.bounds?.length===4,'Authoring target is unavailable: '+name);
 const [x,y,w,h]=target.bounds,point={x:x+w/2,y:y+h/2},height=Number(state.logical.split(',')[1]),top=point.y>height/2;
 if(top!==authorPickerTop){await key(' ','Space',32,' ');authorPickerTop=top;}
 if(frozenName)await capture('controls-'+frozenName+'-picker');
 const after=consoleText('Review picked: ').length;await click(point.x,point.y);const picked=await waitConsole('Review picked: ',after);
 check(picked==='Review picked: '+target.id,'Frozen authoring picker selected covered content instead of '+target.id+': '+picked);
 await capture('controls-'+name);await key('Escape','Escape',27);
 const restored=await platformSnapshot();authorRetained(state,restored,name);
 check(starts().length===jobs,'Reporting '+name+' submitted a database job');
 return{captured,target,point,picked,before:state,after:restored};
}
async function authorActivate(kind,key){
 const menu=authorState(await platformSnapshot(),kind),target=menu.rows.find(row=>row.key===key);
 check(target?.enabled,'Real contextual action is unavailable: '+kind+'.'+key);
 const [x,y,w,h]=target.bounds;await click(x+w/2,y+h/2);return target;
}
async function authorDraft(text){
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await key('a','KeyA',65,'',2);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));
 for(const character of text)await key(character,/\d/.test(character)?'Digit'+character:character==='.'?'Period':'Key'+character.toUpperCase(),character==='.'?190:character.toUpperCase().charCodeAt(0),character);
 await key('End','End',35);
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));await key('ArrowLeft','ArrowLeft',37,'',8);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));
 const state=await platformSnapshot();
 check(Number(state.search_cursor)===text.length-1&&Number(state.search_anchor)===text.length,'Real dialog draft did not retain its partial text selection');return state;
}
async function reviewManualDialogs(results){
 const baseline=await platformSnapshot(),jobs=starts().length,[x,y]=baseline.body.split(',').map(Number),[width,height]=baseline.grid_cell.split(',').map(Number);
 await rightClick(x+width/2,y+height/2);await authorActivate('context','width');
 const widthDraft=await authorDraft('177.5');check(widthDraft.author_kind==='column_width','Width action did not open its actual dialog');
 results.picks.push(await authorPick('column_width',menu=>menu.rows.find(row=>row.key==='width'),'width-draft',{frozenName:'column-width'}));
 results.picks.push(await authorPick('column_width',menu=>({id:menu.state.author_parent+'.backdrop.background',bounds:[1,Number(menu.state.logical.split(',')[1])/2,2,2]}),'width-backdrop'));
 await authorActivate('column_width','apply');const widthApplied=await platformSnapshot();
 check(widthApplied.modal==='0'&&Math.abs(Number(widthApplied.grid_cell.split(',')[0])-177.5*Number(widthApplied.ui_scale)*Number(widthApplied.zoom))<.002,'Reporting lost the edited width before the real Apply controller');
 check(['manual_cells','active','anchor','extent','request','source_revision','schema'].every(field=>widthApplied[field]===baseline[field]),'Width Apply changed worksheet data, selection or source ownership');
 const [mx,my]=widthApplied.body.split(',').map(Number),[mw,mh]=widthApplied.grid_cell.split(',').map(Number);
 await rightClick(mx+mw/2,my+mh/2);await authorActivate('context','move');
 const moveDraft=await authorDraft('C3');check(moveDraft.author_kind==='move_range','Move action did not open its actual dialog');
 results.picks.push(await authorPick('move_range',menu=>menu.rows.find(row=>row.key==='destination'),'move-draft',{frozenName:'move-range'}));
 await authorActivate('move_range','apply');const moved=await platformSnapshot();
 check(moved.modal==='0'&&moved.active==='2,2'&&moved.anchor==='2,2'&&moved.extent==='2,2'&&moved.manual_cells==='1','Reporting lost the edited destination or real Move changed the retained sparse-cell count');
 check(['request','source_revision','schema','page','spare','sheet_count'].every(field=>moved[field]===widthApplied[field])&&starts().length===jobs,'Manual width/Move dialogs submitted queries or changed source publication');
 results.dialogs={widthDraft,widthApplied,moveDraft,moved,scope:'Real editable C width/Move drafts, partial selections and report cancellation followed by actual Apply; no clipboard or invented controller state.'};
}
async function reviewUnsavedDialog(results){
 const baseline=await platformSnapshot(),jobs=starts().length;
 await key('Alt','AltLeft',18);await key('f','KeyF',70);await key('c','KeyC',67);
 const guarded=authorState(await platformSnapshot(),'unsaved');
 check(guarded.rows.map(row=>row.key).join(',')==='save,discard,cancel'&&!guarded.rows[0].enabled&&guarded.rows[1].enabled&&guarded.rows[2].enabled,'Browser Close guard lost disabled Save or available discard/cancel targets');
 results.picks.push(await authorPick('unsaved',menu=>menu.rows.find(row=>row.key==='save'&&!row.enabled),'unsaved-disabled-save',{frozenName:'unsaved'}));
 results.picks.push(await authorPick('unsaved',menu=>({id:menu.state.author_parent+'.backdrop.background',bounds:[1,Number(menu.state.logical.split(',')[1])/2,2,2]}),'unsaved-backdrop'));
 await authorActivate('unsaved','cancel');const cancelled=await platformSnapshot();
 check(cancelled.modal==='0'&&['sheet','manual_cells','sheet_count','dirty','active','anchor','extent','request','source_revision','schema'].every(field=>cancelled[field]===baseline[field]),'Reporting or actual Cancel lost the guarded workbook');
 check(starts().length===jobs,'Reporting the Close guard submitted a database job');results.dialogs.unsaved={baseline,guarded:guarded.state,cancelled};
}
async function rightClick(x,y){
 ({x,y}=await canvasPoint(x,y));
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',x,y});
 await send('Input.dispatchMouseEvent',{type:'mousePressed',x,y,button:'right',clickCount:1});await frames();
 await send('Input.dispatchMouseEvent',{type:'mouseReleased',x,y,button:'right',clickCount:1});await frames();
}
async function authorNavigation(kind){
 const states=[];
 for(const [name,code,virtual,last]of[['End','End',35,true],['Home','Home',36,false]]){
  await key(name,code,virtual);const state=await platformSnapshot(),menu=authorState(state,kind),available=menu.rows.filter(row=>row.enabled),expected=last?available.at(-1):available[0];
  check(expected?.focused,'Context '+name+' did not focus its '+(last?'last':'first')+' enabled semantic target');states.push(state);
 }
 return states;
}
async function reviewControls(){
 const source=await zoomSource(),results={source,picks:[]};
 let state=await platformSnapshot();const [bx,by]=state.body.split(',').map(Number),[cw,rh]=state.grid_cell.split(',').map(Number);
 await rightClick(bx+cw/2,by+rh/2);
 const sourceMenu=authorState(await platformSnapshot(),'source_context');
 check(sourceMenu.rows.length===6&&sourceMenu.rows.every(row=>row.enabled),'Real source-cell menu does not expose all six current actions');
 check(sourceMenu.rows.map(row=>row.key).join(',')==='copy,sort.asc,sort.desc,filter,create_analysis,inspect_sql','Source-cell menu lost its stable action semantics');
 results.picks.push(await authorPick('source_context',menu=>({id:menu.state.author_parent+'.background',bounds:[menu.panel[0]+1,menu.panel[1]+1,2,2]}),'source-background',{frozenName:'source-context'}));
 results.picks.push(await authorPick('source_context',menu=>({id:menu.state.author_parent+'.heading',bounds:menu.heading}),'source-heading'));
 results.picks.push(await authorPick('source_context',menu=>menu.rows.find(row=>row.key==='copy'),'source-copy'));
 results.picks.push(await authorPick('source_context',menu=>menu.rows.find(row=>row.key==='filter'),'source-filter'));
 results.sourceNavigation=await authorNavigation('source_context');await key('Escape','Escape',27);
 state=await platformSnapshot();const [gx]=state.grid_rect.split(',').map(Number);
 await rightClick((gx+bx)/2,by+rh/2);
 const sourceRows=authorState(await platformSnapshot(),'context');
 check(sourceRows.state.grid_selection==='1'&&sourceRows.rows.length===6&&sourceRows.rows.filter(row=>!row.enabled).length>=3,'Imported row menu lost its protected structural/edit states');
 check(sourceRows.rows.filter(row=>!row.enabled).map(row=>row.key).join(',')==='insert.above,insert.below,delete.row,edit','Imported row protections do not match the registered actions');
 results.picks.push(await authorPick('context',menu=>menu.rows.find(row=>row.key==='insert.above'&&!row.enabled),'protected-source-row',{frozenName:'protected-context'}));
 results.picks.push(await authorPick('context',menu=>menu.rows.find(row=>row.key==='edit'&&!row.enabled),'protected-source-edit'));
 results.rowNavigation=await authorNavigation('context');await key('Escape','Escape',27);
 // F6 is the real focus-region command. Find Fields without relying on dock
 // dimensions, then open its current field with the real context shortcut.
 await click(bx+cw/2,by+rh/2);let fields=false;
 for(let index=0;index<8;index++){await key('F6','F6',117);state=await platformSnapshot();if(state.focus?.split('/')[1]==='1'){fields=true;break;}}
 check(fields,'Actual focus-region traversal did not reach source Fields');
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));await key('F10','F10',121,'',8);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));await frames();
 const fieldsMenu=authorState(await platformSnapshot(),'field');check(fieldsMenu.rows.length===9,'Real field menu did not register its navigation and assignment children');
 check(fieldsMenu.rows.map(row=>row.key).join(',')==='reveal_column,center_column,select_column,assign.rows,assign.columns,assign.values,assign.filters,assign.color,assign.detail','Field menu lost its stable child semantics');
 results.picks.push(await authorPick('field',menu=>({id:menu.state.author_parent+'.heading',bounds:menu.heading}),'field-heading',{frozenName:'field'}));
 results.picks.push(await authorPick('field',menu=>menu.rows.find(row=>row.key==='assign.rows'),'field-add-rows'));
 results.fieldNavigation=await authorNavigation('field');await key('Escape','Escape',27);await key('Escape','Escape',27);
 // A separately owned sparse sheet lets us verify manual-range targets without
 // pretending the browser supports native analytical-object queries.
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));await key('F11','F11',122,'',8);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));
 await key('7','Digit7',55,'7');await key('Enter','Enter',13);await key('ArrowUp','ArrowUp',38);
 const manual=await platformSnapshot();check(manual.manual_cells==='1','Real manual entry did not retain the sparse sentinel');
 const [mx,my]=manual.body.split(',').map(Number),[mw,mh]=manual.grid_cell.split(',').map(Number);await rightClick(mx+mw/2,my+mh/2);
 const manualMenu=authorState(await platformSnapshot(),'context');check(manualMenu.rows.length===8&&manualMenu.rows.every(row=>row.enabled),'Manual cell menu lost structural/edit/move targets');
 results.picks.push(await authorPick('context',menu=>menu.rows.find(row=>row.key==='move'),'manual-move'));
 results.manualNavigation=await authorNavigation('context');await key('Escape','Escape',27);
 await reviewManualDialogs(results);
 await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('7','Digit7',55);await key('Home','Home',36);for(let i=0;i<3;i++)await key('ArrowDown','ArrowDown',40);await key('Enter','Enter',13);
 await send('Emulation.setDeviceMetricsOverride',{width:640,height:640,deviceScaleFactor:dpr,mobile:false});await frames(4);
 const narrow=await platformSnapshot();check(narrow.ui_scale==='1.500000'&&narrow.manual_cells==='1','Narrow review preparation changed manual data or interface scale');
 const [nx,ny,nw,nh]=narrow.body.split(',').map(Number);await rightClick(nx+nw-24,ny+nh-24);
 const narrowMenu=authorState(await platformSnapshot(),'context');check(narrowMenu.rows.length===8,'Narrow manual menu lost visible semantic children');
 results.picks.push(await authorPick('context',menu=>({id:menu.state.author_parent+'.heading',bounds:menu.heading}),'manual-narrow-heading',{frozenName:'manual-narrow'}));
 results.picks.push(await authorPick('context',menu=>menu.rows.find(row=>row.key==='move'),'manual-narrow-move'));
 const widthBefore=await platformSnapshot(),widthJobs=starts().length;await authorActivate('context','width');
 const narrowWidth=authorState(await platformSnapshot(),'column_width');
 check(narrowWidth.state.ui_scale==='1.500000'&&narrowWidth.rows.map(row=>row.key).join(',')==='width,apply,cancel','Narrow150% width dialog lost its actual editor/actions');
 results.picks.push(await authorPick('column_width',menu=>menu.rows.find(row=>row.key==='cancel'),'width-narrow-cancel',{frozenName:'column-width-narrow'}));
 await key('Escape','Escape',27);const widthCancelled=await platformSnapshot();
 check(widthCancelled.modal==='0'&&['grid_cell','manual_cells','active','anchor','extent','request','source_revision','schema','page','spare','dirty'].every(field=>widthCancelled[field]===widthBefore[field]),'Narrow width capture or actual Escape changed width, cells, selection or published source');
 check(starts().length===widthJobs,'Narrow width reporting/cancel submitted a database job');
 results.dialogs.narrowWidth={before:widthBefore,opened:narrowWidth.state,cancelled:widthCancelled};
 await reviewUnsavedDialog(results);const final=await platformSnapshot();
 check(final.manual_cells==='1'&&final.modal==='0','Cancelled contextual review changed sparse data or left a modal open');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Cancel-only controls gate staged a report');
 const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Cancel-only controls gate wrote into the persistent queue');
 results.final=final;results.scope='Actual shared C source/manual contextual actions and Fields children, editable width/Move drafts and guarded Close through trusted input; same-frame semantic bounds, disabled/backdrop picking and cancel retention, real width/Move Apply and narrow150% clipping. Real Worker source import; no analytical-object, physical IME or report-upload acceptance.';
 await fs.writeFile(path.join(output,'review-controls.json'),JSON.stringify(results,null,2));return results;
}
function fieldHash(name){
 let value=14695981039346656037n;
 for(const byte of Buffer.from(name,'utf8'))value=BigInt.asUintN(64,(value^BigInt(byte))*1099511628211n);
 return value.toString(16).padStart(16,'0');
}
async function ribbonAnalysisAction(letter){await key('Alt','AltLeft',18);await key('y','KeyY',89);await key(letter,'Key'+letter.toUpperCase(),letter.toUpperCase().charCodeAt(0));}
async function assignAnalysisField(name,role){
 await ribbonAnalysisAction(role==='rows'?'r':'v');
 const menu=authorState(await platformSnapshot(),'assign_field'),id='author.assign_field.action.assign.f'+fieldHash(name),target=menu.rows.find(row=>row.id===id);
 check(target?.enabled,'Actual '+role+' field menu omitted '+name+' with its stable field identity');
 const [x,y,w,h]=target.bounds;await capture('analysis-assign-'+role);await click(x+w/2,y+h/2);return{field:name,role,id};
}
function analysisObject(state){
 check(state.selected_object==='0'&&state.object_count==='1','Same-sheet analysis did not retain its one selected object');
 const object=state.object?.split(','),page=state.object_page?.split(',').map(Number),counts=state.pivot_counts?.split(',').map(Number);
 check(object?.length===6&&/^[1-9][0-9]*$/.test(object[0])&&object.slice(1).map(Number).every(Number.isFinite),'Missing real C object identity/placement');
 check(page?.length===6&&page.every(Number.isFinite)&&counts?.length===4&&counts.every(Number.isInteger),'Missing real C result/shelf state');
 check(object[1]==='0'&&object[2]==='0'&&object[3]==='8','Empty pivot was not anchored beside the real source at I1');
 return{object,page,counts};
}
async function waitPivot(afterId=null){
 for(const deadline=Date.now()+15000;Date.now()<deadline;){
  const reply=replies().filter(value=>value.kind===5&&(afterId===null||BigInt(value.id)>BigInt(afterId))).at(-1);
  if(reply){check(!reply.error,'Real browser analytical object failed');return reply;}await pause(20);
 }
 throw new Error('Actual C analytical object did not publish a Worker reply');
}
async function sourceHome(){
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await key('Home','Home',36,'',2);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));await frames();
}
async function browserAnalysis(){
 await send('Emulation.setDeviceMetricsOverride',{width:1440,height:988,deviceScaleFactor:dpr,mobile:false});await frames(4);
 const source=await zoomSource();await sourceHome();
 for(let i=0;i<7;i++)await key('ArrowRight','ArrowRight',39);for(let i=0;i<9;i++)await key('ArrowDown','ArrowDown',40);
 for(const digit of '991')await key(digit,'Digit'+digit,digit.charCodeAt(0),digit);await key('Enter','Enter',13);await key('ArrowUp','ArrowUp',38);
 const sentinel=await platformSnapshot();check(sentinel.active==='9,7'&&sentinel.manual_cells==='1','Actual source worksheet did not retain the H10 sparse sentinel');
 await sourceHome();const baseline=await platformSnapshot(),[x,y]=baseline.body.split(',').map(Number),[width,height]=baseline.grid_cell.split(',').map(Number);
 await rightClick(x+width/2,y+height/2);await authorActivate('source_context','create_analysis');
 const empty=await platformSnapshot(),emptyObject=analysisObject(empty);
 check(emptyObject.counts.join(',')==='0,0,0,0'&&empty.manual_cells==='1'&&empty.sheet===baseline.sheet&&empty.page===baseline.page,'Empty same-sheet pivot changed source/manual data or started with invented shelf fields');
 await capture('analysis-empty');const rows=await assignAnalysisField('City','rows'),rowsState=await platformSnapshot(),rowsObject=analysisObject(rowsState);
 check(rowsObject.counts.join(',')==='1,0,0,0','Real Rows shelf did not retain City before Values assignment');
 const previous=replies().filter(reply=>reply.kind===5).at(-1)?.id,values=await assignAnalysisField('Amount','values'),reply=await waitPivot(previous??null);await frames(4);
 check(reply.rows===4&&reply.columns===2&&reply.total==='4'&&!reply.pending,'Real Rows City/SUM Amount result is not four grouped rows/two columns');
 const published=await platformSnapshot(),model=analysisObject(published);
 check(model.counts.join(',')==='1,0,1,0'&&model.page.join(',')==='0,4,2,4,0,0','App did not publish the complete actual matrix into its selected object cache');
 check(published.totals_count==='4,1'&&published.object_grand0_null==='0'&&published.object_grand0_truncated==='0','Actual matrix source-derived row/grand totals are unavailable or truncated');
 const grand=Buffer.from(published.object_grand0_hex??'','hex').toString('utf8');check(Number(grand)===145.25,'Real source grand total differs from 145.25');
 const [logicalWidth,logicalHeight]=published.logical.split(',').map(Number);
 for(const field of ['matrix_rect','preview_rect']){const rect=published[field]?.split(',').map(Number);check(rect?.length===4&&rect.every(Number.isFinite)&&rect[0]>=0&&rect[1]>=0&&rect[2]>0&&rect[3]>0&&rect[0]+rect[2]<=logicalWidth+.01&&rect[1]+rect[3]<=logicalHeight+.01,'Actual analysis did not expose a fully visible '+field);}
 check(published.manual_cells===sentinel.manual_cells&&['sheet','page','source_revision','schema'].every(field=>published[field]===baseline[field]),'Analytical publication changed the primary source or sparse-cell ownership');
 await capture('analysis-matrix-footer');await pause(220);await frames(3);const idleBefore=starts().length,idleStart=Date.now();await pause(250);await frames(3);const idleMilliseconds=Date.now()-idleStart;
 check(starts().length===idleBefore,'Idle matrix rendering submitted additional database jobs');
 await ribbonAnalysisAction('d');const returned=await platformSnapshot();
 check(returned.selected_object==='-1'&&returned.object_count==='1'&&returned.manual_cells==='1'&&['sheet','page','source_revision','schema','active','anchor','extent'].every(field=>returned[field]===baseline[field]),'Return to source lost its cached source selection, sparse data or same-sheet pivot');
 await capture('analysis-returned-source');for(let i=0;i<7;i++)await key('ArrowRight','ArrowRight',39);for(let i=0;i<9;i++)await key('ArrowDown','ArrowDown',40);
 const retained=await platformSnapshot();check(retained.active==='9,7'&&retained.manual_cells==='1'&&retained.object_count==='1','Returned worksheet could not revisit the retained H10 sentinel beside its pivot');await capture('analysis-retained-manual');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Analysis-only capture unexpectedly staged a report');
 const result={source,sentinel,baseline,empty,rows,rowsState,values,reply,published,grand,returned,retained,idle:{minimum_milliseconds:250,observed_milliseconds:idleMilliseconds,started_jobs:idleBefore},
  scope:'Actual shared C worksheet import/manual entry, anchored empty analysis and real Rows/Values menus, official Worker 4x2 matrix with source-derived 145.25 grand total, cached C App publication, visible matrix/footer/source/sparse-cell captures and a bounded no-idle-job interval. Columns pivot, broad analytical semantics, physical input and report-upload acceptance remain separate.'};
 await fs.writeFile(path.join(output,'analysis.json'),JSON.stringify(result,null,2));return result;
}
// These run after source/filter/report acceptance because Close/Discard must
// exercise the real document lifecycle and intentionally resets the workbook.
async function accessKeys(){
 const before=await platformSnapshot(),jobs=starts().length;
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));
 await key('F11','F11',122,'',8);
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));await frames(2);
 await key('7','Digit7',55,'7');await key('Enter','Enter',13);await key('ArrowUp','ArrowUp',38);
 const seeded=await platformSnapshot();
 check(Number(seeded.sheet_count)===Number(before.sheet_count)+1&&Number(seeded.manual_cells)===1&&seeded.dirty==='1','Shift F11 and real manual entry did not prepare a dirty sentinel sheet');
 const boldBatch=await fastKeys([['Alt','AltLeft',18],['h','KeyH',72],['1','Digit1',49]]);await pause(180);
 const bold=await platformSnapshot();
 check(Number(bold.active_format_flags)===(Number(seeded.active_format_flags)^1)&&Number(bold.keytips)===0&&bold.modal==='0','Same-frame Alt H 1 did not execute exactly one Bold action');
 const copyBatch=await fastKeys([['Alt','AltLeft',18],['h','KeyH',72],['c','KeyC',67],['c','KeyC',67]]);await pause(180);
 const copied=await platformSnapshot();
 check(copied.last_command==='0'&&bold.last_command==='5'&&Number(copied.keytips)===0&&copied.modal==='0','Same-frame Alt H C C lost its repeated prefix or failed actual Copy dispatch');
 check(copied.manual_cells===seeded.manual_cells&&copied.active===seeded.active&&copied.extent===seeded.extent,'Copy keytips mutated worksheet cells or selection');
 await click(40,14);const menu=await platformSnapshot();
 check(menu.file_menu==='1','Pointer File menu did not open');
 const [menuX,menuY,menuWidth]=menu.file_panel.split(',').map(Number),menuScale=Number(menu.ui_scale),filePicks=[];
 for(const [id,x,y]of [['file.background',menuX+2*menuScale,menuY+2*menuScale],['file.heading',menuX+40*menuScale,menuY+20*menuScale],['file.action.save',menuX+menuWidth/2,menuY+(35+2*28+14)*menuScale]])filePicks.push(await pickAndCancel(id,x,y,{captureName:id==='file.action.save'?'file-disabled-save':null}));
 const filePickRestored=await platformSnapshot();
 check(filePickRestored.file_menu==='1'&&filePickRestored.file_focus===menu.file_focus&&filePickRestored.file_scroll===menu.file_scroll&&filePickRestored.active===menu.active&&filePickRestored.manual_cells===seeded.manual_cells&&filePickRestored.active_format_flags===bold.active_format_flags&&filePickRestored.dirty===menu.dirty,'Cancelling File target reports mutated the open menu or workbook');
 await key('Delete','Delete',46);const protectedCell=await platformSnapshot();
 check(protectedCell.file_menu==='1'&&protectedCell.manual_cells===seeded.manual_cells&&protectedCell.active_format_flags===bold.active_format_flags&&protectedCell.modal==='0','Delete leaked through the pointer File menu into worksheet editing');
 const fileNavigation=[];
 for(const [name,code,virtual,expected] of [['End','End',35,4],['Home','Home',36,0],['ArrowUp','ArrowUp',38,4],['ArrowDown','ArrowDown',40,0],['Tab','Tab',9,1],['Tab','Tab',9,4]]){
  await key(name,code,virtual);const state=await platformSnapshot();fileNavigation.push({key:name,state});
  check(state.file_menu==='1'&&Number(state.file_focus)===expected,'File navigation failed to wrap or skip unavailable browser commands: '+name);
 }
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));
 await key('Tab','Tab',9,'',8);
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));await frames(2);
 const reverseTab=await platformSnapshot();check(reverseTab.file_focus==='1','Reverse Tab entered a disabled File command');
 await send('Emulation.setDeviceMetricsOverride',{width:1280,height:180,deviceScaleFactor:dpr,mobile:false});await frames(5);
 await key('End','End',35);const shortFile=await platformSnapshot();
 const [fileX,fileY,fileWidth,fileHeight]=shortFile.file_panel.split(',').map(Number);
 check(shortFile.file_menu==='1'&&shortFile.file_focus==='4'&&Number(shortFile.file_visible)>0&&Number(shortFile.file_visible)<6&&Number(shortFile.file_scroll)>0,'Short-window File menu lost its rows or failed to reveal the focused command');
 check(fileX>=0&&fileY>=0&&fileX+fileWidth<=1280&&fileY+fileHeight<=180,'Short-window File menu escaped the canvas');
 await capture('file-menu-short');
 await click(fileX+20,fileY+15);const headingFile=await platformSnapshot();
 check(headingFile.file_menu==='1'&&headingFile.file_focus===shortFile.file_focus,'Clamped File heading clicked through to the underlying brand anchor');
 await key('Home','Home',36);const homeFile=await platformSnapshot();check(homeFile.file_focus==='0'&&homeFile.file_scroll==='0','File Home did not reveal the first command');
 const filePointer=await canvasPoint(fileX+fileWidth/2,fileY+fileHeight-20);
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',...filePointer});
 await send('Input.dispatchMouseEvent',{type:'mouseWheel',...filePointer,deltaX:0,deltaY:80});await frames(4);
 const scrolledFile=await platformSnapshot();check(Number(scrolledFile.file_scroll)>0&&scrolledFile.manual_cells===seeded.manual_cells&&scrolledFile.active===seeded.active,'File wheel failed to scroll or leaked to worksheet selection');
 const [scrollX,scrollY,scrollWidth]=scrolledFile.file_panel.split(',').map(Number),visible=Number(scrolledFile.file_visible),trackHeight=visible*28*menuScale,thumbHeight=Math.min(trackHeight,Math.max(26*menuScale,trackHeight*visible/6)),thumbY=scrollY+35*menuScale+(trackHeight-thumbHeight)*Number(scrolledFile.file_scroll)/(6-visible);
 const thumbPick=await pickAndCancel('file.scrollbar.thumb',scrollX+scrollWidth-9*menuScale,thumbY+thumbHeight-4*menuScale,{moveBar:true,captureName:'file-short-thumb'}),thumbRestored=await platformSnapshot();
 check(thumbRestored.file_menu==='1'&&thumbRestored.file_focus===scrolledFile.file_focus&&thumbRestored.file_scroll===scrolledFile.file_scroll&&thumbRestored.active===scrolledFile.active&&thumbRestored.manual_cells===seeded.manual_cells,'Moved reporting bar or thumb pick changed the frozen File scroll/workbook');
 await key('End','End',35);await key(' ','Space',32,' ');const spaceClose=await platformSnapshot();
 check(spaceClose.file_menu==='0'&&spaceClose.modal!=='0'&&spaceClose.manual_cells===seeded.manual_cells,'Space failed to activate focused Close in the short File menu');
 await key('Escape','Escape',27);
 await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:dpr,mobile:false});await frames(5);
 await click(40,14);const reopenedFile=await platformSnapshot();
 check(reopenedFile.file_menu==='1'&&reopenedFile.file_focus==='0'&&reopenedFile.file_scroll==='0','Reopened File retained obsolete short-window focus or scrolling');
 await click(1262,42);const helpBlocked=await platformSnapshot();
 check(helpBlocked.file_menu==='0'&&helpBlocked.modal==='0'&&helpBlocked.manual_cells===seeded.manual_cells,'Outside File click activated Help behind the menu');
 await click(40,14);await send('Emulation.setDeviceMetricsOverride',{width:1280,height:70,deviceScaleFactor:dpr,mobile:false});await frames(5);
 const tinyFile=await platformSnapshot();check(tinyFile.file_menu==='0','Unrenderable tiny File menu stayed invisibly active');
 await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:dpr,mobile:false});await frames(5);
 await click(40,14);
 const [bodyX,bodyY,bodyWidth,bodyHeight]=protectedCell.body.split(',').map(Number);
 await click(bodyX+Math.min(160,bodyWidth-8),bodyY+Math.min(70,bodyHeight-8));const outside=await platformSnapshot();
 check(outside.file_menu==='0'&&outside.manual_cells===seeded.manual_cells&&outside.active===protectedCell.active&&outside.extent===protectedCell.extent&&outside.modal==='0','Normal outside click failed to dismiss File or clicked through into worksheet editing/selection');
 await key('Escape','Escape',27);
 await key('Alt','AltLeft',18);await key('f','KeyF',70);await key('c','KeyC',67);
 const closePrompt=await platformSnapshot();
 check(closePrompt.modal!=='0'&&closePrompt.manual_cells===seeded.manual_cells&&closePrompt.dirty==='1','Alt F C failed to retain the dirty workbook behind its save prompt');
 await key('Escape','Escape',27);const cancelled=await platformSnapshot();
 check(cancelled.modal==='0'&&cancelled.manual_cells===seeded.manual_cells&&cancelled.sheet_count===seeded.sheet_count&&cancelled.dirty==='1','Cancel Close lost the existing workbook');
 await key('Alt','AltLeft',18);await key('f','KeyF',70);await key('c','KeyC',67);
 await key('ArrowRight','ArrowRight',39);await key('Enter','Enter',13);const closed=await platformSnapshot();
 check(closed.modal==='0'&&closed.dirty==='0'&&closed.manual_cells==='0'&&closed.sheet_count==='1','Discard Close did not retain a live blank browser surface');
 check(starts().length===jobs,'Access keys or document close added unexpected database jobs');
 const result={before,seeded,boldBatch,bold,copyBatch,copied,menu,filePicks,filePickRestored,protectedCell,fileNavigation,reverseTab,shortFile,headingFile,homeFile,scrolledFile,thumbPick,thumbRestored,spaceClose,reopenedFile,helpBlocked,tinyFile,outside,closePrompt,cancelled,closed,
  scope:'Trusted CDP sequences, actual C formatting/Copy dispatch, File-menu edit quarantine, disabled-skipping/wrapping navigation, short-window keyboard/wheel scrolling, Space activation, frozen File background/heading/disabled Save/scrolled thumb picks with reporting-bar relocation and cancel retention, normal held outside click and cancel/discard document lifecycle. This does not accept completed between-frame pointer capture, browser clipboard delivery, Worker queries or report upload.'};
 await fs.writeFile(path.join(output,'access-keys.json'),JSON.stringify(result,null,2));
 return result;
}
async function choiceReviewPick(){
 await click(40,42);await click(1262,124);await pause(180);const before=await platformSnapshot();
 check(before.modal!=='0'&&before.ribbon==='0/0','Ribbon display choice did not open from the real Home ribbon');
 // The full-ribbon definition anchors this 240 point menu at (1034,129).
 // Its second 28 point row is Compact; actual semantic picking is the oracle.
 const picked=await pickAndCancel('choice.ribbon_display.option.compact',1154,206,{captureName:'ribbon-choice'}),restored=await platformSnapshot();
 check(restored.modal===before.modal&&restored.ribbon===before.ribbon&&restored.active===before.active&&restored.extent===before.extent&&restored.manual_cells===before.manual_cells&&restored.dirty===before.dirty,'Cancelling a headed-choice report applied its option or changed the workbook');
 await key('Escape','Escape',27);
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Cancelled access/choice picks unexpectedly staged a report');
 const result={before,picked,restored,scope:'Actual transformed browser pointer selection of the shared C Ribbon display Compact semantic option, with unchanged modal/workbook and no staged report. Other headed choices retain separate native acceptance.'};
 await fs.writeFile(path.join(output,'choice-review.json'),JSON.stringify(result,null,2));return result;
}
async function wheelBatch(dx,dy,count=1,options={}){
 const result=await evaluate(`(()=>{const canvas=document.querySelector('canvas'),r=canvas.getBoundingClientRect();let claimed=0,point;for(let i=0;i<${count};i++){const event=new WheelEvent('wheel',{bubbles:true,cancelable:true,clientX:r.x+r.width*${options.x??.65},clientY:r.y+r.height*${options.y??.65},deltaX:${dx},deltaY:${dy},deltaMode:${options.mode??0},ctrlKey:${!!options.ctrl},shiftKey:${!!options.shift}});canvas.dispatchEvent(event);if(event.defaultPrevented)claimed++;point={x:Math.fround((event.clientX-r.x)*canvas.clientWidth/r.width),y:Math.fround((event.clientY-r.y)*canvas.clientHeight/r.height)};}return{claimed,point};})()`);
 check(result.claimed===count,'Canvas wheel default/duplicate ownership failed');await frames(3);return result.point;
}
// Geometry comes from the captured real C frame. Text bounds come from one
// actual compositor screenshot, independently of the geometry under test.
// Reading WebGL's non-preserved default buffer from a later rAF can sample a
// discarded frame. This reads the presented C pixels once, without retries.
let gridLabelCapture=0;
async function gridLabelPixels(state){
 const [rx,ry]=state.grid_rect.split(',').map(Number),[bx,by]=state.body.split(',').map(Number),[cw,rh]=state.grid_cell.split(',').map(Number),ui=Number(state.ui_scale),zoom=Number(state.zoom);
 const columnCenter=bx-Number(state.horizontal)*ui*zoom+cw/2,rowCenter=by+(2.5-Number(state.fraction))*rh;
 const regions={column:{x:columnCenter-10*ui,y:ry+ui,w:20*ui,h:(state.source_header==='1'?20:22)*ui},row:{x:rx+2*ui,y:rowCenter-8*ui,w:bx-rx-4*ui,h:16*ui}};
 const surface=await evaluate('(()=>{const c=document.querySelector("canvas"),r=c.getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height,viewportWidth:innerWidth,viewportHeight:innerHeight}})()');
 const screenshot=await send('Page.captureScreenshot',{format:'png',fromSurface:true,captureBeyondViewport:false}),png=Buffer.from(screenshot.data,'base64'),capture='zoom-labels-'+(++gridLabelCapture);
 await fs.writeFile(path.join(output,capture+'.png'),png);
 const result=await evaluate(`(async()=>{const encoded=atob(${JSON.stringify(screenshot.data)}),bytes=Uint8Array.from(encoded,c=>c.charCodeAt(0)),image=await createImageBitmap(new Blob([bytes],{type:'image/png'})),canvas=new OffscreenCanvas(image.width,image.height),context=canvas.getContext('2d',{willReadFrequently:true});context.drawImage(image,0,0);const imageSize={width:image.width,height:image.height};image.close();const surface=${JSON.stringify(surface)},regions=${JSON.stringify(regions)},logical=${JSON.stringify(state.logical.split(',').map(Number))},dpr=canvas.width/surface.viewportWidth,ratio=dpr*surface.width/logical[0],ratioY=canvas.height/surface.viewportHeight*surface.height/logical[1],labels={};for(const [name,r]of Object.entries(regions)){const x=Math.floor(surface.x*dpr+r.x*ratio),y=Math.floor(surface.y*dpr+r.y*ratioY),w=Math.ceil(r.w*ratio),h=Math.ceil(r.h*ratioY),pixels=context.getImageData(x,y,w,h).data;let left=w,right=-1,top=h,bottom=-1,count=0,min=255,max=0;for(let j=0;j<h;j++)for(let i=0;i<w;i++){const p=(j*w+i)*4;min=Math.min(min,pixels[p],pixels[p+1],pixels[p+2]);max=Math.max(max,pixels[p],pixels[p+1],pixels[p+2]);if(pixels[p+3]===255&&pixels[p]<180&&pixels[p+1]<180&&pixels[p+2]<180){left=Math.min(left,i);right=Math.max(right,i);top=Math.min(top,j);bottom=Math.max(bottom,j);count++;}}labels[name]={width:count?(right-left+1)/ratio:0,height:count?(bottom-top+1)/ratioY:0,ink:count,min,max,region:r,pixels:{x,y,w,h}};}return{ratio,ratioY,labels,image:imageSize,surface};})()`);
 result.capture=capture+'.png';result.sha256=createHash('sha256').update(png).digest('hex');result.method='single CDP compositor screenshot of actual C canvas';
 await fs.writeFile(path.join(output,capture+'.json'),JSON.stringify({state,result},null,2));
 check(Object.values(result.labels).every(label=>label.ink>0&&label.width>0&&label.height>0),'Header label GPU bounds are missing: '+JSON.stringify(result));
 return result;
}
async function gridZoomHeaders(name,sourceHeader){
 // An owned fresh browser defaults to Freeze first two source columns. Turn
 // that actual UI option off so both axes exercise normal sheet hit testing.
 if(sourceHeader){await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('9','Digit9',57);await click(40,42);}
 const baseline=await platformSnapshot(),jobs=starts().length;
 for(const field of ['grid_rect','grid_gutter','grid_header','grid_cell','grid_selection','source_header'])check(baseline[field]!==undefined,'C zoom capture omitted '+field);
 check(baseline.source_header===(sourceHeader?'1':'0')&&baseline.top==='0'&&Number(baseline.horizontal)===0,'Zoom gate requires the fresh expected grid at its origin');
 const pixels=await gridLabelPixels(baseline),samples=[];
 // Pulling from the header/gutter corner clamps the zoom anchor to A1. An
 // interior anchor cannot stay fixed while zooming out at the origin: that
 // would require negative canonical scroll. Pure grid tests cover interiors.
 const [bx,by]=baseline.body.split(',').map(Number),[lw,lh]=baseline.logical.split(',').map(Number),anchor={x:bx-2,y:by-2};
 const wheelOptions={ctrl:true,x:anchor.x/lw,y:anchor.y/lh};
 function near(a,b,tolerance=.004){return Math.abs(Number(a)-Number(b))<=tolerance;}
 function dataAnchor(state,point){const [x,y,w,h]=state.body.split(',').map(Number),scale=Number(state.ui_scale)*Number(state.zoom),px=Math.max(x,Math.min(point.x,x+w)),py=Math.max(y,Math.min(point.y,y+h));return{x:Number(state.horizontal)+(px-x)/scale,y:Number(state.top)+Number(state.fraction)+(py-y)/(Number(state.row_height)*scale)};}
 function fixedGeometry(state,point=anchor){
  check(near(state.grid_gutter,baseline.grid_gutter)&&near(state.grid_header,baseline.grid_header),'Sheet zoom resized row gutter or column header');
  check(state.ui_scale===baseline.ui_scale&&state.grid_rect===baseline.grid_rect&&state.body===baseline.body,'Sheet zoom moved UI chrome or changed interface scale');
  const actual=state.grid_cell.split(',').map(Number),initial=baseline.grid_cell.split(',').map(Number),factor=Number(state.zoom)/Number(baseline.zoom);
  check(actual.every((value,i)=>near(value,initial[i]*factor,.01)),'Cell width/row height did not follow canvas zoom');
  const a=dataAnchor(baseline,point),b=dataAnchor(state,point);check(near(a.x,b.x,.01)&&near(a.y,b.y,.0001),'Fixed header zoom lost the data under its pointer anchor');
  check(state.top==='0'&&near(state.horizontal,0)&&near(state.fraction,0,.000001),'Corner zoom moved the canonical A1 origin');
  check(state.active===baseline.active&&state.anchor===baseline.anchor&&state.extent===baseline.extent,'Zoom changed active cell or range selection');
 }
 function fixedLabels(actual){for(const label of ['column','row']){const before=pixels.labels[label],after=actual.labels[label],tolerance=1.25/actual.ratio+.02;check(near(after.width,before.width,tolerance)&&near(after.height,before.height,tolerance),'Sheet zoom resized rendered '+label+' label glyphs');}}
 for(const [label,delta] of [['in',-150],['back',150],['out',150],['restored',-150]]){
  const point=await wheelBatch(0,delta,1,wheelOptions);const state=await platformSnapshot();fixedGeometry(state,point);
  if(label==='in')check(Number(state.zoom)>Number(baseline.zoom)*1.2,'Zoom-in input did not enlarge the data cells');
  if(label==='out')check(Number(state.zoom)<Number(baseline.zoom)*.85,'Zoom-out input did not shrink the data cells');
  if(label==='back'||label==='restored')check(near(state.zoom,baseline.zoom,.00001),'Reciprocal zoom did not restore the sheet');
  const glyphs=await gridLabelPixels(state);fixedLabels(glyphs);samples.push({label,point,state,glyphs});
  if(label==='in'||label==='out')await capture('zoom-'+name+'-'+label);
 }
 // A real Interface scale command must still resize header UI independently.
 async function interfaceScale(index){await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('7','Digit7',55);await key('Home','Home',36);for(let i=0;i<index;i++)await key('ArrowDown','ArrowDown',40);await key('Enter','Enter',13);await frames(4);return platformSnapshot();}
 const interfaceScaled=await interfaceScale(2);
 check(near(interfaceScaled.ui_scale,1.25)&&near(interfaceScaled.zoom,baseline.zoom),'Interface scale did not remain independent of canvas zoom');
 check(near(Number(interfaceScaled.grid_header)/1.25,baseline.grid_header)&&near(Number(interfaceScaled.grid_gutter)/1.25,baseline.grid_gutter,1),'Interface scale failed to resize header chrome');
 const interfaceRestored=await interfaceScale(1);fixedGeometry(interfaceRestored);
 // Exercise hit testing at an enlarged zoom, rather than accepting geometry alone.
 await click(40,42);const enlargedPoint=await wheelBatch(0,-150,1,wheelOptions);let enlarged=await platformSnapshot();fixedGeometry(enlarged,enlargedPoint);
 check(starts().length===jobs,'Canvas zoom or Interface scale submitted a database job');
 const [gx,gy]=enlarged.grid_rect.split(',').map(Number),[bodyX,bodyY]=enlarged.body.split(',').map(Number),[cellWidth,rowHeight]=enlarged.grid_cell.split(',').map(Number),ui=Number(enlarged.ui_scale),scale=ui*Number(enlarged.zoom),left=bodyX-Number(enlarged.horizontal)*scale;
 await click(left+cellWidth/2,gy+11*ui);const columnSelected=await platformSnapshot();
 check(columnSelected.grid_selection==='2'&&columnSelected.active.split(',')[1]==='0','Enlarged column header did not select the actual column');
 const rowY=bodyY+(2.5-Number(enlarged.fraction))*rowHeight;
 await click(gx+(bodyX-gx)/2,rowY);const rowSelected=await platformSnapshot();
 check(rowSelected.grid_selection==='1'&&Number(rowSelected.active.split(',')[0])===Number(enlarged.top)+2,'Enlarged row gutter did not select its actual row');
 const from=await canvasPoint(left+cellWidth,gy+11*ui),to=await canvasPoint(left+cellWidth+24,gy+11*ui);
 await evaluate(`(()=>{const state={events:[]};globalThis.duxcelResizePointer=state;state.listener=event=>{const canvas=document.querySelector('canvas');if(event.target!==canvas)return;const r=canvas.getBoundingClientRect();state.events.push({type:event.type,trusted:event.isTrusted,buttons:event.buttons,x:Math.fround((event.pageX-window.scrollX-r.x)*canvas.clientWidth/r.width),y:Math.fround((event.pageY-window.scrollY-r.y)*canvas.clientHeight/r.height)});};for(const type of ['mousedown','mousemove','mouseup'])window.addEventListener(type,state.listener,true);return true;})()`);
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',...from});await send('Input.dispatchMouseEvent',{type:'mousePressed',...from,button:'left',clickCount:1});await frames(2);
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',...to,button:'left',buttons:1});await frames(2);
 await send('Input.dispatchMouseEvent',{type:'mouseReleased',...to,button:'left',clickCount:1});await frames(3);
 const resizePointer=await evaluate(`(()=>{const state=globalThis.duxcelResizePointer;for(const type of ['mousedown','mousemove','mouseup'])window.removeEventListener(type,state.listener,true);return state.events;})()`),dragStart=resizePointer.find(event=>event.type==='mousedown'),dragEnd=resizePointer.filter(event=>event.type==='mousemove'&&(event.buttons&1)).at(-1);
 check(dragStart&&dragEnd&&resizePointer.some(event=>event.type==='mouseup')&&resizePointer.every(event=>event.trusted),'Resize drag did not deliver its actual trusted DOM phases');
 const deliveredResize=dragEnd.x-dragStart.x;check(deliveredResize>20&&deliveredResize<28&&Math.abs(dragEnd.y-dragStart.y)<1,'Resize drag coordinates did not deliver the intended horizontal motion');
 const resized=await platformSnapshot();
 check(near(Number(resized.grid_cell.split(',')[0])-cellWidth,deliveredResize,.02)&&resized.grid_header===enlarged.grid_header&&resized.grid_gutter===enlarged.grid_gutter,'Zoomed column resize diverged from delivered pointer motion or resized chrome');
 let filter;
 if(sourceHeader){const previous=replies().filter(reply=>reply.kind===4).at(-1)?.id;await click(left+Number(resized.grid_cell.split(',')[0])-12*ui,gy+35*ui);const distinct=await waitDistinct(previous??null);const state=await platformSnapshot();check(state.modal!=='0'&&state.filter_field==='0'&&distinct.distinct_total==='5','Zoomed fixed-height filter arrow did not open first-column real available values');filter={state,distinct};await key('Escape','Escape',27);}
 await wheelBatch(0,150,1,wheelOptions);const final=await platformSnapshot();
 check(near(final.zoom,baseline.zoom,.00001)&&final.ui_scale===baseline.ui_scale&&final.modal==='0','Zoom pointer gate left a modal or changed UI/canvas scale');
 const newJobs=starts().slice(jobs);check(sourceHeader?newJobs.some(job=>job.kind===4)&&newJobs.every(job=>job.kind===4||job.kind===10):newJobs.every(job=>job.kind===10),'Header interactions submitted jobs beyond available values and existing selection statistics');
 const result={baseline,pixels,samples,interfaceScaled,interfaceRestored,enlarged,columnSelected,rowSelected,resizePointer,deliveredResize,resized,filter,final,jobs:newJobs,scope:'Actual shared C capture geometry and GL label bounds, DOM pixel Ctrl-wheel through C zoom, trusted pointer column/row selection and column resizing, real optional Worker distinct values, and keyboard Interface scale. Zoom/scale submit zero jobs; source selection may request existing browser-unsupported statistics. No physical pinch/trackpad or report-upload acceptance.'};
 await fs.writeFile(path.join(output,'zoom-'+name+'.json'),JSON.stringify(result,null,2));return result;
}
async function zoomSource(){
 await send('Page.setInterceptFileChooserDialog',{enabled:true});
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await key('o','KeyO',79,'',2);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));await frames();
 const document=await send('DOM.getDocument'),input=await send('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#duxcel-file-input'});
 check(input.nodeId,'C Open did not invoke browser file selection for zoom gate');await send('DOM.setFileInputFiles',{nodeId:input.nodeId,files:[sourceFile]});
 const imported=await waitPage();check(imported.total==='5'&&imported.columns===6,'Zoom gate requires the real transactions source');await frames(4);return imported;
}
async function pendingReports(){
 return evaluate(`(async()=>{const {createIndexedDBReviewStore}=await import('/review_queue.mjs');const store=createIndexedDBReviewStore();try{return(await store.listPending()).map(row=>({report_id:row.reportId,digest:row.digest,bytes:row.blob.size}));}finally{store.close();}})()`);
}
async function waitPage(afterId=null){
 for(const deadline=Date.now()+15000;Date.now()<deadline;){
  const current=replies().filter(r=>r.kind===0&&(afterId===null||BigInt(r.id)>BigInt(afterId))).at(-1);
  if(current){check(!current.error,'C browser query failed');if(!current.pending)return current;const count=replies().find(r=>r.kind===8&&BigInt(r.id)>BigInt(current.id));if(count){check(!count.error,'C browser count failed');return {...current,total:count.counted};}}
  await pause(20);
 }
 throw new Error('C source page/count did not complete');
}
async function waitDistinct(afterId=null){
 for(const deadline=Date.now()+15000;Date.now()<deadline;){
  const current=replies().filter(r=>r.kind===4&&(afterId===null||BigInt(r.id)>BigInt(afterId))).at(-1);
  if(current){check(!current.error,'C available-values query failed');return current;}
  await pause(20);
 }
 throw new Error('C available-values reply did not complete');
}
async function capture(name){await fs.writeFile(path.join(output,name+'.png'),Buffer.from((await send('Page.captureScreenshot',{format:'png'})).data,'base64'));}
async function failureDiagnostics(){
 // Snapshot the original failed request before diagnostic requests can replace
 // transport failure metadata. These probes never retry application input.
 const diagnostics={failure:cdp?.failure??null,pending:cdp?.pending()??[],held_keys:cdp?.heldKeys()??[]};
 async function probe(name,action){
  const started_ms=Date.now();
  try{const value=await action();diagnostics[name]={status:'passed',started_ms,elapsed_ms:Date.now()-started_ms,value};}
  catch(error){diagnostics[name]={status:'failed',started_ms,elapsed_ms:Date.now()-started_ms,error:String(error)};}
 }
 await probe('visibility_focus',async()=>{
  const result=await send('Runtime.evaluate',{expression:'({visibility:document.visibilityState,hidden:document.hidden,focused:document.hasFocus(),activeElement:{tag:document.activeElement?.tagName,id:document.activeElement?.id},readyState:document.readyState})',returnByValue:true,awaitPromise:false},2000);
  if(result.exceptionDetails)throw new Error(JSON.stringify(result.exceptionDetails));
  return result.result?.value;
 });
 const eventStart=events.length;
 try{
  await probe('debugger_pause',async()=>{
   await send('Debugger.pause',{},2000);await pause(300);
   const paused=events.slice(eventStart).filter(event=>event.method==='Debugger.paused');
   await fs.writeFile(path.join(output,'pause.json'),JSON.stringify(paused,null,2));
   if(!paused.length)throw new Error('Debugger.pause returned without a Debugger.paused stack event');
   return paused;
  });
 }finally{
  // Even a timed-out pause command may subsequently pause the target.
  await probe('debugger_resume',()=>send('Debugger.resume',{},2000));
 }
 await probe('screenshot',async()=>{await capture('failed');return 'failed.png';});
 await fs.writeFile(path.join(output,'diagnostics.json'),JSON.stringify(diagnostics,null,2));
 return diagnostics;
}

try{
 browser.stderr.on('data',data=>{stderr+=data;});
 for(let i=0;i<100&&!stderr.includes('DevTools listening on');i++)await pause(50);
 const port=stderr.match(/DevTools listening on ws:\/\/127\.0\.0\.1:(\d+)\//)?.[1];check(port,'No owned DevTools endpoint');
 const targets=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json();
 ws=new WebSocket(targets.find(t=>t.type==='page').webSocketDebuggerUrl);
 cdp=createCDP(ws,events);
 await new Promise((resolve,reject)=>{
  ws.addEventListener('open',resolve,{once:true});
  ws.addEventListener('error',()=>reject(new Error('CDP WebSocket connection error')),{once:true});
  ws.addEventListener('close',event=>reject(new Error('CDP WebSocket closed while connecting (code '+event.code+')')),{once:true});
 });
 await send('Runtime.enable');await send('Debugger.enable');await send('Page.enable');await send('Log.enable');await send('Network.enable');
 await send('Emulation.setFocusEmulationEnabled',{enabled:true});
 await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:dpr,mobile:false});
 if(review)await send('Browser.setDownloadBehavior',{behavior:'allow',downloadPath:output});
 if(reviewOffline)await send('Page.addScriptToEvaluateOnNewDocument',{source:`globalThis.duxcelReviewOffline=true;const originalFetch=globalThis.fetch.bind(globalThis);globalThis.fetch=(input,...args)=>globalThis.duxcelReviewOffline&&new URL(typeof input==='string'?input:input.url??input,location.href).pathname.startsWith('/__review/')?Promise.reject(new Error('Owned offline review acceptance')):originalFetch(input,...args);`});
 const navigation=await send('Page.navigate',{url});check(!navigation.errorText,'Navigation failed: '+navigation.errorText);
 if(workerDriver){
  let report;
  for(const deadline=Date.now()+45000;Date.now()<deadline;){report=await evaluate('globalThis.duxcelWorkerResult');if(report)break;await pause(20);}
  await fs.writeFile(path.join(output,'worker-driver.json'),JSON.stringify(report??{ok:false,error:'C driver did not complete'},null,2));
  check(report?.ok,'Real C/Worker bridge failed; see worker-driver.json and events.json');
  check(report.stages===49,'C/Worker acceptance stage inventory changed');
  await fs.writeFile(path.join(output,'events.json'),JSON.stringify(events,null,2));
  console.log(JSON.stringify({status:'passed',checks,report,output}));
 }else{
 let ready=false;
 for(const deadline=Date.now()+15000;Date.now()<deadline;){
  const error=await evaluate('document.getElementById("error")?.textContent');if(error)throw new Error(error);
  ready=events.some(e=>e.method==='Runtime.consoleAPICalled'&&e.params.args.some(a=>String(a.value??'').includes('Render batch buffers=')));
  if(ready)break;await pause(100);
 }
 check(ready,'Shared C App did not initialize');await pause(350);
 await evaluate('globalThis.duxcelTestNavigation=[];window.addEventListener("keydown",event=>{if(["ArrowUp","ArrowDown","ArrowLeft","ArrowRight","Home","End","PageUp","PageDown"].includes(event.key))globalThis.duxcelTestNavigation.push({key:event.key,shift:event.shiftKey,default_prevented:event.defaultPrevented,canvas:event.target===document.querySelector("canvas")})});true');
 await evaluate('globalThis.duxcelTestAltEvents=[];window.addEventListener("keydown",e=>{if(e.key==="Alt")globalThis.duxcelTestAltEvents.push({canvas:e.target===document.querySelector("canvas"),prevented:e.defaultPrevented,trusted:e.isTrusted})});true');
 if(canvasTransform){await evaluate('Object.assign(document.querySelector("canvas").style,{position:"absolute",left:"8%",top:"6%",transform:"scale(.8)",transformOrigin:"top left"})');await frames();}
 const errors=events.filter(e=>e.method==='Runtime.exceptionThrown');check(!errors.length,'Uncaught browser exception');
 const glErrors=events.filter(e=>e.method==='Log.entryAdded'&&/WebGL: (INVALID|too many)/.test(e.params.entry.text));
 check(!glErrors.length,'WebGL reported invalid rendering operations');
 const surface=await evaluate('({width:innerWidth,height:innerHeight,dpr:devicePixelRatio,canvas:[document.querySelector("canvas").width,document.querySelector("canvas").height]})');
 check(surface.width===1280&&surface.height===820,'Viewport mismatch');
 check(surface.dpr===dpr,'Browser device pixel ratio mismatch');
 check(surface.canvas[0]===1280*dpr&&surface.canvas[1]===820*dpr,'Framebuffer is not rendered at actual DPR');
 await capture('wide');
 if(analysisOnly){
  analysisResult=await browserAnalysis();
 }else if(reviewControlsOnly){
  reviewControlsResult=await reviewControls();
 }else if(accessOnly){
  accessKeyResult=await accessKeys();
  reviewResult=await choiceReviewPick();
  check(starts().length===0,'Blank-workbook access-only flow submitted a database job');
 }else if(zoomOnly){
  zoomResult={};
  if(sourceFile){
   sourceResult=await zoomSource();zoomResult.source=await gridZoomHeaders('source',true);
   await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));await key('F11','F11',122,'',8);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));await frames(3);
  }
  zoomResult.blank=await gridZoomHeaders('blank',false);
 }else{
 if(persistence){
  const result=await evaluate(`(async()=>{const [{runPersistenceProbe},vendor]=await Promise.all([import('./persistence_probe.mjs'),fetch('./vendor.json').then(r=>r.json())]);const report=await runPersistenceProbe({baseURL:location.href,moduleURL:new URL(vendor.worker.moduleURL,location.href).href,workerURL:new URL(vendor.worker.workerURL,location.href).href,fixtureURL:new URL('./native-fixture.dxls',location.href).href});if(report.export){const exported=report.export;const dataURL=await new Promise((resolve,reject)=>{const reader=new FileReader();reader.onload=()=>resolve(reader.result);reader.onerror=()=>reject(reader.error);reader.readAsDataURL(new Blob([exported.bytes]));});report.export={...exported,bytes:undefined,base64:dataURL.slice(dataURL.indexOf(',')+1)};}return report;})()`,90000);
  if(result.export?.base64){
   const bytes=Buffer.from(result.export.base64,'base64');delete result.export.base64;
   check(bytes.length===result.export.byteLength&&createHash('sha256').update(bytes).digest('hex')===result.export.sha256,'BP0 exported database hash/length mismatch');
   await fs.writeFile(path.join(output,'bp0-browser-export.dxls'),bytes);
  }
  await fs.writeFile(path.join(output,'persistence-probe.json'),JSON.stringify(result,null,2));
  check(result.ok,'Real native/Worker persistence compatibility failed; see persistence-probe.json');
  check(result.ownedWorkersCreated===1&&result.checks.every(step=>step.status==='passed'),'BP0 Worker/step ownership failed');
  persistenceResult=result;
 }
 if(workerProbe){
  const probe=await evaluate(`import('./official_worker_probe.mjs').then(module=>fetch('./vendor.json').then(response=>response.json()).then(vendor=>module.runWorkerProbe({baseURL:location.href,...vendor.worker})))`,65000);
  await fs.writeFile(path.join(output,'worker-probe.json'),JSON.stringify(probe,null,2));
  check(probe?.status==='passed','Real official Worker probe failed; see worker-probe.json');
 }
 if(sourceFile){
  await send('Page.setInterceptFileChooserDialog',{enabled:true});
  await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Control',code:'ControlLeft',windowsVirtualKeyCode:17,modifiers:2});await frames();
  await key('o','KeyO',79,'',2);
  await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Control',code:'ControlLeft',windowsVirtualKeyCode:17});await frames();
  const document=await send('DOM.getDocument');const input=await send('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#duxcel-file-input'});
  check(input.nodeId,'C Open command did not invoke the browser file chooser');
  const previousJobs=new Set(starts().map(r=>r.id)),importStarted=Date.now();await send('DOM.setFileInputFiles',{nodeId:input.nodeId,files:[sourceFile]});
  if(busy){
   let job;
   for(const deadline=Date.now()+15000;Date.now()<deadline;){job=starts().find(r=>r.kind===0&&!previousJobs.has(r.id));if(job)break;await pause(10);}
   check(job,'C source job did not start');check(!replies().some(r=>r.id===job.id),'Source job already completed before the active-query UI check');
   const inputStarted=Date.now();await click(294,42);await capture('busy-view');
   check(!replies().some(r=>r.id===job.id),'Source job completed before the View frame was captured');
   busyResult={job,input_to_capture_ms:Date.now()-inputStarted,captured_before_reply:true,scope:'Actual shared C ribbon input/frame while a source query job is pending; not engine-only execution or input latency'};
   await click(38,42);
  }
  const imported=await waitPage();sourceResult={...imported,selection_to_c_reply_ms:Date.now()-importStarted};check(imported.rows>0&&imported.columns>0,'Imported C Page is empty');await frames(4);await capture('imported');
  if(sourceFile.endsWith('/transactions.csv')){check(imported.total==='5'&&imported.columns===6,'CSV import count/schema changed');await click(716,102);const sorted=await waitPage(imported.id);check(sorted.total==='5','Sort changed source count');await frames();await capture('sorted');}
  if(platform){
   await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('9','Digit9',57);await key('Escape','Escape',27);await click(40,42);
   const baseline=await platformSnapshot(),beforeJobs=starts().length;
   const hints=await hintMotion();
   const overscroll=await transientPull(baseline);
   const momentum=await momentumPull(baseline);
   const reversalMomentum=await momentumPull(baseline,true);
   const factor=canvasTransform?1/.8:1;
   await wheelBatch(.125,.25,40);
   const panned=await platformSnapshot();
   const vertical=state=>Number(state.top)+Number(state.fraction);
   check(Math.abs(Number(panned.horizontal)-Number(baseline.horizontal)-5*factor)<.002,'Fractional horizontal pan was lost/quantized/reversed');
   check(Math.abs((vertical(panned)-vertical(baseline))*Number(baseline.row_height)-10*factor)<.002,'Fractional vertical tails were lost/quantized');
   check(panned.active===baseline.active&&panned.extent===baseline.extent,'Panning moved selection');
   await wheelBatch(-.125,-.25,40);const reversed=await platformSnapshot();
   check(Math.abs(vertical(reversed)-vertical(baseline))<.00001&&Math.abs(Number(reversed.horizontal)-Number(baseline.horizontal))<.002,'Reverse pan did not return to origin');
   // A real browser-generated wheel also enters the same C pixel route.
   await evaluate('window.addEventListener("wheel",event=>{globalThis.duxcelTestLastWheel={dx:event.deltaX,dy:event.deltaY,mode:event.deltaMode,trusted:event.isTrusted}},{capture:true,once:true});true');
   const rect=await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return{x:r.x+r.width*.65,y:r.y+r.height*.65}})()');
   await send('Input.dispatchMouseEvent',{type:'mouseWheel',x:rect.x,y:rect.y,deltaX:.5,deltaY:.25});await frames(3);
   const deliveredWheel=await evaluate('globalThis.duxcelTestLastWheel');
   check(deliveredWheel?.trusted&&deliveredWheel.mode===0,'CDP did not generate a real pixel wheel event');
   const trusted=await platformSnapshot();
   check(Math.abs(Number(trusted.horizontal)-Number(reversed.horizontal)-deliveredWheel.dx*factor)<.002&&Math.abs((vertical(trusted)-vertical(reversed))*Number(baseline.row_height)-deliveredWheel.dy*factor)<.002,'Browser wheel delivery diverges from DOM pixel input');
   await wheelBatch(-deliveredWheel.dx,-deliveredWheel.dy);
   await wheelBatch(0,.5,4,{shift:true});const shifted=await platformSnapshot();
   check(Math.abs(Number(shifted.horizontal)-Number(reversed.horizontal)-2*factor)<.002&&vertical(shifted)===vertical(reversed),'Shift wheel ownership/axis mismatch');
   await wheelBatch(0,-.5,4,{shift:true});
   await wheelBatch(.5,.75,4,{y:.05});const chrome=await platformSnapshot();
   check(Math.abs(vertical(chrome)-vertical(baseline))<.00001&&Math.abs(Number(chrome.horizontal)-Number(baseline.horizontal))<.002,'Ribbon wheel leaked into worksheet');
   await wheelBatch(.5,.75,4,{x:.05});const sidebar=await platformSnapshot();
   check(Math.abs(vertical(sidebar)-vertical(baseline))<.00001&&Math.abs(Number(sidebar.horizontal)-Number(baseline.horizontal))<.002,'Sidebar wheel leaked into worksheet');
   await wheelBatch(0,-20,1,{ctrl:true});const zoomed=await platformSnapshot();
   check(Number(zoomed.zoom)>Number(baseline.zoom)&&zoomed.ui_scale===baseline.ui_scale,'Pinch did not zoom canvas independently');
   await wheelBatch(0,20,1,{ctrl:true});const restored=await platformSnapshot();
   check(Math.abs(Number(restored.zoom)-Number(baseline.zoom))<.00001,'Reciprocal pinch did not restore canvas zoom');
   const densities=[];
   for(const ratio of [1.25,2,1]){
    await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:ratio,mobile:false});await pause(300);await frames(5);
    const state=await platformSnapshot();densities.push({ratio,state});
    check(state.logical==='1280,820'&&state.framebuffer===`${Math.floor(1280*ratio)},${Math.floor(820*ratio)}`,'DPR change did not synchronize logical/framebuffer dimensions');
    check(state.ui_scale===baseline.ui_scale&&state.zoom===restored.zoom&&state.active===baseline.active,'DPR change altered UI/grid scale or cell selection');
    await capture('density-'+ratio);
   }
   // CSS embedding is independent of the browser viewport. GLFW must preserve
   // the host's explicit sizing through a subsequent DPR notification.
   await evaluate('Object.assign(document.querySelector("canvas").style,{width:"75%",height:"80%"})');await frames(5);
   await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:2,mobile:false});await pause(300);await frames(5);
   const embedded=await platformSnapshot();
   check(embedded.logical==='960,656'&&embedded.framebuffer==='1920,1312','CSS-sized embedded surface was overwritten by GLFW/DPR');
   check(await evaluate('document.querySelector("canvas").style.width==="75%"&&document.querySelector("canvas").style.height==="80%"'),'GLFW removed host canvas CSS dimensions');
   await evaluate('const s=document.querySelector("canvas").style;s.removeProperty("width");s.removeProperty("height");true');
   await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:dpr,mobile:false});await frames(5);
   // Restore the pan origin before the shared filter/header regression.
   const final=await platformSnapshot();
   await wheelBatch(-Number(final.horizontal)/factor,-vertical(final)*Number(final.row_height)/factor);await frames();
   platformResult={baseline,hints,overscroll,momentum,reversalMomentum,panned,reversed,trusted,deliveredWheel,shifted,chrome,sidebar,zoomed,restored,densities,embedded,query_jobs:starts().length-beforeJobs,
    scope:'Real C grid under fractional DOM wheel bursts, DPI changes and captured GPU frames; physical trackpad/Safari gesture feel remains separate.'};
  }
  if(filters){
   // Fixed 1280x820, 100% reference state; input goes through the actual C UI.
   await click(512,200);const cities=await waitDistinct();
   check(cities.distinct_count===4&&cities.distinct_total==='4','City checklist did not enumerate the four real source values');
   await frames();await capture('city-values');
   await click(420,375);await click(440,404);const beforeFilter=replies().filter(r=>r.kind===0).at(-1).id;
   await click(648,704);const filtered=await waitPage(beforeFilter);
   check(filtered.total==='1','Selecting Ankara did not filter the real source to one row');
   await frames();await capture('city-filtered');
   await click(768,200);const years=await waitDistinct(cities.id);
   check(years.distinct_count===1&&years.distinct_total==='1','Date years did not retain the City filter');
   await frames();await capture('date-years');
   await click(914,431);const months=await waitDistinct(years.id);
   check(months.distinct_count===1&&months.distinct_total==='1','Year expansion did not enumerate January');
   await frames();await capture('date-months');
   await click(914,431);const days=await waitDistinct(months.id);
   check(days.distinct_count===1&&days.distinct_total==='1','Month expansion did not enumerate the selected source day');
   await frames();await capture('date-days');await key('Escape','Escape',27);
   filterResult={cities,filtered,years,months,days};
  }
  if(review){
   await click(512,200);await waitDistinct();await frames();await capture('review-before');const jobsBefore=starts().length;
   let captured=await beginReview();const chromePicks=[];
   for(const [id,x,y] of [['chrome.brand',40,14],['chrome.document_title',180,14],['chrome.save',1258,14],
    ['ribbon.tab.home',40,42],['ribbon.tab.insert',110,42],['ribbon.tab.data',165,42],['ribbon.tab.analyze',228,42],['ribbon.tab.view',285,42],
    ['ribbon.display',1262,124],['chrome.help',1262,42],['chrome.title_bar.background',2,14],['ribbon.tabs.background',2,42]]){
    const after=consoleText('Review picked: ').length;await click(x,y);const picked=await waitConsole('Review picked: ',after);
    check(picked==='Review picked: '+id,'Chrome picker selected the wrong target: '+picked+' expected '+id);chromePicks.push(id);
    if(id==='ribbon.tab.home')await capture('review-chrome-home');
    await key('Escape','Escape',27);captured=await beginReview();
   }
   await capture('review-picker');
   await click(530,341);await capture('review-composer');
   for(const character of 'İ filter')await key(character,character===' '?'Space':'KeyI',character.codePointAt(0),character);
   await click(829,554);const prepared=await waitConsole('Browser review prepared: ');
   let offlineResult;
   if(reviewOffline){
    const pending=await pendingReports();check(pending.length===1&&prepared.includes(pending[0].report_id),'Offline report was not durably committed in real IndexedDB');
    const listing=await(await fetch(new URL('/__review/queue',url))).json();check(!listing.reports.some(row=>row.report_id===pending[0].report_id),'Offline report incorrectly claimed shared queue entry');
    await frames();await capture('review-pending');await key('Enter','Enter',13);await frames();
    check(starts().length===jobsBefore,'Offline reporting scheduled unexpected database jobs');
    const eventStart=events.length;await send('Page.reload',{ignoreCache:true});
    let reloaded=false;
    for(const deadline=Date.now()+15000;Date.now()<deadline;){reloaded=events.slice(eventStart).some(e=>e.method==='Runtime.consoleAPICalled'&&e.params.args.some(a=>String(a.value??'').includes('Render batch buffers=')));if(reloaded)break;await pause(30);}
    check(reloaded,'Reloaded shared C application did not start');
    const retained=await pendingReports();check(JSON.stringify(retained)===JSON.stringify(pending),'Reload changed or lost the staged immutable archive');
    await evaluate('globalThis.duxcelReviewOffline=false;window.dispatchEvent(new Event("online"));true');
    offlineResult={pending_before_reload:pending,retained_after_reload:retained};
   }
   let queued;
   for(const deadline=Date.now()+15000;Date.now()<deadline;){const listing=await(await fetch(new URL('/__review/queue',url))).json();queued=listing.reports.find(report=>report.report_id&&prepared.includes(report.report_id));if(queued)break;await pause(20);}
   check(queued,'Browser review did not reach the durable shared queue');
   if(reviewOffline){
    check((await pendingReports()).length===0,'Server acknowledgment did not retire the pending browser archive');
    const listing=await(await fetch(new URL('/__review/queue',url))).json();check(listing.reports.filter(row=>row.report_id===queued.report_id).length===1,'Offline replay duplicated the shared report');
   }
   check(!(await fs.readdir(output)).some(name=>name.endsWith('.tar')),'Queue submission unexpectedly downloaded an archive');
   const files=new Map();
   for(const name of ['manifest.json','frame.png','browser.json']){
    const response=await fetch(new URL('/__review/bundle/'+queued.report_id+'/'+name,url));check(response.ok,'Queued report member is unavailable');
    files.set(queued.report_id+'/'+name,Buffer.from(await response.arrayBuffer()));
   }
   const manifestName=[...files.keys()].find(name=>name.endsWith('/manifest.json'));
   check(manifestName&&files.size===3,'Expected one manifest/PNG/browser metadata bundle');const base=manifestName.slice(0,-'manifest.json'.length);
   const manifest=JSON.parse(files.get(manifestName)),metadata=JSON.parse(files.get(base+'browser.json')),png=files.get(base+'frame.png');
   check(manifest.target.widget.id==='filter.search','Browser picker selected the wrong C widget');
   check(manifest.note==='İ filter','UTF-8 composer note did not round-trip');
   check(manifest.frame.window.width===1280&&manifest.frame.window.height===820,'Review logical window dimensions mismatch');
   check(png&&png.subarray(0,8).equals(Buffer.from([137,80,78,71,13,10,26,10])),'Review image is not PNG');
   check(png.readUInt32BE(16)===manifest.frame.framebuffer.width&&png.readUInt32BE(20)===manifest.frame.framebuffer.height,'Captured PNG/framebuffer dimensions mismatch');
   check(metadata.report_id===manifest.report_id&&metadata.frame_id===manifest.frame.frame_id,'Browser sidecar references a different C frame');
   check(metadata.binary_sha256===manifest.frame.binary_sha256&&metadata.device_pixel_ratio===dpr,'Browser capture build/DPR mismatch');
   check(metadata.files['manifest.json'].sha256===createHash('sha256').update(files.get(manifestName)).digest('hex')&&metadata.files['frame.png'].sha256===createHash('sha256').update(png).digest('hex'),'Exported manifest/PNG hash binding mismatch');
   check(metadata.canvas_backing.width===manifest.frame.framebuffer.width&&metadata.canvas_backing.height===manifest.frame.framebuffer.height,'Browser canvas backing differs from the captured C framebuffer');
   if(canvasTransform)check(Math.abs(metadata.transforms.logical_to_css.scale_x-.8)<.001&&Math.abs(metadata.transforms.logical_to_css.scale_y-.8)<.001&&metadata.transforms.logical_to_css.translate_x>0&&metadata.transforms.logical_to_css.translate_y>0,'Inset canvas transform was not frozen with the C frame');
   await fs.writeFile(path.join(output,'report-manifest.json'),files.get(manifestName));await fs.writeFile(path.join(output,'report-browser.json'),files.get(base+'browser.json'));await fs.writeFile(path.join(output,'report-frame.png'),png);
   if(!reviewOffline){await frames();await capture('review-prepared');await key('Enter','Enter',13);await frames();await capture('review-returned');check(starts().length===jobsBefore,'Reporting scheduled unexpected database jobs');}
   const saved=manifest.widgets.find(widget=>widget.id==='chrome.save');
   check(saved&&!saved.enabled&&saved.pickable,'Browser Save must remain disabled but reportable');
   check(manifest.widgets.find(widget=>widget.id==='ribbon.tab.home')?.checked,'Active ribbon tab was not recorded');
   reviewResult={captured,prepared,transport:'unified-review-queue',queued,offlineResult,chromePicks,report_id:manifest.report_id,frame_id:manifest.frame.frame_id,widget:manifest.target.widget.id,framebuffer:manifest.frame.framebuffer};
   await key('Escape','Escape',27);
  }
  if(sourceFile.endsWith('/yellow_tripdata_2025-01.parquet'))check(imported.total==='3475226'&&imported.columns===20,'Public Parquet source count/schema changed');
 }else if(!persistence){
 for(const digit of '9007199254740993')await key(digit,'Digit'+digit,digit.charCodeAt(0),digit);
 await key('Enter','Enter',13);await key('ArrowUp','ArrowUp',38);await capture('edited');
 await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Shift',code:'ShiftLeft',windowsVirtualKeyCode:16,modifiers:8});
 await key('ArrowRight','ArrowRight',39,'',8);await key('ArrowDown','ArrowDown',40,'',8);
 await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Shift',code:'ShiftLeft',windowsVirtualKeyCode:16});await frames();await capture('selection');
 const navigation=await evaluate('globalThis.duxcelTestNavigation');
 await fs.writeFile(path.join(output,'navigation.json'),JSON.stringify(navigation,null,2));
 check(['ArrowUp','ArrowRight','ArrowDown'].every(key=>navigation.some(event=>event.key===key&&event.canvas&&event.default_prevented)),'Shared canvas navigation escaped to browser defaults');
 check(navigation.some(event=>event.key==='ArrowRight'&&event.shift)&&navigation.some(event=>event.key==='ArrowDown'&&event.shift),'Shift navigation did not reach the real canvas');
 }
 if(platform)accessKeyResult=await accessKeys();
 await click(294,42);await capture('view-ribbon');
 await click(1262,124);await capture('ribbon-display');await key('Escape','Escape',27);
 await send('Emulation.setDeviceMetricsOverride',{width:640,height:640,deviceScaleFactor:dpr,mobile:false});await frames(4);await capture('narrow');
 const narrow=await evaluate('({width:innerWidth,height:innerHeight,canvas:[document.querySelector("canvas").width,document.querySelector("canvas").height]})');
 check(narrow.width===640&&narrow.height===640&&narrow.canvas[0]===640*dpr&&narrow.canvas[1]===640*dpr,'Narrow canvas mismatch');
 }
 const lateErrors=events.filter(e=>e.method==='Runtime.exceptionThrown'||e.method==='Log.entryAdded'&&/WebGL: (INVALID|too many)/.test(e.params.entry.text));
 check(!lateErrors.length,'Runtime/rendering error after interactions');
 await fs.writeFile(path.join(output,'events.json'),JSON.stringify(events,null,2));
 const resources=await evaluate('({first_frame:performance.getEntriesByName("duxcel:first-frame").map(e=>({start_ms:e.startTime})),resources:performance.getEntriesByType("resource").map(e=>({name:e.name,start_ms:e.startTime,duration_ms:e.duration,transfer_bytes:e.transferSize,encoded_bytes:e.encodedBodySize,decoded_bytes:e.decodedBodySize})),main_js_heap:performance.memory?{used:performance.memory.usedJSHeapSize,total:performance.memory.totalJSHeapSize,limit:performance.memory.jsHeapSizeLimit}:null})');
 resources.c_bridge=consoleText('Browser worker metrics: ').map(line=>JSON.parse(line.slice('Browser worker metrics: '.length)));
 resources.scope='Navigation-to-first completed C frame and parent resource timing; main JS heap excludes Worker/GPU/process RSS. C bridge metrics are review-only bounded callback/job observations; decode_ms is C validation/allocation/copy, elapsed_ms includes asynchronous engine/transport scheduling. Worker network bytes are not inferred from parent Resource Timing.';
 await fs.writeFile(path.join(output,'resources.json'),JSON.stringify(resources,null,2));
 await fs.writeFile(path.join(output,'result.json'),JSON.stringify({status:'passed',checks,url,surface,elapsed_ms:Date.now()-started,steps,sourceFile,sourceResult,filterResult,busyResult,reviewResult,platformResult,accessKeyResult,persistenceResult,zoomResult,reviewControlsResult,analysisResult,workerProbe,
  profile:analysisOnly?'analysis-only':reviewControlsOnly?'review-controls-only':zoomOnly?'zoom-only':accessOnly?'access-only':'standard',
  scope:analysisOnly?'Shared C same-sheet Rows/Values analysis, real matrix/grand totals and source/manual ownership; no Columns pivot, broad analytics or report upload.':reviewControlsOnly?'Shared C source/manual contextual targets and field-menu children, editable width/Move drafts and guarded Close, trusted input ownership and cancel retention, narrow150% clipping; no analytical-object or report-upload acceptance.':zoomOnly?'Shared C fixed header geometry/rendered glyphs during blank/source canvas zoom, pointer header selection/resize/filter and independent interface scale; no report upload or physical gesture acceptance.':accessOnly?'Shared C startup and focused blank-workbook access keys/File/document interactions; no Worker query, clipboard delivery or report-upload acceptance.':'Shared C99 startup and captured real interactions for visual verification; source imports, filters, active-query frames and review exports are explicit optional gates; BP0 is file compatibility only, browser workbook persistence is not accepted'},null,2));
 console.log(JSON.stringify({status:'passed',checks,surface,output}));
 }
}catch(error){
 const diagnostics=await failureDiagnostics();
 await fs.writeFile(path.join(output,'events.json'),JSON.stringify(events,null,2));
 await fs.writeFile(path.join(output,'result.json'),JSON.stringify({status:'failed',checks,error:String(error),url,elapsed_ms:Date.now()-started,steps,diagnostics},null,2));
 console.error(error);process.exitCode=1;
}finally{
 const released=await cdp?.releaseHeldKeys()??[];
 const beforeCleanup={pid:browser.pid,exit_code:browser.exitCode,signal:browser.signalCode};
 ws?.close();browser.kill('SIGTERM');
 await Promise.race([new Promise(resolve=>browser.once('exit',resolve)),pause(2000)]);
 if(browser.exitCode===null)browser.kill('SIGKILL');
 await fs.writeFile(path.join(output,'cleanup.json'),JSON.stringify({held_keys:released,browser_before_cleanup:beforeCleanup,browser_after_cleanup:{pid:browser.pid,exit_code:browser.exitCode,signal:browser.signalCode}},null,2));
 await fs.writeFile(path.join(output,'browser.txt'),stderr);
 await fs.rm(profile,{recursive:true,force:true});
}
