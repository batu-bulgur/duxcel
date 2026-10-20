/** Opt-in headless transport probe; imported only by the integration owner.
 * The caller supplies one exclusively idle adapter and privately registered
 * immutable archive token, and owns close/drop and the outer hard deadline.
 * SQL is synthetic SELECT/range data, not workbook decoding or native parity.
 */
import {LIMITS,pageCellText,workbookMetadataStreamRequest} from '../../official_worker.mjs';

const MAX_U64=18446744073709551615n,STOP_ROWS=513,MAX_TOTAL_ROWS=20000;
const identity=Object.freeze({generation:'9007199254740993',documentId:'9007199254740995',
    sourceId:'0',objectId:'0',revision:'18446744073709551615',offset:'0'});
const idFields=['requestId','generation','documentId','sourceId','objectId','revision','offset'];
const now=()=>globalThis.performance?.now()??Date.now();
const pending=()=>{let resolve,reject;const promise=new Promise((yes,no)=>{resolve=yes;reject=no;});return {promise,resolve,reject};};
const errorRecord=error=>({code:String(error?.code??error?.name??'ERROR'),message:String(error?.message??error).slice(0,2048)});
const columns=family=>family==='columns'?
    [{name:'sheet_id',typeName:'INTEGER',numberKind:'i32'},{name:'column_id',typeName:'UBIGINT',numberKind:'u64'},
        {name:'name',typeName:'VARCHAR',numberKind:'none'},{name:'width',typeName:'DOUBLE',numberKind:'float'}]:
    [{name:'sheet_id',typeName:'INTEGER',numberKind:'i32'},{name:'row_id',typeName:'UBIGINT',numberKind:'u64'},
        {name:'enabled',typeName:'BOOLEAN',numberKind:'none'},{name:'value',typeName:'VARCHAR',numberKind:'none'}];
// Follow the C metadata projection boundary: SQL renders BOOLEAN as exact
// canonical text, while the captured original sidecar still says BOOLEAN.
const sourceSql=(family,rows,tag)=>`/* metadata-stream-probe:${tag} */ SELECT
    (i%64)::INTEGER AS sheet_id,
    CASE WHEN i%2=0 THEN '18446744073709551615'::UBIGINT ELSE '9007199254740993'::UBIGINT END AS ${family==='columns'?'column_id':'row_id'},
    ${family==='columns'?`CASE WHEN i=0 THEN repeat('N',65537) WHEN i%4=0 THEN NULL::VARCHAR WHEN i%4=1 THEN '' WHEN i%4=2 THEN 'NULL' ELSE 'İstanbul 🦆' END AS name,
    CASE WHEN i%5=0 THEN NULL::DOUBLE ELSE (i%17)::DOUBLE+0.25 END AS width`:
        `CAST(CASE WHEN i%3=2 THEN NULL::BOOLEAN ELSE i%3=0 END AS VARCHAR) AS enabled,
    CASE WHEN i%4=0 THEN NULL::VARCHAR WHEN i%4=1 THEN '' WHEN i%4=2 THEN 'NULL' ELSE 'İstanbul 🦆' END AS value`}
    FROM range(${rows}) AS source(i) ORDER BY i`;
async function deadline(value,ms) {
    let timer;
    try{return await Promise.race([value,new Promise((resolve,reject)=>{timer=setTimeout(()=>{
        const error=new Error(`Metadata stream probe exceeded ${ms}ms; caller must close a nonretired engine`);error.code='PROBE_TIMEOUT';reject(error);
    },ms);})]);}finally{clearTimeout(timer);}
}

