import React,{useEffect,useLayoutEffect,useRef,useState} from 'react';
import {createPortal} from 'react-dom';
import {Button,Icon,ActionMenu,usePopup,position} from './ui.jsx';

function Group({group,children,onExpand,expanded=false}){
  return <div className={`ribbon-group ${group.className||''}`} data-group={group.id}>
    <div className="ribbon-group-content">{children}</div>
    <span className="ribbon-group-name">{group.label}</span>{onExpand&&<Button className="ribbon-group-launcher" icon="ChevronDown" label={`${group.label} commands`} title={`All ${group.label.toLowerCase()} commands`} aria-haspopup="dialog" aria-expanded={expanded} onClick={onExpand}/>}
  </div>;
}
function CollapsedGroup({group,mode,onClick,expanded=false}){
  return <button className="ribbon-collapse" data-group={group.id} aria-label={`${group.label} commands`} aria-haspopup="dialog" aria-expanded={expanded} title={`${group.label} commands`} onClick={onClick}>
    <Icon name={group.icon} size={mode==='full'?20:16}/><span>{group.label}</span><Icon name="ChevronDown" size={11}/>
  </button>;
}
function GroupFlyout({group,anchor,onClose,children}){
  const popup=usePopup(onClose,anchor),rect=anchor.getBoundingClientRect();
  return <section {...popup} className="popup ribbon-flyout" role="dialog" aria-label={`${group.label} commands`} style={position(rect.left,rect.bottom+4,280,160)}>
    <div className="popup-title"><strong>{group.label}</strong><Button icon="Close" label={`Close ${group.label} commands`} onClick={onClose}/></div>
    <div className="ribbon-flyout-body"><Group group={group}>{children}</Group></div>
  </section>;
}

