// Test-driver transport only; never packaged with the browser application.
export function createCDP(socket,events,{now=Date.now,setTimer=setTimeout,clearTimer=clearTimeout}={}) {
    let nextId=0,disconnected=null,lastFailure=null;
    const pending=new Map(),held=new Map();
    const snapshot=()=>[...pending.values()].map(request=>({...request.metadata}));
    function failure(kind,message,request=null) {
        const error=new Error(message);
        lastFailure={kind,error:message,time:now(),request,pending:snapshot()};
        return error;
    }
    function settle(id,error,value) {
        const request=pending.get(id); if (!request) return;
        pending.delete(id); clearTimer(request.timer);
        if (error) request.reject(error); else request.resolve(value);
    }
    function disconnect(message) {
        if (disconnected) return;
        disconnected=message;
        const error=failure('disconnected',message);
        for (const id of [...pending.keys()]) settle(id,error);
    }
    socket.addEventListener('close',event=>disconnect(`CDP WebSocket closed (code ${event.code??'unknown'}${event.reason?': '+event.reason:''})`));
    socket.addEventListener('error',event=>disconnect('CDP WebSocket error'+(event.message?': '+event.message:'')));
    socket.addEventListener('message',event=>{
        let message;
        try { message=JSON.parse(event.data); }
        catch (error) { disconnect('Invalid CDP WebSocket message: '+error.message); return; }
        if (!message.id) { events.push(message); return; }
        const request=pending.get(message.id); if (!request) return;
        if (message.error) settle(message.id,failure('protocol',`CDP ${request.metadata.method}: ${JSON.stringify(message.error)}`,request.metadata));
        else settle(message.id,null,message.result);
    });
    function send(method,params={},timeout=15000,context='') {
        if (!disconnected && socket.readyState!==1) disconnect('CDP WebSocket is not open (state '+socket.readyState+')');
        if (disconnected) return Promise.reject(new Error(disconnected));
        const id=++nextId,metadata={id,method,started_ms:now(),timeout_ms:timeout,context};
        return new Promise((resolve,reject)=>{
            const timer=setTimer(()=>settle(id,failure('timeout',`Timed out: ${method} (CDP #${id}${context?' '+context:''})`,metadata)),timeout);
            pending.set(id,{metadata,timer,resolve,reject});
            const key=params.code??params.key;
            // A timed-out keyDown may already have reached the page. Own it
            // before sending, and retain it until a keyUp is acknowledged.
            if (method==='Input.dispatchKeyEvent' && params.type==='keyDown' && key) held.set(key,{...params});
            try { socket.send(JSON.stringify({id,method,params})); }
            catch (error) { settle(id,failure('send',`CDP ${method} send failed: ${error.message}`,metadata)); }
        }).then(value=>{
            if (method==='Input.dispatchKeyEvent' && params.type==='keyUp') held.delete(params.code??params.key);
            return value;
        });
    }
    async function releaseHeldKeys() {
        return Promise.all([...held.values()].reverse().map(async params=>{
            const {key,code,windowsVirtualKeyCode,nativeVirtualKeyCode}=params;
            try {
                await send('Input.dispatchKeyEvent',{type:'keyUp',key,code,windowsVirtualKeyCode,nativeVirtualKeyCode,modifiers:0},1000,'cleanup held '+key);
                return {key,code,status:'released'};
            } catch (error) { return {key,code,status:'failed',error:String(error)}; }
        }));
    }
    return {send,pending:snapshot,heldKeys:()=>[...held.values()].map(params=>({...params})),releaseHeldKeys,
        get failure(){return lastFailure;},get disconnected(){return disconnected;}};
}
