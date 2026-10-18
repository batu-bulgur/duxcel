#!/usr/bin/env python3
"""Capture frozen prototype inspector measurements in an owned hidden browser.

This reads an existing dist/client; it never builds or writes the prototype.
Run this wrapper, rather than the internal Node driver, to own the desktop lock.
"""
import argparse
import datetime
import functools
import hashlib
import http.server
import json
import os
from pathlib import Path
import signal
import socketserver
import subprocess
import tempfile
import threading
import time

from test import FileLock, native_lock_path

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(prototype):
    """Record sources and prebuilt output independently, without a freshness claim."""
    paths = [prototype / "index.html", prototype / "package.json", prototype / "package-lock.json"]
    paths += sorted((prototype / "src").rglob("*"))
    paths += sorted((prototype / "dist/client").rglob("*"))
    result = {}
    for path in paths:
        if path.is_symlink():
            raise RuntimeError(f"Reference inputs must not be symlinks: {path}")
        if path.is_file():
            result[path.relative_to(prototype).as_posix()] = {"sha256": digest(path), "bytes": path.stat().st_size}
    for name in ("src/App.jsx", "src/styles.css", "dist/client/index.html"):
        if name not in result:
            raise RuntimeError(f"Missing existing reference input: {prototype / name}; no rebuild is performed")
    return result


class LoopbackServer(http.server.ThreadingHTTPServer):
    def server_bind(self):
        # Avoid HTTPServer's unnecessary reverse DNS on this explicit loopback.
        socketserver.TCPServer.server_bind(self)
        self.server_name = "127.0.0.1"
        self.server_port = self.server_address[1]


class ReadOnlyHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def stop_owned(process):
    if process is None or process.poll() is not None:
        return
    os.killpg(process.pid, signal.SIGTERM)
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=3)


