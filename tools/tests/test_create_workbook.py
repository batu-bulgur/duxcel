"""Finite synthetic inputs, real DuckDB conversion and failure publication."""
import csv
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
                self.assertEqual(records, [{"version": 12, "sheets": 4, "source": prefix + ".parquet"}])
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
    def test_identity_only_fixture_and_failed_publication(self):
        with tempfile.TemporaryDirectory(prefix="duxcel identity ' ") as directory:
            output = pathlib.Path(directory)
            target = fixture.create_identity(output)
            before = target.read_bytes()
            records = fixture.execute(shutil.which("duckdb"), target,
                "SELECT version,(SELECT count(*) FROM _duxcel_workspace.sheets) AS sheets,"
                "(SELECT text FROM _duxcel_workspace.cells WHERE row_id=1 AND column_id=1) AS value "
                "FROM _duxcel_workspace.meta")
            self.assertEqual(records, [{"version": 12, "sheets": 1, "value": "Legacy original"}])
            with mock.patch.object(fixture, "execute", side_effect=RuntimeError("candidate failed")):
                with self.assertRaisesRegex(RuntimeError, "candidate failed"):
                    fixture.create_identity(output, duckdb="existing-duckdb")
            self.assertEqual(target.read_bytes(), before)
            self.assertFalse(list(output.glob(".identity-*")))

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


if __name__ == "__main__":
    unittest.main()
