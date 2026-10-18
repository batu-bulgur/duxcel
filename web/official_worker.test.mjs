import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Table, Int32, Int64, Uint64, Decimal, Utf8, TimestampNanosecond,
    vectorFromArray, makeVector, makeData, RecordBatchReader, tableToIPC } from 'apache-arrow';
import { LIMITS, pageRequest, decimalText, encodePage, pageCellText, pageTransferables,
    DuckDBWorkerAdapter, createOfficialWorkerAdapter,EXPORT_LIMITS,exportRequest,inspectParquetExport,workbookSortsRequest } from './official_worker.mjs';

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
const workbookSortColumns=[
    {name:'sheet_id',typeName:'INTEGER',numberKind:'i32'},
    {name:'position',typeName:'INTEGER',numberKind:'i32'},
    {name:'name',typeName:'VARCHAR',numberKind:'none'},
    {name:'descending',typeName:'BOOLEAN',numberKind:'none'},
];
function workbookSortInput(id='1',change={}){
    return {requestId:id,generation:'9007199254740993',documentId:'7',sourceId:'0',objectId:'0',revision:'18446744073709551615',
        offset:'0',knownTotal:null,rowLimit:449,purpose:'workbook-sorts',sql:'C OWNED CANONICAL SORTS PROJECTION LIMIT 449',
        columns:workbookSortColumns.map(column=>({...column})),
        sourceBindings:[{name:'fixture.dxls',engineEpoch:'1',registrationId:'1'}],...change};
}
function workbookSortTable(rows=449){
    const names=[null,'','NULL','9007199254740993','18446744073709551615','İstanbul 🦆'];
    return new Table({sheet_id:vectorFromArray(Array.from({length:rows},(_,i)=>i%64),new Int32()),
        position:vectorFromArray(Array.from({length:rows},(_,i)=>i%7),new Int32()),
        name:vectorFromArray(Array.from({length:rows},(_,i)=>names[i%names.length]),new Utf8()),
        descending:vectorFromArray(Array.from({length:rows},(_,i)=>Boolean(i%2)))});
}
test('workbook-sorts request is a separate frozen 449-row four-column single-source contract',()=>{
    const input=workbookSortInput(),parsed=workbookSortsRequest(input);
    assert.equal(parsed.purpose,'workbook-sorts');assert.equal(parsed.rowLimit,449);assert.equal(parsed.offset,'0');assert.equal(parsed.knownTotal,null);
    assert.ok(Object.isFrozen(parsed));assert.ok(Object.isFrozen(parsed.columns));assert.ok(Object.isFrozen(parsed.sourceBindings));
    input.columns[0].name='changed';input.sourceBindings[0].registrationId='2';
    assert.equal(parsed.columns[0].name,'sheet_id');assert.equal(parsed.sourceBindings[0].registrationId,'1');
    assert.throws(()=>pageRequest(workbookSortInput()),{code:'INVALID_REQUEST'});
    assert.throws(()=>pageRequest({requestId:'2',sql:'normal',rowLimit:257}),{code:'INVALID_REQUEST'});
    for(const change of [{purpose:undefined},{purpose:'copy'},{purpose:'Workbook-sorts'},{purpose:{}},{offset:'1'},
        {knownTotal:'449'},{rowLimit:0},{rowLimit:450},{rowLimit:NaN},{rowLimit:1.5},{columns:undefined},
        {columns:[]},{columns:workbookSortColumns.slice(0,3)},{columns:[...workbookSortColumns,{name:'extra',typeName:'VARCHAR'}]},
        {sourceBindings:undefined},{sourceBindings:[]},{sourceBindings:[{name:'a.dxls',engineEpoch:'1',registrationId:'1'},{name:'b.dxls',engineEpoch:'1',registrationId:'2'}]},
        {sourceBindings:[{name:'fixture.dxls',engineEpoch:'0',registrationId:'1'}]}]){
        assert.throws(()=>workbookSortsRequest(workbookSortInput('2',change)),{code:'INVALID_REQUEST'});
    }
});
test('workbook-sorts consumes one Arrow reader while preserving raw text and the normal row ceiling',async()=>{
    const source=reader(workbookSortTable()),parsed=workbookSortsRequest(workbookSortInput());
    const output=await encodePage(source,parsed);
    assert.equal(output.rowCount,449);assert.equal(output.columnCount,4);assert.equal(output.total,'449');assert.equal(output.countPending,true);
    assert.deepEqual(Array.from({length:6},(_,row)=>pageCellText(output,row,2)),[null,'','NULL','9007199254740993','18446744073709551615','İstanbul 🦆']);
    assert.equal(pageCellText(output,448,0),'0');assert.equal(pageCellText(output,448,1),'0');
    assert.equal(output.columns[0].typeName,'INTEGER');assert.equal(output.columns[3].typeName,'BOOLEAN');
    const valid=await encodePage(reader(workbookSortTable(448)),parsed);assert.equal(valid.rowCount,448);assert.equal(valid.countPending,false);
    await assert.rejects(encodePage(reader(workbookSortTable(450)),parsed),{code:'PAGE_LIMIT'});
    await assert.rejects(encodePage(reader(workbookSortTable(257)),request()),{code:'PAGE_LIMIT'});
    const wrong=new Table({sheet_id:vectorFromArray([0],new Int32()),position:vectorFromArray([0],new Int32()),name:vectorFromArray(['x'],new Utf8())});
    await assert.rejects(encodePage(reader(wrong),parsed),{code:'PAGE_LIMIT'});
    const huge=new Table({sheet_id:vectorFromArray([0],new Int32()),position:vectorFromArray([0],new Int32()),
        name:vectorFromArray(['x'.repeat(LIMITS.bytes+1)],new Utf8()),descending:vectorFromArray([false])});
    await assert.rejects(encodePage(reader(huge),parsed),{code:'PAGE_LIMIT'});
});
const deferred = () => { let resolve, reject; const promise = new Promise((yes, no) => { resolve = yes; reject = no; }); return { promise, resolve, reject }; };
const tick = () => new Promise(resolve => setImmediate(resolve));
function connection(extra = {}) {
    return { async send() { return reader(simple()); }, async cancelSent() { return true; }, async close() {}, ...extra };
}

