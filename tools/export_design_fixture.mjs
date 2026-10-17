// Offline acceptance fixture only. The shipped C99 app never loads this module.
// Reuse the read-only design's data so native/browser comparisons have the same
// values, filters and aggregates instead of comparing unrelated sample tables.
import {mkdirSync,writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {makeData,columns,filterRows,initialFilters,aggregateView} from '../design/prototype/src/data.js';

process.chdir(fileURLToPath(new URL('..',import.meta.url)));
mkdirSync('tests/generated',{recursive:true});
const large=process.argv.includes('--large'),prefix=large?'design-large':'design-definition';
// App.jsx uses this deliberate sample, rather than makeData's full default.
const data=large?makeData():[...makeData(240),...makeData(40,48176).map(r=>({...r,currency:'USD'})),...makeData(30,37000)];
const quote=value=>value==null?'':`"${String(value).replaceAll('"','""')}"`;
const amount=cents=>`${cents<0?'-':''}${Math.floor(Math.abs(cents)/100)}.${String(Math.abs(cents)%100).padStart(2,'0')}`;
writeFileSync(`tests/generated/${prefix}.csv`,[
  columns.map(c=>quote(c.name)).join(','),
  ...data.map(row=>columns.map(c=>c.id==='amount'?amount(row.cents):quote(row[c.id])).join(','))
].join('\n')+'\n');
const sql=`COPY (SELECT * REPLACE ("Amount (USD)"::DECIMAL(18,2) AS "Amount (USD)") FROM read_csv('tests/generated/${prefix}.csv',header=true)) TO 'tests/generated/${prefix}.parquet' (FORMAT PARQUET)`;
const result=spawnSync('duckdb',['-no-stdin','-c',sql],{encoding:'utf8'});
if(result.status!==0)throw new Error(result.stderr||result.error||'Design fixture conversion failed');
const filtered=filterRows(data,initialFilters);
const matrix=aggregateView(filtered,{rows:['account','category'],columns:['year','quarter','month']});
writeFileSync(`tests/generated/${prefix}.expected.json`,JSON.stringify({
  rows:data.length,filteredRows:filtered.length,cents:filtered.reduce((sum,row)=>sum+row.cents,0),
  columns:columns.map(c=>({name:c.name,width:c.width})),
  groups:matrix.groups.map(g=>({dimensions:g.values,cents:g.cents,count:g.count})),
  columnKeys:matrix.columnKeys
},null,2)+'\n');
console.log(`Design fixture: ${data.length} real rows, ${filtered.length} filtered, ${matrix.groups.length} groups`);
