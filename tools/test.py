#!/usr/bin/env python3
"""Bounded edit-loop checks and reproducible native regression; stdlib only."""

import argparse
import concurrent.futures
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
FAST_MODULES = (
    "cellreadout", "filterlabel", "grid", "selection_ladder", "source_scope", "objects", "brand", "format", "chartview", "chartencoding",
    "inspector", "stepview", "preview", "preview_input", "preview_input_browser", "textview", "textview_review", "statsview", "statsview_input", "statsview_input_review",
    "ui_update_input", "ui_update_input_browser", "ui_update_input_control", "guideview", "guideview_input", "guideview_input_review", "sortview_input", "sortview_input_review", "fontmetrics", "ui_font_fallback",
    "motion", "keytips", "choiceview", "paintlayer", "popup", "marksview", "page",
)
RECORD_MODULES = ("record_sql", "record_pages")
ENGINE_MODULES = ("core", "workbook", *RECORD_MODULES)
FORMATS_COMPLETION_PATTERN = r"^PASS: [1-9]\d* focused workspace formatting checks \(format round-trip, validation, scoped-save rejection and original-file preservation\)$"
# These are existing, independently bootstrapped layout checkpoints. The end is
# exclusive, as in DUXCEL_LAYOUT_TO. Never jump into arbitrary stateful stages.
LAYOUT_PROFILES = {
    "selection-start": (0, 45, "PASS: keyboard destination move"),
    "ribbon": (0, 64, "PASS: measured native ribbon"),
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
    "source-record-format": (734, 764, "PASS: native captured Verify Collect Commit"),
    "table-record-format": (764, 800, "PASS: native Table record formatting"),
    "source-result-format": (800, 824, "PASS: native verified exact-result and guarded styles"),
    "legacy-format-baseline": (824, 846, "PASS: native legacy format baselines"),
    "manual-format-import": (846, 861, "PASS: native manual format history stays manual"),
    "stats-input": (861, 867, "PASS: native Statistics Enter keypad Enter and Space"),
    "legacy-format-budget-retry": (867, 882, "PASS: native legacy format budget admission retries"),
    "inspector-scroll": (882, 898, "PASS: native independent Steps outer and inner scrolling"),
}
# Kept explicit: changes to terminal assertions/stage counts must be reviewed.
# Frames are a failure budget, not a claim that the app exits on completion.
NATIVE = (
    ("layout", 898, 24000, "PASS: native independent Steps outer and inner scrolling"),
    ("base", 6, 240, None),
    ("workspace", 4, 300, "PASS: native workspace save/load"),
    ("filter", 66, 6200, "PASS: clamped text-condition submenu"),
    ("pivot", 45, 1800, "PASS: categorical Values default to distinct count"),
    ("export", 20, 1000, "PASS: worker Parquet export"),
    ("count", 12, 900, "PASS: background count"),
    ("clipboard", 13, 900, "PASS: cached source/manual/pivot rectangular copy"),
    ("sort", 12, 900, "PASS: multi-column sort"),
    ("navigation", 13, 1200, "PASS: native sheet rename"),
    ("nyc", 8, 1000, "PASS: public NYC 3,475,226-row source"),
    ("document", 10, 700, "PASS: pending cell commit"),
    ("chart", 33, 1600, "PASS: pivot/chart names"),
    ("statistics", 31, 2500, "PASS: frame selection reports object identity"),
    ("table", 73, 3400, "PASS: independent same-sheet CSV/Parquet/DuckDB tables"),
    ("icon", 8, 500, "PASS: shared vector and prebuilt app icon previews"),
    ("query", 47, 3600, "PASS: actual native date-token menu changes Month to Year"),
    ("unique", 32, 1800, "PASS: structured unique rows on primary and independent sources"),
)
SANITIZER_FAILURE = re.compile(r"ERROR: (?:AddressSanitizer|LeakSanitizer)|runtime error:|SUMMARY: (?:AddressSanitizer|UndefinedBehaviorSanitizer)")


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def fingerprint(paths):
    return {str(path): sha256(path) for path in paths}


def ensure_unchanged(before):
    for path, expected in before.items():
        if not Path(path).is_file() or sha256(path) != expected:
            raise RuntimeError("Test binary changed during the run: " + path)


