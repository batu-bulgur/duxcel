/** BP0 acceptance only: real native .dxls metadata -> official Worker file.
 * Inert until invoked by the integration owner's owned headless browser gate.
 * Copies engine-described native metadata, not a browser Workbook/save codec.
 * No production loader imports or packages this module.
 */
import {probeConfig} from './official_worker_probe.mjs';

const now=()=>globalThis.performance?.now()??Date.now();
export const PERSISTENCE_LIMITS=Object.freeze({fixtureBytes:32*1024*1024,exportBytes:64*1024*1024,
    metadataTables:32,metadataColumns:512,ddlBytes:1024*1024,timeoutMs:20000});
const TABLES=Object.freeze(['cells','columns','filters','formats','marks','meta','objects','pivot_fields',
    'preferences','queries','sheets','sorts','source_options','table_filters','table_sorts','table_sources']);
const encoder=new TextEncoder();
const SENTINELS=Object.freeze({signed_min:'-9223372036854775808',above_js_integer:'9007199254740993',
    max_unsigned:'18446744073709551615',exact_decimal:'9007199254740993.1234567890',
    negative_decimal:'-0.0000000001',missing:null,zero_length:'',literal_null:'NULL',utf8:'İstanbul 😀'});
const SOURCE_FILE='bp0-native-fixture.dxls',EXPORT_FILE='bp0-browser-export.dxls',REOPEN_FILE='bp0-browser-reopen.dxls';
const CATALOGS=new Set(['native_fixture','browser_export','browser_reopen']);
function check(condition,message){if(!condition)throw new Error(message);}
function equal(actual,expected,label){check(actual===expected,`${label}: expected ${JSON.stringify(expected)}, received ${JSON.stringify(actual)}`);}
function quoted(name){return '"'+name.replaceAll('"','""')+'"';}
function errorRecord(error){return {code:error.code??error.name??'ERROR',message:String(error.message??error).slice(0,2048)};}
function ownedBytes(value,label,maximum){
    if(!(value instanceof Uint8Array)||!(value.buffer instanceof ArrayBuffer))throw new TypeError(`${label} must be an owned non-shared Uint8Array`);
    try{new Uint8Array(value.buffer,value.byteOffset,value.byteLength);}catch{throw new TypeError(`${label} is detached`);}
    if(value.byteLength<20||value.byteLength>maximum)throw new TypeError(`${label} must contain 20–${maximum} bytes`);
    return value;
}
export function inspectDuckDBHeader(value,maximum=PERSISTENCE_LIMITS.fixtureBytes){
    const bytes=ownedBytes(value,'DuckDB file',maximum);
    if(bytes[8]!==68||bytes[9]!==85||bytes[10]!==67||bytes[11]!==75)throw new TypeError('Fixture is not a DuckDB file (DUCK header missing)');
    const view=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
    return Object.freeze({bytes:bytes.byteLength,storageVersion:view.getBigUint64(12,true).toString()});
}
export function persistenceProbeConfig(input={}){
    const engine=probeConfig(input),base=new URL(input.baseURL??globalThis.location?.href??import.meta.url);
    const hasBytes=input.fixtureBytes!==undefined,hasURL=input.fixtureURL!==undefined;
    if(hasBytes===hasURL)throw new TypeError('Supply exactly one real native fixtureBytes or fixtureURL');
    let fixtureURL=null;
    if(hasBytes)inspectDuckDBHeader(input.fixtureBytes);
    else{
        fixtureURL=new URL(input.fixtureURL,base);
        if(!['http:','https:'].includes(fixtureURL.protocol)||fixtureURL.origin!==base.origin||fixtureURL.username||fixtureURL.password)throw new TypeError('Native fixture URL must be same-origin HTTP(S)');
        fixtureURL=fixtureURL.href;
    }
    return Object.freeze({moduleURL:engine.moduleURL,workerURL:engine.workerURL,timeoutMs:engine.timeoutMs,
        fixtureURL,fixtureBytes:hasBytes?input.fixtureBytes:null});
}

/** Accept only the known metadata table's engine-derived CREATE TABLE prefix.
 * This is not an SQL sanitizer/importer for arbitrary untrusted databases.
 */
