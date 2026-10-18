/** Test-only C callback seam for the partial workbook cursor. Not an App lease,
 * production ABI, SQL builder or owner of the caller's adapter/engine lifetime.
 * C must check cookie/ticket/status before borrowing any temporary pointer.
 */
import {workbookMetadataStreamRequest} from './official_worker.mjs';

const encoder=new TextEncoder(),MAX_U64=18446744073709551615n,MAX_U32=4294967295;
const IDENTITIES=['requestId','documentId','generation','sourceId','objectId','revision','offset'];
function check(value,message){if(!value)throw new TypeError(`Codec stream: ${message}`);}
function uint32(value){return Number.isInteger(value)&&value>0&&value<=MAX_U32;}
function decimal(value){return typeof value==='string'&&/^(0|[1-9][0-9]{0,19})$/.test(value)&&BigInt(value)<=MAX_U64;}
function message(error){
    let text;try{text=String(error?.message??error);}catch{text='Codec stream transport failed';}
    let result='',bytes=0;
    for(const value of text){const ch=value==='\0'?'\ufffd':value,length=encoder.encode(ch).byteLength;if(bytes+length>2048)break;result+=ch;bytes+=length;}
    return result;
}
function problem(error,request){return {...Object.fromEntries(IDENTITIES.map(name=>[name,request[name]])),status:'error',error:{code:'CODEC_STREAM_TRANSPORT',message:message(error)}};}
function temporary(runtime,body){
    const owned=[];let result,thrown,failed=false;
    const bytes=data=>{
        const pointer=runtime.allocate(data.byteLength||1);
        check(uint32(pointer),'C allocation failed or returned an invalid pointer');owned.push(pointer);
        const heap=runtime.heap(); // allocate may have grown/detached the old HEAPU8.
        check(heap instanceof Uint8Array&&heap.byteOffset===0&&pointer<=heap.byteLength-data.byteLength,'C allocation is outside the current heap');
        heap.set(data,pointer);return pointer;
    };
    const string=value=>{const data=encoder.encode(value),terminated=new Uint8Array(data.byteLength+1);terminated.set(data);return bytes(terminated);};
    try{result=body(bytes,string);}catch(error){thrown=error;failed=true;}
    finally{for(const pointer of owned.reverse())try{runtime.release(pointer);}catch(error){if(!failed)thrown=error;failed=true;}}
    if(failed)throw thrown;return result;
}
export function createCodecStreamTransport(adapter,runtime){
    check(adapter&&typeof adapter.submitWorkbookMetadataStream==='function'&&typeof adapter.cancel==='function','dedicated adapter required');
    check(runtime&&['heap','allocate','release','chunk','done','held'].every(name=>typeof runtime[name]==='function'),'complete C runtime required');
    let operation=null,retiring=null,held=null,replaying=false,closed=false;
    const busy=()=>Boolean(operation||retiring||replaying);
    async function quiescent(){
        // The adapter resolves a task just before its outer drain clears active.
        // Await that public drain fence before C completion or administration.
        while(adapter.draining)await adapter.draining;
    }
    function deliver(session,chunk){
        const accepted=temporary(runtime,(bytes,string)=>runtime.chunk(session.cookie,session.ticket,chunk.sequence,
            string(chunk.firstRow),chunk.end?1:0,bytes(chunk.wire),chunk.wire.byteLength,string(chunk.page.total),chunk.page.countPending?1:0));
        // Every temporary allocation is retired BEFORE this boolean ACK escapes.
        if(accepted&&typeof accepted.then==='function')void Promise.resolve(accepted).catch(()=>{}); // Async callbacks cannot ACK C borrows.
        const ack=accepted===1;
        if(ack&&!session.finished){session.acked++;session.rows+=BigInt(chunk.page.rowCount);session.eof=chunk.end;}
        return ack;
    }
    function consume(session,chunk){
        check(operation===session&&!closed&&!session.cancelRequested,'inactive stream delivery');
        for(const name of IDENTITIES)check(chunk?.[name]===session.request[name],`mismatched chunk ${name}`);
        check(Number.isInteger(chunk.sequence)&&chunk.sequence===session.acked+1&&chunk.sequence<=session.request.rowLimit+1,'invalid chunk sequence');
        check(decimal(chunk.firstRow)&&BigInt(chunk.firstRow)===session.rows,'invalid chunk firstRow');
        check(typeof chunk.end==='boolean'&&!session.eof,'invalid EOF lifetime');
        const page=chunk.page,wire=chunk.wire;
        check(page&&Number.isInteger(page.rowCount)&&page.rowCount>=0&&page.rowCount<=256&&page.columnCount===4&&page.offset===chunk.firstRow,'invalid chunk geometry');
        for(const name of IDENTITIES.filter(name=>name!=='offset'))check(page[name]===session.request[name],`mismatched Page ${name}`);
        check(decimal(page.total)&&BigInt(page.total)===session.rows+BigInt(page.rowCount)&&BigInt(page.total)<=BigInt(session.request.rowLimit),'invalid chunk total');
        check(page.countPending===!chunk.end&&(chunk.end?page.rowCount===0:page.rowCount>0),'invalid chunk EOF/pending state');
        check(wire instanceof Uint8Array&&wire.buffer instanceof ArrayBuffer&&wire.byteLength>=24&&wire.byteLength<=6*1024*1024&&wire.buffer!==runtime.heap().buffer,'invalid owned chunk wire');
        if(chunk.sequence!==session.holdSequence)return deliver(session,chunk);
        check(!held,'only one held chunk is permitted');
        let resolve;const promise=new Promise(yes=>{resolve=yes;});held={session,chunk,resolve};
        try{runtime.held(session.cookie,session.ticket,chunk.sequence);}
        catch(error){session.transportError=error;held=null;resolve(false);}
        return promise;
    }
    function complete(session,reply){
        let result=reply;
        try{
            if(session.transportError)throw session.transportError;
            for(const name of IDENTITIES)check(reply?.[name]===session.request[name],`mismatched completion ${name}`);
            check(['ok','cancelled','superseded','error'].includes(reply.status),'invalid completion status');
            if(reply.status==='ok')check(session.eof&&decimal(reply.rowCount)&&BigInt(reply.rowCount)===session.rows&&
                reply.chunks===session.acked&&reply.dataChunks===session.acked-1,'success requires acknowledged final EOF');
            if(session.cancelRequested&&reply.status==='ok')result={...reply,status:'cancelled'};
        }catch(error){result=problem(error,session.request);}
        let code=result.status==='ok'?0:result.status==='cancelled'||result.status==='superseded'?1:2;
        const rows=code===0?result.rowCount:'0',chunks=session.acked,error=code===0?'':message(result.error?.message??`Metadata stream ${result.status}`);
        let called=false;
        try{temporary(runtime,(_bytes,string)=>{const count=string(rows),diagnostic=error?string(error):0;called=true;
            return runtime.done(session.cookie,session.ticket,code,count,chunks,diagnostic);});}
        catch(failure){
            result=problem(failure,session.request);
            // Static C failure fallback only before callback entry: never call
            // completion twice if the synchronous runtime itself threw.
            if(!called)runtime.done(session.cookie,session.ticket,2,0,chunks,0);
        }
        return result;
    }
    function submit(cookie,ticket,input,options={}){
        check(uint32(cookie)&&uint32(ticket),'nonzero uint32 cookie/ticket required');
        const request=workbookMetadataStreamRequest(input),holdSequence=options?.holdSequence??0;
        check(request.documentId===String(cookie)&&request.requestId===String(ticket),'request cookie/ticket mismatch');
        check(Number.isInteger(holdSequence)&&holdSequence>=0&&holdSequence<=request.rowLimit+1,'invalid hold sequence');
        check(!closed&&!busy()&&!held&&!adapter.active&&!adapter.pending&&!adapter.draining&&!adapter.admin&&!adapter.closed,'transport/adapter is not quiescent');
        const session={cookie,ticket,request,holdSequence,acked:0,rows:0n,eof:false,finished:false,cancelRequested:false,transportError:null,promise:null};operation=session;
        let submitted;try{submitted=adapter.submitWorkbookMetadataStream(request,chunk=>consume(session,chunk));}catch(error){submitted=Promise.reject(error);}
        session.promise=(async()=>{
            let reply;try{reply=await submitted;}catch(error){reply=problem(error,session.request);}
            try{await quiescent();}catch(error){reply=problem(error,session.request);}
            return complete(session,reply);
        })().finally(()=>{session.finished=true;if(operation===session)operation=null;});
        return session.promise;
    }
    function releaseHeld(){
        if(!held||closed)return false;
        const value=held;held=null;replaying=true;let ack=false;
        try{ack=deliver(value.session,value.chunk);}catch(error){value.session.transportError=error;}
        finally{replaying=false;value.resolve(ack);}
        return ack;
    }
    function cancel(){
        if(retiring)return retiring;
        const current=operation;if(current)current.cancelRequested=true;
        let cancelled;try{cancelled=adapter.cancel();}catch(error){cancelled=Promise.reject(error);}
        retiring=(async()=>{await cancelled;if(current)await current.promise;await quiescent();})().finally(()=>{retiring=null;});
        return retiring;
    }
    function close(){
        closed=true;const result=cancel();
        if(held){const value=held;held=null;value.resolve(false);}
        return result; // The caller still owns adapter.close()/database disposal.
    }
    return Object.freeze({submit,cancel,releaseHeld,close,get active(){return busy();},get hasHeld(){return Boolean(held);},get closed(){return closed;}});
}
