#!/usr/bin/env python3
"""Run the actual shared C browser UI in an owned, hidden browser session."""
import argparse, datetime, functools, hashlib, http.server, json, os, re, shutil, signal, socketserver, subprocess, threading, time
from pathlib import Path
from test import FileLock, native_lock_path, source_metadata
ROOT=Path(__file__).resolve().parents[1]
class LoopbackServer(http.server.ThreadingHTTPServer):
    def server_bind(self):
        # HTTPServer's getfqdn() spends 35 s in reverse DNS on this host.
        # This acceptance server always binds an explicit loopback address.
        socketserver.TCPServer.server_bind(self)
        self.server_name='127.0.0.1';self.server_port=self.server_address[1]
class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args): pass
    def do_GET(self):
        # Test-only probe is served by this owned harness, never packaged into App.
        probe={'/official_worker_probe.mjs':ROOT/'web/official_worker_probe.mjs','/persistence_probe.mjs':ROOT/'web/persistence_probe.mjs'}
        if self.path in probe:
            content=probe[self.path].read_bytes()
            self.send_response(200);self.send_header('Content-Type','text/javascript');self.send_header('Content-Length',str(len(content)));self.end_headers();self.wfile.write(content)
        elif self.path=='/native-fixture.dxls' and getattr(self.server,'persistence_fixture',None):
            content=self.server.persistence_fixture.read_bytes()
            self.send_response(200);self.send_header('Content-Type','application/octet-stream');self.send_header('Content-Length',str(len(content)));self.end_headers();self.wfile.write(content)
        else: super().do_GET()

