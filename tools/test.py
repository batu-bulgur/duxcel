#!/usr/bin/env python3
"""Bounded edit-loop checks and reproducible native regression; stdlib only."""

import argparse
import concurrent.futures
import contextlib
import shutil
import signal
import threading
import datetime
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time


from test_catalog import FEATURE_FIXTURES, FEATURES, STANDALONE_GUI, ENGINE_PROCESS_CHECKS, plan as feature_plan
from workspace import (ROOT, FileLock, native_lock_path, source_metadata,
                       utc_now, sha256, fingerprint, ensure_unchanged, make_jobserver_fds, operation_lease, inherited_lease_fds)
FAST_MODULES = (
    "history_profile_codec",
    "history_profile",
    "range_history_slot",
    "range_history_payload",
    'manual_cells_plan',
    'manual_range_wire',
    'manual_value_wire',
    'manual_value',
    'primary_source_pending',
    'primary_source_topology',

    'sheet_source_view',

    'primary_source_publish',

    'cell_history_payload',
    'source_history_slot',
    'history_event_slot',
    'source_history_payload',
    'sheet_history_applicability',
    'source_versions_admission',
    'history_event_frame',
    'source_dependency_admission',
    'primary_source_draft',
    'sheet_history_batch',
    'sheet_history_slot',
    'sheet_session_chronology',
    'sheet_metadata_replay',
    'sheet_history_payload',
    'sheet_topology_replay',
    'sheet_topology_duplicate',
    'sheet_topology_event',
    'history_sheet_topology',
    'sheet_topology_remove',
    'column_widths',
    'column_width_history',
    'mixed_range_plan',
    'sheet_presence_asset',
    'sheet_lifecycle_command',
    'sheet_lifecycle',
    'build_info',
    'workbook_lifecycle',
    'workbook_history',
    'workbook_identity',
    'cell_command',
    'format_command',
    'source_format_command',
    'analysis_command',
    'primary_replacement',
    'object_source_plan',
    'table_command',
    'table_replacement',
    'object_name_command',
    'object_presentation_command',
    'object_pivot_command',
    'object_binding_command',
    'object_delete_command',
    'object_move_command',
    'object_create_command',
    'object_import_draft',
    'object_layout_command',
    'object_marks_command',
    'cell_tsv',
    'cell_paste_plan',
    'cell_range_plan',
    'range_event',
    'range_command',
    'history_range',
    'cell_delta',
    'cell_delta_codec',
    'cellreadout',
    'filterlabel',
    'grid',
    'selection_ladder',
    'selection',
    'cell_regions',
    'source_scope',
    'source_versions_codec',
    'workbook_codec',
    'workbook_schema',
    'version_index',
    'history_window',
    'history_anchors',
    'history_path',
    'history_stream',
    'history_format',
    'analysis_asset',
    'pivot_asset',
    'marks_asset',
    'objects_asset',
    'definition_dependencies',
    'definition_state',
    'definition_origin',
    'definition_baseline',
    'definition_event',
    'history_definition',
    'version_tag',
    'version_chunks',
    'workbook_codec_read',
    'objects',
    'brand',
    'format',
    'format_delta_codec',
    'format_dependencies',
    'chartview',
    'chartencoding',
    'inspector',
    'shadowview',
    'stepview',
    'preview',
    'preview_input',
    'preview_input_browser',
    'textview',
    'textview_review',
    'statsview',
    'statsview_input',
    'statsview_input_review',
    'ui_update_input',
    'ui_update_input_browser',
    'ui_update_input_control',
    'guideview',
    'guideview_input',
    'guideview_input_review',
    'sortview_input',
    'sortview_input_review',
    'fontmetrics',
    'ui_font_fallback',
    'motion',
    'keytips',
    'choiceview',
    'paintlayer',
    'popup',
    'marksview',
    'page',

)
RECORD_MODULES = ("record_sql", "record_pages")
FAST_MODULES += ("source_range_plan", "sourceview", "browse_preview")
ENGINE_MODULES = ("version_publication", "history_after_save", "history_component", "history_native_capture", "core", "workbook", "fixture", *RECORD_MODULES, "workbook_codec_native", "workbook_identity_native", "workbook_journal", "journal_queue", "journal_session", "journal_performance", "workbook_save", "workbook_payload", "core_payload", "core_restore", "core_compare", "version_stage", "version_read", "version_restore", "archive_read", "archive_write", "version_save", "version_tag_save", "version_preview", "version_lifetime", "worker_archive", "preview_worker", "app_versions", "app_preview", "file_conflict", "version_checkpoint", "worker_native", "record_selection_worker", "worker_source_digest", "worker_save", "file_lease", "workbook_recovery", "directory_cursor", "recovery_catalog", "recovery_worker", "recovery_process")
FORMATS_COMPLETION_PATTERN = r"^PASS: [1-9]\d* focused workspace formatting checks \(format round-trip, validation, scoped-save rejection and original-file preservation\)$"
# These are existing, independently bootstrapped layout checkpoints. The end is
# exclusive, as in DUXCEL_LAYOUT_TO. Never jump into arbitrary stateful stages.
LAYOUT_PROFILES = {
    "selection-start": (0, 45, "PASS: keyboard destination move"),
    "ribbon": (0, 64, "PASS: measured native ribbon"),
    "format-controls": (0, 148, "PASS: real native Bold/Italic/Underline"),
    "inspector-basics": (216, 250, "PASS: real native historical step preview/Return"),
    "stats-budget": (861, 867, "PASS: native Statistics Enter keypad Enter and Space"),
    "foundation-layout-end": (882, 950, "PASS: native deterministic sparse containing regions"),
    "foundation-sources": (730, 950, "PASS: native deterministic sparse containing regions"),
    "source-layout": (474, 522, "PASS: exact source value strip at 100/150 percent"),
    "foundation-final": (522, 950, "PASS: native deterministic sparse containing regions"),
    "foundation-remainder": (400, 950, "PASS: native deterministic sparse containing regions"),
    "foundation-layout": (250, 950, "PASS: native deterministic sparse containing regions"),
    "statistics": (250, 266, "PASS: exact compact native statistics popover"),
    "dock": (311, 335, "PASS: measured native status bar"),
    "pane-motion": (357, 400, "PASS: real nested pane/Workbook group-opacity"),
    "popup-motion": (400, 426, "PASS: real view-filter draft"),
    "shelves": (426, 458, "PASS: compact 240-unit Add field menu"),
    "view-menus": (458, 474, "PASS: exact four-choice Text color menu"),
    "source-menu": (474, 500, "PASS: exact wide/narrow authoring title"),
    "source-controls": (500, 522, "PASS: exact source value strip"),
    "shared-actions": (510, 540, "PASS: shared compact native row/column/range/object action menus"),
    "author-menus": (540, 555, "PASS: exact headed Marks/step/worksheet menus"),
    "active-cell": (555, 596, "PASS: active-origin queued drag/Shift/Tab/Enter"),
    "criteria-selects": (596, 620, "PASS: cached actual view-filter criteria"),
    "ribbon-collapse": (620, 638, "PASS: exact reserved collapsed ribbon"),
    "ribbon-overflow": (638, 665, "PASS: exact More ribbon commands"),
    "record-pages": (665, 675, "PASS: native source record pages"),
    "zoom-headers": (675, 687, "PASS: fixed native sheet header chrome"),
    "selection-ladder": (687, 714, "PASS: progressive native Select All"),
    "source-cache": (687, 716, "PASS: source replacement retires"),
    "source-format": (687, 730, "PASS: native source-aware primary Table matrix preview"),
    "format-journal": (730, 734, "PASS: native prepared Analysis and format transaction"),
    "source-format-first-binding": (734, 800, "PASS: native Table record formatting"),
    "source-record-format": (734, 764, "PASS: native captured Verify Collect Commit"),
    "table-record-format": (764, 800, "PASS: native Table record formatting"),
    "source-result-format": (800, 824, "PASS: native verified exact-result and guarded styles"),
    "manual-format-import": (846, 861, "PASS: native manual format history stays manual"),
    "stats-input": (861, 867, "PASS: native Statistics Enter keypad Enter and Space"),
    "inspector-scroll": (882, 898, "PASS: native independent Steps outer and inner scrolling"),
    "inspector-sql": (882, 911, "PASS: native SQL inspector keyboard scrolling"),
    "range-move": (911, 921, "PASS: native Option interior and border range moves"),
    "range-replace": (921, 936, "PASS: native explicit manual Replace/Cancel"),
    "containing-regions": (936, 950, "PASS: native deterministic sparse containing regions"),
}
# Removed compatibility-only cases retain terminal IDs so surviving focused
# bootstraps do not shift. Count executed cases separately from the terminal ID.
RETIRED_LAYOUT_RANGES = ((824, 846), (867, 882))


