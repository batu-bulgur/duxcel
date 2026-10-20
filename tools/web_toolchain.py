#!/usr/bin/env python3
"""Install/use pinned web tools inside this checkout's ignored .cache/web tree.

No shell activation, global packages, PATH changes, UI, browser or engine build.
Only bootstrap downloads; status is offline and run uses an installed toolchain.
"""
import argparse
import base64
import contextlib
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request
from workspace import FileLock

PROJECT = Path(__file__).resolve().parents[1]
TOOLS = {name: name + ".py" for name in ("emcc", "em++", "emar", "emranlib", "emmake", "emcmake")}


class ToolchainError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ToolchainError(message)


def sha256(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(data)
    return result.hexdigest()


def load_lock(path):
    lock = json.loads(Path(path).read_text())
    require(lock.get("schema") == "duxcel.web-toolchain" and type(lock.get("version")) is int and lock["version"] == 1, "Unsupported web toolchain lock")
    for name in ("emsdk", "raylib"):
        item = lock[name]
        require(re.fullmatch(r"[0-9a-f]{40}", item["commit"]) is not None, "Invalid immutable source commit: " + name)
        require(re.fullmatch(r"[0-9a-f]{64}", item["sha256"]) is not None, "Invalid archive checksum: " + name)
        require(item["url"] == "https://codeload.github.com/" + item["repository"] + "/tar.gz/" + item["commit"], "Source archive must match its official immutable commit")
    require(lock["emsdk"]["repository"] == "emscripten-core/emsdk" and lock["raylib"]["repository"] == "raysan5/raylib", "Only official toolchain sources are supported")
    require(re.fullmatch(r"\d+\.\d+\.\d+", lock["emsdk"]["version"]) is not None, "SDK requires an exact release version")
    require(re.fullmatch(r"[0-9a-f]{40}", lock["emsdk"]["release_hash"]) is not None, "Invalid SDK release hash")
    for key, item in lock["emsdk"]["artifacts"].items():
        require(re.fullmatch(r"[0-9a-f]{64}", item["sha256"]) is not None, "Invalid SDK artifact checksum")
        require(item["url"].startswith("https://storage.googleapis.com/webassembly/emscripten-releases-builds/") and "/" + lock["emsdk"]["release_hash"] + "/" in item["url"], "SDK artifact does not match its frozen release")
        require(Path(item["filename"]).name == item["filename"] and re.fullmatch(r"[A-Za-z0-9_.-]+", item["filename"]), "Invalid SDK archive filename: " + key)
    for name in ("@duckdb/duckdb-wasm", "apache-arrow"):
        item = lock["browser_packages"][name]
        require(re.fullmatch(r"\d+\.\d+\.\d+", item["version"]) is not None, "Browser packages require exact stable versions")
        require(item["tarball"].startswith("https://registry.npmjs.org/" + name + "/-/"), "Browser package must come from its official npm registry record")
        require(item["integrity"].startswith("sha512-") and len(base64.b64decode(item["integrity"][7:], validate=True)) == 64, "Invalid browser package integrity")
    return lock


def local_root(project, requested=None):
    project = Path(project).resolve()
    root = Path(requested) if requested else project / ".cache/web/toolchain"
    if not root.is_absolute():
        root = project / root
    base = project / ".cache/web"
    require(root != base and root.is_relative_to(base) and ".." not in root.parts, "Toolchain root must be below this checkout's .cache/web directory")
    current = project
    for part in root.relative_to(project).parts:
        current = current / part
        require(not current.is_symlink(), "Toolchain directories cannot redirect through symlinks")
    return root


def check_local_paths(root):
    for name in ("downloads", "emsdk", "raylib", "cache", "emsdk/downloads", "emsdk/upstream", ".emscripten", "state.json"):
        require(not (root / name).is_symlink(), "Toolchain state cannot redirect through symlinks: " + name)


def fetch(item, path, offline=False):
    path = Path(path)
    if path.exists():
        require(path.is_file() and not path.is_symlink() and sha256(path) == item["sha256"], "Cached archive checksum mismatch: " + str(path))
        return 0
    require(not offline, "Offline: missing pinned archive " + str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, name = tempfile.mkstemp(prefix="." + path.name + ".part-", dir=path.parent)
    os.close(descriptor)
    temporary = Path(name)
    size, digest = 0, hashlib.sha256()
    try:
        with urllib.request.urlopen(item["url"], timeout=60) as response, temporary.open("wb") as output:
            require(response.geturl().startswith("https://"), "Download redirected outside HTTPS")
            while True:
                data = response.read(1024 * 1024)
                if not data:
                    break
                size += len(data)
                require(size <= 1024 * 1024 * 1024, "Toolchain archive exceeds 1 GiB")
                output.write(data)
                digest.update(data)
        require(digest.hexdigest() == item["sha256"], "Downloaded archive checksum mismatch: " + item["url"])
        temporary.replace(path)
        return size
    finally:
        with contextlib.suppress(FileNotFoundError):
            temporary.unlink()


def unpack_source(archive, destination, item):
    destination = Path(destination)
    stamp = {key: item[key] for key in ("version", "commit", "sha256")}
    if destination.exists():
        require(not destination.is_symlink() and (destination / ".duxcel-source.json").is_file(), "Refusing to replace an unrecognized source directory: " + str(destination))
        require(json.loads((destination / ".duxcel-source.json").read_text()) == stamp, "Source pins changed; choose a new isolated --root")
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".source-", dir=destination.parent) as directory:
        staging = Path(directory) / "tree"
        staging.mkdir()
        total, top = 0, None
        with tarfile.open(archive, "r:gz") as source:
            for index, member in enumerate(source):
                require(index < 100000, "Source archive has too many entries")
                name = PurePosixPath(member.name)
                require(not name.is_absolute() and ".." not in name.parts and name.parts, "Unsafe source archive path")
                top = top or name.parts[0]
                require(name.parts[0] == top and (member.isdir() or member.isfile()), "Source archives require one root and ordinary files/directories")
                path = staging.joinpath(*name.parts[1:])
                if member.isdir():
                    path.mkdir(parents=True, exist_ok=True)
                    continue
                total += member.size
                require(total <= 512 * 1024 * 1024, "Source archive expands beyond 512 MiB")
                path.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as incoming, path.open("xb") as output:
                    shutil.copyfileobj(incoming, output, 1024 * 1024)
                path.chmod(member.mode & 0o755)
        require(top is not None, "Empty source archive")
        (staging / ".duxcel-source.json").write_text(json.dumps(stamp, indent=2) + "\n")
        staging.rename(destination)


def command(arguments, env=None, timeout=None):
    result = subprocess.run([str(arg) for arg in arguments], env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    require(result.returncode == 0, "Command failed: " + str(arguments[0]) + "\n" + result.stdout[-4096:])
    return result.stdout


def runtime(candidates, minimum, label):
    for candidate in dict.fromkeys(str(path) for path in candidates if path):
        try:
            candidate = shutil.which(os.path.expanduser(candidate)) or os.path.expanduser(candidate)
            output = command([candidate, "--version"], timeout=10)
            match = re.search(r"(?:Python\s+|v)(\d+)\.(\d+)\.(\d+)", output)
            if match and tuple(map(int, match.groups())) >= tuple(minimum):
                return {"path": str(Path(candidate).resolve()), "version": ".".join(match.groups())}
        except (OSError, ToolchainError, subprocess.SubprocessError):
            pass
    raise ToolchainError("Existing " + label + " >= " + ".".join(map(str, minimum)) + " required; pass its explicit executable path")


def environment(root, state):
    result = {key: value for key, value in os.environ.items() if not key.startswith(("EM_", "EMCC_", "EMSDK_", "BINARYEN_"))}
    result.update(EM_CONFIG=str(root / ".emscripten"), EM_CACHE=str(root / "cache"),
                  EMSDK=str(root / "emsdk"), EMSDK_PYTHON=state["python"]["path"],
                  EMSDK_NODE=state["node"]["path"], EMSDK_KEEP_DOWNLOADS="1",
                  EMSDK_NUM_CORES="1", EMCC_CORES="1", BINARYEN_CORES="1")
    return result


def configuration(root, node):
    upstream = root / "emsdk/upstream"
    fields = {"LLVM_ROOT": str(upstream / "bin"), "BINARYEN_ROOT": str(upstream),
              "EMSCRIPTEN_ROOT": str(upstream / "emscripten"), "NODE_JS": [node["path"]],
              "CACHE": str(root / "cache")}
    return "# Project-local generated Emscripten configuration.\n" + "".join(name + " = " + repr(value) + "\n" for name, value in fields.items())


def write_json(path, value):
    temporary = path.with_name(path.name + ".new")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


def compiler_version(root, state, expected, config=None):
    child = environment(root, state)
    if config is not None:
        child["EM_CONFIG"] = str(config)
    output = command([state["python"]["path"], root / "emsdk/upstream/emscripten/emcc.py", "--version"], child, timeout=15)
    match = re.search(r"^emcc\s+.*?\s(\d+\.\d+\.\d+)(?:\s|$)", output, re.MULTILINE)
    require(match and match.group(1) == expected, "Installed emcc version does not match " + expected + ": " + output[:256])
    return output.splitlines()[0]


def status(root, lock, lock_digest):
    check_local_paths(root)
    state_file = root / "state.json"
    if not state_file.is_file():
        return {"ready": False, "root": str(root), "reason": "Run bootstrap; no successful installation recorded", "network": False}
    state = json.loads(state_file.read_text())
    require(state.get("lock_sha256") == lock_digest, "Installed toolchain uses a different lock; run bootstrap in a new isolated --root")
    require((root / ".emscripten").read_text() == configuration(root, state["node"]), "Project-local compiler configuration changed; run bootstrap")
    version = compiler_version(root, state, lock["emsdk"]["version"])
    header = (root / "raylib/src/raylib.h").read_text()
    require(re.search(r'#define RAYLIB_VERSION\s+"' + re.escape(lock["raylib"]["version"]) + r'(?:\.0)?"', header), "Pinned raylib source version does not match the lock")
    return dict(state, ready=True, root=str(root), emcc=version, network=False)


def bootstrap(root, lock, lock_digest, offline=False, python=None, node=None):
    check_local_paths(root)
    root.mkdir(parents=True, exist_ok=True)
    # Do not let two bootstrap processes mutate the same SDK. A crashed process
    # leaves a clear lock directory for explicit recovery; never delete its work.
    guard = root / ".bootstrap-lock"
    require(not guard.exists(), "Bootstrap lock exists: " + str(guard))
    guard.mkdir()
    try:
        host = platform.system().lower() + "-" + platform.machine().lower()
        require(host in lock["emsdk"]["artifacts"], "No pinned SDK artifact for host " + host)
        state = {"schema": "duxcel.web-toolchain.install", "version": 1, "lock_sha256": lock_digest,
                 "host": host, "python": runtime([python] if python else [sys.executable] + [shutil.which("python" + value) for value in ("3.13", "3.14", "3.12", "3.11", "3.10")], lock["runtime"]["python_min"], "Python"),
                 "node": runtime([node] if node else [shutil.which("node")], lock["runtime"]["node_min"], "Node")}
        downloaded = 0
        for name in ("emsdk", "raylib"):
            item = lock[name]
            archive = root / "downloads" / (name + "-" + item["commit"] + ".tar.gz")
            downloaded += fetch(item, archive, offline)
            unpack_source(archive, root / name, item)
        releases = json.loads((root / "emsdk/emscripten-releases-tags.json").read_text())
        require(releases["releases"].get(lock["emsdk"]["version"]) == lock["emsdk"]["release_hash"], "SDK driver release map does not match the locked compiler")
        artifact = lock["emsdk"]["artifacts"][host]
        archive = root / "downloads" / artifact["filename"]
        downloaded += fetch(artifact, archive, offline)
        emcc = root / "emsdk/upstream/emscripten/emcc.py"
        if not emcc.is_file():
            require(not offline, "Offline: SDK compiler not installed; bootstrap online once")
            cache = root / "emsdk/downloads" / (lock["emsdk"]["release_hash"] + "-" + artifact["url"].rsplit("/", 1)[1])
            cache.parent.mkdir(parents=True, exist_ok=True)
            if not cache.exists():
                os.link(archive, cache)  # installer reuses the already verified immutable archive
            require(sha256(cache) == artifact["sha256"], "SDK installer cache checksum mismatch")
            output = command([state["python"]["path"], root / "emsdk/emsdk.py", "install",
                              "releases-" + lock["emsdk"]["release_hash"] + "-64bit"], environment(root, state))
            print(output, file=sys.stderr, end="")
        temporary = root / ".emscripten.new"
        temporary.write_text(configuration(root, state["node"]))
        try:
            state["emcc"] = compiler_version(root, state, lock["emsdk"]["version"], config=temporary)
        except Exception:
            temporary.unlink()
            raise
        temporary.replace(root / ".emscripten")
        state["downloaded_bytes"] = downloaded
        state["installed_at_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        write_json(root / "state.json", state)
        result = status(root, lock, lock_digest)
        result["network"] = downloaded > 0
        return result
    finally:
        guard.rmdir()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", help="isolated directory below this checkout's .cache/web (default: .cache/web/toolchain)")
    parser.add_argument("--offline", action="store_true", help="bootstrap uses cached sources and an already installed compiler only")
    parser.add_argument("--python", help="existing Python >=3.10 executable for emsdk/emcc")
    parser.add_argument("--node", help="existing Node >=18.3 executable")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("bootstrap")
    commands.add_parser("status")
    run = commands.add_parser("run")
    run.add_argument("tool", choices=TOOLS)
    run.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    try:
        lock_path = PROJECT / "web/toolchain.lock.json"
        lock, digest = load_lock(lock_path), sha256(lock_path)
        root = local_root(PROJECT, args.root)
        if args.command == "bootstrap":
            result = bootstrap(root, lock, digest, args.offline, args.python, args.node)
        else:
            result = status(root, lock, digest)
            if args.command == "run":
                require(result["ready"], "Run bootstrap first")
                require(not args.offline, "--offline applies to bootstrap/status, not later compiler port/cache downloads")
                script = root / "emsdk/upstream/emscripten" / TOOLS[args.tool]
                arguments = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
                return subprocess.call([result["python"]["path"], str(script), *arguments], env=environment(root, result))
        print(json.dumps(result, indent=2))
        return 0 if result["ready"] else 1
    except (OSError, ValueError, KeyError, tarfile.TarError, subprocess.SubprocessError) as error:
        print(json.dumps({"error": str(error)}), file=sys.stderr)
        return 2


if __name__ == "__main__":
    with FileLock(PROJECT / '.state/web-build.lock', 'Web builds in this worktree'):
        raise SystemExit(main())
