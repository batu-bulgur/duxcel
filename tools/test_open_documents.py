#!/usr/bin/env python3
"""Hidden LaunchServices cold/warm workbook Open checks in an isolated bundle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import signal
import subprocess
import time

from workspace import ROOT, FileLock, native_lock_path, source_metadata


def main(argv=None):
    argparse.ArgumentParser(description=__doc__).parse_args(argv)
    output = ROOT / 'build/tests/results/runs' / (time.strftime('%Y%m%dT%H%M%S', time.gmtime()) + f'-open-documents-{os.getpid()}')
    output.mkdir(parents=True)
    bundle = output / 'DocumentOpenTest.app'
    executable = bundle / 'Contents/MacOS/duxcel'
    fixture = ROOT / 'build/tests/generated/layout-option-range.dxls'
    workbook = output / 'range move ünicode.dxls'
    alert_guard = ROOT / 'build/tests/native-alert-guard.dylib'
    summary = {'status': 'failed', 'source': source_metadata(ROOT), 'results': []}
    started = time.monotonic()
    env = {**os.environ, 'DUXCEL_TEST_VISIBLE': '0', 'DUXCEL_TEST_INSTANCE': '1'}
    for key in ('DUXCEL_LAYOUT_FROM', 'DUXCEL_LAYOUT_TO'):
        env.pop(key, None)
    lsregister = '/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister'

    def owned_pids():
        result = subprocess.run(['ps', '-axo', 'pid=,args='], text=True, capture_output=True, check=True)
        return [int(line.split(None, 1)[0]) for line in result.stdout.splitlines()
                if len(line.split(None, 1)) == 2 and line.split(None, 1)[1].startswith(str(executable) + ' ')]

    def run(argv, name, timeout=60, expected=0, extra_env=None):
        before = time.monotonic()
        result = subprocess.run(argv, cwd='/', env={**env, **(extra_env or {})}, text=True, capture_output=True, timeout=timeout)
        (output / (name + '.txt')).write_text(result.stdout + result.stderr)
        if result.returncode != expected:
            raise RuntimeError(f'{name} exited {result.returncode}')
        summary['results'].append({'name': name, 'command': argv, 'exit_code': result.returncode, 'elapsed_seconds': round(time.monotonic() - before, 3)})
        return result

    try:
        if not fixture.is_file():
            raise RuntimeError('Run the native range-move profile first; its generated manual-only workbook is the Open fixture')
        shutil.copyfile(fixture, workbook)
        plist = plistlib.loads((ROOT / 'build/Duxcel.app/Contents/Info.plist').read_bytes())
        assert plist['CFBundleDocumentTypes'][0]['CFBundleTypeRole'] == 'Editor'
        assert plist['CFBundleDocumentTypes'][0]['LSItemContentTypes'] == ['dev.duxcel.workbook']
        assert set(plist['UTExportedTypeDeclarations'][0]['UTTypeTagSpecification']['public.filename-extension']) == {'dxls', 'duxcel'}
        run(['make', '-C', str(ROOT), 'build/tests/native-alert-guard.dylib'], 'build-alert-guard')
        with FileLock(native_lock_path(), 'LaunchServices document Open checks'):
            try:
                shutil.copytree(ROOT / 'build/Duxcel.app', bundle)
                plist['CFBundleIdentifier'] = f'dev.duxcel.document-test.p{os.getpid()}'
                plist['CFBundleDocumentTypes'][0]['LSHandlerRank'] = 'None'
                (bundle / 'Contents/Info.plist').write_bytes(plistlib.dumps(plist))
                run(['codesign', '--force', '--sign', '-', str(bundle)], 'sign-private-bundle')
                summary['binary_sha256'] = hashlib.sha256(executable.read_bytes()).hexdigest()
                summary['workbook_sha256'] = hashlib.sha256(workbook.read_bytes()).hexdigest()
                guard_env = {'DYLD_INSERT_LIBRARIES': str(alert_guard)}
                control = run([str(executable), '--help'], 'alert-negative-control', expected=73,
                              extra_env={**guard_env, 'DUXCEL_TEST_ALERT_PROBE': '1'})
                assert 'FAIL: native alert intercepted' in control.stderr, 'Native alert negative control did not fire'
                reference = output / 'reference.png'
                run([str(executable), str(workbook), '--frames', '120', '--screenshot', str(reference)], 'direct-path', extra_env=guard_env)
                cold, cold_out, cold_err = (output / name for name in ('cold.png', 'cold-out.txt', 'cold-err.txt'))
                run(['open', '-W', '-n', '-g', '-j', '-a', str(bundle), '--stdout', str(cold_out), '--stderr', str(cold_err),
                     '--env', 'DUXCEL_TEST_VISIBLE=0', '--env', 'DUXCEL_TEST_INSTANCE=1', '--env', 'DYLD_INSERT_LIBRARIES=' + str(alert_guard),
                     str(workbook), '--args', '--frames', '120', '--screenshot', str(cold)], 'cold-launch')
                warm = output / 'warm.png'
                with (output / 'warm-launcher.txt').open('w') as stream:
                    process = subprocess.Popen(['open', '-W', '-n', '-g', '-j', '-a', str(bundle),
                        '--stdout', str(output / 'warm.txt'), '--stderr', str(output / 'warm-err.txt'),
                        '--env', 'DUXCEL_TEST_VISIBLE=0', '--env', 'DUXCEL_TEST_INSTANCE=1', '--env', 'DYLD_INSERT_LIBRARIES=' + str(alert_guard),
                        '--args', '--frames', '300', '--screenshot', str(warm)],
                        cwd='/', env=env, stdout=stream, stderr=stream)
                    try:
                        deadline = time.monotonic() + 10
                        while not (output / 'warm.txt').exists() or 'Native test window: hidden, unfocused' not in (output / 'warm.txt').read_text():
                            if process.poll() is not None or time.monotonic() > deadline:
                                raise RuntimeError('Owned warm app did not initialize hidden')
                            time.sleep(.02)
                        pids = owned_pids()
                        assert len(pids) == 1
                        summary['warm_pid'] = pids[0]
                        run(['open', '-g', '-j', '-a', str(bundle), str(workbook)], 'warm-open')
                        assert owned_pids() == pids, 'Warm Open launched a second process'
                        if process.wait(timeout=30):
                            raise RuntimeError('Owned warm app failed')
                    finally:
                        if process.poll() is None:
                            process.terminate()
                            process.wait(timeout=10)
                expected = reference.read_bytes()
                for name, path in (('cold', cold), ('warm', warm)):
                    assert path.read_bytes() == expected, f'{name} Open did not render the same workbook as the direct-path baseline'
                    summary[name + '_png_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
                for name in ('direct-path.txt', 'cold-err.txt', 'warm-err.txt'):
                    log = (output / name).read_text()
                    assert 'Native alert guard installed' in log, f'{name}: alert guard was not loaded'
                    assert 'FAIL: native alert' not in log, f'{name}: unexpected native alert'
                assert hashlib.sha256(workbook.read_bytes()).hexdigest() == summary['workbook_sha256']
                summary['status'] = 'passed'
            finally:
                # A timeout may kill open's waiter before the app. Only the
                # unique private executable belongs to this runner.
                for pid in owned_pids():
                    os.kill(pid, signal.SIGTERM)
                if bundle.exists():
                    subprocess.run([lsregister, '-u', str(bundle)], capture_output=True, timeout=10)
                    shutil.rmtree(bundle)
    except Exception as error:
        summary['error'] = str(error)
    summary['elapsed_seconds'] = round(time.monotonic() - started, 3)
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps({'status': summary['status'], 'result': str(output), 'error': summary.get('error')}, indent=2))
    return summary['status'] != 'passed'


if __name__ == '__main__':
    raise SystemExit(main())
