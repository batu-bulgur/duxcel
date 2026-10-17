import {test} from 'node:test';
import assert from 'node:assert/strict';
import {makeData,columns,initialFilters,filterRows,sortRows,selectionStats,statsFromValues,aggregateView} from '../src/data.js';
const raw=makeData();const filtered=filterRows(raw,initialFilters);const rows=sortRows(filtered,{column:'id',dir:'asc'});
test('synthetic dataset counts agree with the visible transformation chain',()=>{assert.equal(raw.length,48216);assert.equal(filterRows(raw,{status:initialFilters.status}).length,38450);assert.equal(filtered.length,36984);assert.equal(new Set(raw.map(r=>r.id)).size,raw.length);});
test('the four financial fixture cells produce the correct exact sum',()=>{const s=selectionStats(rows,columns,{r:5,r2:8,c:5,c2:5});assert.deepEqual([s.count,s.filled,s.numbers,s.distinct],[4,4,4,4]);assert.equal(s.sum,-224.62);assert.equal(s.average,-56.155);assert.equal(s.min,-18340);assert.equal(s.max,32800);});
test('blank, numeric, text, and distinct statistics have separate semantics',()=>{const s=statsFromValues([10,10,0,'10','Northstar','',null]);assert.deepEqual([s.count,s.filled,s.numbers,s.distinct,s.sum],[7,5,3,4,20]);});
test('sorting changes real row order while retaining all rows',()=>{const ordered=sortRows(filtered,{column:'amount',dir:'desc'});assert.equal(ordered.length,filtered.length);assert.ok(ordered.every((r,i)=>!i || ordered[i-1].amount>=r.amount));});
test('multi-field aggregation preserves the financial total',()=>{const result=aggregateView(filtered,{rows:['account','category'],columns:['year','quarter','month']});assert.equal(result.groups.reduce((s,g)=>s+g.count,0),filtered.length);assert.equal(result.groups.reduce((s,g)=>s+g.cents,0),filtered.reduce((s,r)=>s+r.cents,0));assert.equal(result.columnKeys.length,3);});
