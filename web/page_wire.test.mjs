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
test('copy transport purpose alone permits a bounded 20,000-cell envelope without changing normal pages',()=>{
    const values=Array.from({length:20000},(_,i)=>[null,'','NULL','9007199254740993'][i%4]);
    const input=page(values);
    assert.throws(()=>encodePageWire(input),/row count/);
    assert.deepEqual(inspect(encodePageWire(input,{purpose:'copy'})).values,values);
    assert.throws(()=>encodePageWire(page([...values,'x']),{purpose:'copy'}),/row count/);
    assert.throws(()=>encodePageWire(page([...values,'x','y'],[column(),column({name:'w'})]),{purpose:'copy'}),/20,000 cells/);
    for(const purpose of [null,false,'page','Copy',{},1])assert.throws(()=>encodePageWire(page(),{purpose}),/unknown transport purpose/);
    assert.throws(()=>encodePageWire(page(['x'.repeat(4*1024*1024+1)]),{purpose:'copy'}),/4 MiB/);
});

test('trusted workbook-sorts wire envelope permits 449 by four without changing normal or copy policies',()=>{
    const columns=[column({name:'sheet_id',typeName:'INTEGER',numberKind:'i32'}),column({name:'position',typeName:'INTEGER',numberKind:'i32'}),
        column({name:'name'}),column({name:'descending',typeName:'BOOLEAN'})];
    const values=Array.from({length:449},(_,i)=>['63','6',[null,'','NULL','9007199254740993','18446744073709551615','İstanbul 🦆'][i%6],i%2?'true':'false']).flat();
    const input=page(values,columns),before={text:input.text.slice(),offsets:input.offsets.slice(),nulls:input.nulls.slice()};
    assert.throws(()=>encodePageWire(input),/row count/);
    const wire=encodePageWire(input,{purpose:'workbook-sorts'}),output=inspect(wire);
    assert.equal(output.rows,449);assert.equal(output.columns,4);assert.deepEqual(output.values,values);assert.deepEqual(output.metadata,columns);
    for(const part of ['text','offsets','nulls'])assert.deepEqual(input[part],before[part]);
    assert.throws(()=>encodePageWire(page([...values,'0','0','extra','false'],columns),{purpose:'workbook-sorts'}),/row count/);
    for(const arity of [0,1,3,5])assert.throws(()=>encodePageWire(page(Array(arity).fill(null),Array.from({length:arity},()=>column()),1),{purpose:'workbook-sorts'}),/column/);
    assert.throws(()=>encodePageWire(page(Array(257*4).fill(null),columns),{purpose:'unknown'}),/unknown transport purpose/);
    assert.equal(inspect(encodePageWire(page([],columns,0),{purpose:'workbook-sorts'})).rows,0);
    assert.throws(()=>encodePageWire(page(Array(257*4).fill(null),columns)),/row count/);
    assert.deepEqual(inspect(encodePageWire(input,{purpose:'copy'})).values,values);
    const received=structuredClone(wire,{transfer:[wire.buffer]});assert.equal(wire.length,0);assert.deepEqual(inspect(received).values,values);
});
test('workbook-sorts wire retains the same text limit and rejects invalid cells atomically',()=>{
    const columns=Array.from({length:4},(_,i)=>column({name:'c'+i}));
    const values=['x'.repeat(4*1024*1024-4),null,'','NULL'],input=page(values,columns);
    assert.deepEqual(inspect(encodePageWire(input,{purpose:'workbook-sorts'})).values,values);
    assert.throws(()=>encodePageWire(page(['x'.repeat(4*1024*1024-3),null,'','NULL'],columns),{purpose:'workbook-sorts'}),/4 MiB/);
    const malformed=page(['a\0b',null,'','NULL'],columns),before=malformed.text.slice();
    assert.throws(()=>encodePageWire(malformed,{purpose:'workbook-sorts'}),/NUL/);assert.deepEqual(malformed.text,before);
    assert.throws(()=>encodePageWire(page(['x',null,'','NULL'],columns),{purpose:'Workbook-sorts'}),/unknown transport purpose/);
});

