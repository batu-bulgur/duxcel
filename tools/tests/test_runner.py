#!/usr/bin/env python3
"""Runner checks without compiler, shared fixture writes, windows or clipboard."""

import contextlib
import importlib.util
import io
import json
import pathlib
import os
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location("duxcel_test_runner", pathlib.Path(__file__).resolve().parents[1].joinpath("test.py"))
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class RunnerTests(unittest.TestCase):
    def test_recursive_make_inspection_modes_do_not_launch_tests(self):
        for flags,result in (("n -j8",0),("t",0),("q",1)):
            with mock.patch.dict(os.environ,{"MAKEFLAGS":flags,"MAKELEVEL":"1"}), mock.patch.object(runner,"Runner") as build, contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(runner.main(["feature","--feature","recovery-ui"]),result)
                build.assert_not_called()

    def test_jobserver_borrows_only_live_pipe_descriptors(self):
        self.assertEqual(runner.make_jobserver_fds({"MAKEFLAGS":" -j8"}), ())
        read,write=os.pipe()
        try:
            for option in ("fds", "auth"):
                self.assertEqual(runner.make_jobserver_fds({"MAKEFLAGS":f" -j --jobserver-{option}={read},{write}"}), (read,write))
            self.assertEqual(runner.make_jobserver_fds({"MAKEFLAGS":f"--jobserver-fds={read},{read}"}), ())
            with tempfile.TemporaryFile() as regular:
                self.assertEqual(runner.make_jobserver_fds({"MAKEFLAGS":f"--jobserver-fds={regular.fileno()},{write}"}), ())
        finally:
            os.close(read); os.close(write)
        self.assertEqual(runner.make_jobserver_fds({"MAKEFLAGS":f"--jobserver-fds={read},{write}"}), ())

    def test_web_runtime_inputs_are_fingerprinted_without_generated_dependencies(self):
        with tempfile.TemporaryDirectory() as directory:
            root=pathlib.Path(directory);(root/'Makefile').write_text('test:\n')
            web=root/'web';web.mkdir();runtime=web/'host.mjs';runtime.write_text('first')
            (web/'package-lock.json').write_text('{}')
            ignored=web/'node_modules';ignored.mkdir();dependency=ignored/'large.js';dependency.write_text('first')
            first=runner.source_metadata(root)
            dependency.write_text('changed generated dependency')
            self.assertEqual(first['source_sha256'],runner.source_metadata(root)['source_sha256'])
            runtime.write_text('changed runtime')
            second=runner.source_metadata(root)
            self.assertNotEqual(first['source_sha256'],second['source_sha256'])
            (web/'package-lock.json').write_text('{"changed":true}')
            self.assertNotEqual(second['source_sha256'],runner.source_metadata(root)['source_sha256'])

    def test_inventory(self):
        self.assertEqual(len(runner.NATIVE), 18)
        self.assertEqual(sum(row[1] for row in runner.NATIVE), 1390)
        self.assertEqual(len({row[0] for row in runner.NATIVE}), 18)
        self.assertNotIn("core", runner.FAST_MODULES)
        self.assertNotIn("workbook", runner.FAST_MODULES)
        self.assertEqual(runner.RECORD_MODULES, ("record_sql", "record_pages"))
        self.assertEqual(runner.ENGINE_MODULES[:2], ("core", "workbook"))
        self.assertTrue(set(runner.RECORD_MODULES).isdisjoint(runner.FAST_MODULES))
        self.assertEqual(runner.configurations("both"), ("release", "sanitize"))
        self.assertEqual(runner.test_binary("grid", "sanitize"), "build/tests/test_grid-debug")
        runner.validate_native_contract(runner.ROOT)

    def test_authored_fixtures_change_provenance_but_generated_files_do_not(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory); (root / "Makefile").write_text("")
            fixtures = root / "tests/fixtures/ui"; fixtures.mkdir(parents=True)
            values = fixtures.parent / "null.csv"; values.write_text('value\nNULL\n')
            geometry = fixtures / "rail.json"; geometry.write_text('{"width":240}')
            generated = root / "build/tests/generated/input.csv"; generated.parent.mkdir(parents=True)
            generated.write_text("old")
            first = runner.source_metadata(root)["source_sha256"]
            generated.write_text("new")
            self.assertEqual(first, runner.source_metadata(root)["source_sha256"])
            values.write_text('value\n""\n')
            second = runner.source_metadata(root)["source_sha256"]
            self.assertNotEqual(first, second)
            geometry.write_text('{"width":280}')
            self.assertNotEqual(second, runner.source_metadata(root)["source_sha256"])

    def test_focus_ranges_have_supported_bootstraps_and_terminal_assertions(self):
        source = (runner.ROOT / "src/app/app.c").read_text()
        for name, (start, end, marker) in runner.LAYOUT_PROFILES.items():
            with self.subTest(feature=name):
                self.assertGreater(end, start)
                self.assertLessEqual(end, 950)
                self.assertIn(marker, source)
                if start in (687, 730, 734, 764, 800, 824, 846, 861, 867, 882):
                    self.assertIn(f'!strcmp(layout_from,"{start}")', source)
                    self.assertIn('if (focused_layout && (layout_start==687 || layout_start==730 || layout_start==734 || layout_start==764 || layout_start==800 || layout_start==824 || layout_start==846 || layout_start==861 || layout_start==867 || layout_start==882))', source)
                    self.assertIn('session->layout_smoke=focused_layout ? (layout_start==687 || layout_start==730 || layout_start==734 || layout_start==764 || layout_start==800 || layout_start==824 || layout_start==846 || layout_start==861 || layout_start==867 || layout_start==882 ? layout_start : 250) : 0;', source)
                elif start not in (0, 250):
                    self.assertIn(f'"{start}"', source)
                    self.assertIn(f'if (!strcmp(getenv("DUXCEL_LAYOUT_FROM"),"{start}")) {{ *step={start}; return; }}', source)

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

    def test_layout_completion_requires_the_exact_runtime_range(self):
        marker = "PASS: final assertion"
        pattern = runner.layout_completion_pattern(638, 665)
        shutdown = "\nDuxcel frames=120 mean=17.0 ms smoke=0\n"
        runner.validate_output(marker + "\nLayout smoke completed [638,665)" + shutdown,
                               0, marker, native=True, completion_pattern=pattern)
        bad_lines = ("", "Layout smoke completed [638,660)", "Layout smoke completed [637,665)",
                     "INFO: Layout smoke completed [638,665)", " Layout smoke completed [638,665)",
                     "Layout smoke completed [638,665) ", "Layout smoke completed [638,665) extra",
                     "Layout smoke completed [638, 665)", "Layout smoke completed [0638,665)",
                     "Layout smoke completed [638,665]")
        for line in bad_lines:
            with self.subTest(line=line), self.assertRaisesRegex(RuntimeError, "Missing completion summary"):
                runner.validate_output(marker + "\n" + line + shutdown, 0, marker,
                                       native=True, completion_pattern=pattern)
        # A genuine range line still cannot substitute for normal lifecycle
        # checks or erase a later failed/unfinished assertion.
        with self.assertRaisesRegex(RuntimeError, "unfinished"):
            runner.validate_output(marker + "\nLayout smoke completed [638,665)\nLayout smoke unfinished at stage 660" + shutdown,
                                   0, marker, native=True, completion_pattern=pattern)

    def test_affected_no_build_rejects_legacy_same_pass_and_wrong_range(self):
        feature = "ribbon-overflow"
        start, end, marker = runner.LAYOUT_PROFILES[feature]
        valid = f"Layout smoke completed [{start},{end})\n"
        for completion in ("", f"Layout smoke completed [{start},{end-5})\n", valid):
            with self.subTest(completion=completion), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory)
                (root / "Makefile").write_text("")
                args = runner.parser().parse_args(["affected", "--feature", feature, "--no-build"])
                instance = runner.Runner(args, root)

                def child(command, **kwargs):
                    self.assertIn("--layout-smoke", command)
                    self.assertEqual(kwargs["env"]["DUXCEL_LAYOUT_FROM"], str(start))
                    self.assertEqual(kwargs["env"]["DUXCEL_LAYOUT_TO"], str(end))
                    kwargs["stdout"].write(marker + "\n" + completion + "Duxcel frames=120 mean=17.0 ms smoke=0\n")
                    return mock.Mock(returncode=0)

                with mock.patch.object(instance, "headless"), mock.patch.object(instance, "prepare_native"), \
                     mock.patch.object(instance, "background_check"), mock.patch.object(runner, "FileLock"), \
                     mock.patch.object(runner.subprocess, "run", side_effect=child), contextlib.redirect_stdout(io.StringIO()):
                    if completion == valid:
                        instance.affected()
                    else:
                        with self.assertRaisesRegex(RuntimeError, "Missing completion summary"):
                            instance.affected()
                result = instance.summary["results"][-1]
                self.assertEqual(result["status"], "passed" if completion == valid else "failed")
                self.assertEqual(result["completion_pattern"], runner.layout_completion_pattern(start, end))

    def test_full_and_named_layout_require_the_entire_inventory_range(self):
        args = runner.parser().parse_args(["native", "--scenario", "layout", "icon", "--no-build"])
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "Makefile").write_text("")
            instance = runner.Runner(args, root)

            def completed(name, command, marker=None, **kwargs):
                return {"name": name, "command": command, "status": "passed", "expected_stages": kwargs["stages"]}

            with mock.patch.object(instance, "command", side_effect=completed) as command:
                # full() uses this same serial scenario dispatcher.
                instance.native_scenarios(["layout", "icon"])
            stages = next(row[1] for row in runner.NATIVE if row[0] == "layout")
            self.assertEqual(command.call_args_list[0].kwargs["completion_pattern"], runner.layout_completion_pattern(0, stages))
            self.assertIsNone(command.call_args_list[1].kwargs["completion_pattern"])

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

    def test_native_visibility_is_explicit_and_not_inherited(self):
        for visible in (False, True):
            with self.subTest(visible=visible), tempfile.TemporaryDirectory() as directory:
                root=pathlib.Path(directory)
                (root / "Makefile").write_text("")
                args=runner.parser().parse_args(["native", "--scenario", "icon"] + (["--visible"] if visible else []))
                instance=runner.Runner(args,root)
                def child(command, **kwargs):
                    self.assertEqual(kwargs["env"]["DUXCEL_TEST_VISIBLE"], "1" if visible else "0")
                    self.assertEqual(pathlib.Path(kwargs["env"]["DUXCEL_TEST_RECOVERY_ROOT"]),instance.log_dir / "hidden-native-recovery")
                    self.assertTrue(pathlib.Path(kwargs["env"]["DUXCEL_TEST_RECOVERY_ROOT"]).is_dir())
                    kwargs["stdout"].write("PASS: final\nDuxcel frames=10 mean=1.0 ms smoke=0\n")
                    return mock.Mock(returncode=0)
                with mock.patch.dict(runner.os.environ,{"DUXCEL_TEST_VISIBLE":"1","DUXCEL_TEST_RECOVERY_ROOT":"/must-not-inherit"}), mock.patch.object(runner.subprocess,"run",side_effect=child), contextlib.redirect_stdout(io.StringIO()):
                    result=instance.command("hidden-native",["test-owned-process"],"PASS: final",native=True)
                self.assertEqual(result["status"],"passed")
                self.assertEqual(instance.summary["native_visibility"],"visible" if visible else "hidden")

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

    def test_records_tier_builds_retains_and_runs_only_selected_configs_sequentially(self):
        for config, no_build in (("release", False), ("sanitize", False), ("both", False), ("both", True)):
            with self.subTest(config=config, no_build=no_build), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory); (root / "Makefile").write_text("")
                arguments = ["records", "--config", config] + (["--no-build"] if no_build else [])
                instance = runner.Runner(runner.parser().parse_args(arguments), root)
                binaries = [runner.test_binary(module, build) for build in runner.configurations(config)
                            for module in runner.RECORD_MODULES]
                def create_binaries(targets):
                    for binary in targets:
                        path = root / binary; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(binary)
                if no_build:
                    create_binaries(binaries)
                def completed(name, command, marker=None, **kwargs):
                    if command[0] == "make":
                        self.assertEqual(command, ["make", *binaries]); create_binaries(command[1:])
                    else:
                        self.assertEqual(marker, "PASS:"); self.assertFalse(kwargs.get("native"))
                    return {"name": name, "command": command, "status": "passed"}
                with mock.patch.object(instance, "command", side_effect=completed) as command, \
                     mock.patch.object(runner, "native_lock_path", side_effect=AssertionError("No records desktop")), \
                     contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(instance.run(), 0)
                expected = ([] if no_build else [["make", *binaries]]) + [["./" + binary] for binary in binaries]
                self.assertEqual([call.args[1] for call in command.call_args_list], expected)
                self.assertEqual(instance.summary["binaries"], runner.fingerprint([root / binary for binary in binaries]))
                self.assertEqual(instance.summary["binaries_after"], instance.summary["binaries"])
                saved = json.loads((instance.log_dir / "summary.json").read_text())
                self.assertEqual(saved["tier"], "records"); self.assertEqual(saved["status"], "passed")
                self.assertEqual(saved["source"]["source_sha256"], runner.source_metadata(root)["source_sha256"])
                self.assertTrue((root / "build/locks/test-runner.lock").exists())

    def test_records_no_build_requires_all_binaries_before_any_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory); (root / "Makefile").write_text("")
            binary = root / "build/tests/test_record_sql"; binary.parent.mkdir(parents=True); binary.write_text("existing")
            args = runner.parser().parse_args(["records", "--config", "release", "--no-build"])
            instance = runner.Runner(args, root)
            with mock.patch.object(instance, "command") as command, self.assertRaisesRegex(RuntimeError, "Missing test binary.*test_record_pages"):
                instance.records()
            command.assert_not_called()

    def test_records_stop_after_failure_or_binary_replacement(self):
        for overwrite in (False, True):
            with self.subTest(overwrite=overwrite), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory); (root / "Makefile").write_text("")
                for module in runner.RECORD_MODULES:
                    binary = root / runner.test_binary(module, "release"); binary.parent.mkdir(parents=True, exist_ok=True); binary.write_text(module)
                args = runner.parser().parse_args(["records", "--config", "release", "--no-build"])
                instance = runner.Runner(args, root)
                def completed(name, command, marker=None, **kwargs):
                    if overwrite:
                        (root / "build/tests/test_record_pages").write_text("different build")
                    return {"name": name, "command": command, "status": "passed" if overwrite else "failed", "error": "Engine assertion failed"}
                with mock.patch.object(instance, "command", side_effect=completed) as command, \
                     contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(instance.run(), 1)
                self.assertEqual(command.call_count, 1)
                self.assertEqual(instance.summary["status"], "failed")
                self.assertIn("changed" if overwrite else "Engine assertion failed", instance.summary["error"])

    def test_full_builds_record_suites_without_interrupting_core_workbook_fixture_order(self):
        args = runner.parser().parse_args(["full", "--config", "both", "--jobs", "2"])
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory); (root / "Makefile").write_text("")
            instance = runner.Runner(args, root)
            def completed(name, command, marker=None, **kwargs):
                if command[0] == "make":
                    for target in command[1:]:
                        if target.startswith(("build/tests/", "build/bin/duxcel")):
                            path = root / target; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(target)
                    fixture = root / "build/tests/generated/design-definition.dxls"
                    fixture.parent.mkdir(parents=True); fixture.write_text("fixture")
                return {"name": name, "command": command, "status": "passed"}
            with mock.patch.object(instance, "command", side_effect=completed) as command, \
                 mock.patch.object(instance, "headless"), mock.patch.object(instance, "background_check"), \
                 mock.patch.object(instance, "about_check") as about_check, \
                 mock.patch.object(instance, "standalone_gui_check") as standalone_gui_check, \
                 mock.patch.object(instance, "native_scenarios"), mock.patch.object(runner, "validate_native_contract"), \
                 mock.patch.object(runner, "native_lock_path", return_value=root / "desktop.lock"):
                instance.full()
            about_check.assert_called_once_with()
            self.assertEqual(standalone_gui_check.call_args_list, [mock.call(name) for name in runner.STANDALONE_GUI])
            engines = [runner.test_binary(module, config) for config in ("release", "sanitize")
                       for module in runner.ENGINE_MODULES]
            self.assertTrue(set(engines).issubset(command.call_args_list[0].args[1]))
            expected=[[runner.sys.executable, "tools/create_workbook.py", "--journal-paste-only"]]
            for binary in engines:
                module=pathlib.Path(binary).name.removeprefix("test_").removesuffix("-debug")
                check=runner.ENGINE_PROCESS_CHECKS.get(module)
                if not check or not check.primary:
                    expected.append(["./"+binary])
                if 'test_workbook_codec_native' in binary:
                    expected.append(["./"+binary,"--sorts-only"])
                    expected.append(["./"+binary,"--preferences-only"])
                    expected.append(["./"+binary,"--objects-only"])
                    expected.append(["./"+binary,"--queries-only"])
                    expected.append(["./"+binary,"--source-options-only"])
                    expected.append(["./"+binary,"--tables-only"])
                    expected.append(["./"+binary,"--pivot-marks-only"])
                    expected.append(["./"+binary,"--filters-only"])
                    expected.append(["./"+binary,"--grid-only"])
                    expected.append(["./"+binary,"--counters-only"])
                    expected.append(["./"+binary,"--source-counters-only"])
                    expected.append(["./"+binary,"--source-versions-only"])
                    expected.append(["./"+binary,"--source-bindings-only"])
                    expected.append(["./"+binary,"--format-rows-only"])
                module=pathlib.Path(binary).name.removeprefix("test_").removesuffix("-debug")
                if module in runner.ENGINE_PROCESS_CHECKS:
                    check=runner.ENGINE_PROCESS_CHECKS[module]
                    config="sanitize" if binary.endswith("-debug") else "release"
                    extra=["--output",str(instance.log_dir / f"{module}-{config}-metrics.json")] if check.primary else []
                    expected.append([runner.sys.executable,check.script,"--binary",binary,*check.arguments,*extra])
            self.assertEqual([call.args[1] for call in command.call_args_list[1:-1]],expected)

    def test_records_cli_rejects_unrelated_or_parallel_engine_options_before_runner_creation(self):
        extras = (["--modules", "grid"], ["--scenario", "icon"], ["--feature", "shelves"],
                  ["--visible"], ["--require-data"], ["--jobs", "2"])
        with mock.patch.object(runner, "Runner") as constructor:
            for extra in extras:
                with self.subTest(extra=extra), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    runner.main(["records", *extra])
            constructor.assert_not_called()
            constructor.return_value.run.return_value = 0
            self.assertEqual(runner.main(["records", "--config", "sanitize"]), 0)
            args = constructor.call_args.args[0]
            self.assertEqual((args.tier, args.config, args.jobs), ("records", "sanitize", 1))

    def test_formats_tier_builds_retains_and_runs_only_selected_configs_sequentially(self):
        for config in ("release", "sanitize", "both"):
            for no_build in (False, True):
                with self.subTest(config=config, no_build=no_build), tempfile.TemporaryDirectory() as directory:
                    root = pathlib.Path(directory); (root / "Makefile").write_text("")
                    arguments = ["formats", "--config", config] + (["--no-build"] if no_build else [])
                    instance = runner.Runner(runner.parser().parse_args(arguments), root)
                    binaries = [runner.test_binary("workbook", build) for build in runner.configurations(config)]

                    def create_binaries(targets):
                        for binary in targets:
                            path = root / binary; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(binary)

                    if no_build:
                        create_binaries(binaries)

                    def completed(name, command, marker=None, **kwargs):
                        if command[0] == "make":
                            self.assertEqual(command, ["make", *binaries]); create_binaries(command[1:])
                        else:
                            self.assertEqual(marker, "PASS:"); self.assertFalse(kwargs.get("native"))
                            self.assertEqual(kwargs["completion_pattern"], runner.FORMATS_COMPLETION_PATTERN)
                            self.assertTrue((root / "build/tests/generated").is_dir())
                        return {"name": name, "command": command, "status": "passed"}

                    with mock.patch.object(instance, "command", side_effect=completed) as command, \
                         mock.patch.object(instance, "headless", side_effect=AssertionError("No broad modules")), \
                         mock.patch.object(instance, "prepare_native", side_effect=AssertionError("No native preparation")), \
                         mock.patch.object(runner, "native_lock_path", side_effect=AssertionError("No formats desktop")), \
                         contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(instance.run(), 0)
                    expected = ([] if no_build else [["make", *binaries]]) + [["./" + binary, "--formats-only"] for binary in binaries]
                    self.assertEqual([call.args[1] for call in command.call_args_list], expected)
                    self.assertEqual(instance.summary["binaries"], runner.fingerprint([root / binary for binary in binaries]))
                    self.assertEqual(instance.summary["binaries_after"], instance.summary["binaries"])
                    saved = json.loads((instance.log_dir / "summary.json").read_text())
                    self.assertEqual(saved["tier"], "formats"); self.assertEqual(saved["status"], "passed")
                    self.assertEqual(saved["source"]["source_sha256"], runner.source_metadata(root)["source_sha256"])
                    self.assertTrue((root / "build/locks/test-runner.lock").exists())
                    self.assertFalse((root / "build/tests/generated/million.parquet").exists())

    def test_formats_no_build_requires_all_selected_binaries_before_any_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory); (root / "Makefile").write_text("")
            binary = root / "build/tests/test_workbook"; binary.parent.mkdir(parents=True); binary.write_text("existing")
            instance = runner.Runner(runner.parser().parse_args(["formats", "--config", "both", "--no-build"]), root)
            with mock.patch.object(instance, "command") as command, self.assertRaisesRegex(RuntimeError, "Missing test binary.*test_workbook-debug"):
                instance.formats()
            command.assert_not_called()

    def test_formats_stop_after_failure_or_binary_replacement(self):
        for overwrite in (False, True):
            with self.subTest(overwrite=overwrite), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory); (root / "Makefile").write_text("")
                for config in ("release", "sanitize"):
                    binary = root / runner.test_binary("workbook", config); binary.parent.mkdir(parents=True, exist_ok=True); binary.write_text(config)
                instance = runner.Runner(runner.parser().parse_args(["formats", "--config", "both", "--no-build"]), root)

                def completed(name, command, marker=None, **kwargs):
                    if overwrite:
                        (root / "build/tests/test_workbook-debug").write_text("different build")
                    return {"name": name, "command": command, "status": "passed" if overwrite else "failed", "error": "Format save assertion failed"}

                with mock.patch.object(instance, "command", side_effect=completed) as command, \
                     contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(instance.run(), 1)
                self.assertEqual(command.call_count, 1)
                self.assertEqual(instance.summary["status"], "failed")
                self.assertIn("changed" if overwrite else "Format save assertion failed", instance.summary["error"])

    def test_formats_requires_the_focused_completion_summary(self):
        focused = "PASS: 383 focused workspace formatting checks (format round-trip, validation, scoped-save rejection and original-file preservation)\n"
        runner.validate_output(focused, 0, "PASS:", completion_pattern=runner.FORMATS_COMPLETION_PATTERN)
        for output in ("PASS: 383 workspace checks (whole suite)\n", focused.replace("383", "0"), "PASS: an earlier format assertion\n"):
            with self.subTest(output=output), self.assertRaisesRegex(RuntimeError, "Missing completion summary"):
                runner.validate_output(output, 0, "PASS:", completion_pattern=runner.FORMATS_COMPLETION_PATTERN)

    def test_formats_cli_rejects_unrelated_or_parallel_engine_options_before_runner_creation(self):
        extras = (["--modules", "format"], ["--scenario", "icon"], ["--feature", "shelves"],
                  ["--visible"], ["--require-data"], ["--jobs", "2"])
        with mock.patch.object(runner, "Runner") as constructor:
            for extra in extras:
                with self.subTest(extra=extra), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    runner.main(["formats", *extra])
            constructor.assert_not_called()
            constructor.return_value.run.return_value = 0
            self.assertEqual(runner.main(["formats", "--config", "both", "--no-build"]), 0)
            args = constructor.call_args.args[0]
            self.assertEqual((args.tier, args.config, args.jobs, args.no_build), ("formats", "both", 1, True))

    def test_named_native_workflows_keep_order_and_deduplicate_without_full_suite(self):
        args = runner.parser().parse_args(["native", "--scenario", "export", "icon", "export"])
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "Makefile").write_text("")
            instance = runner.Runner(args, root)
            def completed(name, command, marker=None, **kwargs):
                return {"name": name, "command": command, "status": "passed", "expected_stages": kwargs["stages"]}
            with mock.patch.object(instance, "command", side_effect=completed) as command:
                instance.native_scenarios(args.scenario)
            self.assertEqual(command.call_count, 2)
            self.assertEqual([r["name"] for r in instance.summary["results"]], ["native-export", "native-icon"])
            self.assertEqual(sum(r["expected_stages"] for r in instance.summary["results"]), 28)
            self.assertIn("--export-smoke", command.call_args_list[0].args[1])
            self.assertIn("--icon-smoke", command.call_args_list[1].args[1])

    def test_edit_cli_is_opt_in_and_rejects_other_tiers_or_release_before_runner_creation(self):
        invalid = (["--edit"], ["fast", "--edit", "--list"], ["full", "--edit"],
                   ["records", "--edit"], ["formats", "--edit"],
                   ["native", "--scenario", "icon", "--edit", "--config", "release"],
                   ["affected", "--feature", "shelves", "--edit", "--config", "release"])
        with mock.patch.object(runner, "Runner") as constructor:
            for arguments in invalid:
                with self.subTest(arguments=arguments), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    runner.main(arguments)
            constructor.assert_not_called()
            constructor.return_value.run.return_value = 0
            for tier, selector in (("native", ["--scenario", "icon"]), ("affected", ["--feature", "shelves"])):
                for config in ("sanitize", "both"):
                    with self.subTest(tier=tier, config=config):
                        self.assertEqual(runner.main([tier, *selector, "--edit", "--config", config, "--no-build"]), 0)
                        args = constructor.call_args.args[0]
                        self.assertEqual((args.edit, args.config, args.no_build), (True, config, True))

    def test_edit_changes_only_the_native_app_target_and_summary(self):
        for config, binary in (("release", "build/bin/duxcel"), ("sanitize", "build/bin/duxcel-debug"), ("both", "build/bin/duxcel-debug")):
            with self.subTest(config=config), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory); (root / "Makefile").write_text("")
                args = runner.parser().parse_args(["native", "--scenario", "icon", "--config", config])
                instance = runner.Runner(args, root)
                self.assertEqual(instance.native_binary(), binary)
                self.assertFalse(instance.summary["edit"])
                self.assertEqual(instance.summary["app_config"], "release" if config == "release" else "sanitize")
                self.assertNotIn("app_optimization", instance.summary)
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory); (root / "Makefile").write_text("")
            fixture = root / "build/tests/generated/design-definition.dxls"; fixture.parent.mkdir(parents=True); fixture.write_bytes(b"fixture")
            args = runner.parser().parse_args(["native", "--scenario", "icon", "--edit"])
            instance = runner.Runner(args, root)
            expected = ["build/edit-sanitize/duxcel", "build/tests/test_background-native-debug", "design-fixture"]
            def build(targets, name):
                self.assertEqual((targets, name), (expected, "build-integration"))
                for binary in targets[:2]:
                    path = root / binary; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(binary)
            with mock.patch.object(instance, "build", side_effect=build), mock.patch.object(runner, "validate_native_contract"):
                instance.prepare_native()
            self.assertEqual(instance.summary["binaries"], runner.fingerprint([root / name for name in expected[:2]]))
            self.assertEqual((instance.summary["edit"], instance.summary["app_config"], instance.summary["app_optimization"]),
                             (True, "edit-sanitize", "-O0"))
            self.assertEqual(runner.configurations(args.config), ("release", "sanitize"))

    def test_edit_no_build_requires_the_exact_binary_and_retains_locks_hashes_and_stages(self):
        for present, replace in ((False, False), (True, False), (True, True)):
            with self.subTest(present=present, replace=replace), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory); (root / "Makefile").write_text("")
                fixture = root / "build/tests/generated/design-definition.dxls"; fixture.parent.mkdir(parents=True); fixture.write_bytes(b"fixture")
                binary = root / "build/edit-sanitize/duxcel"; binary.parent.mkdir(parents=True)
                if present: binary.write_text("owned edit binary")
                (root / "build/bin").mkdir(parents=True, exist_ok=True)
                (root / "build/bin/duxcel-debug").write_text("ordinary sanitizer is not an edit fallback")
                background = root / "build/tests/test_background-native-debug"; background.write_text("owned background binary")
                args = runner.parser().parse_args(["native", "--scenario", "icon", "--edit", "--no-build"])
                instance = runner.Runner(args, root)
                def completed(name, command, marker=None, **kwargs):
                    self.assertNotEqual(command[0], "make")
                    if name == "native-icon":
                        self.assertEqual(command[0], "./build/edit-sanitize/duxcel")
                        self.assertEqual(command[1:4], ["--icon-smoke", "--frames", "500"])
                        self.assertEqual((kwargs["native"], kwargs["stages"]), (True, 8))
                        if replace: binary.write_text("replaced edit binary")
                    return {"name": name, "command": command, "status": "passed", "expected_stages": kwargs.get("stages")}
                with mock.patch.object(instance, "command", side_effect=completed) as command, \
                     mock.patch.object(runner, "validate_native_contract"), \
                     mock.patch.object(runner, "native_lock_path", return_value=root / "desktop.lock"), \
                     contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(instance.run(), 0 if present and not replace else 1)
                self.assertTrue((root / "build/locks/test-runner.lock").is_file())
                self.assertEqual((root / "desktop.lock").exists(), present)
                self.assertEqual(command.call_count, 2 if present else 0)
                if not present:
                    self.assertIn("Missing test binary", instance.summary["error"])
                    self.assertIn("edit-sanitize/duxcel", instance.summary["error"])
                elif replace:
                    self.assertIn("changed", instance.summary["error"])
                else:
                    self.assertEqual(instance.summary["binaries_after"], instance.summary["binaries"])
                    self.assertEqual(instance.summary["native_visibility"], "hidden")

    def test_edit_affected_preserves_headless_configs_and_exact_native_range(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory); (root / "Makefile").write_text("")
            args = runner.parser().parse_args(["affected", "--feature", "shelves", "--modules", "format", "--edit", "--config", "both"])
            instance = runner.Runner(args, root)
            start, end, marker = runner.LAYOUT_PROFILES[args.feature]
            def completed(name, command, marker=None, **kwargs):
                self.assertEqual(command[0], "./build/edit-sanitize/duxcel")
                self.assertEqual(command[1:4], ["--layout-smoke", "--frames", str(max(1200, (end-start+10)*48))])
                self.assertEqual(kwargs["env_extra"], {"DUXCEL_LAYOUT_FROM": str(start), "DUXCEL_LAYOUT_TO": str(end)})
                self.assertEqual(kwargs["completion_pattern"], runner.layout_completion_pattern(start, end))
                self.assertEqual(kwargs["stages"], end-start)
                return {"name": name, "status": "passed"}
            with mock.patch.object(instance, "headless") as headless, mock.patch.object(instance, "prepare_native"), \
                 mock.patch.object(instance, "background_check"), mock.patch.object(instance, "command", side_effect=completed) as command, \
                 mock.patch.object(runner, "FileLock"), contextlib.redirect_stdout(io.StringIO()):
                instance.affected()
            headless.assert_called_once_with(["format"])
            self.assertEqual(runner.configurations(instance.args.config), ("release", "sanitize"))
            self.assertEqual(command.call_args.args[2], marker)

    def test_edit_native_visibility_is_still_hidden_and_not_inherited(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory); (root / "Makefile").write_text("")
            args = runner.parser().parse_args(["native", "--scenario", "icon", "--edit"])
            instance = runner.Runner(args, root)
            def child(command, **kwargs):
                self.assertEqual(kwargs["env"]["DUXCEL_TEST_VISIBLE"], "0")
                self.assertIn("halt_on_error=1", kwargs["env"]["ASAN_OPTIONS"])
                self.assertIn("halt_on_error=1:print_stacktrace=1", kwargs["env"]["UBSAN_OPTIONS"])
                kwargs["stdout"].write("PASS: final\nDuxcel frames=10 mean=1.0 ms smoke=0\n")
                return mock.Mock(returncode=0)
            with mock.patch.dict(runner.os.environ, {"DUXCEL_TEST_VISIBLE": "1"}), \
                 mock.patch.object(runner.subprocess, "run", side_effect=child), contextlib.redirect_stdout(io.StringIO()):
                result = instance.command("edit-native", ["./" + instance.native_binary()], "PASS: final", native=True)
            self.assertEqual(result["status"], "passed")
            self.assertEqual(instance.summary["native_visibility"], "hidden")

    def test_native_missing_inputs_fail_before_opening_desktop(self):
        for extra in (["--scenario", "table", "--no-build"], ["--scenario", "nyc", "--require-data"]):
            args = runner.parser().parse_args(["native", *extra])
            with self.subTest(args=extra), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory)
                (root / "Makefile").write_text("")
                instance = runner.Runner(args, root)
                with mock.patch.object(instance, "prepare_native"), mock.patch.object(runner, "FileLock") as desktop:
                    with self.assertRaisesRegex(RuntimeError, "Missing table fixtures|NYC dataset missing"):
                        instance.native()
                    desktop.assert_not_called()

    def test_cli_validation_and_inventory_do_not_construct_runner(self):
        with mock.patch.object(runner, "Runner") as constructor:
            with contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(runner.main(["--list"]), 0)
            self.assertIn("1390 application stages", output.getvalue())
            self.assertIn("Records: record_sql, record_pages", output.getvalue())
            self.assertIn("Formats: workbook --formats-only", output.getvalue())
            constructor.assert_not_called()
        for arguments in (["affected"], ["full", "--modules", "grid"], ["native"], ["native", "--scenario", "export", "--modules", "grid"], ["fast", "--scenario", "export"], ["fast", "--jobs", "3"], ["fast", "--timeout", "0"], ["fast", "--feature", "shelves"], ["affected", "--feature", "shelves", "--require-data"]):
            with self.subTest(arguments=arguments), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                runner.main(arguments)



if __name__ == "__main__":
    unittest.main(verbosity=2)
