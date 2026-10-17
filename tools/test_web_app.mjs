#!/usr/bin/env node
// Headless Chromium driver for the local shared C UI. Its Python/server parent
// owns the cross-worktree desktop lock. No user browser/profile is accessed.
import {spawn} from 'node:child_process';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {createHash} from 'node:crypto';
import {createCDP} from './test_web_cdp.mjs';
const url=process.argv[2]??'http://127.0.0.1:8767';
const output=path.resolve(process.argv[3]??'build/web/browser-qa/cdp');
await fs.mkdir(output,{recursive:true});
const profile=await fs.mkdtemp(path.join(os.tmpdir(),'duxcel-browser-'));
const browser=spawn(process.env.DUXCEL_CHROMIUM??'/Applications/Chromium.app/Contents/MacOS/Chromium',
 ['--headless','--disable-background-timer-throttling','--disable-renderer-backgrounding','--disable-backgrounding-occluded-windows','--remote-debugging-port=0','--user-data-dir='+profile,'--no-first-run','--no-default-browser-check','--no-proxy-server','about:blank'],{stdio:['ignore','ignore','pipe']});
await fs.writeFile(path.join(output,'browser-process.json'),JSON.stringify({pid:browser.pid,parent_pid:process.pid,started_ms:Date.now(),headless:true,profile:'owned temporary profile'},null,2));
const workerProbe=process.argv.includes('--worker-probe');
const workerDriver=process.argv.includes('--worker-driver');
const filters=process.argv.includes('--filters');
const busy=process.argv.includes('--busy');
const review=process.argv.includes('--review');
const reviewOffline=process.argv.includes('--review-offline');
const canvasTransform=process.argv.includes('--canvas-transform');
const platform=process.argv.includes('--platform');
const persistence=process.argv.includes('--persistence');
const dprIndex=process.argv.indexOf('--dpr'),dpr=dprIndex<0?1:Number(process.argv[dprIndex+1]);
let sourceResult;
let filterResult;
let busyResult;
let reviewResult;
let platformResult;
let persistenceResult;
const sourceIndex=process.argv.indexOf('--source');const sourceFile=sourceIndex<0?null:process.argv[sourceIndex+1];
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
 await phase('keyDown',()=>send('Input.dispatchKeyEvent',{type:'keyDown',key,code,windowsVirtualKeyCode:virtual,nativeVirtualKeyCode:virtual,text,unmodifiedText:text,modifiers}));
 await phase('frames_after_keyDown',()=>frames());
 await phase('keyUp',()=>send('Input.dispatchKeyEvent',{type:'keyUp',key,code,windowsVirtualKeyCode:virtual,nativeVirtualKeyCode:virtual,modifiers}));
 await phase('frames_after_keyUp',()=>frames());
}
async function click(x,y){
 if(canvasTransform){const rect=await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height,w:innerWidth,h:innerHeight}})()');x=rect.x+x*rect.width/rect.w;y=rect.y+y*rect.height/rect.h;}
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
 const line=consoleText('Browser review state: ').at(-1);
 check(line,'Missing real C platform state');
 const state=Object.fromEntries([...line.matchAll(/([a-z_]+)=([^ ]+)/g)].map(match=>[match[1],match[2]]));
 await key('Escape','Escape',27);return state;
}
async function wheelBatch(dx,dy,count=1,options={}){
 const result=await evaluate(`(()=>{const canvas=document.querySelector('canvas'),r=canvas.getBoundingClientRect();let claimed=0;for(let i=0;i<${count};i++){const event=new WheelEvent('wheel',{bubbles:true,cancelable:true,clientX:r.x+r.width*${options.x??.65},clientY:r.y+r.height*${options.y??.65},deltaX:${dx},deltaY:${dy},deltaMode:${options.mode??0},ctrlKey:${!!options.ctrl},shiftKey:${!!options.shift}});canvas.dispatchEvent(event);if(event.defaultPrevented)claimed++;}return claimed;})()`);
 check(result===count,'Canvas wheel default/duplicate ownership failed');await frames(3);
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
  check(report.stages===33,'C/Worker acceptance stage inventory changed');
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
 if(canvasTransform){await evaluate('Object.assign(document.querySelector("canvas").style,{position:"absolute",left:"8%",top:"6%",transform:"scale(.8)",transformOrigin:"top left"})');await frames();}
 const errors=events.filter(e=>e.method==='Runtime.exceptionThrown');check(!errors.length,'Uncaught browser exception');
 const glErrors=events.filter(e=>e.method==='Log.entryAdded'&&/WebGL: (INVALID|too many)/.test(e.params.entry.text));
 check(!glErrors.length,'WebGL reported invalid rendering operations');
 const surface=await evaluate('({width:innerWidth,height:innerHeight,dpr:devicePixelRatio,canvas:[document.querySelector("canvas").width,document.querySelector("canvas").height]})');
 check(surface.width===1280&&surface.height===820,'Viewport mismatch');
 check(surface.dpr===dpr,'Browser device pixel ratio mismatch');
 check(surface.canvas[0]===1280*dpr&&surface.canvas[1]===820*dpr,'Framebuffer is not rendered at actual DPR');
 await capture('wide');
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
   const baseline=await platformSnapshot(),beforeJobs=starts().length;
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
   platformResult={baseline,panned,reversed,trusted,deliveredWheel,shifted,chrome,sidebar,zoomed,restored,densities,embedded,query_jobs:starts().length-beforeJobs,
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
    ['ribbon.display',1170,42],['chrome.help',1262,42],['chrome.title_bar.background',2,14],['ribbon.tabs.background',2,42]]){
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
 await click(294,42);await capture('view-ribbon');
 await click(1174,42);await capture('ribbon-display');await key('Escape','Escape',27);
 await send('Emulation.setDeviceMetricsOverride',{width:640,height:640,deviceScaleFactor:dpr,mobile:false});await frames(4);await capture('narrow');
 const narrow=await evaluate('({width:innerWidth,height:innerHeight,canvas:[document.querySelector("canvas").width,document.querySelector("canvas").height]})');
 check(narrow.width===640&&narrow.height===640&&narrow.canvas[0]===640*dpr&&narrow.canvas[1]===640*dpr,'Narrow canvas mismatch');
 const lateErrors=events.filter(e=>e.method==='Runtime.exceptionThrown'||e.method==='Log.entryAdded'&&/WebGL: (INVALID|too many)/.test(e.params.entry.text));
 check(!lateErrors.length,'Runtime/rendering error after interactions');
 await fs.writeFile(path.join(output,'events.json'),JSON.stringify(events,null,2));
 const resources=await evaluate('({first_frame:performance.getEntriesByName("duxcel:first-frame").map(e=>({start_ms:e.startTime})),resources:performance.getEntriesByType("resource").map(e=>({name:e.name,start_ms:e.startTime,duration_ms:e.duration,transfer_bytes:e.transferSize,encoded_bytes:e.encodedBodySize,decoded_bytes:e.decodedBodySize})),main_js_heap:performance.memory?{used:performance.memory.usedJSHeapSize,total:performance.memory.totalJSHeapSize,limit:performance.memory.jsHeapSizeLimit}:null})');
 resources.c_bridge=consoleText('Browser worker metrics: ').map(line=>JSON.parse(line.slice('Browser worker metrics: '.length)));
 resources.scope='Navigation-to-first completed C frame and parent resource timing; main JS heap excludes Worker/GPU/process RSS. C bridge metrics are review-only bounded callback/job observations; decode_ms is C validation/allocation/copy, elapsed_ms includes asynchronous engine/transport scheduling. Worker network bytes are not inferred from parent Resource Timing.';
 await fs.writeFile(path.join(output,'resources.json'),JSON.stringify(resources,null,2));
 await fs.writeFile(path.join(output,'result.json'),JSON.stringify({status:'passed',checks,url,surface,elapsed_ms:Date.now()-started,steps,sourceFile,sourceResult,filterResult,busyResult,reviewResult,platformResult,persistenceResult,workerProbe,scope:'Shared C99 startup and captured real interactions for visual verification; source imports, filters, active-query frames and review exports are explicit optional gates; BP0 is file compatibility only, browser workbook persistence is not accepted'},null,2));
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
