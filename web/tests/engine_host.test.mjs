import test from 'node:test';
import assert from 'node:assert/strict';

// The override permits testing the integration owner's draft in another worktree.
const { createEngineHost } = await import(process.env.DUXCEL_ENGINE_HOST_MODULE ?? '../engine_host.mjs');
const encoder = new TextEncoder(), decoder = new TextDecoder();
const tick = () => new Promise(resolve => setImmediate(resolve));
function deferred() { let resolve, reject; const promise = new Promise((yes, no) => { resolve = yes; reject = no; }); return { promise, resolve, reject }; }
function request(id = '9007199254740993') {
    return { requestId: id, generation: '18446744073709551615', documentId: '4294967295',
        sourceId: '0', objectId: '9007199254740995', revision: '9007199254740997', offset: '9007199254740999',
        knownTotal: null, rowLimit: 256, sql: 'C-built SQL' };
}
function reply(input, overrides = {}) {
    const text = encoder.encode('9007199254740993');
    const { requestId, generation, documentId, sourceId, objectId, revision, offset } = input;
    return { requestId, generation, documentId, sourceId, objectId, revision, offset, status: 'ok',
        page: { schema: 'duxcel.page', version: 1, columnCount: 1, rowCount: 1,
            columns: [{ name: 'Exact', typeName: 'BIGINT', numberKind: 'i64', precision: 0, scale: 0 }],
            text, offsets: new Uint32Array([0, text.length]), nulls: new Uint8Array([0]),
            total: '18446744073709551615', countPending: true }, ...overrides };
}
function runtime(kind = 'query', failAfter = Infinity,adopt=false) {
    let heap = new Uint8Array(32), next = 8, allocations = 0;
    const live = new Map(), releases = [], calls = [];
    const string = pointer => pointer ? decoder.decode(heap.subarray(pointer, heap.indexOf(0, pointer))) : null;
    return {
        calls, live, releases,
        allocate(size) {
            if (allocations++ >= failAfter) return 0;
            const pointer = next; next += size + 8;
            // Relocate every time: the host must fetch the current Wasm heap.
            const grown = new Uint8Array(Math.max(next, heap.length * 2)); grown.set(heap); heap = grown;
            live.set(pointer, size); return pointer;
        },
        heap: () => heap,
        release(pointer) {
            assert.ok(live.has(pointer), 'only a live owned allocation is freed');
            heap.fill(0xdd, pointer, pointer + live.get(pointer)); live.delete(pointer); releases.push(pointer);
        },
        reply(...args) {
            if (kind === 'query') {
                const [cookie, ticket, wire, size, total, pending, error] = args;
                calls.push({ args, cookie, ticket, wire: wire ? heap.slice(wire, wire + size) : null,
                    total: string(total), pending, error: string(error) });
            } else if(kind==='export'){
                const [cookie,ticket,wire,size,data,bytes,error]=args;
                calls.push({args,cookie,ticket,wire:wire?heap.slice(wire,wire+size):null,data:data?heap.slice(data,data+bytes):null,
                    ownedPointer:adopt&&data?data:0,error:string(error)});
                return adopt&&data?1:0;
            } else {
                const [session, name, label, engineEpoch, registrationId, error] = args;
                calls.push({ args, session, name: string(name), label: string(label), engineEpoch: string(engineEpoch), registrationId: string(registrationId), error: string(error) });
            }
        },
    };
}
function engine(overrides = {}) {
    const calls = []; let nextRegistrationId=1n;
    return { calls,
        registration(name,bytes){return Object.freeze({name,bytes:bytes.byteLength,engineEpoch:'9007199254740993',registrationId:String(nextRegistrationId++)});},
        async submit(input) { calls.push(['submit', input.requestId]); return reply(input); },
        async exportParquet(input){calls.push(['export',input.requestId]);return exportReply(input);},
        async cancel() { calls.push(['cancel']); },
        async registerFile(name, bytes) { calls.push(['register', name, bytes.slice()]); return this.registration(name,bytes); },
        async dropFile(name) { calls.push(['drop', name]); },
        async close() { calls.push(['close']); }, ...overrides };
}
function exportInput(id='1',change={}){return {...request(id),documentId:'7',offset:'0',rowLimit:1,knownTotal:'1',fileName:`export-7-${id}.parquet`,...change};}
function exportReply(input,overrides={}){
    const result=reply(input),text=encoder.encode('600'),data=new Uint8Array(32);data.set([80,65,82,49]);data.set([80,65,82,49],28);new DataView(data.buffer).setUint32(24,8,true);
    return {...result,page:{...result.page,columns:[{name:'Count',typeName:'BIGINT',numberKind:'i64',precision:0,scale:0}],
        text,offsets:new Uint32Array([0,text.length]),total:'1',countPending:false},data,...overrides};
}
test('export transfers exactly one binary heap allocation only after explicit synchronous adoption',async()=>{
    for(const adopt of [false,true]){
        const target=runtime('export',Infinity,adopt),adapter=engine(),host=createEngineHost({},async()=>adapter),input=exportInput();
        const expected=exportReply(input).data;await host.exportParquet(target,7,1,input);
        assert.equal(target.calls.length,1);assert.equal(target.calls[0].error,null);assert.deepEqual(target.calls[0].data,expected);
        assert.equal(new DataView(target.calls[0].wire.buffer).getUint32(0,true),0x31505844);
        assert.equal(target.live.size,adopt?1:0);assert.equal(target.releases.length,adopt?1:2);
        if(adopt){const pointer=target.calls[0].ownedPointer;assert.deepEqual(target.heap().slice(pointer,pointer+32),expected);target.release(pointer);assert.equal(target.live.size,0);}
        assert.deepEqual(adapter.calls,[['export','1']]);await host.cancel(true);
    }
});
test('export allocation failure returns a single static error fallback and retains no partial bytes',async()=>{
    for(const failAfter of [0,1]){
        const target=runtime('export',failAfter,true),host=createEngineHost({},async()=>engine());await host.exportParquet(target,7,1,exportInput());
        assert.equal(target.calls.length,1);assert.deepEqual(target.calls[0].args,[7,1,0,0,0,0,0]);assert.equal(target.live.size,0);
        assert.equal(target.releases.length,failAfter);await host.cancel(true);
    }
    const target=runtime('export',0),host=createEngineHost({},async()=>{throw new Error('initialization failed');});
    await host.exportParquet(target,7,1,exportInput());assert.equal(target.calls.length,1);assert.deepEqual(target.calls[0].args,[7,1,0,0,0,0,0]);assert.equal(target.live.size,0);
});
test('invalid export names or cookie/ticket identities fail before engine initialization',async()=>{
    let factories=0;const host=createEngineHost({},async()=>{factories++;return engine();});
    for(const [cookie,ticket,input] of [[7,1,exportInput('1',{fileName:'source.parquet'})],[8,1,exportInput()],[7,2,exportInput()],
        [7,1,exportInput('1',{purpose:'copy'})],[7,1,exportInput('1',{columns:[]})]]){
        const target=runtime('export');await host.exportParquet(target,cookie,ticket,input);assert.equal(target.calls.length,1);assert.ok(target.calls[0].error);assert.equal(target.live.size,0);
    }
    assert.equal(factories,0);await host.cancel(true);
});
test('invalid export input cannot supersede an already active ordinary query',async()=>{
    const pending=deferred(),adapter=engine({submit:()=>pending.promise}),host=createEngineHost({},async()=>adapter),queryTarget=runtime(),badTarget=runtime('export');
    const query=host.submit(queryTarget,7,1,request('1'));await tick();
    await host.exportParquet(badTarget,7,2,exportInput('2',{fileName:'wrong.parquet'}));
    pending.resolve(reply(request('1')));await query;assert.equal(queryTarget.calls.length,1);assert.equal(badTarget.calls.length,1);assert.ok(badTarget.calls[0].error);await host.cancel(true);
});
test('mismatched export replies and malformed output reject before binary ownership transfer',async()=>{
    for(const field of ['requestId','generation','documentId','sourceId','objectId','revision','offset']){
        const target=runtime('export',Infinity,true),input=exportInput(),adapter=engine({async exportParquet(){return exportReply(input,{[field]:input[field]==='0'?'1':'0'});}});
        const host=createEngineHost({},async()=>adapter);await host.exportParquet(target,7,1,input);
        assert.equal(target.calls.length,1);assert.match(target.calls[0].error,/mismatched.*identity/);assert.equal(target.live.size,0);await host.cancel(true);
    }
    for(const result of [exportReply(exportInput(),{status:'error',error:{message:'private cleanup failed'}}),exportReply(exportInput(),{data:new Uint8Array(32)}),
        exportReply(exportInput(),{data:new Uint8Array(new SharedArrayBuffer(32))})]){
        const target=runtime('export',Infinity,true),host=createEngineHost({},async()=>engine({async exportParquet(){return result;}}));
        await host.exportParquet(target,7,1,exportInput());assert.equal(target.calls.length,1);assert.ok(target.calls[0].error);assert.equal(target.calls[0].data,null);assert.equal(target.live.size,0);await host.cancel(true);
    }
});
test('superseded, cancelled and closed exports cannot allocate or deliver late binary results',async()=>{
    for(const action of ['new-query','cancel','close']){
        const pending=deferred(),adapter=engine({exportParquet:()=>pending.promise}),host=createEngineHost({},async()=>adapter),target=runtime('export',Infinity,true);
        const waiting=host.exportParquet(target,7,1,exportInput());await tick();
        if(action==='new-query')await host.submit(runtime(),7,2,request('2'));else await host.cancel(action==='close');
        pending.resolve(exportReply(exportInput()));await waiting;assert.equal(target.calls.length,0);assert.equal(target.live.size,0);assert.equal(target.releases.length,0);
        if(action!=='close')await host.cancel(true);
    }
    const initialization=deferred(),host=createEngineHost({},()=>initialization.promise),target=runtime('export');
    const waiting=host.exportParquet(target,7,1,exportInput()),closed=host.cancel(true);initialization.resolve(engine());await Promise.all([waiting,closed]);assert.equal(target.calls.length,0);
});
test('host chooses copy wire bounds only from its explicit request purpose',async()=>{
    const adapter=engine({async submit(input){
        const output=reply(input),rows=600;
        output.page={...output.page,rowCount:rows,text:new Uint8Array(),offsets:new Uint32Array(rows+1),nulls:new Uint8Array(rows).fill(1)};
        return output;
    }});
    const host=createEngineHost({},async()=>adapter),copied=runtime(),ordinary=runtime();
    await host.submit(copied,1,1,{...request('1'),purpose:'copy',rowLimit:600});
    assert.equal(copied.calls.length,1);assert.equal(copied.calls[0].error,null);
    assert.equal(new DataView(copied.calls[0].wire.buffer).getUint32(12,true),600);assert.equal(copied.live.size,0);
    await host.submit(ordinary,1,2,request('2'));
    assert.match(ordinary.calls[0].error,/row count/);assert.equal(ordinary.live.size,0);
    await host.cancel(true);
});
async function withDOM(run) {
    const previous = globalThis.document, inputs = [];
    globalThis.document = {
        body: { append(input) { inputs.push(input); } },
        createElement(tag) {
            assert.equal(tag, 'input'); const listeners = new Map();
            return { removed: false, clicked: false,
                addEventListener(type, handler, options) { listeners.set(type, { handler, once: options?.once }); },
                dispatch(type) { const event = listeners.get(type); if (event?.once) listeners.delete(type); return event?.handler(); },
                remove() { this.removed = true; }, click() { this.clicked = true; } };
        },
    };
    try { return await run(inputs); }
    finally { if (previous === undefined) delete globalThis.document; else globalThis.document = previous; }
}
function choose(host, target, inputs, options = {}) {
    host.chooseFile(target, 77);
    const input = inputs.at(-1);
    const bytes = options.bytes ?? Uint8Array.from([1, 2, 3]).buffer;
    input.files = [{ name: options.name ?? 'transactions.csv', size: options.size ?? bytes.byteLength,
        arrayBuffer: options.arrayBuffer ?? (async () => bytes) }];
    return { input, completion: input.dispatch('change') };
}

