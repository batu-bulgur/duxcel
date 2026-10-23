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

    def test_about_does_not_build_full_app_or_canonical_fixture(self):
        self.assertEqual(FEATURES["about"].modules, ("build_info",))
        self.assertEqual(plan("about")["gui"], "about")
        self.assertIn("tools.tests.test_build_info", plan("about")["tool_tests"])
