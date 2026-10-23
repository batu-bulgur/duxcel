#!/usr/bin/env python3
"""Exercise private journal write/sync/close errors without filling a real disk."""
import argparse
import os
from pathlib import Path
import re
import resource
import signal
import subprocess
import sys
import tempfile
import time

from workspace import ROOT, fingerprint, ensure_unchanged


def environment(library=None, path=None, mode=None, disarmed=False):
    env = os.environ.copy()
    for name in ("DUXCEL_JOURNAL_FAULT", "DUXCEL_JOURNAL_FAULT_PATH", "DUXCEL_JOURNAL_FAULT_DISARMED"):
        env.pop(name, None)
    env["ASAN_OPTIONS"] = env.get("ASAN_OPTIONS", "") + ":halt_on_error=1"
    env["UBSAN_OPTIONS"] = env.get("UBSAN_OPTIONS", "") + ":halt_on_error=1:print_stacktrace=1"
    if library:
        key = "DYLD_INSERT_LIBRARIES" if sys.platform == "darwin" else "LD_PRELOAD"
        env[key] = str(library)
        env["DUXCEL_JOURNAL_FAULT_PATH"] = str(path)
        env["DUXCEL_JOURNAL_FAULT"] = mode
        if disarmed:
            env["DUXCEL_JOURNAL_FAULT_DISARMED"] = "1"
    return env


def run(binary, *arguments, env=None, preexec_fn=None, marker):
    result = subprocess.run([str(binary), *map(str, arguments)], cwd=ROOT,
                            capture_output=True, text=True, timeout=20,
                            env=env or environment(), preexec_fn=preexec_fn)
    output = result.stdout + result.stderr
    if result.returncode or not re.search(r"^" + re.escape(marker) + r"$", output, re.M):
        raise RuntimeError(f"Journal fault helper failed: {result.returncode}\n{output}")
    if re.search(r"AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:", output):
        raise RuntimeError("Sanitizer diagnostic in journal fault helper:\n" + output)
    return output


def fault_marker(output, mode):
    required = ("JOURNAL_FAULT partial-write", "JOURNAL_FAULT ENOSPC") if mode == "partial" else \
               ("JOURNAL_FAULT EIO",) if mode in ("eio", "fsync", "close") else ("JOURNAL_FAULT ENOSPC",)
    if not all(line in output.splitlines() for line in required):
        raise RuntimeError("The requested filesystem fault was not exercised:\n" + output)


def file_limit():
    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
    resource.setrlimit(resource.RLIMIT_FSIZE, (1, 1))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def storage_cases(binary, library, directory):
    checks = 0
    for mode in ("enospc", "eio", "partial", "fsync", "close", "file-limit"):
        path = Path(directory) / (mode + ".duckdb")
        run(binary, "--init", path, marker="PASS: journal initialized"); checks += 1
        if mode == "file-limit":
            run(binary, "--expect-failure", path, preexec_fn=file_limit,
                marker="PASS: journal write failure preserved receipt")
        else:
            output = run(binary, "--close-fault" if mode == "close" else "--expect-failure", path,
                         env=environment(library, path, "eio" if mode == "close" else mode, mode == "close"),
                         marker="PASS: journal close fault completed" if mode == "close" else "PASS: journal write failure preserved receipt")
            fault_marker(output, mode)
        checks += 1
        output = run(binary, "--inspect-any", path, marker="PASS: complete journal state"); checks += 1
        head = re.search(r"^STATE durable-head=([02])$", output, re.M)
        if not head or (mode == "close" and head[1] != "2"):
            raise RuntimeError("A journal fault left an incomplete durable state:\n" + output)
        for _ in range(2):
            run(binary, "--retry", path, marker="PASS: journal commit"); checks += 1
            run(binary, "--inspect", path, 2, marker="PASS: complete journal state"); checks += 1
        print(f"Filesystem {mode}: complete head {head[1]}, unchanged failure receipt, exact retry", flush=True)
    return checks


def queue_cases(binary, library, directory):
    checks = 0
    for fresh in (False, True):
        for mode in ("enospc", "eio", "partial", "fsync"):
            name = ("initial-" if fresh else "existing-") + mode
            path = Path(directory) / (name + ".duckdb")
            if not fresh:
                run(binary, "--init", path, marker="PASS: queue journal initialized"); checks += 1
            output = run(binary, "--fault", path, env=environment(library, path, mode),
                         marker="PASS: queue I/O retry retained exact records"); checks += 1
            fault_marker(output, mode)
            run(binary, "--inspect", path, marker="PASS: queue journal complete"); checks += 1
            print(f"Queue {name}: retained two owned records, repaired retry, exact reopened state", flush=True)
    for mode in ("enospc", "eio", "partial", "fsync"):
        path = Path(directory) / ("origin-" + mode + ".duckdb")
        output = run(binary, "--origin-fault", path, env=environment(library, path, mode, True),
                     marker="PASS: queue origin I/O retry retained exact records"); checks += 1
        fault_marker(output, mode)
        run(binary, "--inspect-origin", path, marker="PASS: queue origin journal complete"); checks += 1
        print(f"Queue origin-{mode}: immutable origin and two exact records survive repair/retry", flush=True)
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--fault-library", required=True, type=Path)
    parser.add_argument("--queue", action="store_true")
    args = parser.parse_args()
    binary, library = (ROOT / args.binary).resolve(), (ROOT / args.fault_library).resolve()
    provenance = fingerprint([binary, library]); started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="journal-faults-", dir=ROOT / "build/tests") as directory:
        checks = (queue_cases if args.queue else storage_cases)(binary, library, directory)
    ensure_unchanged(provenance)
    print(f"Filesystem fault duration: {time.monotonic() - started:.3f}s")
    print(f"PASS: journal {'queue ' if args.queue else ''}filesystem failures ({12 if args.queue else 6} cases, {checks} checks)")


if __name__ == "__main__":
    main()
