/**
 * Transport adapter for the official @duckdb/duckdb-wasm 1.32.0 async Worker.
 * C supplies SQL (including LIMIT/OFFSET/transforms); this module never builds it.
 * Replies own their UTF-8/offset/null buffers. C must copy these into its own
 * allocator before publishing a Page, then release the transport buffers.
 *
 * Source contracts: package parallel/async_connection.{send,cancelSent},
 * bindings/config.{arrowLosslessConversion,query}, Arrow 17 visitor/get.ts.
 * Temporal/complex Arrow getters can be lossy: unsupported types fail explicitly.
 * C may instead project canonical VARCHAR in SQL and supply original metadata.
 */
import { DataType } from 'apache-arrow';

export const LIMITS = Object.freeze({ rows: 256, columns: 256, bytes: 4 * 1024 * 1024,
    copyCells:20000,sqlBytes: 1024 * 1024, fileBytes: 256 * 1024 * 1024, fileNames: 1024, sourceBindings: 33 });
// Bounds transferred output, not COPY's private engine file or process peak.
export const EXPORT_LIMITS=Object.freeze({bytes:64*1024*1024});
const MAX_U64 = 18446744073709551615n;
const encoder = new TextEncoder();
const decoder = new TextDecoder('utf-8', { fatal: true });
const NUMBER_KINDS = new Set(['none', 'i8', 'i16', 'i32', 'i64', 'i128', 'u8', 'u16', 'u32', 'u64', 'u128', 'decimal', 'float']);
// Reconnecting through another adapter must not reset this database object's
// reservation history or session tokens. These describe only immutable buffers
// registered through this API: raw engine writes, distinct wrappers around the
// same underlying database and persistent/content identity remain unsupported.
const fileNameRegistries = new WeakMap();
let nextDatabaseEpoch = 1n;

