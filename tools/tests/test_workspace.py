"""Workspace ownership, input retention and complete build cleanup."""
import pathlib
import tempfile
import unittest
from unittest import mock
import workspace as runner

class WorkspaceTests(unittest.TestCase):
    def test_source_fingerprint_includes_authored_workbook_sql(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "Makefile").write_text("all:\n")
            fixture = root / "tests/fixtures/workbook/input.sql"
            fixture.parent.mkdir(parents=True)
            fixture.write_text("SELECT 1;\n")
            before = runner.source_metadata(root)["source_sha256"]
            fixture.write_text("SELECT 2;\n")
            self.assertNotEqual(runner.source_metadata(root)["source_sha256"], before)

    def test_clean_removes_outputs_but_preserves_authored_inputs_and_cached_dependencies(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            kept = ["tests/fixtures/null.csv", "tests/test_example.c", "data/downloads/public.parquet", ".cache/web/toolchain/emcc", ".cache/web/raylib/libraylib.a"]
            removed = ["build/bin/duxcel", "build/tests/test_example", "build/tests/generated/book.dxls", "build/tests/results/frame.png", "build/web/app/old/duxcel.wasm", "tests/generated/old.dxls", ".DS_Store"]
            for name in kept + removed:
                path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b"owned")
            with mock.patch.object(runner, "ROOT", root), mock.patch.object(runner, "native_lock_path", return_value=root / "native.lock"):
                runner.clean_outputs()
            self.assertTrue(all((root / name).read_bytes() == b"owned" for name in kept))
            self.assertFalse(any((root / name).exists() for name in removed))
            self.assertFalse((root / "build").exists())
            self.assertFalse((root / ".state").exists())

    def test_clean_refuses_active_test_ownership_before_deleting_anything(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory); binary = root / "build/bin/duxcel"
            binary.parent.mkdir(parents=True); binary.write_bytes(b"accepted")
            for name in ("test-runner.lock", "web-build.lock"):
                with self.subTest(lock=name):
                    lock = root / "build/locks" / name
                    with runner.FileLock(lock, "test"), mock.patch.object(runner, "ROOT", root):
                        inode = lock.stat().st_ino
                        with self.assertRaises(RuntimeError):
                            runner.clean_outputs()
                        self.assertEqual(lock.stat().st_ino, inode)
                    self.assertEqual(binary.read_bytes(), b"accepted")

    def test_cleanup_gate_prevents_replacement_lock_and_allows_independent_owners(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            test = root / "build/locks/test-runner.lock"
            web = root / "build/locks/web-build.lock"
            with runner.FileLock(runner.checkout_gate_path(root), "cleanup"):
                with self.assertRaisesRegex(RuntimeError, "Workspace cleanup"):
                    with runner.FileLock(test, "test"):
                        self.fail("A new build lock escaped cleanup")
                self.assertFalse((root / "build").exists())
            with runner.FileLock(test, "test"), runner.FileLock(web, "web"):
                with self.assertRaises(RuntimeError):
                    with runner.FileLock(test, "second test"):
                        self.fail("Exclusive owner was bypassed")
                with self.assertRaises(RuntimeError):
                    with runner.FileLock(runner.checkout_gate_path(root), "cleanup"):
                        self.fail("Cleanup escaped the shared gate")
            with mock.patch.object(runner, "ROOT", root), mock.patch.object(runner, "native_lock_path", return_value=root / "native.lock"):
                runner.clean_outputs()
            self.assertFalse((root / "build").exists())

    def test_legacy_active_lock_is_honored_then_obsolete_state_is_removed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            lock = root / ".state/test-runner.lock"
            with mock.patch.object(runner, "ROOT", root), mock.patch.object(runner, "native_lock_path", return_value=root / "native.lock"):
                with runner.FileLock(lock, "old runner"), self.assertRaises(RuntimeError):
                    runner.clean_outputs()
                self.assertTrue(lock.exists())
                runner.clean_outputs()
            self.assertFalse((root / ".state").exists())

    def test_clean_migrates_legacy_inputs_and_refuses_conflicting_copies(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            old = root / "build/data/taxi.parquet"; old.parent.mkdir(parents=True); old.write_bytes(b"public input")
            with mock.patch.object(runner, "ROOT", root), mock.patch.object(runner, "native_lock_path", return_value=root / "native.lock"):
                runner.clean_outputs()
            self.assertEqual((root / "data/downloads/taxi.parquet").read_bytes(), b"public input")
            self.assertFalse((root / "build").exists())
            old.parent.mkdir(parents=True); old.write_bytes(b"other input")
            with mock.patch.object(runner, "ROOT", root), mock.patch.object(runner, "native_lock_path", return_value=root / "native.lock"), self.assertRaisesRegex(RuntimeError, "Both legacy and current"):
                runner.clean_outputs(full=True)
            self.assertEqual(old.read_bytes(), b"other input")
            self.assertEqual((root / "data/downloads/taxi.parquet").read_bytes(), b"public input")

    def test_distclean_also_removes_downloaded_caches_but_never_authored_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            for name in ["tests/test_example.inc", "data/downloads/input.parquet", ".cache/web/toolchain/emcc", "web/node_modules/package/index.js"]:
                path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_text("owned")
            with mock.patch.object(runner, "ROOT", root), mock.patch.object(runner, "native_lock_path", return_value=root / "native.lock"):
                runner.clean_outputs(full=True)
            self.assertEqual((root / "tests/test_example.inc").read_text(), "owned")
            self.assertEqual((root / "data/downloads/input.parquet").read_text(), "owned")
            self.assertFalse((root / ".cache").exists())
            self.assertFalse((root / "build/web").exists())
            self.assertFalse((root / "web/node_modules").exists())
