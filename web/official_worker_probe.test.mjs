// Pure probe configuration/assertion contracts. No Worker, engine or adapter
// mocks; importing the probe must not start any application or engine runtime.
import test from 'node:test';
import assert from 'node:assert/strict';
import { probeConfig, equalProbeValue, runWorkerProbe } from './official_worker_probe.mjs';

test('probe import is inert and same-origin official URLs resolve against the artifact page', () => {
    assert.equal(typeof runWorkerProbe, 'function');
    const config = probeConfig({ baseURL: 'http://127.0.0.1:8767/artifact/index.html' });
    assert.equal(config.moduleURL, 'http://127.0.0.1:8767/artifact/vendor/duckdb/dist/duckdb-mvp.wasm');
    assert.equal(config.workerURL, 'http://127.0.0.1:8767/artifact/vendor/duckdb/dist/duckdb-browser-mvp.worker.js');
    assert.equal(config.timeoutMs, 20000);
    assert.equal(config.cancelRows, 10000000);
    assert.equal(Object.isFrozen(config), true);
});

test('external/mismatched/threaded assets and unbounded probe work are rejected', () => {
    const base = { baseURL: 'http://127.0.0.1:8767/artifact/' };
    for (const change of [{ baseURL: 'file:///tmp/index.html' },
        { moduleURL: 'https://other.invalid/duckdb-mvp.wasm' },
        { workerURL: 'https://other.invalid/duckdb-browser-mvp.worker.js' },
        { moduleURL: './duckdb-coi.wasm', workerURL: './duckdb-browser-coi.worker.js' },
        { moduleURL: './duckdb-eh.wasm' }, { timeoutMs: 0 }, { timeoutMs: 120001 },
        { cancelRows: 0 }, { cancelRows: 50000001 }, { cancelRows: '10000000' }]) {
        assert.throws(() => probeConfig({ ...base, ...change }), TypeError);
    }
    assert.ok(probeConfig({ ...base, moduleURL: './duckdb-eh.wasm', workerURL: './duckdb-browser-eh.worker.js' }));
});

test('probe equality never merges SQL NULL, empty text, literal NULL or rounded integers', () => {
    for (const value of [null, '', 'NULL', '9007199254740993', '-0.0000000001']) equalProbeValue(value, value, 'exact value');
    for (const [left, right] of [[null, ''], [null, 'NULL'], ['', 'NULL'],
        ['9007199254740993', '9007199254740992'], ['9007199254740993', 9007199254740992]]) {
        assert.throws(() => equalProbeValue(left, right, 'distinct value'), /distinct value/);
    }
});
