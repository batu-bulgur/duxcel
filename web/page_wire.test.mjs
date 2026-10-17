import test from 'node:test';
import assert from 'node:assert/strict';
import { encodePageWire } from './page_wire.mjs';

const encoder = new TextEncoder();
const decoder = new TextDecoder('utf-8', { fatal: true, ignoreBOM: true });
const column = (overrides = {}) => ({ name: 'v', typeName: 'VARCHAR', numberKind: 'none', precision: 0, scale: 0, ...overrides });
function page(values = [null, '', 'NULL'], columns = [column()], rows = values.length / columns.length) {
    const chunks = values.map(value => value === null ? new Uint8Array() : encoder.encode(value));
    const offsets = new Uint32Array(chunks.length + 1);
    for (let index = 0; index < chunks.length; index++) offsets[index + 1] = offsets[index] + chunks[index].length;
    const text = new Uint8Array(offsets.at(-1));
    for (let index = 0; index < chunks.length; index++) text.set(chunks[index], offsets[index]);
    return { schema: 'duxcel.page', version: 1, columns, columnCount: columns.length, rowCount: rows,
        text, offsets, nulls: Uint8Array.from(values, value => value === null ? 1 : 0) };
}
function inspect(wire) {
    const view = new DataView(wire.buffer, wire.byteOffset, wire.byteLength);
    let at = 0;
    const word = () => { const result = view.getUint32(at, true); at += 4; return result; };
    const bytes = length => { const result = wire.subarray(at, at + length); at += length; return result; };
    const [magic, version, columns, rows, textBytes] = Array.from({ length: 5 }, word);
    const metadata = Array.from({ length: columns }, () => {
        const [name, typeName, numberKind, precision, scale] = Array.from({ length: 5 }, word);
        return { name: decoder.decode(bytes(name)), typeName: decoder.decode(bytes(typeName)),
            numberKind: decoder.decode(bytes(numberKind)), precision, scale };
    });
    const offsets = Array.from({ length: columns * rows + 1 }, word);
    const nulls = Array.from(bytes(columns * rows));
    const text = bytes(textBytes);
    assert.equal(at, wire.length, 'DXP1 has no padding or trailing bytes');
    return { magic, version, columns, rows, textBytes, metadata, offsets, nulls,
        values: nulls.map((isNull, index) => isNull ? null : decoder.decode(text.subarray(offsets[index], offsets[index + 1]))) };
}
function rejects(value, message) { assert.throws(() => encodePageWire(value), { name: 'TypeError', message }); }

test('DXP1 little-endian golden bytes retain SQL NULL, empty text and literal NULL', () => {
    const wire = encodePageWire(page());
    assert.equal(Buffer.from(wire).toString('hex'), [
        '44585031', '01000000', '01000000', '03000000', '04000000',
        '01000000', '07000000', '04000000', '00000000', '00000000',
        '76', '56415243484152', '6e6f6e65',
        '00000000', '00000000', '00000000', '04000000', '010000', '4e554c4c',
    ].join(''));
    assert.deepEqual(inspect(wire).values, [null, '', 'NULL']);
});

test('exact integer/DECIMAL text and Unicode metadata cross without number coercion', () => {
    const columns = [column({ name: '城市 🦆', typeName: 'BIGINT', numberKind: 'i64' }),
        column({ name: 'Amount', typeName: 'DECIMAL(38,18)', numberKind: 'decimal', precision: 38, scale: 18 }),
        column({ name: '', typeName: '' })];
    const values = ['9007199254740993', '-12345678901234567890.123456789012345678', '\ufeffé中🦆',
        '-9223372036854775808', '99999999999999999999.999999999999999999', 'NULL'];
    const input = page(values, columns);
    for (const name of ['requestId', 'generation', 'documentId', 'sourceId', 'objectId', 'revision', 'offset', 'total']) {
        Object.defineProperty(input, name, { get() { throw new Error(`Must not read/coerce ${name}`); } });
    }
    const result = inspect(encodePageWire(input));
    assert.equal(result.magic, 0x31505844);
    assert.equal(result.version, 1);
    assert.equal(result.rows, 2);
    assert.deepEqual(result.metadata, columns);
    assert.deepEqual(result.values, values);
    assert.equal(result.textBytes, input.text.length);
});

