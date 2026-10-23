import contextlib
import io
from pathlib import Path
import shlex
import unittest
from unittest import mock
import test as runner
from test_catalog import FEATURES, STANDALONE_GUI, plan


class FeatureCatalogTests(unittest.TestCase):
    def test_dependencies_are_small_and_registered(self):
        for name, feature in FEATURES.items():
            self.assertTrue(set(feature.modules) <= set(runner.FAST_MODULES), name)
            self.assertLess(len(feature.modules), 5, name)
            if feature.gui and feature.gui != "about" and feature.gui not in STANDALONE_GUI:
                self.assertIn(feature.gui, runner.LAYOUT_PROFILES)
            self.assertTrue(feature.interactions)
            for command in feature.interactions:
                script = shlex.split(command)[1]
                self.assertTrue(runner.ROOT.joinpath(script).is_file(), script)

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
        self.assertEqual(events[1][1][-1], "build/tests/test_workbook_journal")
        self.assertEqual(events[3][1][-1], "build/tests/test_workbook_journal-debug")
        self.assertIn("completion_pattern", events[1][2])

    def test_about_does_not_build_full_app_or_canonical_fixture(self):
        self.assertEqual(FEATURES["about"].modules, ("build_info",))
        self.assertEqual(plan("about")["gui"], "about")
        self.assertIn("tools.tests.test_build_info", plan("about")["tool_tests"])