test('query delivery keeps exact identity/total and frees transport only after the synchronous C callback', async () => {
    const target = runtime(), adapter = engine(), config = { sentinel: true };
    let factories = 0;
    const host = createEngineHost(config, async received => { factories++; assert.equal(received, config); return adapter; });
    await host.submit(target, 0xffffffff, 0xfffffffe, request());
    assert.equal(factories, 1); assert.equal(target.calls.length, 1);
    const received = target.calls[0];
    assert.equal(received.cookie, 0xffffffff); assert.equal(received.ticket, 0xfffffffe);
    assert.equal(received.total, '18446744073709551615'); assert.equal(received.pending, 1); assert.equal(received.error, null);
    assert.equal(new DataView(received.wire.buffer).getUint32(0, true), 0x31505844);
    assert.equal(decoder.decode(received.wire.subarray(-16)), '9007199254740993');
    assert.equal(target.live.size, 0); assert.equal(target.releases.length, 2);
    assert.deepEqual(adapter.calls, [['submit', '9007199254740993']]);
});

test('superseded/cancelled replies and pending initialization cannot publish stale C results', async () => {
    const old = deferred(), adapter = engine({ submit: input => input.requestId === '1' ? old.promise : Promise.resolve(reply(input)) });
    const host = createEngineHost({}, async () => adapter), first = runtime(), second = runtime();
    const before = host.submit(first, 1, 1, request('1')); await tick();
    await host.submit(second, 1, 2, request('2')); old.resolve(reply(request('1'))); await before;
    assert.equal(first.calls.length, 0); assert.equal(second.calls.length, 1);
    const cancelled = deferred(), cancelledTarget = runtime();
    adapter.submit = () => cancelled.promise;
    const pending = host.submit(cancelledTarget, 1, 3, request('3')); await tick();
    await host.cancel(); cancelled.resolve(reply(request('3'))); await pending;
    assert.equal(cancelledTarget.calls.length, 0);
    const initialization = deferred(), initializationTarget = runtime();
    const waitingHost = createEngineHost({}, () => initialization.promise);
    const waiting = waitingHost.submit(initializationTarget, 2, 1, request('1'));
    const shutdown = waitingHost.cancel(true); initialization.resolve(adapter); await Promise.all([waiting, shutdown]);
    assert.equal(initializationTarget.calls.length, 0); assert.deepEqual(adapter.calls.at(-1), ['close']);
});

