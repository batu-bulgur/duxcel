import test from 'node:test';
import assert from 'node:assert/strict';

// The override permits testing the integration owner's draft in another worktree.
const { createEngineHost } = await import(process.env.DUXCEL_ENGINE_HOST_MODULE ?? './engine_host.mjs');
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
function runtime(kind = 'query', failAfter = Infinity) {
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
            } else {
                const [session, name, label, error] = args;
                calls.push({ args, session, name: string(name), label: string(label), error: string(error) });
            }
        },
    };
}
function engine(overrides = {}) {
    const calls = [];
    return { calls,
        async submit(input) { calls.push(['submit', input.requestId]); return reply(input); },
        async cancel() { calls.push(['cancel']); },
        async registerFile(name, bytes) { calls.push(['register', name, bytes.slice()]); },
        async dropFile(name) { calls.push(['drop', name]); },
        async close() { calls.push(['close']); }, ...overrides };
}
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
        async registerFile() { order.push('register:start'); began.resolve(); await registering.promise; order.push('register:end'); },
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
    const adapter = engine({ async registerFile() { order.push('register:start'); began.resolve(); await registering.promise; order.push('register:end'); },
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
    const adapter = engine({ async registerFile(name) {
        order.push(['register:start', name]);
        if (name.endsWith('first.csv')) { began.resolve(); await registering.promise; }
        order.push(['register:end', name]);
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
    const adapter = engine({ async registerFile() { if (failures++ === 0) throw new Error('registration failed'); } });
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
    for (const failAfter of [0, 1]) {
        const fileTarget = runtime('file', failAfter), host = createEngineHost({}, async () => engine());
        const selected = choose(host, fileTarget, inputs); await selected.completion;
        assert.equal(fileTarget.calls.length, 1); assert.deepEqual(fileTarget.calls[0].args, [77, 0, 0, 0]);
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
        const adapter = engine({ async registerFile() { order.push('register:start'); began.resolve(); await registering.promise; order.push('register:end'); },
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
    const adapter = engine({ async registerFile(name) { owned.add(name); }, submit() { return pending.promise; },
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
        const adapter = engine({ async registerFile(name) { names.push(name); if (names.length === 1) { began.resolve(); await registering.promise; } },
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

test('a file reply OOM releases its registered bytes without exposing a name to C', async () => withDOM(async inputs => {
    const adapter = engine(), host = createEngineHost({}, async () => adapter), target = runtime('file', 0);
    await choose(host, target, inputs).completion; await tick();
    assert.deepEqual(target.calls[0].args, [77, 0, 0, 0]);
    assert.deepEqual(adapter.calls.map(call => call[0]), ['cancel', 'register', 'drop']);
}));
