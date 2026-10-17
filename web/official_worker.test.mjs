import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Table, Int32, Int64, Uint64, Decimal, Utf8, TimestampNanosecond,
    vectorFromArray, makeVector, makeData, RecordBatchReader, tableToIPC } from 'apache-arrow';
import { LIMITS, pageRequest, decimalText, encodePage, pageCellText, pageTransferables,
    DuckDBWorkerAdapter, createOfficialWorkerAdapter,EXPORT_LIMITS,exportRequest,inspectParquetExport } from './official_worker.mjs';

const request = (id = '1', extra = {}) => pageRequest({ requestId: id, sql: 'C BUILT SQL LIMIT 256', ...extra });
function words(value, length = 4) {
    const result = new Uint32Array(length);
    value = BigInt.asUintN(length * 32, value);
    for (let i = 0; i < length; i++, value >>= 32n) result[i] = Number(value & 0xffffffffn);
    return result;
}
function reader(table) {
    return { schema: table.schema, cancelled: 0, open() { return this; },
        async *[Symbol.asyncIterator]() { yield* table.batches; }, async cancel() { this.cancelled++; } };
}
const simple = () => new Table({ value: vectorFromArray([1, 2], new Int32()) });
test('copy-only row envelopes keep ordinary page bounds and enforce 20,000 actual cells',async()=>{
    const table=new Table({value:vectorFromArray(Array.from({length:20000},(_,i)=>i),new Int32())});
    const copied=request('1',{purpose:'copy',rowLimit:20000});
    assert.equal(copied.purpose,'copy');
    const output=await encodePage(reader(table),copied);assert.equal(output.rowCount,20000);assert.equal(pageCellText(output,19999,0),'19999');
    assert.throws(()=>request('2',{rowLimit:257}),{code:'INVALID_REQUEST'});
    assert.throws(()=>request('2',{purpose:'copy',rowLimit:20001}),{code:'INVALID_REQUEST'});
    for(const purpose of [null,false,'page','Copy',{},1])assert.throws(()=>request('2',{purpose}),{code:'INVALID_REQUEST'});
    assert.throws(()=>request('2',{purpose:'copy',rowLimit:10001,columns:[{name:'a',typeName:'INTEGER'},{name:'b',typeName:'INTEGER'}]}),{code:'INVALID_REQUEST'});
    const wide=new Table({a:vectorFromArray(Array(10001).fill(1),new Int32()),b:vectorFromArray(Array(10001).fill(2),new Int32())});
    await assert.rejects(encodePage(reader(wide),request('3',{purpose:'copy',rowLimit:10001})),{code:'PAGE_LIMIT'});
});
const deferred = () => { let resolve, reject; const promise = new Promise((yes, no) => { resolve = yes; reject = no; }); return { promise, resolve, reject }; };
const tick = () => new Promise(resolve => setImmediate(resolve));
function connection(extra = {}) {
    return { async send() { return reader(simple()); }, async cancelSent() { return true; }, async close() {}, ...extra };
}

test('installed browser dependencies and transitive lock retain approved exact integrity pins', async () => {
    const lock = JSON.parse(await readFile(new URL('./package-lock.json', import.meta.url)));
    const pins = JSON.parse(await readFile(new URL('./toolchain.lock.json', import.meta.url))).browser_packages;
    for (const [name, pin] of Object.entries(pins)) {
        const installed = lock.packages[`node_modules/${name}`];
        assert.equal(installed.version, pin.version);
        assert.equal(installed.integrity, pin.integrity);
    }
});

test('request identities remain exact uint64 strings and invalid requests do not enter the queue', () => {
    const input = { requestId: '18446744073709551615', sql: 'SELECT 1 LIMIT 1', rowLimit: 1,
        generation: '9007199254740993', columns: [{ name: 'x', typeName: 'INTEGER', numberKind: 'i32' }] };
    const copied = pageRequest(input);
    input.columns[0].name = 'changed';
    assert.equal(copied.columns[0].name, 'x');
    assert.equal(copied.generation, '9007199254740993');
    for (const edit of [{ requestId: 2 }, { requestId: '01' }, { requestId: '18446744073709551616' },
        { requestId: '0' }, { rowLimit: 257 }, { rowLimit: 0 }, { offset: '-1' },
        { sql: 'SELECT\0hidden' }, { sql: 'x'.repeat(LIMITS.sqlBytes + 1) }]) {
        assert.throws(() => pageRequest({ ...input, ...edit }), { code: 'INVALID_REQUEST' });
    }
});