test('every mismatched exact identity and engine/query/transport error yields one owned error reply', async () => {
    for (const field of ['requestId', 'generation', 'documentId', 'sourceId', 'objectId', 'revision', 'offset']) {
        const target = runtime(), input = request();
        const host = createEngineHost({}, async () => engine({ async submit() { return reply(input, { [field]: input[field] === '0' ? '1' : '0' }); } }));
        await host.submit(target, 1, 1, input);
        assert.equal(target.calls.length, 1); assert.match(target.calls[0].error, /mismatched.*identity/);
        assert.equal(target.live.size, 0);
    }
    for (const createAdapter of [async () => { throw new Error('initialization failed'); },
        async () => engine({ async submit() { throw new Error('engine rejected'); } }),
        async () => engine({ async submit(input) { return reply(input, { status: 'error', error: { message: 'SQL binder failed' } }); } }),
        async () => engine({ async submit(input) { const result = reply(input); result.page.text[0] = 0; return result; } })]) {
        const target = runtime(); await createEngineHost({}, createAdapter).submit(target, 1, 1, request());
        assert.equal(target.calls.length, 1); assert.ok(target.calls[0].error); assert.equal(target.live.size, 0);
    }
});

test('a superseded or closed file read never registers stale bytes or calls the old session', async () => withDOM(async inputs => {
    const adapter = engine(), host = createEngineHost({}, async () => adapter), first = runtime('file'), latest = runtime('file');
    const reading = deferred(), old = choose(host, first, inputs, { arrayBuffer: () => reading.promise });
    const current = choose(host, latest, inputs, { name: 'latest.parquet' }); await current.completion;
    reading.resolve(new Uint8Array([9]).buffer); await old.completion;
    assert.equal(first.calls.length, 0); assert.equal(latest.calls.length, 1);
    assert.equal(latest.calls[0].label, 'latest.parquet'); assert.match(latest.calls[0].name, /^import-\d+-latest\.parquet$/);
    assert.equal(adapter.calls.filter(call => call[0] === 'register').length, 1);
    assert.ok(old.input.removed && current.input.removed);
    const late = deferred(), closedTarget = runtime('file');
    const closed = choose(host, closedTarget, inputs, { arrayBuffer: () => late.promise });
    await host.cancel(true); late.resolve(new Uint8Array([8]).buffer); await closed.completion;
    assert.equal(closedTarget.calls.length, 0); assert.equal(adapter.calls.filter(call => call[0] === 'register').length, 1);
}));

test('source import cancels the active C query exactly once before registering', async () => withDOM(async inputs => {
    const pending = deferred(), queryTarget = runtime(), fileTarget = runtime('file');
    const adapter = engine({ submit: () => pending.promise });
    const host = createEngineHost({}, async () => adapter), query = host.submit(queryTarget, 5, 9, request()); await tick();
    const selected = choose(host, fileTarget, inputs); await selected.completion;
    assert.equal(queryTarget.calls.length, 1); assert.match(queryTarget.calls[0].error, /cancelled for source import/);
    pending.resolve(reply(request(), { status: 'cancelled' })); await query;
    assert.equal(queryTarget.calls.length, 1); assert.equal(fileTarget.calls.length, 1);
    assert.deepEqual(adapter.calls.map(call => call[0]), ['cancel', 'register']);
    assert.equal(queryTarget.live.size, 0); assert.equal(fileTarget.live.size, 0);
}));
test('source import cancels export with its own callback and waits for actual adapter cleanup',async()=>withDOM(async inputs=>{
    const pending=deferred(),draining=deferred(),began=deferred();
    const adapter=engine({exportParquet:()=>pending.promise,async cancel(){this.calls.push(['cancel']);began.resolve();await draining.promise;pending.resolve(exportReply(exportInput(),{status:'cancelled'}));}});
    const host=createEngineHost({},async()=>adapter),target=runtime('export',Infinity,true),fileTarget=runtime('file');
    const exporting=host.exportParquet(target,7,1,exportInput());await tick();const selected=choose(host,fileTarget,inputs);await began.promise;
    assert.equal(target.calls.length,1);assert.equal(target.calls[0].args.length,7);assert.match(target.calls[0].error,/Export cancelled for source import/);
    assert.equal(target.calls[0].data,null);assert.equal(target.live.size,0);assert.equal(fileTarget.calls.length,0);assert.equal(adapter.calls.some(value=>value[0]==='register'),false);
    draining.resolve();await Promise.all([exporting,selected.completion]);assert.equal(target.calls.length,1);assert.equal(fileTarget.calls.length,1);
    assert.deepEqual(adapter.calls.map(value=>value[0]),['cancel','register']);await host.cancel(true);
}));
test('deferred source release waits through export acceptance and a synchronous following query',async()=>withDOM(async inputs=>{
    const exporting=deferred(),querying=deferred(),adapter=engine({exportParquet:()=>exporting.promise,submit:()=>querying.promise});
    const host=createEngineHost({},async()=>adapter),fileTarget=runtime('file'),target=runtime('export',Infinity,true),next=runtime();
    await choose(host,fileTarget,inputs).completion;const sourceName=fileTarget.calls[0].name;
    let nextQuery;const deliver=target.reply;target.reply=(...args)=>{const adopted=deliver(...args);nextQuery=host.submit(next,7,2,request('2'));return adopted;};
    const pending=host.exportParquet(target,7,1,exportInput());await tick();assert.equal(host.releaseFile(sourceName),true);await tick();
    assert.equal(adapter.calls.some(value=>value[0]==='drop'),false);exporting.resolve(exportReply(exportInput()));await pending;await tick();
    assert.equal(target.calls.length,1);assert.equal(adapter.calls.some(value=>value[0]==='drop'),false);assert.equal(next.calls.length,0);
    querying.resolve(reply(request('2')));await nextQuery;await tick();assert.equal(next.calls.length,1);
    assert.deepEqual(adapter.calls.filter(value=>value[0]==='drop'),[['drop',sourceName]]);target.release(target.calls[0].ownedPointer);assert.equal(target.live.size,0);await host.cancel(true);
}));

test('superseded import checks identity after engine initialization and asynchronous cancel', async () => withDOM(async inputs => {
    const initialization = deferred(), cancelling = deferred(), cancelBegan = deferred();
    const adapter = engine({ async cancel() { cancelBegan.resolve(); await cancelling.promise; } });
    const host = createEngineHost({}, () => initialization.promise), oldTarget = runtime('file'), newerTarget = runtime('file');
    const old = choose(host, oldTarget, inputs, { name: 'old.csv' }); await tick();
    const newer = choose(host, newerTarget, inputs, { name: 'newer.csv' }); initialization.resolve(adapter);
    await cancelBegan.promise;
    // The newest picker is cancelled while the previous import awaits cancel.
    const cancelledTarget = runtime('file'); host.chooseFile(cancelledTarget, 88);
    await inputs.at(-1).dispatch('cancel'); cancelling.resolve();
    await Promise.all([old.completion, newer.completion]);
    assert.equal(oldTarget.calls.length, 0); assert.equal(newerTarget.calls.length, 0);
    assert.equal(cancelledTarget.calls.length, 1); assert.equal(cancelledTarget.calls[0].name, null);
    assert.equal(adapter.calls.filter(call => call[0] === 'register').length, 0);
    assert.ok(old.input.removed && newer.input.removed);
}));

