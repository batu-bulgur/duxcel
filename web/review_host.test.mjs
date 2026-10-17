import test from 'node:test';
import assert from 'node:assert/strict';
import { webcrypto, createHash } from 'node:crypto';
import { deflateSync, inflateSync } from 'node:zlib';
import { createReviewHost, REVIEW_LIMITS } from './review_host.mjs';

const encoder = new TextEncoder(), decoder = new TextDecoder('utf-8', { fatal: true });
const frameId = '9007199254740993', reportId = 'R-20261017-0001', wasmHash = '0123456789abcdef'.repeat(4);
const tick = () => new Promise(resolve => setImmediate(resolve));
function deferred() { let resolve, reject; const promise = new Promise((yes, no) => { resolve = yes; reject = no; }); return { promise, resolve, reject }; }
function sha256(bytes) { return createHash('sha256').update(bytes).digest('hex'); }
function crc32(bytes) {
    let value = 0xffffffff;
    for (const byte of bytes) {
        value ^= byte;
        for (let bit = 0; bit < 8; bit++) value = (value >>> 1) ^ (value & 1 ? 0xedb88320 : 0);
    }
    return (value ^ 0xffffffff) >>> 0;
}
function pngChunk(name, data) {
    const result = new Uint8Array(data.length + 12), view = new DataView(result.buffer);
    view.setUint32(0, data.length); result.set(encoder.encode(name), 4); result.set(data, 8);
    view.setUint32(result.length - 4, crc32(result.subarray(4, -4))); return result;
}
function encodePNG({ rgba, width, height }) {
    const ihdr = new Uint8Array(13), view = new DataView(ihdr.buffer);
    view.setUint32(0, width); view.setUint32(4, height); ihdr[8] = 8; ihdr[9] = 6;
    const scanlines = new Uint8Array(height * (width * 4 + 1));
    for (let row = 0; row < height; row++) scanlines.set(rgba.subarray(row * width * 4, (row + 1) * width * 4), row * (width * 4 + 1) + 1);
    return new Uint8Array(Buffer.concat([Uint8Array.from([137, 80, 78, 71, 13, 10, 26, 10]),
        pngChunk('IHDR', ihdr), pngChunk('IDAT', deflateSync(scanlines)), pngChunk('IEND', new Uint8Array())]));
}
function decodePNG(bytes) {
    assert.deepEqual([...bytes.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength), data = [];
    let offset = 8, width, height;
    while (offset < bytes.length) {
        const size = view.getUint32(offset), kind = decoder.decode(bytes.subarray(offset + 4, offset + 8));
        assert.ok(offset + 12 + size <= bytes.length, 'PNG chunk fits');
        assert.equal(view.getUint32(offset + 8 + size), crc32(bytes.subarray(offset + 4, offset + 8 + size)), 'PNG CRC is valid');
        if (kind === 'IHDR') { width = view.getUint32(offset + 8); height = view.getUint32(offset + 12); }
        if (kind === 'IDAT') data.push(bytes.subarray(offset + 8, offset + 8 + size));
        offset += size + 12;
    }
    assert.equal(offset, bytes.length);
    const raw = inflateSync(Buffer.concat(data)), rgba = new Uint8Array(width * height * 4);
    assert.equal(raw.length, height * (width * 4 + 1));
    for (let row = 0; row < height; row++) {
        assert.equal(raw[row * (width * 4 + 1)], 0);
        rgba.set(raw.subarray(row * (width * 4 + 1) + 1, (row + 1) * (width * 4 + 1)), row * width * 4);
    }
    return { rgba, width, height };
}
function zero(bytes) { assert.ok(bytes.every(byte => byte === 0), 'archive padding is zeroed'); }
async function parseTar(blob) {
    assert.equal(blob.type, 'application/x-tar');
    const bytes = new Uint8Array(await blob.arrayBuffer()), files = new Map();
    assert.equal(bytes.length % 512, 0);
    let offset = 0;
    const string = (header, start, size) => decoder.decode(header.subarray(start, start + size)).replace(/\0.*$/s, '');
    const octal = (header, start, size) => parseInt(string(header, start, size).trim(), 8);
    while (offset + 512 <= bytes.length && bytes[offset] !== 0) {
        const header = bytes.subarray(offset, offset + 512), prefix = string(header, 345, 155), name = string(header, 0, 100);
        assert.equal(string(header, 257, 6), 'ustar'); assert.equal(string(header, 263, 2), '00');
        assert.equal(header[156], 48); assert.equal(octal(header, 100, 8), 0o600);
        assert.equal(octal(header, 108, 8), 0); assert.equal(octal(header, 116, 8), 0); assert.equal(octal(header, 136, 12), 0);
        const checksum = [...header].reduce((sum, byte, index) => sum + (index >= 148 && index < 156 ? 32 : byte), 0);
        assert.equal(octal(header, 148, 8), checksum, 'USTAR checksum is valid');
        const size = octal(header, 124, 12), path = prefix ? `${prefix}/${name}` : name;
        assert.ok(!files.has(path)); assert.ok(Number.isSafeInteger(size) && size >= 0);
        const start = offset + 512, end = start + size;
        assert.ok(end <= bytes.length); files.set(path, bytes.slice(start, end));
        offset = start + Math.ceil(size / 512) * 512; zero(bytes.subarray(end, offset));
    }
    assert.equal(bytes.length - offset, 1024, 'archive ends in exactly two zero blocks'); zero(bytes.subarray(offset));
    return files;
}
function browserGeometry() {
    return { canvasCSS: { x: 37, y: 55, width: 240, height: 120 }, canvasBacking: { width: 4, height: 2 }, devicePixelRatio: 2,
        visualViewport: { scale: 1.25, offsetLeft: 11, offsetTop: 7, width: 800, height: 600 },
        viewport: { width: 1024, height: 768, scrollX: 23, scrollY: 31 }, userAgent: 'Browser/1 🦆', language: 'tr-TR' };
}
function canvasFixture() {
    const listeners = new Map(), changes = [], document = {};
    const canvas = { width: 4, height: 2, ownerDocument: document,
        addEventListener(type, handler, capture) { assert.ok(!listeners.has(type)); listeners.set(type, handler); changes.push(['add', type, capture]); },
        removeEventListener(type, handler, capture) { assert.equal(listeners.get(type), handler); listeners.delete(type); changes.push(['remove', type, capture]); },
        getBoundingClientRect() { return { x: 37, y: 55, width: 240, height: 120 }; },
        getContext() { throw new Error('transport must not read the live canvas'); },
        toDataURL() { throw new Error('transport must not read the live canvas'); } };
    document.activeElement = canvas;
    return { canvas, document, changes, listeners,
        key(overrides = {}) {
            const event = { target: canvas, code: 'Period', key: '>', ctrlKey: true, metaKey: false, shiftKey: true,
                altKey: false, isComposing: false, repeat: false, prevented: 0, stopped: 0,
                preventDefault() { this.prevented++; }, stopImmediatePropagation() { this.stopped++; }, ...overrides };
            listeners.get('keydown')?.(event); return event;
        } };
}
function harness(overrides = {}) {
    const target = canvasFixture(), downloads = [], completions = [], encoded = [], geometry = browserGeometry();
    const build = { schema: 'duxcel.web-build', signature: 'test-source', artifacts: { 'duxcel.wasm': wasmHash }, source: { revision: '6d613a0' } };
    let geometries = 0;
    const host = createReviewHost({ canvas: target.canvas, build, crypto: webcrypto,
        geometry: () => { geometries++; return geometry; },
        encodePNG(input) { encoded.push(input); return encodePNG(input); },
        download(blob, filename) { downloads.push({ blob, filename }); }, ...overrides });
    const identity = { reportId, frameId, logicalWidth: 8, logicalHeight: 4 };
    function manifest(change = {}, id = identity) {
        const value = { schema: 'duxcel.native-review', version: 1, report_id: id.reportId, session_id: host.sessionId,
            note: 'Türkçe 🦆 · SQL NULL ≠ empty text ≠ literal NULL',
            frame: { frame_id: id.frameId, window: { width: id.logicalWidth, height: id.logicalHeight }, framebuffer: { width: 4, height: 2 }, binary_sha256: host.binarySHA256 }, ...change };
        return encoder.encode(JSON.stringify(value));
    }
    function request(change = {}) {
        return { reportId: identity.reportId, frameId: identity.frameId, width: 4, height: 2, json: manifest(),
            rgba: Uint8Array.from({ length: 32 }, (_, index) => (index * 7 + 13) & 255),
            complete(ok, error) { completions.push({ ok, error }); }, ...change };
    }
    return { ...target, host, downloads, completions, encoded, geometry, build, identity, manifest, request,
        geometries: () => geometries,
        begin(change = {}) { return host.beginFrame({ ...identity, ...change }); } };
}

test('one UTF-8 USTAR archive binds exact C bytes, owned pixels, frozen transforms, build and real hashes', async () => {
    const encoding = deferred(), h = harness({ encodePNG: input => { h.encoded.push(input); return encoding.promise; } });
    assert.match(h.host.sessionId, /^S-[0-9a-f]{32}$/); assert.equal(h.host.binarySHA256, wasmHash);
    assert.equal(h.geometries(), 0); assert.equal(h.host.takeRequest(), false); assert.equal(h.begin(), true); assert.equal(h.geometries(), 1);
    const input = h.request(), expectedJSON = input.json.slice(), expectedRGBA = input.rgba.slice();
    const promise = h.host.exportReport(input);
    input.json.fill(0xdd); input.rgba.fill(0xee); h.geometry.canvasCSS.x = -999; h.geometry.canvasBacking.width = 99;
    h.build.artifacts['duxcel.wasm'] = 'a'.repeat(64); h.build.source.revision = 'mutated';
    assert.equal(h.encoded.length, 0); assert.equal(h.completions.length, 0);
    await tick(); assert.equal(h.encoded.length, 1); assert.deepEqual(h.encoded[0].rgba, expectedRGBA);
    assert.notEqual(h.encoded[0].rgba.buffer, input.rgba.buffer); encoding.resolve(encodePNG(h.encoded[0]));
    assert.equal(await promise, true); assert.deepEqual(h.completions, [{ ok: true, error: '' }]); assert.equal(h.downloads.length, 1);
    assert.equal(h.downloads[0].filename, `${reportId}.dxreview.tar`);
    const files = await parseTar(h.downloads[0].blob);
    assert.deepEqual([...files.keys()], [`${reportId}/manifest.json`, `${reportId}/frame.png`, `${reportId}/browser.json`]);
    assert.deepEqual(files.get(`${reportId}/manifest.json`), expectedJSON);
    const png = files.get(`${reportId}/frame.png`); assert.deepEqual(decodePNG(png), { rgba: expectedRGBA, width: 4, height: 2 });
    const sidecar = JSON.parse(decoder.decode(files.get(`${reportId}/browser.json`)));
    assert.equal(sidecar.schema, 'duxcel.browser-review'); assert.equal(sidecar.version, 1);
    assert.equal(sidecar.report_id, reportId); assert.equal(sidecar.frame_id, frameId); assert.equal(sidecar.session_id, h.host.sessionId);
    assert.equal(sidecar.binary_sha256, wasmHash); assert.equal(sidecar.build.artifacts['duxcel.wasm'], wasmHash); assert.equal(sidecar.build.source.revision, '6d613a0');
    assert.deepEqual(sidecar.logical, { width: 8, height: 4 }); assert.deepEqual(sidecar.canvas_css, { x: 37, y: 55, width: 240, height: 120 });
    assert.deepEqual(sidecar.canvas_backing, { width: 4, height: 2 }); assert.equal(sidecar.device_pixel_ratio, 2);
    assert.deepEqual(sidecar.visual_viewport, { scale: 1.25, offset_left: 11, offset_top: 7, width: 800, height: 600 });
    assert.deepEqual(sidecar.viewport, { width: 1024, height: 768, scroll_x: 23, scroll_y: 31 });
    assert.deepEqual(sidecar.transforms, { logical_to_css: { scale_x: 30, scale_y: 30, translate_x: 37, translate_y: 55 }, logical_to_framebuffer: { scale_x: 0.5, scale_y: 0.5 } });
    assert.equal(sidecar.user_agent, 'Browser/1 🦆'); assert.equal(sidecar.language, 'tr-TR');
    assert.deepEqual(sidecar.transport, { kind: 'browser-download', durable: false });
    assert.deepEqual(sidecar.files['manifest.json'], { sha256: sha256(expectedJSON), bytes: expectedJSON.length });
    assert.deepEqual(sidecar.files['frame.png'], { sha256: sha256(png), bytes: png.length, width: 4, height: 2 });
    assert.ok(Number.isFinite(sidecar.timing.owned_copy_ms) && sidecar.timing.owned_copy_ms >= 0);
    assert.ok(sidecar.timing.export_preparation_ms >= sidecar.timing.owned_copy_ms); assert.match(sidecar.timing.scope, /not input latency/);
    assert.equal(h.geometries(), 1, 'export never resamples live geometry'); h.host.dispose();
});

test('subarray offsets, Blob PNG encoder output and longest allowed report ID retain exact bytes', async () => {
    const h = harness({ encodePNG: input => new Blob([encodePNG(input)], { type: 'image/png' }) });
    const id = 'R'.repeat(95), identity = { ...h.identity, reportId: id, frameId: '18446744073709551615' };
    h.host.beginFrame(identity);
    const json = h.manifest({}, identity), jsonArena = new Uint8Array(json.length + 9); jsonArena.set(json, 3);
    const rgbaArena = new Uint8Array(40); rgbaArena.set(h.request().rgba, 5);
    assert.equal(await h.host.exportReport(h.request({ reportId: id, frameId: identity.frameId, json: jsonArena.subarray(3, 3 + json.length), rgba: rgbaArena.subarray(5, 37) })), true);
    const files = await parseTar(h.downloads[0].blob);
    assert.deepEqual([...files.keys()], [`${id}/manifest.json`, `${id}/frame.png`, `${id}/browser.json`]);
    assert.deepEqual(files.get(`${id}/manifest.json`), json); assert.deepEqual(decodePNG(files.get(`${id}/frame.png`)).rgba, rgbaArena.subarray(5, 37)); h.host.dispose();
});

test('shortcut capture consumes only a focused canvas chord before GLFW and queues one request', () => {
    const h = harness(); assert.deepEqual(h.changes, [['add', 'keydown', true]]);
    assert.equal(h.host.closed, false); assert.throws(() => { h.host.closed = true; }, TypeError); assert.equal(h.host.closed, false);
    for (const change of [{ ctrlKey: false, metaKey: false }, { shiftKey: false }, { altKey: true }, { isComposing: true }, { code: 'Comma', key: ',' }, { target: {} }]) {
        const event = h.key(change); assert.equal(event.prevented, 0); assert.equal(event.stopped, 0); assert.equal(h.host.takeRequest(), false);
    }
    h.document.activeElement = {}; const ignored = h.key(); assert.equal(ignored.prevented, 0); assert.equal(h.host.takeRequest(), false);
    h.document.activeElement = h.canvas;
    for (const change of [{}, { ctrlKey: false, metaKey: true }, { code: '', key: '.' }, { code: undefined, key: '>' }]) {
        const event = h.key(change); assert.equal(event.prevented, 1); assert.equal(event.stopped, 1);
        assert.equal(h.host.takeRequest(), true); assert.equal(h.host.takeRequest(), false);
    }
    const repeated = h.key({ repeat: true }); assert.equal(repeated.prevented, 1); assert.equal(repeated.stopped, 1); assert.equal(h.host.takeRequest(), false);
    h.key(); h.key(); assert.equal(h.host.takeRequest(), true); assert.equal(h.host.takeRequest(), false);
    assert.equal(h.geometries(), 0); assert.equal(h.encoded.length, 0); assert.equal(h.downloads.length, 0);
    h.key(); h.host.cancel(); assert.equal(h.host.takeRequest(), false);
    h.host.dispose(); assert.equal(h.host.closed, true); h.host.dispose(); assert.equal(h.host.closed, true);
    assert.deepEqual(h.changes, [['add', 'keydown', true], ['remove', 'keydown', true]]);
    assert.equal(h.listeners.size, 0); assert.equal(h.key().prevented, 0); assert.equal(h.host.takeRequest(), false);
});

test('window capture registered before GLFW intercepts only the focused canvas and removes the exact listener', () => {
    const target = canvasFixture(), handlers = [];
    const window = {
        addEventListener(type, handler, capture) { assert.equal(type, 'keydown'); assert.equal(capture, true); handlers.push(handler); },
        removeEventListener(type, handler, capture) {
            assert.equal(type, 'keydown'); assert.equal(capture, true); const index = handlers.indexOf(handler); assert.ok(index >= 0); handlers.splice(index, 1);
        }
    };
    target.document.defaultView = window;
    const h = harness({ canvas: target.canvas });
    assert.equal(target.listeners.size, 0, 'host does not wait for the canvas phase');
    assert.equal(handlers.length, 1); let glfwCalls = 0;
    window.addEventListener('keydown', () => glfwCalls++, true);
    function key(change = {}) {
        const event = { target: target.canvas, code: 'Period', ctrlKey: true, shiftKey: true, prevented: false, stopped: false,
            preventDefault() { this.prevented = true; }, stopImmediatePropagation() { this.stopped = true; }, ...change };
        for (const handler of handlers) { handler(event); if (event.stopped) break; } return event;
    }
    assert.equal(key().prevented, true); assert.equal(glfwCalls, 0); assert.equal(h.host.takeRequest(), true);
    assert.equal(key({ target: {} }).prevented, false); assert.equal(glfwCalls, 1); assert.equal(h.host.takeRequest(), false);
    target.document.activeElement = {}; assert.equal(key().prevented, false); assert.equal(glfwCalls, 2); assert.equal(h.host.takeRequest(), false);
    target.document.activeElement = target.canvas; key(); assert.equal(h.host.takeRequest(), true); assert.equal(glfwCalls, 2);
    h.host.dispose(); assert.equal(handlers.length, 1); assert.equal(key().prevented, false); assert.equal(glfwCalls, 3);
});

test('invalid constructor and synchronous capture metadata are rejected without corrupting a prior frame', async () => {
    const canvas = canvasFixture().canvas, valid = { canvas, build: { artifacts: { 'duxcel.wasm': wasmHash } }, crypto: webcrypto };
    for (const change of [{ canvas: null }, { crypto: null }, { crypto: { getRandomValues() {} } }, { build: {} },
        { build: { artifacts: { 'duxcel.wasm': wasmHash.toUpperCase() } } }, { encodePNG: 1 }, { geometry: false }, { download: 1 },
        { build: { artifacts: { 'duxcel.wasm': wasmHash }, oversized: 'x'.repeat(REVIEW_LIMITS.build) } }]) assert.throws(() => createReviewHost({ ...valid, ...change }));
    const h = harness(); h.begin();
    for (const change of [{ reportId: '../escape' }, { reportId: '' }, { reportId: 'R'.repeat(96) }, { reportId: '🦆' },
        { frameId: '01' }, { frameId: '-1' }, { frameId: '18446744073709551616' }, { frameId: 1 }, { logicalWidth: 0 },
        { logicalWidth: 1.5 }, { logicalHeight: Infinity }, { logicalWidth: 32768, logicalHeight: 32768 }]) assert.throws(() => h.begin(change));
    const badGeometries = [
        value => { value.canvasCSS.x = NaN; }, value => { value.canvasCSS.width = 0; }, value => { value.canvasBacking.width = 1.5; },
        value => { value.devicePixelRatio = 0; }, value => { value.visualViewport.scale = -1; }, value => { value.viewport.scrollY = Infinity; },
        value => { value.userAgent = 'x'.repeat(2049); }, value => { value.language = 'x'.repeat(129); }
    ];
    for (const mutate of badGeometries) {
        const validGeometry = structuredClone(h.geometry); mutate(h.geometry); assert.throws(() => h.begin());
        Object.assign(h.geometry, validGeometry);
    }
    assert.equal(await h.host.exportReport(h.request()), true, 'last valid frozen frame remains usable');
    h.host.dispose(); assert.throws(() => h.begin(), /closed/);
});

test('manifest identity, UTF-8, dimensions, byte bounds and missing capture fail asynchronously once', async () => {
    const cases = [
        h => ({ frameId: '9007199254740994' }), h => ({ reportId: 'different' }), h => ({ width: 2 }),
        h => ({ height: 0 }), h => ({ width: REVIEW_LIMITS.pixels + 1 }), h => ({ rgba: new Uint8Array(31) }),
        h => ({ json: new Uint8Array() }), h => ({ json: new Uint8Array(REVIEW_LIMITS.json + 1) }),
        h => ({ json: Uint8Array.of(0xc0, 0x80) }), h => ({ json: encoder.encode('{') }), h => ({ json: encoder.encode('null') }),
        h => ({ json: h.manifest({ schema: 'different' }) }), h => ({ json: h.manifest({ version: 2 }) }),
        h => ({ json: h.manifest({ session_id: 'wrong' }) }), h => ({ json: h.manifest({ report_id: 'wrong' }) }),
        h => ({ json: h.manifest({ frame: { frame_id: '1' } }) }),
        h => { const value = JSON.parse(decoder.decode(h.manifest())); value.frame.binary_sha256 = 'f'.repeat(64); return { json: encoder.encode(JSON.stringify(value)) }; },
        h => { const value = JSON.parse(decoder.decode(h.manifest())); value.frame.window.width++; return { json: encoder.encode(JSON.stringify(value)) }; },
        h => { const value = JSON.parse(decoder.decode(h.manifest())); value.frame.framebuffer.height++; return { json: encoder.encode(JSON.stringify(value)) }; }
    ];
    for (const change of cases) {
        const h = harness(); h.begin(); const promise = h.host.exportReport(h.request(change(h)));
        assert.equal(h.completions.length, 0, 'C is never reentered during export invocation');
        assert.equal(await promise, false); assert.equal(h.completions.length, 1); assert.equal(h.completions[0].ok, false);
        assert.ok(h.completions[0].error.length > 0 && h.completions[0].error.length <= 512);
        assert.equal(h.encoded.length, 0); assert.equal(h.downloads.length, 0); h.host.dispose();
    }
    const h = harness(); assert.equal(await h.host.exportReport(h.request()), false); assert.match(h.completions[0].error, /frozen frame/);
    h.begin(); assert.equal(await h.host.exportReport(h.request({ complete: undefined })), false); assert.equal(h.completions.length, 1);
    h.host.dispose(); assert.equal(await h.host.exportReport(h.request()), false); assert.equal(h.completions.length, 1, 'closed C sessions never receive a callback');
});

test('encoder/hash/download failure reports one useful error and preserves the frame for retry', async () => {
    for (const failure of ['encoder', 'png', 'dimensions', 'blob-limit', 'hash', 'digest-size', 'download']) {
        let fail = true, reads = 0;
        class OversizedBlob extends Blob { get size() { return REVIEW_LIMITS.png + 1; } async arrayBuffer() { reads++; throw new Error('must reject before reading'); } }
        const h = harness({
            encodePNG(input) {
                if (fail && failure === 'encoder') throw new Error('encoder failed 🦆');
                if (fail && failure === 'png') return Uint8Array.of(1, 2, 3);
                if (fail && failure === 'dimensions') return encodePNG({ ...input, width: 2, height: 4 });
                if (fail && failure === 'blob-limit') return new OversizedBlob();
                return encodePNG(input);
            },
            crypto: { getRandomValues: bytes => webcrypto.getRandomValues(bytes), subtle: { digest(...args) {
                if (fail && failure === 'hash') return Promise.reject(new Error('hash failed'));
                if (fail && failure === 'digest-size') return Promise.resolve(new ArrayBuffer(2));
                return webcrypto.subtle.digest(...args);
            } } },
            download(blob, filename) { if (fail && failure === 'download') throw new Error('download request failed'); h.downloads.push({ blob, filename }); }
        });
        h.begin(); assert.equal(await h.host.exportReport(h.request()), false, failure); assert.equal(h.completions.length, 1);
        assert.equal(h.completions[0].ok, false); assert.ok(h.completions[0].error.length > 0); assert.equal(h.downloads.length, 0); assert.equal(reads, 0);
        fail = false; assert.equal(await h.host.exportReport(h.request()), true, `${failure} retry`);
        assert.deepEqual(h.completions[1], { ok: true, error: '' }); assert.equal(h.downloads.length, 1); h.host.dispose();
    }
});

test('callback exceptions are contained and never produce a second callback or unhandled rejection', async () => {
    const h = harness(); h.begin(); let calls = 0;
    assert.equal(await h.host.exportReport(h.request({ complete() { calls++; throw new Error('C bridge ended'); } })), false);
    assert.equal(calls, 1); assert.equal(h.downloads.length, 1); h.host.dispose();
});

test('cancellation before work or during encoding suppresses downloads and stale completion', async () => {
    for (const boundary of ['before-work', 'during-encoding']) {
        const encoding = deferred(), h = harness({ encodePNG: input => { h.encoded.push(input); return encoding.promise; } });
        h.begin(); const pending = h.host.exportReport(h.request());
        if (boundary === 'during-encoding') { await tick(); assert.equal(h.encoded.length, 1); }
        h.host.cancel(); encoding.resolve(encodePNG({ rgba: h.request().rgba, width: 4, height: 2 }));
        assert.equal(await pending, false); assert.equal(h.downloads.length, 0); assert.equal(h.completions.length, 0);
        assert.equal(h.encoded.length, boundary === 'before-work' ? 0 : 1);
        assert.equal(await h.host.exportReport(h.request()), false); assert.equal(h.completions.length, 1); assert.match(h.completions[0].error, /frozen frame/); h.host.dispose();
    }
});

test('cancel during hashing, new capture, or disposal invalidates late async work without download', async () => {
    for (const action of ['cancel', 'new-frame', 'dispose']) {
        const hashes = [], h = harness({ crypto: { getRandomValues: bytes => webcrypto.getRandomValues(bytes), subtle: { digest(algorithm, bytes) {
            const result = deferred(); hashes.push({ ...result, bytes: bytes.slice() }); return result.promise;
        } } } });
        h.begin(); const pending = h.host.exportReport(h.request()); await tick(); assert.equal(hashes.length, 2);
        if (action === 'new-frame') h.begin({ frameId: '9007199254740994' }); else h.host[action]();
        for (const hash of hashes) hash.resolve(await webcrypto.subtle.digest('SHA-256', hash.bytes));
        assert.equal(await pending, false); assert.equal(h.downloads.length, 0); assert.equal(h.completions.length, 0); h.host.dispose();
    }
});

test('latest export wins while earlier encoding is pending, without publishing the old C callback', async () => {
    const first = deferred(); let encodes = 0;
    const h = harness({ encodePNG: input => ++encodes === 1 ? first.promise : encodePNG(input) });
    h.begin(); const old = h.host.exportReport(h.request()); await tick();
    const current = h.request(); current.rgba.fill(77); assert.equal(await h.host.exportReport(current), true);
    first.resolve(encodePNG({ rgba: h.request().rgba, width: 4, height: 2 })); assert.equal(await old, false);
    assert.deepEqual(h.completions, [{ ok: true, error: '' }]); assert.equal(h.downloads.length, 1);
    const files = await parseTar(h.downloads[0].blob); assert.ok(decodePNG(files.get(`${reportId}/frame.png`)).rgba.every(byte => byte === 77)); h.host.dispose();
});

test('an already requested download cannot be retracted, but close suppresses its pending C completion', async () => {
    const downloading = deferred(), h = harness({ download(blob, filename) { h.downloads.push({ blob, filename }); return downloading.promise; } });
    h.begin(); const pending = h.host.exportReport(h.request());
    for (let wait = 0; wait < 20 && h.downloads.length === 0; wait++) await tick();
    assert.equal(h.downloads.length, 1); h.host.dispose(); downloading.resolve(); assert.equal(await pending, false); assert.equal(h.completions.length, 0);
});

test('default encoder uses a separate OffscreenCanvas and default download releases Blob URLs', async () => {
    const previous = { OffscreenCanvas: globalThis.OffscreenCanvas, ImageData: globalThis.ImageData,
        create: URL.createObjectURL, revoke: URL.revokeObjectURL }, created = [], revoked = [], anchors = [], writes = [];
    class TestImageData { constructor(data, width, height) { this.data = data; this.width = width; this.height = height; } }
    class TestOffscreenCanvas {
        constructor(width, height) { this.width = width; this.height = height; }
        getContext(kind) { assert.equal(kind, '2d'); return { putImageData: (image, x, y) => { assert.equal(x, 0); assert.equal(y, 0); this.image = image; writes.push(image); } }; }
        async convertToBlob(options) { assert.deepEqual(options, { type: 'image/png' }); return new Blob([encodePNG({ rgba: this.image.data, width: this.width, height: this.height })]); }
    }
    globalThis.OffscreenCanvas = TestOffscreenCanvas; globalThis.ImageData = TestImageData;
    URL.createObjectURL = blob => { const url = `blob:test-${created.length}`; created.push({ blob, url }); return url; };
    URL.revokeObjectURL = url => revoked.push(url);
    const target = canvasFixture();
    target.document.body = { append(anchor) { anchors.push(anchor); } };
    target.document.createElement = tag => { assert.equal(tag, 'a'); return { click() { this.clicked = true; }, remove() { this.removed = true; } }; };
    let host;
    try {
        host = createReviewHost({ canvas: target.canvas, build: { artifacts: { 'duxcel.wasm': wasmHash } }, crypto: webcrypto });
        host.beginFrame({ reportId, frameId, logicalWidth: 8, logicalHeight: 4 });
        const h = harness(); const input = h.request({ json: h.manifest({ session_id: host.sessionId }) });
        assert.equal(await host.exportReport(input), true); assert.equal(writes.length, 1); assert.equal(anchors.length, 1);
        assert.equal(anchors[0].hidden, true); assert.equal(anchors[0].download, `${reportId}.dxreview.tar`); assert.equal(anchors[0].clicked, true); assert.equal(anchors[0].removed, true);
        assert.equal(created.length, 1); const files = await parseTar(created[0].blob);
        assert.deepEqual(decodePNG(files.get(`${reportId}/frame.png`)).rgba, input.rgba);
        host.dispose(); assert.deepEqual(revoked, ['blob:test-0']); h.host.dispose();
    } finally {
        host?.dispose();
        for (const key of ['OffscreenCanvas', 'ImageData']) if (previous[key] === undefined) delete globalThis[key]; else globalThis[key] = previous[key];
        URL.createObjectURL = previous.create; URL.revokeObjectURL = previous.revoke;
    }
});
