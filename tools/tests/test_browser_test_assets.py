from pathlib import Path
import re
import unittest
from browser_test_assets import flat_test_module

ROOT = Path(__file__).resolve().parents[2]
class BrowserTestModuleTests(unittest.TestCase):
    def test_every_relocated_probe_and_host_has_valid_flat_package_imports(self):
        support = ROOT / 'web/tests/support'
        generated = {'duxcel.mjs'}
        known = generated | {p.name for p in (ROOT / 'web').glob('*.mjs')} | {p.name for p in support.glob('*.mjs')}
        names = ('export_probe','official_worker_probe','persistence_probe','metadata_stream_probe','worker_test_host','codec_test_host')
        for name in names:
            with self.subTest(module=name):
                source = flat_test_module(support / (name + '.mjs'))
                imports = re.findall(r"(?:from\s*|import\s*\(\s*)['\"](\.[^'\"]+)['\"]", source)
                self.assertTrue(imports)
                self.assertTrue(all(value.startswith('./') and '/' not in value[2:] and value[2:] in known for value in imports))
                self.assertNotIn("from '../../", source)
