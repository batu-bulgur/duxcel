from pathlib import Path
import json,subprocess,time,os,hashlib
root=Path.cwd();out=root/'build/bp1-pivot-marks-native';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
prior=json.loads(Path('/Users/h2o/.codex/worktrees/duxcel-bp1-tables-native/duxcel/build/bp1-tables-native-null-corrected/manifest.json').read_text())
files={n:sha(root/n) for n in prior['files']};files['tests/test_workbook_codec_pivot_marks_native.inc']=sha(root/'tests/test_workbook_codec_pivot_marks_native.inc');records=[]
m={'scope':'Native partial codec Pivot/Marks complete definitions only, not Worker/App Open. Three small mixed4object fixtures plusone2sheet34object fixture,288Pivot/320Marks; one result/family split<=256. Original source SQL/files never executed. Reader staging+explicit input bound2MiB; oracle/file images/DuckDB allocator/process peak excluded; no LSan. Full model row caps remain separate pure/transport scope.','dependency':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'files':files,'toolchain_files':{n:sha(n) for n in prior['toolchain_files']},'results':records,'status':'running'}
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def save():m['seconds']=round(sum(x['seconds'] for x in records),6);(out/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for config in ['release','sanitize']:
 command=next(x['command'][:] for x in prior['results'] if x['stage']==config+'-build');command[-1]=str(out/('native-'+config))
 for label,cmd,marker in [(config+'-build',command,None),(config+'-pivot-marks',[command[-1],'--pivot-marks-only'],'PASS: native Pivot Marks '),(config+'-default',[command[-1]],'PASS: native workbook header 1611 checks;')]:
  t=time.monotonic();r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=180);log=out/(label+'.txt');log.write_text(r.stdout+r.stderr);records.append({'stage':label,'command':cmd,'seconds':round(time.monotonic()-t,6),'exit_code':r.returncode,'log':str(log.relative_to(root)),'sha256':sha(log)});save();print(label,records[-1]['seconds'],r.returncode,flush=True);assert not r.returncode,r.stdout+r.stderr
  if marker:assert r.stdout.startswith(marker);print(r.stdout.strip(),flush=True)
for n,h in files.items():assert sha(root/n)==h,n
m['binary_sha256']={c:sha(out/('native-'+c)) for c in ['release','sanitize']};m['status']='native-passed-emission-pending';save();print('manifest',sha(out/'manifest.json'),flush=True)
