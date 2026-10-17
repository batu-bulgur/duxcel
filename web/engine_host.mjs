/** Browser platform/engine glue. SQL and workbook UI are owned by C99. */
import {createOfficialWorkerAdapter} from './official_worker.mjs';
import {encodePageWire} from './page_wire.mjs';
const encoder=new TextEncoder();
const FILE_LIMIT=1024;
class AllocationError extends Error {}
function allocated(runtime,bytes,owned){
    const pointer=runtime.allocate(bytes.byteLength||1);
    if(!pointer)throw new AllocationError('Not enough UI memory for the query reply.');
    owned.push(pointer);runtime.heap().set(bytes,pointer);return pointer;
}
function cString(runtime,text,owned){
    const bytes=encoder.encode(text),terminated=new Uint8Array(bytes.length+1);terminated.set(bytes);
    return allocated(runtime,terminated,owned);
}
function deliver(runtime,cookie,ticket,page,error,purpose){
    const owned=[];
    try{
        if(error){runtime.reply(cookie,ticket,0,0,0,0,cString(runtime,String(error).slice(0,2048),owned));return;}
        const wire=encodePageWire(page,{purpose}),pointer=allocated(runtime,wire,owned),total=cString(runtime,page.total,owned);
        runtime.reply(cookie,ticket,pointer,wire.length,total,page.countPending?1:0,0);
    }catch(error){
        if(!(error instanceof AllocationError))throw error;
        // C owns a static protocol-error fallback; it needs no transport malloc.
        runtime.reply(cookie,ticket,0,0,0,0,0);
    }finally{for(const pointer of owned)runtime.release(pointer);}
}
export function createEngineHost(config,createAdapter=createOfficialWorkerAdapter){
    let enginePromise,epoch=0,closed=false,fileEpoch=0,fileSequence=0,activeSubmission;
    const files=new Map(),pendingDrops=new Set();let dropScheduled=false;
    let administration=Promise.resolve();
    const administrate=operation=>{
        const next=administration.then(operation);administration=next.catch(()=>{});return next;
    };
    async function idleAdministration(){
        for(;;){const current=administration;await current;if(current===administration)return;}
    }
    const getEngine=()=>{
        if(closed)throw new Error('The browser engine is closed. Reload this page.');
        return enginePromise??=createAdapter(config);
    };
    const setFileStatus=(name,status)=>{if(files.has(name))files.set(name,status);};
    async function dropFile(engine,name){
        pendingDrops.delete(name);setFileStatus(name,'dropping');
        try{await engine.dropFile(name);setFileStatus(name,'released');}
        catch(error){setFileStatus(name,'failed');console.error(`Browser source cleanup failed for ${name}:`,error);}
    }
    function flushDrops(){
        if(closed||activeSubmission||dropScheduled||!pendingDrops.size||!enginePromise)return;
        dropScheduled=true;
        // Never await an active submission from inside administration: it may
        // itself be waiting for this barrier before entering the adapter.
        void administrate(async()=>{
            if(closed||activeSubmission)return;
            const engine=await enginePromise;
            for(const name of pendingDrops){if(closed||activeSubmission)break;await dropFile(engine,name);}
        }).then(()=>{dropScheduled=false;flushDrops();},error=>{
            dropScheduled=false;
            for(const name of pendingDrops)setFileStatus(name,'failed');pendingDrops.clear();
            console.error('Browser source cleanup failed:',error);
        });
    }
    function releaseFile(name){
        if(closed||typeof name!=='string'||name.length>255||encoder.encode(name).length>255||!/^import-[1-9][0-9]*-[^\\/\x00-\x1f\x7f]+$/.test(name))return false;
        const status=files.get(name);
        if(status==='queued'||status==='dropping'||status==='released')return true;
        if(status!=='registered')return false;
        files.set(name,'queued');pendingDrops.add(name);flushDrops();return true;
    }
    return {
        async submit(runtime,cookie,ticket,request){
            const current=++epoch,submission={runtime,cookie,ticket};activeSubmission=submission;
            try{
                const engine=await getEngine();await idleAdministration();if(current!==epoch||closed)return;
                const reply=await engine.submit(request);if(current!==epoch)return;
                for(const field of ['requestId','generation','documentId','sourceId','objectId','revision','offset'])
                    if(reply[field]!==request[field])throw new Error('Stale or mismatched browser query identity.');
                if(reply.status!=='ok')throw new Error(reply.error?.message??`Query ${reply.status}.`);
                deliver(runtime,cookie,ticket,reply.page,null,request.purpose);
            }catch(error){if(current===epoch&&!closed)deliver(runtime,cookie,ticket,null,error.message??error);}
            finally{if(activeSubmission===submission)activeSubmission=null;flushDrops();}
        },
        cancel(close=false){
            const current=++epoch;activeSubmission=null;if(close){closed=true;fileEpoch++;}
            // Reserve the barrier now, before a same-turn release can enqueue
            // a drop while the cancelled adapter query is still draining.
            if(enginePromise)return administrate(async()=>{
                const engine=await enginePromise;
                // Teardown is unconditional: a later epoch cannot revoke close.
                if(close)return engine.close();
                if(current===epoch)return engine.cancel();
            }).catch(error=>console.error('Browser engine cleanup failed:',error)).finally(()=>{if(close){pendingDrops.clear();files.clear();}else flushDrops();});
            if(close){pendingDrops.clear();files.clear();}
        },
        releaseFile,
        chooseFile(runtime,session){
            if(closed)return;
            const current=++fileEpoch;
            const input=document.createElement('input');input.type='file';input.id='duxcel-file-input';
            input.accept='.parquet,.csv,.tsv,.csv.gz';input.hidden=true;document.body.append(input);
            const finish=(name,label,error)=>{
                if(current!==fileEpoch||closed){input.remove();return false;}
                let delivered=true;const owned=[];try{runtime.reply(session,name?cString(runtime,name,owned):0,label?cString(runtime,label,owned):0,error?cString(runtime,error,owned):0);}
                catch(failure){if(!(failure instanceof AllocationError))throw failure;delivered=false;runtime.reply(session,0,0,0);}
                finally{for(const pointer of owned)runtime.release(pointer);input.remove();}
                return delivered;
            };
            input.addEventListener('cancel',()=>finish(null,null,null),{once:true});
            input.addEventListener('change',async()=>{
                const file=input.files?.[0];if(!file){finish(null,null,null);return;}
                let name;
                try{
                    if(file.size>256*1024*1024)throw new Error('This browser proof imports files up to 256 MiB.');
                    if(!/\.(parquet|tsv|csv(?:\.gz)?)$/i.test(file.name))throw new Error('Choose a CSV, TSV or Parquet source.');
                    const safe=file.name.replace(/[\\/\x00-\x1f\x7f]/g,'_');
                    if(encoder.encode(safe).length>200)throw new Error('Use a source filename of at most 200 UTF-8 bytes.');
                    if(files.size>=FILE_LIMIT)throw new Error('This browser engine session has reached its 1024 source-name limit. Start another Duxcel session to import more sources.');
                    name=`import-${++fileSequence}-${safe}`;files.set(name,'reading');
                    const bytes=new Uint8Array(await file.arrayBuffer());if(current!==fileEpoch||closed){setFileStatus(name,'unused');return;}
                    const engine=await getEngine();if(current!==fileEpoch||closed){setFileStatus(name,'unused');return;}
                    await administrate(async()=>{
                        if(current!==fileEpoch||closed){setFileStatus(name,'unused');return;}
                        const previous=activeSubmission;epoch++;activeSubmission=null;
                        if(previous)deliver(previous.runtime,previous.cookie,previous.ticket,null,'Query cancelled for source import.');
                        await engine.cancel();if(current!==fileEpoch||closed){setFileStatus(name,'unused');return;}
                        setFileStatus(name,'registering');
                        try{await engine.registerFile(name,bytes);}
                        catch(error){
                            // Worker registration may fail after adopting bytes.
                            // Complete best-effort cleanup under the same barrier
                            // before a waiting query or error callback can run.
                            await dropFile(engine,name);throw error;
                        }
                        setFileStatus(name,'registered');
                        if(current!==fileEpoch){await dropFile(engine,name);return;}
                        if(!closed&&!finish(name,file.name,null))releaseFile(name);
                    });
                }catch(error){if(name&&!['released','failed'].includes(files.get(name)))setFileStatus(name,'failed');finish(null,null,String(error.message??error));}
                finally{input.remove();flushDrops();}
            },{once:true});
            input.click();
        }
    };
}
