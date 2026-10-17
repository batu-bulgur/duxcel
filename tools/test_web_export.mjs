#!/usr/bin/env node
// Its Python parent owns the existing cross-worktree desktop lock and server.
import {spawn} from 'node:child_process';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {createCDP} from './test_web_cdp.mjs';
const url=process.argv[2],output=path.resolve(process.argv[3]);
if(!/^http:\/\/127\.0\.0\.1:\d+\/$/.test(url))throw new Error('Owned loopback server URL required');
const started=Date.now(),events=[];
const profile=await fs.mkdtemp(path.join(os.tmpdir(),'duxcel-parquet-proof-'));
const browser=spawn(process.env.DUXCEL_CHROMIUM??'/Applications/Chromium.app/Contents/MacOS/Chromium',
    ['--headless','--no-first-run','--no-default-browser-check','--no-proxy-server','--remote-debugging-port=0',
        '--user-data-dir='+profile,'about:blank'],{stdio:['ignore','ignore','pipe']});
let stderr='',socket,cdp,checks=0;
browser.stderr.on('data',chunk=>{stderr+=chunk;});
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function check(value,message){checks++;if(!value)throw new Error(message);}
async function evaluate(expression,timeout=15000){
    const result=await cdp.send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true},timeout);
    if(result.exceptionDetails)throw new Error(JSON.stringify(result.exceptionDetails));return result.result?.value;
}
try{
    await fs.writeFile(path.join(output,'browser-process.json'),JSON.stringify({pid:browser.pid,parent_pid:process.pid,headless:true,profile:'owned temporary profile'},null,2));
    for(let n=0;n<100&&!stderr.includes('DevTools listening on');n++)await pause(50);
    const port=stderr.match(/DevTools listening on ws:\/\/127\.0\.0\.1:(\d+)\//)?.[1];check(port,'No owned DevTools endpoint');
    const targets=await(await fetch(`http://127.0.0.1:${port}/json/list`)).json(),target=targets.find(value=>value.type==='page');
    check(target,'No owned blank page target');socket=new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
    cdp=createCDP(socket,events);await cdp.send('Runtime.enable');await cdp.send('Page.enable');
    await cdp.send('Page.setDownloadBehavior',{behavior:'deny'});
    const navigation=await cdp.send('Page.navigate',{url});check(!navigation.errorText,'Owned blank page navigation failed');
    for(let n=0;n<100;n++){if(await evaluate(`location.href===${JSON.stringify(url)}&&document.readyState==='complete'`))break;await pause(20);}
    const report=await evaluate(`(async()=>{
        const [{runExportProbe},vendor]=await Promise.all([import('./export_probe.mjs'),fetch('./vendor.json').then(response=>response.json())]);
        const report=await runExportProbe({baseURL:location.href,moduleURL:new URL(vendor.worker.moduleURL,location.href).href,workerURL:new URL(vendor.worker.workerURL,location.href).href});
        for(const output of report.outputs){const bytes=output.bytes;const data=await new Promise((resolve,reject)=>{const reader=new FileReader();reader.onload=()=>resolve(reader.result);reader.onerror=()=>reject(reader.error);reader.readAsDataURL(new Blob([bytes]));});output.bytes=undefined;output.base64=data.slice(data.indexOf(',')+1);}
        return report;
    })()`,90000);
    for(const exported of report.outputs){
        check(['plain','buffer'].includes(exported.mode),'Unexpected accepted export mode');
        const bytes=Buffer.from(exported.base64,'base64');delete exported.base64;
        check(bytes.length===exported.byteLength&&bytes.length<=64*1024*1024,'Exported Parquet size mismatch');
        check(createHash('sha256').update(bytes).digest('hex')===exported.sha256,'Exported Parquet hash mismatch');
        await fs.writeFile(path.join(output,`probe-${exported.mode}.parquet`),bytes,{flag:'wx'});
    }
    await fs.writeFile(path.join(output,'export-probe.json'),JSON.stringify(report,null,2));
    check(report.ok===true,'Pinned official Worker Parquet capability proof failed');
    check(report.ownedWorkersCreated===1,'Proof did not own exactly one official Worker');
    check(report.acceptedModes.length>0&&report.acceptedModes.length===report.outputs.length,'No accepted virtual-file creation mode');
    check(report.checks.every(value=>value.status==='passed'||value.status==='unsupported'&&/^parquet-(plain|buffer)-candidate$/.test(value.name)),'Proof includes a failed non-candidate step');
    for(const name of ['original-source-unchanged','parquet-plain-private-cleanup','parquet-buffer-private-cleanup','bounded-owned-worker-cleanup'])
        check(report.checks.some(value=>value.name===name&&value.status==='passed'),`Missing cleanup/source proof: ${name}`);
    check(!events.some(value=>value.method==='Runtime.exceptionThrown'),'Unexpected browser exception');
    await fs.writeFile(path.join(output,'result.json'),JSON.stringify({status:'passed',checks,elapsed_ms:Date.now()-started,acceptedModes:report.acceptedModes,scope:report.scope},null,2));
    console.log(JSON.stringify({status:'passed',checks,acceptedModes:report.acceptedModes,output}));
}catch(error){
    await fs.writeFile(path.join(output,'result.json'),JSON.stringify({status:'failed',checks,error:String(error),elapsed_ms:Date.now()-started},null,2));
    console.error(error);process.exitCode=1;
}finally{
    socket?.close();browser.kill('SIGTERM');
    await Promise.race([new Promise(resolve=>browser.once('exit',resolve)),pause(2000)]);
    if(browser.exitCode===null&&browser.signalCode===null){browser.kill('SIGKILL');await Promise.race([new Promise(resolve=>browser.once('exit',resolve)),pause(1000)]);}
    await fs.writeFile(path.join(output,'events.json'),JSON.stringify(events,null,2));
    await fs.writeFile(path.join(output,'browser.txt'),stderr);
    await fs.writeFile(path.join(output,'cleanup.json'),JSON.stringify({pid:browser.pid,exit_code:browser.exitCode,signal:browser.signalCode,profile:'removed'},null,2));
    await fs.rm(profile,{recursive:true,force:true});
}
