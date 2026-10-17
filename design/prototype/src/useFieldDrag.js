import {useEffect,useRef,useState} from 'react';

export function useFieldDrag(onDrop,scale){
  const [ghost,setGhost]=useState(null),pending=useRef(null),ghostElement=useRef(null),drop=useRef(onDrop),scaleRef=useRef(scale),ignoreClick=useRef(false);
  drop.current=onDrop;scaleRef.current=scale;
  useEffect(()=>{
    let frame=0,lastZone=null,lastToken=null;
    function clearTargets(){lastZone?.classList.remove('drag-over');lastToken?.classList.remove('drop-insertion');lastZone=null;lastToken=null;document.body.classList.remove('dragging-field');}
    function paint(){frame=0;const d=pending.current;if(!d?.active)return;const el=ghostElement.current;if(el){el.style.transform=`translate3d(${(d.x+14)/scaleRef.current}px,${(d.y+13)/scaleRef.current}px,0)`;const note=el.querySelector('small');if(note)note.textContent=d.destination?`Add to ${d.destination==='viewFilter'?'view filters':d.destination}`:'Choose a highlighted target';}}
    function move(e){const d=pending.current;if(!d)return;d.x=e.clientX;d.y=e.clientY;if(!d.active&&Math.hypot(d.x-d.startX,d.y-d.startY)>5){d.active=true;d.element.setPointerCapture?.(d.pointerId);setGhost({label:d.label,id:d.payload.id,x:d.x,y:d.y});document.body.classList.add('dragging-field');}if(!d.active)return;e.preventDefault();const hit=document.elementFromPoint(d.x,d.y),candidate=hit?.closest('[data-drop-zone]'),zone=(d.payload.kind==='sheet'?candidate?.dataset.dropZone==='workbook':candidate?.dataset.dropZone!=='workbook')?candidate:null,token=hit?.closest('[data-drop-index]');if(zone!==lastZone){lastZone?.classList.remove('drag-over');lastZone=zone;lastZone?.classList.add('drag-over');}if(token!==lastToken){lastToken?.classList.remove('drop-insertion');lastToken=token;lastToken?.classList.add('drop-insertion');}d.destination=zone?.dataset.dropZone;d.index=token&&zone?.contains(token)?Number(token.dataset.dropIndex):undefined;if(!frame)frame=requestAnimationFrame(paint);}
    function finish(e){const d=pending.current;if(!d)return;pending.current=null;cancelAnimationFrame(frame);frame=0;if(d.active){ignoreClick.current=true;setTimeout(()=>ignoreClick.current=false,0);if(d.destination&&e.type!=='pointercancel')drop.current(d.payload,d.destination,d.index);}clearTargets();setGhost(null);}
    function cancel(e){if(e.key!=='Escape'||!pending.current?.active)return;e.preventDefault();e.stopPropagation();pending.current=null;clearTargets();setGhost(null);}
    window.addEventListener('pointermove',move,{passive:false});window.addEventListener('pointerup',finish);window.addEventListener('pointercancel',finish);window.addEventListener('keydown',cancel,true);
    return()=>{window.removeEventListener('pointermove',move);window.removeEventListener('pointerup',finish);window.removeEventListener('pointercancel',finish);window.removeEventListener('keydown',cancel,true);cancelAnimationFrame(frame);clearTargets();};
  },[]);
  function beginDrag(e,payload,label){if(e.button!==0)return;pending.current={payload,label,startX:e.clientX,startY:e.clientY,x:e.clientX,y:e.clientY,active:false,element:e.currentTarget,pointerId:e.pointerId};}
  function captureClick(e){if(ignoreClick.current){e.preventDefault();e.stopPropagation();}}
  return {ghost,ghostElement,beginDrag,captureClick};
}