export class BridgeError extends Error {
    constructor(code, message) { super(message); this.name = 'BridgeError'; this.code = code; }
}
function check(condition, code, message) { if (!condition) throw new BridgeError(code, message); }
function boundedText(value, limit, name) {
    check(typeof value === 'string' && value.length <= limit && encoder.encode(value).byteLength <= limit,
        'INVALID_REQUEST', `${name} exceeds its UTF-8 byte limit`);
    return value;
}
function u64(value, name) {
    check(typeof value === 'string' && /^(0|[1-9][0-9]{0,19})$/.test(value) && BigInt(value) <= MAX_U64,
        'INVALID_REQUEST', `${name} must be an exact unsigned 64-bit decimal string`);
    return value;
}
function sourceBindings(value) {
    // Explicit caller-owned dependencies only. Parsing these tokens neither
    // inspects SQL nor certifies that the caller listed every dependency.
    check(Array.isArray(value) && value.length > 0 && value.length <= LIMITS.sourceBindings,
        'INVALID_REQUEST', 'Explicit source bindings require 1–33 registered sources');
    const names = new Set();
    return Object.freeze(Array.from(value, binding => {
        check(binding && typeof binding === 'object', 'INVALID_REQUEST', 'Source binding required');
        const name = virtualName(binding.name);
        const engineEpoch = u64(binding.engineEpoch, 'Source engine epoch');
        const registrationId = u64(binding.registrationId, 'Source registration ID');
        check(engineEpoch !== '0' && registrationId !== '0', 'INVALID_REQUEST', 'Source tokens must be nonzero');
        check(!names.has(name), 'INVALID_REQUEST', 'Duplicate source binding name'); names.add(name);
        return Object.freeze({ name, engineEpoch, registrationId });
    }));
}
function newFileNameRegistry() {
    check(nextDatabaseEpoch <= MAX_U64, 'FILE_REGISTRY_LIMIT', 'Source engine epochs are exhausted. Start a new browser session.');
    return { epoch: String(nextDatabaseEpoch++), nextId: 1n, names: new Map(), closed: false };
}
function descriptor(value) {
    const result = { name: boundedText(value.name, 4096, 'Column name'),
        typeName: boundedText(value.typeName, 128, 'SQL type'), numberKind: value.numberKind ?? 'none',
        precision: value.precision ?? 0, scale: value.scale ?? 0 };
    check(NUMBER_KINDS.has(result.numberKind) && Number.isInteger(result.precision) && result.precision >= 0 && result.precision <= 76 &&
        Number.isInteger(result.scale) && Math.abs(result.scale) <= 76, 'INVALID_REQUEST', 'Invalid exact numeric column metadata');
    return Object.freeze(result);
}
export function pageRequest(value) {
    check(value && typeof value === 'object', 'INVALID_REQUEST', 'Page request required');
    const request = {};
    for (const name of ['requestId', 'generation', 'documentId', 'sourceId', 'objectId', 'revision', 'offset']) {
        request[name] = u64(value[name] ?? (name === 'requestId' ? undefined : '0'), name);
    }
    check(request.requestId !== '0', 'INVALID_REQUEST', 'requestId must be nonzero and increase within an adapter');
    request.sql = boundedText(value.sql, LIMITS.sqlBytes, 'C-built SQL');
    check(request.sql.trim() && !request.sql.includes('\0'), 'INVALID_REQUEST', 'Nonempty SQL without embedded NUL required');
    check(value.purpose===undefined||value.purpose==='copy','INVALID_REQUEST','Unknown query purpose');
    if(value.purpose==='copy')request.purpose='copy';
    request.rowLimit = value.rowLimit ?? LIMITS.rows;
    const maxRows=request.purpose==='copy'?LIMITS.copyCells:LIMITS.rows;
    check(Number.isInteger(request.rowLimit) && request.rowLimit > 0 && request.rowLimit <= maxRows, 'INVALID_REQUEST', `Page row limit must be 1–${maxRows}`);
    request.knownTotal = value.knownTotal == null ? null : u64(value.knownTotal, 'knownTotal');
    if (value.columns != null) {
        check(Array.isArray(value.columns) && value.columns.length <= LIMITS.columns, 'INVALID_REQUEST', 'Too many requested columns');
        request.columns = Object.freeze(value.columns.map(descriptor));
        check(request.purpose!=='copy'||request.rowLimit*request.columns.length<=LIMITS.copyCells,'INVALID_REQUEST','Copy projection exceeds 20,000 cells');
    }
    // Absence keeps the existing unbound query contract. Explicit malformed
    // bindings must never silently downgrade to an unbound request.
    if (value.sourceBindings !== undefined) request.sourceBindings = sourceBindings(value.sourceBindings);
    return Object.freeze(request);
}
// A trusted active workbook cursor selects this separate policy. Ordinary
// submit/pageRequest never admit this purpose or raise their 256-row bound.
// Four columns and one registered immutable archive cover only secondary sorts.
export function workbookSortsRequest(value) {
    check(value?.purpose==='workbook-sorts','INVALID_REQUEST','Explicit workbook-sorts request required');
    const request=pageRequest({...value,purpose:undefined,rowLimit:1});
    const rows=value.rowLimit??449;
    check(Number.isInteger(rows)&&rows>0&&rows<=449&&request.offset==='0'&&request.knownTotal===null,
        'INVALID_REQUEST','Workbook sorts require offset zero, unknown total and at most449 rows');
    check(request.columns?.length===4&&request.sourceBindings?.length===1,
        'INVALID_REQUEST','Workbook sorts require four typed columns and one captured archive binding');
    return Object.freeze({...request,purpose:'workbook-sorts',rowLimit:rows});
}
export function exportRequest(value) {
    const request=pageRequest(value);
    check(value.rowLimit===1 && value.offset==='0' && value.knownTotal==='1' && value.columns===undefined && value.purpose===undefined,
        'INVALID_EXPORT','Parquet export requires one unsupplied Count column at offset zero');
    for(const field of ['requestId','documentId'])check(BigInt(request[field])>0n && BigInt(request[field])<=4294967295n,
        'INVALID_EXPORT','Parquet export cookies and tickets must be nonzero uint32 values');
    const fileName=`export-${request.documentId}-${request.requestId}.parquet`;
    check(value.fileName===fileName,'INVALID_EXPORT','Parquet export must use its own private cookie/ticket basename');
    return Object.freeze({...request,fileName});
}
export function inspectParquetExport(bytes,expectedSize=null) {
    check(bytes instanceof Uint8Array && bytes.buffer instanceof ArrayBuffer,'INVALID_EXPORT','Parquet export requires an owned non-shared byte buffer');
    try{new Uint8Array(bytes.buffer,bytes.byteOffset,bytes.byteLength);}catch{throw new BridgeError('INVALID_EXPORT','Parquet export bytes are detached');}
    const size=bytes.byteLength;
    check(size>=12 && size<=EXPORT_LIMITS.bytes && (expectedSize===null || Number.isSafeInteger(expectedSize) && expectedSize===size),
        'EXPORT_LIMIT','Parquet export exceeds 64 MiB or disagrees with its exact file size');
    for(const offset of [0,size-4])check(bytes[offset]===80 && bytes[offset+1]===65 && bytes[offset+2]===82 && bytes[offset+3]===49,
        'INVALID_EXPORT','Parquet PAR1 header/footer missing');
    const footerBytes=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength).getUint32(size-8,true);
    check(footerBytes>0 && footerBytes<=size-12,'INVALID_EXPORT','Parquet footer lies outside the owned file');
    return {byteLength:size,footerBytes};
}
function identity(request) {
    return Object.fromEntries(['requestId', 'generation', 'documentId', 'sourceId', 'objectId', 'revision', 'offset'].map(name => [name, request[name]]));
}

