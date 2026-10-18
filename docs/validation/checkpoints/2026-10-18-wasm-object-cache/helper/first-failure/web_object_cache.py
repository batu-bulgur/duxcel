#!/usr/bin/env python3
"""Per-translation-unit object reuse; the caller owns configuration and locking.

Every call performs a compiler dependency scan. The caller's configuration must
identify its mode, pinned compiler/SDK and any compiler-affecting environment.
Hold the namespace lock through compilation AND linking. validate_source must
raise if the caller's frozen source/input snapshot changed; it runs on reuse too.
This helper does not change artifact signatures or publish application artifacts.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

SCHEMA = "duxcel.web-object-cache"
VERSION = 1
TARGET = "duxcel_object"


class ObjectCacheError(ValueError):
    pass


def parse_depfile(text):
    """Read our single -MQ target's Make prerequisites without shell expansion.

    Make escapes whitespace/# with backslashes, dollars as $$, and doubles
    backslash runs before escaped separators. Ordinary backslashes are literal.
    -MP phony rules and Make variables are deliberately not accepted.
    """
    prefix = TARGET + ":"
    if not text.startswith(prefix):
        raise ObjectCacheError("Unexpected compiler dependency target")
    text = text[len(prefix):]
    paths, token, i = [], [], 0

    def finish():
        if token:
            paths.append("".join(token))
            token.clear()

    while i < len(text):
        c = text[i]
        if c == "\\":
            start = i
            while i < len(text) and text[i] == "\\":
                i += 1
            count = i - start
            following = text[i] if i < len(text) else ""
            if following in ("\n", "\r"):
                if count != 1:
                    raise ObjectCacheError("Ambiguous dependency line continuation")
                if following == "\r":
                    if text[i:i+2] != "\r\n":
                        raise ObjectCacheError("Invalid dependency line ending")
                    i += 1
                finish()
                i += 1
            elif following and following in " \t#:":
                token.extend("\\" * (count // 2))
                if count % 2:
                    token.append(following)
                    i += 1
                # An even run leaves the delimiter for the next iteration.
            else:
                token.extend("\\" * count)
        elif c.isspace():
            finish()
            i += 1
        elif c == "#":
            finish()
            end = text.find("\n", i)
            i = len(text) if end < 0 else end
        elif c == "$":
            if text[i:i+2] != "$$":
                raise ObjectCacheError("Make variables are not dependency paths")
            token.append("$")
            i += 2
        else:
            token.append(c)
            i += 1
    finish()
    if not paths or any("\0" in path for path in paths):
        raise ObjectCacheError("Empty or invalid compiler dependencies")
    return sorted(set(paths))


def _sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _snapshot(paths, cwd):
    result = {}
    for recorded in paths:
        path = Path(recorded)
        if not path.is_absolute():
            path = cwd / path
        if not path.is_file():
            raise ObjectCacheError("Compiler dependency disappeared: " + recorded)
        result[recorded] = {"resolved": str(path.resolve(strict=True)), "sha256": _sha(path)}
    return result


def _run(arguments, cwd, env, log, phase):
    result = subprocess.run(arguments, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise ObjectCacheError(phase + " failed (exit " + str(result.returncode) + ")")


def ensure_object(*, compiler, flags, source, target, cwd, env, log,
                  configuration, validate_source):
    """Return True after rebuilding, False after verified reuse.

    All arguments are explicit; compiler/flags are argv sequences, log is an
    open subprocess-compatible output stream, configuration is an opaque string,
    and validate_source is a raising no-argument callback. No shell is invoked.
    Failed scans/compiles/validation leave the previously published triplet
    alone. Metadata commits last; interruption between renames cannot produce a
    valid mismatched cache because object and dependency-file hashes are checked.
    """
    cwd = Path(cwd).resolve(strict=True)
    source = os.fspath(source)
    target = Path(target)
    if not target.is_absolute():
        target = cwd / target
    compiler, flags = list(map(os.fspath, compiler)), list(map(os.fspath, flags))
    if not compiler or not isinstance(configuration, str) or not configuration:
        raise ObjectCacheError("Explicit compiler and configuration identity required")
    source_path = Path(source)
    if not source_path.is_absolute():
        source_path = cwd / source_path
    target.parent.mkdir(parents=True, exist_ok=True)
    depfile, metadata = target.with_suffix(".d"), target.with_suffix(".json")
    identity = {"schema": SCHEMA, "version": VERSION, "configuration": configuration,
                "compiler": compiler, "flags": flags, "cwd": str(cwd), "source": source}
    validate_source()
    with tempfile.TemporaryDirectory(prefix=".object-", dir=target.parent) as temporary:
        temporary = Path(temporary)
        scanned, compiled, candidate = temporary/"scan.d", temporary/"compile.d", temporary/"object.o"
        _run([*compiler, *flags, "-MM", "-MF", str(scanned), "-MQ", TARGET, source],
             cwd, env, log, "Dependency scan")
        dependencies = _snapshot(parse_depfile(scanned.read_text()), cwd)
        if not any(os.path.abspath(cwd/path) == os.path.abspath(source_path) for path in dependencies):
            raise ObjectCacheError("Compiler dependencies omit their source")
        validate_source()
        try:
            record = json.loads(metadata.read_text())
            reusable = (target.is_file() and depfile.is_file() and
                        all(record.get(key) == value for key, value in identity.items()) and
                        record.get("dependencies") == dependencies and
                        record.get("object_sha256") == _sha(target) and
                        record.get("depfile_sha256") == _sha(depfile) and
                        _snapshot(parse_depfile(depfile.read_text()), cwd) == dependencies)
        except (OSError, ValueError, TypeError, AttributeError):
            reusable = False
        if reusable:
            if _snapshot(dependencies, cwd) != dependencies:
                raise ObjectCacheError("Dependencies changed during cache validation")
            validate_source()
            return False
        _run([*compiler, *flags, "-MMD", "-MF", str(compiled), "-MQ", TARGET,
              "-c", source, "-o", str(candidate)], cwd, env, log, "Compilation")
        actual = _snapshot(parse_depfile(compiled.read_text()), cwd)
        if actual != dependencies:
            raise ObjectCacheError("Dependency closure or bytes changed during compilation")
        if not candidate.is_file():
            raise ObjectCacheError("Compiler did not produce an object")
        record = dict(identity, dependencies=actual, object_sha256=_sha(candidate),
                      depfile_sha256=_sha(compiled))
        pending = temporary/"metadata.json"
        pending.write_text(json.dumps(record, sort_keys=True, indent=2) + "\n")
        if _snapshot(dependencies, cwd) != dependencies:
            raise ObjectCacheError("Dependencies changed before object publication")
        validate_source()
        compiled.replace(depfile)
        candidate.replace(target)
        pending.replace(metadata)
        return True