test('dedicated workbook-sorts admission retains source-token guards without widening normal submit',async()=>{
    let sends=0;const ownedReader=reader(workbookSortTable());
    const database={async registerFileBuffer(){},async dropFile(){}};
    const adapter=new DuckDBWorkerAdapter({database,connection:connection({async send(){sends++;return sends===1?ownedReader:reader(simple());}})});
    const token=await adapter.registerFile('fixture.dxls',new Uint8Array([1]));
    const input=workbookSortInput('1',{sourceBindings:[token]});
    assert.throws(()=>adapter.submit(input),{code:'INVALID_REQUEST'});assert.equal(adapter.latestId,0n);assert.equal(sends,0);
    const reply=await adapter.submitWorkbookSorts(input);
    assert.equal(reply.status,'ok');assert.equal(reply.page.rowCount,449);assert.equal(sends,1);assert.equal(ownedReader.cancelled,1);
    await adapter.dropFile('fixture.dxls');const previous=adapter.latestId;
    assert.throws(()=>adapter.submitWorkbookSorts(workbookSortInput('2',{sourceBindings:[token]})),{code:'SOURCE_INVALIDATED'});
    assert.equal(adapter.latestId,previous);assert.equal(sends,1);
    // A source-free ordinary recovery query still has its unchanged envelope.
    const recovered=await adapter.submit({requestId:'2',sql:'ordinary recovery',rowLimit:256});
    assert.equal(recovered.status,'ok');assert.equal(recovered.page.rowCount,2);assert.equal(sends,2);
    await adapter.close();
});

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
    const registered = await adapter.registerFile('source.parquet', input.subarray(1, 4));
    assert.deepEqual(registered, { name: 'source.parquet', bytes: 3, engineEpoch: registered.engineEpoch, registrationId: '1' });
    assert.match(registered.engineEpoch, /^[1-9][0-9]*$/); assert.ok(Object.isFrozen(registered));
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