def layout_stage_count(start, end):
    return end-start-sum(max(0, min(end, stop)-max(start, begin))
                         for begin, stop in RETIRED_LAYOUT_RANGES)


# Kept explicit: changes to terminal assertions/stage counts must be reviewed.
# Frames are a failure budget, not a claim that the app exits on completion.
NATIVE = (
    ("layout", 950, 46080, "PASS: native deterministic sparse containing regions"),
    ("base", 6, 240, None),
    ("workspace", 4, 300, "PASS: native workspace save/load"),
    ("filter", 66, 6200, "PASS: clamped text-condition submenu"),
    ("pivot", 45, 4800, "PASS: categorical Values default to distinct count"),
    ("export", 20, 1000, "PASS: worker Parquet export"),
    ("count", 12, 900, "PASS: background count"),
    ("clipboard", 13, 900, "PASS: cached source/manual/pivot rectangular copy"),
    ("sort", 12, 900, "PASS: multi-column sort"),
    ("navigation", 13, 1200, "PASS: native sheet rename"),
    ("nyc", 8, 1000, "PASS: public NYC 3,475,226-row source"),
    ("document", 10, 2400, "PASS: pending cell commit"),
    ("chart", 40, 4800, "PASS: source-derived chart Other"),
    ("statistics", 31, 2500, "PASS: frame selection reports object identity"),
    ("table", 73, 7200, "PASS: independent same-sheet CSV/Parquet/DuckDB tables"),
    ("icon", 8, 500, "PASS: shared vector and prebuilt app icon previews"),
    ("query", 47, 3600, "PASS: actual native date-token menu changes Month to Year"),
    ("unique", 32, 3600, "PASS: structured unique rows on primary and independent sources"),
)
SANITIZER_FAILURE = re.compile(r"ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|SUMMARY: (?:AddressSanitizer|UndefinedBehaviorSanitizer)")


# These layout checkpoints perform real OS clipboard access (including the
# preceding injected Copy gesture). Other layout input is process-local injection.
CLIPBOARD_LAYOUT_RANGES = ((225,227), (235,237), (252,254), (259,261), (479,481))
# Explicitly reviewed process-local checkpoints. Unknown/new ranges remain
# conservative until their input, clipboard, panels and settings are reviewed.
PRIVATE_LAYOUT_RANGES = {
    (0,45), (0,64), (0,148), (311,335), (357,400), (400,426),
    (426,458), (458,474), (500,522), (510,540), (522,950),
    (540,555), (555,596), (596,620), (620,638), (638,665),
    (665,675), (675,687), (687,714), (687,716), (687,730),
    (730,734), (730,950), (734,764), (764,800), (800,824),
    (846,861), (861,867),
    (882,898), (882,911), (882,950), (911,921), (921,936), (936,950),
}
PRIVATE_NATIVE_SCENARIOS = {"base", "workspace", "filter", "export", "count", "navigation", "nyc", "document", "chart", "statistics", "icon", "query", "unique"}
PRIVATE_STANDALONE_GUI = {
    "source-initial-view",
    "primary-source-kinds",
    "primary-source",
    "table-origin",
    "definition-origin",
    "object-create-command", "object-move-command", "object-delete-command", "object-binding-command",
    "primary-replacement", "table-replacement", "object-pivot-command",
    "object-layout-command", "object-marks-command", "object-presentation-command",
    "object-name-command", "table-command", "primary-steps", "primary-query",
    "primary-transforms", "analysis-command", "format-plan", "history-preview-sanitize",
    "history-preview", "version-controls", "recovery-menu", "recovery", "workbook-save",
    "journal-session", "workbook-lifecycle", "workbook-history", "workbook-identity", "cell-command",
}