test('every supported numeric kind retains exact text and metadata', () => {
    const kinds = ['none', 'i8', 'i16', 'i32', 'i64', 'i128', 'u8', 'u16', 'u32', 'u64', 'u128', 'decimal', 'float'];
    const columns = kinds.map(numberKind => column({ name: numberKind, numberKind,
        precision: numberKind === 'decimal' ? 38 : 0, scale: numberKind === 'decimal' ? 38 : 0 }));
    const values = ['text', '-128', '-32768', '-2147483648', '-9223372036854775808', '-170141183460469231731687303715884105728',
        '255', '65535', '4294967295', '18446744073709551615', '340282366920938463463374607431768211455',
        '0.12345678901234567890123456789012345678', '-0'];
    const result = inspect(encodePageWire(page(values, columns)));
    assert.deepEqual(result.metadata, columns);
    assert.deepEqual(result.values, values);
});

test('result is newly owned and transferable; caller subarrays remain attached and independent', () => {
    const input = page(['é', '', null, 'NULL']);
    for (const name of ['text', 'nulls', 'offsets']) {
        const original = input[name], backing = new original.constructor(original.length + 6);
        backing.set(original, 3); input[name] = backing.subarray(3, 3 + original.length);
    }
    const before = { text: input.text.slice(), nulls: input.nulls.slice(), offsets: input.offsets.slice() };
    const wire = encodePageWire(input);
    assert.equal(wire.byteOffset, 0);
    assert.equal(wire.byteLength, wire.buffer.byteLength);
    for (const name of ['text', 'nulls', 'offsets']) assert.notEqual(wire.buffer, input[name].buffer);
    wire[0] = 0;
    for (const name of ['text', 'nulls', 'offsets']) assert.deepEqual(input[name], before[name]);
    const second = encodePageWire(input), snapshot = second.slice();
    input.text.fill(120); input.offsets.fill(0); input.nulls.fill(1); input.columns[0].name = 'Changed';
    assert.deepEqual(second, snapshot);
    const received = structuredClone(second, { transfer: [second.buffer] });
    assert.equal(second.byteLength, 0);
    assert.deepEqual(received, snapshot);
    for (const name of ['text', 'nulls', 'offsets']) assert.ok(input[name].buffer.byteLength > 0);
});

test('dimensions, version and buffer shapes must match exactly', () => {
    for (const input of [null, {}, { ...page(), schema: 'other' }, { ...page(), version: 2 }]) rejects(input, /version 1/);
    for (const name of ['columnCount', 'rowCount']) for (const value of [-1, 257, 1.5, NaN, Infinity, '1', 1n, undefined]) {
        rejects({ ...page(), [name]: value }, /count exceeds/);
    }
    rejects({ ...page(), columns: {} }, /descriptors disagree/);
    rejects({ ...page(), columns: [] }, /descriptors disagree/);
    rejects({ ...page(), columns: [null] }, /descriptor required/);
    rejects({ ...page(), columns: Array(1) }, /descriptor required/);
    for (const name of ['text', 'offsets', 'nulls']) {
        rejects({ ...page(), [name]: [] }, /must be a non-shared/);
        const input = page();
        input[name] = new (name === 'offsets' ? Uint8Array : Uint32Array)(input[name].length);
        rejects(input, /must be a non-shared/);
    }
    rejects({ ...page(), offsets: new Uint32Array(3) }, /buffer lengths/);
    rejects({ ...page(), nulls: new Uint8Array(2) }, /buffer lengths/);
    for (const name of ['text', 'offsets', 'nulls']) {
        const input = page([], [], 0);
        structuredClone(input[name], { transfer: [input[name].buffer] });
        rejects(input, /detached/);
        if (typeof SharedArrayBuffer !== 'undefined') {
            const shared = page();
            shared[name] = new shared[name].constructor(new SharedArrayBuffer(shared[name].byteLength));
            rejects(shared, /must be a non-shared/);
        }
    }
});

