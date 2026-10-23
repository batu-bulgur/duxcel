#!/usr/bin/env python3
"""Crash only an owned hidden TEST App, then verify exclusion and native recovery."""
import argparse
import contextlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time
from workspace import ROOT, FileLock, native_lock_path, fingerprint, ensure_unchanged, sha256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    binary = (ROOT / args.binary).resolve()
    provenance = fingerprint([binary])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    evidence = args.output.with_suffix("")
    evidence.mkdir(parents=True, exist_ok=True)
    processes = []
    handles = []
    def launch(role, root):
        env = os.environ.copy()
        for key in ("DUXCEL_TEST_VISIBLE", "DYLD_INSERT_LIBRARIES", "LD_PRELOAD"):
            env.pop(key, None)
        env.update(DUXCEL_TEST_INSTANCE="1", DUXCEL_TEST_RECOVERY_ROOT=str(root),
                   DUXCEL_TEST_RECOVERY_STARTUP="1", DUXCEL_TEST_RECOVERY_PROCESS_ROLE=role)
        log = evidence / (role + ".txt")
        handle = log.open("w")
        handles.append(handle)
        process = subprocess.Popen([str(binary)], cwd=ROOT, env=env, stdout=handle, stderr=subprocess.STDOUT)
        processes.append(process)
        return process, log
    def wait_for(process, log, pattern, timeout=45):
        deadline = time.monotonic() + timeout
        while True:
            content = log.read_text(errors="replace")
            if re.search(r"AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|FAIL", content):
                raise RuntimeError(f"Native recovery process failed: {log}")
            match = re.search(pattern, content, re.M)
            if match:
                return match
            if process.poll() is not None or time.monotonic() >= deadline:
                raise RuntimeError(f"Native recovery process did not reach its marker: {log}")
            time.sleep(.05)
    def finish(process, log):
        if process.wait(timeout=30):
            raise RuntimeError(f"Native recovery process failed at shutdown: {log}")
        content = log.read_text(errors="replace")
        if re.search(r"AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|FAIL", content):
            raise RuntimeError(f"Native recovery process diagnostics: {log}")
        if "Native test window: hidden, unfocused" not in content:
            raise RuntimeError("Recovery processes must remain hidden TEST instances")
    def artifacts(root):
        return {str(p.relative_to(root)): sha256(p) for p in root.rglob("*") if p.is_file()}
    def cleanup():
        for process in processes:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)
        for handle in handles:
            handle.close()
    with contextlib.ExitStack() as resources:
        resources.enter_context(FileLock(native_lock_path(), "Native recovery process tests"))
        directory=resources.enter_context(tempfile.TemporaryDirectory(prefix="recovery-process-", dir=ROOT / "build/tests"))
        # Stop owned processes before deleting their private working files.
        resources.callback(cleanup)
        root = Path(directory) / "sessions"
        writer, writer_log = launch("writer", root)
        ready = wait_for(writer, writer_log, r"^RECOVERY_WRITER_READY ([0-9a-f]{32}) (\d+)$")
        before = artifacts(root)
        if not before or not any(name.endswith("journal.duckdb") for name in before):
            raise RuntimeError("A committed real-App recovery generation is required")
        observer, observer_log = launch("observer", root)
        wait_for(observer, observer_log, r"^RECOVERY_ACTIVE_EXCLUDED$")
        finish(observer, observer_log)
        if artifacts(root) != before or writer.poll() is not None:
            raise RuntimeError("Inspection changed the live owner's artifacts or process")
        os.kill(writer.pid, signal.SIGKILL)
        if writer.wait(timeout=10) != -signal.SIGKILL:
            raise RuntimeError("The owned TEST writer was not killed as expected")
        if artifacts(root) != before:
            raise RuntimeError("Crash changed committed recovery bytes")
        restored, restore_log = launch("restore", root)
        result = wait_for(restored, restore_log, r"^RECOVERY_RESTORED ([0-9a-f]{32}) (\d+)$")
        finish(restored, restore_log)
        if result.groups() != ready.groups() or result[2] != "1":
            raise RuntimeError("Recovered identity/revision differs from the committed App state")
        if artifacts(root) != before:
            raise RuntimeError("Recovery inspection rewrote or removed committed artifacts")
        ensure_unchanged(provenance)
        args.output.write_text(json.dumps({"provenance": provenance, "document": result[1], "revision": result[2],
                                          "artifacts": before, "limits": "Private hidden native TEST processes only; no physical input or user instance interaction."}, indent=2) + "\n")
    print("PASS: native recovery crash and ownership (3 processes)")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