test('exact signed/unsigned integers, DECIMAL and SQL NULL/empty/literal values survive real Arrow vectors', async () => {
    const table = new Table({
        signed: vectorFromArray([-9223372036854775808n, 9007199254740993n, null], new Int64()),
        unsigned: vectorFromArray([18446744073709551615n, 0n, 1n], new Uint64()),
        exact: vectorFromArray([words(90071992547409931234567890n), words(-1n), null], new Decimal(10, 38, 128)),
        text: vectorFromArray([null, '', 'NULL'], new Utf8()),
    });
    const page = await encodePage(reader(table), request('1', { offset: '9007199254740993', generation: '18446744073709551615' }));
    assert.equal(page.generation, '18446744073709551615');
    assert.equal(page.total, '9007199254740996');
    assert.equal(pageCellText(page, 0, 0), '-9223372036854775808');
    assert.equal(pageCellText(page, 1, 0), '9007199254740993');
    assert.equal(pageCellText(page, 0, 1), '18446744073709551615');
    assert.equal(pageCellText(page, 0, 2), '9007199254740993.1234567890');
    assert.equal(pageCellText(page, 1, 2), '-0.0000000001');
    assert.equal(pageCellText(page, 2, 2), null);
    assert.deepEqual([0, 1, 2].map(row => pageCellText(page, row, 3)), [null, '', 'NULL']);
    assert.deepEqual(page.columns[2], { name: 'exact', typeName: 'DECIMAL(38,10)', numberKind: 'decimal',
        precision: 38, scale: 10, arrowType: 'Decimal[38e+10]', nullable: true, sqlTypeProvided: false, numeric: true });
    assert.equal(decimalText(words(-(1n << 127n)), 0), '-170141183460469231731687303715884105728');
});

test('C-owned SQL metadata remains distinct from Arrow transport metadata', async () => {
    const table = new Table({ huge: vectorFromArray([words((1n << 127n) - 1n)], new Decimal(0, 38, 128)) });
    const page = await encodePage(reader(table), request('1', { columns: [{ name: 'huge', typeName: 'HUGEINT', numberKind: 'i128' }] }));
    assert.equal(page.columns[0].typeName, 'HUGEINT');
    assert.equal(page.columns[0].numberKind, 'i128');
    assert.equal(page.columns[0].sqlTypeProvided, true);
    assert.equal(pageCellText(page, 0, 0), '170141183460469231731687303715884105727');
    await assert.rejects(encodePage(reader(table), request('2', { columns: [{ name: 'renamed', typeName: 'HUGEINT' }] })), { code: 'SCHEMA_CHANGED' });
});

test('lazy official Arrow IPC readers are opened before accessing schema', async () => {
    const table = simple(), bytes = tableToIPC(table, 'stream');
    const lazy = await RecordBatchReader.from((async function* () { yield bytes; })());
    const page = await encodePage(lazy, request());
    assert.equal(page.rowCount, 2);
    assert.equal(pageCellText(page, 1, 0), '2');
    await lazy.cancel();
});

test('owned page buffers detach cleanly on transfer and do not alias Arrow memory', async () => {
    const table = new Table({ label: vectorFromArray(['İstanbul 😀', '', 'NULL'], new Utf8()) });
    const page = await encodePage(reader(table), request());
    table.getChildAt(0).data[0].values.fill(0);
    const received = structuredClone(page, { transfer: pageTransferables(page) });
    assert.equal(page.text.byteLength, 0);
    assert.equal(page.offsets.byteLength, 0);
    assert.equal(page.nulls.byteLength, 0);
    assert.equal(pageCellText(received, 0, 0), 'İstanbul 😀');
    assert.equal(pageCellText(received, 1, 0), '');
    assert.throws(() => pageCellText(received, 3, 0), { code: 'INVALID_CELL' });
});

test('row/column/byte budgets and lossy/unsupported Arrow types fail explicitly', async () => {
    const many = new Table({ value: vectorFromArray(new Int32Array(257), new Int32()) });
    await assert.rejects(encodePage(reader(many), request()), { code: 'PAGE_LIMIT' });
    await assert.rejects(encodePage({ schema: { fields: new Array(257) } }, request()), { code: 'PAGE_LIMIT' });
    const large = new Table({ value: vectorFromArray(['x'.repeat(LIMITS.bytes)], new Utf8()) });
    await assert.rejects(encodePage(reader(large), request()), { code: 'PAGE_LIMIT' });
    const timestamp = new Table({ value: makeVector(makeData({ type: new TimestampNanosecond(), data: new BigInt64Array([9007199254740993n]), length: 1 })) });
    await assert.rejects(encodePage(reader(timestamp), request()), { code: 'UNSUPPORTED_TYPE' });
    const lossy = { schema: { fields: [{ name: 'value', type: new Int64(), nullable: false }] },
        async *[Symbol.asyncIterator]() { yield { numCols: 1, numRows: 1, getChildAt() { return { get() { return 9007199254740992; } }; } }; } };
    await assert.rejects(encodePage(lossy, request()), { code: 'LOSSY_VALUE' });
});

