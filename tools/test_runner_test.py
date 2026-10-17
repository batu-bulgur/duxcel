#!/usr/bin/env python3
"""Runner checks without compiler, fixture writes, native windows or clipboard."""

import contextlib
import importlib.util
import io
import json
import pathlib
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location("duxcel_test_runner", pathlib.Path(__file__).with_name("test.py"))
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class RunnerTests(unittest.TestCase):
    def test_inventory(self):
        self.assertEqual(len(runner.NATIVE), 18)
        self.assertEqual(sum(row[1] for row in runner.NATIVE), 988)
        self.assertEqual(len({row[0] for row in runner.NATIVE}), 18)
        self.assertNotIn("core", runner.FAST_MODULES)
        self.assertNotIn("workbook", runner.FAST_MODULES)
        self.assertEqual(runner.configurations("both"), ("release", "sanitize"))
        self.assertEqual(runner.test_binary("grid", "sanitize"), "tests/test_grid-debug")
        runner.validate_native_contract(runner.ROOT)

    def test_focus_ranges_have_supported_bootstraps_and_terminal_assertions(self):
        source = (runner.ROOT / "src/app/app.c").read_text()
        for name, (start, end, marker) in runner.LAYOUT_PROFILES.items():
            with self.subTest(feature=name):
                self.assertGreater(end, start)
                self.assertLessEqual(end, 555)
                self.assertIn(marker, source)
                if start != 250:
                    self.assertIn(f'"{start}"', source)

    def test_success_requires_terminal_marker(self):
        with self.assertRaisesRegex(RuntimeError, "Missing completion"):
            runner.validate_output("PASS: an earlier assertion\n", 0, "PASS: final assertion")
        runner.validate_output("PASS: final assertion\n", 0, "PASS: final assertion")
        with self.assertRaisesRegex(RuntimeError, "status 1"):
            runner.validate_output("PASS: final assertion\n", 1, "PASS: final assertion")
        runner.validate_output("16812 grid checks\n", 0, completion_pattern=r"^\d+ grid checks$")
        with self.assertRaisesRegex(RuntimeError, "Missing completion summary"):
            runner.validate_output("", 0, completion_pattern=r"^\d+ grid checks$")

    def test_native_requires_complete_shutdown(self):
        marker = "PASS: final assertion"
        with self.assertRaisesRegex(RuntimeError, "shutdown summary"):
            runner.validate_output(marker + "\n", 0, marker, native=True)
        output = marker + "\nDuxcel frames=120 mean=17.0 ms smoke=0\n"
        runner.validate_output(output, 0, marker, native=True)
        with self.assertRaisesRegex(RuntimeError, "unfinished"):
            runner.validate_output(output + "Filter smoke unfinished at stage 5\n", 0, marker, native=True)

    def test_base_uses_actual_existing_state_instead_of_invented_pass(self):
        output = "Final relation rows=4 columns=2 cache offset=0\nDuxcel frames=240 mean=17.0 ms smoke=6\n"
        runner.validate_output(output, 0, native=True, base=True)
        for altered in (output.replace("smoke=6", "smoke=5"), output.replace("rows=4", "rows=3")):
            with self.assertRaises(RuntimeError):
                runner.validate_output(altered, 0, native=True, base=True)

    def test_sanitizer_diagnostics_fail_even_with_zero_exit(self):
        for diagnostic in ("ERROR: AddressSanitizer: heap-use-after-free", "file.c:1: runtime error: signed integer overflow"):
            with self.subTest(diagnostic=diagnostic), self.assertRaisesRegex(RuntimeError, "Sanitizer"):
                runner.validate_output("PASS: final\n" + diagnostic, 0, "PASS: final")

    def test_overwritten_or_removed_binary_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "binary"
            path.write_bytes(b"first build")
            before = runner.fingerprint([path])
            runner.ensure_unchanged(before)
            path.write_bytes(b"other build")
            with self.assertRaisesRegex(RuntimeError, "changed during"):
                runner.ensure_unchanged(before)
            path.unlink()
            with self.assertRaises(RuntimeError):
                runner.ensure_unchanged(before)

    def test_lock_exclusion_and_release_without_unlink(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "desktop.lock"
            with runner.FileLock(path, "Desktop"):
                metadata = json.loads(path.read_text())
                self.assertIn("pid", metadata)
                with self.assertRaisesRegex(RuntimeError, "already running"):
                    with runner.FileLock(path, "Desktop"):
                        self.fail("Second native runner acquired the same desktop")
            self.assertTrue(path.exists())
            with runner.FileLock(path, "Desktop"):
                pass

    def test_full_requires_optional_data_before_building(self):
        args = runner.parser().parse_args(["full", "--require-data"])
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "Makefile").write_text("")
            for name in ("src", "tests", "tools"):
                (root / name).mkdir()
            instance = runner.Runner(args, root)
            with mock.patch.object(instance, "headless") as headless:
                with self.assertRaisesRegex(RuntimeError, "NYC dataset missing"):
                    instance.full()
                headless.assert_not_called()

    def test_cli_validation_and_inventory_do_not_construct_runner(self):
        with mock.patch.object(runner, "Runner") as constructor:
            with contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(runner.main(["--list"]), 0)
            self.assertIn("988 application stages", output.getvalue())
            constructor.assert_not_called()
        for arguments in (["affected"], ["full", "--modules", "grid"], ["fast", "--jobs", "3"], ["fast", "--timeout", "0"], ["fast", "--feature", "shelves"], ["affected", "--feature", "shelves", "--require-data"]):
            with self.subTest(arguments=arguments), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                runner.main(arguments)


if __name__ == "__main__":
    unittest.main(verbosity=2)
