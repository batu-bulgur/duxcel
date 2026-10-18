from pathlib import Path
import hashlib,json,subprocess,time,shlex
root=Path.cwd();out=root/'build/bp1-source-bindings-native';closure={};receipts=[]
for config,log in [('release','exact-build.txt'),('sanitize','build-sanitize.txt')]:
 line=next(x for x in (out/log).read_text().splitlines() if x.startswith('clang '));full=shlex.split(line);end=full.index('-o');cmd=[full[0],'-MM',*full[1:end]];start=time.monotonic();r=subprocess.run(cmd,cwd=root,capture_output=True,text=True,check=True,timeout=30);p=out/f'{config}-dependencies.d';p.write_text(r.stdout);(out/f'{config}-dependencies.stderr').write_text(r.stderr);deps=set()
 for row in r.stdout.replace('\\\n',' ').splitlines():
  _,tail=row.split(':',1);deps.update(str((root/p).resolve()) for p in shlex.split(tail))
 project={str(Path(p).relative_to(root)):hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in sorted(deps) if Path(p).is_relative_to(root)};closure[config]=project;receipts.append(dict(config=config,exact_build_command=full,source_build_log=log,dependency_command=cmd,seconds=time.monotonic()-start,raw=str(p.relative_to(root)),raw_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),stderr=r.stderr,project_files=len(project)))
assert closure['release']==closure['sanitize']
commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
for path,sha in closure['release'].items():assert hashlib.sha256(subprocess.check_output(['git','show',commit+':'+path])).hexdigest()==sha,path
installed={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in ['/opt/homebrew/include/duckdb.h','/opt/homebrew/lib/libduckdb.dylib']}
m=dict(scope='Exact clang -MM project preprocessor dependencies for tested native helper release/O1 sanitizer recipes plus explicit linked DuckDB SDK/header. Materialized result/allocator/RSS/system headers excluded; no browser source-inventory equivalence.',tested_commit=commit,source_status=subprocess.check_output(['git','status','--short','--untracked-files=all'],text=True),project_files=closure['release'],project_count=len(closure['release']),installed=installed,receipts=receipts)
p=out/'preprocessor-closure.json';p.write_text(json.dumps(m,indent=2)+'\n');print(len(closure['release']),hashlib.sha256(p.read_bytes()).hexdigest(),flush=True)
# Narrow wrapper witness: recover the exact old source by removing only new include/dispatch/usage.
path='tests/test_workbook_codec_native.c';old=subprocess.check_output(['git','show','2bb5bf4:'+path]);s=Path(path).read_bytes()
s=s.replace(b'#include "test_workbook_codec_source_bindings_native.inc"\n',b'').replace(b'    if(argc==3&&!strcmp(argv[1],"--emit-source-bindings-fixtures"))return emit_source_bindings(argv[2]);\n',b'').replace(b'    if(argc==2&&!strcmp(argv[1],"--source-bindings-only"))return source_bindings_only();\n',b'').replace(b' | --source-bindings-only | --emit-source-bindings-fixtures DIR',b'')
assert s==old
prefix={}
for f in sorted(root.glob('tests/test_workbook_codec_*_native.inc')):
 rel=str(f.relative_to(root))
 if 'source_bindings_native.inc' in rel:continue
 assert f.read_bytes()==subprocess.check_output(['git','show','2bb5bf4:'+rel]);prefix[rel]=hashlib.sha256(f.read_bytes()).hexdigest()
(out/'unchanged-native-prefix.json').write_text(json.dumps(dict(base='2bb5bf4',tested=commit,old_wrapper_sha256=hashlib.sha256(old).hexdigest(),recovered_wrapper_sha256=hashlib.sha256(s).hexdigest(),unchanged_includes=prefix,old_result_feed_body_unchanged=True),indent=2)+'\n')
