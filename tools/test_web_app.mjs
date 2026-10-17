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
const canvasTransform=process.argv.includes('--canvas-transform');
const dprIndex=process.argv.indexOf('--dpr'),dpr=dprIndex<0?1:Number(process.argv[dprIndex+1]);
let sourceResult;
let filterResult;
let busyResult;
let reviewResult;
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
async function waitConsole(prefix){for(const deadline=Date.now()+15000;Date.now()<deadline;){const text=consoleText(prefix).at(-1);if(text)return text;await pause(20);}throw new Error('Missing C event: '+prefix);}
function archiveFiles(bytes){
 const files=new Map();
 for(let offset=0;offset+512<=bytes.length;){const header=bytes.subarray(offset,offset+512);if(header.every(byte=>byte===0))break;
  const field=(start,length)=>header.subarray(start,start+length).toString('utf8').replace(/\0.*$/s,'');
  const name=[field(345,155),field(0,100)].filter(Boolean).join('/'),size=parseInt(field(124,12).trim(),8);check(Number.isSafeInteger(size)&&size>=0&&offset+512+size<=bytes.length,'Invalid report archive member');check(!files.has(name),'Duplicate report archive member');files.set(name,bytes.subarray(offset+512,offset+512+size));offset+=512+Math.ceil(size/512)*512;
 }return files;
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
 if(canvasTransform){await evaluate('Object.assign(document.querySelector("canvas").style,{position:"absolute",left:"8%",top:"6%",width:"80%",height:"80%"})');await frames();}
 const errors=events.filter(e=>e.method==='Runtime.exceptionThrown');check(!errors.length,'Uncaught browser exception');
 const glErrors=events.filter(e=>e.method==='Log.entryAdded'&&/WebGL: (INVALID|too many)/.test(e.params.entry.text));
 check(!glErrors.length,'WebGL reported invalid rendering operations');
 const surface=await evaluate('({width:innerWidth,height:innerHeight,dpr:devicePixelRatio,canvas:[document.querySelector("canvas").width,document.querySelector("canvas").height]})');
 check(surface.width===1280&&surface.height===820,'Viewport mismatch');
 check(surface.dpr===dpr,'Browser device pixel ratio mismatch');
 await capture('wide');
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
   await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Control',code:'ControlLeft',windowsVirtualKeyCode:17,modifiers:2});
   await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Shift',code:'ShiftLeft',windowsVirtualKeyCode:16,modifiers:10});
   await key('.','Period',190,'',10);
   await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Shift',code:'ShiftLeft',windowsVirtualKeyCode:16,modifiers:2});
   await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Control',code:'ControlLeft',windowsVirtualKeyCode:17});
   const captured=await waitConsole('Browser review captured: ');await frames();await capture('review-picker');
   await click(530,341);await capture('review-composer');
   for(const character of 'İ filter')await key(character,character===' '?'Space':'KeyI',character.codePointAt(0),character);
   await click(829,554);const prepared=await waitConsole('Browser review prepared: ');
   let archive;
   for(const deadline=Date.now()+15000;Date.now()<deadline;){archive=(await fs.readdir(output)).find(name=>name.endsWith('.tar'));if(archive)break;await pause(20);}
   check(archive,'Browser review did not request a report archive download');
   const files=archiveFiles(await fs.readFile(path.join(output,archive))),manifestName=[...files.keys()].find(name=>name.endsWith('/manifest.json'));
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
   await frames();await capture('review-prepared');await key('Enter','Enter',13);await frames();await capture('review-returned');
   check(starts().length===jobsBefore,'Reporting scheduled unexpected database jobs');
   reviewResult={captured,prepared,archive,report_id:manifest.report_id,frame_id:manifest.frame.frame_id,widget:manifest.target.widget.id,framebuffer:manifest.frame.framebuffer,archive_sha256:createHash('sha256').update(await fs.readFile(path.join(output,archive))).digest('hex')};
   await key('Escape','Escape',27);
  }
  if(sourceFile.endsWith('/yellow_tripdata_2025-01.parquet'))check(imported.total==='3475226'&&imported.columns===20,'Public Parquet source count/schema changed');
 }else{
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
 check(narrow.width===640&&narrow.height===640&&narrow.canvas[0]===640&&narrow.canvas[1]===640,'Narrow canvas mismatch');
 const lateErrors=events.filter(e=>e.method==='Runtime.exceptionThrown'||e.method==='Log.entryAdded'&&/WebGL: (INVALID|too many)/.test(e.params.entry.text));
 check(!lateErrors.length,'Runtime/rendering error after interactions');
 await fs.writeFile(path.join(output,'events.json'),JSON.stringify(events,null,2));
 await fs.writeFile(path.join(output,'result.json'),JSON.stringify({status:'passed',checks,url,surface,elapsed_ms:Date.now()-started,steps,sourceFile,sourceResult,filterResult,busyResult,reviewResult,workerProbe,scope:'Shared C99 startup and captured real interactions for visual verification; source imports, filters, active-query frames and review exports are explicit optional gates; workbook persistence is not accepted'},null,2));
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
