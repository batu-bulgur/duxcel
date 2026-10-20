// Memory-only probe contracts. These use the real adapter with owned Arrow
// fixtures; no official Worker, SQL engine, archive decoder or UI runs here.
import test from 'node:test';
import assert from 'node:assert/strict';
import {Table,Int32,Uint64,Utf8,Float64,vectorFromArray} from 'apache-arrow';
import {DuckDBWorkerAdapter} from '../official_worker.mjs';
import {runMetadataStreamProbe} from './support/metadata_stream_probe.mjs';

const values=row=>row%4===0?null:row%4===1?'':row%4===2?'NULL':'İstanbul 🦆';
function table(family,rows,change={}) {
    const series=make=>Array.from({length:rows},(_,i)=>make(i));
    const common={sheet_id:vectorFromArray(series(i=>i%64),new Int32()),
        [family==='columns'?'column_id':'row_id']:vectorFromArray(series(i=>i%2?9007199254740993n:18446744073709551615n),new Uint64())};
    return new Table({...common,...(family==='columns'?{
        name:vectorFromArray(series(i=>i===0?'N'.repeat(change.shortName?65536:65537):values(i)),new Utf8()),
        width:vectorFromArray(series(i=>i%5===0?null:i%17+.25),new Float64())}:{
        enabled:vectorFromArray(series(i=>i%3===2?null:i%3===0?'true':'false'),new Utf8()),
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
        const batch=count?this.#source.slice(first,first+count).batches[0]:{numCols:this.#source.numCols,numRows:0};
        return {done:false,value:this.#change.badColumns?{...batch,numCols:2,numRows:batch.numRows}:batch};
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
        if(recovery){const reader=new Reader(new Table({recovered:vectorFromArray([Number(recovery[1])],new Int32())}),{badColumns:change.badRecovery});raw.push(reader);return reader;}
        const match=query.match(/metadata-stream-probe:(columns|cells|cancel|false-ack)/),rows=Number(query.match(/range\((\d+)\)/)?.[1]);assert.ok(match&&rows);
        const family=match[1]==='columns'?'columns':'cells',fixture=table(family,rows,change),lengths=rows===513?(family==='columns'?[1,0,255,257]:[17,256,240]):[rows];
        if(family==='cells')assert.match(query,/CAST\(CASE WHEN i%3=2 THEN NULL::BOOLEAN ELSE i%3=0 END AS VARCHAR\) AS enabled/);
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
    const raw=report.readers[1].rawSchema;assert.equal(raw.columnCount,4);assert.equal(raw.fieldsTruncated,false);assert.deepEqual(raw.fields.map(field=>[field.name,field.type]),[['sheet_id',String(new Int32())],['row_id',String(new Uint64())],['enabled',String(new Utf8())],['value',String(new Utf8())]]);
    assert.ok(report.readers.every(reader=>reader.schemaObservationError===null&&reader.rawSchema.fields.every(field=>!field.nameTruncated&&!field.typeTruncated)));
    for(const item of report.cases.slice(0,2)){assert.equal(item.rows,513);assert.equal(item.eofAfterCleanup,true);assert.equal(item.firstAckPostedTurn,true);assert.equal(item.finalAckPostedTurn,true);assert.deepEqual(item.shapes.map(c=>[c.firstRow,c.rows,c.end]),[['0',256,false],['256',256,false],['512',1,false],['513',0,true]]);}
    for(const reader of report.readers){assert.equal(reader.open,1);assert.equal(reader.cancelRequested,1);assert.equal(reader.cancelFulfilled,1);assert.equal(reader.maxInflight,1);}
    assert.deepEqual(h.raw.map(reader=>reader.calls.returned),[0,0,0,0,0,0]);
    assert.equal(report.cases[2].streamStatus,'cancelled');assert.equal(report.cases[2].ends,0);assert.equal(report.cases[3].value,43);assert.equal(report.cases[4].errorCode,'STREAM_CONSUMER');assert.equal(report.cases[5].value,44);
    assert.deepEqual(report.proofs,{exactTypedFamilies:2,strictEofCleanups:2,dataAckPostedTurns:2,finalAckPostedTurns:2,cancelledWithoutEof:true,falseAckWithoutEof:true,canonicalBooleanProjection:true,ordinaryAfterCancel:43,ordinaryAfterFalseAck:44});
    assert.ok(report.cases.filter(item=>item.family==='cells').every(item=>item.canonicalBooleanProjection));
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
test('an ordinary for-await validation error forwards iterator.return with its original receiver',async()=>{
    const h=await harness({badRecovery:true}),send=h.connection.send,report=await runMetadataStreamProbe(h.adapter,h.binding);
    assert.equal(report.ok,false);assert.equal(report.sqlExecutions,4);assert.equal(report.cases.at(-1).name,'ordinary-after-cancel');assert.equal(h.raw[3].calls.returned,1);assert.equal(h.raw[3].calls.cancel,1);assert.equal(h.connection.send,send);await h.adapter.close();
});
test('missing or rejected actual reader.cancel remains a failed probe and poisons later admission',async()=>{
    for(const change of [{missingCancel:true},{cleanupError:true}]){
        const h=await harness(change),send=h.connection.send,report=await runMetadataStreamProbe(h.adapter,h.binding);assert.equal(report.ok,false);assert.match(report.error.message,/STREAM_CLEANUP/);assert.equal(report.sqlExecutions,1);assert.equal(report.cases.length,1);assert.equal(h.connection.send,send);assert.equal(report.readers[0].cancelFulfilled,0);
        assert.throws(()=>h.adapter.submit({requestId:String(h.adapter.latestId+1n),sql:'SELECT 43::INTEGER AS recovered'}),{code:'STREAM_CLEANUP'});assert.equal(h.sql.length,1);await h.adapter.close();
    }
});

// Transport-only format rows: these fixtures do not certify FormatPatch rules.
// Keep the original six-query probe and every earlier test definition unchanged.
import {workbookMetadataStreamRequest,pageRequest,pageCellText,LIMITS} from '../official_worker.mjs';
const formatProbeNames=['sheet_id','position','r0','r1','c0','c1','mask','flags','alignment','color','font_size','numeric_format','decimals'];
function formatProbeColumns(){return formatProbeNames.map((name,i)=>({name,typeName:i===2||i===3?'UBIGINT':'INTEGER',numberKind:i===2||i===3?'u64':'i32'}));}
function formatProbeInput(binding,change={}){return {requestId:'1',documentId:'7',sql:'C FORMAT ROW SQL ORDER BY sheet_id,position LIMIT 32769',purpose:'workbook-metadata-stream',family:'format-rows',offset:'0',knownTotal:null,columns:formatProbeColumns(),sourceBindings:[{...binding}],...change};}
function formatProbeValue(row,col){return col===2?(row%2?9007199254740993n:18446744073709551615n):col===3?18446744073709551615n:row*13+col;}
function formatProbeTable(rows,{canonical=false,cell=formatProbeValue}={}){
    return new Table(Object.fromEntries(formatProbeNames.map((name,col)=>[name,vectorFromArray(Array.from({length:rows},(_,r)=>{const value=cell(r,col);return canonical&&value!==null?String(value):value;}),canonical?new Utf8():col===2||col===3?new Uint64():new Int32())])));
}
function formatProbeReader(source,lengths=[source.numRows],options={}){
    let first=0,index=0;const batches=lengths.map(count=>{const batch=count?source.slice(first,first+count).batches[0]:{numRows:0,numCols:13};first+=count;return batch;});assert.equal(first,source.numRows);
    return {schema:source.schema,cancelled:0,reads:0,nextCalls:0,open(){return this;},[Symbol.asyncIterator](){return {next:async()=>{
        this.nextCalls++;if(index===batches.length)return {done:true};const batch=batches[index++];return {done:false,value:{numCols:options.numCols??13,numRows:options.numRows??batch.numRows,getChildAt:c=>({get:r=>{this.reads++;return batch.getChildAt(c).get(r);}})}};
    }};},async cancel(){this.cancelled++;}};
}
async function formatProbeHarness(source){
    const connection={sends:[],async send(sql){this.sends.push(sql);return source;},async cancelSent(){return true;},async close(){}};
    const adapter=new DuckDBWorkerAdapter({database:{async registerFileBuffer(){},async dropFile(){}},connection});
    const binding=await adapter.registerFile('format-fixture.dxls',new Uint8Array([1]));return {adapter,binding,connection};
}
function formatProbeFootprint(page){return new TextEncoder().encode(JSON.stringify(page.columns)).byteLength+page.text.byteLength+page.offsets.byteLength+page.nulls.byteLength;}

test('format-row stream captures its family once and rejects malformed13-column admission before tickets or SQL',async()=>{
    const source=formatProbeReader(formatProbeTable(1)),h=await formatProbeHarness(source),value=formatProbeInput(h.binding);let reads=0;
    Object.defineProperty(value,'family',{enumerable:true,get(){return ++reads===1?'format-rows':'cells';}});
    const frozen=workbookMetadataStreamRequest(value);assert.equal(frozen.family,'format-rows');assert.equal(frozen.rowLimit,32769);assert.equal(frozen.columns.length,13);assert.ok(Object.isFrozen(frozen.columns[12]));
    const holes=formatProbeColumns();delete holes[12];const inherited=formatProbeColumns();delete inherited[12];Object.setPrototypeOf(inherited,Object.assign(Object.create(Array.prototype),{12:formatProbeColumns()[12]}));
    for(const change of [{columns:holes},{columns:inherited},{columns:Array(13)},{columns:formatProbeColumns().slice(1)},{columns:[...formatProbeColumns(),formatProbeColumns()[0]]},{rowLimit:32770},{rowLimit:0},{rowLimit:1.5},{offset:'1'},{knownTotal:'0'},{sourceBindings:[]},{sourceBindings:Array(1)},{purpose:'copy'},{family:'formats'},{family:'constructor'},...['columns','cells','source-versions'].map(family=>({family}))]){
        assert.throws(()=>h.adapter.submitWorkbookMetadataStream(formatProbeInput(h.binding,change),()=>true),{code:'INVALID_REQUEST'});assert.equal(h.adapter.latestId,0n);assert.equal(h.connection.sends.length,0);
    }
    assert.throws(()=>pageRequest(formatProbeInput(h.binding)),{code:'INVALID_REQUEST'});assert.throws(()=>h.adapter.submit(formatProbeInput(h.binding)),{code:'INVALID_REQUEST'});await h.adapter.close();
});
test('format-row7/285 typed original sidecars retain exactU64 and independent256-row chunking/EOF',async()=>{
    for(const rows of [7,285]){
        const source=formatProbeReader(formatProbeTable(rows),rows===7?[0,1,6]:[17,0,240,28]),h=await formatProbeHarness(source),chunks=[];
        const request=formatProbeInput(h.binding),pending=h.adapter.submitWorkbookMetadataStream(request,chunk=>{
            assert.equal(chunk.page.columnCount,13);assert.ok(formatProbeFootprint(chunk.page)<=LIMITS.bytes);assert.ok(chunk.wire.byteLength<=6*1024*1024);
            assert.deepEqual(chunk.page.columns.map(c=>[c.typeName,c.numberKind]),formatProbeColumns().map(c=>[c.typeName,c.numberKind]));
            if(chunk.end)assert.equal(source.cancelled,1);else{const row=Number(chunk.firstRow);assert.equal(pageCellText(chunk.page,0,2),String(formatProbeValue(row,2)));assert.equal(pageCellText(chunk.page,chunk.page.rowCount-1,3),'18446744073709551615');}
            chunks.push(chunk);return true;
        });request.columns[2].typeName='DOUBLE';request.sourceBindings[0].registrationId='999';request.sql='must not replace captured SQL';
        assert.equal((await pending).status,'ok');assert.deepEqual(chunks.map(c=>[c.firstRow,c.page.rowCount,c.end]),rows===7?[['0',7,false],['7',0,true]]:[['0',256,false],['256',29,false],['285',0,true]]);
        assert.equal(source.reads,rows*13);assert.equal(h.connection.sends.length,1);assert.equal(h.connection.sends[0],'C FORMAT ROW SQL ORDER BY sheet_id,position LIMIT 32769');assert.equal(source.cancelled,1);await h.adapter.close();
    }
});
test('format-row canonical text preserves numeric sidecars and malformed NULL/empty/literal atoms for C validation',async()=>{
    const source=formatProbeReader(formatProbeTable(4,{canonical:true,cell:(r,c)=>c===0?[null,'','NULL','0'][r]:formatProbeValue(r,c)})),h=await formatProbeHarness(source),chunks=[];
    assert.equal((await h.adapter.submitWorkbookMetadataStream(formatProbeInput(h.binding),c=>{chunks.push(c);return true;})).status,'ok');
    assert.deepEqual(Array.from({length:4},(_,r)=>pageCellText(chunks[0].page,r,0)),[null,'','NULL','0']);assert.equal(chunks[0].page.columns[0].typeName,'INTEGER');assert.equal(chunks[0].page.columns[0].arrowType,'Utf8');assert.equal(pageCellText(chunks[0].page,1,2),'9007199254740993');assert.equal(pageCellText(chunks[0].page,0,3),'18446744073709551615');await h.adapter.close();
});
test('format-row32769 transport sentinel streams one immutable result without expanded ordinary pages',async()=>{
    const source=formatProbeReader(formatProbeTable(32769)),h=await formatProbeHarness(source);let count=0,chunks=0,ends=0;
    const result=await h.adapter.submitWorkbookMetadataStream(formatProbeInput(h.binding),c=>{
        assert.equal(c.firstRow,String(count));assert.ok(c.page.rowCount<=256);assert.ok(formatProbeFootprint(c.page)<=LIMITS.bytes);assert.ok(c.wire.byteLength<=6*1024*1024);assert.equal(c.page.columnCount,13);
        if(c.end){ends++;assert.equal(source.cancelled,1);}else{assert.equal(pageCellText(c.page,0,3),'18446744073709551615');count+=c.page.rowCount;}chunks++;return true;
    });assert.equal(result.status,'ok');assert.equal(result.rowCount,'32769');assert.equal(count,32769);assert.equal(chunks,130);assert.equal(ends,1);assert.equal(h.connection.sends.length,1);await h.adapter.close();
});
test('format-row result overflow/wrongarity and individual oversized row fail before any chunk',async()=>{
    for(const [source,code] of [[formatProbeReader(formatProbeTable(1),[1],{numRows:32770}),'PAGE_LIMIT'],[formatProbeReader(formatProbeTable(1),[1],{numCols:12}),'SCHEMA_CHANGED'],[formatProbeReader(formatProbeTable(1),[1],{numCols:14}),'SCHEMA_CHANGED'],[formatProbeReader(formatProbeTable(1,{canonical:true,cell:(r,c)=>c===0?'9'.repeat(LIMITS.bytes):formatProbeValue(r,c)})),'PAGE_LIMIT']]){
        const h=await formatProbeHarness(source);let calls=0;const result=await h.adapter.submitWorkbookMetadataStream(formatProbeInput(h.binding),()=>{calls++;return true;});assert.equal(result.status,'error');assert.equal(result.error.code,code);assert.equal(calls,0);assert.equal(source.cancelled,1);await h.adapter.close();
    }
});
test('format-row paused ACK prevents advancement and cancellation retires reader before ordinary recovery',async()=>{
    const source=formatProbeReader(formatProbeTable(600)),h=await formatProbeHarness(source);let enter,ack;const entered=new Promise(r=>{enter=r;}),held=new Promise(r=>{ack=r;});
    const pending=h.adapter.submitWorkbookMetadataStream(formatProbeInput(h.binding),()=>{enter();return held;});await entered;const reads=source.reads,next=source.nextCalls;
    await new Promise(r=>setImmediate(r));assert.equal(source.reads,256*13);assert.equal(source.reads,reads);assert.equal(source.nextCalls,next);
    await h.adapter.cancel();assert.equal((await pending).status,'cancelled');assert.equal(source.cancelled,1);ack(true);h.connection.send=async()=>new Reader(new Table({recovered:vectorFromArray([43],new Int32())}));
    assert.equal((await h.adapter.submit({requestId:'2',sql:'ordinary recovery'})).status,'ok');await h.adapter.close();
});
