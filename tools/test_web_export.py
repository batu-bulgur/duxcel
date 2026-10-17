#!/usr/bin/env python3
"""Isolated test-only Parquet capability proof; never builds or opens a user UI."""
import argparse
import datetime
import functools
import hashlib
import http.server
import json
import os
from pathlib import Path
import signal
import socketserver
import subprocess
import threading
import time
from test import FileLock, native_lock_path, source_metadata

ROOT = Path(__file__).resolve().parents[1]


class Server(http.server.ThreadingHTTPServer):
    def server_bind(self):
        socketserver.TCPServer.server_bind(self)
        self.server_name = '127.0.0.1'
        self.server_port = self.server_address[1]


class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        probes = {f'/{name}': ROOT/'web'/name for name in ('export_probe.mjs', 'official_worker_probe.mjs')}
        if self.path == '/':
            content = ('<!doctype html><meta charset="utf-8"><title>Duxcel isolated Parquet proof</title>'
                       '<script type="importmap">'+json.dumps(self.server.import_map)+'</script>').encode()
            mime = 'text/html; charset=utf-8'
        elif self.path in probes:
            content, mime = probes[self.path].read_bytes(), 'text/javascript'
        elif self.path in ('/vendor.json', '/official_worker.mjs') or self.path.startswith('/vendor/'):
            return super().do_GET()
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(content)))
        self.end_headers()
        self.wfile.write(content)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    paths = parser.add_mutually_exclusive_group(required=True)
    paths.add_argument('--artifact', type=Path, help='Existing immutable browser artifact directory')
    paths.add_argument('--latest', type=Path, help='Existing browser latest.json; no build is run')
    options = parser.parse_args()
    artifact = (options.artifact or Path(json.loads(options.latest.read_text())['directory'])).resolve(strict=True)
    manifest = json.loads((artifact/'build.json').read_text())
    assets = manifest['artifacts']
    if any(digest(artifact/name) != expected for name, expected in assets.items()):
        raise RuntimeError('Existing browser artifact hash mismatch')
    vendor = json.loads((artifact/'vendor.json').read_text())
    if vendor['packages']['@duckdb/duckdb-wasm']['version'] != '1.32.0':
        raise RuntimeError('This proof requires the pinned official Worker package 1.32.0')
    output = ROOT/'tests/results/runs'/f'{datetime.datetime.now(datetime.timezone.utc):%Y%m%dT%H%M%S}-parquet-export-{os.getpid()}'
    output.mkdir(parents=True, exist_ok=False)
    before = source_metadata(ROOT)
    started = time.monotonic()
    with FileLock(native_lock_path(), 'isolated hidden official Worker Parquet proof'):
        server = Server(('127.0.0.1', 0), functools.partial(Handler, directory=str(artifact)))
        server.import_map = vendor['import_map']
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        command = ['node', 'tools/test_web_export.mjs', f'http://127.0.0.1:{server.server_port}/', str(output)]
        process = None
        try:
            process = subprocess.Popen(command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            try:
                log, _ = process.communicate(timeout=100)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    log, _ = process.communicate(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    log, _ = process.communicate()
            (output/'driver.txt').write_text(log)
            after = source_metadata(ROOT)
            unchanged = before['commit'] == after['commit'] and before['source_sha256'] == after['source_sha256']
            artifact_unchanged = all(digest(artifact/name) == expected for name, expected in assets.items())
            passed = process.returncode == 0 and unchanged and artifact_unchanged
            summary = {'status': 'passed' if passed else 'failed', 'source': before, 'artifact': str(artifact),
                       'artifact_source': manifest['source'], 'vendor': vendor['packages'], 'command': command,
                       'exit_code': process.returncode, 'source_unchanged': unchanged,
                       'artifacts_unchanged': artifact_unchanged, 'elapsed_seconds': round(time.monotonic()-started, 3),
                       'desktop_visibility': 'one owned headless browser/profile under the desktop lock; no downloads',
                       'scope': 'Pinned Parquet virtual-file capability only; no C export/controller or product activation.'}
            (output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
            print(json.dumps({'status': summary['status'], 'result': str(output), 'elapsed_seconds': summary['elapsed_seconds']}))
            return 0 if passed else 1
        finally:
            if process and process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == '__main__':
    raise SystemExit(main())
