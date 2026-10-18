from pathlib import Path
import json,hashlib,os,stat,subprocess,tempfile,time
root=Path.cwd();out=root/'build/bp1-tables-native-null-corrected';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();records=[];started=time.monotonic()
entries=[{'name':n,'version':v,'layout':25} for n,v in [('tables-v12.dxls',12),('tables-v13.dxls',13),('tables-v14.dxls',14),('tables-v14-reordered.dxls',14)]]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def invoke(binary,args,label):
 command=[str(binary),*map(str,args)];t=time.monotonic();r=subprocess.run(command,capture_output=True,text=True,env=env,timeout=30 if binary.name=='native-release' else 180);records.append({'stage':label,'command':command,'seconds':round(time.monotonic()-t,6),'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr});return r
sql="""SELECT (SELECT version FROM _duxcel_workspace.meta) AS version,(SELECT active_sheet FROM _duxcel_workspace.meta) AS active,(SELECT user_scale FROM _duxcel_workspace.meta) AS scale,
(SELECT list(name ORDER BY id) FROM _duxcel_workspace.sheets) AS names,(SELECT list(path ORDER BY id) FROM _duxcel_workspace.sheets) AS refs,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=id::VARCHAR,name:=name,row_id:=row_id,column_id:=column_id,height:=height,width:=width,kind:=kind,mark:=mark,source_id:=source_id::VARCHAR,measure:=chart_measure) ORDER BY sheet_id,id) FROM _duxcel_workspace.objects) AS objects,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,query:=query) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.queries) AS queries,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,value:=unique_rows) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.source_options) AS options,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,kind:=kind,path:=path,schema_text:=source_schema,table_text:=source_table,filter_column:=filter_column,filter_value:=filter_value,filter_op:=filter_op,group_column:=group_column,measure_column:=measure_column,aggregate:=aggregate,sort_column:=sort_column,descending:=descending) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.table_sources) AS sources,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,"position":=position,name:=name,op:=op,value:=value) ORDER BY sheet_id,object_id,position) FROM _duxcel_workspace.table_filters) AS filters,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,"position":=position,name:=name,descending:=descending) ORDER BY sheet_id,object_id,position) FROM _duxcel_workspace.table_sorts) AS sorts,
(SELECT list(struct_pack(name:=table_name,cols:=cols) ORDER BY table_name) FROM (SELECT table_name,list(column_name ORDER BY ordinal_position) AS cols FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name IN ('sheets','objects','queries','source_options','table_sources','table_filters','table_sorts') GROUP BY table_name)) AS physical"""
def expected(version,large):
 objects=[];queries=[];options=[];sources=[];filters=[];sorts=[];sheets=2 if large else 1;count=32 if large else 4
 for s in range(sheets):
  queries.append({'sheet':s,'owner':'0','query':f"SELECT * FROM __BP1_PRIMARY_MUST_NOT_EXECUTE /* 'İ' {s} */;  \n"})
  if s%2:options.append({'sheet':s,'owner':'0','value':True})
  for j in range(count):
   owner=str(18446744073709551615 if large and s==1 and j==31 or not large and version==14 and j==3 else 9007199254740993+j)
   objects.append({'sheet':s,'owner':owner,'name':f"Table 'İ' {s}:{j}" if j else 'NULL','row_id':10+j*12,'column_id':8,'height':8,'width':4,'kind':2,'mark':0,'source_id':'0','measure':0})
   queries.append({'sheet':s,'owner':owner,'query':f"SELECT * FROM __BP1_TABLE_MUST_NOT_EXECUTE /* 'İ' {s}:{j} */;  \n"})
   if (s+j+1)%2:options.append({'sheet':s,'owner':owner,'value':True})
   row={'sheet':s,'owner':owner,'kind':1 if large else j+1,'path':'missing-source.csv' if large else ['missing-source.csv','missing-source.parquet','missing-source.duckdb',''][j],
    'schema_text':[None,'','NULL'][j%3] if large else [None,'','NULL',None][j],'table_text':['','NULL',None][j%3] if large else [None,'NULL','','NULL'][j],
    'filter_column':"P 'İ'" if large else None if j==1 else f"P 'İ' {j}",'filter_value':[None,'','NULL'][(s+j)%3] if large else None if j<2 else '' if j==2 else 'NULL','filter_op':0 if large or j==1 or j==3 else 4 if j==0 else 1,
    'group_column':"Group 'İ'" if large and j%2 or not large and j==1 else None,'measure_column':"Measure 'İ'" if large and j%2 or not large and j==1 else None,'aggregate':3 if large and j%2 or not large and j==1 else 0,
    'sort_column':"S 'İ' 0" if large or j==1 else None if j==0 else 'NULL' if j==2 else "S 'İ' 3",'descending':bool((s+j)%2) if large else j==1 or j==3}
   sources.append(row)
   if large:
    for f in range(8):filters.append({'sheet':s,'owner':owner,'position':f,'name':f"F 'İ' {f}",'op':10 if f==7 else 0,'value':'["10","20"]' if f==7 else [None,'','NULL'][f%3]})
    for n in range(7):sorts.append({'sheet':s,'owner':owner,'position':n,'name':f"S 'İ' {n+1}",'descending':bool((n+1)%2)})
   elif version==14 and j==0:filters.append({'sheet':s,'owner':owner,'position':0,'name':"Large 'İ'",'op':0,'value':'x'*65537})
 return {'objects':objects,'queries':queries,'options':options,'sources':sources,'filters':filters or None,'sorts':sorts or None}
for config in ['release','sanitize']:
 binary=out/f'native-{config}';directory=Path(tempfile.mkdtemp(prefix=f'tables-{config}-',dir=out));alias=out/('alias-'+directory.name);alias.symlink_to(directory,target_is_directory=True)
 r=invoke(binary,['--emit-tables-fixtures',alias],config+'-emit');assert r.returncode==0 and not r.stderr,r;protocol=json.loads(r.stdout);assert protocol=={'schema':'duxcel.codec-fixtures','version':1,'files':entries};assert sorted(p.name for p in directory.iterdir())==sorted(x['name'] for x in entries)
 snapshots={};inodes=[];inspection=[]
 for entry in entries:
  p=directory/entry['name'];st=p.stat();assert stat.S_ISREG(st.st_mode) and stat.S_IMODE(st.st_mode)==0o444 and st.st_nlink==1 and 0<st.st_size<=64*1024*1024;inodes.append((st.st_dev,st.st_ino));snapshots[p.name]={'bytes':st.st_size,'sha256':sha(p)}
  t=time.monotonic();r=subprocess.run(['/opt/homebrew/bin/duckdb','-init','/dev/null','-readonly','-batch','-json',str(p),sql],capture_output=True,text=True,timeout=30);assert not r.returncode and not r.stderr,r;v=json.loads(r.stdout)[0];large='reordered' in p.name;expect=expected(entry['version'],large)
  assert v['version']==entry['version'] and v['active']==str(int(large)) and v['scale']==1.25 and v['names']==[f"Table 'İ' {s}" for s in range(2 if large else 1)] and v['refs']==['missing-source.csv']*(2 if large else 1)
  for key,want in expect.items():assert v[key]==want,(entry['name'],key)
  physical={x['name']:x['cols'] for x in v['physical']};assert len(physical['sheets'])==25 and physical['sheets'][0]==('canvas_zoom' if large else'id');assert len(physical['objects'])==11 and physical['objects'][0]==('chart_measure' if large else'sheet_id')
  assert physical['queries']==(['query','object_id','sheet_id'] if large else['sheet_id','object_id','query']);assert physical['source_options']==(['unique_rows','object_id','sheet_id'] if large else['sheet_id','object_id','unique_rows'])
  assert len(physical['table_sources'])==14 and physical['table_sources'][0]==('descending' if large else'sheet_id');assert physical['table_filters']==(['value','op','name','position','object_id','sheet_id'] if large else['sheet_id','object_id','position','name','op','value']);assert physical['table_sorts']==(['descending','name','position','object_id','sheet_id'] if large else['sheet_id','object_id','position','name','descending'])
  assert sha(p)==snapshots[p.name]['sha256'];inspection.append({'name':p.name,'seconds':round(time.monotonic()-t,6),'counts':{key:len(value or[]) for key,value in expect.items()},'full_exact_field_values_checked':True,'metadata_stdout_sha256':hashlib.sha256(r.stdout.encode()).hexdigest(),'physical':physical})
 assert len(set(inodes))==4 and not any((directory/n).exists() for n in ['missing-source.csv','missing-source.parquet','missing-source.duckdb'])
 r=invoke(binary,['--emit-tables-fixtures',directory],config+'-repeat-refused');assert r.returncode and not r.stdout;assert {p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in directory.iterdir()}==snapshots
 for case in ['source','file','symlink']:
  refusal=Path(tempfile.mkdtemp(prefix=f'tables-refuse-{config}-{case}-',dir=out));target=refusal/('missing-source.csv' if case=='source' else entries[0]['name'])
  if case=='symlink':target.symlink_to('absent-user-file')
  else:target.write_bytes(b'preserve existing input')
  r=invoke(binary,['--emit-tables-fixtures',refusal],config+'-'+case+'-refused');assert r.returncode and not r.stdout and len(list(refusal.iterdir()))==1
  assert target.is_symlink() and os.readlink(target)=='absent-user-file' if case=='symlink' else target.read_bytes()==b'preserve existing input';target.unlink();refusal.rmdir()
 race=Path(tempfile.mkdtemp(prefix=f'tables-race-{config}-',dir=out));command=[str(binary),'--emit-tables-fixtures',str(race)];t=time.monotonic();proc=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env);deadline=time.monotonic()+20
 while not(race/entries[0]['name']).exists() and proc.poll() is None and time.monotonic()<deadline:time.sleep(.0005)
 assert (race/entries[0]['name']).exists() and proc.poll() is None,'missed late-conflict setup'
 conflict=race/entries[1]['name'];fd=os.open(conflict,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600);os.write(fd,b'preserve independent conflict');os.close(fd);stdout,stderr=proc.communicate(timeout=30 if config=='release' else 180)
 assert proc.returncode and not stdout and 'cannot publish fixture' in stderr and sorted(p.name for p in race.iterdir())==[entries[1]['name']] and conflict.read_bytes()==b'preserve independent conflict';records.append({'stage':config+'-late-conflict','seconds':round(time.monotonic()-t,6),'exit_code':proc.returncode,'stderr':stderr,'remaining':[entries[1]['name']]});conflict.unlink();race.rmdir()
 for args in [['--emit-tables-fixtures'],['--tables-only','extra'],['--emit-tables-fixtures',out/'absent-directory']]:r=invoke(binary,args,config+'-invalid-args');assert r.returncode and not r.stdout
 alias.unlink();records.append({'stage':config+'-inspection','directory':str(directory.relative_to(root)),'protocol':protocol,'files':snapshots,'inspection':inspection});print(config,'four-file emission/exact metadata/refusal/cleanup passed',flush=True)
p=out/'emitter-results.json';p.write_text(json.dumps(records,indent=2,ensure_ascii=False)+'\n');mp=out/'manifest.json';m=json.loads(mp.read_text());elapsed=round(time.monotonic()-started,6);m['emitter_results']={'path':str(p.relative_to(root)),'sha256':sha(p),'seconds':elapsed,'script':'build/bp1-tables-native-null-corrected/check-emitters.py','script_sha256':sha(out/'check-emitters.py')};m['total_seconds']=round(m['seconds']+elapsed,6);m['supplementary_cli']={'path':'/opt/homebrew/bin/duckdb','sha256':sha('/opt/homebrew/bin/duckdb'),'scope':'Serial read-only metadata field inspection; native chunk-based type proof recorded separately.'};m['status']='passed';mp.write_text(json.dumps(m,indent=2)+'\n');print('manifest',sha(mp),flush=True)
