import test from 'node:test';
import assert from 'node:assert/strict';
import {webcrypto} from 'node:crypto';
import {createReviewQueue,createIndexedDBReviewStore,REVIEW_QUEUE_LIMITS} from '../review_queue.mjs';
const reportId='R-browser-1',projectId='P-duxcel-test',token='review_token_0123456789';
const config={schema:'duxcel.review-service',version:1,project_id:projectId,token};
const blob=new Blob(['the first immutable C report archive'],{type:'application/x-tar'});
const reply=(value,status=200)=>new Response(JSON.stringify(value),{status,headers:{'Content-Type':'application/json'}});
const tick=()=>new Promise(resolve=>setImmediate(resolve));
function storeFixture(){
    const rows=new Map(),calls=[];
    return {rows,calls,
        async get(id){calls.push(['get',id]);return rows.get(id)??null;},
        async listPending(){calls.push(['pending']);return [...rows.values()].filter(row=>row.status==='pending').sort((a,b)=>a.createdAt-b.createdAt);},
        async putIfAbsent(row){calls.push(['put',row.reportId]);if(!rows.has(row.reportId))rows.set(row.reportId,structuredClone(row));return rows.get(row.reportId);},
        async markQueued(id,receipt){calls.push(['ack',id]);const old=rows.get(id),row={...old,status:'queued',bytes:0,receipt};delete row.blob;rows.set(id,row);return row;},
        close(){calls.push(['close']);}
    };
}
function timerFixture(){
    const timers=new Map();let serial=0;
    return {timers,setTimer(fn,delay){const id=++serial;timers.set(id,{fn,delay});return id;},clearTimer(id){timers.delete(id);},
        run(delay){const entry=[...timers].find(([,value])=>value.delay===delay);assert.ok(entry,`timer ${delay} exists`);timers.delete(entry[0]);entry[1].fn();}};
}
function harness(options={}){
    const store=options.store??storeFixture(),timers=timerFixture(),requests=[],listeners=new Map();
    const onlineTarget={addEventListener(type,handler){listeners.set(type,handler);},removeEventListener(type,handler){assert.equal(listeners.get(type),handler);listeners.delete(type);}};
    const fetch=async(url,request)=>{requests.push({url,request});if(url.endsWith('/config'))return reply(config);return reply({report_id:reportId,durable:true,queued:true,project_id:projectId});};
    const queue=createReviewQueue({store,crypto:webcrypto,baseURL:'http://127.0.0.1:8768/subdir/index.html',fetch,onlineTarget,...timers,autoStart:false,...options});
    return {queue,store,timers,requests,listeners};
}

test('service ACK follows the persistent commit and uses bounded same-origin authenticated requests',async()=>{
    const store=storeFixture(),requests=[];
    const h=harness({store,fetch:async(url,request)=>{
        requests.push({url,request});assert.equal(store.rows.get(reportId).status,'pending','the archive is committed before networking');
        assert.equal(request.mode,'same-origin');assert.equal(request.credentials,'same-origin');assert.equal(request.redirect,'error');assert.equal(request.cache,'no-store');
        assert.ok(request.signal instanceof AbortSignal);
        if(url.endsWith('/config'))return reply(config);
        assert.equal(url,'http://127.0.0.1:8768/__review/reports');assert.equal(request.method,'POST');
        assert.deepEqual(request.headers,{'Content-Type':'application/x-tar','X-Duxcel-Review-Token':token});assert.equal(await request.body.text(),await blob.text());
        return reply({report_id:reportId,durable:true,queued:true});
    }});
    assert.deepEqual(await h.queue.submit(blob,reportId,{projectId}),{report_id:reportId,project_id:projectId,durable:true,queued:true,local:false});
    assert.equal(requests.length,2);assert.equal(store.rows.get(reportId).status,'queued');assert.equal(store.rows.get(reportId).blob,undefined);
    assert.equal(h.timers.timers.size,0);h.queue.dispose();assert.equal(h.listeners.size,0);
});

