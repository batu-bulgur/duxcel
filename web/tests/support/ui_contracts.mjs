// Offline expected-value loaders for the real C UI tests. Never packaged.
import fs from 'node:fs/promises';
import {createHash} from 'node:crypto';
const assertContract=(value,message)=>{if(!value)throw new Error(message);};
export async function paneRailExpandedReference(check=assertContract){
 // Accepted native geometry; no archived session or executable mock is read.
 const relative='tests/fixtures/ui/',file=relative+'rail-measurements.json',expected='c5f76db9444a5fb51e79ed2d56a9e02860ec6a3e115c1a794abcd292b616a1ce';
 const borderFile='tests/fixtures/ui/steps-scroll.json';
 const [bytes,borderBytes]=await Promise.all([fs.readFile(new URL('../../../'+file,import.meta.url)),fs.readFile(new URL('../../../'+borderFile,import.meta.url))]);
 const sha256=createHash('sha256').update(bytes).digest('hex'),measured=JSON.parse(bytes);
 check(sha256===expected&&measured.schema==='duxcel.ui.rail-measurements-contract'&&measured.version===2,'Expanded-header contract changed or has an unsupported schema');
 // Accepted native regression dimensions; no executable mock remains authoritative.
 const gap=2,headerHeight=35,buttonWidth=23;
 const cases={};
 for(const name of ['wide-100','wide-150','narrow-100']){
  const record=measured.states.find(item=>item.case===name&&item.name==='expanded'),state=record?.state,panel=state?.wrapper?.rect,collapse=state?.collapse?.rect,scale=state?.scale;
  check(record?.validated&&state.ribbon==='compact'&&panel&&collapse&&collapse.width===buttonWidth*scale&&collapse.height===collapse.width,'Frozen expanded reference lacks its measured23-unit Collapse: '+name);
  const bounds=rect=>[rect.x,rect.y,rect.width,rect.height],dismiss={...collapse,x:collapse.x+collapse.width+gap*scale};
  cases[name]={viewport:state.viewport,scale,panel:bounds(panel),grid:bounds(state.grid.rect),headerHeight:headerHeight*scale,controls:{collapse:bounds(collapse),dismiss:bounds(dismiss)},capture:record.capture,derivation:{collapse:'measured DOM',dismiss:'measured Collapse width + frozen inspector-title gap; adjacent shared23-unit Button'}};
 }
 // Accepted border/fill colors and their measured inside allocation.
 const borderColors={cases:['wide-100','wide-150','narrow-100','narrow-150'],borderColor:'rgb(202, 210, 218)',background:'rgb(251, 252, 253)',borderStyle:'solid'};
 const borderSha256=createHash('sha256').update(borderBytes).digest('hex'),borderProvider=JSON.parse(borderBytes),borderCases={};
 check(borderSha256==='8d873a083aaf9af60cc60aebb539a0186c3cafeacc8a63e22529b7fe74a27aee'&&borderProvider.schema==='duxcel.ui.steps-scroll-contract'&&borderProvider.version===2,'Expanded-border contract changed or has an unsupported schema');
 for(const name of ['wide-100','wide-150','narrow-100','narrow-150']){
  const record=borderProvider.states.find(item=>item.case===name&&item.name==='default'),state=record?.state,parts=state?.steps?.parts,outer=parts?.outer,header=parts?.header,prior=measured.states.find(item=>item.case===name&&item.name==='expanded');
  const left=outer?.box?Math.min(...outer.box.border.filter((_,i)=>i%2===0)):NaN,paddingLeft=outer?.box?Math.min(...outer.box.padding.filter((_,i)=>i%2===0)):NaN;
  check(record?.validated&&record.anatomy_retained&&state.steps.rowsTotal===6&&outer.rect.x===left&&paddingLeft-left===1&&Number.parseFloat(outer.style.paddingLeft)===0&&header.rect.x===paddingLeft&&header.rect.y===outer.rect.y&&prior?.validated&&borderColors.cases.includes(name)&&borderColors.borderStyle==='solid'&&borderColors.borderColor==='rgb(202, 210, 218)'&&borderColors.background==='rgb(251, 252, 253)','Frozen expanded border lacks its measured one-point inside allocation or actual colors: '+name);
  // Use the measured header's empty left edge, away from horizontal lines and
  //title ink. This is panel.x, not the retained rail's viewportRight-32 slot.
  const y=header.rect.y+header.rect.height/4,h=header.rect.height/2;
  borderCases[name]={viewport:state.viewport,scale:state.scale,panel:[outer.rect.x,outer.rect.y,outer.rect.width,outer.rect.height],grid:[prior.state.grid.rect.x,prior.state.grid.rect.y,prior.state.grid.rect.width,prior.state.grid.rect.height],borderQuad:outer.box.border,paddingQuad:outer.box.padding,capture:record.capture,region:{x:left,y,w:3,h,bands:[{name:'expanded_inside_border',x:left,w:paddingLeft-left,rgba:[202,210,218,255]},{name:'expanded_after_border',x:paddingLeft,w:2,rgba:[251,252,253,255]}]},scope:'Measured expanded left-edge paint only; short150 outer scrolling/header-action parity and expanded shadow remain separate.'};
  if(!cases[name]){
   const bounds=rect=>[rect.x,rect.y,rect.width,rect.height],collapse=parts.collapse.rect,dismiss=parts.dismiss.rect;
   check(collapse.width===buttonWidth*state.scale&&collapse.height===collapse.width&&dismiss.width===collapse.width&&dismiss.height===collapse.height&&dismiss.x===collapse.x+collapse.width+gap*state.scale,'Short expanded controls lost their measured23-unit size or frozen gap');
   cases[name]={viewport:state.viewport,scale:state.scale,panel:borderCases[name].panel,grid:borderCases[name].grid,headerHeight:header.rect.height,controls:{collapse:bounds(collapse),dismiss:bounds(dismiss)},capture:record.capture,derivation:{collapse:'measured default Steps DOM with outer scrollbar lane',dismiss:'measured adjacent Dismiss DOM with outer scrollbar lane'}};
  }
 }
 return{file,sha256,cases,border:{file:borderFile,sha256:borderSha256,computedColors:borderColors,cases:borderCases},excluded:{}};
}

export async function inspectorScrollReference(check=assertContract){
 const file='tests/fixtures/ui/steps-scroll.json',bytes=await fs.readFile(new URL('../../../'+file,import.meta.url)),sha256=createHash('sha256').update(bytes).digest('hex'),data=JSON.parse(bytes);
 check(sha256==='8d873a083aaf9af60cc60aebb539a0186c3cafeacc8a63e22529b7fe74a27aee'&&data.schema==='duxcel.ui.steps-scroll-contract'&&data.version===2,'Inspector scroll contract changed or has an unsupported schema');
 return{file,sha256,states:data.states.filter(state=>state.anatomy_retained)};
}

export async function inspectorSQLReference(check=assertContract){
 const file='tests/fixtures/ui/sql-inspector.json',bytes=await fs.readFile(new URL('../../../'+file,import.meta.url)),sha256=createHash('sha256').update(bytes).digest('hex'),data=JSON.parse(bytes);
 check(sha256==='d281a82242a5837ab5a76be79ea234360e4b513b458d7c8a8b8eb2b74896bb6d'&&data.schema==='duxcel.ui.sql-inspector-contract'&&data.version===2,'SQL inspector contract changed or has an unsupported schema');
 return{file,sha256,states:data.states.filter(item=>item.state.parts),derived_rules:data.derived_rules};
}
