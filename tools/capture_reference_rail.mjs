#!/usr/bin/env node
// Internal driver: capture existing frozen DOM, never mutate React/app state.
// Its Python parent owns the desktop lock, ephemeral static server and profile.
import {spawn} from 'node:child_process';
import fs from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {createCDP} from './test_web_cdp.mjs';
import {cdpKeyEvent} from './cdp_keyboard.mjs';

if(process.argv.length!==3)throw new Error('Run tools/capture_reference_rail.py, not this internal driver');
const config=JSON.parse(await fs.readFile(process.argv[2],'utf8'));
if(config.desktop_lock_pid!==process.ppid||new URL(config.url).hostname!=='127.0.0.1'||![1,2].includes(config.dpr)||
   !path.isAbsolute(config.profile)||(await fs.readdir(config.profile)).length)throw new Error('Expected the locking wrapper and its new empty temporary profile');
const output=config.output,events=[],states=[],actions=[],loadedAssets=[];
const validatedRequests=new Set();
let checks=0,stderr='',ws,cdp,browser,browserVersion,interrupted=false;
const began=Date.now(),pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
function check(value,message){checks++;if(!value)throw new Error(message);}
function near(a,b,tolerance=.08){return Number.isFinite(a)&&Math.abs(a-b)<=tolerance;}
// CSSOM serializes resolved fractional widths as decimal text. Bound only that
// serialization rounding; do not invent a zoom/DPR border-snapping algorithm.
function resolvedCssInterval(text,multiplier=1){
 const match=/^(\d+)(?:\.(\d+))?px$/.exec(text??'');if(!match)return null;
 const value=Number.parseFloat(text),rounding=match[2]?0.5*10**(-match[2].length):0;
 return{value:value*multiplier,minimum:(value-rounding)*multiplier,maximum:(value+rounding)*multiplier};
}
function resolvedCssMatches(actual,text,multiplier=1){const interval=resolvedCssInterval(text,multiplier);return interval&&Number.isFinite(actual)&&actual>=interval.minimum&&actual<=interval.maximum;}
async function send(method,params={},timeout=10000){return cdp.send(method,params,timeout,'frozen reference rail');}
async function evaluate(expression){const result=await send('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});if(result.exceptionDetails)throw new Error(JSON.stringify(result.exceptionDetails));return result.result?.value;}
async function waitFor(expression,label){for(const end=Date.now()+8000;Date.now()<end;){if(interrupted)throw new Error('Reference capture interrupted');if(await evaluate(expression))return;await pause(25);}throw new Error('Timed out waiting for '+label);}
async function settle(){await pause(220);await evaluate('document.fonts.ready.then(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(()=>resolve(true)))))');await waitFor('document.getAnimations().every(animation=>animation.playState!=="running")','natural prototype transitions');}
async function click(selector){
 const target=await evaluate(`(()=>{const nodes=[...document.querySelectorAll(${JSON.stringify(selector)})].filter(el=>{const r=el.getBoundingClientRect();return r.width>0&&r.height>0;});if(nodes.length!==1)return{error:'Expected one visible control',count:nodes.length};const el=nodes[0],r=el.getBoundingClientRect(),x=(Math.max(0,r.left)+Math.min(innerWidth,r.right))/2,y=(Math.max(0,r.top)+Math.min(innerHeight,r.bottom))/2,hit=document.elementFromPoint(x,y);return{rect:{x:r.x,y:r.y,width:r.width,height:r.height},x,y,reachable:!!hit&&(hit===el||el.contains(hit)),disabled:el.disabled,label:el.getAttribute('aria-label')||el.textContent};})()`);
 check(!target.error&&!target.disabled&&target.reachable,'Actual reference control is not reachable: '+selector+' '+JSON.stringify(target));
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',x:target.x,y:target.y});
 await send('Input.dispatchMouseEvent',{type:'mousePressed',button:'left',clickCount:1,x:target.x,y:target.y});
 await send('Input.dispatchMouseEvent',{type:'mouseReleased',button:'left',clickCount:1,x:target.x,y:target.y});
 actions.push({selector,...target});await settle();
}
async function menuItem(menu,prefix){
 const selector=await evaluate(`(()=>{const menu=document.querySelector('[role="menu"][aria-label='+JSON.stringify(${JSON.stringify(menu)})+']');if(!menu)return null;const nodes=[...menu.querySelectorAll('button[role^="menuitem"]')];const i=nodes.findIndex(el=>el.textContent.trim().startsWith(${JSON.stringify(prefix)}));return i<0?null:'[role="menu"][aria-label='+JSON.stringify(${JSON.stringify(menu)})+'] button[role^="menuitem"]:nth-of-type('+(i+1)+')';})()`);
 check(selector,'Missing actual '+menu+' item '+prefix);await click(selector);
}
async function shortcut(){
 for(const type of ['keyDown','keyUp'])await send('Input.dispatchKeyEvent',cdpKeyEvent(type,'t','KeyT',84,{modifiers:3}));
 await settle();
}
async function snapshot(){return evaluate(`(()=>{
 const rect=el=>{if(!el)return null;const r=el.getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height};};
 const describe=selector=>{const el=document.querySelector(selector);if(!el)return null;const s=getComputedStyle(el);return{rect:rect(el),background:s.backgroundColor,shadow:s.boxShadow,borderLeft:s.borderLeftWidth,borderStyle:s.borderLeftStyle,borderColor:s.borderLeftColor,opacity:s.opacity,position:s.position,display:s.display,width:s.width,height:s.height};};
 const app=document.querySelector('.duxcel-app'),grid=document.querySelector('[role="grid"]');
 const railDeclarations=[];function rules(list,href){for(const rule of list){if(rule.selectorText==='.retained-rail')railDeclarations.push({stylesheet:href,selector:rule.selectorText,borderLeft:rule.style.getPropertyValue('border-left'),borderLeftWidth:rule.style.borderLeftWidth,borderLeftStyle:rule.style.borderLeftStyle,borderLeftColor:rule.style.borderLeftColor});if(rule.cssRules)rules(rule.cssRules,href);}}for(const sheet of document.styleSheets)rules(sheet.cssRules,sheet.href);
 const cells=[...grid.querySelectorAll('[data-grid-cell]')].map(el=>({row:Number(el.dataset.row),col:Number(el.dataset.col),id:el.id,text:el.textContent,value:el.title,selected:el.getAttribute('aria-selected')}));
 return{viewport:{width:innerWidth,height:innerHeight,dpr:devicePixelRatio},scale:Number(app.dataset.uiScale),ribbon:app.dataset.ribbonMode,authoring:app.dataset.authoring,narrow:app.classList.contains('narrow-workspace'),compact:app.classList.contains('compact-workspace'),
  workspace:describe('.workspace'),sheet:describe('.sheet-workspace'),grid:describe('[role="grid"]'),wrapper:describe('.right-pane-wrap'),rail:describe('.retained-rail'),railDeclarations,inspector:describe('.right-inspector'),
  expand:describe('[aria-label="Expand Transformations"]'),dismiss:describe('[aria-label="Dismiss retained pane"]'),collapse:describe('[aria-label="Collapse and retain pane"]'),
  source:document.querySelector('.inspector-source')?.textContent||null,phase:document.querySelector('.right-pane-wrap')?.className||null,
  model:{rowCount:grid.getAttribute('aria-rowcount'),columnCount:grid.getAttribute('aria-colcount'),active:grid.getAttribute('aria-activedescendant'),address:document.querySelector('.cell-address')?.textContent,rows:document.querySelector('.row-count')?.textContent,
   selection:cells.filter(cell=>cell.selected==='true').map(({row,col,id,value})=>({row,col,id,value})),cells,scroll:{left:grid.scrollLeft,top:grid.scrollTop},filters:[...document.querySelectorAll('.filter-trigger.filtered')].map(el=>el.dataset.filter),sort:[...document.querySelectorAll('[role="columnheader"]')].filter(el=>el.getAttribute('aria-sort')!=='none').map(el=>({field:el.getAttribute('aria-label'),sort:el.getAttribute('aria-sort')}))}};
 })()`);}
async function capture(caseName,name,baseline){
 // Park away from the rail so hover/tooltip pixels do not contaminate its fill.
 await send('Input.dispatchMouseEvent',{type:'mouseMoved',x:8,y:8});await settle();
 const state=await snapshot(),scale=state.scale,retained=name.startsWith('retained'),dismissed=name==='dismissed';
 const geometry=state.rail?{resolvedBorderCss:state.rail.borderLeft,resolvedBorderViewport:resolvedCssInterval(state.rail.borderLeft,scale),resolvedBorderDevice:resolvedCssInterval(state.rail.borderLeft,scale*state.viewport.dpr),buttons:Object.fromEntries(['expand','dismiss'].map(part=>{const button=state[part];return[part,button?{offsetX:button.rect.x-state.rail.rect.x,offsetY:button.rect.y-state.rail.rect.y,contentCenterBorderViewport:2*(button.rect.x-state.rail.rect.x)-(state.rail.rect.width-button.rect.width)}:null];}))}:null;
 // Persist raw geometry before any assertion, including the first failing state.
 const record={case:caseName,name,state,geometry,validated:false};states.push(record);
 const stateFile=path.join(output,caseName+'-'+name+'.state.json');
 await fs.writeFile(stateFile,JSON.stringify(record,null,2)+'\n');
 const screenshot=Buffer.from((await send('Page.captureScreenshot',{format:'png',fromSurface:true,captureBeyondViewport:false})).data,'base64');
 const filename=caseName+'-'+name+'.png';await fs.writeFile(path.join(output,filename),screenshot);
 record.capture={file:filename,sha256:hash(screenshot),bytes:screenshot.length};
 const image=await evaluate(`(async()=>{const bytes=Uint8Array.from(atob(${JSON.stringify(screenshot.toString('base64'))}),c=>c.charCodeAt(0)),image=await createImageBitmap(new Blob([bytes],{type:'image/png'})),surface=new OffscreenCanvas(image.width,image.height),ctx=surface.getContext('2d',{willReadFrequently:true});ctx.drawImage(image,0,0);const ratio=image.width/innerWidth,scale=${scale},panel=${JSON.stringify(state.rail?.rect??null)},rail=${JSON.stringify(state.rail)},workspace=${JSON.stringify(state.workspace.rect)},x=innerWidth-32*scale,y=workspace.y+Math.min(110*scale,workspace.height/2),p=(x,y)=>Array.from(ctx.getImageData(Math.floor(x*ratio),Math.floor(y*ratio),1,1).data),strip=ctx.getImageData(Math.ceil((x-5*scale)*ratio),Math.floor(y*ratio),Math.max(1,Math.floor(4*scale*ratio)),1).data;let border=null;if(panel){const width=Number.parseFloat(rail.borderLeft)*scale,first=Math.floor(panel.x*ratio),last=Math.ceil((panel.x+width)*ratio)-1,start=Math.max(0,first-1),end=Math.min(image.width-1,last+1),rgba=[...(rail.borderColor.match(/[0-9.]+/g)||[]).map(Number),255].slice(0,4),samples=[];for(let at=start;at<=end;at++)samples.push({x:at,rgba:Array.from(ctx.getImageData(at,Math.floor(y*ratio),1,1).data)});const ink=samples.filter(sample=>sample.rgba.every((value,i)=>value===rgba[i])).map(sample=>sample.x);border={resolvedCss:rail.borderLeft,viewportWidth:width,deviceWidth:width*ratio,rgba,envelope:{first,last},samples,inkPixels:ink,inkWidth:ink.length,contiguous:ink.every((at,i)=>!i||at===ink[i-1]+1)};}const result={width:image.width,height:image.height,ratio,railSample:panel?p(panel.x+16*scale,y):null,adjacentStrip:Array.from(strip),sampleY:y,border};image.close();return result;})()`);
 record.image=image;await fs.writeFile(stateFile,JSON.stringify(record,null,2)+'\n');
 check(JSON.stringify(state.model)===JSON.stringify(baseline.model),'Rail lifecycle changed source, selection, values or scroll: '+caseName+'/'+name);
 check(state.ribbon==='compact'&&state.authoring==='false','Capture left compact source-browsing state');
 check(state.narrow===(state.viewport.width/scale<1040),'Reference narrow-workspace rule differs from frozen definition');
 check(state.compact===(state.viewport.width/scale<700),'Reference compact-workspace rule differs from frozen definition');
 if(dismissed){check(!state.wrapper&&!state.rail&&!state.inspector,'Dismiss retained a pane or rail');}
 else{
  check(state.phase?.includes('presence-open')&&state.wrapper.opacity==='1','Pane captured before its normal presence animation settled');
  check(near(state.wrapper.rect.width,(retained?32:280)*scale),'Reference pane width mismatch');
  check(near(state.wrapper.rect.x+state.wrapper.rect.width,state.viewport.width),'Reference pane is not at the right edge');
  check(state.wrapper.position===(state.narrow?'absolute':'relative'),'Reference dock/overlay ownership mismatch');
  check(state.wrapper.shadow===(state.narrow&&!retained?'rgba(39, 61, 79, 0.13) -5px 0px 14px 0px':'none'),'Reference narrow retained/expanded shadow mismatch');
  if(retained){
   check(!state.inspector&&state.rail?.background==='rgb(237, 242, 246)','Retained reference fill is not #edf2f6');
   // The var() shorthand can leave its declared longhands pending substitution.
   // Inspect its actual CSSOM declaration, independently of resolved/raster width.
   check(state.railDeclarations.length===1&&state.railDeclarations[0].borderLeft==='1px solid var(--line)','Loaded reference CSS no longer declares the one-pixel solid rail border');
   check(state.rail.borderStyle==='solid'&&state.rail.borderColor==='rgb(202, 210, 218)'&&Number.parseFloat(state.rail.borderLeft)>0,'Resolved retained rail border disappeared or changed style/color');
   const border=image.border;
   check(border?.contiguous&&border.inkWidth>0&&resolvedCssMatches(border.inkWidth,state.rail.borderLeft,scale*image.ratio),'Presented rail border ink does not match resolved CSS width×zoom×DPR');
   check(border.inkPixels.every(x=>x>=border.envelope.first&&x<=border.envelope.last),'Presented rail border ink escapes its resolved border-box envelope');
   for(const [part,y]of [['expand',5],['dismiss',36]]){
    const button=state[part];
    check(button&&near(button.rect.width,26*scale)&&near(button.rect.height,26*scale),'Retained control is not26×26 logical units');
    check(resolvedCssMatches(geometry.buttons[part].contentCenterBorderViewport,state.rail.borderLeft,scale)&&near(button.rect.y-state.rail.rect.y,y*scale),'Retained control no longer centers in the actual resolved border-box content');
   }
  }else check(!state.rail&&state.inspector?.background==='rgb(251, 252, 253)'&&state.source==='Source: Transactions','Expanded reference source/background mismatch');
 }
 const expectedWidth=state.workspace.rect.width-(state.compact?0:228*scale)-(state.narrow||dismissed?0:(retained?32:280)*scale);
 check(near(state.sheet.rect.width,expectedWidth,.12),'Reference pane reserved an incorrect grid width');
 check(image.width===Math.round(state.viewport.width*config.dpr)&&image.height===Math.round(state.viewport.height*config.dpr),'Reference compositor screenshot DPR mismatch');
 if(retained)check(JSON.stringify(image.railSample)==='[237,242,246,255]','Presented retained rail pixels differ from its computed #edf2f6');
 record.validated=true;await fs.writeFile(stateFile,JSON.stringify(record,null,2)+'\n');return record;
}
async function validateLoadedAssets(){
 const responses=events.filter(event=>event.method==='Network.responseReceived'&&!validatedRequests.has(event.params.requestId)&&['Document','Stylesheet','Script','Font'].includes(event.params.type));
 for(const event of responses){const response=event.params.response,url=new URL(response.url);check(url.origin===new URL(config.url).origin,'Reference loaded an external dependency: '+url.href);const name=decodeURIComponent(url.pathname).replace(/^\//,'')||'index.html',expected=config.assets[name];check(response.status===200&&expected,'Loaded unknown/missing prebuilt asset: '+name);const body=await send('Network.getResponseBody',{requestId:event.params.requestId}),bytes=Buffer.from(body.body,body.base64Encoded?'base64':'utf8');check(hash(bytes)===expected.sha256,'Actual served asset hash mismatch: '+name);loadedAssets.push({file:name,sha256:hash(bytes),bytes:bytes.length,type:event.params.type});validatedRequests.add(event.params.requestId);}
 check(loadedAssets.some(asset=>asset.type==='Stylesheet')&&loadedAssets.some(asset=>asset.type==='Script')&&loadedAssets.some(asset=>asset.type==='Font'),'Expected actual local stylesheet, script and font response evidence');
}

try{
 browser=spawn(process.env.DUXCEL_CHROMIUM??'/Applications/Chromium.app/Contents/MacOS/Chromium',[
  '--headless','--disable-background-networking','--disable-component-update','--disable-sync','--disable-background-timer-throttling','--disable-renderer-backgrounding','--disable-backgrounding-occluded-windows','--remote-debugging-port=0','--user-data-dir='+config.profile,'--no-first-run','--no-default-browser-check','--no-proxy-server','about:blank'],{stdio:['ignore','ignore','pipe']});
 browser.stderr.on('data',data=>{stderr+=data;});let launchError;browser.once('error',error=>{launchError=error;});
 for(const signal of ['SIGTERM','SIGINT'])process.once(signal,()=>{interrupted=true;ws?.close();browser.kill('SIGTERM');});
 for(const end=Date.now()+10000;Date.now()<end&&!/DevTools listening/.test(stderr);){if(launchError)throw launchError;if(browser.exitCode!==null)throw new Error('Owned Chromium exited before CDP');await pause(25);}
 const port=stderr.match(/DevTools listening on ws:\/\/127\.0\.0\.1:(\d+)\//)?.[1];check(port,'No owned Chromium CDP endpoint');
 const targets=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json(),target=targets.find(item=>item.type==='page');check(target,'Owned headless browser has no page target');
 ws=new WebSocket(target.webSocketDebuggerUrl);cdp=createCDP(ws,events);await new Promise((resolve,reject)=>{ws.addEventListener('open',resolve,{once:true});ws.addEventListener('error',()=>reject(new Error('CDP connection failed')),{once:true});});
 await send('Page.enable');await send('Runtime.enable');await send('Network.enable');await send('Network.setCacheDisabled',{cacheDisabled:true});await send('Emulation.setFocusEmulationEnabled',{enabled:true});
 browserVersion=await send('Browser.getVersion');
 await send('Page.addScriptToEvaluateOnNewDocument',{source:`globalThis.__duxcelRailInputs=[];for(const type of ['click','keydown'])document.addEventListener(type,event=>{const el=event.target.closest?.('button');__duxcelRailInputs.push({type,trusted:event.isTrusted,key:event.key,ctrl:event.ctrlKey,alt:event.altKey,label:el?.getAttribute('aria-label')||el?.textContent||null});},true);`});
 for(const fixture of [{name:'wide-100',width:1440,height:1024,scale:1},{name:'wide-150',width:1920,height:1120,scale:1.5},{name:'narrow-100',width:640,height:720,scale:1},{name:'narrow-150',width:640,height:640,scale:1.5}]){
  await send('Emulation.setDeviceMetricsOverride',{width:fixture.width,height:fixture.height,deviceScaleFactor:config.dpr,mobile:false});
  const location=new URL(config.url);location.searchParams.set('rail-case',fixture.name);
  const navigation=await send('Page.navigate',{url:location.href});check(!navigation.errorText,'Reference navigation failed');await waitFor('location.search==='+JSON.stringify(location.search)+'&&!!document.querySelector("[role=grid]")','new frozen grid document');await settle();
  if(await evaluate('document.querySelector(".duxcel-app").dataset.ribbonMode!=="compact"')){await click('[aria-label="Ribbon display"]');await menuItem('Ribbon display','Compact ·');}
  if(fixture.scale!==1){await click('.scale-control');await menuItem('Interface scale','150%');}
  const baseline=await snapshot();check(baseline.scale===fixture.scale&&baseline.ribbon==='compact','Actual reference scale/ribbon setup failed');
  check(baseline.model.rowCount==='241'&&baseline.model.columnCount==='12'&&baseline.model.rows==='240 sample rows'&&baseline.model.address==='F7','Frozen source fixture is not240×12/activeF7');
  check(JSON.stringify(baseline.model.selection.map(cell=>[cell.row,cell.col]))==='[[5,5],[6,5],[7,5],[8,5]]','Frozen source selection is notF7:F10');
  check(JSON.stringify(baseline.model.filters)==='["status","currency"]'&&JSON.stringify(baseline.model.sort)==='[{"field":"Transaction ID","sort":"ascending"}]','Frozen source filters/sort differ from definition');
  await shortcut();await capture(fixture.name,'expanded',baseline);
  await click('[aria-label="Collapse and retain pane"]');const retained=await capture(fixture.name,'retained',baseline);
  await click('[aria-label="Expand Transformations"]');await capture(fixture.name,'expanded-again',baseline);
  await click('[aria-label="Collapse and retain pane"]');await capture(fixture.name,'retained-again',baseline);
  await click('[aria-label="Dismiss retained pane"]');const dismissed=await capture(fixture.name,'dismissed',baseline);
  if(retained.state.narrow)check(JSON.stringify(retained.image.adjacentStrip)===JSON.stringify(dismissed.image.adjacentStrip),'Collapsed narrow reference paints a shadow over the underlying grid');
  const inputs=await evaluate('__duxcelRailInputs');check(inputs.length>0&&inputs.every(input=>input.trusted),'Reference lifecycle bypassed actual trusted DOM input');
  check(inputs.some(input=>input.type==='keydown'&&input.key==='t'&&input.ctrl&&input.alt),'Reference Transformations was not opened through its real keyboard handler');
  actions.push({case:fixture.name,deliveredInputs:inputs});
  // Validate bodies before the next navigation can retire their CDP resources.
  await validateLoadedAssets();
 }
 await validateLoadedAssets();check(!events.some(event=>event.method==='Runtime.exceptionThrown'),'Reference raised a JavaScript exception');
 await fs.writeFile(path.join(output,'reference.json'),JSON.stringify({status:'passed',checks,elapsed_ms:Date.now()-began,dpr:config.dpr,browserVersion,states,actions,loadedAssets,scope:'Frozen prebuilt prototype rail DOM/style/layout and presented pixels at four viewports using trusted CDP input. Synthetic prototype data/calculations are reference-only; no native/Wasm parity, SQL, OS input or current dist/source derivation claim.'},null,2)+'\n');
 console.log(JSON.stringify({status:'passed',checks,captures:states.length,output}));
}catch(error){
 try{if(cdp){const image=await send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.join(output,'failed.png'),Buffer.from(image.data,'base64'));}}catch{}
 await fs.writeFile(path.join(output,'reference.json'),JSON.stringify({status:'failed',checks,error:String(error),elapsed_ms:Date.now()-began,states,actions,loadedAssets,transport:cdp?.failure},null,2)+'\n');console.error(error);process.exitCode=1;
}finally{
 await cdp?.releaseHeldKeys();ws?.close();
 if(browser?.pid&&browser.exitCode===null&&browser.signalCode===null){const exited=new Promise(resolve=>browser.once('exit',resolve));browser.kill('SIGTERM');await Promise.race([exited,pause(2000)]);if(browser.exitCode===null&&browser.signalCode===null){browser.kill('SIGKILL');await exited;}}
 await fs.writeFile(path.join(output,'browser.txt'),stderr);
 await fs.writeFile(path.join(output,'events.json'),JSON.stringify(events,null,2)+'\n');
 await fs.writeFile(path.join(output,'cleanup.json'),JSON.stringify({browser_pid:browser?.pid,exit_code:browser?.exitCode,signal:browser?.signalCode,profile_owned_by_wrapper:true},null,2)+'\n');
}
