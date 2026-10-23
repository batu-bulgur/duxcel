#!/usr/bin/env python3
"""Kill private native journal writers at explicit commit barriers; no desktop."""
import argparse
from pathlib import Path
import select
import subprocess
import tempfile
import time

from workspace import ROOT, fingerprint, ensure_unchanged


def command(binary, *arguments):
    result = subprocess.run([str(binary), *map(str, arguments)], cwd=ROOT,
                            capture_output=True, text=True, timeout=20)
    if result.returncode or "PASS:" not in result.stdout:
        raise RuntimeError(f"Journal helper failed: {result.stdout}\n{result.stderr}")
    return result.stdout


def kill_at_barrier(binary, path, phase):
    process = subprocess.Popen([str(binary), "--hold-" + phase, str(path)], cwd=ROOT,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
    try:
        ready, _, _ = select.select([process.stdout], [], [], 20)
        if not ready:
            raise RuntimeError(f"Journal {phase} barrier did not arrive")
        line = process.stdout.readline().strip()
        if line != f"BARRIER {phase}-commit":
            raise RuntimeError(f"Unexpected journal barrier: {line}")
        process.kill()
        _, error = process.communicate(timeout=10)
        if process.returncode != -9:
            raise RuntimeError(f"Writer was not killed at its barrier: {error}")
    finally:
        if process.poll() is None:
            process.kill()
        process.communicate(timeout=10)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    args = parser.parse_args()
    binary = (ROOT / args.binary).resolve()
    provenance = fingerprint([binary])
    started = time.monotonic()
    checks = 0
    with tempfile.TemporaryDirectory(prefix="journal-process-", dir=ROOT / "build/tests") as directory:
        for phase, expected in (("before", 0), ("after", 2)):
            path = Path(directory) / (phase + ".duckdb")
            command(binary, "--init", path); checks += 1
            kill_at_barrier(binary, path, phase); checks += 1
            command(binary, "--inspect", path, expected); checks += 1
            command(binary, "--retry", path); checks += 1
            command(binary, "--inspect", path, 2); checks += 1
            command(binary, "--retry", path); checks += 1
            command(binary, "--inspect", path, 2); checks += 1
    ensure_unchanged(provenance)
    print(f"Journal crash/retry duration: {time.monotonic() - started:.3f}s")
    print(f"PASS: journal process boundaries ({checks} checks)")


if __name__ == "__main__":
    main()
