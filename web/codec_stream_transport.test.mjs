import test from 'node:test';
import assert from 'node:assert/strict';
import {Table,Int32,Uint64,Float64,Utf8,vectorFromArray} from 'apache-arrow';
import {DuckDBWorkerAdapter} from './official_worker.mjs';
import {createCodecStreamTransport} from './codec_stream_transport.mjs';

const decoder=new TextDecoder('utf-8',{fatal:true}),encoder=new TextEncoder();
const deferred=()=>{let resolve,reject;const promise=new Promise((yes,no)=>{resolve=yes;reject=no;});return {promise,resolve,reject};};
const tick=()=>new Promise(resolve=>setImmediate(resolve));
function descriptors(family='cells'){
    return family==='cells'?[{name:'sheet_id',typeName:'INTEGER',numberKind:'i32'},{name:'row_id',typeName:'UBIGINT',numberKind:'u64'},
        {name:'column_id',typeName:'INTEGER',numberKind:'i32'},{name:'text',typeName:'VARCHAR',numberKind:'none'}]:
        [{name:'sheet_id',typeName:'INTEGER',numberKind:'i32'},{name:'column_id',typeName:'INTEGER',numberKind:'i32'},
            {name:'name',typeName:'VARCHAR',numberKind:'none'},{name:'width',typeName:'DOUBLE',numberKind:'float'}];
}
function input(token,family='cells',ticket=1,change={}){
    return {purpose:'workbook-metadata-stream',family,documentId:'7',requestId:String(ticket),generation:'9007199254740993',sourceId:'0',objectId:'0',revision:'18446744073709551615',offset:'0',knownTotal:null,
        sql:'C OWNED ORDERED METADATA, NOT REWRITTEN',columns:descriptors(family),sourceBindings:[{...(token??{name:'fixture.dxls',engineEpoch:'1',registrationId:'1'})}],...change};
}
function table(rows,family='cells'){
    const labels=[null,'','NULL',"'İ' 🦆;\n",'9007199254740993','18446744073709551615'];
    return family==='cells'?new Table({sheet_id:vectorFromArray(Array.from({length:rows},(_,r)=>r%64),new Int32()),
        row_id:vectorFromArray(Array.from({length:rows},(_,r)=>r%2?9007199254740993n:18446744073709551615n),new Uint64()),
        column_id:vectorFromArray(Array.from({length:rows},(_,r)=>r%16384),new Int32()),text:vectorFromArray(Array.from({length:rows},(_,r)=>labels[r%6]),new Utf8())}):
        new Table({sheet_id:vectorFromArray(Array.from({length:rows},(_,r)=>r%64),new Int32()),column_id:vectorFromArray(Array.from({length:rows},(_,r)=>r),new Int32()),
            name:vectorFromArray(Array.from({length:rows},(_,r)=>labels[r%6]),new Utf8()),width:vectorFromArray(Array.from({length:rows},(_,r)=>52+r/8),new Float64())});
}
function reader(source,lengths=[source.numRows],options={}){
    let first=0,index=0;const batches=lengths.map(count=>{if(!count)return {numRows:0,numCols:4};const batch=source.slice(first,first+count).batches[0];first+=count;return batch;});assert.equal(first,source.numRows);
    return {schema:source.schema,cancelled:0,reads:0,nextCalls:0,open(){return this;},[Symbol.asyncIterator](){return {next:async()=>{
        this.nextCalls++;if(options.next)await options.next(index);if(index===batches.length)return {done:true};const batch=batches[index++];
        return {done:false,value:{numCols:4,numRows:batch.numRows,getChildAt:c=>({get:r=>{options.read?.(this.reads);this.reads++;return batch.getChildAt(c).get(r);}})}};
    }};},async cancel(){this.cancelled++;await options.cleanup?.();}};
}
function inspect(wire){
    const view=new DataView(wire.buffer,wire.byteOffset,wire.byteLength);let at=0;const word=()=>{const v=view.getUint32(at,true);at+=4;return v;},bytes=n=>{const v=wire.subarray(at,at+n);at+=n;return v;};
    assert.equal(word(),0x31505844);assert.equal(word(),1);const columns=word(),rows=word(),textBytes=word(),metadata=[];
    for(let c=0;c<columns;c++){const name=word(),type=word(),kind=word(),precision=word(),scale=word();metadata.push({name:decoder.decode(bytes(name)),typeName:decoder.decode(bytes(type)),numberKind:decoder.decode(bytes(kind)),precision,scale});}
    const offsets=Array.from({length:columns*rows+1},word),nulls=bytes(columns*rows),text=bytes(textBytes);assert.equal(at,wire.length);
    return {rows,columns,metadata,values:Array.from(nulls,(n,i)=>n?null:decoder.decode(text.subarray(offsets[i],offsets[i+1])))};
}
function runtime(options={}){
    const memory=new WebAssembly.Memory({initial:1,maximum:512}),live=new Map(),events=[],chunks=[],done=[],held=[];let next=8,allocations=0,peak=0;
    const target={live,events,chunks,completions:done,notifications:held,expectedCookie:7,expectedTicket:1,accepting:true,stale:0,growths:0,
        heap:()=>new Uint8Array(memory.buffer),
        allocate(size){allocations++;events.push(['allocate',size]);if(options.failAfter!==undefined&&allocations>options.failAfter)return 0;
            const pointer=next;next+=size+8;const old=memory.buffer;if(options.grow!==false||next>old.byteLength){memory.grow(Math.max(1,Math.ceil((next-old.byteLength)/65536)));target.growths++;assert.equal(old.byteLength,0,'actual Wasm Memory growth detached the old heap');}
            live.set(pointer,size);peak=Math.max(peak,[...live.values()].reduce((a,b)=>a+b,0));return pointer;},
        release(pointer){assert.ok(live.has(pointer),'release only an owned live C allocation');target.heap().fill(0xdd,pointer,pointer+live.get(pointer));live.delete(pointer);events.push(['release',pointer]);if(options.releaseFailure&&!options.releasedFailure){options.releasedFailure=true;throw new Error('release fixture fault');}},
        string(pointer){if(!pointer)return null;const heap=target.heap(),end=heap.indexOf(0,pointer);assert.ok(end>=pointer);return decoder.decode(heap.subarray(pointer,end));},
        chunk(cookie,ticket,sequence,first,end,wire,size,total,pending){
            // Simulated C export ownership check precedes ANY borrowed reads.
            if(!target.accepting||cookie!==target.expectedCookie||ticket!==target.expectedTicket){target.stale++;events.push(['stale',sequence]);return 0;}
            assert.equal(live.size,3);const record={cookie,ticket,sequence,first:target.string(first),end,total:target.string(total),pending,wire:target.heap().slice(wire,wire+size)};
            record.page=inspect(record.wire);chunks.push(record);events.push(['chunk',sequence]);options.chunk?.(record);
            if(Object.hasOwn(options,'throwChunk'))throw options.throwChunk;
            return typeof options.ack==='function'?options.ack(record):Object.hasOwn(options,'ack')?options.ack:1;},
        held(cookie,ticket,sequence){assert.equal(live.size,0,'holding happens before transport malloc');held.push({cookie,ticket,sequence});events.push(['held',sequence]);options.held?.({cookie,ticket,sequence});},
        done(cookie,ticket,status,rows,count,error){
            // Status+IDs are inspected before optional pointer borrows, including
            // the static2/NULL fallback. This fixture is not an actual C ABI test.
            assert.equal(cookie,target.expectedCookie);assert.equal(ticket,target.expectedTicket);assert.ok([0,1,2].includes(status));
            const record={cookie,ticket,status,rows:rows?target.string(rows):null,count,error:error?target.string(error):null};done.push(record);events.push(['done',status]);options.done?.(record);
        },get allocations(){return allocations;},get peak(){return peak;}};
    return target;
}
async function harness(source,options={}){
    const db=options.database??{async registerFileBuffer(){},async dropFile(){}};
    const connection={sends:[],cancelCalls:0,closed:0,async send(sql,stream){this.sends.push(sql);assert.equal(stream,true);if(options.send)return options.send();return source;},async cancelSent(){this.cancelCalls++;await options.cancel?.();return true;},async close(){this.closed++;}};
    const adapter=new DuckDBWorkerAdapter({database:db,connection}),token=await adapter.registerFile('fixture.dxls',new Uint8Array([1,2,3]));
    const target=runtime(options.runtime),transport=createCodecStreamTransport(adapter,target);return {db,connection,adapter,token,target,transport};
}
async function finish(h){await h.transport.close();assert.equal(h.target.live.size,0);await h.adapter.close();}