def native_resource_guard(*, layout=None, scenario=None, standalone=None):
    shared = True
    if layout is not None:
        start, end = layout
        shared = layout not in PRIVATE_LAYOUT_RANGES or any(start < stop and begin < end for begin, stop in CLIPBOARD_LAYOUT_RANGES)
    elif scenario is not None:
        shared = scenario not in PRIVATE_NATIVE_SCENARIOS
    elif standalone is not None:
        shared = standalone not in PRIVATE_STANDALONE_GUI
    return FileLock(native_lock_path(), "Native shared OS services", blocking=True) if shared else contextlib.nullcontext()


def stop_process_group(process):
    """Stop and drain our process group before releasing build ownership."""
    for number in (signal.SIGTERM, signal.SIGKILL):
        try:
            os.killpg(process.pid, number)
        except ProcessLookupError:
            pass
        if number == signal.SIGTERM:
            try:
                process.wait(timeout=.25)
            except subprocess.TimeoutExpired:
                pass
    process.wait()
    # Orphaned grandchildren are reaped by the OS, not waitpid in this process.
    # A zombie has stopped executing; retain ownership until no live member is
    # left, even when the immediate Make/driver process exited earlier.
    while True:
        inventory = subprocess.run(["ps", "-axo", "pgid=,stat="], capture_output=True,
                                   text=True, check=True, timeout=5).stdout
        live = any(int(fields[0]) == process.pid and not fields[1].startswith("Z")
                   for line in inventory.splitlines() if len(fields := line.split()) >= 2)
        if not live:
            return
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        time.sleep(.01)


def run_owned_process(command, *, timeout, **kwargs):
    """Own one session so timeout/interruption cannot leave compiler children."""
    process = subprocess.Popen(command, start_new_session=True, **kwargs)
    previous = None
    main = threading.current_thread() is threading.main_thread()
    if main:
        def terminated(number, frame):
            raise KeyboardInterrupt("Owned command terminated")
        previous = signal.signal(signal.SIGTERM, terminated)
    try:
        try:
            return subprocess.CompletedProcess(command, process.wait(timeout=timeout))
        except BaseException:
            stop_process_group(process)
            raise
    finally:
        if main:
            signal.signal(signal.SIGTERM, previous)


def configurations(name):
    return ("release", "sanitize") if name == "both" else (name,)


def test_binary(module, config):
    return f"build/tests/test_{module}" + ("-debug" if config == "sanitize" else "")


def layout_completion_pattern(start, end):
    # The runtime reports the completed range. A familiar PASS prefix alone
    # cannot establish that a deliberately reused binary has the current stages.
    # This stage-contract guard does not prove arbitrary --no-build freshness.
    return "^" + re.escape(f"Layout smoke completed [{start},{end})") + "$"


def validate_output(text, exit_code, marker=None, native=False, base=False, completion_pattern=None):
    if exit_code:
        raise RuntimeError(f"Process exited with status {exit_code}")
    if SANITIZER_FAILURE.search(text):
        raise RuntimeError("Sanitizer diagnostic found")
    if marker and not any(line.startswith(marker) for line in text.splitlines()):
        raise RuntimeError("Missing completion assertion: " + marker)
    if completion_pattern and not re.search(completion_pattern, text, re.M):
        raise RuntimeError("Missing completion summary: " + completion_pattern)
    if native:
        if not re.search(r"^Duxcel frames=\d+ .* smoke=\d+$", text, re.M):
            raise RuntimeError("Missing application shutdown summary")
        if re.search(r"smoke unfinished|smoke failed|^FAIL:", text, re.I | re.M):
            raise RuntimeError("Native scenario reported unfinished or failed assertions")
    if base:
        if not re.search(r"^Duxcel frames=\d+ .* smoke=6$", text, re.M):
            raise RuntimeError("Base smoke did not complete its six stages")
        if not re.search(r"^Final relation rows=4 columns=2 ", text, re.M):
            raise RuntimeError("Base smoke has an incorrect final relation")


def validate_native_contract(root):
    source = (root / "src/app/app.c").read_text()
    for name, stages, _, _ in NATIVE:
        if name in ("layout", "query"):
            pattern = rf"#define {name.upper()}_SMOKE_STAGES {stages}\b"
        elif name == "base":
            pattern = rf"smoke=={stages}\b"
        elif name == "document":
            pattern = rf"document_smoke=={stages}\b"
        else:
            pattern = rf"{name}_smoke!={stages}\b"
        if not re.search(pattern, source):
            raise RuntimeError(f"Native {name} stage contract changed; review the runner's inventory before accepting this build")