test('metadata byte limits reject NUL, malformed UTF-16 and invalid numeric descriptors', () => {
    for (const name of ['name', 'typeName', 'numberKind']) {
        for (const value of [undefined, 0, {}, 'a\0b', '\ud800', '\udc00', '\ud800x', '\ud800\ud800']) {
            rejects(page([], [column({ [name]: value })], 0), /UTF-8 limit|NUL|unpaired surrogate/);
        }
    }
    const allowed = column({ name: 'é'.repeat(2048), typeName: '中'.repeat(42) + 'aa' });
    assert.deepEqual(inspect(encodePageWire(page([], [allowed], 0))).metadata, [allowed]);
    rejects(page([], [column({ name: 'é'.repeat(2049) })], 0), /UTF-8 limit/);
    rejects(page([], [column({ typeName: '中'.repeat(42) + 'aaa' })], 0), /UTF-8 limit/);
    rejects(page([], [column({ numberKind: 'a'.repeat(17) })], 0), /UTF-8 limit/);
    for (const fields of [{ numberKind: 'integer' }, { numberKind: 'I64' }, { precision: 1 }, { scale: 1 },
        { precision: undefined }, { scale: undefined }, { precision: '0' }, { scale: 0n },
        { numberKind: 'decimal', precision: 0 }, { numberKind: 'decimal', precision: 39 },
        { numberKind: 'decimal', precision: 38.5 }, { numberKind: 'decimal', precision: 1, scale: 2 },
        { numberKind: 'decimal', precision: 1, scale: -1 }, { numberKind: 'decimal', precision: 1, scale: 0.5 },
        { numberKind: 'decimal', precision: Infinity }, { numberKind: 'decimal', precision: 1, scale: NaN }]) {
        rejects(page([], [column(fields)], 0), /numeric kind, precision or scale/);
    }
    const integerScale = column({ numberKind: 'decimal', precision: 1, scale: 0 });
    assert.deepEqual(inspect(encodePageWire(page(['0'], [integerScale]))).metadata, [integerScale]);
});

test('offsets and NULL masks reject holes, regressions, out-of-bounds and bytes owned by NULL', () => {
    for (const offsets of [[1, 1, 1, 4], [0, 3, 2, 4], [0, 0, 5, 4], [0, 0, 0, 3]]) {
        rejects({ ...page(['a', '', 'abc']), offsets: Uint32Array.from(offsets) }, /text extent|monotonic/);
    }
    const marked = page(['a']); marked.nulls[0] = 1;
    rejects(marked, /nonempty NULL/);
    for (const mask of [2, 255]) {
        const input = page(['']); input.nulls[0] = mask;
        rejects(input, /invalid NULL marker/);
    }
});

test('cell UTF-8 rejects NUL, overlong encodings, surrogates, invalid scalars and split boundaries', () => {
    const invalid = [[0], [0x80], [0xc0, 0x80], [0xc1, 0xbf], [0xc2], [0xc2, 65],
        [0xe0, 0x80, 0x80], [0xed, 0xa0, 0x80], [0xed, 0xbf, 0xbf], [0xe2, 0x82],
        [0xf0, 0x80, 0x80, 0x80], [0xf4, 0x90, 0x80, 0x80], [0xf5, 0x80, 0x80, 0x80],
        [0xff], [97, 0, 98], [0xf0, 0x9f, 0xa6]];
    for (const bytes of invalid) {
        const input = page(['x']); input.text = Uint8Array.from(bytes); input.offsets[1] = bytes.length;
        rejects(input, /NUL|UTF-8/);
    }
    const split = page(['é', '']); split.offsets[1] = 1;
    rejects(split, /splits UTF-8/);
    const extremes = '\u0001\u007f\u0080\u07ff\u0800\ud7ff\ue000\u{10000}\u{10ffff}';
    assert.deepEqual(inspect(encodePageWire(page([extremes]))).values, [extremes]);
});

test('empty pages and maximum dimensions/text/metadata stay within the wire budget', () => {
    const empty = inspect(encodePageWire(page([], [], 0)));
    assert.equal(empty.columns, 0); assert.equal(empty.rows, 0);
    assert.deepEqual(empty.offsets, [0]); assert.deepEqual(empty.values, []);
    assert.equal(inspect(encodePageWire(page([], [], 256))).rows, 256);
    const columns = Array.from({ length: 256 }, () => column({ name: 'n'.repeat(4096), typeName: 't'.repeat(128) }));
    const values = Array(256 * 256).fill(null); values[0] = 'x'.repeat(4 * 1024 * 1024);
    const input = page(values, columns, 256), wire = encodePageWire(input);
    assert.ok(wire.byteLength < 6 * 1024 * 1024);
    const result = inspect(wire);
    assert.equal(result.columns, 256); assert.equal(result.rows, 256);
    assert.equal(result.textBytes, 4 * 1024 * 1024);
    assert.equal(result.values[0], values[0]);
    assert.ok(result.values.slice(1).every(value => value === null));
    const excessive = page(['x']); excessive.text = new Uint8Array(4 * 1024 * 1024 + 1);
    excessive.offsets[1] = excessive.text.length;
    rejects(excessive, /text exceeds 4 MiB/);
});
