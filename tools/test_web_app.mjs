#!/usr/bin/env node
// Headless Chromium driver for the local shared C UI. Its Python/server parent
// owns the cross-worktree desktop lock. No user browser/profile is accessed.
import {spawn} from 'node:child_process';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
const url=process.argv[2]??'http://127.0.0.1:8767';
const output=path.resolve(process.argv[3]??'build/web/browser-qa/cdp');
await fs.mkdir(output,{recursive:true});
const profile=await fs.mkdtemp(path.join(os.tmpdir(),'duxcel-browser-'));
const browser=spawn(process.env.DUXCEL_CHROMIUM??'/Applications/Chromium.app/Contents/MacOS/Chromium',
 ['--headless','--disable-background-timer-throttling','--disable-renderer-backgrounding','--disable-backgrounding-occluded-windows','--remote-debugging-port=0','--user-data-dir='+profile,'--no-first-run','--no-default-browser-check','--no-proxy-server','about:blank'],{stdio:['ignore','ignore','pipe']});
const workerProbe=process.argv.includes('--worker-probe');
const workerDriver=process.argv.includes('--worker-driver');
const filters=process.argv.includes('--filters');
const busy=process.argv.includes('--busy');
let sourceResult;
let filterResult;
let busyResult;
const sourceIndex=process.argv.indexOf('--source');const sourceFile=sourceIndex<0?null:process.argv[sourceIndex+1];
let stderr='',ws,id=0;const waiting=new Map(),events=[],steps=[];let checks=0;const started=Date.now();
function check(value,message){checks++;if(!value)throw new Error(message);}
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function send(method,params={},timeout=15000){const request=++id;return new Promise((resolve,reject)=>{
 const timer=setTimeout(()=>{waiting.delete(request);reject(new Error('Timed out: '+method));},timeout);
 waiting.set(request,{resolve:value=>{clearTimeout(timer);resolve(value);},reject:error=>{clearTimeout(timer);reject(error);}});
 ws.send(JSON.stringify({id:request,method,params}));});}
async function evaluate(expression,timeout){const result=await send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true},timeout);if(result.exceptionDetails)throw new Error(JSON.stringify(result.exceptionDetails));return result.result?.value;}
async function frames(count=2){await evaluate(`new Promise(resolve=>{let n=${count};function tick(){if(--n<=0)resolve(true);else requestAnimationFrame(tick)}requestAnimationFrame(tick)})`);}
async function key(key,code,virtual,text='',modifiers=0){
 steps.push({key,time:Date.now()});
 await send('Input.dispatchKeyEvent',{type:'keyDown',key,code,windowsVirtualKeyCode:virtual,nativeVirtualKeyCode:virtual,text,unmodifiedText:text,modifiers});await frames();
 await send('Input.dispatchKeyEvent',{type:'keyUp',key,code,windowsVirtualKeyCode:virtual,nativeVirtualKeyCode:virtual,modifiers});await frames();
}
async function click(x,y){
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',x,y});
 await send('Input.dispatchMouseEvent',{type:'mousePressed',x,y,button:'left',clickCount:1});await frames();
 await send('Input.dispatchMouseEvent',{type:'mouseReleased',x,y,button:'left',clickCount:1});await frames();
}
function replies(){return events.filter(e=>e.method==='Runtime.consoleAPICalled').flatMap(e=>e.params.args).map(a=>String(a.value??'')).filter(t=>t.startsWith('Browser worker reply: ')).map(t=>JSON.parse(t.slice('Browser worker reply: '.length)));}
function starts(){return events.filter(e=>e.method==='Runtime.consoleAPICalled').flatMap(e=>e.params.args).map(a=>String(a.value??'')).filter(t=>t.startsWith('Browser worker started: ')).map(t=>JSON.parse(t.slice('Browser worker started: '.length)));}
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

