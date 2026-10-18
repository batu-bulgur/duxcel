import test from 'node:test';
import assert from 'node:assert/strict';
import {Table,Int32,Uint64,Utf8,Bool,vectorFromArray} from 'apache-arrow';
import {DuckDBWorkerAdapter,LIMITS,pageRequest,pageCellText,encodePage,workbookMetadataStreamRequest} from './official_worker.mjs';

const deferred=()=>{let resolve,reject;const promise=new Promise((yes,no)=>{resolve=yes;reject=no;});return {promise,resolve,reject};};
const columns=()=>[
    {name:'sheet_id',typeName:'INTEGER',numberKind:'i32'},
    {name:'row_id',typeName:'UBIGINT',numberKind:'u64'},
    {name:'text',typeName:'VARCHAR',numberKind:'none'},
    {name:'enabled',typeName:'BOOLEAN',numberKind:'none'},
];
const input=(token,family='cells',id='1',change={})=>({requestId:id,documentId:'7',generation:'9007199254740993',revision:'18446744073709551615',
    purpose:'workbook-metadata-stream',family,offset:'0',knownTotal:null,sql:'C ORDERED METADATA QUERY, NO OFFSET PAGING',
    columns:columns(),sourceBindings:[{...(token??{name:'book.dxls',engineEpoch:'1',registrationId:'1'})}],...change});
function table(rows,label=r=>[null,'','NULL',"İstanbul 🦆 'quoted'"][r%4]){
    return new Table({sheet_id:vectorFromArray(Array.from({length:rows},(_,r)=>r%64),new Int32()),
        row_id:vectorFromArray(Array.from({length:rows},(_,r)=>r%2?9007199254740993n:18446744073709551615n),new Uint64()),
        text:vectorFromArray(Array.from({length:rows},(_,r)=>label(r)),new Utf8()),
        enabled:vectorFromArray(Array.from({length:rows},(_,r)=>r%3===2?null:r%3===0),new Bool())});
}
function reader(source,lengths=[source.numRows],options={}){
    let first=0,index=0;const batches=lengths.map(count=>{
        if(!count)return {numCols:4,numRows:0};
        const batch=source.slice(first,first+count).batches[0];first+=count;return batch;
    });assert.equal(first,source.numRows);
    return {schema:source.schema,nextCalls:0,cancelled:0,reads:0,open(){return this;},
        [Symbol.asyncIterator](){return {next:async()=>{
            this.nextCalls++;if(options.next)return options.next(index++);
            if(index===batches.length)return {done:true};const batch=batches[index++];
            if(!batch.numRows)return {value:batch,done:false};
            return {done:false,value:{numCols:batch.numCols,numRows:batch.numRows,getChildAt:c=>({get:r=>{this.reads++;return batch.getChildAt(c).get(r);}})}};
        }};},
        async cancel(){this.cancelled++;if(options.cleanup)await options.cleanup();}
    };
}
function harness(source,options={}){
    const db=options.database??{registerCalls:0,dropCalls:0,async registerFileBuffer(){this.registerCalls++;},async dropFile(){this.dropCalls++;}};
    const connection={sends:0,cancels:0,closed:0,async send(sql,stream){this.sends++;assert.equal(stream,true);return source;},async cancelSent(){this.cancels++;return true;},async close(){this.closed++;}};
    const adapter=new DuckDBWorkerAdapter({database:db,connection,...(options.dispose?{dispose:options.dispose}:{})});return {adapter,db,connection};
}
async function bound(h){return h.adapter.registerFile('book.dxls',new Uint8Array([1,2]));}
const footprint=p=>new TextEncoder().encode(JSON.stringify(p.columns)).byteLength+p.text.byteLength+p.offsets.byteLength+p.nulls.byteLength;

