#!/usr/bin/env python3
"""Exercise the shipping runtime at a Pages-style subpath or an actual HTTPS URL."""
import argparse
import datetime
import functools
import http.server
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import time
import urllib.request

from workspace import FileLock, native_lock_path, source_metadata
from test_web_app import LoopbackServer

ROOT = Path(__file__).resolve().parents[1]


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def browser_command(url, output, *, remote):
    command = ['node', 'web/tests/driver.mjs', url, str(output / 'cdp'),
               '--source', str(ROOT / 'tests/fixtures/transactions.csv'), '--filters']
    if remote:
        # A fresh public browser also downloads/instantiates the official engine.
        # Only first import gets this allowance; subsequent queries stay bounded.
        command += ['--source-bootstrap-ms', '90000']
    return command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument('--directory', type=Path)
    target.add_argument('--url')
    args = parser.parse_args()
    started = time.monotonic()
    output = ROOT / 'build/tests/results/runs' / f'{datetime.datetime.now(datetime.timezone.utc):%Y%m%dT%H%M%S}-pages'
    output.mkdir(parents=True, exist_ok=False)
    with FileLock(native_lock_path(), 'headless Pages publication acceptance'), tempfile.TemporaryDirectory(prefix='duxcel-pages-') as temporary:
        server = None
        if args.directory:
            # A real project subpath, with ordinary static HTTP response headers.
            shutil.copytree(args.directory.resolve(), Path(temporary) / 'duxcel')
            server = LoopbackServer(('127.0.0.1', 0), functools.partial(QuietHandler, directory=temporary))
            threading.Thread(target=server.serve_forever, daemon=True).start()
            url = f'http://127.0.0.1:{server.server_port}/duxcel/'
        else:
            url = args.url.rstrip('/') + '/'
            if not url.startswith('https://'):
                raise ValueError('Remote publication checks require HTTPS')
        try:
            with urllib.request.urlopen(url + 'release.json', timeout=60) as response:
                release = json.load(response)
            before = source_metadata(ROOT)
            command = browser_command(url, output, remote=bool(args.url))
            with (output / 'browser.txt').open('w') as log:
                result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=240)
            after = source_metadata(ROOT)
            passed = result.returncode == 0 and before == after
            record = {'status': 'passed' if passed else 'failed', 'url': url,
                      'release': release, 'source': before,
                      'source_unchanged': before == after,
                      'elapsed_seconds': round(time.monotonic() - started, 3)}
            (output / 'summary.json').write_text(json.dumps(record, indent=2) + '\n')
            print(json.dumps({'status': record['status'], 'summary': str(output / 'summary.json'),
                              'elapsed_seconds': record['elapsed_seconds']}))
            return 0 if passed else 1
        finally:
            if server:
                server.shutdown()
                server.server_close()


if __name__ == '__main__':
    raise SystemExit(main())
