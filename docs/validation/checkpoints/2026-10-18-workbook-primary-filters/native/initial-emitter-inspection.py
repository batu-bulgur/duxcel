from pathlib import Path
import json,hashlib,os,stat,subprocess,tempfile,time
root=Path.cwd();out=root/'build/bp1-filters-native';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();records=[];started=time.monotonic()
entries=[{'name':n,'version':v,'layout':25} for n,v in [('filters-v12.dxls',12),('filters-v13.dxls',13),('filters-v14.dxls',14),('filters-v14-reordered.dxls',14)]]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def invoke(binary,args,label):
 cmd=[str(binary),*map(str,args)];t=time.monotonic();r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=30 if binary.name=='native-release' else 180);records.append({'stage':label,'command':cmd,'seconds':round(time.monotonic()-t,6),'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr});return r
sql="""SELECT (SELECT version FROM _duxcel_workspace.meta) AS version,(SELECT active_sheet FROM _duxcel_workspace.meta) AS active,(SELECT user_scale FROM _duxcel_workspace.meta) AS scale,
(SELECT list(struct_pack(id:=id,name:=name,kind:=kind,path:=path,source_schema:=source_schema,source_table:=source_table,filter_column:=filter_column,filter_value:=filter_value,filter_op:=filter_op,group_column:=group_column,measure_column:=measure_column,aggregate:=aggregate,sort_column:=sort_column,descending:=descending,active_row:=active_row,active_col:=active_col,anchor_row:=anchor_row,anchor_col:=anchor_col,end_row:=end_row,end_col:=end_col,top_row:=top_row,horizontal:=horizontal,fraction:=fraction,selection:=selection,canvas_zoom:=canvas_zoom) ORDER BY id) FROM _duxcel_workspace.sheets) AS sheets,
(SELECT list(struct_pack(sheet:=sheet_id,"position":=position,name:=name,op:=op,value:=value) ORDER BY sheet_id,position) FROM _duxcel_workspace.filters) AS filters,
(SELECT list(struct_pack(sheet:=sheet_id,"position":=position,name:=name,descending:=descending) ORDER BY sheet_id,position) FROM _duxcel_workspace.sorts) AS sorts,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,query:=query) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.queries) AS queries,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,value:=unique_rows) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.source_options) AS options,
(SELECT count(*) FROM _duxcel_workspace.objects) AS objects,
(SELECT list(struct_pack(name:=table_name,cols:=cols) ORDER BY table_name) FROM (SELECT table_name,list(column_name ORDER BY ordinal_position) AS cols FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name IN ('meta','sheets','filters','sorts','queries','source_options') GROUP BY table_name)) AS physical"""
def expected(version,large):
 sheets=[];filters=[];sorts=[];queries=[];options=[]
 for s in range(19 if large else 1):
  primary='p'*65537 if version==14 and not large else [None,'','NULL'][s%3]
  sheets.append(dict(id=s,name=f"Filtered 'İ' {s}",kind=1,path='missing-source.csv',source_schema='',source_table='NULL',filter_column="Primary 'İ'",filter_value=primary,filter_op=0,group_column="Group 'İ'",measure_column="Measure 'İ'",aggregate=3,sort_column="Sort 'İ' 0",descending=bool(s%2),active_row=str((1<<40)-1),active_col='16383',anchor_row='1',anchor_col='2',end_row='3',end_col='4',top_row='5',horizontal=120.25,fraction=.25,selection=2,canvas_zoom=1.5))
  for f in range(15 if large else 4 if version==14 else 3):
   value='["10","20"]' if large and f==14 else 'x'*65537 if version==14 and not large and f==3 else [None,'','NULL'][f%3]
   filters.append(dict(sheet=s,position=f,name=f"Filter 'İ' {f}",op=10 if large and f==14 else 0,value=value))
  sorts.extend(dict(sheet=s,position=n-1,name=f"Sort 'İ' {n}",descending=bool(n%2)) for n in range(1,8))
  queries.append(dict(sheet=s,owner='0',query=f"SELECT * FROM __BP1_FILTERS_MUST_NOT_EXECUTE /* 'İ' {s} */;  \n"))
  if s%2:options.append(dict(sheet=s,owner='0',value=True))
 return dict(sheets=sheets,filters=filters,sorts=sorts,queries=queries,options=options or None,objects=0)
for config in ['release','sanitize']:
 binary=out/f'native-{config}';directory=Path(tempfile.mkdtemp(prefix=f'emit-{config}-',dir=out));alias=out/('alias-'+directory.name);alias.symlink_to(directory,target_is_directory=True)
 r=invoke(binary,['--emit-filters-fixtures',alias],config+'-emit');assert not r.returncode and not r.stderr,r;protocol=json.loads(r.stdout);assert protocol=={'schema':'duxcel.codec-fixtures','version':1,'files':entries};assert sorted(p.name for p in directory.iterdir())==sorted(x['name'] for x in entries)
 snapshots={};inodes=[];inspections=[]
 for e in entries:
  p=directory/e['name'];st=p.stat();assert stat.S_ISREG(st.st_mode) and stat.S_IMODE(st.st_mode)==0o444 and st.st_nlink==1 and 0<st.st_size<=64*1024*1024;inodes.append((st.st_dev,st.st_ino));snapshots[p.name]={'bytes':st.st_size,'sha256':sha(p)}
  t=time.monotonic();r=subprocess.run(['/opt/homebrew/bin/duckdb','-init','/dev/null','-readonly','-batch','-json',str(p),sql],capture_output=True,text=True,timeout=30);assert not r.returncode and not r.stderr,r;v=json.loads(r.stdout)[0];large='reordered' in p.name;want=expected(e['version'],large);assert v['version']==e['version'] and v['active']==str(18 if large else 0) and v['scale']==1.25
  for k,x in want.items():assert v[k]==x,(e['name'],k)
  physical={x['name']:x['cols'] for x in v['physical']};assert physical['filters']==(['value','op','name','position','sheet_id'] if large else['sheet_id','position','name','op','value'])
  assert physical['sorts']==(['descending','name','position','sheet_id'] if large else['sheet_id','position','name','descending'])
  assert sha(p)==snapshots[p.name]['sha256'];inspections.append({'name':p.name,'seconds':round(time.monotonic()-t,6),'counts':{k:len(x or[]) for k,x in want.items() if isinstance(x,list) or x is None},'all_exact_field_values':True,'stdout_sha256':hashlib.sha256(r.stdout.encode()).hexdigest(),'physical':physical})
 assert len(set(inodes))==4 and not(directory/'missing-source.csv').exists();r=invoke(binary,['--emit-filters-fixtures',directory],config+'-repeat-refused');assert r.returncode and not r.stdout;assert {p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in directory.iterdir()}==snapshots
 for kind in ['source','file','symlink']:
  d=Path(tempfile.mkdtemp(prefix='refuse-'+config+'-'+kind+'-',dir=out));target=d/('missing-source.csv' if kind=='source' else entries[0]['name']);target.symlink_to('absent-owned-input') if kind=='symlink' else target.write_bytes(b'preserve input');r=invoke(binary,['--emit-filters-fixtures',d],config+'-'+kind+'-refused');assert r.returncode and not r.stdout and len(list(d.iterdir()))==1;assert os.readlink(target)=='absent-owned-input' if kind=='symlink' else target.read_bytes()==b'preserve input';target.unlink();d.rmdir()
 d=Path(tempfile.mkdtemp(prefix='race-'+config+'-',dir=out));t=time.monotonic();proc=subprocess.Popen([str(binary),'--emit-filters-fixtures',str(d)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env);deadline=time.monotonic()+20
 while not(d/entries[0]['name']).exists() and proc.poll() is None and time.monotonic()<deadline:time.sleep(.0005)
 assert(d/entries[0]['name']).exists() and proc.poll() is None;conflict=d/entries[1]['name'];fd=os.open(conflict,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600);os.write(fd,b'preserve conflict');os.close(fd);stdout,stderr=proc.communicate(timeout=30 if config=='release' else 180);assert proc.returncode and not stdout and 'cannot publish fixture' in stderr and sorted(p.name for p in d.iterdir())==[entries[1]['name']] and conflict.read_bytes()==b'preserve conflict';records.append({'stage':config+'-late-conflict','seconds':round(time.monotonic()-t,6),'exit_code':proc.returncode,'stderr':stderr});conflict.unlink();d.rmdir()
 for args in [['--emit-filters-fixtures'],['--filters-only','extra'],['--emit-filters-fixtures',out/'missing-dir']]:r=invoke(binary,args,config+'-invalid-args');assert r.returncode and not r.stdout
 alias.unlink();records.append({'stage':config+'-inspection','directory':str(directory.relative_to(root)),'files':snapshots,'protocol':protocol,'inspections':inspections});print(config,'emission/exactmetadata/refusal/cleanup PASS',flush=True)
p=out/'emitter-results.json';p.write_text(json.dumps({'scope':'Actual four closed native archives; exact SQL metadata inspected read-only after emit exit. No source execution. Late publication conflicts preserve only the independently created conflict.','seconds':round(time.monotonic()-started,6),'records':records},indent=2,ensure_ascii=False)+'\n');print('emitter receipt',sha(p),flush=True)