function metadata(field, supplied) {
    boundedText(field.name, 4096, 'Result field name');
    let type = field.type;
    if (DataType.isDictionary(type)) type = type.dictionary;
    let result;
    if (DataType.isInt(type)) {
        const names = type.isSigned ? { 8: 'TINYINT', 16: 'SMALLINT', 32: 'INTEGER', 64: 'BIGINT' } :
            { 8: 'UTINYINT', 16: 'USMALLINT', 32: 'UINTEGER', 64: 'UBIGINT' };
        result = { typeName: names[type.bitWidth], numberKind: `${type.isSigned ? 'i' : 'u'}${type.bitWidth}`, precision: 0, scale: 0 };
    } else if (DataType.isDecimal(type)) {
        result = { typeName: `DECIMAL(${type.precision},${type.scale})`, numberKind: 'decimal', precision: type.precision, scale: type.scale };
    } else if (DataType.isFloat(type)) result = { typeName: type.precision === 2 ? 'DOUBLE' : 'FLOAT', numberKind: 'float', precision: 0, scale: 0 };
    else if (DataType.isUtf8(type) || DataType.isLargeUtf8(type)) result = { typeName: 'VARCHAR', numberKind: 'none', precision: 0, scale: 0 };
    else if (DataType.isBool(type)) result = { typeName: 'BOOLEAN', numberKind: 'none', precision: 0, scale: 0 };
    else if (DataType.isNull(type)) result = { typeName: 'SQLNULL', numberKind: 'none', precision: 0, scale: 0 };
    else throw new BridgeError('UNSUPPORTED_TYPE', `Arrow ${type} requires a C-built canonical VARCHAR projection`);
    if (supplied) check(supplied.name === field.name, 'SCHEMA_CHANGED', 'Result field names disagree with the C-owned schema');
    return { name: field.name, ...result, ...supplied, arrowType: String(field.type), nullable: field.nullable,
        sqlTypeProvided: Boolean(supplied), numeric: (supplied?.numberKind ?? result.numberKind) !== 'none' };
}

/** Signed little-endian decimal words -> fixed-scale text, with no Number cast. */
export function decimalText(words, scale) {
    check(words instanceof Uint32Array && (words.length === 4 || words.length === 8) && Number.isInteger(scale) && Math.abs(scale) <= 76,
        'UNSUPPORTED_TYPE', 'Expected a 128/256-bit Arrow decimal and bounded scale');
    let integer = 0n;
    for (let index = words.length - 1; index >= 0; index--) integer = (integer << 32n) | BigInt(words[index]);
    if (words[words.length - 1] & 0x80000000) integer -= 1n << BigInt(words.length * 32);
    const negative = integer < 0n;
    let digits = (negative ? -integer : integer).toString();
    if (scale > 0) {
        digits = digits.padStart(scale + 1, '0');
        digits = `${digits.slice(0, -scale)}.${digits.slice(-scale)}`;
    } else if (scale < 0) digits += '0'.repeat(-scale);
    return (negative ? '-' : '') + digits;
}
function valueText(value, fieldType) {
    const type = DataType.isDictionary(fieldType) ? fieldType.dictionary : fieldType;
    if (DataType.isDecimal(type)) return decimalText(value, type.scale);
    if (DataType.isInt(type)) {
        check(type.bitWidth === 64 ? typeof value === 'bigint' : Number.isSafeInteger(value), 'LOSSY_VALUE', 'Integer result was converted through an unsafe JavaScript number');
        return value.toString();
    }
    if (DataType.isFloat(type)) {
        check(typeof value === 'number', 'LOSSY_VALUE', 'Expected Arrow floating value');
        return Object.is(value, -0) ? '-0' : value.toString();
    }
    if (DataType.isBool(type)) { check(typeof value === 'boolean', 'LOSSY_VALUE', 'Expected Arrow boolean'); return value ? 'true' : 'false'; }
    check(typeof value === 'string', 'UNSUPPORTED_TYPE', 'Expected canonical Arrow text');
    return value;
}

