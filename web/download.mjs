// Thin browser download dispatch only. It never claims that bytes reached disk.
const MAX_BYTES=64*1024*1024,MAX_NAME_BYTES=255,MAX_ERROR_BYTES=512,GRACE_MS=2000;
const typedPrototype=Object.getPrototypeOf(Uint8Array.prototype);
const byteLength=Object.getOwnPropertyDescriptor(typedPrototype,'byteLength').get;
const backingBuffer=Object.getOwnPropertyDescriptor(typedPrototype,'buffer').get;
function validName(name){
    if(typeof name!=='string'||name.length>MAX_NAME_BYTES||!name.toLowerCase().endsWith('.parquet'))return false;
    let size=0;
    for(let i=0;i<name.length;i++){
        const unit=name.charCodeAt(i);
        if(unit<32||unit===127||unit===47||unit===92)return false;
        if(unit>=0xd800&&unit<=0xdbff){
            const next=name.charCodeAt(++i);if(!(next>=0xdc00&&next<=0xdfff))return false;size+=4;
        }else if(unit>=0xdc00&&unit<=0xdfff)return false;
        else size+=unit<0x80?1:unit<0x800?2:3;
        if(size>MAX_NAME_BYTES)return false;
    }
    return true;
}
function validParquet(bytes){
    if(!(bytes instanceof Uint8Array))return false;
    const size=byteLength.call(bytes),buffer=backingBuffer.call(bytes);
    if(size<12||size>MAX_BYTES||(typeof SharedArrayBuffer!=='undefined'&&buffer instanceof SharedArrayBuffer))return false;
    if(bytes[0]!==80||bytes[1]!==65||bytes[2]!==82||bytes[3]!==49||bytes[size-4]!==80||bytes[size-3]!==65||bytes[size-2]!==82||bytes[size-1]!==49)return false;
    const footer=bytes[size-8]+bytes[size-7]*256+bytes[size-6]*65536+bytes[size-5]*16777216;
    return footer>0&&footer<=size-12;
}
function diagnostic(error){
    let text='Parquet download dispatch failed.';
    try{
        if(typeof error==='string'&&error)text=error;
        else if(error&&(typeof error==='object'||typeof error==='function')){
            const message=error.message;
            if(typeof message==='string'&&message)text=message;
            else{const name=error.name;if(typeof name==='string'&&name)text=name;}
        }
    }catch{/* Hostile exception getters/coercion are not platform diagnostics. */}
    let output='',size=0;
    for(let i=0;i<text.length;i++){
        const unit=text.charCodeAt(i);let value=text[i],bytes;
        if(unit>=0xd800&&unit<=0xdbff){
            const next=text.charCodeAt(i+1);
            if(next>=0xdc00&&next<=0xdfff){value=text.slice(i,i+2);i++;bytes=4;}else{value='\ufffd';bytes=3;}
        }else if(!unit||(unit>=0xdc00&&unit<=0xdfff)){value='\ufffd';bytes=3;}
        else bytes=unit<0x80?1:unit<0x800?2:3;
        if(size+bytes>MAX_ERROR_BYTES)break;output+=value;size+=bytes;
    }
    return output||'Parquet download dispatch failed.';
}
function defaultBlob(bytes){
    if(typeof globalThis.Blob!=='function')throw new Error('Blob downloads are unavailable in this browser.');
    return new globalThis.Blob([bytes],{type:'application/vnd.apache.parquet'});
}
function defaultURL(blob){
    if(typeof globalThis.URL?.createObjectURL!=='function'||typeof globalThis.URL?.revokeObjectURL!=='function')throw new Error('Blob URLs are unavailable in this browser.');
    return globalThis.URL.createObjectURL(blob);
}
function defaultRevoke(url){
    if(typeof globalThis.URL?.revokeObjectURL!=='function')throw new Error('Blob URL cleanup is unavailable in this browser.');
    globalThis.URL.revokeObjectURL(url);
}
function defaultSchedule(callback,delay){
    if(typeof globalThis.setTimeout!=='function'||typeof globalThis.clearTimeout!=='function')throw new Error('Download cleanup timers are unavailable.');
    return globalThis.setTimeout(callback,delay);
}
function defaultCancel(handle){globalThis.clearTimeout(handle);}
function defaultDispatch(url,name){
    const document=globalThis.document;
    if(!document?.body||typeof document.createElement!=='function')throw new Error('Browser download dispatch is unavailable.');
    const anchor=document.createElement('a');anchor.hidden=true;anchor.href=url;anchor.download=name;
    try{document.body.appendChild(anchor);anchor.click();}finally{anchor.remove();}
}
const outcome=(ok,error=null)=>Object.freeze({ok,error});