test('offline commit is truthful local pending; startup after reload uploads the exact original archive',async()=>{
    const store=storeFixture(),first=harness({store,fetch:async()=>{throw new Error('offline');}});
    assert.deepEqual(await first.queue.submit(blob,reportId,{projectId}),{report_id:reportId,durable:true,queued:false,local:true});
    assert.equal(store.rows.get(reportId).status,'pending');assert.ok([...first.timers.timers.values()].some(timer=>timer.delay===1000));first.queue.dispose();
    const second=harness({store,autoStart:true});assert.equal(second.requests.length,0);second.timers.run(0);await second.queue.flush();
    assert.equal(second.requests.length,2);assert.equal(await second.requests[1].request.body.text(),await blob.text());assert.equal(store.rows.get(reportId).status,'queued');second.queue.dispose();
});

test('a retry or second tab cannot overwrite an immutable staged archive or duplicate a queued report',async()=>{
    let offline=true;const requests=[];
    const h=harness({fetch:async(url,request)=>{requests.push({url,request});if(offline)throw new Error('offline');return reply(url.endsWith('/config')?config:{report_id:reportId,durable:true,queued:true});}});
    await h.queue.submit(blob,reportId,{projectId});offline=false;
    const retry=await h.queue.submit(new Blob(['mutated note and timing must not replace original']),reportId,{projectId});
    assert.equal(retry.queued,true);const posts=requests.filter(value=>value.request.method==='POST');assert.equal(posts.length,1);assert.equal(await posts[0].request.body.text(),await blob.text());
    const count=requests.length;assert.equal((await h.queue.submit(blob,reportId,{projectId})).queued,true);assert.equal(requests.length,count);h.queue.dispose();
});

test('concurrent submissions share one flush and one first-writer archive',async()=>{
    const h=harness();const result=await Promise.all([h.queue.submit(blob,reportId,{projectId}),h.queue.submit(new Blob(['later']),reportId,{projectId})]);
    assert.ok(result.every(receipt=>receipt.queued));assert.equal(h.requests.filter(value=>value.request.method==='POST').length,1);h.queue.dispose();
});

test('online event retries local reports and disposal removes the listener and timers',async()=>{
    let offline=true;const h=harness({fetch:async url=>{if(offline)throw new Error('offline');return reply(url.endsWith('/config')?config:{report_id:reportId,durable:true,queued:true});}});
    await h.queue.submit(blob,reportId,{projectId});offline=false;h.listeners.get('online')();await h.queue.flush();assert.equal(h.store.rows.get(reportId).status,'queued');
    h.queue.dispose();assert.equal(h.listeners.size,0);assert.equal(h.timers.timers.size,0);await assert.rejects(h.queue.submit(blob,reportId,{projectId}),/closed/);
});

test('retry backoff is bounded and resets after a successful upload',async()=>{
    let offline=true;const h=harness({fetch:async url=>{if(offline)throw new Error('offline');return reply(url.endsWith('/config')?config:{report_id:reportId,durable:true,queued:true});}});
    await h.queue.submit(blob,reportId,{projectId});
    for(const delay of [1000,2000,4000,8000,16000,32000,60000,60000]){h.timers.run(delay);await h.queue.flush();}
    offline=false;h.timers.run(60000);await h.queue.flush();assert.equal(h.timers.timers.size,0);h.queue.dispose();
});

test('wrong report, non-durable, unqueued, malformed, oversized and rejected ACKs stay pending',async()=>{
    for(const acknowledgment of [()=>reply({report_id:'different',durable:true,queued:true}),()=>reply({report_id:reportId,durable:false,queued:true}),
        ()=>reply({report_id:reportId,durable:true,queued:false}),()=>reply({report_id:reportId,durable:true,queued:true,project_id:'different'}),
        ()=>new Response('{'),()=>new Response('x'.repeat(REVIEW_QUEUE_LIMITS.response+1)),()=>reply({error:'no'},403)]){
        const h=harness({fetch:async url=>url.endsWith('/config')?reply(config):acknowledgment()});
        assert.deepEqual(await h.queue.submit(blob,reportId,{projectId}),{report_id:reportId,durable:true,queued:false,local:true});assert.equal(h.store.rows.get(reportId).status,'pending');h.queue.dispose();
    }
});

