from pathlib import Path
import json,hashlib,os,stat,subprocess,tempfile,time
root=Path.cwd();out=root/'build/bp1-pivot-marks-native';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();records=[];started=time.monotonic()
entries=[{'name':n,'version':v,'layout':25} for n,v in [('pivot-marks-v12.dxls',12),('pivot-marks-v13.dxls',13),('pivot-marks-v14.dxls',14),('pivot-marks-v14-reordered.dxls',14)]]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def invoke(binary,args,label):
 cmd=[str(binary),*map(str,args)];t=time.monotonic();r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=30 if binary.name=='native-release' else 180);records.append({'stage':label,'command':cmd,'seconds':round(time.monotonic()-t,6),'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr});return r
sql="""SELECT (SELECT version FROM _duxcel_workspace.meta) AS version,(SELECT active_sheet FROM _duxcel_workspace.meta) AS active,
(SELECT list(name ORDER BY id) FROM _duxcel_workspace.sheets) AS names,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=id::VARCHAR,name:=name,kind:=kind,mark:=mark,source_id:=source_id::VARCHAR,measure:=chart_measure,row_id:=row_id,column_id:=column_id,height:=height,width:=width) ORDER BY sheet_id,id) FROM _duxcel_workspace.objects) AS objects,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,query:=query) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.queries) AS queries,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,value:=unique_rows) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.source_options) AS options,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,role:=role,"position":=position,name:=name,aggregate:=aggregate,op:=filter_op,value:=value,grain:=grain) ORDER BY sheet_id,object_id,role,position) FROM _duxcel_workspace.pivot_fields) AS fields,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,role:=role,"position":=position,name:=name,measure:=is_measure,aggregate:=aggregate,grain:=grain) ORDER BY sheet_id,object_id,role,position) FROM _duxcel_workspace.marks) AS marks,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,kind:=kind,path:=path,source_schema:=source_schema,source_table:=source_table,filter_column:=filter_column,filter_value:=filter_value,sort_column:=sort_column) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.table_sources) AS sources,
(SELECT count(*) FROM _duxcel_workspace.table_filters) AS filters,(SELECT count(*) FROM _duxcel_workspace.table_sorts) AS sorts,
(SELECT list(struct_pack(name:=table_name,cols:=cols) ORDER BY table_name) FROM (SELECT table_name,list(column_name ORDER BY ordinal_position) AS cols FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name IN ('sheets','objects','pivot_fields','marks') GROUP BY table_name)) AS physical"""
def expected(version,large):
 objects=[];queries=[];options=[];fields=[];marks=[];sources=[]
 for s in range(2 if large else 1):
  count=17 if large else 4;last=count-1;tid=str(18446744073709551615 if version==14 and(not large or s==1) else 9007199254740993+last)
  queries.append({'sheet':s,'owner':'0','query':f"SELECT * FROM __BP1_PM_PRIMARY_MUST_NOT_EXECUTE /* 'İ' {s} */;  \n"})
  if s%2:options.append({'sheet':s,'owner':'0','value':True})
  for j in range(count):
   table=j==last;full=large or j!=0;owner=tid if table else str(9007199254740993+j);kind=2 if table else (j%2 if large else 1 if j==2 else 0);source=tid if not table and(large and j%2==0 or not large and j==1) else'0'
   objects.append({'sheet':s,'owner':owner,'name':f"Pivot 'İ' {s}:{j}" if j else'NULL','kind':kind,'mark':1 if kind==1 else 0,'source_id':source,'measure':1 if not table and full else 0,'row_id':10+j*12,'column_id':8,'height':8,'width':4})
   if table:
    queries.append({'sheet':s,'owner':owner,'query':f"SELECT * FROM __BP1_PM_TABLE_MUST_NOT_EXECUTE /* 'İ' {s} */;  \n"})
    if(s+j+1)%2:options.append({'sheet':s,'owner':owner,'value':True})
    sources.append({'sheet':s,'owner':owner,'kind':1,'path':'missing-source.csv','source_schema':'','source_table':'NULL','filter_column':None,'filter_value':None,'sort_column':None});continue
   if not full:continue
   raw=[(0,0,"Date 'İ'",0,5,None,1),(0,1,"Date 'İ'",0,5,None,2),(1,0,"Column 'İ'",0,5,None,3),(2,0,"Amount 'İ'",3,5,None,0),(2,1,"Amount 'İ'",2,5,None,0)]
   raw +=[(3,f,f"Filter 'İ' {f}",0,10 if f==3 else 0,[None,'','NULL','["10","20"]'][f],0) for f in range(4)]
   for role,pos,name,agg,op,value,grain in raw:fields.append({'sheet':s,'owner':owner,'role':role,'position':pos,'name':name,'aggregate':agg,'op':op,'value':value,'grain':grain})
   for role,n in enumerate([2,1,2,2,2,1]):
    for f in range(n):marks.append({'sheet':s,'owner':owner,'role':role,'position':f,'name':f"Mark 'İ' {role}:{f}",'measure':f!=0,'aggregate':2 if f else 3,'grain':0 if f else 1})
 return {'objects':objects,'queries':queries,'options':options or None,'fields':fields,'marks':marks,'sources':sources,'filters':0,'sorts':0}
for config in ['release','sanitize']:
 binary=out/f'native-{config}';directory=Path(tempfile.mkdtemp(prefix=f'emit-{config}-',dir=out));alias=out/('alias-'+directory.name);alias.symlink_to(directory,target_is_directory=True)
 r=invoke(binary,['--emit-pivot-marks-fixtures',alias],config+'-emit');assert not r.returncode and not r.stderr,r;protocol=json.loads(r.stdout);assert protocol=={'schema':'duxcel.codec-fixtures','version':1,'files':entries};assert sorted(p.name for p in directory.iterdir())==sorted(x['name'] for x in entries)
 snapshots={};inodes=[];inspections=[]
 for e in entries:
  p=directory/e['name'];st=p.stat();assert stat.S_ISREG(st.st_mode) and stat.S_IMODE(st.st_mode)==0o444 and st.st_nlink==1 and 0<st.st_size<=64*1024*1024;inodes.append((st.st_dev,st.st_ino));snapshots[p.name]={'bytes':st.st_size,'sha256':sha(p)}
  r=subprocess.run(['/opt/homebrew/bin/duckdb','-init','/dev/null','-readonly','-batch','-json',str(p),sql],capture_output=True,text=True,timeout=30);assert not r.returncode and not r.stderr,r;v=json.loads(r.stdout)[0];large='reordered' in p.name;want=expected(e['version'],large);assert v['version']==e['version'] and v['active']==str(int(large)) and v['names']==[f"Pivot 'İ' {s}" for s in range(2 if large else 1)]
  for k,x in want.items():assert v[k]==x,(e['name'],k)
  physical={x['name']:x['cols'] for x in v['physical']}
  assert physical['pivot_fields']==(['grain','value','filter_op','aggregate','name','position','role','object_id','sheet_id'] if large else['sheet_id','object_id','role','position','name','aggregate','filter_op','value','grain'])
  assert physical['marks']==(['grain','aggregate','is_measure','name','position','role','object_id','sheet_id'] if large else['sheet_id','object_id','role','position','name','is_measure','aggregate','grain'])
  assert sha(p)==snapshots[p.name]['sha256'];inspections.append({'name':p.name,'counts':{k:len(x or[]) for k,x in want.items() if isinstance(x,list) or x is None},'all_exact_field_values':True,'stdout_sha256':hashlib.sha256(r.stdout.encode()).hexdigest(),'physical':physical})
 assert len(set(inodes))==4 and not(directory/'missing-source.csv').exists();r=invoke(binary,['--emit-pivot-marks-fixtures',directory],config+'-repeat-refused');assert r.returncode and not r.stdout;assert {p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in directory.iterdir()}==snapshots
 for kind in ['source','file','symlink']:
  d=Path(tempfile.mkdtemp(prefix='refuse-'+config+'-'+kind+'-',dir=out));target=d/('missing-source.csv' if kind=='source' else entries[0]['name']);target.symlink_to('absent-owned-input') if kind=='symlink' else target.write_bytes(b'preserve input');r=invoke(binary,['--emit-pivot-marks-fixtures',d],config+'-'+kind+'-refused');assert r.returncode and not r.stdout and len(list(d.iterdir()))==1;assert os.readlink(target)=='absent-owned-input' if kind=='symlink' else target.read_bytes()==b'preserve input';target.unlink();d.rmdir()
 d=Path(tempfile.mkdtemp(prefix='race-'+config+'-',dir=out));t=time.monotonic();proc=subprocess.Popen([str(binary),'--emit-pivot-marks-fixtures',str(d)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env);deadline=time.monotonic()+20
 while not(d/entries[0]['name']).exists() and proc.poll() is None and time.monotonic()<deadline:time.sleep(.0005)
 assert(d/entries[0]['name']).exists() and proc.poll() is None;conflict=d/entries[1]['name'];fd=os.open(conflict,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600);os.write(fd,b'preserve conflict');os.close(fd);stdout,stderr=proc.communicate(timeout=30 if config=='release' else 180);assert proc.returncode and not stdout and 'cannot publish fixture' in stderr and sorted(p.name for p in d.iterdir())==[entries[1]['name']] and conflict.read_bytes()==b'preserve conflict';records.append({'stage':config+'-late-conflict','seconds':round(time.monotonic()-t,6),'exit_code':proc.returncode,'stderr':stderr});conflict.unlink();d.rmdir()
 for args in [['--emit-pivot-marks-fixtures'],['--pivot-marks-only','extra'],['--emit-pivot-marks-fixtures',out/'missing-dir']]:r=invoke(binary,args,config+'-invalid-args');assert r.returncode and not r.stdout
 alias.unlink();records.append({'stage':config+'-inspection','directory':str(directory.relative_to(root)),'files':snapshots,'protocol':protocol,'inspections':inspections});print(config,'emission/exactmetadata/refusal/cleanup PASS',flush=True)
p=out/'emitter-results.json';p.write_text(json.dumps(records,indent=2,ensure_ascii=False)+'\n');mp=out/'manifest.json';m=json.loads(mp.read_text());m['emitter_results']={'path':str(p.relative_to(root)),'sha256':sha(p),'seconds':round(time.monotonic()-started,6),'script_sha256':sha(out/'check-emitters.py')};m['total_seconds']=round(m['seconds']+m['emitter_results']['seconds'],6);m['status']='passed';mp.write_text(json.dumps(m,indent=2)+'\n');print('manifest',sha(mp),flush=True)
