"""Enforce repository role boundaries requested by the maintainer."""
import ast
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
class LayoutTests(unittest.TestCase):
    def test_tooling_has_no_product_linked_c_helpers_or_embedded_test_suites(self):
        self.assertFalse(list((ROOT / 'tools').glob('*.c')))
        self.assertFalse(list((ROOT / 'tools').glob('*.mjs')))
        for path in (ROOT / 'tools').glob('*.py'):
            tree = ast.parse(path.read_text())
            tests = [node.name for node in ast.walk(tree) if isinstance(node, ast.ClassDef) and any(ast.unparse(base).endswith('TestCase') for base in node.bases)]
            self.assertFalse(tests, str(path))
        self.assertTrue((ROOT / 'tests/support/create_workbook.c').is_file())

    def test_browser_runtime_has_no_node_imports_or_browser_test_drivers(self):
        for path in (ROOT / 'web').glob('*.mjs'):
            self.assertNotIn("from 'node:", path.read_text(), str(path))
            self.assertFalse(path.name.endswith('.test.mjs') or '_probe' in path.stem or '_test_host' in path.stem)
        self.assertTrue((ROOT / 'web/tests/driver.mjs').is_file())

    def test_json_fixtures_are_versioned_inputs_not_session_reports(self):
        import json
        files = sorted((ROOT / 'tests/fixtures/ui').glob('*.json'))
        self.assertEqual(len(files), 3)
        for path in files:
            data = json.loads(path.read_text())
            self.assertTrue(data['schema'].startswith('duxcel.ui.'))
            self.assertEqual(data['version'], 2)
            self.assertTrue(data['states'])
            self.assertFalse({'command', 'prototype', 'inputs', 'provenance', 'original', 'elapsed_seconds'} & data.keys())
        self.assertFalse(list((ROOT / 'tests').glob('*.json')))

    def test_ignored_bytecode_and_downloads_are_not_classified_as_test_source(self):
        from project_inventory import category
        self.assertEqual(category(Path('tools/tests/__pycache__/test_runner.pyc')), 'python-bytecode-cache')
        self.assertEqual(category(Path('data/downloads/yellow.parquet')), 'downloaded-reference-data')
        self.assertEqual(category(Path('.cache/web/toolchain/emcc.py')), 'dependency-cache')
        self.assertEqual(category(Path('tools/tests/test_runner.py')), 'tooling-test-source')
