#!/usr/bin/env python3
"""Read native review bundles and record a durable, project-scoped lifecycle.

No daemon, network listener, app actions or non-standard-library dependencies.
The native app publishes manifest.json/frame.png before this tool sees a report.
"""
import argparse
import contextlib
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import re
import secrets
import stat
import struct
import sys
import zlib

SCHEMA = "duxcel.native-review"
LIFECYCLE_SCHEMA = SCHEMA + ".lifecycle"
JSON_LIMIT = 4 * 1024 * 1024
PNG_LIMIT = 128 * 1024 * 1024
LIFECYCLE_LIMIT = 1024 * 1024
TEXT_LIMIT = 4096
HISTORY_LIMIT = 128
REPORT_LIST_LIMIT = 1024
IDENTIFIER = re.compile(r"[A-Za-z0-9_-]{1,95}\Z", re.ASCII)
DESIGN_STATES = ("pending", "accepted", "needs-changes", "rejected")
IMPLEMENTATION_STATES = ("not-started", "in-progress", "ready-for-review")
VERIFICATION_STATES = ("unverified", "passed", "failed")


class ReviewError(ValueError):
    pass


def identifier(value):
    if not isinstance(value, str) or not IDENTIFIER.fullmatch(value):
        raise ReviewError("IDs require 1–95 ASCII alphanumeric, underscore or hyphen characters")
    return value


def default_root(project_id):
    identifier(project_id)
    override = os.environ.get("DUXCEL_REVIEW_OUTBOX")
    if override:
        if not Path(override).is_absolute():
            raise ReviewError("DUXCEL_REVIEW_OUTBOX must be an absolute project outbox path")
        return Path(override)
    if sys.platform == "darwin":
        base = Path.home() / "Library/Application Support"
    elif os.name == "nt":
        if not os.environ.get("LOCALAPPDATA"):
            raise ReviewError("LOCALAPPDATA is unavailable for the review outbox")
        base = Path(os.environ["LOCALAPPDATA"])
    else:
        xdg = os.environ.get("XDG_DATA_HOME")
        base = Path(xdg) if xdg and Path(xdg).is_absolute() else Path.home() / ".local/share"
    if not base.is_absolute():
        raise ReviewError("Application-data base must be an absolute path")
    return base / "Duxcel/Review" / project_id


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ReviewError("Duplicate JSON field: " + key)
        result[key] = value
    return result


def _integer(text):
    if len(text) > 128:
        raise ReviewError("JSON integer exceeds its digit limit; exact native IDs/values use strings")
    return int(text)


def _json(data):
    try:
        value = json.loads(data.decode("utf-8"), object_pairs_hook=_pairs, parse_int=_integer,
                           parse_constant=lambda text: (_ for _ in ()).throw(ReviewError("Non-finite JSON number")))
        pending = [(value, 0)]
        visited = 0
        while pending:
            item, depth = pending.pop()
            visited += 1
            if depth > 64 or visited > 100000:
                raise ReviewError("Review JSON structure exceeds its bounds")
            if isinstance(item, dict):
                pending.extend((child, depth + 1) for child in item.values())
                pending.extend((key, depth + 1) for key in item)
            elif isinstance(item, list):
                pending.extend((child, depth + 1) for child in item)
            elif isinstance(item, str):
                item.encode("utf-8")  # reject unpaired surrogate escape sequences
            elif isinstance(item, float) and not math.isfinite(item):
                raise ReviewError("Non-finite JSON number")
        if not isinstance(value, dict):
            raise ReviewError("Review JSON must be an object")
        return value
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise ReviewError("Invalid bounded UTF-8 JSON: " + str(error)) from None


def _text(value, limit=TEXT_LIMIT, nonempty=False):
    if not isinstance(value, str) or len(value.encode("utf-8")) > limit or (nonempty and not value.strip()):
        raise ReviewError("Invalid or oversized review text")
    return value


def _regular(fd, limit):
    info = os.fstat(fd)
    if not stat.S_ISREG(info.st_mode) or info.st_size < 1 or info.st_size > limit:
        raise ReviewError("Report file is empty, non-regular or exceeds its byte limit")
    return info


class _Bundle:
    def __init__(self, path, fd):
        self.path, self.fd = path, fd

    def open(self, name, flags, mode=0o600):
        flags |= getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0)
        if self.fd is not None:
            return os.open(name, flags, mode, dir_fd=self.fd)
        path = self.path / name
        if path.is_symlink():
            raise ReviewError("Report files cannot be symbolic links")
        return os.open(path, flags, mode)

    def read(self, name, limit):
        fd = self.open(name, os.O_RDONLY)
        with os.fdopen(fd, "rb") as stream:
            _regular(stream.fileno(), limit)
            data = stream.read(limit + 1)
            if len(data) > limit:
                raise ReviewError("Report file grew beyond its byte limit")
            return data

    def unlink(self, name):
        if self.fd is not None:
            os.unlink(name, dir_fd=self.fd)
        else:
            (self.path / name).unlink()


