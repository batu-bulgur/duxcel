// Pure BP0 bounds/DDL contracts. No fake database/Worker and no graphics.
import test from 'node:test';
import assert from 'node:assert/strict';
import {PERSISTENCE_LIMITS,inspectDuckDBHeader,persistenceProbeConfig,metadataDDL,runPersistenceProbe} from './persistence_probe.mjs';
function file(size=32,version=64n){
    const bytes=new Uint8Array(size);bytes.set([68,85,67,75],8);new DataView(bytes.buffer).setBigUint64(12,version,true);return bytes;
}
const base={baseURL:'http://127.0.0.1:8767/acceptance/'};
test('module import is inert; fixture origin, official assets and bounded timeout are explicit',()=>{
    assert.equal(typeof runPersistenceProbe,'function');
    const config=persistenceProbeConfig({...base,fixtureURL:'native.dxls'});
    assert.equal(config.fixtureURL,'http://127.0.0.1:8767/acceptance/native.dxls');
    assert.equal(config.moduleURL,'http://127.0.0.1:8767/acceptance/vendor/duckdb/dist/duckdb-mvp.wasm');
    assert.equal(config.timeoutMs,20000);assert.ok(Object.isFrozen(config));
    const bytes=file();assert.equal(persistenceProbeConfig({...base,fixtureBytes:bytes}).fixtureBytes,bytes);
    assert.ok(persistenceProbeConfig({...base,moduleURL:'./duckdb-eh.wasm',workerURL:'./duckdb-browser-eh.worker.js',fixtureBytes:bytes}));
});
test('configuration rejects ambiguous, remote or unbounded fixture/toolchain input',()=>{
    for(const change of [{},{fixtureBytes:file(),fixtureURL:'native.dxls'},{fixtureURL:'https://elsewhere.invalid/native.dxls'},
        {fixtureURL:'http://user:pass@127.0.0.1:8767/native.dxls'},{fixtureURL:'blob:http://127.0.0.1:8767/owned-id'},{fixtureBytes:new Uint8Array(0)},
        {fixtureBytes:new Uint8Array(PERSISTENCE_LIMITS.fixtureBytes+1)},
        {fixtureBytes:file(),timeoutMs:120001},{fixtureBytes:file(),moduleURL:'./duckdb-coi.wasm'},
        {fixtureBytes:file(),workerURL:'https://elsewhere.invalid/duckdb-browser-mvp.worker.js'}])
        assert.throws(()=>persistenceProbeConfig({...base,...change}),TypeError);
});
test('DuckDB header reads exact uint64 storage version within an offset view without detaching or copying',()=>{
    const storage=18446744073709551615n,bytes=new Uint8Array(80),part=file(32,storage);bytes.set(part,17);
    const view=bytes.subarray(17,49),before=bytes.slice();
    assert.deepEqual(inspectDuckDBHeader(view),{bytes:32,storageVersion:storage.toString()});
    assert.deepEqual(bytes,before);assert.equal(view.buffer,bytes.buffer);
});
test('invalid, shared or detached file headers fail before an engine can start',()=>{
    assert.throws(()=>inspectDuckDBHeader(new Uint8Array(19)),TypeError);
    const invalid=file();invalid[8]=0;assert.throws(()=>inspectDuckDBHeader(invalid),/DUCK/);
    assert.throws(()=>inspectDuckDBHeader(new DataView(file().buffer)),TypeError);
    assert.throws(()=>inspectDuckDBHeader(new Uint8Array(new SharedArrayBuffer(32))),TypeError);
    const detached=file();structuredClone(detached,{transfer:[detached.buffer]});assert.throws(()=>inspectDuckDBHeader(detached),TypeError);
});
test('known engine-described table DDL is retargeted to the test export with constraints intact',()=>{
    const columns='(sheet_id INTEGER, name VARCHAR, PRIMARY KEY(sheet_id));';
    for(const prefix of ['CREATE TABLE _duxcel_workspace.meta','CREATE TABLE "_duxcel_workspace"."meta"',
        'CREATE TABLE native_fixture._duxcel_workspace.meta','CREATE TABLE "native_fixture"."_duxcel_workspace"."meta"'])
        assert.equal(metadataDDL('meta',prefix+columns),'CREATE TABLE browser_export._duxcel_workspace."meta" '+columns);
    assert.equal(metadataDDL('columns','CREATE TABLE _duxcel_workspace."columns" (name VARCHAR);'),
        'CREATE TABLE browser_export._duxcel_workspace."columns" (name VARCHAR);');
});
test('DDL target mismatches, unknown objects and oversized definitions fail explicitly',()=>{
    for(const [name,sql] of [['not_metadata','CREATE TABLE _duxcel_workspace.not_metadata(i INTEGER);'],
        ['meta','CREATE TABLE other._duxcel_workspace.meta(i INTEGER);'],['meta','CREATE TABLE _duxcel_workspace.sheets(i INTEGER);'],
        ['meta','DROP TABLE _duxcel_workspace.meta;'],['meta','x'.repeat(PERSISTENCE_LIMITS.ddlBytes+1)],['meta',null]])
        assert.throws(()=>metadataDDL(name,sql),TypeError);
});
test('missing browser Worker fails explicitly and cleans up without importing/starting an engine',async()=>{
    assert.equal(typeof globalThis.Worker,'undefined','This pure contract runs only in Node without browser globals');
    const result=await runPersistenceProbe({...base,fixtureBytes:file()});
    assert.equal(result.ok,false);assert.equal(result.ownedWorkersCreated,0);assert.equal(result.export,undefined);
    assert.ok(result.checks.some(check=>check.name==='initialize-owned-official-worker'&&check.status==='failed'&&/Real browser Worker/.test(check.error.message)));
    assert.equal(result.checks.at(-1).name,'bounded-owned-worker-cleanup');assert.equal(result.checks.at(-1).status,'passed');
    assert.deepEqual(result.sql,[]);
});
test('owner fixture mode is explicit and cannot change the default v12 probe configuration',()=>{
    const ordinary=persistenceProbeConfig({...base,fixtureBytes:file()});
    assert.equal(Object.hasOwn(ordinary,'mode'),false);
    const owners=persistenceProbeConfig({...base,fixtureURL:'native.dxls',mode:'owners'});
    assert.equal(owners.mode,'owners');assert.ok(Object.isFrozen(owners));
    assert.equal(owners.fixtureURL,'http://127.0.0.1:8767/acceptance/native.dxls');
    for(const mode of [null,false,true,'v12','13','Owners',{},[],0])
        assert.throws(()=>persistenceProbeConfig({...base,fixtureBytes:file(),mode}),TypeError);
});
test('v13 owner package DDL is allowed only by explicit owner mode with the same table and catalog bounds',()=>{
    for(const table of ['object_counters','format_scopes','format_selectors','format_guards']){
        const columns='(sheet_id INTEGER, PRIMARY KEY(sheet_id));';
        const ddl=`CREATE TABLE native_fixture._duxcel_workspace.${table}${columns}`;
        assert.throws(()=>metadataDDL(table,ddl),TypeError);
        assert.equal(metadataDDL(table,ddl,'owners'),`CREATE TABLE browser_export._duxcel_workspace."${table}" ${columns}`);
        assert.equal(metadataDDL(table,`CREATE TABLE "native_fixture"."_duxcel_workspace"."${table}"${columns}`,'owners'),
            `CREATE TABLE browser_export._duxcel_workspace."${table}" ${columns}`);
        assert.throws(()=>metadataDDL(table,ddl,'v13'),TypeError);
        assert.throws(()=>metadataDDL(table,ddl.replace('native_fixture.','other.'),'owners'),TypeError);
        assert.throws(()=>metadataDDL(table,ddl.replace(`.${table}`,'.meta'),'owners'),TypeError);
        assert.throws(()=>metadataDDL(table,'x'.repeat(PERSISTENCE_LIMITS.ddlBytes+1),'owners'),TypeError);
    }
    assert.throws(()=>metadataDDL('not_metadata','CREATE TABLE _duxcel_workspace.not_metadata (id INTEGER);','owners'),TypeError);
    assert.equal(metadataDDL('meta','CREATE TABLE _duxcel_workspace.meta (version INTEGER);','owners'),
        'CREATE TABLE browser_export._duxcel_workspace."meta" (version INTEGER);');
});
test('owner mode remains inert without a real Worker and never claims accepted metadata on initialization failure',async()=>{
    assert.equal(typeof globalThis.Worker,'undefined','This pure contract starts no browser Worker');
    const result=await runPersistenceProbe({...base,fixtureBytes:file(),mode:'owners'});
    assert.equal(result.mode,'owners');assert.equal(result.ok,false);assert.equal(result.ownedWorkersCreated,0);
    assert.equal(result.metadataVersion,undefined);assert.equal(result.ownerHighWater,undefined);assert.equal(result.export,undefined);
    assert.deepEqual(result.sql,[]);
    assert.ok(result.checks.some(check=>check.name==='initialize-owned-official-worker'&&check.status==='failed'&&/Real browser Worker/.test(check.error.message)));
    assert.equal(result.checks.at(-1).name,'bounded-owned-worker-cleanup');assert.equal(result.checks.at(-1).status,'passed');
    assert.match(result.scope,/owner-only v13/);assert.match(result.scope,/No scoped-format activation/);
});
