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
        if self.path=='/official_worker_probe.mjs':
            content=(ROOT/'web/official_worker_probe.mjs').read_bytes()
            self.send_response(200);self.send_header('Content-Type','text/javascript');self.send_header('Content-Length',str(len(content)));self.end_headers();self.wfile.write(content)
        else: super().do_GET()

def main():
    overall=time.monotonic();phases={}
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-build', action='store_true')
    parser.add_argument('--worker-probe', action='store_true')
    parser.add_argument('--source',type=Path)
    parser.add_argument('--filters',action='store_true',help='Exercise the real transactions.csv value and date dropdowns')
    parser.add_argument('--busy',action='store_true',help='Require real UI input/rendering before the imported source job completes')
    parser.add_argument('--worker-driver',action='store_true')
    parser.add_argument('--review',action='store_true')
    parser.add_argument('--review-outbox',type=Path,help='Private shared outbox for an integrated native/browser review gate')
    parser.add_argument('--review-offline',action='store_true',help='Commit a report offline, reload real IndexedDB, then automatically upload')
    parser.add_argument('--dpr',type=int,choices=(1,2),default=1)
    parser.add_argument('--canvas-transform',action='store_true',help='Exercise review picking through an inset canvas CSS transform')
    options=parser.parse_args()
    if options.filters and (not options.source or options.source.name!='transactions.csv'):
        parser.error('--filters requires --source tests/fixtures/transactions.csv')
    if options.busy and not options.source:
        parser.error('--busy requires a real --source')
    if options.review and (options.worker_driver or not options.source or options.source.name!='transactions.csv'):
        parser.error('--review requires --source tests/fixtures/transactions.csv and cannot use --worker-driver')
    if options.canvas_transform and not options.review:
        parser.error('--canvas-transform requires --review')
    if options.review_outbox and not options.review:
        parser.error('--review-outbox requires --review')
    if options.review_offline and not options.review:
        parser.error('--review-offline requires --review')
    if not options.no_build:
        subprocess.run(['python3','tools/build_web.py']+(['--worker-driver'] if options.worker_driver else ['--review'] if options.review else []), cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
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
        if options.review:
            from review_service import ReviewServer
            from review_queue import ReviewQueue
            from native_review import default_project
            queue=ReviewQueue(options.review_outbox or output/'queue-outbox',default_project())
            server=ReviewServer(('127.0.0.1',0),queue,artifact)
        else:
            server=LoopbackServer(('127.0.0.1',0),functools.partial(Handler,directory=str(artifact)))
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
            if options.review_offline: command.append('--review-offline')
            command.extend(['--dpr',str(options.dpr)])
            if options.canvas_transform:command.append('--canvas-transform')
            process=subprocess.Popen(command,cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
            try:log,_=process.communicate(timeout=100)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid,signal.SIGTERM)
                try:log,_=process.communicate(timeout=3)
                except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);log,_=process.communicate()
            (output/'driver.txt').write_text(log)
            reader_error=None
            if options.review and process.returncode==0:
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
            record={'status':'passed' if process.returncode==0 and unchanged and artifacts_unchanged and not reader_error else 'failed','source':before,'artifact':manifest,'reader_error':reader_error,
                    'command':command,'exit_code':process.returncode,'source_unchanged':unchanged,'artifacts_unchanged':artifacts_unchanged,
                    'browser_seconds':round(time.monotonic()-begin,3),'elapsed_seconds':round(time.monotonic()-overall,3),'setup':phases,'desktop_visibility':'owned headless browser, simulated focus; no user profile'}
            (output/'summary.json').write_text(json.dumps(record,indent=2)+'\n')
            print(json.dumps({'status':record['status'],'result':str(output),'elapsed_seconds':record['elapsed_seconds']}))
            return 0 if record['status']=='passed' else 1
        finally:
            server.shutdown();server.server_close();thread.join()
if __name__=='__main__':
    raise SystemExit(main())