def _png(bundle):
    fd = bundle.open("frame.png", os.O_RDONLY)
    with os.fdopen(fd, "rb") as stream:
        _regular(stream.fileno(), PNG_LIMIT)
        digest = hashlib.sha256()
        total = 0

        def take(length):
            nonlocal total
            if length < 0 or total + length > PNG_LIMIT:
                raise ReviewError("PNG exceeds its 128 MiB byte limit")
            data = stream.read(length)
            total += len(data)
            if len(data) != length:
                raise ReviewError("Truncated PNG")
            digest.update(data)
            return data

        if take(8) != b"\x89PNG\r\n\x1a\n":
            raise ReviewError("Invalid PNG signature")
        size = None
        image_data = False
        chunks = 0
        while True:
            chunks += 1
            if chunks > 16384:
                raise ReviewError("PNG has too many chunks")
            length, kind = struct.unpack(">I4s", take(8))
            if any(not (65 <= byte <= 90 or 97 <= byte <= 122) for byte in kind) or not 65 <= kind[2] <= 90:
                raise ReviewError("Invalid PNG chunk type")
            if length > PNG_LIMIT - total - 4:
                raise ReviewError("Invalid PNG chunk length")
            if chunks == 1 and (kind != b"IHDR" or length != 13):
                raise ReviewError("PNG must begin with one IHDR")
            if kind == b"IHDR" and chunks != 1:
                raise ReviewError("Duplicate PNG IHDR")
            crc = zlib.crc32(kind)
            remaining = length
            header = b""
            while remaining:
                data = take(min(65536, remaining))
                crc = zlib.crc32(data, crc)
                if kind == b"IHDR":
                    header += data
                remaining -= len(data)
            if struct.unpack(">I", take(4))[0] != crc & 0xffffffff:
                raise ReviewError("PNG checksum mismatch")
            if kind == b"IHDR":
                width, height, bits, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", header)
                valid_bits = {0: (1, 2, 4, 8, 16), 2: (8, 16), 3: (1, 2, 4, 8), 4: (8, 16), 6: (8, 16)}
                if not width or not height or width > 65535 or height > 65535 or bits not in valid_bits.get(color, ()) or compression or filtering or interlace > 1:
                    raise ReviewError("Invalid PNG image header")
                size = [width, height]
            elif kind == b"IDAT":
                image_data |= length > 0
            elif kind == b"IEND":
                if length or not image_data or stream.read(1):
                    raise ReviewError("Invalid PNG end or trailing data")
                return {"width": size[0], "height": size[1], "bytes": total, "sha256": digest.hexdigest()}