test('query waits behind file registration and the import callback may safely submit a new query', async () => withDOM(async inputs => {
    const registering = deferred(), began = deferred(), order = [];
    const adapter = engine({
        async registerFile(name,bytes) { order.push('register:start'); began.resolve(); await registering.promise; order.push('register:end'); return this.registration(name,bytes); },
        async submit(input) { order.push('query'); return reply(input); },
    });
    const host = createEngineHost({}, async () => adapter), fileTarget = runtime('file'), queryTarget = runtime();
    let reentrant;
    const fileReply = fileTarget.reply;
    fileTarget.reply = (...args) => { fileReply(...args); reentrant = host.submit(queryTarget, 1, 2, request('2')); };
    const selected = choose(host, fileTarget, inputs); await began.promise;
    const superseded = runtime(), waiting = host.submit(superseded, 1, 1, request('1')); await tick();
    assert.deepEqual(order, ['register:start']);
    registering.resolve(); await selected.completion; await Promise.all([waiting, reentrant]);
    assert.deepEqual(order, ['register:start', 'register:end', 'query']);
    assert.equal(superseded.calls.length, 0); assert.equal(queryTarget.calls.length, 1);
}));

test('close during registration waits for stale-file cleanup, then closes unconditionally', async () => withDOM(async inputs => {
    const registering = deferred(), began = deferred(), order = [], fileTarget = runtime('file');
    const adapter = engine({ async registerFile(name,bytes) { order.push('register:start'); began.resolve(); await registering.promise; order.push('register:end'); return this.registration(name,bytes); },
        async dropFile() { order.push('drop'); }, async close() { order.push('close'); } });
    const host = createEngineHost({}, async () => adapter), selected = choose(host, fileTarget, inputs);
    await began.promise; const closing = host.cancel(true);
    // A later rejected submit must not revoke the earlier close operation.
    await host.submit(runtime(), 1, 99, request('99')); await tick();
    assert.deepEqual(order, ['register:start']);
    registering.resolve(); await Promise.all([selected.completion, closing]);
    assert.deepEqual(order, ['register:start', 'register:end', 'drop', 'close']);
    assert.equal(fileTarget.calls.length, 0); assert.ok(selected.input.removed);
    const count = inputs.length; host.chooseFile(fileTarget, 77); assert.equal(inputs.length, count);
}));

test('stale registration drops its file before a newer import uses the engine', async () => withDOM(async inputs => {
    const registering = deferred(), began = deferred(), order = [];
    const adapter = engine({ async registerFile(name,bytes) {
        order.push(['register:start', name]);
        if (name.endsWith('first.csv')) { began.resolve(); await registering.promise; }
        order.push(['register:end', name]); return this.registration(name,bytes);
    }, async dropFile(name) { order.push(['drop', name]); } });
    const host = createEngineHost({}, async () => adapter), first = runtime('file'), second = runtime('file');
    const before = choose(host, first, inputs, { name: 'first.csv' }); await began.promise;
    const after = choose(host, second, inputs, { name: 'second.csv' }); await tick();
    assert.equal(order.length, 1); registering.resolve(); await Promise.all([before.completion, after.completion]);
    assert.deepEqual(order.map(call => call[0]), ['register:start', 'register:end', 'drop', 'register:start', 'register:end']);
    assert.equal(first.calls.length, 0); assert.equal(second.calls.length, 1); assert.equal(second.calls[0].label, 'second.csv');
}));

test('import failure/cancellation cleans inputs and the admin barrier remains usable', async () => withDOM(async inputs => {
    let failures = 0;
    const adapter = engine({ async registerFile(name,bytes) { if (failures++ === 0) throw new Error('registration failed'); return this.registration(name,bytes); } });
    const host = createEngineHost({}, async () => adapter), failed = runtime('file');
    const before = choose(host, failed, inputs); await before.completion;
    assert.equal(failed.calls.length, 1); assert.equal(failed.calls[0].error, 'registration failed');
    assert.ok(before.input.removed); assert.equal(failed.live.size, 0);
    const succeeded = runtime('file'), after = choose(host, succeeded, inputs); await after.completion;
    assert.equal(succeeded.calls.length, 1); assert.equal(succeeded.calls[0].error, null);
    const cancelled = runtime('file'); host.chooseFile(cancelled, 8); const input = inputs.at(-1);
    assert.equal(input.accept, '.parquet,.csv,.tsv,.csv.gz'); await input.dispatch('cancel');
    assert.equal(cancelled.calls.length, 1); assert.equal(cancelled.calls[0].name, null); assert.ok(input.removed);
}));

test('file validation agrees with the C source extensions before allocating/registering bytes', async () => withDOM(async inputs => {
    const adapter = engine(), host = createEngineHost({}, async () => adapter);
    for (const options of [{ name: 'data.tsv.gz' }, { name: 'data.parquet.gz' }, { name: 'data.dxls' },
        { size: 256 * 1024 * 1024 + 1 }, { name: 'x'.repeat(201) + '.csv' }]) {
        const target = runtime('file'), selected = choose(host, target, inputs, { ...options,
            arrayBuffer() { throw new Error('invalid files must be rejected before reading'); } });
        await selected.completion;
        assert.equal(target.calls.length, 1); assert.ok(target.calls[0].error); assert.ok(selected.input.removed);
    }
    assert.equal(adapter.calls.length, 0);
    const accepted = runtime('file'), selected = choose(host, accepted, inputs, { name: 'City É.CSV.GZ' }); await selected.completion;
    assert.equal(accepted.calls[0].label, 'City É.CSV.GZ'); assert.equal(accepted.calls[0].error, null);
}));

test('original filenames reject NUL and unpaired surrogates before reads, registration or name allocation', async () => withDOM(async inputs => {
    const adapter=engine();let factories=0,reads=0;
    const host=createEngineHost({},async()=>{factories++;return adapter;});
    const invalid=['\0data.csv','data\0.csv','data.csv\0','\ud800.csv','pre\udbffpost.csv','data.csv\ud800',
        '\udc00.csv','pre\udfffpost.csv','data.csv\udfff','\ud800\ud801.csv','pre\ud800a.csv','🦆\ud800.csv'];
    for(const name of invalid){
        const target=runtime('file'),allocations=[],allocate=target.allocate;
        target.allocate=size=>{allocations.push(size);return allocate(size);};
        const selected=choose(host,target,inputs,{name,arrayBuffer(){reads++;return Promise.resolve(new ArrayBuffer(0));}});
        await selected.completion;
        assert.equal(reads,0,'invalid original text is rejected before reading source bytes');
        assert.equal(factories,0,'invalid original text cannot initialize the engine');
        assert.equal(target.calls.length,1);const result=target.calls[0];
        assert.equal(result.name,null);assert.equal(result.label,null);assert.match(result.error,/filename.*valid Unicode.*no NUL/);
        assert.deepEqual(result.args.slice(0,5),[77,0,0,0,0]);assert.ok(result.args[5]>0);
        assert.deepEqual(allocations,[encoder.encode(result.error).length+1],'only the diagnostic is allocated');
        assert.ok(allocations[0]<=256);assert.equal(target.releases.length,1);assert.equal(target.live.size,0);assert.ok(selected.input.removed);
    }
    assert.deepEqual(adapter.calls,[]);
    const oom=runtime('file',0);await choose(host,oom,inputs,{name:'bad\0.csv'}).completion;
    assert.deepEqual(oom.calls[0].args,[77,0,0,0,0,0]);assert.equal(oom.live.size,0);assert.equal(oom.releases.length,0);assert.equal(factories,0);
    const accepted=runtime('file');await choose(host,accepted,inputs,{name:'after.csv'}).completion;
    assert.equal(accepted.calls[0].name,'import-1-after.csv','rejections never reserve or burn a registration name');
    assert.equal(accepted.calls[0].label,'after.csv');assert.equal(accepted.calls[0].error,null);await host.cancel(true);
}));