const bindingOf = ({ name, engineEpoch, registrationId }) => ({ name, engineEpoch, registrationId });
const boundRequest = (id, binding, extra = {}) => ({ requestId: id, sql: 'BOUND C SQL', sourceBindings: [bindingOf(binding)], ...extra });
function sourceFixture(extra = {}) {
    const files = new Map(), calls = [];
    const database = {
        async registerFileBuffer(name, bytes) { calls.push(['register', name]); files.set(name, bytes.slice()); },
        async dropFile(name) { calls.push(['drop', name]); files.delete(name); }, ...extra
    };
    return { database, calls, files, adapter: new DuckDBWorkerAdapter({ database, connection: connection({
        async send(sql) { calls.push(['send', sql]); return reader(simple()); }
    }) }) };
}
test('optional source bindings own exact bounded tokens and never silently downgrade malformed input', () => {
    const binding = { name: "O'Brien-İstanbul.csv", engineEpoch: '9007199254740993', registrationId: '18446744073709551615' };
    const input = boundRequest('1', binding), copied = pageRequest(input);
    assert.deepEqual(copied.sourceBindings, [binding]);
    assert.ok(Object.isFrozen(copied.sourceBindings)); assert.ok(Object.isFrozen(copied.sourceBindings[0]));
    input.sourceBindings[0].name = 'changed.csv'; input.sourceBindings.push(binding);
    assert.equal(copied.sourceBindings.length, 1); assert.equal(copied.sourceBindings[0].name, binding.name);
    assert.equal(pageRequest({ requestId: '1', sql: 'UNBOUND' }).sourceBindings, undefined);
    for (const value of [undefined, null, 0, 1, 1n, '', '0', '01', '-1', '1.0', '18446744073709551616']) {
        for (const field of ['engineEpoch', 'registrationId'])
            assert.throws(() => pageRequest(boundRequest('1', { ...binding, [field]: value })), { code: 'INVALID_REQUEST' });
    }
    for (const value of [null, false, {}, [], Array(1), [null], [binding, binding], Array(34).fill(binding)])
        assert.throws(() => pageRequest({ requestId: '1', sql: 'INVALID', sourceBindings: value }));
    for (const name of ['', '.', '..', '../bad.csv', 'bad\0.csv', 'x'.repeat(256)])
        assert.throws(() => pageRequest(boundRequest('1', { ...binding, name })));
    const maximum = Array.from({ length: LIMITS.sourceBindings }, (_, i) => ({ ...binding, name: `source-${i}.csv` }));
    assert.equal(pageRequest({ requestId: '1', sql: 'BOUND', sourceBindings: maximum }).sourceBindings.length, 33);
    assert.deepEqual(exportRequest(exportInput('1', { sourceBindings: [binding] })).sourceBindings, [binding]);
});
test('registration tokens are stable across shared database connections and distinct across independent databases', async () => {
    const fixture = sourceFixture(), first = fixture.adapter;
    const a = await first.registerFile('same.csv', new Uint8Array([1]));
    const second = new DuckDBWorkerAdapter({ database: fixture.database, connection: connection() });
    const b = await second.registerFile('second.csv', new Uint8Array([2]));
    assert.equal(a.engineEpoch, b.engineEpoch); assert.equal(a.registrationId, '1'); assert.equal(b.registrationId, '2');
    assert.equal((await second.submit(boundRequest('1', a))).status, 'ok');
    await first.close(); // Closing one connection does not revoke sibling sources.
    assert.equal((await second.submit(boundRequest('2', a))).status, 'ok');
    const reconnect = new DuckDBWorkerAdapter({ database: fixture.database, connection: connection() });
    assert.equal((await reconnect.submit(boundRequest('1', b))).status, 'ok');
    const independent = sourceFixture(), c = await independent.adapter.registerFile('same.csv', new Uint8Array([9]));
    assert.notEqual(c.engineEpoch, a.engineEpoch); assert.equal(c.registrationId, '1');
    assert.throws(() => independent.adapter.submit(boundRequest('1', a)), { code: 'SOURCE_INVALIDATED' });
    assert.equal(independent.adapter.latestId, 0n); assert.equal(independent.calls.some(call => call[0] === 'send'), false);
    assert.equal((await independent.adapter.submit(boundRequest('1', c))).status, 'ok');
    await second.close(); await reconnect.close(); await independent.adapter.close();
});
test('a stale bound request does not consume a ticket, cancel active work or reserve an export name', async () => {
    const fixture = sourceFixture(), token = await fixture.adapter.registerFile('source.csv', new Uint8Array([1]));
    const waiting = deferred(), began = deferred(); let cancellations = 0;
    fixture.adapter.connection.send = async () => { began.resolve(); return waiting.promise; };
    fixture.adapter.connection.cancelSent = async () => { cancellations++; return true; };
    const active = fixture.adapter.submit(boundRequest('1', token)); await began.promise;
    const wrong = { ...token, registrationId: '9007199254740993' };
    assert.throws(() => fixture.adapter.submit(boundRequest('2', wrong)), { code: 'SOURCE_INVALIDATED' });
    assert.throws(() => fixture.adapter.exportParquet(exportInput('2', { sourceBindings: [bindingOf(wrong)] })), { code: 'SOURCE_INVALIDATED' });
    assert.equal(fixture.adapter.latestId, 1n); assert.equal(cancellations, 0);
    waiting.resolve(reader(simple())); assert.equal((await active).status, 'ok');
    const unused = await fixture.adapter.registerFile('export-7-2.parquet', new Uint8Array([7]));
    assert.equal(unused.registrationId, '2', 'invalid export never reserved its output name');
    await fixture.adapter.close();
});
test('failed registrations and failed drops burn IDs, revoke immediately and never resurrect a token', async () => {
    let failDrop = true;
    const fixture = sourceFixture({
        async registerFileBuffer(name, bytes) { fixture.files.set(name, bytes.slice()); if (name === 'partial.csv') throw new Error('partial registration'); },
        async dropFile(name) { if (failDrop) throw new Error('drop fault'); fixture.files.delete(name); }
    });
    await assert.rejects(fixture.adapter.registerFile('partial.csv', new Uint8Array([1])), /partial registration/);
    const good = await fixture.adapter.registerFile('good.csv', new Uint8Array([2])); assert.equal(good.registrationId, '2');
    const partial = { name: 'partial.csv', engineEpoch: good.engineEpoch, registrationId: '1' };
    assert.throws(() => fixture.adapter.submit(boundRequest('1', partial)), { code: 'SOURCE_INVALIDATED' });
    const sibling = new DuckDBWorkerAdapter({ database: fixture.database, connection: connection() });
    await assert.rejects(sibling.dropFile('good.csv'), /drop fault/);
    assert.deepEqual([...fixture.files.get('good.csv')], [2], 'drop failure may retain bytes but not their token');
    assert.throws(() => fixture.adapter.submit(boundRequest('1', good)), { code: 'SOURCE_INVALIDATED' });
    await assert.rejects(fixture.adapter.registerFile('good.csv', new Uint8Array([3])), { code: 'IMMUTABLE_FILE' });
    failDrop = false; await sibling.dropFile('good.csv'); await sibling.dropFile('partial.csv');
    const fresh = await fixture.adapter.registerFile('fresh.csv', new Uint8Array([4])); assert.equal(fresh.registrationId, '3');
    assert.equal((await fixture.adapter.submit(boundRequest('1', fresh))).status, 'ok');
    await sibling.close(); await fixture.adapter.close();
});
test('a sibling drop during asynchronous registration cannot publish a resurrected ready token', async () => {
    const registering = deferred(), began = deferred(), fixture = sourceFixture({
        async registerFileBuffer() { began.resolve(); await registering.promise; }
    });
    const sibling = new DuckDBWorkerAdapter({ database: fixture.database, connection: connection() });
    const pending = fixture.adapter.registerFile('pending.csv', new Uint8Array([1])); await began.promise;
    await sibling.dropFile('pending.csv'); registering.resolve();
    await assert.rejects(pending, { code: 'SOURCE_INVALIDATED' });
    await assert.rejects(sibling.registerFile('pending.csv', new Uint8Array([2])), { code: 'IMMUTABLE_FILE' });
    const fresh = await sibling.registerFile('fresh.csv', new Uint8Array([2])); assert.equal(fresh.registrationId, '2');
    assert.throws(() => sibling.submit(boundRequest('1', { name: 'pending.csv', engineEpoch: fresh.engineEpoch, registrationId: '1' })), { code: 'SOURCE_INVALIDATED' });
    await sibling.close(); await fixture.adapter.close();
});
test('all captured sources are rechecked after reader cleanup and a stale contributor discards the whole page', async () => {
    const fixture = sourceFixture(), a = await fixture.adapter.registerFile('a.csv', new Uint8Array([1]));
    const b = await fixture.adapter.registerFile('b.csv', new Uint8Array([2]));
    const cleanup = deferred(), began = deferred();
    fixture.adapter.connection.send = async () => { const value = reader(simple()); value.cancel = async () => { began.resolve(); await cleanup.promise; }; return value; };
    const pending = fixture.adapter.submit(boundRequest('1', a, { sourceBindings: [bindingOf(a), bindingOf(b)] })); await began.promise;
    const sibling = new DuckDBWorkerAdapter({ database: fixture.database, connection: connection() }); await sibling.dropFile('b.csv');
    cleanup.resolve(); const result = await pending;
    assert.equal(result.status, 'error'); assert.equal(result.error.code, 'SOURCE_INVALIDATED'); assert.equal(result.page, undefined);
    fixture.adapter.connection.send = async () => reader(simple());
    assert.equal((await fixture.adapter.submit(boundRequest('2', a))).status, 'ok', 'unrelated source remains usable');
    await sibling.close(); await fixture.adapter.close();
});
test('a queued source revoked while an old cancellation drains is rejected before its SQL', async () => {
    const fixture = sourceFixture(), token = await fixture.adapter.registerFile('source.csv', new Uint8Array([1]));
    const sending = deferred(), cancelling = deferred(), began = deferred();
    fixture.adapter.connection.send = async sql => { fixture.calls.push(['send', sql]); return sending.promise; };
    fixture.adapter.connection.cancelSent = async () => { began.resolve(); await cancelling.promise; return true; };
    const old = fixture.adapter.submit({ requestId: '1', sql: 'OLD' });
    const next = fixture.adapter.submit(boundRequest('2', token)); await began.promise;
    const sibling = new DuckDBWorkerAdapter({ database: fixture.database, connection: connection() }); await sibling.dropFile(token.name);
    sending.resolve(reader(simple())); cancelling.resolve();
    assert.equal((await old).status, 'superseded'); const result = await next;
    assert.equal(result.status, 'error'); assert.equal(result.error.code, 'SOURCE_INVALIDATED');
    assert.deepEqual(fixture.calls.filter(call => call[0] === 'send'), [['send', 'OLD']]);
    await sibling.close(); await fixture.adapter.close();
});
test('bound export rechecks after private cleanup and never publishes bytes for a revoked source', async () => {
    const dropping = deferred(), began = deferred();
    const fixture = exportFixture({ drop: async name => { if (name === 'export-7-1.parquet') { began.resolve(); await dropping.promise; } } });
    const token = await fixture.adapter.registerFile('source.csv', new Uint8Array([1]));
    const pending = fixture.adapter.exportParquet(exportInput('1', { sourceBindings: [bindingOf(token)] })); await began.promise;
    const sibling = new DuckDBWorkerAdapter({ database: fixture.database, connection: connection() }); await sibling.dropFile(token.name);
    dropping.resolve(); const result = await pending;
    assert.equal(result.status, 'error'); assert.equal(result.error.code, 'SOURCE_INVALIDATED');
    assert.equal(result.data, undefined); assert.equal(result.page, undefined); assert.equal(fixture.files.size, 0);
    assert.equal((await fixture.adapter.submit({ requestId: '2', sql: 'LATEST' })).status, 'ok');
    await sibling.close(); await fixture.adapter.close();
});
test('a bound export preserves exact output and unrelated source cleanup does not invalidate it', async () => {
    const fixture = exportFixture(), token = await fixture.adapter.registerFile('source.csv', new Uint8Array([1]));
    const other = await fixture.adapter.registerFile('other.csv', new Uint8Array([2]));
    const sibling = new DuckDBWorkerAdapter({ database: fixture.database, connection: connection() }); await sibling.dropFile(other.name);
    const input = exportInput('1', { sourceBindings: [bindingOf(token)] });
    const result = await fixture.adapter.exportParquet(input);
    assert.equal(result.status, 'ok'); assert.equal(pageCellText(result.page, 0, 0), '600');
    assert.deepEqual(result.data, parquetBytes());
    for (const field of ['requestId', 'generation', 'documentId', 'sourceId', 'objectId', 'revision', 'offset']) assert.equal(result[field], input[field]);
    assert.deepEqual([...fixture.files.keys()], [token.name], 'only the source survives private export cleanup');
    assert.equal((await fixture.adapter.submit(boundRequest('2', token))).status, 'ok');
    await sibling.close(); await fixture.adapter.close();
});
test('1024 burned reservations never evict active tokens, and a disposal revokes the whole database lifetime', async () => {
    const fixture = sourceFixture(), token = await fixture.adapter.registerFile('first.csv', new Uint8Array([1]));
    for (let i = 1; i < LIMITS.fileNames; i++) await fixture.adapter.dropFile(`unused-${i}.csv`);
    await assert.rejects(fixture.adapter.registerFile('too-many.csv', new Uint8Array([2])), { code: 'FILE_REGISTRY_LIMIT' });
    assert.equal((await fixture.adapter.submit(boundRequest('1', token))).status, 'ok');
    const terminated = deferred(), began = deferred();
    const owner = new DuckDBWorkerAdapter({ database: fixture.database, connection: connection(), dispose: async () => { began.resolve(); await terminated.promise; } });
    const closing = owner.close(); await began.promise;
    assert.throws(() => fixture.adapter.submit(boundRequest('2', token)), { code: 'SOURCE_INVALIDATED' });
    await assert.rejects(fixture.adapter.registerFile('first.csv', new Uint8Array([3])), { code: 'NOT_AVAILABLE' });
    terminated.resolve(); await closing; await fixture.adapter.close();
});