export function metadataDDL(table,sql){
    if(!TABLES.includes(table)||typeof sql!=='string'||encoder.encode(sql).length>PERSISTENCE_LIMITS.ddlBytes)
        throw new TypeError('Invalid native metadata table/DDL');
    const identifier=name=>`(?:${name}|"${name}")`;
    const prefix=new RegExp(`^CREATE TABLE (?:${identifier('native_fixture')}\\.)?${identifier('_duxcel_workspace')}\\.${identifier(table)}\\s*\\(`);
    const match=prefix.exec(sql);
    if(!match)throw new TypeError('Native metadata DDL does not name its expected table');
    return `CREATE TABLE browser_export._duxcel_workspace.${quoted(table)} (`+sql.slice(match[0].length);
}
async function bounded(promise,milliseconds){
    let timer;
    try{return await Promise.race([promise,new Promise((resolve,reject)=>{timer=setTimeout(()=>{
        const error=new Error(`Persistence probe timed out after ${milliseconds} ms`);error.code='PROBE_TIMEOUT';reject(error);
    },milliseconds);})]);}finally{clearTimeout(timer);}
}
async function digest(bytes){return [...new Uint8Array(await globalThis.crypto.subtle.digest('SHA-256',bytes))].map(n=>n.toString(16).padStart(2,'0')).join('');}
async function fetchFixture(url,signal){
    const response=await fetch(url,{signal,credentials:'same-origin',redirect:'error'});
    check(response.ok,`Native fixture HTTP ${response.status}`);
    const length=response.headers.get('content-length');
    if(length!==null)check(/^\d+$/.test(length)&&Number(length)<=PERSISTENCE_LIMITS.fixtureBytes,'Native fixture exceeds 32 MiB');
    check(response.body,'Native fixture response has no readable body');
    const reader=response.body.getReader(),chunks=[];let bytes=0;
    try{
        for(;;){const next=await reader.read();if(next.done)break;
            bytes+=next.value.byteLength;check(bytes<=PERSISTENCE_LIMITS.fixtureBytes,'Native fixture exceeds 32 MiB');chunks.push(next.value);}
    }finally{try{await reader.cancel();}catch{}reader.releaseLock();}
    const result=new Uint8Array(bytes);let offset=0;for(const chunk of chunks){result.set(chunk,offset);offset+=chunk.length;}return result;
}
function cell(table,row,column){const child=table.getChild(column);check(child,`Missing result column ${column}`);return child.get(row);}
function smallText(value,label,maximum=4096){check(typeof value==='string'&&encoder.encode(value).length<=maximum,`Invalid ${label}`);return value;}

