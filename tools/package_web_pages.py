#!/usr/bin/env python3
"""Stage verified shipping runtime files for static hosting, without build logs."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = {
    'index.html', 'host.mjs', 'duxcel.mjs', 'duxcel.wasm', 'duxcel.data',
    'duxcel-32.png', 'vendor.json', 'canvas_input.mjs', 'canvas_surface.mjs',
    'canvas_wheel.mjs', 'browser_platform.mjs', 'clipboard.mjs', 'download.mjs',
    'official_worker.mjs', 'engine_host.mjs', 'page_wire.mjs',
}
FORBIDDEN = (
    b'dx_review_', b'dx_browser_review_', b'duxcel.native-review',
    b'duxcel.browser-review', b'dxBrowserWorkerTestDone', b'dxCodec',
    b'dx_browser_codec_', b'test_browser_', b'dx_test_source_digest_checkpoint',
)


def package(source, destination):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    manifest = json.loads((source / 'build.json').read_text())
    if (manifest.get('kind') != 'application' or
            any('REVIEW' in flag for flag in manifest['flags']) or
            any(name.startswith(('src/dev/', 'tests/')) for name in manifest['sources'])):
        raise ValueError('Only the shipping application may be published')
    artifacts = manifest['artifacts']
    if not RUNTIME.issubset(artifacts):
        raise ValueError('Incomplete application runtime')
    for name, digest in artifacts.items():
        path = PurePosixPath(name)
        if (path.is_absolute() or '..' in path.parts or
                not (name in RUNTIME or name.startswith('vendor/'))):
            raise ValueError('Unexpected runtime artifact: ' + name)
        file = source / name
        if not file.is_file() or file.is_symlink() or not file.resolve().is_relative_to(source):
            raise ValueError('Runtime artifact must be an owned file: ' + name)
        if hashlib.sha256(file.read_bytes()).hexdigest() != digest:
            raise ValueError('Runtime artifact changed: ' + name)
    wasm = (source / 'duxcel.wasm').read_bytes()
    if any(marker in wasm for marker in FORBIDDEN):
        raise ValueError('Shipping Wasm contains development code')
    host = (source / 'host.mjs').read_text()
    if any(marker in host for marker in ('dxReview', 'dxCodec', '/__review/')):
        raise ValueError('Shipping host contains development controls')
    # Refuse replacement: callers select a fresh, ignored output directory.
    destination.mkdir(parents=True, exist_ok=False)
    for name in artifacts:
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, target)
    (destination / '.nojekyll').write_text('')
    # Public provenance contains no local paths, command lines or source payloads.
    release = {'schema': 'duxcel.pages-release', 'version': 1,
               'commit': manifest['source']['commit'], 'signature': manifest['signature'],
               'artifacts': artifacts,
               'limitations': ['Browser workbook Open/Save is unavailable.']}
    (destination / 'release.json').write_text(json.dumps(release, indent=2) + '\n')
    return {'directory': str(destination), 'files': len(artifacts),
            'bytes': sum((source / name).stat().st_size for name in artifacts)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/pages')
    args = parser.parse_args()
    source = args.source or Path(json.loads((ROOT / 'build/web/app/latest.json').read_text())['directory'])
    print(json.dumps(package(source, args.output)))