function objectColumns(){
    const names=['sheet_id','id','name','row_id','column_id','height','width','kind','mark','source_id','chart_measure'];
    return names.map((name,i)=>column({name,typeName:i===2?'VARCHAR':[1,3,5,9].includes(i)?'UBIGINT':'INTEGER',
        numberKind:i===2?'none':[1,3,5,9].includes(i)?'u64':'i32'}));
}
test('trusted workbook-objects encodes one 2049 by eleven result without relaxing existing purposes',()=>{
    const columns=objectColumns(),names=[null,'','NULL',"Owner 'İ' 🦆",'9007199254740993','18446744073709551615'];
    const values=Array.from({length:2049},(_,i)=>['63','18446744073709551615',names[i%names.length],
        '9007199254740993','16383','12','128','0','0','9007199254740994','0']).flat();
    const input=page(values,columns),before={text:input.text.slice(),offsets:input.offsets.slice(),nulls:input.nulls.slice()};
    assert.throws(()=>encodePageWire(input),/row count/);
    assert.throws(()=>encodePageWire(input,{purpose:'copy'}),/20,000 cells/);
    assert.throws(()=>encodePageWire(input,{purpose:'workbook-sorts'}),/row count/);
    const wire=encodePageWire(input,{purpose:'workbook-objects'}),output=inspect(wire);
    assert.equal(output.rows,2049);assert.equal(output.columns,11);assert.deepEqual(output.metadata,columns);assert.deepEqual(output.values,values);
    for(const part of ['text','offsets','nulls'])assert.deepEqual(input[part],before[part]);
    const valid=inspect(encodePageWire(page(values.slice(0,2048*11),columns),{purpose:'workbook-objects'}));assert.equal(valid.rows,2048);
    assert.throws(()=>encodePageWire(page([...values,...values.slice(0,11)],columns),{purpose:'workbook-objects'}),/row count/);
    for(const count of [0,10,12])assert.throws(()=>encodePageWire(page(Array(count).fill(null),Array.from({length:count},()=>column()),1),{purpose:'workbook-objects'}),/column/);
    assert.equal(inspect(encodePageWire(page([],columns,0),{purpose:'workbook-objects'})).rows,0);
    // An owned transport cannot retain any caller view, including subarrays.
    input.text.fill(0);input.offsets.fill(0);input.nulls.fill(1);assert.deepEqual(inspect(wire).values,values);
    const received=structuredClone(wire,{transfer:[wire.buffer]});assert.equal(wire.length,0);assert.deepEqual(inspect(received).values,values);
});
test('workbook-objects retains exact text bounds and rejects malformed owned inputs before encoding',()=>{
    const columns=objectColumns(),values=Array(11).fill(null);values[2]='x'.repeat(4*1024*1024);
    const input=page(values,columns);assert.deepEqual(inspect(encodePageWire(input,{purpose:'workbook-objects'})).values,values);
    values[2]+='x';assert.throws(()=>encodePageWire(page(values,columns),{purpose:'workbook-objects'}),/4 MiB/);
    for(const text of ['a\0b','é🦆']){
        const values=Array(11).fill(null);values[2]=text;const malformed=page(values,columns),before=malformed.text.slice();
        if(text.includes('\0'))assert.throws(()=>encodePageWire(malformed,{purpose:'workbook-objects'}),/NUL/);
        else{malformed.text[0]=0xff;before[0]=0xff;assert.throws(()=>encodePageWire(malformed,{purpose:'workbook-objects'}),/UTF-8/);}
        assert.deepEqual(malformed.text,before);
    }
    const malformed=page(Array(11).fill(null),columns);malformed.nulls[0]=2;
    assert.throws(()=>encodePageWire(malformed,{purpose:'workbook-objects'}),/NULL/);
    for(const purpose of ['Workbook-objects','workbook-objects ',{},false])assert.throws(()=>encodePageWire(page([],columns,0),{purpose}),/unknown transport purpose/);
});

