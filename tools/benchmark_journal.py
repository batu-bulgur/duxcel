#!/usr/bin/env python3
"""Run the native journal workload once and retain engine/binary provenance."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from workspace import ROOT, fingerprint, ensure_unchanged


def linked_engine(binary):
    if sys.platform == "darwin":
        result = subprocess.run(["otool", "-L", str(binary)], text=True, capture_output=True, check=True)
        names = [line.strip().split(" (", 1)[0] for line in result.stdout.splitlines()[1:] if "duckdb" in line.lower()]
    else:
        result = subprocess.run(["ldd", str(binary)], text=True, capture_output=True, check=True)
        names = [line.split("=>", 1)[-1].strip().split()[0] for line in result.stdout.splitlines() if "duckdb" in line.lower()]
    if len(names) != 1 or not Path(names[0]).is_absolute() or not Path(names[0]).is_file():
        raise RuntimeError("Expected exactly one resolved native DuckDB library")
    return Path(names[0]).resolve()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    binary, source = (ROOT / args.binary).resolve(), (ROOT / args.input).resolve()
    library = linked_engine(binary)
    provenance = fingerprint([binary, library, source])
    env = os.environ.copy()
    for key in ("DYLD_INSERT_LIBRARIES", "LD_PRELOAD"):
        env.pop(key, None)
    result = subprocess.run([str(binary), str(source)], cwd=ROOT, env=env, text=True, capture_output=True, timeout=180)
    print(result.stdout, end="", flush=True)
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    if result.returncode or re.search(r"AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:", result.stdout + result.stderr):
        raise RuntimeError("Native journal workload failed")
    if not re.search(r"^PASS: journal performance \([1-9]\d* checks\)$", result.stdout, re.M):
        raise RuntimeError("Native journal workload did not complete")
    runtime = re.search(r"^ENGINE_LIBRARY (.+)$", result.stdout, re.M)
    version = re.search(r"^ENGINE_VERSION (.+)$", result.stdout, re.M)
    metrics = re.search(r"^METRICS (.+)$", result.stdout, re.M)
    if not runtime or Path(runtime[1]).resolve() != library or not version or not metrics:
        raise RuntimeError("Runtime engine provenance is missing or disagrees with linked library")
    ensure_unchanged(provenance)
    report = {"engine_version": version[1], "provenance": provenance, "metrics": json.loads(metrics[1]),
              "limits": "C producer API timings, not physical input/frame latency. Legacy paste is checkpointed, not journal-admitted. Sanitizer timings are not tuning evidence."}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(report, indent=2) + "\n")
    temporary.replace(args.output)
    print("PASS: journal workload provenance and measurements")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
