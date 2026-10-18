#!/usr/bin/env python3
"""Codec harness boundaries without browser, engine, GUI or shared fixtures."""
import contextlib
import functools
import io
import json
from pathlib import Path
import tempfile
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import urlopen
import test_web_app as harness


class CodecHarnessTests(unittest.TestCase):
    def fixtures(self, directory, inventory=harness.CODEC_FIXTURES):
        files=[]
        for name,(version,layout) in inventory.items():
            path=directory/name;path.write_bytes(name.encode());path.chmod(0o444)
            files.append(dict(name=name,version=version,layout=layout))
        return dict(schema='duxcel.codec-fixtures',version=1,files=files)

    def test_mode_rejects_other_profiles_before_any_process(self):
        self.assertTrue(harness.parse_options(['--codec-read-driver','--no-build']).codec_read_driver)
        self.assertTrue(harness.parse_options(['--codec-read-driver','--codec-sorts','--no-build']).codec_sorts)
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-sorts'])
        for option in ('--worker-driver','--review','--persistence','--canvas-transform','--dpr=2'):
            with self.subTest(option=option),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
                harness.parse_options(['--codec-read-driver',option])

    def test_manifest_covers_bytes_and_refuses_unowned_extra_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);emitted=self.fixtures(directory)
            before=harness.codec_fixture_manifest(directory,emitted)
            file=directory/'v12.dxls';file.chmod(0o644);file.write_bytes(b'changed');file.chmod(0o444)
            self.assertNotEqual(before,harness.codec_fixture_manifest(directory,emitted))
            (directory/'missing-source.csv').write_text('unrelated')
            with self.assertRaisesRegex(RuntimeError,'Unexpected files'):
                harness.codec_fixture_manifest(directory,emitted)

    def test_manifest_refuses_duplicate_names_schema_changes_and_links(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);emitted=self.fixtures(directory)
            duplicate=json.loads(json.dumps(emitted));duplicate['files'][1]=duplicate['files'][0]
            with self.assertRaisesRegex(RuntimeError,'Unexpected native codec fixture'):
                harness.codec_fixture_manifest(directory,duplicate)
            wrong=json.loads(json.dumps(emitted));wrong['files'][0]['layout']=24
            with self.assertRaisesRegex(RuntimeError,'Unexpected native codec fixture'):
                harness.codec_fixture_manifest(directory,wrong)
            target=directory/'v12.dxls';target.unlink();target.symlink_to('v13.dxls')
            with self.assertRaisesRegex(RuntimeError,'read-only bounded'):
                harness.codec_fixture_manifest(directory,emitted)

    def test_sort_archives_require_the_explicit_family_fixture_set(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);emitted=self.fixtures(directory,harness.CODEC_SORTS_FIXTURES)
            manifest=harness.codec_fixture_manifest(directory,emitted,harness.CODEC_SORTS_FIXTURES)
            self.assertEqual({entry['name'] for entry in manifest['files']},set(harness.CODEC_SORTS_FIXTURES))
            with self.assertRaisesRegex(RuntimeError,'Incomplete native codec fixture set'):
                harness.codec_fixture_manifest(directory,emitted)

    def test_reserved_routes_are_sealed_and_never_fall_back_to_static_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);fixtures=directory/'fixtures';fixtures.mkdir()
            manifest=harness.codec_fixture_manifest(fixtures,self.fixtures(fixtures))
            static=directory/'static';(static/'__codec-fixtures').mkdir(parents=True)
            (static/'__codec-fixtures/unlisted.dxls').write_bytes(b'should never be served')
            server=harness.LoopbackServer(('127.0.0.1',0),functools.partial(harness.Handler,directory=str(static)))
            server.codec_manifest=manifest;server.codec_requests=[]
            server.codec_files={'/__codec-fixtures/'+item['name']:(fixtures/item['name'],item) for item in manifest['files']}
            thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
            base=f'http://127.0.0.1:{server.server_port}'
            try:
                with urlopen(base+'/__codec-fixtures/manifest.json',timeout=2) as response:
                    self.assertEqual(json.load(response),manifest)
                with urlopen(base+'/__codec-fixtures/v12.dxls',timeout=2) as response:
                    self.assertEqual(response.read(),b'v12.dxls')
                with self.assertRaises(HTTPError) as caught:urlopen(base+'/__codec-fixtures/unlisted.dxls',timeout=2)
                self.assertEqual(caught.exception.code,404)
                caught.exception.close()
                file=fixtures/'v12.dxls';file.chmod(0o644);file.write_bytes(b'tampered');file.chmod(0o444)
                with self.assertRaises(HTTPError) as caught:urlopen(base+'/__codec-fixtures/v12.dxls',timeout=2)
                self.assertEqual(caught.exception.code,409)
                caught.exception.close()
                self.assertEqual([item['name'] for item in server.codec_requests],['v12.dxls'])
            finally:server.shutdown();server.server_close();thread.join()


if __name__=='__main__':unittest.main()
