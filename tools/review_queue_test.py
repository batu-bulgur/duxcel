#!/usr/bin/env python3
"""Persistent review queue/import tests; no browser, desktop or compiler."""
import concurrent.futures
import contextlib
import hashlib
import io
import json
import multiprocessing
import os
from pathlib import Path
import struct
import subprocess
import sys
import tarfile
import tempfile
import threading
import unittest
from unittest import mock
import zlib

import native_review as review
import review_queue as queue


def png():
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress((b"\0" + b"\x12\x34\x56\xff" * 2) * 2)) + chunk(b"IEND", b"")


def manifest(report_id="DXR-one", project="duxcel"):
    return {"schema": review.SCHEMA, "version": 1, "report_id": report_id, "project_id": project,
            "session_id": "session-1", "frame": {"frame_id": "18446744073709551615", "project_id": project,
            "session_id": "session-1", "binary_sha256": "b" * 64, "window": {"width": 2, "height": 2},
            "framebuffer": {"width": 2, "height": 2}}, "target": {"frame_id": "18446744073709551615", "kind": "widget", "widget": {"id": "filter.apply"}},
            "widgets": [{"id": "null", "value": {"is_null": True, "exact_text": ""}},
                        {"id": "empty", "value": {"is_null": False, "exact_text": ""}},
                        {"id": "literal", "value": {"is_null": False, "exact_text": "NULL"}},
                        {"id": "exact", "value": {"is_null": False, "exact_text": "9007199254740993.1234567890"}}],
            "events": [], "note": "Native and browser evidence 😀"}


def files(report_id="DXR-one", project="duxcel"):
    payload = manifest(report_id, project)
    raw = json.dumps(payload, ensure_ascii=False).encode()
    image = png()
    sidecar = {"schema": "duxcel.browser-review", "version": 1, "report_id": report_id,
               "session_id": payload["session_id"], "frame_id": payload["frame"]["frame_id"],
               "binary_sha256": "b" * 64, "build": {"artifacts": {"duxcel.wasm": "b" * 64}},
               "logical": {"width": 2, "height": 2}, "canvas_backing": {"width": 2, "height": 2},
               "canvas_css": {"x": 10, "y": 20, "width": 4, "height": 4}, "device_pixel_ratio": 2,
               "transforms": {"logical_to_css": {"scale_x": 2, "scale_y": 2, "translate_x": 10, "translate_y": 20},
                              "logical_to_framebuffer": {"scale_x": 1, "scale_y": 1}},
               "transport": {"kind": "unified-review-queue", "durable": False},
               "files": {"manifest.json": {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()},
                         "frame.png": {"bytes": len(image), "sha256": hashlib.sha256(image).hexdigest(), "width": 2, "height": 2}}}
    return {"manifest.json": raw, "frame.png": image, "browser.json": json.dumps(sidecar).encode()}


def archive(members=None, report_id="DXR-one", project="duxcel", extra=(), format=tarfile.USTAR_FORMAT):
    result = io.BytesIO()
    with tarfile.open(fileobj=result, mode="w", format=format) as tar:
        for name, data in (members if members is not None else files(report_id, project)).items():
            info = tarfile.TarInfo(report_id + "/" + name)
            info.size, info.mode = len(data), 0o600
            tar.addfile(info, io.BytesIO(data))
        for info, data in extra:
            tar.addfile(info, io.BytesIO(data) if data is not None else None)
    return result.getvalue()


def process_claim(root, event, output):
    try:
        store = queue.ReviewQueue(root, "duxcel")
        event.wait(5)
        claim = store.claim("process-" + str(os.getpid()))
        output.put(claim["report_id"] if claim else None)
    except Exception as error:
        output.put({"error": str(error)})


class QueueTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="duxcel-review-queue-")
        self.root = Path(self.temporary.name) / "outbox"
        self.queue = queue.ReviewQueue(self.root, "duxcel")

    def tearDown(self):
        self.temporary.cleanup()

    def native(self, report_id="DXR-one", project="duxcel"):
        folder = self.root / report_id
        folder.mkdir()
        payload = files(report_id, project)
        for name in ("manifest.json", "frame.png"):
            (folder / name).write_bytes(payload[name])
        return folder

    def test_native_discovery_and_browser_import_share_ordered_persistent_queue(self):
        self.native("DXR-native")
        imported = self.queue.import_archive(archive(report_id="DXR-browser"))
        self.assertFalse(imported["duplicate"])
        restarted = queue.ReviewQueue(self.root, "duxcel")
        listing = restarted.list()
        self.assertEqual({row["kind"] for row in listing["reports"]}, {"native", "browser"})
        self.assertEqual({row["state"] for row in listing["reports"]}, {"pending"})
        claimed = [restarted.claim("agent"), restarted.claim("agent")]
        self.assertEqual({row["report_id"] for row in claimed}, {"DXR-native", "DXR-browser"})
        for row in claimed:
            lifecycle = row["report"]["lifecycle"]
            self.assertFalse(lifecycle["acknowledged"])
            self.assertEqual((lifecycle["design"], lifecycle["implementation"], lifecycle["verification"]), ("pending", "not-started", "unverified"))
            self.assertEqual(row["report"]["manifest"]["widgets"][3]["value"]["exact_text"], "9007199254740993.1234567890")
        self.assertIsNone(restarted.claim("no-duplicate"))

    def test_two_process_claimers_never_receive_the_same_report(self):
        self.native()
        ctx = multiprocessing.get_context("spawn")
        event, output = ctx.Event(), ctx.Queue()
        workers = [ctx.Process(target=process_claim, args=(str(self.root), event, output)) for _ in range(2)]
        for worker in workers:
            worker.start()
        event.set()
        results = [output.get(timeout=10) for _ in workers]
        for worker in workers:
            worker.join(10)
            self.assertEqual(worker.exitcode, 0)
        self.assertCountEqual(results, ["DXR-one", None])

    def test_abrupt_consumer_exit_keeps_durable_lease_and_recovers(self):
        self.native()
        script = ("import json,os,sys;sys.path.insert(0,sys.argv[1]);"
                  "from review_queue import ReviewQueue;"
                  "q=ReviewQueue(sys.argv[2],'duxcel');"
                  "print(json.dumps(q.claim('crashing-process',30)),flush=True);os._exit(23)")
        result = subprocess.run([sys.executable, "-c", script, str(Path(__file__).parent), str(self.root)],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 23, result.stderr)
        previous = json.loads(result.stdout)
        restarted = queue.ReviewQueue(self.root, "duxcel")
        self.assertIsNone(restarted.claim("too-early"))
        with mock.patch.object(queue.time, "time", return_value=previous["lease_until"] + 1):
            recovered = restarted.claim("recovered-process")
            self.assertEqual(recovered["attempt"], 2)
            self.assertNotEqual(recovered["token"], previous["token"])
            restarted.finish("DXR-one", recovered["token"], result="triaged")
        self.assertEqual(queue.ReviewQueue(self.root, "duxcel").list()["reports"][0]["state"], "completed")

    def test_many_concurrent_claimers_get_each_report_exactly_once(self):
        for index in range(8):
            self.native("DXR-" + str(index))
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(lambda n: queue.ReviewQueue(self.root, "duxcel").claim("agent-" + str(n)), range(8)))
        self.assertEqual(len({row["report_id"] for row in results}), 8)
        self.assertEqual({row["attempt"] for row in results}, {1})

    def test_expired_lease_recovers_after_restart_and_stale_tokens_are_rejected(self):
        self.native()
        with mock.patch.object(queue.time, "time", return_value=100):
            first = self.queue.claim("crashed", 20)
        restarted = queue.ReviewQueue(self.root, "duxcel")
        with mock.patch.object(queue.time, "time", return_value=121):
            self.assertEqual(restarted.sync()["recovered"], ["DXR-one"])
            second = restarted.claim("replacement", 20)
            self.assertEqual(second["attempt"], 2)
            self.assertNotEqual(second["token"], first["token"])
            for action in (lambda: restarted.renew("DXR-one", first["token"]),
                           lambda: restarted.finish("DXR-one", first["token"]),
                           lambda: restarted.release("DXR-one", first["token"])):
                with self.assertRaises(review.ReviewError):
                    action()
            restarted.finish("DXR-one", second["token"], result="triaged", message='{"finding":"Needs fix"}')
        row = restarted.list()["reports"][0]
        self.assertEqual((row["state"], row["result"], row["message"]), ("completed", "triaged", '{"finding":"Needs fix"}'))

    def test_renewal_release_retry_and_terminal_failure_preserve_human_lifecycle(self):
        self.native()
        with mock.patch.object(queue.time, "time", return_value=100):
            first = self.queue.claim("worker", 10)
        with mock.patch.object(queue.time, "time", return_value=105):
            renewed = self.queue.renew("DXR-one", first["token"], 20)
            self.assertEqual(renewed["lease_until"], 125)
            self.queue.release("DXR-one", first["token"], error="consumer failed", retry=True)
            second = self.queue.claim("worker2")
            self.assertEqual(second["attempt"], 2)
            self.queue.release("DXR-one", second["token"], error="Consumer retry limit", retry=False)
        row = self.queue.list()["reports"][0]
        self.assertEqual(row["state"], "failed")
        self.assertEqual(row["error"], "Consumer retry limit")
        self.assertIsNone(self.queue.claim("never"))
        self.assertFalse(self.queue.store.read("DXR-one")["lifecycle"]["acknowledged"])

    def test_expired_token_cannot_renew_without_another_claim(self):
        self.native()
        with mock.patch.object(queue.time, "time", return_value=100):
            held = self.queue.claim("worker", 1)
        with mock.patch.object(queue.time, "time", return_value=101):
            with self.assertRaises(review.ReviewError):
                self.queue.renew("DXR-one", held["token"])

    def test_listing_hides_tokens_and_decisions_survive_queue_completion(self):
        self.native()
        self.queue.store.update("DXR-one", "status", "human", design="needs-changes", implementation="in-progress", verification="failed")
        held = self.queue.claim("worker")
        self.assertNotIn("token", self.queue.list()["reports"][0])
        self.queue.finish("DXR-one", held["token"], message="x" * 16000)
        self.assertEqual(self.queue.store.read("DXR-one")["lifecycle"]["design"], "needs-changes")
        self.assertIsNone(self.queue.claim("next"))

    def test_partial_hidden_wrong_project_and_malformed_native_are_not_delivered(self):
        self.native("DXR-valid")
        self.native("DXR-foreign", project="foreign")
        partial = self.root / "DXR-partial"
        partial.mkdir()
        (partial / "manifest.json").write_bytes(files("DXR-partial")["manifest.json"])
        (self.root / ".import-hidden").mkdir()
        scan = self.queue.sync()
        self.assertEqual(scan["added"], ["DXR-valid"])
        self.assertEqual({row["report_id"] for row in scan["rejected"]}, {"DXR-foreign", "DXR-partial"})
        self.assertEqual(self.queue.claim("worker")["report_id"], "DXR-valid")
        self.assertIsNone(self.queue.claim("worker"))
        (partial / "frame.png").write_bytes(png())
        self.assertEqual(self.queue.claim("worker")["report_id"], "DXR-partial")

    def test_evidence_mutation_invalidates_queued_report(self):
        folder = self.native()
        self.queue.sync()
        changed = manifest()
        changed["note"] = "Mutation after publication"
        (folder / "manifest.json").write_text(json.dumps(changed))
        scan = self.queue.sync()
        self.assertIn("changed", scan["rejected"][0]["error"])
        self.assertIsNone(self.queue.claim("worker"))
        self.assertEqual(self.queue.list()["reports"][0]["state"], "invalid")

    def test_duplicate_import_is_idempotent_but_same_id_different_content_conflicts(self):
        body = archive()
        self.queue.import_archive(body)
        held = self.queue.claim("worker")
        self.assertTrue(self.queue.import_archive(body)["duplicate"])
        self.assertEqual(self.queue.list()["reports"][0]["attempt"], 1)
        different = files()
        payload = json.loads(different["manifest.json"])
        payload["note"] = "Another genuine browser report with same ID"
        different["manifest.json"] = json.dumps(payload).encode()
        sidecar = json.loads(different["browser.json"])
        sidecar["files"]["manifest.json"] = {"bytes": len(different["manifest.json"]), "sha256": hashlib.sha256(different["manifest.json"]).hexdigest()}
        different["browser.json"] = json.dumps(sidecar).encode()
        with self.assertRaises(review.ReviewError):
            self.queue.import_archive(archive(different))
        self.queue.finish("DXR-one", held["token"])
        self.assertEqual((self.root / "DXR-one/manifest.json").read_bytes(), files()["manifest.json"])

    def test_concurrent_identical_import_publishes_once(self):
        body = archive()
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            results = list(pool.map(lambda _: queue.ReviewQueue(self.root, "duxcel").import_archive(body), range(4)))
        self.assertEqual(sum(not row["duplicate"] for row in results), 1)
        self.assertEqual(len(self.queue.list()["reports"]), 1)
        self.assertFalse(any(path.name.startswith(".import-") for path in self.root.iterdir()))

    def test_browser_wrong_project_bad_identity_hash_build_geometry_fail_before_publish(self):
        scenarios = []
        scenarios.append(files(project="foreign"))
        for update in ({"report_id": "DXR-other"}, {"session_id": "session-other"}, {"frame_id": "42"},
                       {"binary_sha256": "c" * 64}, {"version": True}, {"logical": {"width": 3, "height": 2}},
                       {"canvas_backing": {"width": 2, "height": 3}}, {"device_pixel_ratio": 0},
                       {"build": {"artifacts": {"duxcel.wasm": "c" * 64}}}, {"transforms": {}}):
            member = files()
            sidecar = json.loads(member["browser.json"])
            sidecar.update(update)
            member["browser.json"] = json.dumps(sidecar).encode()
            scenarios.append(member)
        for name in ("manifest.json", "frame.png"):
            member = files()
            member[name] += b" "
            scenarios.append(member)
        for members in scenarios:
            with self.subTest(sidecar=members["browser.json"][:70]), self.assertRaises(review.ReviewError):
                self.queue.import_archive(archive(members))
            self.assertFalse((self.root / "DXR-one").exists())
            self.assertFalse(any(path.name.startswith(".import-") for path in self.root.iterdir()))

    def test_archive_traversal_absolute_paths_extra_files_symlinks_and_extensions_rejected(self):
        body = archive()
        for path in ("../outside", "/tmp/outside", "DXR-one/../frame.png", "DXR-one/extra.txt", "DXR-other/frame.png"):
            info = tarfile.TarInfo(path)
            info.size = 1
            with self.subTest(path=path), self.assertRaises(review.ReviewError):
                self.queue.import_archive(archive(extra=((info, b"x"),)))
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.DIRTYPE, tarfile.FIFOTYPE):
            info = tarfile.TarInfo("DXR-one/link")
            info.type, info.linkname = kind, "../outside"
            with self.subTest(kind=kind), self.assertRaises(review.ReviewError):
                self.queue.import_archive(archive(extra=((info, None),)))
        info = tarfile.TarInfo("DXR-one/" + "x" * 180)
        info.size = 1
        for format in (tarfile.GNU_FORMAT, tarfile.PAX_FORMAT):
            with self.assertRaises(review.ReviewError):
                self.queue.import_archive(archive(extra=((info, b"x"),), format=format))
        for malformed in (body[:1024], body[:-1], body + b"\0" * 511 + b"x", body[:148] + b"000000\0 " + body[156:], b"not a tar"):
            with self.subTest(length=len(malformed)), self.assertRaises(review.ReviewError):
                self.queue.import_archive(malformed)
        self.assertFalse((self.root.parent / "outside").exists())

    def test_invalid_first_header_paths_and_link_types_are_rejected_directly(self):
        for name in ("../outside", "/tmp/outside", "nested/manifest.json", "other.txt"):
            member = files()
            member[name] = member.pop("manifest.json")
            # Put the invalid member first, before the three-file count is reached.
            member = {name: member.pop(name), **member}
            with self.subTest(name=name), self.assertRaisesRegex(review.ReviewError, "path"):
                self.queue.import_archive(archive(member))
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.DIRTYPE, tarfile.FIFOTYPE, tarfile.GNUTYPE_LONGNAME, tarfile.XHDTYPE):
            body = bytearray(archive())
            body[156] = kind[0]
            body[148:156] = b" " * 8
            checksum = sum(body[:512])
            body[148:156] = (format(checksum, "06o") + "\0 ").encode()
            with self.subTest(kind=kind), self.assertRaisesRegex(review.ReviewError, "plain USTAR"):
                self.queue.import_archive(bytes(body))

    def test_failed_publication_cleans_staging_and_can_retry_without_data_loss(self):
        body = archive()
        with mock.patch.object(queue.os, "rename", side_effect=OSError("publication unavailable")), self.assertRaises(OSError):
            self.queue.import_archive(body)
        self.assertFalse((self.root / "DXR-one").exists())
        self.assertFalse(any(path.name.startswith(".import-") for path in self.root.iterdir()))
        self.assertEqual(self.queue.list()["reports"], [])
        self.assertFalse(self.queue.import_archive(body)["duplicate"])
        held = self.queue.claim("worker")
        self.queue.finish("DXR-one", held["token"], result="triaged")
        self.assertTrue(self.queue.import_archive(body)["duplicate"])
        self.assertEqual(self.queue.list()["reports"][0]["state"], "completed")

    def test_archive_exact_files_duplicates_size_limits_and_symlink_existing_target(self):
        only_two = files()
        only_two.pop("browser.json")
        with self.assertRaises(review.ReviewError):
            self.queue.import_archive(archive(only_two))
        info = tarfile.TarInfo("DXR-one/frame.png")
        info.size = 1
        with self.assertRaises(review.ReviewError):
            self.queue.import_archive(archive(extra=((info, b"x"),)))
        with mock.patch.object(queue, "ARCHIVE_LIMIT", 10), self.assertRaises(review.ReviewError):
            self.queue.import_archive(archive())
        with mock.patch.object(review, "JSON_LIMIT", 10), self.assertRaises(review.ReviewError):
            self.queue.import_archive(archive())
        outside = self.root.parent / "outside"
        outside.mkdir()
        (self.root / "DXR-one").symlink_to(outside, target_is_directory=True)
        with self.assertRaises((OSError, review.ReviewError)):
            self.queue.import_archive(archive())
        self.assertEqual(list(outside.iterdir()), [])

    def test_project_database_identity_private_files_and_symlink_database(self):
        if os.name != "nt":
            self.assertEqual(self.queue.database.stat().st_mode & 0o777, 0o600)
        with self.assertRaises(review.ReviewError):
            queue.ReviewQueue(self.root, "foreign")
        self.queue.database.unlink()
        target = self.root.parent / "target"
        target.write_bytes(b"private")
        self.queue.database.symlink_to(target)
        with self.assertRaises(review.ReviewError):
            queue.ReviewQueue(self.root, "duxcel")
        self.assertEqual(target.read_bytes(), b"private")

    def test_explicit_retry_recovers_failed_consumer_and_resets_attempts_durably(self):
        self.native()
        held = self.queue.claim("worker")
        self.queue.release("DXR-one", held["token"], error="Authentication failed", retry=False)
        self.assertIsNone(self.queue.claim("blocked-until-manual-retry"))
        with mock.patch.object(queue.time, "time", return_value=held["updated"] + 100):
            retried = self.queue.retry("DXR-one")
        self.assertEqual((retried["state"], retried["attempt"], retried["result"], retried["message"], retried["error"]),
                         ("pending", 0, None, "", ""))
        self.assertGreater(retried["updated"], held["updated"])
        self.assertNotIn("token", retried)
        restarted = queue.ReviewQueue(self.root, "duxcel")
        recovered = restarted.claim("fixed-worker")
        self.assertEqual(recovered["attempt"], 1)
        self.assertFalse(recovered["report"]["lifecycle"]["acknowledged"])
        restarted.finish("DXR-one", recovered["token"], result="triaged")
        self.assertEqual(restarted.list()["reports"][0]["state"], "completed")

    def test_explicit_retry_cannot_steal_leases_replay_completed_or_reset_pending(self):
        self.native()
        self.queue.sync()
        with self.assertRaises(review.ReviewError):
            self.queue.retry("DXR-one")
        held = self.queue.claim("owner")
        with self.assertRaises(review.ReviewError):
            self.queue.retry("DXR-one")
        self.assertEqual(self.queue.renew("DXR-one", held["token"])["worker"], "owner")
        self.queue.finish("DXR-one", held["token"], result="triaged", message="Retain result")
        with self.assertRaises(review.ReviewError):
            self.queue.retry("DXR-one")
        result = self.queue.list()["reports"][0]
        self.assertEqual((result["state"], result["result"], result["message"], result["attempt"]),
                         ("completed", "triaged", "Retain result", 1))
        with self.assertRaises(review.ReviewError):
            self.queue.retry("DXR-missing")

    def test_explicit_retry_requires_original_valid_immutable_evidence(self):
        folder = self.native()
        held = self.queue.claim("worker")
        self.queue.release("DXR-one", held["token"], error="Consumer failed", retry=False)
        original = (folder / "manifest.json").read_bytes()
        altered = original.replace(b"Native and browser", b"Mative and browser")
        (folder / "manifest.json").write_bytes(altered)
        # Retry revalidates directly, even before sync can observe the mutation.
        with self.assertRaisesRegex(review.ReviewError, "changed"):
            self.queue.retry("DXR-one")
        with self.queue._db() as db:
            row = db.execute("SELECT state,attempt,error FROM reports WHERE report_id='DXR-one'").fetchone()
            self.assertEqual(tuple(row), ("failed", 1, "Consumer failed"))
        self.queue.sync()
        self.assertEqual(self.queue.list()["reports"][0]["state"], "invalid")
        (folder / "manifest.json").write_bytes(original)
        with self.assertRaises(review.ReviewError):
            self.queue.retry("DXR-one")

    def test_idle_sync_claim_and_listing_never_reread_unchanged_completed_png(self):
        self.native()
        self.queue.sync()
        held = self.queue.claim("worker")
        self.queue.finish("DXR-one", held["token"], result="triaged")
        with mock.patch.object(review, "_png", side_effect=AssertionError("Idle polling reread PNG")):
            for _ in range(8):
                self.assertEqual(self.queue.sync()["rejected"], [])
                self.assertIsNone(self.queue.claim("idle"))
                self.assertEqual(self.queue.list()["reports"][0]["state"], "completed")
        self.assertEqual(len(self.queue._validation_cache), 1)
        self.assertEqual(len(self.queue._validation_cache["DXR-one"]), 4)

    def test_unchanged_invalid_and_partial_reports_do_not_reread_large_files(self):
        folder = self.native()
        (folder / "frame.png").write_bytes(b"Malformed image")
        partial = self.root / "DXR-partial"
        partial.mkdir()
        (partial / "manifest.json").write_bytes(files("DXR-partial")["manifest.json"])
        with mock.patch.object(review, "_png", wraps=review._png) as decode:
            first = self.queue.sync()
            self.assertEqual(len(first["rejected"]), 2)
            self.assertEqual(decode.call_count, 1)
            for _ in range(8):
                self.assertEqual(len(self.queue.sync()["rejected"]), 2)
            self.assertEqual(decode.call_count, 1)
            (folder / "frame.png").write_bytes(png())
            (partial / "frame.png").write_bytes(png())
            self.assertEqual(set(self.queue.sync()["added"]), {"DXR-one", "DXR-partial"})
            self.assertEqual(decode.call_count, 3)

    def test_cached_discovery_does_not_skip_full_claim_validation(self):
        self.native()
        self.queue.sync()
        with mock.patch.object(review, "_png", wraps=review._png) as decode:
            claim = self.queue.claim("worker")
            self.assertEqual(decode.call_count, 1)
            self.assertEqual(claim["report_id"], "DXR-one")

    def test_lifecycle_signature_change_refreshes_validation_without_changing_evidence(self):
        self.native()
        self.queue.sync()
        self.queue.store.update("DXR-one", "status", "human", design="needs-changes")
        with mock.patch.object(review, "_png", wraps=review._png) as decode:
            self.assertEqual(self.queue.sync()["rejected"], [])
            self.assertEqual(decode.call_count, 1)
            self.assertEqual(self.queue.sync()["rejected"], [])
            self.assertEqual(decode.call_count, 1)
        self.assertEqual(self.queue.claim("worker")["report"]["lifecycle"]["design"], "needs-changes")

    def test_restoring_mtime_after_same_size_tampering_does_not_fool_cache(self):
        folder = self.native()
        self.queue.sync()
        path = folder / "manifest.json"
        old = path.stat()
        contents = path.read_bytes()
        changed = contents.replace(b"Native and browser", b"Mative and browser")
        self.assertNotEqual(changed, contents)
        self.assertEqual(len(changed), len(contents))
        path.write_bytes(changed)
        os.utime(path, ns=(old.st_atime_ns, old.st_mtime_ns))
        self.assertNotEqual(path.stat().st_ctime_ns, old.st_ctime_ns)
        rejected = self.queue.sync()["rejected"]
        self.assertEqual(rejected[0]["report_id"], "DXR-one")
        self.assertIn("changed", rejected[0]["error"])
        self.assertIsNone(self.queue.claim("never"))

    def test_concurrent_publication_during_scan_cannot_invalidate_new_queue_row(self):
        scanned, published = threading.Event(), threading.Event()
        original = os.scandir
        result = []

        @contextlib.contextmanager
        def delayed_scandir(path):
            with original(path) as entries:
                snapshot = list(entries)
            if threading.current_thread().name == "old-queue-scan":
                scanned.set()
                if not published.wait(5):
                    raise AssertionError("Concurrent publisher did not complete")
            yield iter(snapshot)

        def old_scan():
            try:
                result.append(self.queue.sync())
            except Exception as error:
                result.append(error)

        with mock.patch.object(queue.os, "scandir", delayed_scandir):
            thread = threading.Thread(target=old_scan, name="old-queue-scan")
            thread.start()
            try:
                self.assertTrue(scanned.wait(5))
                publisher = queue.ReviewQueue(self.root, "duxcel")
                self.assertFalse(publisher.import_archive(archive())["duplicate"])
            finally:
                published.set()
                thread.join(5)
        self.assertFalse(thread.is_alive())
        self.assertEqual(len(result), 1)
        self.assertIsInstance(result[0], dict)
        self.assertEqual(result[0]["rejected"], [])
        self.assertEqual(self.queue.list()["reports"][0]["state"], "pending")
        self.assertEqual(self.queue.claim("worker")["report_id"], "DXR-one")

    def test_queue_scan_and_consumer_arguments_are_bounded(self):
        self.native()
        for duration in (0, -1, True, float("inf"), float("nan"), 86401):
            with self.subTest(duration=duration), self.assertRaises(review.ReviewError):
                self.queue.claim("worker", duration)
        for worker in ("", "x" * 129):
            with self.assertRaises(review.ReviewError):
                self.queue.claim(worker)
        held = self.queue.claim("worker")
        with self.assertRaises(review.ReviewError):
            self.queue.finish("DXR-one", held["token"], message="x" * (queue.MESSAGE_LIMIT + 1))
        with mock.patch.object(review, "REPORT_LIST_LIMIT", 0), self.assertRaises(review.ReviewError):
            self.queue.sync()


if __name__ == "__main__":
    unittest.main()
