#!/usr/bin/env python3
"""Run isolated lane tests; serialize only explicitly selected compilation.

Initialize once with --checkout PATH --lane NAME --init, then use the same
arguments followed by -- COMMAND ARG... . The existing test runner continues to
own suite selection, fixture order, hidden launches and the desktop lock.
"""
import argparse
import contextlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

from workspace import (FileLock, checkout_gate_path, make_jobserver_fds, repository_lock_path,
                       utc_now, operation_lease, inherited_lease_fds)


def private_outputs(root):
    # Make installs these read-only source assets beside private executables.
    # They belong to this checkout, never to another lane's mutable output.
    asset_links = {root / "build" / name / "assets" for name in
                   ("bin", "tests", "review", "review-sanitize", "edit-sanitize", "lifecycle")}
    for name in ("build", ".cache"):
        folder = root / name
        if folder.is_symlink():
            raise RuntimeError(f"Lane output must be private, not a symlink: {folder}")
        if folder.exists():
            for directory, folders, files in os.walk(folder):
                for child in folders + files:
                    path = Path(directory) / child
                    if path.is_symlink() and not path.resolve().is_relative_to(folder.resolve()):
                        if ((path in asset_links or (path.name == "assets" and path.is_relative_to(root / "build/tests/lanes"))) and path.resolve() == root / "assets"
                                and not (root / "assets").is_symlink()):
                            continue
                        raise RuntimeError(f"Lane output points outside its private directory: {path}")


def identity(root, lane):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,31}", lane):
        raise RuntimeError("Lane must contain 1-32 letters, digits, underscores or hyphens")
    if root != Path(__file__).resolve().parents[1]:
        raise RuntimeError("Expected checkout does not match this lane runner's checkout")
    private_outputs(root)
    return {"checkout": str(root), "lane": lane,
            "repository_lock": str(repository_lock_path(root))}


def run_command(command, root, env):
    # Keep ownership until the command exits even when the wrapper is cancelled.
    # A separate group lets cancellation reach recursive Make/test descendants.
    child = subprocess.Popen(command, cwd=root, env=env, start_new_session=True,
                             pass_fds=tuple(set(make_jobserver_fds(env) + inherited_lease_fds(env))))
    def forward(number, frame):
        try:
            os.killpg(child.pid, number)
        except ProcessLookupError:
            pass
    previous = {number: signal.signal(number, forward) for number in (signal.SIGINT, signal.SIGTERM)}
    try:
        return child.wait()
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)


def execute(root, lane, command, initialize=False, build=False):
    expected = identity(root, lane)
    marker = root / "build/lane.json"
    with FileLock(checkout_gate_path(root), "Lane operation", shared=True):
        if initialize:
            if marker.exists() and json.loads(marker.read_text()) != expected:
                raise RuntimeError("Existing lane marker differs; refusing reassignment")
            marker.parent.mkdir(parents=True, exist_ok=True)
            marker.write_text(json.dumps(expected, indent=2) + "\n")
            return 0
        if command and Path(command[0]).name in {"make", "gmake", "clang", "cc", "gcc"} and not build:
            raise RuntimeError("Direct compilation requires --build; use tools/test.py for mixed build/test workflows")
        if build and (any("test.py" == Path(arg).name for arg in command) or any(arg.startswith("test-") for arg in command[1:])):
            raise RuntimeError("--build accepts compilation only; test workflows acquire their own build leases")
        if not command:
            raise RuntimeError("Supply a command after --")
        if not marker.is_file() or json.loads(marker.read_text()) != expected:
            raise RuntimeError("Missing or mismatched lane marker; initialize this checkout first")
        queued = utc_now()
        with operation_lease(root), (operation_lease(root, compilation=True) if build else contextlib.nullcontext()):
            private_outputs(root)
            if json.loads(marker.read_text()) != expected:
                raise RuntimeError("Lane marker changed while waiting for operation ownership")
            env = os.environ.copy()
            env.update(DUXCEL_TEST_INSTANCE="1", DUXCEL_TEST_VISIBLE="0",
                       DUXCEL_BUILD_LANE=lane, DUXCEL_LANE=lane, BUILD_JOBS="8")
            receipt = dict(expected, command=command, pid=os.getpid(), queued_utc=queued,
                           started_utc=utc_now(), test_instance=True, build_jobs=8, compilation=build)
            result_dir = root / "build/tests/results/lanes"
            result_dir.mkdir(parents=True, exist_ok=True)
            output = result_dir / f"{lane}-{os.getpid()}-{time.time_ns()}.json"
            started = time.monotonic()
            try:
                receipt["returncode"] = run_command(command, root, env)
            except OSError as error:
                receipt.update(returncode=127, error=str(error))
            finally:
                receipt.update(finished_utc=utc_now(), elapsed_seconds=time.monotonic()-started)
                output.write_text(json.dumps(receipt, indent=2) + "\n")
                print(f"Lane receipt: {output}", file=sys.stderr)
            return receipt["returncode"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", type=Path, required=True)
    parser.add_argument("--lane", required=True)
    parser.add_argument("--init", action="store_true")
    parser.add_argument("--build", action="store_true", help="Hold the repository lease for a compilation-only command")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    try:
        return execute(args.checkout.resolve(), args.lane, command, args.init, args.build)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