test('original filenames preserve Unicode scalars, whitespace, apostrophes and import-like prefixes', async () => withDOM(async inputs => {
    const adapter=engine(),host=createEngineHost({},async()=>adapter);
    const names=["import-7-O'Brien-İstanbul-表-🦆.csv",'  two\twords\n.csv','edge-\ud7ff\ue000-\ud800\udc00-\udbff\udfff.parquet',
        'literal/path\\name.csv','🦆'.repeat(49)+'.csv'];
    for(const [i,name] of names.entries()){
        const target=runtime('file'),selected=choose(host,target,inputs,{name});await selected.completion;
        assert.equal(target.calls.length,1);assert.equal(target.calls[0].error,null);assert.equal(target.calls[0].label,name);
        const key=`import-${i+1}-${name.replace(/[\\/\x00-\x1f\x7f]/g,'_')}`;
        assert.equal(target.calls[0].name,key);assert.notEqual(key,name);
        assert.deepEqual(adapter.calls.filter(call=>call[0]==='register').at(-1),['register',key,Uint8Array.from([1,2,3])]);
        assert.equal(target.calls[0].engineEpoch,'9007199254740993');assert.equal(target.calls[0].registrationId,String(i+1));
        assert.equal(target.releases.length,4);assert.equal(target.live.size,0);assert.ok(selected.input.removed);
    }
    assert.equal(encoder.encode(names.at(-1)).length,200);
    const excess=runtime('file');await choose(host,excess,inputs,{name:'🦆'.repeat(49)+'x.csv',arrayBuffer(){assert.fail('existing byte cap precedes file reads');}}).completion;
    assert.match(excess.calls[0].error,/200 UTF-8 bytes/);assert.equal(adapter.calls.filter(call=>call[0]==='register').length,names.length);
    await host.cancel(true);
}));

test('original filename rejection and chooser cancellation leave an existing query active', async () => withDOM(async inputs => {
    const pending=deferred(),input=request('61'),adapter=engine({submit(value){this.calls.push(['submit',value.requestId]);return pending.promise;}});
    const host=createEngineHost({},async()=>adapter),queryTarget=runtime(),query=host.submit(queryTarget,7,61,input);await tick();
    const invalid=runtime('file');await choose(host,invalid,inputs,{name:'bad\udfff.csv',arrayBuffer(){assert.fail('invalid filename must not read');}}).completion;
    assert.match(invalid.calls[0].error,/filename.*valid Unicode.*no NUL/);assert.deepEqual(adapter.calls,[['submit','61']]);
    assert.equal(queryTarget.calls.length,0);assert.equal(invalid.live.size,0);
    const cancelled=runtime('file');host.chooseFile(cancelled,88);const chooser=inputs.at(-1);await chooser.dispatch('cancel');
    assert.deepEqual(cancelled.calls[0].args,[88,0,0,0,0,0]);assert.equal(cancelled.releases.length,0);assert.ok(chooser.removed);
    pending.resolve(reply(input));await query;
    assert.equal(queryTarget.calls.length,1);assert.equal(queryTarget.calls[0].error,null);assert.equal(queryTarget.live.size,0);
    assert.deepEqual(adapter.calls,[['submit','61']]);await host.cancel(true);
}));

test('OOM falls back without transport malloc and frees partially allocated query/file replies', async () => withDOM(async inputs => {
    for (const failAfter of [0, 1]) {
        const target = runtime('query', failAfter);
        await createEngineHost({}, async () => engine()).submit(target, 5, 6, request());
        assert.equal(target.calls.length, 1); assert.deepEqual(target.calls[0].args, [5, 6, 0, 0, 0, 0, 0]);
        assert.equal(target.live.size, 0); assert.equal(target.releases.length, failAfter);
    }
    const target = runtime('query', 0);
    await createEngineHost({}, async () => { throw new Error('initialization failed'); }).submit(target, 7, 8, request());
    assert.deepEqual(target.calls[0].args, [7, 8, 0, 0, 0, 0, 0]);
    for (const failAfter of [0, 1, 2, 3]) {
        const fileTarget = runtime('file', failAfter), host = createEngineHost({}, async () => engine());
        const selected = choose(host, fileTarget, inputs); await selected.completion;
        assert.equal(fileTarget.calls.length, 1); assert.deepEqual(fileTarget.calls[0].args, [77, 0, 0, 0, 0, 0]);
        assert.equal(fileTarget.live.size, 0); assert.equal(fileTarget.releases.length, failAfter); assert.ok(selected.input.removed);
    }
}));

test('releaseFile validates host-owned names and deduplicates drops without cancelling an active query', async () => withDOM(async inputs => {
    const pending = deferred(), adapter = engine({ submit(input) { this.calls.push(['submit', input.requestId]); return pending.promise; } });
    const host = createEngineHost({}, async () => adapter), fileTarget = runtime('file');
    for (const name of [null, 1, {}, '', 'source.csv', 'import-0-file.csv', 'import-1-../file.csv', 'import-1-file\0.csv', 'import-99-unknown.csv'])
        assert.equal(host.releaseFile(name), false);
    assert.equal(adapter.calls.length, 0);
    await choose(host, fileTarget, inputs).completion; const name = fileTarget.calls[0].name;
    const target = runtime(), querying = host.submit(target, 5, 9, request('9')); await tick();
    assert.equal(host.releaseFile(name), true); assert.equal(host.releaseFile(name), true); await tick();
    assert.equal(adapter.calls.filter(call => call[0] === 'drop').length, 0);
    assert.equal(adapter.calls.filter(call => call[0] === 'cancel').length, 1, 'release never cancels the unrelated query');
    pending.resolve(reply(request('9'))); await querying; await tick();
    assert.equal(target.calls.length, 1); assert.equal(target.calls[0].error, null);
    assert.deepEqual(adapter.calls.map(call => call[0]), ['cancel', 'register', 'submit', 'drop']);
    assert.equal(host.releaseFile(name), true); await tick();
    assert.equal(adapter.calls.filter(call => call[0] === 'drop').length, 1, 'completed cleanup remains idempotent');
    const next = runtime('file'); await choose(host, next, inputs).completion;
    assert.notEqual(next.calls[0].name, name, 'retired registration names are never reused');
}));

