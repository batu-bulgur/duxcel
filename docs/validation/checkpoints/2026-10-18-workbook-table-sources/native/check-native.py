from pathlib import Path
import json,subprocess,time,os,hashlib
root=Path.cwd();out=root/'build/bp1-tables-native-null-corrected';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
prior=json.loads((root/'build/bp1-tables-native/manifest.json').read_text());files={n:sha(root/n) for n in prior['files']};results=[]
m={'scope':prior['scope']+' Corrected nullable factory allocations; immediate pre-Save model assertions and independent emitted metadata prove NULL/empty/literal. Prior de7 comparisons did not prove intended NULL fixture entries.','tested_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'shared_dependency':prior['shared_dependency'],'files':files,'toolchain_files':{n:sha(n) for n in prior['toolchain_files']},'results':results,'status':'running'}
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def save():m['seconds']=round(sum(x['seconds'] for x in results),6);(out/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for config in ['release','sanitize']:
 command=next(x['command'][:] for x in prior['results'] if x['stage']==config+'-build');command[-1]=str(out/('native-'+config))
 for label,cmd in [(config+'-build',command),(config+'-tables',[command[-1],'--tables-only'])]:
  t=time.monotonic();r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=180);log=out/(label+'.txt');log.write_text(r.stdout+r.stderr);results.append({'stage':label,'command':cmd,'seconds':round(time.monotonic()-t,6),'exit_code':r.returncode,'log':str(log.relative_to(root)),'sha256':sha(log)});save();print(label,results[-1]['seconds'],r.returncode,flush=True);assert r.returncode==0,r.stdout+r.stderr
  if label.endswith('-tables'):assert r.stdout.startswith('PASS: native Table sources ');print(r.stdout.strip(),flush=True)
for n,h in files.items():assert sha(root/n)==h,n
m['binary_sha256']={c:sha(out/('native-'+c)) for c in ['release','sanitize']};m['status']='native-passed-emission-pending';save();print('manifest',sha(out/'manifest.json'),flush=True)
