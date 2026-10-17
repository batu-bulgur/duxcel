#!/usr/bin/env python3
"""Focused hidden native reporting acceptance with immutable build evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

from test import FileLock, native_lock_path, source_metadata, utc_now
from native_review import ReviewStore, default_project

ROOT = Path(__file__).resolve().parents[1]
REVIEW_STAGES = 230  # 176 authoring/dialog +24 worksheet +12 query +6 relation +12 sort
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def run(argv, path, env=None, timeout=180):
    start=time.monotonic()
    with path.open('w') as stream:
        result=subprocess.run(argv,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=timeout)
    text=path.read_text()
    if result.returncode or re.search(r'AddressSanitizer|runtime error:|FAIL:|unfinished',text):
        raise RuntimeError(f'{argv[0]} failed ({result.returncode}); see {path}')
    return {'command':argv,'elapsed_seconds':round(time.monotonic()-start,3),'log':str(path.relative_to(ROOT))},text

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',choices=['release','sanitize','both'],default='both')
    parser.add_argument('--no-build',action='store_true')
    args=parser.parse_args()
    result=ROOT/'tests/results/runs'/f'{time.strftime("%Y%m%dT%H%M%S",time.gmtime())}-review-{os.getpid()}'
    result.mkdir(parents=True)
    (ROOT/'tests/results/native-review').mkdir(parents=True,exist_ok=True)
    summary={'started_utc':utc_now(),'status':'failed','configs':{},'scope':'hidden renderer/controller acceptance; OS responder/IME/physical gestures remain manual'}
    try:
        with FileLock(ROOT/'tests/results/.runner.lock','worktree tests'):
            configs=['release','sanitize'] if args.config=='both' else [args.config]
            if not args.no_build:
                targets=[('review' if c=='release' else 'review-debug') for c in configs]+['tests/test_background-native-debug']
                summary['build'],_=run(['make',*targets],result/'build.log',timeout=300)
            before=source_metadata(ROOT); summary['source']=before
            with FileLock(native_lock_path(),'native review acceptance'):
                summary['background'],log=run(['./tests/test_background-native-debug'],result/'background.log')
                if 'PASS: 477 background native checks' not in log: raise RuntimeError('Background probe did not finish')
                for config in configs:
                    folder='review' if config=='release' else 'review-sanitize'
                    binary=ROOT/'build'/folder/'duxcel-review'
                    binary_hash=sha(binary)
                    if Path(str(binary)+'.sha256').read_text().strip()!=binary_hash: raise RuntimeError('Binary sidecar mismatch')
                    outbox=result/config/'outbox'; outbox.parent.mkdir()
                    env=os.environ.copy(); env.pop('DUXCEL_TEST_VISIBLE',None)
                    env['DUXCEL_REVIEW_OUTBOX']=str(outbox); env['ASAN_OPTIONS']='halt_on_error=1'
                    record,log=run([str(binary),'tests/fixtures/null-text.csv','--review-smoke','--frames','900'],result/f'{config}.log',env,90)
                    if f'Review smoke completed [0,{REVIEW_STAGES})' not in log or 'Native test window: hidden, unfocused' not in log: raise RuntimeError('Native review did not complete the current stage inventory hidden')
                    if sha(binary)!=binary_hash: raise RuntimeError('Binary changed during native acceptance')
                    store=ReviewStore(outbox,default_project())
                    listing=store.list()
                    if listing['rejected'] or len(listing['reports'])!=3: raise RuntimeError('Expected exactly three valid native reports')
                    reports=[]
                    for item in listing['reports']:
                        report_id=item['report_id']; report=store.read(report_id); m=report['manifest']
                        if m['frame']['binary_sha256']!=binary_hash or m['target']['frame_id']!=m['frame']['frame_id']: raise RuntimeError('Image/target/build provenance mismatch')
                        if m['target']['widget']['id']=='file.scrollbar.thumb' and (m['note']!='Short native composer.\nNULL, empty text, literal NULL.\nTail' or m['frame']['ui_scale']!=1.5): raise RuntimeError('Short composer lost edited note or captured scale')
                        payload_hash=sha(outbox/report_id/'manifest.json')
                        store.update(report_id,'ack','native-test','Retrieved real native frame')
                        store.update(report_id,'reply','native-test','Target, scale and binary verified; design acceptance remains pending')
                        store.update(report_id,'status','native-test',implementation='ready-for-review',verification='passed')
                        reread=store.read(report_id)
                        if not reread['lifecycle']['acknowledged'] or reread['lifecycle']['design']!='pending' or sha(outbox/report_id/'manifest.json')!=payload_hash: raise RuntimeError('Lifecycle changed immutable capture/design status')
                        reports.append({'report_id':report_id,'target':m['target']['widget']['id'],'frame_id':m['frame']['frame_id'],'ui_scale':m['frame']['ui_scale'],'image':report['image'],'capture_sha256':payload_hash})
                    reports.sort(key=lambda report: int(report['frame_id']))
                    if [r['target'] for r in reports]!=['filter.search','filter.condition.option.equal','file.scrollbar.thumb']: raise RuntimeError('Wrong native controls captured')
                    record.update(binary_sha256=binary_hash,build_header_sha256=sha(ROOT/'build'/folder/'review_build_info.h'),completed_stages=REVIEW_STAGES,reports=reports)
                    summary['configs'][config]=record
            if source_metadata(ROOT)!=before: raise RuntimeError('Source changed during acceptance')
            summary['status']='passed'
    except Exception as error:
        summary['error']=str(error)
    summary['finished_utc']=utc_now()
    (result/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({'status':summary['status'],'result':str(result),'error':summary.get('error')},indent=2))
    return summary['status']!='passed'
if __name__=='__main__': raise SystemExit(main())