def parse_options(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prototype-root", type=Path, default=ROOT / "design/prototype")
    parser.add_argument("--output", type=Path, help="New private output directory outside design/prototype")
    parser.add_argument("--dpr", type=int, choices=(1, 2), default=1)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--steps-scroll-only", action="store_true", help="Measure actual Steps anatomy and nested outer/inner scrolling; preserve the separate rail profile")
    modes.add_argument("--sql-only", action="store_true", help="Measure actual SQL flex anatomy, scroll ownership and re-entry lifetime without imposing overflow")
    return parser.parse_args(argv)


def main():
    options = parse_options()
    def interrupted(signum, frame):
        raise KeyboardInterrupt("Reference capture interrupted; cleaning owned processes")
    signal.signal(signal.SIGTERM, interrupted)
    prototype = options.prototype_root.resolve()
    output = (options.output or ROOT / "tests/results/runs" / (
        f"{datetime.datetime.now(datetime.timezone.utc):%Y%m%dT%H%M%S}-reference-{'sql' if options.sql_only else 'steps-scroll' if options.steps_scroll_only else 'rail'}-{os.getpid()}"
    )).resolve()
    if output == prototype or prototype in output.parents:
        raise RuntimeError("Capture output must be outside the read-only prototype")
    # Refuse reused output rather than mixing old screenshots with a new run.
    output.mkdir(parents=True, exist_ok=False)
    started = time.monotonic()
    before = inventory(prototype)
    frozen_reference = None
    if options.steps_scroll_only or options.sql_only:
        provider = ROOT / "docs/validation/checkpoints/2026-10-18-steps-sort-input/rail-reference/manifest.json"
        expected = json.loads(provider.read_text())
        if expected.get("status") != "passed" or any(before.get(name) != entry for name, entry in expected["inputs"].items()):
            raise RuntimeError("Inspector measurements require the exact source and prebuilt inputs of frozen reference030846-1269")
        frozen_reference = {"file": str(provider.relative_to(ROOT)), "sha256": digest(provider), "inputs": expected["inputs"]}
    report = {"status": "failed", "prototype": str(prototype), "inputs": before,
              "capture_tools": {name: digest(ROOT / "tools" / name) for name in (
                  "capture_reference_rail.py", "capture_reference_rail.mjs", "test_web_cdp.mjs", "cdp_keyboard.mjs", "test.py")},
              "source_asset_relationship": "Existing prebuilt dist/client and current source hashes are recorded independently. Relevant live DOM/style/layout is asserted; no general rebuild freshness or production-engine claim.",
              "desktop_visibility": "Owned headless Chromium and temporary profile; no user browser, clipboard or visible window"}
    if options.steps_scroll_only or options.sql_only:
        report["profile"] = "sql-only" if options.sql_only else "steps-scroll-only"
        report["frozen_reference"] = frozen_reference
    process = server = thread = None
    try:
        with FileLock(native_lock_path(), "hidden frozen-prototype rail reference capture"):
            with tempfile.TemporaryDirectory(prefix="duxcel-reference-rail-") as temporary:
                profile = Path(temporary) / "profile"
                profile.mkdir()
                server = LoopbackServer(("127.0.0.1", 0), functools.partial(
                    ReadOnlyHandler, directory=str(prototype / "dist/client")))
                thread = threading.Thread(target=server.serve_forever, daemon=True)
                thread.start()
                config = {"url": f"http://127.0.0.1:{server.server_port}/", "output": str(output),
                          "profile": str(profile), "desktop_lock_pid": os.getpid(), "dpr": options.dpr,
                          "steps_scroll_only": options.steps_scroll_only, "sql_only": options.sql_only,
                          "frozen_reference": frozen_reference,
                          "assets": {name[len('dist/client/'):]: entry for name, entry in before.items() if name.startswith('dist/client/')}}
                config_path = output / "driver-config.json"
                config_path.write_text(json.dumps(config, indent=2) + "\n")
                command = ["node", str(ROOT / "tools/capture_reference_rail.mjs"), str(config_path)]
                report["command"] = command
                with (output / "driver.txt").open("w") as stream:
                    process = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
                    try:
                        code = process.wait(timeout=120)
                    finally:
                        stop_owned(process)
                if code:
                    raise RuntimeError(f"Reference driver exited {code}; see {output / 'driver.txt'}")
                driver = json.loads((output / "reference.json").read_text())
                if driver.get("status") != "passed":
                    raise RuntimeError("Reference driver did not publish a passed result")
                report["driver"] = {"file": "reference.json", "sha256": digest(output / "reference.json"),
                                    "checks": driver["checks"], "captures": len(driver["states"])}
        report["inputs_unchanged"] = inventory(prototype) == before
        if not report["inputs_unchanged"]:
            raise RuntimeError("Frozen reference inputs changed during capture")
        report["status"] = "passed"
    except BaseException as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        stop_owned(process)
        if server:
            server.shutdown()
            server.server_close()
        if thread:
            thread.join(timeout=2)
        report["server_closed"] = server is not None and server.fileno() == -1
        report["elapsed_seconds"] = round(time.monotonic() - started, 3)
        report["inputs_unchanged"] = inventory(prototype) == before
        if not report["inputs_unchanged"]:
            report["status"] = "failed"
            report["error"] = "Frozen reference inputs changed during capture"
        report["capture_tools_unchanged"] = all(digest(ROOT / "tools" / name) == expected for name, expected in report["capture_tools"].items())
        if not report["capture_tools_unchanged"]:
            report["status"] = "failed"
            report["error"] = "Capture tool inputs changed during the run"
        (output / "manifest.json").write_text(json.dumps(report, indent=2) + "\n")
    if report["status"] != "passed":
        raise RuntimeError(report.get("error", "Reference capture failed"))
    print(json.dumps({"status": report["status"], "output": str(output), "elapsed_seconds": report["elapsed_seconds"]}))


if __name__ == "__main__":
    main()
