/** Review-build-only transport. The C-owned report is immutable once staged.
 * IndexedDB commits precede uploads; only an acknowledged service POST proves
 * entry into the shared native/browser queue. No dependency or shipping import. */
export const REVIEW_QUEUE_LIMITS=Object.freeze({archive:134*1024*1024,pending:32,bytes:256*1024*1024,receipts:256,response:16*1024});
const identifier=value=>typeof value==='string'&&/^[A-Za-z0-9_-]{1,95}$/.test(value);
const digest=value=>typeof value==='string'&&/^[a-f0-9]{64}$/.test(value);
function check(ok,message){if(!ok)throw new Error(message);}
function errorText(error){return String(error?.message??error).slice(0,512);}
function validRecord(record){
    check(record&&identifier(record.reportId)&&identifier(record.projectId)&&digest(record.digest),'Invalid locally staged review identity.');
    check(record.status==='pending'||record.status==='queued','Invalid locally staged review state.');
    check(Number.isFinite(record.createdAt)&&record.createdAt>=0,'Invalid locally staged review timestamp.');
    if(record.status==='queued')check(record.receipt?.report_id===record.reportId&&record.receipt.durable===true&&record.receipt.queued===true&&record.receipt.project_id===record.projectId,'Invalid locally staged review receipt.');
    if(record.status==='pending')check(record.blob instanceof Blob&&record.blob.size>0&&record.blob.size<=REVIEW_QUEUE_LIMITS.archive&&record.bytes===record.blob.size,'Invalid locally staged review archive.');
    return record;
}

/** IDB requests and capacity checks share one read/write transaction, including
 * first-writer-wins retries from other tabs. No live token or workbook is stored. */
export function createIndexedDBReviewStore({indexedDB=globalThis.indexedDB,name='duxcel-review-pending-v1'}={}){
    let database=null,opening=null,closed=false;
    function open(){
        check(!closed,'Browser review storage is closed.');
        check(indexedDB&&typeof indexedDB.open==='function','Persistent browser review storage is unavailable.');
        if(database)return Promise.resolve(database);
        if(opening)return opening;
        opening=new Promise((resolve,reject)=>{
            const request=indexedDB.open(name,1);let abandoned=false;
            request.onupgradeneeded=()=>request.result.createObjectStore('reports',{keyPath:'reportId'});
            request.onerror=()=>reject(request.error??new Error('Cannot open browser review storage.'));
            request.onblocked=()=>{abandoned=true;reject(new Error('Browser review storage is blocked by another tab.'));};
            request.onsuccess=()=>{
                if(closed||abandoned){request.result.close();reject(new Error('Browser review storage is closed.'));return;}
                database=request.result;database.onversionchange=()=>{database.close();database=null;};
                database.onclose=()=>{if(database===request.result)database=null;};resolve(database);
            };
        }).finally(()=>{opening=null;});
        return opening;
    }
    async function transaction(mode,operation){
        const db=await open();
        return new Promise((resolve,reject)=>{
            let tx;
            try{tx=db.transaction('reports',mode,{durability:'strict'});}catch{tx=db.transaction('reports',mode);}
            let result,failure;
            tx.oncomplete=()=>resolve(result);
            tx.onerror=()=>reject(failure??tx.error??new Error('Browser review storage failed.'));
            tx.onabort=()=>reject(failure??tx.error??new Error('Browser review storage was not committed.'));
            const store=tx.objectStore('reports');
            function fail(error){failure=error;try{tx.abort();}catch{reject(error);}}
            try{operation(store,value=>{result=value;},fail);}catch(error){fail(error);}
        });
    }
    return {
        get(reportId){return transaction('readonly',(store,done)=>{const request=store.get(reportId);request.onsuccess=()=>done(request.result??null);});},
        listPending(){return transaction('readonly',(store,done,fail)=>{const request=store.getAll();request.onsuccess=()=>{try{done(request.result.filter(row=>row.status==='pending').map(validRecord).sort((a,b)=>a.createdAt-b.createdAt));}catch(error){fail(error);}};});},
        putIfAbsent(record){
            validRecord(record);
            return transaction('readwrite',(store,done,fail)=>{const request=store.getAll();request.onsuccess=()=>{
                try{
                    const existing=request.result.find(row=>row.reportId===record.reportId);
                    if(existing){done(validRecord(existing));return;}
                    const pending=request.result.filter(row=>row.status==='pending').map(validRecord);
                    check(pending.length<REVIEW_QUEUE_LIMITS.pending&&pending.reduce((sum,row)=>sum+row.bytes,0)+record.bytes<=REVIEW_QUEUE_LIMITS.bytes,'Browser review pending queue is full. Your note has not been saved.');
                    store.add(record);done(record);
                }catch(error){fail(error);}
            };});
        },
        markQueued(reportId,receipt){return transaction('readwrite',(store,done,fail)=>{const request=store.getAll();request.onsuccess=()=>{
            try{
                const row=request.result.find(value=>value.reportId===reportId);check(row,'Locally staged review disappeared before acknowledgment.');validRecord(row);
                const saved={reportId:row.reportId,projectId:row.projectId,digest:row.digest,createdAt:row.createdAt,status:'queued',bytes:0,receipt};
                store.put(saved);
                const receipts=request.result.filter(value=>value.status==='queued'&&value.reportId!==reportId).sort((a,b)=>b.createdAt-a.createdAt);
                for(const old of receipts.slice(REVIEW_QUEUE_LIMITS.receipts-1))store.delete(old.reportId);
                done(saved);
            }catch(error){fail(error);}
        };});},
        close(){closed=true;database?.close();database=null;}
    };
}