test('metadata stream factory is dense, frozen, exact and isolated from ordinary admission',async()=>{
    for(const [family,max] of [['columns',1048577],['cells',1280001]]){
        const value=input(undefined,family),request=workbookMetadataStreamRequest(value);
        assert.equal(request.rowLimit,max);assert.equal(request.family,family);assert.equal(request.offset,'0');assert.equal(request.knownTotal,null);
        assert.ok(Object.isFrozen(request)&&Object.isFrozen(request.columns)&&Object.isFrozen(request.columns[0])&&Object.isFrozen(request.sourceBindings[0]));
        value.columns[0].name='mutated';value.sourceBindings[0].registrationId='2';assert.equal(request.columns[0].name,'sheet_id');assert.equal(request.sourceBindings[0].registrationId,'1');
        const holes=columns();delete holes[1];const inherited=columns();delete inherited[1];Object.setPrototypeOf(inherited,Object.assign(Object.create(Array.prototype),{1:columns()[1]}));
        for(const change of [{purpose:undefined},{purpose:'copy'},{family:'__proto__'},{family:'constructor'},{family:new String(family)},
            {rowLimit:0},{rowLimit:max+1},{rowLimit:1.5},{rowLimit:NaN},{offset:'1'},{knownTotal:'0'},
            {columns:holes},{columns:inherited},{columns:Array(4)},{columns:columns().slice(1)},{columns:[...columns(),columns()[0]]},
            {sourceBindings:[]},{sourceBindings:Array(1)},{sourceBindings:[{},{}]},{sourceBindings:undefined},{sql:'a\0b'}])
            assert.throws(()=>workbookMetadataStreamRequest(input(undefined,family,'1',change)),{code:'INVALID_REQUEST'});
        assert.equal(workbookMetadataStreamRequest(input(undefined,family,'1',{rowLimit:1})).rowLimit,1);
        let purposes=0,families=0;const getters=input(undefined,family);Object.defineProperty(getters,'purpose',{enumerable:true,get(){return ++purposes===1?'workbook-metadata-stream':'copy';}});Object.defineProperty(getters,'family',{enumerable:true,get(){return ++families===1?family:'__proto__';}});
        const captured=workbookMetadataStreamRequest(getters);assert.equal(captured.family,family);assert.equal(captured.purpose,'workbook-metadata-stream');assert.equal(captured.rowLimit,max);
        assert.throws(()=>pageRequest(input(undefined,family)),{code:'INVALID_REQUEST'});
        await assert.rejects(encodePage(reader(table(1)),request),{code:'INVALID_REQUEST'});
        const h=harness(reader(table(1)));assert.throws(()=>h.adapter.submitWorkbookMetadataStream(input(undefined,family,'1',{columns:holes}),()=>true),{code:'INVALID_REQUEST'});
        assert.throws(()=>h.adapter.submitWorkbookMetadataStream(input(undefined,family),null),{code:'INVALID_REQUEST'});assert.equal(h.adapter.latestId,0n);assert.equal(h.connection.sends,0);await h.adapter.close();
    }
});
test('one immutable Arrow reader yields irregular batches into exact owned chunks and one post-cleanup EOF',async()=>{
    const source=reader(table(513),[0,1,255,0,256,1]),h=harness(source),token=await bound(h),chunks=[];
    const original=input(token),pending=h.adapter.submitWorkbookMetadataStream(original,chunk=>{
        assert.ok(Object.isFrozen(chunk)&&Object.isFrozen(chunk.page)&&Object.isFrozen(chunk.page.columns[0]));assert.ok(footprint(chunk.page)<=LIMITS.bytes&&chunk.wire.byteLength<=6*1024*1024);
        if(chunk.end)assert.equal(source.cancelled,1);chunks.push(chunk);return true;
    });original.columns[0].name='changed';original.sourceBindings[0].registrationId='99';const result=await pending;
    assert.equal(result.status,'ok');assert.equal(result.rowCount,'513');assert.equal(result.dataChunks,3);assert.equal(result.chunks,4);assert.equal(h.connection.sends,1);assert.equal(source.nextCalls,7);assert.equal(source.cancelled,1);
    assert.deepEqual(chunks.map(c=>[c.sequence,c.firstRow,c.end,c.page.rowCount]),[[1,'0',false,256],[2,'256',false,256],[3,'512',false,1],[4,'513',true,0]]);
    for(const c of chunks)assert.equal(c.page.columns[0].name,'sheet_id');
    const p=chunks[0].page;assert.equal(pageCellText(p,0,1),'18446744073709551615');assert.equal(pageCellText(p,1,1),'9007199254740993');assert.equal(pageCellText(p,0,2),null);assert.equal(pageCellText(p,1,2),'');assert.equal(pageCellText(p,2,2),'NULL');assert.equal(pageCellText(p,3,2),"İstanbul 🦆 'quoted'");assert.equal(pageCellText(p,2,3),null);
    await h.adapter.close();
});
test('backpressure permits no next batch, row conversion or second delivery until true ACK',async()=>{
    const source=reader(table(512)),h=harness(source),token=await bound(h),entered=deferred(),ack=deferred();let calls=0;
    const pending=h.adapter.submitWorkbookMetadataStream(input(token),chunk=>{calls++;if(calls===1){entered.resolve();return ack.promise;}return true;});await entered.promise;
    assert.equal(source.nextCalls,1);assert.equal(source.reads,256*4);assert.equal(calls,1);assert.equal(source.cancelled,0);await Promise.resolve();assert.equal(source.reads,256*4);
    ack.resolve(true);const reply=await pending;assert.equal(reply.status,'ok');assert.equal(source.nextCalls,2);assert.equal(calls,3);assert.equal(source.cancelled,1);await h.adapter.close();
});
test('byte boundaries split rows without requery; a single oversized or malformed row never delivers',async()=>{
    const source=reader(table(3,()=> 'x'.repeat(2*1024*1024))),h=harness(source),token=await bound(h),shapes=[];
    const reply=await h.adapter.submitWorkbookMetadataStream(input(token),chunk=>{shapes.push([chunk.firstRow,chunk.page.rowCount,chunk.end]);assert.ok(footprint(chunk.page)<=LIMITS.bytes);return true;});
    assert.equal(reply.status,'ok');assert.equal(h.connection.sends,1);assert.deepEqual(shapes,[['0',1,false],['1',1,false],['2',1,false],['3',0,true]]);await h.adapter.close();
    for(const [value,code] of [['x'.repeat(LIMITS.bytes),'PAGE_LIMIT'],['a\0b','UNSUPPORTED_TYPE']]){
        const source=reader(table(1,()=>value)),h=harness(source),token=await bound(h);let calls=0;const reply=await h.adapter.submitWorkbookMetadataStream(input(token),()=>{calls++;return true;});
        assert.equal(reply.status,'error');assert.equal(reply.error.code,code);assert.equal(calls,0);assert.equal(source.cancelled,1);assert.equal(h.connection.sends,1);await h.adapter.close();
    }
});
test('false, throwing and rejected ACKs stop without EOF and allow ordinary recovery',async()=>{
    for(const reject of [()=>false,()=>{throw new Error('consumer fault');},()=>Promise.reject(new Error('async fault'))]){
        const source=reader(table(600)),h=harness(source),token=await bound(h);let calls=0;const result=await h.adapter.submitWorkbookMetadataStream(input(token),chunk=>{calls++;assert.equal(chunk.end,false);return reject();});
        assert.equal(result.status,'error');assert.equal(calls,1);assert.equal(source.nextCalls,1);assert.equal(source.reads,1024);assert.equal(source.cancelled,1);
        h.connection.send=async()=>{h.connection.sends++;return reader(table(1));};assert.equal((await h.adapter.submit({requestId:'2',sql:'ordinary recovery'})).status,'ok');await h.adapter.close();
    }
});
test('cancel, latest-wins and close abort an unacknowledged consumer without awaiting its late resolution',async()=>{
    for(const action of ['cancel','supersede','close']){
        const source=reader(table(600)),h=harness(source),token=await bound(h),entered=deferred(),ack=deferred();let calls=0;
        const pending=h.adapter.submitWorkbookMetadataStream(input(token),()=>{calls++;entered.resolve();return ack.promise;});await entered.promise;
        let stopping;if(action==='supersede'){h.connection.send=async()=>reader(table(1));stopping=h.adapter.submit({requestId:'2',sql:'ordinary superseding recovery'});}else stopping=action==='close'?h.adapter.close():h.adapter.cancel();
        const reply=await pending;await stopping;assert.equal(reply.status,action==='supersede'?'superseded':'cancelled');assert.equal(calls,1);assert.equal(source.nextCalls,1);assert.equal(source.cancelled,1);
        ack.reject(new Error('late rejected ACK is already observed'));await new Promise(resolve=>setImmediate(resolve));assert.equal(calls,1);if(action!=='close')await h.adapter.close();
    }
});
test('cancellation retires the one queued MessageChannel yield and cannot advance after a late event',async()=>{
    const Real=globalThis.MessageChannel,posted=deferred();let closes=0,messages=0,channel;
    globalThis.MessageChannel=class {constructor(){channel=this;this.port1={onmessage:null,close(){closes++;}};this.port2={postMessage(){messages++;posted.resolve();},close(){closes++;}};}};
    try{
        const source=reader(table(600)),h=harness(source),token=await bound(h);let calls=0;const pending=h.adapter.submitWorkbookMetadataStream(input(token),()=>{calls++;return true;});await posted.promise;assert.equal(messages,1);assert.equal(calls,1);
        await h.adapter.cancel();assert.equal((await pending).status,'cancelled');assert.equal(closes,2);channel.port1.onmessage();await Promise.resolve();assert.equal(calls,1);assert.equal(source.nextCalls,1);await h.adapter.close();
    }finally{globalThis.MessageChannel=Real;}
});
test('revocation including failed drop invalidates the captured token after ACK and before another chunk',async()=>{
    for(const failDrop of [false,true]){
        const db={async registerFileBuffer(){},async dropFile(){if(failDrop)throw new Error('drop failed');}},source=reader(table(600)),h=harness(source,{database:db}),token=await bound(h),entered=deferred(),ack=deferred();
        const sibling=harness(reader(table(1)),{database:db});let calls=0;const pending=h.adapter.submitWorkbookMetadataStream(input({...token}),()=>{calls++;entered.resolve();return ack.promise;});await entered.promise;
        if(failDrop)await assert.rejects(sibling.adapter.dropFile(token.name),/drop failed/);else await sibling.adapter.dropFile(token.name);
        ack.resolve(true);const reply=await pending;assert.equal(reply.status,'error');assert.equal(reply.error.code,'SOURCE_INVALIDATED');assert.equal(calls,1);assert.equal(source.cancelled,1);
        const last=h.adapter.latestId;assert.throws(()=>h.adapter.submitWorkbookMetadataStream(input(token,'cells','2'),()=>true),{code:'SOURCE_INVALIDATED'});assert.equal(h.adapter.latestId,last);assert.equal(h.connection.sends,1);await h.adapter.close();await sibling.adapter.close();
    }
});
test('EOF waits for strict cleanup and final ACK, then rechecks the original registration',async()=>{
    const cleanup=deferred(),source=reader(table(0),[],{cleanup:()=>cleanup.promise}),h=harness(source),token=await bound(h),entered=deferred(),ack=deferred();let calls=0;
    const sibling=harness(reader(table(1)),{database:h.db}),pending=h.adapter.submitWorkbookMetadataStream(input(token),chunk=>{calls++;assert.equal(chunk.end,true);assert.equal(source.cancelled,1);entered.resolve();return ack.promise;});
    await new Promise(resolve=>setImmediate(resolve));assert.equal(source.cancelled,1);assert.equal(calls,0);cleanup.resolve();await entered.promise;assert.equal(calls,1);await sibling.adapter.dropFile(token.name);ack.resolve(true);
    const result=await pending;assert.equal(result.status,'error');assert.equal(result.error.code,'SOURCE_INVALIDATED');assert.equal(calls,1);await h.adapter.close();await sibling.adapter.close();
});
test('strict cleanup failure poisons every shared-engine admission before ticket or registration mutation',async()=>{
    const source=reader(table(1),[1],{cleanup:()=>{throw new Error('reader did not close');}}),h=harness(source),token=await bound(h),sibling=harness(reader(table(1)),{database:h.db});let ends=0;
    const result=await h.adapter.submitWorkbookMetadataStream(input(token),chunk=>{if(chunk.end)ends++;return true;});assert.equal(result.status,'error');assert.equal(result.error.code,'STREAM_CLEANUP');assert.equal(source.cancelled,1);assert.equal(ends,0);
    const last=h.adapter.latestId;for(const adapter of [h.adapter,sibling.adapter]){
        assert.throws(()=>adapter.submit({requestId:'2',sql:'must never send'}),{code:'STREAM_CLEANUP'});
        assert.throws(()=>adapter.submitWorkbookMetadataStream(input(token,'cells','2'),()=>true),{code:'STREAM_CLEANUP'});
        assert.throws(()=>adapter.exportParquet({requestId:'2',documentId:'7',sql:'C COPY',rowLimit:1,offset:'0',knownTotal:'1',fileName:'export-7-2.parquet'}),{code:'STREAM_CLEANUP'});
        await assert.rejects(adapter.registerFile('new.dxls',new Uint8Array([1])),{code:'STREAM_CLEANUP'});await assert.rejects(adapter.dropFile(token.name),{code:'STREAM_CLEANUP'});
    }
    assert.equal(h.adapter.latestId,last);assert.equal(h.connection.sends,1);assert.equal(sibling.connection.sends,0);assert.equal(h.db.registerCalls,1);assert.equal(h.db.dropCalls,0);
    await h.adapter.close();await sibling.adapter.close();assert.equal(h.connection.closed,1);
    const fresh=harness(reader(table(1))),freshToken=await bound(fresh);assert.equal((await fresh.adapter.submitWorkbookMetadataStream(input(freshToken),()=>true)).status,'ok');await fresh.adapter.close();
});
test('whole-family overflow, schema error and pending reader cancellation do not invent an EOF',async()=>{
    for(const family of ['columns','cells']){
        const cap=family==='columns'?1048577:1280001,source=reader(table(0),[],{next:()=>({done:false,value:{numRows:cap+1,numCols:4}})}),h=harness(source),token=await bound(h);let calls=0;
        const reply=await h.adapter.submitWorkbookMetadataStream(input(token,family),()=>{calls++;return true;});assert.equal(reply.error.code,'PAGE_LIMIT');assert.equal(calls,0);assert.equal(source.cancelled,1);await h.adapter.close();
    }
    const waiting=deferred(),entered=deferred(),source=reader(table(0),[],{next:()=>{entered.resolve();return waiting.promise;}}),h=harness(source),token=await bound(h);let calls=0;
    const pending=h.adapter.submitWorkbookMetadataStream(input(token),()=>{calls++;return true;});await entered.promise;await h.adapter.cancel();assert.equal((await pending).status,'cancelled');assert.equal(calls,0);assert.equal(source.cancelled,1);waiting.reject(new Error('late reader failure'));await new Promise(resolve=>setImmediate(resolve));await h.adapter.close();
    const bad={schema:table(1).schema,cancelled:0,[Symbol.asyncIterator](){return {next:async()=>({done:false,value:{numRows:1,numCols:3}})};},async cancel(){this.cancelled++;}},b=harness(bad),t=await bound(b);assert.equal((await b.adapter.submitWorkbookMetadataStream(input(t),()=>true)).error.code,'SCHEMA_CHANGED');assert.equal(bad.cancelled,1);await b.adapter.close();
});
test('a previously queued ordinary task cannot send after a cancelled stream fails strict cleanup',async()=>{
    const source=reader(table(600),[600],{cleanup:()=>{throw new Error('unproved cancelled cleanup');}}),h=harness(source),token=await bound(h),entered=deferred(),ack=deferred();
    const pending=h.adapter.submitWorkbookMetadataStream(input(token),()=>{entered.resolve();return ack.promise;});await entered.promise;
    const queued=h.adapter.submit({requestId:'2',sql:'already queued, must never send'});const old=await pending,next=await queued;
    assert.equal(old.error.code,'STREAM_CLEANUP');assert.equal(next.status,'error');assert.equal(next.error.code,'STREAM_CLEANUP');assert.equal(h.connection.sends,1);assert.equal(source.cancelled,1);
    const last=h.adapter.latestId;assert.throws(()=>h.adapter.submit({requestId:'3',sql:'poison must not reserve ticket'}),{code:'STREAM_CLEANUP'});assert.equal(h.adapter.latestId,last);ack.resolve(true);await h.adapter.close();
});
test('late open rejection and late next resolution after cancellation cannot resume row conversion',async()=>{
    const opening=deferred(),entered=deferred(),source=reader(table(1));source.open=()=>{entered.resolve();return opening.promise;};
    const h=harness(source),token=await bound(h);let calls=0;const pending=h.adapter.submitWorkbookMetadataStream(input(token),()=>{calls++;return true;});await entered.promise;await h.adapter.cancel();assert.equal((await pending).status,'cancelled');assert.equal(source.cancelled,1);assert.equal(source.nextCalls,0);
    opening.reject(new Error('late open failure already observed'));await new Promise(resolve=>setImmediate(resolve));assert.equal(calls,0);await h.adapter.close();
    const next=deferred(),reading=deferred(),r=reader(table(0),[],{next:()=>{reading.resolve();return next.promise;}}),b=harness(r),t=await bound(b);
    const old=b.adapter.submitWorkbookMetadataStream(input(t),()=>{calls++;return true;});await reading.promise;await b.adapter.cancel();assert.equal((await old).status,'cancelled');
    next.resolve({done:false,value:new Proxy({},{get(){throw new Error('Expired Arrow batch was accessed');}})});await new Promise(resolve=>setImmediate(resolve));assert.equal(calls,0);assert.equal(r.reads,0);assert.equal(r.cancelled,1);await b.adapter.close();
});
test('a missing reader cancel capability cannot acknowledge EOF or admit later work',async()=>{
    const source=reader(table(0),[]);delete source.cancel;const h=harness(source),token=await bound(h);let calls=0;
    const result=await h.adapter.submitWorkbookMetadataStream(input(token),()=>{calls++;return true;});assert.equal(result.status,'error');assert.equal(result.error.code,'STREAM_CLEANUP');assert.equal(calls,0);
    assert.throws(()=>h.adapter.submit({requestId:'2',sql:'missing cleanup cannot send'}),{code:'STREAM_CLEANUP'});assert.equal(h.connection.sends,1);await h.adapter.close();assert.equal(h.connection.closed,1);
});