test('latest pending request wins and old cancellation cannot interrupt the new query', async () => {
    const first = deferred(), cancelled = deferred(), calls = [];
    const conn = connection({ send(sql, stream) { calls.push(`send:${sql}`); assert.equal(stream, true); return sql === 'A' ? first.promise : Promise.resolve(reader(simple())); },
        cancelSent() { calls.push('cancel:A'); return cancelled.promise; } });
    const adapter = new DuckDBWorkerAdapter({ connection: conn });
    const a = adapter.submit({ requestId: '1', sql: 'A' });
    const b = adapter.submit({ requestId: '2', sql: 'B' });
    const c = adapter.submit({ requestId: '3', sql: 'C', generation: '5' });
    assert.equal((await b).status, 'superseded');
    await tick(); first.resolve(reader(simple())); await tick();
    assert.deepEqual(calls, ['send:A', 'cancel:A']);
    cancelled.resolve(true);
    assert.equal((await a).status, 'superseded');
    const reply = await c;
    assert.equal(reply.status, 'ok');
    assert.equal(reply.page.generation, '5');
    assert.deepEqual(calls, ['send:A', 'cancel:A', 'send:C']);
});

test('cancellation clears pending work; stale SQL errors never replace a new page', async () => {
    const first = deferred(); let sends = 0;
    const conn = connection({ send() { sends++; return sends === 1 ? first.promise : Promise.resolve(reader(simple())); },
        async cancelSent() { first.reject(new Error('interrupted old SQL')); return true; } });
    const adapter = new DuckDBWorkerAdapter({ connection: conn });
    const a = adapter.submit({ requestId: '1', sql: 'A' });
    const b = adapter.submit({ requestId: '2', sql: 'B' });
    await adapter.cancel();
    assert.equal((await a).status, 'cancelled');
    assert.equal((await b).status, 'cancelled');
    assert.equal(sends, 1);
    assert.equal((await adapter.submit({ requestId: '3', sql: 'C' })).status, 'ok');
    assert.throws(() => adapter.submit({ requestId: '3', sql: 'duplicate' }), { code: 'STALE_REQUEST' });
    await adapter.close();
    assert.throws(() => adapter.submit({ requestId: '4', sql: 'closed' }), { code: 'NOT_AVAILABLE' });
});

test('SQL errors are bounded replies and a subsequent query can recover', async () => {
    let first = true, cancelled = 0;
    const conn = connection({ async send() { if (first) { first = false; throw new Error('binding failure '.repeat(500)); } return reader(simple()); },
        async cancelSent() { cancelled++; return false; } });
    const adapter = new DuckDBWorkerAdapter({ connection: conn });
    const failed = await adapter.submit({ requestId: '1', sql: 'invalid SQL' });
    assert.equal(failed.status, 'error'); assert.equal(failed.error.code, 'QUERY_FAILED');
    assert.equal(failed.error.message.length, 2048); assert.equal(cancelled, 1);
    assert.equal((await adapter.submit({ requestId: '2', sql: 'valid C SQL' })).status, 'ok');
});

test('file registration transfers only an owned copy, preserving the caller/Wasm heap', async () => {
    const input = new Uint8Array([99, 1, 2, 3, 88]); let stored;
    const database = { async registerFileBuffer(name, data) { assert.equal(name, 'source.parquet'); stored = structuredClone(data, { transfer: [data.buffer] }); } };
    const adapter = new DuckDBWorkerAdapter({ database, connection: connection() });
    assert.deepEqual(await adapter.registerFile('source.parquet', input.subarray(1, 4)), { name: 'source.parquet', bytes: 3 });
    assert.deepEqual([...stored], [1, 2, 3]); assert.deepEqual([...input], [99, 1, 2, 3, 88]);
    await assert.rejects(adapter.registerFile('../source', input), { code: 'INVALID_FILE' });
});

test('registration owns its data across caller detachment and blocks competing commands', async () => {
    const done = deferred(), source = new Uint8Array([1, 2, 3]); let copied;
    const database = { registerFileBuffer(name, bytes) { copied = bytes; return done.promise; } };
    const adapter = new DuckDBWorkerAdapter({ database, connection: connection() });
    const registering = adapter.registerFile('source.parquet', source);
    structuredClone(source, { transfer: [source.buffer] });
    assert.throws(() => adapter.submit({ requestId: '1', sql: 'too early' }), { code: 'NOT_AVAILABLE' });
    await assert.rejects(adapter.close(), { code: 'NOT_AVAILABLE' });
    assert.deepEqual([...copied], [1, 2, 3]);
    done.resolve();
    assert.equal((await registering).bytes, 3);
    assert.equal((await adapter.submit({ requestId: '1', sql: 'after registration' })).status, 'ok');
});

