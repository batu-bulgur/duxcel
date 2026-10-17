// Pure bounded binary contracts, with injected memory file readers only.
// No real/fake SQL engine, browser, download, clipboard or OS file is opened.
import test from 'node:test';
import assert from 'node:assert/strict';
import {EXPORT_PROBE_LIMITS,parquetFileSize,inspectParquet,readParquetExport,exportProbeConfig,runExportProbe} from './export_probe.mjs';
const NAME='export-probe-plain.parquet';
function parquet(size=32,footer=8){
    const bytes=new Uint8Array(size);bytes.set([80,65,82,49]);bytes.set([80,65,82,49],size-4);
    new DataView(bytes.buffer).setUint32(size-8,footer,true);return bytes;
}
function memoryFile({bytes=parquet(),infos=[{fileName:NAME,fileSize:bytes.length}],failure}={}){
    const calls=[];
    return {calls,database:{async flushFiles(){calls.push(['flush']);if(failure==='flush')throw new Error('flush failed');},
        async globFiles(name){calls.push(['glob',name]);if(failure==='glob')throw new Error('glob failed');return infos;},
        async copyFileToBuffer(name){calls.push(['copy',name]);if(failure==='copy')throw new Error('copy failed');return bytes;}}};
}
test('inert import and exact same-origin official asset pair require no engine',()=>{
    assert.equal(typeof runExportProbe,'function');assert.equal(EXPORT_PROBE_LIMITS.bytes,64*1024*1024);
    const config=exportProbeConfig({baseURL:'http://127.0.0.1:8767/'});
    assert.ok(Object.isFrozen(config));assert.equal(config.timeoutMs,20000);
    assert.equal(config.workerURL,'http://127.0.0.1:8767/vendor/duckdb/dist/duckdb-browser-mvp.worker.js');
    for(const change of [{moduleURL:'https://elsewhere.invalid/duckdb-mvp.wasm'},{timeoutMs:120001},
        {moduleURL:'./duckdb-coi.wasm'},{moduleURL:'./duckdb-eh.wasm',workerURL:'./duckdb-browser-mvp.worker.js'}])
        assert.throws(()=>exportProbeConfig({baseURL:'http://127.0.0.1:8767/',...change}),TypeError);
});
test('exact private file stat rejects absent/ambiguous/unsafe/oversized sizes',()=>{
    assert.equal(parquetFileSize([{fileName:NAME,fileSize:32}],NAME),32);
    assert.equal(parquetFileSize([{fileName:NAME,fileSize:64*1024*1024}],NAME),64*1024*1024);
    for(const infos of [null,[],[{fileName:'elsewhere.parquet',fileSize:32}],
        [{fileName:NAME}], [{fileName:NAME,fileSize:null}], [{fileName:NAME,fileSize:'32'}],
        [{fileName:NAME,fileSize:NaN}], [{fileName:NAME,fileSize:Infinity}],
        [{fileName:NAME,fileSize:11}], [{fileName:NAME,fileSize:32.5}],
        [{fileName:NAME,fileSize:EXPORT_PROBE_LIMITS.bytes+1}],
        [{fileName:NAME,fileSize:32},{fileName:NAME,fileSize:32}]])
        assert.throws(()=>parquetFileSize(infos,NAME),TypeError);
    for(const name of ['../../source.parquet','/tmp/source','import-1-source.parquet',NAME+'\0','export-probe-*.parquet'])
        assert.throws(()=>parquetFileSize([{fileName:name,fileSize:32}],name),TypeError);
    for(const limit of [0,11,32.5,Infinity,'32',EXPORT_PROBE_LIMITS.bytes+1])
        assert.throws(()=>parquetFileSize([{fileName:NAME,fileSize:32}],NAME,limit),TypeError);
});
test('PAR1 and little-endian footer validation handles offset views without copies',()=>{
    const backing=new Uint8Array(64),bytes=parquet();backing.set(bytes,13);const view=backing.subarray(13,45),before=backing.slice();
    assert.deepEqual(inspectParquet(view),{byteLength:32,footerBytes:8});assert.deepEqual(backing,before);assert.equal(view.buffer,backing.buffer);
    const corrupted=[new Uint8Array(11),parquet(32,0),parquet(32,21),parquet(32,0xffffffff)];
    for(const position of [0,1,2,3,28,29,30,31]){const value=parquet();value[position]=0;corrupted.push(value);}
    for(const value of corrupted)assert.throws(()=>inspectParquet(value),TypeError);
    assert.throws(()=>inspectParquet(parquet(),31),TypeError);
    assert.throws(()=>inspectParquet(new DataView(parquet().buffer)),TypeError);
    assert.throws(()=>inspectParquet(new Uint8Array(new SharedArrayBuffer(32))),TypeError);
    const detached=parquet();structuredClone(detached,{transfer:[detached.buffer]});assert.throws(()=>inspectParquet(detached),TypeError);
});
test('flush and exact-size proof precede binary allocation; successful bytes retain ownership',async()=>{
    const bytes=parquet(),file=memoryFile({bytes}),before=bytes.slice();
    const result=await readParquetExport(file.database,NAME);
    assert.equal(result,bytes);assert.deepEqual(bytes,before);
    assert.deepEqual(file.calls,[['flush'],['glob',NAME],['copy',NAME]]);
});
test('missing/oversized sizes fail closed before requesting any binary buffer',async()=>{
    for(const info of [{fileName:NAME},{fileName:NAME,fileSize:EXPORT_PROBE_LIMITS.bytes+1},{fileName:'wrong.parquet',fileSize:32}]){
        const file=memoryFile({infos:[info]});await assert.rejects(readParquetExport(file.database,NAME),TypeError);
        assert.deepEqual(file.calls,[['flush'],['glob',NAME]]);
    }
    const invalid=memoryFile();await assert.rejects(readParquetExport(invalid.database,'../../private'),TypeError);assert.deepEqual(invalid.calls,[]);
});
test('failed operations and post-copy shape/length mismatch never return partial output',async()=>{
    for(const failure of ['flush','glob','copy']){
        const file=memoryFile({failure});await assert.rejects(readParquetExport(file.database,NAME),new RegExp(failure+' failed'));
        assert.equal(file.calls.length,['flush','glob','copy'].indexOf(failure)+1);
    }
    const sizeMismatch=memoryFile({infos:[{fileName:NAME,fileSize:33}]});
    await assert.rejects(readParquetExport(sizeMismatch.database,NAME),/byte length/);
    const bytes=parquet();bytes[0]=0;const malformed=memoryFile({bytes});
    await assert.rejects(readParquetExport(malformed.database,NAME),/PAR1/);
});
test('without a real browser Worker the proof fails honestly with no SQL or file activity',async()=>{
    assert.equal(typeof globalThis.Worker,'undefined','Node contract must not start a browser Worker');
    const result=await runExportProbe({baseURL:'http://127.0.0.1:8767/'});
    assert.equal(result.ok,false);assert.equal(result.ownedWorkersCreated,0);assert.deepEqual(result.outputs,[]);assert.deepEqual(result.acceptedModes,[]);assert.deepEqual(result.sql,[]);
    assert.equal(result.checks[0].name,'initialize-owned-official-worker');assert.equal(result.checks[0].status,'failed');assert.match(result.checks[0].error.message,/Real browser Worker/);
    assert.equal(result.checks.at(-1).name,'bounded-owned-worker-cleanup');assert.equal(result.checks.at(-1).status,'passed');
    assert.match(result.scope,/No shared C export/);assert.match(result.scope,/does not bound engine/);
});