export function Ribbon({mode,tab,setTab,authoring,run,onOpenGroup,activeFormat,applyFormat,canUndo,canRedo}){
  const header=useRef(null),toolbar=useRef(null),measureRef=useRef(null),moreRef=useRef(null),groupsRef=useRef([]),popupRef=useRef(null);
  const [layout,setLayout]=useState({collapsed:[],hidden:[],history:true,width:1440});
  const [popup,setPopup]=useState(null);popupRef.current=popup;const focusedBeforeLayout=useRef(null);
  function closePopup(){popupRef.current?.anchor?.focus?.({preventScroll:true});setPopup(null);}
  function invoke(name,anchor){const target=popupRef.current?.anchor||anchor;setPopup(null);target?.focus?.({preventScroll:true});run(name,target);}
  const command=(name,icon,label,details=mode!=='icons',extra={})=><Button icon={icon} label={label} title={extra.title||label} onClick={e=>invoke(name,e.currentTarget)} {...extra}>{details?label:null}</Button>;
  const commands=(items,detailed)=>items.map(([name,icon,label,extra])=><React.Fragment key={name}>{command(name,icon,label,detailed,extra)}</React.Fragment>);
  const def=(id,label,icon,priority,content,className='')=>({id,label,icon,priority,content,className});
  const all=mode==='full';
  const groups=tab==='Home'?[
    def('clipboard','Clipboard','Copy',90,(d=all)=> <>{command('copy','Copy','Copy',mode!=='icons'||d)}{d&&<><Button icon="Paste" disabled title="Source data is read-only in this design study">Paste</Button><Button icon="Cut" disabled title="Source data is read-only in this design study">Cut</Button></>}</>,'clipboard-group'),
    def('font','Font','TextBold',50,(d=all)=><><div className="font-row"><span className="font-family" title="Actual IBM Plex Sans font files">IBM Plex Sans</span><select aria-label="Grid font size" value={activeFormat.size||13} onChange={e=>applyFormat({size:Number(e.target.value)})}>{[11,12,13,14,15,16].map(v=><option key={v}>{v}</option>)}</select></div><div className="inline-tools"><Button icon="TextBold" label="Bold" active={!!activeFormat.bold} onClick={()=>applyFormat({bold:!activeFormat.bold})}/><Button icon="TextItalic" label="Italic" active={!!activeFormat.italic} onClick={()=>applyFormat({italic:!activeFormat.italic})}/>{d&&<Button icon="TextUnderline" label="Underline" active={!!activeFormat.underline} onClick={()=>applyFormat({underline:!activeFormat.underline})}/>}<Button icon="TextColor" label="Text color" onClick={e=>invoke('textColor',e.currentTarget)}/></div></>,'font-group'),
    def('alignment','Alignment','TextAlignLeft',80,(d=all)=><><div className="inline-tools">{['left','center','right'].map(a=><Button key={a} icon={`TextAlign${a[0].toUpperCase()+a.slice(1)}`} label={`Align ${a}`} active={activeFormat.align===a} onClick={()=>applyFormat({align:a})}/>)}</div>{d&&<Button icon="TextWrap" disabled title="Variable-height wrapping is outside this prototype slice">Wrap text</Button>}</>),
    def('number','Number','CurrencyDollar',70,(d=all)=><><select aria-label="Number format" value={activeFormat.format||'Accounting'} onChange={e=>applyFormat({format:e.target.value})}><option>Accounting</option><option>Number</option></select><div className="inline-tools"><Button icon="CurrencyDollar" label="Accounting format" onClick={()=>applyFormat({format:'Accounting'})}/>{d&&<><Button className="decimal-button" label="Decrease decimal places" onClick={()=>applyFormat({decimals:Math.max(0,(activeFormat.decimals??2)-1)})}>.0<Icon name="ArrowLeft" size={10}/></Button><Button className="decimal-button" label="Increase decimal places" onClick={()=>applyFormat({decimals:Math.min(4,(activeFormat.decimals??2)+1)})}>.00<Icon name="ArrowRight" size={10}/></Button></>}</div></>,'number-group'),
    def('data','Data','Filter',20,(d=all)=><><div className="sort-stack">{command('sortAsc','SortAscending','Sort A → Z',mode!=='icons'||d)}{d&&command('sortDesc','SortDescending','Sort Z → A')}</div>{command('filter','Filter','Filter',mode!=='icons'||d)}{d&&command('clearFilters','FilterRemove','Clear',true,{title:'Clear all source filters'})}</>,'data-group'),
    def('analysis','Analysis','DataView',10,(d=all)=><>{command('createView','DataView','Create view',mode!=='icons'||d)}{command('transform','DataStructured','Transform',mode!=='icons'||d)}{d&&command('refresh','Renew','Refresh')}</>,'analysis-group')
  ]:tab==='View'?[
    def('panes','Panes','DataStructured',10,d=>commands([['workbook','Table','Workbook'],['fields','List','Fields'],['transform','DataStructured','Transformations'],['sql','Sql','Inspect SQL']],d??mode!=='icons')),
    def('layout','Layout','SettingsAdjust',20,d=>commands([['focus','Maximize','Focus view'],['density','ArrowsHorizontal','Row density'],['scale','SettingsAdjust','Interface scale']],d??mode!=='icons')),
    def('worksheet','Worksheet','Grid',60,d=>commands([['gridlines','Grid','Gridlines'],['freeze','Column','Freeze first 2 columns'],['resetZoom','ZoomIn','Reset zoom']],d??mode!=='icons')),
    def('study','Study','Keyboard',90,d=>commands([['reset','Renew','Reset study'],['help','Keyboard','Keyboard guide']],d??mode!=='icons'))
  ]:tab==='Data'?[
    def('order','Order','SortAscending',30,d=>commands([['sortAsc','SortAscending','Sort ascending'],['sortDesc','SortDescending','Sort descending'],['removeSort','ArrowsHorizontal','Source order']],d??mode!=='icons')),
    def('filter','Filter','Filter',10,d=>commands([['filter','Filter','Column filter'],['clearFilters','FilterRemove','Clear all filters']],d??mode!=='icons')),
    def('steps','Steps','DataStructured',20,d=>commands([['transform','DataStructured','Transformations'],['sql','Sql','Inspect SQL'],['refresh','Renew','Refresh result']],d??mode!=='icons'))
  ]:tab==='Insert'?[
    def('analysis','Analysis','DataView',10,d=>commands([['createView','DataView','Analysis view'],['matrix','Table','Matrix'],['bar','ChartBar','Bar chart'],['line','ChartLine','Line chart']],d??mode!=='icons')),
    def('workbook','Workbook','Table',50,d=>command('newSheet','DocumentBlank','New sheet',d??mode!=='icons'))
  ]:[
    def('visual','Visuals','ChartBar',10,d=>commands([['createView','DataView','Edit analysis'],['matrix','Table','Matrix'],['bar','ChartBar','Bar chart'],['line','ChartLine','Line chart']],d??mode!=='icons')),
    def('structure','Structure','Row',20,d=>commands([['addRows','Row','Add to Rows'],['addColumns','Column','Add to Columns'],['swap','ArrowsHorizontal','Swap shelves'],['addMeasure','Function','Add measure']],d??mode!=='icons')),
    def('inspect','Inspect','Sql',60,d=>commands([['transform','DataStructured','Transformations'],['sql','Sql','Inspect SQL']],d??mode!=='icons')),
    def('return','Return','Table',90,d=>command('browse','Table','Browse data',d??mode!=='icons'))
  ];
  groupsRef.current=groups;
  useLayoutEffect(()=>{
    let frame;
    const recalculate=()=>{
      const width=toolbar.current.clientWidth,history=width>=700,reserved=history?98:38;
      const entries=[...measureRef.current.children].map((el,i)=>({id:groupsRef.current[i].id,priority:groupsRef.current[i].priority,expanded:el.querySelector('.ribbon-group').offsetWidth,compact:el.querySelector('.ribbon-collapse').offsetWidth+8}));
      let total=entries.reduce((n,e)=>n+e.expanded,0),available=Math.max(0,width-reserved-12);
      const collapsed=[],hidden=[];
      for(const e of [...entries].sort((a,b)=>b.priority-a.priority)){
        if(total<=available)break;
        if(e.expanded>e.compact+4){collapsed.push(e.id);total-=e.expanded-e.compact;}
      }
      for(const e of [...entries].sort((a,b)=>b.priority-a.priority)){
        if(total<=available)break;
        hidden.push(e.id);total-=collapsed.includes(e.id)?e.compact:e.expanded;
      }
      const focused=document.activeElement,groupId=focused?.closest('[data-group]')?.dataset.group;focusedBeforeLayout.current={node:focused,id:groupId};
      const key=JSON.stringify([collapsed,hidden,history,Math.round(width)]);
      setLayout(prev=>prev.key===key?prev:{collapsed,hidden,history,width,key});
    };
    const observer=new ResizeObserver(()=>{cancelAnimationFrame(frame);frame=requestAnimationFrame(recalculate);});
    observer.observe(toolbar.current);[...measureRef.current.children].forEach(el=>observer.observe(el));recalculate();
    return()=>{observer.disconnect();cancelAnimationFrame(frame);};
  },[mode,tab,authoring]);
  useEffect(()=>{setPopup(null);},[mode,tab]);
  useEffect(()=>{if(!authoring&&tab==='Visualization')setTab('Analyze');},[authoring,tab,setTab]);
  useLayoutEffect(()=>{
    const findAnchor=id=>header.current.querySelector(`button[data-group="${id}"]`)||header.current.querySelector(`[data-group="${id}"] button`)||moreRef.current;
    const p=popupRef.current;if(p?.anchor&&!p.anchor.isConnected){setPopup(null);findAnchor(p.id)?.focus?.({preventScroll:true});}
    const focused=focusedBeforeLayout.current;if(focused?.id&&!focused.node.isConnected)findAnchor(focused.id)?.focus?.({preventScroll:true});
  },[layout.key]);
  const openGroup=(group,anchor)=>{onOpenGroup?.();setPopup(p=>p?.id===group.id?null:{kind:'group',id:group.id,anchor});};
  const openOverflow=anchor=>{onOpenGroup?.();setPopup(p=>p?.kind==='overflow'?null:{kind:'overflow',anchor});};
  const currentGroup=groups.find(g=>g.id===popup?.id),hasOverflow=layout.hidden.length>0||!layout.history;
  const rect=popup?.anchor?.getBoundingClientRect();
  const overflowItems=[...groups.filter(g=>layout.hidden.includes(g.id)).map(g=>({label:`${g.label} commands`,icon:g.icon,keepOpen:true,action:()=>setPopup({kind:'group',id:g.id,anchor:moreRef.current})})),...(!layout.history?[{separator:layout.hidden.length>0},{label:'Undo analytical change',icon:'Undo',disabled:!canUndo,action:()=>invoke('undo')},{label:'Redo analytical change',icon:'Redo',disabled:!canRedo,action:()=>invoke('redo')}]:[])].filter(v=>!('separator'in v)||v.separator);
  return <header ref={header} className={`app-chrome adaptive-ribbon ribbon-${mode}`} data-region="ribbon" data-toolbar-density={layout.width>=1400?'relaxed':layout.width>=900?'regular':'tight'}>
    <div className="title-bar"><strong className="wordmark">Duxcel</strong><span className="title-divider"/><span className="document-title">Q3 Treasury Review.dxls</span><span className="title-tail">Design study<span className="title-separator">·</span>16 Oct 2026</span><Button icon="Save" className="title-save" label="Save design state as JSON" onClick={()=>run('save')}/></div>
    <div className="ribbon-tab-row"><div className="ribbon-tabs" role="tablist" aria-label="Ribbon tabs">{['Home','Insert','Data','Analyze','View',...(authoring?['Visualization']:[])].map(name=><button role="tab" aria-selected={tab===name} key={name} className={tab===name?'active':''} onClick={e=>{setTab(name);e.currentTarget.scrollIntoView({block:'nearest',inline:'nearest'});}}>{name}</button>)}</div><div className="ribbon-view-actions"><Button className="ribbon-display" label="Ribbon display" onClick={e=>invoke('ribbonMenu',e.currentTarget)}><span className="ribbon-display-label">Ribbon display</span><Icon name="ChevronDown" size={12}/></Button><Button icon="Help" label="Interaction guide" onClick={()=>run('help')}/></div></div>
    <div className="ribbon-commands" ref={toolbar}>
      <div className="ribbon-group-strip">{groups.filter(g=>!layout.hidden.includes(g.id)).map(g=>layout.collapsed.includes(g.id)?<div className="ribbon-group-slot" key={g.id}><CollapsedGroup group={g} mode={mode} expanded={popup?.id===g.id} onClick={e=>openGroup(g,e.currentTarget)}/></div>:<Group key={g.id} group={g} expanded={popup?.id===g.id} onExpand={e=>openGroup(g,e.currentTarget)}>{g.content()}</Group>)}</div>
      <div className="ribbon-action-dock">{hasOverflow&&<Button ref={moreRef} className="ribbon-overflow" icon="OverflowMenuHorizontal" label="More ribbon commands" aria-haspopup="menu" aria-expanded={popup?.kind==='overflow'} onClick={e=>{moreRef.current=e.currentTarget;openOverflow(e.currentTarget);}}/>}{layout.history&&<div className="ribbon-history"><Button icon="Undo" label="Undo analytical change" disabled={!canUndo} onClick={()=>run('undo')}/><Button icon="Redo" label="Redo analytical change" disabled={!canRedo} onClick={()=>run('redo')}/></div>}</div>
    </div>
    <div className="ribbon-measure" ref={measureRef} aria-hidden="true" inert>{groups.map(g=><div key={g.id} className="ribbon-measure-unit"><Group group={g} onExpand={()=>{}}>{g.content()}</Group><CollapsedGroup group={g} mode={mode}/></div>)}</div>
    {popup&&rect&&createPortal(<div className="popup-layer ribbon-popup-layer">{popup.kind==='group'&&currentGroup?<GroupFlyout key={currentGroup.id} group={currentGroup} anchor={popup.anchor} onClose={closePopup}>{currentGroup.content(true)}</GroupFlyout>:popup.kind==='overflow'?<ActionMenu menu={{title:'More ribbon commands',x:rect.left,y:rect.bottom+4,anchor:popup.anchor,items:overflowItems}} onClose={closePopup}/>:null}</div>,document.querySelector('.duxcel-app'))}
  </header>;
}