test('a registered name cannot be rebound, including after cleanup or in a later query', async () => {
    const stored = new Map(), calls = [];
    const database = {
        async registerFileBuffer(name, bytes) { calls.push(['register', name]); stored.set(name, bytes.slice()); },
        async dropFile(name) { calls.push(['drop', name]); stored.delete(name); }
    };
    const adapter = new DuckDBWorkerAdapter({ database, connection: connection() });
    await adapter.registerFile('source.parquet', new Uint8Array([1]));
    await assert.rejects(adapter.registerFile('source.parquet', new Uint8Array([2])), { code: 'IMMUTABLE_FILE' });
    assert.deepEqual([...stored.get('source.parquet')], [1]);
    await adapter.dropFile('source.parquet');
    assert.equal((await adapter.submit({ requestId: '1', sql: 'C-built unrelated query' })).status, 'ok');
    await assert.rejects(adapter.registerFile('source.parquet', new Uint8Array([3])), { code: 'IMMUTABLE_FILE' });
    await adapter.registerFile('fresh-source.parquet', new Uint8Array([3]));
    assert.deepEqual(calls, [['register', 'source.parquet'], ['drop', 'source.parquet'], ['register', 'fresh-source.parquet']]);
    assert.deepEqual([...stored.get('fresh-source.parquet')], [3]); await adapter.close();
});

test('partial registration failure permanently reserves its name before the engine mutates', async () => {
    const stored = new Map(), calls = [];
    const database = {
        async registerFileBuffer(name, bytes) { calls.push(name); stored.set(name, bytes); if (name === 'partial.csv') throw new Error('engine failed after storing bytes'); },
        async dropFile(name) { stored.delete(name); }
    };
    const adapter = new DuckDBWorkerAdapter({ database, connection: connection() });
    await assert.rejects(adapter.registerFile('partial.csv', new Uint8Array([7])), /after storing/);
    assert.equal(adapter.admin, false); assert.deepEqual([...stored.get('partial.csv')], [7]);
    await assert.rejects(adapter.registerFile('partial.csv', new Uint8Array([8])), { code: 'IMMUTABLE_FILE' });
    await adapter.dropFile('partial.csv'); assert.equal(stored.has('partial.csv'), false);
    await assert.rejects(adapter.registerFile('partial.csv', new Uint8Array([9])), { code: 'IMMUTABLE_FILE' });
    await adapter.registerFile('recovery.csv', new Uint8Array([9]));
    assert.deepEqual(calls, ['partial.csv', 'recovery.csv']); await adapter.close();
});

test('drop attempts reserve unknown names even on failure and preserve administration exclusion', async () => {
    const dropping = deferred(), calls = [];
    const database = {
        async registerFileBuffer(name) { calls.push(['register', name]); },
        async dropFile(name) { calls.push(['drop', name]); if (name === 'unknown.csv') await dropping.promise; }
    };
    const adapter = new DuckDBWorkerAdapter({ database, connection: connection() });
    const removal = adapter.dropFile('unknown.csv');
    assert.equal(adapter.admin, true);
    assert.throws(() => adapter.submit({ requestId: '1', sql: 'query during drop' }), { code: 'NOT_AVAILABLE' });
    await assert.rejects(adapter.close(), { code: 'NOT_AVAILABLE' });
    await assert.rejects(adapter.registerFile('fresh.csv', new Uint8Array([1])), { code: 'NOT_AVAILABLE' });
    await assert.rejects(adapter.dropFile('later.csv'), { code: 'NOT_AVAILABLE' });
    dropping.reject(new Error('drop failed after mutation attempt'));
    await assert.rejects(removal, /drop failed/); assert.equal(adapter.admin, false);
    await assert.rejects(adapter.registerFile('unknown.csv', new Uint8Array([2])), { code: 'IMMUTABLE_FILE' });
    await adapter.registerFile('fresh.csv', new Uint8Array([1]));
    await adapter.registerFile('later.csv', new Uint8Array([2]));
    assert.deepEqual(calls, [['drop', 'unknown.csv'], ['register', 'fresh.csv'], ['register', 'later.csv']]); await adapter.close();
});

test('name registry exhaustion fails closed without eviction and still permits cleanup', async () => {
    let registrations = 0, drops = 0;
    const database = { async registerFileBuffer() { registrations++; }, async dropFile() { drops++; } };
    const adapter = new DuckDBWorkerAdapter({ database, connection: connection() });
    assert.equal(LIMITS.fileNames, 1024); // 1,024 names * <=255 UTF-8 bytes, without recycling.
    for (let i = 0; i < LIMITS.fileNames; i++) {
        if (i % 2) await adapter.dropFile(`retired-${i}.csv`);
        else await adapter.registerFile(`retired-${i}.csv`, new Uint8Array([1]));
    }
    await assert.rejects(adapter.registerFile('overflow.csv', new Uint8Array([2])), { code: 'FILE_REGISTRY_LIMIT' });
    await assert.rejects(adapter.dropFile('overflow-drop.csv'), { code: 'FILE_REGISTRY_LIMIT' });
    await adapter.dropFile('retired-0.csv');
    await assert.rejects(adapter.registerFile('retired-0.csv', new Uint8Array([3])), { code: 'IMMUTABLE_FILE' });
    await assert.rejects(adapter.registerFile('retired-1.csv', new Uint8Array([3])), { code: 'IMMUTABLE_FILE' });
    assert.equal(registrations, LIMITS.fileNames / 2); assert.equal(drops, LIMITS.fileNames / 2 + 1);
    assert.equal((await adapter.submit({ requestId: '1', sql: 'queries still work at the file-name limit' })).status, 'ok');
    await adapter.close();
    const nextSession = new DuckDBWorkerAdapter({ database: { ...database }, connection: connection() });
    await nextSession.registerFile('retired-0.csv', new Uint8Array([4])); await nextSession.close();
});

