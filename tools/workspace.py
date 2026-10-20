"""Shared development filesystem, provenance and lock boundary.

No dependency on the test runner, product model or browser runtime. Build/test
CLIs use these functions; cleanup owns explicit output/cache paths only.
"""
import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

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
                             (Path(folder).is_relative_to(root / "tests/fixtures") and Path(name).suffix in {".csv", ".json"}))
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


def clean_outputs(full=False):
    """Remove disposable outputs while preserving fixtures, work, and lock inodes."""
    with FileLock(ROOT / ".state/test-runner.lock", "Tests in this worktree"), \
         FileLock(ROOT / ".state/web-build.lock", "Web builds in this worktree"), \
         FileLock(native_lock_path(), "Native desktop tests"):
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
