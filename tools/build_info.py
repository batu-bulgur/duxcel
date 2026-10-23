#!/usr/bin/env python3
"""Generate immutable product metadata without invalidating unrelated objects."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess


def git(root, *args):
    try:
        result = subprocess.run(["git", "-C", str(root), *args], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, encoding="utf-8", errors="replace", timeout=10)
        return result.stdout.strip() if result.returncode == 0 else ""
    except (OSError, subprocess.TimeoutExpired):
        return ""


def metadata(root):
    revision = git(root, "rev-parse", "--verify", "HEAD")
    if not revision:
        return {"REVISION": "Unavailable (source archive)", "SUBJECT": "Revision information is unavailable.",
                "AUTHOR": "Unavailable", "COMMIT_DATE": "Unavailable", "KNOWN": 0, "DIRTY": 0,
                "LOG": "Change history is unavailable in this source archive."}
    return {"REVISION": revision, "SUBJECT": git(root, "log", "-1", "--format=%s"),
            "AUTHOR": git(root, "log", "-1", "--format=%an"),
            "COMMIT_DATE": git(root, "log", "-1", "--format=%aI"), "KNOWN": 1,
            "DIRTY": int(bool(git(root, "status", "--porcelain", "--untracked-files=normal"))),
            "LOG": git(root, "log", "-40", "--date=short", "--format=%h  %ad  %s%n    %an%n")}


def c_string(text):
    # UTF-8 bytes avoid invalid C universal escapes for control characters and
    # surrogate pairs, while fixed-width octal cannot consume the next digit.
    return '"' + ''.join(chr(b) if 32 <= b < 127 and b not in (34, 63, 92)
                         else '\\%03o' % b for b in text.encode("utf-8", errors="replace")) + '"'


def source_id(root):
    """Code/build identity also distinguishes successive uncommitted builds."""
    digest = hashlib.sha256()
    paths = [root / name for name in ("Makefile", "packaging/Info.plist", "tools/build_info.py", "tools/build_web.py")]
    paths += [path for path in (root / "src").rglob("*") if path.suffix in (".c", ".h", ".m", ".inc")]
    for path in sorted(paths):
        if path.is_file():
            digest.update(str(path.relative_to(root)).encode() + b"\0")
            digest.update(path.read_bytes())
    return digest.hexdigest()


def generate(root, output):
    values = dict(metadata(root))
    values["SOURCE"] = source_id(root)
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    signature = json.dumps({"metadata": values, "epoch": epoch}, sort_keys=True)
    values["TIME"] = datetime.datetime.fromtimestamp(int(epoch), datetime.timezone.utc).isoformat() if epoch else datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")
    # The signature is JSON and may contain */ in a commit subject. Store it as
    # a line comment, rather than allowing metadata to terminate a C comment.
    marker = "// " + signature
    previous = output.read_text(encoding="utf-8") if output.is_file() else ""
    if previous.splitlines()[:1] == [marker]:
        return False
    text = marker + "\n" + ''.join(f"#define DX_BUILD_{key} {c_string(value) if isinstance(value, str) else value}\n" for key, value in values.items())
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(output)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.root, args.output)


if __name__ == "__main__":
    main()