/** Consume streaming Arrow batches into one owned bounded, row-major page.
 * Caller closes the reader; DuckDBWorkerAdapter does so on every path.
 */
export async function encodePage(reader, request, { obsolete = () => false } = {}) {
    if (reader.open) await reader.open({ autoDestroy: false }); // official send() returns a lazy Arrow IPC reader
    const fields = reader.schema?.fields;
    check(Array.isArray(fields) && fields.length <= LIMITS.columns, 'PAGE_LIMIT', 'Page exceeds 256 columns');
    check(request.purpose!=='workbook-sorts'||fields.length===4,'PAGE_LIMIT','Workbook sorts require exactly four result columns');
    check(!request.columns || request.columns.length === fields.length, 'SCHEMA_CHANGED', 'Result column count disagrees with the C-owned schema');
    const columns = fields.map((field, index) => metadata(field, request.columns?.[index]));
    let bytes = encoder.encode(JSON.stringify(columns)).byteLength, rows = 0;
    const chunks = [], nullValues = [], starts = [0];
    let textBytes = 0;
    for await (const batch of reader) {
        check(!obsolete(), 'SUPERSEDED', 'Request was superseded');
        check(batch.numCols === fields.length && rows + batch.numRows <= request.rowLimit, 'PAGE_LIMIT', 'C-built page SQL returned more than its requested bounded rows');
        check(request.purpose!=='workbook-sorts'||rows+batch.numRows<=449,'PAGE_LIMIT','Workbook sorts exceed449 rows');
        check(request.purpose!=='copy'||fields.length*(rows+batch.numRows)<=LIMITS.copyCells,'PAGE_LIMIT','Copy result exceeds 20,000 cells');
        for (let row = 0; row < batch.numRows; row++) for (let column = 0; column < fields.length; column++) {
            const vector = batch.getChildAt(column), value = vector.get(row);
            const isNull = value === null;
            nullValues.push(isNull ? 1 : 0);
            if (!isNull) {
                const text = valueText(value, fields[column].type);
                check(text.length <= LIMITS.bytes, 'PAGE_LIMIT', 'Cell text exceeds the page budget');
                const encoded = encoder.encode(text);
                bytes += encoded.byteLength;
                check(bytes + (nullValues.length + 1) * 5 <= LIMITS.bytes, 'PAGE_LIMIT', 'Page exceeds its 4 MiB byte budget');
                chunks.push(encoded); textBytes += encoded.byteLength;
            }
            starts.push(textBytes);
        }
        rows += batch.numRows;
    }
    check(!obsolete(), 'SUPERSEDED', 'Request was superseded');
    check(bytes + (starts.length * 4) + nullValues.length <= LIMITS.bytes, 'PAGE_LIMIT', 'Page metadata exceeds its byte budget');
    const text = new Uint8Array(textBytes);
    let position = 0;
    for (const chunk of chunks) { text.set(chunk, position); position += chunk.length; }
    const observed = BigInt(request.offset) + BigInt(rows);
    check(observed <= MAX_U64, 'PAGE_LIMIT', 'Observed row position overflows uint64');
    return { schema: 'duxcel.page', version: 1, ...identity(request), columns, rowCount: rows, columnCount: columns.length,
        total: request.knownTotal ?? (rows || request.offset === '0' ? observed.toString() : '0'),
        countPending: request.knownTotal === null && (rows === request.rowLimit || (request.offset !== '0' && rows === 0)),
        text, offsets: Uint32Array.from(starts), nulls: Uint8Array.from(nullValues) };
}
export function pageTransferables(page) { return [page.text.buffer, page.offsets.buffer, page.nulls.buffer]; }
export function pageCellText(page, row, column) {
    const index = row * page.columnCount + column;
    check(Number.isInteger(row) && Number.isInteger(column) && row >= 0 && row < page.rowCount && column >= 0 && column < page.columnCount, 'INVALID_CELL', 'Cell is outside this page');
    return page.nulls[index] ? null : decoder.decode(page.text.subarray(page.offsets[index], page.offsets[index + 1]));
}
function failure(request, status, error) {
    return { ...identity(request), status, ...(error ? { error: { code: error instanceof BridgeError ? error.code : 'QUERY_FAILED', message: String(error.message ?? error).slice(0, 2048) } } : {}) };
}
function virtualName(name) {
    boundedText(name, 255, 'Virtual filename');
    check(name && name !== '.' && name !== '..' && !/[\\/\0]/.test(name), 'INVALID_FILE', 'Use a virtual basename, not an OS path');
    return name;
}