/** submit resolves success only after a local IDB commit. queued:true additionally
 * requires the matching service's durable ACK. Stored bytes survive reload and
 * retry without re-encoding, changing a note, or generating another report ID. */
export function createReviewQueue({store=createIndexedDBReviewStore(),fetch=globalThis.fetch,crypto=globalThis.crypto,
    baseURL=globalThis.location?.href,onlineTarget=globalThis,setTimer=setTimeout,clearTimer=clearTimeout,
    clock=Date.now,requestTimeoutMS=5000,autoStart=true}={}){
    check(store&&['get','listPending','putIfAbsent','markQueued'].every(key=>typeof store[key]==='function'),'Invalid review queue storage.');
    check(typeof fetch==='function'&&typeof crypto?.subtle?.digest==='function','Review queue requires browser networking and cryptography.');
    const base=new URL(baseURL);check(base.protocol==='http:'||base.protocol==='https:','The review service requires an HTTP origin.');
    check(Number.isFinite(requestTimeoutMS)&&requestTimeoutMS>0,'Invalid review service timeout.');
    const configURL=new URL('/__review/config',base),reportsURL=new URL('/__review/reports',base);
    let disposed=false,flushing=null,timer=null,failures=0;
    const controllers=new Set();
    function clearRetry(){if(timer!==null){clearTimer(timer);timer=null;}}
    function schedule(delay){if(disposed)return;clearRetry();timer=setTimer(()=>{timer=null;void flush();},delay);timer?.unref?.();}
    async function request(url,options={}){
        const controller=new AbortController();controllers.add(controller);
        let timeout;
        try{
            const timeoutPromise=new Promise((_,reject)=>{timeout=setTimer(()=>{controller.abort();reject(new Error('Review service timed out.'));},requestTimeoutMS);timeout?.unref?.();});
            const exchange=async()=>{
                const response=await fetch(url.href,{credentials:'same-origin',mode:'same-origin',redirect:'error',cache:'no-store',...options,signal:controller.signal});
                check(response?.ok,`Review service rejected the request (${response?.status??'unavailable'}).`);
                let text='';
                if(response.body&&typeof response.body.getReader==='function'){
                    const reader=response.body.getReader(),decoder=new TextDecoder('utf-8',{fatal:true});let bytes=0;
                    try{for(;;){const part=await reader.read();if(part.done)break;bytes+=part.value.byteLength;check(bytes<=REVIEW_QUEUE_LIMITS.response,'Review service response exceeds its limit.');text+=decoder.decode(part.value,{stream:true});}text+=decoder.decode();}
                    finally{reader.releaseLock();}
                }else{text=await response.text();check(new TextEncoder().encode(text).byteLength<=REVIEW_QUEUE_LIMITS.response,'Review service response exceeds its limit.');}
                return JSON.parse(text);
            };
            return await Promise.race([exchange(),timeoutPromise]);
        }catch(error){controller.abort();throw error;}
        finally{clearTimer(timeout);controllers.delete(controller);}
    }
    function localReceipt(row){return {report_id:row.reportId,durable:true,queued:false,local:true};}
    async function runFlush(){
        const attempted=new Set(),errors=new Map();let config;
        try{
            for(;;){
                const pending=(await store.listPending()).filter(row=>!attempted.has(row.reportId));
                if(disposed||!pending.length)break;
                if(!config){
                    config=await request(configURL);
                    check(config?.schema==='duxcel.review-service'&&config.version===1&&identifier(config.project_id)&&typeof config.token==='string'&&/^[A-Za-z0-9_-]{16,256}$/.test(config.token),'Invalid review service configuration.');
                }
                for(const row of pending){
                    if(disposed)break;attempted.add(row.reportId);
                    try{
                        validRecord(row);check(row.projectId===config.project_id,'The review service belongs to another project.');
                        const receipt=await request(reportsURL,{method:'POST',headers:{'Content-Type':'application/x-tar','X-Duxcel-Review-Token':config.token},body:row.blob});
                        check(receipt?.report_id===row.reportId&&receipt.durable===true&&receipt.queued===true&&(receipt.project_id===undefined||receipt.project_id===row.projectId),'The review service did not acknowledge this report durably.');
                        await store.markQueued(row.reportId,{report_id:row.reportId,project_id:row.projectId,durable:true,queued:true,local:false});
                    }catch(error){errors.set(row.reportId,errorText(error));}
                }
            }
            const pending=await store.listPending();
            if(pending.length){failures++;schedule(Math.min(60000,1000*2**Math.min(failures-1,6)));}else{failures=0;clearRetry();}
        }catch(error){failures++;schedule(Math.min(60000,1000*2**Math.min(failures-1,6)));errors.set('*',errorText(error));}
        return {attempted,errors};
    }
    function flush(){
        if(disposed)return Promise.resolve({attempted:new Set(),errors:new Map()});
        if(flushing)return flushing;
        clearRetry();flushing=runFlush().finally(()=>{flushing=null;});return flushing;
    }
    const online=()=>{failures=0;void flush();};onlineTarget?.addEventListener?.('online',online);
    if(autoStart)schedule(0);
    return {
        async submit(blob,reportId,{projectId}={}){
            check(!disposed,'Review queue is closed.');check(identifier(reportId)&&identifier(projectId),'Invalid review queue identity.');
            check(blob instanceof Blob&&blob.size>0&&blob.size<=REVIEW_QUEUE_LIMITS.archive,'Review archive exceeds the browser pending-queue limit.');
            let row=await store.get(reportId);
            if(!row){
                const hash=new Uint8Array(await crypto.subtle.digest('SHA-256',await blob.arrayBuffer()));
                check(hash.length===32,'Invalid review archive digest.');
                const checksum=Array.from(hash,byte=>byte.toString(16).padStart(2,'0')).join('');
                row=await store.putIfAbsent({reportId,projectId,digest:checksum,blob,bytes:blob.size,createdAt:clock(),status:'pending'});
            }
            validRecord(row);check(row.projectId===projectId,'This report ID is already bound to another project.');
            if(row.status==='queued')return {...row.receipt};
            const result=await flush();
            row=await store.get(reportId);validRecord(row);
            // A startup flush may finish just before staging became visible.
            if(row.status==='pending'&&!disposed&&!result.attempted.has(reportId)&&!result.errors.size){await flush();row=await store.get(reportId);validRecord(row);}
            return row.status==='queued'?{...row.receipt}:localReceipt(row);
        },
        flush,
        dispose(){if(disposed)return;disposed=true;clearRetry();for(const controller of controllers)controller.abort();onlineTarget?.removeEventListener?.('online',online);store.close?.();}
    };
}
