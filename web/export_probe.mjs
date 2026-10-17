/** Test-only pinned official-Worker Parquet virtual-file capability proof.
 * Inert until explicitly invoked by the desktop owner's isolated runner.
 * Fixture SQL below is acceptance data, not a product export SQL builder.
 * No product loader imports this module; no download, OPFS or OS path is used.
 */
import {probeConfig} from './official_worker_probe.mjs';

export const EXPORT_PROBE_LIMITS=Object.freeze({bytes:64*1024*1024,rows:600,timeoutMs:20000});
const encoder=new TextEncoder(),now=()=>globalThis.performance?.now()??Date.now();
const SOURCE='export-probe-source.csv';
const MODES=Object.freeze(['plain','buffer']);
const SENTINELS=Object.freeze({signed_min:'-9223372036854775808',above_js_integer:'9007199254740993',
    max_unsigned:'18446744073709551615',exact_decimal:'9007199254740993.1234567890',
    negative_decimal:'-0.0000000001',missing:null,zero_length:'',literal_null:'NULL',
    utf8:'İstanbul 😀',punctuation:'apostrophe\' "quote"\ttab\nnewline',marker:'original'});
const TYPES=Object.freeze({row_id:'BIGINT',signed_min:'BIGINT',above_js_integer:'BIGINT',max_unsigned:'UBIGINT',
    exact_decimal:'DECIMAL(38,10)',negative_decimal:'DECIMAL(38,10)',missing:'VARCHAR',zero_length:'VARCHAR',
    literal_null:'VARCHAR',utf8:'VARCHAR',punctuation:'VARCHAR',marker:'VARCHAR'});
function check(value,message){if(!value)throw new Error(message);}
function equal(actual,expected,label){check(actual===expected,`${label}: expected ${JSON.stringify(expected)}, received ${JSON.stringify(actual)}`);}
function errorRecord(error){return {code:error.code??error.name??'ERROR',message:String(error.message??error).slice(0,2048)};}
function maximum(value){if(!Number.isSafeInteger(value)||value<12||value>EXPORT_PROBE_LIMITS.bytes)throw new TypeError('Parquet output limit must be between 12 bytes and 64 MiB');return value;}
function privateName(name){if(typeof name!=='string'||!/^export-probe-(plain|buffer)\.parquet$/.test(name))throw new TypeError('Use only the probe-owned private Parquet basename');return name;}
function quoted(name){return '"'+name.replaceAll('"','""')+'"';}
function literal(text){return "'"+text.replaceAll("'","''")+"'";}

/** Validate the exact private file's declared size before requesting its bytes.
 * Optional/missing size is not sufficient evidence for bounded transport.
 */
export function parquetFileSize(infos,name,limit=EXPORT_PROBE_LIMITS.bytes){
    privateName(name);maximum(limit);
    if(!Array.isArray(infos)||infos.length!==1||infos[0]?.fileName!==name||
        !Number.isSafeInteger(infos[0].fileSize)||infos[0].fileSize<12||infos[0].fileSize>limit)
        throw new TypeError('Expected one exact private Parquet file with a known bounded size');
    return infos[0].fileSize;
}
/** Structural bounds only. The real reopened typed SQL checks prove validity. */
export function inspectParquet(bytes,limit=EXPORT_PROBE_LIMITS.bytes){
    maximum(limit);
    if(!(bytes instanceof Uint8Array)||!(bytes.buffer instanceof ArrayBuffer))throw new TypeError('Parquet bytes must be an owned non-shared Uint8Array');
    try{new Uint8Array(bytes.buffer,bytes.byteOffset,bytes.byteLength);}catch{throw new TypeError('Parquet bytes are detached');}
    const size=bytes.byteLength;
    if(size<12||size>limit)throw new TypeError('Parquet file exceeds its bounded output size');
    for(const offset of [0,size-4])if(bytes[offset]!==80||bytes[offset+1]!==65||bytes[offset+2]!==82||bytes[offset+3]!==49)
        throw new TypeError('Parquet PAR1 header/footer missing');
    const footerBytes=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength).getUint32(size-8,true);
    if(!footerBytes||footerBytes>size-12)throw new TypeError('Parquet footer length is outside the owned file');
    return Object.freeze({byteLength:size,footerBytes});
}
/** Caller serializes the engine and owns private-file cleanup on every path.
 * The pinned official Worker may copy internally; this bounds transport only.
 */
