#!/usr/bin/env python3
"""Run the actual shared C browser UI in an owned, hidden browser session."""
import argparse, datetime, functools, hashlib, http.server, json, os, subprocess, threading, time
from pathlib import Path
from test import FileLock, native_lock_path, source_metadata
ROOT=Path(__file__).resolve().parents[1]
class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args): pass

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-build', action='store_true')
    options=parser.parse_args()
    if not options.no_build:
        subprocess.run(['python3','tools/build_web.py'], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    latest=json.loads((ROOT/'build/web/app/latest.json').read_text())
    artifact=Path(latest['directory']); manifest=json.loads((artifact/'build.json').read_text())
    for name,digest in manifest['artifacts'].items():
        if hashlib.sha256((artifact/name).read_bytes()).hexdigest()!=digest:
            raise RuntimeError('Browser artifact changed: '+name)
    before=source_metadata(ROOT)
    output=ROOT/'tests/results/runs'/f'{datetime.datetime.now(datetime.timezone.utc):%Y%m%dT%H%M%S}-browser-{os.getpid()}'
    output.mkdir(parents=True,exist_ok=True)
    with FileLock(native_lock_path(),'hidden shared C browser acceptance'):
        server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(artifact)))
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        begin=time.monotonic()
        try:
            command=['node','tools/test_web_app.mjs',f'http://127.0.0.1:{server.server_port}/',str(output)]
            result=subprocess.run(command,cwd=ROOT,text=True,capture_output=True,timeout=100)
            (output/'driver.txt').write_text(result.stdout+result.stderr)
            after=source_metadata(ROOT)
            unchanged=after['source_sha256']==before['source_sha256'] and after['commit']==before['commit']
            artifacts_unchanged=all(hashlib.sha256((artifact/name).read_bytes()).hexdigest()==digest for name,digest in manifest['artifacts'].items())
            record={'status':'passed' if result.returncode==0 and unchanged and artifacts_unchanged else 'failed','source':before,'artifact':manifest,
                    'command':command,'exit_code':result.returncode,'source_unchanged':unchanged,'artifacts_unchanged':artifacts_unchanged,
                    'elapsed_seconds':round(time.monotonic()-begin,3),'desktop_visibility':'owned headless browser, simulated focus; no user profile'}
            (output/'summary.json').write_text(json.dumps(record,indent=2)+'\n')
            print(json.dumps({'status':record['status'],'result':str(output),'elapsed_seconds':record['elapsed_seconds']}))
            return 0 if record['status']=='passed' else 1
        finally:
            server.shutdown();server.server_close();thread.join()
if __name__=='__main__':
    raise SystemExit(main())