try{
 browser.stderr.on('data',data=>{stderr+=data;});
 for(let i=0;i<100&&!stderr.includes('DevTools listening on');i++)await pause(50);
 const port=stderr.match(/DevTools listening on ws:\/\/127\.0\.0\.1:(\d+)\//)?.[1];check(port,'No owned DevTools endpoint');
 const targets=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json();
 ws=new WebSocket(targets.find(t=>t.type==='page').webSocketDebuggerUrl);
 await new Promise((resolve,reject)=>{ws.addEventListener('open',resolve,{once:true});ws.addEventListener('error',reject,{once:true});});
 ws.addEventListener('message',event=>{const message=JSON.parse(event.data);if(message.id){const request=waiting.get(message.id);waiting.delete(message.id);if(request)message.error?request.reject(new Error(JSON.stringify(message.error))):request.resolve(message.result);}else events.push(message);});
 await send('Runtime.enable');await send('Debugger.enable');await send('Page.enable');await send('Log.enable');await send('Network.enable');
 await send('Emulation.setFocusEmulationEnabled',{enabled:true});
 await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:1,mobile:false});
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
 const errors=events.filter(e=>e.method==='Runtime.exceptionThrown');check(!errors.length,'Uncaught browser exception');
 const glErrors=events.filter(e=>e.method==='Log.entryAdded'&&/WebGL: (INVALID|too many)/.test(e.params.entry.text));
 check(!glErrors.length,'WebGL reported invalid rendering operations');
 const surface=await evaluate('({width:innerWidth,height:innerHeight,dpr:devicePixelRatio,canvas:[document.querySelector("canvas").width,document.querySelector("canvas").height]})');
 check(surface.width===1280&&surface.height===820,'Viewport mismatch');
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
  if(sourceFile.endsWith('/yellow_tripdata_2025-01.parquet'))check(imported.total==='3475226'&&imported.columns===20,'Public Parquet source count/schema changed');
 }else{
 for(const digit of '9007199254740993')await key(digit,'Digit'+digit,digit.charCodeAt(0),digit);
 await key('Enter','Enter',13);await key('ArrowUp','ArrowUp',38);await capture('edited');
 await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Shift',code:'ShiftLeft',windowsVirtualKeyCode:16,modifiers:8});
 await key('ArrowRight','ArrowRight',39,'',8);await key('ArrowDown','ArrowDown',40,'',8);
 await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Shift',code:'ShiftLeft',windowsVirtualKeyCode:16});await frames();await capture('selection');
 }
 await click(294,42);await capture('view-ribbon');
 await click(1174,42);await capture('ribbon-display');await key('Escape','Escape',27);
 await send('Emulation.setDeviceMetricsOverride',{width:640,height:640,deviceScaleFactor:1,mobile:false});await frames(4);await capture('narrow');
 const narrow=await evaluate('({width:innerWidth,height:innerHeight,canvas:[document.querySelector("canvas").width,document.querySelector("canvas").height]})');
 check(narrow.width===640&&narrow.height===640&&narrow.canvas[0]===640&&narrow.canvas[1]===640,'Narrow canvas mismatch');
 const lateErrors=events.filter(e=>e.method==='Runtime.exceptionThrown'||e.method==='Log.entryAdded'&&/WebGL: (INVALID|too many)/.test(e.params.entry.text));
 check(!lateErrors.length,'Runtime/rendering error after interactions');
 await fs.writeFile(path.join(output,'events.json'),JSON.stringify(events,null,2));
 await fs.writeFile(path.join(output,'result.json'),JSON.stringify({status:'passed',checks,url,surface,elapsed_ms:Date.now()-started,steps,sourceFile,sourceResult,filterResult,busyResult,workerProbe,scope:'Shared C99 startup and captured real interactions for visual verification; source imports, value/date filters, active-query frames and Worker probe are explicit optional gates; workbook persistence is not accepted'},null,2));
 console.log(JSON.stringify({status:'passed',checks,surface,output}));
 }
}catch(error){
 try{await send('Debugger.pause');await pause(300);await fs.writeFile(path.join(output,'pause.json'),JSON.stringify(events.filter(e=>e.method==='Debugger.paused'),null,2));await send('Debugger.resume');}catch{}
 try{await fs.writeFile(path.join(output,'failed.png'),Buffer.from((await send('Page.captureScreenshot',{format:'png'})).data,'base64'));}catch{}
 await fs.writeFile(path.join(output,'events.json'),JSON.stringify(events,null,2));
 await fs.writeFile(path.join(output,'result.json'),JSON.stringify({status:'failed',checks,error:String(error),url,elapsed_ms:Date.now()-started,steps},null,2));
 console.error(error);process.exitCode=1;
}finally{
 ws?.close();browser.kill('SIGTERM');
 await Promise.race([new Promise(resolve=>browser.once('exit',resolve)),pause(2000)]);
 if(browser.exitCode===null)browser.kill('SIGKILL');
 await fs.writeFile(path.join(output,'browser.txt'),stderr);
 await fs.rm(profile,{recursive:true,force:true});
}
