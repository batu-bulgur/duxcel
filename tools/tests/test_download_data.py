import io
from pathlib import Path
import tempfile
import unittest
from unittest import mock
from urllib.error import URLError
import download_test_data as data

class Response(io.BytesIO):
    def __init__(self, value, length=None):
        super().__init__(value); self.headers = {} if length is None else {'Content-Length': str(length)}

class DownloadTests(unittest.TestCase):
    def test_complete_download_is_atomic_and_cache_never_uses_network(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'yellow.parquet'
            with mock.patch.object(data, 'urlopen', return_value=Response(b'PAR1test', 8)) as request:
                self.assertTrue(data.fetch('https://example.test/data', path))
                self.assertFalse(data.fetch('https://example.test/data', path))
                self.assertEqual(request.call_count, 1)
            self.assertEqual(path.read_bytes(), b'PAR1test')
            self.assertEqual(list(Path(folder).iterdir()), [path])

    def test_empty_truncated_and_network_failures_publish_no_partial_file(self):
        for response in (Response(b'', 0), Response(b'part', 8), URLError('offline')):
            with self.subTest(response=type(response).__name__), tempfile.TemporaryDirectory() as folder:
                path = Path(folder) / 'data'
                kwargs = {'side_effect': response} if isinstance(response, Exception) else {'return_value': response}
                with mock.patch.object(data, 'urlopen', **kwargs), self.assertRaises((ValueError, URLError)):
                    data.fetch('https://example.test/data', path)
                self.assertEqual(list(Path(folder).iterdir()), [])

    def test_cli_only_generates_derivatives_when_explicitly_requested(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(data, 'ROOT', Path(folder)), mock.patch.object(data, 'fetch'), mock.patch.object(data.subprocess, 'run') as run:
            self.assertEqual(data.main([]), 0); run.assert_not_called()
            def converted(*args, **kwargs):
                stage = next((Path(folder) / 'build/tests/generated/public-data').glob('.roundtrip-*'))
                (stage / 'yellow.csv').write_text('a,b\n1,2\n')
                (stage / 'yellow_roundtrip.parquet').write_bytes(b'PAR1test')
            run.side_effect = converted
            self.assertEqual(data.main(['--roundtrip']), 0)
            self.assertEqual(run.call_args.args[0][0], 'duckdb')
            self.assertIn('build/tests/generated/public-data', run.call_args.args[0][2])

    def test_conversion_failure_keeps_original_download_and_previous_derivatives(self):
        import subprocess
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(data, 'ROOT', Path(folder)), mock.patch.object(data, 'fetch'):
            output = Path(folder) / 'build/tests/generated/public-data'; output.mkdir(parents=True)
            for name in ('yellow.csv', 'yellow_roundtrip.parquet'):
                (output / name).write_bytes(b'previous')
            with mock.patch.object(data.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'duckdb')), self.assertRaises(subprocess.CalledProcessError):
                data.main(['--roundtrip'])
            self.assertEqual(sorted(p.name for p in output.iterdir()), ['yellow.csv', 'yellow_roundtrip.parquet'])
            self.assertTrue(all(p.read_bytes() == b'previous' for p in output.iterdir()))
