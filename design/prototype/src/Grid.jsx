import React, {forwardRef,useRef,useState,useEffect,useImperativeHandle,useCallback} from 'react';
import {Filter,ChevronDown,ArrowUp,ArrowDown} from '@carbon/icons-react';
import {bounds,displayValue} from './data.js';

export const Grid=forwardRef(function Grid({rows,cols,widths,setWidths,selection,setSelection,sort,filters,onFilter,onContext,onNavigate,onSelection,formats={},zoom=1,density=24,showGrid=true,freeze=true,preview=false,initialScroll,onScrollChange,highlight,onUnfreeze},ref){
  const scroller=useRef(null), dragging=useRef(false), selectionRef=useRef(selection), resize=useRef(null);
  const [scroll,setScroll]=useState({top:initialScroll?.top||0,left:initialScroll?.left||0,height:600});
  const [frozenCrowded,setFrozenCrowded]=useState(false);
  const rowHeight=density*zoom, letterH=22*zoom, nameH=26*zoom, headerH=letterH+nameH, gutter=40*zoom;
  const b=bounds(selection); selectionRef.current=selection;
  const colWidths=cols.map(c=>(widths[c.id]||c.width)*zoom);
  const offsets=[];let contentWidth=gutter;for(const w of colWidths){offsets.push(contentWidth);contentWidth+=w;}
  useEffect(()=>{const el=scroller.current;const check=()=>setFrozenCrowded(freeze&&el.clientWidth<gutter+(colWidths[0]||0)+(colWidths[1]||0)+72);const observer=new ResizeObserver(check);observer.observe(el);check();return()=>observer.disconnect();},[freeze,gutter,colWidths[0],colWidths[1]]);
  const visibleRows=preview ? rows.slice(0,12) : rows;
  const start=preview?0:Math.max(0,Math.floor((scroll.top-headerH)/rowHeight)-4);
  const end=preview?visibleRows.length:Math.min(visibleRows.length,start+Math.ceil(scroll.height/rowHeight)+10);
  useEffect(()=>{const el=scroller.current; const observer=new ResizeObserver(()=>setScroll(s=>s.height===el.clientHeight?s:({...s,height:el.clientHeight})));observer.observe(el);if(initialScroll){el.scrollTop=initialScroll.top;el.scrollLeft=initialScroll.left;}return()=>observer.disconnect();},[]);
  const focusGrid=()=>scroller.current?.focus({preventScroll:true});
  const revealCell=useCallback((r,c,align='nearest')=>{
    const el=scroller.current;if(!el)return;
    const left=offsets[c],right=left+colWidths[c],frozen=freeze?gutter+(colWidths[0]||0)+(colWidths[1]||0):gutter;
    if(!freeze||c>1){if(align==='center')el.scrollLeft=Math.max(0,left-(el.clientWidth-colWidths[c])/2);else if(left<el.scrollLeft+frozen||right>el.scrollLeft+el.clientWidth)el.scrollLeft=Math.max(0,left-frozen);}
    const top=r*rowHeight;if(top<el.scrollTop)el.scrollTop=top;else if(top+rowHeight>el.scrollTop+el.clientHeight-headerH)el.scrollTop=top+rowHeight-el.clientHeight+headerH;
  },[offsets.join(','),rowHeight,freeze]);
  useImperativeHandle(ref,()=>({focus:focusGrid,revealColumn:(c,align)=>revealCell(selection.r,c,align),selectRange:s=>{setSelection(s);revealCell(s.r,s.c);focusGrid();},getScroll:()=>({top:scroller.current.scrollTop,left:scroller.current.scrollLeft})}));
  const select=(r,c,extend=false)=>{const s=selectionRef.current;const next=extend?{...s,r2:r,c2:c}:{r,c,r2:r,c2:c};setSelection(next);onSelection?.();};
  useEffect(()=>{
    function move(e){
      if(resize.current){const d=resize.current;setWidths(w=>({...w,[d.id]:Math.max(60,Math.min(600,d.width+(e.clientX-d.x)/(zoom*Number(document.querySelector('.duxcel-app')?.dataset.uiScale||1))))}));return;}
      if(!dragging.current)return;
      const cell=document.elementFromPoint(e.clientX,e.clientY)?.closest('[data-grid-cell]');
      if(cell&&scroller.current?.contains(cell)){select(Number(cell.dataset.row),Number(cell.dataset.col),true);}
      const box=scroller.current.getBoundingClientRect();
      if(!preview){if(e.clientY>box.bottom-22)scroller.current.scrollTop+=rowHeight;if(e.clientY<box.top+headerH+14)scroller.current.scrollTop-=rowHeight;}
      if(e.clientX>box.right-24)scroller.current.scrollLeft+=18;if(e.clientX<box.left+32)scroller.current.scrollLeft-=18;
    }
    function up(){dragging.current=false;if(resize.current){resize.current=null;document.body.classList.remove('resizing-column');}}
    window.addEventListener('pointermove',move);window.addEventListener('pointerup',up);return()=>{window.removeEventListener('pointermove',move);window.removeEventListener('pointerup',up);};
  },[rowHeight,headerH,zoom,preview,setWidths]);
  function autoFit(col){const canvas=document.createElement('canvas');const ctx=canvas.getContext('2d');ctx.font='13px "IBM Plex Sans"';const values=rows.slice(0,200).map(r=>displayValue(r[col.id],col));const max=Math.max(ctx.measureText(col.name).width+42,...values.map(v=>ctx.measureText(v).width+24));setWidths(w=>({...w,[col.id]:Math.min(400,Math.max(75,Math.ceil(max)))}));}
  function keydown(e){
    if(e.target!==e.currentTarget)return;
    let {r,c,r2,c2}=selectionRef.current;const primary=e.metaKey||e.ctrlKey;
    if(e.altKey&&e.key==='ArrowDown'){e.preventDefault();const header=scroller.current.querySelector(`[data-filter="${cols[c].id}"]`);onFilter(cols[c],header.getBoundingClientRect(),header);return;}
    if(e.altKey&&['ArrowLeft','ArrowRight'].includes(e.key)){e.preventDefault();setWidths(w=>({...w,[cols[c].id]:Math.max(60,(w[cols[c].id]||cols[c].width)+(e.key==='ArrowRight'?8:-8))}));return;}
    if(primary&&e.key.toLowerCase()==='a'){e.preventDefault();setSelection({r:0,c:0,r2:rows.length-1,c2:cols.length-1});return;}
    if(e.shiftKey&&e.key===' '){e.preventDefault();setSelection({r,c:0,r2:r,c2:cols.length-1});return;}
    if(primary&&e.key===' '){e.preventDefault();setSelection({r:0,c,r2:rows.length-1,c2:c});return;}
    const fromR=e.shiftKey?r2:r,fromC=e.shiftKey?c2:c;
    let nr=fromR,nc=fromC;
    if(e.key==='ArrowDown')nr=primary?rows.length-1:fromR+1;else if(e.key==='ArrowUp')nr=primary?0:fromR-1;else if(e.key==='ArrowRight')nc=primary?cols.length-1:fromC+1;else if(e.key==='ArrowLeft')nc=primary?0:fromC-1;else if(e.key==='Tab')nc+=e.shiftKey?-1:1;else if(e.key==='Enter')nr+=e.shiftKey?-1:1;else if(e.key==='PageDown')nr+=Math.floor(scroll.height/rowHeight)-2;else if(e.key==='PageUp')nr-=Math.floor(scroll.height/rowHeight)-2;else if(e.key==='Home'){nc=0;if(primary)nr=0;}else if(e.key==='End'){nc=cols.length-1;if(primary)nr=rows.length-1;}else if(e.key==='F2'){onNavigate?.('edit');return;}else return;
    e.preventDefault();nr=Math.max(0,Math.min(rows.length-1,nr));nc=Math.max(0,Math.min(cols.length-1,nc));select(nr,nc,e.shiftKey&&!['Tab','Enter'].includes(e.key));revealCell(nr,nc);
  }
  const sticky=(c)=>freeze&&c<2?{position:'sticky',left:offsets[c]}:{};
  return <div className="grid-frame"><div ref={scroller} className={`data-grid ${preview?'preview-grid':''} ${showGrid?'':'no-lines'}`} role="grid" aria-label="Transactions spreadsheet" aria-rowcount={rows.length+1} aria-colcount={cols.length} aria-activedescendant={rows[selection.r]?`cell-${rows[selection.r].sourceIndex}-${cols[selection.c].id}`:undefined} tabIndex={0} style={{'--row-height':`${rowHeight}px`,'--grid-font':`${13*zoom}px`}} onKeyDown={keydown} onScroll={e=>{const el=e.currentTarget;setScroll(s=>({...s,top:el.scrollTop,left:el.scrollLeft}));onScrollChange?.({top:el.scrollTop,left:el.scrollLeft});}} onContextMenu={e=>{if(e.target.closest('[data-grid-cell]')){e.preventDefault();onContext?.(e.clientX,e.clientY);}}}>
    <div className="grid-header" role="row" style={{width:contentWidth,height:headerH}}>
      <button className="grid-corner" title="Select all result cells" aria-label="Select all result cells" onClick={()=>{setSelection({r:0,c:0,r2:rows.length-1,c2:cols.length-1});focusGrid();}} style={{width:gutter,height:headerH}} />
      {cols.map((col,c)=><div key={col.id} className={`column-header ${c>=b.c0&&c<=b.c1?'in-selection':''} ${highlight===col.id?'reveal-highlight':''} ${freeze&&c===1?'freeze-edge':''} ${freeze&&c<2?'frozen-column':''}`} style={{width:colWidths[c],...sticky(c)}} role="columnheader" aria-label={col.name} aria-sort={sort?.column===col.id?(sort.dir==='asc'?'ascending':'descending'):'none'}>
        <button className="column-letter" style={{height:letterH}} title={`Select ${col.name} column`} onClick={()=>{setSelection({r:0,r2:rows.length-1,c,c2:c});onSelection?.();focusGrid();}}>{String.fromCharCode(65+c)}</button>
        <div className="column-name" style={{height:nameH}}><span className="column-label">{col.name}</span>{sort?.column===col.id&&(sort.dir==='asc'?<ArrowUp size={12}/>:<ArrowDown size={12}/>)}<button data-filter={col.id} className={`filter-trigger ${filters[col.id]?'filtered':''}`} aria-label={`Sort and filter ${col.name}`} title={`Sort and filter ${col.name} · Alt+↓`} onClick={e=>{e.stopPropagation();onFilter(col,e.currentTarget.getBoundingClientRect(),e.currentTarget);}}>{filters[col.id]?<Filter size={13}/>:<ChevronDown size={13}/>}</button></div>
        <div className="column-resizer" role="separator" aria-label={`Resize ${col.name}`} aria-orientation="vertical" aria-valuemin={60} aria-valuemax={600} aria-valuenow={Math.round(widths[col.id]||col.width)} tabIndex={0} title="Drag to resize · Double-click to fit" onDoubleClick={()=>autoFit(col)} onKeyDown={e=>{if(['ArrowLeft','ArrowRight'].includes(e.key)){e.preventDefault();setWidths(w=>({...w,[col.id]:Math.max(60,(w[col.id]||col.width)+(e.key==='ArrowRight'?8:-8))}));}}} onPointerDown={e=>{e.preventDefault();e.stopPropagation();resize.current={id:col.id,x:e.clientX,width:widths[col.id]||col.width};document.body.classList.add('resizing-column');}}/>
      </div>)}
    </div>
    <div className="grid-body" style={{height:visibleRows.length*rowHeight,width:contentWidth}}>
      {visibleRows.length===0?<div className="grid-empty"><strong>No matching transactions</strong><span>Clear a column filter to see more results.</span></div>:Array.from({length:Math.max(0,end-start)},(_,j)=>{const r=j+start,row=visibleRows[r];return <div key={row.sourceIndex??r} className="grid-row" role="row" aria-rowindex={r+2} style={{top:r*rowHeight,height:rowHeight,width:contentWidth}}>
        <button className={`row-number ${r>=b.r0&&r<=b.r1?'in-selection':''}`} style={{width:gutter}} aria-label={`Select row ${r+2}`} onClick={()=>{setSelection({r,r2:r,c:0,c2:cols.length-1});onSelection?.();focusGrid();}}>{r+2}</button>
        {cols.map((col,c)=>{const selected=r>=b.r0&&r<=b.r1&&c>=b.c0&&c<=b.c1,active=r===selection.r&&c===selection.c;const format=formats[`${row.sourceIndex}:${col.id}`]||{};return <div key={col.id} id={`cell-${row.sourceIndex}-${col.id}`} data-grid-cell data-row={r} data-col={c} className={`grid-cell ${col.type==='number'?'numeric':''} ${row[col.id]==null?'null-value':''} ${col.id==='amount'&&row.amount<0?'negative':''} ${selected?'selected':''} ${active?'active-cell':''} ${selected&&r===b.r0?'selection-top':''} ${selected&&r===b.r1?'selection-bottom':''} ${selected&&c===b.c0?'selection-left':''} ${selected&&c===b.c1?'selection-right':''} ${freeze&&c===1?'freeze-edge':''} ${freeze&&c<2?'frozen-cell':''}`} role="gridcell" aria-colindex={c+1} aria-selected={selected} title={row[col.id]==null?'NULL':String(row[col.id])} style={{width:colWidths[c],height:rowHeight,...sticky(c),fontWeight:format.bold?600:400,fontStyle:format.italic?'italic':'normal',textDecoration:format.underline?'underline':undefined,textAlign:format.align||undefined,color:format.color||undefined,fontSize:format.size?`${format.size*zoom}px`:undefined}} onPointerDown={e=>{if(e.button!==0)return;e.preventDefault();select(r,c,e.shiftKey);dragging.current=true;focusGrid();}} onDoubleClick={()=>onNavigate?.('inspect',row,col)}>{displayValue(row[col.id],col,format)}{active&&<span className="selection-handle"/>}</div>;})}
      </div>;})}
    </div>
  </div>{frozenCrowded&&<div className="frozen-space-hint" role="status"><span>Frozen columns fill this view.</span><button onClick={onUnfreeze}>Unfreeze</button></div>}</div>;
});