class Runner:
    def __init__(self, args, root=ROOT):
        self.args, self.root = args, root
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S")
        self.log_dir = Path(args.log_dir).resolve() if args.log_dir else root / "build/tests/results/runs" / f"{stamp}-{args.tier}-{os.getpid()}"
        self.log_dir.mkdir(parents=True, exist_ok=True)
        self.summary = {"status": "running", "started_utc": utc_now(), "tier": args.tier, "config": args.config,
                        "jobs": args.jobs, "no_build": args.no_build, "edit": args.edit, "native_visibility": "visible" if getattr(args, "visible", False) else "hidden", "root": str(root), "source": source_metadata(root), "results": [], "binaries": {}}
        if args.tier in ("affected", "native", "full"):
            self.summary["app_config"] = "edit-sanitize" if args.edit else "release" if args.config == "release" else "sanitize"
            if args.edit:
                self.summary["app_optimization"] = "-O0"
        self.start = time.monotonic()

    def write_summary(self):
        self.summary["elapsed_seconds"] = round(time.monotonic() - self.start, 3)
        target = self.log_dir / "summary.json"
        temporary = target.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(self.summary, indent=2) + "\n")
        temporary.replace(target)

    def command(self, name, command, marker=None, native=False, base=False, env_extra=None, stages=None, completion_pattern=None):
        # Called in parallel only after builds finish, with distinct test names.
        log = self.log_dir / (name + ".txt")
        env = os.environ.copy()
        env["ASAN_OPTIONS"] = (env.get("ASAN_OPTIONS", "") + ":halt_on_error=1").lstrip(":")
        env["UBSAN_OPTIONS"] = (env.get("UBSAN_OPTIONS", "") + ":halt_on_error=1:print_stacktrace=1").lstrip(":")
        # Do not inherit focused-layout/debug/performance overrides into a run.
        for key in ("DUXCEL_LAYOUT_FROM", "DUXCEL_LAYOUT_TO", "DUXCEL_FRAME_CAP", "DUXCEL_RENDER_BUFFERS", "DUXCEL_FONT", "DUXCEL_MONO_FONT", "DUXCEL_TEST_VISIBLE"):
            env.pop(key, None)
        env["DUXCEL_TEST_INSTANCE"] = "1"
        if native:
            recovery = self.log_dir / (name + "-recovery")
            recovery.mkdir(parents=True, exist_ok=True)
            env["DUXCEL_TEST_RECOVERY_ROOT"] = str(recovery)
        env["DUXCEL_TEST_VISIBLE"] = "1" if getattr(self.args, "visible", False) else "0"
        env["PYTHONPATH"] = str(self.root / "tools") + os.pathsep + env.get("PYTHONPATH", "")
        env.update(env_extra or {})
        start = time.monotonic()
        result = {"name": name, "command": command, "log": str(log), "expected_stages": stages}
        if completion_pattern:
            result["completion_pattern"] = completion_pattern
        if native or name.startswith("build-") or name == "gpu-sanitize":
            print(f"RUN    {name}: {log}", flush=True)
        try:
            with log.open("w", encoding="utf-8") as stream:
                stream.write("Command: " + json.dumps(command) + "\n")
                stream.flush()
                process = run_owned_process(command, cwd=self.root, env=env, stdout=stream, stderr=subprocess.STDOUT,
                                         timeout=self.args.timeout if not native else max(self.args.timeout, 900),
                                         pass_fds=tuple(set(inherited_lease_fds(env) + (make_jobserver_fds(env) if command[0]=="make" else ()))))
            result["exit_code"] = process.returncode
            validate_output(log.read_text(errors="replace"), process.returncode, marker, native, base, completion_pattern)
            result["status"] = "passed"
        except (OSError, subprocess.TimeoutExpired, RuntimeError) as error:
            result["status"], result["error"] = "failed", str(error)
        result["seconds"] = round(time.monotonic() - start, 3)
        print(f"{result['status'].upper():6} {name}: {result['seconds']:.3f}s" + (f" — {result['error']} ({log})" if result.get("error") else ""), flush=True)
        return result

    def record(self, result):
        self.summary["results"].append(result)
        self.write_summary()
        if result["status"] == "failed":
            raise RuntimeError(result.get("error", "Test failed"))

    def build(self, targets, name="build"):
        if not self.args.no_build:
            with operation_lease(self.root, compilation=True):
                self.record(self.command(name, ["make", *dict.fromkeys(targets)]))

    def retain_binaries(self, binaries):
        paths = [self.root / binary for binary in dict.fromkeys(binaries)]
        for path in paths:
            if not path.is_file():
                raise RuntimeError("Missing test binary: " + str(path) + "; omit --no-build to build it")
        self.summary["binaries"].update(fingerprint(paths))
        self.write_summary()

    def headless(self, modules):
        runs = [(module, config, test_binary(module, config))
                for config in configurations(self.args.config) for module in modules]
        binaries = [binary for _, _, binary in runs]
        self.build(binaries, "build-headless")
        self.retain_binaries(binaries)
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.args.jobs) as pool:
            futures = [pool.submit(self.command, f"{module}-{config}", ["./" + binary],
                                   "PASS:" if module != "grid" else None,
                                   completion_pattern=r"^\d+ grid checks$" if module == "grid" else None)
                       for module, config, binary in runs]
            failures = []
            # Collect every launched process before reporting failure or starting
            # mutation/build/native work. Executor joins on context exit as well.
            for future in futures:
                result = future.result()
                self.summary["results"].append(result)
                if result["status"] == "failed":
                    failures.append(result)
            self.write_summary()
        ensure_unchanged(self.summary["binaries"])
        if failures:
            raise RuntimeError(f"{len(failures)} headless check(s) failed; see summary.json")

    def native_binary(self):
        return getattr(self, "_pinned_native", self.native_build_target())

    def native_build_target(self):
        if self.args.edit:
            return "build/edit-sanitize/duxcel"
        return "build/bin/duxcel" if self.args.config == "release" else "build/bin/duxcel-debug"

    def records(self):
        """Native engine checks with private temporary sources, no desktop."""
        runs = [(module, config, test_binary(module, config))
                for config in configurations(self.args.config) for module in RECORD_MODULES]
        self.build([binary for _, _, binary in runs], "build-records")
        self.retain_binaries([binary for _, _, binary in runs])
        for module, config, binary in runs:
            ensure_unchanged(self.summary["binaries"])
            self.record(self.command(f"{module}-{config}", ["./" + binary], "PASS:"))
            ensure_unchanged(self.summary["binaries"])

    def formats(self):
        """Bounded workspace format/save checks; no core fixtures or desktop."""
        (self.root / "build/tests/generated").mkdir(parents=True, exist_ok=True)
        runs = [(config, test_binary("workbook", config)) for config in configurations(self.args.config)]
        self.build([binary for _, binary in runs], "build-formats")
        self.retain_binaries([binary for _, binary in runs])
        for config, binary in runs:
            ensure_unchanged(self.summary["binaries"])
            self.record(self.command(f"formats-{config}", ["./" + binary, "--formats-only"], "PASS:",
                                     completion_pattern=FORMATS_COMPLETION_PATTERN))
            ensure_unchanged(self.summary["binaries"])

    def prepare_native(self, full=False, background=True):
        validate_native_contract(self.root)
        targets = [self.native_build_target()]
        if background:
            targets.append("build/tests/test_background-native-debug")
        targets.append("design-fixture")
        if full:
            targets += [test_binary(module, config) for config in configurations(self.args.config) for module in ENGINE_MODULES]
            targets += sorted({target for check in ENGINE_PROCESS_CHECKS.values() for target in check.support})
            targets += ["build/tests/test_paint-native-debug", "icons-check"]
        self.build(targets, "build-integration")
        # Snapshot helpers write to these existing conventional destinations.
        for name in ("workspace-layout", "native-authoring", "filter-keyboard", "multisort", "sheet-navigation",
                     "current-workspace", "chart-marks", "selection-statistics", "table-sources", "icons", "source-query", "unique-rows"):
            (self.root / "build/tests/results" / name).mkdir(parents=True, exist_ok=True)
        (self.root / "build/tests/generated/workspace-old").mkdir(parents=True, exist_ok=True)
        (self.root / "build/tests/generated/workspace-new").mkdir(parents=True, exist_ok=True)
        if not (self.root / "build/tests/generated/design-definition.dxls").is_file():
            raise RuntimeError("Missing canonical design fixture; omit --no-build")
        self.retain_binaries([self.native_build_target()] + (["build/tests/test_background-native-debug"] if background else []) + (["build/tests/test_paint-native-debug"] if full else []))

        self.pin_native_binary()

    def pin_native_binary(self):
        """Copy one immutable test executable; never launch the ordinary app path."""
        self._pinned_native = self.pin_test_binary(self.native_build_target(), "duxcel")
        self.summary["test_executable"] = self.summary["test_executables"][-1]

    def pin_test_binary(self, binary, label=None):
        """Pin App-bearing feature helpers with the same lane identity as smoke."""
        source = self.root / binary
        lane = os.environ.get("DUXCEL_LANE", "local")
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,31}", lane):
            raise RuntimeError("Invalid TEST executable lane identity")
        folder = self.root / "build/tests/lanes" / lane / self.log_dir.name
        folder.mkdir(parents=True, exist_ok=True)
        target = folder / ((label or source.name) + "-TEST-" + lane + ("-debug" if label and self.args.config != "release" else ""))
        before = sha256(source)
        if target.exists():
            prior = next((entry for entry in self.summary.get("test_executables", [])
                          if entry["path"] == str(target) and entry["source"] == str(source)), None)
            if prior and prior["sha256"] == before == sha256(target):
                return str(target.relative_to(self.root))
            raise RuntimeError("Pinned TEST executable already exists or changed: " + str(target))
        with source.open("rb") as incoming, target.open("xb") as outgoing:
            shutil.copyfileobj(incoming, outgoing)
        target.chmod(source.stat().st_mode & 0o777)
        if sha256(target) != before or sha256(source) != before:
            target.unlink()
            raise RuntimeError("Native binary changed during TEST executable capture")
        assets = self.root / "assets"
        if assets.is_dir() and not (folder / "assets").exists():
            (folder / "assets").symlink_to(assets, target_is_directory=True)
        pinned = str(target.relative_to(self.root))
        self.summary.setdefault("test_executables", []).append({"path": str(target), "source": str(source),
                                           "sha256": before, "lane": lane, "checkout": str(self.root)})
        self.retain_binaries([pinned])
        return pinned

    def background_check(self):
        self.record(self.command("background-sanitize", ["./build/tests/test_background-native-debug"], "PASS: 477 background native checks"))

    def about_check(self):
        if sys.platform != "darwin":
            self.summary["results"].append({"name": "about-native", "status": "skipped", "reason": "AppKit adapter requires macOS"})
            return
        binary = "build/tests/test_about-native-debug"
        self.build([binary], "build-about")
        self.retain_binaries([binary])
        binary = self.pin_test_binary(binary)
        self.record(self.command("about-native", ["./" + binary], "PASS: native About and test identity",
            completion_pattern=r"^PASS: native About and test identity \([1-9]\d* checks\)$"))

    def standalone_gui_check(self,name):
        binary, marker, completion, stages = STANDALONE_GUI[name]
        self.build([binary], "build-feature-gui")
        self.retain_binaries([binary])
        binary = self.pin_test_binary(binary)
        (self.root / "build/tests/generated").mkdir(parents=True,exist_ok=True)
        (self.root / "build/tests/results" / name).mkdir(parents=True,exist_ok=True)
        with native_resource_guard(standalone=name):
            self.record(self.command(name+"-native",["./"+binary],marker,native=True,
                                     completion_pattern=completion,stages=stages))

    def feature_engine_checks(self, modules):
        """Mutable private archive checks run serially, including sanitizer variants."""
        if "workbook" in modules:
            for name in ("workspace-old", "workspace-new"):
                (self.root / "build/tests/generated" / name).mkdir(parents=True, exist_ok=True)
        runs = [(module, config, test_binary(module, config))
                for config in configurations(self.args.config) for module in modules]
        support = sorted({target for module in modules if module in ENGINE_PROCESS_CHECKS and ENGINE_PROCESS_CHECKS[module].selected(self.args.feature) for target in ENGINE_PROCESS_CHECKS[module].support})
        self.build([binary for _, _, binary in runs]+support, "build-feature-engine")
        self.retain_binaries([binary for _, _, binary in runs]+support)
        for module, config, binary in runs:
            ensure_unchanged(self.summary["binaries"])
            check = ENGINE_PROCESS_CHECKS.get(module)
            if not check or not check.primary:
                self.record(self.command(f"{module}-{config}", ["./" + binary], "PASS:"))
            ensure_unchanged(self.summary["binaries"])
            if module=="workbook_codec_native":
                self.codec_native_checks(binary,config)
            self.engine_process_check(module,config,binary)

    def codec_native_checks(self,binary,config):
        """Run the current metadata families against separate private archives."""
        families = (("sorts", "optional sorts"), ("preferences", "optional preferences"),
                    ("objects", "object descriptors"), ("queries", "source queries"),
                    ("source-options", "source options"), ("tables", "Table sources"),
                    ("pivot-marks", "Pivot Marks"), ("filters", "primary filters"),
                    ("grid", "grid metadata"), ("counters", "object counters"),
                    ("source-counters", "source version counters"), ("source-versions", "source versions"),
                    ("source-bindings", "source bindings"), ("format-rows", "format rows"),
                    ("format-tags", "format tags"))
        for option,marker in families:
            self.record(self.command(f"workbook_codec_native-{option}-{config}",
                                     ["./" + binary,f"--{option}-only"],
                                     completion_pattern=rf"^PASS: native {marker} \d+ checks;"))
            ensure_unchanged(self.summary["binaries"])

    def engine_process_check(self,module,config,binary):
        if module in ENGINE_PROCESS_CHECKS and ENGINE_PROCESS_CHECKS[module].selected(self.args.feature):
            check = ENGINE_PROCESS_CHECKS[module]
            extra = ["--output", str(self.log_dir / f"{module}-{config}-metrics.json")] if check.primary else []
            # The recovery driver launches real hidden App processes. Journal
            # performance/fault/process drivers link headless engine helpers.
            if module == "recovery_process":
                binary = self.pin_test_binary(binary)
            self.record(self.command(f"{module}-{config}-process", [sys.executable, check.script, "--binary", binary, *check.arguments, *extra],
                                     check.marker, completion_pattern=check.completion))
            ensure_unchanged(self.summary["binaries"])

    def prepare_feature_fixtures(self, names):
        for name in sorted(set(names)):
            arguments, marker, target = FEATURE_FIXTURES[name]
            if self.args.no_build:
                if not (self.root / target).is_file():
                    raise RuntimeError("Missing feature fixture; omit --no-build: " + target)
            else:
                self.record(self.command(name+"-fixture", [sys.executable, *arguments], marker))

    def engines(self):
        """Complete ordered storage/worker suites without replaying App workflows.

        Registered process checks (including recovery) remain mandatory. This
        is one milestone chunk, not a substitute for full App/GPU scenarios.
        """
        self.prepare_feature_fixtures(FEATURE_FIXTURES)
        self.feature_engine_checks(ENGINE_MODULES)

    def feature(self):
        plan = feature_plan(self.args.feature)
        self.summary["feature_plan"] = plan
        if plan["fixtures"]:
            self.prepare_feature_fixtures(plan["fixtures"])
        gui=plan["gui"]
        sanitized=gui=="about" or (gui in STANDALONE_GUI and STANDALONE_GUI[gui][0].endswith("-debug"))
        self.summary["app_config"]="standalone-sanitize" if sanitized else "release" if gui else None
        if plan["tool_tests"]:
            self.record(self.command("feature-tools", [sys.executable, "-m", "unittest", *plan["tool_tests"]], "OK"))
        if plan["modules"]:
            self.headless(plan["modules"])
        if plan["engine_modules"]:
            self.feature_engine_checks(plan["engine_modules"])
        if plan["gui"] == "about":
            self.about_check()
        elif plan["gui"] in STANDALONE_GUI:
            self.standalone_gui_check(plan["gui"])
        elif plan["gui"]:
            # The local GUI uses release; the selected logic modules retain
            # ASan/UBSan. Full-App sanitizer/background coverage is deferred.
            original_config = self.args.config
            self.args.config = "release"
            try:
                self.prepare_native(background=False)
                start, end, marker = LAYOUT_PROFILES[plan["gui"]]
                with native_resource_guard(layout=(start, end)):
                    self.record(self.command("feature-" + plan["gui"],
                        ["./" + self.native_binary(), "--layout-smoke", "--frames", str(max(1200,(end-start+10)*48)),
                         "--screenshot", str(self.log_dir / "final.png")], marker, native=True,
                        env_extra={"DUXCEL_LAYOUT_FROM": str(start), "DUXCEL_LAYOUT_TO": str(end)},
                        stages=layout_stage_count(start,end), completion_pattern=layout_completion_pattern(start,end)))
            finally:
                self.args.config = original_config
        print("Deferred interaction checks (not run): " + str(len(plan["deferred_interactions"])), flush=True)

    def affected(self):
        self.headless(self.args.modules or FAST_MODULES)
        self.prepare_native()
        start, end, marker = LAYOUT_PROFILES[self.args.feature]
        frames = max(1200, (end - start + 10) * 48)
        with native_resource_guard(layout=(start, end)):
            ensure_unchanged(self.summary["binaries"])
            self.background_check()
            self.record(self.command("layout-" + self.args.feature,
                                    ["./" + self.native_binary(), "--layout-smoke", "--frames", str(frames),
                                     "--screenshot", str(self.log_dir / "final.png")], marker, native=True,
                                    env_extra={"DUXCEL_LAYOUT_FROM": str(start), "DUXCEL_LAYOUT_TO": str(end)}, stages=layout_stage_count(start,end),
                                    completion_pattern=layout_completion_pattern(start, end)))
            ensure_unchanged(self.summary["binaries"])

    def full(self):
        data = self.root / "data/downloads/yellow_tripdata_2025-01.parquet"
        if self.args.require_data and not data.is_file():
            raise RuntimeError("Required NYC dataset missing: " + str(data))
        self.headless(FAST_MODULES)
        self.about_check()
        # Standalone App workflows consume the same finite fixtures as engine
        # suites. Prepare them before any GUI, including on a clean checkout.
        self.prepare_feature_fixtures(FEATURE_FIXTURES)
        for name in STANDALONE_GUI:
            self.standalone_gui_check(name)
        self.prepare_native(full=True)
        self.retain_binaries(sorted({target for check in ENGINE_PROCESS_CHECKS.values() for target in check.support}))
        # The workbook suite consumes core's generated fixtures. Each
        # configuration completes both before another can replace their files.
        # Independent record suites follow that adjacent pair, still serially.
        for config in configurations(self.args.config):
            for module in ENGINE_MODULES:
                binary = test_binary(module, config)
                self.retain_binaries([binary])
                check = ENGINE_PROCESS_CHECKS.get(module)
                if not check or not check.primary:
                    self.record(self.command(f"{module}-{config}", ["./" + binary], "PASS:"))
                ensure_unchanged(self.summary["binaries"])
                if module=="workbook_codec_native":
                    self.codec_native_checks(binary,config)
                self.engine_process_check(module,config,binary)
        self.background_check()
        self.record(self.command("gpu-sanitize", ["./build/tests/test_paint-native-debug"], "PASS: 110 native paint checks"))
        self.native_scenarios([row[0] for row in NATIVE])

    def native_scenarios(self, names):
        """Keep local scenarios ordered; lock only their shared OS services."""
        data = self.root / "data/downloads/yellow_tripdata_2025-01.parquet"
        inventory = {row[0]: row for row in NATIVE}
        for selected in dict.fromkeys(names):
            name, stages, frames, marker = inventory[selected]
            if name == "nyc" and not data.is_file():
                self.summary["results"].append({"name": "native-nyc", "status": "skipped", "expected_stages": stages,
                                                "reason": "NYC dataset missing; use --require-data to make this a failure", "path": str(data)})
                print("SKIP   native-nyc: dataset missing (8 stages not covered)", flush=True)
                self.write_summary()
                continue
            ensure_unchanged(self.summary["binaries"])
            flag = "--smoke" if name == "base" else f"--{name}-smoke"
            with native_resource_guard(scenario=name):
                self.record(self.command("native-" + name,
                                     ["./" + self.native_binary(), flag, "--frames", str(frames),
                                      "--screenshot", str(self.log_dir / (name + ".png"))], marker,
                                     native=True, base=name == "base", stages=layout_stage_count(0,stages) if name == "layout" else stages,
                                     completion_pattern=layout_completion_pattern(0, stages) if name == "layout" else None))
            ensure_unchanged(self.summary["binaries"])

    def native(self):
        if self.args.require_data and "nyc" in self.args.scenario and not (self.root / "data/downloads/yellow_tripdata_2025-01.parquet").is_file():
            raise RuntimeError("Required NYC dataset missing")
        self.prepare_native()
        # Independent table scenarios consume these engine fixtures. Generate
        # them only when absent, once and under the worktree runner lock.
        if "table" in self.args.scenario and any(not (self.root / "build/tests/generated" / name).is_file()
                                                   for name in ("million.parquet", "odd'name.parquet", "fixture.duckdb")):
            if self.args.no_build:
                raise RuntimeError("Missing table fixtures; omit --no-build to generate them")
            binary = test_binary("core", "release")
            self.build([binary], "build-table-fixtures")
            self.retain_binaries([binary])
            self.record(self.command("prepare-table-fixtures", ["./" + binary], "PASS:"))
        self.background_check()
        self.native_scenarios(self.args.scenario)

    def run(self):
        print("Logs: " + str(self.log_dir), flush=True)
        try:
            with operation_lease(self.root):
                if self.args.tier == "fast":
                    self.headless(self.args.modules or FAST_MODULES)
                elif self.args.tier == "feature":
                    self.feature()
                elif self.args.tier == "affected":
                    self.affected()
                elif self.args.tier == "native":
                    self.native()
                elif self.args.tier == "engines":
                    self.engines()
                elif self.args.tier == "records":
                    self.records()
                elif self.args.tier == "formats":
                    self.formats()
                else:
                    self.full()
                ensure_unchanged(self.summary["binaries"])
                current_source = source_metadata(self.root)
                if current_source["source_sha256"] != self.summary["source"]["source_sha256"]:
                    raise RuntimeError("Source changed during this run; results are not a coherent acceptance checkpoint")
                self.summary["status"] = "passed-with-skips" if any(r["status"] == "skipped" for r in self.summary["results"]) else "passed"
        except (OSError, RuntimeError, KeyboardInterrupt) as error:
            self.summary["status"], self.summary["error"] = "failed", str(error) or "Interrupted"
            print("FAILED: " + self.summary["error"], file=sys.stderr)
        finally:
            self.summary["binaries_after"] = {path: sha256(path) if Path(path).is_file() else None for path in self.summary["binaries"]}
            changed = [path for path, digest in self.summary["binaries"].items() if self.summary["binaries_after"][path] != digest]
            if changed:
                self.summary["status"] = "failed"
                self.summary["error"] = "Test binaries changed during the run: " + ", ".join(changed)
            self.summary["finished_utc"] = utc_now()
            self.write_summary()
            passed = sum(r.get("expected_stages") or 0 for r in self.summary["results"] if r["status"] == "passed")
            print(f"{self.summary['status'].upper()}: {self.summary['elapsed_seconds']:.3f}s; {passed} application stages; {self.log_dir / 'summary.json'}", flush=True)
        return 0 if self.summary["status"].startswith("passed") else 1