export async function runPersistenceProbe(input={}){
    const config=persistenceProbeConfig(input),started=now(),checks=[],sql=[],workers=[],controller=new AbortController();
    let adapter,api,fixture,exported,fixtureBytes,engineVersion=null,failed=false;
    async function step(name,action){
        const begin=now();
        try{const details=await bounded(Promise.resolve().then(action),config.timeoutMs);checks.push({name,status:'passed',milliseconds:now()-begin,details});return details;}
        catch(error){checks.push({name,status:'failed',milliseconds:now()-begin,error:errorRecord(error)});throw error;}
    }
    async function query(text){check(!controller.signal.aborted,'Persistence probe is closed');sql.push(text);return adapter.connection.query(text);}
    async function columns(catalog){
        check(CATALOGS.has(catalog),'Invalid probe catalog');
        const result=await query(`SELECT table_name,column_name,data_type,is_nullable::VARCHAR AS is_nullable,column_index::VARCHAR AS position FROM duckdb_columns() WHERE database_name='${catalog}' AND schema_name='_duxcel_workspace' ORDER BY table_name,column_index LIMIT ${PERSISTENCE_LIMITS.metadataColumns+1}`);
        check(result.numRows>0&&result.numRows<=PERSISTENCE_LIMITS.metadataColumns,'Native metadata column limit exceeded');
        return Array.from({length:result.numRows},(_,row)=>({table:smallText(cell(result,row,'table_name'),'table'),
            name:smallText(cell(result,row,'column_name'),'column'),type:smallText(cell(result,row,'data_type'),'SQL type'),
            nullable:smallText(cell(result,row,'is_nullable'),'column nullability',5),position:smallText(cell(result,row,'position'),'column position')}));
    }
    async function readMetadata(catalog){
        check(CATALOGS.has(catalog),'Invalid probe catalog');
        const meta=await query(`SELECT version,active_sheet::VARCHAR AS active_sheet,user_scale,typeof(version) AS version_type,typeof(active_sheet) AS active_type,typeof(user_scale) AS scale_type FROM ${catalog}._duxcel_workspace.meta LIMIT 2`);
        equal(meta.numRows,1,'Native meta row count');equal(cell(meta,0,'version'),12,'Native workspace version');
        equal(cell(meta,0,'version_type'),'INTEGER','Workspace version SQL type');equal(cell(meta,0,'active_type'),'UBIGINT','Active-sheet SQL type');equal(cell(meta,0,'scale_type'),'DOUBLE','User-scale SQL type');
        const active=cell(meta,0,'active_sheet'),scale=cell(meta,0,'user_scale');
        check(typeof active==='string'&&/^(0|[1-9]\d*)$/.test(active),'Invalid active sheet');check(Number.isFinite(scale)&&scale>=.8&&scale<=1.5,'Invalid user scale');
        const sheets=await query(`SELECT id,name,typeof(id) AS id_type,typeof(name) AS name_type FROM ${catalog}._duxcel_workspace.sheets ORDER BY id LIMIT 65`);
        check(sheets.numRows>0&&sheets.numRows<=64&&BigInt(active)<BigInt(sheets.numRows),'Invalid native sheet count/active sheet');
        const names=Array.from({length:sheets.numRows},(_,row)=>{
            equal(cell(sheets,row,'id'),row,'Ordered sheet ID');equal(cell(sheets,row,'id_type'),'INTEGER','Sheet ID SQL type');equal(cell(sheets,row,'name_type'),'VARCHAR','Sheet name SQL type');
            return smallText(cell(sheets,row,'name'),'sheet name');
        });
        const tables=await query(`SELECT table_name,sql FROM duckdb_tables() WHERE database_name='${catalog}' AND schema_name='_duxcel_workspace' ORDER BY table_name LIMIT ${PERSISTENCE_LIMITS.metadataTables+1}`);
        equal(tables.numRows,TABLES.length,'Native v12 metadata table count');
        let ddlBytes=0;const tableInfo=[];
        for(let row=0;row<tables.numRows;row++){
            const name=cell(tables,row,'table_name'),ddl=smallText(cell(tables,row,'sql'),'table DDL',PERSISTENCE_LIMITS.ddlBytes);
            equal(name,TABLES[row],'Native metadata table name');ddlBytes+=encoder.encode(ddl).length;check(ddlBytes<=PERSISTENCE_LIMITS.ddlBytes,'Native metadata DDL exceeds 1 MiB');
            const count=await query(`SELECT count(*)::VARCHAR AS rows FROM ${catalog}._duxcel_workspace.${quoted(name)}`);
            const rows=smallText(cell(count,0,'rows'),'table row count');check(/^\d+$/.test(rows),'Invalid table row count');
            tableInfo.push({name,sql:ddl,rows});
        }
        return {version:12,activeSheet:active,userScale:scale,sheetNames:names,tables:tableInfo,columns:await columns(catalog)};
    }
    async function sentinels(catalog){
        check(CATALOGS.has(catalog),'Invalid probe catalog');
        const projection=Object.keys(SENTINELS).map(name=>`CAST(${quoted(name)} AS VARCHAR) AS ${quoted(name)}`).join(',');
        const values=await query(`SELECT ${projection} FROM ${catalog}._duxcel_browser_probe.sentinels LIMIT 2`);
        equal(values.numRows,1,'Sentinel row count');const exact={};
        for(const [name,expected] of Object.entries(SENTINELS)){exact[name]=cell(values,0,name);equal(exact[name],expected,`Exact persisted ${name}`);}
        const types=await query(`SELECT column_name,data_type FROM duckdb_columns() WHERE database_name='${catalog}' AND schema_name='_duxcel_browser_probe' AND table_name='sentinels' ORDER BY column_index LIMIT 10`);
        equal(types.numRows,9,'Sentinel column count');const descriptors=Array.from({length:types.numRows},(_,row)=>({name:cell(types,row,'column_name'),type:cell(types,row,'data_type')}));
        equal(descriptors[0].type,'BIGINT','Signed integer type');equal(descriptors[2].type,'UBIGINT','Unsigned integer type');equal(descriptors[3].type,'DECIMAL(38,10)','DECIMAL type');
        return {values:exact,columns:descriptors};
    }
    async function metadataRowsEqual(catalog,tables){
        check(CATALOGS.has(catalog),'Invalid probe catalog');
        for(const {name} of tables){
            const original=`native_fixture._duxcel_workspace.${quoted(name)}`,copy=`${catalog}._duxcel_workspace.${quoted(name)}`;
            const result=await query(`SELECT count(*)::VARCHAR AS differences FROM ((SELECT * FROM ${original} EXCEPT ALL SELECT * FROM ${copy}) UNION ALL (SELECT * FROM ${copy} EXCEPT ALL SELECT * FROM ${original}))`);
            equal(cell(result,0,'differences'),'0',`${catalog} exact metadata rows: ${name}`);
        }
    }
    try{
        await step('bounded-native-file-input',async()=>{
            fixtureBytes=new Uint8Array(config.fixtureBytes??await fetchFixture(config.fixtureURL,controller.signal));
            fixture={...inspectDuckDBHeader(fixtureBytes),sha256:await digest(fixtureBytes)};return fixture;
        });
        await step('initialize-owned-official-worker',async()=>{
            check(typeof globalThis.Worker==='function','Real browser Worker constructor required');
            api=await import('./official_worker.mjs');
            function OwnedWorker(...args){check(!controller.signal.aborted,'Persistence probe is closed');const worker=new globalThis.Worker(...args);workers.push(worker);return worker;}
            adapter=await api.createOfficialWorkerAdapter({...config,WorkerClass:OwnedWorker});engineVersion=adapter.engineVersion;
            if(controller.signal.aborted){await bounded(adapter.close(),2000);throw new Error('Persistence probe initialization finished after its deadline');}
            await query("SET autoinstall_known_extensions=false; SET autoload_known_extensions=false; SET memory_limit='128MB'");
            return {engineVersion,moduleURL:config.moduleURL,workerURL:config.workerURL,threads:1};
        });
        let metadata;
        await step('attach-readonly-real-native-v12',async()=>{
            await adapter.database.registerFileBuffer(SOURCE_FILE,new Uint8Array(fixtureBytes));
            await query(`ATTACH '${SOURCE_FILE}' AS native_fixture (READ_ONLY)`);
            let rejected=false;
            try{await query('UPDATE native_fixture._duxcel_workspace.meta SET version=version');}
            catch(error){check(/read.?only|read only/i.test(String(error.message??error)),'Native write failed for an unrelated reason');rejected=true;}
            check(rejected,'Native fixture attachment permitted a write');
            metadata=await readMetadata('native_fixture');return {readOnlyWriteRejected:true,...metadata};
        });
        await step('create-compatible-file-from-native-metadata-ddl',async()=>{
            // An explicit BUFFER registration keeps the newly created database
            // in the official exportable file registry after DETACH.
            // registerEmptyFileBuffer is a no-op in pinned package 1.32.0.
            await adapter.database.registerFileBuffer(EXPORT_FILE,new Uint8Array());
            await query(`ATTACH '${EXPORT_FILE}' AS browser_export (STORAGE_VERSION 'v1.0.0')`);
            await query('CREATE SCHEMA browser_export._duxcel_workspace');
            await query('BEGIN TRANSACTION');
            try{
                for(const table of metadata.tables){
                    await query(metadataDDL(table.name,table.sql));
                    await query(`INSERT INTO browser_export._duxcel_workspace.${quoted(table.name)} SELECT * FROM native_fixture._duxcel_workspace.${quoted(table.name)}`);
                }
                await query('CREATE SCHEMA browser_export._duxcel_browser_probe');
                await query(`CREATE TABLE browser_export._duxcel_browser_probe.sentinels AS SELECT
                    '-9223372036854775808'::BIGINT AS signed_min,'9007199254740993'::BIGINT AS above_js_integer,
                    '18446744073709551615'::UBIGINT AS max_unsigned,'9007199254740993.1234567890'::DECIMAL(38,10) AS exact_decimal,
                    '-0.0000000001'::DECIMAL(38,10) AS negative_decimal,NULL::VARCHAR AS missing,''::VARCHAR AS zero_length,
                    'NULL'::VARCHAR AS literal_null,'İstanbul 😀'::VARCHAR AS utf8`);
                await query('COMMIT');
            }catch(error){try{await query('ROLLBACK');}catch{}throw error;}
            const copied=await readMetadata('browser_export');
            equal(JSON.stringify(copied.columns),JSON.stringify(metadata.columns),'Copied metadata SQL schema');
            equal(JSON.stringify(copied.tables.map(t=>[t.name,t.rows])),JSON.stringify(metadata.tables.map(t=>[t.name,t.rows])),'Copied metadata table counts');
            equal(JSON.stringify(copied.sheetNames),JSON.stringify(metadata.sheetNames),'Copied sheet names');
            equal(copied.activeSheet,metadata.activeSheet,'Copied active sheet');equal(copied.userScale,metadata.userScale,'Copied user scale');
            await metadataRowsEqual('browser_export',metadata.tables);
            return {storageTarget:'v1.0.0',tables:copied.tables.map(({name,rows})=>({name,rows})),sentinels:await sentinels('browser_export')};
        });
        await step('checkpoint-detach-real-database',async()=>{
            await query('CHECKPOINT browser_export');await query('DETACH browser_export');
            return {checkpointed:true,detached:true};
        });
        await step('flush-worker-files',async()=>{await adapter.database.flushFiles();return {flushed:true};});
        await step('export-real-database-buffer',async()=>{
            const bytes=new Uint8Array(await adapter.database.copyFileToBuffer(EXPORT_FILE));
            const header=inspectDuckDBHeader(bytes,PERSISTENCE_LIMITS.exportBytes);
            exported={filename:EXPORT_FILE,bytes,byteLength:header.bytes,storageVersion:header.storageVersion,sha256:await digest(bytes)};
            await adapter.database.dropFile(EXPORT_FILE);
            return {filename:EXPORT_FILE,bytes:bytes.byteLength,storageVersion:exported.storageVersion,sha256:exported.sha256};
        });
        await step('reopen-export-and-check-native-fixture-unchanged',async()=>{
            await adapter.database.registerFileBuffer(REOPEN_FILE,new Uint8Array(exported.bytes));
            await query(`ATTACH '${REOPEN_FILE}' AS browser_reopen (READ_ONLY)`);
            const reopened=await readMetadata('browser_reopen'),exact=await sentinels('browser_reopen');
            equal(JSON.stringify(reopened.columns),JSON.stringify(metadata.columns),'Reopened metadata schema');
            equal(JSON.stringify(reopened.tables.map(t=>[t.name,t.rows])),JSON.stringify(metadata.tables.map(t=>[t.name,t.rows])),'Reopened metadata row counts');
            await metadataRowsEqual('browser_reopen',metadata.tables);
            await query('DETACH browser_reopen');await adapter.database.dropFile(REOPEN_FILE);
            await query('DETACH native_fixture');
            const original=await adapter.database.copyFileToBuffer(SOURCE_FILE);equal(await digest(original),fixture.sha256,'Read-only native fixture SHA-256');
            await adapter.database.dropFile(SOURCE_FILE);return {readOnlyNativeUnchanged:true,sentinels:exact};
        });
    }catch{failed=true;}
    finally{
        controller.abort();
        const begin=now();
        try{if(adapter)await bounded(adapter.close(),2000);checks.push({name:'bounded-owned-worker-cleanup',status:'passed',milliseconds:now()-begin});}
        catch(error){failed=true;checks.push({name:'bounded-owned-worker-cleanup',status:'failed',milliseconds:now()-begin,error:errorRecord(error)});}
        finally{for(const worker of workers)worker.terminate();}
    }
    return {ok:!failed,engineVersion,fixture,export:exported,checks,sql,milliseconds:now()-started,ownedWorkersCreated:workers.length,
        scope:'BP0 official Worker/native-v12 file compatibility only: metadata DDL/rows copied from an actual native fixture, exact sentinel types persisted and exported. Native workbook_load readback is a separate harness gate. No shared C save codec/controller, source relink, OPFS recovery or browser Save acceptance.'};
}