def source_metadata(root):
    paths = [root / "Makefile"]
    for directory in ("src", "tests", "tools", "web", "design/prototype/src"):
        suffixes={".c", ".h", ".m", ".inc", ".py", ".sh", ".mjs", ".js", ".jsx", ".css"}
        if directory=="web": suffixes.update((".html", ".json"))
        for folder,subdirectories,names in os.walk(root / directory):
            subdirectories[:]=[name for name in subdirectories if name not in {"generated", "results", "downloads", "node_modules"}]
            paths.extend(Path(folder)/name for name in names if Path(name).suffix in suffixes and (Path(folder)/name).is_file())
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(str(path.relative_to(root)).encode())
        digest.update(b"\0")
        digest.update(bytes.fromhex(sha256(path)))
    def git(*args):
        result = subprocess.run(["git", *args], cwd=root, text=True, capture_output=True)
        return result.stdout.strip() if result.returncode == 0 else None
    return {"commit": git("rev-parse", "HEAD"), "status": git("status", "--short"),
            "source_sha256": digest.hexdigest(), "source_files": len(paths)}


class FileLock:
    """Do not unlink lock files: that would permit locking a different inode."""

    def __init__(self, path, purpose):
        self.path, self.purpose, self.stream = Path(path), purpose, None

    def __enter__(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.stream = self.path.open("a+", encoding="utf-8")
        try:
            fcntl.flock(self.stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            self.stream.seek(0)
            owner = self.stream.read().strip() or "owner metadata unavailable"
            self.stream.close()
            self.stream = None
            raise RuntimeError(f"{self.purpose} is already running: {owner}") from None
        self.stream.seek(0)
        self.stream.truncate()
        json.dump({"pid": os.getpid(), "cwd": str(ROOT), "started_utc": utc_now(),
                   "command": sys.argv}, self.stream)
        self.stream.flush()
        return self

    def __exit__(self, *unused):
        fcntl.flock(self.stream.fileno(), fcntl.LOCK_UN)
        self.stream.close()


def native_lock_path():
    # gettempdir() can differ between Terminal, Codex and worktrees (TMPDIR).
    return Path("/tmp") / f"duxcel-native-tests-{os.getuid()}.lock"


def configurations(name):
    return ("release", "sanitize") if name == "both" else (name,)


def test_binary(module, config):
    return f"tests/test_{module}" + ("-debug" if config == "sanitize" else "")


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
        self.log_dir = Path(args.log_dir).resolve() if args.log_dir else root / "tests/results/runs" / f"{stamp}-{args.tier}-{os.getpid()}"
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
        env["DUXCEL_TEST_VISIBLE"] = "1" if getattr(self.args, "visible", False) else "0"
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
                process = subprocess.run(command, cwd=self.root, env=env, stdout=stream, stderr=subprocess.STDOUT,
                                         timeout=self.args.timeout if not native else max(self.args.timeout, 900))
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
        if self.args.edit:
            return "build/edit-sanitize/duxcel"
        return "duxcel" if self.args.config == "release" else "duxcel-debug"

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
        (self.root / "tests/generated").mkdir(parents=True, exist_ok=True)
        runs = [(config, test_binary("workbook", config)) for config in configurations(self.args.config)]
        self.build([binary for _, binary in runs], "build-formats")
        self.retain_binaries([binary for _, binary in runs])
        for config, binary in runs:
            ensure_unchanged(self.summary["binaries"])
            self.record(self.command(f"formats-{config}", ["./" + binary, "--formats-only"], "PASS:",
                                     completion_pattern=FORMATS_COMPLETION_PATTERN))
            ensure_unchanged(self.summary["binaries"])

    def prepare_native(self, full=False):
        validate_native_contract(self.root)
        targets = [self.native_binary(), "tests/test_background-native-debug", "design-fixture"]
        if full:
            targets += [test_binary(module, config) for config in configurations(self.args.config) for module in ENGINE_MODULES]
            targets += ["tests/test_paint-native-debug", "icons-check"]
        self.build(targets, "build-integration")
        # Snapshot helpers write to these existing conventional destinations.
        for name in ("workspace-layout", "native-authoring", "filter-keyboard", "multisort", "sheet-navigation",
                     "current-workspace", "chart-marks", "selection-statistics", "table-sources", "icons", "source-query", "unique-rows"):
            (self.root / "tests/results" / name).mkdir(parents=True, exist_ok=True)
        (self.root / "tests/generated/workspace-old").mkdir(parents=True, exist_ok=True)
        (self.root / "tests/generated/workspace-new").mkdir(parents=True, exist_ok=True)
        if not (self.root / "tests/generated/design-definition.dxls").is_file():
            raise RuntimeError("Missing canonical design fixture; omit --no-build")
        self.retain_binaries([self.native_binary(), "tests/test_background-native-debug"] + (["tests/test_paint-native-debug"] if full else []))

    def background_check(self):
        self.record(self.command("background-sanitize", ["./tests/test_background-native-debug"], "PASS: 477 background native checks"))

    def affected(self):
        self.headless(self.args.modules or FAST_MODULES)
        self.prepare_native()
        start, end, marker = LAYOUT_PROFILES[self.args.feature]
        frames = max(1200, (end - start + 10) * 48)
        with FileLock(native_lock_path(), "Native desktop tests"):
            ensure_unchanged(self.summary["binaries"])
            self.background_check()
            self.record(self.command("layout-" + self.args.feature,
                                    ["./" + self.native_binary(), "--layout-smoke", "--frames", str(frames),
                                     "--screenshot", str(self.log_dir / "final.png")], marker, native=True,
                                    env_extra={"DUXCEL_LAYOUT_FROM": str(start), "DUXCEL_LAYOUT_TO": str(end)}, stages=end-start,
                                    completion_pattern=layout_completion_pattern(start, end)))
            ensure_unchanged(self.summary["binaries"])

    def full(self):
        data = self.root / "tests/downloads/yellow_tripdata_2025-01.parquet"
        if self.args.require_data and not data.is_file():
            raise RuntimeError("Required NYC dataset missing: " + str(data))
        self.headless(FAST_MODULES)
        self.prepare_native(full=True)
        # The workbook suite consumes core's generated fixtures. Each
        # configuration completes both before another can replace their files.
        # Independent record suites follow that adjacent pair, still serially.
        for config in configurations(self.args.config):
            for module in ENGINE_MODULES:
                binary = test_binary(module, config)
                self.retain_binaries([binary])
                self.record(self.command(f"{module}-{config}", ["./" + binary], "PASS:"))
                ensure_unchanged(self.summary["binaries"])
        with FileLock(native_lock_path(), "Native desktop tests"):
            self.background_check()
            self.record(self.command("gpu-sanitize", ["./tests/test_paint-native-debug"], "PASS: 110 native paint checks"))
            self.native_scenarios([row[0] for row in NATIVE])

    def native_scenarios(self, names):
        """Caller owns the desktop lock; keep scenarios and clipboard serial."""
        data = self.root / "tests/downloads/yellow_tripdata_2025-01.parquet"
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
            self.record(self.command("native-" + name,
                                     ["./" + self.native_binary(), flag, "--frames", str(frames),
                                      "--screenshot", str(self.log_dir / (name + ".png"))], marker,
                                     native=True, base=name == "base", stages=stages,
                                     completion_pattern=layout_completion_pattern(0, stages) if name == "layout" else None))
            ensure_unchanged(self.summary["binaries"])

    def native(self):
        if self.args.require_data and "nyc" in self.args.scenario and not (self.root / "tests/downloads/yellow_tripdata_2025-01.parquet").is_file():
            raise RuntimeError("Required NYC dataset missing")
        self.prepare_native()
        # Independent table scenarios consume these engine fixtures. Generate
        # them only when absent, once and under the worktree runner lock.
        if "table" in self.args.scenario and any(not (self.root / "tests/generated" / name).is_file()
                                                   for name in ("million.parquet", "odd'name.parquet", "fixture.duckdb")):
            if self.args.no_build:
                raise RuntimeError("Missing table fixtures; omit --no-build to generate them")
            binary = test_binary("core", "release")
            self.build([binary], "build-table-fixtures")
            self.retain_binaries([binary])
            self.record(self.command("prepare-table-fixtures", ["./" + binary], "PASS:"))
        with FileLock(native_lock_path(), "Native desktop tests"):
            self.background_check()
            self.native_scenarios(self.args.scenario)

    def run(self):
        print("Logs: " + str(self.log_dir), flush=True)
        try:
            with FileLock(self.root / "tests/results/.runner.lock", "Tests in this worktree"):
                if self.args.tier == "fast":
                    self.headless(self.args.modules or FAST_MODULES)
                elif self.args.tier == "affected":
                    self.affected()
                elif self.args.tier == "native":
                    self.native()
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
    result.add_argument("tier", nargs="?", choices=("fast", "records", "formats", "affected", "native", "full"), default="fast")
    result.add_argument("--config", choices=("release", "sanitize", "both"), default="both", help="headless build configuration; native uses sanitizer when both (default: both)")
    result.add_argument("--edit", action="store_true", help="affected/native: use the isolated -O0 ASan/UBSan App; headless configurations stay unchanged")
    result.add_argument("--modules", nargs="+", choices=FAST_MODULES, help="read-only headless modules for fast/affected; full always runs all")
    result.add_argument("--feature", choices=tuple(LAYOUT_PROFILES), help="existing independently bootstrapped native slice; required for affected")
    result.add_argument("--scenario", nargs="+", choices=tuple(row[0] for row in NATIVE), help="native: run only the named existing workflows, serially")
    result.add_argument("--jobs", type=int, choices=(1, 2), default=1)
    result.add_argument("--no-build", action="store_true", help="use existing binaries/fixtures; hashes are retained, freshness is the caller's responsibility")
    result.add_argument("--visible", action="store_true", help="deliberately show/activate native QA windows; default hidden framebuffer rendering")
    result.add_argument("--require-data", action="store_true", help="native/full: fail rather than skip NYC if its downloaded Parquet is absent")
    result.add_argument("--timeout", type=float, default=300, help="per-command seconds, native minimum900 (default:300)")
    result.add_argument("--log-dir", help="dedicated output directory; default tests/results/runs/<UTC>-<tier>-<pid>")
    result.add_argument("--list", action="store_true", help="print modules, focused profiles and native inventory without building/running")
    return result


def main(argv=None):
    argument_parser = parser()
    args = argument_parser.parse_args(argv)
    if args.edit and args.tier not in ("affected", "native"):
        argument_parser.error("--edit applies only to affected/native")
    if args.edit and args.config == "release":
        argument_parser.error("--edit requires --config sanitize or both; the edit App uses ASan/UBSan")
    if args.list:
        print("Read-only modules: " + ", ".join(FAST_MODULES))
        print("Records: " + ", ".join(RECORD_MODULES) + "; sequential native engine checks without GUI or shared fixtures")
        print("Formats: workbook --formats-only; sequential format/save checks without GUI or core fixture regeneration")
        for name, (start, end, _) in LAYOUT_PROFILES.items():
            print(f"Affected {name}: layout {start}–{end-1} ({end-start} stages)")
        for name, stages, frames, _ in NATIVE:
            print(f"Full {name}: {stages} stages, {frames} frame timeout")
        print(f"Full total: {sum(row[1] for row in NATIVE)} application stages +110 GPU checks; NYC8 optional unless --require-data")
        return 0
    if args.tier == "affected" and not args.feature:
        argument_parser.error("affected requires --feature (see --list)")
    if args.tier in ("full", "records", "formats") and args.modules:
        argument_parser.error(args.tier + " does not accept --modules")
    if args.tier in ("records", "formats") and args.jobs != 1:
        argument_parser.error(args.tier + " requires --jobs 1; engine checks run sequentially")
    if args.tier == "native" and (not args.scenario or args.modules):
        argument_parser.error("native requires --scenario and does not accept --modules")
    if args.scenario and args.tier != "native":
        argument_parser.error("--scenario applies only to native")
    if args.feature and args.tier != "affected":
        argument_parser.error("--feature applies only to affected")
    if args.require_data and args.tier not in ("native", "full"):
        argument_parser.error("--require-data applies only to native/full")
    if args.visible and args.tier not in ("affected", "native", "full"):
        argument_parser.error("--visible applies only to affected/native/full")
    if args.timeout <= 0:
        argument_parser.error("--timeout must be positive")
    return Runner(args).run()


if __name__ == "__main__":
    sys.exit(main())
