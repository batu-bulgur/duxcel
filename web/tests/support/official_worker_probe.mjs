/** Real-browser, real-SQL acceptance for the official Worker adapter.
 * Imported/run only by the integration owner's browser harness. No fake engine,
 * production SQL builder, UI or automatic startup. This does not establish the
 * shared C request/result bridge, source imports or browser workbook acceptance.
 */
const now = () => globalThis.performance?.now() ?? Date.now();
const pause = () => new Promise(resolve => setTimeout(resolve, 0));
const IDENTITY = Object.freeze({ generation: '9007199254740995', documentId: '9007199254740997',
    sourceId: '18446744073709551613', objectId: '18446744073709551612', revision: '9007199254740999' });
const ID_FIELDS = ['requestId', 'generation', 'documentId', 'sourceId', 'objectId', 'revision', 'offset'];
const BIG_BASE = 9007199254740993n;

export function probeConfig(value = {}) {
    const base = new URL(value.baseURL ?? globalThis.location?.href ?? import.meta.url);
    const moduleURL = new URL(value.moduleURL ?? './vendor/duckdb/dist/duckdb-mvp.wasm', base);
    const workerURL = new URL(value.workerURL ?? './vendor/duckdb/dist/duckdb-browser-mvp.worker.js', base);
    if (!['http:', 'https:'].includes(base.protocol) || moduleURL.origin !== base.origin || workerURL.origin !== base.origin)
        throw new TypeError('Probe requires same-origin HTTP(S) official Worker/Wasm assets');
    const flavor = moduleURL.pathname.match(/\/duckdb-(mvp|eh)\.wasm$/)?.[1];
    if (!flavor || !workerURL.pathname.endsWith(`/duckdb-browser-${flavor}.worker.js`))
        throw new TypeError('Probe requires a matching official single-threaded mvp/eh asset pair');
    const timeoutMs = value.timeoutMs ?? 20000, cancelRows = value.cancelRows ?? 10000000;
    if (!Number.isInteger(timeoutMs) || timeoutMs < 1000 || timeoutMs > 120000 ||
        !Number.isInteger(cancelRows) || cancelRows < 1 || cancelRows > 50000000)
        throw new TypeError('Bound timeoutMs to 1000–120000 and cancelRows to 1–50000000');
    return Object.freeze({ moduleURL: moduleURL.href, workerURL: workerURL.href, timeoutMs, cancelRows });
}

export function equalProbeValue(actual, expected, label) {
    if (actual !== expected) throw new Error(`${label}: expected ${JSON.stringify(expected)}, received ${JSON.stringify(actual)}`);
}
function ensure(value, label) { if (!value) throw new Error(label); }
function errorRecord(error) { return { code: error.code ?? error.name ?? 'ERROR', message: String(error.message ?? error).slice(0, 2048) }; }
async function deadline(promise, milliseconds) {
    let timer;
    try {
        return await Promise.race([promise, new Promise((resolve, reject) => { timer = setTimeout(() => {
            const error = new Error(`Worker probe timed out after ${milliseconds} ms`); error.code = 'PROBE_TIMEOUT'; reject(error);
        }, milliseconds); })]);
    } finally { clearTimeout(timer); }
}
function sameIdentity(reply, request) {
    for (const name of ID_FIELDS) equalProbeValue(reply[name], request[name], `${name} retained`);
}
function successful(reply, request) {
    sameIdentity(reply, request);
    ensure(reply.status === 'ok', `Query ${request.requestId} failed: ${JSON.stringify(reply.error ?? reply.status)}`);
    sameIdentity(reply.page, request);
    ensure(reply.page.rowCount <= request.rowLimit, 'Reply exceeded requested row limit');
    return reply.page;
}

