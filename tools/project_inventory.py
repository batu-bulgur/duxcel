#!/usr/bin/env python3
"""Classify every local file; fingerprint all project-owned/versionable files.

No files are deleted. Both the full inventory and the compact
authored/intentional-derivative manifest stay in the selected output directory.
"""
import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
from collections import defaultdict

ROOT = Path(__file__).resolve().parents[1]


def category(path):
    parts = path.parts
    name = path.name
    if name == ".DS_Store":
        return "os-metadata"
    if "node_modules" in parts:
        return "installed-dependency"
    if parts[:2] == ("build", "data"):
        return "cached-public-dataset"
    if parts[:3] == ("build", "tests", "generated"):
        return "generated-fixture"
    if parts[:3] == ("build", "tests", "results"):
        return "local-test-evidence"
    if parts[0] == "build" or "__pycache__" in parts or any(p.endswith(".dSYM") for p in parts):
        return "build-output"
    if name in ("duxcel", "duxcel-debug", "a.out") or (
        parts[0] == "tests" and name.startswith("test_") and path.suffix in ("", ".exe")
    ):
        return "build-output"
    if parts[:2] == ("docs", "design"):
        return "design-specification-and-provenance"
    if parts[:2] == ("docs", "validation"):
        return "versioned-acceptance-evidence"
    if parts[0] == "docs" or path.suffix == ".md":
        return "project-documentation"
    if name in ("brand_geometry.h", "brand_bitmap.h", "brand_images.h", "carbon_icons.h"):
        return "intentional-generated-source"
    if parts[0] == "src":
        return "native-source"
    if parts[:2] == ("tests", "fixtures"):
        return "authored-test-fixture"
    if parts[0] == "tests":
        return "test-source-and-tools"
    if parts[0] == "tools":
        return "developer-tool"
    if parts[0] == "web":
        return "browser-platform-source-and-tools"
    if parts[:2] in (("assets", "icons"), ("assets", "ui")):
        return "runtime-artwork-and-derivatives"
    if parts[:2] == ("assets", "fonts"):
        return "runtime-font-and-license"
    if parts[0] == "packaging":
        return "packaging-source-and-derivatives"
    if parts[0] == ".github":
        return "ci-configuration"
    if name in ("Makefile", ".gitignore", ".gitattributes"):
        return "build-and-vcs-configuration"
    return "review-unclassified"


def versionable_files():
    raw = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=ROOT
    )
    return {Path(os.fsdecode(p)) for p in raw.split(b"\0") if p}


def fingerprint(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(data)
    return digest.hexdigest()


def tsv(rows):
    output = io.StringIO()
    writer = csv.writer(output, delimiter="\t", lineterminator="\n")
    writer.writerow(("path", "category", "bytes", "sha256"))
    writer.writerows(rows)
    return output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("build/project-audit"))
    parser.add_argument("--check", action="store_true", help="Check compact manifest without writing it")
    args = parser.parse_args()
    destination = (ROOT / args.output).resolve()
    index = destination / "versionable-files.tsv"
    versionable = versionable_files()
    records, compact = [], []
    totals = defaultdict(lambda: {"files": 0, "bytes": 0})
    for base, dirs, files in os.walk(ROOT):
        dirs[:] = sorted(d for d in dirs if d != ".git")
        for name in sorted(files):
            absolute = Path(base) / name
            path = absolute.relative_to(ROOT)
            if absolute.is_relative_to(destination):
                continue  # Avoid self-referential output.
            info = absolute.lstat()
            kind = category(path)
            digest = fingerprint(absolute) if path in versionable and not absolute.is_symlink() else ""
            record = (path.as_posix(), kind, info.st_size, digest)
            records.append(record)
            totals[kind]["files"] += 1
            totals[kind]["bytes"] += info.st_size
            if path in versionable:
                compact.append(record)
    text = tsv(sorted(compact))
    if args.check:
        if not index.exists() or index.read_text() != text:
            parser.exit(1, "Project file manifest is stale; run tools/project_inventory.py.\n")
    else:
        destination.mkdir(parents=True, exist_ok=True)
        (destination / "full-inventory.tsv").write_text(tsv(sorted(records)))
        (destination / "summary.json").write_text(json.dumps(dict(sorted(totals.items())), indent=2) + "\n")
        index.write_text(text)
    print(f"Classified {len(records):,} local files; fingerprinted {len(compact)} versionable files.")
    unknown = [r[0] for r in records if r[1] == "review-unclassified"]
    if unknown:
        print("Unclassified (review, never auto-delete): " + ", ".join(unknown))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