test('trusted workbook-owners encodes exactly three columns through2113 rows without changing existing envelopes',()=>{
    const columns=[column({name:'sheet_id',typeName:'INTEGER',numberKind:'i32'}),column({name:'object_id',typeName:'UBIGINT',numberKind:'u64'}),column({name:'query'})];
    const texts=[null,'','NULL',"SELECT 'İstanbul 🦆', 'apostrophe''s' FROM input;",'9007199254740993','18446744073709551615'];
    const values=Array.from({length:2113},(_,i)=>['63',['0','9007199254740993','18446744073709551615'][i%3],texts[i%6]]).flat();
    const input=page(values,columns),before={text:input.text.slice(),offsets:input.offsets.slice(),nulls:input.nulls.slice()};
    assert.throws(()=>encodePageWire(input),/row count/);assert.throws(()=>encodePageWire(input,{purpose:'workbook-sorts'}),/row count/);assert.throws(()=>encodePageWire(input,{purpose:'workbook-objects'}),/row count/);
    const wire=encodePageWire(input,{purpose:'workbook-owners'}),output=inspect(wire);assert.equal(output.rows,2113);assert.equal(output.columns,3);assert.deepEqual(output.metadata,columns);assert.deepEqual(output.values,values);
    for(const part of ['text','offsets','nulls'])assert.deepEqual(input[part],before[part]);
    for(const rows of [2048,2112])assert.equal(inspect(encodePageWire(page(values.slice(0,rows*3),columns),{purpose:'workbook-owners'})).rows,rows);
    assert.throws(()=>encodePageWire(page([...values,'0','0','sentinel'],columns),{purpose:'workbook-owners'}),/row count/);
    for(const count of [0,2,4])assert.throws(()=>encodePageWire(page(Array(count).fill(null),Array.from({length:count},()=>column()),1),{purpose:'workbook-owners'}),/column/);
    assert.equal(inspect(encodePageWire(page([],columns,0),{purpose:'workbook-owners'})).rows,0);
    input.text.fill(0);input.offsets.fill(0);input.nulls.fill(1);assert.deepEqual(inspect(wire).values,values);
    const received=structuredClone(wire,{transfer:[wire.buffer]});assert.equal(wire.length,0);assert.deepEqual(inspect(received).values,values);
    const options=[columns[0],columns[1],column({name:'unique_rows',typeName:'BOOLEAN'})],booleans=['63','18446744073709551615','true','63','0','false','63','9007199254740993',null];
    assert.deepEqual(inspect(encodePageWire(page(booleans,options),{purpose:'workbook-owners'})).values,booleans);
});
test('workbook-owners keeps the unchanged text bound and rejects malformed strings and masks atomically',()=>{
    const columns=[column({name:'sheet_id',typeName:'INTEGER',numberKind:'i32'}),column({name:'object_id',typeName:'UBIGINT',numberKind:'u64'}),column({name:'query'})];
    const values=[null,null,'x'.repeat(4*1024*1024)],input=page(values,columns);assert.deepEqual(inspect(encodePageWire(input,{purpose:'workbook-owners'})).values,values);
    values[2]+='x';assert.throws(()=>encodePageWire(page(values,columns),{purpose:'workbook-owners'}),/4 MiB/);
    const nul=page(['0','0','a\0b'],columns),before=nul.text.slice();assert.throws(()=>encodePageWire(nul,{purpose:'workbook-owners'}),/NUL/);assert.deepEqual(nul.text,before);
    const bad=page(['0','0','İ🦆'],columns);bad.text[2]=0xff;const raw=bad.text.slice();assert.throws(()=>encodePageWire(bad,{purpose:'workbook-owners'}),/UTF-8/);assert.deepEqual(bad.text,raw);
    const mask=page([null,null,null],columns);mask.nulls[0]=2;assert.throws(()=>encodePageWire(mask,{purpose:'workbook-owners'}),/NULL/);
    for(const purpose of ['Workbook-owners','workbook-owners ',{},false])assert.throws(()=>encodePageWire(page([],columns,0),{purpose}),/unknown transport purpose/);
});