export class DuckDBWorkerAdapter {
    // Registration names are session generation identities. Never recycle a
    // retired/failed name: older C relations may still refer to that basename.
    // This protects this API's registered buffers, not arbitrary SQL writes,
    // remote/custom SQL dependencies, or persistent source identity.
    #fileRegistry;
    #reserveFileName(name, registration) {
        const registry = this.#fileRegistry;
        check(!registry.closed, 'NOT_AVAILABLE', 'This source engine session has closed');
        if (registry.names.has(name)) {
            check(!registration, 'IMMUTABLE_FILE', 'This source name was already used in this engine session. Import with a fresh virtual name.');
            return registry.names.get(name);
        }
        check(registry.names.size < LIMITS.fileNames && registry.nextId <= MAX_U64, 'FILE_REGISTRY_LIMIT', 'The engine session has reached its file-name limit. Start another Duxcel session to import more sources.');
        const entry = { id: String(registry.nextId++), state: 'reserved' };
        registry.names.set(name, entry); return entry;
    }
    #checkSourceBindings(request) {
        if (!request.sourceBindings) return;
        const registry = this.#fileRegistry;
        for (const binding of request.sourceBindings) {
            const entry = registry.names.get(binding.name);
            check(!registry.closed && binding.engineEpoch === registry.epoch && entry?.state === 'ready' && entry.id === binding.registrationId,
                'SOURCE_INVALIDATED', 'A captured browser source registration is unavailable or belongs to another engine session');
        }
    }
    constructor({ database, connection, engineVersion = null, dispose = null }) {
        this.database = database; this.connection = connection; this.engineVersion = engineVersion; this.dispose = dispose;
        if (database && (typeof database === 'object' || typeof database === 'function')) {
            this.#fileRegistry = fileNameRegistries.get(database);
            if (!this.#fileRegistry) { this.#fileRegistry = newFileNameRegistry(); fileNameRegistries.set(database, this.#fileRegistry); }
        } else this.#fileRegistry = newFileNameRegistry(); // Query-only test adapters have no database handle.
        this.active = null; this.pending = null; this.draining = null; this.closed = false; this.admin = false; this.latestId = 0n;
    }
    submit(value) {
        return this.#enqueue(pageRequest(value),false);
    }
    submitWorkbookSorts(value) {
        return this.#enqueue(workbookSortsRequest(value),false);
    }
    exportParquet(value) {
        return this.#enqueue(exportRequest(value),true);
    }
    #enqueue(request,exporting) {
        check(!this.closed && !this.admin, 'NOT_AVAILABLE', 'Engine is closed or registering a file');
        check(BigInt(request.requestId) > this.latestId, 'STALE_REQUEST', 'requestId must increase within this adapter');
        this.#checkSourceBindings(request); // No ticket/output reservation on failed admission.
        if(exporting)this.#reserveFileName(request.fileName,true);
        this.latestId = BigInt(request.requestId);
        if (this.pending) this.pending.resolve(failure(this.pending.request, 'superseded'));
        const result = new Promise(resolve => { this.pending = { request, resolve, reason: null, cancel: null,exporting,privateOwned:false }; });
        if (this.active) this.interrupt(this.active, 'superseded');
        if (!this.draining) this.draining = this.drain();
        return result;
    }
    interrupt(task, reason) {
        task.reason = reason;
        if (!task.cancel) task.cancel = Promise.resolve().then(() => this.connection.cancelSent()).catch(() => false);
    }
    async drain() {
        try {
            while (this.pending) {
                const task = this.active = this.pending; this.pending = null;
                let reader, reply;
                try {
                    this.#checkSourceBindings(task.request);
                    if(task.exporting){
                        const files=await this.database.globFiles(task.request.fileName);
                        check(Array.isArray(files) && files.length===0,'EXPORT_EXISTS','Private export file already exists; it has been preserved');
                        check(!task.reason,'SUPERSEDED','Request was superseded');task.privateOwned=true;
                    }
                    this.#checkSourceBindings(task.request);
                    reader = await this.connection.send(task.request.sql, true);
                    check(reader && !task.reason, 'SUPERSEDED', 'Request was superseded');
                    const page = await encodePage(reader, task.request, { obsolete: () => Boolean(task.reason) });
                    this.#checkSourceBindings(task.request);
                    if(task.exporting){
                        check(page.rowCount===1 && page.columnCount===1 && page.columns[0].name==='Count' &&
                            (page.columns[0].typeName==='BIGINT' && page.columns[0].numberKind==='i64' || page.columns[0].typeName==='UBIGINT' && page.columns[0].numberKind==='u64'),
                            'INVALID_EXPORT','COPY did not return its one exact integer Count column');
                        u64(pageCellText(page,0,0),'COPY row count');
                        // The complete Count reader is quiescent before file
                        // operations begin; no later task enters this queue yet.
                        if(reader?.cancel){await reader.cancel();reader=null;}
                        check(!task.reason,'SUPERSEDED','Request was superseded');
                        await this.database.flushFiles();check(!task.reason,'SUPERSEDED','Request was superseded');
                        this.#checkSourceBindings(task.request);
                        const infos=await this.database.globFiles(task.request.fileName);
                        check(Array.isArray(infos) && infos.length===1 && infos[0]?.fileName===task.request.fileName &&
                            Number.isSafeInteger(infos[0].fileSize) && infos[0].fileSize>=12 && infos[0].fileSize<=EXPORT_LIMITS.bytes,
                            'EXPORT_LIMIT','Private Parquet file must have one exact known size of at most 64 MiB');
                        check(!task.reason,'SUPERSEDED','Request was superseded');
                        this.#checkSourceBindings(task.request);
                        const data=await this.database.copyFileToBuffer(task.request.fileName);
                        check(!task.reason,'SUPERSEDED','Request was superseded');inspectParquetExport(data,infos[0].fileSize);
                        reply={...identity(task.request),status:'ok',page,data};
                    } else reply = { ...identity(task.request), status: 'ok', page };
                } catch (error) {
                    reply = failure(task.request, task.reason ?? 'error', error);
                    if (!task.cancel) this.interrupt(task, task.reason ?? 'error');
                } finally {
                    if (reader?.cancel) { try { await reader.cancel(); } catch { /* engine cancellation may already have closed the stream */ } }
                    if (task.cancel) await task.cancel; // finish the old cancel before a new send can start
                    if(task.privateOwned){
                        try{
                            const remaining=await this.database.globFiles(task.request.fileName);
                            check(Array.isArray(remaining),'EXPORT_CLEANUP','Private export cleanup could not inspect its file');
                            if(remaining.length)await this.database.dropFile(task.request.fileName);
                            const after=await this.database.globFiles(task.request.fileName);
                            check(Array.isArray(after) && after.length===0,'EXPORT_CLEANUP','Private export file was not removed');
                        }catch(error){task.cleanupError=new BridgeError('EXPORT_CLEANUP',`Private Parquet cleanup failed: ${String(error.message??error).slice(0,1800)}`);reply=failure(task.request,'error',task.cleanupError);}
                        // A cancellation can start while a noninterruptible
                        // file copy/drop is pending. Drain it before next send.
                        if(task.cancel)await task.cancel;
                    }
                }
                // Reader cancellation and export cleanup can await sibling
                // administration. Recheck after every owned cleanup has drained,
                // immediately before publishing any success/page/binary.
                if (reply?.status === 'ok' && !task.reason) {
                    try { this.#checkSourceBindings(task.request); }
                    catch (error) { reply = failure(task.request, 'error', error); }
                }
                task.resolve(task.reason && task.reason !== 'error' ? failure(task.request, task.reason,task.cleanupError) : reply);
                this.active = null;
            }
        } finally { this.draining = null; }
    }
    async cancel() {
        if (this.pending) { this.pending.resolve(failure(this.pending.request, 'cancelled')); this.pending = null; }
        if (this.active) this.interrupt(this.active, 'cancelled');
        if (this.draining) await this.draining;
    }
    async registerFile(name, bytes) {
        virtualName(name);
        check(bytes instanceof Uint8Array && bytes.byteLength <= LIMITS.fileBytes, 'FILE_LIMIT', 'File buffer must be at most 256 MiB');
        check(!this.closed && !this.active && !this.pending && !this.admin, 'NOT_AVAILABLE', 'Await an idle adapter before registering a file');
        const entry = this.#reserveFileName(name, true); entry.state = 'registering';
        this.admin = true;
        try {
            const length = bytes.byteLength;
            const owned = new Uint8Array(bytes); // never transfer/detach a C heap or caller-owned subarray
            await this.database.registerFileBuffer(name, owned);
            // A sibling adapter may have revoked this reservation while the
            // asynchronous registration was pending; never resurrect it.
            check(entry.state === 'registering' && !this.#fileRegistry.closed, 'SOURCE_INVALIDATED', 'Source registration was revoked before completion');
            entry.state = 'ready';
            // Opaque session scalars, not a content hash or persisted version.
            return Object.freeze({ name, bytes: length, engineEpoch: this.#fileRegistry.epoch, registrationId: entry.id });
        } catch (error) {
            if (entry.state === 'registering') entry.state = 'failed';
            throw error;
        } finally { this.admin = false; }
    }
    async dropFile(name) {
        virtualName(name);
        check(!this.closed && !this.active && !this.pending && !this.admin, 'NOT_AVAILABLE', 'Await an idle adapter before dropping a file');
        const entry = this.#reserveFileName(name, false); entry.state = 'revoked';
        this.admin = true;
        try { await this.database.dropFile(name); }
        finally { this.admin = false; }
    }
    async close() {
        if (this.closed) return;
        check(!this.admin, 'NOT_AVAILABLE', 'Await file registration before closing');
        this.closed = true;
        // Only the adapter that owns database disposal closes the shared
        // registration lifetime. Closing a sibling connection leaves it intact.
        if (this.dispose) this.#fileRegistry.closed = true;
        await this.cancel();
        try { await this.connection.close(); } finally { if (this.dispose) await this.dispose(); }
    }
}

/** Same-origin official mvp/eh worker + Wasm URLs, supplied by the host packager. */
export async function createOfficialWorkerAdapter({ moduleURL, workerURL, duckdb = null, WorkerClass = globalThis.Worker, logger = null }) {
    check(typeof moduleURL === 'string' && typeof workerURL === 'string' && WorkerClass, 'INVALID_ENGINE', 'Official single-threaded Wasm/worker URLs and Worker constructor required');
    duckdb ??= await import('@duckdb/duckdb-wasm/dist/duckdb-browser.mjs');
    let worker, database;
    try {
        worker = new WorkerClass(workerURL);
        database = new duckdb.AsyncDuckDB(logger ?? new duckdb.VoidLogger(), worker);
        await database.instantiate(moduleURL); // no pthread worker / experimental coi bundle
        await database.open({ maximumThreads: 1, arrowLosslessConversion: true,
            query: { castBigIntToDouble: false, castDecimalToDouble: false, castTimestampToDate: false, castDurationToTime64: false } });
        const connection = await database.connect();
        const engineVersion = await database.getVersion();
        return new DuckDBWorkerAdapter({ database, connection, engineVersion, dispose: () => database.terminate() });
    } catch (error) {
        if (database) { try { await database.terminate(); } catch { worker?.terminate(); } }
        else worker?.terminate();
        throw error;
    }
}