test('a reentrant release and query behind import administration do not wait on each other', { timeout: 2000 }, async () => {
    for (const earlierWaiting of [false, true]) await withDOM(async inputs => {
        const registering = deferred(), began = deferred(), order = [];
        const adapter = engine({ async registerFile(name,bytes) { order.push('register:start'); began.resolve(); await registering.promise; order.push('register:end'); return this.registration(name,bytes); },
            async submit(input) { order.push('query'); return reply(input); }, async dropFile() { order.push('drop'); } });
        const host = createEngineHost({}, async () => adapter), fileTarget = runtime('file'), target = runtime(), stale = runtime();
        const fileReply = fileTarget.reply; let reentrant, waiting;
        fileTarget.reply = (...args) => {
            fileReply(...args); assert.equal(host.releaseFile(fileTarget.calls[0].name), true);
            reentrant = host.submit(target, 1, 2, request('2'));
        };
        const selected = choose(host, fileTarget, inputs); await began.promise;
        if (earlierWaiting) waiting = host.submit(stale, 1, 1, request('1'));
        registering.resolve(); await selected.completion; await Promise.all([waiting, reentrant]); await tick();
        assert.deepEqual(order, ['register:start', 'register:end', 'query', 'drop']);
        assert.equal(stale.calls.length, 0); assert.equal(target.calls.length, 1);
    });
});

test('a query arriving during a drop runs before remaining cleanup and never overlaps administration', { timeout: 2000 }, async () => withDOM(async inputs => {
    const dropping = deferred(), began = deferred(), order = [];
    const adapter = engine({ async dropFile(name) {
        order.push(['drop:start', name]);
        if (!order.some(call => call[0] === 'drop:end')) { began.resolve(); await dropping.promise; }
        order.push(['drop:end', name]);
    }, async submit(input) { order.push(['query', input.requestId]); return reply(input); } });
    const host = createEngineHost({}, async () => adapter), first = runtime('file'), second = runtime('file');
    await choose(host, first, inputs).completion; await choose(host, second, inputs).completion;
    assert.equal(host.releaseFile(first.calls[0].name), true); await began.promise;
    const target = runtime(), query = host.submit(target, 1, 3, request('3'));
    assert.equal(host.releaseFile(second.calls[0].name), true); await tick();
    assert.equal(order.length, 1); dropping.resolve(); await query; await tick();
    assert.deepEqual(order.map(call => call[0]), ['drop:start', 'drop:end', 'query', 'drop:start', 'drop:end']);
    assert.equal(target.calls.length, 1);
}));

test('same-turn cancel reserves administration before a release can drop a still-draining file', { timeout: 2000 }, async () => withDOM(async inputs => {
    const pending = deferred(), cancelling = deferred(), began = deferred(); let running = false;
    const adapter = engine({ submit(input) { this.calls.push(['submit', input.requestId]); running = true; return pending.promise; },
        async cancel() { this.calls.push(['cancel']); if (running) { began.resolve(); await cancelling.promise; running = false; pending.resolve(reply(request('5'), { status: 'cancelled' })); } },
        async dropFile(name) { assert.equal(running, false, 'adapter work has actually drained'); this.calls.push(['drop', name]); } });
    const host = createEngineHost({}, async () => adapter), fileTarget = runtime('file'), target = runtime();
    await choose(host, fileTarget, inputs).completion;
    const query = host.submit(target, 1, 5, request('5')); await tick();
    const cancelled = host.cancel(); assert.equal(host.releaseFile(fileTarget.calls[0].name), true);
    await began.promise; await tick(); assert.equal(adapter.calls.filter(call => call[0] === 'drop').length, 0);
    cancelling.resolve(); await Promise.all([cancelled, query]); await tick();
    assert.equal(target.calls.length, 0); assert.equal(adapter.calls.filter(call => call[0] === 'cancel').length, 2);
    assert.equal(adapter.calls.filter(call => call[0] === 'drop').length, 1);
}));

test('a newer submission still revokes an older cancel while release waits for the latest query', async () => withDOM(async inputs => {
    const pending = deferred(), adapter = engine({ submit(input) {
        this.calls.push(['submit', input.requestId]);
        if (input.requestId === '10') return pending.promise;
        pending.resolve(reply(request('10'), { status: 'superseded' })); return Promise.resolve(reply(input));
    } });
    const host = createEngineHost({}, async () => adapter), fileTarget = runtime('file'), first = runtime(), latest = runtime();
    await choose(host, fileTarget, inputs).completion;
    const old = host.submit(first, 1, 10, request('10')); await tick();
    const cancelled = host.cancel(), current = host.submit(latest, 1, 11, request('11'));
    assert.equal(host.releaseFile(fileTarget.calls[0].name), true);
    await Promise.all([old, cancelled, current]); await tick();
    assert.equal(first.calls.length, 0); assert.equal(latest.calls.length, 1); assert.equal(latest.calls[0].error, null);
    assert.equal(adapter.calls.filter(call => call[0] === 'cancel').length, 1, 'only the original import cancellation ran');
    assert.deepEqual(adapter.calls.slice(-3).map(call => call[0]), ['submit', 'submit', 'drop']);
}));

test('close owns queued registrations until engine cleanup finishes and suppresses later drops', { timeout: 2000 }, async () => withDOM(async inputs => {
    const pending = deferred(), closing = deferred(), began = deferred(), owned = new Set();
    const adapter = engine({ async registerFile(name,bytes) { owned.add(name); return this.registration(name,bytes); }, submit() { return pending.promise; },
        async dropFile() { assert.fail('queued files belong to close, not an overlapping drop'); },
        async close() { began.resolve(); await closing.promise; owned.clear(); pending.resolve(reply(request('6'), { status: 'cancelled' })); } });
    const host = createEngineHost({}, async () => adapter), first = runtime('file'), second = runtime('file'), target = runtime();
    await choose(host, first, inputs).completion; await choose(host, second, inputs).completion;
    const query = host.submit(target, 1, 6, request('6')); await tick();
    assert.equal(host.releaseFile(first.calls[0].name), true); const closed = host.cancel(true); await began.promise;
    assert.equal(owned.size, 2); assert.equal(host.releaseFile(second.calls[0].name), false);
    const count = inputs.length; host.chooseFile(runtime('file'), 3); assert.equal(inputs.length, count);
    closing.resolve(); await Promise.all([closed, query]); await tick();
    assert.equal(owned.size, 0); assert.equal(target.calls.length, 0); assert.equal(host.releaseFile(first.calls[0].name), false);
}));