test('reconnected adapters share name reservations by database identity, including failed names', async () => {
    const stored = new Map(), calls = [], registering = deferred(); let closedConnections = 0;
    const database = {
        async registerFileBuffer(name, bytes) {
            calls.push(['register', name]); stored.set(name, bytes.slice());
            if (name === 'partial.csv') await registering.promise;
        },
        async dropFile(name) { calls.push(['drop', name]); stored.delete(name); }
    };
    const first = new DuckDBWorkerAdapter({ database, connection: connection({ async close() { closedConnections++; } }) });
    const second = new DuckDBWorkerAdapter({ database, connection: connection({ async close() { closedConnections++; } }) });
    assert.notEqual(first.connection, second.connection);
    await first.registerFile('source.csv', new Uint8Array([1]));
    await assert.rejects(second.registerFile('source.csv', new Uint8Array([2])), { code: 'IMMUTABLE_FILE' });
    assert.deepEqual([...stored.get('source.csv')], [1]);
    await second.dropFile('source.csv');
    await assert.rejects(first.registerFile('source.csv', new Uint8Array([3])), { code: 'IMMUTABLE_FILE' });
    const partial = first.registerFile('partial.csv', new Uint8Array([4]));
    await assert.rejects(second.registerFile('partial.csv', new Uint8Array([5])), { code: 'IMMUTABLE_FILE' });
    registering.reject(new Error('partial registration failed'));
    await assert.rejects(partial, /partial registration failed/);
    await second.dropFile('partial.csv');
    await first.close();
    const reconnected = new DuckDBWorkerAdapter({ database, connection: connection({ async close() { closedConnections++; } }) });
    await assert.rejects(reconnected.registerFile('source.csv', new Uint8Array([6])), { code: 'IMMUTABLE_FILE' });
    await assert.rejects(reconnected.registerFile('partial.csv', new Uint8Array([6])), { code: 'IMMUTABLE_FILE' });
    await reconnected.registerFile('fresh.csv', new Uint8Array([7]));
    assert.deepEqual(calls, [['register', 'source.csv'], ['drop', 'source.csv'], ['register', 'partial.csv'], ['drop', 'partial.csv'], ['register', 'fresh.csv']]);
    await second.close(); await reconnected.close(); assert.equal(closedConnections, 3);
    const independentData = new Map();
    const independent = new DuckDBWorkerAdapter({ database: { async registerFileBuffer(name, bytes) { independentData.set(name, bytes.slice()); } }, connection: connection() });
    await independent.registerFile('source.csv', new Uint8Array([8]));
    assert.deepEqual([...independentData.get('source.csv')], [8]); assert.deepEqual([...stored.get('fresh.csv')], [7]); await independent.close();
});

test('nonmutating validation and active-query rejections do not consume a fresh valid name', async () => {
    const sending = deferred(), calls = [];
    const database = { async registerFileBuffer(name) { calls.push(name); } };
    const adapter = new DuckDBWorkerAdapter({ database, connection: connection({
        send: () => sending.promise, async cancelSent() { sending.resolve(reader(simple())); return true; }
    }) });
    await assert.rejects(adapter.registerFile('fresh.csv', null), { code: 'FILE_LIMIT' });
    await assert.rejects(adapter.dropFile('../invalid'), { code: 'INVALID_FILE' });
    const query = adapter.submit({ requestId: '1', sql: 'running C query' });
    await assert.rejects(adapter.registerFile('fresh.csv', new Uint8Array([1])), { code: 'NOT_AVAILABLE' });
    await adapter.cancel(); assert.equal((await query).status, 'cancelled');
    await adapter.registerFile('fresh.csv', new Uint8Array([1])); assert.deepEqual(calls, ['fresh.csv']); await adapter.close();
});

