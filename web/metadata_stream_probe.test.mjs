// Memory-only probe contracts. These use the real adapter with owned Arrow
// fixtures; no official Worker, SQL engine, archive decoder or UI runs here.
import test from 'node:test';
import assert from 'node:assert/strict';
import {Table,Int32,Uint64,Utf8,Bool,Float64,vectorFromArray} from 'apache-arrow';
import {DuckDBWorkerAdapter} from './official_worker.mjs';
import {runMetadataStreamProbe} from './metadata_stream_probe.mjs';

const values=row=>row%4===0?null:row%4===1?'':row%4===2?'NULL':'İstanbul 🦆';
function table(family,rows,change={}) {
    const series=make=>Array.from({length:rows},(_,i)=>make(i));
    const common={sheet_id:vectorFromArray(series(i=>i%64),new Int32()),
        [family==='columns'?'column_id':'row_id']:vectorFromArray(series(i=>i%2?9007199254740993n:18446744073709551615n),new Uint64())};
    return new Table({...common,...(family==='columns'?{
        name:vectorFromArray(series(i=>i===0?'N'.repeat(change.shortName?65536:65537):values(i)),new Utf8()),
        width:vectorFromArray(series(i=>i%5===0?null:i%17+.25),new Float64())}:{
        enabled:vectorFromArray(series(i=>i%3===2?null:i%3===0),new Bool()),
        value:vectorFromArray(series(values),new Utf8())})});
}
class Reader {
    #source;#first=0;#index=0;#lengths;#change;
    constructor(source,change={}){this.#source=source;this.#change=change;this.#lengths=change.lengths??[source.numRows];this.calls={open:0,next:0,cancel:0,returned:0};}
    get schema(){return this.#source.schema;}
    async open(){this.calls.open++;return this;}
    [Symbol.asyncIterator](){return new Iterator(this);}
    async read(){
        this.calls.next++;if(this.#index===this.#lengths.length)return {done:true};const count=this.#lengths[this.#index++],first=this.#first;this.#first+=count;
        return {done:false,value:count?this.#source.slice(first,first+count).batches[0]:{numCols:this.#source.numCols,numRows:0}};
    }
    async cancel(){this.calls.cancel++;if(this.#change.cleanupError)throw new Error('actual cleanup did not fulfill');}
    async returned(){this.calls.returned++;return {done:true};}
}
class Iterator {#reader;constructor(reader){this.#reader=reader;}next(){return this.#reader.read();}return(){return this.#reader.returned();}}
async function harness(change={}) {
    const raw=[],sql=[],db={registrations:0,drops:0,async registerFileBuffer(){this.registrations++;},async dropFile(){this.drops++;}};
    const connection={closed:0,cancels:0,async cancelSent(){this.cancels++;return true;},async close(){this.closed++;},async send(query,stream){
        sql.push(query);assert.equal(stream,true);
        const recovery=query.match(/SELECT (43|44)::INTEGER AS recovered$/);
        if(recovery){const reader=new Reader(new Table({recovered:vectorFromArray([Number(recovery[1])],new Int32())}));raw.push(reader);return reader;}
        const match=query.match(/metadata-stream-probe:(columns|cells|cancel|false-ack)/),rows=Number(query.match(/range\((\d+)\)/)?.[1]);assert.ok(match&&rows);
        const family=match[1]==='columns'?'columns':'cells',fixture=table(family,rows,change),lengths=rows===513?(family==='columns'?[1,0,255,257]:[17,256,240]):[rows];
        const reader=new Reader(fixture,{lengths,...change});if(change.missingCancel)reader.cancel=undefined;raw.push(reader);return reader;
    }};
    const adapter=new DuckDBWorkerAdapter({database:db,connection,engineVersion:'memory-only-mock'});
    const binding=await adapter.registerFile('private-probe.dxls',new Uint8Array([1,2,3]));return {adapter,binding,connection,db,raw,sql};
}

test('probe import is inert and rejects invalid bounds/token/idle ownership before send or reservation',async()=>{
    assert.equal(typeof runMetadataStreamProbe,'function');const h=await harness(),send=h.connection.send,id=h.adapter.latestId;
    for(const options of [{rows:256},{rows:9487},{rows:20000},{rows:NaN},{timeoutMs:999},{timeoutMs:120001}])
        await assert.rejects(runMetadataStreamProbe(h.adapter,h.binding,options),TypeError);
    await assert.rejects(runMetadataStreamProbe(h.adapter,{...h.binding,registrationId:'0'}),{code:'INVALID_REQUEST'});
    h.adapter.admin=true;await assert.rejects(runMetadataStreamProbe(h.adapter,h.binding),TypeError);h.adapter.admin=false;
    assert.equal(h.connection.send,send);assert.equal(h.adapter.latestId,id);assert.equal(h.sql.length,0);assert.equal(h.db.registrations,1);await h.adapter.close();
});
test('real adapter/probe preserves receiver identity, irregular Arrow observations and exact six-query workload',async()=>{
    const h=await harness(),send=h.connection.send,descriptor=Object.getOwnPropertyDescriptor(h.connection,'send');
    const report=await runMetadataStreamProbe(h.adapter,h.binding);assert.equal(report.ok,true,JSON.stringify(report.error));assert.equal(report.sqlExecutions,6);assert.equal(report.plannedSqlRows,2054);assert.equal(report.cases.length,6);assert.equal(report.postedTurns,6);
    assert.equal(h.connection.send,send);assert.deepEqual(Object.getOwnPropertyDescriptor(h.connection,'send'),descriptor);assert.equal(h.db.registrations,1);assert.equal(h.db.drops,0);assert.equal(h.connection.closed,0);
    assert.deepEqual(report.readers[0].batchRows,[1,0,255,257]);assert.deepEqual(report.readers[1].batchRows,[17,256,240]);
    for(const item of report.cases.slice(0,2)){assert.equal(item.rows,513);assert.equal(item.eofAfterCleanup,true);assert.equal(item.firstAckPostedTurn,true);assert.equal(item.finalAckPostedTurn,true);assert.deepEqual(item.shapes.map(c=>[c.firstRow,c.rows,c.end]),[['0',256,false],['256',256,false],['512',1,false],['513',0,true]]);}
    for(const reader of report.readers){assert.equal(reader.open,1);assert.equal(reader.cancelRequested,1);assert.equal(reader.cancelFulfilled,1);assert.equal(reader.maxInflight,1);}
    assert.deepEqual(h.raw.map(reader=>reader.calls.returned),[0,0,0,1,0,1]);
    assert.equal(report.cases[2].streamStatus,'cancelled');assert.equal(report.cases[2].ends,0);assert.equal(report.cases[3].value,43);assert.equal(report.cases[4].errorCode,'STREAM_CONSUMER');assert.equal(report.cases[5].value,44);
    assert.deepEqual(report.proofs,{exactTypedFamilies:2,strictEofCleanups:2,dataAckPostedTurns:2,finalAckPostedTurns:2,cancelledWithoutEof:true,falseAckWithoutEof:true,ordinaryAfterCancel:43,ordinaryAfterFalseAck:44});
    assert.ok(h.sql.every(query=>!query.includes('read_csv')&&!query.includes('OFFSET')));assert.ok(report.checks>4000);assert.ok(Object.isFrozen(report));await h.adapter.close();
});
test('maximum total row admission is20,000 and reporting remains bounded without storing row text',async()=>{
    const h=await harness(),report=await runMetadataStreamProbe(h.adapter,h.binding,{rows:9486});assert.equal(report.ok,true,JSON.stringify(report.error));assert.equal(report.plannedSqlRows,20000);assert.equal(report.sqlExecutions,6);
    assert.ok(JSON.stringify(report).length<30000);assert.equal(report.rowsPerSuccessfulFamily,9486);assert.equal(report.cases[0].shapes.at(-2).rows,14);assert.equal(report.cases[1].shapes.at(-2).rows,14);await h.adapter.close();
});
test('a prototype-owned send method is restored without leaving an own shadow property',async()=>{
    const h=await harness(),send=h.connection.send,prototype={send};delete h.connection.send;Object.setPrototypeOf(h.connection,prototype);
    const report=await runMetadataStreamProbe(h.adapter,h.binding);assert.equal(report.ok,true,JSON.stringify(report.error));assert.equal(Object.hasOwn(h.connection,'send'),false);assert.equal(Object.getPrototypeOf(h.connection),prototype);assert.equal(h.connection.send,send);await h.adapter.close();
});
test('first exact long-name mismatch stops atomically, drains, restores observer and admits recovery',async()=>{
    const h=await harness({shortName:true}),send=h.connection.send,report=await runMetadataStreamProbe(h.adapter,h.binding);
    assert.equal(report.ok,false);assert.match(report.error.message,/Exact long name/);assert.equal(report.sqlExecutions,1);assert.equal(report.cases.length,1);assert.equal(report.cleanup.cancelDrained,true);assert.equal(h.connection.send,send);
    assert.equal(h.raw[0].calls.cancel,1);assert.equal(h.db.drops,0);assert.equal(h.connection.closed,0);
    const reply=await h.adapter.submit({requestId:String(h.adapter.latestId+1n),sql:'SELECT 43::INTEGER AS recovered'});assert.equal(reply.status,'ok');await h.adapter.close();
});
test('missing or rejected actual reader.cancel remains a failed probe and poisons later admission',async()=>{
    for(const change of [{missingCancel:true},{cleanupError:true}]){
        const h=await harness(change),send=h.connection.send,report=await runMetadataStreamProbe(h.adapter,h.binding);assert.equal(report.ok,false);assert.match(report.error.message,/STREAM_CLEANUP/);assert.equal(report.sqlExecutions,1);assert.equal(report.cases.length,1);assert.equal(h.connection.send,send);assert.equal(report.readers[0].cancelFulfilled,0);
        assert.throws(()=>h.adapter.submit({requestId:String(h.adapter.latestId+1n),sql:'SELECT 43::INTEGER AS recovered'}),{code:'STREAM_CLEANUP'});assert.equal(h.sql.length,1);await h.adapter.close();
    }
});
