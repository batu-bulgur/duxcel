/** Owned DXP1 transport for src/data/page_wire.c; no SQL or value coercion.
 * Request identity, exact offset/total and publication lifetime travel separately.
 * Caller buffers remain owned and attached; only the returned buffer may transfer.
 */
const MAX_ROWS = 256, MAX_COLUMNS = 256;
const MAX_COPY_CELLS=20000;
const MAX_TEXT = 4 * 1024 * 1024, MAX_WIRE = 6 * 1024 * 1024;
const NUMBER_KINDS = new Set(['none', 'i8', 'i16', 'i32', 'i64', 'i128',
    'u8', 'u16', 'u32', 'u64', 'u128', 'decimal', 'float']);
const encoder = new TextEncoder();

function check(condition, message) {
    if (!condition) throw new TypeError(`Invalid DXP1 page: ${message}`);
}
function count(value, maximum, name) {
    check(Number.isInteger(value) && value >= 0 && value <= maximum, `${name} exceeds its limit`);
    return value;
}
function buffer(value, Type, name) {
    check(value instanceof Type && value.buffer instanceof ArrayBuffer, `${name} must be a non-shared ${Type.name}`);
    // Detached empty views otherwise resemble valid empty pages. This creates a
    // view only, and never copies, retains or transfers the caller's allocation.
    try { new Uint8Array(value.buffer, value.byteOffset, value.byteLength); }
    catch { check(false, `${name} is detached`); }
    return value;
}
function metadataText(value, maximum, name) {
    check(typeof value === 'string' && value.length <= maximum, `${name} exceeds its UTF-8 limit`);
    // TextEncoder replaces lone UTF-16 surrogates; reject them rather than alter
    // a field name/type silently. U+0000 cannot be represented by C strings.
    for (let index = 0; index < value.length; index++) {
        const unit = value.charCodeAt(index);
        check(unit !== 0, `${name} contains NUL`);
        if (unit >= 0xd800 && unit <= 0xdbff) {
            const next = value.charCodeAt(++index);
            check(next >= 0xdc00 && next <= 0xdfff, `${name} contains an unpaired surrogate`);
        } else check(unit < 0xdc00 || unit > 0xdfff, `${name} contains an unpaired surrogate`);
    }
    const bytes = encoder.encode(value);
    check(bytes.byteLength <= maximum, `${name} exceeds its UTF-8 limit`);
    return bytes;
}
// Scalar-value UTF-8, checked per cell so no offset may split a character.
// Scan bytes directly: validation allocates no decoded strings per cell/page.
function cellText(text, start, end) {
    for (let index = start; index < end;) {
        const first = text[index++];
        check(first !== 0, 'cell text contains NUL');
        if (first < 0x80) continue;
        let remaining, value, minimum;
        if (first >= 0xc2 && first <= 0xdf) { remaining = 1; value = first & 0x1f; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { remaining = 2; value = first & 0x0f; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { remaining = 3; value = first & 7; minimum = 0x10000; }
        else check(false, 'cell text is not scalar UTF-8');
        check(remaining <= end - index, 'cell offset splits UTF-8');
        while (remaining--) {
            const next = text[index++];
            check((next & 0xc0) === 0x80, 'cell text is not scalar UTF-8');
            value = (value << 6) | (next & 0x3f);
        }
        check(value >= minimum && value <= 0x10ffff && (value < 0xd800 || value > 0xdfff),
            'cell text is not scalar UTF-8');
    }
}

/** Encode a duxcel.page v1 reply into a new, transferable Uint8Array.
 * All dimensions/metadata/buffers are validated before allocating the output.
 * Exact text bytes are authoritative and are never parsed as JavaScript numbers.
 */
export function encodePageWire(page,{purpose}={}) {
    check(purpose===undefined||purpose==='copy'||purpose==='workbook-sorts'||purpose==='workbook-objects','unknown transport purpose');
    check(page && typeof page === 'object' && page.schema === 'duxcel.page' && page.version === 1,
        'expected duxcel.page version 1');
    const columns = count(page.columnCount, MAX_COLUMNS, 'column count');
    const rows = count(page.rowCount, purpose==='copy'?MAX_COPY_CELLS:purpose==='workbook-sorts'?449:purpose==='workbook-objects'?2049:MAX_ROWS, 'row count');
    check(purpose!=='workbook-sorts'||columns===4,'workbook sorts require exactly four columns');
    check(purpose!=='workbook-objects'||columns===11,'workbook objects require exactly eleven columns');
    check(Array.isArray(page.columns) && page.columns.length === columns, 'column descriptors disagree with column count');
    const cells = columns * rows;
    check(purpose!=='copy'||cells<=MAX_COPY_CELLS,'copy exceeds 20,000 cells');
    const text = buffer(page.text, Uint8Array, 'text');
    const offsets = buffer(page.offsets, Uint32Array, 'offsets');
    const nulls = buffer(page.nulls, Uint8Array, 'NULL mask');
    check(text.byteLength <= MAX_TEXT, 'text exceeds 4 MiB');
    check(offsets.length === cells + 1 && nulls.length === cells, 'cell buffer lengths disagree with dimensions');
    check(offsets[0] === 0 && offsets[cells] === text.byteLength, 'text extent disagrees with offsets');

    let size = 20 + (cells + 1) * 4 + cells + text.byteLength;
    const descriptors = [];
    for (let index = 0; index < columns; index++) {
        const column = page.columns[index];
        check(column && typeof column === 'object', 'column descriptor required');
        const name = metadataText(column.name, 4096, 'column name');
        const type = metadataText(column.typeName, 128, 'SQL type');
        const kind = metadataText(column.numberKind, 16, 'numeric kind');
        const { precision, scale } = column;
        check(NUMBER_KINDS.has(column.numberKind) && Number.isInteger(precision) && Number.isInteger(scale) &&
            (column.numberKind === 'decimal' ? precision >= 1 && precision <= 38 && scale >= 0 && scale <= precision :
                precision === 0 && scale === 0), 'invalid numeric kind, precision or scale');
        size += 20 + name.byteLength + type.byteLength + kind.byteLength;
        descriptors.push({ name, type, kind, precision, scale });
    }
    check(size <= MAX_WIRE, 'transport exceeds 6 MiB');
    for (let index = 0, start = 0; index < cells; index++) {
        const end = offsets[index + 1];
        check(end >= start && end <= text.byteLength, 'cell offsets are not monotonic or exceed text');
        check(nulls[index] <= 1 && (nulls[index] === 0 || start === end), 'invalid NULL marker or nonempty NULL');
        cellText(text, start, end);
        start = end;
    }

    const owned = new Uint8Array(size), view = new DataView(owned.buffer);
    let at = 0;
    const word = value => { view.setUint32(at, value, true); at += 4; };
    const bytes = value => { owned.set(value, at); at += value.byteLength; };
    word(0x31505844); word(1); word(columns); word(rows); word(text.byteLength);
    for (const column of descriptors) {
        word(column.name.byteLength); word(column.type.byteLength); word(column.kind.byteLength);
        word(column.precision); word(column.scale);
        bytes(column.name); bytes(column.type); bytes(column.kind);
    }
    for (let index = 0; index < offsets.length; index++) word(offsets[index]);
    bytes(nulls); bytes(text);
    return owned;
}