const tableWirePolicies=[
    {purpose:'workbook-table-sources',rows:2049,columns:14},
    {purpose:'workbook-table-filters',rows:30721,columns:6},
    {purpose:'workbook-table-sorts',rows:14337,columns:5},
];
function tableWireColumns(policy){
    return Array.from({length:policy.columns},(_,i)=>column(i===0?{name:'sheet_id',typeName:'INTEGER',numberKind:'i32'}:
        i===1?{name:'object_id',typeName:'UBIGINT',numberKind:'u64'}:i===2?{name:'enabled',typeName:'BOOLEAN'}:{name:'text_'+i}));
}
for(const policy of tableWirePolicies)test(`${policy.purpose} wire has a fixed independent full envelope and transferable ownership`,()=>{
    const columns=tableWireColumns(policy),texts=[null,'','NULL',"İstanbul 🦆 'quoted'"];
    const values=Array.from({length:policy.rows},(_,r)=>columns.map((_,i)=>i===0?'63':i===1?'18446744073709551615':
        i===2?[null,'true','false'][r%3]:texts[(r+i)%4])).flat();
    const input=page(values,columns),before={text:input.text.slice(),offsets:input.offsets.slice(),nulls:input.nulls.slice()};
    input.purpose=policy.purpose;assert.throws(()=>encodePageWire(input),/row count/); // Wire data cannot select its own policy.
    for(const purpose of ['copy','workbook-sorts','workbook-objects','workbook-owners',...tableWirePolicies.filter(x=>x!==policy).map(x=>x.purpose)])
        assert.throws(()=>encodePageWire(input,{purpose}),/row count|column|20,000 cells/);
    const wire=encodePageWire(input,{purpose:policy.purpose}),output=inspect(wire);
    assert.equal(output.rows,policy.rows);assert.equal(output.columns,policy.columns);assert.deepEqual(output.metadata,columns);
    for(const r of [0,1,2,3,policy.rows-1])for(const c of [0,1,2,3,policy.columns-1])assert.equal(output.values[r*policy.columns+c],values[r*policy.columns+c]);
    for(const part of ['text','offsets','nulls'])assert.deepEqual(input[part],before[part]);
    assert.throws(()=>encodePageWire(page([...values,...values.slice(0,policy.columns)],columns),{purpose:policy.purpose}),/row count/);
    for(const count of [0,policy.columns-1,policy.columns+1])assert.throws(()=>encodePageWire(page(Array(count).fill(null),Array.from({length:count},()=>column()),1),{purpose:policy.purpose}),/column/);
    assert.equal(inspect(encodePageWire(page([],columns,0),{purpose:policy.purpose})).rows,0);
    input.text.fill(0);input.offsets.fill(0);input.nulls.fill(1);input.columns[0].name='changed';
    assert.equal(inspect(wire).values.at(-policy.columns+1),'18446744073709551615');
    const received=structuredClone(wire,{transfer:[wire.buffer]});assert.equal(wire.byteLength,0);
    assert.equal(inspect(received).values.at(-policy.columns+1),'18446744073709551615');
});
test('Table wire policies retain exact text admission and reject malformed masks, NUL, UTF8 and prototype names',()=>{
    for(const policy of tableWirePolicies){
        const columns=tableWireColumns(policy),values=Array(policy.columns).fill(null);values[3]='x'.repeat(4*1024*1024);
        assert.equal(inspect(encodePageWire(page(values,columns),{purpose:policy.purpose})).textBytes,4*1024*1024);
        values[3]+='x';assert.throws(()=>encodePageWire(page(values,columns),{purpose:policy.purpose}),/4 MiB/);
        for(const text of ['a\0b','İ🦆']){
            const values=Array(policy.columns).fill(null);values[3]=text;const bad=page(values,columns);
            if(text.includes('\0')){const before=bad.text.slice();assert.throws(()=>encodePageWire(bad,{purpose:policy.purpose}),/NUL/);assert.deepEqual(bad.text,before);}
            else{bad.text[0]=0xff;const before=bad.text.slice();assert.throws(()=>encodePageWire(bad,{purpose:policy.purpose}),/UTF-8/);assert.deepEqual(bad.text,before);}
        }
        const mask=page(Array(policy.columns).fill(null),columns);mask.nulls[0]=2;assert.throws(()=>encodePageWire(mask,{purpose:policy.purpose}),/NULL/);
        for(const purpose of ['__proto__','constructor','toString',new String(policy.purpose),Object.create({rows:30721,columns:6})])
            assert.throws(()=>encodePageWire(page([],columns,0),{purpose}),/unknown transport purpose/);
    }
});

