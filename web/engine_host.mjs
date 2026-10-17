/** Browser platform/engine glue. SQL and workbook UI are owned by C99. */
import {createOfficialWorkerAdapter} from './official_worker.mjs';
import {encodePageWire} from './page_wire.mjs';
const encoder=new TextEncoder();
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
function deliver(runtime,cookie,ticket,page,error){
    const owned=[];
    try{
        if(error){runtime.reply(cookie,ticket,0,0,0,0,cString(runtime,String(error).slice(0,2048),owned));return;}
        const wire=encodePageWire(page),pointer=allocated(runtime,wire,owned),total=cString(runtime,page.total,owned);
        runtime.reply(cookie,ticket,pointer,wire.length,total,page.countPending?1:0,0);
    }catch(error){
        if(!(error instanceof AllocationError))throw error;
        // C owns a static protocol-error fallback; it needs no transport malloc.
        runtime.reply(cookie,ticket,0,0,0,0,0);
    }finally{for(const pointer of owned)runtime.release(pointer);}
}
export function createEngineHost(config,createAdapter=createOfficialWorkerAdapter){
    let enginePromise,epoch=0,closed=false,fileEpoch=0,fileSequence=0,activeSubmission;
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
    return {
        async submit(runtime,cookie,ticket,request){
            const current=++epoch,submission={runtime,cookie,ticket};activeSubmission=submission;
            try{
                const engine=await getEngine();await idleAdministration();if(current!==epoch||closed)return;
                const reply=await engine.submit(request);if(current!==epoch)return;
                for(const field of ['requestId','generation','documentId','sourceId','objectId','revision','offset'])
                    if(reply[field]!==request[field])throw new Error('Stale or mismatched browser query identity.');
                if(reply.status!=='ok')throw new Error(reply.error?.message??`Query ${reply.status}.`);
                deliver(runtime,cookie,ticket,reply.page);
            }catch(error){if(current===epoch&&!closed)deliver(runtime,cookie,ticket,null,error.message??error);}
            finally{if(activeSubmission===submission)activeSubmission=null;}
        },
        cancel(close=false){
            const current=++epoch;activeSubmission=null;if(close){closed=true;fileEpoch++;}
            if(enginePromise)return enginePromise.then(async engine=>{
                await idleAdministration();
                // Teardown is unconditional: a later epoch cannot revoke close.
                if(close)return engine.close();
                if(current===epoch)return engine.cancel();
            }).catch(error=>console.error('Browser engine cleanup failed:',error));
        },
        chooseFile(runtime,session){
            if(closed)return;
            const current=++fileEpoch;
            const input=document.createElement('input');input.type='file';input.id='duxcel-file-input';
            input.accept='.parquet,.csv,.tsv,.csv.gz';input.hidden=true;document.body.append(input);
            const finish=(name,label,error)=>{
                if(current!==fileEpoch||closed){input.remove();return;}
                const owned=[];try{runtime.reply(session,name?cString(runtime,name,owned):0,label?cString(runtime,label,owned):0,error?cString(runtime,error,owned):0);}
                catch(failure){if(!(failure instanceof AllocationError))throw failure;runtime.reply(session,0,0,0);}
                finally{for(const pointer of owned)runtime.release(pointer);input.remove();}
            };
            input.addEventListener('cancel',()=>finish(null,null,null),{once:true});
            input.addEventListener('change',async()=>{
                const file=input.files?.[0];if(!file){finish(null,null,null);return;}
                try{
                    if(file.size>256*1024*1024)throw new Error('This browser proof imports files up to 256 MiB.');
                    if(!/\.(parquet|tsv|csv(?:\.gz)?)$/i.test(file.name))throw new Error('Choose a CSV, TSV or Parquet source.');
                    const safe=file.name.replace(/[\\/\x00-\x1f\x7f]/g,'_');
                    if(encoder.encode(safe).length>200)throw new Error('Use a source filename of at most 200 UTF-8 bytes.');
                    const name=`import-${++fileSequence}-${safe}`;
                    const bytes=new Uint8Array(await file.arrayBuffer());if(current!==fileEpoch||closed)return;
                    const engine=await getEngine();if(current!==fileEpoch||closed)return;
                    await administrate(async()=>{
                        if(current!==fileEpoch||closed)return;
                        const previous=activeSubmission;epoch++;activeSubmission=null;
                        if(previous)deliver(previous.runtime,previous.cookie,previous.ticket,null,'Query cancelled for source import.');
                        await engine.cancel();if(current!==fileEpoch||closed)return;
                        await engine.registerFile(name,bytes);
                        if(current!==fileEpoch){await engine.dropFile(name);return;}
                        if(!closed)finish(name,file.name,null);
                    });
                }catch(error){finish(null,null,String(error.message??error));}
                finally{input.remove();}
            },{once:true});
            input.click();
        }
    };
}