test('factory and malformed cookie/request/options fail before adapter ticket admission',async()=>{
    assert.throws(()=>createCodecStreamTransport({},runtime()),TypeError);const h=await harness(reader(table(1)));
    for(const name of ['heap','allocate','release','chunk','done','held'])assert.throws(()=>createCodecStreamTransport(h.adapter,{...h.target,[name]:null}),TypeError);
    const holes=descriptors();delete holes[1];const inherited=descriptors();delete inherited[1];Object.setPrototypeOf(inherited,Object.assign(Object.create(Array.prototype),{1:descriptors()[1]}));
    for(const [cookie,ticket,request,hold] of [[0,1,input(h.token),0],[7,0,input(h.token),0],[4294967296,1,input(h.token),0],[7,1.5,input(h.token),0],[8,1,input(h.token),0],[7,2,input(h.token),0],
        [7,1,input(h.token,'cells',1,{columns:holes}),0],[7,1,input(h.token,'cells',1,{columns:inherited}),0],[7,1,input(h.token,'cells',1,{sourceBindings:Array(1)}),0],
        [7,1,input(h.token,'cells',1,{family:'constructor'}),0],[7,1,input(h.token,'cells',1,{purpose:'copy'}),0],[7,1,input(h.token,'cells',1,{requestId:'01'}),0],
        ...[-1,1.5,NaN,'1',1280003].map(hold=>[7,1,input(h.token),hold])])assert.throws(()=>h.transport.submit(cookie,ticket,request,{holdSequence:hold}),error=>error instanceof TypeError||error?.code==='INVALID_REQUEST');
    assert.equal(h.adapter.latestId,0n);assert.equal(h.connection.sends.length,0);assert.equal(h.target.allocations,0);await finish(h);
});
for(const family of ['columns','cells'])test(`real adapter ${family} chunks copy exact data with growth, cleanup before ACK and post-drain done`,async()=>{
    let h;const source=reader(table(513,family),[0,1,255,0,256,1],{read(){if(h)assert.equal(h.target.live.size,0);}});
    h=await harness(source,{runtime:{done(record){assert.equal(record.status,0);assert.equal(source.cancelled,1);assert.equal(h.adapter.active,null);assert.equal(h.adapter.draining,null);assert.equal(h.transport.active,true);}}});
    const original=h.adapter.submitWorkbookMetadataStream.bind(h.adapter),wires=[];h.adapter.submitWorkbookMetadataStream=(request,callback)=>{
        assert.ok(Object.isFrozen(request)&&Object.isFrozen(request.columns)&&Object.isFrozen(request.columns[0])&&Object.isFrozen(request.sourceBindings[0]));
        return original(request,chunk=>{wires.push(chunk.wire);return callback(chunk);});};
    const request=input(h.token,family);const pending=h.transport.submit(7,1,request);request.columns[0].name='poisoned';request.sourceBindings[0].registrationId='999';request.sql='poisoned SQL';const reply=await pending;
    assert.equal(reply.status,'ok');assert.equal(reply.rowCount,'513');assert.equal(h.connection.sends.length,1);assert.equal(h.connection.sends[0],'C OWNED ORDERED METADATA, NOT REWRITTEN');assert.equal(source.cancelled,1);
    assert.deepEqual(h.target.chunks.map(c=>[c.sequence,c.first,c.end,c.total,c.pending]),[[1,'0',0,'256',1],[2,'256',0,'512',1],[3,'512',0,'513',1],[4,'513',1,'513',0]]);
    assert.deepEqual(h.target.completions,[{cookie:7,ticket:1,status:0,rows:'513',count:4,error:null}]);assert.equal(h.target.live.size,0);assert.equal(h.transport.active,false);assert.ok(h.target.growths>=13);
    const p=h.target.chunks[0].page;assert.equal(p.metadata[0].name,'sheet_id');assert.equal(p.values[family==='cells'?1:2],family==='cells'?'18446744073709551615':null);assert.equal(p.values[family==='cells'?3:2],null);assert.equal(p.values[4+(family==='cells'?3:2)],'');assert.equal(p.values[8+(family==='cells'?3:2)],'NULL');
    assert.equal(h.target.chunks.at(-1).page.rows,0);assert.equal(wires.length,4);for(const wire of wires){assert.ok(wire.byteLength>0&&wire.buffer!==h.target.heap().buffer);assert.equal(inspect(wire).columns,4);}await finish(h);
});
test('empty stream still requires one EOF ACK and uint32 MAX cookie/ticket stays exact',async()=>{
    const h=await harness(reader(table(0)));h.target.expectedCookie=4294967295;h.target.expectedTicket=4294967295;
    const result=await h.transport.submit(4294967295,4294967295,input(h.token,'cells',4294967295,{documentId:'4294967295'}));
    assert.equal(result.status,'ok');assert.deepEqual(h.target.chunks.map(c=>[c.sequence,c.first,c.end,c.page.rows]),[[1,'0',1,0]]);assert.equal(h.target.completions[0].rows,'0');assert.equal(h.target.completions[0].count,1);await finish(h);
});
test('only synchronous numeric1 ACKs, including the final EOF, can complete successfully',async()=>{
    for(const ack of [true,'1',0,undefined,()=>Promise.resolve(1),()=>Promise.reject(new Error('async ACK rejected'))]){
        const h=await harness(reader(table(1)),{runtime:{ack}});const result=await h.transport.submit(7,1,input(h.token));assert.equal(result.status,'error');assert.equal(h.target.chunks.length,1);assert.equal(h.target.completions[0].status,2);assert.equal(h.target.live.size,0);await finish(h);
    }
    const h=await harness(reader(table(1)),{runtime:{ack:r=>r.end?0:1}});assert.equal((await h.transport.submit(7,1,input(h.token))).status,'error');assert.equal(h.target.chunks.length,2);assert.equal(h.target.completions[0].status,2);assert.equal(h.target.completions[0].count,1);await finish(h);
});
test('chunk exceptions (including falsy throws) and release faults retire every temporary allocation',async()=>{
    for(const options of [{throwChunk:new Error('C callback fault')},{throwChunk:0},{releaseFailure:true}]){
        const h=await harness(reader(table(1)),{runtime:options});assert.equal((await h.transport.submit(7,1,input(h.token))).status,'error');assert.equal(h.target.completions[0].status,2);assert.equal(h.target.live.size,0);assert.equal(h.target.events.filter(e=>e[0]==='release').length,h.target.allocations);await finish(h);
    }
});
test('malloc failures clean partial chunk/done buffers and use only a static error completion fallback',async()=>{
    for(const failAfter of [0,1,2,3,4,5,6]){
        const h=await harness(reader(table(1)),{runtime:{failAfter}});const result=await h.transport.submit(7,1,input(h.token));assert.equal(result.status,'error');assert.equal(h.target.completions.length,1);assert.equal(h.target.completions[0].status,2);assert.equal(h.target.completions[0].rows,null);assert.equal(h.target.completions[0].error,null);assert.equal(h.target.chunks.length,failAfter<3?0:failAfter<6?1:2);assert.equal(h.target.live.size,0);assert.equal(h.target.events.filter(e=>e[0]==='release').length,failAfter);await finish(h);
    }
    const h=await harness(reader(table(1)),{send:()=>{throw new Error('engine mock failure');},runtime:{failAfter:1}});assert.equal((await h.transport.submit(7,1,input(h.token))).status,'error');assert.equal(h.target.completions[0].rows,null);assert.equal(h.target.live.size,0);await finish(h);
});
test('completion callback exceptions do not cause duplicate completion or leak its buffers',async()=>{
    const h=await harness(reader(table(1)),{runtime:{done(){throw new Error('done callback fixture');}}});assert.equal((await h.transport.submit(7,1,input(h.token))).status,'error');assert.equal(h.target.completions.length,1);assert.equal(h.target.live.size,0);await finish(h);
});
test('owned error C string is UTF8-complete, NUL-free and at most2048 bytes',async()=>{
    const h=await harness(reader(table(1)),{send:()=>{throw new Error(('界🦆\0').repeat(1000));}});const result=await h.transport.submit(7,1,input(h.token));assert.equal(result.status,'error');const record=h.target.completions[0];assert.equal(record.status,2);assert.equal(record.rows,'0');assert.ok(encoder.encode(record.error).length<=2048);assert.ok(!record.error.includes('\0'));assert.doesNotThrow(()=>decoder.decode(encoder.encode(record.error)));assert.equal(h.target.live.size,0);await finish(h);
});
test('held chunk blocks before any C malloc, then replays once and resumes only after cleanup',async()=>{
    const entered=deferred(),h=await harness(reader(table(513)),{runtime:{held:entered.resolve}});const pending=h.transport.submit(7,1,input(h.token),{holdSequence:2});await entered.promise;
    assert.equal(h.target.chunks.length,1);assert.equal(h.target.allocations,3);assert.equal(h.target.live.size,0);assert.equal(h.transport.hasHeld,true);assert.equal(h.transport.active,true);const last=h.adapter.latestId;
    assert.throws(()=>h.transport.submit(7,2,input(h.token,'cells',2)),/quiescent/);assert.equal(h.adapter.latestId,last);await assert.rejects(h.adapter.registerFile('blocked.dxls',new Uint8Array([3])),{code:'NOT_AVAILABLE'});
    assert.equal(h.transport.releaseHeld(),true);assert.equal(h.target.live.size,0);assert.equal(h.transport.releaseHeld(),false);assert.equal((await pending).status,'ok');assert.equal(h.target.chunks.length,4);assert.equal(h.transport.hasHeld,false);await finish(h);
});
test('cancel drains strict reader cleanup and cancelSent while retaining one late stale replay',async()=>{
    const entered=deferred(),cleanup=deferred(),cancellation=deferred(),source=reader(table(600),[600],{cleanup:()=>cleanup.promise});
    const h=await harness(source,{cancel:()=>cancellation.promise,runtime:{held:entered.resolve}});const pending=h.transport.submit(7,1,input(h.token),{holdSequence:1});await entered.promise;assert.equal(h.target.allocations,0);
    h.target.accepting=false;const cancelling=h.transport.cancel();let finished=false;cancelling.then(()=>{finished=true;});await tick();assert.equal(finished,false);assert.equal(h.target.completions.length,0);assert.equal(h.transport.hasHeld,true);assert.throws(()=>h.transport.submit(7,2,input(h.token,'cells',2)),/quiescent/);
    cleanup.resolve();cancellation.resolve();await cancelling;assert.equal((await pending).status,'cancelled');assert.equal(source.cancelled,1);assert.equal(h.target.completions[0].status,1);assert.equal(h.transport.active,false);assert.equal(h.transport.hasHeld,true);assert.equal(h.target.chunks.length,0);assert.equal(h.adapter.draining,null);
    h.target.expectedTicket=2; // A new C owner exists; stale export checks first.
    // done for ticket1 has already completed. Late replay makes only chunk, no done.
    assert.equal(h.transport.releaseHeld(),false);assert.equal(h.target.stale,1);assert.equal(h.target.live.size,0);assert.equal(h.transport.releaseHeld(),false);
    h.target.accepting=true;h.connection.send=async()=>reader(table(1));assert.equal((await h.transport.submit(7,2,input(h.token,'cells',2))).status,'ok');await finish(h);
});
test('external adapter supersession drains before completion and leaves held reply for explicit stale C replay',async()=>{
    const entered=deferred(),h=await harness(reader(table(600)),{runtime:{held:entered.resolve}});const pending=h.transport.submit(7,1,input(h.token),{holdSequence:1});await entered.promise;
    h.connection.send=async()=>reader(table(1));const ordinary=h.adapter.submit({requestId:'2',sql:'ordinary recovery',rowLimit:256});assert.equal((await ordinary).status,'ok');assert.equal((await pending).status,'superseded');assert.equal(h.target.completions[0].status,1);assert.equal(h.adapter.draining,null);
    h.target.expectedTicket=3;h.target.accepting=false;assert.equal(h.transport.releaseHeld(),false);assert.equal(h.target.stale,1);h.target.accepting=true;assert.equal((await h.transport.submit(7,3,input(h.token,'cells',3))).status,'ok');await finish(h);
});
test('close discards held JS ownership without C replay and never closes the caller adapter',async()=>{
    const entered=deferred(),source=reader(table(600)),h=await harness(source,{runtime:{held:entered.resolve}});const pending=h.transport.submit(7,1,input(h.token),{holdSequence:1});await entered.promise;
    await h.transport.close();assert.equal((await pending).status,'cancelled');assert.equal(h.target.completions[0].status,1);assert.equal(h.target.chunks.length,0);assert.equal(h.transport.hasHeld,false);assert.equal(h.transport.closed,true);assert.equal(h.transport.active,false);assert.equal(h.target.live.size,0);assert.equal(source.cancelled,1);assert.equal(h.connection.closed,0);assert.equal(h.adapter.closed,false);assert.equal(h.transport.releaseHeld(),false);assert.throws(()=>h.transport.submit(7,2,input(h.token,'cells',2)),/quiescent/);await h.transport.close();assert.equal(h.target.completions.length,1);await h.adapter.close();
});
test('throwing held notification resolves false and discards the slot before recovery',async()=>{
    const options={held(){throw new Error('held notification fixture');}},h=await harness(reader(table(1)),{runtime:options});const result=await h.transport.submit(7,1,input(h.token),{holdSequence:1});assert.equal(result.status,'error');assert.match(h.target.completions[0].error,/held notification fixture/);assert.equal(h.transport.hasHeld,false);assert.equal(h.target.chunks.length,0);assert.equal(h.target.allocations,2);options.held=()=>{};h.target.expectedTicket=2;h.connection.send=async()=>reader(table(1));assert.equal((await h.transport.submit(7,2,input(h.token,'cells',2))).status,'ok');await finish(h);
});
test('late revoked archive capture, including failed drop, never receives success and can recover',async()=>{
    for(const failed of [false,true]){
        const entered=deferred(),db={async registerFileBuffer(){},async dropFile(){if(failed)throw new Error('drop fixture');}},h=await harness(reader(table(600)),{database:db,runtime:{held:entered.resolve}});
        const sibling=new DuckDBWorkerAdapter({database:db,connection:{async send(){return reader(table(1));},async cancelSent(){return true;},async close(){}}});
        const pending=h.transport.submit(7,1,input(h.token),{holdSequence:1});await entered.promise;if(failed)await assert.rejects(sibling.dropFile(h.token.name),/drop fixture/);else await sibling.dropFile(h.token.name);assert.equal(h.transport.releaseHeld(),true);assert.equal((await pending).status,'error');assert.equal(h.target.completions[0].status,2);
        const previous=h.adapter.latestId,sends=h.connection.sends.length;h.target.expectedTicket=2;const denied=await h.transport.submit(7,2,input(h.token,'cells',2));assert.equal(denied.status,'error');assert.match(h.target.completions.at(-1).error,/captured browser source registration/);assert.equal(h.adapter.latestId,previous);assert.equal(h.connection.sends.length,sends);
        const next=await h.adapter.registerFile('recovered.dxls',new Uint8Array([2]));h.connection.send=async()=>reader(table(1));assert.equal((await h.transport.submit(7,2,input(next,'cells',2))).status,'ok');await finish(h);await sibling.close();
    }
});
test('invalid chunk sequence/u64/geometry/identity and forged success are bounded transport errors',async()=>{
    const faults=[c=>({...c,sequence:0}),c=>({...c,sequence:1.5}),c=>({...c,sequence:4294967296}),c=>({...c,firstRow:'01'}),c=>({...c,firstRow:'18446744073709551616'}),
        c=>({...c,documentId:'8'}),c=>({...c,page:{...c.page,total:'18446744073709551616'}}),c=>({...c,page:{...c.page,rowCount:257}}),c=>({...c,page:{...c.page,countPending:false}}),c=>({...c,wire:new Uint8Array(6*1024*1024+1)}),(c,h)=>({...c,wire:h.target.heap().subarray(0,24)})];
    for(const fault of faults){const h=await harness(reader(table(1))),original=h.adapter.submitWorkbookMetadataStream.bind(h.adapter);h.adapter.submitWorkbookMetadataStream=(r,cb)=>original(r,c=>cb(fault(c,h)));assert.equal((await h.transport.submit(7,1,input(h.token))).status,'error');assert.equal(h.target.chunks.length,0);assert.equal(h.target.completions[0].status,2);assert.equal(h.target.live.size,0);await finish(h);}
    for(const change of [{rowCount:'01'},{rowCount:'18446744073709551616'},{chunks:99},{dataChunks:99},{requestId:'2'}]){const h=await harness(reader(table(1))),original=h.adapter.submitWorkbookMetadataStream.bind(h.adapter);h.adapter.submitWorkbookMetadataStream=(r,cb)=>original(r,cb).then(reply=>({...reply,...change}));assert.equal((await h.transport.submit(7,1,input(h.token))).status,'error');assert.equal(h.target.completions[0].status,2);assert.equal(h.target.live.size,0);await finish(h);}
    const h=await harness(reader(table(1))),original=h.adapter.submitWorkbookMetadataStream.bind(h.adapter);h.adapter.submitWorkbookMetadataStream=(r,cb)=>original(r,c=>c.end?true:cb(c));assert.equal((await h.transport.submit(7,1,input(h.token))).status,'error');assert.equal(h.target.completions[0].status,2);await finish(h);
});

test('cancel in the completed-adapter/pre-C-done gap cannot publish success',async()=>{
    const h=await harness(reader(table(1))),original=h.adapter.submitWorkbookMetadataStream.bind(h.adapter);let cancellation;
    h.adapter.submitWorkbookMetadataStream=(r,cb)=>original(r,cb).then(reply=>{cancellation=h.transport.cancel();return reply;});
    const result=await h.transport.submit(7,1,input(h.token));await cancellation;assert.equal(result.status,'cancelled');assert.equal(h.target.completions[0].status,1);assert.equal(h.target.live.size,0);await finish(h);
});
