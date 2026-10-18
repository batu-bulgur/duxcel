from pathlib import Path
import json,hashlib,os,stat,subprocess,tempfile,time
root=Path.cwd();out=root/'build/bp1-source-bindings-native';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();records=[];started=time.monotonic();base=9007199254740993
entries=[{'name':n,'version':v,'layout':25} for n,v in [('source-bindings-v12.dxls',12),('source-bindings-v13.dxls',13),('source-bindings-v14.dxls',14),('source-bindings-v14-reordered.dxls',14)]]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def invoke(binary,args,label):
 cmd=[str(binary),*map(str,args)];t=time.monotonic();r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=30 if not binary.name.endswith('debug') else 180);records.append(dict(stage=label,command=cmd,seconds=time.monotonic()-t,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'emitter-progress.json').write_text(json.dumps(records,indent=2)+'\n');return r
def expected(version,large):
 sheets=[];objects=[];counters=[];source_counters=[];sources=[];queries=[];ledger=[];bindings=[]
 def bound(sheet,owner):
  if version<14:return 0
  if owner==0:return 1 if sheet in(0,2) else base+(27 if large else 2) if sheet==1 else 0
  return (1<<64)-1 if sheet==5 and owner==base else 7 if sheet==6 and owner==base+2 else 0
 for s in range(64):
  role=s%8;count=0 if role<3 else 3 if role==6 else 1
  live=0 if not count else 1 if role==3 else(1<<64)-1 if role==7 else base+count-1
  high=live if version==12 else base+30 if role in(1,5) else(1<<64)-1 if role==2 else live
  sh=0 if version<14 else (257 if large else base+17) if s==0 else (base+27 if large else base+2) if s==1 else (1<<64)-1 if s in(2,5,63) else 7 if s==6 else 0
  sheets.append(dict(id=s,name=f"Source bindings 'İ' {s}",kind=1 if s==0 else 2 if s<3 else 0,path='missing-source.csv' if s==0 else 'missing-source.parquet' if s<3 else '',schema='' if s<3 else None,relation='NULL' if s<3 else None,row_id=str((1<<40)-1),column_id=16383,anchor_row='1',anchor_col=2,end_row='3',end_col=4,top='5',horizontal=120.25,fraction=.25,selection=2,zoom=1.5))
  if s<3:queries.append(dict(sheet=s,owner='0',sql_text='SELECT * FROM __BP1_SOURCE_BINDINGS_MUST_NOT_EXECUTE'))
  if version>=13:counters.append(dict(sheet=s,high=str(high)))
  if version==14:
   bindings.append(dict(sheet=s,owner='0',version=str(bound(s,0))))
   source_counters.append(dict(sheet=s,high=str(sh)))
   entries=(257 if large else 2) if s==0 else (26 if large else 1) if s==1 else 1 if s in(2,5) else 0
   for i in range(entries):
    n=i if s==0 else (257 if large else 2)+i if s==1 else (283 if large else 3) if s==2 else (284 if large else 4)
    v=(i+1 if large else base if i else 1) if s==0 else base+1+i if s==1 else (1<<64)-1
    o=0 if s==0 else base+30 if s==1 else (1<<64)-1 if s==2 else base
    ledger.append(dict(sheet=s,version=str(v),owner=str(o),kind=1+n%2,reference='missing-source.csv' if n%2==0 else 'missing-source.parquet',encoding=0 if n%3==0 else 1,pending=n%3==0,bytes=None if n%3==0 else 147))
   if s in(1,2,6):ledger.append(dict(sheet=s,version=str(base+(27 if large else 2) if s==1 else 1 if s==2 else 7),owner=str(base+2 if s==6 else 0),kind=2,reference='missing-source.parquet',encoding=1 if s==1 else 0,pending=s!=1,bytes=147 if s==1 else None))
  for j in range(count):
   kind=0 if role==3 else 1 if role==4 else 2 if role==5 else(s//8+j)%3;identity=1 if role==3 else(1<<64)-1 if role==7 else base+j
   objects.append(dict(sheet=s,id=str(identity),name=f"Owner 'İ' {s}:{j}" if j else'NULL',row_id=str(10+20*j),column_id=8,height='8',width=4,kind=kind,mark=1 if kind==1 else 0,source_id='0',measure=0))
   if kind==2:
    sources.append(dict(sheet=s,id=str(identity),kind=2 if s==6 else 1,path='missing-source.parquet' if s==6 else 'missing-source.csv',schema='',relation='NULL'))
    queries.append(dict(sheet=s,owner=str(identity),sql_text='SELECT * FROM __BP1_SOURCE_BINDINGS_MUST_NOT_EXECUTE'))
    if version==14:bindings.append(dict(sheet=s,owner=str(identity),version=str(bound(s,identity))))
 ledger.sort(key=lambda r:(r['sheet'],int(r['version'])))
 return dict(sheets=sheets,objects=objects,counters=counters,source_counters=source_counters,sources=sources,queries=queries,ledger=ledger,bindings=bindings)
for config,binary_name in [('release','tests/test_workbook_codec_native'),('sanitize','tests/test_workbook_codec_native-debug')]:
 binary=root/binary_name;directory=Path(tempfile.mkdtemp(prefix=f'emit-{config}-',dir=out));alias=out/('alias-'+directory.name);alias.symlink_to(directory,target_is_directory=True)
 r=invoke(binary,['--emit-source-bindings-fixtures',alias],config+'-emit');assert not r.returncode and not r.stderr,r;protocol=json.loads(r.stdout);assert protocol==dict(schema='duxcel.codec-fixtures',version=1,files=entries);assert sorted(p.name for p in directory.iterdir())==sorted(x['name'] for x in entries)
 snapshots={};inodes=[];inspections=[]
 for e in entries:
  p=directory/e['name'];st=p.stat();assert stat.S_ISREG(st.st_mode) and stat.S_IMODE(st.st_mode)==0o444 and st.st_nlink==1 and 0<st.st_size<=64*1024*1024;inodes.append((st.st_dev,st.st_ino));snapshots[p.name]=dict(bytes=st.st_size,sha256=sha(p))
  counters="(SELECT list(struct_pack(sheet:=sheet_id,high:=high_water::VARCHAR) ORDER BY sheet_id) FROM _duxcel_workspace.object_counters)" if e['version']>=13 else"[]"
  source_counters="(SELECT list(struct_pack(sheet:=sheet_id,high:=high_water::VARCHAR) ORDER BY sheet_id) FROM _duxcel_workspace.source_version_counters)" if e['version']==14 else"[]"
  ledger="(SELECT list(struct_pack(sheet:=sheet_id,version:=version::VARCHAR,owner:=owner::VARCHAR,kind:=source_kind,reference:=source_reference,encoding:=encoding,pending:=definition IS NULL,bytes:=octet_length(definition)) ORDER BY sheet_id,version) FROM _duxcel_workspace.source_versions)" if e['version']==14 else"[]"
  bindings="(SELECT list(struct_pack(sheet:=sheet_id,owner:=owner::VARCHAR,version:=version::VARCHAR) ORDER BY sheet_id,owner) FROM _duxcel_workspace.source_bindings)" if e['version']==14 else"[]"
  aware="(SELECT count(*) FROM _duxcel_workspace.format_state WHERE source_aware)" if e['version']==14 else"0"
  sql="""SELECT (SELECT version FROM _duxcel_workspace.meta) AS version,(SELECT active_sheet::VARCHAR FROM _duxcel_workspace.meta) AS active,(SELECT user_scale FROM _duxcel_workspace.meta) AS scale,
(SELECT list(struct_pack(id:=id,name:=name,kind:=kind,path:=path,schema:=source_schema,relation:=source_table,row_id:=active_row::VARCHAR,column_id:=active_col,anchor_row:=anchor_row::VARCHAR,anchor_col:=anchor_col,end_row:=end_row::VARCHAR,end_col:=end_col,top:=top_row::VARCHAR,horizontal:=horizontal,fraction:=fraction,selection:=selection,zoom:=canvas_zoom) ORDER BY id) FROM _duxcel_workspace.sheets) AS sheets,
(SELECT list(struct_pack(sheet:=sheet_id,id:=id::VARCHAR,name:=name,row_id:=row_id::VARCHAR,column_id:=column_id,height:=height::VARCHAR,width:=width,kind:=kind,mark:=mark,source_id:=source_id::VARCHAR,measure:=chart_measure) ORDER BY sheet_id,id) FROM _duxcel_workspace.objects) AS objects,
COUNTERS AS counters,
SOURCE_COUNTERS AS source_counters, LEDGER AS ledger,BINDINGS AS bindings,AWARE AS aware_rows,
(SELECT list(struct_pack(sheet:=sheet_id,id:=object_id::VARCHAR,kind:=kind,path:=path,schema:=source_schema,relation:=source_table) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.table_sources) AS sources,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,sql_text:=query) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.queries) AS queries,
(SELECT list(struct_pack(name:=table_name,cols:=cols,types:=types) ORDER BY table_name) FROM (SELECT table_name,list(column_name ORDER BY ordinal_position) AS cols,list(data_type ORDER BY ordinal_position) AS types FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name IN ('meta','sheets','objects','object_counters','source_version_counters','source_versions','source_bindings') GROUP BY table_name)) AS physical""".replace('SOURCE_COUNTERS',source_counters).replace('COUNTERS',counters).replace('LEDGER',ledger).replace('AWARE',aware).replace('BINDINGS',bindings)
  t=time.monotonic();r=subprocess.run(['/opt/homebrew/bin/duckdb','-init','/dev/null','-readonly','-batch','-json',str(p),sql],capture_output=True,text=True,timeout=30);assert not r.returncode and not r.stderr,r;v=json.loads(r.stdout)[0];want=expected(e['version'],e['name'].endswith('reordered.dxls'));assert v['version']==e['version'] and v['active']=='63' and v['scale']==1.25 and v['aware_rows']==0
  for key,value in want.items():assert v[key]==value,(e['name'],key,v[key] if key not in ('objects','ledger') else 'bounded-mismatch')
  physical={x['name']:x['cols'] for x in v['physical']};types={x['name']:x['types'] for x in v['physical']};assert ('object_counters' in physical)==(e['version']>=13) and ('source_version_counters' in physical)==(e['version']==14)
  if e['version']>=13:assert physical['object_counters']==(['high_water','sheet_id'] if e['name'].endswith('reordered.dxls') else ['sheet_id','high_water'])
  if e['version']==14:assert physical['source_version_counters']==(['high_water','sheet_id'] if e['name'].endswith('reordered.dxls') else ['sheet_id','high_water']); assert physical['source_versions']==(['definition','encoding','source_reference','source_kind','owner','version','sheet_id'] if e['name'].endswith('reordered.dxls') else ['sheet_id','version','owner','source_kind','source_reference','encoding','definition'])
  if e['version']==14:assert physical['source_bindings']==(['version','owner','sheet_id'] if e['name'].endswith('reordered.dxls') else ['sheet_id','owner','version'])
  assert sha(p)==snapshots[p.name]['sha256'];inspections.append(dict(name=p.name,seconds=time.monotonic()-t,all_exact_field_values=True,counts={k:len(x) for k,x in want.items()},stdout_sha256=hashlib.sha256(r.stdout.encode()).hexdigest(),physical=physical))
 assert len(set(inodes))==4 and not(directory/'missing-source.csv').exists() and not(directory/'missing-source.parquet').exists();r=invoke(binary,['--emit-source-bindings-fixtures',directory],config+'-repeat-refused');assert r.returncode and not r.stdout;assert {p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in directory.iterdir()}==snapshots
 for kind in ['source','file','symlink']:
  d=Path(tempfile.mkdtemp(prefix='refuse-'+config+'-'+kind+'-',dir=out));target=d/('missing-source.csv' if kind=='source' else entries[0]['name']);target.symlink_to('absent-owned-input') if kind=='symlink' else target.write_bytes(b'preserve input');r=invoke(binary,['--emit-source-bindings-fixtures',d],config+'-'+kind+'-refused');assert r.returncode and not r.stdout and len(list(d.iterdir()))==1;assert os.readlink(target)=='absent-owned-input' if kind=='symlink' else target.read_bytes()==b'preserve input';target.unlink();d.rmdir()
 d=Path(tempfile.mkdtemp(prefix='race-'+config+'-',dir=out));t=time.monotonic();proc=subprocess.Popen([str(binary),'--emit-source-bindings-fixtures',str(d)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env);deadline=time.monotonic()+20
 while not(d/entries[0]['name']).exists() and proc.poll() is None and time.monotonic()<deadline:time.sleep(.0005)
 assert(d/entries[0]['name']).exists() and proc.poll() is None;conflict=d/entries[1]['name'];fd=os.open(conflict,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600);os.write(fd,b'preserve conflict');os.close(fd);stdout,stderr=proc.communicate(timeout=30 if config=='release' else 180);assert proc.returncode and not stdout and 'cannot publish fixture' in stderr and sorted(p.name for p in d.iterdir())==[entries[1]['name']] and conflict.read_bytes()==b'preserve conflict';records.append(dict(stage=config+'-late-conflict',seconds=time.monotonic()-t,exit_code=proc.returncode,stderr=stderr));conflict.unlink();d.rmdir()
 for args in [['--emit-source-bindings-fixtures'],['--source-bindings-only','extra'],['--emit-source-bindings-fixtures',out/'missing-dir']]:r=invoke(binary,args,config+'-invalid-args');assert r.returncode and not r.stdout
 alias.unlink();records.append(dict(stage=config+'-inspection',directory=str(directory.relative_to(root)),files=snapshots,protocol=protocol,inspections=inspections));print(config,'emission/exactmetadata/refusal/cleanup PASS',flush=True)
p=out/'emitter-results.json';p.write_text(json.dumps(dict(scope='Actual four closed native archives, exact SQL metadata inspected read-only. Source SQL is stored only. Late publication conflicts preserve the independent conflicting input.',seconds=time.monotonic()-started,records=records),indent=2,ensure_ascii=False)+'\n');print('emitter receipt',sha(p),flush=True)
