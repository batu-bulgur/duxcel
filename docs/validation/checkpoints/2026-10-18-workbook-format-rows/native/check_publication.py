import pathlib,subprocess,time,os,json,hashlib,stat
root=pathlib.Path.cwd();base=root/'build/bp1-format-rows-native';names=['format-rows-v12.dxls','format-rows-v13.dxls','format-rows-v14.dxls','format-rows-v14-reordered.dxls'];versions=[12,13,14,14]
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def invoke(cmd,name,timeout=180,env=None):
 t=time.monotonic()
 try:p=subprocess.run([str(x) for x in cmd],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout,env=env);rc=p.returncode;out=p.stdout
 except subprocess.TimeoutExpired as e:rc=124;out=e.stdout or b''
 (base/(name+'.log')).write_bytes(out);r={'command':[str(x) for x in cmd],'exit':rc,'seconds':time.monotonic()-t};(base/(name+'.json')).write_text(json.dumps(r,indent=2)+'\n');return r,out
records={}
for config in ['release','sanitize']:
 directory=base/('fixtures-'+config);directory.mkdir();env=os.environ.copy();env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1');r,out=invoke([base/('test-'+config),'--emit-format-rows-fixtures',directory],config+'-emission',30 if config=='release' else 180,env);assert r['exit']==0,(r,out);protocol=json.loads(out);assert protocol=={'schema':'duxcel.codec-fixtures','version':1,'files':[{'name':n,'version':v,'layout':25} for n,v in zip(names,versions)]};assert sorted(x.name for x in directory.iterdir())==sorted(names)
 files=[]
 for index,(name,v) in enumerate(zip(names,versions)):
  f=directory/name;before=digest(f);assert stat.S_IMODE(f.stat().st_mode)==0o444
  large=index==3;final_s,final_p=(1,27) if large else (2,3)
  conditions=[f'r0<>CASE WHEN sheet_id={final_s} AND position={final_p} THEN 1099511627775 WHEN position IN (0,1) THEN 0 ELSE 3*position+sheet_id END',f'r1<>CASE WHEN sheet_id={final_s} AND position={final_p} THEN 1099511627775 WHEN position=0 THEN 1099511627775 WHEN position=1 THEN 0 ELSE 3*position+sheet_id+1 END',f'c0<>CASE WHEN sheet_id={final_s} AND position={final_p} THEN 16383 WHEN position IN (0,1) THEN 0 ELSE (sheet_id+position)%10 END',f'c1<>CASE WHEN sheet_id={final_s} AND position={final_p} THEN 16383 WHEN position=0 THEN 16383 WHEN position=1 THEN 0 ELSE (sheet_id+position)%10+2 END','mask<>CASE WHEN position%2=1 THEN 17 ELSE 255 END','flags<>(sheet_id+position)%8','alignment<>(sheet_id+position)%4','color<>(sheet_id+position)%4','font_size<>11+(sheet_id+position)%6','numeric_format<>(sheet_id+position)%3','decimals<>(sheet_id+position)%5']
  q="SELECT (SELECT count(*) FROM _duxcel_workspace.sheets) sheets,(SELECT count(*) FROM _duxcel_workspace.objects) objects,(SELECT count(*) FROM _duxcel_workspace.queries) queries,(SELECT count(*) FROM _duxcel_workspace.formats) formats,(SELECT count(*) FROM _duxcel_workspace.formats WHERE "+' OR '.join(conditions)+") mismatches,(SELECT string_agg(column_name||':'||data_type,',' ORDER BY ordinal_position) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='formats') physical_shape"
  if v==14:q+=",(SELECT count(*) FROM _duxcel_workspace.source_bindings) bindings,(SELECT count(*) FROM _duxcel_workspace.source_versions) ledger,(SELECT count(*) FROM _duxcel_workspace.format_state WHERE source_aware) aware"
  rr,raw=invoke(['/opt/homebrew/bin/duckdb','-readonly','-json',f,q],config+'-metadata-'+str(index),30);assert rr['exit']==0,(rr,raw);m=json.loads(raw)[0];assert (m['sheets'],m['objects'],m['queries'],m['formats'],m['mismatches'])==(64,56,21,285 if large else 7,0),m
  shape=['sheet_id:INTEGER','position:INTEGER','r0:UBIGINT','r1:UBIGINT','c0:INTEGER','c1:INTEGER','mask:INTEGER','flags:INTEGER','alignment:INTEGER','color:INTEGER','font_size:INTEGER','numeric_format:INTEGER','decimals:INTEGER'];assert m['physical_shape']==','.join(reversed(shape) if large else shape),m
  if v==14:assert (m['bindings'],m['ledger'],m['aware'])==(82,288 if large else 8,0),m
  assert digest(f)==before;files.append({'name':name,'version':v,'bytes':f.stat().st_size,'sha256':before,'mode':'0444','metadata':m})
 rr,raw=invoke([base/('test-'+config),'--emit-format-rows-fixtures',directory],config+'-existing-refusal',30,env);assert rr['exit']!=0 and b'must not exist' in raw;assert {f.name:digest(f) for f in directory.iterdir()}=={f['name']:f['sha256'] for f in files}
 records[config]={'emission':r,'files':files,'existing_refusal':rr}
# Destination and missing-source preflight cannot create outputs.
for label,directory in [('absent',base/'absent-destination'),('file',base/'not-a-directory')]:
 if label=='file':directory.write_text('keep-user-file')
 r,out=invoke([base/'test-release','--emit-format-rows-fixtures',directory],label+'-refusal',30);assert r['exit']!=0 and b'existing accessible directory' in out
 if label=='absent':assert not directory.exists()
 else:assert directory.read_text()=='keep-user-file'
 directory=base/('missing-conflict-'+label);directory.mkdir();(directory/'missing-source.csv').write_text('keep-user-source');r,out=invoke([base/'test-release','--emit-format-rows-fixtures',directory],label+'-source-refusal',30);assert r['exit']!=0 and b'must not exist' in out and [p.name for p in directory.iterdir()]==['missing-source.csv'] and (directory/'missing-source.csv').read_text()=='keep-user-source'
# A real target appears after preflight and after the first owned publication.
late=base/'late-conflict';late.mkdir();target=late/names[-1];log=base/'late-conflict.log';t=time.monotonic()
with log.open('wb') as fd:
 child=subprocess.Popen([str(base/'test-release'),'--emit-format-rows-fixtures',str(late)],stdout=fd,stderr=subprocess.STDOUT);created=False
 while child.poll() is None:
  if (late/names[0]).exists() and not created:target.write_bytes(b'keep-user-conflict');created=True
  if time.monotonic()-t>30:child.kill();break
  time.sleep(.002)
 rc=child.wait()
assert created and rc!=0 and target.read_bytes()==b'keep-user-conflict' and sorted(p.name for p in late.iterdir())==[names[-1]],(created,rc,list(late.iterdir()))
records['late_conflict']={'exit':rc,'seconds':time.monotonic()-t,'preserved_sha256':digest(target),'owned_outputs_removed':True}
(base/'publication.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps({k:({'seconds':v['emission']['seconds'],'files':len(v['files'])} if k!='late_conflict' else v) for k,v in records.items()},indent=2))
