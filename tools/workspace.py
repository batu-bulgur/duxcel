"""Shared development filesystem, provenance and lock boundary.

No dependency on the test runner, product model or browser runtime. Build/test
CLIs use these functions; cleanup owns explicit output/cache paths only.
"""
import argparse
import contextlib
import datetime
import fcntl
import hashlib
import json
import os
import re
import stat
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def make_jobserver_fds(env=None):
    """Borrow inherited GNU Make pipe descriptors only for recursive Make calls."""
    flags=(os.environ if env is None else env).get("MAKEFLAGS", "")
    matches=re.findall(r"--jobserver-(?:fds|auth)=(\d+),(\d+)(?:\s|$)", flags)
    if not matches:
        return ()
    pair=tuple(map(int,matches[-1]))
    if pair[0]==pair[1] or min(pair)<3:
        return ()
    try:
        return pair if all(stat.S_ISFIFO(os.fstat(fd).st_mode) for fd in pair) else ()
    except OSError:
        return ()


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
    for directory in ("src", "tests", "tools", "web"):
        suffixes={".c", ".h", ".m", ".inc", ".py", ".sh", ".mjs", ".js", ".jsx", ".css"}
        if directory=="web": suffixes.update((".html", ".json"))
        for folder,subdirectories,names in os.walk(root / directory):
            subdirectories[:]=[name for name in subdirectories if name not in {"generated", "results", "downloads", "node_modules"}]
            paths.extend(Path(folder)/name for name in names
                         if (Path(name).suffix in suffixes or
                             (Path(folder).is_relative_to(root / "tests/fixtures") and Path(name).suffix in {".csv", ".json", ".sql"}))
                         and (Path(folder)/name).is_file())
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
    """Build locks are disposable; an OS-temp gate protects their lifetime.

    Every build-local lock holds the gate shared. Cleanup holds it exclusively
    before removing build/, so no process can open a replacement lock inode.
    """

    def __init__(self, path, purpose, *, shared=False):
        self.path, self.purpose, self.stream = Path(path), purpose, None
        self.shared, self.gate = shared, None

    def __enter__(self):
        build = next((p for p in self.path.resolve().parents if p.name == "build"), None)
        if build is not None:
            self.gate = FileLock(checkout_gate_path(build.parent), "Workspace cleanup", shared=True)
            self.gate.__enter__()
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            self.stream = self.path.open("a+", encoding="utf-8")
            try:
                mode = fcntl.LOCK_SH if self.shared else fcntl.LOCK_EX
                fcntl.flock(self.stream.fileno(), mode | fcntl.LOCK_NB)
            except BlockingIOError:
                self.stream.seek(0)
                owner = self.stream.read().strip() or "workspace operation active"
                raise RuntimeError(f"{self.purpose} is already running: {owner}") from None
            if not self.shared:
                self.stream.seek(0)
                self.stream.truncate()
                json.dump({"pid": os.getpid(), "cwd": str(ROOT), "started_utc": utc_now(),
                           "command": sys.argv}, self.stream)
                self.stream.flush()
        except BaseException:
            if self.stream:
                self.stream.close()
                self.stream = None
            if self.gate:
                self.gate.__exit__(None, None, None)
                self.gate = None
            raise
        return self

    def __exit__(self, *unused):
        fcntl.flock(self.stream.fileno(), fcntl.LOCK_UN)
        self.stream.close()
        if self.gate:
            self.gate.__exit__(None, None, None)


def checkout_gate_path(root=None):
    identity = hashlib.sha256(os.fsencode((ROOT if root is None else root).resolve())).hexdigest()[:20]
    return Path("/tmp") / f"duxcel-workspace-{os.getuid()}-{identity}.lock"


def native_lock_path():
    # gettempdir() can differ between Terminal, Codex and worktrees (TMPDIR).
    return Path("/tmp") / f"duxcel-native-tests-{os.getuid()}.lock"


def clean_outputs(full=False):
    """Remove all build output; preserve authored inputs and active ownership."""
    with FileLock(checkout_gate_path(), "Workspace is in use"), \
         FileLock(native_lock_path(), "Native desktop tests"), contextlib.ExitStack() as legacy:
        # Old checkouts used .state/. Refuse an old active runner too, then
        # retire the obsolete directory with the rest of the disposable output.
        for name in ("test-runner.lock", "web-build.lock"):
            path = ROOT / ".state" / name
            if path.exists():
                legacy.enter_context(FileLock(path, "Legacy workspace operation"))
        def remove(path):
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
            elif path.exists() or path.is_symlink():
                path.unlink()
        # Preserve earlier checkouts' downloaded inputs before deleting build/.
        # Never merge/overwrite independently populated directories implicitly.
        legacy = [(ROOT / "build/data", ROOT / "data/downloads")]
        if not full:
            legacy += [(ROOT / "build/web" / name, ROOT / ".cache/web" / name)
                       for name in ("toolchain", "raylib")]
        for old, new in legacy:
            if old.exists() and new.exists():
                raise RuntimeError(f"Both legacy and current cache exist; refusing cleanup: {old}, {new}")
        for old, new in legacy:
            if old.exists():
                new.parent.mkdir(parents=True, exist_ok=True)
                old.rename(new)
        remove(ROOT / "build")
        remove(ROOT / ".state")
        if full:
            remove(ROOT / ".cache")
            remove(ROOT / "web/node_modules")
        for name in ("duxcel", "duxcel-debug", "duxcel.new", "duxcel-debug.new", "a.out", "duxcel.dSYM", "duxcel-debug.dSYM", "tests/generated", "tests/results"):
            remove(ROOT / name)
        for path in (ROOT / "tests").glob("test_*"):
            if path.name.endswith(".dSYM"):
                remove(path)
            elif path.is_file() and path.suffix not in {".c", ".h", ".m", ".inc", ".py", ".sh"}:
                with path.open("rb") as stream:
                    magic = stream.read(4)
                if magic in {b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\x7fELF"}:
                    remove(path)
        for folder, directories, names in os.walk(ROOT):
            directories[:] = [name for name in directories if name not in {".git", "node_modules", "toolchain", "__pycache__"}]
            if ".DS_Store" in names:
                remove(Path(folder) / ".DS_Store")
        print("Cleaned generated outputs; " + ("dependency caches removed; downloaded data retained." if full else "dependency caches and downloaded data retained."))



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("clean", "distclean"))
    args = parser.parse_args()
    try:
        clean_outputs(full=args.action == "distclean")
        return 0
    except (OSError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