def parser():
    result = argparse.ArgumentParser(description=__doc__, epilog="Native windows and clipboard checks run serially under a cross-worktree lock. --jobs only overlaps already-built read-only headless tests. No packages are installed.")
    result.add_argument("tier", nargs="?", choices=("feature", "fast", "records", "formats", "engines", "affected", "native", "full"), default="feature")
    result.add_argument("--config", choices=("release", "sanitize", "both"), default="both", help="headless build configuration; native uses sanitizer when both (default: both)")
    result.add_argument("--edit", action="store_true", help="affected/native: use the isolated -O0 ASan/UBSan App; headless configurations stay unchanged")
    result.add_argument("--modules", nargs="+", choices=FAST_MODULES, help="read-only headless modules for fast/affected; full always runs all")
    result.add_argument("--feature", choices=tuple(dict.fromkeys((*LAYOUT_PROFILES, *FEATURES))), help="registered local feature, or existing layout profile for affected")
    result.add_argument("--scenario", nargs="+", choices=tuple(row[0] for row in NATIVE), help="native: run only the named existing workflows, serially")
    result.add_argument("--jobs", type=int, choices=(1, 2), default=1)
    result.add_argument("--no-build", action="store_true", help="use existing binaries/fixtures; hashes are retained, freshness is the caller's responsibility")
    result.add_argument("--visible", action="store_true", help="deliberately show/activate native QA windows; default hidden framebuffer rendering")
    result.add_argument("--require-data", action="store_true", help="native/full: fail rather than skip NYC if its downloaded Parquet is absent")
    result.add_argument("--timeout", type=float, default=300, help="per-command seconds, native minimum900 (default:300)")
    result.add_argument("--log-dir", help="dedicated output directory; default build/tests/results/runs/<UTC>-<tier>-<pid>")
    result.add_argument("--plan", action="store_true", help="feature: print local/deferred coverage without building or launching")
    result.add_argument("--list", action="store_true", help="print modules, focused profiles and native inventory without building/running")
    return result


