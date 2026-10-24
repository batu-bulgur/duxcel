"""Real Make metadata contracts for direct and make-launched Wasm builds."""
import contextlib
import io
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import build_web

TARGETS = ('web-sources', 'web-review-sources', 'web-worker-sources',
           'web-codec-read-sources', 'web-duckdb-flags')


class WebBuildMetadataTests(unittest.TestCase):
    def test_product_metadata_is_confined_to_its_translation_unit(self):
        flags=['-O2','-DDUXCEL_BROWSER_HOST']
        for source in ('src/app/app.c','src/grid/grid.c','src/app/selection.c'):
            self.assertEqual(build_web.source_compile_flags(flags,source,Path('info.h')),flags)
        self.assertEqual(build_web.source_compile_flags(flags,'src/app/build_info.c',Path('info.h')),
                         flags+['-include','info.h'])
        self.assertEqual(flags,['-O2','-DDUXCEL_BROWSER_HOST'])

    def test_all_source_inventories_reference_existing_c_files(self):
        for target in TARGETS[:-1]:
            with self.subTest(target=target):
                sources = build_web.make_metadata(target).splitlines()
                self.assertTrue(sources)
                self.assertFalse(set(sources) & {"src/workbook/version_restore.c", "src/workbook/version_read.c", "src/workbook/version_sql.c", "src/workbook/version_stage.c", "src/workbook/core_compare.c", "src/workbook/core_payload.c", "src/workbook/recovery_worker.c", "src/platform/directory.c", "src/workbook/recovery_catalog.c", "src/platform/file_lease.c", "src/workbook/recovery.c", "src/workbook/journal.c", "src/workbook/journal_queue.c", "src/workbook/journal_session.c"})
                self.assertEqual(len(sources), len(set(sources)))
                self.assertTrue(all(name.endswith('.c') and
                                    (build_web.ROOT / name).is_file() for name in sources))

    def test_parallel_make_parent_preserves_sources_and_cli_header_override(self):
        # A normal recipe closes jobserver descriptors before starting Python,
        # exactly as `make web` does. Do not mark this recipe recursive with '+'.
        expression = ('import json,sys; sys.path.insert(0,' + repr(str(build_web.ROOT / 'tools')) + '); '
                      'import build_web; print(json.dumps({target:build_web.make_metadata(target) '
                      'for target in ' + repr(TARGETS) + '}))')
        with tempfile.TemporaryDirectory(prefix='duxcel-web-make-') as directory:
            makefile = Path(directory) / 'probe.mk'
            makefile.write_text('probe-web-metadata:\n\t@' + shlex.join([sys.executable, '-c', expression]) + '\n')
            result = subprocess.run(['make', '--no-print-directory', '-f', 'Makefile',
                                     '-f', str(makefile), 'probe-web-metadata',
                                     'DUCKDB_CFLAGS=-Ici-header-override'],
                                    cwd=build_web.ROOT, text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        metadata = json.loads(result.stdout)
        self.assertEqual(metadata['web-duckdb-flags'].strip(), '-Ici-header-override')
        for target in TARGETS[:-1]:
            with self.subTest(target=target):
                self.assertEqual(metadata[target], build_web.make_metadata(target))

    def test_diagnostics_remain_visible_without_polluting_metadata(self):
        result = subprocess.CompletedProcess([], 0, 'src/main.c\n', 'make: jobserver warning\n')
        errors = io.StringIO()
        with mock.patch.object(build_web.subprocess, 'run', return_value=result), \
             contextlib.redirect_stderr(errors):
            self.assertEqual(build_web.make_metadata('web-sources'), 'src/main.c\n')
        self.assertEqual(errors.getvalue(), result.stderr)

    def test_failed_query_reports_target_and_make_error(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaisesRegex(build_web.tools.ToolchainError,
                                        'Make metadata query failed: no-such-web-metadata-target') as failure:
                build_web.make_metadata('no-such-web-metadata-target')
        self.assertIn('No rule to make target', str(failure.exception))


if __name__ == '__main__':
    unittest.main()
