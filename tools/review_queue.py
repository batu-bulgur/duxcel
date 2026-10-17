#!/usr/bin/env python3
"""Development-only durable queue for validated native and Wasm review bundles.

The queue never changes design/implementation/verification decisions. Consumers use
exclusive, expiring leases; delivery is at least once, so consumer side effects
must be idempotent by report_id. No listener or application polling lives here.
"""
import contextlib
import hashlib
import math
import os
from pathlib import Path
import secrets
import shutil
import sqlite3
import stat
import tempfile
import threading
import time

import native_review as review

SIDECAR_LIMIT = 1024 * 1024
ARCHIVE_LIMIT = review.JSON_LIMIT + review.PNG_LIMIT + SIDECAR_LIMIT + 8192
LEASE_MAX = 86400
MESSAGE_LIMIT = 16 * 1024
SCHEMA_VERSION = "1"


def _hash(data):
    return hashlib.sha256(data).hexdigest()


def _lease(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not 0 < value <= LEASE_MAX:
        raise review.ReviewError("Lease duration must be positive and at most 86400 seconds")
    return float(value)


def _finite(value, positive=False):
    return type(value) in (int, float) and math.isfinite(value) and (not positive or value > 0)


def _dimensions(value, expected=None):
    if not isinstance(value, dict) or any(type(value.get(axis)) is not int or not 0 < value[axis] <= 65535 for axis in ("width", "height")):
        raise review.ReviewError("Invalid browser review dimensions")
    if expected is not None and any(value[axis] != expected[axis] for axis in ("width", "height")):
        raise review.ReviewError("Browser review dimensions disagree with the captured frame")


def _browser(data, manifest_bytes, manifest, image):
    """Validate browser-owned geometry/build provenance against C-owned evidence."""
    sidecar = review._json(data)
    frame = manifest["frame"]
    frame_id = frame.get("frame_id")
    if not isinstance(frame_id, str) or not frame_id.isascii() or not frame_id.isdecimal() or len(frame_id) > 20 or int(frame_id) > 18446744073709551615:
        raise review.ReviewError("Browser review has no exact uint64 frame identity")
    _dimensions(frame.get("window"))
    _dimensions(frame.get("framebuffer"), image)
    if "project_id" in sidecar and sidecar["project_id"] != manifest["project_id"]:
        raise review.ReviewError("Browser sidecar project disagrees with the C report")
    expected = {"schema": "duxcel.browser-review", "version": 1, "report_id": manifest["report_id"],
                "session_id": manifest["session_id"], "frame_id": frame.get("frame_id"),
                "binary_sha256": frame.get("binary_sha256")}
    if any(sidecar.get(key) != value for key, value in expected.items()) or type(sidecar.get("version")) is not int:
        raise review.ReviewError("Browser sidecar identity/schema disagrees with the C report")
    binary = sidecar.get("binary_sha256")
    if not isinstance(binary, str) or len(binary) != 64 or any(char not in "0123456789abcdef" for char in binary):
        raise review.ReviewError("Browser review has no exact Wasm SHA-256")
    build = sidecar.get("build")
    if not isinstance(build, dict) or not isinstance(build.get("artifacts"), dict) or build["artifacts"].get("duxcel.wasm") != binary:
        raise review.ReviewError("Browser build provenance disagrees with the captured Wasm")
    _dimensions(sidecar.get("logical"), frame.get("window"))
    _dimensions(sidecar.get("canvas_backing"), image)
    if any(sidecar[key]["width"] * sidecar[key]["height"] > 32 * 1024 * 1024 for key in ("logical", "canvas_backing")):
        raise review.ReviewError("Browser review exceeds its captured-pixel limit")
    files = sidecar.get("files")
    if not isinstance(files, dict) or set(files) != {"manifest.json", "frame.png"}:
        raise review.ReviewError("Browser sidecar requires the exact captured file hashes")
    for name, size, digest in (("manifest.json", len(manifest_bytes), _hash(manifest_bytes)),
                               ("frame.png", image["bytes"], image["sha256"])):
        item = files[name]
        if not isinstance(item, dict) or type(item.get("bytes")) is not int or item["bytes"] != size or item.get("sha256") != digest:
            raise review.ReviewError("Browser report file size/hash mismatch: " + name)
    _dimensions(files["frame.png"], image)
    css = sidecar.get("canvas_css")
    if not isinstance(css, dict) or any(not _finite(css.get(axis), axis in ("width", "height")) for axis in ("x", "y", "width", "height")) or not _finite(sidecar.get("device_pixel_ratio"), True):
        raise review.ReviewError("Invalid browser canvas/DPR geometry")
    transforms = sidecar.get("transforms")
    if not isinstance(transforms, dict):
        raise review.ReviewError("Browser review has no coordinate transforms")
    logical = sidecar["logical"]
    expected_transforms = {"logical_to_css": {"scale_x": css["width"] / logical["width"],
        "scale_y": css["height"] / logical["height"], "translate_x": css["x"], "translate_y": css["y"]},
        "logical_to_framebuffer": {"scale_x": image["width"] / logical["width"], "scale_y": image["height"] / logical["height"]}}
    for name, values in expected_transforms.items():
        actual = transforms.get(name)
        if not isinstance(actual, dict) or any(not _finite(actual.get(key)) or not math.isclose(actual[key], value, rel_tol=1e-9, abs_tol=1e-9) for key, value in values.items()):
            raise review.ReviewError("Browser coordinate transform disagrees with captured geometry")
    return sidecar


def _archive_members(data):
    """Parse only the small USTAR subset emitted by the review host.

    Parsing headers directly avoids tarfile's transparent GNU/PAX long-name and
    sparse-member expansion. Only three bounded regular payloads are ever copied.
    """
    members, report_id, offset = {}, None, 0

    def octal(field):
        field = field.rstrip(b"\0 ").lstrip(b" ")
        if not field or any(char not in b"01234567" for char in field):
            raise review.ReviewError("Invalid browser archive numeric header")
        return int(field, 8)

    def text(field):
        head, _, tail = field.partition(b"\0")
        if any(tail):
            raise review.ReviewError("Invalid browser archive text padding")
        try:
            return head.decode("ascii")
        except UnicodeError:
            raise review.ReviewError("Browser archive paths must be ASCII") from None

    while offset + 512 <= len(data):
        header = data[offset:offset + 512]
        if not any(header):
            if len(data) - offset < 1024 or any(data[offset:]):
                raise review.ReviewError("Invalid browser archive terminator/trailing data")
            break
        if len(members) >= 3 or header[156:157] not in (b"0", b"\0") or header[257:265] != b"ustar\0" + b"00":
            raise review.ReviewError("Browser archive requires exactly three plain USTAR regular files")
        if octal(header[148:156]) != sum(header[:148]) + 8 * 32 + sum(header[156:]):
            raise review.ReviewError("Browser archive header checksum mismatch")
        if any(header[157:257]):
            raise review.ReviewError("Browser archive links are forbidden")
        name, prefix = text(header[:100]), text(header[345:500])
        path = (prefix + "/" if prefix else "") + name
        pieces = path.split("/")
        if len(pieces) != 2 or pieces[1] not in ("manifest.json", "frame.png", "browser.json"):
            raise review.ReviewError("Unsafe or unexpected browser archive path")
        current = review.identifier(pieces[0])
        if report_id is not None and current != report_id:
            raise review.ReviewError("Browser archive contains multiple report identities")
        report_id, name = current, pieces[1]
        size = octal(header[124:136])
        limit = {"manifest.json": review.JSON_LIMIT, "frame.png": review.PNG_LIMIT, "browser.json": SIDECAR_LIMIT}[name]
        if name in members or not 0 < size <= limit:
            raise review.ReviewError("Duplicate, empty or oversized browser archive member")
        offset += 512
        end = offset + size
        padded = end + (-size % 512)
        if padded > len(data) or any(data[end:padded]):
            raise review.ReviewError("Truncated browser archive or nonzero member padding")
        members[name] = data[offset:end]
        offset = padded
    else:
        raise review.ReviewError("Browser archive has no complete terminator")
    if set(members) != {"manifest.json", "frame.png", "browser.json"}:
        raise review.ReviewError("Browser review archive is incomplete")
    return members, report_id


class ReviewQueue:
    def __init__(self, root, project_id):
        self.store = review.ReviewStore(root, project_id)
        self.root, self.project_id = self.store.root, self.store.project_id
        self._sync_lock = threading.Lock()
        # Only bounded file signatures, digest/kind or validation errors are cached;
        # never retained PNG bytes, manifests or lifecycle snapshots.
        self._validation_cache = {}
        self.root.mkdir(mode=0o700, parents=True, exist_ok=True)
        if self.root.is_symlink() or not self.root.is_dir():
            raise review.ReviewError("Review queue root must be an ordinary directory")
        self.database = self.root / ".queue.sqlite3"
        flags = os.O_RDWR | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0)
        try:
            fd = os.open(self.database, flags, 0o600)
        except FileExistsError:
            info = self.database.lstat()
            if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
                raise review.ReviewError("Review queue database must be a private regular file")
        else:
            os.close(fd)
        with self._db() as db:
            db.execute("PRAGMA journal_mode=WAL")
            db.execute("BEGIN IMMEDIATE")
            db.execute("CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL)")
            for key, value in (("schema", SCHEMA_VERSION), ("project_id", self.project_id)):
                db.execute("INSERT OR IGNORE INTO metadata VALUES (?, ?)", (key, value))
                if db.execute("SELECT value FROM metadata WHERE key=?", (key,)).fetchone()[0] != value:
                    raise review.ReviewError("Review queue schema/project identity mismatch")
            db.execute("""CREATE TABLE IF NOT EXISTS reports (
                report_id TEXT PRIMARY KEY, fingerprint TEXT NOT NULL, kind TEXT NOT NULL,
                state TEXT NOT NULL DEFAULT 'pending', attempt INTEGER NOT NULL DEFAULT 0,
                discovered REAL NOT NULL, updated REAL NOT NULL, worker TEXT, token TEXT,
                lease_until REAL, result TEXT, message TEXT NOT NULL DEFAULT '', error TEXT NOT NULL DEFAULT '')""")
            db.execute("CREATE INDEX IF NOT EXISTS reports_pending ON reports(state, discovered, report_id)")
            db.commit()

    @contextlib.contextmanager
    def _db(self):
        info = self.database.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
            raise review.ReviewError("Review queue database must be a private regular file")
        db = sqlite3.connect(self.database, timeout=10, isolation_level=None)
        db.row_factory = sqlite3.Row
        try:
            db.execute("PRAGMA synchronous=FULL")
            db.execute("PRAGMA busy_timeout=10000")
            yield db
        except BaseException:
            if db.in_transaction:
                db.rollback()
            raise
        finally:
            db.close()

    def _read(self, report_id):
        with self.store.bundle(report_id) as bundle:
            manifest, image = self.store._complete(bundle, report_id)
            raw_manifest = bundle.read("manifest.json", review.JSON_LIMIT)
            if review._json(raw_manifest) != manifest:
                raise review.ReviewError("Review manifest changed during validation")
            state = self.store._lifecycle(bundle, report_id)
            try:
                raw_browser = bundle.read("browser.json", SIDECAR_LIMIT)
            except FileNotFoundError:
                raw_browser = b""
            report = {"manifest": manifest, "image": dict(image, path=str(bundle.path / "frame.png")), "lifecycle": state}
            if raw_browser:
                report["browser"] = _browser(raw_browser, raw_manifest, manifest, image)
            digest = _hash((_hash(raw_manifest) + image["sha256"] + _hash(raw_browser)).encode("ascii"))
            return report, digest

    @staticmethod
    def _recover(db, now):
        recovered = [row[0] for row in db.execute("SELECT report_id FROM reports WHERE state='leased' AND lease_until<=? ORDER BY report_id", (now,))]
        db.execute("UPDATE reports SET state='pending', updated=?, worker=NULL, token=NULL, lease_until=NULL, error='Lease expired; returned to queue' WHERE state='leased' AND lease_until<=?", (now, now))
        return recovered

    def _signature(self, report_id):
        """No-follow metadata for immutable evidence and the mutable lifecycle.

        ctime catches content writes followed by restored mtime. A new report
        directory also invalidates the cache even when filenames/mtime match.
        Claims always perform full validation independently of this poll cache.
        """
        with self.store.bundle(report_id) as bundle:
            directory = os.fstat(bundle.fd) if bundle.fd is not None else bundle.path.lstat()
            result = [(directory.st_dev, directory.st_ino)]
            for name, limit, optional in (("manifest.json", review.JSON_LIMIT, False),
                                          ("frame.png", review.PNG_LIMIT, False),
                                          ("browser.json", SIDECAR_LIMIT, True),
                                          ("lifecycle.json", review.LIFECYCLE_LIMIT, True)):
                try:
                    info = os.stat(name, dir_fd=bundle.fd, follow_symlinks=False) if bundle.fd is not None else (bundle.path / name).lstat()
                except FileNotFoundError:
                    if not optional:
                        raise
                    result.append(None)
                    continue
                if not stat.S_ISREG(info.st_mode) or not 0 < info.st_size <= limit:
                    raise review.ReviewError("Report file is empty, non-regular or exceeds its byte limit")
                result.append((info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns))
            return tuple(result)

    def sync(self):
        """Discover bounded bundles without rereading unchanged images every poll."""
        with self._sync_lock:
            return self._sync()

    def _sync(self):
        # Take known database identities BEFORE scanning directory entries. A
        # different queue process may publish and insert a report during this scan;
        # it must not be invalidated merely because it was absent from our snapshot.
        with self._db() as db:
            known = {row[0] for row in db.execute("SELECT report_id FROM reports LIMIT ?", (review.REPORT_LIST_LIMIT + 1,))}
        if len(known) > review.REPORT_LIST_LIMIT:
            raise review.ReviewError("Review queue scan exceeds 1024 historical reports")
        names, valid, rejected = [], [], []
        with os.scandir(self.root) as entries:
            for entry in entries:
                if entry.name.startswith("."):
                    continue
                if len(names) >= review.REPORT_LIST_LIMIT:
                    raise review.ReviewError("Review queue scan exceeds 1024 reports")
                names.append(entry.name)
        for name in sorted(names):
            signature = None
            try:
                signature = self._signature(name)
                cached = self._validation_cache.get(name)
                if cached is not None and cached[0] == signature:
                    _, digest, kind, error = cached
                    if error is not None:
                        rejected.append({"report_id": name, "error": error})
                    else:
                        valid.append((name, digest, kind))
                    continue
                report, digest = self._read(name)
                if self._signature(name) != signature:
                    raise review.ReviewError("Review evidence changed during validation")
                kind = "browser" if "browser" in report else "native"
                self._validation_cache[name] = (signature, digest, kind, None)
                valid.append((name, digest, kind))
            except (OSError, review.ReviewError) as error:
                message = str(error).encode("utf-8", errors="replace")[:review.TEXT_LIMIT].decode("utf-8", errors="ignore")
                rejected.append({"report_id": name, "error": message})
                self._validation_cache.pop(name, None)
                if signature is not None:
                    # A malformed but unchanged completed bundle should not cause
                    # repeated large reads either. Partial/missing files cost only
                    # metadata probes, and are retried when publication completes.
                    with contextlib.suppress(OSError, review.ReviewError):
                        if self._signature(name) == signature:
                            self._validation_cache[name] = (signature, None, None, message)
        present = set(names)
        self._validation_cache = {name: value for name, value in self._validation_cache.items() if name in present}
        added, now = [], time.time()
        with self._db() as db:
            db.execute("BEGIN IMMEDIATE")
            recovered = self._recover(db, now)
            for name, digest, kind in valid:
                row = db.execute("SELECT fingerprint FROM reports WHERE report_id=?", (name,)).fetchone()
                if row is None:
                    db.execute("INSERT INTO reports(report_id,fingerprint,kind,discovered,updated) VALUES (?,?,?,?,?)", (name, digest, kind, now, now))
                    added.append(name)
                elif row[0] != digest:
                    rejected.append({"report_id": name, "error": "Published immutable review evidence changed"})
            for name in sorted(known - present):
                rejected.append({"report_id": name, "error": "Published review evidence is missing"})
            for item in rejected:
                db.execute("UPDATE reports SET state='invalid', updated=?, worker=NULL,token=NULL,lease_until=NULL,error=? WHERE report_id=?", (now, item["error"][:review.TEXT_LIMIT], item["report_id"]))
            db.commit()
        return {"added": added, "recovered": recovered, "rejected": rejected}

    def list(self):
        scan = self.sync()
        with self._db() as db:
            rows = [dict(row) for row in db.execute("SELECT * FROM reports ORDER BY discovered,report_id LIMIT ?", (review.REPORT_LIST_LIMIT + 1,))]
        if len(rows) > review.REPORT_LIST_LIMIT:
            raise review.ReviewError("Review queue listing exceeds 1024 historical reports")
        # Tokens are capabilities; only claim/renew responses expose them.
        for row in rows:
            row.pop("token", None)
        return {"project_id": self.project_id, "root": str(self.root), "reports": rows, "rejected": scan["rejected"]}

    def claim(self, worker, lease_seconds=300):
        review._text(worker, 128, nonempty=True)
        duration = _lease(lease_seconds)
        self.sync()
        with self._db() as db:
            db.execute("BEGIN IMMEDIATE")
            now = time.time()
            self._recover(db, now)
            while True:
                row = db.execute("SELECT * FROM reports WHERE state='pending' ORDER BY discovered,report_id LIMIT 1").fetchone()
                if row is None:
                    db.commit()
                    return None
                try:
                    report, digest = self._read(row["report_id"])
                    if digest != row["fingerprint"]:
                        raise review.ReviewError("Published immutable review evidence changed")
                except (OSError, review.ReviewError) as error:
                    db.execute("UPDATE reports SET state='invalid',updated=?,error=? WHERE report_id=?", (now, str(error)[:review.TEXT_LIMIT], row["report_id"]))
                    continue
                token = secrets.token_urlsafe(32)
                db.execute("UPDATE reports SET state='leased',attempt=attempt+1,updated=?,worker=?,token=?,lease_until=?,error='' WHERE report_id=?", (now, worker, token, now + duration, row["report_id"]))
                result = dict(db.execute("SELECT * FROM reports WHERE report_id=?", (row["report_id"],)).fetchone())
                result["report"] = report
                db.commit()
                return result

    @staticmethod
    def _held(db, report_id, token, now):
        review.identifier(report_id)
        review._text(token, 128, nonempty=True)
        if not token.isascii():
            raise review.ReviewError("Queue lease tokens must be ASCII capabilities")
        row = db.execute("SELECT * FROM reports WHERE report_id=?", (report_id,)).fetchone()
        if row is None or row["state"] != "leased" or not secrets.compare_digest(row["token"] or "", token) or row["lease_until"] <= now:
            raise review.ReviewError("Report lease is missing, expired or owned by another consumer")
        return row

    def renew(self, report_id, token, lease_seconds=300):
        duration = _lease(lease_seconds)
        with self._db() as db:
            db.execute("BEGIN IMMEDIATE")
            now = time.time()
            self._held(db, report_id, token, now)
            db.execute("UPDATE reports SET updated=?,lease_until=? WHERE report_id=?", (now, now + duration, report_id))
            result = dict(db.execute("SELECT * FROM reports WHERE report_id=?", (report_id,)).fetchone())
            db.commit()
            return result

    def finish(self, report_id, token, result="completed", message=""):
        review._text(result, 128, nonempty=True)
        review._text(message, MESSAGE_LIMIT)
        return self._close(report_id, token, "completed", result=result, message=message)

    def release(self, report_id, token, error="", retry=True):
        review._text(error)
        if type(retry) is not bool:
            raise review.ReviewError("Queue retry must be boolean")
        return self._close(report_id, token, "pending" if retry else "failed", error=error)

    def _close(self, report_id, token, state, result=None, message="", error=""):
        with self._db() as db:
            db.execute("BEGIN IMMEDIATE")
            now = time.time()
            self._held(db, report_id, token, now)
            db.execute("UPDATE reports SET state=?,updated=?,worker=NULL,token=NULL,lease_until=NULL,result=?,message=?,error=? WHERE report_id=?", (state, now, result, message, error, report_id))
            output = dict(db.execute("SELECT * FROM reports WHERE report_id=?", (report_id,)).fetchone())
            output.pop("token", None)
            db.commit()
            return output

    def retry(self, report_id):
        """Explicitly recover a failed consumer after its configuration is fixed.

        This cannot replay completed work, clear invalid evidence or steal a live
        lease. Revalidation happens inside the same writer transaction as reset.
        """
        review.identifier(report_id)
        with self._db() as db:
            db.execute("BEGIN IMMEDIATE")
            row = db.execute("SELECT * FROM reports WHERE report_id=?", (report_id,)).fetchone()
            if row is None or row["state"] != "failed":
                raise review.ReviewError("Only a terminal failed report can be retried explicitly")
            _, digest = self._read(report_id)
            if digest != row["fingerprint"]:
                raise review.ReviewError("Published immutable review evidence changed")
            db.execute("UPDATE reports SET state='pending',attempt=0,updated=?,worker=NULL,token=NULL,lease_until=NULL,result=NULL,message='',error='' WHERE report_id=?", (time.time(), report_id))
            output = dict(db.execute("SELECT * FROM reports WHERE report_id=?", (report_id,)).fetchone())
            output.pop("token", None)
            db.commit()
            return output

    def import_archive(self, data):
        """Validate a bounded plain USTAR browser archive, then atomically publish it.

        No extractall, compression, links, sparse entries, extended headers or extra
        members are accepted. Existing IDs are idempotent only for identical bytes.
        """
        if not isinstance(data, bytes) or not 0 < len(data) <= ARCHIVE_LIMIT or len(data) % 512:
            raise review.ReviewError("Browser review archive is missing or exceeds its byte limit")
        members, report_id = _archive_members(data)
        stage = Path(tempfile.mkdtemp(prefix=".import-", dir=self.root))
        try:
            for name, payload in members.items():
                with (stage / name).open("xb") as stream:
                    os.chmod(stage / name, 0o600)
                    stream.write(payload)
                    stream.flush()
                    os.fsync(stream.fileno())
            # Use the same strict reader against a non-published staging parent.
            # _Bundle deliberately does not validate the staging directory's hidden
            # name as an identifier; the manifest keeps the public report identity.
            fd = None
            try:
                if os.name != "nt":
                    fd = os.open(stage, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
                bundle = review._Bundle(stage, fd)
                manifest, image = self.store._complete(bundle, report_id)
                _browser(members["browser.json"], members["manifest.json"], manifest, image)
                if fd is not None:
                    os.fsync(fd)
            finally:
                if fd is not None:
                    os.close(fd)
            duplicate = False
            # Queue transaction serializes same-ID imports with other importers;
            # native publication uses collision-resistant IDs and atomic rename.
            with self._db() as db:
                db.execute("BEGIN IMMEDIATE")
                target = self.root / report_id
                if target.exists() or target.is_symlink():
                    try:
                        with self.store.bundle(report_id) as existing:
                            for name, payload in members.items():
                                if existing.read(name, len(payload)) != payload:
                                    raise review.ReviewError("Conflicting browser report identity already exists")
                    except FileNotFoundError:
                        raise review.ReviewError("Conflicting non-browser report identity already exists") from None
                    self._read(report_id)
                    duplicate = True
                else:
                    os.rename(stage, target)
                    if os.name != "nt":
                        parent = os.open(self.root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
                        try:
                            os.fsync(parent)
                        finally:
                            os.close(parent)
                db.commit()
            self.sync()
            return {"report_id": report_id, "duplicate": duplicate, "report": self._read(report_id)[0]}
        finally:
            if stage.exists():
                shutil.rmtree(stage)
