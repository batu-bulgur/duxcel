// BP1 acceptance-only transport. Metadata SQL, schemas and interpretation live
// in the C driver; this host never parses workbook tables or DXP1.
import createDuxcel from './duxcel.mjs';
import {createEngineHost} from './engine_host.mjs';
import {createOfficialWorkerAdapter} from './official_worker.mjs';

const parameters=new URLSearchParams(location.search);
const tablesMode=parameters.get('codec_tables')==='1';
const sortsMode=parameters.get('codec_sorts')==='1',preferencesMode=parameters.get('codec_preferences')==='1',objectsMode=parameters.get('codec_objects')==='1',queriesMode=parameters.get('codec_queries')==='1',optionsMode=parameters.get('codec_source_options')==='1';
const names=tablesMode?['tables-v12.dxls','tables-v13.dxls','tables-v14.dxls','tables-v14-reordered.dxls']:optionsMode?['options-v12.dxls','options-v13.dxls','options-v14.dxls','options-v14-reordered.dxls']:queriesMode?['queries-v12.dxls','queries-v14-blank.dxls','queries-v14-nul.dxls','queries-v13.dxls','queries-v14.dxls','queries-v14-reordered.dxls']:objectsMode?['objects-v12.dxls','objects-v13.dxls','objects-v14.dxls','objects-v14-reordered.dxls']:preferencesMode?['preferences-v12.dxls','preferences-v14-unknown-null.dxls','preferences-v14-unknown-nul.dxls','preferences-v13.dxls','preferences-v14.dxls','preferences-v14-reordered.dxls']:sortsMode?['sorts-v12.dxls','sorts-v13.dxls','sorts-v14.dxls','sorts-v14-reordered.dxls']:['v12.dxls','v12-reordered.dxls','v12-legacy24.dxls','v13.dxls','v13-reordered.dxls','v14.dxls','v14-reordered.dxls'];
const encoder=new TextEncoder();
const receipts={schema:'duxcel.codec-read-transport',version:1,fixtures:[],queries:[],held:0,released:0};
let adapter,engine,registered=null,held=null,module;
const errorText=error=>String(error?.message??error).slice(0,4096);
function callback(runtime,cookie,ticket,event,epoch='',registration='',sha='',bytes=0,error='') {
    const owned=[];
    const string=value=>{const data=encoder.encode(value+'\0'),pointer=runtime.allocate(data.length);if(!pointer)throw new Error('Codec callback allocation failed');owned.push(pointer);runtime.heap().set(data,pointer);return pointer;};
    try {runtime.fixture(cookie,ticket,event,string(epoch),string(registration),string(sha),bytes,error?string(error):0);}
    finally {for(const pointer of owned)runtime.release(pointer);}
}
async function manifest() {
    const response=await fetch('/__codec-fixtures/manifest.json',{cache:'no-store'});
    if(!response.ok)throw new Error('Codec fixture manifest unavailable');
    const text=await response.text();if(text.length>65536)throw new Error('Codec fixture manifest exceeds bound');
    const value=JSON.parse(text);
    if(value.schema!=='duxcel.codec-fixtures'||value.version!==1||!Array.isArray(value.files)||value.files.length!==names.length)throw new Error('Unexpected codec fixture manifest');
    for(const name of names){const entries=value.files.filter(file=>file.name===name);if(entries.length!==1||!Number.isSafeInteger(entries[0].bytes)||entries[0].bytes<=0||entries[0].bytes>64*1024*1024||!/^[a-f0-9]{64}$/.test(entries[0].sha256))throw new Error('Invalid bounded codec fixture identity');}
    return value;
}
try {
    if(Number(sortsMode)+Number(preferencesMode)+Number(objectsMode)+Number(queriesMode)+Number(optionsMode)+Number(tablesMode)>1)throw new Error('Ambiguous codec acceptance modes');
    const vendor=await(await fetch('./vendor.json')).json();
    const config={moduleURL:new URL(vendor.worker.moduleURL,location.href).href,workerURL:new URL(vendor.worker.workerURL,location.href).href};
    const fixtureManifest=await manifest();
    const adapterPromise=createOfficialWorkerAdapter(config).then(value=>{
        if(sortsMode){const ordinary=value.submit.bind(value);value.submit=request=>request?.purpose==='workbook-sorts'?value.submitWorkbookSorts(request):ordinary(request);}
        if(objectsMode){const ordinary=value.submit.bind(value);value.submit=request=>request?.purpose==='workbook-objects'?value.submitWorkbookObjects(request):ordinary(request);}
        if(queriesMode||optionsMode||tablesMode){const ordinary=value.submit.bind(value);value.submit=request=>tablesMode&&['workbook-table-sources','workbook-table-filters','workbook-table-sorts'].includes(request?.purpose)?value.submitWorkbookTables(request):request?.purpose==='workbook-owners'?value.submitWorkbookOwners(request):request?.purpose==='workbook-objects'?value.submitWorkbookObjects(request):ordinary(request);}
        receipts.engineVersion=value.engineVersion;adapter=value;return value;
    });
    engine=createEngineHost(config,()=>adapterPromise);
    const options={print:message=>console.log(message),printErr:message=>console.error(message),dxEngine:engine,dxCodecSortsMode:sortsMode,dxCodecPreferencesMode:preferencesMode,dxCodecObjectsMode:objectsMode,dxCodecQueriesMode:queriesMode,dxCodecSourceOptionsMode:optionsMode,dxCodecTablesMode:tablesMode};
    options.dxCodecFixture=(runtime,cookie,ticket,name,attachSQL,detachSQL)=>{
        // Copy all C-owned strings before the first await.
        const file=String(name),attach=String(attachSQL),detach=String(detachSQL);
        Promise.resolve().then(async()=>{
            await adapterPromise;
            if(adapter.active||adapter.pending||adapter.draining)throw new Error('Codec fixture administration requires an idle adapter');
            if(registered){await adapter.connection.query(detach);await adapter.dropFile(registered.name);registered=null;}
            if(!file){callback(runtime,cookie,ticket,0);return;}
            const expected=fixtureManifest.files.find(entry=>entry.name===file);if(!expected)throw new Error('Unlisted codec fixture');
            const response=await fetch('/__codec-fixtures/'+encodeURIComponent(file),{cache:'no-store'});if(!response.ok)throw new Error('Codec fixture fetch failed');
            const bytes=new Uint8Array(await response.arrayBuffer());if(bytes.length!==expected.bytes)throw new Error('Codec fixture byte count changed');
            const sha=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),byte=>byte.toString(16).padStart(2,'0')).join('');
            if(sha!==expected.sha256)throw new Error('Codec fixture digest changed');
            registered=await adapter.registerFile(file,bytes);
            await adapter.connection.query(attach);
            receipts.fixtures.push({name:file,bytes:bytes.length,sha256:sha,engineEpoch:registered.engineEpoch,registrationId:registered.registrationId});
            callback(runtime,cookie,ticket,0,registered.engineEpoch,registered.registrationId,sha,bytes.length);
        }).catch(error=>callback(runtime,cookie,ticket,0,'','','',0,errorText(error)));
    };
    options.dxCodecSubmit=(runtime,cookie,ticket,request,hold)=>{
        const frozen=structuredClone(request);
        receipts.queries.push({ticket,sql:frozen.sql,sourceBindings:frozen.sourceBindings,...(frozen.purpose?{purpose:frozen.purpose,rowLimit:frozen.rowLimit}:preferencesMode?{rowLimit:frozen.rowLimit}:{})});
        let heldReady=false;
        if(hold){const original=runtime;runtime={...runtime,reply:(...args)=>{
            const [replyCookie,replyTicket,pointer,size,total,pending,error]=args;
            if(held)throw new Error('Only one codec reply may be held');
            held={runtime:original,cookie:replyCookie,ticket:replyTicket,data:pointer?original.heap().slice(pointer,pointer+size):null,size,total:total?original.string(total):null,pending,error:error?original.string(error):null};
            receipts.held++;heldReady=true;
        }};}
        Promise.resolve(engine.submit(runtime,cookie,ticket,frozen)).then(()=>{
            if(heldReady)callback(runtime,cookie,ticket,1);
        }).catch(error=>{console.error(error);globalThis.duxcelCodecReadResult={ok:false,error:errorText(error),transport:receipts};});
    };
    options.dxCodecRelease=()=>{
        if(!held)throw new Error('No held codec callback');
        const value=held;held=null;const runtime=value.runtime,owned=[];
        const bytes=data=>{const pointer=runtime.allocate(data.length||1);if(!pointer)throw new Error('Held codec allocation failed');owned.push(pointer);runtime.heap().set(data,pointer);return pointer;};
        const string=text=>text===null?0:bytes(encoder.encode(text+'\0'));
        try {runtime.reply(value.cookie,value.ticket,value.data?bytes(value.data):0,value.size,string(value.total),value.pending,string(value.error));receipts.released++;}
        finally {for(const pointer of owned)runtime.release(pointer);}
    };
    options.dxCodecReadTestDone=async result=>{
        try {await engine.cancel(true);globalThis.duxcelCodecReadResult={...result,transport:receipts};}
        catch(error){globalThis.duxcelCodecReadResult={ok:false,error:errorText(error),transport:receipts};}
    };
    module=await createDuxcel(options);
} catch(error) {if(engine)await engine.cancel(true).catch(()=>{});globalThis.duxcelCodecReadResult={ok:false,error:errorText(error),transport:receipts};console.error(error);}