// BEGIN Pivot/Marks wire tests: no original-schema or owner semantics.
const pivotMarksWirePolicies=[
    {purpose:'workbook-pivot-fields',rows:65537,columns:9},
    {purpose:'workbook-marks',rows:98305,columns:8},
];
for(const policy of pivotMarksWirePolicies)test(`${policy.purpose} sparse full wire envelope retains one transferable owned result`,()=>{
    const columns=tableWireColumns(policy),values=Array(policy.rows*policy.columns).fill(null);
    const first=['63','18446744073709551615','true','', 'NULL',"İstanbul 🦆 'quoted'"];
    values.splice(0,first.length,...first);values.splice((policy.rows-1)*policy.columns,3,'0','9007199254740993','false');
    const input=page(values,columns),before={text:input.text.slice(),offsets:input.offsets.slice(),nulls:input.nulls.slice()};
    input.purpose=policy.purpose;assert.throws(()=>encodePageWire(input),/row count/);
    for(const purpose of ['copy','workbook-sorts','workbook-objects','workbook-owners',...tableWirePolicies.map(x=>x.purpose),...pivotMarksWirePolicies.filter(x=>x!==policy).map(x=>x.purpose)])
        assert.throws(()=>encodePageWire(input,{purpose}),/row count|column|20,000 cells/);
    const wire=encodePageWire(input,{purpose:policy.purpose});assert.ok(wire.byteLength<=6*1024*1024);
    for(const part of ['text','offsets','nulls'])assert.deepEqual(input[part],before[part]);
    assert.throws(()=>encodePageWire(page([...values,...Array(policy.columns).fill(null)],columns),{purpose:policy.purpose}),/row count/);
    for(const count of [0,policy.columns-1,policy.columns+1])assert.throws(()=>encodePageWire(page(Array(count).fill(null),Array.from({length:count},()=>column()),1),{purpose:policy.purpose}),/column/);
    input.text.fill(0);input.offsets.fill(0);input.nulls.fill(1);input.columns[0].name='changed';
    const received=structuredClone(wire,{transfer:[wire.buffer]});assert.equal(wire.byteLength,0);
    const output=inspect(received);assert.equal(output.rows,policy.rows);assert.equal(output.columns,policy.columns);assert.deepEqual(output.metadata,columns.map((c,i)=>i?c:{...c,name:'sheet_id'}));
    for(const r of [0,1,policy.rows-1])for(const c of [0,1,2,3,4,5,policy.columns-1])assert.equal(output.values[r*policy.columns+c],values[r*policy.columns+c]);
    assert.equal(inspect(encodePageWire(page([],tableWireColumns(policy),0),{purpose:policy.purpose})).rows,0);
});
test('Pivot/Marks wire keeps text limits and rejects malformed bytes or prototype policy names without mutation',()=>{
    for(const policy of pivotMarksWirePolicies){
        const columns=tableWireColumns(policy),values=Array(policy.columns).fill(null);values[3]='x'.repeat(4*1024*1024);
        const exact=encodePageWire(page(values,columns),{purpose:policy.purpose});assert.equal(new DataView(exact.buffer,exact.byteOffset).getUint32(16,true),4*1024*1024);
        values[3]+='x';assert.throws(()=>encodePageWire(page(values,columns),{purpose:policy.purpose}),/4 MiB/);
        for(const value of ['a\0b','İ🦆']){
            const entries=Array(policy.columns).fill(null);entries[3]=value;const bad=page(entries,columns);
            if(value.includes('\0')){const before=bad.text.slice();assert.throws(()=>encodePageWire(bad,{purpose:policy.purpose}),/NUL/);assert.deepEqual(bad.text,before);}
            else{bad.text[0]=0xff;const before=bad.text.slice();assert.throws(()=>encodePageWire(bad,{purpose:policy.purpose}),/UTF-8/);assert.deepEqual(bad.text,before);}
        }
        const mask=page(Array(policy.columns).fill(null),columns);mask.nulls[0]=2;assert.throws(()=>encodePageWire(mask,{purpose:policy.purpose}),/NULL/);
        for(const purpose of ['__proto__','constructor','toString',new String(policy.purpose),Object.create({rows:98305,columns:8})])
            assert.throws(()=>encodePageWire(page([],columns,0),{purpose}),/unknown transport purpose/);
    }
});
// END Pivot/Marks wire tests.

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
