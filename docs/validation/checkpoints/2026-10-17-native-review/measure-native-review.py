#!/usr/bin/env python3
"""Run from repo root after builds finish. Hidden optimized observations only."""
import hashlib,json,os,re,subprocess,sys,time
from pathlib import Path
ROOT=Path.cwd(); sys.path.insert(0,str(ROOT/'tools'))
from test import FileLock,native_lock_path,source_metadata,utc_now
OUT=ROOT/'tests/results/runs'/f'{time.strftime("%Y%m%dT%H%M%S",time.gmtime())}-review-performance-{os.getpid()}'
OUT.mkdir(parents=True)
binaries=['duxcel','build/review/duxcel-review']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
result={'started_utc':utc_now(),'status':'failed','scope':'Three paired hidden optimized 360-frame idle-source observations; one separate active 26-stage capture workflow. Active and idle workloads differ; peak RSS includes graphics/engine/allocator startup, not only review storage. Frame timings are hidden renderer/controller measurements, not physical input-to-display latency.','samples':[]}
try:
    with FileLock(ROOT/'tests/results/.runner.lock','review performance'),FileLock(native_lock_path(),'review performance'):
        source=source_metadata(ROOT); hashes={p:sha(ROOT/p) for p in binaries}
        result.update(source=source,binaries=hashes)
        env=os.environ.copy(); env.pop('DUXCEL_TEST_VISIBLE',None)
        env['DUXCEL_REVIEW_OUTBOX']=str(OUT/'active-outbox')
        for i,(mode,binary) in enumerate([('shipping',binaries[0]),('inactive',binaries[1])]*3+[('active',binaries[1])]):
            argv=['/usr/bin/time','-l',str(ROOT/binary),'tests/fixtures/null-text.csv','--frames','900' if mode=='active' else '360']
            if mode=='active': argv.append('--review-smoke')
            path=OUT/f'{i}-{mode}.txt'; start=time.monotonic()
            with path.open('w') as stream:
                run=subprocess.run(argv,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=45)
            log=path.read_text()
            if run.returncode or 'FAIL:' in log or 'Native test window: hidden, unfocused' not in log: raise RuntimeError(f'failed {path}')
            if mode=='active' and 'Review smoke completed [0,26)' not in log: raise RuntimeError('Active workflow incomplete')
            frame=re.search(r'Duxcel frames=(\d+) mean=([\d.]+) ms p95=([\d.]+) ms max=([\d.]+) ms',log)
            draw=re.search(r'Draw submission CPU p95=([\d.]+) ms',log)
            rss=re.search(r'(\d+)\s+maximum resident set size',log)
            jobs=re.search(r'Worker jobs=(\d+) SQL executions=(\d+) count queries=(\d+)',log)
            if not all((frame,draw,rss,jobs)): raise RuntimeError('Missing timing/memory/counter evidence')
            result['samples'].append({'mode':mode,'command':argv,'elapsed_seconds':round(time.monotonic()-start,3),'frames':int(frame[1]),'frame_mean_ms':float(frame[2]),'frame_p95_ms':float(frame[3]),'frame_max_ms':float(frame[4]),'draw_cpu_p95_ms':float(draw[1]),'peak_rss_bytes':int(rss[1]),'jobs':int(jobs[1]),'sql':int(jobs[2]),'counts':int(jobs[3]),'log':str(path.relative_to(ROOT))})
        if source_metadata(ROOT)!=source or any(sha(ROOT/p)!=h for p,h in hashes.items()): raise RuntimeError('Source/binary changed')
        result['status']='passed'
except Exception as error: result['error']=str(error)
result['finished_utc']=utc_now()
(OUT/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'status':result['status'],'result':str(OUT),'error':result.get('error')},indent=2))
raise SystemExit(result['status']!='passed')
