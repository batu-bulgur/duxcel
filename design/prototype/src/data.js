export const columns = [
  { id: 'date', name: 'Date', type: 'date', width: 112 },
  { id: 'id', name: 'Transaction ID', type: 'text', width: 146 },
  { id: 'counterparty', name: 'Counterparty', type: 'text', width: 184 },
  { id: 'category', name: 'Category', type: 'text', width: 150 },
  { id: 'account', name: 'Account', type: 'text', width: 116 },
  { id: 'amount', name: 'Amount (USD)', type: 'number', width: 146 },
  { id: 'status', name: 'Status', type: 'text', width: 100 },
  { id: 'currency', name: 'Currency', type: 'text', width: 94 },
  { id: 'month', name: 'Month', type: 'derived', width: 118 },
  { id: 'region', name: 'Region', type: 'text', width: 132 },
  { id: 'reference', name: 'Reference', type: 'text', width: 156 },
  { id: 'memo', name: 'Memo', type: 'text', width: 220 },
];
const first = [
  ['2026-09-14', 'TX-0914-1042', 'Northstar Labs', 'Customer receipt', 12845000],
  ['2026-09-14', 'TX-0914-1043', 'Atlas Supply Co.', 'Inventory', -2486000],
  ['2026-09-14', 'TX-0914-1044', 'Harbor Cloud', 'Software', -485000],
  ['2026-09-15', 'TX-0915-1045', 'Payroll batch', 'Payroll', -9642000],
  ['2026-09-15', 'TX-0915-1046', 'Meridian Retail', 'Customer receipt', 7420000],
  ['2026-09-15', 'TX-0915-1047', 'City Properties', 'Rent', -1250000],
  ['2026-09-16', 'TX-0916-1048', 'Acme Components', 'Inventory', -1834000],
  ['2026-09-16', 'TX-0916-1049', 'Juniper Studio', 'Customer receipt', 3280000],
  ['2026-09-16', 'TX-0916-1050', 'Metro Utilities', 'Utilities', -218462],
  ['2026-09-17', 'TX-0917-1051', 'Summit Logistics', 'Freight', -764000],
  ['2026-09-17', 'TX-0917-1052', 'Bluepeak Media', 'Software', -312000],
  ['2026-09-18', 'TX-0918-1053', 'Pinecrest Capital', 'Customer receipt', 4790000],
];
const partners = ['Northstar Labs', 'Atlas Supply Co.', 'Harbor Cloud', 'Meridian Retail', 'City Properties', 'Acme Components', 'Juniper Studio', 'Metro Utilities', 'Summit Logistics', 'Bluepeak Media', 'Pinecrest Capital', 'Redwood Systems', 'Lakeview Solutions', 'Clearwater Inc.', 'Crescent Manufacturing', 'Oakridge Ventures', 'Vertex Technology', 'Maple Office Supply', 'Ridgeway Telecom', 'Westlake Industries'];
const categories = ['Customer receipt', 'Inventory', 'Software', 'Payroll', 'Rent', 'Utilities', 'Freight', 'Equipment', 'Marketing', 'Interest'];
export function makeData(length=48216,offset=0) {
  return Array.from({ length }, (_, index) => {
    const i=index+offset;
    const seed = first[i];
    const month = 7 + (i * 7 % 3), day = 1 + (i * 11 % 28);
    const date = seed?.[0] || `2026-${String(month).padStart(2,'0')}-${String(day).padStart(2,'0')}`;
    const category = seed?.[3] || categories[(i * 7) % categories.length];
    const positive = category === 'Customer receipt' || category === 'Interest';
    const cents = seed?.[4] ?? ((positive ? 1 : -1) * (25000 + (i * 7919 % (positive ? 12500000 : 2400000))));
    return { sourceIndex: i, date, id: seed?.[1] || `TX-${date.slice(2,7).replace('-','')}-${String(i + 1).padStart(6,'0')}`, counterparty: seed?.[2] || partners[i % partners.length], category, account: i < 12 ? 'Operating' : ['Operating', 'Reserve', 'Payroll'][i % 7 < 5 ? 0 : i % 7 === 5 ? 1 : 2], cents, amount: cents / 100, status: i < 38450 ? 'Posted' : i % 2 ? 'Pending' : 'Reversed', currency: i < 36984 ? 'USD' : i % 3 ? 'EUR' : 'GBP', month: date.slice(0,7), region: ['North America','Europe','Asia Pacific'][i % 3], reference: i % 23 === 0 ? null : `INV-${String(100000+i).padStart(6,'0')}`, memo: i % 19 === 0 ? '' : i % 31 === 0 ? null : ['Quarterly settlement','Bank transfer','Recurring payment','Invoice cleared','Batch settlement'][i % 5] };
  });
}
export const initialFilters = { status: { values: ['Posted'] }, currency: { values: ['USD'] } };
export const keyOf = value => value === null || value === undefined ? '__NULL__' : value === '' ? '__EMPTY__' : String(value);
export const valueLabel = value => value === '__NULL__' ? '(NULL)' : value === '__EMPTY__' ? '(Empty string)' : value;
export function matches(row, id, filter) {
  if (!filter) return true;
  const value = row[id];
  if (filter.values && !filter.values.includes(keyOf(value))) return false;
  if (!filter.op || !filter.term) return true;
  const term = filter.term;
  if (filter.op === 'contains') return String(value ?? '').toLowerCase().includes(term.toLowerCase());
  if (filter.op === 'equals') return String(value ?? '') === term;
  if (filter.op === 'greater') return id === 'amount' ? value > Number(term) : String(value) > term;
  if (filter.op === 'less') return id === 'amount' ? value < Number(term) : String(value) < term;
  if (filter.op === 'between') return value >= (id === 'amount' ? Number(term) : term) && value <= (id === 'amount' ? Number(filter.term2) : filter.term2);
  return true;
}
export function filterRows(source, filters, omit) { return source.filter(row => Object.entries(filters).every(([id, filter]) => id === omit || matches(row,id,filter))); }
export function sortRows(rows, sort) {
  if (!sort?.column) return rows;
  const dir = sort.dir === 'desc' ? -1 : 1;
  return [...rows].sort((a,b) => { const av = a[sort.column], bv = b[sort.column]; return ((typeof av === 'number' && typeof bv === 'number' ? av-bv : String(av ?? '').localeCompare(String(bv ?? ''))) * dir) || a.sourceIndex-b.sourceIndex; });
}
export function formatAmount(value, { decimals=2, format='Accounting' }={}) {
  if (value == null || !Number.isFinite(value)) return '—';
  const abs = Math.abs(value).toLocaleString('en-US', { minimumFractionDigits: decimals, maximumFractionDigits: decimals });
  return value < 0 ? format === 'Accounting' ? `(${abs})` : `−${abs}` : abs;
}
export function displayValue(value, col, options={}) {
  if (value === null || value === undefined) return 'NULL';
  if (col.type === 'number') return formatAmount(value,options);
  if (col.id === 'month') return monthLabel(value);
  return String(value);
}
export const monthLabel = value => { const [y,m] = String(value).split('-'); return `${['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'][Number(m)-1] || m} ${y}`; };
export const bounds = s => ({ r0:Math.min(s.r,s.r2), r1:Math.max(s.r,s.r2), c0:Math.min(s.c,s.c2), c1:Math.max(s.c,s.c2) });
export function statsFromValues(values) {
  let filled=0, numbers=0, sum=0, min=Infinity, max=-Infinity; const distinct=new Set();
  for (const v of values) { if(v===null || v===undefined || v==='')continue; filled++; distinct.add(`${typeof v}:${v}`); if(typeof v==='number' && Number.isFinite(v)){numbers++; sum+=v; min=Math.min(min,v);max=Math.max(max,v);} }
  return { count: values.length, filled, numbers, distinct:distinct.size, sum:numbers ? sum:null, average:numbers ? sum/numbers:null, min:numbers?min:null, max:numbers?max:null };
}
export function selectionStats(rows, cols, selection) {
  const b=bounds(selection); const values=[]; let cents=0, amountCount=0; const units=new Set();
  for(let r=b.r0;r<=Math.min(b.r1,rows.length-1);r++)for(let c=b.c0;c<=Math.min(b.c1,cols.length-1);c++){const col=cols[c]; values.push(rows[r][col.id]);if(col.id==='amount'){cents+=rows[r].cents;amountCount++;units.add(rows[r].currency);}}
  const s=statsFromValues(values);
  if(amountCount===s.numbers && amountCount){s.sum=cents/100;s.average=cents/amountCount/100;}
  return {...s,mixedUnits:units.size>1};
}
export function dimensionValue(row, id) {
  if(id==='year')return row.date.slice(0,4);
  if(id==='quarter')return `Q${Math.ceil(Number(row.date.slice(5,7))/3)}`;
  if(id==='month')return row.month;
  return String(row[id] ?? '(NULL)');
}
export function aggregateView(rows, definition) {
  const dims=definition.rows.length ? definition.rows : ['category'];
  const colDims=definition.columns.length ? definition.columns : ['month'];
  const groups=new Map(), colKeys=new Set();
  for(const row of rows) {
    const values=dims.map(id=>dimensionValue(row,id));const key=values.join('\u001f');
    const cv=colDims.map(id=>dimensionValue(row,id));const ck=cv.join('\u001f');colKeys.add(ck);
    if(!groups.has(key))groups.set(key,{key,values,cells:new Map(),cents:0,count:0});
    const g=groups.get(key);if(!g.cells.has(ck))g.cells.set(ck,{cents:0,count:0});const cell=g.cells.get(ck);cell.cents+=row.cents;cell.count++;g.cents+=row.cents;g.count++;
  }
  return { dims, columnDims:colDims, columnKeys:[...colKeys].sort(), groups:[...groups.values()].sort((a,b)=>a.key.localeCompare(b.key)) };
}
export function chartValues(rows) {
  const map=new Map();for(const row of rows){if(!map.has(row.month))map.set(row.month,{month:row.month,Receipts:0,Outflows:0});const g=map.get(row.month);g[row.cents>=0?'Receipts':'Outflows']+=row.cents/100;}
  return [...map.values()].sort((a,b)=>a.month.localeCompare(b.month));
}
export const metricLabels={count:'Count of cells',filled:'Filled cells',numbers:'Numeric count',distinct:'Distinct count',sum:'Sum',average:'Average',min:'Minimum',max:'Maximum'};
export const metricShort={count:'Count',filled:'Filled',numbers:'Numbers',distinct:'Distinct',sum:'Sum',average:'Avg',min:'Min',max:'Max'};
