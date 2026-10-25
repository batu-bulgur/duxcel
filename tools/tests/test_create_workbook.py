"""Finite synthetic inputs, real DuckDB conversion and failure publication."""
import csv
import hashlib
from decimal import Decimal
import pathlib
import shutil
import tempfile
import unittest
from unittest import mock

import create_workbook as fixture


class FixtureTests(unittest.TestCase):
    def test_csv_contract_is_deterministic_and_preserves_quoted_empty_text(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "input.csv"
            fixture.write_csv(path, False)
            before = path.read_bytes()
            fixture.write_csv(path, False)
            self.assertEqual(path.read_bytes(), before)
            with path.open() as source:
                rows = list(csv.DictReader(source))
            self.assertEqual(len(rows), 310)
            selected = [r for r in rows if r["Status"] == "Posted" and r["Currency"] == "USD"]
            self.assertEqual(len(selected), 240)
            self.assertEqual(sum(Decimal(r["Amount (USD)"]) for r in selected), Decimal("-1288834.30"))
            self.assertIn(b',""\n', before)
            self.assertIn(b',\n', before)
            self.assertIn(b',"NULL"\n', before)

    @unittest.skipUnless(shutil.which("duckdb"), "existing DuckDB CLI required")
    def test_real_canonical_and_large_parquet_and_versioned_workbook_inputs(self):
        with tempfile.TemporaryDirectory(prefix="duxcel fixture ' ") as directory:
            output = pathlib.Path(directory)
            for large, prefix, rows in ((False, "design-definition", 310), (True, "design-large", 48216)):
                stats = fixture.create(output, large=large)
                self.assertEqual(stats["rows"], rows)
                self.assertTrue(all(stats[key] for key in ("nulls", "empty_text", "literal_null", "exact_decimal")))
                book = output / (prefix + ".dxls")
                records = fixture.execute(shutil.which("duckdb"), book,
                    "SELECT version,(SELECT count(*) FROM _duxcel_workspace.sheets) AS sheets,"
                    "(SELECT path FROM _duxcel_workspace.sheets WHERE id=0) AS source FROM _duxcel_workspace.meta;")
                self.assertEqual(records, [{"version": 16, "sheets": 4, "source": str((output / (prefix + ".parquet")).resolve())}])
                archive = fixture.execute(shutil.which("duckdb"), book,
                    "SELECT (SELECT count(*) FROM information_schema.tables WHERE table_schema='_duxcel_workspace') AS tables,"
                    "codec,generation,saved_generation,hex(saved_key) AS saved FROM _duxcel_versions.meta")[0]
                self.assertEqual((archive["tables"], archive["codec"], int(archive["generation"]), int(archive["saved_generation"])), (28, 1, 1, 1))
                chunks = fixture.execute(shutil.which("duckdb"), book,
                    "SELECT position,hex(data) AS data FROM _duxcel_versions.chunks ORDER BY position")
                payload = b"".join(bytes.fromhex(chunk["data"]) for chunk in chunks)
                self.assertEqual(hashlib.sha256(payload).hexdigest().upper(), archive["saved"])
                self.assertEqual([int(chunk["position"]) for chunk in chunks], list(range(len(chunks))))
                self.assertTrue(all(len(bytes.fromhex(chunk["data"])) == 65536 for chunk in chunks[:-1]))
                core = output / "admitted-current-core.dxls"
                core.write_bytes(payload)
                inner = fixture.execute(shutil.which("duckdb"), core,
                    "SELECT version,(SELECT path FROM _duxcel_workspace.sheets WHERE id=0) AS source FROM _duxcel_workspace.meta")
                self.assertEqual(inner, [{"version": 15, "source": str((output / (prefix + ".parquet")).resolve())}])
                core.unlink()
                self.assertFalse(list(output.glob(".transactions-*")))

    def test_failed_candidate_never_replaces_existing_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory)
            paths = [output / ("design-definition" + suffix) for suffix in (".csv", ".parquet", ".dxls")]
            for path in paths:
                path.write_bytes(b"accepted")
            with mock.patch.object(fixture, "execute", side_effect=RuntimeError("conversion failed")):
                with self.assertRaisesRegex(RuntimeError, "conversion failed"):
                    fixture.create(output, duckdb="existing-duckdb")
            self.assertTrue(all(path.read_bytes() == b"accepted" for path in paths))
            self.assertFalse(list(output.glob(".transactions-*")))

    @unittest.skipUnless(shutil.which("duckdb"), "existing DuckDB CLI required")
    def test_failed_container_never_replaces_existing_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory)
            paths = [output / ("design-definition" + suffix) for suffix in (".csv", ".parquet", ".dxls")]
            for path in paths:
                path.write_bytes(b"accepted")
            original = fixture.execute
            def fail_container(executable, database, sql):
                if "CREATE SCHEMA _duxcel_versions" in sql:
                    raise RuntimeError("container failed")
                return original(executable, database, sql)
            with mock.patch.object(fixture, "execute", side_effect=fail_container):
                with self.assertRaisesRegex(RuntimeError, "container failed"):
                    fixture.create(output)
            self.assertTrue(all(path.read_bytes() == b"accepted" for path in paths))
            self.assertFalse(list(output.glob(".transactions-*")))

    def test_missing_cli_fails_before_creating_outputs(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(fixture.shutil, "which", return_value=None):
            output = pathlib.Path(directory) / "new"
            with self.assertRaisesRegex(RuntimeError, "existing DuckDB CLI"):
                fixture.create(output)
            self.assertFalse(output.exists())


class SourceConnectionTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("duckdb"), "existing DuckDB CLI required")
    def test_finite_relations_and_failure_before_publication(self):
        with tempfile.TemporaryDirectory(prefix="duxcel connections ' ") as directory:
            output = pathlib.Path(directory)
            targets = fixture.create_source_connections(output)
            self.assertEqual(len(targets), 2)
            for index, path in enumerate(targets):
                rows = fixture.execute(shutil.which("duckdb"), path,
                    "SELECT (SELECT count(*) FROM alpha) AS a,(SELECT count(*) FROM beta_view) AS b,"
                    "(SELECT sum(Amount) FROM beta) AS amount;")
                self.assertEqual(rows, [{"a": 3, "b": 2, "amount": "300.00" if not index else "3000.00"}])
            before = [path.read_bytes() for path in targets]
            original = fixture.execute
            calls = 0
            def fail_second(*args, **kwargs):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise RuntimeError("second candidate failed")
                return original(*args, **kwargs)
            with mock.patch.object(fixture, "execute", side_effect=fail_second):
                with self.assertRaisesRegex(RuntimeError, "second candidate failed"):
                    fixture.create_source_connections(output)
            self.assertEqual([path.read_bytes() for path in targets], before)
            self.assertFalse(list(output.glob(".source-connections-*")))


class SourceOpenTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("duckdb"), "existing DuckDB CLI required")
    def test_finite_file_probes_and_failure_before_publication(self):
        with tempfile.TemporaryDirectory(prefix="duxcel source opens '") as directory:
            output = pathlib.Path(directory)
            targets = fixture.create_source_opens(output)
            self.assertEqual([path.name for path in targets], ["source-open.csv", "source-open.parquet", "source-open-bad.csv.gz", "source-open-bad.parquet", "source-open-wide.parquet"])
            executable = shutil.which("duckdb")
            for path, reader in ((targets[0], "read_csv"), (targets[1], "read_parquet")):
                options = ",allow_quoted_nulls=false" if reader == "read_csv" else ""
                rows = fixture.execute(executable, ":memory:", f"SELECT count(*) AS n,sum(Amount)::DECIMAL(12,2) AS amount,"
                    f"count(Value) AS present,count_if(Value='')::BIGINT AS empty,count_if(Value='NULL')::BIGINT AS literal FROM {reader}({fixture.quote(path)}{options});")
                self.assertEqual(rows, [{"n": 3, "amount": "60.00", "present": 2, "empty": 1, "literal": 1}])
            for path, reader in ((targets[2], "read_csv"), (targets[3], "read_parquet")):
                with self.assertRaises(RuntimeError):
                    fixture.execute(executable, ":memory:", f"SELECT * FROM {reader}({fixture.quote(path)});")
            wide = fixture.execute(executable, ":memory:", f"DESCRIBE SELECT * FROM read_parquet({fixture.quote(targets[4])});")
            self.assertEqual(len(wide), 40)
            self.assertEqual(wide[0]["column_name"], "Moment")
            self.assertEqual(wide[0]["column_type"], "TIMESTAMP")
            before = [path.read_bytes() for path in targets]
            original, calls = fixture.execute, 0
            def fail_export(*args, **kwargs):
                nonlocal calls
                calls += 1
                if calls == 3:
                    raise RuntimeError("file export failed")
                return original(*args, **kwargs)
            with mock.patch.object(fixture, "execute", side_effect=fail_export):
                with self.assertRaisesRegex(RuntimeError, "file export failed"):
                    fixture.create_source_opens(output)
            self.assertEqual([path.read_bytes() for path in targets], before)
            self.assertFalse(list(output.glob(".source-opens-*")))


if __name__ == "__main__":
    unittest.main()