class ReviewStore:
    def __init__(self, root, project_id):
        self.root = Path(root).expanduser().absolute()
        self.project_id = identifier(project_id)

    @contextlib.contextmanager
    def bundle(self, report_id):
        identifier(report_id)
        parent = child = None
        path = self.root / report_id
        try:
            if os.name == "nt":
                for folder in (self.root, path):
                    info = folder.lstat()
                    if not stat.S_ISDIR(info.st_mode) or info.st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:
                        raise ReviewError("Review outbox/report must be an ordinary directory")
            else:
                flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
                parent = os.open(self.root, flags)
                child = os.open(report_id, flags, dir_fd=parent)
            yield _Bundle(path, child)
        finally:
            if child is not None:
                os.close(child)
            if parent is not None:
                os.close(parent)

    def _manifest(self, bundle, report_id):
        manifest = _json(bundle.read("manifest.json", JSON_LIMIT))
        if manifest.get("schema") != SCHEMA or type(manifest.get("version")) is not int or manifest["version"] != 1:
            raise ReviewError("Unsupported native review schema/version")
        if identifier(manifest.get("report_id")) != report_id or identifier(manifest.get("project_id")) != self.project_id:
            raise ReviewError("Report or project identity does not match the requested outbox")
        identifier(manifest.get("session_id"))
        if not isinstance(manifest.get("frame"), dict) or not isinstance(manifest.get("target"), dict):
            raise ReviewError("Review frame/target must be objects")
        for key in ("project_id", "session_id"):
            if key in manifest["frame"] and manifest["frame"][key] != manifest[key]:
                raise ReviewError("Frame identity disagrees with its report")
        for key, maximum in (("widgets", 2048), ("events", 128)):
            values = manifest.get(key)
            if not isinstance(values, list) or len(values) > maximum or any(not isinstance(item, dict) for item in values):
                raise ReviewError("Invalid or oversized " + key)
        _text(manifest.get("note"))
        return manifest

    def _lifecycle(self, bundle, report_id):
        try:
            state = _json(bundle.read("lifecycle.json", LIFECYCLE_LIMIT))
        except FileNotFoundError:
            return {"schema": LIFECYCLE_SCHEMA, "version": 1, "report_id": report_id,
                    "project_id": self.project_id, "revision": 0, "acknowledged": False,
                    "design": "pending", "implementation": "not-started", "verification": "unverified", "history": []}
        if state.get("schema") != LIFECYCLE_SCHEMA or type(state.get("version")) is not int or state["version"] != 1 or state.get("report_id") != report_id or state.get("project_id") != self.project_id:
            raise ReviewError("Invalid lifecycle identity/schema")
        if type(state.get("revision")) is not int or state["revision"] < 0 or type(state.get("acknowledged")) is not bool or state.get("design") not in DESIGN_STATES or state.get("implementation") not in IMPLEMENTATION_STATES or state.get("verification") not in VERIFICATION_STATES:
            raise ReviewError("Invalid lifecycle state")
        history = state.get("history")
        if not isinstance(history, list) or len(history) > HISTORY_LIMIT or state["revision"] != len(history):
            raise ReviewError("Invalid lifecycle revision/history")
        for sequence, entry in enumerate(history, 1):
            if not isinstance(entry, dict) or type(entry.get("sequence")) is not int or entry["sequence"] != sequence or entry.get("kind") not in ("ack", "reply", "status"):
                raise ReviewError("Invalid lifecycle history entry")
            _text(entry.get("actor"), 128, nonempty=True)
            _text(entry.get("message"))
            _text(entry.get("at"), 64, nonempty=True)
            changes = entry.get("changes", {})
            choices = {"design": DESIGN_STATES, "implementation": IMPLEMENTATION_STATES, "verification": VERIFICATION_STATES}
            if not isinstance(changes, dict) or any(key not in choices or value not in choices[key] for key, value in changes.items()) or (changes and entry["kind"] != "status"):
                raise ReviewError("Invalid lifecycle history changes")
        return state

    def _complete(self, bundle, report_id):
        manifest, image = self._manifest(bundle, report_id), _png(bundle)
        frame, target = manifest["frame"], manifest["target"]
        if "frame_id" in frame and "frame_id" in target and frame["frame_id"] != target["frame_id"]:
            raise ReviewError("Target belongs to a different captured frame")
        if "framebuffer" in frame:
            bounds = frame["framebuffer"]
            if not isinstance(bounds, dict) or any(type(bounds.get(axis)) is not int or bounds[axis] != image[axis] for axis in ("width", "height")):
                raise ReviewError("Frame image dimensions disagree with the captured framebuffer")
        return manifest, image

    def read(self, report_id):
        with self.bundle(report_id) as bundle:
            manifest, image = self._complete(bundle, report_id)
            state = self._lifecycle(bundle, report_id)
            return {"manifest": manifest, "image": dict(image, path=str(bundle.path / "frame.png")), "lifecycle": state}

    def list(self):
        reports, rejected = [], []
        if self.root.is_symlink():
            raise ReviewError("Review outbox must be an ordinary directory")
        if not self.root.exists():
            return {"project_id": self.project_id, "root": str(self.root), "reports": reports, "rejected": rejected}
        if not self.root.is_dir():
            raise ReviewError("Review outbox must be an ordinary directory")
        names = []
        with os.scandir(self.root) as entries:
            for entry in entries:
                if entry.name.startswith("."):
                    continue  # private staging/lock files never become reports
                if len(names) >= REPORT_LIST_LIMIT:
                    raise ReviewError("Outbox listing exceeds 1024 reports; read a known report ID directly")
                names.append(entry.name)
        for name in sorted(names):
            try:
                report = self.read(name)
                manifest, lifecycle = report["manifest"], report["lifecycle"]
                state = {key: lifecycle[key] for key in ("revision", "acknowledged", "design", "implementation", "verification")}
                reports.append({"report_id": name, "session_id": manifest["session_id"], "note": manifest["note"],
                                "image_path": report["image"]["path"], "lifecycle": state})
            except (OSError, ReviewError) as error:
                rejected.append({"report_id": name, "error": str(error)})
        return {"project_id": self.project_id, "root": str(self.root), "reports": reports, "rejected": rejected}

    def update(self, report_id, kind, actor, message="", **changes):
        _text(actor, 128, nonempty=True)
        _text(message, nonempty=kind == "reply")
        if kind not in ("ack", "reply", "status"):
            raise ReviewError("Unsupported lifecycle operation")
        choices = {"design": DESIGN_STATES, "implementation": IMPLEMENTATION_STATES, "verification": VERIFICATION_STATES}
        if any(key not in choices or value not in choices[key] for key, value in changes.items()) or (changes and kind != "status"):
            raise ReviewError("Invalid lifecycle state change")
        with self.bundle(report_id) as bundle:
            self._complete(bundle, report_id)
            # Darwin can return ENOENT for simultaneous non-exclusive O_CREAT
            # calls even though the winning process has created the file. Keep
            # creation exclusive and open the persistent winner separately.
            try:
                lock = bundle.open(".lifecycle.lock", os.O_RDWR | os.O_CREAT | os.O_EXCL)
            except FileExistsError:
                lock = bundle.open(".lifecycle.lock", os.O_RDWR)
            try:
                if not stat.S_ISREG(os.fstat(lock).st_mode):
                    raise ReviewError("Lifecycle lock must be a regular file")
                if os.name == "nt":
                    import msvcrt
                    if not os.fstat(lock).st_size:
                        os.write(lock, b"\0")
                    os.lseek(lock, 0, os.SEEK_SET)
                    msvcrt.locking(lock, msvcrt.LK_LOCK, 1)
                else:
                    import fcntl
                    fcntl.flock(lock, fcntl.LOCK_EX)
                state = self._lifecycle(bundle, report_id)
                if len(state["history"]) >= HISTORY_LIMIT:
                    raise ReviewError("Lifecycle history is full; existing entries are retained")
                state["revision"] += 1
                if kind == "ack":
                    state["acknowledged"] = True
                state.update(changes)
                entry = {"sequence": state["revision"], "at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                         "kind": kind, "actor": actor, "message": message}
                if changes:
                    entry["changes"] = changes
                state["history"].append(entry)
                data = (json.dumps(state, ensure_ascii=False, allow_nan=False, indent=2) + "\n").encode("utf-8")
                if len(data) > LIFECYCLE_LIMIT:
                    raise ReviewError("Lifecycle exceeds its byte limit; existing state is retained")
                temporary = ".lifecycle-" + secrets.token_hex(12)
                fd = bundle.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL)
                try:
                    with os.fdopen(fd, "wb") as stream:
                        stream.write(data)
                        stream.flush()
                        os.fsync(stream.fileno())
                    if bundle.fd is not None:
                        os.replace(temporary, "lifecycle.json", src_dir_fd=bundle.fd, dst_dir_fd=bundle.fd)
                        os.fsync(bundle.fd)
                    else:
                        os.replace(bundle.path / temporary, bundle.path / "lifecycle.json")
                finally:
                    with contextlib.suppress(FileNotFoundError):
                        bundle.unlink(temporary)
                return state
            finally:
                os.close(lock)  # releases the process's advisory lifecycle lock


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--project", default="duxcel", help="exact project_id in the native capture (default: duxcel)")
    result.add_argument("--root", help="project outbox path; otherwise use the platform root/DUXCEL_REVIEW_OUTBOX")
    commands = result.add_subparsers(dest="command", required=True)
    commands.add_parser("list", help="list complete reports and explicit rejections; private staging is ignored")
    for name in ("read", "ack", "reply", "status"):
        command = commands.add_parser(name)
        command.add_argument("report_id")
        if name in ("ack", "reply", "status"):
            command.add_argument("--actor", default="agent")
            command.add_argument("--message", default="", required=name == "reply")
        if name == "status":
            command.add_argument("--design", choices=DESIGN_STATES)
            command.add_argument("--implementation", choices=IMPLEMENTATION_STATES)
            command.add_argument("--verification", choices=VERIFICATION_STATES)
    return result


def main(argv=None):
    args = parser().parse_args(argv)
    try:
        store = ReviewStore(args.root or default_root(args.project), args.project)
        if args.command == "list":
            output = store.list()
        elif args.command == "read":
            output = store.read(args.report_id)
        elif args.command == "status":
            changes = {key: getattr(args, key) for key in ("design", "implementation", "verification") if getattr(args, key) is not None}
            output = store.update(args.report_id, "status", args.actor, args.message, **changes) if changes else store.read(args.report_id)["lifecycle"]
        else:
            output = store.update(args.report_id, args.command, args.actor, args.message)
        print(json.dumps(output, ensure_ascii=False, allow_nan=False, indent=2))
        return 0
    except (OSError, ReviewError, UnicodeError) as error:
        print(json.dumps({"error": str(error)}, ensure_ascii=False), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
