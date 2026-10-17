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
    sqlBytes: 1024 * 1024, fileBytes: 256 * 1024 * 1024, fileNames: 1024 });
const MAX_U64 = 18446744073709551615n;
const encoder = new TextEncoder();
const decoder = new TextDecoder('utf-8', { fatal: true });
const NUMBER_KINDS = new Set(['none', 'i8', 'i16', 'i32', 'i64', 'i128', 'u8', 'u16', 'u32', 'u64', 'u128', 'decimal', 'float']);
// Reconnecting through another adapter must not reset this database object's
// reservation history. A distinct database wrapper/raw engine write remains
// outside this API guard; this is not a verified registration epoch.
const fileNameRegistries = new WeakMap();

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
    request.rowLimit = value.rowLimit ?? LIMITS.rows;
    check(Number.isInteger(request.rowLimit) && request.rowLimit > 0 && request.rowLimit <= LIMITS.rows, 'INVALID_REQUEST', 'Page row limit must be 1–256');
    request.knownTotal = value.knownTotal == null ? null : u64(value.knownTotal, 'knownTotal');
    if (value.columns != null) {
        check(Array.isArray(value.columns) && value.columns.length <= LIMITS.columns, 'INVALID_REQUEST', 'Too many requested columns');
        request.columns = Object.freeze(value.columns.map(descriptor));
    }
    return Object.freeze(request);
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
    check(!request.columns || request.columns.length === fields.length, 'SCHEMA_CHANGED', 'Result column count disagrees with the C-owned schema');
    const columns = fields.map((field, index) => metadata(field, request.columns?.[index]));
    let bytes = encoder.encode(JSON.stringify(columns)).byteLength, rows = 0;
    const chunks = [], nullValues = [], starts = [0];
    let textBytes = 0;
    for await (const batch of reader) {
        check(!obsolete(), 'SUPERSEDED', 'Request was superseded');
        check(batch.numCols === fields.length && rows + batch.numRows <= request.rowLimit, 'PAGE_LIMIT', 'C-built page SQL returned more than its requested bounded rows');
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
    #fileNames;
    #reserveFileName(name, registration) {
        if (this.#fileNames.has(name)) {
            check(!registration, 'IMMUTABLE_FILE', 'This source name was already used in this engine session. Import with a fresh virtual name.');
            return;
        }
        check(this.#fileNames.size < LIMITS.fileNames, 'FILE_REGISTRY_LIMIT', 'The engine session has reached its file-name limit. Reload before importing more sources.');
        this.#fileNames.add(name);
    }
    constructor({ database, connection, engineVersion = null, dispose = null }) {
        this.database = database; this.connection = connection; this.engineVersion = engineVersion; this.dispose = dispose;
        if (database && (typeof database === 'object' || typeof database === 'function')) {
            this.#fileNames = fileNameRegistries.get(database);
            if (!this.#fileNames) { this.#fileNames = new Set(); fileNameRegistries.set(database, this.#fileNames); }
        } else this.#fileNames = new Set(); // Query-only test adapters have no database handle.
        this.active = null; this.pending = null; this.draining = null; this.closed = false; this.admin = false; this.latestId = 0n;
    }
    submit(value) {
        const request = pageRequest(value);
        check(!this.closed && !this.admin, 'NOT_AVAILABLE', 'Engine is closed or registering a file');
        check(BigInt(request.requestId) > this.latestId, 'STALE_REQUEST', 'requestId must increase within this adapter');
        this.latestId = BigInt(request.requestId);
        if (this.pending) this.pending.resolve(failure(this.pending.request, 'superseded'));
        const result = new Promise(resolve => { this.pending = { request, resolve, reason: null, cancel: null }; });
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
                    reader = await this.connection.send(task.request.sql, true);
                    check(reader && !task.reason, 'SUPERSEDED', 'Request was superseded');
                    const page = await encodePage(reader, task.request, { obsolete: () => Boolean(task.reason) });
                    reply = { ...identity(task.request), status: 'ok', page };
                } catch (error) {
                    reply = failure(task.request, task.reason ?? 'error', error);
                    if (!task.cancel) this.interrupt(task, task.reason ?? 'error');
                } finally {
                    if (reader?.cancel) { try { await reader.cancel(); } catch { /* engine cancellation may already have closed the stream */ } }
                    if (task.cancel) await task.cancel; // finish the old cancel before a new send can start
                }
                task.resolve(task.reason && task.reason !== 'error' ? failure(task.request, task.reason) : reply);
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
        this.#reserveFileName(name, true);
        this.admin = true;
        try {
            const length = bytes.byteLength;
            const owned = new Uint8Array(bytes); // never transfer/detach a C heap or caller-owned subarray
            await this.database.registerFileBuffer(name, owned);
            return { name, bytes: length };
        } finally { this.admin = false; }
    }
    async dropFile(name) {
        virtualName(name);
        check(!this.closed && !this.active && !this.pending && !this.admin, 'NOT_AVAILABLE', 'Await an idle adapter before dropping a file');
        this.#reserveFileName(name, false);
        this.admin = true;
        try { await this.database.dropFile(name); }
        finally { this.admin = false; }
    }
    async close() {
        if (this.closed) return;
        check(!this.admin, 'NOT_AVAILABLE', 'Await file registration before closing');
        this.closed = true;
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
