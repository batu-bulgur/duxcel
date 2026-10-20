#!/usr/bin/env python3
"""Native outbox reader/lifecycle tests; no app, engine, daemon or desktop."""
import concurrent.futures
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib

SPEC = importlib.util.spec_from_file_location("native_review", Path(__file__).resolve().parents[1].joinpath("native_review.py"))
review = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(review)


def chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)


def png(width=2, height=2):
    scanline = b"\0" + b"\x11\x22\x33\xff" * width
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(scanline * height)) + chunk(b"IEND", b"")


def manifest(report_id="DXR-one", project="duxcel"):
    return {"schema": review.SCHEMA, "version": 1, "report_id": report_id, "session_id": "session_1", "project_id": project,
            "frame": {"frame_id": "18446744073709551615", "project_id": project, "framebuffer": {"width": 2, "height": 2},
                      "ui_scale": 1.5, "grid_zoom": .8, "binary_sha256": "abc", "source_fingerprint": "dirty-source"},
            "target": {"kind": "widget", "frame_id": "18446744073709551615", "widget": {"id": "filter.apply"}},
            "widgets": [{"id": "decimal", "value": {"sql_type": "DECIMAL(38,10)", "is_null": False, "exact_text": "9007199254740993.1234567890"}},
                        {"id": "null", "value": {"sql_type": "VARCHAR", "is_null": True, "exact_text": ""}},
                        {"id": "empty", "value": {"sql_type": "VARCHAR", "is_null": False, "exact_text": ""}},
                        {"id": "literal", "value": {"sql_type": "VARCHAR", "is_null": False, "exact_text": "NULL"}}],
            "events": [{"kind": "trace", "text": "filter open"}], "note": "A quote \" and newline\nİstanbul 😀"}


class ReviewTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="duxcel-review-reader-")
        self.root = Path(self.temporary.name)
        self.store = review.ReviewStore(self.root, "duxcel")
        self.report = self.root / "DXR-one"
        self.report.mkdir()
        self.payload = manifest()
        self.write()

    def tearDown(self):
        self.temporary.cleanup()

    def write(self, payload=None):
        (self.report / "manifest.json").write_text(json.dumps(self.payload if payload is None else payload, ensure_ascii=False))
        (self.report / "frame.png").write_bytes(png())

    def test_platform_root_override_and_project_scope(self):
        with mock.patch.dict(os.environ, {"DUXCEL_REVIEW_OUTBOX": str(self.root)}):
            self.assertEqual(review.default_root("project"), self.root)
        with mock.patch.dict(os.environ, {"DUXCEL_REVIEW_OUTBOX": "relative"}):
            with self.assertRaises(review.ReviewError):
                review.default_root("project")
        with mock.patch.dict(os.environ, {}, clear=True), mock.patch.object(review.sys, "platform", "darwin"), mock.patch.object(review.Path, "home", return_value=self.root):
            self.assertEqual(review.default_root("project"), self.root / "Library/Application Support/Duxcel/Review/project")
        for value in ("", "../project", "x/y", "x\\y", "é", "a" * 96):
            with self.subTest(project=value), self.assertRaises(review.ReviewError):
                review.default_root(value)

    def test_default_project_matches_native_common_directory_and_gitless_worktrees(self):
        common = self.root / "shared.git"
        common.mkdir()
        expected = "duxcel-" + hashlib.sha256(str(common.resolve()).encode()).hexdigest()[:16]
        with mock.patch.object(review.subprocess, "check_output", return_value="shared.git\n"):
            self.assertEqual(review.default_project(self.root), expected)
        (self.root / ".git").mkdir()
        expected_main = "duxcel-" + hashlib.sha256(str((self.root / ".git").resolve()).encode()).hexdigest()[:16]
        with mock.patch.object(review.subprocess, "check_output", side_effect=FileNotFoundError("git unavailable")):
            self.assertEqual(review.default_project(self.root), expected_main)
            (self.root / ".git").rmdir()
            worktree = common / "worktrees" / "other"
            worktree.mkdir(parents=True)
            (worktree / "commondir").write_text("../..\n")
            (self.root / ".git").write_text("gitdir: shared.git/worktrees/other\n")
            self.assertEqual(review.default_project(self.root), expected)
            (self.root / ".git").unlink()
            expected_standalone = "duxcel-" + hashlib.sha256(str(self.root.resolve()).encode()).hexdigest()[:16]
            self.assertEqual(review.default_project(self.root), expected_standalone)

    def test_complete_report_preserves_native_types_and_exact_values(self):
        self.payload["future_extension"] = {"enabled": True}
        self.write()
        result = self.store.read("DXR-one")
        self.assertEqual(result["manifest"], self.payload)
        self.assertEqual(result["image"]["width"], 2)
        self.assertEqual(result["image"]["height"], 2)
        self.assertEqual(result["manifest"]["frame"]["frame_id"], "18446744073709551615")
        values = [item["value"] for item in result["manifest"]["widgets"]]
        self.assertEqual(values[0]["exact_text"], "9007199254740993.1234567890")
        self.assertTrue(values[1]["is_null"])
        self.assertFalse(values[2]["is_null"])
        self.assertEqual(values[3]["exact_text"], "NULL")
        self.assertEqual(result["lifecycle"]["revision"], 0)

    def test_strict_schema_identity_structure_and_frame_checks(self):
        changes = [{"schema": "other"}, {"version": 2}, {"version": True}, {"report_id": "DXR-another"},
                   {"project_id": "foreign"}, {"session_id": "../bad"}, {"frame": []}, {"target": None},
                   {"widgets": [{}] * 2049}, {"events": [{}] * 129}, {"note": "é" * 2049}]
        for change in changes:
            with self.subTest(change=list(change)), self.assertRaises(review.ReviewError):
                payload = copy.deepcopy(self.payload)
                payload.update(change)
                self.write(payload)
                self.store.read("DXR-one")
        for key, value in (("project_id", "foreign"), ("framebuffer", {"width": 3, "height": 2})):
            payload = copy.deepcopy(self.payload)
            payload["frame"][key] = value
            self.write(payload)
            with self.assertRaises(review.ReviewError):
                self.store.read("DXR-one")
        payload = copy.deepcopy(self.payload)
        payload["target"]["frame_id"] = "123"
        self.write(payload)
        with self.assertRaises(review.ReviewError):
            self.store.read("DXR-one")

    def test_json_duplicate_nonfinite_invalid_utf8_and_surrogates_rejected(self):
        for data in (b'{"schema":1,"schema":2}', b'{"x":NaN}', b'{"x":1e999}', b'{"x":"\xff"}', b'{"x":"\\ud800"}', b'{"x":' + b'9' * 5000 + b'}'):
            with self.subTest(data=data), self.assertRaises(review.ReviewError):
                review._json(data)

    def test_partial_reports_hidden_stages_and_foreign_projects_are_explicit(self):
        (self.root / ".tmp-unpublished").mkdir()
        (self.root / "DXR-partial").mkdir()
        foreign = self.root / "DXR-foreign"
        foreign.mkdir()
        (foreign / "manifest.json").write_text(json.dumps(manifest("DXR-foreign", "different")))
        (foreign / "frame.png").write_bytes(png())
        listing = self.store.list()
        self.assertEqual([item["report_id"] for item in listing["reports"]], ["DXR-one"])
        self.assertEqual({item["report_id"] for item in listing["rejected"]}, {"DXR-partial", "DXR-foreign"})
        with self.assertRaises(OSError):
            self.store.read("DXR-partial")

    def test_path_traversal_and_symlink_report_files_are_rejected(self):
        for value in ("../DXR-one", "DXR-one/frame.png", ".tmp-unpublished", "", "a" * 96):
            with self.subTest(value=value), self.assertRaises(review.ReviewError):
                self.store.read(value)
        (self.root / "DXR-link").symlink_to(self.report, target_is_directory=True)
        with self.assertRaises(OSError):
            self.store.read("DXR-link")
        outside = self.root / "outside.json"
        outside.write_text(json.dumps(self.payload))
        (self.report / "manifest.json").unlink()
        (self.report / "manifest.json").symlink_to(outside)
        with self.assertRaises(OSError):
            self.store.read("DXR-one")

    def test_png_crc_truncation_structure_and_byte_limits(self):
        original = png()
        bad = bytearray(original)
        bad[-1] ^= 1
        for data in (original[:8], original[:-1], bytes(bad), original + b"extra", b"not a PNG"):
            with self.subTest(length=len(data)), self.assertRaises(review.ReviewError):
                (self.report / "frame.png").write_bytes(data)
                self.store.read("DXR-one")
        self.write()
        with (self.report / "manifest.json").open("wb") as stream:
            stream.truncate(review.JSON_LIMIT + 1)
        with self.assertRaises(review.ReviewError):
            self.store.read("DXR-one")
        self.write()
        with (self.report / "frame.png").open("wb") as stream:
            stream.truncate(review.PNG_LIMIT + 1)
        with self.assertRaises(review.ReviewError):
            self.store.read("DXR-one")

    def test_lifecycle_survives_reopen_and_keeps_design_implementation_separate(self):
        before = (self.report / "manifest.json").read_bytes(), (self.report / "frame.png").read_bytes()
        self.store.update("DXR-one", "ack", "Codex", "Investigating")
        self.store.update("DXR-one", "reply", "Codex", "The native widget is filter.apply")
        state = self.store.update("DXR-one", "status", "Codex", implementation="ready-for-review")
        self.assertEqual(state["design"], "pending")
        state = self.store.update("DXR-one", "status", "user", design="accepted")
        self.assertEqual(state["implementation"], "ready-for-review")
        self.assertEqual(state["verification"], "unverified")
        other = review.ReviewStore(self.root, "duxcel").read("DXR-one")["lifecycle"]
        self.assertTrue(other["acknowledged"])
        self.assertEqual(other["revision"], 4)
        self.assertEqual([item["sequence"] for item in other["history"]], [1, 2, 3, 4])
        self.assertEqual(before, ((self.report / "manifest.json").read_bytes(), (self.report / "frame.png").read_bytes()))
        self.assertNotIn("history", self.store.list()["reports"][0]["lifecycle"])

    def test_failed_atomic_replacement_preserves_prior_lifecycle_and_cleans_stage(self):
        self.store.update("DXR-one", "ack", "Codex")
        before = (self.report / "lifecycle.json").read_bytes()
        with mock.patch.object(review.os, "replace", side_effect=OSError("disk denied")), self.assertRaises(OSError):
            self.store.update("DXR-one", "reply", "Codex", "retry me")
        self.assertEqual((self.report / "lifecycle.json").read_bytes(), before)
        self.assertEqual([p.name for p in self.report.iterdir() if p.name.startswith(".lifecycle-")], [])
        self.assertEqual(self.store.update("DXR-one", "reply", "Codex", "retry succeeded")["revision"], 2)

    def test_concurrent_lifecycle_updates_are_not_lost(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            futures = [pool.submit(review.ReviewStore(self.root, "duxcel").update, "DXR-one", "reply", "Codex", f"reply-{i}") for i in range(12)]
            for future in futures:
                future.result(timeout=10)
        state = self.store.read("DXR-one")["lifecycle"]
        self.assertEqual(state["revision"], 12)
        self.assertEqual({item["message"] for item in state["history"]}, {f"reply-{i}" for i in range(12)})
        base = [sys.executable, str(Path(review.__file__)), "--project", "duxcel", "--root", str(self.root), "reply", "DXR-one", "--message"]
        processes = [subprocess.Popen(base + [f"process-{i}"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) for i in range(4)]
        for process in processes:
            output, error = process.communicate(timeout=10)
            self.assertEqual(process.returncode, 0, error)
            self.assertIsInstance(json.loads(output), dict)
        self.assertEqual(self.store.read("DXR-one")["lifecycle"]["revision"], 16)

    def test_corrupt_or_full_lifecycle_is_retained_without_mutation(self):
        state = self.store.update("DXR-one", "ack", "Codex")
        changes = [{"revision": True}, {"revision": 2}, {"design": "implemented"},
                   {"history": [dict(state["history"][0], sequence=True)]},
                   {"history": [dict(state["history"][0], changes={"design": "accepted"})]}]
        for change in changes:
            with self.subTest(change=change):
                broken = dict(state, **change)
                before = json.dumps(broken).encode()
                (self.report / "lifecycle.json").write_bytes(before)
                with self.assertRaises(review.ReviewError):
                    self.store.update("DXR-one", "reply", "Codex", "retain corrupted evidence")
                self.assertEqual((self.report / "lifecycle.json").read_bytes(), before)
        full = dict(state, revision=review.HISTORY_LIMIT,
                    history=[dict(state["history"][0], sequence=i) for i in range(1, review.HISTORY_LIMIT + 1)])
        before = json.dumps(full).encode()
        (self.report / "lifecycle.json").write_bytes(before)
        with self.assertRaises(review.ReviewError):
            self.store.update("DXR-one", "reply", "Codex", "retain bounded history")
        self.assertEqual((self.report / "lifecycle.json").read_bytes(), before)

    def test_oversized_text_invalid_states_and_symlink_lifecycle_never_mutate(self):
        for kind, actor, message, changes in (("reply", "agent", "", {}), ("reply", "agent", "x" * 4097, {}),
                                               ("ack", "", "", {}), ("status", "agent", "", {"design": "implemented"}),
                                               ("ack", "agent", "", {"design": "accepted"})):
            with self.subTest(kind=kind, changes=changes), self.assertRaises(review.ReviewError):
                self.store.update("DXR-one", kind, actor, message, **changes)
        outside = self.root / "foreign-state.json"
        outside.write_text("do not overwrite")
        (self.report / "lifecycle.json").symlink_to(outside)
        with self.assertRaises(OSError):
            self.store.update("DXR-one", "ack", "agent")
        self.assertEqual(outside.read_text(), "do not overwrite")

    def test_cli_offline_read_ack_reply_status_and_failure(self):
        base = [sys.executable, str(Path(review.__file__)), "--root", str(self.root), "--project", "duxcel"]
        for args in (["list"], ["read", "DXR-one"], ["ack", "DXR-one", "--actor", "Codex"],
                     ["reply", "DXR-one", "--message", "Inspect the captured target"],
                     ["status", "DXR-one", "--implementation", "ready-for-review"], ["status", "DXR-one"]):
            result = subprocess.run(base + args, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIsInstance(json.loads(result.stdout), dict)
        result = subprocess.run(base + ["read", "../outside"], text=True, capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("error", json.loads(result.stderr))

    def test_cli_default_project_finds_the_matching_native_outbox(self):
        project = review.default_project()
        self.payload["project_id"] = self.payload["frame"]["project_id"] = project
        self.write()
        environment = dict(os.environ, DUXCEL_REVIEW_OUTBOX=str(self.root))
        result = subprocess.run([sys.executable, str(Path(review.__file__)), "list"], env=environment,
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        listing = json.loads(result.stdout)
        self.assertEqual(listing["project_id"], project)
        self.assertEqual([item["report_id"] for item in listing["reports"]], ["DXR-one"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
