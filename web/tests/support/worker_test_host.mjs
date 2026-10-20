// Acceptance-only host, packaged only by build_web.py --worker-driver.
import createDuxcel from '../../duxcel.mjs';
import {createEngineHost} from '../../engine_host.mjs';
import {createOfficialWorkerAdapter} from '../../official_worker.mjs';
try {
    const vendor=await (await fetch('./vendor.json')).json();
    const config={moduleURL:new URL(vendor.worker.moduleURL,location.href).href,workerURL:new URL(vendor.worker.workerURL,location.href).href};
    const options={dxTestCancels:0,print:message=>console.log(message),printErr:message=>console.error(message)};
    let module;
    const freshEngine=()=>{
        const engine=createEngineHost(config,async settings=>{
            const adapter=await createOfficialWorkerAdapter(settings);
            (module??options).dxTestAdapter=adapter;
            return adapter;
        });
        return {...engine,cancel(close){(module??options).dxTestCancels++;return engine.cancel(close);}};
    };
    options.dxEngine=freshEngine();
    // The existing driver closes its first Worker before checking its terminal
    // counters and late callbacks. Later export cases need a fresh real engine,
    // never an overlapping Worker or a reset of observed workload counters.
    options.dxResetTestEngine=async()=>{
        const runtime=module??options;
        await runtime.dxEngine.cancel(true);
        runtime.dxTestAdapter=undefined;
        runtime.dxEngine=freshEngine();
    };
    module=await createDuxcel(options);
    const timer=setInterval(()=>{
        if(module.dxBrowserWorkerTestDone){globalThis.duxcelWorkerResult=module.dxBrowserWorkerTestDone;clearInterval(timer);}
    },10);
} catch(error) {globalThis.duxcelWorkerResult={ok:false,error:String(error)};console.error(error);}