test('wrong project or malformed service configuration never sends a report or token',async()=>{
    for(const value of [{...config,project_id:'another-project'},{...config,schema:'other'},{...config,version:2},{...config,token:'bad\r\nheader'},null]){
        const requests=[],h=harness({fetch:async(url,request)=>{requests.push(request);return reply(value);}});
        assert.equal((await h.queue.submit(blob,reportId,{projectId})).local,true);assert.equal(requests.length,1);assert.equal(requests[0].headers,undefined);h.queue.dispose();
    }
});

test('storage quota or transaction failure does not claim saved and does not attempt a network request',async()=>{
    for(const operation of ['get','putIfAbsent']){
        const store=storeFixture();store[operation]=async()=>{throw new Error('QuotaExceededError: transaction aborted');};const h=harness({store});
        await assert.rejects(h.queue.submit(blob,reportId,{projectId}),/QuotaExceeded/);assert.equal(h.requests.length,0);assert.equal(store.rows.size,0);h.queue.dispose();
    }
});

test('lost local receipt after server success retries exact bytes idempotently',async()=>{
    const store=storeFixture(),mark=store.markQueued;let fail=true;store.markQueued=async(...args)=>{if(fail)throw new Error('commit failed');return mark(...args);};const h=harness({store});
    assert.equal((await h.queue.submit(blob,reportId,{projectId})).local,true);fail=false;await h.queue.flush();
    const posts=h.requests.filter(value=>value.request.method==='POST');assert.equal(posts.length,2);assert.equal(await posts[0].request.body.text(),await posts[1].request.body.text());assert.equal(store.rows.get(reportId).status,'queued');h.queue.dispose();
});

test('invalid identifiers, oversized archive and another project cannot alter staged data',async()=>{
    const h=harness();
    for(const [id,project]of [['../escape',projectId],[reportId,''],[reportId,'p'.repeat(96)]])await assert.rejects(h.queue.submit(blob,id,{projectId:project}),/identity/);
    class Oversized extends Blob{get size(){return REVIEW_QUEUE_LIMITS.archive+1;}}
    await assert.rejects(h.queue.submit(new Oversized(),reportId,{projectId}),/limit/);assert.equal(h.store.rows.size,0);assert.equal(h.requests.length,0);
    await h.queue.submit(blob,reportId,{projectId});await assert.rejects(h.queue.submit(blob,reportId,{projectId:'other-project'}),/another project/);h.queue.dispose();
});

test('idle startup needs no service request and invalid stores/origins fail explicitly',async()=>{
    const h=harness({autoStart:true});h.timers.run(0);await h.queue.flush();assert.equal(h.requests.length,0);assert.equal(h.timers.timers.size,0);h.queue.dispose();
    for(const change of [{baseURL:'file:///tmp/index.html'},{store:{}},{fetch:null},{crypto:{}},{requestTimeoutMS:0}])assert.throws(()=>harness(change));
    const unavailable=createIndexedDBReviewStore({indexedDB:null});await assert.rejects(async()=>unavailable.get(reportId),/unavailable/);unavailable.close();
});

test('a hung service is aborted after a bounded deadline while the local report remains owned',async()=>{
    const h=harness({fetch:()=>new Promise(()=>{}),requestTimeoutMS:50});const submitting=h.queue.submit(blob,reportId,{projectId});
    for(let wait=0;wait<30&&![...h.timers.timers.values()].some(value=>value.delay===50);wait++)await tick();
    h.timers.run(50);const receipt=await submitting;assert.equal(receipt.local,true);assert.equal(h.store.rows.get(reportId).status,'pending');h.queue.dispose();
});
