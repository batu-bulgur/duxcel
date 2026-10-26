import contextlib
import ast
import io
from pathlib import Path
import re
import shlex
import unittest
from unittest import mock
import test as runner
from test_catalog import FEATURES, STANDALONE_GUI, plan


class FeatureCatalogTests(unittest.TestCase):
    def test_literal_gui_completion_contracts_match_their_consumers(self):
        # Catch unrelated stage-count edits before compiling/launching the App.
        # Dynamic summaries still require their existing runtime gates.
        messages = []
        for folder in (runner.ROOT / "src", runner.ROOT / "tests"):
            for path in folder.rglob("*"):
                if path.suffix not in (".c", ".inc"):
                    continue
                for raw in re.findall(r'"(PASS: (?:[^"\\]|\\.)*)"', path.read_text()):
                    message = ast.literal_eval('"' + raw + '"').strip()
                    stages = re.search(r'\((\d+) stages,', message)
                    if stages:
                        messages.append((path, message, int(stages.group(1))))
        checked = 0
        for name, (_, marker, completion, expected) in STANDALONE_GUI.items():
            for path, message, stages in messages:
                if not message.startswith(marker + " ("):
                    continue
                checked += 1
                with self.subTest(feature=name, consumer=str(path.relative_to(runner.ROOT))):
                    self.assertEqual(stages, expected)
                    rendered = re.sub(r'%[lz]*[udi]', '1', message)
                    self.assertRegex(rendered, completion)
        self.assertGreater(checked, 0)

    def test_dependencies_are_small_and_registered(self):
        for name, feature in FEATURES.items():
            self.assertTrue(set(feature.modules) <= set(runner.FAST_MODULES), name)
            self.assertLess(len(feature.modules), 5, name)
            if feature.gui and feature.gui != "about" and feature.gui not in STANDALONE_GUI:
                self.assertIn(feature.gui, runner.LAYOUT_PROFILES)
            self.assertTrue(feature.interactions)
            for command in feature.interactions:
                parts = shlex.split(command)
                script = parts[1]
                self.assertTrue(runner.ROOT.joinpath(script).is_file(), script)
                if script == "tools/test.py" and parts[2] == "feature":
                    self.assertIn(parts[parts.index("--feature") + 1], FEATURES, command)

    def test_plan_launches_nothing(self):
        output = io.StringIO()
        with mock.patch.object(runner, "Runner") as fake, contextlib.redirect_stdout(output):
            self.assertEqual(runner.main(["feature", "--feature", "containing-regions", "--plan"]), 0)
        fake.assert_not_called()
        self.assertIn('"deferred_interactions"', output.getvalue())
        self.assertNotIn('"grid"', output.getvalue())

    def test_local_selection_does_not_expand_to_full_fast_suite(self):
        obj = runner.Runner.__new__(runner.Runner)
        obj.args = runner.parser().parse_args(["feature", "--feature", "containing-regions"])
        obj.summary = {}; obj.log_dir = Path("build/tests/results/feature-contract")
        obj.headless = mock.Mock(); obj.prepare_native = mock.Mock()
        obj.record = mock.Mock(); obj.command = mock.Mock(); obj.native_binary = mock.Mock(return_value="build/bin/duxcel")
        with mock.patch.object(runner, "FileLock", return_value=contextlib.nullcontext()), contextlib.redirect_stdout(io.StringIO()):
            obj.feature()
        obj.headless.assert_called_once_with(["cell_regions", "selection_ladder", "selection"])
        obj.prepare_native.assert_called_once_with(background=False)
        obj.native_binary.assert_called_once()
        self.assertEqual(obj.command.call_count, 1)
        self.assertEqual(obj.args.config, "both")
        self.assertEqual(obj.summary["feature_plan"]["deferred_interactions"], plan("containing-regions")["deferred_interactions"])

    def test_command_gui_has_no_canonical_or_background_dependency(self):
        obj = runner.Runner.__new__(runner.Runner)
        obj.args = runner.parser().parse_args(["feature", "--feature", "cell-command"])
        obj.summary = {}; obj.headless = mock.Mock(); obj.standalone_gui_check = mock.Mock()
        obj.prepare_native = mock.Mock(side_effect=AssertionError("Unrelated native bootstrap"))
        with contextlib.redirect_stdout(io.StringIO()):
            obj.feature()
        obj.headless.assert_called_once_with(["cell_command"])
        obj.standalone_gui_check.assert_called_once_with("cell-command")
        obj.prepare_native.assert_not_called()

    def test_identity_gate_keeps_engine_separate_and_gui_narrow(self):
        obj = runner.Runner.__new__(runner.Runner)
        obj.args = runner.parser().parse_args(["feature", "--feature", "workbook-identity"])
        obj.summary = {}; obj.headless = mock.Mock(); obj.feature_engine_checks = mock.Mock()
        obj.standalone_gui_check = mock.Mock()
        obj.prepare_native = mock.Mock(side_effect=AssertionError("Unrelated canonical bootstrap"))
        with contextlib.redirect_stdout(io.StringIO()):
            obj.feature()
        obj.headless.assert_called_once_with(["workbook_identity", "cell_command", "workbook_codec"])
        obj.feature_engine_checks.assert_called_once_with(["workbook_identity_native"])
        obj.standalone_gui_check.assert_called_once_with("workbook-identity")
        obj.prepare_native.assert_not_called()

    def test_engine_variants_finish_serially(self):
        obj = runner.Runner.__new__(runner.Runner)
        obj.args = runner.parser().parse_args(["feature", "--feature", "workbook-identity"])
        obj.summary = {"binaries": {}}; events = []
        obj.build = mock.Mock(); obj.retain_binaries = mock.Mock()
        obj.command = lambda name, argv, marker: events.append(name) or {"name": name}
        obj.record = lambda result: events.append("completed:" + result["name"])
        with mock.patch.object(runner, "ensure_unchanged"):
            obj.feature_engine_checks(["workbook_identity_native"])
        self.assertEqual(events, ["workbook_identity_native-release", "completed:workbook_identity_native-release",
                                  "workbook_identity_native-sanitize", "completed:workbook_identity_native-sanitize"])

    def test_history_gate_avoids_engine_and_canonical_bootstrap(self):
        obj = runner.Runner.__new__(runner.Runner)
        obj.args = runner.parser().parse_args(["feature", "--feature", "workbook-history"])
        obj.summary = {}; obj.headless = mock.Mock(); obj.standalone_gui_check = mock.Mock()
        obj.prepare_native = mock.Mock(side_effect=AssertionError("Unrelated native bootstrap"))
        obj.feature_engine_checks = mock.Mock(side_effect=AssertionError("Unrelated archive matrix"))
        with contextlib.redirect_stdout(io.StringIO()):
            obj.feature()
        obj.headless.assert_called_once_with(["workbook_history", "cell_command", "workbook_identity"])
        obj.standalone_gui_check.assert_called_once_with("workbook-history")
        obj.prepare_native.assert_not_called(); obj.feature_engine_checks.assert_not_called()

    def test_lifecycle_gate_keeps_history_interactions_deferred(self):
        local = plan("workbook-lifecycle")
        self.assertEqual(local["modules"], ["workbook_lifecycle", "workbook_history", "cell_command"])
        self.assertEqual(local["gui"], "workbook-lifecycle")
        self.assertEqual(local["engine_modules"], [])
        self.assertIn("python3 tools/test.py feature --feature workbook-history", local["deferred_interactions"])

    def test_journal_plan_has_no_app_or_fixture_bootstrap(self):
        local = plan("workbook-journal")
        self.assertEqual(local["modules"], [])
        self.assertIsNone(local["gui"])
        self.assertEqual(local["engine_modules"], ["workbook_journal"])
        self.assertEqual(local["engine_process_checks"], ["tools/test_journal_process.py"])

    def test_journal_process_barriers_use_each_already_built_variant_serially(self):
        obj = runner.Runner.__new__(runner.Runner)
        obj.args = runner.parser().parse_args(["feature", "--feature", "workbook-journal"])
        obj.summary = {"binaries": {}}; events = []
        obj.build = mock.Mock(); obj.retain_binaries = mock.Mock()
        obj.command = lambda name, argv, marker, **kwargs: events.append((name, argv, kwargs)) or {"name": name}
        obj.record = mock.Mock()
        with mock.patch.object(runner, "ensure_unchanged"):
            obj.feature_engine_checks(["workbook_journal"])
        self.assertEqual([event[0] for event in events], [
            "workbook_journal-release", "workbook_journal-release-process",
            "workbook_journal-sanitize", "workbook_journal-sanitize-process"])
        self.assertEqual(events[1][1][3], "build/tests/test_workbook_journal")
        self.assertEqual(events[3][1][3], "build/tests/test_workbook_journal-debug")
        self.assertIn("completion_pattern", events[1][2])

    def test_queue_gate_has_no_desktop_or_canonical_fixture(self):
        local = plan("journal-queue")
        self.assertEqual(local["modules"], [])
        self.assertIsNone(local["gui"])
        self.assertEqual(local["engine_modules"], ["journal_queue", "workbook_journal"])
        self.assertEqual(local["engine_process_checks"], ["tools/test_journal_faults.py"])

    def test_checkpoint_gate_admits_only_its_modules_and_private_gui(self):
        local = plan("journal-session")
        self.assertEqual(local["modules"], ["cell_command"])
        self.assertEqual(local["gui"], "journal-session")
        self.assertEqual(local["engine_modules"], ["journal_session", "journal_queue", "workbook_journal"])
        self.assertEqual(local["engine_process_checks"], [])
        self.assertEqual(local["process_support_targets"], [])
        self.assertIn("python3 tools/test.py feature --feature journal-faults", local["deferred_interactions"])

    def test_fault_gate_builds_and_fingerprints_private_support(self):
        local = plan("journal-faults")
        self.assertIsNone(local["gui"])
        self.assertEqual(local["engine_modules"], ["workbook_journal", "journal_queue"])
        self.assertEqual(local["process_support_targets"], ["build/tests/libjournal_fault.dylib"])
        obj = runner.Runner.__new__(runner.Runner)
        obj.args = runner.parser().parse_args(["feature", "--feature", "journal-faults", "--config", "release"])
        obj.summary = {"binaries": {}}; obj.build = mock.Mock(); obj.retain_binaries = mock.Mock()
        obj.command = mock.Mock(return_value={}); obj.record = mock.Mock()
        with mock.patch.object(runner, "ensure_unchanged"):
            obj.feature_engine_checks(["workbook_journal", "journal_queue"])
        expected = ["build/tests/test_workbook_journal", "build/tests/test_journal_queue", "build/tests/libjournal_fault.dylib"]
        obj.build.assert_called_once_with(expected, "build-feature-engine")
        obj.retain_binaries.assert_called_once_with(expected)

    def test_about_does_not_build_full_app_or_canonical_fixture(self):
        self.assertEqual(FEATURES["about"].modules, ("build_info",))
        self.assertEqual(plan("about")["gui"], "about")
        self.assertIn("tools.tests.test_build_info", plan("about")["tool_tests"])
