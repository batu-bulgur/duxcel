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
const feedbackReviewOnly=process.argv.includes('--feedback-review-only');
const workspaceReviewOnly=process.argv.includes('--workspace-review-only');
const exportCancelOnly=process.argv.includes('--export-cancel-only');
const exportOnly=process.argv.includes('--export-only');
const copyOnly=process.argv.includes('--copy-only');
const focusOnly=process.argv.includes('--focus-only');
const reviewErrorOnly=process.argv.includes('--review-error-only');
const reviewCellOnly=process.argv.includes('--review-cell-only');
const reviewGuideOnly=process.argv.includes('--review-guide-only');
const accessOnly=process.argv.includes('--access-only');
const zoomOnly=process.argv.includes('--zoom-only');
const reviewControlsOnly=process.argv.includes('--review-controls-only');
const analysisOnly=process.argv.includes('--analysis-only');
const analysisColumnsOnly=process.argv.includes('--analysis-columns-only');
const reviewSQLOnly=process.argv.includes('--review-sql-only');
const reviewQueryOnly=process.argv.includes('--review-query-only');
const mixedStatisticsOnly=process.argv.includes('--mixed-statistics-only');
const tableStatisticsOnly=process.argv.includes('--table-statistics-only');
const reviewValuesOnly=process.argv.includes('--review-values-only');
const analysisStatisticsOnly=process.argv.includes('--analysis-statistics-only');
const statisticsOnly=process.argv.includes('--statistics-only');
const queryApply=process.argv.includes('--query-apply');
const reviewOffline=process.argv.includes('--review-offline');
const canvasTransform=process.argv.includes('--canvas-transform');
const platform=process.argv.includes('--platform');
const altStress=process.argv.includes('--alt-stress');
const persistenceOwners=process.argv.includes('--persistence-owners');
const persistence=process.argv.includes('--persistence')||persistenceOwners;
const sourceIndex=process.argv.indexOf('--source');const sourceFile=sourceIndex<0?null:process.argv[sourceIndex+1];
if((exportOnly||exportCancelOnly)&&(!review||!sourceFile||path.basename(sourceFile)!=='null-text.csv'||process.argv.some(flag=>/^--.*-only$/.test(flag)&&flag!==(exportCancelOnly?'--export-cancel-only':'--export-only'))||['--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--persistence-owners','--query-apply','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error((exportCancelOnly?'--export-cancel-only':'--export-only')+' requires --review/null-text.csv and excludes other profiles/engine/report-upload options');
if(feedbackReviewOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||process.argv.some(flag=>/^--.*-only$/.test(flag)&&flag!=='--feedback-review-only')||['--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--persistence-owners','--query-apply','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--feedback-review-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options');
if(workspaceReviewOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||process.argv.some(flag=>/^--.*-only$/.test(flag)&&flag!=='--workspace-review-only')||['--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--persistence-owners','--query-apply','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--workspace-review-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options');
if(copyOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='null-text.csv'||['--focus-only','--review-error-only','--review-cell-only','--review-guide-only','--access-only','--zoom-only','--review-controls-only','--analysis-only','--analysis-columns-only','--analysis-statistics-only','--statistics-only','--review-query-only','--review-sql-only','--review-values-only','--table-statistics-only','--mixed-statistics-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--persistence-owners','--query-apply','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--copy-only requires --review/null-text.csv and excludes other profiles/engine/report-upload options');
if(focusOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||['--review-error-only','--review-cell-only','--review-guide-only','--access-only','--zoom-only','--review-controls-only','--analysis-only','--analysis-columns-only','--analysis-statistics-only','--statistics-only','--review-query-only','--review-sql-only','--review-values-only','--table-statistics-only','--mixed-statistics-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--persistence-owners','--query-apply','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--focus-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options');
if(reviewErrorOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||['--review-cell-only','--review-guide-only','--access-only','--zoom-only','--review-controls-only','--analysis-only','--analysis-columns-only','--analysis-statistics-only','--statistics-only','--review-query-only','--review-sql-only','--review-values-only','--table-statistics-only','--mixed-statistics-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--persistence-owners','--query-apply','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--review-error-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options');
if(reviewCellOnly&&(!review||['--source','--review-guide-only','--access-only','--zoom-only','--review-controls-only','--analysis-only','--analysis-columns-only','--analysis-statistics-only','--statistics-only','--review-query-only','--review-sql-only','--review-values-only','--table-statistics-only','--mixed-statistics-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--persistence-owners','--query-apply','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--review-cell-only requires --review and excludes source/other profiles/engine/report-upload options');
if(reviewGuideOnly&&(!review||['--source','--access-only','--zoom-only','--review-controls-only','--analysis-only','--analysis-columns-only','--analysis-statistics-only','--statistics-only','--review-query-only','--review-sql-only','--review-values-only','--table-statistics-only','--mixed-statistics-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--persistence-owners','--query-apply','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--review-guide-only requires --review and excludes source/other profiles/engine/report-upload options');
if(review&&workerProbe)throw new Error('--review and --worker-probe require separate artifact/server profiles');
if(accessOnly&&(!review||['--review-sql-only','--mixed-statistics-only','--table-statistics-only','--review-values-only','--analysis-statistics-only','--statistics-only','--review-query-only','--analysis-only','--analysis-columns-only','--review-controls-only','--source','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline'].some(flag=>process.argv.includes(flag))))throw new Error('--access-only requires --review and excludes source/engine/platform/report-upload options');
if(zoomOnly&&(!review||['--review-sql-only','--mixed-statistics-only','--table-statistics-only','--review-values-only','--analysis-statistics-only','--statistics-only','--review-query-only','--analysis-only','--analysis-columns-only','--review-controls-only','--access-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline'].some(flag=>process.argv.includes(flag))))throw new Error('--zoom-only requires --review and excludes access/engine/platform/report-upload options');
if(reviewControlsOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||['--review-sql-only','--mixed-statistics-only','--table-statistics-only','--review-values-only','--analysis-statistics-only','--statistics-only','--review-query-only','--analysis-only','--analysis-columns-only','--access-only','--zoom-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--review-controls-only requires --review/transactions.csv and excludes access/zoom/engine/platform/report-upload options');
if((analysisOnly||analysisColumnsOnly||analysisStatisticsOnly||tableStatisticsOnly||mixedStatisticsOnly)&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||Number(analysisOnly)+Number(analysisColumnsOnly)+Number(analysisStatisticsOnly)+Number(tableStatisticsOnly)+Number(mixedStatisticsOnly)>1||['--review-sql-only','--review-values-only','--statistics-only','--review-query-only','--access-only','--zoom-only','--review-controls-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('Analytical profiles require --review/transactions.csv and exclude other focused profiles or report-upload options');
if(reviewQueryOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||['--review-sql-only','--mixed-statistics-only','--table-statistics-only','--review-values-only','--analysis-statistics-only','--statistics-only','--analysis-only','--analysis-columns-only','--access-only','--zoom-only','--review-controls-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--review-query-only requires --review/transactions.csv and excludes other profiles or report-upload options');
if(statisticsOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||['--review-sql-only','--mixed-statistics-only','--table-statistics-only','--review-values-only','--analysis-statistics-only','--review-query-only','--analysis-only','--analysis-columns-only','--access-only','--zoom-only','--review-controls-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--statistics-only requires --review/transactions.csv and excludes other profiles or report-upload options');
if(reviewValuesOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='null-text.csv'||['--review-sql-only','--mixed-statistics-only','--table-statistics-only','--analysis-statistics-only','--statistics-only','--review-query-only','--analysis-only','--analysis-columns-only','--access-only','--zoom-only','--review-controls-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--review-values-only requires --review/null-text.csv and excludes other profiles or report-upload options');
if(reviewSQLOnly&&(!review||!sourceFile||path.basename(sourceFile)!=='transactions.csv'||['--mixed-statistics-only','--table-statistics-only','--review-values-only','--analysis-statistics-only','--statistics-only','--review-query-only','--analysis-only','--analysis-columns-only','--access-only','--zoom-only','--review-controls-only','--filters','--busy','--worker-driver','--worker-probe','--platform','--alt-stress','--persistence','--review-offline','--review-outbox'].some(flag=>process.argv.includes(flag))))throw new Error('--review-sql-only requires --review/transactions.csv and excludes other profiles or report-upload options');
if(persistenceOwners&&['--persistence','--worker-probe','--worker-driver','--source','--review','--filters','--busy','--access-only','--zoom-only','--review-controls-only','--analysis-only','--analysis-columns-only','--analysis-statistics-only','--statistics-only','--review-query-only','--review-sql-only','--review-values-only','--table-statistics-only','--mixed-statistics-only','--platform','--alt-stress','--canvas-transform','--query-apply','--review-outbox','--review-offline'].some(flag=>process.argv.includes(flag)))throw new Error('--persistence-owners is a separate normal-artifact v13 proof without other profiles/source/engine/report options');
if(queryApply&&!reviewQueryOnly)throw new Error('--query-apply requires --review-query-only');
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
let queryReviewResult;
let sqlReviewResult;
let statisticsResult;
let reviewValuesResult;
let reviewGuideResult;
let reviewCellResult;
let reviewErrorResult;
let focusResult;
let copyResult;
let exportResult;
let exportCancelResult;
let workspaceReviewResult;
let feedbackReviewResult;
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
function copyClipboardHarnessSource(){
 return `(()=>{const pending=new Map(),state={mode:'defer',memory:'owned clipboard baseline',attempts:[],fallbacks:[]};
 const writeText=text=>{text=String(text);const attempt={index:state.attempts.length,text,mode:state.mode,status:'pending'};state.attempts.push(attempt);
  if(state.mode==='throw'){attempt.status='threw';throw new Error('Owned clipboard synchronous failure');}
  if(state.mode==='reject'){attempt.status='rejected';return Promise.reject(new Error('Owned clipboard rejected write'));}
  if(state.mode==='success'){state.memory=text;attempt.status='fulfilled';return Promise.resolve();}
  return new Promise((resolve,reject)=>pending.set(attempt.index,{resolve:()=>{state.memory=text;attempt.status='fulfilled';resolve();},reject:()=>{attempt.status='rejected';reject(new Error('Owned deferred clipboard rejection'));}}));};
 const clipboard=Object.freeze({writeText,readText:()=>Promise.resolve(state.memory),write:()=>Promise.reject(new Error('Owned clipboard supports text only'))});
 Object.defineProperty(navigator,'clipboard',{configurable:true,get:()=>state.mode==='unavailable'?undefined:clipboard});
 const exec=document.execCommand?.bind(document);document.execCommand=(command,...args)=>{if(/^(copy|cut|paste)$/i.test(command)){state.fallbacks.push('execCommand:'+command);throw new Error('OS clipboard fallback is disabled by owned acceptance');}return exec?.(command,...args)??false;};
 for(const name of ['copy','cut','paste'])window.addEventListener(name,event=>{event.preventDefault();state.fallbacks.push('default:'+name);},true);
 state.resolve=index=>{const entry=pending.get(index);if(!entry)throw new Error('Unknown owned clipboard Promise');pending.delete(index);entry.resolve();};
 state.reject=index=>{const entry=pending.get(index);if(!entry)throw new Error('Unknown owned clipboard Promise');pending.delete(index);entry.reject();};
 Object.defineProperty(globalThis,'duxcelTestClipboard',{value:state});})();`;
}
function persistenceOwnerReport(report){
 const highWater=['9007199254740994','18446744073709551615','0'];
 check(report.mode==='owners'&&report.metadataVersion===13&&JSON.stringify(report.ownerHighWater)===JSON.stringify(highWater),'Owner persistence probe did not verify exact v13 uint64 allocation lifetimes');
 const metadata=report.checks.find(step=>step.status==='passed'&&step.details?.version===13)?.details;
 check(metadata?.activeSheet==='0'&&JSON.stringify(metadata.sheetNames)===JSON.stringify(['Owner lifetime','Exhausted owner','Never allocated'])&&JSON.stringify(metadata.ownerHighWater)===JSON.stringify(highWater),'Owner persistence attachment did not retain its three actual sheet/owner identities');
 const names=['cells','columns','filters','format_guards','format_scopes','format_selectors','formats','marks','meta','object_counters','objects','pivot_fields','preferences','queries','sheets','sorts','source_options','table_filters','table_sorts','table_sources'];
 check(Array.isArray(metadata.tables)&&JSON.stringify(metadata.tables.map(table=>table.name))===JSON.stringify(names),'Owner persistence did not verify all 16 standard and four v13 extension metadata tables');
 const counts={cells:'2',format_guards:'0',format_scopes:'0',format_selectors:'2',formats:'2',meta:'1',object_counters:'3',objects:'1',sheets:'3'};
 check(Object.entries(counts).every(([name,count])=>metadata.tables.find(table=>table.name===name)?.rows===count),'Owner persistence lost sparse cells, one live object, spatial-only formats or per-sheet high-water rows');
 return{version:metadata.version,activeSheet:metadata.activeSheet,sheetNames:metadata.sheetNames,highWater,tables:metadata.tables.map(({name,rows})=>({name,rows}))};
}
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
  const plain=state.author_parent+'.action.'+key,stableId=id===plain||['assign_field','marks'].includes(kind)&&key==='assign'&&new RegExp('^'+plain.replaceAll('.','\\.')+'\\.f[0-9a-f]{16}$').test(id);
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
 check(['rows','columns','values'].includes(role),'Unknown analytical assignment role');
 await ribbonAnalysisAction(role==='rows'?'r':role==='columns'?'c':'v');
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
async function browserAnalysis(columnsMode=false,statisticsMode=false,tableMode=false,sqlMode=false){
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
 const previous=replies().filter(reply=>reply.kind===5).at(-1)?.id,values=await assignAnalysisField('Amount','values');let reply=await waitPivot(previous??null);await frames(4);
 check(reply.rows===4&&reply.columns===2&&reply.total==='4'&&!reply.pending,'Real Rows City/SUM Amount result is not four grouped rows/two columns');
 let published=await platformSnapshot(),model=analysisObject(published);
 check(model.counts.join(',')==='1,0,1,0'&&model.page.join(',')==='0,4,2,4,0,0','App did not publish the complete actual matrix into its selected object cache');
 check(published.totals_count==='4,1'&&published.object_grand0_null==='0'&&published.object_grand0_truncated==='0','Actual matrix source-derived row/grand totals are unavailable or truncated');
 let grand=Buffer.from(published.object_grand0_hex??'','hex').toString('utf8');check(Number(grand)===145.25,'Real source grand total differs from 145.25');
 if(sqlMode)return browserGeneratedSQL({source,sentinel,baseline,empty,rows,rowsState,values,reply,published,grand});
 if(tableMode)return browserTableStatistics({source,sentinel,baseline,empty,rows,rowsState,values,reply,published,grand});
 if(statisticsMode)return browserAnalysisStatistics({source,sentinel,baseline,empty,rows,rowsState,values,reply,published,grand});
 let columns,categories,rowsFoundation;
 if(columnsMode){
  rowsFoundation={reply,published,grand};await capture('analysis-columns-rows-foundation');
  columns=await assignAnalysisField('Approved','columns');reply=await waitPivot(reply.id);await frames(4);
  check(reply.rows===4&&reply.columns===3&&reply.total==='4'&&!reply.pending,'Real Columns Approved result is not four City rows and two category measures');
  published=await platformSnapshot();model=analysisObject(published);
  check(model.counts.join(',')==='1,1,1,0'&&model.page.join(',')==='0,4,3,4,0,0','App did not publish the complete actual Columns matrix into its selected object cache');
  check(published.totals_count==='4,1'&&published.totals_shape==='4,1,1,3,1','Columns matrix did not retain source-derived row/category/grand footer shapes');
  categories=['false','true'].map((expected,index)=>{
   const prefix='object_category'+index+'_';
   check(published[prefix+'present']==='1'&&published[prefix+'null']==='0'&&published[prefix+'truncated']==='0','Actual Columns header category is missing, NULL or truncated');
   const text=Buffer.from(published[prefix+'hex']??'','hex').toString('utf8');check(text===expected,'Actual Columns header did not retain its typed '+expected+' category');return text;
  });
  check(published.object_grand0_null==='0'&&published.object_grand0_truncated==='0','Columns matrix grand total is NULL or truncated');
  grand=Buffer.from(published.object_grand0_hex??'','hex').toString('utf8');check(Number(grand)===145.25,'Columns source grand total differs from 145.25');
 }
 const [logicalWidth,logicalHeight]=published.logical.split(',').map(Number);
 for(const field of ['matrix_rect','preview_rect']){const rect=published[field]?.split(',').map(Number);check(rect?.length===4&&rect.every(Number.isFinite)&&rect[0]>=0&&rect[1]>=0&&rect[2]>0&&rect[3]>0&&rect[0]+rect[2]<=logicalWidth+.01&&rect[1]+rect[3]<=logicalHeight+.01,'Actual analysis did not expose a fully visible '+field);}
 check(published.manual_cells===sentinel.manual_cells&&['sheet','page','source_revision','schema'].every(field=>published[field]===baseline[field]),'Analytical publication changed the primary source or sparse-cell ownership');
 await capture(columnsMode?'analysis-columns-matrix-footer':'analysis-matrix-footer');await pause(220);await frames(3);const idleBefore=starts().length,idleStart=Date.now();await pause(250);await frames(3);const idleMilliseconds=Date.now()-idleStart;
 check(starts().length===idleBefore,'Idle matrix rendering submitted additional database jobs');
 await ribbonAnalysisAction('d');const returned=await platformSnapshot();
 check(returned.selected_object==='-1'&&returned.object_count==='1'&&returned.manual_cells==='1'&&['sheet','page','source_revision','schema','active','anchor','extent'].every(field=>returned[field]===baseline[field]),'Return to source lost its cached source selection, sparse data or same-sheet pivot');
 await capture('analysis-returned-source');for(let i=0;i<7;i++)await key('ArrowRight','ArrowRight',39);for(let i=0;i<9;i++)await key('ArrowDown','ArrowDown',40);
 const retained=await platformSnapshot();check(retained.active==='9,7'&&retained.manual_cells==='1'&&retained.object_count==='1','Returned worksheet could not revisit the retained H10 sentinel beside its pivot');await capture('analysis-retained-manual');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Analysis-only capture unexpectedly staged a report');
 const result={source,sentinel,baseline,empty,rows,rowsState,values,columns,categories,rowsFoundation,reply,published,grand,returned,retained,idle:{minimum_milliseconds:250,observed_milliseconds:idleMilliseconds,started_jobs:idleBefore},
  scope:columnsMode?'Actual shared C Rows/Values foundation, real Add Columns Approved action and official Worker 4x3 Columns PIVOT, cached false/true category headers, source-derived row/category/grand footer shapes and 145.25 overall sum, selected C result publication, source/manual retention and a bounded no-idle-job interval. Detailed analytical/null semantics remain with the official Worker gate; broad analytics, physical input and report upload are separate.':'Actual shared C worksheet import/manual entry, anchored empty analysis and real Rows/Values menus, official Worker 4x2 matrix with source-derived 145.25 grand total, cached C App publication, visible matrix/footer/source/sparse-cell captures and a bounded no-idle-job interval. Columns pivot, broad analytical semantics, physical input and report-upload acceptance remain separate.'};
 await fs.writeFile(path.join(output,columnsMode?'analysis-columns.json':'analysis.json'),JSON.stringify(result,null,2));return result;
}
const workspaceControlIds={fields_search:'pane.fields.search',context_browse:'analysis.context.browse',rows_add:'analysis.shelf.rows.add',columns_add:'analysis.shelf.columns.add',representation:'analysis.marks.representation',values_add:'analysis.shelf.values.add',filters_add:'analysis.shelf.filters.add',color:'analysis.marks.target.color'};
function workspaceState(state,{populated=false}={}){
 const model=analysisObject(state),[width,height]=state.logical.split(',').map(Number),targets={};check(state.modal==='0'&&state.file_menu==='0'&&state.busy==='0'&&state.io_busy==='0'&&model.counts.join(',')===(populated?'1,1,1,1':'0,0,0,0'),'Workspace capture does not own the actual idle empty/populated pivot');
 const target=(part,id)=>{const bounds=state['workspace_'+part+'_rect']?.split(',').map(Number);check(bounds?.length===4&&bounds.every(Number.isFinite)&&bounds[2]>0&&bounds[3]>0&&bounds[0]>=0&&bounds[1]>=0&&bounds[0]+bounds[2]<=width+.01&&bounds[1]+bounds[3]<=height+.01,'Workspace omitted or mis-clipped its actual '+part+' target');targets[part]={part,id,bounds};};
 for(const [part,id]of Object.entries(workspaceControlIds))target(part,id);
 const more=state.workspace_field_more_id,fields=['ID','City','Amount','Date','Approved','Note'];check(fields.some(name=>more==='pane.fields.field.f'+fieldHash(name)+'.more'),'First visible Fields overflow target lost its real source-field identity');target('field_more',more);
 const tokens={rows_token:'analysis.shelf.rows.token.f'+fieldHash('City')+'.distinct.exact',columns_token:'analysis.shelf.columns.token.f'+fieldHash('Approved')+'.distinct.exact',values_remove:'analysis.shelf.values.token.f'+fieldHash('Amount')+'.sum.exact.remove'};
 if(populated){for(const [part,id]of Object.entries(tokens)){check(state['workspace_'+part+'_id']===id,'Workspace token lost its actual field/aggregate/grain identity');target(part,id);}check(state.workspace_values_remove_enabled==='0','Sole SUM measure removal was not honestly disabled');check(model.page.join(',')==='0,4,3,4,0,0'&&state.totals_shape==='4,1,1,3,1'&&state.object_grand0_null==='0'&&state.object_grand0_truncated==='0'&&Number(Buffer.from(state.object_grand0_hex??'','hex').toString('utf8'))===145.25,'Workspace authoring replaced the actual complete Columns matrix or source-derived grand total');}
 else check([...Object.keys(tokens).flatMap(part=>['workspace_'+part+'_id','workspace_'+part+'_rect']),'workspace_values_remove_enabled'].every(name=>state[name]===undefined),'Empty pivot fabricated assigned tokens or a measure-removal target');
 return{model,targets,populated};
}
function workspaceRetained(before,after,message){
 const fields=[...authorOwnedFields,'focus','ribbon','selected_object','object_count','object','object_revision','object_page','pivot_counts','totals_count','totals_shape','object_grand0_null','object_grand0_hex','object_grand0_truncated',...Object.keys(before).filter(name=>name.startsWith('workspace_')||name.startsWith('stats_')||name.startsWith('query_')||name.startsWith('clipboard_'))];
 check(fields.every(name=>before[name]===after[name]),message);
}
async function workspacePick(part,name,{populated=false,exposed=false}={}){
 const jobs=starts().length,captured=await beginReview(),before=capturedState(),workspace=workspaceState(before,{populated}),target=workspace.targets[part];check(target,'Missing real workspace pick '+part);
 const surface=await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height};})()');[surface.logicalWidth,surface.logicalHeight]=before.logical.split(',').map(Number);const reachable=reachableGuidePoint(target.bounds,surface),top=reachable.logical.y>surface.logicalHeight/2;if(top!==authorPickerTop){await key(' ','Space',32,' ');authorPickerTop=top;}if(exposed)await capture(name+'-picker');
 const count=consoleText('Review picked: ').length;await send('Input.dispatchMouseEvent',{type:'mouseMoved',...reachable.css});await send('Input.dispatchMouseEvent',{type:'mousePressed',...reachable.css,button:'left',clickCount:1});await frames();await send('Input.dispatchMouseEvent',{type:'mouseReleased',...reachable.css,button:'left',clickCount:1});await frames();const picked=await waitConsole('Review picked: ',count);check(picked==='Review picked: '+target.id,'Frozen workspace picker selected covered content instead of '+target.id+': '+picked);if(part==='values_remove')await capture(name+'-disabled-composer');await key('Escape','Escape',27);const after=await platformSnapshot();workspaceState(after,{populated});workspaceRetained(before,after,'Workspace capture/composer cancellation changed source/model/pages/input or authoring state');check(starts().length===jobs,'Workspace review activated an authoring control or submitted SQL');return{captured,target,reachable,picked,before,after};
}
async function workspaceAssign(role,name){
 const state=await platformSnapshot(),key='workspace_'+role+'_add_rect',bounds=state[key]?.split(',').map(Number);check(bounds?.length===4&&bounds[2]>0&&bounds[3]>0,'Actual workspace '+role+' Add control is not painted');await click(bounds[0]+bounds[2]/2,bounds[1]+bounds[3]/2);
 const menu=authorState(await platformSnapshot(),'assign_field'),id='author.assign_field.action.assign.f'+fieldHash(name),target=menu.rows.find(row=>row.id===id);check(target?.enabled,'Actual workspace '+role+' chooser omitted '+name);const [x,y,w,h]=target.bounds;await click(x+w/2,y+h/2);return{role,name,id};
}
async function browserWorkspaceReview(){
 await send('Emulation.setDeviceMetricsOverride',{width:1440,height:988,deviceScaleFactor:dpr,mobile:false});await frames(4);const source=await zoomSource();await sourceHome();for(let index=0;index<7;index++)await key('ArrowRight','ArrowRight',39);for(let index=0;index<9;index++)await key('ArrowDown','ArrowDown',40);for(const digit of '991')await key(digit,'Digit'+digit,digit.charCodeAt(0),digit);await key('Enter','Enter',13);await key('ArrowUp','ArrowUp',38);const manual=await platformSnapshot();check(manual.manual_cells==='1'&&manual.active==='9,7','Workspace fixture did not retain its actual H10 sparse sentinel');await sourceHome();const baseline=await platformSnapshot(),[x,y]=baseline.body.split(',').map(Number),[cw,rh]=baseline.grid_cell.split(',').map(Number);await rightClick(x+cw/2,y+rh/2);await authorActivate('source_context','create_analysis');const empty=await platformSnapshot();workspaceState(empty);check(empty.ui_scale==='1.000000'&&empty.sheet===baseline.sheet&&empty.page===baseline.page&&empty.manual_cells==='1','Empty workspace changed source/manual ownership or scale');await capture('workspace-empty');const emptyJobs=starts().length,emptyPicks=[];
 for(const part of [...Object.keys(workspaceControlIds),'field_more'])emptyPicks.push(await workspacePick(part,'workspace-empty-'+part,{exposed:part==='fields_search'}));check(starts().length===emptyJobs,'Empty workspace review activated a control');
 await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('7','Digit7',55);await key('Home','Home',36);for(let index=0;index<3;index++)await key('ArrowDown','ArrowDown',40);await key('Enter','Enter',13);await send('Emulation.setDeviceMetricsOverride',{width:1920,height:1440,deviceScaleFactor:dpr,mobile:false});await frames(4);
 const rows=await workspaceAssign('rows','City'),prior=replies().filter(reply=>reply.kind===5).at(-1)?.id,values=await workspaceAssign('values','Amount'),rowsReply=await waitPivot(prior??null);check(rowsReply.rows===4&&rowsReply.columns===2&&rowsReply.total==='4','Actual workspace Rows/Values buttons did not publish the real grouped matrix');const columns=await workspaceAssign('columns','Approved'),columnsReply=await waitPivot(rowsReply.id);check(columnsReply.rows===4&&columnsReply.columns===3&&columnsReply.total==='4','Actual workspace Columns button did not publish the real Approved categories');await frames(4);
 const previousDistinct=replies().filter(reply=>reply.kind===4).at(-1)?.id,filter=await workspaceAssign('filters','Note'),distinct=await waitDistinct(previousDistinct??null),filterDraft=await platformSnapshot();check(distinct.distinct_count===5&&distinct.distinct_total==='5'&&filterDraft.filter_field==='5'&&filterDraft.filter_focus==='3','Actual Note filter did not enumerate its five real values including NULL');await key('Tab','Tab',9);await key(' ','Space',32,' ');await key('Tab','Tab',9);await key('Home','Home',36);for(let index=0;index<5;index++){await key(' ','Space',32,' ');if(index<4)await key('ArrowDown','ArrowDown',40);}await key('Enter','Enter',13);const filteredReply=await waitPivot(columnsReply.id);check(filteredReply.rows===4&&filteredReply.columns===3&&filteredReply.total==='4','Selecting all five actual Note values changed the matrix instead of installing a complete explicit filter');await frames(4);
 const beforeColor=await platformSnapshot(),color=beforeColor.workspace_color_rect?.split(',').map(Number);check(color?.length===4&&color[2]>0&&color[3]>0,'Actual Color target is not visible at150 percent');await click(color[0]+color[2]/2,color[1]+color[3]/2);const marks=authorState(await platformSnapshot(),'marks'),city=marks.rows.find(row=>row.id==='author.marks.action.assign.f'+fieldHash('City'));check(city?.enabled,'Actual Color chooser omitted City with its stable field identity');await click(city.bounds[0]+city.bounds[2]/2,city.bounds[1]+city.bounds[3]/2);const colored=await platformSnapshot();workspaceState(colored,{populated:true});check(colored.ui_scale==='1.500000'&&['sheet','page','source_revision','schema','manual_cells'].every(name=>colored[name]===baseline[name]),'Populated150 percent workspace lost its source/manual owner');
 const colorBounds=colored.workspace_color_rect.split(',').map(Number);await click(colorBounds[0]+colorBounds[2]/2,colorBounds[1]+colorBounds[3]/2);const assigned=authorState(await platformSnapshot(),'marks');check(assigned.rows.some(row=>row.key==='clear'&&row.enabled),'Normal City Color assignment did not create a retained Marks assignment');await key('Escape','Escape',27);const published=await platformSnapshot();workspaceState(published,{populated:true});await capture('workspace-populated-150');const populatedPicks=[],jobs=starts().length;
 for(const part of ['rows_token','columns_token','values_remove','color'])populatedPicks.push(await workspacePick(part,'workspace-populated-'+part,{populated:true,exposed:part==='rows_token'}));const retained=await platformSnapshot();workspaceState(retained,{populated:true});workspaceRetained(published,retained,'Populated workspace picking changed its analytical/source/input cache');check(starts().length===jobs,'Populated workspace review removed its sole measure or submitted queries');await pause(300);await frames(3);const idle=await platformSnapshot();workspaceRetained(retained,idle,'Idle workspace changed its retained model or caches');check(starts().length===jobs,'Idle authoring rendering resubmitted SQL');
 await ribbonAnalysisAction('d');await sourceHome();for(let index=0;index<7;index++)await key('ArrowRight','ArrowRight',39);for(let index=0;index<9;index++)await key('ArrowDown','ArrowDown',40);const returned=await platformSnapshot();check(returned.selected_object==='-1'&&returned.object_count==='1'&&returned.active==='9,7'&&returned.manual_cells==='1'&&['sheet','page','source_revision','schema'].every(name=>returned[name]===baseline[name]),'Workspace review lost the retained primary source or H10 manual sentinel');check(starts().length===jobs,'Returning to the cached source/manual cells submitted SQL');check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Workspace review staged a saved report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Workspace review wrote into the persistent queue');
 const result={source,manual,baseline,empty,emptyPicks,rows,values,rowsReply,columns,columnsReply,filter,distinct,filterDraft,filteredReply,beforeColor,colored,assigned:assigned.state,published,populatedPicks,retained,idle,returned,scope:'Actual empty same-sheet pivot workspace at100 percent and normal shared-C Rows City/Columns Approved/SUM Amount, complete explicit five-value Note filter and City Color assignment at150 percent. Thirteen frozen picks use actual paint-site clipped rectangles/semantic IDs across Fields, compact shelves and Marks, including the honestly disabled sole SUM remove. Capture/composer cancellation preserves observed model/pages/source/input/request ownership and adds no jobs; idle and source/manual return stay cached. Native372 separately verifies complete registry scope and owned input/model internals. No clipboard, saved reports or physical gesture claim.'};await fs.writeFile(path.join(output,'workspace-review.json'),JSON.stringify(result,null,2));return result;
}
function browserPicker(state,{pending=false,row=0,column=0,source=state}={}){
 const parts=state.browser_picker?.split(',');check(parts?.length===6&&parts.every(value=>/^[0-9]+$/.test(value)),'Missing actual browser chooser intent/destination ownership');
 if(pending)check(parts.join(',')===['2','0',source.source_revision,source.sheet,String(row),String(column)].join(','),'Import Table chooser did not pin the actual source/sheet/destination');
 else check(parts[0]==='0'&&parts[1]==='0','Completed browser chooser retained a pending owned result');return parts;
}
async function tableImportChooser(source){
 await send('Page.setInterceptFileChooserDialog',{enabled:true});await key('Alt','AltLeft',18);await key('f','KeyF',70);const file=await platformSnapshot();
 check(file.file_menu==='1'&&file.file_visible==='8','Actual File menu omitted its eight-command Import/Export extensions');
 await key('i','KeyI',73);await frames();const document=await send('DOM.getDocument'),input=await send('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#duxcel-file-input'});
 check(input.nodeId,'Alt F I did not invoke the actual browser Import Table chooser');
 await evaluate(`(()=>{const input=document.querySelector('#duxcel-file-input');globalThis.duxcelFileInputEvents=[];for(const name of ['input','change','cancel'])input.addEventListener(name,event=>globalThis.duxcelFileInputEvents.push({type:event.type,trusted:event.isTrusted,files:input.files?.length??0}));return true;})()`);
 const state=await platformSnapshot();browserPicker(state,{pending:true,row:12,column:1,source});return{node:input.nodeId,file,state};
}
async function completeTableChooser(chooser,files){
 check(Array.isArray(files)&&files.length>0,'Actual chooser file delivery requires a selected file; cancellation is a separate platform-boundary scenario');
 await send('DOM.setFileInputFiles',{nodeId:chooser.node,files});
 for(const deadline=Date.now()+15000;Date.now()<deadline;){if(await evaluate('document.querySelector("#duxcel-file-input")===null')){await frames(4);const events=await evaluate('globalThis.duxcelFileInputEvents');check(events.some(event=>event.trusted&&['change','cancel'].includes(event.type)&&event.files===files.length),'Chooser completion was not delivered through a real trusted file input event');return events;}await pause(20);}
 throw new Error('Actual browser file input did not retire after '+(files.length?'selection':'empty/cancelled selection'));
}
async function cancelTableChooser(chooser){
 // Clearing an initially empty CDP file input emits no browser event. Exercise
 // the real host listener explicitly; this is synthetic boundary coverage,
 // separate from trusted file delivery and physical/OS dialog cancellation.
 check(chooser.node,'Missing actual chooser input for cancellation');
 await evaluate(`(()=>{const input=document.querySelector('#duxcel-file-input');if(!input)throw new Error('Actual chooser input is unavailable');return input.dispatchEvent(new Event('cancel'));})()`);
 for(const deadline=Date.now()+15000;Date.now()<deadline;){if(await evaluate('document.querySelector("#duxcel-file-input")===null')){await frames(4);const events=await evaluate('globalThis.duxcelFileInputEvents');check(events.some(event=>event.type==='cancel'&&!event.trusted&&event.files===0),'Synthetic chooser cancellation did not traverse the actual file input listener');return events;}await pause(20);}
 throw new Error('Actual chooser host did not retire its synthetic cancel event');
}
async function tableAnchor(row=12){await sourceHome();await key('ArrowRight','ArrowRight',39);for(let index=0;index<row;index++)await key('ArrowDown','ArrowDown',40);return platformSnapshot();}
function retainedTableSource(before,after,message,{schema=true}={}){const fields=['sheet','sheet_count','source_revision','page','spare','manual_cells'];if(schema)fields.push('schema');check(fields.every(name=>before[name]===after[name]),message);}
function importedTable(state){
 const object=state.object?.split(','),page=state.object_page?.split(',');
 check(state.selected_object==='1'&&state.object_count==='2'&&object?.length===6&&/^[1-9][0-9]*$/.test(object[0])&&object.slice(1).join(',')==='2,12,1,7,6','Import Table did not create its one independent same-sheet B13:G19 object');
 check(page?.join(',')==='0,5,6,5,0,0'&&state.pivot_counts==='0,0,0,0'&&/^[1-9][0-9]*$/.test(state.object_revision??''),'Imported Table did not publish its five-row/six-column source cache');return{object,page,revision:state.object_revision};
}
async function browserTableStatistics(foundation){
 const {published,baseline,sentinel}=foundation;await ribbonAnalysisAction('d');const source=await platformSnapshot();
 check(source.selected_object==='-1'&&source.object_count==='1'&&source.manual_cells===sentinel.manual_cells,'Table gate did not return to the retained primary source/matrix/sparse cells');retainedTableSource(baseline,source,'Existing matrix setup changed its primary source');
 const anchor=await tableAnchor(),jobs=starts().length;check(anchor.active==='12,1'&&anchor.extent==='12,1','Actual Table import destination is not B13');
 const cancelledChooser=await tableImportChooser(anchor),cancelEvents=await cancelTableChooser(cancelledChooser),cancelled=await platformSnapshot();browserPicker(cancelled);
 retainedTableSource(anchor,cancelled,'Cancelling Import Table changed its retained source/sparse data');check(cancelled.object_count==='1'&&cancelled.active===anchor.active&&cancelled.anchor===anchor.anchor&&cancelled.extent===anchor.extent&&starts().length===jobs,'Cancelling Import Table created an object, changed selection or submitted SQL');
 const staleChooser=await tableImportChooser(cancelled);await key('ArrowDown','ArrowDown',40);const moved=await platformSnapshot();check(moved.active==='13,1','Trusted navigation did not change the pending Table destination');
 const staleEvents=await completeTableChooser(staleChooser,[sourceFile]),rejected=await platformSnapshot();browserPicker(rejected);retainedTableSource(anchor,rejected,'Rejecting a stale Table destination changed source/sparse data');
 check(rejected.object_count==='1'&&rejected.active==='13,1'&&starts().length===jobs,'A stale chooser result created an object, replaced selection or submitted a Table query');
 await tableAnchor();const chooser=await tableImportChooser(rejected),chosenEvents=await completeTableChooser(chooser,[sourceFile]),reply=await waitPivot(foundation.reply.id);await frames(4);const table=await platformSnapshot(),model=importedTable(table);browserPicker(table);
 check(reply.rows===5&&reply.columns===6&&reply.total==='5'&&!reply.pending&&model.object[0]!==published.object.split(',')[0],'Actual Table import did not publish an independent five-row transaction relation');retainedTableSource(source,table,'Table import replaced the primary source/workbook/sparse data',{schema:false});check(Number(table.schema)===Number(source.schema)+1,'New Table publication did not advance the shared field schema generation exactly once');await capture('table-statistics-imported');
 // Table title B13 and header B14 precede its actual five data rows. This
 // navigation enters D15:D19 through the ordinary shared grid controller.
 await key('ArrowDown','ArrowDown',40);await key('ArrowDown','ArrowDown',40);await key('ArrowRight','ArrowRight',39);await key('ArrowRight','ArrowRight',39);const cell=await platformSnapshot();
 check(cell.active==='14,3'&&cell.extent==='14,3'&&cell.selected_object==='1','Trusted grid navigation did not enter the imported Table Amount cell D15');
 const beforeStatistics=starts().length,previous=replies().filter(value=>value.kind===10).at(-1)?.id;await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));
 try{for(let index=0;index<4;index++)await key('ArrowDown','ArrowDown',40,'',8);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));}await frames();
 const statisticsReply=await waitStatistics(previous??null),ready=await platformSnapshot(),cache=statisticsCache(ready,{rows:5,row:14,column:3,owner:1,values:[145.25,29.05,-4.5,100],reply:statisticsReply});
 importedTable(ready);retainedAnalysisCache(table,ready,'Imported Table statistics replaced its object/revision/page/source cache');const statisticsJobs=starts().slice(beforeStatistics);
 check(statisticsJobs.length===1&&statisticsJobs[0].kind===10&&statisticsJobs[0].id===statisticsReply.id,'Table cell selection submitted work beyond its one current statistics job');await capture('table-statistics-selection');
 const idleJobs=starts().length;await pause(300);await frames(3);const idle=await platformSnapshot();retainedStatistics(ready,idle,'Idle Table statistics lost its current result');retainedAnalysisCache(ready,idle,'Idle Table statistics changed its owned Table/source cache');check(starts().length===idleJobs,'Idle imported Table statistics resubmitted SQL');
 await sourceHome();for(let index=0;index<8;index++)await key('ArrowRight','ArrowRight',39);const matrix=await platformSnapshot();
 const matrixFields=['object','object_revision','object_page','pivot_counts','totals_count','totals_shape','object_grand0_null','object_grand0_hex','object_grand0_truncated'];
 check(matrix.selected_object==='0'&&matrix.object_count==='2'&&matrixFields.every(name=>matrix[name]===published[name]),'Independent Table import/statistics changed the existing Rows/Values matrix');retainedTableSource(published,matrix,'Table statistics changed the original matrix source/sparse data',{schema:false});check(matrix.schema===table.schema,'Revisiting the original matrix changed the shared field schema generation');await capture('table-statistics-retained-matrix');
 await ribbonAnalysisAction('d');await sourceHome();for(let index=0;index<7;index++)await key('ArrowRight','ArrowRight',39);for(let index=0;index<9;index++)await key('ArrowDown','ArrowDown',40);const manual=await platformSnapshot();
 check(manual.active==='9,7'&&manual.manual_cells==='1'&&manual.object_count==='2','Imported Table statistics did not retain the H10 sparse sentinel');retainedTableSource(source,manual,'Returning from Table statistics changed the primary source',{schema:false});check(manual.schema===table.schema,'Returning to sparse/source cells changed the shared schema generation');await capture('table-statistics-retained-manual');check(starts().length===idleJobs,'Revisiting cached matrix/source/manual cells submitted unexpected SQL');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Table statistics unexpectedly staged a report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Table statistics wrote into the review queue');
 const result={foundation,source,anchor,cancelledChooser,cancelEvents,cancellation_delivery:'synthetic untrusted DOM cancel at actual file input listener; physical/OS cancellation not accepted',cancelled,staleChooser,moved,staleEvents,rejected,chooser,chosenEvents,reply,table,cell,statisticsReply,ready,cache,idle,matrix,manual,
  scope:'Actual C File Import table/Alt F I, explicitly synthetic DOM cancellation through the actual chooser listener, trusted stale-destination navigation/rejection and CSV selection; independent same-sheet OBJECT_TABLE at B13 beside a retained primary source, Rows/Values matrix and H10 sparse value. Real D15:D19 selection publishes object-owned kind10 counts and 145.25/29.05/-4.5/100, with unchanged Table page/revision and no idle/presentation jobs. Database selection, workbook persistence, physical chooser UX and general rejected-file memory recovery remain separate; no saved reports or engine callbacks are used.'};
 await fs.writeFile(path.join(output,'table-statistics.json'),JSON.stringify(result,null,2));return result;
}
async function selectMixedWorksheet(){
 await sourceHome();await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16,{modifiers:2}));
 try{
  // The actual used extent is K19 (matrix footprint plus Table). Extend its
  // canonical Ctrl+Shift+End selection three columns into missing cells.
  await key('End','End',35,'',10);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17,{modifiers:8}));
  for(let index=0;index<3;index++)await key('ArrowRight','ArrowRight',39,'',8);
 }finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));}await frames();
}
async function selectSparseRange(column){
 await sourceHome();for(let index=0;index<9;index++)await key('ArrowDown','ArrowDown',40);for(let index=0;index<column;index++)await key('ArrowRight','ArrowRight',39);
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));try{await key('ArrowDown','ArrowDown',40,'',8);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));}await frames();
}
async function clearMixedSelection(source,jobs){
 await sourceHome();const state=await platformSnapshot();
 check(state.active==='0,0'&&state.anchor==='0,0'&&state.extent==='0,0'&&state.stats_state==='0'&&state.stats_request==='0'&&state.stats_object==='-1'&&state.stats_counts==='0,0,0,0'&&state.stats_key?.split(',').at(-1)==='0','Ordinary range deselection retained a stale mixed statistics owner/readout');
 check(['sum','average','minimum','maximum'].every(name=>state['stats_'+name+'_null']==='1'&&state['stats_'+name+'_hex']==='-'),'Ordinary range deselection retained old numeric text');
 retainedTableSource(source,state,'Deselecting mixed statistics changed a retained source/sparse owner');check(state.object_count==='2'&&starts().length===jobs,'Range deselection changed objects or submitted SQL');return state;
}
async function browserMixedStatistics(tableFoundation){
 await send('Emulation.setDeviceMetricsOverride',{width:1920,height:988,deviceScaleFactor:dpr,mobile:false});await frames(4);const source=await platformSnapshot();retainedTableSource(tableFoundation.manual,source,'Mixed statistics setup changed the retained primary source');
 // Independent inventory from transactions.csv and the declared object titles/
 // displayed headers. This is a fixed oracle, not an aggregate of cached pages:
 // body25 + six Table headers + Table/Pivot titles + SUM(Amount) caption +
 // new DOUBLE19.75 + VARCHAR 991 =36 typed global distinct values.
 const inventory={logical:19*14,primary_filled:29,table_filled:36,matrix_filled:11,manual_filled:1,numeric:24,distinct:36,
  source_headers:['ID','City','Amount','Date','Approved','Note'],table_title:'Table 2',matrix_title:'Pivot 1',matrix_headers:['City','SUM(Amount)'],sparse_text:'991',sum:465.75,average:19.40625,minimum:-4.5,maximum:100};
 const before=starts().length,previous=replies().filter(value=>value.kind===10).at(-1)?.id;await selectMixedWorksheet();
 const reply=await waitStatistics(previous??null),ready=await platformSnapshot(),cache=statisticsCache(ready,{rows:19,columns:14,row:0,column:0,owner:-2,counts:[266,77,24,36],values:[465.75,19.40625,-4.5,100],reply});
 retainedTableSource(source,ready,'Mixed publication changed its primary source/sparse owner');check(ready.selected_object==='-1'&&ready.object_count==='2','Mixed worksheet statistics lost its two analytical/source objects');
 let jobs=starts().length;const started=starts().slice(before);check(started.length===1&&started[0].kind===10&&started[0].id===reply.id,'Actual mixed range submitted work beyond its one current statistics job');await capture('mixed-statistics-selection');
 await pause(300);await frames(3);const idle=await platformSnapshot();retainedStatistics(ready,idle,'Idle mixed worksheet lost its current statistics');retainedTableSource(source,idle,'Idle mixed statistics replaced source/sparse data');check(starts().length===jobs,'Idle mixed statistics resubmitted SQL');
 const deselected=await clearMixedSelection(source,jobs),sparse=[];
 for(const specification of [{name:'manual-and-missing',column:7,counts:[2,1,0,1]},{name:'missing-only',column:11,counts:[2,0,0,0]}]){
  const beforeSparse=starts().length,prior=replies().filter(value=>value.kind===10).at(-1)?.id;await selectSparseRange(specification.column);
  const reply=await waitStatistics(prior??null),state=await platformSnapshot(),cache=statisticsCache(state,{rows:2,row:9,column:specification.column,owner:-2,counts:specification.counts,values:[null,null,null,null],flags:'0,0,0,0',reply});
  retainedTableSource(source,state,'Sparse/blank statistics replaced the primary source or manual cells');const newJobs=starts().slice(beforeSparse);check(state.object_count==='2'&&newJobs.length===1&&newJobs[0].kind===10&&newJobs[0].id===reply.id,'Sparse range selection changed objects or submitted excess work');await capture('mixed-statistics-'+specification.name);
  jobs=starts().length;await pause(300);await frames(3);const idle=await platformSnapshot();retainedStatistics(state,idle,'Idle sparse statistics lost NULL metrics or text ownership');check(starts().length===jobs,'Idle sparse statistics resubmitted SQL');
  const cleared=await clearMixedSelection(source,jobs);sparse.push({specification,reply,state,cache,idle,cleared});
 }
 // Revisit actual cached objects after all mixed/manual/blank jobs. Their
 // page/revision/footer ownership must survive without issuing another query.
 await tableAnchor();const table=await platformSnapshot();importedTable(table);retainedAnalysisCache(tableFoundation.table,table,'Mixed statistics replaced the imported Table cache/revision');await capture('mixed-statistics-retained-table');
 await sourceHome();for(let index=0;index<8;index++)await key('ArrowRight','ArrowRight',39);const matrix=await platformSnapshot();retainedAnalysisCache(tableFoundation.matrix,matrix,'Mixed statistics replaced the original matrix/source/footer cache');await capture('mixed-statistics-retained-matrix');
 await ribbonAnalysisAction('d');await sourceHome();const final=await platformSnapshot();retainedTableSource(source,final,'Returning from mixed statistics changed the primary source');check(final.object_count==='2'&&final.manual_cells==='1'&&final.stats_state==='0'&&starts().length===jobs,'Cached object/source presentation resubmitted SQL or lost sparse cells');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Mixed statistics unexpectedly staged a report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Mixed statistics wrote into the review queue');
 const result={tableFoundation,inventory,source,reply,ready,cache,idle,deselected,sparse,table,matrix,final,
  scope:'Actual shared C primary-source/Table/Rows-matrix/sparse setup, trusted Ctrl+Shift+End plus Shift extension to A1:N19, and real mixed owner -2 Worker publication: 266 logical/77 filled/24 numeric/36 globally typed distinct, 465.75/19.40625/-4.5/100. Independent fixture/title/header inventory includes VARCHAR 991 and excludes source-derived footer readouts. Actual H10:H11 manual/missing and L10:L11 missing-only selections retain SQL NULL numeric metrics rather than zero. Current readouts, ordinary deselection, source/object/page/revision/footer/manual ownership and bounded idle intervals are checked without additional queries or report saves. Engine limits, physical input and forced in-flight cancellation remain separate.'};
 await fs.writeFile(path.join(output,'mixed-statistics.json'),JSON.stringify(result,null,2));return result;
}
function retainedAnalysisCache(before,after,message){
 const fields=['sheet','source_revision','schema','page','manual_cells','selected_object','object_count','object','object_revision','object_page','pivot_counts','totals_count','totals_shape','object_grand0_null','object_grand0_hex','object_grand0_truncated'];
 for(let category=0;category<2;category++)for(const part of ['present','null','hex','truncated']){const name='object_category'+category+'_'+part;if(before[name]!==undefined)fields.push(name);}
 check(fields.every(name=>before[name]!==undefined&&after[name]===before[name]),message);
}
async function selectMatrixMeasure(state,rows){
 const model=analysisObject(state),row=Number(model.object[2])+2,column=Number(model.object[3])+model.counts[0];
 const rect=name=>{const value=state[name]?.split(',').map(Number);check(value?.length===4&&value.every(Number.isFinite)&&value[2]>0&&value[3]>0,'Missing actual matrix paint geometry '+name);return value;};
 const panel=rect('matrix_rect'),data=rect('matrix_data_rect'),cell=rect('matrix_first_measure_rect');
 check(state.matrix_data_row==='0','Statistics fixture requires the first visible actual result row');
 for(const outer of [panel,data])check(cell[0]>=outer[0]-.01&&cell[1]>=outer[1]-.01&&cell[0]+cell[2]<=outer[0]+outer[2]+.01&&cell[1]+cell[3]<=outer[1]+outer[3]+.01,'Actual numeric result cell escaped its painted matrix clip');
 const previous=replies().filter(value=>value.kind===10).at(-1)?.id,jobs=starts().length;
 await click(cell[0]+cell[2]/2,cell[1]+cell[3]/2);const entered=await platformSnapshot();
 check(entered.selected_object==='0'&&entered.active===row+','+column&&entered.anchor===row+','+column&&entered.extent===row+','+column,'Trusted pointer did not enter the actual first matrix measure cell');
 retainedAnalysisCache(state,entered,'Entering a matrix cell changed its analytical/source cache');
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));
 try{for(let index=1;index<rows;index++)await key('ArrowDown','ArrowDown',40,'',8);}
 finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));}await frames();
 const reply=await waitStatistics(previous??null),published=await platformSnapshot(),started=starts().slice(jobs);
 check(started.length===1&&started[0].kind===10&&started[0].id===reply.id,'Matrix selection resubmitted analytical/source work beyond its one statistics job');
 check(/^[1-9][0-9]*$/.test(published.object_revision??'')&&published.object_revision===state.object_revision,'Selection statistics changed or lost its analytical owner revision');
 retainedAnalysisCache(state,published,'Matrix selection statistics replaced its analytical/source cache');return{row,column,panel,data,cell,entered,reply,published,started};
}
async function browserAnalysisStatistics(foundation){
 const {published,baseline,sentinel}=foundation;check(/^[1-9][0-9]*$/.test(published.object_revision??''),'Actual selected matrix owner revision is missing');
 await capture('analysis-statistics-rows-matrix');const rows=await selectMatrixMeasure(published,4),rowsCache=statisticsCache(rows.published,{rows:4,row:rows.row,column:rows.column,owner:0,values:[145.25,36.3125,-4.5,100],reply:rows.reply});
 await capture('analysis-statistics-rows-selection');const rowsJobs=starts().length;await pause(300);await frames(3);const rowsIdle=await platformSnapshot();
 retainedStatistics(rows.published,rowsIdle,'Idle Rows matrix lost its current selection statistics');retainedAnalysisCache(rows.published,rowsIdle,'Idle Rows statistics replaced the matrix/source');check(starts().length===rowsJobs,'Idle Rows matrix statistics resubmitted a database job');
 // A real title click changes cell selection back to frame semantics before
 // authoring another query grain. It must clear the numeric readout, not query.
 const [x,y,width]=rowsIdle.matrix_rect.split(',').map(Number),scale=Number(rowsIdle.ui_scale);await click(x+Math.min(40*scale,width/4),y+15*scale);const frame=await platformSnapshot();
 check(frame.stats_state==='0'&&frame.stats_request==='0'&&frame.stats_cells==='0'&&frame.stats_sum_null==='1','Matrix title did not clear cell statistics into frame selection');
 retainedAnalysisCache(rowsIdle,frame,'Selecting the matrix frame replaced analytical/source data');check(starts().length===rowsJobs,'Matrix frame selection submitted a statistics/source job');
 const columns=await assignAnalysisField('Approved','columns'),reply=await waitPivot(foundation.reply.id);await frames(4);const columnMatrix=await platformSnapshot(),model=analysisObject(columnMatrix);
 check(reply.rows===4&&reply.columns===3&&reply.total==='4'&&!reply.pending&&model.counts.join(',')==='1,1,1,0'&&model.page.join(',')==='0,4,3,4,0,0','Actual Columns assignment did not publish the four-row/three-column result');
 check(model.object[0]===analysisObject(published).object[0]&&BigInt(columnMatrix.object_revision)>BigInt(published.object_revision),'Columns authoring lost the matrix identity or failed to advance its revision');
 check(columnMatrix.totals_count==='4,1'&&columnMatrix.totals_shape==='4,1,1,3,1'&&columnMatrix.object_grand0_null==='0'&&columnMatrix.object_grand0_truncated==='0'&&Number(Buffer.from(columnMatrix.object_grand0_hex??'','hex').toString('utf8'))===145.25,'Columns result lost its independently computed source footer totals');
 const categories=['false','true'].map((expected,index)=>{const prefix='object_category'+index+'_';check(columnMatrix[prefix+'present']==='1'&&columnMatrix[prefix+'null']==='0'&&columnMatrix[prefix+'truncated']==='0','Actual category measure header is missing or truncated');const value=Buffer.from(columnMatrix[prefix+'hex']??'','hex').toString('utf8');check(value===expected,'Columns result category order differs from the real typed domain');return value;});
 check(['sheet','source_revision','schema','page','manual_cells'].every(name=>columnMatrix[name]===published[name]),'Columns statistics setup changed the retained primary source/sparse cells');await capture('analysis-statistics-columns-matrix');
 const category=await selectMatrixMeasure(columnMatrix,4),categoryCache=statisticsCache(category.published,{rows:4,row:category.row,column:category.column,owner:0,counts:[4,2,2,2],values:[130,65,30,100],reply:category.reply});
 check(Number(categoryCache.metrics.sum)!==145.25,'Category result-cell statistics incorrectly reused the source grand total');await capture('analysis-statistics-category-selection');
 const idleJobs=starts().length,idleStart=Date.now();await pause(300);await frames(3);const final=await platformSnapshot();
 retainedStatistics(category.published,final,'Idle category matrix lost its current selection statistics');retainedAnalysisCache(category.published,final,'Idle category statistics replaced the matrix/source');check(starts().length===idleJobs,'Idle category statistics resubmitted database work');
 await ribbonAnalysisAction('d');const returned=await platformSnapshot();
 check(returned.selected_object==='-1'&&returned.object_count==='1'&&returned.manual_cells===sentinel.manual_cells&&['sheet','page','source_revision','schema','active','anchor','extent'].every(name=>returned[name]===baseline[name]),'Returning from matrix statistics lost the primary source/bookmark/sparse cells');await capture('analysis-statistics-returned-source');
 for(let index=0;index<7;index++)await key('ArrowRight','ArrowRight',39);for(let index=0;index<9;index++)await key('ArrowDown','ArrowDown',40);const manual=await platformSnapshot();
 check(manual.active==='9,7'&&manual.manual_cells==='1'&&manual.object_count==='1','Matrix statistics did not retain the H10 sparse sentinel');await capture('analysis-statistics-retained-manual');
 check(starts().length===idleJobs,'Returning to cached source/manual cells submitted an unexpected database job');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Analytical statistics unexpectedly staged a report');
 const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Analytical statistics gate wrote into the review queue');
 const result={foundation,rows,rowsCache,rowsIdle,frame,columns,reply,columnMatrix,categories,category,categoryCache,final,returned,manual,idle:{minimum_milliseconds:300,observed_milliseconds:Date.now()-idleStart,started_jobs:idleJobs},
  scope:'Actual same-sheet Rows City/SUM Amount and Columns Approved authoring through shared C menus; trusted pointer entry using actual paint-site result-cell geometry and Shift selection; official Worker kind10 statistics bound to the selected analytical object/revision. Four Rows aggregates produce 145.25/36.3125/-4.5/100; the false category has four cells/two present values and 130/65/30/100, separate from the 145.25 source footer. Frame/readout invalidation, unchanged cached matrix/source/sparse data and bounded idle intervals submit no extra jobs. Mixed/manual statistics, forced pending cancellation, physical input and report upload remain separate.'};
 await fs.writeFile(path.join(output,'analysis-statistics.json'),JSON.stringify(result,null,2));return result;
}
// Statistics are read from the same-frame C cache after the real Worker job.
// The driver never reads engine cells or computes substitute aggregates.
async function waitStatistics(afterId=null){
 for(const deadline=Date.now()+15000;Date.now()<deadline;){
  const reply=replies().filter(value=>value.kind===10&&(afterId===null||BigInt(value.id)>BigInt(afterId))).at(-1);
  if(reply){check(!reply.error,'Real browser selection statistics failed');await frames(4);return reply;}await pause(20);
 }
 throw new Error('Actual C selection did not publish statistics');
}
function statisticsCache(state,{rows,columns=1,values,reply,row=0,column=2,owner=-1,counts=[rows*columns,rows*columns,rows*columns,rows*columns],flags='1,0,0,0'}={}){
 check(state.stats_state==='3'&&state.stats_request==='0'&&state.stats_object===String(owner)&&state.stats_error==='0','Actual selection statistics are not ready/error-free with released request ownership');
 const key=state.stats_key?.split(',');check(key?.length===7&&key.every(value=>/^[0-9]+$/.test(value)),'Missing actual cached statistics selection/source key');
 check(key.join(',')===[String(row),String(row+rows-1),String(column),String(column+columns-1),state.source_revision,state.sheet,'1'].join(','),'Statistics cache belongs to a stale source/selection');
 check(state.stats_cells===String(rows*columns)&&state.stats_counts===counts.join(','),'Statistics did not retain selected-cell, filled, numeric and distinct counts');
 check(state.stats_flags===flags,'Amount statistics lost approximate/finite/overflow semantics');
 check(state.grid_selection==='0'&&state.active===row+','+column&&state.anchor===row+','+column&&state.extent===(row+rows-1)+','+(column+columns-1),'Statistics changed the actual selected Amount cells');
 if(reply)check(state.request===reply.id,'App did not adopt the current real statistics reply');
 const metrics={};for(const [index,name]of ['sum','average','minimum','maximum'].entries()){
  const prefix='stats_'+name+'_';
  if(values[index]===null){check(state[prefix+'null']==='1'&&state[prefix+'hex']==='-'&&state[prefix+'truncated']==='0','Missing numeric '+name+' did not remain a real SQL NULL');metrics[name]=null;continue;}
  check(state[prefix+'null']==='0'&&state[prefix+'truncated']==='0','Actual '+name+' statistics are missing or truncated');
  const hex=state[prefix+'hex'];check(hex&&/^(?:[0-9a-f]{2})+$/.test(hex),'Missing bounded exact C '+name+' text');
  const text=Buffer.from(hex,'hex').toString('utf8');check(Number(text)===values[index],'Real cached '+name+' differs from the source aggregate');metrics[name]=text;
 }
 return{key,metrics};
}
function retainedStatistics(before,after,message){
 const fields=Object.keys(before).filter(name=>name.startsWith('stats_')).concat(['sheet','request','source_revision','schema','page','manual_cells','active','anchor','extent','grid_selection']);
 check(fields.every(name=>after[name]===before[name]),message);
}
async function selectAmount(rows){
 await sourceHome();await key('ArrowRight','ArrowRight',39);await key('ArrowRight','ArrowRight',39);
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));
 try{for(let row=1;row<rows;row++)await key('ArrowDown','ArrowDown',40,'',8);}
 finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));}await frames();
}
async function browserStatistics(){
 const source=await zoomSource(),initial=await platformSnapshot();check(initial.manual_cells==='0'&&initial.object_count==='0','Statistics gate requires an unmodified primary source');
 await selectAmount(5);const reply=await waitStatistics(),ready=await platformSnapshot(),expected={rows:5,values:[145.25,29.05,-4.5,100],reply},cache=statisticsCache(ready,expected);
 check(ready.sheet===initial.sheet&&ready.page===initial.page&&ready.source_revision===initial.source_revision,'Source statistics altered imported source ownership');await capture('statistics-source-selection');
 const jobs=starts().length,viewport=[];
 for(const [dx,dy]of [[.25,.5],[-.25,-.5]]){await wheelBatch(dx,dy,4);const state=await platformSnapshot();statisticsCache(state,expected);retainedStatistics(ready,state,'Sheet pan invalidated retained statistics');viewport.push({input:{dx,dy,count:4},state});}
 check(viewport[0].state.horizontal!==ready.horizontal||viewport[0].state.fraction!==ready.fraction,'Statistics pan sample did not move the real C sheet');
 const [bx,by]=ready.body.split(',').map(Number),[lw,lh]=ready.logical.split(',').map(Number),wheel={ctrl:true,x:(bx-2)/lw,y:(by-2)/lh};
 for(const delta of [-150,150]){await wheelBatch(0,delta,1,wheel);const state=await platformSnapshot();statisticsCache(state,expected);retainedStatistics(ready,state,'Canvas zoom invalidated retained statistics');viewport.push({input:{delta,ctrl:true},state});}
 check(Number(viewport[2].state.zoom)>Number(ready.zoom)*1.2&&Math.abs(Number(viewport[3].state.zoom)-Number(ready.zoom))<.00001,'Statistics zoom did not enlarge and restore the actual cells');
 let statusFocus=false;for(let index=0;index<8;index++){await key('F6','F6',117);const state=await platformSnapshot();if(state.focus?.split('/')[0]==='10'){statusFocus=true;break;}}
 check(statusFocus,'Actual C focus traversal did not reach the statistics status control');await key('Enter','Enter',13);const dialog=await platformSnapshot();
 check(Number(dialog.modal)>0,'Statistics status action did not open its actual dialog');statisticsCache(dialog,expected);retainedStatistics(ready,dialog,'Opening the statistics dialog lost its cached values');await capture('statistics-dialog');
 await key('Escape','Escape',27);await key('Escape','Escape',27);await pause(300);await frames(3);const retained=await platformSnapshot();
 statisticsCache(retained,expected);retainedStatistics(ready,retained,'Idle statistics dialog/pan/zoom changed the source or aggregate cache');check(starts().length===jobs,'Pan/zoom, statistics dialog or idle rendering resubmitted database work');
 // Collapse to one real source cell: its old range readout must disappear.
 // This proves selection invalidation, not an artificially delayed Worker.
 await sourceHome();await key('ArrowRight','ArrowRight',39);const single=await platformSnapshot();
 check(single.active==='0,1'&&single.extent==='0,1'&&single.stats_state==='0'&&single.stats_request==='0'&&single.stats_key?.split(',').at(-1)==='0','Single-cell selection retained a stale active range statistic');
 check(single.stats_sum_null==='1'&&single.stats_average_null==='1'&&single.stats_minimum_null==='1'&&single.stats_maximum_null==='1','Collapsed source selection retained old aggregate text');
 const [x,y]=single.body.split(',').map(Number),[width,height]=single.grid_cell.split(',').map(Number);
 await rightClick(x+width*1.5,y+height/2);await authorActivate('source_context','filter');const cities=await waitDistinct();
 check(cities.distinct_count===4&&cities.distinct_total==='4','Statistics source filter did not enumerate the actual City domain');
 let filter=await platformSnapshot();check(filter.filter_field==='1'&&filter.filter_focus==='3','Actual City filter did not own its search focus');
 await key('Tab','Tab',9);await key(' ','Space',32,' ');await key('Tab','Tab',9);await key('Home','Home',36);await key('ArrowDown','ArrowDown',40);await key('ArrowDown','ArrowDown',40);await key(' ','Space',32,' ');
 filter=await platformSnapshot();check(filter.filter_focus==='5','Actual City checklist did not retain value-list focus');await capture('statistics-city-filter');
 // Enter applies the actual checklist draft from its focused value list.
 const beforePage=replies().filter(value=>value.kind===0).at(-1).id;await key('Enter','Enter',13);const filtered=await waitPage(beforePage);await frames(4);const filteredSource=await platformSnapshot();
 check(filtered.total==='2'&&filtered.columns===6&&filteredSource.modal==='0'&&Number(filteredSource.source_revision)>Number(ready.source_revision),'Actual City selection did not publish a new two-row source revision');
 check(filteredSource.stats_state==='0'&&filteredSource.stats_request==='0'&&filteredSource.stats_sum_null==='1','Filtered single-cell source republished stale range statistics');
 await selectAmount(2);const filteredReply=await waitStatistics(reply.id),published=await platformSnapshot(),filteredCache=statisticsCache(published,{rows:2,values:[19.75,9.875,7.25,12.5],reply:filteredReply});
 check(published.source_revision===filteredSource.source_revision&&published.manual_cells==='0'&&published.object_count==='0'&&published.sheet===ready.sheet,'Filtered statistics changed the current source or fabricated worksheet cells/objects');await capture('statistics-filtered-selection');
 const idleJobs=starts().length,idleStart=Date.now();await pause(300);await frames(3);const final=await platformSnapshot();retainedStatistics(published,final,'Idle filtered source lost its current statistics');
 check(starts().length===idleJobs,'Idle filtered statistics resubmitted database work');
 const statisticsReplies=replies().filter(value=>value.kind===10);check(statisticsReplies.length>=2&&statisticsReplies.every(value=>!value.error),'A real source statistics reply retained an unsupported/error status');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Statistics capture unexpectedly staged a report');
 const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Statistics-only gate wrote into the persistent review queue');
 const result={source,initial,reply,ready,cache,viewport,dialog,retained,single,cities,filter,filtered,filteredSource,filteredReply,published,filteredCache,final,statisticsReplies,
  idle:{minimum_milliseconds:300,observed_milliseconds:Date.now()-idleStart,started_jobs:idleJobs},
  scope:'Actual trusted primary-source Amount C1:C5 selection, official Worker kind10 publication and owned C counts/Sum/Average/Minimum/Maximum; current result retained through real sheet pan/zoom, status dialog and idle rendering without resubmission. Single-cell invalidation and actual City checklist filtering create a fresh two-row source revision/readout. Mixed/manual/object statistics, forced in-flight cancellation, physical input and report upload remain separate.'};
 await fs.writeFile(path.join(output,'statistics.json'),JSON.stringify(result,null,2));return result;
}
function retainedValueReview(pick,prefix){
 const fields=Object.keys(pick.before).filter(name=>name.startsWith(prefix));
 check(fields.length>0&&fields.every(name=>pick.after[name]===pick.before[name]),'Review cancellation changed owned '+prefix+' state/value/origin');
}
async function browserFocus(){
 const source=await zoomSource(),initial=await platformSnapshot(),jobs=starts().length;check(source.rows===5&&source.columns===6&&source.total==='5'&&initial.object_count==='0'&&initial.manual_cells==='0','Focus regression requires the actual unmodified transactions source');
 await focusTransformations();const pane=await platformSnapshot();check(pane.focus?.split('/')[0]==='8'&&pane.modal==='0','Real Transformations traversal did not retain its keyboard focus');cellModelRetained(initial,pane,'Focusing Transformations changed source/model ownership');
 // This is the existing tested chrome.help anchor in logical C units. Its
 // transform is handled by canvasPoint, while all Guide targets below use
 // their actual captured paint rectangles. No Fields geometry is guessed.
 const help={x:Number(pane.logical.split(',')[0])-18*Number(pane.ui_scale),y:42*Number(pane.ui_scale)};await click(help.x,help.y);const first=guideState(await platformSnapshot());cellModelRetained(pane,first.state,'Pointer Help from pane focus changed source/model ownership');check(starts().length===jobs,'Pane-focus Help activation submitted database work');await capture('focus-pane-help');
 const [x,y,w,h]=first.bounds.close;await click(x+w/2,y+h/2);const closed=await platformSnapshot();check(closed.modal==='0','Actual measured Guide Close did not dismiss Help');cellModelRetained(pane,closed,'Guide Close changed source/model ownership');
 await click(40*Number(closed.ui_scale),14*Number(closed.ui_scale));const file=await platformSnapshot();check(file.file_menu==='1'&&file.modal==='0'&&file.file_visible==='8','Actual brand control did not open the eight-command File menu');const [fx,fy,fw,fh]=file.file_panel.split(',').map(Number);check(!(help.x>=fx&&help.x<fx+fw&&help.y>=fy&&help.y<fy+fh),'Help regression point is not outside the actual File menu');
 await click(help.x,help.y);const dismissed=await platformSnapshot();check(dismissed.file_menu==='0'&&dismissed.modal==='0'&&dismissed.author_kind!=='guide','First outside File click activated Help instead of only dismissing File');cellModelRetained(pane,dismissed,'First File outside click changed the source/model');check(starts().length===jobs,'File outside dismissal submitted database work');await capture('focus-file-dismissed');
 await click(help.x,help.y);const second=guideState(await platformSnapshot());cellModelRetained(pane,second.state,'Second Help activation lost source/model ownership');check(second.scroll===0&&second.focus===0&&second.state.file_menu==='0','Second actual Help click did not open a fresh Guide after File dismissal');await capture('focus-file-second-help');await key('Escape','Escape',27);const escaped=await platformSnapshot();check(escaped.modal==='0'&&escaped.file_menu==='0','Normal Guide Escape left a modal or File menu open');cellModelRetained(pane,escaped,'Normal Guide Escape changed source/model ownership');
 await pause(250);await frames(3);const final=await platformSnapshot();cellModelRetained(pane,final,'Idle focused source changed ownership');check(final.modal==='0'&&final.request===pane.request&&starts().length===jobs,'Focus/Help/File/idle flow submitted additional SQL');check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Focus regression staged a saved report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Focus regression wrote into the persistent queue');
 const result={source,initial,pane,help,first:first.state,closed,file,dismissed,second:second.state,escaped,final,scope:'Actual transactions source and real Transformations focus8 followed by pointer Help, measured Guide Close, first File outside-click dismissal and second Help activation. Normal Escape and bounded idle preserve source/manual/object/page/request ownership with zero additional jobs and no saved reports. Uses the existing tested Help/brand chrome anchors and actual Guide/File paint bounds; no guessed Fields coordinates or physical-input claim.'};await fs.writeFile(path.join(output,'focus.json'),JSON.stringify(result,null,2));return result;
}
const feedbackIds={pipeline_inspect:'pane.transformations.inspect',pipeline_sql:'pane.transformations.sql',pipeline_copy:'pane.transformations.copy',pipeline_back:'pane.transformations.back',pipeline_preview:'pane.transformations.preview',preview_status:'preview.status',preview_return:'preview.return',filter_status:'filter.values.status',filter_thumb:'filter.values.scrollbar.thumb',source_count:'status.source_count',readout_value:'cell.readout.value',toast:'toast.content'};
function feedbackTarget(state,part){
 const bounds=state['feedback_'+part+'_rect']?.split(',').map(Number),[width,height]=state.logical.split(',').map(Number),id=part==='pipeline_step'?state.feedback_pipeline_step_id:feedbackIds[part];
 check(id&&bounds?.length===4&&bounds.every(Number.isFinite)&&bounds[2]>0&&bounds[3]>0&&bounds[0]>=0&&bounds[1]>=0&&bounds[0]+bounds[2]<=width+.01&&bounds[1]+bounds[3]<=height+.01,'Feedback target has no authoritative visible geometry: '+part);
 if(part==='pipeline_step')check(id==='pane.transformations.step.source.f'+fieldHash('')+'.n0','First visible primary source step lost its full logical field/index identity');return{part,id,bounds};
}
function feedbackText(state,part){const hex=state['feedback_'+part+'_hex'],truncated=state['feedback_'+part+'_truncated'];check((hex==='-'||/^(?:[0-9a-f]{2}){1,64}$/.test(hex??''))&&['0','1'].includes(truncated),'Feedback omitted bounded status text or explicit truncation');const text=hex==='-'?'':Buffer.from(hex,'hex').toString('utf8');check(!text.includes('\ufffd'),'Feedback prefix split UTF-8');return{text,truncated};}
function feedbackRetained(before,after,part){
 const fields=[...authorOwnedFields,'focus','ribbon','selected_object','object_count','busy','io_busy',...Object.keys(before).filter(name=>name.startsWith('clipboard_')||name.startsWith('stats_')||name.startsWith('feedback_sql_')||name.startsWith('feedback_preview')||name.startsWith('feedback_filter')||name.startsWith('feedback_stats_')||name.startsWith('feedback_'+part+'_'))];
 if(part==='toast')fields.push('feedback_toast');check(fields.every(name=>before[name]===after[name]),'Feedback capture/composer cancellation changed '+part+' producer cache, input or source/model/request ownership');
}
async function feedbackPick(part,name,{exposed=false}={}){
 const jobs=starts().length,captured=await beginReview(),before=capturedState(),target=feedbackTarget(before,part),surface=await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height};})()');[surface.logicalWidth,surface.logicalHeight]=before.logical.split(',').map(Number);const reachable=reachableGuidePoint(target.bounds,surface),top=reachable.logical.y>surface.logicalHeight/2;if(top!==authorPickerTop){await key(' ','Space',32,' ');authorPickerTop=top;}if(exposed)await capture(name+'-picker');
 const index=consoleText('Review picked: ').length;await send('Input.dispatchMouseEvent',{type:'mouseMoved',...reachable.css});await send('Input.dispatchMouseEvent',{type:'mousePressed',...reachable.css,button:'left',clickCount:1});await frames();await send('Input.dispatchMouseEvent',{type:'mouseReleased',...reachable.css,button:'left',clickCount:1});await frames();const picked=await waitConsole('Review picked: ',index);check(picked==='Review picked: '+target.id,'Frozen feedback picker selected covered content instead of '+target.id+': '+picked);await key('Escape','Escape',27);const after=await platformSnapshot();feedbackRetained(before,after,part);check(starts().length===jobs,'Feedback review activated a control or submitted database work');return{captured,target,reachable,picked,before,after};
}
async function feedbackClick(part){const state=await platformSnapshot(),target=feedbackTarget(state,part),[x,y,w,h]=target.bounds;await click(x+w/2,y+h/2);return{state,target};}
async function feedbackReply(kind,previous=null,{error=false}={}){
 for(const deadline=Date.now()+15000;Date.now()<deadline;){const reply=replies().filter(reply=>reply.kind===kind&&(previous===null||BigInt(reply.id)>BigInt(previous))).at(-1);if(reply){check(Boolean(reply.error)===error,'Real feedback job completed with an unexpected error state');return reply;}await pause(20);}throw new Error('Actual feedback job did not complete kind'+kind);
}
function feedbackSourceRetained(before,after,message){check(['sheet','manual_cells','sheet_count','dirty','source_revision','schema','page','spare','active','anchor','extent','grid_selection','selected_object','object_count'].every(name=>before[name]===after[name]),message);}
function feedbackFilter(state,{failed=false}={}){
 const parts=state.feedback_filter?.split(','),field=failed?'1':'0';check(parts?.length===5&&parts[0]===state.search_revision&&parts[1]==='-1'&&parts[2]===(failed?'-1':'0')&&parts[3]==='0'&&parts[4]===String(Number(failed))&&state.filter_field===field&&state.busy==='0'&&state.io_busy==='0','Filter feedback belongs to a stale/pending domain or the wrong real field');
 check(state.feedback_filter_truncated==='0,0','Small real filter keys were unexpectedly truncated');const context=Buffer.from(state.feedback_filter_context_hex??'','hex').toString('utf8');check(context.startsWith(state.source_revision+':'+field+':-1:'),'Filter feedback lost its current primary source revision');if(failed)check(state.feedback_filter_pending_hex===state.feedback_filter_context_hex,'Failed distinct feedback discarded or replaced its actual attempted key');else check(context.includes('zzzznomatch')&&state.feedback_filter_pending_hex==='-','No-match feedback retained a pending or stale search key');
 const value=feedbackText(state,'filter_status');check(failed?value.text.startsWith('Conversion Error:')&&value.text.includes('Could not convert'):value.text==='No matching values'&&value.truncated==='0','Filter feedback is not the actual no-match/conversion-error message');feedbackTarget(state,'filter_status');return{parts,context,...value};
}
async function feedbackOpenFilter(column){
 let state=await platformSnapshot();const [x,y,w,h]=state.body.split(',').map(Number),[cw,rh]=state.grid_cell.split(',').map(Number);await click(x+Math.min(w,cw)/2,y+Math.min(h,rh)/2);await sourceHome();state=await platformSnapshot();const [bx,by]=state.body.split(',').map(Number),[width,height]=state.grid_cell.split(',').map(Number);await rightClick(bx+width*(column+.5),by+height/2);await authorActivate('source_context','filter');return state;
}
function feedbackInspectorWidth(state,{sql=false,scale=1}={}){
 const logical=state.logical?.split(',').map(Number),grid=state.grid_rect?.split(',').map(Number);check(logical?.length===2&&grid?.length===4&&[...logical,...grid].every(Number.isFinite)&&Number(state.ui_scale)===scale,'Inspector width check lost its real logical viewport/grid or independent interface scale');
 // The actual grid right edge is the reserved inspector boundary. These are
 // frozen paint/layout coordinates, not a second implementation of its layout.
 const reserved=logical[0]-grid[0]-grid[2],expected=(sql?340:280)*scale;check(Math.abs(reserved-expected)<.02,'Actual '+(sql?'SQL':'Steps')+' inspector width differs from the frozen design: '+reserved+' vs '+expected);
 let code=null;if(sql){code=feedbackTarget(state,'pipeline_sql').bounds;check(Math.abs(code[2]-304*scale)<.02&&code[0]>=logical[0]-reserved-.02&&code[0]+code[2]<=logical[0]+.02,'Actual painted SQL code width/clipping differs from304 design units at the current interface scale');}else{check(state.feedback_pipeline_sql_rect===undefined,'Steps still exposes the SQL code surface');feedbackTarget(state,'pipeline_inspect');}
 return{scale,reserved,design_width:reserved/scale,code,design_code_width:code?code[2]/scale:null};
}
async function feedbackInspectorScaleCycle(steps,sql){
 const jobs=starts().length,clipboard=['clipboard_pending','clipboard_completed','clipboard_status'];
 async function scale(large){await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('7','Digit7',55);await key(large?'End':'Home',large?'End':'Home',large?35:36);if(!large)await key('ArrowDown','ArrowDown',40);await key('Enter','Enter',13);await pause(220);await frames(3);}
 await send('Emulation.setDeviceMetricsOverride',{width:1920,height:1120,deviceScaleFactor:dpr,mobile:false});await frames(4);await scale(true);const largeSteps=await platformSnapshot(),stepsSize=feedbackInspectorWidth(largeSteps,{scale:1.5});feedbackSourceRetained(steps,largeSteps,'Actual150 percent scale changed Steps/source/model/page ownership');check(largeSteps.zoom===steps.zoom&&largeSteps.feedback_pipeline_step_id===steps.feedback_pipeline_step_id,'Interface scaling changed canvas zoom or the current visible source step');
 await feedbackClick('pipeline_inspect');await pause(220);await frames(3);const largeSQL=await platformSnapshot(),sqlSize=feedbackInspectorWidth(largeSQL,{sql:true,scale:1.5}),layout=largeSQL.feedback_sql_layout?.split(',');feedbackSourceRetained(steps,largeSQL,'Actual150 percent SQL inspection changed source/model/pages');check(largeSQL.feedback_sql_cache===sql.feedback_sql_cache&&layout?.length===3&&layout[0]===sql.feedback_sql_layout.split(',')[0]&&Number(layout[1])>0&&layout[2]==='0','Scaled SQL inspection lost its actual cache identity, scroll or bounded wrapped lines');await capture('feedback-inline-sql-width-150');
 await feedbackClick('pipeline_back');await pause(220);await frames(3);const largeBack=await platformSnapshot(),backSize=feedbackInspectorWidth(largeBack,{scale:1.5});feedbackSourceRetained(steps,largeBack,'Back from150 percent SQL changed Steps/source/model/pages');check(largeBack.feedback_pipeline_step_id===steps.feedback_pipeline_step_id,'Back from scaled SQL lost the selected visible source step');
 await scale(false);await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:dpr,mobile:false});await frames(4);const restored=await platformSnapshot(),restoredSize=feedbackInspectorWidth(restored);feedbackSourceRetained(steps,restored,'Restoring Standard scale lost Steps/source/model/pages');check(restored.zoom===steps.zoom&&restored.feedback_pipeline_step_id===steps.feedback_pipeline_step_id&&[largeSteps,largeSQL,largeBack,restored].every(state=>clipboard.every(name=>state[name]!==undefined&&state[name]===steps[name]))&&starts().length===jobs,'Measured inspector scale/Back cycle activated clipboard, changed source step/zoom or submitted database work');return{largeSteps,stepsSize,largeSQL,sqlSize,largeBack,backSize,restored,restoredSize};
}
async function browserFeedbackReview(){
 const source=await zoomSource(),initial=await platformSnapshot();check(initial.object_count==='0'&&initial.manual_cells==='0'&&initial.clipboard_pending==='0'&&initial.clipboard_completed==='0','Feedback gate requires an unmodified real source and untouched clipboard');await feedbackOpenFilter(1);const cities=await waitDistinct();check(cities.distinct_count===4&&cities.distinct_total==='4','Feedback setup did not enumerate the real City domain');await key('Tab','Tab',9);await key(' ','Space',32,' ');await key('Tab','Tab',9);await key('Home','Home',36);await key('ArrowDown','ArrowDown',40);await key('ArrowDown','ArrowDown',40);await key(' ','Space',32,' ');await key('Enter','Enter',13);const filtered=await waitPage(source.id);check(filtered.rows===2&&filtered.columns===6&&filtered.total==='2','Actual City selection did not publish the two Istanbul rows');await frames(4);await focusTransformations();const steps=await platformSnapshot(),stepsSize=feedbackInspectorWidth(steps),picks=[];picks.push(await feedbackPick('pipeline_step','feedback-source-step',{exposed:true}));
 await feedbackClick('pipeline_inspect');await pause(220);await frames(3);const sql=await platformSnapshot(),sqlSize=feedbackInspectorWidth(sql,{sql:true}),cache=sql.feedback_sql_cache?.split(','),layout=sql.feedback_sql_layout?.split(',');check(cache?.length===5&&cache[0]==='0'&&cache[1]===sql.source_revision&&cache[2]===sql.sheet&&cache[3]===sql.source_revision&&/^\d+$/.test(cache[4])&&layout?.length===3&&Number(layout[0])>=0&&Number(layout[1])>0&&layout[2]==='0','Inline source SQL lost its actual owned primary cache/line layout');for(const part of ['pipeline_sql','pipeline_copy','pipeline_back'])picks.push(await feedbackPick(part,'feedback-inline-'+part,{exposed:part==='pipeline_sql'}));await feedbackClick('pipeline_back');await pause(220);await frames(3);const back=await platformSnapshot(),backSize=feedbackInspectorWidth(back);feedbackSourceRetained(steps,back,'Back from SQL changed the retained Steps/source/model/pages');const widthCycle=await feedbackInspectorScaleCycle(back,sql),beforePreview=widthCycle.restored,previousPreview=replies().filter(reply=>reply.kind===12).at(-1)?.id;await feedbackClick('pipeline_preview');const attemptedPreview=await platformSnapshot(),attempt=attemptedPreview.feedback_preview?.split(',');check(attempt?.length===6&&attempt[0]==='0'&&attempt[1]===beforePreview.source_revision&&attempt[3]==='0'&&attempt[5]==='0'&&['0','1'].includes(attempt[4]),'Historical preview did not pin the actual primary source step/revision');const pendingObserved=attempt[4]==='1';if(pendingObserved){check(attempt[2]!=='0'&&attemptedPreview.busy==='1'&&feedbackText(attemptedPreview,'preview_status').text==='Loading step preview…','Genuine pending preview lost its request/loading status');feedbackTarget(attemptedPreview,'preview_status');}
 const previewReply=await feedbackReply(12,previousPreview??null);check(previewReply.rows===5&&previewReply.columns===6&&previewReply.total==='5','Real Source-step preview did not publish the historical five-row CSV');await frames(4);const preview=await platformSnapshot();check(preview.feedback_preview===['0',beforePreview.source_revision,'0','0','0','0'].join(',')&&preview.feedback_preview_records?.endsWith(',0')&&preview.feedback_preview_status_rect===undefined,'Ready historical preview retained pending/error ownership or fabricated a loading label');feedbackSourceRetained(beforePreview,preview,'Historical preview replaced the live filtered source/model/pages');for(const part of ['preview_return','source_count','readout_value'])picks.push(await feedbackPick(part,'feedback-preview-'+part,{exposed:part==='preview_return'}));await feedbackClick('preview_return');const restored=await platformSnapshot();feedbackSourceRetained(beforePreview,restored,'Normal preview Return changed the retained live source/model/pages');check(restored.feedback_preview===undefined,'Normal Return did not close the temporary historical view');picks.push(await feedbackPick('toast','feedback-restored-toast',{exposed:true}));
 await focusTransformations(false);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));try{await key('F10','F10',121,'',8);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));}await authorActivate('step','edit');const text='SELECT i*5+ID AS ID,COALESCE(CAST(CAST(CASE WHEN i*5+ID>4096 THEN City ELSE NULL END AS INTEGER) AS VARCHAR),City) AS City,Amount,Date,Approved,Note FROM input CROSS JOIN range(1200) AS t(i)',draft=await sourceQueryDraft(text),previousValidation=replies().filter(reply=>reply.kind===11).at(-1)?.id,previousPage=replies().filter(reply=>reply.kind===0).at(-1).id;await authorActivate('source_query','apply');const validation=await feedbackReply(11,previousValidation??null),expanded=await waitPage(previousPage);check(expanded.rows===256&&expanded.columns===6&&expanded.total==='6000','Real late-error fixture must publish its safe first256 rows and complete6000-row COUNT before any distinct failure');await frames(4);const expandedSource=await platformSnapshot();check(expandedSource.object_count==='0'&&expandedSource.manual_cells==='0'&&BigInt(expandedSource.source_revision)>BigInt(restored.source_revision),'Real query Apply did not retain the source-only fixture');
 const oldDistinct=replies().filter(reply=>reply.kind===4).at(-1)?.id;await feedbackOpenFilter(0);const ids=await waitDistinct(oldDistinct??null),domain=await platformSnapshot();check(ids.distinct_total==='6000'&&ids.distinct_count>0&&domain.filter_field==='0'&&domain.filter_focus==='3','Real ID domain did not enumerate6000 actual values');feedbackTarget(domain,'filter_thumb');picks.push(await feedbackPick('filter_thumb','feedback-distinct-thumb',{exposed:true}));for(const character of 'zzzznomatch')await key(character,'Key'+character.toUpperCase(),character.toUpperCase().charCodeAt(0),character);const noMatchReply=await waitDistinct(ids.id);check(noMatchReply.distinct_count===0&&noMatchReply.distinct_total==='0','Real distinct search did not publish an empty domain');await frames(4);const noMatch=await platformSnapshot(),noMatchFeedback=feedbackFilter(noMatch);picks.push(await feedbackPick('filter_status','feedback-no-match',{exposed:true}));const noMatchJobs=starts().length;await key('Escape','Escape',27);const closedSearch=await platformSnapshot();feedbackSourceRetained(domain,closedSearch,'Cancelling real no-match search changed the primary source/model/pages');check(closedSearch.modal==='0'&&starts().length===noMatchJobs,'No-match Cancel submitted new work');
 const beforeFailure=replies().filter(reply=>reply.kind===4).at(-1).id;await feedbackOpenFilter(1);const failedReply=await feedbackReply(4,beforeFailure,{error:true});await frames(4);const failed=await platformSnapshot(),failedFeedback=feedbackFilter(failed,{failed:true});check(failed.request===failedReply.id&&failed.modal!=='0','Actual full City distinct failure did not remain in its owning dropdown');picks.push(await feedbackPick('filter_status','feedback-genuine-error',{exposed:true}));const failedJobs=starts().length;await key('Escape','Escape',27);const closedFailure=await platformSnapshot();feedbackSourceRetained(failed,closedFailure,'Closing genuine distinct error replaced the successful primary page/model');check(closedFailure.modal==='0'&&starts().length===failedJobs,'Error dropdown Cancel submitted new work');
 async function undoSource(total){const prior=replies().filter(reply=>reply.kind===0).at(-1).id;await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));try{await key('z','KeyZ',90,'',2);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));}const page=await waitPage(prior);check(page.rows===Number(total)&&page.columns===6&&page.total===total,'Normal source Undo did not recover the actual prior CSV result');return page;}
 const recoveredQuery=await undoSource('2'),recoveredFilter=await undoSource('5');await frames(4);const recovered=await platformSnapshot();check(recovered.modal==='0'&&recovered.object_count==='0'&&recovered.manual_cells==='0'&&recovered.clipboard_pending==='0'&&recovered.clipboard_completed==='0'&&recovered.sheet===initial.sheet,'Normal recovery lost source/workbook ownership or activated clipboard');const idleJobs=starts().length;await pause(300);await frames(3);const idle=await platformSnapshot();feedbackSourceRetained(recovered,idle,'Idle recovered source changed its pages or model');check(starts().length===idleJobs&&picks.length===11,'Feedback review resubmitted idle SQL or changed its bounded eleven-target inventory');check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Feedback review staged a saved report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Feedback review wrote into the persistent queue');
 const result={source,initial,cities,filtered,steps,stepsSize,sql,sqlSize,cache,layout,back,backSize,widthCycle,beforePreview,attemptedPreview,pendingObserved,previewReply,preview,restored,text,draft,validation,expanded,expandedSource,ids,domain,noMatchReply,noMatch,noMatchFeedback,closedSearch,failedReply,failed,failedFeedback,closedFailure,recoveredQuery,recoveredFilter,recovered,idle,picks,scope:'Actual primary Steps280→SQL340/code304→Back280 width at100 percent, repeated at150 percent with actual reserved widths420→510/code456→420, and Standard restoration. Real grid/paint geometry, source/cache/scroll/clipboard ownership and zero width-cycle jobs are checked without a second layout implementation. Existing Steps/owned inline SQL/Copy/Back inspection, historical kind12 preview of five rows beside the retained two-row live filter, normal Return and preview footer/readout/toast picking. Pending is reported only if genuinely captured; native held-reply coverage remains separate. A real ordinal-dependent nested CASE query first publishes256 rows and COUNT6000; actual ID distinct6000 exposes its thumb and genuine zero-match search, then full City distinct fails through the official Worker. Eleven frozen picks/cancel retain observed producer caches/input/source/model/request ownership; normal Undo restores the original five-row CSV and idle submits no SQL. No Copy activation, OS clipboard, fake callback, saved report or physical-input claim.'};await fs.writeFile(path.join(output,'feedback-review.json'),JSON.stringify(result,null,2));return result;
}
function errorReviewState(menu,failed){
 const state=menu.state,bounds=state.error_content_rect?.split(',').map(Number),[x,y,w,h]=menu.panel,bytes=Number(state.error_bytes),lines=Number(state.error_lines),since=Number(state.error_message_since),hex=state.error_text_hex;
 check(menu.rows.map(row=>row.key).join(',')==='x_close,copy_error,relink,close'&&menu.rows.every(row=>row.enabled===(row.key!=='relink')&&!row.checked&&!row.focused),'Real Error actions lost Close/Copy or honest disabled browser Relink');
 check(state.error_kind==='document-error'&&state.error_origin==='0,0,0,0,0,0,0,0','Error targets falsely inherited a selected worksheet/source owner');
 check(state.busy==='0'&&state.io_busy==='0'&&state.error_busy==='0'&&state.error_io_busy==='0'&&state.error_request===state.request&&state.error_request===String(failed.id)&&state.error_pending_kind==='0','Error capture did not describe the actual idle failed page request');
 check(Number.isInteger(bytes)&&bytes>64&&bytes<1024&&Number.isInteger(lines)&&lines>0&&Number.isFinite(since)&&since>0&&['0','1'].includes(state.error_clipped)&&state.error_text_truncated==='1'&&state.error_value_truncated==='0'&&/^(?:[0-9a-f]{2}){1,64}$/.test(hex??''),'Error message lost bounded content, actual painted lines/clipping or timestamp');
 const prefix=Buffer.from(hex,'hex').toString('utf8');check(prefix.startsWith('Query failed: Conversion Error:')&&!prefix.includes('\ufffd'),'Error content is not the real source conversion failure or splits UTF-8');
 check(bounds?.length===4&&bounds.every(Number.isFinite)&&bounds[2]>0&&bounds[3]>0&&bounds[0]>=x&&bounds[1]>=y&&bounds[0]+bounds[2]<=x+w+.01&&bounds[1]+bounds[3]<=y+h+.01&&lines<=Math.ceil(bounds[3]/(22*Number(state.ui_scale)))+1,'Error painted content escaped its real panel/line budget');return{bounds,bytes,lines,since,prefix,clipped:state.error_clipped==='1'};
}
async function browserReviewError(){
 const source=await zoomSource(),initial=await platformSnapshot(),text='SELECT ID,CAST(City AS INTEGER) AS City,Amount,Date,Approved,Note FROM input',cases=[];let previous=source;
 for(const narrow of [false,true]){
  if(narrow){await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('7','Digit7',55);await key('Home','Home',36);for(let i=0;i<3;i++)await key('ArrowDown','ArrowDown',40);await key('Enter','Enter',13);await send('Emulation.setDeviceMetricsOverride',{width:760,height:640,deviceScaleFactor:dpr,mobile:false});await frames(4);}
  const before=await platformSnapshot();check(before.modal==='0'&&before.object_count==='0'&&before.manual_cells==='0'&&before.ui_scale===(narrow?'1.500000':'1.000000'),'Error fixture source/scale changed before actual query editing');await focusTransformations(!narrow);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));await key('F10','F10',121,'',8);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));await authorActivate('step','edit');
  const editor=authorState(await platformSnapshot(),'source_query');check(editor.state.query_object==='-1'&&editor.state.selected_object==='-1'&&editor.state.query_source_revision===before.source_revision,'Conversion query editor did not own the actual primary source');const draft=await sourceQueryDraft(text),known=new Set(replies().map(reply=>reply.id)),applyStart=starts().length;await authorActivate('source_query','apply');let validation,failed;
  for(const deadline=Date.now()+15000;Date.now()<deadline;){validation=replies().find(reply=>reply.kind===11&&!known.has(reply.id));if(validation)break;await pause(20);}check(validation&&!validation.error,'CAST query did not bind successfully through real Worker validation');
  for(const deadline=Date.now()+15000;Date.now()<deadline;){failed=replies().find(reply=>reply.kind===0&&!known.has(reply.id)&&BigInt(reply.id)>BigInt(validation.id));if(failed)break;await pause(20);}check(failed?.error,'Actual page execution did not raise the requested conversion error');await frames(4);
  // The first real idle Error frame is the preservation baseline. Applying a
  // query legitimately changed the source revision and invalidated its page.
  const baseline=authorState(await platformSnapshot(),'error'),value=errorReviewState(baseline,failed),jobs=starts().length,applyJobs=starts().slice(applyStart);check(Number(baseline.state.source_revision)>Number(before.source_revision)&&applyJobs.some(job=>job.kind===11&&job.id===validation.id)&&applyJobs.some(job=>job.kind===0&&job.id===failed.id)&&applyJobs.every(job=>[0,8,11].includes(job.kind)),'Real failure did not originate in ordinary validation/source execution');
  const picks=[],prefix=narrow?'error-short150':'error-wide100';
  for(const [name,choose,frozenName]of[
   ['content',menu=>({id:'author.error.content',bounds:menu.state.error_content_rect.split(',').map(Number)}),prefix],
   ['panel',menu=>({id:'author.error.background',bounds:[menu.panel[0]+1,menu.panel[1]+1,2,2]}),null],
   ['title',menu=>({id:'author.error.heading',bounds:menu.heading}),null],
   ['x-close',menu=>menu.rows.find(row=>row.key==='x_close'),null],
   ['copy',menu=>menu.rows.find(row=>row.key==='copy_error'),null],
   ['relink-disabled',menu=>menu.rows.find(row=>row.key==='relink'&&!row.enabled),null],
   ['footer-close',menu=>menu.rows.find(row=>row.key==='close'),null],
   ['backdrop',menu=>({id:'author.error.backdrop.background',bounds:[1,Number(menu.state.logical.split(',')[1])/2,2,2]}),null]]){const pick=await authorPick('error',choose,prefix+'-'+name,{frozenName});retainedValueReview(pick,'error_');cellModelRetained(baseline.state,pick.after,'Error reporting changed its post-failure workbook baseline');errorReviewState(authorState(pick.after,'error'),failed);picks.push(pick);}
  check(starts().length===jobs,'Error-only review activated Copy/Relink or submitted SQL');if(narrow)await key('Escape','Escape',27);else await authorActivate('error','close');const closed=await platformSnapshot();cellModelRetained(baseline.state,closed,'Normal Error dismissal changed the failed source baseline');check(closed.modal==='0'&&closed.request===baseline.state.request&&starts().length===jobs,'Normal Error dismissal submitted recovery work or left the modal open');
  await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await key('z','KeyZ',90,'',2);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));const recovered=await waitPage(failed.id);await frames(4);const restored=await platformSnapshot();check(recovered.rows===5&&recovered.columns===6&&recovered.total==='5'&&restored.modal==='0'&&restored.page==='0/5'&&restored.object_count==='0'&&restored.manual_cells==='0'&&Number(restored.source_revision)>Number(baseline.state.source_revision),'Normal source Undo did not recover the real five-row transactions data');const recoveryJobs=starts().slice(jobs);check(recoveryJobs.some(job=>job.kind===0&&job.id===recovered.id)&&recoveryJobs.every(job=>[0,8].includes(job.kind)),'Error recovery used an unexpected controller operation');await capture(prefix+'-recovered');
  const idleJobs=starts().length;await pause(250);await frames(3);const idle=await platformSnapshot();check(idle.modal==='0'&&idle.page===restored.page&&idle.source_revision===restored.source_revision&&starts().length===idleJobs,'Recovered Error source resubmitted work while idle');cases.push({narrow,before,editor:editor.state,text,draft,validation,failed,applyJobs,baseline:baseline.state,value,picks,closed,recovered,restored,recoveryJobs,idle});previous=recovered;
 }
 await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('7','Digit7',55);await key('Home','Home',36);await key('ArrowDown','ArrowDown',40);await key('Enter','Enter',13);await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:dpr,mobile:false});await frames(4);const final=await platformSnapshot();check(final.modal==='0'&&final.ui_scale==='1.000000'&&final.zoom===initial.zoom&&final.page==='0/5'&&final.manual_cells==='0'&&final.object_count==='0'&&previous.total==='5','Error review did not restore the actual source and Standard scale');check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Error review staged a saved report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Error review wrote into the persistent queue');
 const result={source,initial,cases,final,scope:'Actual primary source-query CAST binding followed by real official Worker conversion failures, captured only after idle Error adoption. Wide100 and short150 actual wrapped content/line/clipping/time/document identity, panel/title/Close/Copy and rendered disabled browser Relink/backdrop picking preserve the post-failure baseline through composer cancellation with no queries. Copy/Relink never activate; normal Close/Escape and source Undo republish five real rows, then remain idle. Actual clipping is recorded without fabrication; no saved reports, clipboard or broad pending-state acceptance.'};await fs.writeFile(path.join(output,'review-error.json'),JSON.stringify(result,null,2));return result;
}
function cellEditorState(state,text,{partial=false,fromModel=false}={}){
 const bytes=Buffer.from(text,'utf8'),length=bytes.length,cursor=partial?length-Buffer.byteLength([...text].at(-1)):length;
 let prefix=bytes.subarray(0,64);while(prefix.toString('utf8').includes('\ufffd'))prefix=prefix.subarray(0,-1);
 check(state.author_kind==='cell_editor'&&state.cell_editor_bytes===String(length)&&state.cell_editor_text_hex===(length?prefix.toString('hex'):'-')&&state.cell_editor_text_truncated===String(Number(prefix.length<length))&&state.cell_editor_value_truncated==='0','Inline editor lost its exact bounded UTF-8 VARCHAR draft');
 check(state.cell_editor_cursor===String(cursor)&&state.cell_editor_anchor===String(length)&&(length?state.cell_editor_select_all===(fromModel?'1':'0'):['0','1'].includes(state.cell_editor_select_all)),'Inline editor lost its byte-based partial selection');
 const origin=state.cell_editor_origin?.split(','),[row,column]=state.active.split(',');check(origin?.length===8&&origin.every(value=>/^\d+$/.test(value))&&origin.join(',')===[String(Number(state.sheet)+1),'0','0','0',origin[4],row,column,'1'].join(','),'Inline editor pinned a source/object instead of the actual manual cell/revision');
 const bounds=state.cell_editor_rect?.split(',').map(Number),body=state.body.split(',').map(Number),history=state.cell_editor_history?.split(',').map(Number),historyBytes=state.cell_editor_history_bytes?.split(',').map(Number),view=Number(state.cell_editor_view_start);
 check(bounds?.length===4&&bounds.every(Number.isFinite)&&bounds[2]>0&&bounds[3]>0&&bounds[0]>=body[0]-.01&&bounds[1]>=body[1]-.01&&bounds[0]+bounds[2]<=body[0]+body[2]+.01&&bounds[1]+bounds[3]<=body[1]+body[3]+.01,'Inline editor escaped its actual worksheet clip');
 check(history?.length===2&&history.every(value=>Number.isInteger(value)&&value>=0&&value<=32)&&historyBytes?.length===2&&historyBytes.every(value=>Number.isSafeInteger(value)&&value>=0)&&history[1]===0&&Number.isInteger(view)&&view>=0&&view<=length&&/^\d+$/.test(state.cell_editor_revision??''),'Inline editor lost owned draft history/revision/scroll metadata');
 if(length)check((fromModel?history.join(',')==='0,0':history[0]>0&&historyBytes[0]>0)&&view>0,'Long actual draft did not retain its expected editor history and horizontal scrolling');
 else check(history.join(',')==='0,0'&&view===0,'Empty F2 draft retained prior text history or horizontal scroll');
 return{bounds,origin,history,historyBytes,cursor,length,view,prefix:prefix.toString('hex')};
}
function cellModelRetained(before,after,message){
 const fields=['sheet','manual_cells','sheet_count','dirty','source_revision','schema','page','spare','active','anchor','extent','grid_selection','selected_object','object_count'];check(fields.every(name=>before[name]!==undefined&&before[name]===after[name]),message);
}
function copyExpectedTSV(repetitions=1){
 // Independent fixture inventory, not a renderer/cache read or a second codec.
 // The fourth fixture value contains two literal double quotes (six in TSV).
 const rows=['1\t\t10','2\tNULL\t10','3\t""\t10','4\t""""""\t10','5\t(blank)\t10','6\tnull\t10'];
 return Array.from({length:6*repetitions},(_,row)=>rows[row%6]+'\t\t'+(row===0?'"O\'Brien ""quoted"""':'')).join('\n');
}
function copyState(state){
 const {clipboard_pending:pending,clipboard_completed:completed,clipboard_status:status,clipboard_message_hex:hex,clipboard_message_truncated:truncated}=state;
 check(/^\d+$/.test(pending??'')&&/^\d+$/.test(completed??'')&&['0','1','2'].includes(status),'Copy lost actual pending/completed/status ownership');
 check((hex==='-'||/^(?:[0-9a-f]{2}){1,64}$/.test(hex??''))&&['0','1'].includes(truncated),'Copy lost its bounded actual status text');
 const message=hex==='-'?'':Buffer.from(hex,'hex').toString('utf8');check(!message.includes('\ufffd'),'Copy status prefix split a UTF-8 codepoint');
 if(pending!=='0')check(BigInt(pending)>BigInt(completed)&&status==='0','Pending clipboard Promise was reported as completed or successful');
 if(status!=='0')check(pending==='0'&&completed!=='0','Terminal clipboard status has no adopted result identity');
 if(status==='2')check(message.startsWith('Could not copy: '),'Rejected/unavailable browser copy did not show its actual failure');
 return{pending,completed,status,message,truncated};
}
async function copyMemory(){return evaluate('({mode:duxcelTestClipboard.mode,memory:duxcelTestClipboard.memory,attempts:duxcelTestClipboard.attempts.map(row=>({...row})),fallbacks:[...duxcelTestClipboard.fallbacks]})');}
async function copyMode(mode){check(['defer','reject','throw','unavailable','success'].includes(mode),'Invalid owned clipboard test mode');await evaluate('duxcelTestClipboard.mode='+JSON.stringify(mode));}
async function copyKey(){
 await evaluate('globalThis.duxcelCopyKeys=[];globalThis.duxcelCopyKeyListener=event=>{if(event.code==="KeyC")duxcelCopyKeys.push({key:event.key,control:event.ctrlKey,trusted:event.isTrusted,canvas:event.target===document.querySelector("canvas")});};window.addEventListener("keydown",duxcelCopyKeyListener);true');
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));try{await key('c','KeyC',67,'',2);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));}
 const delivered=await evaluate('window.removeEventListener("keydown",duxcelCopyKeyListener);duxcelCopyKeys');check(delivered.length===1&&delivered[0].key==='c'&&delivered[0].control&&delivered[0].trusted&&delivered[0].canvas,'Normal CtrlC did not reach the focused shared-C canvas as a trusted shortcut');return delivered;
}
async function waitCopyCompleted(previous,status){
 for(const deadline=Date.now()+15000;Date.now()<deadline;){const state=await platformSnapshot(),ownership=copyState(state);if(BigInt(ownership.completed)>BigInt(previous)){check(ownership.pending==='0'&&ownership.status===String(status),'Real clipboard result was adopted with the wrong terminal status');return{state,ownership};}await pause(20);}
 throw new Error('Real asynchronous clipboard result was not adopted by an ordinary C frame');
}
function copyRetained(before,after,message,{request=true}={}){
 cellModelRetained(before,after,message);check(Object.keys(before).filter(name=>name.startsWith('stats_')).every(name=>before[name]===after[name]),message);
 const fields=['modal','busy','io_busy','focus','top','horizontal','fraction'];if(request)fields.push('request');
 check(fields.every(name=>before[name]!==undefined&&before[name]===after[name]),message);
}
function copyCacheCoverage(state,rows){
 const intervals=['page','spare'].map(name=>{const pair=state[name]?.split('/').map(Number);check(pair?.length===2&&pair.every(Number.isSafeInteger)&&pair[0]>=0&&pair[1]>=0,'Copy cache metadata omitted its actual owned page range');return pair;});
 let covered=0;for(let row=0;row<rows;row++)if(intervals.some(([offset,count])=>row>=offset&&row<offset+count))covered++;
 check(covered>0&&covered<rows,'Uncached-copy fixture is wholly cached or has no actual cached contributor');return{intervals,covered,missing:rows-covered};
}
function exportDownloadHarnessSource(){
 return `(()=>{const create=URL.createObjectURL.bind(URL),revoke=URL.revokeObjectURL.bind(URL),state={mode:'success',attempts:[],revocations:[]};
 URL.createObjectURL=function(blob){
  if(!(blob instanceof Blob)||blob.type!=='application/vnd.apache.parquet')return create(blob);
  const attempt={index:state.attempts.length,mode:state.mode,size:blob.size,type:blob.type,created_ms:performance.now()};state.attempts.push(attempt);
  if(state.mode==='throw'){attempt.error='Owned URL creation failure';throw new Error(attempt.error);}
  const url=create(blob);attempt.url=url;
  blob.arrayBuffer().then(async buffer=>{const bytes=new Uint8Array(buffer);attempt.sha256=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',buffer)),byte=>byte.toString(16).padStart(2,'0')).join('');attempt.header=Array.from(bytes.slice(0,4));attempt.footer=Array.from(bytes.slice(-4));attempt.hashed_bytes=bytes.byteLength;}).catch(error=>{attempt.observation_error=String(error);});
  return url;
 };
 URL.revokeObjectURL=function(url){const result=revoke(url);if(state.attempts.some(attempt=>attempt.url===url))state.revocations.push({url,revoked_ms:performance.now()});return result;};
 Object.defineProperty(globalThis,'duxcelTestDownload',{value:state});})();`;
}
async function exportCanvasHome(){
 const state=await platformSnapshot(),[x,y,w,h]=state.body.split(',').map(Number),[cw,rh]=state.grid_cell.split(',').map(Number);await click(x+Math.min(cw,w)/2,y+Math.min(rh,h)/2);await sourceHome();
 const selected=await platformSnapshot();check(selected.modal==='0'&&selected.active==='0,0'&&selected.extent==='0,0'&&selected.selected_object==='-1','Actual export source canvas did not retain a single A1 selection');return selected;
}
async function exportAction({fileMenu=false}={}){
 if(fileMenu){await key('Alt','AltLeft',18);await key('f','KeyF',70);const state=await platformSnapshot();check(state.file_menu==='1'&&state.file_visible==='8','Actual export File menu omitted its eight real commands');await key('e','KeyE',69);}
 else{
  await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16,{modifiers:2}));
  try{await key('e','KeyE',69,'',10);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16,{modifiers:2}));await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));}
 }
}
async function exportDownload(beforeEvents,beforeFiles,index){
 const directory=path.join(output,'downloads');let begin,complete,observed;
 for(const deadline=Date.now()+15000;Date.now()<deadline;){
  const incoming=events.slice(beforeEvents);begin=incoming.find(event=>event.method==='Browser.downloadWillBegin')?.params;complete=begin&&incoming.find(event=>event.method==='Browser.downloadProgress'&&event.params.guid===begin.guid&&event.params.state==='completed')?.params;
  observed=await evaluate('duxcelTestDownload.attempts['+index+']');if(begin&&complete&&observed?.sha256)break;check(!observed?.observation_error,'Actual download Blob observation failed');await pause(20);
 }
 check(begin&&complete&&observed?.sha256,'Actual browser download did not complete with an observed Parquet Blob');check(begin.url===observed.url&&begin.suggestedFilename==='null-text.csv-export.parquet','Download basename did not use the displayed source sheet or actual Blob URL');
 const files=(await fs.readdir(directory)).filter(name=>!beforeFiles.includes(name));check(files.length===1&&!files[0].endsWith('.crdownload'),'Actual completed export did not produce exactly one private file');const file=path.resolve(directory,files[0]);check(path.dirname(file)===directory,'Browser export escaped its owned download folder');
 const bytes=await fs.readFile(file),sha256=createHash('sha256').update(bytes).digest('hex');check(complete.totalBytes===bytes.length&&complete.receivedBytes===bytes.length,'Completed browser download counters differ from the actual private file');check(observed.type==='application/vnd.apache.parquet'&&observed.size===bytes.length&&observed.hashed_bytes===bytes.length&&observed.sha256===sha256,'Downloaded bytes differ from the actual synchronously snapshotted Parquet Blob');
 check(bytes.length>=12&&bytes.subarray(0,4).equals(Buffer.from('PAR1'))&&bytes.subarray(-4).equals(Buffer.from('PAR1'))&&observed.header.join(',')==='80,65,82,49'&&observed.footer.join(',')==='80,65,82,49','Actual exported file lost its Parquet header/footer');const footer=bytes.readUInt32LE(bytes.length-8);check(footer>0&&footer<=bytes.length-12,'Actual Parquet footer length is outside the owned file');
 for(const deadline=Date.now()+5000;Date.now()<deadline;){if(await evaluate('duxcelTestDownload.revocations.some(item=>item.url==='+JSON.stringify(observed.url)+')'))break;await pause(20);}
 const state=await evaluate('duxcelTestDownload');check(state.revocations.filter(item=>item.url===observed.url).length===1,'Actual export Blob URL did not release exactly once before another request');return{file,bytes:bytes.length,sha256,footer,begin,complete,observed,revocations:state.revocations};
}
async function exportRetainDownload(download){
 // Chromium may replace a same-named download. Preserve the independently
 // verified first artifact before recovery creates that basename again.
 const directory=path.join(output,'retained-exports'),file=path.join(directory,path.basename(download.file));check(path.dirname(download.file)===path.join(output,'downloads')&&path.dirname(file)===directory,'Export retention escaped its private run directories');await fs.mkdir(directory);await fs.rename(download.file,file);
 const bytes=await fs.readFile(file);check(bytes.length===download.bytes&&createHash('sha256').update(bytes).digest('hex')===download.sha256,'Moving the verified export changed its retained bytes');const retained={...download,download_file:download.file,file};await fs.writeFile(path.join(output,'export-retained.json'),JSON.stringify(retained,null,2));return retained;
}
async function exportOpenDownloaded(file,afterId){
 await send('Page.setInterceptFileChooserDialog',{enabled:true});await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await key('o','KeyO',79,'',2);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));
 const document=await send('DOM.getDocument'),input=await send('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#duxcel-file-input'});check(input.nodeId,'Normal Open did not expose the actual file chooser input for the downloaded Parquet');
 await evaluate(`(()=>{const input=document.querySelector('#duxcel-file-input');globalThis.duxcelFileInputEvents=[];for(const name of ['input','change','cancel'])input.addEventListener(name,event=>globalThis.duxcelFileInputEvents.push({type:event.type,trusted:event.isTrusted,files:input.files?.length??0}));return true;})()`);
 const delivery=await completeTableChooser({node:input.nodeId},[file]),page=await waitPage(afterId);check(page.rows===6&&page.columns===5&&page.total==='6','Downloaded Parquet did not reopen as the actual six-row/five-column typed source');await frames(4);return{delivery,page};
}
async function exportInspect(specification){
 await sourceHome();for(let index=0;index<specification.column;index++)await key('ArrowRight','ArrowRight',39);for(let index=0;index<specification.row;index++)await key('ArrowDown','ArrowDown',40);const selected=await platformSnapshot();check(selected.active===specification.row+','+specification.column&&selected.extent===selected.active,'Actual reopened Parquet navigation selected the wrong typed sentinel');
 const [bx]=selected.body.split(',').map(Number),[cw]=selected.grid_cell.split(',').map(Number),[,gy]=selected.grid_rect.split(',').map(Number),scale=Number(selected.ui_scale);await rightClick(bx+(specification.column+.5)*cw-Number(selected.horizontal)*scale*Number(selected.zoom),gy+10*scale);await authorActivate('context','inspect');const inspector=authorState(await platformSnapshot(),'cell_inspector'),value=inspectorReviewState(inspector,specification);
 check(inspector.state.manual_cells==='0'&&inspector.state.object_count==='0'&&inspector.state.page==='0/6','Reopened typed inspection introduced manual/object cells or changed source data');await capture('export-inspector-'+specification.name);await authorActivate('cell_inspector','close');await sourceHome();return{specification,selected,inspector:inspector.state,value};
}
async function exportCancelGesture(){
 const jobs=starts().length,eventStart=events.length;await evaluate(`(()=>{globalThis.duxcelExportCancelKeys=[];globalThis.duxcelExportCancelListener=event=>{if(event.type!=='keydown'&&event.type!=='keyup')return;duxcelExportCancelKeys.push({key:event.key,type:event.type,trusted:event.isTrusted,canvas:event.target===document.querySelector('canvas'),control:event.ctrlKey,shift:event.shiftKey,download_attempts:duxcelTestDownload.attempts.length,time_ms:performance.timeOrigin+performance.now()});};window.addEventListener('keydown',duxcelExportCancelListener,true);window.addEventListener('keyup',duxcelExportCancelListener,true);return true;})()`);
 let started;steps.push({key:'Ctrl Shift E → actual Export start → Escape',time:Date.now(),one_frame_export:true});
 // Keep the actual modifiers held through one ordinary C frame. No capture,
 // held reply, engine delay or synthetic application callback creates this race.
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16,{modifiers:2}));
 try{
  await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','e','KeyE',69,{modifiers:10}));await frames(1);
  for(const deadline=Date.now()+5000;Date.now()<deadline;){started=starts().slice(jobs).find(job=>job.kind===7);if(started)break;await pause(1);}
  check(started&&starts().slice(jobs).length===1,'Trusted export chord did not start exactly one real kind7 job');
 }finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','e','KeyE',69,{modifiers:10}));await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16,{modifiers:2}));await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));}
 const attempts=await evaluate('duxcelTestDownload.attempts.length');check(!replies().some(reply=>reply.kind===7&&reply.id===started.id)&&attempts===0&&events.slice(eventStart).every(event=>event.method!=='Browser.downloadWillBegin'),'Missed natural pending-export race: completion/Blob/download already occurred before Escape');
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Escape','Escape',27));try{await frames(1);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Escape','Escape',27));}await frames(2);
 const delivered=await evaluate(`(()=>{window.removeEventListener('keydown',duxcelExportCancelListener,true);window.removeEventListener('keyup',duxcelExportCancelListener,true);return duxcelExportCancelKeys;})()`),escape=delivered.find(event=>event.type==='keydown'&&event.key==='Escape'),chord=delivered.find(event=>event.type==='keydown'&&event.key.toLowerCase()==='e');
 check(chord?.trusted&&chord.canvas&&chord.control&&chord.shift&&escape?.trusted&&escape.canvas&&!escape.control&&!escape.shift&&escape.download_attempts===0,'Actual export/Escape trusted canvas delivery or pending Blob precondition failed');check(['Control','Shift','e','Escape'].every(name=>delivered.some(event=>event.key===name&&event.type==='keydown')&&delivered.some(event=>event.key===name&&event.type==='keyup')),'Cancellation gesture left an actual key unbalanced');return{started,eventStart,jobs,attempts,delivered};
}
async function browserExportCancel(){
 await send('Emulation.setDeviceMetricsOverride',{width:1440,height:988,deviceScaleFactor:dpr,mobile:false});await frames(4);const source=await zoomSource({total:'6',columns:3});await sourceHome();const initial=await platformSnapshot();check(initial.manual_cells==='0'&&initial.object_count==='0','Export cancellation requires the real unmodified NULL/text source');
 for(let index=0;index<7;index++)await key('ArrowRight','ArrowRight',39);for(let index=0;index<9;index++)await key('ArrowDown','ArrowDown',40);for(const digit of '991')await key(digit,'Digit'+digit,digit.charCodeAt(0),digit);await key('Enter','Enter',13);await sourceHome();const manual=await platformSnapshot();check(manual.manual_cells==='1'&&manual.dirty==='1','Cancellation fixture did not retain its actual H10 manual owner');
 await focusTransformations();await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));try{await key('F10','F10',121,'',8);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));}await authorActivate('step','edit');const text='SELECT input.*,i AS Replica FROM input CROSS JOIN range(100000) AS t(i)',draft=await sourceQueryDraft(text),beforeValidation=replies().filter(reply=>reply.kind===11).at(-1)?.id,beforePage=replies().filter(reply=>reply.kind===0).at(-1).id;await authorActivate('source_query','apply');const validation=await feedbackReply(11,beforeValidation??null),expanded=await waitPage(beforePage);check(expanded.rows===256&&expanded.columns===4&&expanded.total==='600000','Natural export fixture did not publish the actual Page256 and full600000-row COUNT');await frames(4);const baseline=await exportCanvasHome();check(baseline.manual_cells==='1'&&baseline.dirty==='1'&&baseline.object_count==='0'&&baseline.page==='0/256'&&BigInt(baseline.source_revision)>BigInt(manual.source_revision),'Expanded source query lost its owned manual/source baseline');await fs.writeFile(path.join(output,'export-cancel-source.json'),JSON.stringify({source,initial,manual,text,draft,validation,expanded,baseline},null,2));
 const gesture=await exportCancelGesture(),cancelled=await platformSnapshot();cellModelRetained(baseline,cancelled,'Real Escape cancellation changed source/pages/selection/manual ownership or cleared dirty');check(cancelled.modal==='0'&&cancelled.busy==='0'&&cancelled.io_busy==='0'&&BigInt(cancelled.request)===BigInt(gesture.started.id)+1n&&Buffer.from(cancelled.clipboard_message_hex,'hex').toString('utf8')==='Export cancellation requested.','Actual pending export did not publish its owned cancellation request/idle state');const directory=path.join(output,'downloads');await pause(300);await frames(3);const settled=await platformSnapshot();cellModelRetained(cancelled,settled,'Cancelled export changed retained source/model while settling');check(settled.request===cancelled.request&&settled.busy==='0'&&settled.modal==='0'&&starts().length===gesture.jobs+1&&!replies().some(reply=>reply.kind===7&&reply.id===gesture.started.id),'Cancelled job published a completion or resubmitted engine work');check((await evaluate('duxcelTestDownload.attempts.length'))===0&&events.slice(gesture.eventStart).every(event=>event.method!=='Browser.downloadWillBegin')&&(await fs.readdir(directory)).length===0,'Cancelled pending export created an actual Blob/download/file');await capture('export-cancelled');await fs.writeFile(path.join(output,'export-cancel-gesture.json'),JSON.stringify({gesture,cancelled,settled},null,2));
 const beforeUndoPage=replies().filter(reply=>reply.kind===0).at(-1).id;await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));try{await key('z','KeyZ',90,'',2);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));}const restoredPage=await waitPage(beforeUndoPage);check(restoredPage.rows===6&&restoredPage.columns===3&&restoredPage.total==='6','Normal Undo after cancellation did not restore the real six-row source');await frames(4);const restored=await exportCanvasHome();check(restored.manual_cells==='1'&&restored.object_count==='0'&&restored.dirty==='1'&&restored.sheet===baseline.sheet&&restored.sheet_count===baseline.sheet_count&&BigInt(restored.source_revision)>BigInt(baseline.source_revision),'Normal Undo lost original source/manual ownership or failed revision invalidation');
 const recoveryEvents=events.length,recoveryFiles=await fs.readdir(directory),recoveryAttempt=await evaluate('duxcelTestDownload.attempts.length'),recoveryJobs=starts().length;await exportAction();const recoveredReply=await feedbackReply(7,gesture.started.id);await frames(4);const recovered=await platformSnapshot();cellModelRetained(restored,recovered,'Small export recovery changed original source/pages/manual ownership or dirty');check(recovered.modal==='0'&&recovered.busy==='0'&&Buffer.from(recovered.clipboard_message_hex,'hex').toString('utf8').startsWith('Download requested for 6 rows:'),'Real six-row export did not recover after cancellation');const recovery=await exportDownload(recoveryEvents,recoveryFiles,recoveryAttempt);check(starts().slice(recoveryJobs).length===1&&starts()[recoveryJobs].kind===7&&starts()[recoveryJobs].id===recoveredReply.id,'Small recovery submitted unexpected engine work');
 for(let index=0;index<7;index++)await key('ArrowRight','ArrowRight',39);for(let index=0;index<9;index++)await key('ArrowDown','ArrowDown',40);await key('F2','F2',113);const retainedManual=await platformSnapshot();check(retainedManual.cell_editor_text_hex===Buffer.from('991').toString('hex')&&retainedManual.cell_editor_bytes==='3'&&retainedManual.cell_editor_value_truncated==='0'&&retainedManual.manual_cells==='1','Cancelled/recovered export lost the exact H10 manual text');await key('Escape','Escape',27);await sourceHome();const idleBase=await platformSnapshot(),idleJobs=starts().length;await pause(300);await frames(3);const idle=await platformSnapshot();cellModelRetained(idleBase,idle,'Idle cancellation recovery changed its owned workbook/source');check(idle.modal==='0'&&idle.busy==='0'&&idle.request===idleBase.request&&starts().length===idleJobs&&!replies().some(reply=>reply.kind===7&&reply.id===gesture.started.id)&&(await evaluate('duxcelTestDownload.attempts.length'))===1&&events.slice(gesture.eventStart).filter(event=>event.method==='Browser.downloadWillBegin').length===1,'Cancelled job leaked a late completion/download or idle resubmitted work');check(idle.clipboard_pending==='0'&&idle.clipboard_completed==='0'&&consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Cancellation acceptance activated clipboard or staged a saved review');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Cancellation acceptance wrote into the persistent review queue');
 const result={source,initial,manual,text,draft,validation,expanded,baseline,gesture,cancelled,settled,restoredPage,restored,recoveredReply,recovered,recovery,retainedManual,idle,scope:'Actual Query Apply publishes Page256 and COUNT600000 from the real six-row input CROSS JOIN range100000. A balanced trusted one-frame CtrlShiftE starts kind7; trusted Escape arrives before any C completion/Blob/download, or the gate fails the missed race honestly. Normal App cancellation discards the job, retains source/pages/manual/selection/dirty and creates no Blob/file. Normal Undo restores six rows; a new real kind7 recovery export passes actual browser event/Blob/file SHA256/PAR1/footer/revocation checks. H10 and bounded idle remain owned. No held reply, artificial delay, synthetic C callback, user Downloads, OS clipboard, report upload or Save/durability claim.'};await fs.writeFile(path.join(output,'export-cancel.json'),JSON.stringify(result,null,2));return result;
}
async function browserExport(){
 await send('Emulation.setDeviceMetricsOverride',{width:1440,height:988,deviceScaleFactor:dpr,mobile:false});await frames(4);const source=await zoomSource({total:'6',columns:3});await sourceHome();const initial=await platformSnapshot();check(initial.manual_cells==='0'&&initial.object_count==='0','Export acceptance requires the real unmodified NULL/text source');
 for(let index=0;index<7;index++)await key('ArrowRight','ArrowRight',39);for(let index=0;index<9;index++)await key('ArrowDown','ArrowDown',40);for(const digit of '991')await key(digit,'Digit'+digit,digit.charCodeAt(0),digit);await key('Enter','Enter',13);await sourceHome();const manual=await platformSnapshot();check(manual.manual_cells==='1'&&manual.dirty==='1','Actual export fixture did not retain the H10 manual sentinel');
 await focusTransformations();await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));try{await key('F10','F10',121,'',8);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));}await authorActivate('step','edit');
 const text='SELECT *,CAST(9007199254740993 AS BIGINT) AS ExactID,CAST(9007199254740993.1234567890 AS DECIMAL(38,10)) AS ExactDecimal FROM input',draft=await sourceQueryDraft(text),beforeValidation=replies().filter(reply=>reply.kind===11).at(-1)?.id,beforePage=replies().filter(reply=>reply.kind===0).at(-1).id;await authorActivate('source_query','apply');const validation=await feedbackReply(11,beforeValidation??null),typed=await waitPage(beforePage);check(typed.rows===6&&typed.columns===5&&typed.total==='6','Actual typed export query failed to publish its six exact-value rows');await frames(4);const baseline=await exportCanvasHome();check(baseline.manual_cells==='1'&&baseline.dirty==='1'&&baseline.object_count==='0'&&baseline.page==='0/6'&&BigInt(baseline.source_revision)>BigInt(manual.source_revision),'Typed query Apply lost original manual/source ownership');
 const directory=path.join(output,'downloads'),jobs=starts().length,firstEvents=events.length,firstFiles=await fs.readdir(directory),firstAttempt=await evaluate('duxcelTestDownload.attempts.length'),previousExport=replies().filter(reply=>reply.kind===7).at(-1)?.id;await exportAction({fileMenu:true});const exported=await feedbackReply(7,previousExport??null);await frames(4);const succeeded=await platformSnapshot();cellModelRetained(baseline,succeeded,'Actual export changed source/pages/selection/manual data or cleared workbook dirty');check(succeeded.modal==='0'&&succeeded.busy==='0'&&succeeded.io_busy==='0'&&Buffer.from(succeeded.clipboard_message_hex,'hex').toString('utf8').startsWith('Download requested for 6 rows: null-text.csv-export.parquet'),'Export dispatch did not report its honest requested-download status');const first=await exportDownload(firstEvents,firstFiles,firstAttempt);await fs.writeFile(path.join(output,'export-first.json'),JSON.stringify({text,draft,validation,typed,baseline,exported,succeeded,first},null,2));await capture('export-requested');
 const failureEvents=events.length,failureFiles=await fs.readdir(directory),failureAttempt=await evaluate('duxcelTestDownload.attempts.length');await evaluate("duxcelTestDownload.mode='throw';true");await exportAction();const failedReply=await feedbackReply(7,exported.id);await frames(4);const failed=await platformSnapshot();cellModelRetained(baseline,failed,'Download dispatch failure changed the original workbook/source/manual data');check(failed.author_kind==='error'&&failed.busy==='0'&&failed.io_busy==='0'&&Buffer.from(failed.clipboard_message_hex,'hex').toString('utf8').startsWith('Could not request download: Owned URL creation failure'),'Actual synchronous download dispatch failure was not an honest owned Error');const failedAttempt=await evaluate('duxcelTestDownload.attempts['+failureAttempt+']');check(failedAttempt?.mode==='throw'&&failedAttempt.error==='Owned URL creation failure'&&!failedAttempt.url,'Failure case did not reach the real URL boundary without creating a URL');check(events.slice(failureEvents).every(event=>event.method!=='Browser.downloadWillBegin')&&JSON.stringify(await fs.readdir(directory))===JSON.stringify(failureFiles),'Failed URL creation unexpectedly dispatched or wrote a file');await fs.writeFile(path.join(output,'export-dispatch-error.json'),JSON.stringify({failedReply,failed,failedAttempt},null,2));await capture('export-dispatch-error');await key('Escape','Escape',27);const cancelled=await platformSnapshot();cellModelRetained(baseline,cancelled,'Normal Error Escape changed original source/manual ownership');check(cancelled.modal==='0','Normal Escape did not close the real export Error');await evaluate("duxcelTestDownload.mode='success';true");
 const retainedFirst=await exportRetainDownload(first);
 const recoveryEvents=events.length,recoveryFiles=await fs.readdir(directory),recoveryAttempt=await evaluate('duxcelTestDownload.attempts.length');await exportAction();const recoveredReply=await feedbackReply(7,failedReply.id);await frames(4);const recovered=await platformSnapshot();cellModelRetained(baseline,recovered,'Recovered export changed original source/manual ownership or dirty state');check(recovered.modal==='0'&&recovered.busy==='0'&&Buffer.from(recovered.clipboard_message_hex,'hex').toString('utf8').startsWith('Download requested for 6 rows:'),'Normal export did not recover after the real URL boundary failure');const recovery=await exportDownload(recoveryEvents,recoveryFiles,recoveryAttempt);check(starts().slice(jobs).length===3&&starts().slice(jobs).every(job=>job.kind===7),'Three normal export attempts submitted unexpected engine work');
 const reopened=await exportOpenDownloaded(retainedFirst.file,replies().filter(reply=>reply.kind===0).at(-1).id),downloaded=await exportCanvasHome();check(Number(downloaded.sheet_count)===Number(baseline.sheet_count)+1&&downloaded.sheet!==baseline.sheet&&downloaded.manual_cells==='0'&&downloaded.object_count==='0'&&downloaded.dirty==='1','Real downloaded Parquet Open did not create a separate typed source sheet without clearing dirty');const inspections=[];
 for(const specification of [{name:'sql-null',row:0,column:1,text:'',isNull:true},{name:'empty-text',row:2,column:1,text:'',isNull:false},{name:'literal-null',row:1,column:1,text:'NULL',isNull:false},{name:'literal-quotes',row:3,column:1,text:'""',isNull:false},{name:'exact-bigint',row:0,column:3,type:'BIGINT',field:'ExactID',text:'9007199254740993',isNull:false},{name:'exact-decimal',row:0,column:4,type:'DECIMAL(38,10)',field:'ExactDecimal',text:'9007199254740993.1234567890',isNull:false}])inspections.push(await exportInspect(specification));
 let workbookFocus=false;for(let index=0;index<16;index++){await key('F6','F6',117);if((await platformSnapshot()).focus?.split('/')[0]==='2'){workbookFocus=true;break;}}check(workbookFocus,'Actual focus traversal could not return to the original workbook sheet navigator');await key('Home','Home',36);for(let index=0;index<Number(baseline.sheet);index++)await key('ArrowDown','ArrowDown',40);const returnPage=replies().filter(reply=>reply.kind===0).at(-1).id;await key('Enter','Enter',13);const returnedPage=await waitPage(returnPage);await frames(4);const returned=await exportCanvasHome();check(returned.sheet===baseline.sheet&&returned.manual_cells==='1'&&returned.object_count==='0'&&returned.dirty==='1'&&returnedPage.rows===6&&returnedPage.columns===5&&returnedPage.total==='6','Normal sheet navigation did not restore the original typed query and manual owner');
 for(let index=0;index<7;index++)await key('ArrowRight','ArrowRight',39);for(let index=0;index<9;index++)await key('ArrowDown','ArrowDown',40);await key('F2','F2',113);const retainedManual=await platformSnapshot();check(retainedManual.cell_editor_text_hex===Buffer.from('991').toString('hex')&&retainedManual.cell_editor_bytes==='3'&&retainedManual.cell_editor_value_truncated==='0'&&retainedManual.manual_cells==='1','Export/reopen lost the exact original H10 manual text');await key('Escape','Escape',27);await sourceHome();const idleBase=await platformSnapshot(),idleJobs=starts().length;await pause(300);await frames(3);const idle=await platformSnapshot();cellModelRetained(idleBase,idle,'Idle exported/reopened source changed its workbook or source cache');check(starts().length===idleJobs&&idle.clipboard_pending==='0'&&idle.clipboard_completed==='0','Idle export acceptance submitted more work or activated clipboard');check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Export acceptance staged a saved review report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Export acceptance wrote a report into the persistent queue');
 const result={source,initial,manual,text,draft,validation,typed,baseline,exported,succeeded,first,failedReply,failed,failedAttempt,cancelled,recoveredReply,recovered,recovery,retainedFirst,reopened,downloaded,inspections,returnedPage,returned,retainedManual,idle,scope:'Actual shared-C source Query Apply adds exact BIGINT/DECIMAL to the real NULL/empty/literal/quotes fixture while H10 stays a separate manual owner. Alt F E and CtrlShiftE run real kind7 exports. Actual Parquet Blobs, browser download events and private on-disk bytes agree by SHA256/header/footer; one synchronous actual URL.createObjectURL throw produces an owned Error with no file, then normal Escape and restored URL boundary recover. Trusted FileOpen reimports the real exported file, Cell Inspector verifies exact types/text and NULL distinction, and normal workbook navigation retains the original query/manual cell. Real revocations precede repeat requests; bounded idle adds no jobs. No user Downloads, OS clipboard, Save/durability claim, artificial pending/cancellation or report upload.'};await fs.writeFile(path.join(output,'export.json'),JSON.stringify(result,null,2));return result;
}
async function browserCopy(){
 const source=await zoomSource({total:'6',columns:3});await sourceHome();const initial=await platformSnapshot();copyState(initial);check(initial.manual_cells==='0'&&initial.object_count==='0','Copy gate requires the real unmodified null/text source');
 for(let index=0;index<4;index++)await key('ArrowRight','ArrowRight',39);await key('F2','F2',113);
 const manualText='O\'Brien "quoted"';for(const character of manualText){const letter=/^[a-z]$/i.test(character),code=letter?'Key'+character.toUpperCase():character===' '?'Space':'Quote',virtual=letter?character.toUpperCase().charCodeAt(0):character===' '?32:222;await key(character,code,virtual,character);}await key('Enter','Enter',13);await sourceHome();
 const manual=await platformSnapshot();check(manual.manual_cells==='1'&&manual.active==='0,0'&&manual.object_count==='0','Actual F2 commit did not retain one manual quote/apostrophe cell beside the source');
 async function selectUsed(rows){
  const state=await platformSnapshot(),[x,y,w,h]=state.body.split(',').map(Number),[cw,rh]=state.grid_cell.split(',').map(Number);await click(x+Math.min(cw,w)/2,y+Math.min(rh,h)/2);await sourceHome();const prior=replies().filter(reply=>reply.kind===10).at(-1)?.id;
  await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16,{modifiers:2}));try{await key('End','End',35,'',10);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16,{modifiers:2}));await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));}
  const reply=await waitStatistics(prior??null),selected=await platformSnapshot();check(selected.active==='0,0'&&selected.anchor==='0,0'&&selected.extent===(rows-1)+',4'&&selected.grid_selection==='0'&&selected.stats_state==='3'&&selected.stats_cells===String(rows*5)&&selected.busy==='0'&&selected.io_busy==='0','Real used-range selection did not settle over the expected source/manual rectangle');return{reply,state:selected};
 }
 const selected=await selectUsed(6),expected=copyExpectedTSV(),baseline=selected.state,jobs=starts().length,memoryBefore=await copyMemory();check(memoryBefore.memory==='owned clipboard baseline'&&memoryBefore.attempts.length===0&&memoryBefore.fallbacks.length===0,'Pre-loader memory clipboard was not isolated from OS fallback');
 await copyMode('defer');const delivered=await copyKey(),pending=await platformSnapshot(),pendingOwnership=copyState(pending),unresolved=await copyMemory();copyRetained(baseline,pending,'Cached copy changed workbook/selection or submitted SQL');check(pendingOwnership.pending!=='0'&&pendingOwnership.completed==='0'&&pendingOwnership.status==='0'&&unresolved.attempts.length===1&&unresolved.attempts[0].status==='pending'&&unresolved.attempts[0].text===expected&&unresolved.memory===memoryBefore.memory,'Unresolved cached copy changed the clipboard, lost exact typed TSV or claimed success');
 await beginReview();const frozen=capturedState();copyState(frozen);check(frozen.clipboard_pending===pendingOwnership.pending&&frozen.clipboard_completed==='0','Frozen capture did not pin the actual unresolved clipboard write');await evaluate('duxcelTestClipboard.resolve(0);true');await frames(5);const resolvedFrozen=await copyMemory();check(resolvedFrozen.memory===expected&&resolvedFrozen.attempts[0].status==='fulfilled'&&JSON.stringify(capturedState())===JSON.stringify(frozen),'Frozen review state did not retain pending ownership across the platform Promise result');await capture('copy-frozen-pending');await key('Escape','Escape',27);
 const fulfilled=await waitCopyCompleted('0',1);check(fulfilled.ownership.completed===pendingOwnership.pending,'Post-cancel clipboard adoption took a different write result');copyRetained(baseline,fulfilled.state,'Clipboard adoption after review cancellation changed workbook/selection/cache');const failures=[];let previous=fulfilled.ownership.completed;
 for(const mode of ['reject','throw','unavailable']){
  await copyMode(mode);const before=await platformSnapshot(),memory=await copyMemory(),delivery=await copyKey(),outcome=await waitCopyCompleted(previous,2),after=await copyMemory();copyRetained(before,outcome.state,'Failed '+mode+' copy changed the retained workbook/selection/cache');check(after.memory===expected&&after.memory===memory.memory&&after.fallbacks.length===0,'Failed '+mode+' copy changed memory or attempted an OS clipboard fallback');
  if(mode==='unavailable')check(after.attempts.length===memory.attempts.length,'Unavailable clipboard invoked a nonexistent writer');else{const attempt=after.attempts.at(-1);check(after.attempts.length===memory.attempts.length+1&&attempt.text===expected&&attempt.mode===mode&&attempt.status===(mode==='throw'?'threw':'rejected'),'Failure mode did not run through the actual dynamically acquired platform writer');}
  check(starts().length===jobs,'Cached failed clipboard write submitted database work');failures.push({mode,before,delivery,outcome,memory:after});previous=outcome.ownership.completed;
 }
 await copyMode('success');const recoveryBefore=await platformSnapshot(),recoveryDelivery=await copyKey(),recovery=await waitCopyCompleted(previous,1),recoveredMemory=await copyMemory();copyRetained(recoveryBefore,recovery.state,'Post-failure copy recovery changed workbook/selection/cache');check(recoveredMemory.memory===expected&&recoveredMemory.attempts.at(-1).status==='fulfilled'&&starts().length===jobs,'Actual clipboard recovery lost exact TSV or submitted SQL');await capture('copy-cached-recovered');
 // A real source edit creates more rows than the two owned C pages can hold.
 // Neither the query nor clipboard controller is called directly from JS.
 await focusTransformations();await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));try{await key('F10','F10',121,'',8);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));}await authorActivate('step','edit');const draft=await sourceQueryDraft('SELECT ID,Value,Amount FROM input CROSS JOIN range(100) AS t(i) ORDER BY i,ID'),known=new Set(replies().map(reply=>reply.id));await authorActivate('source_query','apply');
 let validation;for(const deadline=Date.now()+15000;Date.now()<deadline&&!validation;){validation=replies().find(reply=>reply.kind===11&&!known.has(reply.id));if(!validation)await pause(20);}check(validation&&!validation.error,'Actual uncached-copy source query did not bind successfully');const expanded=await waitPage(source.id);check(expanded.total==='600'&&expanded.columns===3,'Real source query did not publish the independent 600-row null/text fixture');await frames(4);
 const expandedSelection=await selectUsed(600),largeBaseline=expandedSelection.state,coverage=copyCacheCoverage(largeBaseline,600);check(largeBaseline.manual_cells==='1'&&largeBaseline.object_count==='0'&&BigInt(largeBaseline.source_revision)>BigInt(baseline.source_revision),'Source Apply lost the manual owner or did not produce a fresh source revision');const largeExpected=copyExpectedTSV(100),beforeCopyJobs=starts().length,previousCopyReply=replies().filter(reply=>reply.kind===9).at(-1)?.id;
 await copyMode('defer');const workerDelivery=await copyKey();let copyReply;for(const deadline=Date.now()+15000;Date.now()<deadline&&!copyReply;){copyReply=replies().filter(reply=>reply.kind===9&&(previousCopyReply===undefined||BigInt(reply.id)>BigInt(previousCopyReply))).at(-1);if(!copyReply)await pause(20);}check(copyReply&&!copyReply.error,'Uncached real C selection did not complete JOB_RANGE_COPY9');await frames(4);const workerPending=await platformSnapshot(),workerOwnership=copyState(workerPending),workerMemory=await copyMemory();copyRetained(largeBaseline,workerPending,'Real range-copy reply changed source/manual/selection/cache',{request:false});check(workerPending.request===copyReply.id&&workerOwnership.pending!=='0'&&workerOwnership.completed===recovery.ownership.completed&&workerOwnership.status==='0'&&workerMemory.attempts.at(-1).status==='pending'&&workerMemory.attempts.at(-1).text===largeExpected&&workerMemory.memory===expected,'Real Worker copy lost exact typed TSV, pending identity or retained clipboard');
 const copyJobs=starts().slice(beforeCopyJobs);check(copyJobs.length===1&&copyJobs[0].kind===9&&copyJobs[0].id===copyReply.id,'Uncached copy did not use exactly one real shared-C range-copy Worker job');await evaluate('duxcelTestClipboard.resolve('+workerMemory.attempts.at(-1).index+');true');const workerCompleted=await waitCopyCompleted(recovery.ownership.completed,1);check(workerCompleted.ownership.completed===workerOwnership.pending,'Worker clipboard acknowledgement adopted a different result');copyRetained(workerPending,workerCompleted.state,'Worker clipboard acknowledgement changed cached source/manual/selection');const finalMemory=await copyMemory();check(finalMemory.memory===largeExpected&&finalMemory.fallbacks.length===0,'Worker copy lost exact six-state TSV or attempted OS clipboard fallback');await capture('copy-worker-completed');
 const idleJobs=starts().length;await pause(300);await frames(3);const final=await platformSnapshot();copyRetained(workerCompleted.state,final,'Idle copied selection changed its retained source/manual/cache');check(starts().length===idleJobs,'Idle copied selection resubmitted database work');check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Copy acceptance staged a saved review report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Copy-only acceptance wrote into the persistent queue');
 const result={source,initial,manual,selected,expected,baseline,delivered,pending,pendingOwnership,unresolved,frozen,resolvedFrozen,fulfilled,failures,recoveryDelivery,recovery,recoveredMemory,draft,validation,expanded,expandedSelection,coverage,workerDelivery,copyReply,copyJobs,workerPending,workerOwnership,workerCompleted,finalMemory,final,scope:'Actual shared-C cached and uncached rectangular copy through trusted CtrlC into a pre-loader owned memory Promise writer. Exact NULL/zero-byte text/literal NULL/quoted text/apostrophe TSV, unresolved versus terminal ownership, Promise rejection/synchronous failure/unavailable writer and recovery. A captured pending review snapshot stays pinned across platform fulfillment and is adopted after normal cancellation. Real query Apply creates 600 rows; actual page coverage proves uncached cells and exactly one official Worker kind9 supplies complete TSV. Workbook/selection/statistics/cache and idle query ownership remain intact. No OS clipboard, native clipboard, physical typing, saved reports, Table/Pivot copy or export acceptance.'};await fs.writeFile(path.join(output,'copy.json'),JSON.stringify(result,null,2));return result;
}
async function cellEditorPick(text,name){
 const jobs=starts().length,captured=await beginReview(),before=capturedState(),editor=cellEditorState(before,text,{partial:true}),target={id:'author.cell_editor.input',bounds:editor.bounds};
 const surface=await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height};})()');[surface.logicalWidth,surface.logicalHeight]=before.logical.split(',').map(Number);
 const reachable=reachableGuidePoint(editor.bounds,surface),top=reachable.logical.y>surface.logicalHeight/2;if(top!==authorPickerTop){await key(' ','Space',32,' ');authorPickerTop=top;}await capture(name+'-picker');
 const after=consoleText('Review picked: ').length;await send('Input.dispatchMouseEvent',{type:'mouseMoved',...reachable.css});await send('Input.dispatchMouseEvent',{type:'mousePressed',...reachable.css,button:'left',clickCount:1});await frames();await send('Input.dispatchMouseEvent',{type:'mouseReleased',...reachable.css,button:'left',clickCount:1});await frames();const picked=await waitConsole('Review picked: ',after);
 check(picked==='Review picked: '+target.id,'Inline picker invented a panel or selected worksheet content instead of the sole real textbox');await capture(name+'-composer');await key('Escape','Escape',27);const restored=await platformSnapshot();cellEditorState(restored,text,{partial:true});cellModelRetained(before,restored,'Inline review cancellation changed the manual model/selection/source');
 check(authorOwnedFields.every(field=>before[field]===restored[field])&&Object.keys(before).filter(field=>field.startsWith('cell_editor_')).every(field=>before[field]===restored[field]),'Inline review cancellation mutated draft bytes, selection, horizontal scroll, owned histories or revisions');check(starts().length===jobs,'Inline review cancellation submitted SQL');return{captured,target,reachable,picked,before,after:restored};
}
async function browserReviewCell(){
 const initial=await platformSnapshot();check(initial.manual_cells==='0'&&initial.object_count==='0'&&initial.ui_scale==='1.000000'&&starts().length===0,'Inline-only profile requires an actual blank workbook');
 async function select(row,column){const before=await platformSnapshot(),[x,y,w,h]=before.body.split(',').map(Number),[cw,rh]=before.grid_cell.split(',').map(Number);await click(x+Math.min(cw,w)/2,y+Math.min(rh,h)/2);await sourceHome();for(let i=0;i<column;i++)await key('ArrowRight','ArrowRight',39);for(let i=0;i<row;i++)await key('ArrowDown','ArrowDown',40);const state=await platformSnapshot();check(state.active===row+','+column&&state.extent===state.active,'Real navigation did not reach the intended sparse cell');return state;}
 async function type(text){
  await evaluate('globalThis.duxcelCellKeys=[];globalThis.duxcelCellKeyListener=event=>globalThis.duxcelCellKeys.push({key:event.key,trusted:event.isTrusted,canvas:event.target===document.querySelector("canvas")});window.addEventListener("keydown",globalThis.duxcelCellKeyListener);true');
  for(const character of text){const letter=/^[a-z]$/i.test(character),digit=/^\d$/.test(character),code=letter?'Key'+character.toUpperCase():digit?'Digit'+character:character===' '?'Space':character==='"'?'Quote':'KeyI',virtual=letter?character.toUpperCase().charCodeAt(0):character==='"'?222:character.codePointAt(0);await key(character,code,virtual,character);}
  const delivered=await evaluate('window.removeEventListener("keydown",globalThis.duxcelCellKeyListener);globalThis.duxcelCellKeys');check(delivered.length===[...text].length&&delivered.every((event,index)=>event.trusted&&event.canvas&&event.key===[...text][index]),'Actual inline printable/UTF-8 keys did not reach the focused canvas in order');
  await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));await key('ArrowLeft','ArrowLeft',37,'',8);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));return delivered;
 }
 const cases=[];let priorCellsRevision;
 for(const specification of [{name:'cell-wide',column:1,text:'İstanbul "NULL" — a long draft with quoted "text" and retained UTF-8: ç',commit:false},{name:'cell-150',column:2,text:'NULL "literal NULL" — İstanbul remains text with quoted "values" and long UTF-8: ç',commit:true}]){
  if(specification.commit){await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('7','Digit7',55);await key('Home','Home',36);for(let i=0;i<3;i++)await key('ArrowDown','ArrowDown',40);await key('Enter','Enter',13);await send('Emulation.setDeviceMetricsOverride',{width:760,height:640,deviceScaleFactor:dpr,mobile:false});await frames(4);}
  const baseline=await select(2,specification.column);check(baseline.manual_cells==='0'&&baseline.ui_scale===(specification.commit?'1.500000':'1.000000'),'Cell fixture scale or missing-cell model changed');await key('F2','F2',113);const empty=await platformSnapshot(),emptyEditor=cellEditorState(empty,'');cellModelRetained(baseline,empty,'F2 fabricated a manual cell before editing');if(priorCellsRevision!==undefined)check(emptyEditor.origin[4]===priorCellsRevision,'Cancelled prior draft mutated worksheet cell revision');
  const delivered=await type(specification.text),draft=await platformSnapshot(),draftEditor=cellEditorState(draft,specification.text,{partial:true});cellModelRetained(baseline,draft,'Typing an uncommitted draft mutated worksheet cells');check(draftEditor.origin[4]===emptyEditor.origin[4],'Uncommitted draft changed worksheet revision');const pick=await cellEditorPick(specification.text,specification.name),beforeFinish=starts().length;
  await key(specification.commit?'Enter':'Escape',specification.commit?'Enter':'Escape',specification.commit?13:27);const finished=await platformSnapshot();check(finished.modal==='0'&&finished.manual_cells===(specification.commit?'1':'0'),'Normal editor completion did not commit one cell or retain sparse NULL');check(['search_cursor','search_anchor','search_revision','sheet','source_revision','schema','page','spare','selected_object','object_count'].every(field=>finished[field]===pick.after[field]),'Normal editor completion lost its retained draft selection or source/object cache');
  let committed,restored;
  if(specification.commit){await key('ArrowUp','ArrowUp',38);await key('F2','F2',113);committed=await platformSnapshot();const value=cellEditorState(committed,specification.text,{fromModel:true});check(BigInt(value.origin[4])===BigInt(emptyEditor.origin[4])+1n,'Real Enter did not make exactly one worksheet mutation');await key('Escape','Escape',27);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await key('z','KeyZ',90,'',2);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));await select(2,specification.column);await key('F2','F2',113);restored=await platformSnapshot();const missing=cellEditorState(restored,'');check(restored.manual_cells==='0'&&BigInt(missing.origin[4])===BigInt(emptyEditor.origin[4])+2n,'Normal Undo did not restore the sparse missing cell');await key('Escape','Escape',27);}
  else{cellModelRetained(baseline,finished,'Normal Escape changed the sparse worksheet');await key('F2','F2',113);restored=await platformSnapshot();const missing=cellEditorState(restored,'');check(restored.manual_cells==='0'&&missing.origin[4]===emptyEditor.origin[4],'Escape converted a missing cell to empty text');priorCellsRevision=missing.origin[4];await key('Escape','Escape',27);}
  const ordinaryJobs=starts().slice(beforeFinish);check(ordinaryJobs.every(job=>job.kind===10),'Ordinary commit/Undo submitted work beyond legitimate selection statistics');cases.push({specification,baseline,empty,delivered,draft,draftEditor,pick,finished,committed,restored,ordinaryJobs});
 }
 await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('7','Digit7',55);await key('Home','Home',36);await key('ArrowDown','ArrowDown',40);await key('Enter','Enter',13);await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:dpr,mobile:false});await frames(4);await pause(250);const final=await platformSnapshot();check(final.modal==='0'&&final.ui_scale==='1.000000'&&final.zoom===initial.zoom&&final.manual_cells==='0'&&final.object_count==='0'&&final.source_revision===initial.source_revision&&starts().every(job=>job.kind===10),'Inline review changed source/object ownership or failed scale/missing-cell restoration');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Inline review staged a saved report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Inline review wrote into the persistent queue');const result={initial,cases,final,scope:'Actual blank-cell F2/trusted printable UTF-8 and quote editing at100/150%, byte-based partial selection/horizontal scrolling and the sole textbox frozen target. Capture/composer cancellation retains draft, owned input histories, manual-cell revision and source/selection with no SQL. Normal Escape preserves sparse NULL; real Enter commits one quoted/literal-NULL text cell, normal Undo restores missing with exact worksheet revision changes. Intentional commit statistics jobs are separately recorded. No clipboard, Input.insertText assumption, saved reports or physical typing claim.'};await fs.writeFile(path.join(output,'review-cell.json'),JSON.stringify(result,null,2));return result;
}
function guideState(state){
 check(state.author_kind==='guide'&&state.author_parent==='author.guide'&&state.guide_kind==='guide-document'&&state.guide_origin==='0,0,0,0,0,0,0,0','Guide review fabricated a worksheet/data owner instead of its current document identity');
 const count=Number(state.guide_count),scroll=Number(state.guide_scroll),maximum=Number(state.guide_maximum),focus=Number(state.guide_focus);
 check(state.guide_overflow==='0'&&Number.isInteger(count)&&count>0&&count<=64&&Number.isFinite(scroll)&&Number.isFinite(maximum)&&maximum>=0&&scroll>=0&&scroll<=maximum+.02&&[0,1].includes(focus),'Guide capture lost bounded visible targets or actual scroll/focus');
 const rect=(name,required=false)=>{const raw=state['guide_'+name+'_rect'];if(raw===undefined){check(!required,'Guide omitted visible '+name+' bounds');return null;}const value=raw.split(',').map(Number);check(value.length===4&&value.every(Number.isFinite)&&value[2]>0&&value[3]>0,'Guide emitted invalid clipped '+name+' bounds');return value;};
 const panel=rect('panel',true),[width,height]=state.logical.split(',').map(Number),bounds={panel};
 check(panel[0]>=0&&panel[1]>=0&&panel[0]+panel[2]<=width+.01&&panel[1]+panel[3]<=height+.01,'Guide panel escaped the actual viewport');
 for(const name of ['heading','close','start','scrollbar','thumb','text','key']){const value=rect(name,name==='text');if(!value)continue;check(value[0]>=panel[0]-.01&&value[1]>=panel[1]-.01&&value[0]+value[2]<=panel[0]+panel[2]+.01&&value[1]+value[3]<=panel[1]+panel[3]+.01,'Guide '+name+' target escaped its actual panel clip');bounds[name]=value;}
 const decode=name=>{const hex=state['guide_'+name+'_hex'],truncated=state['guide_'+name+'_truncated'];check(/^(?:[0-9a-f]{2}){1,64}$/.test(hex??'')&&['0','1'].includes(truncated),'Guide lost its bounded '+name+' text/truncation');const value=Buffer.from(hex,'hex').toString('utf8');check(!value.includes('\ufffd'),'Guide split a captured UTF-8 codepoint');return value;};
 const text=decode('text'),key=bounds.key?decode('key'):null,textId=state.guide_text_id,keyId=state.guide_key_id;
 check(/^author\.guide\.(?:(?:intro|note)\.line\d+|section\.(?:grid|fields|panes|scaling|status)\.(?:name|description)\.line\d+|shortcut\.(?:selection|regions|filter|sheets|panes|focus)\.label\.line\d+)$/.test(textId??''),'Guide body line lost its actual semantic identity');
 const shortcut=keyId?.match(/^author\.guide\.shortcut\.(selection|regions|filter|sheets|panes|focus)\.key\.line0$/),keys={selection:'Arrows / Shift+Arrows',regions:'F6 / Shift+F6',filter:'Alt+↓',sheets:'Ctrl+Tab / Ctrl+Shift+Tab',panes:'⌘/Ctrl+Alt+T / F',focus:'⌘/Ctrl+Shift+F'};
 if(bounds.key)check(shortcut&&keys[shortcut[1]]===key&&state.guide_key_truncated==='0','Guide shortcut value differs from its actual key/semantic row');
 else check(['id','hex','truncated'].every(name=>state['guide_key_'+name]===undefined),'Guide fabricated a shortcut key outside the visible clip');
 if(bounds.thumb)check(bounds.scrollbar&&bounds.thumb[0]>=bounds.scrollbar[0]-.01&&bounds.thumb[1]>=bounds.scrollbar[1]-.01&&bounds.thumb[0]+bounds.thumb[2]<=bounds.scrollbar[0]+bounds.scrollbar[2]+.01&&bounds.thumb[1]+bounds.thumb[3]<=bounds.scrollbar[1]+bounds.scrollbar[3]+.01,'Guide thumb escaped its actual scrollbar track');
 return{state,count,scroll,maximum,focus,bounds,text,key,textId,keyId};
}
function guideUnderlying(before,after,message){
 const fields=[...authorOwnedFields.filter(name=>name!=='modal'),'selected_object','object_count'];
 check(fields.every(name=>before[name]!==undefined&&after[name]===before[name]),message);
}
function reachableGuidePoint(bounds,surface){
 const [x,y,width,height]=bounds;
 check(bounds.length===4&&bounds.every(Number.isFinite)&&width>0&&height>0&&['x','y','width','height','logicalWidth','logicalHeight'].every(name=>Number.isFinite(surface[name]))&&surface.width>0&&surface.height>0&&surface.logicalWidth>0&&surface.logicalHeight>0,'Guide pointer requires actual finite canvas/target bounds');
 const axis=(start,length,origin,painted,logical)=>{const scale=painted/logical,low=origin+start*scale,high=origin+(start+length)*scale,first=Math.ceil(low),last=Math.ceil(high)-1;
  check(first<=last,'Guide target has no reachable integer browser CSS pixel');const css=Math.max(first,Math.min(last,Math.round((low+high)/2))),value=(css-origin)/scale;
  check(value>=start&&value<start+length,'Guide CSS pixel mapped outside the captured target');return{css,logical:value};};
 const horizontal=axis(x,width,surface.x,surface.width,surface.logicalWidth),vertical=axis(y,height,surface.y,surface.height,surface.logicalHeight);
 return{css:{x:horizontal.css,y:vertical.css},logical:{x:horizontal.logical,y:vertical.logical}};
}
const guidePointerDeliveries=[];
async function guidePick(choose,name,{frozenName=null}={}){
 const jobs=starts().length,captured=await beginReview(),state=capturedState(),guide=guideState(state),target=choose(guide);
 check(target?.id&&target.bounds?.length===4,'Guide target is not actually visible: '+name);
 const surface=await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height};})()');[surface.logicalWidth,surface.logicalHeight]=state.logical.split(',').map(Number);
 // Mouse delivery may quantize fractional client coordinates. Pick a reachable
 // browser pixel inside the actual clipped target and send CSS integers once;
 // remapping its inverse through canvasPoint could round back outside a thin
 // transformed thumb. The C paint bounds and semantic target stay unchanged.
 const reachable=reachableGuidePoint(target.bounds,surface),point=reachable.logical,height=Number(state.logical.split(',')[1]),top=point.y>height/2;
 if(top!==authorPickerTop){await key(' ','Space',32,' ');authorPickerTop=top;}
 if(frozenName)await capture(frozenName+'-picker');const after=consoleText('Review picked: ').length;
 await evaluate('globalThis.duxcelGuideMouse=null;window.addEventListener("mousedown",event=>{globalThis.duxcelGuideMouse={trusted:event.isTrusted,canvas:event.target===document.querySelector("canvas"),button:event.button,x:event.clientX,y:event.clientY};},{once:true,capture:true});true');
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',...reachable.css});await send('Input.dispatchMouseEvent',{type:'mousePressed',...reachable.css,button:'left',clickCount:1});await frames();await send('Input.dispatchMouseEvent',{type:'mouseReleased',...reachable.css,button:'left',clickCount:1});await frames();
 const delivered=await evaluate('globalThis.duxcelGuideMouse'),mapped=delivered?{x:(delivered.x-surface.x)*surface.logicalWidth/surface.width,y:(delivered.y-surface.y)*surface.logicalHeight/surface.height}:null,delivery={name,target,surface,requested:reachable,delivered,mapped};guidePointerDeliveries.push(delivery);await fs.writeFile(path.join(output,'guide-pointer-delivery.json'),JSON.stringify(guidePointerDeliveries,null,2));
 const [x,y,w,h]=target.bounds;check(delivered?.trusted&&delivered.canvas&&delivered.button===0&&Number.isFinite(mapped?.x)&&Number.isFinite(mapped?.y)&&mapped.x>=x&&mapped.x<x+w&&mapped.y>=y&&mapped.y<y+h,'Delivered Guide pointer missed its actual clipped target; see guide-pointer-delivery.json');
 const picked=await waitConsole('Review picked: ',after);
 check(picked==='Review picked: '+target.id,'Frozen Guide picker chose covered content instead of '+target.id+': '+picked);await capture('guide-'+name+'-composer');await key('Escape','Escape',27);
 const restored=await platformSnapshot();guideState(restored);guideUnderlying(state,restored,'Guide composer cancellation changed workbook/cell/source ownership');
 check(state.modal===restored.modal&&state.author_kind===restored.author_kind&&Object.keys(state).filter(name=>name.startsWith('guide_')).every(name=>restored[name]===state[name]),'Guide composer cancellation changed visible content, scroll, focus or document origin');
 check(starts().length===jobs,'Guide reporting activated a database operation');return{captured,target,point,delivery,picked,before:state,after:restored};
}
async function browserReviewGuide(){
 const initial=await platformSnapshot();check(initial.modal==='0'&&initial.ui_scale==='1.000000'&&initial.manual_cells==='0'&&initial.object_count==='0'&&starts().length===0,'Guide-only profile requires the actual untouched blank workbook');
 async function openHelp(state){const width=Number(state.logical.split(',')[0]),scale=Number(state.ui_scale);await click(width-18*scale,42*scale);return guideState(await platformSnapshot());}
 const wide=await openHelp(initial);guideUnderlying(initial,wide.state,'Opening Guide changed the blank workbook');
 check(wide.maximum===0&&wide.scroll===0&&wide.focus===0&&wide.bounds.close&&wide.bounds.start&&wide.bounds.heading&&wide.bounds.key&&!wide.bounds.scrollbar,'Wide100 Guide lost its complete text/actions or invented scrolling');
 check(wide.textId==='author.guide.intro.line0'&&wide.text==='Explore the workspace, transitions, and interaction details.','Wide Guide did not expose its real introduction');const widePicks=[];
 for(const [name,choose,frozenName]of[
  ['wide-text',guide=>({id:guide.textId,bounds:guide.bounds.text}),'guide-wide'],
  ['wide-key',guide=>({id:guide.keyId,bounds:guide.bounds.key}),null],
  ['wide-close',guide=>({id:'author.guide.action.close',bounds:guide.bounds.close}),null],
  ['wide-start',guide=>({id:'author.guide.action.start',bounds:guide.bounds.start}),null],
  ['wide-backdrop',guide=>({id:'author.guide.backdrop.background',bounds:[1,Number(guide.state.logical.split(',')[1])/2,2,2]}),null]])widePicks.push(await guidePick(choose,name,{frozenName}));
 await key('Tab','Tab',9);const wideTab=guideState(await platformSnapshot());check(wideTab.focus===1&&wideTab.scroll===0,'Actual wide Guide Tab did not focus Start without scrolling');await key('Enter','Enter',13);const wideClosed=await platformSnapshot();guideUnderlying(initial,wideClosed,'Actual Guide Start dismissal changed the blank workbook');check(wideClosed.modal==='0','Actual focused Start did not dismiss Guide');
 await key('Enter','Enter',13);const reopened=guideState(await platformSnapshot());check(reopened.scroll===0&&reopened.focus===0,'Guide dismissal did not retain real Help keyboard focus for reopening');await key('Escape','Escape',27);
 // This is the same actual View→Interface scale command as the existing
 // narrow controls gate; canvas zoom stays independent.
 await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('7','Digit7',55);await key('Home','Home',36);for(let index=0;index<3;index++)await key('ArrowDown','ArrowDown',40);await key('Enter','Enter',13);
 await send('Emulation.setDeviceMetricsOverride',{width:640,height:640,deviceScaleFactor:dpr,mobile:false});await frames(4);const narrowBase=await platformSnapshot();
 check(narrowBase.ui_scale==='1.500000'&&narrowBase.zoom===initial.zoom&&narrowBase.manual_cells==='0'&&narrowBase.object_count==='0'&&narrowBase.source_revision===initial.source_revision&&narrowBase.request===initial.request,'Short150 setup changed canvas zoom, workbook ownership or query generation');
 const narrow=await openHelp(narrowBase);guideUnderlying(narrowBase,narrow.state,'Opening short150 Guide changed the blank workbook');check(narrow.maximum>0&&narrow.scroll===0&&narrow.focus===0&&narrow.bounds.close&&narrow.bounds.heading&&narrow.bounds.scrollbar&&narrow.bounds.thumb,'Short150 Guide did not expose its real clipped scrollable header');
 const narrowTop=[];for(const [name,choose,frozenName]of[
  ['short-text',guide=>({id:guide.textId,bounds:guide.bounds.text}),'guide-short-top'],
  ['short-close',guide=>({id:'author.guide.action.close',bounds:guide.bounds.close}),null]])narrowTop.push(await guidePick(choose,name,{frozenName}));
 await key('End','End',35);await key('Tab','Tab',9);const bottom=guideState(await platformSnapshot());
 check(bottom.scroll>0&&Math.abs(bottom.scroll-bottom.maximum)<.02&&bottom.focus===1&&bottom.bounds.start&&bottom.bounds.thumb&&bottom.bounds.key&&!bottom.bounds.heading&&!bottom.bounds.close,'Actual End/Tab did not expose Start at the clipped Guide footer with nonzero scroll');const narrowBottom=[];
 for(const [name,choose,frozenName]of[
  ['short-key',guide=>({id:guide.keyId,bounds:guide.bounds.key}),'guide-short-footer'],
  ['short-start',guide=>({id:'author.guide.action.start',bounds:guide.bounds.start}),null],
  ['short-thumb',guide=>({id:'author.guide.scrollbar.thumb',bounds:guide.bounds.thumb}),null],
  ['short-backdrop',guide=>({id:'author.guide.backdrop.background',bounds:[1,Number(guide.state.logical.split(',')[1])/2,2,2]}),null]]){const pick=await guidePick(choose,name,{frozenName});check(Number(pick.after.guide_scroll)>0&&pick.after.guide_focus==='1','Guide reporting lost nonzero footer scroll/focus');narrowBottom.push(pick);}
 await key('Escape','Escape',27);const escaped=await platformSnapshot();guideUnderlying(narrowBase,escaped,'Normal Escape from scrolled Guide changed the blank workbook');check(escaped.modal==='0','Normal Guide Escape left the modal open');
 await key('Enter','Enter',13);const reset=guideState(await platformSnapshot());check(reset.scroll===0&&reset.focus===0&&reset.maximum===narrow.maximum,'Help focus did not reopen a reset actual short Guide');await key('Tab','Tab',9);const start=guideState(await platformSnapshot());check(start.focus===1&&start.scroll>0&&start.bounds.start,'Actual Tab did not reveal the offscreen Start action');const [sx,sy,sw,sh]=start.bounds.start;await click(sx+sw/2,sy+sh/2);const startClosed=await platformSnapshot();guideUnderlying(narrowBase,startClosed,'Actual visible Start click changed the blank workbook');check(startClosed.modal==='0','Actual visible Start click did not dismiss Guide');
 await key('Alt','AltLeft',18);await key('w','KeyW',87);await key('z','KeyZ',90);await key('7','Digit7',55);await key('Home','Home',36);await key('ArrowDown','ArrowDown',40);await key('Enter','Enter',13);await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:dpr,mobile:false});await frames(4);await pause(250);const final=await platformSnapshot();
 check(final.modal==='0'&&final.ui_scale==='1.000000'&&final.zoom===initial.zoom&&['sheet','manual_cells','sheet_count','dirty','active','anchor','extent','source_revision','schema','page','spare','request','selected_object','object_count'].every(name=>final[name]===initial[name])&&starts().length===0&&replies().length===0,'Guide-only input/capture/idle changed the blank workbook or submitted database work');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Cancel-only Guide review staged a saved report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Guide review wrote into the persistent queue');
 const result={initial,wide:wide.state,widePicks,wideTab:wideTab.state,wideClosed,reopened:reopened.state,narrowBase,narrow:narrow.state,narrowTop,bottom:bottom.state,narrowBottom,escaped,reset:reset.state,start:start.state,startClosed,final,
  scope:'Actual blank-workbook Help/Guide at wide100 and640×640/150% interface scale, with real rendered introduction/shortcut values and document-only target identity. Frozen visible text/key/Close/Start/backdrop and scrollbar-thumb picking plus composer cancellation preserve actual geometry/content, nonzero footer scroll, focused Start and workbook ownership. Actual End/Tab, Start Enter/click, Escape, Help-focus reopen/reset, independent canvas zoom, restored scale and bounded idle all produce zero database jobs/replies and no saved reports. No physical gesture or browser Save acceptance.'};
 await fs.writeFile(path.join(output,'review-guide.json'),JSON.stringify(result,null,2));return result;
}
function statisticsReviewMenu(menu,expected){
 const state=menu.state,keys=['cells','filled','numeric','distinct','sum','average','minimum','maximum','done'];
 check(menu.rows.map(row=>row.key).join(',')===keys.join(',')&&menu.rows.every(row=>row.enabled),'Actual Statistics popup lost a metric checkbox or Done action');
 check(state.stats_menu_enabled==='1'&&state.stats_menu_preview==='0'&&state.stats_menu_frame==='0'&&state.stats_menu_object==='-1'&&state.stats_menu_state==='3'&&state.stats_menu_range==='0,5,2,2'&&state.stats_menu_flags==='0,0,0,0','Actual Statistics popup does not describe its current exact source range');
 const mask=Number(state.stats_menu_mask),focus=Number(state.stats_menu_focus);
 check(Number.isInteger(mask)&&Number.isInteger(focus)&&menu.rows.every((row,index)=>row.checked===(index<8&&!!(mask&(1<<index)))&&row.focused===(index===focus)),'Statistics checkbox/focus flags differ from the actual cached mask');
 statisticsCache(state,expected);return{mask,focus};
}
function inspectorReviewState(menu,{row,text,isNull,column=1,type='VARCHAR',field='Value'}){
 const state=menu.state;check(menu.rows.map(item=>item.key).join(',')==='copy,close'&&menu.rows[0].enabled===!isNull&&menu.rows[1].enabled&&menu.rows.every(item=>!item.checked&&!item.focused),'Inspector Copy availability or Close semantics do not match the actual NULL value');
 const decode=name=>{const value=state[name];check(value==='-'||/^(?:[0-9a-f]{2})+$/.test(value??''),'Missing bounded inspector '+name);return value==='-'?'':Buffer.from(value,'hex').toString('utf8');};
 check(state.inspector_null===String(Number(isNull))&&state.inspector_bytes===String(Buffer.byteLength(text))&&decode('inspector_text_hex')===text&&decode('inspector_type_hex')===type&&decode('inspector_field_hex')===field,'Inspector lost the exact SQL NULL/empty/literal source value or type');
 check(['inspector_origin_truncated','inspector_value_truncated','inspector_text_truncated','inspector_type_truncated','inspector_field_truncated','inspector_limited','inspector_origin_stale'].every(name=>state[name]==='0')&&state.inspector_origin_valid==='1','Inspector origin/value is stale, limited or truncated');
 const origin=state.inspector_origin?.split(',');check(origin?.join(',')===['1','0',String(Number(state.sheet)+1),'0','0','0',state.source_revision,String(row),String(column)].join(','),'Inspector pinned the wrong source cell/revision');
 const bounds=state.inspector_value_rect?.split(',').map(Number),panel=menu.panel;check(bounds?.length===4&&bounds.every(Number.isFinite)&&bounds[2]>0&&bounds[3]>0&&bounds[0]>=panel[0]&&bounds[1]>=panel[1]&&bounds[0]+bounds[2]<=panel[0]+panel[2]+.01&&bounds[1]+bounds[3]<=panel[1]+panel[3]+.01,'Inspector value target escaped its actual clipped panel');
 check(state.inspector_scroll==='0'&&Number(state.inspector_lines)>=0,'Short inspector value unexpectedly retained a scroll offset');return{origin,bounds,text,isNull};
}
async function browserReviewValues(){
 const source=await zoomSource({total:'6',columns:3}),initial=await platformSnapshot();check(initial.manual_cells==='0'&&initial.object_count==='0','Value review requires the unmodified imported NULL/text fixture');
 await selectAmount(6);const reply=await waitStatistics(),ready=await platformSnapshot(),expected={rows:6,counts:[6,6,6,1],values:[60,10,10,10],flags:'0,0,0,0',reply},cache=statisticsCache(ready,expected);
 let status=false;for(let index=0;index<8;index++){await key('F6','F6',117);const state=await platformSnapshot();if(state.focus?.split('/')[0]==='10'){status=true;break;}}
 check(status,'Real status focus traversal could not open Statistics');await key('Enter','Enter',13);
 const opened=authorState(await platformSnapshot(),'statistics'),original=statisticsReviewMenu(opened,expected),statisticsJobs=starts().length;
 await authorActivate('statistics','minimum');const checked=authorState(await platformSnapshot(),'statistics'),toggled=statisticsReviewMenu(checked,expected);
 check(toggled.mask===(original.mask^64)&&toggled.focus===6&&checked.rows[6].checked!==opened.rows[6].checked,'Actual minimum checkbox did not toggle without changing cached values');
 const statisticsPicks=[];
 for(const [name,choose,frozenName]of[
  ['values-statistics-sum',menu=>menu.rows.find(row=>row.key==='sum'),'values-statistics'],
  ['values-statistics-heading',menu=>({id:menu.state.author_parent+'.heading',bounds:menu.heading}),null],
  ['values-statistics-done',menu=>menu.rows.find(row=>row.key==='done'),null]]){
  const picked=await authorPick('statistics',choose,name,{frozenName});retainedValueReview(picked,'stats_');statisticsPicks.push(picked);
 }
 await authorActivate('statistics','minimum');const restored=authorState(await platformSnapshot(),'statistics');check(statisticsReviewMenu(restored,expected).mask===original.mask,'Actual checkbox restoration lost the original metric mask');
 check(starts().length===statisticsJobs,'Statistics checkbox/picker cancellation submitted additional SQL');await authorActivate('statistics','done');await key('Escape','Escape',27);await sourceHome();
 const cases=[];
 for(const specification of [{name:'sql-null',row:0,text:'',isNull:true},{name:'empty-varchar',row:2,text:'',isNull:false},{name:'literal-null',row:1,text:'NULL',isNull:false}]){
  await sourceHome();await key('ArrowRight','ArrowRight',39);for(let index=0;index<specification.row;index++)await key('ArrowDown','ArrowDown',40);const selected=await platformSnapshot();
  check(selected.active===specification.row+',1'&&selected.extent===selected.active,'Actual source row navigation did not select the intended Value cell');
  const [bx]=selected.body.split(',').map(Number),[cw]=selected.grid_cell.split(',').map(Number),[,gy]=selected.grid_rect.split(',').map(Number),scale=Number(selected.ui_scale);
  await rightClick(bx+1.5*cw-Number(selected.horizontal)*scale*Number(selected.zoom),gy+10*scale);await authorActivate('context','inspect');
  if(specification.name==='literal-null'){await send('Emulation.setDeviceMetricsOverride',{width:640,height:480,deviceScaleFactor:dpr,mobile:false});await frames(4);}
  const inspector=authorState(await platformSnapshot(),'cell_inspector'),value=inspectorReviewState(inspector,specification),picks=[],jobs=starts().length;await capture('values-inspector-'+specification.name);
  const valuePick=await authorPick('cell_inspector',menu=>({id:'author.cell_inspector.value',bounds:menu.state.inspector_value_rect.split(',').map(Number)}),'values-'+specification.name,{frozenName:specification.isNull?'values-inspector':null});retainedValueReview(valuePick,'inspector_');picks.push(valuePick);
  if(specification.name==='literal-null')for(const [name,choose]of[
   ['values-inspector-heading',menu=>({id:menu.state.author_parent+'.heading',bounds:menu.heading})],
   ['values-inspector-backdrop',menu=>({id:menu.state.author_parent+'.backdrop.background',bounds:[1,Number(menu.state.logical.split(',')[1])/2,2,2]} )],
   ['values-inspector-close',menu=>menu.rows.find(row=>row.key==='close')]]){
   const pick=await authorPick('cell_inspector',choose,name);retainedValueReview(pick,'inspector_');picks.push(pick);
  }
  const retained=authorState(await platformSnapshot(),'cell_inspector');inspectorReviewState(retained,specification);check(starts().length===jobs,'Cell Inspector review/picker cancellation submitted database work');
  await authorActivate('cell_inspector','close');await sourceHome();const returned=await platformSnapshot();
  check(returned.modal==='0'&&returned.active==='0,0'&&returned.extent==='0,0'&&returned.selected_object==='-1'&&['sheet','source_revision','schema','page','manual_cells','object_count'].every(name=>returned[name]===initial[name]),'Closing Inspector lost source ownership or fabricated worksheet values');
  // Header inspection selects a whole column. Its ordinary Close may schedule
  // legitimate range statistics before Ctrl+Home clears it; report cancellation
  // above must not schedule anything. Keep these operations explicitly separate.
  const closeJobs=starts().slice(jobs);check(closeJobs.every(job=>job.kind===10),'Ordinary Inspector Close scheduled work beyond existing selection statistics');
  cases.push({specification,selected,opened:inspector.state,value,picks,retained:retained.state,returned,ordinary_close_jobs:closeJobs});
 }
 await send('Emulation.setDeviceMetricsOverride',{width:1280,height:820,deviceScaleFactor:dpr,mobile:false});await frames(4);const idleJobs=starts().length;await pause(300);await frames(3);const final=await platformSnapshot();
 check(final.modal==='0'&&final.stats_state==='0'&&final.stats_request==='0'&&starts().length===idleJobs,'Idle restored source retained a stale inspector/statistics request or resubmitted SQL');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Value review unexpectedly staged a saved report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Value review wrote into the persistent queue');
 const result={source,initial,reply,ready,cache,statistics:{opened:opened.state,original,checked:checked.state,toggled,picks:statisticsPicks,restored:restored.state},cases,final,
  scope:'Actual imported null-text.csv through the shared C Open control and official Worker; exact cached Statistics values, real metric checkbox toggling and frozen Sum/heading/Done picking. Visible column-header Inspect value opens owned SQL NULL, zero-byte VARCHAR and literal NULL inspectors with exact type/text and pinned source cell/revision, including narrow-window clipping. Value/heading/backdrop/Close picking and composer cancellation preserve all cached state without SQL. Copy is never activated; ordinary header-inspector Close statistics are recorded separately. No clipboard, saved reports, mixed-range or forced pending-query acceptance.'};
 await fs.writeFile(path.join(output,'review-values.json'),JSON.stringify(result,null,2));return result;
}
async function focusTransformations(open=true){
 if(open)await ribbonAnalysisAction('t');let focused=false;
 for(let index=0;index<16;index++){await key('F6','F6',117);const state=await platformSnapshot();if(state.focus?.split('/')[0]==='8'){focused=true;break;}}
 check(focused,'Real focus traversal did not reach Transformations');await key('Home','Home',36);
}
function generatedSQLState(menu,{cached=false}={}){
 const state=menu.state,model=analysisObject(state);
 check(menu.rows.map(row=>row.key).join(',')==='close,copy'&&menu.rows.every(row=>row.enabled&&!row.checked&&!row.focused),'Generated SQL lost its actual Close/Copy button semantics');
 check(state.sql_origin_valid==='1'&&state.sql_origin_stale==='0'&&state.sql_value_truncated==='0','Generated SQL has an unknown, stale or truncated owned definition');
 const origin=state.sql_origin?.split(','),expected=[String(Number(state.sheet)+1),cached?model.object[0]:'0','0',cached?state.source_revision:'0',cached?state.object_revision:state.source_revision];
 check(origin?.join(',')===expected.join(','),'Generated SQL pinned the selected object instead of its actual '+(cached?'cached result':'primary source')+' origin');
 const bytes=Number(state.sql_bytes),lines=Number(state.sql_lines),visible=Number(state.sql_visible),scroll=Number(state.sql_scroll),hex=state.sql_text_hex;
 check(Number.isInteger(bytes)&&bytes>64&&bytes<1024&&Number.isInteger(lines)&&lines>0&&Number.isInteger(visible)&&visible>0&&Number.isInteger(scroll)&&scroll>=0&&scroll<=Math.max(0,lines-visible),'Generated SQL emitted invalid bounded text/line/scroll metadata');
 check(/^(?:[0-9a-f]{2}){64}$/.test(hex??'')&&state.sql_text_truncated==='1','Generated SQL lost its explicitly bounded 64-byte prefix');
 const text=Buffer.from(hex,'hex').toString('utf8');
 check(cached?/^SELECT \* FROM \(PIVOT \(/.test(text):text.startsWith('-- Current source result\nWITH input AS (\n'),'Generated SQL content does not belong to its '+(cached?'real cached PIVOT':'primary source query')+' controller');
 const bounds=state.sql_content_rect?.split(',').map(Number),panel=menu.panel;
 check(bounds?.length===4&&bounds.every(Number.isFinite)&&bounds[2]>0&&bounds[3]>0&&bounds[0]>=panel[0]&&bounds[1]>=panel[1]&&bounds[0]+bounds[2]<=panel[0]+panel[2]+.01&&bounds[1]+bounds[3]<=panel[1]+panel[3]+.01,'Generated SQL content escaped its actual clipped panel');
 return{origin,bytes,lines,visible,scroll,text,bounds,cached};
}
async function generatedSQLWheel(state,deltaY){
 const [x,y,w,h]=state.sql_content_rect.split(',').map(Number),point=await canvasPoint(x+w/2,y+h/2);
 await evaluate('globalThis.duxcelSQLWheelEvent=null;window.addEventListener("wheel",event=>{globalThis.duxcelSQLWheelEvent={trusted:event.isTrusted,canvas:event.target===document.querySelector("canvas"),deltaY:event.deltaY};},{once:true,capture:true});true');
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',...point});await send('Input.dispatchMouseEvent',{type:'mouseWheel',...point,deltaX:0,deltaY});await frames(3);
 const delivered=await evaluate('globalThis.duxcelSQLWheelEvent');
 await fs.writeFile(path.join(output,'generated-sql-wheel-delivery.json'),JSON.stringify({deltaY,point,delivered},null,2));
 // Chromium/macOS can scale protocol wheel deltas at Retina density (120
 // protocol units arrive as60 DOM pixels at DPR2). Check real delivery and
 // direction here; the actual C scroll/clamp assertions below verify use.
 check(delivered?.trusted&&delivered.canvas&&Number.isFinite(delivered.deltaY)&&Math.sign(delivered.deltaY)===Math.sign(deltaY),'Generated SQL wheel did not reach the actual canvas through trusted CDP input');
 return{deltaY,point,delivered,state:await platformSnapshot()};
}
async function browserGeneratedSQL(foundation){
 const {published,source,reply:originalPivot}=foundation;
 await focusTransformations();
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));await key('F10','F10',121,'',8);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));await authorActivate('step','edit');
 const editor=authorState(await platformSnapshot(),'source_query');
 check(editor.state.query_object==='-1'&&editor.state.query_source_revision===published.source_revision&&editor.state.selected_object==='0','Long SQL editor did not retain primary ownership while the real Rows pivot stayed selected');
 // Internal whitespace survives the real query parser without changing data.
 // Trusted Enter keys produce enough genuine SQL lines to exercise scrolling.
 const text='SELECT *'+'\n'.repeat(42)+'FROM input',draft=await sourceQueryDraft(text),jobsBeforeApply=starts().length,known=new Set(replies().map(reply=>reply.id));
 await authorActivate('source_query','apply');let validation;
 for(const deadline=Date.now()+15000;Date.now()<deadline;){validation=replies().find(value=>value.kind===11&&!known.has(value.id));if(validation)break;await pause(20);}
 check(validation&&!validation.error,'Real multiline source Apply did not complete successful Worker validation');
 const sourcePage=await waitPage(source.id),pivot=await waitPivot(originalPivot.id);await frames(4);const applied=await platformSnapshot(),model=analysisObject(applied);
 check(sourcePage.rows===5&&sourcePage.columns===6&&sourcePage.total==='5'&&pivot.rows===4&&pivot.columns===2&&pivot.total==='4'&&!pivot.pending,'Multiline source query did not republish its actual five source/four City rows');
 check(Number(applied.source_revision)>Number(published.source_revision)&&applied.object_revision===published.object_revision&&model.object.join(',')===published.object&&model.page.join(',')==='0,4,2,4,0,0'&&applied.pivot_counts==='1,0,1,0','Source Apply lost the selected same-sheet object identity, revision, shelves or actual matrix');
 check(applied.modal==='0'&&applied.busy==='0'&&applied.io_busy==='0'&&applied.page==='0/5'&&applied.manual_cells==='1'&&applied.object_grand0_null==='0'&&applied.object_grand0_truncated==='0'&&Number(Buffer.from(applied.object_grand0_hex,'hex').toString('utf8'))===145.25,'Source Apply changed sparse data/grand totals or left a pending source job');
 const applyJobs=starts().slice(jobsBeforeApply);check(applyJobs.some(job=>job.kind===11&&job.id===validation.id)&&applyJobs.some(job=>job.kind===0&&job.id===sourcePage.id)&&applyJobs.some(job=>job.kind===5&&job.id===pivot.id)&&applyJobs.every(job=>[0,5,8,11].includes(job.kind)),'Real source Apply used an unexpected operation instead of validation/source/result publication');
 const jobs=starts().length;
 // Enter on the visible Source step invokes inspect_source_sql. The existing
 // pane remains open after Apply, so do not toggle Transformations closed.
 await focusTransformations(false);await key('Enter','Enter',13);
 const primary=authorState(await platformSnapshot(),'generated_sql'),primaryValue=generatedSQLState(primary);retainedAnalysisCache(applied,primary.state,'Opening primary Generated SQL replaced the actual matrix/source/manual cache');
 check(primaryValue.scroll===0&&primaryValue.lines>primaryValue.visible&&primaryValue.lines>=43,'Applied multiline primary SQL did not create a legitimately scrollable definition');
 const moved=await generatedSQLWheel(primary.state,120),scrolled=authorState(moved.state,'generated_sql'),scrollValue=generatedSQLState(scrolled);retainedAnalysisCache(applied,moved.state,'SQL scrolling changed the analytical/source cache');
 check(scrollValue.scroll>0,'Trusted wheel did not move the actual long Generated SQL content');await capture('generated-sql-primary-scrolled');
 const primaryPicks=[];
 for(const [name,choose,frozenName]of[
  ['sql-primary-content',menu=>({id:'author.generated_sql.content',bounds:menu.state.sql_content_rect.split(',').map(Number)}),'generated-sql-primary'],
  ['sql-primary-copy',menu=>menu.rows.find(row=>row.key==='copy'),null],
  ['sql-primary-close',menu=>menu.rows.find(row=>row.key==='close'),null],
  ['sql-primary-backdrop',menu=>({id:menu.state.author_parent+'.backdrop.background',bounds:[1,Number(menu.state.logical.split(',')[1])/2,2,2]}),null]]){
  const pick=await authorPick('generated_sql',choose,name,{frozenName});retainedValueReview(pick,'sql_');retainedAnalysisCache(pick.before,pick.after,'Primary SQL picker cancellation changed its selected matrix/source cache');
  check(generatedSQLState(authorState(pick.after,'generated_sql')).scroll===scrollValue.scroll,'Report cancellation lost the actual nonzero SQL scroll');primaryPicks.push(pick);
 }
 const end=await generatedSQLWheel(moved.state,100000),endValue=generatedSQLState(authorState(end.state,'generated_sql'));
 check(endValue.scroll===endValue.lines-endValue.visible,'Generated SQL wheel did not clamp at its actual final visible line');
 const start=await generatedSQLWheel(end.state,-100000),startValue=generatedSQLState(authorState(start.state,'generated_sql'));check(startValue.scroll===0,'Generated SQL reverse wheel did not return to line zero');
 for(const wheel of [end,start]){retainedAnalysisCache(applied,wheel.state,'SQL wheel clamp changed source/matrix ownership');check(['sql_bytes','sql_lines','sql_visible','sql_origin','sql_text_hex','sql_text_truncated','sql_value_truncated'].every(field=>wheel.state[field]===primary.state[field]),'SQL wheel clamp mutated its owned definition or pinned origin');}
 await authorActivate('generated_sql','close');const sourceClosed=await platformSnapshot();retainedAnalysisCache(applied,sourceClosed,'Actual source SQL Close lost matrix/source ownership');check(sourceClosed.modal==='0','Actual SQL Close did not dismiss the source viewer');
 // The selected matrix card is the ordinary object context surface. Its
 // Inspect SQL action uses the complete cached Worker result definition.
 const [mx,my,mw]=sourceClosed.matrix_rect.split(',').map(Number),scale=Number(sourceClosed.ui_scale);await rightClick(mx+Math.min(40*scale,mw/4),my+15*scale);await authorActivate('object','inspect_sql');
 const cached=authorState(await platformSnapshot(),'generated_sql'),cachedValue=generatedSQLState(cached,{cached:true});retainedAnalysisCache(applied,cached.state,'Opening cached pivot SQL replaced matrix/source ownership');
 check(cachedValue.scroll===0&&cached.state.sql_text_hex!==primary.state.sql_text_hex&&cached.state.sql_origin!==primary.state.sql_origin,'Direct pivot SQL reused the primary definition, origin or prior scroll');await capture('generated-sql-cached-pivot');
 const cachedPicks=[];
 for(const [name,choose,frozenName]of[
  ['sql-cached-content',menu=>({id:'author.generated_sql.content',bounds:menu.state.sql_content_rect.split(',').map(Number)}),'generated-sql-cached'],
  ['sql-cached-copy',menu=>menu.rows.find(row=>row.key==='copy'),null],
  ['sql-cached-close',menu=>menu.rows.find(row=>row.key==='close'),null],
  ['sql-cached-backdrop',menu=>({id:menu.state.author_parent+'.backdrop.background',bounds:[1,Number(menu.state.logical.split(',')[1])/2,2,2]}),null]]){
  const pick=await authorPick('generated_sql',choose,name,{frozenName});retainedValueReview(pick,'sql_');retainedAnalysisCache(pick.before,pick.after,'Cached SQL picker cancellation changed matrix/source ownership');generatedSQLState(authorState(pick.after,'generated_sql'),{cached:true});cachedPicks.push(pick);
 }
 await key('Escape','Escape',27);const closed=await platformSnapshot();retainedAnalysisCache(applied,closed,'Actual Escape from cached SQL lost the selected matrix/source');check(closed.modal==='0'&&starts().length===jobs,'SQL inspection, wheel or reporting activated Copy/SQL work');
 await pause(300);await frames(3);const idle=await platformSnapshot();retainedAnalysisCache(applied,idle,'Idle after Generated SQL inspection replaced analytical/source caches');check(starts().length===jobs,'Idle SQL inspection resubmitted database work');
 await ribbonAnalysisAction('d');await sourceHome();for(let index=0;index<7;index++)await key('ArrowRight','ArrowRight',39);for(let index=0;index<9;index++)await key('ArrowDown','ArrowDown',40);const manual=await platformSnapshot();
 check(manual.modal==='0'&&manual.selected_object==='-1'&&manual.active==='9,7'&&manual.extent==='9,7'&&manual.manual_cells==='1'&&manual.object_count==='1'&&['sheet','source_revision','schema','page'].every(field=>manual[field]===applied[field])&&starts().length===jobs,'Returning to source after SQL review lost sparse H10 data or scheduled database work');
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Cancel-only Generated SQL review staged a saved report');const queue=await(await fetch(new URL('/__review/queue',url))).json();check(queue.reports.length===0,'Generated SQL review wrote into the persistent queue');
 const result={foundation,editor:editor.state,text,draft,validation,sourcePage,pivot,applied,applyJobs,primary:primary.state,primaryValue,moved,scrollValue,primaryPicks,end,start,sourceClosed,cached:cached.state,cachedValue,cachedPicks,closed,idle,manual,
  scope:'Actual Rows City/SUM Amount selected while Transformations edits/applies a primary source query with 42 internal newlines, real Worker validation and fresh five-row source/four-row PIVOT publication. Transformations Source opens primary Generated SQL with a pinned primary origin; direct matrix Inspect SQL opens the cached PIVOT with independent object/source revision identity. Trusted CDP wheel produces nonzero SQL scroll, retains it through content/Copy/Close/backdrop picking and composer cancellation, then clamps at both ends. Actual Close/Escape, cached source/matrix/footer/manual ownership and idle query counts remain intact. Copy is never activated; no clipboard, saved reports, forced in-flight cancellation or physical gesture acceptance.'};
 await fs.writeFile(path.join(output,'review-sql.json'),JSON.stringify(result,null,2));return result;
}
const queryOwnedFields=['query_object','query_source_revision','query_draft_revision','query_focus','query_loading','query_cursor','query_anchor','query_scroll','selected_object','object_count','object','object_page','pivot_counts'];
async function sourceQueryDraft(text){
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await key('a','KeyA',65,'',2);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));
 for(const character of text){
  if(character==='\n')await key('Enter','Enter',13);
  else if(character===' ')await key(' ','Space',32,' ');
  else if(character===',')await key(',','Comma',188,',');
  else if(character==='(')await key('(','Digit9',57,'(');
  else if(character===')')await key(')','Digit0',48,')');
  else if(/^\d$/.test(character))await key(character,'Digit'+character,character.charCodeAt(0),character);
  else if(character==='_')await key('_','Minus',189,'_');
  else if(character==='*')await key('*','Digit8',56,'*');
  else if(character==='+')await key('+','Equal',187,'+');
  else if(character==='>')await key('>','Period',190,'>');
  else if(character==='.')await key('.','Period',190,'.');
  else await key(character,'Key'+character.toUpperCase(),character.toUpperCase().charCodeAt(0),character);
 }
 await key('End','End',35);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));await key('ArrowLeft','ArrowLeft',37,'',8);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));
 const state=await platformSnapshot();
 check(state.query_cursor===String(text.length-1)&&state.query_anchor===String(text.length),'Real multiline SQL editing did not retain its partial text selection');return state;
}
function sourceQueryFeedback(menu,{error=false}={}){
 const state=menu.state,panel=menu.panel,rect=name=>{const value=state[name]?.split(',').map(Number);check(value?.length===4&&value.every(Number.isFinite)&&value[2]>0&&value[3]>0&&value[0]>=panel[0]-.01&&value[1]>=panel[1]-.01&&value[0]+value[2]<=panel[0]+panel[2]+.01&&value[1]+value[3]<=panel[1]+panel[3]+.01,'Query feedback lost its actual visible '+name+' clip');return value;};
 check(state.query_object==='-1'&&state.selected_object==='0'&&state.object_count==='1'&&state.query_source_revision===state.source_revision&&state.query_loading==='0'&&state.query_status_loading==='0'&&state.busy==='0'&&state.io_busy==='0','Query feedback lost the idle pinned primary source while the same-sheet pivot is selected');
 const status=rect('query_status_rect'),expected='Apply clears source filters, sort and summary in one undoable action.',statusBytes=Buffer.from(expected);
 check(state.query_status_hex===statusBytes.subarray(0,64).toString('hex')&&state.query_status_truncated===String(Number(statusBytes.length>64))&&state.query_status_value_truncated==='0','Query status is not the actual complete owned idle VARCHAR feedback');
 if(!error){check(!Object.keys(state).some(name=>name.startsWith('query_error_')),'A fresh source-query editor retained a stale validation error');return{status};}
 const bounds=rect('query_error_rect'),bytes=Number(state.query_error_bytes),lines=Number(state.query_error_lines),scroll=Number(state.query_error_scroll),visible=Math.max(1,Math.floor(bounds[3]/(21*Number(state.ui_scale)))),maximum=Math.max(0,lines-visible),hex=state.query_error_hex;
 check(Number.isInteger(bytes)&&bytes>64&&bytes<1024&&Number.isInteger(lines)&&lines>=2&&Number.isInteger(scroll)&&scroll>=0&&scroll<=maximum&&state.query_error_limited==='0'&&state.query_error_truncated==='1'&&state.query_error_value_truncated==='0'&&/^(?:[0-9a-f]{2}){64}$/.test(hex??''),'Validation feedback lost its owned bounded message, actual TextLines cache or scroll');
 const prefix=Buffer.from(hex,'hex').toString('utf8');check(prefix.startsWith('Binder Error:')&&prefix.includes('duxcel_missing_review_column')&&!prefix.includes('\ufffd'),'Validation feedback is not the real missing-column engine error');
 return{status,error:bounds,bytes,lines,scroll,visible,maximum,prefix};
}
async function browserQueryFeedback(){
 const baseline=await platformSnapshot(),beforeJobs=starts().length;check(baseline.modal==='0'&&baseline.selected_object==='0'&&baseline.object_count==='1','Query feedback must follow the retained same-sheet pivot/source flow');
 await focusTransformations(false);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));try{await key('F10','F10',121,'',8);}finally{await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));}await authorActivate('step','edit');const opened=authorState(await platformSnapshot(),'source_query');sourceQueryFeedback(opened);
 const text='SELECT duxcel_missing_review_column,\n       City\nFROM input',draft=await sourceQueryDraft(text),known=new Set(replies().map(reply=>reply.id));await authorActivate('source_query','apply');let validation;
 for(const deadline=Date.now()+15000;Date.now()<deadline;){validation=replies().find(reply=>reply.kind===11&&!known.has(reply.id));if(validation)break;await pause(20);}check(validation&&validation.error,'Real missing-column source draft did not fail official Worker validation');await frames(4);const failed=authorState(await platformSnapshot(),'source_query'),feedback=sourceQueryFeedback(failed,{error:true});
 check(failed.state.request===validation.id&&failed.state.query_cursor===draft.query_cursor&&failed.state.query_anchor===draft.query_anchor&&failed.state.query_draft_revision===draft.query_draft_revision,'Failed validation replaced the real draft revision or partial text selection');
 const retained=['sheet','page','spare','source_revision','schema','manual_cells','dirty','active','anchor','extent','grid_selection','selected_object','object_count','object','object_page','pivot_counts'];check(retained.every(name=>baseline[name]!==undefined&&failed.state[name]===baseline[name]),'Failed validation activated a source query or replaced the selected pivot/model/pages');
 const submitted=starts().slice(beforeJobs);check(submitted.length===1&&submitted[0].kind===11&&submitted[0].id===validation.id,'Missing-column feedback submitted work beyond exactly one real validation job');await capture('source-query-validation-error');
 const [x,y,w,h]=feedback.error,point=await canvasPoint(x+w/2,y+h/2);await evaluate('globalThis.duxcelQueryErrorWheel=null;window.addEventListener("wheel",event=>{duxcelQueryErrorWheel={trusted:event.isTrusted,canvas:event.target===document.querySelector("canvas"),deltaY:event.deltaY};},{once:true,capture:true});true');await send('Input.dispatchMouseEvent',{type:'mouseMoved',...point});await send('Input.dispatchMouseEvent',{type:'mouseWheel',...point,deltaX:0,deltaY:10000});await frames(3);
 const delivered=await evaluate('duxcelQueryErrorWheel'),scrolled=authorState(await platformSnapshot(),'source_query'),scrolledFeedback=sourceQueryFeedback(scrolled,{error:true});check(delivered?.trusted&&delivered.canvas&&Number.isFinite(delivered.deltaY)&&delivered.deltaY>0,'Actual validation-error wheel did not reach its focused canvas region');check(scrolledFeedback.scroll===feedback.maximum,'Real validation-error wheel did not clamp to the actual TextLines extent');
 check([...queryOwnedFields,...Object.keys(failed.state).filter(name=>name.startsWith('query_status_')||name.startsWith('query_error_')&&name!=='query_error_scroll')].every(name=>failed.state[name]===scrolled.state[name]),'Validation-error scrolling changed draft, owner, message or cached line geometry');const jobs=starts().length,picks=[];
 for(const [name,key,id]of [['query-status','query_status_rect','author.source_query.status'],['query-validation-error','query_error_rect','author.source_query.validation_error']]){
  const picked=await authorPick('source_query',menu=>({id,bounds:menu.state[key].split(',').map(Number)}),name);sourceQueryFeedback(authorState(picked.before,'source_query'),{error:true});sourceQueryFeedback(authorState(picked.after,'source_query'),{error:true});retainedValueReview(picked,'query_');check(retained.every(field=>picked.before[field]===picked.after[field]),'Feedback picking changed the primary source or selected pivot/cache');picks.push(picked);
 }
 const beforeEscape=authorState(await platformSnapshot(),'source_query');sourceQueryFeedback(beforeEscape,{error:true});await key('Escape','Escape',27);const closed=await platformSnapshot();check(closed.modal==='0'&&[...retained,'request','busy','io_busy','search_cursor','search_anchor','search_revision'].every(name=>beforeEscape.state[name]!==undefined&&beforeEscape.state[name]===closed[name]),'Normal Escape from invalid query changed its retained input revision/selection, source/model/pages or request');check(starts().length===jobs&&jobs===beforeJobs+1,'Feedback scrolling/picking/cancel/Escape submitted additional validation or source work');await capture('source-query-feedback-closed');
 return{baseline,opened:opened.state,text,draft,validation,failed:failed.state,feedback,submitted,point,delivered,scrolled:scrolled.state,scrolledFeedback,picks,beforeEscape:beforeEscape.state,closed,scope:'One actual missing-column kind11 validation after the original query-review flow. Real idle status and Binder error TextLines/scroll are picked through their pinned primary-query descendants while the same-sheet pivot remains selected. Trusted error wheel clamps to the actual visible line extent, which may legitimately be zero. Capture/composer cancellation and normal Escape retain observed draft revision/selection, feedback, source/model/pages and request ownership with no extra jobs. Full TextInput history storage and pending validation remain separately native accepted; no simulated callback or report upload.'};
}
async function browserQueryReview(){
 const source=await zoomSource();await sourceHome();const initial=await platformSnapshot(),[x,y]=initial.body.split(',').map(Number),[width,height]=initial.grid_cell.split(',').map(Number);
 await rightClick(x+width/2,y+height/2);await authorActivate('source_context','create_analysis');
 const selected=await platformSnapshot();analysisObject(selected);check(selected.pivot_counts==='0,0,0,0','Query review requires a real empty analytical object');
 // The shared C Transformations source-step action edits the primary source
 // while retaining the selected pivot. No test-only controller command exists.
 await focusTransformations();
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Shift','ShiftLeft',16));await key('F10','F10',121,'',8);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Shift','ShiftLeft',16));await authorActivate('step','edit');
 const opened=authorState(await platformSnapshot(),'source_query');
 check(opened.rows.map(row=>row.key).join(',')==='sql,original,cancel,apply'&&opened.rows.every(row=>row.enabled),'Actual source-query editor omitted an editable SQL/action target');
 check(opened.state.query_object==='-1'&&opened.state.selected_object==='0'&&opened.state.query_source_revision===selected.source_revision&&opened.state.query_loading==='0','Source-query owner was confused with the selected analytical object');
 const text='SELECT City, Amount FROM input\nWHERE Approved',draft=await sourceQueryDraft(text),jobs=starts().length,picks=[];
 for(const [name,choose,frozenName]of[
  ['query-sql',menu=>menu.rows.find(row=>row.key==='sql'),'source-query'],
  ['query-heading',menu=>({id:menu.state.author_parent+'.heading',bounds:menu.heading}),null],
  ['query-backdrop',menu=>({id:menu.state.author_parent+'.backdrop.background',bounds:[1,Number(menu.state.logical.split(',')[1])/2,2,2]}),null],
  ['query-apply',menu=>menu.rows.find(row=>row.key==='apply'),null]]){
  const picked=await authorPick('source_query',choose,name,{frozenName});
  check(queryOwnedFields.every(field=>picked.before[field]!==undefined&&picked.before[field]===picked.after[field]),'Review '+name+' changed the actual SQL owner, draft, editor or selected-object state');picks.push(picked);
 }
 check(starts().length===jobs,'Review-only SQL picking activated validation or another database job');
 let applied,cancelled;
 if(queryApply){
  const known=new Set(replies().map(reply=>reply.id));await authorActivate('source_query','apply');let validation;
  for(const deadline=Date.now()+15000;Date.now()<deadline;){validation=replies().find(reply=>reply.kind===11&&!known.has(reply.id));if(validation)break;await pause(20);}
  check(validation&&!validation.error,'Real source-query Apply did not complete successful Worker validation');
  const page=await waitPage(source.id);await frames(4);const state=await platformSnapshot();
  check(page.rows===3&&page.columns===2&&page.total==='3'&&state.modal==='0','Actual retained SQL draft did not publish the three approved City/Amount rows');
  check(state.selected_object==='0'&&state.object_count==='1'&&state.sheet===selected.sheet&&Number(state.source_revision)>Number(selected.source_revision),'Applied primary source query lost the selected same-sheet pivot or source revision');
  await capture('source-query-applied');applied={validation,page,state};
 }else{
  await authorActivate('source_query','cancel');cancelled=await platformSnapshot();
  check(cancelled.modal==='0'&&['sheet','page','source_revision','schema','manual_cells','dirty','active','anchor','extent','selected_object','object_count','object','object_page','pivot_counts'].every(field=>cancelled[field]===draft[field]),'Actual editor Cancel changed the source or selected analytical object');
  check(starts().length===jobs,'Actual editor Cancel submitted a database job');await capture('source-query-cancelled');
 }
 check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Source-query review unexpectedly staged a report');
 const feedback=await browserQueryFeedback();check(consoleText('Browser review prepared: ').length===0&&(await pendingReports()).length===0,'Query feedback review staged a saved report');
 const result={source,selected,opened:opened.state,text,draft,picks,applied,cancelled,feedback,
  scope:'Actual selected-pivot Transformations source-step Edit, primary source ownership and multiline draft/cursor/anchor/scroll retained through frozen SQL/heading/backdrop/Apply picking with zero review-triggered jobs; '+(queryApply?'explicit real Worker validation and three-row City/Amount source publication. ':'actual editor Cancel preserves the source. ')+'A separate real missing-column validation then exposes owned status/error descendants, actual TextLines/wheel clamp and frozen picking/normal Escape with retained source/model/pages/revision/selection and exactly one intentional validation job. Full input histories and pending-validation reply ownership remain separate native gates.'};
 await fs.writeFile(path.join(output,'source-query-review.json'),JSON.stringify(result,null,2));return result;
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
 check(menu.file_menu==='1'&&menu.file_visible==='8','Pointer File menu did not expose all eight commands');
 const [menuX,menuY,menuWidth]=menu.file_panel.split(',').map(Number),menuScale=Number(menu.ui_scale),filePicks=[];
 for(const [id,x,y]of [['file.background',menuX+2*menuScale,menuY+2*menuScale],['file.heading',menuX+40*menuScale,menuY+20*menuScale],['file.action.save',menuX+menuWidth/2,menuY+(35+2*28+14)*menuScale]])filePicks.push(await pickAndCancel(id,x,y,{captureName:id==='file.action.save'?'file-disabled-save':null}));
 const filePickRestored=await platformSnapshot();
 check(filePickRestored.file_menu==='1'&&filePickRestored.file_focus===menu.file_focus&&filePickRestored.file_scroll===menu.file_scroll&&filePickRestored.active===menu.active&&filePickRestored.manual_cells===seeded.manual_cells&&filePickRestored.active_format_flags===bold.active_format_flags&&filePickRestored.dirty===menu.dirty,'Cancelling File target reports mutated the open menu or workbook');
 await key('Delete','Delete',46);const protectedCell=await platformSnapshot();
 check(protectedCell.file_menu==='1'&&protectedCell.manual_cells===seeded.manual_cells&&protectedCell.active_format_flags===bold.active_format_flags&&protectedCell.modal==='0','Delete leaked through the pointer File menu into worksheet editing');
 // This new manual-only sheet has no exportable source: Import remains the
 // last enabled row, while Export/Quit still occupy honest disabled rows.
 check(seeded.page==='0/0'&&seeded.object_count==='0','File navigation fixture unexpectedly has an exportable source/object');
 const fileNavigation=[];
 for(const [name,code,virtual,expected] of [['End','End',35,5],['Home','Home',36,0],['ArrowUp','ArrowUp',38,5],['ArrowDown','ArrowDown',40,0],['Tab','Tab',9,1],['Tab','Tab',9,4]]){
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
 check(shortFile.file_menu==='1'&&shortFile.file_focus==='5'&&Number(shortFile.file_visible)>0&&Number(shortFile.file_visible)<8&&Number(shortFile.file_scroll)>0,'Short-window File menu lost its rows or failed to reveal the focused command');
 check(fileX>=0&&fileY>=0&&fileX+fileWidth<=1280&&fileY+fileHeight<=180,'Short-window File menu escaped the canvas');
 await capture('file-menu-short');
 await click(fileX+20,fileY+15);const headingFile=await platformSnapshot();
 check(headingFile.file_menu==='1'&&headingFile.file_focus===shortFile.file_focus,'Clamped File heading clicked through to the underlying brand anchor');
 await key('Home','Home',36);const homeFile=await platformSnapshot();check(homeFile.file_focus==='0'&&homeFile.file_scroll==='0','File Home did not reveal the first command');
 const filePointer=await canvasPoint(fileX+fileWidth/2,fileY+fileHeight-20);
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',...filePointer});
 await send('Input.dispatchMouseEvent',{type:'mouseWheel',...filePointer,deltaX:0,deltaY:80});await frames(4);
 const scrolledFile=await platformSnapshot();check(Number(scrolledFile.file_scroll)>0&&scrolledFile.manual_cells===seeded.manual_cells&&scrolledFile.active===seeded.active,'File wheel failed to scroll or leaked to worksheet selection');
 const [scrollX,scrollY,scrollWidth]=scrolledFile.file_panel.split(',').map(Number),visible=Number(scrolledFile.file_visible),trackHeight=visible*28*menuScale,thumbHeight=Math.min(trackHeight,Math.max(26*menuScale,trackHeight*visible/8)),thumbY=scrollY+35*menuScale+(trackHeight-thumbHeight)*Number(scrolledFile.file_scroll)/(8-visible);
 const thumbPick=await pickAndCancel('file.scrollbar.thumb',scrollX+scrollWidth-9*menuScale,thumbY+thumbHeight-4*menuScale,{moveBar:true,captureName:'file-short-thumb'}),thumbRestored=await platformSnapshot();
 check(thumbRestored.file_menu==='1'&&thumbRestored.file_focus===scrolledFile.file_focus&&thumbRestored.file_scroll===scrolledFile.file_scroll&&thumbRestored.active===scrolledFile.active&&thumbRestored.manual_cells===seeded.manual_cells,'Moved reporting bar or thumb pick changed the frozen File scroll/workbook');
 await key('End','End',35);await key('ArrowUp','ArrowUp',38);const closeFocus=await platformSnapshot();check(closeFocus.file_focus==='4','Reverse navigation from Import did not retain the focused Close action');await key(' ','Space',32,' ');const spaceClose=await platformSnapshot();
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
 const result={before,seeded,boldBatch,bold,copyBatch,copied,menu,filePicks,filePickRestored,protectedCell,fileNavigation,reverseTab,shortFile,headingFile,homeFile,scrolledFile,thumbPick,thumbRestored,closeFocus,spaceClose,reopenedFile,helpBlocked,tinyFile,outside,closePrompt,cancelled,closed,
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
async function zoomSource({total='5',columns=6}={}){
 await send('Page.setInterceptFileChooserDialog',{enabled:true});
 await send('Input.dispatchKeyEvent',cdpKeyEvent('keyDown','Control','ControlLeft',17));await key('o','KeyO',79,'',2);await send('Input.dispatchKeyEvent',cdpKeyEvent('keyUp','Control','ControlLeft',17));await frames();
 const document=await send('DOM.getDocument'),input=await send('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#duxcel-file-input'});
 check(input.nodeId,'C Open did not invoke browser file selection for review gate');await send('DOM.setFileInputFiles',{nodeId:input.nodeId,files:[sourceFile]});
 const imported=await waitPage();check(imported.total===total&&imported.columns===columns,'Review gate requires the expected real imported source');await frames(4);return imported;
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
 if(exportOnly||exportCancelOnly)await fs.mkdir(path.join(output,'downloads'));
 if(review)await send('Browser.setDownloadBehavior',{behavior:'allow',downloadPath:exportOnly||exportCancelOnly?path.join(output,'downloads'):output,...(exportOnly||exportCancelOnly?{eventsEnabled:true}:{})});
 if(exportOnly||exportCancelOnly)await send('Page.addScriptToEvaluateOnNewDocument',{source:exportDownloadHarnessSource()});
 if(copyOnly)await send('Page.addScriptToEvaluateOnNewDocument',{source:copyClipboardHarnessSource()});
 if(reviewOffline)await send('Page.addScriptToEvaluateOnNewDocument',{source:`globalThis.duxcelReviewOffline=true;const originalFetch=globalThis.fetch.bind(globalThis);globalThis.fetch=(input,...args)=>globalThis.duxcelReviewOffline&&new URL(typeof input==='string'?input:input.url??input,location.href).pathname.startsWith('/__review/')?Promise.reject(new Error('Owned offline review acceptance')):originalFetch(input,...args);`});
 const navigation=await send('Page.navigate',{url});check(!navigation.errorText,'Navigation failed: '+navigation.errorText);
 if(workerDriver){
  let report;
  for(const deadline=Date.now()+45000;Date.now()<deadline;){report=await evaluate('globalThis.duxcelWorkerResult');if(report)break;await pause(20);}
  await fs.writeFile(path.join(output,'worker-driver.json'),JSON.stringify(report??{ok:false,error:'C driver did not complete'},null,2));
  check(report?.ok,'Real C/Worker bridge failed; see worker-driver.json and events.json');
  check(report.stages===184,'C/Worker acceptance stage inventory changed');
  const workloadLines=events.filter(event=>event.method==='Runtime.consoleAPICalled').flatMap(event=>event.params.args.map(arg=>String(arg.value??''))).filter(line=>line.startsWith('Browser worker workloads: '));
  check(workloadLines.length===1,'C/Worker did not report one actual sequential workload inventory');
  const workloads=JSON.parse(workloadLines[0].slice('Browser worker workloads: '.length));
  check(workloads.sequential_workers===2&&workloads.baseline?.jobs===146&&workloads.baseline?.sql===325&&workloads.baseline?.counts===21,'Original139-stage Worker workload changed');
  check(workloads.export?.jobs===43&&workloads.export?.sql===74&&workloads.export?.counts===0,'Export Worker workload changed');
  await fs.writeFile(path.join(output,'worker-workloads.json'),JSON.stringify(workloads,null,2));
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
 if(exportCancelOnly){
  exportCancelResult=await browserExportCancel();
 }else if(exportOnly){
  exportResult=await browserExport();
 }else if(feedbackReviewOnly){
  feedbackReviewResult=await browserFeedbackReview();
 }else if(workspaceReviewOnly){
  workspaceReviewResult=await browserWorkspaceReview();
 }else if(copyOnly){
  copyResult=await browserCopy();
 }else if(focusOnly){
  focusResult=await browserFocus();
 }else if(reviewErrorOnly){
  reviewErrorResult=await browserReviewError();
 }else if(reviewCellOnly){
  reviewCellResult=await browserReviewCell();
 }else if(reviewGuideOnly){
  reviewGuideResult=await browserReviewGuide();
 }else if(reviewValuesOnly){
  reviewValuesResult=await browserReviewValues();
 }else if(statisticsOnly){
  statisticsResult=await browserStatistics();
 }else if(reviewSQLOnly){
  sqlReviewResult=await browserAnalysis(false,false,false,true);
 }else if(reviewQueryOnly){
  queryReviewResult=await browserQueryReview();
 }else if(analysisOnly||analysisColumnsOnly||analysisStatisticsOnly||tableStatisticsOnly||mixedStatisticsOnly){
  analysisResult=await browserAnalysis(analysisColumnsOnly,analysisStatisticsOnly,tableStatisticsOnly||mixedStatisticsOnly);
  if(mixedStatisticsOnly)analysisResult=await browserMixedStatistics(analysisResult);
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
  const result=await evaluate(`(async()=>{const [{runPersistenceProbe},vendor]=await Promise.all([import('./persistence_probe.mjs'),fetch('./vendor.json').then(r=>r.json())]);const report=await runPersistenceProbe({baseURL:location.href,moduleURL:new URL(vendor.worker.moduleURL,location.href).href,workerURL:new URL(vendor.worker.workerURL,location.href).href,fixtureURL:new URL('./native-fixture.dxls',location.href).href${persistenceOwners?",mode:'owners'":''}});if(report.export){const exported=report.export;const dataURL=await new Promise((resolve,reject)=>{const reader=new FileReader();reader.onload=()=>resolve(reader.result);reader.onerror=()=>reject(reader.error);reader.readAsDataURL(new Blob([exported.bytes]));});report.export={...exported,bytes:undefined,base64:dataURL.slice(dataURL.indexOf(',')+1)};}return report;})()`,90000);
  if(result.export?.base64){
   const bytes=Buffer.from(result.export.base64,'base64');delete result.export.base64;
   check(bytes.length===result.export.byteLength&&createHash('sha256').update(bytes).digest('hex')===result.export.sha256,'BP0 exported database hash/length mismatch');
   await fs.writeFile(path.join(output,'bp0-browser-export.dxls'),bytes);
  }
  await fs.writeFile(path.join(output,'persistence-probe.json'),JSON.stringify(result,null,2));
  check(result.ok,'Real native/Worker persistence compatibility failed; see persistence-probe.json');
  check(result.ownedWorkersCreated===1&&result.checks.every(step=>step.status==='passed'),'BP0 Worker/step ownership failed');
  if(persistenceOwners)await fs.writeFile(path.join(output,'persistence-owners.json'),JSON.stringify(persistenceOwnerReport(result),null,2));
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
 await fs.writeFile(path.join(output,'result.json'),JSON.stringify({status:'passed',checks,url,surface,elapsed_ms:Date.now()-started,steps,sourceFile,sourceResult,filterResult,busyResult,reviewResult,platformResult,accessKeyResult,persistenceResult,zoomResult,reviewControlsResult,analysisResult,queryReviewResult,statisticsResult,reviewValuesResult,reviewGuideResult,reviewCellResult,reviewErrorResult,focusResult,copyResult,exportResult,exportCancelResult,workspaceReviewResult,feedbackReviewResult,sqlReviewResult,workerProbe,
  profile:exportCancelOnly?'export-cancel-only':exportOnly?'export-only':feedbackReviewOnly?'feedback-review-only':workspaceReviewOnly?'workspace-review-only':copyOnly?'copy-only':focusOnly?'focus-only':reviewErrorOnly?'review-error-only':reviewCellOnly?'review-cell-only':reviewGuideOnly?'review-guide-only':persistenceOwners?'persistence-owners':reviewSQLOnly?'review-sql-only':mixedStatisticsOnly?'mixed-statistics-only':tableStatisticsOnly?'table-statistics-only':reviewValuesOnly?'review-values-only':analysisStatisticsOnly?'analysis-statistics-only':statisticsOnly?'statistics-only':queryApply?'review-query-apply':reviewQueryOnly?'review-query-only':analysisColumnsOnly?'analysis-columns-only':analysisOnly?'analysis-only':reviewControlsOnly?'review-controls-only':zoomOnly?'zoom-only':accessOnly?'access-only':'standard',
  scope:exportCancelOnly?exportCancelResult.scope:exportOnly?exportResult.scope:feedbackReviewOnly?feedbackReviewResult.scope:workspaceReviewOnly?workspaceReviewResult.scope:copyOnly?copyResult.scope:focusOnly?focusResult.scope:reviewErrorOnly?reviewErrorResult.scope:reviewCellOnly?reviewCellResult.scope:reviewGuideOnly?reviewGuideResult.scope:persistenceOwners?'BP0 official Worker/native v13 owner lifetime file compatibility: typed metadata rewrite/export/reopen and separate native release/ASan readback; no shared C browser Save/Open codec/controller or OPFS acceptance.':reviewSQLOnly?sqlReviewResult.scope:mixedStatisticsOnly?analysisResult.scope:tableStatisticsOnly?analysisResult.scope:reviewValuesOnly?reviewValuesResult.scope:analysisStatisticsOnly?analysisResult.scope:statisticsOnly?statisticsResult.scope:reviewQueryOnly?queryReviewResult.scope:analysisColumnsOnly?analysisResult.scope:analysisOnly?'Shared C same-sheet Rows/Values analysis, real matrix/grand totals and source/manual ownership; no Columns pivot, broad analytics or report upload.':reviewControlsOnly?'Shared C source/manual contextual targets and field-menu children, editable width/Move drafts and guarded Close, trusted input ownership and cancel retention, narrow150% clipping; no analytical-object or report-upload acceptance.':zoomOnly?'Shared C fixed header geometry/rendered glyphs during blank/source canvas zoom, pointer header selection/resize/filter and independent interface scale; no report upload or physical gesture acceptance.':accessOnly?'Shared C startup and focused blank-workbook access keys/File/document interactions; no Worker query, clipboard delivery or report-upload acceptance.':'Shared C99 startup and captured real interactions for visual verification; source imports, filters, active-query frames and review exports are explicit optional gates; BP0 is file compatibility only, browser workbook persistence is not accepted'},null,2));
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