export async function readParquetExport(database,name,limit=EXPORT_PROBE_LIMITS.bytes){
    privateName(name);maximum(limit);
    await database.flushFiles();
    const size=parquetFileSize(await database.globFiles(name),name,limit);
    const bytes=await database.copyFileToBuffer(name),header=inspectParquet(bytes,limit);
    equal(header.byteLength,size,'Copied Parquet byte length');
    return bytes;
}
export function exportProbeConfig(input={}){
    const config=probeConfig(input);
    for(const value of [config.moduleURL,config.workerURL]){const url=new URL(value);if(url.username||url.password)throw new TypeError('Official assets cannot contain credentials');}
    return Object.freeze({moduleURL:config.moduleURL,workerURL:config.workerURL,timeoutMs:config.timeoutMs});
}
async function bounded(promise,milliseconds){
    let timer;
    try{return await Promise.race([promise,new Promise((resolve,reject)=>{timer=setTimeout(()=>{
        const error=new Error(`Parquet probe timed out after ${milliseconds} ms`);error.code='PROBE_TIMEOUT';reject(error);
    },milliseconds);})]);}finally{clearTimeout(timer);}
}
async function digest(bytes){return [...new Uint8Array(await globalThis.crypto.subtle.digest('SHA-256',bytes))].map(n=>n.toString(16).padStart(2,'0')).join('');}
function cell(table,row,column){const child=table.getChild(column);check(child,`Missing probe result ${column}`);return child.get(row);}
function fixtureSQL(){
    const values=Object.entries(SENTINELS).filter(([name])=>name!=='marker').map(([name,value])=>`${value===null?'NULL':literal(value)}::${TYPES[name]} AS ${quoted(name)}`);
    // Only the marker comes from the separately registered source. Every other
    // sentinel remains explicitly typed; no JS numeric conversion occurs.
    return `SELECT i::BIGINT AS row_id,${values.join(',')},marker FROM range(${EXPORT_PROBE_LIMITS.rows}) t(i) CROSS JOIN read_csv('${SOURCE}',header=true,columns={'marker':'VARCHAR'}) ORDER BY i`;
}

