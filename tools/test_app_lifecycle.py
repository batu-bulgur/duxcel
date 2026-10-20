#!/usr/bin/env python3
"""Hidden real-App session lifetime checks with a shared desktop lock."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
from workspace import FileLock, native_lock_path, source_metadata, utc_now
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-build',action='store_true')
    args=parser.parse_args()
    output=ROOT/'build/tests/results/runs'/f'{time.strftime("%Y%m%dT%H%M%S",time.gmtime())}-lifecycle-{os.getpid()}'
    output.mkdir(parents=True)
    summary={'started_utc':utc_now(),'status':'failed','results':[]}
    def run(argv,name,marker):
        start=time.monotonic()
        env=os.environ.copy(); env.pop('DUXCEL_TEST_VISIBLE',None); env['ASAN_OPTIONS']='halt_on_error=1'
        log=output/(name+'.txt')
        with log.open('w') as stream:
            result=subprocess.run(argv,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=300)
        text=log.read_text()
        if result.returncode or re.search(r'AddressSanitizer|runtime error:|FAIL:',text) or (marker and marker not in text):
            raise RuntimeError(f'{name} failed; see {log}')
        summary['results'].append({'command':argv,'elapsed_seconds':round(time.monotonic()-start,3),'log':str(log.relative_to(ROOT))})
    try:
        with FileLock(ROOT/'.state/test-runner.lock','worktree lifecycle checks'):
            if not args.no_build:
                run(['make','build/lifecycle/test-app','build/lifecycle/test-app-sanitize','build/tests/test_background-native-debug'],'build',None)
            before=source_metadata(ROOT); summary['source']=before
            binaries=[ROOT/'build/lifecycle'/name for name in ('test-app','test-app-sanitize')]
            hashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in binaries}
            summary['binaries']=hashes
            with FileLock(native_lock_path(),'App lifecycle native checks'):
                run(['./build/tests/test_background-native-debug'],'background','PASS: 477 background native checks')
                for binary in binaries:
                    run([str(binary)],binary.name,'PASS: 26 App lifecycle checks')
            if source_metadata(ROOT)!=before or any(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()!=h for p,h in hashes.items()):
                raise RuntimeError('Source or binary changed during acceptance')
            summary['status']='passed'
    except Exception as error:
        summary['error']=str(error)
    summary['finished_utc']=utc_now()
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({'status':summary['status'],'result':str(output),'error':summary.get('error')},indent=2))
    return summary['status']!='passed'
if __name__=='__main__': raise SystemExit(main())
