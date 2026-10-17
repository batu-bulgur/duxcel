// Acceptance-only host, packaged only by build_web.py --worker-driver.
import createDuxcel from './duxcel.mjs';
import {createEngineHost} from './engine_host.mjs';
try {
    const vendor=await (await fetch('./vendor.json')).json();
    const engine=createEngineHost({moduleURL:new URL(vendor.worker.moduleURL,location.href).href,workerURL:new URL(vendor.worker.workerURL,location.href).href});
    const options={dxTestCancels:0,print:message=>console.log(message),printErr:message=>console.error(message)};
    let module;
    options.dxEngine={...engine,cancel(close){(module??options).dxTestCancels++;return engine.cancel(close);}};
    module=await createDuxcel(options);
    const timer=setInterval(()=>{
        if(module.dxBrowserWorkerTestDone){globalThis.duxcelWorkerResult=module.dxBrowserWorkerTestDone;clearInterval(timer);}
    },10);
} catch(error) {globalThis.duxcelWorkerResult={ok:false,error:String(error)};console.error(error);}
