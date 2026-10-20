#!/usr/bin/env python3
"""Check review build separation and linked production absence without running apps.

Run after `make duxcel review`, or pass --shipping/--review for other completed,
unstripped artifacts. --contract-only checks Make's graph, not linked absence. --self-test
uses fixtures only: it does not build, launch an app or acquire the desktop.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MODULES = ("registry", "store", "input", "capture")
REQUIRED_SYMBOLS = ("dx_review_begin", "dx_review_store_write", "dx_review_input_blocked",
                    "dx_review_capture_create", "ui_review_discard_input", "dx_review_take_native_request")
MARKERS = (b"--review-smoke", b"DUXCEL_REVIEW_OUTBOX", b"duxcel.native-review", b"Report UI issue")
TEST_MARKERS = (b"dx_test_source_digest_checkpoint",)
SYMBOL = re.compile(r"(?<![A-Za-z0-9_])_?((?:dx|ui)_review_[A-Za-z0-9_]+)(?![A-Za-z0-9_])")


class ContractError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ContractError(message)


def _variables(text):
    """Read default assignments conservatively across all platform branches."""
    variables = {}
    for line in text.replace("\\\n", " ").splitlines():
        match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*(\+=|\?=|:=|=)\s*(.*)", line)
        if match:
            name, operation, value = match.groups()
            if operation == "+=":
                variables[name] = variables.get(name, "") + " " + value
            elif operation != "?=" or name not in variables:
                variables[name] = value
    return variables


def _arguments(text):
    depth, begin, result = 0, 0, []
    for index, character in enumerate(text):
        depth += character == "("
        depth -= character == ")"
        if character == "," and not depth:
            result.append(text[begin:index])
            begin = index + 1
    return result + [text[begin:]]


def _expand(text, variables, depth=0):
    require(depth < 32, "Cyclic or overly deep Make variable expansion")
    result = ""
    while "$(" in text:
        start = text.index("$(")
        end, nesting = start + 2, 1
        while end < len(text) and nesting:
            nesting += text[end] == "("
            nesting -= text[end] == ")"
            end += 1
        require(not nesting, "Unbalanced Make expression")
        expression = text[start + 2:end - 1]
        name, separator, arguments = expression.partition(" ")
        if not separator:
            value = _expand(variables.get(name, ""), variables, depth + 1)
        elif name == "shell":
            value = ""  # no brew/pkg-config/find process or build recipes run
        else:
            parts = [_expand(part, variables, depth + 1) for part in _arguments(arguments)]
            if name in ("filter", "filter-out") and len(parts) == 2:
                patterns = [re.compile("^" + re.escape(pattern).replace("%", ".*") + "$") for pattern in parts[0].split()]
                value = " ".join(word for word in parts[1].split() if any(pattern.fullmatch(word) for pattern in patterns) != (name == "filter-out"))
            elif name in ("addprefix", "addsuffix") and len(parts) == 2:
                value = " ".join(parts[0] + word if name == "addprefix" else word + parts[0] for word in parts[1].split())
            elif name == "basename" and len(parts) == 1:
                value = " ".join(str(Path(word).with_suffix("")) for word in parts[0].split())
            else:
                raise ContractError("Unsupported Make expression in reviewed configuration: " + name)
        result += text[:start] + value
        text = text[end:]
    return result + text


def _rule(text, target):
    match = re.search(r"^" + re.escape(target) + r":[^\n]*\n(?:\t[^\n]*(?:\n|$))*", text, re.MULTILINE)
    require(match is not None, "Missing build rule: " + target)
    return match.group()


def check_makefile(text):
    variables = _variables(text)
    words = lambda name: _expand(variables.get(name, ""), variables).split()
    app = set(words("APP_SOURCES"))
    review = set(words("REVIEW_SOURCES"))
    required = {"src/dev/review_" + module + ".c" for module in MODULES}
    require(app and not any("src/dev/review_" in source for source in app), "Shipping sources include review modules")
    require(required <= review and app <= review, "Review sources omit a production source or review module")
    for name in ("APP_RELEASE_FLAGS", "APP_SANITIZE_FLAGS", "FIXTURE_FLAGS"):
        flags = words(name)
        require(flags and not any("DUXCEL_DEV_REVIEW" in flag or "src/dev/review_" in flag for flag in flags), name + " leaks review defines/includes")
    for name, include in (("REVIEW_FLAGS", "build/review"), ("REVIEW_SANITIZE_FLAGS", "build/review-sanitize")):
        flags = words(name)
        require("-DDUXCEL_DEV_REVIEW" in flags and "-I" + include in flags, name + " lacks its review define or isolated generated-header directory")
        require("-include" in flags and "src/dev/review_input.h" in flags, name + " omits input quarantine")
    object_sets = []
    configurations = (("APP_RELEASE_OBJECTS", "release", "APP_RELEASE_FLAGS"),
                      ("APP_SANITIZE_OBJECTS", "sanitize", "APP_SANITIZE_FLAGS"),
                      ("REVIEW_OBJECTS", "review", "REVIEW_FLAGS"),
                      ("REVIEW_SANITIZE_OBJECTS", "review-sanitize", "REVIEW_SANITIZE_FLAGS"))
    for name, configuration, flags in configurations:
        objects = set(words(name))
        prefix = "build/obj/" + configuration + "/app/"
        require(objects and all(path.startswith(prefix) and path.endswith(".o") for path in objects), name + " escapes its isolated configuration")
        require(not any(objects & previous for previous in object_sets), "Build configurations share writable objects")
        object_sets.append(objects)
        for suffix in ("c", "m"):
            # Both source-language rules have the same target: inspect each explicitly.
            pattern = re.escape(prefix + "%.o") + r": %\." + suffix + r"[^\n]*\n(?:\t[^\n]*(?:\n|$))*"
            match = re.search(r"^" + pattern, text, re.MULTILINE)
            require(match is not None and "$(call compile_object,$(" + flags + "))" in match.group(), "Missing isolated compile flags for " + configuration + "/" + suffix)
        require("$(" + flags + ")" in _rule(text, prefix + "compile.flags"), "Missing compile signature for " + configuration)
        require("$(" + name + ")" in _rule(text, prefix + "link.flags"), "Missing link signature for " + configuration)
    incremental = set(words("INCREMENTAL_OBJECTS"))
    require(all(objects <= incremental for objects in object_sets), "A configuration omits incremental dependency tracking")
    require("-include $(INCREMENTAL_OBJECTS:.o=.d)" in text, "Missing generated dependency inclusion")
    for target, object_name in (("build/bin/duxcel", "APP_RELEASE_OBJECTS"), ("build/bin/duxcel-debug", "APP_SANITIZE_OBJECTS"),
                                ("build/review/duxcel-review", "REVIEW_OBJECTS"),
                                ("build/review-sanitize/duxcel-review", "REVIEW_SANITIZE_OBJECTS")):
        rule = _rule(text, target)
        require("$(" + object_name + ")" in rule and "$(call link_binary," in rule, "Wrong/nonatomic linked configuration: " + target)
        if target in ("build/bin/duxcel", "build/bin/duxcel-debug"):
            require("REVIEW" not in rule and "src/dev/" not in rule, "Shipping link rule includes review inputs")
    require("review: build/review/duxcel-review" in text and "review-debug: build/review-sanitize/duxcel-review" in text, "Missing separate review entry points")
    return {"scope": "default Make graph across platform branches; no recipes executed",
            "shipping_sources": len(app), "review_sources": len(review),
            "objects": {configuration: len(objects) for (_, configuration, _), objects in zip(configurations, object_sets)}}


def inspect_binary(path, nm="nm"):
    path = Path(path).resolve()
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NONBLOCK", 0))
    with os.fdopen(descriptor, "rb") as stream:
        before = os.fstat(stream.fileno())
        require(stat.S_ISREG(before.st_mode) and 0 < before.st_size <= 256 * 1024 * 1024, "Binary must be a nonempty regular file of at most 256 MiB")
        data = stream.read(256 * 1024 * 1024 + 1)
        after = os.fstat(stream.fileno())
        require(len(data) == before.st_size == after.st_size and before.st_mtime_ns == after.st_mtime_ns, "Binary changed during inspection")
    # A private immutable snapshot keeps nm and strings associated with one
    # exact artifact even if a build atomically replaces the original pathname.
    with tempfile.TemporaryDirectory(prefix="duxcel-review-absence-") as directory:
        snapshot = Path(directory) / "artifact"
        snapshot.write_bytes(data)
        result = subprocess.run([nm, str(snapshot)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    require(result.returncode == 0, "Cannot inspect linked symbols: " + result.stderr.strip())
    return {"path": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "symbols": sorted(set(SYMBOL.findall(result.stdout))),
            "markers": [marker.decode() for marker in MARKERS if marker in data],
            "test_markers": [marker.decode() for marker in TEST_MARKERS if marker in data],
            "review_source_paths": bool(re.search(rb"src[/\\]dev[/\\]review_[A-Za-z_]+\.[ch]", data))}


def check_binaries(shipping, review, nm="nm"):
    require(Path(shipping).resolve() != Path(review).resolve(), "Shipping and review artifacts must be distinct")
    production, development = inspect_binary(shipping, nm), inspect_binary(review, nm)
    require(not production["test_markers"] and not development["test_markers"], "Application artifact includes isolated test checkpoints")
    require(not production["symbols"], "Shipping linked review symbols: " + ", ".join(production["symbols"]))
    require(not production["markers"], "Shipping embeds review command/data strings: " + ", ".join(production["markers"]))
    require(not production["review_source_paths"], "Shipping embeds review module source paths")
    require(set(REQUIRED_SYMBOLS) <= set(development["symbols"]), "Review artifact omits required linked feature symbols")
    require(set(marker.decode() for marker in MARKERS) <= set(development["markers"]), "Review artifact omits its command/data strings")
    return {"shipping": production, "review": development,
            "scope": "linked symbols and feature strings; no app execution or native behavior claim"}



def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shipping", type=Path, default=ROOT / "build/bin/duxcel")
    parser.add_argument("--review", type=Path, default=ROOT / "build/review/duxcel-review")
    parser.add_argument("--makefile", type=Path, default=ROOT / "Makefile")
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--contract-only", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        return subprocess.call([sys.executable, "-m", "unittest", "tools.tests.test_review_build"], cwd=ROOT)
    try:
        result = {"build_contract": check_makefile(args.makefile.read_text()), "binary_absence_checked": not args.contract_only}
        if not args.contract_only:
            result.update(check_binaries(args.shipping, args.review, args.nm))
        print(json.dumps(result, indent=2))
        return 0
    except (OSError, subprocess.SubprocessError, ContractError) as error:
        print(json.dumps({"error": str(error)}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
