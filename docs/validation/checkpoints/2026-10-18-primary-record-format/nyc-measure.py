from pathlib import Path
import hashlib,json,platform,subprocess,time
root=Path('/Users/h2o/dev/duxcel')
folder=Path(__file__).resolve().parent
source=root/'tests/downloads/yellow_tripdata_2025-01.parquet'
sources=[root/p for p in ['src/common.c','src/transforms/analysis.c','src/transforms/number.c','src/transforms/record_sql.c','src/platform/platform.c','src/transforms/pivot.c','src/transforms/distinct.c','src/data/page.c','src/data/source_digest.c','src/data/database.c']]
files=sources+sorted((root/'src').rglob('*.h'))+[folder/'bench.c']
def hashes(): return {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
manifest={'schema':'duxcel.native-record-collection-perf','version':1,'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'root_status':subprocess.check_output(['git','status','--short'],cwd=root,text=True),'compiled_sources':[str(p) for p in sources],'inputs_sha256':hashes(),'compiler':subprocess.check_output(['clang','--version'],text=True),'host':platform.platform(),'scope':'Integrated SDK digest and database_collect_records command cost only; no UI/Worker/App latency, no comparison to unmatched business-query baseline. Existing pinned source read-only; private normal Database temp, memory limit lowered to384MiB; no business-value Page requested. Engine scan/sort/materialization and temporary allocation remain possible. First invocation follows full SDK file read; it is not an OS cold-cache measurement.'}
manifest['input']={'path':str(source),'bytes':source.stat().st_size,'pinned_sha256':'9af277e4c0d3f9deb30644da822981e1e7df6af58313170fd3aa8a474485488a'}
lib=Path('/opt/homebrew/lib/libduckdb.dylib').resolve()
manifest['duckdb_library']={'path':str(lib),'sha256':hashlib.sha256(lib.read_bytes()).hexdigest()}
argv=['clang','-I'+str(root/'src'),'-I/opt/homebrew/include','-O2','-g','-std=c99','-Wall','-Wextra','-Wpedantic','-Werror',*[str(p) for p in sources],str(folder/'bench.c'),'-o',str(folder/'bench'),'-L/opt/homebrew/lib','-lduckdb','-pthread','-lm']
manifest['compile_argv']=argv
start=time.monotonic(); built=subprocess.run(argv,capture_output=True,text=True); manifest['compile_seconds']=time.monotonic()-start
(folder/'build.log').write_text(built.stdout+built.stderr)
if built.returncode:
 print(built.stderr); raise SystemExit(built.returncode)
assert hashes()==manifest['inputs_sha256'],'compiled inputs changed during build'
manifest['binary_sha256']=hashlib.sha256((folder/'bench').read_bytes()).hexdigest()
start=time.monotonic(); run=subprocess.run([str(folder/'bench'),str(source)],capture_output=True,text=True,timeout=120)
manifest['run_seconds']=time.monotonic()-start; manifest['exit']=run.returncode
(folder/'run.jsonl').write_text(run.stdout); (folder/'stderr.log').write_text(run.stderr)
manifest['compiled_inputs_unchanged']=hashes()==manifest['inputs_sha256']
records=[json.loads(line) for line in run.stdout.splitlines() if line.strip()]
manifest['measurements']=records
(folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'manifest':str(folder/'manifest.json'),'exit':run.returncode,'compile_seconds':round(manifest['compile_seconds'],3),'run_seconds':round(manifest['run_seconds'],3),'measurements':records,'stderr':run.stderr},indent=2),flush=True)
assert run.returncode==0 and manifest['compiled_inputs_unchanged']
assert all(record['sha256']==manifest['input']['pinned_sha256'] for record in records if record['phase']=='digest')
assert len([record for record in records if record['phase']=='collect'])==6