/** createBlob(view) MUST synchronously snapshot its borrowed byte view, as the
 * built-in Blob constructor does. No extra full-size JS byte copy is made here.
 * URL/dispatch/timer functions are synchronous; success means dispatch returned,
 * never durable saving. Only one URL/timer is retained, never the borrowed view.
 * dispose is idempotent; failed revocation retains ownership for a later retry. */
export function createBrowserDownload({createBlob=defaultBlob,createURL=defaultURL,revokeURL=defaultRevoke,dispatch=defaultDispatch,schedule=defaultSchedule,cancel=defaultCancel}={}){
    for(const [name,fn] of Object.entries({createBlob,createURL,revokeURL,dispatch,schedule,cancel}))if(fn!==null&&typeof fn!=='function')throw new TypeError(`Download ${name} must be a function or unavailable.`);
    let active=null,disposed=false;
    function release(operation){
        if(operation.releasing)return null;
        operation.releasing=true;let failure=null;
        if(operation.hasTimer){
            const timer=operation.timer;operation.hasTimer=false;operation.timer=undefined;
            try{if(!cancel)throw new Error('Download timer cancellation is unavailable.');cancel(timer);}catch(error){failure=error;}
        }
        if(operation.url!==null){
            const url=operation.url;operation.url=null;
            try{if(!revokeURL)throw new Error('Blob URL cleanup is unavailable.');revokeURL(url);}catch(error){operation.url=url;failure??=error;}
        }
        operation.releasing=false;if(operation.url===null&&active===operation)active=null;
        return failure;
    }
    function current(operation){if(disposed||active!==operation)throw new Error('Download dispatch was disposed.');}
    // Build timer closures outside request's borrowed-buffer scope. The only
    // per-request value they retain is the scalar URL/timer ownership record.
    function expiry(operation){return ()=>{
        operation.fired=true;
        if(active!==operation)return;
        operation.hasTimer=false;operation.timer=undefined;release(operation);
    };}
    function request(bytes,name){
        if(disposed)return outcome(false,'Download dispatch is disposed.');
        if(active)return outcome(false,'A previous download URL is still being released.');
        try{if(!validName(name))throw new Error('Choose a valid Parquet basename of at most 255 UTF-8 bytes.');if(!validParquet(bytes))throw new Error('Expected a bounded Parquet file with a valid PAR1 header/footer.');}catch(error){return outcome(false,diagnostic(error));}
        const operation=active={url:null,timer:undefined,hasTimer:false,releasing:false,fired:false};
        try{
            if(!createBlob||!createURL||!revokeURL||!dispatch||!schedule||!cancel)throw new Error('Browser download dispatch is unavailable.');
            // Blob construction takes its snapshot before this function returns
            // and before the C owner may release/grow its borrowed heap buffer.
            const blob=createBlob(bytes);current(operation);
            const url=createURL(blob);
            if(typeof url!=='string'||!url)throw new Error('Download URL creation did not return a URL.');
            operation.url=url;current(operation);dispatch(url,name);current(operation);
            const timer=schedule(expiry(operation),GRACE_MS);
            operation.timer=timer;operation.hasTimer=true;
            if(operation.fired)throw new Error('Download cleanup timer fired synchronously.');
            current(operation);return outcome(true);
        }catch(error){release(operation);return outcome(false,diagnostic(error));}
    }
    function dispose(){disposed=true;if(active)release(active);}
    return Object.freeze({request,dispose});
}
