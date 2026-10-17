#!/usr/bin/env python3
"""Verify review isolation in the actual normal/review browser artifacts."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inspect(kind):
    folder=ROOT/'build/web'/kind;directory=Path(json.loads((folder/'latest.json').read_text())['directory'])
    manifest=json.loads((directory/'build.json').read_text())
    for name,digest in manifest['artifacts'].items():
        if sha(directory/name)!=digest:raise RuntimeError('Artifact changed: '+str(directory/name))
    script="import fs from 'node:fs';console.log(JSON.stringify(WebAssembly.Module.exports(new WebAssembly.Module(fs.readFileSync(process.argv[1]))).map(value=>value.name)))"
    exports=json.loads(subprocess.check_output(['node','--input-type=module','-e',script,str(directory/'duxcel.wasm')],text=True))
    return directory,manifest,exports
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--no-build',action='store_true');args=parser.parse_args()
    if not args.no_build:
        for mode in ([],['--review']):subprocess.run(['python3','tools/build_web.py',*mode],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
    normal,shipping,exports=inspect('app');review,development,review_exports=inspect('review')
    failures=[]
    def check(ok,message):
        if not ok:failures.append(message)
    callback='dx_browser_review_result'
    check(not any(callback in name for name in exports),'Shipping Wasm exports a review callback')
    check(any(callback in name for name in review_exports),'Review Wasm lacks its callback')
    check(not any(source.startswith('src/dev/review_') for source in shipping['sources']),'Shipping source inventory includes review code')
    expected={'src/dev/review_registry.c','src/dev/review_input.c','src/dev/review_capture.c','src/dev/review_browser.c'}
    check(expected.issubset(set(development['sources'])),'Review source inventory is incomplete')
    check('src/dev/review_store.c' not in development['sources'],'Browser links native durable storage')
    check('-DDUXCEL_DEV_REVIEW' not in shipping['flags'] and '-DDUXCEL_DEV_REVIEW' in development['flags'],'Review compile configurations overlap')
    check('dxReview' not in (normal/'host.mjs').read_text(),'Shipping loader includes the review bridge')
    check('createReviewHost' in (review/'host.mjs').read_text() and (review/'review_host.mjs').is_file(),'Review platform host is missing')
    forbidden=[b'dx_review_',b'duxcel.native-review',b'duxcel.browser-review',b'dxBrowserWorkerTestDone',b'test_browser_worker.c']
    wasm=(normal/'duxcel.wasm').read_bytes();markers={value.decode():value not in wasm for value in forbidden}
    check(all(markers.values()),'Shipping Wasm contains review/acceptance markers')
    check(not any((normal/name).exists() for name in ('review_host.mjs','host_review.mjs','worker_test_host.mjs','official_worker_probe.mjs')),'Shipping contains review/acceptance host assets')
    native={}
    for name in ('duxcel','duxcel-debug'):
        binary=ROOT/name;symbols=subprocess.check_output(['nm','-g',str(binary)],text=True)
        added=[line for line in symbols.splitlines() if 'dx_review_' in line or 'dx_browser_' in line]
        check(not added,'Native shipping contains a browser/review bridge: '+name)
        native[name]={'sha256':sha(binary),'unexpected_symbols':added}
    record={'status':'failed' if failures else 'passed','normal_directory':str(normal),'review_directory':str(review),'normal_wasm_sha256':sha(normal/'duxcel.wasm'),'review_wasm_sha256':sha(review/'duxcel.wasm'),'normal_exports':exports,'review_exports':review_exports,'shipping_marker_absence':markers,'native':native,'failures':failures}
    output=ROOT/'build/web/review-absence.json';output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps({'status':record['status'],'result':str(output),'failures':failures}))
    return bool(failures)
if __name__=='__main__':raise SystemExit(main())
