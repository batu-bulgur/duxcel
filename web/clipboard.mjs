// Browser clipboard Promise ownership only. The platform injects writeText;
// this module has no navigator, DOM or C-memory access.
const MAX_BYTES=8*1024*1024,MAX_ERROR_BYTES=512,MAX_ID=0x7fffffff;
function validText(text,maximum){
    if(typeof text!=='string'||text.length>maximum)return false;
    let bytes=0;
    for(let i=0;i<text.length;i++){
        const unit=text.charCodeAt(i);
        if(unit>=0xd800&&unit<=0xdbff){
            const next=text.charCodeAt(++i);
            if(!(next>=0xdc00&&next<=0xdfff))return false;
            bytes+=4;
        }else if(unit>=0xdc00&&unit<=0xdfff)return false;
        else bytes+=unit<0x80?1:unit<0x800?2:3;
        if(bytes>maximum)return false;
    }
    return true;
}
function diagnosticPrefix(text,maximum){
    let output='',bytes=0;
    for(let i=0;i<text.length;i++){
        const unit=text.charCodeAt(i);let value=text[i],size;
        if(unit>=0xd800&&unit<=0xdbff){
            const next=text.charCodeAt(i+1);
            if(next>=0xdc00&&next<=0xdfff){value=text.slice(i,i+2);i++;size=4;}
            else{value='\ufffd';size=3;}
        }else if(!unit||(unit>=0xdc00&&unit<=0xdfff)){value='\ufffd';size=3;}
        else size=unit<0x80?1:unit<0x800?2:3;
        if(bytes+size>maximum)break;
        output+=value;bytes+=size;
    }
    return output;
}
function diagnostic(error,maximum){
    let text='Clipboard write failed.';
    try{
        if(typeof error==='string'&&error)text=error;
        else if(error&&(typeof error==='object'||typeof error==='function')){
            const message=error.message;
            if(typeof message==='string'&&message)text=message;
            else{const name=error.name;if(typeof name==='string'&&name)text=name;}
        }
    }catch{/* A rejection value need not expose safe getters or coercion. */}
    return diagnosticPrefix(text,maximum)||diagnosticPrefix('Clipboard write failed.',maximum);
}

/** One pending write or unconsumed outcome. begin calls the injected writer
 * synchronously, retaining immutable JS text before its Promise can suspend.
 * Positive IDs never wrap/recycle; 0 means invalid, busy, exhausted or disposed.
 * Success requires Promise fulfillment. take moves one bounded terminal result;
 * disposal consumes no OS operation and prevents every late publication. */
export function createBrowserClipboard({writeText,maxBytes=MAX_BYTES,maxErrorBytes=MAX_ERROR_BYTES}={}){
    if(writeText!==undefined&&writeText!==null&&typeof writeText!=='function')throw new TypeError('Clipboard writeText must be a function or unavailable.');
    if(!Number.isSafeInteger(maxBytes)||maxBytes<0||maxBytes>MAX_BYTES)throw new TypeError('Clipboard maxBytes must be 0–8388608.');
    if(!Number.isSafeInteger(maxErrorBytes)||maxErrorBytes<1||maxErrorBytes>MAX_ERROR_BYTES)throw new TypeError('Clipboard maxErrorBytes must be 1–512.');
    let active=null,lastId=0,disposed=false;
    function settle(operation,ok,error){
        if(disposed||active!==operation||operation.result)return;
        const result=Object.freeze({id:operation.id,ok,error:ok?null:diagnostic(error,maxErrorBytes)});
        operation.text=null;
        if(!disposed&&active===operation)operation.result=result;
    }
    function begin(text){
        if(disposed||active||lastId===MAX_ID||!validText(text,maxBytes))return 0;
        const operation=active={id:++lastId,text,result:null};
        try{
            if(!writeText)throw new Error('Clipboard writing is unavailable in this browser or context.');
            const promise=writeText(operation.text),then=promise?.then;
            if(typeof then!=='function')throw new Error('Clipboard writer did not return a Promise.');
            // Capture then once; Promise adoption protects against duplicate
            // resolve/reject calls and handles exceptions without an unhandled
            // rejection even when this owner is disposed in the meantime.
            new Promise((resolve,reject)=>then.call(promise,resolve,reject)).then(
                ()=>settle(operation,true,null),error=>settle(operation,false,error));
        }catch(error){settle(operation,false,error);}
        return operation.id;
    }
    function take(){
        if(disposed||!active?.result)return null;
        const result=active.result;active=null;return result;
    }
    function dispose(){disposed=true;if(active)active.text=null;active=null;}
    return Object.freeze({begin,take,dispose});
}