export async function runWorkerProbe(input = {}) {
    const config = probeConfig(input), started = now(), checks = [], requests = [];
    const ownedWorkers = [];
    let adapter, api, nextId = BIG_BASE, fatal = false;
    const capabilities = {};
    function request(sql, options = {}) {
        const value = { ...IDENTITY, requestId: (nextId++).toString(), offset: '0', rowLimit: 1, knownTotal: null, sql, ...options };
        requests.push(value);
        return value;
    }
    async function step(name, operation) {
        const begin = now();
        try {
            const details = await deadline(operation(), config.timeoutMs);
            checks.push({ name, status: details?.unsupported ? 'unsupported' : 'passed', milliseconds: now() - begin, details });
        } catch (error) {
            checks.push({ name, status: 'failed', milliseconds: now() - begin, error: errorRecord(error) });
            if (error.code === 'PROBE_TIMEOUT') { fatal = true; throw error; }
        }
    }
    async function query(sql, options = {}) {
        const value = request(sql, options), reply = await adapter.submit(value);
        return { request: value, reply, page: successful(reply, value) };
    }
    try {
        await step('initialize-official-worker', async () => {
            api = await import('./official_worker.mjs');
            ensure(typeof globalThis.Worker === 'function', 'Real browser Worker constructor required');
            function OwnedWorker(...arguments_) {
                const worker = new globalThis.Worker(...arguments_);
                ownedWorkers.push(worker);
                return worker;
            }
            adapter = await api.createOfficialWorkerAdapter({ ...config, WorkerClass: OwnedWorker });
            ensure(typeof adapter.engineVersion === 'string' && adapter.engineVersion.length > 0, 'getVersion returned no engine version');
            return { engineVersion: adapter.engineVersion, moduleURL: config.moduleURL, workerURL: config.workerURL };
        });
        if (!adapter) fatal = true;
        if (!fatal) {
            await step('engine-version-from-library-and-sql', async () => {
                const { page } = await query('SELECT version() AS engine_version LIMIT 1');
                const version = api.pageCellText(page, 0, 0);
                equalProbeValue(version.replace(/^v/, ''), adapter.engineVersion.replace(/^v/, ''), 'SQL/library engine version');
                return { sql: version, library: adapter.engineVersion };
            });
            await step('registered-source-names-never-rebind', async () => {
                const original = 'probe-source-original.csv', fresh = 'probe-source-fresh.csv';
                const bytes = marker => new TextEncoder().encode(`marker\n${marker}\n`);
                const binding = token => Object.freeze({ name: token.name, engineEpoch: token.engineEpoch, registrationId: token.registrationId });
                const admissions = [];
                const rejectBinding = async (owner, token, code, label) => {
                    const value = request(`SELECT marker FROM read_csv('${original}', header=true) LIMIT 1`, { sourceBindings: [token] });
                    const latest = owner.latestId, send = owner.connection.send;
                    const descriptor = Object.getOwnPropertyDescriptor(owner.connection, 'send'); let sends = 0, rejected = false;
                    // Observe calls to the real connection without replacing SQL,
                    // engine results or the adapter's actual workload counters.
                    owner.connection.send = function (...arguments_) { sends++; return send.apply(this, arguments_); };
                    try {
                        try { await owner.submit(value); }
                        catch (error) { equalProbeValue(error.code, code, `${label} rejection`); rejected = true; }
                        ensure(rejected, `${label} was not rejected at admission (sends=${sends}, latestId=${owner.latestId})`);
                        equalProbeValue(owner.latestId, latest, `${label} retained latestId`);
                        equalProbeValue(sends, 0, `${label} submitted no SQL`);
                        admissions.push({ label, code, requestId: value.requestId, latestId: latest.toString(), sends });
                    } finally {
                        if (descriptor) Object.defineProperty(owner.connection, 'send', descriptor);
                        else delete owner.connection.send;
                    }
                };
                const rejected = async (owner, name) => {
                    try { await owner.registerFile(name, bytes('replacement')); }
                    catch (error) { equalProbeValue(error.code, 'IMMUTABLE_FILE', 'Source replacement rejected before mutation'); return; }
                    throw new Error('A reserved source name was rebound');
                };
                const originalToken = binding(await adapter.registerFile(original, bytes('original')));
                await rejected(adapter, original);
                const retained = await query(`SELECT marker FROM read_csv('${original}', header=true) LIMIT 1`, { sourceBindings: [originalToken] });
                equalProbeValue(api.pageCellText(retained.page, 0, 0), 'original', 'Existing source bytes retained');
                await rejectBinding(adapter, { ...originalToken, registrationId: '0' }, 'INVALID_REQUEST', 'Zero source token');
                await rejectBinding(adapter, { ...originalToken, registrationId: '18446744073709551615' }, 'SOURCE_INVALIDATED', 'Unissued source token');
                // A fresh connection to the same official database must retain
                // the reservation history, including after the file is dropped.
                const sibling = new api.DuckDBWorkerAdapter({ database: adapter.database,
                    connection: await adapter.database.connect() });
                let freshToken;
                try {
                    await rejected(sibling, original);
                    await sibling.dropFile(original);
                    await rejected(adapter, original);
                    await rejected(sibling, original);
                    await rejectBinding(adapter, originalToken, 'SOURCE_INVALIDATED', 'Sibling drop revoked captured token');
                    const reconnected = new api.DuckDBWorkerAdapter({ database: adapter.database,
                        connection: await adapter.database.connect() });
                    try { await rejectBinding(reconnected, originalToken, 'SOURCE_INVALIDATED', 'Reconnected database retained revocation'); }
                    finally { await reconnected.close(); }
                    freshToken = binding(await sibling.registerFile(fresh, bytes('fresh')));
                    equalProbeValue(freshToken.engineEpoch, originalToken.engineEpoch, 'Shared database source epoch');
                    ensure(freshToken.registrationId !== originalToken.registrationId, 'A retired source token was reused');
                } finally { await sibling.close(); }
                const imported = await query(`SELECT marker FROM read_csv('${fresh}', header=true) LIMIT 1`, { sourceBindings: [freshToken] });
                equalProbeValue(api.pageCellText(imported.page, 0, 0), 'fresh', 'Fresh-name import works after rejected replacement');
                await adapter.dropFile(fresh);
                // Retire the entire first engine before creating another. The
                // remaining original probe stages use only SELECT/range inputs;
                // they do not depend on this database or its dropped sources.
                const engineVersion = adapter.engineVersion;
                await adapter.close();
                adapter = await api.createOfficialWorkerAdapter({ ...config, WorkerClass: function (...arguments_) {
                    const worker = new globalThis.Worker(...arguments_); ownedWorkers.push(worker); return worker;
                } });
                equalProbeValue(adapter.engineVersion, engineVersion, 'Sequential database uses the same pinned engine');
                const nextToken = binding(await adapter.registerFile(original, bytes('new-database')));
                ensure(nextToken.engineEpoch !== originalToken.engineEpoch, 'A genuinely new database reused the source epoch');
                equalProbeValue(nextToken.registrationId, originalToken.registrationId, 'Independent database may reuse only its local registration ID');
                await rejectBinding(adapter, originalToken, 'SOURCE_INVALIDATED', 'Same basename in a new database rejected old epoch');
                const next = await query(`SELECT marker FROM read_csv('${original}', header=true) LIMIT 1`, { sourceBindings: [nextToken] });
                equalProbeValue(api.pageCellText(next.page, 0, 0), 'new-database', 'Fresh database accepts only its captured token');
                await adapter.dropFile(original);
                equalProbeValue(ownedWorkers.length, 2, 'Exactly two sequential probe-owned Workers');
                return { original, fresh, originalToken, freshToken, nextToken, admissions, workers: 'Two sequential owned official Workers; no overlap',
                    scope: 'Standalone adapter probe only; C192 driver unchanged. Explicit immutable registered buffers only; raw engine writes, distinct wrappers and persistent/content identity excluded' };
            });
            await step('exact-arrow-values-and-null-states', async () => {
                const { page, request: value } = await query(`SELECT
                    '-9223372036854775808'::BIGINT AS min_bigint,
                    '9007199254740993'::BIGINT AS above_js_integer,
                    '18446744073709551615'::UBIGINT AS max_unsigned,
                    '9007199254740993.1234567890'::DECIMAL(38,10) AS exact_decimal,
                    '-0.0000000001'::DECIMAL(38,10) AS negative_decimal,
                    NULL::VARCHAR AS missing, ''::VARCHAR AS zero_length,
                    'NULL'::VARCHAR AS literal_null, 'İstanbul 😀'::VARCHAR AS utf8 LIMIT 1`);
                const expected = ['-9223372036854775808', '9007199254740993', '18446744073709551615',
                    '9007199254740993.1234567890', '-0.0000000001', null, '', 'NULL', 'İstanbul 😀'];
                equalProbeValue(page.rowCount, 1, 'Typed row count'); equalProbeValue(page.columnCount, expected.length, 'Typed column count');
                const values = expected.map((text, column) => { const actual = api.pageCellText(page, 0, column); equalProbeValue(actual, text, `Exact column ${column}`); return actual; });
                equalProbeValue(page.nulls[5], 1, 'SQL NULL mask'); equalProbeValue(page.nulls[6], 0, 'Zero-length text mask');
                equalProbeValue(page.nulls[7], 0, 'Literal NULL text mask');
                equalProbeValue(page.columns[3].typeName, 'DECIMAL(38,10)', 'Exact DECIMAL logical type');
                return { requestId: value.requestId, values, columns: page.columns };
            });
            await step('canonical-varchar-retains-original-sql-metadata', async () => {
                const columns = [
                    { name: 'exact_integer', typeName: 'BIGINT', numberKind: 'i64', precision: 0, scale: 0 },
                    { name: 'exact_decimal', typeName: 'DECIMAL(38,10)', numberKind: 'decimal', precision: 38, scale: 10 },
                    { name: 'typed_null', typeName: 'DECIMAL(38,10)', numberKind: 'decimal', precision: 38, scale: 10 },
                    { name: 'timestamp_ns', typeName: 'TIMESTAMP_NS', numberKind: 'none', precision: 0, scale: 0 },
                ];
                const { page } = await query(`SELECT CAST('9007199254740993'::BIGINT AS VARCHAR) AS exact_integer,
                    CAST('9007199254740993.1234567890'::DECIMAL(38,10) AS VARCHAR) AS exact_decimal,
                    CAST(NULL::DECIMAL(38,10) AS VARCHAR) AS typed_null,
                    CAST('2026-10-17 12:34:56.123456789'::TIMESTAMP_NS AS VARCHAR) AS timestamp_ns LIMIT 1`, { columns });
                const values = ['9007199254740993', '9007199254740993.1234567890', null, '2026-10-17 12:34:56.123456789'];
                for (let column = 0; column < columns.length; column++) {
                    equalProbeValue(api.pageCellText(page, 0, column), values[column], 'Canonical engine text');
                    for (const [key, expected] of Object.entries(columns[column])) equalProbeValue(page.columns[column][key], expected, `Original SQL ${key}`);
                    equalProbeValue(page.columns[column].sqlTypeProvided, true, 'Explicit SQL metadata');
                    ensure(/Utf8$/.test(page.columns[column].arrowType), 'Canonical projection must arrive as Arrow text');
                }
                return { values, columns: page.columns };
            });
            await step('direct-hugeint-arrow-capability', async () => {
                const value = request(`SELECT '170141183460469231731687303715884105727'::HUGEINT AS huge_max,
                    '-170141183460469231731687303715884105728'::HUGEINT AS huge_min LIMIT 1`);
                const reply = await adapter.submit(value); sameIdentity(reply, value);
                if (reply.status === 'error' && (reply.error?.code === 'UNSUPPORTED_TYPE' || /unsupported|not supported|not implemented/i.test(reply.error?.message ?? ''))) {
                    capabilities.hugeIntArrow = false;
                    return { unsupported: true, error: reply.error, alternative: 'Engine-produced VARCHAR with explicit HUGEINT metadata is tested next' };
                }
                const page = successful(reply, value);
                equalProbeValue(api.pageCellText(page, 0, 0), '170141183460469231731687303715884105727', 'Direct HUGEINT maximum');
                equalProbeValue(api.pageCellText(page, 0, 1), '-170141183460469231731687303715884105728', 'Direct HUGEINT minimum');
                capabilities.hugeIntArrow = true;
                return { columns: page.columns };
            });
            await step('canonical-hugeint-exact-range', async () => {
                const columns = ['huge_max', 'huge_min'].map(name => ({ name, typeName: 'HUGEINT', numberKind: 'i128' }));
                const { page } = await query(`SELECT CAST('170141183460469231731687303715884105727'::HUGEINT AS VARCHAR) AS huge_max,
                    CAST('-170141183460469231731687303715884105728'::HUGEINT AS VARCHAR) AS huge_min LIMIT 1`, { columns });
                const values = [api.pageCellText(page, 0, 0), api.pageCellText(page, 0, 1)];
                equalProbeValue(values[0], '170141183460469231731687303715884105727', 'Canonical HUGEINT maximum');
                equalProbeValue(values[1], '-170141183460469231731687303715884105728', 'Canonical HUGEINT minimum');
                ensure(page.columns.every(column => column.typeName === 'HUGEINT' && column.numberKind === 'i128' && column.sqlTypeProvided), 'HUGEINT identity lost through canonical projection');
                return { values, columns: page.columns };
            });
            await step('bounded-offset-pages-and-owned-transfer', async () => {
                const pages = [];
                for (const [offset, rows] of [[0, 256], [256, 256], [512, 88]]) {
                    const { page } = await query(`SELECT i+9007199254740993::BIGINT AS exact_id,
                        CASE WHEN i%3=0 THEN NULL WHEN i%3=1 THEN '' ELSE 'NULL' END AS text_state
                        FROM range(600) AS r(i) ORDER BY i LIMIT 256 OFFSET ${offset}`, { offset: String(offset), rowLimit: 256 });
                    equalProbeValue(page.rowCount, rows, 'Bounded page rows');
                    equalProbeValue(page.total, String(offset + rows), 'Observed lower bound');
                    equalProbeValue(page.countPending, rows === 256, 'Pending count');
                    const first = (BIG_BASE + BigInt(offset)).toString(), last = (BIG_BASE + BigInt(offset + rows - 1)).toString();
                    equalProbeValue(api.pageCellText(page, 0, 0), first, 'Page first exact integer');
                    equalProbeValue(api.pageCellText(page, rows - 1, 0), last, 'Page last exact integer');
                    const transferred = structuredClone(page, { transfer: api.pageTransferables(page) });
                    ensure(page.text.byteLength === 0 && page.offsets.byteLength === 0 && page.nulls.byteLength === 0, 'Owned buffers did not detach');
                    equalProbeValue(api.pageCellText(transferred, rows - 1, 0), last, 'Transferred exact text');
                    pages.push({ offset: transferred.offset, rows, total: transferred.total, countPending: transferred.countPending, first, last });
                }
                const { page } = await query('SELECT i::BIGINT AS exact_id FROM range(600) AS r(i) LIMIT 1 OFFSET 9007199254740993',
                    { offset: '9007199254740993', rowLimit: 1, knownTotal: '600' });
                equalProbeValue(page.offset, '9007199254740993', 'Large exact page offset');
                equalProbeValue(page.rowCount, 0, 'Large offset empty result');
                equalProbeValue(page.total, '600', 'Known total retained'); equalProbeValue(page.countPending, false, 'Known total does not trigger count');
                return { pages, largeOffset: page.offset };
            });
            await step('real-sql-error-and-bounded-page-recovery', async () => {
                const value = request('SELECT * FROM __duxcel_worker_probe_missing_table LIMIT 1');
                const reply = await adapter.submit(value); sameIdentity(reply, value);
                equalProbeValue(reply.status, 'error', 'Binder error status'); equalProbeValue(reply.error?.code, 'QUERY_FAILED', 'Binder error code');
                ensure(reply.error.message.length > 0 && reply.error.message.length <= 2048, 'Unbounded/missing query error');
                const oversized = request('SELECT i::INTEGER AS value FROM range(257) AS r(i) LIMIT 257', { rowLimit: 256 });
                const bounded = await adapter.submit(oversized); sameIdentity(bounded, oversized);
                equalProbeValue(bounded.status, 'error', 'Oversized row page status'); equalProbeValue(bounded.error?.code, 'PAGE_LIMIT', 'Oversized row page code');
                const { page } = await query('SELECT 7::INTEGER AS recovered LIMIT 1');
                equalProbeValue(api.pageCellText(page, 0, 0), '7', 'Recovery after errors');
                return { binder: reply.error, rowBudget: bounded.error };
            });
            await step('latest-wins-and-cancellation-cleanup', async () => {
                const slow = `SELECT CAST(sum(hash(i)) AS VARCHAR) AS old_result FROM range(${config.cancelRows}) AS r(i) LIMIT 1`;
                const a = request(slow), b = request("SELECT 'middle' AS marker LIMIT 1"),
                    c = request("SELECT 'newest' AS marker LIMIT 1", { generation: (BigInt(IDENTITY.generation) + 1n).toString() });
                const first = adapter.submit(a), middle = adapter.submit(b), newest = adapter.submit(c);
                const replies = await Promise.all([first, middle, newest]);
                [a, b, c].forEach((value, index) => sameIdentity(replies[index], value));
                equalProbeValue(replies[0].status, 'superseded', 'Old active query'); equalProbeValue(replies[1].status, 'superseded', 'Old pending query');
                const page = successful(replies[2], c); equalProbeValue(api.pageCellText(page, 0, 0), 'newest', 'Latest page after old cancellation');
                ensure(!replies[0].page && !replies[1].page, 'Obsolete rows escaped publication');
                const cancelledRequest = request(slow), pending = adapter.submit(cancelledRequest);
                await pause(); await adapter.cancel();
                const cancelled = await pending; sameIdentity(cancelled, cancelledRequest);
                equalProbeValue(cancelled.status, 'cancelled', 'Explicit cancellation status'); ensure(!cancelled.page, 'Cancelled rows escaped publication');
                const recovery = await query("SELECT 'after-cancel' AS marker LIMIT 1");
                equalProbeValue(api.pageCellText(recovery.page, 0, 0), 'after-cancel', 'Cleanup/recovery query');
                return { latest: replies.map(reply => ({ requestId: reply.requestId, status: reply.status })),
                    cancelled: { requestId: cancelled.requestId, status: cancelled.status }, cancelRows: config.cancelRows };
            });
        }
    } catch (error) {
        fatal = true;
        if (!checks.some(check => check.error?.code === error.code)) checks.push({ name: 'probe-abort', status: 'failed', error: errorRecord(error) });
    } finally {
        if (adapter) {
            const begin = now();
            try {
                await deadline(adapter.close(), 3000);
                checks.push({ name: 'close-worker-and-connection', status: 'passed', milliseconds: now() - begin });
            } catch (error) {
                fatal = true;
                checks.push({ name: 'close-worker-and-connection', status: 'failed', milliseconds: now() - begin, error: errorRecord(error) });
                try { await deadline(adapter.database.terminate(), 3000); }
                catch (termination) { checks.push({ name: 'force-worker-termination', status: 'failed', error: errorRecord(termination) }); }
            }
        }
        // Includes a Worker created by an initialization promise that timed out
        // before the factory could return an adapter. Only this probe owns them.
        for (const worker of ownedWorkers) {
            try { worker.terminate(); }
            catch (error) { fatal = true; checks.push({ name: 'terminate-owned-worker', status: 'failed', error: errorRecord(error) }); }
        }
    }
    return { schema: 'duxcel.worker-probe', version: 1,
        status: fatal || checks.some(check => check.status === 'failed') ? 'failed' : 'passed',
        engineVersion: adapter?.engineVersion ?? null, milliseconds: now() - started, checks, capabilities, requests, ownedWorkersCreated: ownedWorkers.length,
        scope: 'Real official Worker adapter SQL/type/ownership/lifecycle acceptance; shared C bridge, UI responsiveness, source import and workbook acceptance remain separate.' };
}