test('cleanup failure is explicit once, does not retry-spin, and leaves later queries usable', async () => withDOM(async inputs => {
    const previous = console.error, errors = []; console.error = (...args) => errors.push(args);
    try {
        let drops = 0; const adapter = engine({ async dropFile() { drops++; throw new Error('drop failed'); } });
        const host = createEngineHost({}, async () => adapter), fileTarget = runtime('file');
        await choose(host, fileTarget, inputs).completion; const name = fileTarget.calls[0].name;
        assert.equal(host.releaseFile(name), true); assert.equal(host.releaseFile(name), true); await tick();
        assert.equal(drops, 1); assert.equal(errors.length, 1); assert.match(errors[0][0], new RegExp(name.replaceAll('.', '\\.'))); assert.equal(errors[0][1].message, 'drop failed');
        assert.equal(host.releaseFile(name), false);
        const target = runtime(); await host.submit(target, 1, 7, request('7')); await tick();
        assert.equal(target.calls.length, 1); assert.equal(target.calls[0].error, null); assert.equal(drops, 1);
        adapter.close = async () => { throw new Error('close failed'); };
        await host.cancel(true); await tick(); assert.equal(errors.length, 2); assert.equal(errors[1][1].message, 'close failed');
        assert.equal(host.releaseFile(name), false); assert.equal(drops, 1);
    } finally { console.error = previous; }
}));

test('obsolete registration cleanup shares the same terminal status and reports failures', async () => withDOM(async inputs => {
    const previous = console.error, errors = []; console.error = (...args) => errors.push(args);
    try {
        const registering = deferred(), began = deferred(), names = []; let drops = 0;
        const adapter = engine({ async registerFile(name,bytes) { names.push(name); if (names.length === 1) { began.resolve(); await registering.promise; } return this.registration(name,bytes); },
            async dropFile() { drops++; throw new Error('obsolete drop failed'); } });
        const host = createEngineHost({}, async () => adapter), first = runtime('file'), second = runtime('file');
        const before = choose(host, first, inputs); await began.promise;
        const after = choose(host, second, inputs); registering.resolve(); await Promise.all([before.completion, after.completion]);
        assert.equal(first.calls.length, 0); assert.equal(second.calls.length, 1); assert.equal(drops, 1); assert.equal(errors.length, 1);
        assert.equal(host.releaseFile(names[0]), false, 'failed stale cleanup is not retried through releaseFile');
        await tick(); assert.equal(drops, 1);
    } finally { console.error = previous; }
}));

test('1024 issued names and pending drops stay bounded, and retired names never reopen capacity', { timeout: 3000 }, async () => withDOM(async inputs => {
    const pending = deferred(), names = [], adapter = engine({ submit() { return pending.promise; } });
    const host = createEngineHost({}, async () => adapter);
    for (let i = 0; i < 1024; i++) {
        const target = runtime('file'); await choose(host, target, inputs).completion;
        assert.equal(target.calls[0].error, null); names.push(target.calls[0].name);
    }
    assert.equal(new Set(names).size, 1024);
    const target = runtime(), query = host.submit(target, 1, 8, request('8')); await tick();
    for (const name of names) { assert.equal(host.releaseFile(name), true); assert.equal(host.releaseFile(name), true); }
    await tick(); assert.equal(adapter.calls.filter(call => call[0] === 'drop').length, 0);
    for (const afterCleanup of [false, true]) {
        if (afterCleanup) { pending.resolve(reply(request('8'))); await query; await tick(); }
        const excess = runtime('file'), selected = choose(host, excess, inputs, { arrayBuffer() { assert.fail('name capacity is checked before reading bytes'); } });
        await selected.completion; assert.match(excess.calls[0].error, /1024 source-name limit/);
        assert.equal(adapter.calls.filter(call => call[0] === 'register').length, 1024);
    }
    const drops = adapter.calls.filter(call => call[0] === 'drop').map(call => call[1]);
    assert.equal(drops.length, 1024); assert.equal(new Set(drops).size, 1024); assert.equal(target.calls.length, 1);
}));

test('every file reply allocation OOM releases registered bytes without exposing a name or token to C', async () => withDOM(async inputs => {
    for (const failAfter of [0, 1, 2, 3]) {
        const adapter = engine(), host = createEngineHost({}, async () => adapter), target = runtime('file', failAfter);
        await choose(host, target, inputs).completion; await tick();
        assert.deepEqual(target.calls[0].args, [77, 0, 0, 0, 0, 0]);
        assert.equal(target.live.size,0); assert.equal(target.releases.length,failAfter);
        assert.deepEqual(adapter.calls.map(call => call[0]), ['cancel', 'register', 'drop']);
    }
}));

test('partial registration failure cleans adopted bytes before the original error and waiting query publish', { timeout: 2000 }, async () => withDOM(async inputs => {
    const registering = deferred(), began = deferred(), owned = new Set(), order = []; let failedName;
    const adapter = engine({ async registerFile(name) {
        owned.add(name); failedName = name; order.push('register'); began.resolve(); await registering.promise;
        throw new Error('registration failed after adoption');
    }, async dropFile(name) { assert.ok(owned.delete(name)); order.push('drop'); },
    async submit(input) { assert.equal(owned.size, 0); order.push('query'); return reply(input); } });
    const host = createEngineHost({}, async () => adapter), fileTarget = runtime('file'), target = runtime();
    const fileReply = fileTarget.reply; fileTarget.reply = (...args) => { order.push('file:error'); fileReply(...args); };
    const selected = choose(host, fileTarget, inputs); await began.promise;
    const query = host.submit(target, 1, 21, request('21')); registering.resolve();
    await Promise.all([selected.completion, query]); await tick();
    assert.deepEqual(order, ['register', 'drop', 'file:error', 'query']); assert.equal(owned.size, 0);
    assert.equal(fileTarget.calls[0].error, 'registration failed after adoption'); assert.equal(target.calls[0].error, null);
    assert.equal(host.releaseFile(failedName), true, 'successful best-effort cleanup remains idempotently released');
    adapter.registerFile = async (name,bytes) => { assert.notEqual(name, failedName, 'failed names remain burned'); return adapter.registration(name,bytes); };
    const next = runtime('file'); await choose(host, next, inputs).completion; assert.equal(next.calls[0].error, null);
}));

test('failed partial-registration cleanup logs once, preserves the registration error, and waits for close', async () => withDOM(async inputs => {
    const previous = console.error, errors = []; console.error = (...args) => errors.push(args);
    try {
        const owned = new Set(); let failedName, drops = 0;
        const adapter = engine({ async registerFile(name) { owned.add(name); failedName = name; throw new Error('registration broke'); },
            async dropFile() { drops++; throw new Error('cleanup broke'); }, async close() { owned.clear(); } });
        const host = createEngineHost({}, async () => adapter), fileTarget = runtime('file');
        await choose(host, fileTarget, inputs).completion; await tick();
        assert.equal(fileTarget.calls[0].error, 'registration broke'); assert.equal(owned.size, 1); assert.equal(drops, 1);
        assert.equal(errors.length, 1); assert.ok(errors[0][0].includes(failedName)); assert.equal(errors[0][1].message, 'cleanup broke');
        assert.equal(host.releaseFile(failedName), false);
        const target = runtime(); await host.submit(target, 1, 22, request('22')); await tick();
        assert.equal(target.calls[0].error, null); assert.equal(drops, 1); assert.equal(errors.length, 1);
        await host.cancel(true); assert.equal(owned.size, 0); assert.equal(drops, 1);
    } finally { console.error = previous; }
}));

