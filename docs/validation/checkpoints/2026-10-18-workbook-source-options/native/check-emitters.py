from pathlib import Path
import hashlib,json,os,stat,subprocess,tempfile,time
root=Path.cwd();out=root/'build/bp1-options-native';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();receipts=[];start=time.perf_counter()
entries=[{'name':name,'version':version,'layout':25} for name,version in [('options-v12.dxls',12),('options-v13.dxls',13),('options-v14.dxls',14),('options-v14-reordered.dxls',14)]]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def invoke(binary,args,label):
 cmd=[str(binary),*map(str,args)];t=time.perf_counter();r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=30 if binary.name=='native-release' and args and args[0]=='--emit-source-options-fixtures' else 180);receipts.append({'stage':label,'command':cmd,'seconds':round(time.perf_counter()-t,6),'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr});return r
sql="""SELECT (SELECT version FROM _duxcel_workspace.meta) AS version,
(SELECT active_sheet::VARCHAR FROM _duxcel_workspace.meta) AS active,
(SELECT user_scale FROM _duxcel_workspace.meta) AS scale,
(SELECT list(name ORDER BY id) FROM _duxcel_workspace.sheets) AS names,
(SELECT list(path ORDER BY id) FROM _duxcel_workspace.sheets) AS refs,
(SELECT list(source_schema ORDER BY id) FROM _duxcel_workspace.sheets) AS schemas,
(SELECT list(source_table ORDER BY id) FROM _duxcel_workspace.sheets) AS source_tables,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,text_hex:=hex(query)) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.queries) AS queries,
(SELECT count(*) FROM _duxcel_workspace.objects) AS objects,
(SELECT count(*) FROM _duxcel_workspace.objects WHERE kind<>2 OR mark<>0 OR source_id<>0 OR chart_measure<>0 OR row_id<10 OR (row_id-10)%12<>0 OR row_id>382 OR column_id<>8 OR height<>8 OR width<>4) AS bad_objects,
(SELECT count(*) FROM _duxcel_workspace.table_sources) AS table_sources,
(SELECT count(*) FROM _duxcel_workspace.table_sources WHERE path<>'missing-source.csv' OR kind<>1 OR source_schema<>'' OR source_table<>'NULL') AS bad_sources,
(SELECT count(*) FROM _duxcel_workspace.sorts) AS sorts,
(SELECT count(*) FROM _duxcel_workspace.preferences) AS preferences,
(SELECT count(*) FROM _duxcel_workspace.preferences WHERE name='reduce_motion' AND value=false) AS motion_false,
(SELECT list(column_name ORDER BY ordinal_position) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='sheets') AS physical_sheets,
(SELECT list(column_name ORDER BY ordinal_position) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='objects') AS physical_objects,
(SELECT list(column_name ORDER BY ordinal_position) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='queries') AS physical_queries,
(SELECT list(struct_pack(sheet:=sheet_id,owner:=object_id::VARCHAR,value:=unique_rows) ORDER BY sheet_id,object_id) FROM _duxcel_workspace.source_options) AS options,
(SELECT list(column_name ORDER BY ordinal_position) FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='source_options') AS physical_options,
(SELECT data_type FROM information_schema.columns WHERE table_schema='_duxcel_workspace' AND table_name='source_options' AND column_name='unique_rows') AS option_type"""
for config in ['release','sanitize']:
 binary=out/f'native-{config}';directory=Path(tempfile.mkdtemp(prefix=f'options-{config}-',dir=out));alias=out/f'alias-options-{config}-{directory.name}';alias.symlink_to(directory,target_is_directory=True)
 run=invoke(binary,['--emit-source-options-fixtures',alias],f'{config}-source-options-emit');assert run.returncode==0 and not run.stderr,run
 protocol=json.loads(run.stdout);assert protocol=={'schema':'duxcel.codec-fixtures','version':1,'files':entries};assert sorted(p.name for p in directory.iterdir())==sorted(e['name'] for e in entries)
 snapshots={};inodes=[];metadata=[]
 for entry in entries:
  p=directory/entry['name'];st=p.stat();assert stat.S_ISREG(st.st_mode) and stat.S_IMODE(st.st_mode)==0o444 and st.st_nlink==1 and 0<st.st_size<=64*1024*1024;inodes.append((st.st_dev,st.st_ino));snapshots[p.name]={'bytes':st.st_size,'sha256':sha(p)}
  t=time.perf_counter();r=subprocess.run(['/opt/homebrew/bin/duckdb','-init','/dev/null','-readonly','-batch','-json',str(p),sql],capture_output=True,text=True,timeout=30);assert r.returncode==0 and not r.stderr,r;v=json.loads(r.stdout)[0]
  assert v['version']==entry['version'] and v['active']=='63' and v['scale']==1.25 and v['names']==[f"Sorted 'İ' {i}" for i in range(64)]
  assert v['refs']==['missing-source.csv']*64 and v['schemas']==['']*64 and v['source_tables']==['NULL']*64
  assert v['objects']==2048 and v['table_sources']==2048 and not v['bad_objects'] and not v['bad_sources'] and not v['sorts'] and v['preferences']==1 and v['motion_false']==1
  expected=[]
  for sheet in range(64):
   for slot in range(33):
    owner=0 if not slot else 18446744073709551615 if sheet==63 and slot==32 else 9007199254740993+slot-1
    text='NULL' if not sheet and not slot else f"SELECT 1 /* 'İ' {sheet}:{slot} */;  \n"
    if sheet==63 and slot==32:
     if 'blank' in p.name:text='   '
     if 'nul' in p.name:text='a\0b'
    expected.append({'sheet':sheet,'owner':str(owner),'text_hex':text.encode().hex().upper()})
  assert len(v['queries'])==2112 and v['queries']==expected
  assert v['options']==[{'sheet':item['sheet'],'owner':item['owner'],'value':True} for item in expected] and v['option_type']=='BOOLEAN'
  assert v['physical_options']==(['unique_rows','object_id','sheet_id'] if 'reordered' in p.name else ['sheet_id','object_id','unique_rows'])
  reordered='reordered' in p.name;assert len(v['physical_sheets'])==25 and v['physical_sheets'][0]==('canvas_zoom' if reordered else'id')
  assert len(v['physical_objects'])==11 and v['physical_objects'][0]==('chart_measure' if reordered else'sheet_id')
  assert v['physical_queries']==(['query','object_id','sheet_id'] if reordered else['sheet_id','object_id','query'])
  assert sha(p)==snapshots[p.name]['sha256'];metadata.append({'name':p.name,'seconds':round(time.perf_counter()-t,6),'rows':2112,'objects':2048,'all_exact_owner_texts_checked':True,'all_exact_owner_boolean_values_checked':True,'boolean_type':v['option_type'],'physical_options':v['physical_options'],'first_hex':v['queries'][0]['text_hex'],'last_hex':v['queries'][-1]['text_hex'],'physical_queries':v['physical_queries']})
 assert len(set(inodes))==4 and not(directory/'missing-source.csv').exists()
 repeat=invoke(binary,['--emit-source-options-fixtures',directory],f'{config}-repeat-refused');assert repeat.returncode!=0 and not repeat.stdout
 assert {p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in directory.iterdir()}==snapshots
 for case in ['source','file','symlink']:
  refusal=Path(tempfile.mkdtemp(prefix=f'options-refuse-{config}-{case}-',dir=out));target=refusal/('missing-source.csv' if case=='source' else entries[0]['name'])
  if case=='symlink':target.symlink_to('absent-user-target')
  else:target.write_bytes(b'preserve existing input')
  failed=invoke(binary,['--emit-source-options-fixtures',refusal],f'{config}-existing-{case}-refused');assert failed.returncode!=0 and not failed.stdout and len(list(refusal.iterdir()))==1
  if case=='symlink':assert target.is_symlink() and os.readlink(target)=='absent-user-target'
  else:assert target.read_bytes()==b'preserve existing input'
  target.unlink();refusal.rmdir()
 race=Path(tempfile.mkdtemp(prefix=f'options-race-{config}-',dir=out));cmd=[str(binary),'--emit-source-options-fixtures',str(race)];t=time.perf_counter();proc=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env);deadline=time.monotonic()+20
 while not(race/entries[0]['name']).exists() and proc.poll() is None and time.monotonic()<deadline:time.sleep(.0005)
 assert (race/entries[0]['name']).exists() and proc.poll() is None,'late-conflict setup missed'
 conflict=race/entries[-1]['name'];fd=os.open(conflict,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600);os.write(fd,b'preserve independent conflict');os.close(fd);stdout,stderr=proc.communicate(timeout=30 if config=='release' else 180)
 assert proc.returncode!=0 and not stdout and 'cannot publish fixture' in stderr and sorted(p.name for p in race.iterdir())==[entries[-1]['name']] and conflict.read_bytes()==b'preserve independent conflict'
 receipts.append({'stage':f'{config}-late-conflict-cleanup','seconds':round(time.perf_counter()-t,6),'exit_code':proc.returncode,'stderr':stderr,'remaining':[entries[-1]['name']]});conflict.unlink();race.rmdir()
 for args in [['--emit-source-options-fixtures'],['--source-options-only','extra'],['--emit-source-options-fixtures',out/'absent-directory']]:
  failed=invoke(binary,args,f'{config}-invalid-invocation');assert failed.returncode!=0 and not failed.stdout
 alias.unlink();receipts.append({'stage':f'{config}-inspection','directory':str(directory.relative_to(root)),'protocol':protocol,'files':snapshots,'metadata':metadata,'checks':['four closed independent native-produced archives','exact JSON protocol/order/versions/layout25','all2112 native BOOLEAN true values and exact retained query bytes checked','source references canonical/missing without execution','mode0444','repeat/existing/dangling refusal','late-conflict removes only owned outputs']});print(config,'options emission/metadata/refusal/cleanup passed',flush=True)
results=out/'emitter-results.json';results.write_text(json.dumps(receipts,indent=2,ensure_ascii=False)+'\n');mp=out/'manifest.json';m=json.loads(mp.read_text());m['emitter_results']={'path':str(results.relative_to(root)),'sha256':sha(results),'seconds':round(time.perf_counter()-start,6),'script':'build/bp1-options-native/check-emitters.py','script_sha256':sha(out/'check-emitters.py')};m['total_seconds']=round(m['seconds']+time.perf_counter()-start,6);m['supplementary_cli']={'path':'/opt/homebrew/bin/duckdb','sha256':sha('/opt/homebrew/bin/duckdb'),'scope':'Sequential read-only exact metadata inspection only; native C typed chunk comparison separately recorded.'};m['status']='passed';mp.write_text(json.dumps(m,indent=2)+'\n');print('emitter manifest',sha(mp),flush=True)
