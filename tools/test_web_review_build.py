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
    check('review_queue.mjs' in (review/'host.mjs').read_text() and (review/'review_queue.mjs').is_file(),'Review queue transport is missing')
    for directory in (normal,review):
        check("import {installCanvasInput} from './canvas_input.mjs'" in (directory/'host.mjs').read_text() and (directory/'canvas_input.mjs').is_file(),'Canvas input platform policy is missing')
    check((normal/'canvas_input.mjs').read_bytes()==(review/'canvas_input.mjs').read_bytes(),'Normal/review canvas input policies differ')
    forbidden=[b'dx_review_',b'duxcel.native-review',b'duxcel.browser-review',b'dxBrowserWorkerTestDone',b'test_browser_worker.c',b'dxCodecReadTestDone',b'test_browser_codec_read.c',b'test_browser_codec_sorts.inc',b'dxCodecSortsMode',b'test_browser_codec_preferences.inc',b'dxCodecPreferencesMode',b'test_browser_codec_objects.inc',b'dxCodecObjectsMode',b'test_browser_codec_queries.inc',b'dxCodecQueriesMode',b'test_browser_codec_options.inc',b'dxCodecSourceOptionsMode',b'test_browser_codec_tables.inc',b'dxCodecTablesMode',b'test_browser_codec_pivot_marks.inc',b'dxCodecPivotMarksMode',b'test_browser_codec_filters.inc',b'dxCodecFiltersMode',b'dx_browser_codec_fixture_result',b'dx_browser_codec_result']
    wasm=(normal/'duxcel.wasm').read_bytes();markers={value.decode():value not in wasm for value in forbidden}
    check(all(markers.values()),'Shipping Wasm contains review/acceptance markers')
    for directory in (normal,review):
        check(b'dx_test_source_digest_checkpoint' not in (directory/'duxcel.wasm').read_bytes(),'Browser App contains an isolated native verification test checkpoint')
    check(not any((normal/name).exists() for name in ('review_host.mjs','host_review.mjs','review_queue.mjs','worker_test_host.mjs','codec_test_host.mjs','official_worker_probe.mjs','persistence_probe.mjs')),'Shipping contains review/acceptance host assets')
    for directory,build,module_exports in ((normal,shipping,exports),(review,development,review_exports)):
        check('tests/test_browser_codec_read.c' not in build['sources'],'Browser App includes the partial-codec acceptance driver')
        check(not any('dx_browser_codec_' in name for name in module_exports),'Browser App exports partial-codec acceptance callbacks')
        check(not any((directory/name).exists() for name in ('codec_test_host.mjs','codec-fixture-manifest.json')),'Browser App packages codec acceptance assets')
        check('dxCodec' not in (directory/'host.mjs').read_text(),'Browser App loader contains codec acceptance controls')
        check(not any(marker in (directory/'duxcel.wasm').read_bytes() for marker in (b'dxCodecReadTestDone',b'test_browser_codec_read.c',b'test_browser_codec_sorts.inc',b'test_browser_codec_preferences.inc',b'dxCodecPreferencesMode',b'test_browser_codec_objects.inc',b'dxCodecObjectsMode',b'test_browser_codec_queries.inc',b'dxCodecQueriesMode',b'test_browser_codec_options.inc',b'dxCodecSourceOptionsMode',b'test_browser_codec_tables.inc',b'dxCodecTablesMode',b'test_browser_codec_pivot_marks.inc',b'dxCodecPivotMarksMode',b'test_browser_codec_filters.inc',b'dxCodecFiltersMode')),'Browser App contains the codec acceptance workflow')
    for directory in (normal,review):
        for name in ('canvas_surface.mjs','canvas_wheel.mjs','browser_platform.mjs','clipboard.mjs','download.mjs'):
            check((directory/name).is_file(),'Browser device adapter asset is missing: '+name)
        check(not any((directory/name).exists() for name in ('download.test.mjs','clipboard.test.mjs','export_probe.mjs','export_probe.test.mjs')),'Browser artifact includes delivery tests or export acceptance probes')
    check('/__review/' not in (normal/'host.mjs').read_text(),'Shipping loader includes review queue routes')
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