export async function runExportProbe(input={}){
    const config=exportProbeConfig(input),started=now(),checks=[],sql=[],workers=[],outputs=[];
    let adapter,failed=false,closed=false,engineVersion=null,sourceHash;
    async function step(name,operation){
        const begin=now();
        try{const details=await bounded(Promise.resolve().then(operation),config.timeoutMs);checks.push({name,status:'passed',milliseconds:now()-begin,details});return details;}
        catch(error){checks.push({name,status:'failed',milliseconds:now()-begin,error:errorRecord(error)});throw error;}
    }
    async function query(text){check(!closed,'Parquet probe is closed');check(encoder.encode(text).length<=1024*1024,'Probe SQL exceeds 1 MiB');sql.push(text);return adapter.connection.query(text);}
    async function absent(name){equal((await adapter.database.globFiles(name)).length,0,`Private file removed: ${name}`);}
    async function verify(name){
        const relation=`read_parquet('${name}')`,description=await query(`DESCRIBE SELECT * FROM ${relation}`);
        equal(description.numRows,Object.keys(TYPES).length,'Reopened Parquet column count');
        const columns=Array.from({length:description.numRows},(_,r)=>[cell(description,r,'column_name'),cell(description,r,'column_type')]);
        equal(JSON.stringify(columns),JSON.stringify(Object.entries(TYPES)),'Reopened exact Parquet SQL types');
        const projected=Object.keys(TYPES).map(name=>`CAST(${quoted(name)} AS VARCHAR) AS ${quoted(name)}`).join(',');
        const selected=await query(`SELECT ${projected} FROM ${relation} WHERE row_id IN (0,257,599) ORDER BY row_id`);
        equal(selected.numRows,3,'Reopened first/deep/last rows');
        for(let r=0;r<3;r++){
            equal(cell(selected,r,'row_id'),String([0,257,599][r]),'Exact row ID');
            for(const [name,value] of Object.entries(SENTINELS))equal(cell(selected,r,name),value,`Reopened ${name} row ${r}`);
        }
        const totals=await query(`SELECT COUNT(*)::VARCHAR AS rows,COUNT(missing)::VARCHAR AS present,COUNT(DISTINCT row_id)::VARCHAR AS distinct_rows FROM ${relation}`);
        equal(cell(totals,0,'rows'),'600','Full export row count');equal(cell(totals,0,'present'),'0','SQL NULL remains missing');equal(cell(totals,0,'distinct_rows'),'600','Every row survives');
        return {rows:'600',columns,selectedRows:['0','257','599'],exactSentinels:true};
    }
    try{
        await step('initialize-owned-official-worker',async()=>{
            check(typeof globalThis.Worker==='function','Real browser Worker constructor required');
            const api=await import('./official_worker.mjs');
            function OwnedWorker(...args){const worker=new globalThis.Worker(...args);workers.push(worker);return worker;}
            adapter=await api.createOfficialWorkerAdapter({...config,WorkerClass:OwnedWorker});
            engineVersion=adapter.engineVersion;check(typeof engineVersion==='string'&&engineVersion,'Official engine version missing');
            return {engineVersion,moduleURL:config.moduleURL,workerURL:config.workerURL};
        });
        await step('register-original-source',async()=>{
            const bytes=encoder.encode('marker\noriginal\n');sourceHash=await digest(bytes);
            await adapter.registerFile(SOURCE,bytes);return {name:SOURCE,bytes:bytes.length,sha256:sourceHash};
        });
        for(const mode of MODES){
            const name=`export-probe-${mode}.parquet`,reopen=`export-probe-reopen-${mode}.parquet`;
            let exported,candidateError,cleanupError;
            const begin=now();
            try{
                await absent(name);await absent(reopen);
                if(mode==='buffer')await adapter.registerFile(name,new Uint8Array(0));
                const copied=await query(`COPY (${fixtureSQL()}) TO '${name}' (FORMAT PARQUET)`);
                equal(copied.numRows,1,'COPY affected-row scalar');equal(String(cell(copied,0,'Count')),'600','COPY exported full relation');
                const bytes=await readParquetExport(adapter.database,name),header=inspectParquet(bytes);
                exported={mode,filename:name,bytes,byteLength:header.byteLength,footerBytes:header.footerBytes,sha256:await digest(bytes)};
                // Drop the generated file before reopening independently owned
                // bytes under a fresh immutable registration name.
                await adapter.dropFile(name);await absent(name);
                await adapter.registerFile(reopen,new Uint8Array(bytes));
                exported.readback=await verify(reopen);
            }catch(error){candidateError=error;}
            finally{
                try{
                    for(const file of [reopen,name]){
                        const matches=await bounded(adapter.database.globFiles(file),2000);
                        if(matches.length)await bounded(adapter.dropFile(file),2000);
                        await bounded(absent(file),2000);
                    }
                }catch(error){cleanupError=error;}
            }
            checks.push({name:`parquet-${mode}-candidate`,status:candidateError?'unsupported':'passed',milliseconds:now()-begin,
                ...(candidateError?{error:errorRecord(candidateError)}:{details:{...exported,bytes:undefined}})});
            checks.push({name:`parquet-${mode}-private-cleanup`,status:cleanupError?'failed':'passed',...(cleanupError?{error:errorRecord(cleanupError)}:{})});
            if(cleanupError)throw cleanupError;
            if(candidateError?.code==='PROBE_TIMEOUT')throw candidateError;
            if(!candidateError)outputs.push(exported);
        }
        await step('original-source-unchanged',async()=>{
            const original=await adapter.database.copyFileToBuffer(SOURCE);equal(await digest(original),sourceHash,'Original registered source SHA-256');
            const table=await query(`SELECT marker FROM read_csv('${SOURCE}',header=true,columns={'marker':'VARCHAR'})`);
            equal(table.numRows,1,'Original source row count');equal(cell(table,0,'marker'),'original','Original source value');
            await adapter.dropFile(SOURCE);await absent(SOURCE);return {sha256:sourceHash,rows:'1',value:'original'};
        });
        await step('at-least-one-accepted-file-mode',()=>{check(outputs.length>0,'Neither pinned virtual Parquet file creation mode is supported');return {acceptedModes:outputs.map(value=>value.mode)};});
    }catch{failed=true;}
    finally{
        closed=true;const begin=now();
        try{if(adapter)await bounded(adapter.close(),2000);checks.push({name:'bounded-owned-worker-cleanup',status:'passed',milliseconds:now()-begin});}
        catch(error){failed=true;checks.push({name:'bounded-owned-worker-cleanup',status:'failed',milliseconds:now()-begin,error:errorRecord(error)});}
        finally{for(const worker of workers)worker.terminate();}
    }
    return {ok:!failed,engineVersion,outputs,acceptedModes:outputs.map(value=>value.mode),checks,sql,milliseconds:now()-started,
        ownedWorkersCreated:workers.length,limits:EXPORT_PROBE_LIMITS,
        scope:'Pinned official Worker Parquet virtual-file capability only: typed fixture COPY, known-size bounded binary copy, cleanup and fresh exact reimport. No shared C export SQL/controller, download, OPFS or product export acceptance. Output transfer size does not bound engine/process memory during COPY.'};
}