export async function runMetadataStreamProbe(adapter,binding,{rows=513,timeoutMs=20000}={}) {
    if(!Number.isInteger(rows)||rows<257||2*rows+2*STOP_ROWS+2>MAX_TOTAL_ROWS||
        !Number.isInteger(timeoutMs)||timeoutMs<1000||timeoutMs>120000)
        throw new TypeError('Use257..9486 success rows, at most20,000 total SQL rows, and1000..120000ms per case');
    if(!adapter||typeof adapter.submitWorkbookMetadataStream!=='function'||typeof adapter.submit!=='function'||
        typeof adapter.cancel!=='function'||!adapter.connection||typeof adapter.connection.send!=='function'||
        typeof adapter.latestId!=='bigint'||adapter.latestId<0n||adapter.latestId>MAX_U64-6n||
        adapter.closed||adapter.active||adapter.pending||adapter.draining||adapter.admin)
        throw new TypeError('Probe requires one exclusively idle live adapter with six available request IDs');
    if(!binding||typeof binding!=='object')throw new TypeError('Privately registered immutable archive token required');
    const token=Object.freeze({name:binding.name,engineEpoch:binding.engineEpoch,registrationId:binding.registrationId});
    let next=adapter.latestId+1n;
    const request=(sql,extra={})=>({...identity,requestId:String(next++),sql,knownTotal:null,sourceBindings:[token],...extra});
    // Validate the borrowed binding before wrapping any method or reserving ID.
    workbookMetadataStreamRequest({...identity,requestId:String(next),sql:'SELECT 1',purpose:'workbook-metadata-stream',family:'cells',columns:columns('cells'),sourceBindings:[token]});
    const started=now(),cases=[],records=[];let checks=0,postedTurns=0,failure=null,cleanup=null;
    const check=(value,label)=>{checks++;if(!value)throw new Error(label);};
    const equal=(actual,expected,label)=>check(actual===expected,`${label}: expected${String(expected).slice(0,80)} received${String(actual).slice(0,80)}`);
    const turn=async()=>{await new Promise(resolve=>setTimeout(resolve,0));postedTurns++;};
    const connection=adapter.connection,original=connection.send,descriptor=Object.getOwnPropertyDescriptor(connection,'send');
    // Observe only; every method receives the original reader/iterator as this.
    // Do not fabricate a schema, batch, completion or successful cleanup.
    connection.send=async function(...args) {
        const record={sql:args[0],stream:args[1],open:0,next:0,inflight:0,maxInflight:0,batches:0,batchRows:[],
            eof:0,cancelRequested:0,cancelFulfilled:0,cancelError:null,cancelMethod:false,rawSchema:null,schemaObservationError:null};records.push(record);
        const reader=await original.apply(this,args);record.cancelMethod=typeof reader?.cancel==='function';
        const observed={get schema(){
            const schema=reader.schema;
            // First already-open physical schema only: no DESCRIBE/fetch, type
            // inference or Arrow rewrite. Diagnostics must not change delivery.
            if(record.rawSchema===null&&!record.schemaObservationError)try{
                if(Array.isArray(schema?.fields))record.rawSchema={columnCount:schema.fields.length,fieldsTruncated:schema.fields.length>4,
                    fields:schema.fields.slice(0,4).map(field=>{const name=String(field.name),type=String(field.type);return {
                        name:name.slice(0,4096),nameTruncated:name.length>4096,type:type.slice(0,256),typeTruncated:type.length>256};})};
            }catch(error){record.schemaObservationError=errorRecord(error);}
            return schema;
        },[Symbol.asyncIterator](){
            const iterator=reader[Symbol.asyncIterator](),observedIterator={async next(...arguments_) {
                record.next++;record.inflight++;record.maxInflight=Math.max(record.maxInflight,record.inflight);
                try{const item=await iterator.next.apply(iterator,arguments_);if(item.done)record.eof++;else{record.batches++;if(record.batchRows.length<16)record.batchRows.push(item.value.numRows);}return item;}
                finally{record.inflight--;}
            }};
            for(const name of ['return','throw'])if(typeof iterator[name]==='function')observedIterator[name]=(...arguments_)=>iterator[name].apply(iterator,arguments_);
            return observedIterator;
        }};
        if(typeof reader.open==='function')observed.open=(...arguments_)=>{record.open++;return reader.open.apply(reader,arguments_);};
        if(record.cancelMethod)observed.cancel=async(...arguments_)=>{record.cancelRequested++;try{const value=await reader.cancel.apply(reader,arguments_);record.cancelFulfilled++;return value;}catch(error){record.cancelError=errorRecord(error);throw error;}};
        return observed;
    };
    const recordFor=request=>{const record=records.find(value=>value.sql===request.sql);check(record,'Actual connection.send observation missing');return record;};
    const validate=(chunk,req,state,family,expectedRows)=>{
        for(const name of idFields)equal(chunk[name],req[name],`Chunk ${name}`);
        equal(chunk.sequence,++state.sequence,'One-based chunk sequence');equal(chunk.firstRow,String(state.rows),'Exact firstRow');
        const page=chunk.page;check(Object.isFrozen(chunk)&&Object.isFrozen(page),'Frozen owned chunk/Page envelope');
        equal(page.columnCount,4,'Four stream columns');equal(page.rowCount,chunk.end?0:Math.min(256,expectedRows-state.rows),'Dense256 chunk and exact tail');
        equal(page.offset,String(state.rows),'Page offset');equal(page.total,String(state.rows+page.rowCount),'Exact total lower bound');equal(page.countPending,!chunk.end,'Explicit EOF completeness');
        for(const name of idFields)if(name!=='offset')equal(page[name],req[name],`Page ${name}`);
        const expected=columns(family);for(let c=0;c<4;c++){equal(page.columns[c].name,expected[c].name,'Typed field name');equal(page.columns[c].typeName,expected[c].typeName,'Original SQL type');equal(page.columns[c].numberKind,expected[c].numberKind,'Numeric sidecar');}
        if(family==='cells'){
            equal(page.columns[2].arrowType,'Utf8','Canonical BOOLEAN physical Arrow type');check(Object.isFrozen(page.columns[2]),'Original BOOLEAN sidecar remains frozen');
            state.canonicalBooleanProjection=true;
        }
        const bytes=new TextEncoder().encode(JSON.stringify(page.columns)).byteLength+page.text.byteLength+page.offsets.byteLength+page.nulls.byteLength;
        check(bytes<=LIMITS.bytes&&chunk.wire instanceof Uint8Array&&chunk.wire.byteLength<=6*1024*1024,'Page+wire transport bounds');
        state.maxPageBytes=Math.max(state.maxPageBytes,bytes);state.maxWireBytes=Math.max(state.maxWireBytes,chunk.wire.byteLength);
        state.shapes.push({sequence:chunk.sequence,firstRow:chunk.firstRow,rows:page.rowCount,end:chunk.end});
        for(let row=0;row<page.rowCount;row++) {
            const i=state.rows+row,text=i%4===0?null:i%4===1?'':i%4===2?'NULL':'İstanbul 🦆';
            equal(pageCellText(page,row,0),String(i%64),'Exact sheet INTEGER');equal(pageCellText(page,row,1),i%2?'9007199254740993':'18446744073709551615','Exact full U64 text');
            if(family==='columns'){
                equal(pageCellText(page,row,2),i===0?'N'.repeat(65537):text,'Exact long name/NULL/empty/literal');equal(pageCellText(page,row,3),i%5===0?null:String(i%17+.25),'Exact fractional width/NULL');
            }else{equal(pageCellText(page,row,2),i%3===2?null:i%3===0?'true':'false','Exact BOOLEAN/NULL');equal(pageCellText(page,row,3),text,'Exact value NULL/empty/literal/Unicode');}
        }
        state.rows+=page.rowCount;state.callbacks++;if(chunk.end)state.ends++;
    };
    const state=()=>({rows:0,sequence:0,callbacks:0,ends:0,shapes:[],maxPageBytes:0,maxWireBytes:0,canonicalBooleanProjection:false});
    async function step(name,operation) {
        const start=now(),before=records.length;
        try{const result=await deadline(operation(),timeoutMs);equal(records.length-before,1,`${name} exactly one SQL execution`);cases.push({name,status:'passed',milliseconds:now()-start,...result});}
        catch(error){cases.push({name,status:'failed',milliseconds:now()-start,error:errorRecord(error)});throw error;}
    }
    async function complete(family) {
        const req=request(sourceSql(family,rows,family),{purpose:'workbook-metadata-stream',family,columns:columns(family)}),observed=state();let settled=false;
        const work=adapter.submitWorkbookMetadataStream(req,async chunk=>{
            const record=recordFor(req);validate(chunk,req,observed,family,rows);
            if(chunk.sequence===1){const before=record.next,callbacks=observed.callbacks;await turn();equal(record.next,before,'Paused data ACK prevents iterator advancement');equal(observed.callbacks,callbacks,'Paused data ACK prevents second delivery');observed.firstAckPostedTurn=true;}
            if(chunk.end){equal(observed.rows,rows,'All rows precede EOF');equal(record.eof,1,'Real Arrow iterator EOF observed');equal(record.cancelRequested,1,'Real reader.cancel requested');equal(record.cancelFulfilled,1,'Real reader.cancel fulfilled before EOF callback');const before=record.next;await turn();check(!settled,'Final result cannot settle before final true ACK');equal(record.next,before,'Final ACK performs no additional iterator read');observed.eofAfterCleanup=true;observed.finalAckPostedTurn=true;}
            return true;
        });work.then(()=>{settled=true;},()=>{settled=true;});const reply=await work;
        equal(reply.status,'ok',JSON.stringify(reply.error??reply.status));equal(reply.rowCount,String(rows),'Exact successful row count');equal(reply.dataChunks,Math.ceil(rows/256),'Data chunk count');equal(reply.chunks,observed.callbacks,'Final ACK counted');equal(observed.ends,1,'Exactly one explicit EOF');
        for(const name of idFields)equal(reply[name],req[name],`Reply ${name}`);return {family,requestId:req.requestId,...observed};
    }
    async function interrupted(kind) {
        const req=request(sourceSql('cells',STOP_ROWS,kind),{purpose:'workbook-metadata-stream',family:'cells',columns:columns('cells')}),observed=state(),entered=pending(),ack=pending();
        const work=adapter.submitWorkbookMetadataStream(req,chunk=>{validate(chunk,req,observed,'cells',STOP_ROWS);check(!chunk.end,'Interrupted stream emitted no EOF');entered.resolve();return kind==='cancel'?ack.promise:false;});
        await entered.promise;const record=recordFor(req);
        if(kind==='cancel'){const before=record.next;await turn();equal(record.next,before,'Cancelled suspended ACK blocks iterator advancement');equal(observed.callbacks,1,'One suspended delivery');await adapter.cancel();}
        const reply=await work;equal(reply.status,kind==='cancel'?'cancelled':'error',JSON.stringify(reply.error??reply.status));
        if(kind!=='cancel')equal(reply.error?.code,'STREAM_CONSUMER','False ACK reports consumer error');equal(observed.callbacks,1,'Interrupted stream stops after one data chunk');equal(observed.ends,0,'Interrupted stream has no final callback');equal(record.cancelRequested,1,'Interrupted real reader cleanup requested');equal(record.cancelFulfilled,1,'Interrupted real reader cleanup fulfilled');
        if(kind==='cancel'){ack.reject(new Error('Probe late ACK rejection after cancellation'));await turn();equal(observed.callbacks,1,'Late rejected ACK cannot resume stream');}
        return {family:'cells',requestId:req.requestId,streamStatus:reply.status,errorCode:reply.error?.code??null,...observed};
    }
    async function recovery(marker) {
        const req=request(`/* metadata-stream-probe:recovery-${marker} */ SELECT ${marker}::INTEGER AS recovered`,{rowLimit:1}),reply=await adapter.submit(req);
        equal(reply.status,'ok',JSON.stringify(reply.error??reply.status));equal(reply.page.rowCount,1,'Ordinary recovery one row');equal(reply.page.columnCount,1,'Ordinary recovery one column');equal(pageCellText(reply.page,0,0),String(marker),'Unrelated ordinary query recovers');
        for(const name of idFields)equal(reply[name],req[name],`Recovery ${name}`);return {requestId:req.requestId,value:marker};
    }
    try {
        await step('columns-exact-and-final-ack',()=>complete('columns'));
        await step('cells-exact-and-final-ack',()=>complete('cells'));
        await step('cancel-suspended-ack',()=>interrupted('cancel'));
        await step('ordinary-after-cancel',()=>recovery(43));
        await step('reject-consumer-ack',()=>interrupted('false-ack'));
        await step('ordinary-after-failed-ack',()=>recovery(44));
        equal(records.length,6,'Six total connection sends');check(records.every(value=>value.stream===true&&value.maxInflight<=1),'One real streaming Arrow iterator at a time');
    } catch(error) {
        failure=errorRecord(error);
        try{await deadline(adapter.cancel(),timeoutMs);cleanup={cancelDrained:true};}catch(error){cleanup={cancelDrained:false,error:errorRecord(error)};}
    } finally {
        if(descriptor)Object.defineProperty(connection,'send',descriptor);else delete connection.send;
    }
    const passed=name=>cases.find(value=>value.name===name&&value.status==='passed');
    const positive=cases.filter(value=>value.status==='passed'&&value.eofAfterCleanup);
    const cancelled=passed('cancel-suspended-ack'),rejected=passed('reject-consumer-ack');
    return Object.freeze({ok:!failure,schema:'duxcel.metadata-stream-probe',version:1,checks,milliseconds:now()-started,
        engineVersion:adapter.engineVersion??null,rowsPerSuccessfulFamily:rows,plannedSqlRows:2*rows+2*STOP_ROWS+2,sqlExecutions:records.length,postedTurns,cases,
        proofs:{exactTypedFamilies:positive.length,strictEofCleanups:positive.filter(value=>value.ends===1).length,
            dataAckPostedTurns:positive.filter(value=>value.firstAckPostedTurn).length,finalAckPostedTurns:positive.filter(value=>value.finalAckPostedTurn).length,
            cancelledWithoutEof:Boolean(cancelled&&cancelled.streamStatus==='cancelled'&&!cancelled.ends),
            falseAckWithoutEof:Boolean(rejected&&rejected.errorCode==='STREAM_CONSUMER'&&!rejected.ends),
            canonicalBooleanProjection:Boolean(passed('cells-exact-and-final-ack')?.canonicalBooleanProjection&&cancelled?.canonicalBooleanProjection&&rejected?.canonicalBooleanProjection),
            ordinaryAfterCancel:passed('ordinary-after-cancel')?.value??null,ordinaryAfterFalseAck:passed('ordinary-after-failed-ack')?.value??null},
        readers:records.map(({sql,...value})=>({...value,batchRows:[...value.batchRows],sql})),error:failure,cleanup,
        boundary:'Caller-supplied adapter provenance required; synthetic SQL transport only. One Page/wire plus staged row excludes Arrow/allocator/caller-retained memory. No C codec, native metadata parity, Workbook publication, UI, source content or Save/Open claim.'});
}