function exportInput(id='1',change={}){
    return {requestId:id,documentId:'7',generation:'9007199254740993',sourceId:'0',objectId:'18446744073709551615',revision:'9007199254740995',
        offset:'0',knownTotal:'1',rowLimit:1,sql:'COPY (C GENERATED SQL) TO PRIVATE PARQUET',fileName:`export-7-${id}.parquet`,...change};
}
function parquetBytes(size=32){const bytes=new Uint8Array(size);bytes.set([80,65,82,49]);bytes.set([80,65,82,49],size-4);new DataView(bytes.buffer).setUint32(size-8,8,true);return bytes;}
function exportFixture({table=new Table({Count:vectorFromArray([600n],new Int64())}),stat,output=parquetBytes(),send,drop,copy}={}){
    const calls=[],files=new Map();
    const database={async globFiles(name){calls.push(['glob',name]);return files.has(name)?[{fileName:name,...(stat??{fileSize:files.get(name).length})}]:[];},
        async flushFiles(){calls.push(['flush']);},async copyFileToBuffer(name){calls.push(['copy',name]);return copy?copy(name):files.get(name).slice();},
        async dropFile(name){calls.push(['drop',name]);if(drop)await drop(name);files.delete(name);},async registerFileBuffer(name,bytes){files.set(name,bytes.slice());}};
    const conn=connection({async send(sql){calls.push(['send',sql]);if(sql==='LATEST')return reader(simple());if(sql.startsWith('COPY'))files.set('export-7-1.parquet',output.slice());return send?send(sql):reader(table);},
        async cancelSent(){calls.push(['cancel']);return true;},async close(){calls.push(['close']);}});
    return {calls,files,database,adapter:new DuckDBWorkerAdapter({database,connection:conn})};
}
test('export request is explicit and cannot enlarge ordinary pages or read arbitrary files',()=>{
    const value=exportRequest(exportInput());assert.ok(Object.isFrozen(value));assert.equal(value.fileName,'export-7-1.parquet');
    assert.equal(EXPORT_LIMITS.bytes,64*1024*1024);assert.equal(LIMITS.rows,256);
    for(const change of [{documentId:'0'},{requestId:'4294967296'},{documentId:'4294967296'},{fileName:'/tmp/a.parquet'},
        {fileName:'export-7-2.parquet'},{fileName:'import-1-source.parquet'},{offset:'1'},{knownTotal:null},{rowLimit:2},{columns:[]},{columns:null},{purpose:'copy'}])
        assert.throws(()=>exportRequest(exportInput('1',change)));
    const bytes=parquetBytes(),storage=new Uint8Array(64);storage.set(bytes,13);
    assert.deepEqual(inspectParquetExport(storage.subarray(13,45),32),{byteLength:32,footerBytes:8});
    assert.throws(()=>inspectParquetExport(bytes,33),{code:'EXPORT_LIMIT'});
    const invalid=bytes.slice();invalid[0]=0;assert.throws(()=>inspectParquetExport(invalid),{code:'INVALID_EXPORT'});
    const badFooter=bytes.slice();new DataView(badFooter.buffer).setUint32(24,21,true);assert.throws(()=>inspectParquetExport(badFooter),{code:'INVALID_EXPORT'});
    assert.throws(()=>inspectParquetExport(new Uint8Array(new SharedArrayBuffer(32))),{code:'INVALID_EXPORT'});
    const detached=bytes.slice();structuredClone(detached,{transfer:[detached.buffer]});assert.throws(()=>inspectParquetExport(detached));
});
test('export drains Count before bounded file copy, removes its private file and owns exact output',async()=>{
    const fixture=exportFixture(),input=exportInput(),before=parquetBytes();
    const result=await fixture.adapter.exportParquet(input);
    assert.equal(result.status,'ok');assert.equal(pageCellText(result.page,0,0),'600');assert.deepEqual(result.data,before);
    for(const field of ['requestId','generation','documentId','sourceId','objectId','revision','offset'])assert.equal(result[field],input[field]);
    assert.equal(fixture.files.size,0);
    assert.deepEqual(fixture.calls.map(value=>value[0]),['glob','send','flush','glob','copy','glob','drop','glob']);
    await assert.rejects(fixture.adapter.registerFile(input.fileName,new Uint8Array([9])),{code:'IMMUTABLE_FILE'});
    const sibling=new DuckDBWorkerAdapter({database:fixture.database,connection:connection()});
    assert.throws(()=>sibling.exportParquet(input),{code:'IMMUTABLE_FILE'});await sibling.close();await fixture.adapter.close();
});
test('exact large Count and reader shutdown precede every private file operation',async()=>{
    const stopping=deferred(),began=deferred(),table=new Table({Count:vectorFromArray([9007199254740993n],new Int64())});
    const fixture=exportFixture({send:async()=>{const value=reader(table);value.cancel=async()=>{began.resolve();await stopping.promise;};return value;}});
    const pending=fixture.adapter.exportParquet(exportInput());await began.promise;await tick();
    assert.equal(fixture.calls.some(value=>value[0]==='flush'||value[0]==='copy'),false);
    stopping.resolve();const result=await pending;assert.equal(result.status,'ok');assert.equal(pageCellText(result.page,0,0),'9007199254740993');
    assert.equal(fixture.files.size,0);await fixture.adapter.close();
});
test('existing private path is preserved and invalid requests never send SQL',async()=>{
    const fixture=exportFixture(),input=exportInput();fixture.files.set(input.fileName,new Uint8Array([9]));
    const result=await fixture.adapter.exportParquet(input);assert.equal(result.status,'error');assert.equal(result.error.code,'EXPORT_EXISTS');
    assert.deepEqual([...fixture.files.get(input.fileName)],[9]);assert.equal(fixture.calls.filter(value=>['send','copy','drop'].includes(value[0])).length,0);
    const fresh=exportFixture();assert.throws(()=>fresh.adapter.exportParquet(exportInput('1',{fileName:'source.parquet'})));
    assert.equal(fresh.adapter.latestId,0n);assert.deepEqual(fresh.calls,[]);await fresh.adapter.close();await fixture.adapter.close();
});
test('missing/zero/oversized file sizes reject before copy; malformed binary and Count never publish bytes',async()=>{
    for(const stat of [{},{fileSize:0},{fileSize:NaN},{fileSize:'32'},{fileSize:EXPORT_LIMITS.bytes+1}]){
        const fixture=exportFixture({stat}),result=await fixture.adapter.exportParquet(exportInput());
        assert.equal(result.status,'error');assert.equal(result.data,undefined);assert.equal(result.error.code,'EXPORT_LIMIT');
        assert.equal(fixture.calls.filter(value=>value[0]==='copy').length,0);assert.equal(fixture.files.size,0);await fixture.adapter.close();
    }
    for(const output of [new Uint8Array(32),parquetBytes(40)]){
        const fixture=exportFixture({output,stat:{fileSize:32}}),result=await fixture.adapter.exportParquet(exportInput());
        assert.equal(result.status,'error');assert.equal(result.data,undefined);assert.equal(fixture.files.size,0);await fixture.adapter.close();
    }
    for(const table of [new Table({Other:vectorFromArray([1n],new Int64())}),new Table({Count:vectorFromArray([-1n],new Int64())}),
        new Table({Count:vectorFromArray([null],new Int64())}),new Table({Count:vectorFromArray([1,2],new Int32())})]){
        const fixture=exportFixture({table}),result=await fixture.adapter.exportParquet(exportInput());
        assert.equal(result.status,'error');assert.equal(result.data,undefined);assert.equal(fixture.calls.filter(value=>value[0]==='copy').length,0);
        assert.equal(fixture.files.size,0);await fixture.adapter.close();
    }
});
test('SQL/flush/copy/drop failures discard all export data and permit a later ordinary query',async()=>{
    for(const kind of ['send','flush','copy','drop']){
        const fixture=exportFixture({...(kind==='send'?{send:async()=>{throw new Error('partial COPY failed');}}:{}),
            ...(kind==='copy'?{copy:async()=>{throw new Error('binary copy failed');}}:{}),...(kind==='drop'?{drop:async()=>{throw new Error('drop failed');}}:{})});
        if(kind==='flush')fixture.database.flushFiles=async()=>{throw new Error('flush failed');};
        const result=await fixture.adapter.exportParquet(exportInput());assert.equal(result.status,'error');assert.equal(result.data,undefined);
        if(kind==='drop')assert.equal(result.error.code,'EXPORT_CLEANUP');else assert.equal(fixture.files.size,0);
        assert.equal((await fixture.adapter.submit({requestId:'2',sql:'LATEST'})).status,'ok');await fixture.adapter.close();
    }
});
test('superseding export waits through noninterruptible copy and private drop before next SQL',async()=>{
    const copying=deferred(),copyBegan=deferred(),dropping=deferred(),dropBegan=deferred();
    const fixture=exportFixture({copy:async()=>{copyBegan.resolve();return copying.promise;},drop:async()=>{dropBegan.resolve();await dropping.promise;}});
    const original=fixture.adapter.exportParquet(exportInput());await copyBegan.promise;
    const latest=fixture.adapter.submit({requestId:'2',sql:'LATEST'});await tick();
    assert.equal(fixture.calls.some(value=>value[0]==='send'&&value[1]==='LATEST'),false);
    copying.resolve(parquetBytes());await dropBegan.promise;await tick();
    assert.equal(fixture.calls.some(value=>value[0]==='send'&&value[1]==='LATEST'),false);
    dropping.resolve();const old=await original;assert.equal(old.status,'superseded');assert.equal(old.data,undefined);
    assert.equal((await latest).status,'ok');assert.equal(fixture.files.size,0);
    assert.ok(fixture.calls.findIndex(value=>value[0]==='drop')<fixture.calls.findIndex(value=>value[0]==='send'&&value[1]==='LATEST'));await fixture.adapter.close();
});
test('a cancellation begun during private cleanup drains before the next query can enter',async()=>{
    const dropping=deferred(),dropBegan=deferred(),cancelling=deferred(),cancelBegan=deferred();
    const fixture=exportFixture({drop:async()=>{dropBegan.resolve();await dropping.promise;}});
    fixture.adapter.connection.cancelSent=async()=>{cancelBegan.resolve();await cancelling.promise;return true;};
    const old=fixture.adapter.exportParquet(exportInput());await dropBegan.promise;
    const latest=fixture.adapter.submit({requestId:'2',sql:'LATEST'});await cancelBegan.promise;dropping.resolve();await tick();
    assert.equal(fixture.calls.some(value=>value[0]==='send'&&value[1]==='LATEST'),false);
    cancelling.resolve();assert.equal((await old).status,'superseded');assert.equal((await latest).status,'ok');assert.equal(fixture.files.size,0);await fixture.adapter.close();
});
test('cancel and close own export cleanup until completion and never return partial bytes',async()=>{
    for(const closing of [false,true]){
        const copying=deferred(),began=deferred(),fixture=exportFixture({copy:async()=>{began.resolve();return copying.promise;}});
        const pending=fixture.adapter.exportParquet(exportInput());await began.promise;let ended=false;
        const cancelled=(closing?fixture.adapter.close():fixture.adapter.cancel()).then(()=>{ended=true;});await tick();assert.equal(ended,false);
        copying.resolve(parquetBytes());await cancelled;const result=await pending;assert.equal(result.status,'cancelled');assert.equal(result.data,undefined);assert.equal(fixture.files.size,0);
        if(closing)assert.equal(fixture.calls.at(-1)[0],'close');else {assert.equal((await fixture.adapter.submit({requestId:'2',sql:'LATEST'})).status,'ok');await fixture.adapter.close();}
    }
});
test('superseded pending exports burn names without mutating files or growing normal limits',async()=>{
    const first=deferred(),fixture=exportFixture({send:async()=>first.promise});
    const active=fixture.adapter.submit({requestId:'1',sql:'WAIT'}),pending=fixture.adapter.exportParquet(exportInput('2'));
    const latest=fixture.adapter.submit({requestId:'3',sql:'LATEST'});assert.equal((await pending).status,'superseded');first.resolve(reader(simple()));
    assert.equal((await active).status,'superseded');assert.equal((await latest).status,'ok');
    await assert.rejects(fixture.adapter.registerFile('export-7-2.parquet',new Uint8Array([1])),{code:'IMMUTABLE_FILE'});
    assert.equal(fixture.calls.filter(value=>value[0]==='send'&&value[1].startsWith('COPY')).length,0);await fixture.adapter.close();
    const full=exportFixture();for(let i=0;i<LIMITS.fileNames;i++)await full.adapter.dropFile(`retired-${i}.csv`);
    const before=full.calls.length;assert.throws(()=>full.adapter.exportParquet(exportInput()),{code:'FILE_REGISTRY_LIMIT'});assert.equal(full.calls.length,before);
    assert.equal((await full.adapter.submit({requestId:'1',sql:'LATEST'})).status,'ok');await full.adapter.close();
});

