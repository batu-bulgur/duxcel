#!/usr/bin/env python3
"""Publication boundary checks: integrity, isolation, and a minimal public payload."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

from package_web_pages import RUNTIME, package
SPEC = importlib.util.spec_from_file_location('duxcel_pages_acceptance', Path(__file__).resolve().parents[1] / 'test_web_pages.py')
pages_acceptance = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pages_acceptance)


class PagesBrowserCommandTests(unittest.TestCase):
    def test_remote_bootstrap_allowance_preserves_source_and_filters(self):
        local = pages_acceptance.browser_command('http://127.0.0.1/duxcel/', Path('result'), remote=False)
        remote = pages_acceptance.browser_command('https://example.com/duxcel/', Path('result'), remote=True)
        self.assertNotIn('--source-bootstrap-ms', local)
        self.assertEqual(remote[-2:], ['--source-bootstrap-ms', '90000'])
        self.assertEqual(local[4:], remote[4:-2])
        self.assertIn('--source', remote)
        self.assertIn('--filters', remote)


class PagesPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / 'app'
        self.source.mkdir()
        self.output = Path(self.temp.name) / 'pages'
        for name in RUNTIME | {'vendor/duckdb/LICENSE'}:
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'runtime')
        self.manifest = {'kind': 'application', 'flags': ['-O2'],
                         'sources': ['src/main.c'], 'source': {'commit': 'abc123'},
                         'signature': 'signature', 'artifacts': {
                             path.relative_to(self.source).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                             for path in self.source.rglob('*') if path.is_file()}}
        self.write_manifest()

    def write_manifest(self):
        (self.source / 'build.json').write_text(json.dumps(self.manifest))

    def test_runtime_only_and_public_provenance(self):
        (self.source / 'build.txt').write_text('/private/path and compiler logs')
        package(self.source, self.output)
        self.assertEqual({path.relative_to(self.output).as_posix() for path in self.output.rglob('*') if path.is_file()},
                         set(self.manifest['artifacts']) | {'.nojekyll', 'release.json'})
        self.assertEqual(json.loads((self.output / 'release.json').read_text())['commit'], 'abc123')

    def test_review_and_acceptance_builds_rejected(self):
        for key, value in [('kind', 'review'), ('flags', ['-DDUXCEL_DEV_REVIEW']),
                           ('sources', ['src/dev/review_browser.c']), ('sources', ['tests/test_browser_worker.c'])]:
            with self.subTest(key=key, value=value):
                original = self.manifest[key]
                self.manifest[key] = value
                self.write_manifest()
                with self.assertRaises(ValueError):
                    package(self.source, self.output)
                self.manifest[key] = original

    def test_missing_or_changed_runtime_rejected(self):
        (self.source / 'duxcel.wasm').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'changed'):
            package(self.source, self.output)
        del self.manifest['artifacts']['duxcel.wasm']
        self.write_manifest()
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            package(self.source, self.output)

    def test_unexpected_and_traversing_files_rejected(self):
        for name in ['../outside', '/absolute', 'review_host.mjs', 'build.json']:
            with self.subTest(name=name):
                self.manifest['artifacts'][name] = 'unused'
                self.write_manifest()
                with self.assertRaisesRegex(ValueError, 'Unexpected'):
                    package(self.source, self.output)
                del self.manifest['artifacts'][name]

    def test_linked_development_markers_rejected(self):
        (self.source / 'duxcel.wasm').write_bytes(b'dx_review_begin')
        self.manifest['artifacts']['duxcel.wasm'] = hashlib.sha256(b'dx_review_begin').hexdigest()
        self.write_manifest()
        with self.assertRaisesRegex(ValueError, 'development'):
            package(self.source, self.output)

    def test_existing_destination_preserved(self):
        self.output.mkdir()
        (self.output / 'keep').write_text('existing')
        with self.assertRaises(FileExistsError):
            package(self.source, self.output)
        self.assertEqual((self.output / 'keep').read_text(), 'existing')


if __name__ == '__main__':
    unittest.main()
