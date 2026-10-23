import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import benchmark_journal as benchmark
import test as runner
from test_catalog import plan


class JournalBenchmarkTests(unittest.TestCase):
    def test_workload_is_owned_by_dedicated_plan(self):
        local = plan("journal-performance")
        self.assertIsNone(local["gui"])
        self.assertEqual(local["fixtures"], ["journal-paste"])
        self.assertEqual(local["engine_modules"], ["journal_performance"])
        self.assertEqual(plan("journal-session")["fixtures"], [])

    def test_wrapper_runs_each_variant_once_and_keeps_reports_separate(self):
        obj = runner.Runner.__new__(runner.Runner)
        obj.args = runner.parser().parse_args(["feature", "--feature", "journal-performance"])
        obj.log_dir = Path("build/tests/results/benchmark-contract")
        obj.summary = {"binaries": {}}
        obj.build = mock.Mock(); obj.retain_binaries = mock.Mock(); obj.record = mock.Mock()
        obj.command = mock.Mock(return_value={})
        with mock.patch.object(runner, "ensure_unchanged"):
            obj.feature_engine_checks(["journal_performance"])
        self.assertEqual(obj.command.call_count, 2)
        commands = [call.args[1] for call in obj.command.call_args_list]
        self.assertTrue(all(command[1] == "tools/benchmark_journal.py" for command in commands))
        self.assertIn("build/tests/test_journal_performance", commands[0])
        self.assertIn("build/tests/test_journal_performance-debug", commands[1])
        self.assertNotEqual(commands[0][-1], commands[1][-1])

    def test_no_build_requires_existing_finite_input(self):
        obj = runner.Runner.__new__(runner.Runner)
        obj.args = runner.parser().parse_args(["feature", "--feature", "journal-performance", "--no-build"])
        with tempfile.TemporaryDirectory() as directory:
            obj.root = Path(directory)
            obj.command = mock.Mock()
            with self.assertRaisesRegex(RuntimeError, "Missing feature fixture"):
                obj.prepare_feature_fixtures(["journal-paste"])
            obj.command.assert_not_called()

    def test_runtime_library_mismatch_refuses_measurement_report(self):
        output = "ENGINE_VERSION v1\nENGINE_LIBRARY /wrong/library\nMETRICS {}\nPASS: journal performance (1 checks)\n"
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "metrics.json"
            argv = ["benchmark_journal.py", "--binary", "binary", "--input", "fixture", "--output", str(report)]
            with mock.patch("sys.argv", argv), mock.patch.object(benchmark, "linked_engine", return_value=Path("/expected/library")), \
                 mock.patch.object(benchmark, "fingerprint", return_value={}), \
                 mock.patch.object(benchmark.subprocess, "run", return_value=mock.Mock(returncode=0, stdout=output, stderr="")), \
                 contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(RuntimeError, "provenance"):
                    benchmark.main()
            self.assertFalse(report.exists())