def main(argv=None):
    argument_parser = parser()
    args = argument_parser.parse_args(argv)
    if args.edit and args.tier not in ("affected", "native"):
        argument_parser.error("--edit applies only to affected/native")
    if args.edit and args.config == "release":
        argument_parser.error("--edit requires --config sanitize or both; the edit App uses ASan/UBSan")
    if args.plan:
        if args.tier != "feature" or args.feature not in FEATURES:
            argument_parser.error("--plan requires feature --feature <registered feature>")
        print(json.dumps(feature_plan(args.feature), indent=2))
        return 0
    if not args.list and args.tier == "feature" and (args.feature not in FEATURES or args.modules):
        argument_parser.error("feature requires a registered --feature and does not accept --modules; inspect --plan")
    if args.tier == "affected" and args.feature not in LAYOUT_PROFILES:
        argument_parser.error("affected requires an existing layout profile")
    if args.list:
        print("Feature slices: " + ", ".join(FEATURES))
        print("Read-only modules: " + ", ".join(FAST_MODULES))
        print("Engines: all registered ordered engine suites and process gates; App/GPU scenarios remain separate")
        print("Records: " + ", ".join(RECORD_MODULES) + "; sequential native engine checks without GUI or shared fixtures")
        print("Formats: workbook --formats-only; sequential format/save checks without GUI or core fixture regeneration")
        for name, (start, end, _) in LAYOUT_PROFILES.items():
            print(f"Affected {name}: layout {start}–{end-1} ({layout_stage_count(start,end)} stages)")
        for name, stages, frames, _ in NATIVE:
            print(f"Full {name}: {layout_stage_count(0,stages) if name == 'layout' else stages} stages, {frames} frame timeout")
        for name, (_, _, _, stages) in STANDALONE_GUI.items():
            print(f"Full standalone {name}: {stages} application stages")
        native_stages = sum(layout_stage_count(0,row[1]) if row[0] == "layout" else row[1] for row in NATIVE)
        print(f"Full native scenarios: {native_stages} application stages +110 GPU checks; NYC8 optional unless --require-data")
        print(f"Full total: {native_stages+sum(row[3] for row in STANDALONE_GUI.values())} application stages plus About adapter checks")
        return 0
    if args.tier == "affected" and not args.feature:
        argument_parser.error("affected requires --feature (see --list)")
    if args.tier in ("full", "records", "formats", "engines") and args.modules:
        argument_parser.error(args.tier + " does not accept --modules")
    if args.tier in ("records", "formats", "engines") and args.jobs != 1:
        argument_parser.error(args.tier + " requires --jobs 1; engine checks run sequentially")
    if args.tier == "native" and (not args.scenario or args.modules):
        argument_parser.error("native requires --scenario and does not accept --modules")
    if args.scenario and args.tier != "native":
        argument_parser.error("--scenario applies only to native")
    if args.feature and args.tier not in ("affected", "feature"):
        argument_parser.error("--feature applies only to feature/affected")
    if args.require_data and args.tier not in ("native", "full"):
        argument_parser.error("--require-data applies only to native/full")
    if args.visible and args.tier not in ("affected", "native", "full"):
        argument_parser.error("--visible applies only to affected/native/full")
    if args.timeout <= 0:
        argument_parser.error("--timeout must be positive")
    # Recursive recipes expose the jobserver, so Make also invokes this process
    # under -n/-t/-q. Preserve those non-executing modes before creating a runner.
    flags=os.environ.get("MAKEFLAGS", "").split()
    modes=flags[0] if flags and not flags[0].startswith("-") else ""
    if os.environ.get("MAKELEVEL", "0")!="0" and set(modes)&set("ntq"):
        print("Make inspection mode: tests were not built or launched.")
        return 1 if "q" in modes else 0
    return Runner(args).run()


if __name__ == "__main__":
    sys.exit(main())