test('a synchronous stale file-delivery guard retires registered bytes through drop or close', async () => {
    for (const closing of [false, true]) await withDOM(async inputs => {
        const owned = new Set(), order = []; let registered = false, superseded = false, closed, name;
        const adapter = engine({ async registerFile(value,bytes) { name = value; owned.add(value); registered = true; order.push('register'); return this.registration(value,bytes); },
            async dropFile(value) { assert.ok(owned.delete(value)); order.push('drop'); }, async close() { owned.clear(); order.push('close'); } });
        const host = createEngineHost({}, async () => adapter), first = runtime('file'), latest = runtime('file');
        host.chooseFile(first, 77); const input = inputs.at(-1);
        input.files = [{ get name() {
            if (registered && !superseded) { superseded = true; if (closing) closed = host.cancel(true); else host.chooseFile(latest, 88); }
            return 'guarded.csv';
        }, size: 3, async arrayBuffer() { return Uint8Array.from([1, 2, 3]).buffer; } }];
        await input.dispatch('change'); if (closed) await closed; await tick();
        assert.equal(first.calls.length, 0); assert.equal(owned.size, 0); assert.deepEqual(order, ['register', closing ? 'close' : 'drop']);
        assert.equal(host.releaseFile(name), !closing);
        if (!closing) { await inputs.at(-1).dispatch('cancel'); assert.equal(latest.calls[0].name, null); }
    });
});

test('successful import carries exact captured registration tokens in independently owned C strings', async () => withDOM(async inputs => {
    const registrations=[];
    const adapter=engine({async registerFile(name,bytes){
        const value={name,bytes:bytes.byteLength,engineEpoch:'18446744073709551615',registrationId:String(9007199254740993n+BigInt(registrations.length))};
        registrations.push(value);return value;
    }}),host=createEngineHost({},async()=>adapter);
    const first=runtime('file'),sizes=[],allocate=first.allocate;first.allocate=size=>{sizes.push(size);return allocate(size);};
    await choose(host,first,inputs,{name:"import-7-O'Brien-İstanbul-🦆.csv"}).completion;
    assert.equal(first.calls.length,1);const actual=first.calls[0];
    assert.equal(actual.args.length,6);assert.equal(actual.engineEpoch,'18446744073709551615');assert.equal(actual.registrationId,'9007199254740993');
    assert.equal(actual.label,"import-7-O'Brien-İstanbul-🦆.csv");assert.equal(actual.name,"import-1-import-7-O'Brien-İstanbul-🦆.csv");assert.equal(actual.error,null);
    assert.equal(new Set(actual.args.slice(1,5)).size,4,'all strings have separate owned allocations');
    assert.deepEqual(sizes,[encoder.encode(actual.name).length+1,encoder.encode(actual.label).length+1,21,17]);
    assert.equal(first.releases.length,4);assert.equal(first.live.size,0);
    registrations[0].engineEpoch='1';registrations[0].registrationId='2';
    assert.equal(actual.engineEpoch,'18446744073709551615');assert.equal(actual.registrationId,'9007199254740993');
    const second=runtime('file');await choose(host,second,inputs,{name:"import-7-O'Brien-İstanbul-🦆.csv"}).completion;
    assert.equal(second.calls[0].engineEpoch,actual.engineEpoch);assert.equal(second.calls[0].registrationId,'9007199254740994');
    assert.notEqual(second.calls[0].name,actual.name);assert.equal(second.calls[0].label,actual.label);
    await host.cancel(true);
}));
test('malformed registration success is cleaned before one bounded owned diagnostic reaches C', async () => withDOM(async inputs => {
    const edits=[null,undefined,{}, {name:'other.csv'},{bytes:4}];
    for(const field of ['engineEpoch','registrationId'])for(const value of [undefined,null,1,1n,'','0','01','-1','1.0','18446744073709551616','1'.repeat(1000)])edits.push({[field]:value});
    for(const edit of edits){
        const order=[],adapter=engine({async registerFile(name,bytes){order.push('register');const value=this.registration(name,bytes);return edit===null||edit===undefined||!Object.keys(edit).length?edit:{...value,...edit};},
            async dropFile(){order.push('drop');}}),host=createEngineHost({},async()=>adapter),target=runtime('file');
        const original=target.reply;target.reply=(...args)=>{order.push('diagnostic');original(...args);};
        await choose(host,target,inputs).completion;await tick();
        assert.deepEqual(order,['register','drop','diagnostic']);assert.equal(target.calls.length,1);
        const result=target.calls[0];assert.equal(result.name,null);assert.equal(result.label,null);assert.equal(result.engineEpoch,null);assert.equal(result.registrationId,null);
        assert.match(result.error,/invalid source registration token/);assert.equal(target.releases.length,1);assert.equal(target.live.size,0);
        await host.cancel(true);
    }
}));
test('chooser supersession or close during token transport frees every string without a stale callback', async () => {
    for(const closing of [false,true])for(const boundary of [1,2,3,4])await withDOM(async inputs=>{
        const adapter=engine(),host=createEngineHost({},async()=>adapter),target=runtime('file'),latest=runtime('file');
        let allocations=0,closed;const allocate=target.allocate;
        target.allocate=size=>{const pointer=allocate(size);if(++allocations===boundary){if(closing)closed=host.cancel(true);else host.chooseFile(latest,88);}return pointer;};
        await choose(host,target,inputs).completion;if(closed)await closed;await tick();
        assert.equal(target.calls.length,0);assert.equal(target.live.size,0);assert.equal(target.releases.length,4);
        assert.deepEqual(adapter.calls.map(call=>call[0]),['cancel','register',closing?'close':'drop']);
        if(!closing){await inputs.at(-1).dispatch('cancel');assert.deepEqual(latest.calls[0].args,[88,0,0,0,0,0]);await host.cancel(true);}
    });
});
test('invalid token cleanup failure remains explicit and is not retried while the host recovers', async () => withDOM(async inputs=>{
    const previous=console.error,errors=[];console.error=(...args)=>errors.push(args);
    try{
        const adapter=engine({async registerFile(){return null;},async dropFile(){throw new Error('token cleanup failed');}}),host=createEngineHost({},async()=>adapter),target=runtime('file');
        await choose(host,target,inputs).completion;await tick();
        assert.match(target.calls[0].error,/invalid source registration token/);assert.equal(target.calls[0].engineEpoch,null);assert.equal(target.live.size,0);
        assert.equal(errors.length,1);assert.equal(errors[0][1].message,'token cleanup failed');await tick();assert.equal(errors.length,1);
        const next=runtime();await host.submit(next,7,1,request('1'));assert.equal(next.calls[0].error,null);await host.cancel(true);
    }finally{console.error=previous;}
}));
test('hostile import diagnostics remain bounded and never allocate source identity strings', async () => withDOM(async inputs=>{
    const adapter=engine({async registerFile(){throw new Error('界'.repeat(8192));}}),host=createEngineHost({},async()=>adapter),target=runtime('file');
    const sizes=[],allocate=target.allocate;target.allocate=size=>{sizes.push(size);return allocate(size);};
    await choose(host,target,inputs).completion;
    assert.equal(target.calls.length,1);const result=target.calls[0];assert.deepEqual(result.args.slice(0,5),[77,0,0,0,0]);
    assert.equal(result.error,'界'.repeat(2048));assert.deepEqual(sizes,[6145]);assert.equal(target.live.size,0);assert.equal(target.releases.length,1);
    await host.cancel(true);
}));