def parse_options(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-build', action='store_true')
    parser.add_argument('--worker-probe', action='store_true')
    parser.add_argument('--source',type=Path)
    parser.add_argument('--filters',action='store_true',help='Exercise the real transactions.csv value and date dropdowns')
    parser.add_argument('--busy',action='store_true',help='Require real UI input/rendering before the imported source job completes')
    parser.add_argument('--worker-driver',action='store_true')
    parser.add_argument('--review',action='store_true')
    parser.add_argument('--access-only',action='store_true',help='Focused blank-workbook access keys and File/document flow; requires --review')
    parser.add_argument('--zoom-only',action='store_true',help='Focused real C grid zoom/header geometry and pointer checks; requires --review, optional transactions.csv')
    parser.add_argument('--review-controls-only',action='store_true',help='Focused real C contextual targets, editable width/Move and Close guard; requires --review and transactions.csv')
    parser.add_argument('--analysis-only',action='store_true',help='Real C same-sheet Rows/Values matrix and grand totals; requires --review and transactions.csv')
    parser.add_argument('--review-query-only',action='store_true',help='Primary source-query review ownership while a pivot is selected; requires --review and transactions.csv')
    parser.add_argument('--query-apply',action='store_true',help='Additionally validate/apply the real edited source through the official Worker; requires --review-query-only')
    parser.add_argument('--review-outbox',type=Path,help='Private shared outbox for an integrated native/browser review gate')
    parser.add_argument('--review-offline',action='store_true',help='Commit a report offline, reload real IndexedDB, then automatically upload')
    parser.add_argument('--dpr',type=int,choices=(1,2),default=1)
    parser.add_argument('--canvas-transform',action='store_true',help='Exercise review picking through an inset canvas CSS transform')
    parser.add_argument('--alt-stress',action='store_true',help='Repeat trusted ribbon Alt shortcuts in the owned platform gate')
    parser.add_argument('--platform',action='store_true',help='Real C DPI/pixel pan/zoom gate (review source fixture)')
    parser.add_argument('--persistence',action='store_true',help='BP0 real native/Worker file compatibility (test-only, no Save UI claim)')
    options=parser.parse_args(argv)
    if options.query_apply and not options.review_query_only:
        parser.error('--query-apply requires --review-query-only')
    if options.review_query_only:
        if not options.review or not options.source or options.source.name!='transactions.csv':
            parser.error('--review-query-only requires --review and --source tests/fixtures/transactions.csv')
        if any((options.access_only,options.zoom_only,options.review_controls_only,options.analysis_only,
                options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,
                options.alt_stress,options.persistence,options.review_offline,options.review_outbox)):
            parser.error('--review-query-only is a separate source-query gate without other profiles or report-upload options')
    if options.analysis_only:
        if not options.review or not options.source or options.source.name!='transactions.csv':
            parser.error('--analysis-only requires --review and --source tests/fixtures/transactions.csv')
        if any((options.review_query_only,options.access_only,options.zoom_only,options.review_controls_only,options.filters,
                options.busy,options.worker_driver,options.worker_probe,options.platform,
                options.alt_stress,options.persistence,options.review_offline,options.review_outbox)):
            parser.error('--analysis-only is a separate Rows/Values gate without other focused profiles or report-upload options')
    if options.review_controls_only:
        if not options.review or not options.source or options.source.name!='transactions.csv':
            parser.error('--review-controls-only requires --review and --source tests/fixtures/transactions.csv')
        if any((options.review_query_only,options.analysis_only,options.access_only,options.zoom_only,options.filters,options.busy,options.worker_driver,
                options.worker_probe,options.platform,options.alt_stress,options.persistence,
                options.review_offline,options.review_outbox)):
            parser.error('--review-controls-only is a separate contextual review gate without access/zoom/engine/platform/report-upload options')
    if options.zoom_only:
        if not options.review:
            parser.error('--zoom-only requires --review')
        if any((options.review_query_only,options.analysis_only,options.access_only,options.review_controls_only,options.filters,options.busy,options.worker_driver,options.worker_probe,
                options.platform,options.alt_stress,options.persistence,options.review_offline,
                options.review_outbox)):
            parser.error('--zoom-only is a separate grid gate without access/engine/platform/report-upload options')
        if options.source and options.source.name!='transactions.csv':
            parser.error('--zoom-only source must be tests/fixtures/transactions.csv')
    if options.access_only:
        if not options.review:
            parser.error('--access-only requires --review')
        if any((options.review_query_only,options.analysis_only,options.source,options.review_controls_only,options.filters,options.busy,options.worker_driver,options.worker_probe,
                options.platform,options.alt_stress,options.persistence,options.review_offline,
                options.review_outbox)):
            parser.error('--access-only is a separate blank-workbook gate without source/engine/platform/report-upload options')
    if options.filters and (not options.source or options.source.name!='transactions.csv'):
        parser.error('--filters requires --source tests/fixtures/transactions.csv')
    if options.busy and not options.source:
        parser.error('--busy requires a real --source')
    if options.review and not options.access_only and not options.zoom_only and (options.worker_driver or not options.source or options.source.name!='transactions.csv'):
        parser.error('--review requires --source tests/fixtures/transactions.csv and cannot use --worker-driver')
    if options.canvas_transform and not options.review:
        parser.error('--canvas-transform requires --review')
    if options.platform and not options.review:
        parser.error('--platform requires --review')
    if options.alt_stress and not options.platform:
        parser.error('--alt-stress requires --platform')
    if options.persistence and (options.review or options.worker_driver or options.source):
        parser.error('--persistence is a separate normal-artifact file proof without source/review/worker-driver')
    if options.review_outbox and not options.review:
        parser.error('--review-outbox requires --review')
    if options.review_offline and not options.review:
        parser.error('--review-offline requires --review')
    return options

def main():
    overall=time.monotonic();phases={}
    options=parse_options()
    if not options.no_build:
        subprocess.run(['python3','tools/build_web.py']+(['--worker-driver'] if options.worker_driver else ['--review'] if options.review else []), cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        if options.persistence:subprocess.run(['make','build/web-tests/test-persistence','build/web-tests/test-persistence-sanitize'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
    phases['build_seconds']=round(time.monotonic()-overall,3);phase=time.monotonic()
    latest=json.loads((ROOT/('build/web/worker-test/latest.json' if options.worker_driver else 'build/web/review/latest.json' if options.review else 'build/web/app/latest.json')).read_text())
    artifact=Path(latest['directory']); manifest=json.loads((artifact/'build.json').read_text())
    for name,digest in manifest['artifacts'].items():
        if hashlib.sha256((artifact/name).read_bytes()).hexdigest()!=digest:
            raise RuntimeError('Browser artifact changed: '+name)
    phases['artifact_validation_seconds']=round(time.monotonic()-phase,3);phase=time.monotonic()
    before=source_metadata(ROOT)
    phases['source_metadata_seconds']=round(time.monotonic()-phase,3);phase=time.monotonic()
    output=ROOT/'tests/results/runs'/f'{datetime.datetime.now(datetime.timezone.utc):%Y%m%dT%H%M%S}-browser-{os.getpid()}'
    output.mkdir(parents=True,exist_ok=True)
    with FileLock(native_lock_path(),'hidden shared C browser acceptance'):
        persistence_binary=ROOT/'build/web-tests/test-persistence'
        persistence_sanitizer=ROOT/'build/web-tests/test-persistence-sanitize'
        persistence_hashes={}
        if options.persistence:
            for executable in (persistence_binary,persistence_sanitizer):
                persistence_hashes[str(executable)]=hashlib.sha256(executable.read_bytes()).hexdigest()
            fixture=output/'native-fixture.dxls'
            created=subprocess.run([str(persistence_binary),'create',str(fixture)],cwd=ROOT,text=True,capture_output=True,check=True,timeout=30)
            (output/'persistence-native-create.txt').write_text(created.stdout+created.stderr)
            fixture_hash=hashlib.sha256(fixture.read_bytes()).hexdigest()
        if options.review:
            from review_service import ReviewServer
            from review_queue import ReviewQueue
            from native_review import default_project
            queue=ReviewQueue(options.review_outbox or output/'queue-outbox',default_project())
            server=ReviewServer(('127.0.0.1',0),queue,artifact)
        else:
            server=LoopbackServer(('127.0.0.1',0),functools.partial(Handler,directory=str(artifact)))
            if options.persistence:server.persistence_fixture=fixture
        phases['server_setup_seconds']=round(time.monotonic()-phase,3)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        begin=time.monotonic()
        try:
            command=['node','tools/test_web_app.mjs',f'http://127.0.0.1:{server.server_port}/',str(output)]
            if options.worker_driver:command.append('--worker-driver')
            if options.worker_probe: command.append('--worker-probe')
            if options.source: command.extend(['--source',str(options.source.resolve(strict=True))])
            if options.filters: command.append('--filters')
            if options.busy: command.append('--busy')
            if options.review: command.append('--review')
            if options.access_only: command.append('--access-only')
            if options.zoom_only: command.append('--zoom-only')
            if options.review_controls_only: command.append('--review-controls-only')
            if options.analysis_only: command.append('--analysis-only')
            if options.review_query_only: command.append('--review-query-only')
            if options.query_apply: command.append('--query-apply')
            if options.review_offline: command.append('--review-offline')
            command.extend(['--dpr',str(options.dpr)])
            if options.canvas_transform:command.append('--canvas-transform')
            if options.platform:command.append('--platform')
            if options.alt_stress:command.append('--alt-stress')
            if options.persistence:command.append('--persistence')
            process=subprocess.Popen(command,cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
            try:log,_=process.communicate(timeout=100)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid,signal.SIGTERM)
                try:log,_=process.communicate(timeout=3)
                except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);log,_=process.communicate()
            (output/'driver.txt').write_text(log)
            reader_error=None
            if options.persistence and process.returncode==0:
                try:
                    for executable in (persistence_binary,persistence_sanitizer):
                        verified=subprocess.run([str(executable),'verify',str(fixture),str(output/'bp0-browser-export.dxls')],cwd=ROOT,text=True,capture_output=True,timeout=30)
                        (output/('persistence-native-verify-sanitize.txt' if executable==persistence_sanitizer else 'persistence-native-verify.txt')).write_text(verified.stdout+verified.stderr)
                        if verified.returncode or re.search(r'ERROR: AddressSanitizer|runtime error:',verified.stderr):raise RuntimeError('Native workbook readback failed: '+str(executable))
                    if hashlib.sha256(fixture.read_bytes()).hexdigest()!=fixture_hash:raise RuntimeError('Browser/native proof mutated its original fixture')
                    if any(hashlib.sha256(Path(name).read_bytes()).hexdigest()!=digest for name,digest in persistence_hashes.items()):raise RuntimeError('Native proof helper changed during acceptance')
                except Exception as error:reader_error=str(error)
            if options.review and not options.access_only and not options.zoom_only and not options.review_controls_only and not options.analysis_only and not options.review_query_only and process.returncode==0:
                try:
                    from native_review import ReviewStore,default_project
                    report=json.loads((output/'report-manifest.json').read_text());report_id=report['report_id']
                    if not re.fullmatch(r'[A-Za-z0-9_-]{1,95}',report_id):raise RuntimeError('Invalid report archive identity')
                    extracted=output/'extracted';folder=extracted/report_id;folder.mkdir(parents=True)
                    shutil.copy2(output/'report-manifest.json',folder/'manifest.json');shutil.copy2(output/'report-frame.png',folder/'frame.png')
                    retrieved=ReviewStore(extracted,default_project()).read(report_id)
                    if retrieved['manifest']['target']['widget']['id']!='filter.search':raise RuntimeError('Reader retrieved the wrong C target')
                    (output/'reader.json').write_text(json.dumps({'status':'passed','report_id':report_id,'target':retrieved['manifest']['target'],'image':retrieved['image'],'extracted':str(extracted)},indent=2)+'\n')
                except Exception as error:reader_error=str(error)
            after=source_metadata(ROOT)
            unchanged=after['source_sha256']==before['source_sha256'] and after['commit']==before['commit']
            artifacts_unchanged=all(hashlib.sha256((artifact/name).read_bytes()).hexdigest()==digest for name,digest in manifest['artifacts'].items())
            record={'status':'passed' if process.returncode==0 and unchanged and artifacts_unchanged and not reader_error else 'failed','source':before,'artifact':manifest,'reader_error':reader_error,'native_persistence_helpers':persistence_hashes,
                    'command':command,'exit_code':process.returncode,'source_unchanged':unchanged,'artifacts_unchanged':artifacts_unchanged,
                    'browser_seconds':round(time.monotonic()-begin,3),'elapsed_seconds':round(time.monotonic()-overall,3),'setup':phases,'desktop_visibility':'owned headless browser, simulated focus; no user profile'}
            record['profile']='review-query-apply' if options.query_apply else 'review-query-only' if options.review_query_only else 'analysis-only' if options.analysis_only else 'review-controls-only' if options.review_controls_only else 'zoom-only' if options.zoom_only else 'access-only' if options.access_only else 'standard'
            if options.review_query_only:
                record['scope']='Actual primary source-query editor reached through selected-pivot Transformations, frozen target/draft/owner retention with no report-triggered SQL; '+('explicit real Worker validation and applied source publication.' if options.query_apply else 'actual editor Cancel preserves the displayed source; no Apply/engine-validation acceptance.')
            if options.analysis_only:
                record['scope']='Shared C same-sheet empty pivot, Rows City/SUM Amount assignment, real official Worker matrix/grand totals, return to source and sparse-cell retention; no Columns pivot, broad analytics or report-upload acceptance.'
            if options.review_controls_only:
                record['scope']='Shared C source/manual contextual targets, editable width/Move drafts and guarded Close, modal ownership and frozen-state cancellation; real source import, no analytical-object or report-upload acceptance.'
            if options.zoom_only:
                record['scope']='Shared C blank/source grid zoom, fixed header geometry and rendered label bounds, pointer selection/resize/filter and independent UI scale; no report upload or physical gesture acceptance.'
            if options.access_only:
                record['scope']='Shared C access keys and File/document interactions on a blank workbook; no Worker query, clipboard delivery or report-upload acceptance.'
            (output/'summary.json').write_text(json.dumps(record,indent=2)+'\n')
            print(json.dumps({'status':record['status'],'result':str(output),'elapsed_seconds':record['elapsed_seconds']}))
            return 0 if record['status']=='passed' else 1
        finally:
            server.shutdown();server.server_close();thread.join()
if __name__=='__main__':
    raise SystemExit(main())