test('official Worker factory uses single-threaded lossless options and owns shutdown/failure cleanup', async () => {
    const calls = [], conn = connection({ async close() { calls.push('close'); } });
    class FakeWorker { constructor(url) { calls.push(['worker', url]); } terminate() { calls.push('worker-terminate'); } }
    class FakeDatabase {
        constructor(logger, worker) { assert.ok(worker instanceof FakeWorker); }
        async instantiate(...args) { calls.push(['instantiate', ...args]); }
        async open(options) { calls.push(['open', options]); }
        async connect() { return conn; }
        async getVersion() { return 'fixture-only-version'; }
        async terminate() { calls.push('terminate'); }
    }
    const duckdb = { AsyncDuckDB: FakeDatabase, VoidLogger: class {} };
    const adapter = await createOfficialWorkerAdapter({ moduleURL: '/duckdb-mvp.wasm', workerURL: '/duckdb-browser-mvp.worker.js', duckdb, WorkerClass: FakeWorker });
    assert.equal(adapter.engineVersion, 'fixture-only-version');
    assert.deepEqual(calls[1], ['instantiate', '/duckdb-mvp.wasm']);
    assert.deepEqual(calls[2][1], { maximumThreads: 1, arrowLosslessConversion: true,
        query: { castBigIntToDouble: false, castDecimalToDouble: false, castTimestampToDate: false, castDurationToTime64: false } });
    await adapter.close(); assert.deepEqual(calls.slice(-2), ['close', 'terminate']);
    class BrokenDatabase extends FakeDatabase { async instantiate() { throw new Error('bad module'); } }
    await assert.rejects(createOfficialWorkerAdapter({ moduleURL: '/bad.wasm', workerURL: '/worker.js', duckdb: { ...duckdb, AsyncDuckDB: BrokenDatabase }, WorkerClass: FakeWorker }), /bad module/);
    assert.equal(calls.at(-1), 'terminate');
});
